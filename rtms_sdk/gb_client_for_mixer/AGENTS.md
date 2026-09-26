# gb_client_for_mixer 项目说明

## 项目位置与用途

- Git 仓库根目录：`/home/tronlong/lyp/code/rtms_sdk`；项目源码：`apps/gb_client_for_mixer/`；本文档目标目录：`/home/tronlong/lyp/perCode/idevelop_md/rtms_sdk/gb_client_for_mixer/`。这是同一个 SDK 仓库内的应用目录，不是独立仓库。
- 整理本文档时，仓库分支为 `develop/rtms_sdk_v1.3_20240408`，提交短哈希为 `bc60961e`（由 Git 查询核对）。分支属于整个 `rtms_sdk` 仓库；切换分支或更新源码后，应重新核对文档中的默认值和流程。
- 本目录生成设备侧 `gb_client` 可执行程序。它从本机 CAN、GPS、MCU 服务取得车辆、发动机和定位数据，按发动机类型组织非道路国四（源码简称“非四”，`HJ1014`）或国六（`HJ1239`）报文，经签名设备处理后向平台发送；待补传的数据存入 SQLite 数据库。
- 虽然目录名是 `gb_client_for_mixer`，命令行仍支持 SAC、STC、搅拌车 `VT_MIXER` 及混动车型 `VT_HYBRID_LC`。无效参数会回退到 `VT_MIXER`、`HJ1239_UNDEF`、禁用 GPS 时间；实际接入车型和协议行为应由启动参数及目标设备确定。
- `apps/CMakeLists.txt` 中的 `WITH_GB_CLIENT_FOR_MIXER` 默认 `OFF`；启用后才通过 `add_subdirectory(gb_client_for_mixer)` 纳入 SDK 顶层构建。本目录自身的 CMake 项目名、输出程序名均为 `gb_client`，版本为 1.3。

### 相关技术与名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| RTMS SDK、CMake、交叉编译 | `rtms_sdk` 是包含本应用的源码仓库。CMake 根据两级 `CMakeLists.txt` 生成构建规则；交叉编译是在开发主机上使用目标设备工具链生成程序。`QL_MODULE_PLATFORM` 选择平台头文件、宏和库。 |
| `gb_client`、T-Box | `gb_client` 是本目录生成的车载终端进程，负责采集、打包、签名、缓存及上报排放相关数据；T-Box 指它所在的车载通信终端环境。它依赖设备中的 CAN、MCU、GPS、签名接口和网络服务。 |
| 非道路国四／`HJ1014`、国六／`HJ1239` | 两套由 `engine_type` 选择的数据上报流程。`HJ1014_DIESEL`、`HJ1014_NONDIESEL` 走 `fei_4`；`HJ1239_DPFSCR`、`HJ1239_TWC_NoNOx`、`HJ1239_TWC_NOx`、`HJ1239_UNDEF` 走 `gb6`。这些是源码中的枚举和协议命名，具体适用范围以设备配置和标准文本为准。 |
| CAN、J1939、PGN | CAN 是车载控制器总线。搅拌车解析代码按 J1939 的 29 位扩展帧 CAN ID 处理部分报文；PGN 是其中表示参数组的编号。不同车型在 `src/can_parse/` 中有不同解析入口。 |
| OBD、ECD、DPF、SCR、NOx | OBD 是车载诊断；ECD 在本项目中指排放控制诊断数据；DPF 是颗粒物捕集器；SCR 是选择性催化还原系统；NOx 是氮氧化物。它们对应 CAN 字段、诊断/排放数据包，字段缩放和无效值要按源码及协议核对。 |
| VIN、GPS、MCU | VIN 是 17 字节车辆识别码，可从配置或 `/opt/machine_vin` 获取；GPS 提供定位信息；MCU 是微控制器，程序通过本机消息接口定期请求其状态。 |
| ICCID | SIM 卡的集成电路卡识别码；登录报文结构中预留该字段。字段是否具有有效值，应核对打包函数与目标设备的数据来源。 |
| nanomsg PUB/SUB、REQ/REP | 本机进程通信模式：订阅 CAN/GPS 发布的数据（SUB），向 CAN 发送端发布报文（PUB），向 MCU 服务发送请求并接收答复（REQ/REP）。`tcp://127.0.0.1` 地址只用于本机进程间通信。 |
| TCP、MQTT、`ENABLE_TCP_PLATFORM` | 两种远端通信路径。CMake 默认将 `ENABLE_TCP_PLATFORM` 设为 `ON`，编译 TCP 上报线程；关闭后编译 Mosquitto MQTT 客户端。代码中两条路径的连接配置和行为不同，部署时须匹配实际构建选项。 |
| 签名设备、`/dev/ttySIGN` | `data_sign_task` 通过设备节点发送待签名数据、读取结果并处理重试。该路径需要目标设备提供对应驱动/服务；源码中的芯片 ID 和公钥默认值不能代替生产设备配置验证。 |
| SQLite、历史补传 | `src/db/` 使用 SQLite 的 BLOB 表存储待补传报文；国六与非四使用不同数据库及表名。在线数据、备份数据和签名队列由各协议模块管理，不能把数据库存在等同于补传已经成功。 |
| 备案／激活信息、防篡改 | 两个协议模块在注册阶段向各自国家平台发送防篡改相关报文，并等待平台答复。国六帧定义将该命令标为“激活信息”；此链路与常规企业平台上报链路分开。 |
| BLOB、流水号 | BLOB 是 SQLite 保存二进制报文体的字段类型；流水号用于标识登录和数据报文，在对应 INI 中保存，并在源码指定的日期变化时重置。 |
| BCC、SM2、SM4 | BCC 是报文校验字段，帧结构中用异或校验；SM2、SM4 是国密算法标识。代码声明了多种加密类型，不能仅据枚举推断当前报文都采用相应算法；实际签名由设备接口完成。 |
| INI、`iniparser`、cJSON | INI 用于保存 VIN、登录/注册状态和流水号等设备配置；`iniparser` 负责解析。cJSON 用于处理 MCU、MQTT 等接口的 JSON 消息。 |
| nanomsg、Mosquitto、OpenSSL | 分别用于本机消息、可选 MQTT 连接和加密相关库链接；具体启用路径由编译选项及运行环境决定。 |

## 主要文件与职责

| 路径 | 职责 |
| --- | --- |
| `CMakeLists.txt`、`global_config.h.in` | 定义版本、平台、依赖库、`gb_client` 构建和安装规则。 |
| `main.c` | 解析车型/发动机/GPS 时间参数，初始化状态和数据库，启动 CAN、本机消息、远端上报、签名及协议线程。 |
| `src/app_mng/` | 保存共享状态、VIN、数据库路径、数据队列及本机 CAN 发布 socket。 |
| `src/can_server/`、`src/can_parse/` | 订阅 CAN0/1/2 数据并按车型解析；`vt_mixer.c` 包含搅拌车及混动车相关处理。 |
| `src/app_server/` | 接收 GPS 数据，定时请求并解析 MCU JSON 状态。 |
| `src/fei_4/`、`src/gb6/` | 分别实现非四、国六协议状态机、报文打包、登录/登出、数据上报和历史处理。 |
| `src/data_signature/` | 构造签名请求，经 `/dev/ttySIGN` 与签名设备通信，处理签名结果。 |
| `src/platform/`、`src/mosquitto/` | TCP 远端上报实现，以及关闭 TCP 选项时使用的 MQTT 客户端实现。 |
| `src/db/`、`src/iniparser/` | SQLite 历史数据读写和 INI 配置解析。 |
| `src/common/`、`src/skt_res/`、`src/list/` | 时间、配置、socket 和队列等辅助功能。 |

## 启动与数据流

1. 启动参数格式为 `gb_client <vehicle_type> <engine_type> <enable_gps_time>`。源码列出车型值 0–5、发动机值 0–4/254/255、GPS 时间开关 0/1；实际参数限制及车型组合见下表。
2. `main()` 初始化共享状态，从对应 INI 或设备文件取得 VIN，打开并创建非四或国六 SQLite 表；注册进程保活信息，初始化 CAN 发布连接。
3. CAN 线程订阅本机三个 CAN 通道；应用消息线程接收 GPS，并每 5 秒请求 MCU 信息。对应解析结果写入共享状态。
4. 默认 TCP 构建启动 `plaform_data_report_task`；关闭 TCP 选项后启动 `mosq_client_task`。签名线程同时等待待签数据，通过 `/dev/ttySIGN` 获取处理结果。
5. 发动机类型决定启动 `fei4_task` 或 `gb6_task`。协议线程周期调用 `can_pgn_request()` 主动请求 CAN 数据，并按 VIN、注册状态、远端连接和登录状态推进流程。备案/激活报文通过另一条到国家平台的 TCP 连接处理，常规数据经签名队列进入企业平台发送队列；离线或缓存数据按各自状态机进入补传路径。

### 参数与实际 CAN 解析范围

| 参数 | 源码列出的取值 | 实际行为与限制 |
| --- | --- | --- |
| `vehicle_type` | 0 `SAC4000C8`；1 `SAC2000C8`；2 `STC250E5`；3 `STC250_550C5`；4 `VT_MIXER`；5 `VT_HYBRID_LC` | `main.c` 只检查 0～`VT_MAX`（255），而 `can_parse.c` 只为 0～5 定义解析分支。6～255 可能通过参数检查，却不会解析对应车型的 CAN。 |
| `engine_type` | 0 `HJ1014_DIESEL`；1 `HJ1014_NONDIESEL`；2 `HJ1239_DPFSCR`；3 `HJ1239_TWC_NoNOx`；4 `HJ1239_TWC_NOx`；254 `HJ1014_UNDEF`；255 `HJ1239_UNDEF` | `main.c` 只检查 0～255；实际只对 0、1 启动非四线程，对 2、3、4、255 启动国六线程。254 及其他未列值不会启动协议线程。 |
| `enable_gps_time` | 0 关闭；1 开启 | `main.c` 解析整数但未限制只能为 0/1；国六流程将非零值视为启用 GPS 时间驱动的数据入队，并等待 GPS 队列中的消息。当前目录中可找到队列初始化和消费，却找不到向 `gps_lst` 写入消息的代码，因此仅凭当前源码启用该选项可能持续等不到实时数据；此参数也不是设备系统时钟同步开关。 |

`can_parse.c` 的组合分支更窄：SAC4000C8/SAC2000C8 只解析非四柴油（0）或国六 DPF+SCR（2）；STC250E5/STC250_550C5 只解析国六 DPF+SCR（2）；搅拌车与混动冷藏车解析国六 2、3、4、255。参数通过范围检查并不表示该组合能产生有效上报数据。

### 协议处理和上报节奏

- `gb6_task` 和 `fei4_task` 各自在约 100 毫秒的循环中推进注册、数据打包、签名调度和 `can_pgn_request()`；只有当前发动机类型对应的协议线程会启动。`main.c` 的保活线程每 10 秒更新一次进程访问时间，注册的超时值为 20 秒。
- 两个协议模块通过 CAN 活跃状态、VIN、远端连接状态和登录状态决定处理分支。CAN 无数据超时常量均为 15 秒；从活跃变为非活跃时，代码会把当前实时队列转存为历史数据，并尝试发送拆除报警和登出。设备是否真的完成报警/登出，仍取决于连接及平台应答。
- 国六头文件给出实时流 10 秒、可选补充流 60 秒、混动附加流 60 秒、OBD 120 秒；非四给出数据流 60 秒、ECD 600 秒。以上是源码中的触发间隔常量，实际上报还取决于 CAN、VIN、GPS、队列和登录条件。`ENABLE_SUPPLEMENT_REPORT` 默认关闭。
- 上报流程中，未登录或远端离线时生成的报文进入历史数据；登录且在线后，优先处理数据库里的历史记录，再处理实时队列。设备重启后若 INI 保留 `LOGIN` 状态，代码会先尝试登出，再重新登录。协议登录状态是代码维护的本地状态，不单独代表平台已确认接收每一帧。

## 关键配置与通信接口

| 位置或接口 | 当前源码约定 |
| --- | --- |
| `/opt/fei4_conf.ini`、`/opt/gb6_conf.ini` | 分别保存两套协议的 VIN、注册/登录状态、流水号和日期等键；程序可能写回。 |
| `/opt/machine_vin` | 国六 VIN 配置缺失时，尝试读取 17 字节并写入国六 INI。 |
| `/media/sdcard/hj1014.db`、`/media/sdcard/hj1239.db` | SQLite 历史数据文件；表名分别为 `FEI4`、`GB6`。目标设备需具备存储目录和可写权限。 |
| `127.0.0.1:16002/16003/16004` | nanomsg SUB 接收 CAN0/1/2；`26002/26003/26004` 是对应的 PUB 发送连接地址。 |
| `127.0.0.1:16005`、`127.0.0.1:38000` | 分别是 GPS 的 nanomsg SUB 连接、MCU 的 nanomsg REQ 连接。 |
| `/dev/ttySIGN` | 签名线程访问的设备节点。 |
| 国家平台备案连接 | 国六模块解析 `g6check.vecc.org.cn:20006`；非四模块解析 `fdlpfjk.vecc.org.cn:55001`。这两条连接用于防篡改备案/激活及答复，地址来自当前头文件常量；生产环境需按部署要求核对。 |
| TCP 远端地址 | `platform.c` 默认 `140.143.114.43:50201`，可由运行环境变量 `TRUCKLINK_GW_IP`、`TRUCKLINK_GW_PORT` 覆盖。这是当前源码默认值，不代表所有部署环境。 |
| MQTT 远端地址与状态 | 关闭 TCP 选项时，`client_mosquitto.c` 当前写有 `192.168.1.253:1883`，订阅 VIN 命令主题 `v4/s/post/device/cmd/json/1.0`；连接状态还受 `/tmp/clound_status` 文件内容影响。该路径名的 `clound` 为源码原拼写。 |

企业平台数据上报与国家平台备案分别建立连接；默认 TCP 上报线程使用 `platform.c` 的网关地址，国六/非四注册逻辑使用各自国家平台域名。排查连接时需先区分这两条链路。

两个 INI 的常用键分别以 `fei4:`、`gb6:` 为前缀，包含 `vin`、`registerStat`、`loginStat`、`loginSession`、`loginDate`、`realDataSession`、`realdataDate`、`takeDownSession`。`loginDate`、`realdataDate` 用于按日期重置相应流水号；不要在运行中的设备上手工改写这些值而不检查状态机。

### 历史数据与运行边界

- 国六历史数据每次最多读取 10 条、合成补发帧；非四每次读取 1 条。两条路径在将数据加入签名队列后，就调用 `db_delete_t4hj_rows()` 删除已取出的行，并没有等待远端平台确认。因此 SQLite 表清空不能证明补传已获平台接收；签名失败、进程退出或发送失败时还可能失去这批待补传数据。
- `db_max_rows` 分别设置为非四 11088、国六 61488；达到上限时，协议线程按上限的十分之一删除较早记录。此限制是源码中的容量策略，不应解读为实际设备必然保存完整 7 天。
- 国六 `enable_gps_time=1` 时，实时数据打包依赖 `gps_lst` 有数据；当前源码只初始化和消费此队列，未见入队调用。若部署需要该模式，应先确认是否有其他构建改动提供入队逻辑，并在目标设备验证；`enable_gps_time=0` 时按国六上报间隔使用当前 GPS 状态并取设备时间戳。
- `main.c` 在初始化数据库或保活信息失败时退出；CAN、GPS、MCU、签名设备和远端服务均由目标环境提供。主机侧无法仅凭程序启动判定端到端上报成功。

## 构建、部署与维护

- 本目录要求 CMake 3.11，使用 C99。顶层构建需显式打开 `WITH_GB_CLIENT_FOR_MIXER`；本目录 `ENABLE_TCP_PLATFORM` 默认 `ON`，`ENABLE_SUPPLEMENT_REPORT` 默认 `OFF`。改动选项后应清理或使用独立构建目录，确认编译宏及最终线程路径。
- 以仓库 README 中的 EC200A 交叉编译方式为例，先加载 `cross-profiles/ql-ol-crosstool-env-init-ec200a`，再在独立构建目录配置 `cmake .. -DCMAKE_TOOLCHAIN_FILE=../cross-profiles/toolchain_armv7-a-ec200a.cmake -DWITH_GB_CLIENT_FOR_MIXER=ON -DCMAKE_INSTALL_PREFIX=./package_install`，随后按需编译 `gb_client` 目标并安装。运行前须确认环境脚本内 SDK 路径、工具链和目标平台一致；此处是配置示例，并未在当前环境执行完整编译。
- CMake 查找 nanomsg 1.2、OpenSSL 1.1.1、cJSON 1.7.15、appmng 1.0、Mosquitto 2.0.15，并链接 cn-cbor、tbox-common、SQLite、pthread 等依赖。交叉编译分支处理 `EC200A`、`EG25G`、`MCIMX6Y2CVM08AB`；其他 `QL_MODULE_PLATFORM` 值会触发配置错误。安装规则将程序放到安装前缀下的 `usr/bin`，并安装若干共享库到 `usr/lib`。
- 修改车型 CAN 信号时，联查 `src/can_parse/` 中对应车型文件、`can_server.c` 和 `app_mng.h` 的共享字段；确认通道、CAN ID、缩放和无效值。修改上报字段时，联查协议模块及对应 `*_frame_pack.*`，再核对签名与历史数据格式。
- 修改平台连接或设备配置时，分别检查 `platform.c`/`client_mosquitto.c`、INI 键、VIN 来源和目标设备上的启动参数。当前目录未见专用测试目标；主机侧可做配置/编译验证，链路、签名、CAN、断网补传须在具备本机服务和设备节点的目标环境验证。

### 排查顺序

1. 核对实际启动参数、构建时的 `WITH_GB_CLIENT_FOR_MIXER`/`ENABLE_TCP_PLATFORM` 选项和目标设备上的 `gb_client` 版本；确认车型和发动机组合在 CAN 解析分支中存在。
2. 检查本机 CAN/GPS/MCU nanomsg 服务是否提供对应端口，检查 VIN 来源与 INI 中的注册、登录状态。若开启 GPS 时间选项，重点检查 `gps_lst` 没有入队实现这一源码限制。
3. 检查 `/dev/ttySIGN` 是否可访问，区分国家平台备案连接与企业平台 TCP/MQTT 连接；MQTT 构建还要检查 `/tmp/clound_status` 的状态来源。
4. 查看 SQLite 表中的待补传行数及发送队列状态；不要把数据库行数下降或本地 `LOGIN` 标记当作平台接收确认。最后在目标设备上用协议日志与平台记录核对端到端结果。
