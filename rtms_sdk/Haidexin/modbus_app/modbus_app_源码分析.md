# modbus_app 源码逻辑全景分析

**阅读入口：**[主逻辑流程图](#3-主逻辑流程图)｜[单点 RTU 处理流程图](#7-单点-rtu-处理流程)｜[设备排障流程图](#12-设备故障排查手册)｜[配置语义](#5-配置加载的精确语义)｜[已核实的实现风险](#10-可核实的异常路径与实现风险)。流程图使用普通文本代码块，打开 Markdown 即可见，不依赖 Mermaid 渲染。

## 1. 分析范围与证据

- 源码根目录：`/home/tronlong/lyp/code/rtms_sdk/apps/modbus_app`；所属 Git 仓库根目录为 `/home/tronlong/lyp/code/rtms_sdk`。
- 分析时检出的分支：`rk3568_ubuntu_20241218`；提交：`dcd34abb4ec5f26529e093ecc4fd3fce5b7e6dff`。以下 `src/...` 等路径均相对于源码根目录。
- 覆盖主程序、所有 `src/` 下的 C 源码与头文件、CMake 构建、默认 JSON 配置，以及 `tool/` 中的脚本和调试工具。点表数据来自对实际 JSON 的解析，不依据文件名推测设备含义。
- 本文区分“源码确定的行为”和“需要设备验证的效果”。未在目标 RK3568 设备上运行程序、连接串口从站或测量网络行为。对 `cfmakeraw()` 的本机行为另做了最小 C 探针，结论限定在本机 libc。

## 2. 一句话定位

`modbus_app` 是 **Modbus RTU 主站采集器和 nanomsg 发布器**：它从固定 JSON 文件读取串口与点表配置，为最多四个串口各开一个线程，依次发送读请求，把通过校验的响应原始数据装入 `modbus_frame_t`，通过对应的 TCP PUB 端口发送。它不提供 Modbus TCP 服务，也不在本项目内解释电压、温度等业务量的比例系数和单位。依据：`main.c:13-32`、`src/modbus_forward.c:279-375`、`src/modbus_forward.h:37-46`。

## 3. 主逻辑流程图

```text
┌───────────────────────────────────────────────┐
│ main：openlog、记录版本、调用 modbus_forward_start │
└──────────────────────┬────────────────────────┘
                       ↓
          初始化 4 个通道并读取固定路径 JSON
                       ↓
          配置可解析且 com_count > 0？
                 ├─ 否 → 初始化失败，main 返回 -1
                 └─ 是 → 为 com0～com3 创建并分离线程
                             ├─ 主线程每 10 秒休眠，维持进程
                             └─ 每个通道线程：
                                  enable 为真？
                                  ├─ 否 → 线程退出
                                  └─ 是 → 尝试打开/配置串口
                                            ↓
                                         尝试绑定 PUB
                                            ↓
                                  串口与 PUB 均可用？
                                  ├─ 否 → 返回重试
                                  └─ 是 → 按顺序遍历点表 ────┐
                                            ↓               │
                                     检查发布时机与统计日志    │
                                            ↓               │
                                     发送 RTU 读请求          │
                                            ↓               │
                                     单次 read 并校验响应     │
                                            ↓               │
                                  通过 → 追加采集帧到缓存     │
                                  未通过 → 记录/忽略本次点     │
                                            └───────────────┘
                                  一轮结束后继续尝试下一轮
```

流程图简化了响应校验条件；站号与功能码比较的实际布尔条件见第 10 节。图只展示源码中的主控制路径，线程创建失败、`nn_send` 失败等边界另见下文。依据：`main.c:13-32`、`src/modbus_forward.c:279-375,458-620`。

## 4. 模块与构建关系

| 文件 | 实际职责 |
| --- | --- |
| `main.c`、`global_config.h.in` | 初始化 syslog；打印由 CMake 生成的主、次版本；启动采集，失败即返回，否则无限休眠。 |
| `src/modbus_forward.h/.c` | 全部通道参数、帧结构、JSON 解析、轮询、RTU 编解码与批量发布。 |
| `src/serial.h/.c` | `open()` 串口、配置 termios、循环写完请求、关闭 fd。 |
| `src/ipc_pubsub.h/.c` | 封装 nanomsg PUB/SUB 的创建、绑定/连接、发送、接收与释放；主程序只使用 PUB 路径。 |
| `src/sany_list.h/.c` | 通用单链表；主程序使用 `List_list(NULL)`、`List_push_end`、`List_length`、`List_get` 保存并按顺序访问点表。其他列表 API 未参与主链路。 |
| `src/log_macro.h` | `LOG_D/I/W/E` 等宏映射到 syslog 级别。 |
| `etc/modbus_app_config.json` | 安装时复制的默认串口和点表。 |
| `tool/modbus_xlsx2json.py`、点表 xlsx、exe | Excel 点表转换工具及其示例/二进制文件，不参与主程序运行。 |
| `tool/modbus_tool.c`、`build.sh`、现成 ARM 可执行文件 | 独立 nanomsg SUB 调试工具及其编译入口，不参与主程序的 CMake 目标。 |

`apps/CMakeLists.txt` 用 `add_subdirectory(modbus_app)` 纳入 SDK；本目录 `CMakeLists.txt` 定义版本 1.5，要求 CMake 3.11，查找 nanomsg 1.2 和 cjson 1.7.15，把 `src/*.c` 递归加入 `modbus_app`，链接 nanomsg、cjson、pthread、stdc++。交叉编译分支要求环境变量 `QL_MODULE_PLATFORM=RK3568_UBUNTU`；其他值会在该分支触发 CMake 错误。目标安装路径为 `usr/local/bin`，配置安装到 `usr/local/etc`，同时安装两库的指定共享对象。构建前命令会在构建目录生成 `modbus_app_startup_level.json`，值为 1；本 CMake 文件未将该 JSON 列为安装文件。依据：`CMakeLists.txt:1-49`、SDK `apps/CMakeLists.txt:20`、SDK `generate_img.sh:115-123`。

## 5. 配置加载的精确语义

固定读取路径是 `/usr/local/etc/modbus_app_config.json`；`etc/` 内的文件是安装输入，不是运行时相对路径。程序一次性读完整文件，用 `cJSON_ParseWithLength` 解析，只循环查找 `com0` 至 `com3`。每个存在的通道键才分配硬编码的设备节点和 PUB 地址。依据：`src/modbus_forward.h:8-35`、`src/modbus_forward.c:458-516`。

每个通道的典型结构如下；`names` 是随附 JSON 的说明数组，主程序按固定下标读 `point`，并不读取 `names` 来决定字段位置：

```json
{
  "com0": {
    "enable": 1,
    "serial_port": {"baudrate": 19200, "data_bit": 8, "stop_bit": 1, "parity": 0},
    "modbus_point": {
      "names": ["description", "slave_id", "function_code", "read_address", "read_quantity"],
      "point": [["city_volt_ab", 8, 3, 1036, 1]]
    }
  }
}
```

`point` 每行第 0 列名称仅供点表识别；程序实际读取第 1 至 4 列。点的次序由 JSON 数组和尾插链表保留，重复点不会去重。读取功能码仅实现 1、2、3、4；其他值进入 `switch default`，该次不形成有效采集帧。功能码 1/2 的数量会收敛到 1～64 位，3/4 收敛到 1～4 个寄存器；0 变为 1，超上限被截断。地址按数值直接发到 RTU 请求，不进行常见寄存器显示编号换算。依据：`src/modbus_forward.c:67-110,177-212,407-455`。

配置异常处理有几个容易误读的地方：

- 缺文件、空文件、读取不完整、JSON 解析失败、找不到任何计入 `com_count` 的通道：`modbus_forward_init` 返回空，主程序退出。依据：`src/modbus_forward.c:471-497,543-611`。
- `enable` 存在且不等于 1 时直接跳过；缺少 `enable` 时，通道初值仍是禁用，但这条路径会使 `com_count++`。所以“初始化成功”和“至少有一个启用线程”不是等价条件。依据：`src/modbus_forward.c:518-543,576-590`。
- 启用通道缺少任一串口参数键会被禁用；但整个 `serial_port` 对象缺失时，`modbus_load_serial_config` 仍返回成功。点表没有可装入的点会使该通道禁用。源码主要检查字段存在性，没有完整检查 JSON 类型和值域。依据：`src/modbus_forward.c:377-455,527-540`。
- `com_count` 不是实际可用串口数量；它只在对应代码路径末尾增加。全是显式禁用或无效启用通道时，通常为 0，启动失败。依据：`src/modbus_forward.c:518-555`。

## 6. 通道与随附点表

| 通道 | 串口 | PUB 绑定地址 | 默认配置 | 点数 | 站号与功能码 |
| --- | --- | --- | --- | ---: | --- |
| `com0` | `/dev/ttyS3` | `tcp://0.0.0.0:16006` | 启用，19200，8N1 | 12 | 站号 8；功能码 3 为 11 点，1 为 1 点 |
| `com1` | `/dev/ttyS4` | `tcp://0.0.0.0:16007` | 启用，19200，8N1 | 28 | 站号 7；功能码 3 为 27 点，1 为 1 点 |
| `com2` | `/dev/ttyS7` | `tcp://0.0.0.0:16030` | 启用，9600，8N1 | 6 | 站号 1、2、3、49；功能码均为 3 |
| `com3` | `/dev/ttyS8` | `tcp://0.0.0.0:16031` | 无条目，线程立即退出 | 0 | 可在配置中新增 |

地址和名称对应关系可以直接在 `etc/modbus_app_config.json` 中查看。为避免把点名误认成真实物理单位，下表列出全部默认点的 **名称:站号/功能码/地址/数量**，不做业务换算：

| 通道 | 默认采集点 |
| --- | --- |
| `com0` | `city_volt_ab:8/3/1036/1`、`city_volt_ac:8/3/1037/1`、`city_volt_bc:8/3/1038/1`、`city_a_volt:8/3/1033/1`、`city_b_volt:8/3/1034/1`、`city_c_volt:8/3/1035/1`、`city_Ia:8/3/1026/1`、`city_Ib:8/3/1027/1`、`city_Ic:8/3/1028/1`、`city_P:8/3/1001/1`、`city_Q:8/3/1006/1`、`city_MCB:8/1/0/1`。 |
| `com1` | `a_volt:7/3/1020/1`、`b_volt:7/3/1021/1`、`c_volt:7/3/1022/1`、`volt_ab:7/3/1023/1`、`volt_ac:7/3/1025/1`、`volt_bc:7/3/1024/1`、`Ia:7/3/1026/1`、`Ib:7/3/1027/1`、`Ic:7/3/1028/1`、`P:7/3/1040/1`、`Q:7/3/1041/1`、`Load_P:7/3/1046/2`、`Load_Q:7/3/1048/2`、`mom_freq:7/3/1032/1`、`mon_Ua:7/3/1033/1`、`mon_Ub:7/3/1034/1`、`mon_Uc:7/3/1035/1`、`mon_Uab:7/3/1036/1`、`mon_Ubc:7/3/1037/1`、`mon_Uac:7/3/1038/1`、`total_kwh:7/3/1283/2`、`gen_work_time:7/3/1291/2`、`gen_engine_rotate:7/3/1000/1`、`batt_volt:7/3/1051/1`、`oil_presure:7/3/1053/1`、`watertemp:7/3/1054/1`、`oil_level:7/3/1055/1`、`GCB:7/1/60/1`。 |
| `com2` | `temp1:2/3/0/1`、`humidity1:2/3/1/1`、`temp2:3/3/0/1`、`humidity2:3/3/1/1`、`noise:1/3/0/1`、`smoke:49/3/3/1`。 |

以上计数和数据由 `jq` 读取随附 JSON 核对；点名只是配置字符串，不能单独证明传感器型号、数据类型、比例系数或单位。设备映射来自 `src/modbus_forward.h:27-35`；点表来自 `etc/modbus_app_config.json`。

## 7. 单点 RTU 处理流程

```text
取下一个采集点
       ↓
功能码是 1、2、3、4？
  ├─ 否 → 不产生成功帧 → 下一个点
  └─ 是
       ↓
slave_id 为 0？
  ├─ 是 → 尝试站号 1～15；读取超时设为 400 ms
  └─ 否 → 使用配置站号；读取超时设为 1000 ms
       ↓
限制读取数量 → 拼接 8 字节 RTU 请求及 CRC
       ↓
tcflush → 循环 write → tcdrain → 等待 5 ms
       ↓
完整写入 8 字节？
  ├─ 否 → 本点无成功帧 → 下一个点
  └─ 是 → 单次 read，最多读取 64 字节
               ↓
          长度、字节数、CRC 等检查通过？
            ├─ 否 → 本点无成功帧 → 下一个点
            └─ 是 → 若在探测站号，则固定本点站号
                         ↓
                    将响应数据复制到 16 字节内部帧
                         ↓
                    加入该通道发布缓存 → 下一个点
```

请求固定为 8 字节：站号 1、功能码 1、起始地址 2、数量 2、CRC 2。响应读取前等待 5 ms，随后单次 `read(fd,rsp,64)`；读到数据或读错误后等待 200 ms，超时返回 0 时没有该 200 ms 等待。线圈/离散量预计数据字节数为 `ceil(quantity/8)`，寄存器为 `2*quantity`；总长度须等于数据字节数加 5，CRC 计算结果须为 0。有效数据从响应的第 4 个字节开始拷入 8 字节 `data` 区，其余清零。依据：`src/modbus_forward.c:24-56,58-276`。

站号为 0 时，`try_slave_id` 在 1～15 间轮换，收到通过代码校验的响应后把当前尝试值写回该点 `slave_id`，以后不再探测。注释“1～16”与取模 16 后跳过 0 的代码不一致；实际尝试范围为 1～15。依据：`src/modbus_forward.c:78-96,155-159,179-196,255-259`。

## 8. 时间、缓存和发布格式

- 每个启用通道各有串口 fd、点链表、计数器、发布缓存和 nanomsg PUB 端点；线程间没有在此模块共享点表或发布缓存。线程是 `pthread_detach` 的，主线程不 `join`。依据：`src/modbus_forward.h:48-85`、`src/modbus_forward.c:602-619`。
- 线程开始时尝试打开串口，并尝试绑定 PUB；任一尚未就绪则不轮询。打开串口或建立端点失败时会睡眠 1 秒并在下一轮重试。成功打开串口后，`setup_serial` 的返回值未被检查。依据：`src/modbus_forward.c:307-337`。
- 发送检查发生在 **每个点请求之前**：若与上次检查时间相差超过 1000 ms 且缓存非空，就发送缓存并清零；若加入下一帧将超过 2048 字节，也先发送并清零。计时使用 `CLOCK_MONOTONIC`。这是检查条件，不代表精确每 1 秒发送；如果后续点迟迟未执行，已有缓存会延后发送。依据：`src/modbus_forward.h:10-16`、`src/modbus_forward.c:290-295,341-367`。
- `nn_send` 的返回值在调用处没有判断；即使发送失败也清空缓存，因而不能把“写入缓存”或“调用发送”直接等同于订阅端收到数据。依据：`src/ipc_pubsub.c:16-26`、`src/modbus_forward.c:343-355`。
- 每 30 秒检查一次统计日志条件，日志显示累计 `tx_ok/tx_err/rx_ok/rx_err`；检查也在点循环内，因此并非独立定时任务。写入完整请求记 `tx_ok`；写入失败记 `tx_err`；读取错误、超时或显式校验失败记 `rx_err`；通过校验记 `rx_ok`。依据：`src/modbus_forward.c:117-169,217-269,356-362`。

`modbus_frame_t` 在当前主机头文件布局的最小探针中为 **16 字节**；字段按顺序为 `uint8_t slave_id`、`uint8_t function_code`、`uint16_t read_address`、`uint16_t read_quantity`、`uint8_t try_slave_id`、`uint8_t reserve[1]`、`uint8_t data[8]`。发送时是一个或多个结构体直接拼接的进程内字节，不含 JSON、长度头、时间戳、点名称或单位；多字节字段取决于目标 ABI/字节序，接收方必须按实际目标构建确认。依据：`src/modbus_forward.h:37-46`、`src/modbus_forward.c:160-164,260-264,364-367`。

## 9. 串口、IPC 和工具细节

`setup_serial` 通过 termios 设置波特率、数据位、停止位、校验和 `VTIME/VMIN`。代码列出的波特率是 2400、4800、9600、19200、38400、57600、115200、230400；其他值回退为 9600。读取模式设 `VMIN=0`，`VTIME` 以 100 ms 为单位。每次请求前会 `tcflush(TCIOFLUSH)`，写完后调用 `tcdrain`。依据：`src/serial.c:22-169`、`src/modbus_forward.c:88-116,188-216`。

`write_serial_and_retry` 会累计部分写入；`EAGAIN` 或返回 0 时每次睡眠 10 ms 后再试，其他负返回使循环中断。没有固定的重试次数或截止时间。依据：`src/serial.c:171-190`。

主程序用 `init_pubsub_endpoint(true,url,false,NULL)` 创建 `NN_PUB` 并绑定 `tcp://0.0.0.0:端口`；`ipc_pubsub.c` 的 SUB 接口是通用封装，在主程序路径未使用。调试工具 `modbus_tool` 则创建 4 个 `NN_SUB`，订阅全部消息，通过 libev 监听可读 fd，`recv debug` 时逐帧打印字段。工具源码的帧结构用 2 字节 `reserve` 覆盖主程序的 `try_slave_id+reserve[1]` 位置，字段布局在这些字节之后一致；工具不会打印名称、单位或换算值。依据：`src/ipc_pubsub.c:78-154`、`src/modbus_forward.c:326-334`、`tool/modbus_tool.c:11-49,70-112,114-204`。

`tool/build.sh` 调用 `aarch64-linux-gnu-gcc`，从相对的 SDK 构建目录取头文件和库，链接 libev、nanomsg；仓库内已有的 `tool/modbus_tool` 是 AArch64 ELF，`tool/modbus_xlsx2json.exe` 是 Windows x86-64 GUI 文件。Python 转换器使用 Tkinter 选择 xlsx、openpyxl 读取活动工作表，以 `port` 列分组，输出到执行时当前工作目录的 `modbus_app_config.json`；运行需要相应的 Python 包和图形环境。依据：`tool/build.sh`、`tool/modbus_xlsx2json.py:1-66`、文件格式检查。

## 10. 可核实的异常路径与实现风险

下表只陈述源码直接可见或本机可复现的事实，不把“可能影响”写成已在设备上出现的故障。

| 现象/风险 | 源码证据 | 可以确定的范围 |
| --- | --- | --- |
| 站号与功能码比较使用 `(rsp[0] != req[0]) && (rsp[1] != req[1])` | `src/modbus_forward.c:129,229` | 只有两者都不相等才因这条判断被拒绝；其中一个不匹配时仍可能继续后续校验。是否最终误收还取决于长度、字节数和 CRC。 |
| 串口数据位/奇偶校验设置顺序有问题 | `src/serial.c:32-71,149-156` | 先设数据位和 `PARENB`，再调用 `cfmakeraw`；本机 libc 探针显示 7 位+奇校验经该调用后变为 8 位且 `PARENB=0`，随后代码还清除 `INPCK/ISTRIP`。目标系统需用实际工具链或设备复核。默认配置均为 8N1，此风险主要涉及非默认串口格式。 |
| `setup_serial` 用 `char` 接收 fd；调用处未检查返回值 | `src/serial.h:4-6`、`src/serial.c:22-29`、`src/modbus_forward.c:315-323` | fd 先从 `int` 转为 `char`；超出 `char` 可表示范围时有截断风险。即使配置调用返回错误，线程仍继续后续逻辑。 |
| 响应只做一次 `read`，没有拼接逻辑 | `src/modbus_forward.c:40-55,119,219` | 分段到达或一次读到非预期长度时，后续长度检查会拒绝；具体发生频率需设备时序验证。 |
| 发送失败后缓存仍被清零 | `src/modbus_forward.c:343-355`、`src/ipc_pubsub.c:16-26` | 源码没有针对失败发送的缓存重试；不能据发送调用推断交付成功。 |
| 写操作可无限重试特定结果 | `src/serial.c:171-190` | 若写持续返回 0 或 `EAGAIN`，循环无上限，通道可能一直卡在该点；是否发生取决于串口驱动与运行条件。 |
| 配置解析未释放 `cJSON` 树，失败路径也只释放通道数组 | `src/modbus_forward.c:494-555,594-599` | 启动时有资源未完整清理；常驻进程只加载一次配置，因此不能据此推断持续增长的周期性泄漏。 |
| 线程创建与 detach 返回值未检查 | `src/modbus_forward.c:614-617` | `modbus_forward_start` 可在个别线程未成功创建时仍返回 0；实际是否触发需运行验证。 |
| 调试工具的 `usage` 标签在正常调用返回后也会执行 | `tool/modbus_tool.c:207-232` | `modbus_recv` 返回时会继续打印用法；这不是主程序采集路径。 |

## 11. 验证记录与边界

已完成的静态核对：逐一读取本目录 C 源码、头文件、CMake、Python 转换器和默认 JSON；用 `jq` 核对默认三个通道的点数、站号与功能码；用 `file` 检查现成工具的二进制平台；用最小 C 程序在本机检查 `sizeof(modbus_frame_t)=16` 和 `cfmakeraw` 对 `CS7/PARENB` 的影响。文档没有改动源码，也没有在目标设备上运行采集服务。

尚不能仅由这些证据确定的事项：真实串口和从站是否连通、实际波特率及校验位是否匹配、设备寄存器数值的单位/比例/有符号解释、订阅方使用何种 ABI 解帧、程序在目标 libc 与驱动上的超时和分段接收表现、`0.0.0.0` 绑定在实际网络环境中的可达性。要确认这些，需要目标固件、设备端日志和串口/订阅端实测。

## 12. 设备故障排查手册

本节针对“设备异常可能由 `modbus_app` 引起”的场景。先保留现场，再按链路逐层定位。下面命令只用于读取状态；不同固件可能没有 `journalctl`、`logread`、`ss` 或 `jq`，使用设备实际提供的等效工具即可。不要把示例命令的成功执行当成通信成功证据。

### 12.1 排障决策流程图

```text
设备数据异常
    ↓
确认运行的二进制、配置文件、故障时间
    ↓
modbus_app 进程存在？
  ├─ 否 → 检查安装文件、启动日志、配置读取与 JSON 解析
  └─ 是
       ↓
目标 comN 被启用且线程运行？
  ├─ 否 → 核对 enable、串口字段、点表、启动日志
  └─ 是
       ↓
目标串口能打开并完成配置？
  ├─ 否 → 核对设备节点、权限、占用、termios 实际值
  └─ 是
       ↓
请求已写出且有有效 RTU 响应？
  ├─ 否 → 分别判断写失败、超时、站号/功能码、
  │        字节数、帧长、CRC；必要时做被动抓帧
  └─ 是
       ↓
目标 PUB 端口存在且有消息？
  ├─ 否 → 查绑定失败、nn_send 错误、缓存发送时机
  └─ 是
       ↓
订阅端解帧和业务换算正确？
  ├─ 否 → 比较 16 字节帧、地址、数量、原始字节、
  │        字节序、符号位、比例系数与点表
  └─ 是 → 再调查其他应用或设备链路
```

该图是**定位顺序**，不是“经过某检查即证明下一层正确”。例如 `tx_ok` 只证明程序的 `write` 返回完整字节数；`rx_ok` 只证明通过当前代码的校验；`nn_send` 被调用也不证明订阅端收到消息。依据：`src/modbus_forward.c:117-169,217-269,343-367`、`src/ipc_pubsub.c:16-26`。

### 12.2 保留现场与确认运行版本

先记录故障起止时间、受影响通道/点名、是否持续或间歇、设备固件版本、实际程序文件及配置文件的修改时间与校验值。运行设备上的程序未必与本文分析的 Git 提交一致；`main` 启动日志只打印 `1.5` 主次版本，不提供提交号，不能据此证明代码完全相同。程序未实现 `--version` 参数；`main` 不解析命令行选项。依据：`main.c:13-32`、`CMakeLists.txt:1-2`。

```sh
ps -ef | grep '[m]odbus_app'
ls -l /usr/local/bin/modbus_app /usr/local/etc/modbus_app_config.json
sha256sum /usr/local/bin/modbus_app /usr/local/etc/modbus_app_config.json
```

若设备没有 `sha256sum`，记录文件大小、修改时间并把配置和可执行文件留存供离线比对。定位到进程 PID 后，可用设备支持的 `/proc/<PID>/exe` 或等效方式核对正在执行的文件；不要只看磁盘上同名文件。拿不到设备上的二进制或固件构建记录时，本文的源码结论只能作为候选解释。

### 12.3 启动、配置与通道线程

运行时配置必须在 `/usr/local/etc/modbus_app_config.json`，不是源码目录中的 `etc/modbus_app_config.json`。先在设备上验证 JSON 语法，再核对 `comN.enable`、串口四个参数和每行点数组的第 1～4 项。若设备有 `jq`，可执行：

```sh
jq empty /usr/local/etc/modbus_app_config.json
jq -r 'to_entries[] | [.key, .value.enable, (.value.modbus_point.point | length)] | @tsv' /usr/local/etc/modbus_app_config.json
```

第二条命令只适用于条目含 `modbus_point.point` 的配置；若报错，应直接打开 JSON 检查结构。源码对 `enable` 缺失、`serial_port` 整体缺失等情况并非严格报错，`init com count` 也不等于实际工作线程数。默认只配置并启用 `com0`、`com1`、`com2`；`com3` 没有配置，看到 `disable modbus request task, channel = 3` 是预期。修改配置后程序不会动态重载；先留存原配置和日志，再按设备实际的进程管理方式重启并观察。依据：`src/modbus_forward.c:377-455,471-555,558-619`、`etc/modbus_app_config.json`。

若进程启动即退出，先查启动日志中的 `failed to init modbus forward`、`failed to start modbus forward`、`invalid comN serial config`、`invalid comN point config`。配置文件打不开或 JSON 解析失败时，源码没有打印更具体的文件/解析错误，因此要直接检查路径、权限、文件内容及配置来源。依据：`main.c:21-27`、`src/modbus_forward.c:471-497,527-540,608-612`。

### 12.4 日志获取与关键日志对照

`openlog` 的标识是 `modbus_app`；`LOG_D/I/W/E` 分别进入 syslog 的 debug/info/warning/error 级别。根据设备实际日志系统选择一种读取方式：

```sh
journalctl -t modbus_app
logread | grep modbus_app
```

两行是**备选方式**，不是要求都能运行；若固件使用其他 syslog 收集方式，查对应日志文件或服务。debug 级别的 30 秒统计日志可能被日志配置过滤，缺少这条日志不表示线程必然停止。源码对读超时只增加 `rx_err`，对应警告输出被注释，因此“无错误日志”不能排除从站未响应。依据：`main.c:17-20`、`src/log_macro.h:5-11`、`src/modbus_forward.c:124-127,224-227,356-362`。

| 日志片段 | 可直接确认的代码位置 | 下一步 |
| --- | --- | --- |
| `start modbus_app version 1.5` | `main.c:18-20` 已执行 | 继续找初始化和线程日志；只确认主次版本。 |
| `failed to init modbus forward` / `failed to start modbus forward` | `src/modbus_forward.c:608-612` / `main.c:21-27` | 核对固定配置路径、JSON、`enable` 与点表。 |
| `invalid comN serial config, disable` | `src/modbus_forward.c:527-533` | 查 `serial_port` 内四个必需字段是否缺失。 |
| `invalid comN point config, disable` | `src/modbus_forward.c:535-540` | 查 `modbus_point.point` 是否有能读出第 1～4 项的行。 |
| `failed to open /dev/ttyS*` | `src/modbus_forward.c:309-313` | 查节点、权限、占用和硬件映射。 |
| `failed to open ipc for tcp://...` / `nn_bind:` | `src/modbus_forward.c:326-334`、`src/ipc_pubsub.c:103-105` | 查对应端口是否已被占用和进程网络环境。 |
| `write rc ...` | `src/modbus_forward.c:166-169,266-269` | 查串口写入、设备驱动和物理链路；结合 `tx_err`。 |
| `invalid modbus bytes number` / `frame len` / `crc` | `src/modbus_forward.c:135-152,235-252` | 对照请求、原始响应及到达时序，不直接认定是从站故障。 |
| `nn_send:` | `src/ipc_pubsub.c:16-25` | 查发布端错误及订阅链路；该批缓存之后会被清零。 |
| `tx_ok ... rx_ok ... rx_lost ...` | `src/modbus_forward.c:356-362` | 比较一段时间内计数的增量，勿只看单次累计值。 |

日志中的 `tx_lost` 和 `rx_lost` 分别打印结构体的 `tx_err`、`rx_err`；名称不代表已做网络丢包测量。成功绑定 PUB 的日志使用 `LOG_E`，所以日志级别为 error 的 `open ipc for` **不一定是错误**。依据：`src/modbus_forward.c:333,356-360`。

### 12.5 串口打开与实际通信参数

按第 6 节映射核对设备节点。默认配置是 `com0=/dev/ttyS3,19200,8N1`、`com1=/dev/ttyS4,19200,8N1`、`com2=/dev/ttyS7,9600,8N1`；`com3=/dev/ttyS8` 仅代码预留。可先只读检查：

```sh
ls -l /dev/ttyS3 /dev/ttyS4 /dev/ttyS7 /dev/ttyS8
stty -F /dev/ttyS3 -a
```

`stty` 示例只查询 `com0`；其他通道替换设备节点。还应核对程序运行用户对节点的权限、是否被另一进程占用、设备树/复用配置与现场接线。后几项属于现场检查，不能单靠本文源码判定。日志 `setup serial ...` 只打印请求设置，`setup_serial()` 的返回值被忽略，不能代替实际 termios 查询。若配置为 7 位或奇偶校验，优先验证第 10 节指出的 `cfmakeraw` 顺序问题在目标 libc 上是否复现。依据：`src/modbus_forward.c:309-323`、`src/serial.c:22-169`。

### 12.6 判断请求、响应和点表是否正确

优先针对一个异常通道、一个点定位，按“配置行 → 实际请求 → 原始响应 → 发布帧”做同一时刻的对照。功能码 1/2 与 3/4 的数量上限不同；`slave_id=0` 只自动探测 1～15，默认点表中的 `com2` 站号 49 是固定站号，不受该探测范围限制。调试时先查日志里的 `write rc`、`read error`、`invalid modbus ...`，再看计数增量。依据：`src/modbus_forward.c:58-276`、`etc/modbus_app_config.json`。

| 观察 | 能确定什么 | 仍需检查什么 |
| --- | --- | --- |
| `tx_ok` 不增长 | 本程序没有记录到完整 8 字节写入 | 是否线程启用、串口/PUB 是否就绪、点表是否有支持的功能码、写入是否卡住。 |
| `tx_ok` 增长、`rx_err` 增长、`rx_ok` 不增长 | 请求写入返回完整，但没有通过本程序响应校验 | 从站是否响应、串口参数、站号、地址/功能码、接线、分段读取、CRC；超时可能无警告。 |
| 出现字节数/帧长错误 | 收到的数据与程序预期不一致 | 被动串口抓帧核对完整响应，检查一帧是否被单次 `read` 拆开。 |
| 出现 CRC 错误 | 程序对本次读到的字节计算 CRC 不为 0 | 保留原始字节后再判断线路噪声、时序或解析问题；仅凭该日志不能归因。 |
| `rx_ok` 增长 | 收到通过**当前代码**校验的响应 | 仍需考虑站号/功能码条件的 `&&` 问题，并检查发布与业务解码。 |

需要抓物理串口时优先采用**不向总线注入请求的被动采集**，记录请求和响应的完整十六进制、时间戳及串口参数；不要同时启动第二个 Modbus 主站抢占同一端口。这样才能区分设备无响应、接收分段、校验问题和点表不匹配。抓帧属于设备侧实测步骤，本文没有假定其结果。

### 12.7 PUB 端口、订阅工具和业务数值

进程运行且 `rx_ok` 增长后，再查发布路径。预期绑定端口由已启用的配置决定，默认是 16006、16007、16030；默认没有 `com3` 线程发布 16031。设备若有 `ss`，可用 `ss -ltnp` 查看监听者并与第 6 节映射比较；若没有则用固件自带网络查看工具。程序使用 `0.0.0.0` 绑定，实际外部能否访问还取决于设备网络环境。依据：`src/modbus_forward.h:27-35`、`src/modbus_forward.c:326-337`。

仓库的 AArch64 调试工具可在**匹配其运行依赖的目标设备**上执行 `modbus_tool recv debug`。它同时连接四个端口，逐帧打印站号、功能码、地址、数量和 8 字节数据；默认 `com3` 没有发布者，不应仅因该通道无数据显示就判主程序故障。现成工具需要 libev、nanomsg 等运行库；也可以用已知正确的订阅端对照。依据：`tool/modbus_tool.c:11-49,70-112,150-232`、`tool/build.sh`。

若 `rx_ok` 在增长但订阅端看不到消息，依次查 PUB 是否成功绑定、是否有 `nn_send:` 错误、订阅端连接的地址/端口与订阅过滤、采集帧是否尚在缓存等待下一个点触发发送。源码发送失败仍清零缓存，且没有消息级确认或重发。依据：`src/modbus_forward.c:341-367`、`src/ipc_pubsub.c:16-26`。

若收到了帧但业务值不对，先对比点表里的站号/功能码/地址/数量与下发请求，再对比 `data[8]` 原始字节和设备手册。发布帧没有 `description`、单位或比例系数，也没有显式字节序标记；`read_address` 是请求中的原始地址。需要检查下游对 16 字节结构体、寄存器高低字节、多个寄存器拼接顺序、有符号/浮点格式和比例系数的解释；这些解释不在本程序源码中，必须以设备协议和下游实现为准。依据：`src/modbus_forward.h:37-46`、`src/modbus_forward.c:407-445,160-164,260-264`。

### 12.8 间歇故障、性能与排查结论

对间歇故障，至少在正常与异常两个时间段记录同一通道的 `tx_ok/tx_err/rx_ok/rx_err` 增量、串口/网络日志和同一点原始帧。程序逐点串行轮询，串口 `read` 设置了超时，读到数据或读错误后另等 200 ms；`1000 ms` 是缓存发送检查间隔，并非每个点固定采样周期。整个单点请求没有统一的完成期限：`write_serial_and_retry` 对持续的 0/`EAGAIN` 没有退出期限，遇到卡点时应结合线程栈或系统调用观测，不能仅凭“进程还在”判定轮询继续。依据：`src/modbus_forward.c:40-55,279-375`、`src/serial.c:171-190`。

最终交接问题时，附上：设备与固件标识、运行二进制与配置的校验值、受影响通道/点、故障时段、对应日志、计数增量、串口原始请求/响应、PUB 端口状态、订阅端原始 16 字节帧，以及已经核对的设备寄存器定义。将每个结论写成“**观察到的证据 → 与哪段源码对应 → 仍需哪项验证**”，避免把“无数据”直接归因于程序、接线或下游中的某一方。
