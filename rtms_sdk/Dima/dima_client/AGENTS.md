# dima_client 项目说明

## 项目位置与用途

- Git 仓库根目录：`/home/tronlong/lyp/code/rtms_sdk`；项目源码目录：`apps/dima_client/`；本文档目录：`/home/tronlong/lyp/perCode/idevelop_md/rtms_sdk/Dima/dima_client/`。本项目是 `rtms_sdk` 仓库中的一个应用目录，没有独立的 Git 分支。
- 整理本文档时，仓库分支为 `develop/rtms_sdk_v1.3_20240408`，提交短哈希为 `bc60961e`（分别由 `git branch --show-current`、`git rev-parse --short HEAD` 核对）。切换分支或更新源码后，应重新核对下文的配置、接口和行为。
- 本目录构建设备侧程序 `dima_client`：从本机 CAN 服务接收报文，按 `/opt/Pointsheet_info1.json` 中的点表解析信号，调用场景库判断运行状态，并按点表配置的场景和间隔把 JSON 数据发布到 MQTT。场景判断逻辑单独编译为共享库 `libscene.so`。
- `apps/CMakeLists.txt` 中 `WITH_DIMA_CLIENT` 默认是 `OFF`；启用后才会把本目录纳入 SDK 顶层构建。本目录 CMake 的项目版本是 1.5，生成程序名为 `dima_client`。

### 相关技术与名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| RTMS SDK、Git 分支、CMake | `rtms_sdk` 是包含多个应用的源码仓库；Git 分支标识当前查看的整仓源码版本。CMake 读取顶层和本目录的 `CMakeLists.txt`，根据构建选项生成编译与安装规则。 |
| T-Box、`dima_client` | T-Box 是设备侧通信终端的常见称呼；`dima_client` 是本目录生成的进程，负责把本机 CAN 数据转为配置驱动的 MQTT 上报内容。具体硬件型号和部署方式以目标设备为准。 |
| CAN、CAN0/1/2、CAN ID | CAN 是控制器局域网总线。程序通过本机消息服务订阅三个 CAN 通道；CAN ID 用于匹配点表规则，报文数据字节按规则中的位偏移、长度和字节序解析。 |
| 点表、信号、复用帧 | 点表是 `/opt/Pointsheet_info1.json`，定义设备通道、CAN 报文与数据项、场景和上报间隔。信号是从报文字段解析出的值；复用帧逻辑可依据数量项及关联帧组合数据项，具体映射由点表决定。 |
| 位偏移、字节序、有符号数、缩放 | 点表的 `ofs`、`len`、`bo`、`sign`、`sc`、`pofs` 等字段决定从 CAN 帧的哪一段取值，以及如何解释补码并换算为物理值；`min`、`max` 可限制结果范围。字段取值须与实际点表和报文协议核对。 |
| `sce`、`scems`、`udif` | 数据项的 `sce` 指定适用场景，`scems` 指定各场景的上报间隔（毫秒），`udif` 指定变化阈值。首次收到的值可上报；之后阈值为零时按间隔上报，非零时数值变化达到阈值或字符串发生变化才上报。 |
| 场景、`libscene.so` | 场景用于决定哪些信号在何种状态及间隔下上报。程序以 `dlopen` 加载 `/usr/lib/libscene.so` 中的 `judge_scene`；当前示例按 `0x171`、`0x172` 两种 CAN ID 对应的发动机转速和速度，返回 `power_on`、`idle`、`work`、`drive`。这是当前源码的判断规则，非通用车辆状态定义。 |
| nanomsg、PUB/SUB | nanomsg 是本机进程间通信库；本程序以 SUB 模式连接 `127.0.0.1` 上的 CAN 发布端。源码另有 PUB 发送 CAN 请求的函数，但对应线程在 `main.c` 中被注释，默认不会启动。 |
| MQTT、Mosquitto、主题、QoS | MQTT 是发布/订阅消息协议，Mosquitto 是本程序使用的客户端库。当前有效的上报主题是 `TBOX/REPORT`，JSON 数据发布时 QoS 为 0；连接身份和服务器地址来自设备配置。QoS 0 表示客户端发布时不要求协议层确认。 |
| JSON、cJSON、CBOR、schema | JSON 用于点表解析和当前上报载荷，cJSON 是处理 JSON 的库；CBOR 是二进制对象表示格式，schema 是字段模型描述。源码保留 CBOR/schema 函数，但当前 MQTT 发布分支只启用 JSON，不能把保留代码当作正在运行的功能。 |
| VIN、J1939 TP.BAM/TP.DT、PGN | VIN 是车辆识别码；源码可从 `0x18ECFFEE` 的广播传输管理帧（TP.BAM）和 `0x18EBFFEE` 的数据帧（TP.DT）重组 17 字节 VIN，并检查 PGN `0xFEEC`。这些是当前特殊报文解析分支，并不表示所有车辆都会提供该报文。 |
| `NOT_USED_SCHEMA` | 本目录 CMake 默认开启该选项并定义同名宏，跳过 MQTT 的 schema 订阅和初始 schema 发布。关闭该选项会走另一段条件编译代码；是否可构建及运行，需要结合当前头文件宏和依赖重新验证。 |
| INI、`iniparser` | INI 是设备配置文件格式，`iniparser` 负责读取。`/opt/conf.ini` 提供设备 ID，`/opt/conf_dima.ini` 提供 MQTT 服务端地址、端口及认证信息。 |
| `QL_MODULE_PLATFORM`、交叉编译 | `QL_MODULE_PLATFORM` 是交叉编译时选择目标平台库和宏的环境变量；交叉编译指在开发主机上用目标设备工具链构建程序。本目录 CMake 处理 `EC200A`、`EG25G`、`MCIMX6Y2CVM08AB`，其他平台值在交叉编译分支中会报错。 |
| 进程保活、CPActive | `appmng` 提供的进程状态机制。程序注册 10 秒超时，并约每 5 秒更新时间；这反映本地进程活动，不代表 CAN 或 MQTT 链路健康。 |

## 主要文件与职责

| 路径 | 职责 |
| --- | --- |
| `CMakeLists.txt`、`global_config.h.in` | 定义版本、依赖、平台选项、`dima_client` 与 `scene` 两个构建目标及安装规则。 |
| `main.c` | 单实例锁、日志、配置与点表加载、保活线程，以及 CAN/MQTT 线程启动。 |
| `src/can_mng/` | 读取设备 ID 和 MQTT 配置；初始化共享管理对象并加载场景库。 |
| `src/can_server/` | 订阅 CAN0/1/2，接收帧并交给点表解析和场景判断；保留 CAN 请求发送逻辑。 |
| `src/data_process/` | 解析点表、建立 CAN 规则、提取及换算数据项，解析软件版本和 VIN 特殊报文，按场景、间隔和变化阈值生成上报 JSON；也保留 CBOR/schema 相关实现。 |
| `src/scene/`、`src/scene_lib/` | 前者动态加载场景库；后者实现 `judge_scene` 并编译成 `libscene.so`。 |
| `src/mosquitto/` | 创建 MQTT 客户端、连接与回调、按点表间隔发布数据。 |
| `src/list/` | 数据规则使用的链表辅助实现。 |

## 启动与数据流

1. `main()` 用 `/tmp/dima_client.pid` 的文件锁限制同名实例，初始化 Mosquitto，并建立 CAN/数据处理管理对象；初始化过程尝试加载 `/usr/lib/libscene.so`。
2. 程序读取 `/opt/conf.ini` 的 `dev:id`，读取 `/opt/conf_dima.ini` 的 `remote:ip`、`remote:port`、`remote:username`、`remote:password`，再加载 `/opt/Pointsheet_info1.json`。配置或点表加载失败时，`main()` 会退出；场景库加载失败的返回值目前没有在 `can_mng_init()` 中检查，运行前应核对库是否实际可用。
3. 程序注册 CPActive 保活，启动保活、CAN 接收和 MQTT 三类线程。CAN 线程以 nanomsg SUB 连接本机 `16002`、`16003`、`16004` 端口，将收到的帧按通道与 CAN ID 匹配点表规则，并更新当前场景。特殊报文分支还会从 `0x1CFDD1FD` 提取软件版本写入应用数据项 `Sr0001`，从 J1939 多帧报文重组 VIN 写入 `Sr0002`；这两个数据项须在点表的 `app` 中提供相应名称。
4. MQTT 线程连接配置的服务端。连接成功后，按点表生成的间隔列表检查是否到期；当前发布路径只处理 `MSG_TYPE_JSON`，将包含 `body.id`、`items[].ts` 和 `items[].properties` 的 JSON 发布到 `TBOX/REPORT`。数据项还要满足场景匹配、已收到值和变化阈值条件；没有满足条件的数据项时，不生成该次 JSON 载荷。代码在构造 JSON 时更新数据项的上次上报时间和值，未等待 MQTT 对端确认，因此这些本地状态不能证明平台收到数据。
5. 保活线程检查点表文件的修改时间；检测到变化时调用 `exit(0)`，交由外部进程管理机制决定是否重启。程序本身没有在原进程内热加载点表。

## 关键配置与运行边界

| 位置或接口 | 当前源码约定 |
| --- | --- |
| `/opt/conf.ini` | `dev:id` 用作设备 ID，也用作 MQTT 客户端 ID 和 JSON 上报中的 `body.id`。 |
| `/opt/conf_dima.ini` | `remote:ip`、`remote:port`、`remote:username`、`remote:password` 决定 MQTT 连接；源码带有回退值，实际部署应以设备配置为准。当前配置读取函数还会把用户名和密码打印到标准输出，收集日志时应注意。 |
| `/opt/Pointsheet_info1.json` | 点表路径在 `main.c` 写定；解析器读取 `sce`、`dev`、`app`、`mtyp` 等字段，依点表建立规则与间隔。本项目目录没有设备点表；仓库 `todel/opt/device_apps/` 有其他设备的同名格式样例，不能直接视为现场使用文件。修改文件后程序会退出，由外部机制重启后重新加载。 |
| `tcp://127.0.0.1:16002`～`16004` | 分别订阅 CAN0、CAN1、CAN2；依赖其他本机进程提供发布端。 |
| MQTT 主题 `TBOX/REPORT` | 当前有效的 JSON 上报主题；头文件中另有被 `#if 0` 屏蔽的主题定义，不能按那些名称配置当前服务。 |
| `/usr/lib/libscene.so` | 运行时固定加载路径；CMake 把生成的场景共享库安装到 `usr/lib`。缺失或符号加载失败时，后续 CAN 场景调用可能无法正常工作。 |
| `/tmp/dima_client.pid` | 单实例文件锁所用路径。 |

## 构建与维护

- 本目录要求 CMake 3.11、C99；顶层构建时需显式启用 `WITH_DIMA_CLIENT=ON`。CMake 查找 nanomsg 1.2、OpenSSL 1.1.1、cJSON 1.7.15、appmng 1.0、Mosquitto 2.0.15，并链接 cn-cbor、tbox-common、pthread 等库。目标平台还需对应 SDK 库与工具链。
- 安装规则将 `dima_client` 放到安装前缀的 `usr/bin`，`libscene.so` 及若干依赖共享库放到 `usr/lib`。仅编译通过不能证明目标设备上的 CAN 发布端、MQTT 服务端、点表和场景库均可用。
- 修改 CAN 点表字段时，联查 `load_pointsheet_json()`、`parse_common_msg()`、`parse_special_msg()` 和实际设备点表；修改场景规则时，联查 `scene_judge.c` 中的 CAN ID、位偏移和阈值，以及点表中的场景名。
- 默认 `NOT_USED_SCHEMA=ON` 时，点表 `app` 中名称为 `dima_client` 的应用数据项须有 `cbidx` 字段，否则解析代码会退出；软件版本和 VIN 上报还须存在对应的 `Sr0001`、`Sr0002` 应用项。普通 CAN 数据项由 `dev` 中的通道、报文规则和 `pts` 列表定义。部署前应以目标设备的真实点表验证这些约定。
- 修改上报格式或频率时，联查 `json_work_data_generate()`、点表间隔列表和 `start_mqtt_task()`。当前源码仅启用 JSON 发布；CBOR/schema 逻辑和其他主题处于条件编译或屏蔽代码中。
- 本目录未见专用自动化测试目标。验证时先核对 CMake 配置与目标平台编译，再在具备本机 CAN 服务、配置文件、场景库和 MQTT 服务端的设备上核对解析、场景切换、上报间隔与点表改动后的重启。
