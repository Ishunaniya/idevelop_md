# electric_station_truck_cn 项目说明（自卸车分支）

## 项目位置与用途

- 源码仓库：`/home/tronlong/lyp/code/electric_station_truck_cn/`。本文档存放位置：`/home/tronlong/lyp/perCode/idevelop_md/electric_station_truck_cn/dump_truck_20211021/AGENTS.md`。
- 本文依据 Git 分支 `feat/dump_truck_20211021` 的提交 `e50ab6dfc0191e14e85aed4ea6f04a50a96c6a9d` 核对。分支名标识源码开发线，提交哈希标识此次阅读的具体版本；后续切换分支或更新提交，应重新核对本文。`dump_truck_20211021` 是文档目录名，不是设备运行时的配置项。
- 仓库生成车载换电客户端 `bs_client`，用于国内版车辆换电场景：从本机服务取得 CAN 数据，采集锁销、连接器和 ACC 状态，连接换电站完成登录、鉴权、心跳、状态与参数交互，再把换电控制和故障信息交给本机命令服务。当前分支默认车型是 `dump_truck`（自卸车），同时保留 `heavy_truck`（重卡）代码；**实际走哪个车型分支由设备上的 `/opt/electic_station.ini` 中 `station:vehicle_type` 决定，不能只看 Git 分支名。**
- 项目主要使用 C，日志模块为 C++，通过 Makefile 构建；根目录 `Makefile` 指向 `Makefile-EC200A`，另有 `Makefile-EG25`。目标是移远模块上的 Linux 设备，需要相应 SDK、交叉编译工具链和设备侧服务。根目录 `README.md` 只有项目名称，阅读入口以本文件和源码为准。

### 相关技术与名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| Git 分支、提交哈希 | `feat/dump_truck_20211021` 是当前源码开发线；上述 40 位哈希固定此次核对版本。分支会继续移动，复现行为时应同时记录提交。 |
| T-Box、`bs_client`、换电站 | T-Box 是车载通信终端；`bs_client` 是本仓库产出的设备进程；换电站是它通过 TCP 连接的站端系统。 |
| EC200A、EG25、SDK、交叉编译 | 两套 Makefile 对应不同移远模块环境。SDK 提供目标平台头文件和库；交叉编译是在开发主机上生成目标设备可执行文件。默认 EC200A 配置还定义了 `EC200A_ENABLE`。 |
| TCP socket、`bridge0:0` | TCP socket 用于站端及本机服务通信。连接站端前，代码要求网络接口 `bridge0:0` 的地址为 `192.168.100.1`；启动脚本也配置该地址。源码中的 `wifiState` 在站端 TCP 建连/断开时更新，并非无线物理链路的直接检测值。 |
| CAN、VCU、BMS、GPS | CAN 是车内控制器通信总线；VCU 是整车控制器；BMS 是电池管理系统。本进程通过本机 TCP 服务获取三路 CAN 帧和一路 GPS 数据，再解析车况、电池、VIN 等字段。GPS 数据接收路径存在，但当前提交已屏蔽 GPS/4G 故障判断。 |
| BCC1、锁销、连接器、DI、DO | `BCC1_*` 是代码中换电控制和状态字段的命名。DI 是数字输入，锁销与连接器反馈通过 `/tmp/di_in_07`、`/tmp/di_in_08`、`/tmp/di_pwm_09` 读取并按 MCU 反向电平换算；DO 是数字输出，自卸车的 `/tmp/do_06` 用于提交控制电平。实际电气动作应以设备接线与控制器协议核实。 |
| ACC、`VCU_ePTState` | ACC 是车辆钥匙电状态。自卸车主循环读取 `/tmp/acc_stat`：首字节不是字符 `0` 时，将 `VCU_ePTState` 设为 2、`VCU_ExPowerBatAllow` 设为 0；首字节为 `0` 时分别设为 0 和 1。这是本分支的状态映射；文件读取异常时不能直接等同于真实钥匙电状态。 |
| VIN、ICCID | VIN 是 17 字节车辆识别码；ICCID 是 SIM 卡标识。自卸车可从配置、J1939 TP 分包或运行期间的 `/opt/machine_vin` 获取 VIN。EC200A 构建中的 `get_iccid()` 调用移远 SIM SDK；非 EC200A 编译分支返回固定占位串 `12345678901234567890`，不能将占位串视为真实 ICCID。 |
| J1939 TP.BAM / TP.DT、PGN | J1939 是基于 CAN 的重型车辆通信协议。TP.BAM 宣告多包消息长度和 PGN，TP.DT 承载分包数据；自卸车 VIN 路径解析 `0x98ECFF80` 与 `0x98EBFF80` 报文，要求长度 17、包数 3、PGN `0xFEEC` 后拼接 VIN。PGN 标识报文参数组。 |
| AES-128、鉴权、BCC | 登录后，站端随机数和扩展参数参与 AES-128 ECB 加密应答；当前活动路径调用 OpenSSL `libcrypto`，源码内置 16 字节密钥。站端报文末尾的 BCC 是异或校验字节，与 `BCC1_*` 状态字段不是一回事。 |
| INI、`iniparser` | INI 是节与键组成的文本配置格式；`src/iniparser/` 负责解析。运行时使用 `/opt/electic_station.ini`；`electic` 是源码实际拼写。仓库 `ini/` 下的是部署样例，不会自动成为设备配置。 |
| 故障掩码、故障等级 | `src/fault/` 为自卸车计算 `fault_mask` 和 `fault_level`，经本机命令服务发送。当前提交已用 `#if 0` 屏蔽 GPS/4G 故障判断；仍检查 CAN、Wi-Fi 接口、空闲内存、锁销和连接器。这里的 Wi-Fi 检查使用 `wlan0` 接口标志，与站端 TCP 的 `wifiState` 不同。 |

## 主要文件与职责

| 路径 | 职责 |
| --- | --- |
| `Makefile`、`Makefile-EC200A`、`Makefile-EG25`、`sdk.mk` | 选择构建平台、编译和链接 `bs_client`；`sdk.mk` 指向 EC200A SDK 与工具链，本地构建前需按实际环境核对路径。 |
| `main.c` | 单实例启动、信号处理、线程创建及约 500 ms 一轮的周期任务；处理 DI、ACC、自卸车状态迁移、DO 和故障。 |
| `src/app_mng/` | 建立共享状态，读取设备 ID、车型、VIN、站端地址、历史换电状态和 ICCID。 |
| `src/batt_stand_client/` | 站端 TCP 连接、消息收发、登录、鉴权、心跳、参数设置与属性查询；自卸车站端锁止/解锁请求也在这里触发 DO。 |
| `src/can_server/` | 连接本机三路 CAN 服务及一路 GPS 服务；解析 CAN 帧和 GPS 数据。自卸车路径解析手刹、SOC、容量、里程及 J1939 TP VIN。 |
| `src/cmd/` | 将唤醒请求、换电状态、故障掩码与等级写给本机命令服务。 |
| `src/fault/` | 自卸车故障判定；本提交屏蔽了 GPS/4G 故障判断代码。 |
| `src/common/`、`src/iniparser/`、`src/net_utils/`、`src/skt_res/` | 设备信息与配置读写、INI 解析、网络地址与 TCP 辅助函数。 |
| `src/aes128/`、`src/list/`、`src/log/` | 自带 AES 实现、待发消息链表、异步日志；当前站端鉴权调用 OpenSSL AES 接口。 |
| `ini/`、`scripts/start_bs.sh` | 配置样例；设备端启动、保活和 `bridge0:0` 地址配置。 |

## 启动、通信和自卸车流程

1. `main()` 检查单实例，初始化日志，读取配置与设备标识；然后启动站端接收、站端发送、本机 CAN 接收三个线程，主线程进入周期任务。周期循环末尾 `usleep(500000)`，所以实际周期还包含本轮处理时间。
2. 站端接收线程确认 `bridge0:0` 地址后连接配置的站端 TCP 服务；发送线程依次尝试车辆登录、请求随机数、发送加密鉴权应答、发送心跳和队列消息。接收线程处理站端参数设置及属性查询。断开后重置登录、鉴权状态并尝试重连。
3. CAN 接收线程先连接本机三路 CAN TCP 服务和 GPS TCP 服务，再读取相应数据；任一路连接失败会使本轮连接过程重试。自卸车路径从相应 CAN ID 解析手刹、SOC、电池额定容量、累计充放电量、里程；VIN 可由 J1939 TP.BAM/TP.DT 分包拼接。主循环不会主动调用重卡 CAN VIN 请求函数，即使 `enable_vin_request=true` 也不会走那条主动请求路径；但仍会周期性尝试读取 `/opt/machine_vin`。
4. 周期任务读取旧电池底托 DI 反馈，处理站端参数应答、CAN/BMS 超时、本机命令发送和离线锁止。自卸车另外按 ACC 改写 VCU 状态；根据锁销反馈在 `0x00`、`0x02`、`0x03`、`0x04`、`0x06` 状态间修正换电状态，变化时写回 `station:exchange_state`；随后执行自卸车故障判断。
5. 站端下发 `0x02`（准备换电/解锁）时，自卸车调用 `do_ctrl(1)`；下发 `0x04`（电池装配/锁止）且连接器反馈为已连接时调用 `do_ctrl(0)`。两种请求都等待锁销反馈再应答：反馈到位时回复成功，超过约 10 秒时回复超时；其他有效换电状态通常立即应答。满足车辆 Ready、手刹释放和站端断连等条件时，周期任务还可能设置锁止状态，并对自卸车调用 `do_ctrl(0)`。`SIGUSR1` / `SIGUSR2` 也能在进程内置入解锁 / 锁止状态；具体动作仍依赖设备侧其他进程与硬件。

## 配置、端口与部署

| 项目 | 当前源码约定 |
| --- | --- |
| 车型与站端配置 | `/opt/electic_station.ini` 的 `station:vehicle_type` 默认 `dump_truck`；`station:server_ip`、`station:server_port` 默认 `192.168.100.101:7701`；还读取 `station:vin`、`station:enable_vin_request`（默认 `true`）、`station:exchange_state`（默认 0）。启动时会写回部分配置，自卸车状态变化时也写回 `exchange_state`。 |
| 配置样例 | `ini/electic_station_dump_truck.ini` 设置 `vehicle_type=dump_truck` 和 `enable_vin_request=true`；需按设备部署要求放入 `/opt/electic_station.ini`。 |
| 设备身份 | `/opt/conf.ini` 的 `dev:id` 提供 T-Box ID，缺失时使用源码默认值；`/opt/machine_vin` 可在运行期间更新 VIN。 |
| 本机服务 | CAN 三路 TCP 为 `127.0.0.1:16002`、`:16003`、`:16004`，GPS TCP 为 `127.0.0.1:16005`；命令 TCP 为 `127.0.0.1:16008`。接收线程要先连通前四路才进入读循环；服务端由设备环境提供。 |
| 状态文件和日志 | `/tmp/acc_stat` 为 ACC；`/tmp/di_in_07`、`/tmp/di_in_08`、`/tmp/di_pwm_09` 为反馈；`/tmp/do_06` 为控制输出；`/tmp/wifi_state` 记录站端连接状态。日志目录为 `/media/sdcard/bs_client_log/`，初始日志名以 `truck_hdz-` 开头。`bs_client [days]` 的可选参数指定启动时清理多少天前的日志，默认 2 天，大于 7 时也按 2 天处理。 |
| 启动脚本 | `scripts/start_bs.sh` 配置 `bridge0:0`，从 `/opt/bs_client` 启动进程，并约每 20 秒检查进程及接口地址。 |
| 构建 | 默认 `make` 走 EC200A 配置，版本号 `1.32`，定义 `USE_MACHINE_VIN`、`EC200A_ENABLE` 并链接 SDK、`pthread`、C++ 运行库及 `libcrypto`；EG25 入口为 `make -f Makefile-EG25`，同样写入 Git 提交与 `1.32` 版本号。EG25 的源码列表未包含 `src/fault/`，而 `main.c` 引用了 `fault_handle.h` 与 `fault_handle()`，使用前应核对构建依赖。 |

## 修改与验证提示

- 修改车型行为时，同时核对 `main.c`、`src/app_mng/`、`src/can_server/`、`src/batt_stand_client/` 和 `src/fault/`，并用目标设备的 `vehicle_type` 配置检查实际分支。分支名称不改变运行配置。
- 修改站端协议时核对 `batt_message_pack.c`、`batt_message_parse.c` 和 `batt_stand_client.c`；修改本机控制数据时核对 `src/cmd/`、状态字段结构和设备侧接收方；修改 DI/DO 时在实车或台架验证电平极性和动作。
- 站端协议外层帧以 `0x23 0x23` 开始，包含命令、应答标志、17 字节 VIN、加密标志、长度、数据和异或 BCC；登录、心跳、自定义命令分别使用 `0x01`、`0x07`、`0x81`。参数设置与属性查询在鉴权成功后才会排队回复；登录、鉴权、心跳尝试间隔均为 5 秒。超过约 60 秒没有有效站端响应会断线重连，登录失败后约 120 秒才再次尝试。
- 当前站端接收每次最多读 1440 字节并立即交给解析器，未见跨 `read()` 保存不完整帧的机制；TCP 分包需要联调验证。全量属性查询 `_propNumber=255` 分支传入的是请求中的 `_propListLen` 而非计算后的属性数量，结果应在联调中核对。自卸车 TP VIN 拼接未检查三个分包是否属于同一轮且顺序完整；遇到乱序或旧缓存时需用原始 CAN 帧核查。
- 自卸车 `waiting_acc_key_on()` 在 `/tmp/acc_stat` 打开失败后仍继续对无效文件描述符执行读取，初始缓冲首字节不是字符 `0`，可能把缺失文件判成 ACC 开启。`do_ctrl()` 只有首次调用或当前整秒时间大于上次写入时间加 1 时才写 `/tmp/do_06`；快速连续控制请求和设备文件异常需在台架核验。站端断开时登录、鉴权状态重置，但换电唤醒和换电状态有意保留；主循环用最近一次在线时间与 1800 秒比较后清除唤醒请求，若从未在线，静态初值为 0，不能简单理解为断线后必然等待完整 1800 秒。
- 当前仓库未见专用自动化测试目标。可先在匹配的 SDK 和工具链下编译，再在具备站端、本机 CAN/命令服务及硬件反馈的设备上验证登录、鉴权、心跳、VIN、解锁/锁止、状态写回和故障上报。主机侧编译不能证明硬件控制结果。
