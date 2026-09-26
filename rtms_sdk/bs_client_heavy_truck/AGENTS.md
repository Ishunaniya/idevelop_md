# bs_client_heavy_truck 项目说明

## 项目位置与用途

- Git 仓库根目录：`/home/tronlong/lyp/code/rtms_sdk`；本项目源码目录：`apps/bs_client_heavy_truck/`；本文档目录：`/home/tronlong/lyp/perCode/idevelop_md/rtms_sdk/bs_client_heavy_truck/`。
- 编写本文档时，仓库当前分支为 `develop/rtms_sdk_v1.3_20240408`，提交短哈希为 `bc60961e`（由 `git branch --show-current` 和 `git rev-parse --short HEAD` 核对）。这是整个 `rtms_sdk` 仓库的分支，项目目录没有独立分支；切换分支后需重新核对本文内容。
- 本目录实现设备侧的 `bs_client` 进程：连接换电站控制端，处理车辆登录、鉴权、心跳、参数与属性命令；接收本机 CAN、MCU、GPS 和数字输入数据；再将换电状态、控制指令及故障信息发给本机相关进程。虽然目录名为 `heavy_truck`，源码也包含 `dump_truck`、`dump_truck_newc` 的分支逻辑，实际车型由配置决定。
- 当前 `apps/CMakeLists.txt` 未引用 `bs_client_heavy_truck`，所以从仓库根目录执行常规顶层构建不会自动生成它。目录内有独立 `CMakeLists.txt`；若要纳入 SDK 构建，须先确认目标平台及顶层集成方案。

### 相关技术与名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| RTMS SDK、CMake、交叉编译 | `rtms_sdk` 是应用和第三方库所在仓库。CMake 根据 `CMakeLists.txt` 生成构建规则；交叉编译指在开发主机上使用目标设备的工具链生成程序。 |
| `bs_client`、T-Box、换电站 | `bs_client` 是本目录生成的可执行文件。T-Box 是车载通信终端，本进程运行在其设备环境中；换电站是它通过 TCP 连接的站端系统。 |
| `BCC1`、VCU、BMS、MCU | `BCC1` 是源码中换电控制状态和 CAN 字段的命名；VCU 是整车控制器，BMS 是电池管理系统，MCU 是微控制器。它们的状态或命令经本机服务、CAN 消息和站端协议相互传递。具体信号含义以设备协议和对应结构体为准。 |
| CAN、VIN、ICCID | CAN 是车内控制器通信总线；VIN 是 17 字节车辆识别码；ICCID 是 SIM 卡标识。VIN 可来自设备文件、站端配置或 CAN 请求，ICCID 在初始化时读取。 |
| DI、DO、锁销反馈 | DI 是数字输入，读取锁销、气缸和连接器到位状态；DO 是数字输出，程序通过本机命令接口控制输出电平。不同平台使用的 DI/DO 通道在条件编译中有差异。 |
| nanomsg PUB/SUB、REQ/REP | 本机进程间通信模式。CAN 与 GPS 使用 SUB 接收发布数据，控制命令使用 PUB 发送；MCU 和 DI 使用 REQ 请求、接收服务端应答。消息按项目自定义结构或 JSON 格式处理。 |
| TCP socket、`bridge0:0` | 程序以 TCP 客户端连接换电站地址；连接前检查本机 `bridge0:0` 是否具有 `192.168.100.1` 地址，必要时调用 `ifconfig` 配置。它依赖目标设备的网络环境。 |
| 登录、鉴权随机数、AES-128 | 站端交互包含车辆登录及随机数请求、加密应答等步骤；`src/aes128/` 实现 AES 算法，报文打包和解析模块负责协议字段。不要将算法实现等同于已验证的整套安全协议。 |
| INI、`iniparser` | INI 是配置文件格式；`src/iniparser/` 和 `src/common/` 提供读取与写入配置值的功能。项目读取站端地址、车型、VIN、换电状态及设备 ID。 |
| `QL_MODULE_PLATFORM` | 交叉编译时选择平台依赖的环境变量。该目录的 CMake 识别 `EC200A`、`EG25G`、`MCIMX6Y2CVM08AB`、`AG35GL`；平台宏还影响 DI/DO 等实现。 |

## 主要文件与职责

| 路径 | 职责 |
| --- | --- |
| `CMakeLists.txt`、`global_config.h.in` | 定义版本、平台依赖、`bs_client` 可执行文件和安装规则；生成版本与 Git 哈希头文件。 |
| `main.c` | 单实例检查、日志初始化、状态对象初始化、线程启动及 500 毫秒周期任务；处理进程信号。 |
| `src/app_mng/` | 管理共享运行状态，读取设备 ID、车型、VIN、换电站地址和历史换电状态。 |
| `src/batt_stand_client/` | 站端 TCP 连接、收发队列、心跳、车辆登录、鉴权、报文打包与解析。 |
| `src/can_server/` | 订阅本机 CAN 数据、解析车辆与电池状态、请求 VIN、构造相关 CAN 发送数据。 |
| `src/app_server/` | 订阅并解析 MCU、GPS、DI 数据，维护相关状态和定时请求。 |
| `src/cmd/` | 将换电控制、DO 输出和连接状态等命令发布给本机命令服务。 |
| `src/fault/` | 换电相关故障处理。 |
| `src/aes128/`、`src/iniparser/` | 本地 AES 实现及 INI 配置解析。 |
| `src/common/`、`src/net_utils/`、`src/skt_res/`、`src/list/`、`src/log/` | 时间、配置、网络、socket、队列及日志等辅助功能。 |

## 启动与数据流

1. `main()` 检查同名实例，初始化日志、版本信息与状态；读取 `/opt/conf.ini` 和 `/opt/electic_station.ini` 等配置，并读取 SIM ICCID。初始化失败时退出。
2. 主进程分别启动换电站接收、站端发送、CAN 接收和本机应用消息线程；主线程运行周期任务。
3. 站端线程检查 `bridge0:0` 地址后连接配置的站端 TCP 地址；连接成功后处理站端报文，断开时清除部分控制状态并重连。发送线程负责发送队列及周期性登录、鉴权、心跳等消息。
4. CAN 线程订阅本机 CAN 消息；应用消息线程订阅 GPS，并定时通过 REQ/REP 接口请求 MCU、DI 数据。解析结果写入共享状态，用于站端应答、车辆状态和控制决策。
5. 周期任务每约 500 毫秒更新 DI/换电状态，检查 CAN、BMS 数据超时，发布本机控制命令，并处理 VIN 请求与换电异常场景。信号 `SIGUSR1`、`SIGUSR2` 分别触发源码中的解锁、锁止状态设置。

## 关键配置与通信地址

| 项目 | 源码中的约定 |
| --- | --- |
| `/opt/electic_station.ini` | 读取 `station:vehicle_type`、`station:vin`、`station:enable_vin_request`、`station:exchange_state`、`station:server_ip`、`station:server_port`；程序会写回部分值。文件名中的 `electic` 为源码原样拼写。 |
| `/opt/conf.ini` | 读取 `dev:id` 作为本机 T-Box ID。 |
| `/opt/machine_vin` | 定义于 `VIN_FILE`；启用 `USE_MACHINE_VIN` 时优先读取。 |
| 站端默认地址 | `192.168.100.101:7701`；可由上述站端 INI 配置覆盖。`batt_stand_client.h` 中另有 `192.168.250.1:7701` 常量，但当前连接函数使用配置值。 |
| 本机网络 | `bridge0:0` 预期为 `192.168.100.1`；连接站端前会检查。`/tmp/wifi_state` 记录代码判断的站端连接状态。 |
| 本机 CAN 订阅 | nanomsg SUB 连接 `127.0.0.1` 的 `16002`、`16003`、`16004`，分别对应 CAN0、CAN1、CAN2。 |
| GPS、MCU、DI 数据 | GPS 以 nanomsg SUB 连接 `tcp://127.0.0.1:16005`；MCU、DI 各以 nanomsg REQ 连接 `tcp://127.0.0.1:38000`，通过定时请求取得 JSON 状态。 |
| 本机控制发布 | nanomsg PUB 连接 `tcp://127.0.0.1:26008`；CAN 相关发送还使用 `tcp://127.0.0.1:26002`。对应的绑定方需由本机其他进程提供。 |
| 日志保留天数 | `bs_client [days]` 的可选参数；不传或数值大于 7 时使用 2 天，并在启动时清理旧日志。 |

实际网络接口、IP、端口、配置内容和外部服务须以目标设备为准；表格记录的是当前源码约定，不表示所有设备都采用这些值。

站端接收循环使用 1 秒 `select()` 等待；成功解析的报文会更新最后响应时间，超过约 60 秒没有有效站端响应则断开并进入重连流程。登录失败时，代码等待约 120 秒后才再次尝试。`wifiState` 和 `/tmp/wifi_state` 表示程序判断的站端连接状态，不能单独证明无线网络链路健康。

## 构建与部署

- 本目录 CMake 最低版本为 3.11，项目版本为 2.1，C 标准为 C99。构建目标名是 `bs_client`，源码由 `main.c` 和 `src/*/` 下的 C/C++ 文件组成；安装规则将可执行文件放到安装前缀下的 `opt/`。
- CMake 查找 nanomsg 1.2、OpenSSL 1.1.1、tbox-common 1.0、cJSON 1.7.15、appmng 1.0，并链接 cn-cbor、pthread、stdc++ 等库；不同平台还需对应 Quectel SDK 或设备库。安装规则同时列出若干运行所需共享库。
- 在交叉编译模式下，须先加载对应 `cross-profiles/` 环境，确保 `QL_MODULE_PLATFORM`、SDK 路径和 sysroot 与工具链一致。`EC200A`、`EG25G`、`MCIMX6Y2CVM08AB`、`AG35GL` 是此目录当前明确处理的平台值；其他值会触发 CMake 错误。
- 当前顶层应用清单未包含本目录。若要通过 SDK 顶层构建，需先添加受控的 `add_subdirectory(bs_client_heavy_truck)` 条件，并检查第三方依赖与目标平台；不能仅凭顶层 README 的通用命令断定这个目标已经参与构建。

## 修改与验证时关注

- 修改站端协议字段时，同时核对 `batt_message_pack.c/.h`、`batt_message_parse.c/.h` 和连接收发逻辑；消息 ID、应答标志及 VIN 长度在头文件中定义。
- 修改 CAN 信号或车型行为时，联查 `can_server.c`、`can_frame_pack.c`、`app_mng.h` 和 `main.c`。`heavy_truck`、`dump_truck`、`dump_truck_newc` 走不同条件分支。
- 修改 DI/DO 映射时，联查 `main.c`、`src/app_server/di.*`、`src/cmd/` 中的平台条件编译，并在对应硬件上核验电平与锁止状态。锁止控制属于设备动作，不能仅凭主机侧编译判断行为正确。
- 修改配置键、默认站端地址或网络接口时，联查 `app_mng.c/.h` 与 `batt_stand_client.c`；程序会写回站端配置，验证前应备份设备配置。
- 本目录未见专用自动化测试目标。可先验证 CMake 配置与编译，再在具备站端、CAN、MCU、DI 和本机 nanomsg 服务的目标环境核对登录、断线重连、状态上报及控制动作。
