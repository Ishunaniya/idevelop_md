# stub-gb-client 项目说明

## 项目位置与用途

- Git 仓库及源码根目录：`/home/tronlong/lyp/code/stub-gb-client/`；本文档存放目录：`/home/tronlong/lyp/perCode/idevelop_md/stub-gb-client/`。源码仓库是独立 Git 仓库，文档库是另一处目录。
- 整理本文档时，源码仓库当前分支为 `master`，提交短哈希为 `163def2`。这是当前工作树状态的记录；切换分支或更新提交后，应重新核对以下参数、接口和构建说明。
- 项目是三一“新 C”环境中的车载排放数据适配程序，CMake 目标和安装后程序名为 `stub-gb`。程序从本机 CAN、GPS、加密服务取数据，按发动机类型组织非道路国四（源码名“非四”）或国六报文。防篡改备案经独立 TCP 连接发往对应国家平台；登录、实时/历史数据等由 `stub-linker` 接入 SDK 设备事件通道。SQLite 保存待补传报文。
- `README.md` 说明此目录通常放入 `rootcloud-stp-sdk/erk-plugins/plugins/scp-adapter/tools/`，由 SDK 顶层 CMake 纳入构建。当前独立检出的路径用于源码阅读；`CMakeLists.txt` 包含上级目录和 `erk-core` 头文件、链接 SDK 库，不能据此认定单独在此目录直接编译就具备全部依赖。
- `stub.ini` 的示例参数是 `2 2 0`，即 STC250E5、国六 DPF+SCR、关闭 GPS 时间模式。实际生效参数还取决于 SDK 启动配置和目标设备。

### 相关技术与名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| `stub-gb`、`stub-linker`、SCP | `stub-gb` 是本目录生成的进程；`stub-linker` 提供本地发布、订阅、主题和消息封装接口。源码中的 SCP 指相邻 SDK 侧的通信对象，进程用心跳判断它是否在线；还会读取默认网络端点的 `connected` 状态。消息进入本地链路不能证明最终云端已接收。 |
| 非四／`HJ1014`、国六／`HJ1239` | 两套由 `engine_type` 选择的排放数据报文和状态机：`src/fei_4/` 处理非道路国四，`src/gb6/` 处理国六。`HJ1014`、`HJ1239` 是源码所用标准标识；字段、版本和适用车型须按实际标准及设备配置核对。 |
| CAN、J1939、PGN | CAN 是车辆控制器通信总线；J1939 是工程车辆常用的 CAN 高层协议；PGN 是参数组编号。本项目按车型解析 CAN 帧，并由 `can_pgn_request()` 请求部分参数组。 |
| SAC、STC、DPF、SCR、TWC、NOx | SAC/STC 是源码中的起重机车型标识。DPF 是柴油颗粒物过滤器，SCR 是选择性催化还原，TWC 是三元催化器，NOx 是氮氧化物；这些名字用于区分发动机和后处理数据路径。 |
| ECD、OBD、SPN、FMI | ECD 是排放控制诊断数据，OBD 是车载诊断；SPN/FMI 分别表示 J1939 故障参数编号与故障模式标识。解析和缩放以对应车型源码为准。 |
| VIN、GPS、MCU | VIN 是车辆识别码（源码按 17 字节处理）；GPS 提供位置及可选时间驱动；MCU 是微控制器。本程序从配置或本机文件取得 VIN，通过本机 TCP 端口接收 GPS 和加密服务数据。 |
| 防篡改备案／激活 | 程序把芯片 ID、公钥等信息组织成备案报文，国六帧定义也称“激活信息”。签名后直接经国家平台 TCP 连接发送并等待答复；常规登录和数据帧进入 SDK 事件通道。这两条路径的连接状态与应答不能混为一谈。 |
| 签名、国密芯片 | `src/data_signature/` 管理待签名数据及结果，本机 `16009` 端口连接加密服务。源码有默认芯片 ID 和公钥字面值，它们只说明当前代码默认配置，不能证明设备上的密钥或签名链路正常。 |
| BCC、SM2、SM4 | BCC 是报文末尾的异或校验字节，打包与备案答复校验代码都使用它。SM2/SM4 是帧头枚举中的国密算法标识；不能仅凭存在枚举就推断所有报文实际采用该算法，签名由加密服务完成。 |
| SQLite、补传、BLOB | SQLite 在设备文件中保存历史二进制报文；BLOB 是保存报文字节的列类型。数据库记录被读取或删除，只表示本地流程推进，不能代替平台接收确认。 |
| JSON 事件、`encoding=raw` | `nanomsg_push_event_msg()` 把原始报文字节转为大写十六进制字符串，放入设备事件的 `param.data`，并在 `param.encoding` 标记 `raw`；`raw` 是本项目消息字段取值，不能理解为 JSON 内直接包含二进制字节。 |
| INI、cJSON、nanomsg | INI 保存 VIN、注册/登录状态和流水号；cJSON 构造与解析本地 JSON 消息；`stub-linker` 底层依赖 nanomsg 消息库。本机 CAN/GPS/加密数据另走普通 TCP socket。 |
| CMake、交叉编译、`QL_SYSROOT_DIR` | CMake 生成构建规则；交叉编译在开发机上生成目标 ARM 设备程序。构建脚本引用移远工具链 sysroot 的头文件和库，依赖 SDK 提供的包及目标平台环境。 |

## 主要文件与职责

| 路径 | 职责 |
| --- | --- |
| `README.md`、`CMakeLists.txt`、`stub.ini` | SDK 集成与构建说明、依赖及安装规则、启动参数示例。 |
| `stub-gb-client.c/.h` | `main()`、本地 stub 发布订阅、心跳/网络状态、消息缓存、日志与定时任务；头文件保存消息结构和版本定义。 |
| `t4hj_main.c/.h` | 参数检查、应用初始化、数据库打开、启动本地数据接收/签名/协议线程。 |
| `src/app_mng/` | 全局状态、VIN/芯片信息、队列、协议数据库路径和 CAN 数据初始值。 |
| `src/local_skt/`、`src/skt_res/` | 连接本机 CAN/GPS/加密 TCP 服务，接收并分发帧。 |
| `src/can_parse/` | 按 SAC/STC 车型和发动机类型解析 CAN 帧及请求 PGN。 |
| `src/fei_4/`、`src/gb6/` | 非四/国六状态机、帧打包、备案、登录、实时上报及历史处理。 |
| `src/data_signature/` | 待签名数据队列、请求和结果处理。 |
| `src/nanomsg/` | 把协议报文包装成 SDK 设备事件，通过 `stub-linker` 推送；依据 SCP 在线和网络状态选择发送。 |
| `src/db/`、`src/iniparser/`、`src/json/` | SQLite 历史数据、INI 解析、JSON 处理。 |
| `src/common/`、`src/list/`、`src/log/` | 公共函数、链表与日志辅助。 |
| `startup-service-install.sh` | 修改设备端 `hqinit.hrc` 中 `erk-daemon` 自启动项的辅助脚本，末行实际调用了 `install_service`。默认命令指向 `/media/card/erk-root/erk-daemon.sh`，可用 `-d/--dest` 覆盖；运行要求 root 和目标文件存在。它管理 SDK 守护进程，不是直接启动 `stub-gb` 的脚本。 |

## 启动、参数与数据流

1. 运行格式为 `stub-gb <vehicle_type> <engine_type> <enable_gps_time>`。`check_input_args()` 要求三个参数都可解析为整数，车型范围 0～3，发动机范围 0～5；对 GPS 时间参数仅做整数解析，没有限制为 0/1。参数通过检查不等于该组合有 CAN 解析实现。
2. `main()` 初始化日志和 `stub-linker`：订阅 `127.0.0.1:11216`，发布到 `127.0.0.1:11215`，并通过心跳、网络状态消息维护在线状态。随后 `t4hj_main()` 初始化队列、VIN、数据库和线程。
3. 本地 socket 线程连接 CAN0/1/2、GPS 和加密服务；车型解析代码更新共享 CAN 数据。非四或国六线程负责打包、备案/登录、实时与历史流程；签名线程调用加密服务。备案签名结果走国家平台 TCP，其他待上报帧由 `src/nanomsg/` 包装为 SDK 设备事件 JSON。
4. 主循环处理 stub 消息及定时任务，包括版本、心跳、网络状态和设备参数请求。缓存发送线程只在 SCP 在线时发送；协议数据的企业平台上报还检查网络连接状态。

### 登录、签名与缓存的实际条件

- 协议线程约每 100 毫秒运行一次，同时调用 `can_pgn_request()`；CAN 超过 15 秒无有效帧后走失活分支，把实时队列转为历史数据，并尝试拆除报警和登出。备案、登录和实时上报还受 VIN、网络状态及本地 INI 状态影响。
- 非四实时数据流的间隔常量为 60 秒，ECD 数据为 600 秒；国六流数据为 10 秒、补充流为 60 秒、OBD 为 600 秒。它们是代码中的触发间隔，不能直接解释为平台必定按此频率收到数据。
- 签名线程通过 `127.0.0.1:16009` 发送请求；等待答复超时常量是 3 秒，重发次数常量为 3。超过重试限制后会移除待签名项。签名成功会把常规报文加入企业平台发送队列；防篡改备案报文则直接写国家平台 socket。
- 非四一次读取最多 1 条历史记录，国六一次读取最多 10 条并合包。两套流程都在历史记录进入签名处理流程附近删除数据库行，删除不等待签名成功或企业平台回执。国六 `enable_gps_time` 为非零时，实时签名路径要求至少积累 10 条实时数据项；缺少 GPS 消息时，流数据打包会等待。
- `stub-gb-client.c` 的发布缓存最多容纳 1024 条；满时先丢最旧消息。`nanomsg_push_event_msg()` 调用 `push_msg()` 后固定返回 0，而 `nanomsg_client_task()` 据此移除发送队列项，因此该返回值只代表程序执行到本地入队流程，不能作为消息成功入队、发往 SCP 或云端确认的证明。
- 国六或非四登录函数把登录帧放进本地发送队列；状态机在函数返回成功时更新本地 `LOGIN` 状态。当前回调只处理心跳、版本和网络状态消息，没有在这里验证企业平台对登录帧或数据帧的业务应答。
- `fei4_send_out()` 声明返回 `int`，正常入队分支却没有 `return`；其调用方用返回值判断非四登录/登出是否成功。这里存在未定义的返回值行为，因此不能把非四本地登录状态当成稳定、可靠的入队结果。签名结果处理也未检查 `data_signatured_handler()` 的返回值就把该项标为成功，排障时应结合后续队列与平台记录核对。

### 参数与 CAN 解析范围

| 参数值 | 名称 | 当前 `can_parse.c` 的实际解析分支 |
| --- | --- | --- |
| 车型 0、1 | `SAC4000C8`、`SAC2000C8` | 发动机 0（非四柴油）或 2（国六 DPF+SCR）。 |
| 车型 2、3 | `STC250E5`、`STC250_550C5` | 仅发动机 2（国六 DPF+SCR）。 |
| 发动机 1、3、4、5 | 非四非柴油、国六 TWC 无/有 NOx、国六混动 | 参数检查允许，并分别进入非四或国六线程；当前 `can_parse.c` 没有对应车型组合的 CAN 解析分支。 |
| GPS 时间参数 | 0 关闭；非零值在条件判断中视为开启 | GPS 消息仅在车型 2/3 且发动机 2 时进入国六 GPS 队列；其他组合启用该参数时，不能据此假设实时数据能按 GPS 时间生成。 |

## 配置与运行接口

| 位置或接口 | 当前源码约定 |
| --- | --- |
| `127.0.0.1:11215`、`11216` | `stub-linker` 发布与订阅连接。 |
| `127.0.0.1:16002`～`16004`、`16005`、`16009` | CAN0/1/2、GPS、加密服务的本机 TCP 连接；这些端口需要对应服务端存在。 |
| `/opt/fei4_conf.ini`、`/opt/gb6_conf.ini` | 两套协议各自的 VIN、注册/登录状态、日期和流水号；程序可能写回。 |
| `/tmp/vin.ini` | 国六配置没有 VIN 时，初始化代码会每 5 秒重试读取此文件，读到内容后写入国六 INI。 |
| `/media/sdcard/hj1014.db`、`/media/sdcard/hj1239.db` | 历史 SQLite 文件，表名分别为 `FEI4`、`GB6`；目标设备需具备目录与写权限。 |
| `/tmp/stub-gb-client.ini`、`/tmp/stub-gb-client.log` | 日志级别配置和日志文件；`SIGUSR1` 会循环调整日志级别。 |
| `fdlpfjk.vecc.org.cn:55001`、`g6check.vecc.org.cn:19006` | 非四与国六防篡改备案使用的国家平台 TCP 连接常量。`fei4.h` 另定义 `53001` 数据端口，但当前 `fei4_connect_server()` 只使用 `55001`；常规登录/数据由 SDK 事件通道处理。部署时按目标环境核对。 |
| 程序版本 | `stub-gb-client.h` 的当前宏为 `1.7.0.0`；`main()` 还会打印 `stub-linker` 协议库的版本。版本宏与 Git 提交是不同的标识。 |

### 常见排查顺序

1. 核对实际进程参数、`stub.ini`、仓库分支和可执行文件版本；先确认车型与发动机组合在 `can_parse.c` 有对应分支。
2. 检查本机 CAN/GPS/加密服务的 TCP 端口、VIN 来源、`/media/sdcard` 可写性，以及两个 INI 的注册/登录状态。国六缺 VIN 时，初始化会持续等待读到 `/tmp/vin.ini`。
3. 分别检查 SCP 心跳、默认网络端点的 `connected` 状态、企业平台事件链路，以及国家平台备案 socket 与答复。区分本地登录标志、发送队列出队和平台实际接收。
4. 排查补传时核对 SQLite 行数、签名重试日志、`stub-gb-client.log` 和平台记录；数据库行减少或本地日志打印 `sent`，均不能单独证明平台收到报文。

## 构建与维护

- 按仓库 `README.md`，将工程放入 `rootcloud-stp-sdk/erk-plugins/plugins/scp-adapter/tools/`，在该目录的 `CMakeLists.txt` 增加 `add_subdirectory(stub-gb-client)`；加载 `ql-ol-crosstool-env-init`，在 SDK 的 `build` 目录使用 `cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain_armv7-a-ec200a.cmake`，再执行 `make` 和 `make package`。这是一套文档给出的 EC200A 构建步骤，尚需实际 SDK 环境验证。
- 本目录 CMake 使用 C99、Debug 构建，查找 `e2fsprogs-git`、`cjson-git`、`nanomsg-git`、`libcbor-git`、`collections-c-git`、`zlib-git`、`stub-linker-git`，链接 SDK 工具库、SQLite、pthread 等。`install()` 把 `stub-gb` 和 `stub.ini` 放到 `tools/stub-gb/`。
- 修改 CAN 信号时，联查对应车型解析文件、`can_parse.c`、`app_mng.h` 和协议打包器；检查 CAN ID、字节序、缩放、无效值与车型/发动机组合。修改平台上报时，联查 `src/nanomsg/`、`stub-gb-client.c` 的在线判定和两套协议队列。
- 历史数据行数上限是非四 11088、国六 61488。达到上限时，协议模块删除上限值约十分之一的较早记录。这是源码中的容量策略，不能据此断言设备必然保留完整七天数据。
- 当前仓库没有独立测试目录或可直接运行的主机侧测试入口。文档依据 `master` 分支源码和 README 整理；实际通信、备案、签名及补传需要在具备 SDK、本机服务和目标设备的环境验证。
