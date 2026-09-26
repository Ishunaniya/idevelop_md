# bs_client_port_crane 项目说明

## 项目位置与用途

- Git 仓库根目录：`/home/tronlong/lyp/code/rtms_sdk`；项目源码目录：`apps/bs_client_port_crane/`；本文档目录：`/home/tronlong/lyp/perCode/idevelop_md/rtms_sdk/bs_client_port_crane/`。项目是 RTMS SDK 仓库中的一个应用目录，没有独立的 Git 仓库或分支。
- 本文依据仓库分支 `develop/rtms_sdk_v1.3_20240408`、提交 `bc60961e` 的检出源码编写（由 `git branch --show-current`、`git rev-parse --short HEAD` 核对）。分支名属于整个仓库；切换分支或更新提交后，应重新核对构建入口、配置和运行流程。
- 本目录定义设备侧 `bs_client` 程序，面向港机换电场景：从本机 CAN 服务取得车辆和电池状态，经 Wi-Fi 网络与换电站控制端建立 TCP 连接，发送心跳与车辆信息，解析站端的换电状态指令，再向本机命令服务发布换电控制状态。`PORT_CRANE` 是发给命令服务的车型标识。
- 当前 `apps/CMakeLists.txt` 中的 `add_subdirectory(bs_client_port_crane)` 被注释，顶层常规构建不会生成此目标。本目录有独立 `CMakeLists.txt`，可作为单独目标配置；纳入顶层构建前需确认平台、依赖和安装方式。
- `main.c` 中 `app_msg_task` 的线程创建行也被注释。因此 `src/app_server/` 虽实现了 GPS 订阅和 MCU 请求，当前入口不会启动它们；不要把它们写成当前程序运行时的数据来源。

### 相关技术与名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| RTMS SDK、Git 分支、提交 | `rtms_sdk` 是应用所属仓库；分支是仓库代码的开发线，提交哈希定位本文核对的版本。`apps/bs_client_port_crane` 只是仓库子目录，不单独切换分支。 |
| CMake、交叉编译、`QL_MODULE_PLATFORM` | CMake 读取 `CMakeLists.txt` 生成构建规则；交叉编译指在开发主机上用目标设备工具链编译。环境变量 `QL_MODULE_PLATFORM` 在交叉编译时选择 EC200A、EG25G 或 MCIMX6Y2CVM08AB 对应的宏、头文件和库。 |
| `bs_client`、T-Box、港机、换电站 | `bs_client` 是生成的设备进程；T-Box 是车载通信终端；港机是本目录代码中的车型用途；换电站控制端是程序通过 TCP 连接的站端。程序本身不实现站端服务。 |
| Wi-Fi STA、SSID、DHCP、`wlan0` | STA 指 Wi-Fi 客户端模式，SSID 是所连接网络的名称；DHCP 给接口分配地址。程序在建连前检查 `wlan0` 有无 IP，并从站端配置读取 SSID/密码；`station.script` 是供 `udhcpc` 调用的 DHCP 地址配置脚本。SSID/密码来自设备配置，需以实际部署值为准。 |
| TCP socket、心跳、握手 | TCP socket 用于到换电站的长连接。程序定时发送心跳；从心跳答复取得站端识别的盒子 ID，并与本机 ID 或 VIN 比较，维护本地握手状态。TCP 建连成功不等于握手通过。 |
| CAN、CAN0/1/2、VCU、BCC1 | CAN 是车载控制器总线；程序通过本机消息服务订阅三路 CAN。VCU 指整车控制器；`BCC1` 是源码中换电控制状态与相关 CAN 信号的命名前缀，具体物理含义应按车辆协议核对。 |
| BMS、SOC、VIN、T-Box ID | BMS 是电池管理系统；SOC 是电池荷电状态；VIN 是 17 字节车辆识别码。T-Box ID 从 `/opt/conf.ini` 读取，用于与站端返回的标识比对；VIN 可从站端配置或分段 CAN 帧获取，两者不能视为同一个字段。 |
| nanomsg PUB/SUB | 本机进程通信模式。`bs_client` 用 SUB 接收 CAN 发布数据，用 PUB 向本机命令服务发布换电状态。环回地址 `127.0.0.1` 表示本机，不是换电站 IP。 |
| nanomsg REQ/REP、GPS、MCU | REQ/REP 是请求与应答模式。`src/app_server/` 预备通过 REQ 请求 MCU 状态，并通过 SUB 接收 GPS；由于入口未启动该线程，这两条路径在当前版本未运行。MCU 是微控制器，GPS 是定位数据来源。 |
| INI、JSON、cJSON | `/opt/electic_station.ini`、`/opt/conf.ini` 使用 INI 键值配置；`/etc/config/config.json` 使用 JSON 保存 Wi-Fi 参数，代码由 cJSON 解析和修改。`electic` 是现有文件名拼写。 |
| BCC 校验、大端序 | 站端协议用固定帧头、长度、命令、VIN、数据体和异或 BCC 组成报文；多字节数字打包时调用大端转换。当前接收解析函数中的 BCC 校验代码被注释，不能据此认定入站报文已完成校验。 |
| CPActive、保活 | 程序通过 `appmng` 库注册进程信息，循环刷新访问时间；这是本机进程存活监测，不是换电站 TCP 心跳。 |

## 主要文件与职责

| 路径 | 职责 |
| --- | --- |
| `CMakeLists.txt`、`global_config.h.in` | 定义 `bs_client` 版本 1.0、依赖、平台选择、生成头文件和安装规则。 |
| `main.c` | 单实例检查、信号处理、初始化、CPActive 保活、站端收发与 CAN 线程、100 毫秒周期控制线程。 |
| `src/app_mng/` | 分配共享状态和发送队列，读取 T-Box ID、VIN、站端地址与 Wi-Fi 配置；必要时更新系统 Wi-Fi JSON 并重启。 |
| `src/batt_stand_client/` | TCP 建连与重连、心跳和车辆信息打包、发送队列、站端报文解析及握手状态。 |
| `src/can_server/` | 订阅三路 CAN，解析车辆、电池和 VIN 帧，更新超时状态；`can_frame_pack.c` 中还有 CAN 指令打包函数，需按调用关系判断是否运行。 |
| `src/cmd/` | 将唤醒请求、换电状态和 `PORT_CRANE` 车型标识封装成命令，发布到本机命令服务。 |
| `src/app_server/` | GPS 与 MCU 消息处理的预备实现；当前 `main.c` 未启动 `app_msg_task`。 |
| `src/common/`、`src/iniparser/`、`src/net_utils/`、`src/skt_res/`、`src/list/` | 配置、时间、位操作、网络、TCP socket 和发送队列等辅助功能。 |
| `scripts/electic_station.ini`、`scripts/station.script` | 站端配置样例和 `udhcpc` 地址配置脚本；安装到目标前缀下的 `opt/`、`etc/`。 |

## 当前启动与数据流

1. `main()` 检查同名进程，注册 `SIGPIPE`、`SIGUSR1`、`SIGUSR2`，设置日志标签，再调用 `canapp_mng_init()`。该函数读取配置并初始化发送队列、CAN 与站端共享状态；Wi-Fi 参数与 `/etc/config/config.json` 不一致时，代码会写回该 JSON 并触发设备重启。
2. 程序以 30 秒超时注册 CPActive，启动保活线程，每 15 秒刷新一次。随后分别创建站端接收、站端发送、CAN 订阅和周期控制线程。`main()` 只保存最后一次 `pthread_create` 的线程 ID 并对其 `pthread_join`；源码没有逐一检查线程创建结果。
3. 站端接收线程先检查 `wlan0` 是否有 IP，再连接配置中的站端 IP/端口。连接成功后读取报文、解析心跳答复和换电指令；连接断开会清除 `CarInfo_Req`，等待约 3 秒后重试。未接收到数据超过约 20 次一秒 `select()` 超时后断开。握手失败时，实际运行的等待条件为约 10 秒，附近“3 分钟”注释不是当前执行值。
4. 站端发送线程在 TCP 已连接时以源码常量约 3 秒间隔入队心跳。只有 `canState == 1` 且站端已请求车辆信息，才以源码常量约 1 秒间隔入队车辆信息。发送线程每次从队列取出一项后调用 `send()`；TCP 写成功不代表站端已经处理报文。
5. CAN 线程订阅本机三个端口，解析车辆手刹/换电许可/锁销/连接器、里程、SOC、容量、电池编码和分段 VIN。周期线程每 100 毫秒检查 CAN/BMS 空闲计数，调用 `send_cmd_ctrl_period()`；CAN/BMS 超时阈值均为 50 次循环，约 5 秒。命令状态变化或间隔超过 5 秒时发布本机命令。
6. 周期线程还根据 `wlan0` 的 `operstate` 更新 `wifiState`，并在换电请求活跃、Wi-Fi 或站端车辆信息请求中断约 120 秒后清除唤醒请求。`SIGUSR1`、`SIGUSR2` 可直接修改换电状态与唤醒请求；这是软件状态变化，实际锁止动作须在设备上验证。

## 配置与通信接口

| 位置或接口 | 当前源码约定 |
| --- | --- |
| `/opt/electic_station.ini` | `station:server_ip`、`station:server_port` 默认 `192.168.250.1:7701`；`station:ssid`、`station:pass` 有源码默认值；`station:vin` 是初始 VIN，`station:enable_vin_request` 只控制是否接受 VIN CAN 分段。当前目录未找到主动发送 VIN 请求帧的调用。程序会写回站端 IP/端口，收到有效 VIN 后也会写回 VIN。样例文件开启 VIN 接收。 |
| `/opt/conf.ini` | `dev:id` 是本机 T-Box ID；缺失时使用源码中的默认 ID。配置中的真实 ID 应与站端绑定关系一致。 |
| `/etc/config/config.json`、`/tmp/bs-tmp.json` | `wpa_sta_init()` 比对 JSON 中的 Wi-Fi 模式、SSID 和密码；不一致时把更新后的 JSON 写到临时文件，再覆盖系统配置并执行 `reboot`。执行前需备份目标设备配置。 |
| `wlan0`、`/sys/class/net/wlan0/operstate` | 前者需在站端连接前取得 IP；后者供周期线程判断 Wi-Fi 接口状态。接口为 `up` 不等于站端 TCP 或握手健康。 |
| 换电站 TCP | 默认连接 `192.168.250.1:7701`，可由 INI 覆盖。协议帧头固定 `31 18 66`（十六进制）；心跳、车辆信息由本机发往站端，站端返回心跳答复、车辆信息请求及换电状态。 |
| `tcp://127.0.0.1:16002/16003/16004` | nanomsg SUB 分别订阅本机 CAN0、CAN1、CAN2 数据；当前目录未提供这些端口的绑定服务。 |
| `tcp://127.0.0.1:26008` | nanomsg PUB 向本机命令服务发布 `cmd_item_t`，包含唤醒请求、换电状态和车型标识。对应订阅/绑定方由其他进程提供。 |
| `tcp://127.0.0.1:16005`、`tcp://127.0.0.1:38000` | GPS SUB 与 MCU REQ 的代码接口；当前入口未启动，不能把端口可达性当作此进程已消费数据的证据。 |
| `scripts/station.script` | 给 `udhcpc` 处理 `deconfig`、`renew`、`bound` 事件；写入 `/etc/${interface}_gateway_ip`。脚本安装不等于该项目直接启动 `udhcpc`。 |

### 站端命令与数据

| 方向与命令码 | 当前实现 |
| --- | --- |
| 本机 → 站端 `0x0101` | 心跳，报文体包含单字节流水号；`batt_message_pack_heart_beat()` 虽接收 `canState` 参数，当前报文体未写入 CAN 状态。 |
| 站端 → 本机 `0x0202` | 心跳答复，读取帧中 17 字节标识并触发本地 ID/VIN 比对。 |
| 站端 → 本机 `0x0303` | 请求车辆信息，设置 `CarInfo_Req` 和唤醒请求；发送线程仍需等待 CAN 状态正常。 |
| 本机 → 站端 `0x0505` | 车辆信息，打包手刹、换电许可、锁销、里程、SOC、额定容量、累计充放电量、电池编码和连接器状态。 |
| 站端 → 本机 `0x0606` | 用数据域第一个字节表示换电开始、解锁、换电中、锁止、失败或成功，并更新共享状态。 |

`batt_message_pack.h` 还声明了 `0x0404`、`0x0707`～`0x0A0A` 等命令，但当前接收解析的 `switch` 没有处理它们；不要把头文件中的枚举当作完整协议支持清单。

### CAN 输入与状态

| CAN ID（源码原值） | 提取的数据与用途 |
| --- | --- |
| `0x98FFB0D8` | 手刹、允许换电、锁销和连接器状态；刷新 `canState`。 |
| `0x98FEF217` | 累计里程；刷新 `canState`。 |
| `0x98FFA1F3` | 电池 SOC；刷新 `bmsState`。 |
| `0x98E1EFF3` | 四帧、每帧 6 字节拼接 24 字节电池编码；刷新 `bmsState`。 |
| `0x98E2EFF3`、`0x98FFA9F3` | 额定容量、累计充电量和累计放电量；刷新 `bmsState`。 |
| `0x98E1F3EF` | 三段拼接 17 字节 VIN；仅在 `vin_req.done == 0` 且字符检查通过后更新本地 VIN 与 INI。 |

这些 ID 是代码中 `switch (frdup.can_id)` 的完整值；若本机 CAN 服务另行添加标志位或改写 ID，仍要核对 `can_frame_t` 的传输格式。`canState` 和 `bmsState` 分开维护，不能用其中一个状态代表全部 CAN 信号正常。

## 协议和维护时要核对的边界

- 站端接收解析只检查最短长度、固定帧头和命令类型；入站 BCC 校验被注释，也未见按 TCP 流重组完整帧的逻辑。TCP 可能拆包或合包，修改协议时应先核对长度、校验、分帧及异常报文处理，再做真实站端联调。
- 站端心跳答复带回的 17 字节标识与本机 T-Box ID 或 VIN 比对。`BCC1_RemoteTboxID` 的缓冲区长度正好是 17 字节，后续字符串比较须注意终止符与长度；不能只凭当前比较结果证明认证逻辑健全。
- 换电状态命令 `0x01`～`0x06` 对应开始、解锁、换电中、锁止、失败、成功。失败分支中 `if (p_batt_data->BCC1_LockPin_Sts = 0x2)` 使用赋值而不是比较，会把锁销状态写成 `0x2` 并固定进入第一个分支；分析故障状态时应计入该源码问题。
- `CAR_INFO_TIMEVAL` 的值为 1 秒，头文件旁的“5 秒”注释与代码不一致；握手失败等待的“3 分钟”注释也与执行条件不一致。时间参数应以可执行表达式为准。
- `src/can_server/can_frame_pack.c` 提供若干 CAN 打包函数，但当前 `main.c` 到 CAN 线程的路径主要是订阅与解析，不能仅因函数存在就断定会向 CAN 总线发送这些帧。改动控制命令时需联查 `src/cmd/`、外部命令服务和整车协议。
- `station:enable_vin_request=true` 只令 `vin_req.done=0`；虽然头文件定义了请求阶段，源码并未在当前执行路径中调度这些阶段，也未主动发出 VIN 请求帧。VIN 能否更新取决于外部 CAN 服务是否主动提供相应分段帧。
- 心跳答复识别成功时会设置 `hand_shake_flgs`，但发送线程只检查 TCP 文件描述符，不检查该标志；因此“握手通过”不是车辆信息发送的代码前置条件。站端协议若要求认证后再交互，应在联调时核对行为。
- `send()` 只在返回值恰好等于报文长度时释放队列项；返回正数但不足长度的部分发送没有处理剩余字节。排查站端偶发报文缺失时，应同时检查发送日志与 TCP 实际字节流。
- 本目录没有专用自动化测试目标。主机侧可检查 CMake 配置和编译；Wi-Fi 参数改写、站端握手、CAN 信号、换电控制及故障状态需要在隔离设备或联调环境按设备协议验证。

## 构建与部署

- 独立 CMake 工程要求 CMake 3.11、C99，目标和输出名均为 `bs_client`，版本 1.0。交叉编译分支支持 `EC200A`、`EG25G`、`MCIMX6Y2CVM08AB`；其他 `QL_MODULE_PLATFORM` 值会在该分支触发配置错误。
- 构建需 nanomsg、OpenSSL、cJSON、appmng，以及 cn-cbor、tbox-common、pthread、stdc++ 等链接依赖；平台还需相应 Quectel SDK 或设备库。工程安装程序至安装前缀下 `opt/`，安装配置样例至 `opt/`、DHCP 脚本至 `etc/`，并列出部分运行共享库。
- 当前顶层没有启用本目录；需要顶层产物时，应先把它纳入受控的 CMake 选项并核对目标平台依赖。部署前还需确认站端地址、T-Box ID、Wi-Fi 配置及外部 CAN/命令服务与目标设备一致。
