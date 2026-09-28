# electric_station_truck_cn 项目说明

## 项目位置与用途

- Git 仓库与源码目录：`/home/tronlong/lyp/code/electric_station_truck_cn/`；本文档存放目录：`/home/tronlong/lyp/perCode/idevelop_md/electric_station_truck_cn/heavy_truck_20241021/`。
- 编写本文档时，仓库当前分支为 `feat/heavy_truck_20241021`，提交短哈希为 `d67a4ac`。分支属于整个 Git 仓库，并非某个目录独有；切换分支或更新提交后，应重新核对文档与源码。
- 本项目生成设备端程序 `bs_client`，服务于国内重卡换电。它通过 TCP 与换电站连接，处理车辆登录、鉴权、心跳、站端参数及状态交互；从本机 CAN 服务读取整车和电池数据，并把换电控制状态交给本机命令服务。源码同时包含 `dump_truck` 自卸车逻辑，实际行为还取决于设备配置中的 `vehicle_type`。`heavy_truck` 是默认车型值，不能仅凭 Git 分支名判断设备实际车型。
- 根目录的 `Makefile` 指向 `Makefile-EC200A`；另有 `Makefile-EG25`。当前分支采用 Make 构建，不是 CMake 项目。构建依赖对应移远模块 SDK、交叉编译环境及目标设备服务。

### 相关技术与名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| Git 分支、提交哈希 | 分支 `feat/heavy_truck_20241021` 指向当前开发线；`d67a4ac` 标识本文核对时的源码版本。分支可以继续移动，排查差异时应同时记录提交哈希。 |
| EC200A、EG25、交叉编译 | 两套移远模块平台的 Makefile 分别使用相应 SDK 和编译选项。交叉编译是在开发主机上使用目标平台工具链生成设备可执行文件；仅凭普通主机编译不能验证设备行为。 |
| T-Box、`bs_client`、换电站 | T-Box 是车载通信终端；`bs_client` 是本仓库生成、运行在设备端的进程；换电站是它经站端 TCP 地址连接的系统。 |
| TCP socket、`bridge0:0` | 程序通过 TCP socket 连接站端；连接前要求设备网络接口 `bridge0:0` 的地址为 `192.168.100.1`。启动脚本也会配置该地址。 |
| CAN、VCU、BMS、BCC1 | CAN 是车内控制器通信总线；VCU 是整车控制器；BMS 是电池管理系统。`BCC1` 是源码中换电控制状态及 CAN 信号字段的命名；各字段的工程含义以对应车辆协议为准。 |
| VIN、ICCID | VIN 是 17 字节车辆识别码，可从 `/opt/machine_vin`、站端配置或 CAN 请求取得；ICCID 是 SIM 卡标识。**当前 `src/common/common.c` 中真实 SIM 接口实现被整段注释，`get_iccid()` 实际总是返回固定占位值 `12345678901234567890`，包括 EC200A 编译分支。**不能把这个值当作设备真实 ICCID。 |
| DI、DO、锁销反馈 | DI 是数字输入；源码读取锁销、气缸和连接器反馈文件。DO 是数字输出，`/tmp/do_06` 用于提交控制电平。反馈电平在 `main.c` 中经过反向处理，具体接线应以目标硬件为准。 |
| 登录、鉴权随机数、AES-128 | 站端交互包含车辆登录、请求随机数及加密应答。活动代码在 `batt_message_pack.c` 中使用 OpenSSL 的 AES-128 ECB 接口与 PKCS#7 填充；`src/aes128/` 也被 Makefile 编译，但这条鉴权路径没有调用它。密钥目前是源码固定的 16 字节值。 |
| INI、`iniparser` | INI 是按节和键存储配置的文本格式。`src/iniparser/` 提供解析能力，程序读取并写入 `/opt/electic_station.ini` 的部分配置项。文件名 `electic` 是源码原样拼写。 |
| BCC、J1939 DM1、SPN/FMI | 站端帧末的 BCC 是异或校验字节，与 `BCC1` 状态字段不是同一个概念。DM1 是 J1939 故障报文；SPN/FMI 分别标识故障参数与故障模式，`src/fault/` 把本机故障映射为相应报文。 |
| `io_mng`、本机命令服务 | CAN 数据由设备上的 `io_mng` 类本机服务提供，程序从本机 TCP 端口读取；换电状态通过另一个本机 TCP 端口写出。服务端的部署与协议需要结合目标设备检查。 |

## 主要文件与职责

| 路径 | 职责 |
| --- | --- |
| `Makefile`、`Makefile-EC200A`、`Makefile-EG25` | 选择构建平台，编译 C 源码与 C++ 日志模块，链接 `bs_client`。EC200A 版本还包含 `src/fault/`。 |
| `main.c` | 单实例检查、日志初始化、信号处理、线程启动与约 500 毫秒的周期任务；处理锁销反馈、VIN、故障和控制状态。 |
| `src/app_mng/` | 初始化共享状态，读取车型、VIN、T-Box ID、ICCID 和站端地址。 |
| `src/batt_stand_client/` | 站端 TCP 连接、登录、鉴权、心跳、消息打包解析及收发队列。 |
| `src/can_server/` | 连接本机 CAN 数据服务，解析车辆和电池数据，处理 VIN 请求及故障报文。 |
| `src/cmd/` | 将换电控制状态写入本机命令服务。 |
| `src/fault/` | 判断锁销与连接器故障，构造 J1939 DM1 故障报文；当前 EC200A Makefile 纳入构建。 |
| `src/skt_res/`、`src/net_utils/`、`src/common/` | TCP 连接、网络地址、配置与设备信息等辅助功能。 |
| `src/iniparser/`、`src/aes128/`、`src/list/`、`src/log/` | INI 解析、AES 实现、发送队列和日志。 |
| `ini/`、`scripts/start_bs.sh` | 站端配置示例和设备端启动、保活脚本。 |

## 启动与数据流

1. `main()` 检查是否已有同名实例，初始化日志并清理旧日志；随后从设备配置读取 T-Box ID、车型、VIN 和站端地址，并调用 `get_iccid()` 填充 ICCID。当前该函数返回固定占位值；初始化失败时不启动后续任务。
2. 主进程启动站端接收、站端发送及 CAN 接收三个线程；主线程持续执行约 500 毫秒一次的周期任务。
3. 站端接收线程检查 `bridge0:0` 地址，连接配置的站端 TCP 地址。连接后解析站端报文；发送线程按状态依次尝试登录、请求鉴权随机数、发送加密应答，再发送心跳及排队消息。成功鉴权是心跳、参数应答和属性应答的前提。登录、鉴权与心跳的间隔常量均为 5 秒；约 60 秒无有效站端响应则断开重连，登录失败后约 120 秒才再次尝试。
4. CAN 线程通过本机 TCP 服务接收三路 CAN 数据并更新共享状态。周期任务检查 CAN/BMS 超时、处理 VIN 请求、锁销与连接器反馈，并向本机命令服务写入换电状态。命令在状态变化或约 5 秒间隔后发送，`Car_Type` 固定写为 `CHINA_CAR`。
5. 站端参数设置中的换电状态 `0x02`、`0x04` 分别触发解锁、锁止流程；程序等待锁销反馈后答复，超过约 10 秒则答复超时。其他有效状态通常直接答复。站端属性查询触发车辆状态上报；非全量属性查询还会设置唤醒请求。
6. 对重卡车型，周期任务处理故障上报；对自卸车车型，钥匙电状态取自 `/tmp/acc_stat`。`SIGUSR1` 和 `SIGUSR2` 分别设置源码中的解锁与锁止状态。

### 控制与反馈的边界

- 旧电池底托从 `/tmp/di_in_07`、`/tmp/di_in_08`、`/tmp/di_pwm_09` 读取反馈并反向换算锁销和连接器状态；收到 CAN ID `0x98FFD1EF` 后，源码把车辆标为新电池底托，锁销状态改由该 CAN 报文更新，周期任务不再调用上述 DI 状态更新函数。
- 站端断开后会清除登录、鉴权状态，并把 `BCC1_VCUControl` 置为无效值；部分换电状态被有意保留。程序在满足解锁或唤醒、车辆 Ready、手刹释放、站端断连等条件时设置锁止状态；唤醒请求在离线超过约 1800 秒后清除。实际锁止动作由本机其他进程和控制器完成，应在目标硬件上核对。

### 站端协议要点

- 外层帧以 `0x23 0x23` 开始，含命令类型、应答标志、17 字节 VIN、加密标志、两字节数据域长度，末尾为异或 BCC。当前代码对登录 `0x01`、心跳 `0x07`、自定义命令 `0x81` 分类处理；自定义扩展报文的客户代号为 `0x34`。
- 自定义消息 ID 涵盖随机数请求与应答、加密应答、参数设置及属性查询。参数设置中的 `ExchangeState` 是站端下发的运行状态字段，不等同于 INI 配置键；属性查询从当前 CAN、BMS 与换电状态组织回复。
- `batt_stand_message_process()` 每次从 TCP 连接读取最多 1440 字节，再交给解析器。当前实现没有跨 `read()` 调用保留未满一帧的数据；TCP 分包或一次读到多帧时，应通过实际站端联调验证解析结果，不能仅凭一次主机侧构建判断协议收发可靠。

## 配置与通信约定

| 项目 | 当前源码约定 |
| --- | --- |
| `/opt/electic_station.ini` | `station:server_ip`、`station:server_port`、`station:vehicle_type`、`station:vin`、`station:enable_vin_request`。程序会写回站端地址，成功取得 CAN VIN 时也会写回 VIN。默认车型为 `heavy_truck`。 |
| `/opt/conf.ini` | `dev:id` 提供 T-Box ID；缺失时源码使用默认值。 |
| `/opt/machine_vin` | 两套 Makefile 均定义 `USE_MACHINE_VIN`。重卡启动时优先读取；若不存在则尝试站端 INI 中的 VIN，然后由配置决定是否发起 CAN VIN 请求。周期任务还会在未取得机器 VIN 时约每 15 秒重试。 |
| 站端地址 | 默认 `192.168.100.101:7701`，可通过站端 INI 配置覆盖。连接前要求 `bridge0:0` 为 `192.168.100.1`。 |
| 本机 CAN 服务 | TCP 客户端连接 `127.0.0.1:16002`、`:16003`、`:16004`，分别对应三路 CAN。实际服务需由设备上的其他进程提供。 |
| 本机命令服务 | TCP 客户端连接 `127.0.0.1:16008`，写入 `cmd_item_t` 换电控制数据。 |
| 设备临时文件 | `/tmp/di_in_07`、`/tmp/di_in_08`、`/tmp/di_pwm_09` 用于反馈；`/tmp/do_06` 用于控制；`/tmp/wifi_state` 是程序记录的站端连接状态，不单独证明无线链路健康。 |
| 日志与单实例文件 | 日志目录为 `/media/sdcard/bs_client_log/`，日志文件名以 `truck_hdz-` 开头；`bs_client [days]` 的可选参数控制启动时清理多少天前的日志，默认 2 天，大于 7 也按 2 天处理。单实例检查使用 `/tmp/` 下的进程锁文件。 |
| 启动脚本 | `scripts/start_bs.sh` 在设备上配置 `bridge0:0`，先结束已有 `bs_client`，再从 `/opt/bs_client` 启动；每约 20 秒检查进程并在退出时重启，同时检查接口地址。 |

## 构建、部署与验证

- 默认 `make` 使用 `Makefile-EC200A`，目标名为 `bs_client`；EC200A 配置定义 `EC200A_ENABLE`、`USE_MACHINE_VIN`，版本号为 `1.29`。`make -f Makefile-EG25` 使用 EG25 配置，版本号为 `1.28`。两者均将 Git 提交哈希编入版本日志。
- Makefile 依赖移远 SDK 的编译变量、头文件和库，以及 pthread、C++ 运行库和 OpenSSL `libcrypto`。EC200A Makefile 以可选方式包含 `../../sdk.mk`，而当前检出位置没有该文件；`make -n bs_client` 在此环境中显示回退为主机 `cc`/`g++`，因此不能将它视为目标平台编译验证。`Makefile-EG25` 未列入 `src/fault/` 的头文件目录和源码，但 `main.c` 引用了 `fault_handle.h` 与 `fault_handle()`，使用该构建入口前须先核对并修正依赖。
- 部署时需把可执行文件和相应配置放到设备 `/opt/`，并确保站端网络、三路 CAN 本机服务和命令服务可用；若站端要求真实 ICCID，还须先修正当前占位值实现。配置样例位于 `ini/`；`electic_station_dump_truck.ini` 把车型设为 `dump_truck`。
- 修改站端协议时联查消息打包、解析和收发逻辑；修改车型或 CAN 信号时联查 `can_server.c`、`can_frame_pack.c`、`app_mng.h` 和 `main.c`；修改锁止相关行为时还需在目标硬件上核验反馈与动作。
- 本仓库未见专用自动化测试目标。可先在对应 SDK 环境编译，再在具备站端与本机服务的设备上检查登录、鉴权、心跳、断线重连、VIN、状态上报及控制动作。特别核对真实 ICCID 的来源、鉴权密钥配置和站端 TCP 分包行为；当前源码包含固定 ICCID 占位值和固定 AES 密钥。
