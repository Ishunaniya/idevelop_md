# hdx_aux 源码分析

> 分析对象：`/home/tronlong/lyp/code/rtms_sdk/apps/hdx_aux`；Git 分支 `rk3568_ubuntu_20241218`，提交 `dcd34abb4ec5`。本目录 `src/` 有 44 个 C 文件、约 2 万行代码。本文依据该版本的源码、CMake 与 `chs/` 脚本静态分析，覆盖全部源码目录的职责、当前两个入口的执行路径和外部依赖；它不是每个函数逐行注释。行号以此提交为准。文中的“运行”指能从当前 `main()` 路径到达；“编译”只指被 CMake 纳入目标。未在目标设备上实测。

## 1. 结论与边界

`hdx_aux` 是 SDK 中的 C 可执行程序。实际入口由 `argv[0]` 决定：名称包含 `hdx_aux` 时进入常驻辅助服务，名称包含 `hdx_dev` 时执行一项产测命令。CMake 只声明和安装 `hdx_aux`，没有独立创建 `hdx_dev`。常驻入口启动 CAN、RS485、RS422 处理线程，随后每秒检查并配置以太网与 Wi-Fi。产测入口涵盖配置写入、时钟、网络、CAN、串口、安全芯片、GPS、蜂窝通信和屏幕。以上均可由 `main.c:996` 至文件末尾核对。

`src/` 下所有 `.c` 文件都会进入同一个可执行文件，因此源码包含 IEC 101/104、MQTT、Modbus、安全层和数据处理等大量实现。然而当前 `main()` 没有调用 `initIoLayer()`、`startIoLayer()`、`initMqttLayer()`、`startMqttLayer()`、`start_modbus_server()`、`data_mng_init()` 或 `app_msg_task()`。不能把这些代码视为常驻服务的实际运行路径。它们的功能以下单列为“编译但未从主入口启动”。

无法仅凭本目录确认：设备镜像如何提供 `hdx_dev` 名称、nanomsg 各端口背后的服务是谁、硬件连线与产测接线、服务单元的启动顺序、目标设备上的实际执行结果。涉及这些内容时，本文只陈述源码中的调用和依赖。

## 2. 构建与交付

| 项目 | 代码事实 | 依据 |
| --- | --- | --- |
| 构建入口 | SDK 顶层 `CMakeLists.txt` 加载 `apps/`，`apps/CMakeLists.txt` 加载 `hdx_aux/`。 | SDK 两级 CMake 文件 |
| 目标 | CMake 3.11 起；`project(hdx_aux VERSION 1.0)`；`main.c` 加递归收集的 `src/*.c`。 | `CMakeLists.txt:1-22` |
| 头文件 | 生成 `global_config.h`；递归收集 `src/*.h` 所在目录并加入包含路径。 | `CMakeLists.txt:21-37`、`global_config.h.in` |
| 依赖 | 查找 nanomsg 1.2、cjson 1.7.15、cn-cbor 1.0、curl 7.69.1；链接 pthread、rt、stdc++ 等。 | `CMakeLists.txt:4-7,35` |
| 交叉编译 | 交叉编译分支要求 `QL_MODULE_PLATFORM=RK3568_UBUNTU`；RK3568 环境脚本设置编译器和此变量。 | `CMakeLists.txt:10-18`、`cross-profiles/rk3568-crosstool-env-init` |
| 安装 | 安装 `hdx_aux` 至 `usr/local/bin`，并复制 nanomsg、curl、cjson、cn-cbor 共享库至 `usr/local/lib`。 | `CMakeLists.txt:45-49` |
| 启动级别 | 目标定义 `STARTUP_LEVEL=1`，构建前写出 `hdx_aux_startup_level.json`。该 JSON 没有本目录的 `install()` 规则。 | `CMakeLists.txt:40-43` |

顶层 `README.md` 给出了 RK3568 的环境脚本、工具链文件、CMake、make 和安装流程。源码目录本身没有独立的 CTest 入口；`src/secLayer/s1161y_test.c` 因递归通配进入编译，文件名不代表自动执行。

## 3. 总体逻辑流程图

以下流程图使用纯文本，直接显示在 Markdown 中，不依赖 Mermaid 插件。

```text
main(argc, argv)
  │
  ├─ argv[0] 包含 "hdx_aux" → openlog
  │    ├─ 创建 CAN 线程
  │    ├─ 创建 RS485 线程
  │    ├─ 创建 RS422 线程
  │    └─ 无限循环 ─→ init_aux_eth() ─→ init_aux_wifi() ─→ sleep(1) ─┐
  │                       ↑                                             │
  │                       └─────────────────────────────────────────────┘
  │
  ├─ 否则 argv[0] 包含 "hdx_dev" → openlog
  │    ├─ argc < 2 → 打印参数错误 → 返回 -1
  │    └─ argc ≥ 2 → 按命令与参数条件匹配
  │         ├─ 匹配 → 调用相应产测函数 → 打印 success/failed
  │         └─ 未匹配 → 无命令结果提示
  │       两者最终均到达 main 的 return 0
  │
  └─ 两种名称均不包含 → return 0
```

依据：`main.c:996-1191`。两个名称判断是 `if ... else if`，因此若名称同时含两个子串，先进入 `hdx_aux` 分支。`hdx_dev` 分支中各命令是独立的 `if`；正常传入一个命令只会命中一项。未知命令不会提示错误，最后仍返回 0。常驻入口忽略三个线程创建函数及网络初始化函数的返回值，主线程继续循环。

### 3.1 常驻入口：网络维护

- `init_aux_eth()` 查询 `br0` 是否有 `192.168.2.61`；没有时设置固定 MAC 和该地址，返回 0；已存在时返回 -1。调用方没有利用返回值（`main.c:51-69,1007-1014`）。
- `init_aux_wifi()` 首次调用会停用 `mng_wifi`、结束 hostapd、关闭 `wlan0`、从 `br0` 移除无线接口；每次写 `/root/hostapd.conf`，检查并尝试加载 `rtl8189es.ko`、设置 `wlan0=192.168.4.61`、启动 hostapd（`main.c:103-172`）。配置文件中的 SSID 和口令是硬编码的，不从 `/opt/conf.ini` 获取。
- 上述两个函数在无限循环中每秒调用。函数内的 `system()` 依赖 `ifconfig`、`brctl`、`lsmod`、`hostapd`、`systemctl` 等目标设备命令。

### 3.2 常驻入口：CAN 与串口

```text
本目录未定义的 CAN 端口提供方
  └─ 16002/16003 → start_can_server() → can_msg_process()
       └─ ID=1/2 且 8 字节均为 0x55？
            ├─ 是 → 生成同 ID、8 字节 0xAA 响应 → 26002/26003
            └─ 否 → 忽略并等待下一帧

ttyS3/S4/S7/S8 → RS485 线程 → select/read
  └─ 首字节为通道号且随后 7 字节为 0x55？
       ├─ 是 → 同串口写通道号 + 7 字节 0xAA
       └─ 否 → 等待下一次读取

ttyS5 → RS422 线程 → 同样的 select/read 与应答规则
```

- CAN 线程建立两个 nanomsg `NN_SUB`，连接本机 `16002/16003`，按端口 0/1 区分 CAN0/CAN1。接收帧中的 `can_id` 分别为 1/2 且 8 字节均为 `0x55` 时，创建 `0xAA` 响应帧，通过 `NN_PUB` 连接 `26002/26003` 发出（`src/can_server/can_server.c:16-189`）。本程序在此处使用 `nn_connect`，没有绑定这些端口；端口提供者在本目录未定义。
- RS485 线程打开 `/dev/ttyS3`、`ttyS4`、`ttyS7`、`ttyS8`，RS422 线程打开 `/dev/ttyS5`，均设置 115200/8 位/1 停止位/无校验；用 `select()` 等待读取。请求是通道序号 1 至 4 加 7 个 `0x55`，回复是该序号加 7 个 `0xAA`。打开失败会在后续循环重试（`src/serial/rs485.c:17-149`）。

## 4. hdx_dev 产测命令逐项分析

产测入口要求至少一个命令参数。除 `id_secret` 和 `rtc` 只在 `argc == 3` 时执行外，其余命令按 `argv[1]` 匹配。`main()` 末尾固定返回 0；表中的“失败”指调用的测试函数及打印结果，并非进程退出码（`main.c:1017-1191`）。

| 命令 | 源码中的步骤与通过条件 | 主要副作用或依赖 | 依据 |
| --- | --- | --- | --- |
| `id_secret <值>` | `test_conf_ini()` 把值同时写作 `[dev]` 下的 `id` 和 `secret`，成功写文件返回 0。 | 覆盖 `/opt/conf.ini`。 | `main.c:175-189,1024-1033` |
| `rtc <秒数>` | `atoi()` 转 `int32_t`，通过 shell `date -s` 尝试设置系统时间；随后 `set_mcu_time()` 向 `26008` 发布时间命令，并在 `16008` 等待匹配 PID 和命令标记的回复，最多轮询 30 次、每次间隔 100 毫秒。以该函数返回值判断，未检查 `date -s` 的退出状态。 | 会尝试修改系统与 MCU 时间；本目录没有 MCU 端的命令处理实现。 | `main.c:191-199,1034-1043`、`src/ioLayer/ioLayer.c:521-591` |
| `eth` | ping `192.168.2.61` 5 次，命令退出状态为 0 即成功。 | 依赖对端或本机该 IP 可达；代码没有证明物理链路健康。 | `main.c:201-210` |
| `can` | 分别向 `26002/26003` 发送 CAN ID 1/2、8 个 `0x55`；从 `16002/16003` 等待 ID 相同且数据为 8 个 `0xAA`；每通道最多发送 3 次，接收超时 1 秒。 | 依赖 nanomsg 端口及响应方。 | `main.c:212-287` |
| `rs485` | 先停 `modbus_app`；依次测试 ttyS3/S4/S7/S8，各发通道号加 7 个 `0x55`，最多 5 次读取匹配的 `0xAA`；任何通道失败即返回 -1。 | 改动服务状态；串口目标须有回送方。 | `main.c:289-387` |
| `rs422` | 在 ttyS5 发通道号 1 加 7 个 `0x55`，最多 5 次读取匹配的 `0xAA`。 | 依赖串口回送方。 | `main.c:389-421` |
| `enc` | 停 `iec_app`；初始化 S1161Y SPI，依次取密钥版本、随机数、芯片序列号、数据签名；每项状态码应为 `0x9000`，失败再试一次。 | 访问 `/dev/spidev0.0` 及 SPI 片选 GPIO；函数的正数返回值区分失败步骤。 | `main.c:423-471`、`src/secLayer/s1161y.c:99-112` |
| `gps` | 订阅本机 `16005`，最多接收 10 次、每次超时 3 秒；解析 JSON 的 `location_info`，纬度或经度非 0 即成功。 | 失败时依据最后解析到的 `antenna_valid` 返回 -1 或 -2；并非直接操作 GPS 硬件。 | `main.c:474-513`、`src/app_server/gps.c:35-88` |
| `wifi` | 停相关网络服务，写 `wpa_supplicant.conf`，至多尝试 5 次启动驱动、hostapd、虚拟无线接口及 wpa_supplicant；最后 ping `192.168.4.61`。 | 会影响现有网络；依赖常驻入口生成的 `/root/hostapd.conf`，此函数本身没有创建它。 | `main.c:515-622` |
| `cellular` | 顺序执行 CAT4 和 CAT1；分别启动脚本、等待 `/dev/ttyUSB1`，发送 AT 命令，等待注册、启用 `usb0`/DHCP、获取地址并 ping 公网 IP；结果位 0 表 CAT4 失败，位 1 表 CAT1 失败。 | 停服务、改动 usb0、调用 `/root/chs/` 脚本；结束各阶段后调用对应停机脚本。 | `main.c:624-983` |
| `touchscreen` | 停 `car_fei` 与 `touch-backlight-c`，写背光亮度，执行 Qt 显示测试脚本，随后重新启动两个服务；以脚本退出状态判断。 | 影响显示及服务；需要 `/root/chs/qt/` 资源。 | `main.c:985-994` |

`cellular` 的主要状态顺序如下，CAT4 和 CAT1 采用基本相同的代码；两者分别位于 `main.c:688-821` 与 `823-954`。

```text
分别对 CAT4、CAT1 顺序执行：
  停既有服务和脚本 → 启动对应模组脚本 → 等待 /dev/ttyUSB1
       → ATE0 → ATI → AT+CPIN? → 配置 NAT → 设置/查询 CEREG
       → qnetdevctl → usb0 up + dhclient → 读取 usb0 地址
       → ping 114.114.114.114 ──成功──┐
                   │失败              │
                   ↓                  │
             ping 8.8.8.8 ───成功─────┤
                   │失败              ↓
                   └─ 返回地址/连通性检查阶段
  任一检查阶段重试次数耗尽 → 失败
  成功或失败 → 调用对应停机脚本 → 返回该段结果
  两段结果合并：位 0 = CAT4 失败；位 1 = CAT1 失败
```

每阶段成功会将计数重置为 `MAX_TRY=30`；失败时递减并睡 1 秒。CAT4 初始计数为 20，CAT1 为 30；成功并非保证网络业务长期稳定，只表示上述源码检查当时通过。

## 5. 编译进入目标但未由当前主入口启动的模块

下表描述源码中可核实的职责，不把它们画进第 3 节的常驻流程。直接从 `main()` 调用的例外会注明。

| 模块 | 源码职责与关键入口 | 当前入口关系 |
| --- | --- | --- |
| `src/config/general_config.c`、`ini_process.c` | 解析通用 JSON 参数；从 `/opt/conf.ini` 取 `dev:id`、`dev:secret`。 | `main()` 只直接调用写 INI 的 `test_conf_ini()`；没有初始化通用 JSON。 |
| `src/data_process/`、`src/data_mng.c` | 加载点表与通用参数，按 CAN/Modbus 规则更新数据项，生成 IEC、JSON、CBOR 工作数据。点表常量为 `/usr/local/etc/iec_app_pointsheet.json`。 | `data_mng_init()` 未由 `main()` 调用；CAN 常驻回调传入的参数为 NULL，仅走特殊产测帧处理。 |
| `src/modbus_server/` | 订阅 `16006/16007/16030/16031`，解析 Modbus 帧并交给数据处理模块。 | `start_modbus_server()` 未由 `main()` 调用。 |
| `src/app_server/`、`src/app_server/gps.c` | 订阅 GPS `16005`、解析 JSON；应用任务可更新数据项。 | `main()` 的 `gps` 产测直接用 `gps_info_parse()`；`app_msg_task()` 未启动。 |
| `src/linkLayer/` | TCP/串口链路、会话收发与安全层回调。 | `initLinkLayer()`、`startLinkLayer()` 未从 `main()` 启动。 |
| `src/secLayer/` | 安全帧、证书与 S1161Y SPI 命令。 | 安全层框架未启动；`enc` 产测直接调用 S1161Y API。 |
| `src/iecLayer/`、`src/ioLayer/` | IEC 101/104 帧、消息队列、遥测和控制处理。 | 层初始化未由 `main()` 调用；`rtc` 产测直接调用 `ioLayer.c` 的 `set_mcu_time()`。 |
| `src/mqttLayer/`、`src/MQTTPacket/` | MQTT 连接、发布订阅、报文编解码。 | `initMqttLayer()`、`startMqttLayer()` 未由 `main()` 调用。 |
| `src/http/` | 使用 curl 的 HTTP 登录及响应解析。 | `main()` 未调用。 |
| `src/scene/`、`src/scene_lib/` | 场景相关实现；`judge_scene()` 根据 CAN 参数返回场景名。 | `main()` 未调用；CAN 特殊帧处理函数虽包含相关头文件，当前没有调用场景判断。 |
| `src/common/`、`src/list/`、`src/iniparser/` | 日志宏、定时器、队列、nanomsg PUB/SUB 封装、链表与 INI 解析辅助。 | 被其他模块引用；其中日志宏由实际入口使用。 |

若未来启用协议栈，按已有函数关系看，`startIoLayer()` 调用 `startIecLayer()`，再调用 `startSecLayer()`，再调用 `startLinkLayer()`（分别见 `src/ioLayer/ioLayer.c:217`、`src/iecLayer/iecLayer.c:73`、`src/secLayer/secLayer.c:1981`）。这是**源码中的潜在启动链**，当前 `main()` 没有触发。接收方向的回调接口在各层中定义，但本目录未见当前入口对整条链的初始化和回调注册。

### 5.1 这些模块内部已实现的处理链

以下只描述函数内部关系，不表示它们当前已运行：

- **数据规则**：`data_mng_init()` 分配数据管理对象，调用 `data_process_mng_init()`，再从 `CAN_POINTSHEET` 路径读取点表（`src/data_process/data_mng.c:7-30`）。`load_pointsheet_json()` 解析点表版本、场景、设备槽位、报文规则和应用规则；`parse_common_msg()` 与 `parse_modbus_msg()` 按规则更新数据项，后续函数可以生成 IEC、JSON、CBOR 数据（`src/data_process/data_process.c:1174-1395,2202-2647`）。当前 CAN 常驻线程传入 NULL，没有建立这条数据管理链。
- **链路与协议**：`startLinkLayer()` 根据接口类型选择 TCP 客户端、TCP 服务端或串口接收任务（`src/linkLayer/linkLayer.c:649-742`）。`startSecLayer()` 先在 ARM 架构上初始化 S1161Y，再启动链路层（`src/secLayer/secLayer.c:1981-2013`）。`iecLayerRecvCb()` 按协议选 IEC 101 或 IEC 104 接收处理；`startIecLayer()` 还创建 System V 消息队列和 IEC 线程（`src/iecLayer/iecLayer.c:73-109,194-208`）。I/O 层定义遥测、遥控和时间同步接口，`startIoLayer()` 将启动动作转交 IEC 层。以上仍需正确初始化、设置选项并注册回调才可形成完整运行链。
- **MQTT 与 HTTP**：`startMqttLayer()` 创建 MQTT 链路线程并转调用安全层；发布线程的启动代码被 `#if 0` 屏蔽（`src/mqttLayer/mqttLayer.c:445-469`）。`httpLogin()` 将终端 ID 转成 Huawei ESN 后执行 curl 登录，未满足结果条件时每 10 秒重试（`src/http/http_login.c:155-176`）。当前 `main()` 没有启动 MQTT，也没有发起 HTTP 登录。

### 5.2 关键数据结构与配置边界

- `can_frame_t` 在 `src/can_server/can_server.h` 中定义为 `can_id`、`can_dlc`、保留/运行时间字段、8 字节数据、时间戳。常驻 CAN 线程与 `hdx_dev can` 都直接把此结构体作为 nanomsg 消息发送或接收；本目录没有跨平台字节序或结构体序列化层，因此通信双方须使用一致的数据布局。
- `gps_info_t` 在 `src/app_server/gps.h` 中保存时间、经纬度、高度、速度、卫星数、天线状态等字段。`gps_info_parse()` 只从 JSON 的 `location_info` 对象取值；`hdx_dev gps` 的通过条件只看纬度或经度非零，不检查时间、卫星数和精度。
- `general_config_t` 在 `src/config/general_config.h` 中包含远端协议、安全模式、TCP 模式、监听/远端端口、终端 ID、IEC 窗口和地址长度、MQTT 客户端字段。`load_general_config()` 可解析该 JSON，但当前两个入口都没有调用它；文件默认路径定义为 `/usr/local/etc/iec_app_general.json`。
- `data_mng_t` 持有数据处理管理对象；`data_process.c` 根据点表中的场景、槽位和规则组织数据。IEC 的 ASDU 类型枚举集中在 `src/data_process/data_type.h`。这些结构为协议层提供数据表达，当前常驻 CAN 特殊帧处理并未加载点表。
- `linkLayer.h` 定义最大会话数为 1、默认监听端口 2404，并提供 TCP 客户端、TCP 服务端及串口接口类型。它们是链路层默认值或可选模式，当前主入口并未启动该链路层，不能把 2404 列为本程序当前已监听端口。

### 5.3 设备脚本实际做了什么

`chs/start_cat4.sh` 使用 GPIO 120/123/122，`chs/start_cat1.sh` 使用 GPIO 121/125/124；两者均通过 `/sys/class/gpio` 配置电源和开机脉冲。对应 `stop_cat*.sh` 则发送关机脉冲并关闭电源。`main.c` 的蜂窝产测调用的是部署后的 `/root/chs/` 路径；仓库中的脚本内容说明了预期动作，但源码没有证明设备上该路径一定与仓库版本相同。`chs/qt/test_display.sh` 设置 Qt 的 framebuffer、插件、字体及库路径后执行传入的程序；`main.c` 将 `/root/chs/qt/qt_app` 作为参数传给它。

## 6. 资源与外部依赖索引

| 类别 | 源码中的值或路径 | 说明 |
| --- | --- | --- |
| 网络接口 | `br0`、`wlan0`、`wlan0-vxd`、`usb0` | 分别用于辅助以太网、Wi-Fi AP/虚拟接口、蜂窝网卡。 |
| 固定地址 | `192.168.2.61`、`192.168.4.61`、`192.168.4.51` | 前两者在常驻初始化及产测中出现；第三个用于 Wi-Fi 虚拟接口。 |
| CAN nanomsg | `16002/16003` 接收，`26002/26003` 发送 | 本程序通过 `nn_connect` 连接本机地址。 |
| GPS nanomsg | `16005` | `gps` 产测订阅；GPS 服务来源未在本目录确定。 |
| MCU 时间命令 | `26008` 发布、`16008` 订阅 | `rtc` 产测经 `set_mcu_time()` 收发；端口提供者未在本目录确定。 |
| 串口与 SPI | `/dev/ttyS3/S4/S7/S8/S5`、`/dev/ttyUSB1`、`/dev/spidev0.0` | 分别用于 RS485、RS422、蜂窝 AT 命令、安全芯片。 |
| 配置文件 | `/opt/conf.ini`、`/root/hostapd.conf`、`/root/wpa_supplicant.conf` | 第一项由 `id_secret` 写；后两项由网络逻辑生成。 |
| 编译模块配置 | `/usr/local/etc/iec_app_general.json`、`iec_app_pointsheet.json` | 由配置及数据模块定义，不能据此断言常驻入口已读取。 |
| 脚本 | `/root/chs/start_cat4.sh`、`start_cat1.sh`、对应 `stop_*.sh`、`qt/test_display.sh` | 由蜂窝及屏幕产测调用；本仓库 `chs/` 含对应脚本。 |
| 日志 | syslog、标准输出 | `LOG_*` 宏映射到 syslog；部分产测结果通过 `printf` 输出。 |

## 7. 静态分析发现的实现问题

以下是代码可直接定位的事实及其可能影响，未把设备故障当作已发生事件。

1. **CAN 轮询数组长度不一致**：`src/can_server/can_server.c:109-118` 定义并填充 2 个 `nn_pollfd`，`can_msg_process()` 在第 120 行却调用 `nn_poll(pfd, 3, 2000)`。库会按 3 个元素读取调用方内存，存在越界读取风险。
2. **常驻线程与网络初始化失败没有统一处理**：`main.c:1007-1014` 不检查 `init_aux_*()` 返回值。线程创建失败或网络配置失败后，主循环仍继续，日志之外没有明确的失败退出或恢复策略。
3. **Wi-Fi 初始化返回值检查不对应命令结果**：`main.c:106` 把 `rc` 初始化为 -1；`init_aux_wifi()` 在 `insmod`、`ifconfig`、`hostapd` 的 `system()` 调用后检查 `rc`，却没有把这些调用的返回值赋给 `rc`（`main.c:145-169`）。因此相应分支会返回 -1，即使命令已经成功执行。下一轮主循环可能继续推进，但单次返回值不可靠。
4. **产测进程退出码不能代表测试结果**：`hdx_dev` 分支打印各项结果后，`main.c:1190` 固定返回 0；未知命令也落到该返回路径。调用脚本须读取输出或改进代码后才能仅用退出码判定。
5. **CAN/GPS 成功接收路径未释放 nanomsg 消息**：`main.c:244-250,276-282,493-500` 在成功匹配时先 `break`，跳过 `nn_freemsg(msg)`。每次对应产测至少可能遗留一块由 nanomsg 分配的消息内存；进程结束后由系统回收。
6. **串口响应长度未严格校验**：常驻 RS485/RS422 线程在 `read()` 返回大于 0 后即比较第 0 字节和随后 7 字节（`src/serial/rs485.c:63-72,130-139`）；产测串口函数同样以 `len > 0` 进入 8 字节比较（`main.c:313-378,407-416`）。短帧时比较会使用本次未收到的缓冲区内容，结果不可靠。
7. **设备路径与固定值耦合**：IP、串口节点、Wi-Fi 名称和若干脚本路径写死在 `main.c`，搬到不同镜像或硬件版本时须逐项核对。此为可移植性限制，不代表这些值在当前目标设备上错误。
8. **RTC 结果只确认收到匹配回复**：`test_dev_rtc()` 未检查 `date -s` 的退出状态；`set_mcu_time()` 在 `src/ioLayer/ioLayer.c:566-583` 只按 PID 和命令标记匹配回复，没有检查回复结构中的 `error` 字段。因此产测打印成功也不能单独证明两个时钟都已被正确设置，应读回系统和 MCU 时间确认。

## 8. 核对依据与分析限度

本分析逐项查看了 `main.c` 的全部入口和产测函数、`CMakeLists.txt`、CAN 与串口常驻线程、GPS 解析、安全芯片接口、通信层启动函数、数据管理入口及 `chs/` 相关脚本；并用全项目文本检索核对端口、设备路径和关键函数的调用位置。第 3、4 节描述的是当前入口可达行为；第 5 节描述同一目标中的其他源码能力。文档没有运行程序、改动源码或验证实机连线，因此不对实际硬件产测通过率、服务部署方式或第三方库运行时行为作保证。

## 9. 设备故障排查手册

本节用于设备上怀疑 `hdx_aux` 或 `hdx_dev` 出问题时定位原因。先记录故障时间、软件版本、启动命令、现象和最近的变更，再按“进程 → 日志 → 对应功能 → 外部服务/硬件”的顺序排查。下面的命令是设备端示例；设备镜像若没有某个工具，使用其已有的等效命令。诊断时先做只读检查，产测命令会改动服务、网络、时间或硬件状态，应在允许的维护窗口执行。

```text
发现故障
  → 进程在吗？
      ├─ 否 → 核对启动命令/名称、动态库、启动日志、内核异常
      └─ 是 → 对应工作线程在吗？
          ├─ 否 → 查线程创建和退出时的日志、CAN/串口处理函数
          └─ 是 → 识别故障功能与最早失败步骤
              → 本程序发出正确请求了吗？
                  ├─ 否 → 查本程序调用链、参数、设备节点、返回值
                  └─ 是 → 查对端服务、物理连线、回复内容与时序
              → 记录证据 → 修复 → 在目标设备复核功能和副作用
```

### 9.1 先确认跑的是哪个入口和哪个版本

下列 `<PID>` 是占位符，使用时替换成实际进程号。

```sh
ps -ef | grep '[h]dx_'
pidof hdx_aux
ps -T -p <PID>
readlink -f /proc/<PID>/exe
tr '\0' ' ' < /proc/<PID>/cmdline
```

- `main.c:1004-1015` 按 `argv[0]` 的名字而非可执行文件内容选择分支。应同时核对启动命令、符号链接名及 `/proc/<PID>/exe`：执行文件实际是 `hdx_aux`，但以 `hdx_dev` 名称调用才会进入产测分支。若名称两者都不包含，`main()` 直接返回 0。
- 常驻入口正常创建主线程加 CAN、RS485、RS422 三个线程。线程数少于预期时，应结合启动日志和对应线程源码检查；线程成功创建不代表后来一直存活。`main.c:71-101` 只在创建失败时记错误，主线程不会因此退出。
- 程序编译版本由 `global_config.h.in` 与 CMake 的 `1.0` 决定，启动时通过 syslog 打印 `start hdx_aux version ...` 或 `start hdx_dev version ...`。设备文件的时间戳、包版本或 Git 标记需结合镜像部署资料核对；本源码没有在运行时输出 Git 提交号。

### 9.2 先取日志，再按现象定位

```sh
journalctl -t hdx_aux --since '1 hour ago'
journalctl -t hdx_dev --since '1 hour ago'
journalctl -k --since '1 hour ago'
systemctl status hdx_aux
```

`LOG_*` 宏在 `src/common/log_macro.h` 中映射到 syslog；`main.c` 为两个入口设置不同 ident。若镜像未使用 journald，可查设备上的 syslog 文件或日志服务；`chs/factory.sh` 还从 `/var/log/kern.log` 过滤 `hdx_dev`。`systemctl status hdx_aux` 只有在镜像确实定义了同名服务时才有意义，不应把“unit 不存在”直接判为程序损坏。记录首次错误及其前后的系统、内核日志，尤其留意串口、SPI、Wi-Fi 驱动、USB 枚举和进程异常退出。

| 观察到的现象 | 优先检查 | 可得结论与下一步 |
| --- | --- | --- |
| 进程不存在或反复重启 | 启动命令、`argv[0]`、动态库、启动日志、内核异常记录 | 名称不匹配会立即返回 0；启动失败还需看镜像启动方式，不能只归因于代码。 |
| 主进程在但 CAN/串口无响应 | `ps -T` 线程、CAN/串口日志、端口与设备节点 | 主循环可在工作线程退出后继续运行；逐一核对相应线程。 |
| 产测打印 `failed` 但退出码为 0 | 第 4 节命令表及实际输出 | 这是当前 `main()` 的返回值行为；以打印内容及对应函数条件分析。 |
| 启动后网络或服务被改动 | `main.c:51-172`、产测命令表 | 常驻初始化和部分产测主动操作网络、systemd 服务。先确认是否由该入口触发。 |
| 日志显示 CAN `nn_poll` 错误或进程异常 | `src/can_server/can_server.c:109-120` | 优先检查数组长度与 `nn_poll(..., 3, ...)` 不一致的问题，并结合异常记录确认实际影响。 |

### 9.3 常驻网络问题：以太网与 Wi-Fi

```sh
ip -br addr show br0
ip -br addr show wlan0
ip -br addr show wlan0-vxd
ip link show br0
lsmod | grep rtl8189es
ps -ef | grep '[h]ostapd'
systemctl status mng_wifi
```

**`br0` 没有 `192.168.2.61`**：对照 `main.c:51-69` 检查 `br0` 是否存在、设备是否有执行 `ifconfig` 的权限，以及命令是否报错。源码只查询当前地址并在缺失时执行 `ifconfig`，没有检查每条设置命令的退出状态。**有该地址仍产测失败**：`hdx_dev eth` 只是 ping `192.168.2.61`；它可能命中本机地址，既不能单独证明外部以太网线通，也不能定位交换机或对端故障。还需检查物理链路及目标业务通信。

**`wlan0` 不存在或 AP 未启动**：核对 `rtl8189es` 模块、`/root/rtl8189es.ko`、`/root/hostapd.conf` 是否存在，以及 `hostapd` 的日志。常驻函数第一次运行会停用 `mng_wifi`、结束 hostapd、关闭接口；之后每秒写 AP 配置并尝试启动。`main.c:106,145-169` 的 `rc` 检查没有读取相应 `system()` 结果，所以一次函数返回 -1 不能直接证明驱动或 hostapd 命令失败。使用接口状态、进程状态和实际命令日志交叉判断。

**`hdx_dev wifi` 失败**：它与常驻 AP 流程不同，会改动 NetworkManager、wpa_supplicant 和虚拟接口 `wlan0-vxd`，最终 ping `192.168.4.61`。检查 `/root/wpa_supplicant.conf` 是否写出、`wlan0-vxd` 是否得到 `192.168.4.51`、hostapd 与 wpa_supplicant 是否存活。该产测需要 `/root/hostapd.conf` 已经存在；若只单独执行 `hdx_dev wifi`，不能假定该文件由测试函数创建（`main.c:515-622`）。

### 9.4 CAN 与 RS485/RS422 无响应

```sh
ls -l /dev/ttyS3 /dev/ttyS4 /dev/ttyS5 /dev/ttyS7 /dev/ttyS8
ss -tnp
systemctl status modbus_app
```

- **CAN**：`hdx_aux` 作为 nanomsg 客户端连接本机 `16002/16003` 接收、`26002/26003` 发响应。先查提供这些端口的进程是否存在、连接是否建立，再核对它实际收到或发出的 `can_frame_t` 的 ID、长度与 8 字节内容。`nn_connect` 成功不等于 CAN 总线上已有报文；`hdx_dev can` 成功也只证明其约定的请求和响应在当前路径上成立。常驻 CAN 线程一旦因 `nn_poll` 或 `nn_recv` 错误退出，主进程仍可能存活；同时检查第 7 节的数组越界风险。
- **RS485/RS422**：先确认节点与权限，再查串口是否被其他服务占用。常驻 RS485 会打开 ttyS3/S4/S7/S8，RS422 会打开 ttyS5；它们只对“通道号 + 7 个 `0x55`”应答“通道号 + 7 个 `0xAA`”。未按该格式发报文不会有测试响应。`hdx_dev rs485` 会先停 `modbus_app`，但常驻 `hdx_aux` 本身也占用这些串口；产测失败要同时核对服务和进程占用。源码未严格检查短帧长度，若读到不足 8 字节，测试结果可能不可靠。
- **定位顺序**：端口/节点存在 → 权限和占用 → 对端服务或接线 → 请求格式与响应内容 → 本程序日志/线程。对硬件总线是否正常的结论须来自总线侧证据，不能仅由本程序进程存在推断。

### 9.5 单项产测故障定位

| 失败项 | 先看什么 | 源码中的判定范围 |
| --- | --- | --- |
| `id_secret` | `/opt/` 是否可写、文件内容是否为 `[dev]` 的 `id` 与 `secret` | 只判断写文件和关闭文件流程；同一个参数被写到两个键。 |
| `rtc` | 系统时间、`26008/16008` 的消息端点与回复、MCU 端日志 | 返回值只来自 `set_mcu_time()` 收到匹配 PID/命令标记的回复；`date -s` 执行状态和 MCU 回复的 `error` 字段均未参与判定。 |
| `enc` | `/dev/spidev0.0`、SPI GPIO、`iec_app` 是否占用芯片、返回码 | `-1` 为初始化失败；1、2、3、4 分别对应取密钥版本、随机数、序列号、签名两次仍未得 `0x9000`。 |
| `gps` | `16005` 的消息、JSON `location_info`、经纬度及 `antenna_valid` | 通过条件是纬度或经度非零；失败后的天线文案来自字段值，不等同于硬件检测仪结论。 |
| `cellular` | `/root/chs/` 脚本、GPIO、`/dev/ttyUSB1` 枚举、AT 回复、`usb0` 地址、外网 ping | 结果位 0 为 CAT4，位 1 为 CAT1；每段结束都调用对应停机脚本。按第 4 节状态流程定位卡住阶段。 |
| `touchscreen` | `/root/chs/qt/test_display.sh`、`qt_app`、`/dev/fb0`、Qt 插件和库、背光节点 | 仅以脚本返回值判断；触控输入是否准确需要另行观察。 |

`cellular` 失败时先看日志里最后的 `phase`，再沿 `main.c:688-954` 查该阶段使用的 AT 命令或网络条件。检查脚本中 GPIO 号是否对应当前硬件版本。不要把 `ping` 失败直接归因于模组：`usb0` 地址、路由、DNS 以外的联网条件及外部网络都可能影响结果；这里代码实际 ping 的是两个数字 IP，所以 DNS 不在其判定路径中。

### 9.6 确认问题属于本程序后如何收敛

1. 保存故障时间附近的 syslog、内核日志、进程和线程状态，以及相关接口或设备节点状态；记录使用的命令和实际输出，不只记录“失败”。
2. 用本节表格找到对应源码函数，核对其**通过条件、返回值、调用方是否检查返回值**。例如 `init_aux_wifi()` 的单次返回 -1 与实际接口状态可能不一致，`hdx_dev` 的进程退出码也不能代表测试结果。
3. 区分程序内部失败与外部前置条件缺失：nanomsg 端口由谁提供、`/root/chs/` 是否部署、驱动是否加载、串口是否被其他服务占用。能够观察到本程序发出正确请求但对端无回复时，应继续追查对端；尚未发出请求时追查本程序调用链。
4. 最小化复现：确定是常驻入口还是单项产测、首次运行还是循环后发生、只影响某个端口/通道还是全部通道。`hdx_aux` 主线程长期存活不表示各工作线程健康。
5. 修复后分别核对功能结果与副作用：例如 CAN 响应、串口帧、网络接口、被停止的服务是否按设备方案恢复。此步骤需要目标设备环境，不能用本目录的静态分析替代。

### 9.7 进程启动失败、崩溃或卡住

```sh
ls -l /usr/local/bin/hdx_aux
ldd /usr/local/bin/hdx_aux
journalctl -k --since '1 hour ago'
cat /proc/<PID>/status
ps -T -p <PID>
```

- **启动后立即退出**：先核对执行名是否包含 `hdx_aux` 或 `hdx_dev`，以及动态加载器是否报告缺少 nanomsg、cjson、cn-cbor、curl 等库。`ldd` 仅在目标系统提供时使用；安装规则将相关共享库复制到 `usr/local/lib`，还要核对设备实际动态库搜索路径。若只看到 `hdx_dev` 缺参数提示，按入口规则补充命令；若名称不匹配，程序会正常返回 0。
- **异常终止**：先保存内核崩溃记录或设备上的 core dump，再定位故障线程和栈帧。若栈位于 `can_msg_process()`/`nn_poll()` 附近，优先检查第 7 节的 `pfd[2]` 与轮询数量 3 不一致；若位于串口读帧附近，检查短帧处理。静态风险不等于已证实的崩溃原因，仍需栈和现场证据。
- **进程在但功能卡住**：比较线程列表、最后一条日志与外部依赖状态。主线程在每秒网络循环中，CAN 线程在 `nn_poll()` 中等待，串口线程在 `select()` 中等待；看到线程处于等待状态本身并不代表死锁。要看是否有输入、超时后是否继续处理，以及对端是否有响应。
- **只在运行一段时间后失效**：记录内存、线程数、文件描述符数量随时间的变化，并检查网络或串口设备是否重枚举。CAN/GPS 产测成功路径有未释放 nanomsg 消息的代码事实，但它们是短生命周期产测命令；不能仅凭此推断常驻进程长期内存增长。
