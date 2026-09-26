# gb32960_client_for_dima 项目说明

## 项目位置与用途

- Git 仓库根目录：`/home/tronlong/lyp/code/rtms_sdk`；本项目源码：`apps/gb32960_client_for_dima/`；本文档目标目录：`/home/tronlong/lyp/perCode/idevelop_md/rtms_sdk/Dima/gb32960_client_for_dima/`。本项目是 `rtms_sdk` 仓库中的一个应用目录，不是独立 Git 仓库。
- 整理本文档时，仓库分支为 `develop/rtms_sdk_v1.3_20240408`，提交短哈希为 `bc60961e`。该分支名和提交号属于整个 SDK 仓库；切换分支、更新代码或调整构建选项后，应重新核对本文所述行为。
- 本目录构建 `gb32960_client`（CMake 项目版本 1.1）及共享库 `libscene.so`。程序接收本机 CAN、GPS、MCU 信息，按代码中的 GB 32960 相关帧结构组织车辆登录、实时数据、补发数据和登出，通过 TCP 连接配置的平台；离线时将待补发报文写入 SD 卡目录。
- 目录名包含 `for_dima`，但 CMake 同时提供 `GB32960_DEVICE_DIMA` 与 `GB32960_DEVICE_YUANJIN` 选项。Dima 分支有具体车辆 CAN 解析；远锦分支的 `parse_data_msg()` 当前仅有 TODO。构建时必须至少选择一个设备选项；两者同时为 ON 时 CMake 优先定义 Dima 宏。实际车辆信号和协议适用性须结合选项及目标设备核对。
- `apps/CMakeLists.txt` 中的 `WITH_GB32960_CLIENT_FOR_DIMA` 默认 OFF；只有打开后本目录才参与 SDK 顶层构建。本程序依赖设备上的 CAN/GPS/MCU 本机服务、网络和 `/opt`、`/media/sdcard` 配置及存储路径。

### 相关技术与名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| RTMS SDK、Git 分支、CMake、交叉编译 | `rtms_sdk` 是上级源码仓库；Git 分支标识本次文档对应的代码线，提交哈希定位具体快照。CMake 通过顶层与本目录的 `CMakeLists.txt` 生成构建规则；交叉编译是在开发机上用目标设备工具链生成程序。 |
| GB 32960、T-Box、`gb32960_client` | GB 32960 是项目采用的电动汽车远程服务与管理系统通信协议系列名称；源码中的 `gb4_*` 是现有函数命名，不应理解为另一项单独协议。T-Box 指车载通信终端环境；`gb32960_client` 是在其中采集、组包并上报的进程。实际符合哪一版标准，应以部署协议和测试结果确认。 |
| Dima、远锦、编译宏 | 两套设备侧 CAN 信号适配入口。`GB32960_DEVICE_DIMA` 定义 Dima 解析代码，`GB32960_DEVICE_YUANJIN` 定义远锦入口；远锦的实际数据解析当前未完成。 |
| CAN、CAN ID、BMS | CAN 是车辆控制器局域网；CAN ID 标识总线报文，代码从三个本机 CAN 通道订阅并解析。BMS 是电池管理系统，提供电芯电压、温度、荷电状态和告警等数据。不同车型的 ID 与信号缩放须按对应代码和点表核对。 |
| VIN、ICCID | VIN 是 17 位车辆识别代号，用于报文标识；启动时从 `/opt/machine_vin` 读取，Dima CAN 解析还可能更新该文件。ICCID 是 SIM 卡识别号；程序先读本地配置，再尝试模组接口，仍为空时使用源码中的占位字符串 `123456789`，不能将占位值当作有效 SIM 信息。 |
| GPS、MCU、ACC | GPS 提供定位；MCU 是微控制器，本进程定时向其服务请求状态。ACC 指车辆点火/附件电源状态；上报状态机在 ACC 开启，或充电且 CAN 活跃时尝试连接和登录。 |
| SOC、DCDC、绝缘电阻 | SOC 是电池荷电状态；DCDC 是车载直流电压转换器；绝缘电阻是高压系统状态量。这些值由 CAN 数据及点表规则转换，报文中的无效值与缩放需要结合代码检查。 |
| nanomsg、PUB/SUB、REQ/REP | nanomsg 是设备内部进程通信库。程序通过 SUB 订阅 CAN 和 GPS，通过 REQ/REP 向 MCU 请求状态；`127.0.0.1` 地址仅是设备本机接口，不是远端上报平台。 |
| TCP、域名、ACK | 远端上报使用 TCP socket，地址从 `/opt/parameter_conf.ini` 读取。ACK 指平台应答；源码部分流程只检查收到字节或本地 `send()` 结果，不能由此直接判定平台已按协议接受全部报文。 |
| 实时上报、历史补传、BCC | 实时帧按周期发送；离线时保存为文件，恢复连接后将命令字改成补发类型并重新计算 BCC。BCC 是对指定报文字节做异或的校验值。历史文件删除时机与平台确认存在差距，见下文。 |
| AES128、加密标志 | AES128 是对称加密算法。源码保留 AES128 密钥常量与打包函数，但当前登录、登出和实时上报调用的是普通组帧函数，其帧头加密标志为 `0x01`（不加密）；不能仅凭 AES 函数存在就认为线上报文已加密。 |
| 点表、JSON、cJSON、CBOR | 点表是 `/opt/Pointsheet_info1_gb32960.json` 的信号规则配置；cJSON 用于读取规则和本机 JSON 数据。`src/data_process/` 还实现 CBOR 数据和模型结构编码；CBOR 是紧凑二进制数据格式，不能把这些辅助编码函数等同于 GB 32960 主上报链路。 |
| INI、`iniparser`、`libscene.so` | INI 文件保存平台和登录流水号等配置，`iniparser` 负责读写。`libscene.so` 是本目录编译的场景共享库；当前 `can_mng_init()` 中加载场景库的调用被注释，需核对实际调用路径后再判断运行时是否使用。 |

## 主要文件与职责

| 路径 | 职责 |
| --- | --- |
| `CMakeLists.txt`、`global_config.h.in` | 定义版本、设备宏、平台依赖、`gb32960_client` 与 `scene` 共享库及安装规则。 |
| `main.c` | 初始化管理对象、平台信息、VIN/ICCID、点表和进程保活，启动 CAN、本机消息、TCP 上报、历史补传和历史保存线程。 |
| `src/can_mng/`、`src/can_server/` | 管理车辆共享状态，订阅 CAN0/1/2 并判断 CAN 活跃度；活跃度实现位于 `src/can_server/can_active.c`。 |
| `src/data_process/data_process.*` | 读取 JSON 点表，解析通用 CAN 信号，管理数据规则及 CBOR 编码。 |
| `src/data_process/dima_data_process.*`、`yuanjin_data_process.*` | 设备专用 CAN 解析；Dima 实现了多类车辆、电池及告警字段，远锦解析目前是 TODO。 |
| `src/app_server/` | 订阅 GPS 消息、定时请求 MCU 状态并解析响应。 |
| `src/new_energy/gb32960.*` | TCP 连接和登录/上报/登出状态机，离线数据保存与历史补传。 |
| `src/new_energy/new_energy_frame_pack.*` | 组装登录、实时数据、登出等帧及定位、电池、告警字段。 |
| `src/common/`、`src/list/`、`src/scene/`、`src/scene_lib/` | 时间与配置、链表容器、场景逻辑及共享库实现。 |

## 启动与数据流

1. `main()` 不解析命令行参数。它创建 `can_mng`，从 `/opt/parameter_conf.ini` 获取远端域名和端口，从 `/opt/machine_vin` 获取 VIN，读取本地或模组 ICCID，并加载 `/opt/Pointsheet_info1_gb32960.json` 点表。
2. 程序注册进程保活信息，每 10 秒更新时间戳，超时配置为 20 秒；随后启动 CAN 订阅和本机 GPS/MCU 消息线程。本机消息线程每 5 秒向 MCU 请求状态。
3. 主线程先等待 10 秒，以便收集单体电池数据，然后启动实时上报、历史补传、离线保存线程。CAN 数据由通用点表解析与设备专用解析共同处理。
4. 实时上报状态机在 ACC 开启，或车辆处于充电且 CAN 活跃时尝试解析平台域名并建立 TCP 连接；若本地登录状态仍为 LOGIN，重连时先尝试登出，再重新登录。收到登录应答字节后进入实时上报状态。
5. 常规实时上报周期约 10 秒；告警等级为三级时改为约 1 秒。电芯较多时按 `_splitFrameNum` 分帧。ACC 关闭且不满足充电条件时尝试登出。上述周期取决于线程调度、数据可用性和连接状态。
6. 网络离线时，保存线程约每 9.99 秒打包一次，三级告警时约每 0.99 秒一次；补传线程联网后读取历史文件，把报文命令字改为补发类型并发送。

## 配置、接口与运行边界

| 位置或接口 | 当前源码约定 |
| --- | --- |
| `/opt/parameter_conf.ini` | `[server]` 中读取 `domain` 和 `port`；域名缺省字符串为 `fyai.top`，端口缺失时读取为 0，平台配置函数返回失败。`main()` 当前未检查该返回值，实际连接前应核对配置。 |
| `/opt/conf.ini` | `[dev]` 中读取 `id` 与 `secret`，并可读取本地 ICCID；读取设备 ID/密钥失败时 `can_mng_init()` 会打印错误，但不会因此直接终止。 |
| `/opt/conf_ext2.ini` | `get_dev_config()` 可读取本机网关 `mgw:local_ip`、`mgw:local_port`，默认 `127.0.0.1:1883`；该函数不代表 GB 32960 主链路改用 MQTT。 |
| `/opt/machine_vin`、`/opt/sim_info` | VIN 与 SIM 信息来源；Dima CAN 收到完整 VIN 时可回写 VIN 文件。 |
| `/opt/Pointsheet_info1_gb32960.json` | 启动时加载的通用 CAN 信号点表；`main.c` 当前未因加载失败而退出，应检查目标设备文件和运行日志。 |
| `/opt/conf_loginout.ini` | 保存 `loginout:stat`（LOGIN/LOGOUT）及 `session:id`；不存在或为空时可生成默认配置。此状态是本地状态，不能单凭它证明平台已确认全部业务数据。 |
| `127.0.0.1:16002/16003/16004` | nanomsg SUB 接收三路 CAN 数据；本文件虽声明 26002/26003/26004 数组，但 `start_can_server()` 当前没有使用其发送 socket。 |
| `127.0.0.1:16005`、`127.0.0.1:38000` | 分别是 GPS 订阅和 MCU 请求/应答服务地址。 |
| `/media/sdcard/gb32960_history` | 离线历史报文目录。单文件约 5 MB 后轮换；启动保存线程时若总量超过 450 MiB，会调用旧文件删除函数。目录和存储介质须在目标设备上可用。 |
| `/media/sdcard/gb32960.log`、`/tmp/offline.flag` | 离线相关日志与标志路径；其使用需结合实际触发分支和文件权限检查。 |
| `/opt/gps_data.txt` | 定位组包逻辑保存/读取本地 GPS 数据时使用。 |

### 特别注意的源码边界

- 远锦设备选项能够通过 CMake 选择，但专用解析函数是 TODO；不能把编译成功视作远锦车辆数据已可用。
- 历史补传代码对每条记录调用 `data_send_out()` 后不检查平台业务确认；处理完文件即执行删除。若发送失败、连接中断或平台拒收，可能丢失尚未确认的数据。排查补传时须核对目标设备日志与平台接收记录。
- 登录与登出使用 `recv()` 是否收到字节来推进状态，当前代码未见对平台应答码的完整业务校验。发送成功和本地 LOGIN 标记均不能单独证明平台接受数据。
- 源码有 `NEW_ENERGY_IP`/`NEW_ENERGY_PORT` 测试常量，也链接 Mosquitto、OpenSSL、cn-cbor 等库；主上报连接实际从 INI 获取平台地址并走 TCP。`USE_AES128_ENCRYPTO` 当前定义为 0，AES128 打包函数没有被当前登录、登出、实时上报路径调用。不要用未调用的常量或已链接库推断当前主链路行为。
- `can_mng_init()` 中 `load_libscene()` 调用已被注释；虽然仍构建并安装 `libscene.so`，当前主流程不会借助该库切换场景。`/tmp/offline.flag` 和 `/media/sdcard/gb32960_recorder` 等路径也出现在源码中，但不能仅凭声明认定运行时会持续写入。
- `main.c` 中 `snprintf(path, PATH_MAX, "/opt/Pointsheet_info1_gb32960.json", can_mng->absolute_path)` 带有未使用的额外参数；实际加载路径仍是固定的 `/opt` 文件。

## 构建与维护

- 顶层构建需打开 `WITH_GB32960_CLIENT_FOR_DIMA=ON`，同时选择 `GB32960_DEVICE_DIMA=ON`（或确需远锦入口时选 `GB32960_DEVICE_YUANJIN=ON`）；两个设备选项默认都是 OFF，均不选择会在 CMake 配置阶段报错。本目录要求 CMake 3.11 和 C99。
- 交叉编译分支支持 `QL_MODULE_PLATFORM` 为 `EC200A`、`EG25G`、`MCIMX6Y2CVM08AB`；平台宏、SDK 头文件和库路径由对应工具链环境决定。CMake 查找 nanomsg、OpenSSL、cJSON、appmng、Mosquitto，并链接 cn-cbor、tbox-common、pthread 等库。输出安装到前缀下 `usr/bin`，共享库安装到 `usr/lib`。
- 以仓库 README 的 EC200A 工具链为例，在 SDK 根目录加载 `cross-profiles/ql-ol-crosstool-env-init-ec200a` 后，可用独立构建目录配置：`cmake -S . -B build-gb32960-dima -DCMAKE_TOOLCHAIN_FILE=cross-profiles/toolchain_armv7-a-ec200a.cmake -DWITH_GB32960_CLIENT_FOR_DIMA=ON -DGB32960_DEVICE_DIMA=ON -DCMAKE_INSTALL_PREFIX=./package_install`；再运行 `cmake --build build-gb32960-dima --target gb32960_client scene`。执行前核对环境脚本中的 SDK 路径及第三方库；此命令是配置示例，并非本次已完成的编译验证。
- 修改 Dima 车辆信号时，联查 `dima_data_process.c`、通用点表规则、`can_server.c` 以及组包字段；核对 CAN ID、位偏移、比例系数和无效值。修改平台上报时，联查 `gb32960.c` 与 `new_energy_frame_pack.c`，尤其检查登录应答、分帧、离线保存及补传删除时机。
- 当前目录未见专用测试目标。文档依据上述源码与 Git 状态整理，未在本机完成目标设备的 CAN、MCU、GPS、平台和 SD 卡端到端验证；实际部署应在目标设备上验证登录、告警周期、断网恢复和补传结果。

### 排查顺序

1. 先确认运行程序和编译选项：`WITH_GB32960_CLIENT_FOR_DIMA` 已启用，设备宏选的是实际车型；远锦解析当前仍是 TODO。核对目标机上的 `/opt/Pointsheet_info1_gb32960.json` 与 Dima CAN 信号是否匹配。
2. 检查 CAN 三路、GPS 和 MCU 的本机 nanomsg 服务；核对 VIN 是否为 17 位、`/opt/sim_info` 中 ICCID 是否有效，并确认 MCU 上报的 ACC 状态或充电加 CAN 活跃条件成立。
3. 检查 `/opt/parameter_conf.ini` 的平台域名和端口、DNS 与 TCP 连接，再看 `/opt/conf_loginout.ini` 的本地登录状态和流水号；以平台日志或抓包确认真实应答，不仅凭本地 `send()` 成功判断上报成功。
4. 断网补传时核对 `/media/sdcard/gb32960_history` 的可写性、文件数量及平台接收记录；当前代码可能在没有业务确认时删除历史文件，定位丢数应保留设备日志并核对服务端数据。
