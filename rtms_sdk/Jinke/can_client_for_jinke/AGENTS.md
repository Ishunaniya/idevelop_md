# can_client_for_jinke 项目说明

本文是项目概览；逐模块调用链、点表字段、消息格式、编译组合及设备排查见[全链路源码分析与流程图](can_client_for_jinke_全链路源码分析与流程图.md)。

## 项目位置与用途

- Git 仓库根目录：`/home/tronlong/lyp/code/rtms_sdk`；项目源码：`apps/can_client_for_jinke/`；本文档目录：`/home/tronlong/lyp/perCode/idevelop_md/rtms_sdk/Jinke/can_client_for_jinke/`。本项目是 `rtms_sdk` 仓库中的应用子目录，不是独立 Git 仓库。
- 整理本文档时，仓库分支为 `develop/rtms_sdk_v1.3_20240408`，跟踪 `origin/develop/rtms_sdk_v1.3_20240408`；当前提交短哈希为 `bc60961e`。分支属于整个 SDK 仓库；切换分支或更新提交后，应重新核对源码行为与本文。
- 本目录声明 C99、`can_client` 可执行程序（CMake 项目版本 1.5）和 `libscene.so` 场景判断共享库。程序从本机三个 CAN 消息通道取得帧，依据 JSON 点表解析数据、判断设备场景，按配置的时间间隔生成 JSON、名为 `JSONGZ` 的另一种 JSON 结构或 CBOR 数据，并向配置的 MQTT Broker 发布。`can_client_for_jinke` 是源码目录名，程序/构建目标名为 `can_client`。
- 顶层 `apps/CMakeLists.txt` 的 `WITH_CAN_CLIENT_FOR_JINKE` 默认关闭，开启后才纳入 SDK 构建。本目录的 `NOT_USED_SCHEMA` 与 `USE_JINKE` 也默认关闭：前者控制是否跳过 `GeneralParam.json` 并启用旧应用消息路径，后者与前者共同控制金科事件和 FEI4 统计；实际生效条件须同时看两个编译宏。不能仅凭目录名认定金科专有事件在默认构建中运行。
- 当前源码中 `can_data_t` 的声明受 `NOT_USED_SCHEMA && USE_JINKE` 保护，但 `can_mng.c` 无条件使用它；因此两个本目录选项都保持默认 `OFF` 时有明确的类型不可见编译问题。下文的默认路径说明描述代码分支，不能当作已编译或已部署的证明。

### 相关技术与名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| RTMS SDK、Git 分支 | `rtms_sdk` 是包含本程序和其他设备应用的仓库；分支标识所依据的源码版本，不是设备型号或程序版本。`develop/rtms_sdk_v1.3_20240408` 是当前检出的仓库分支，`1.5` 是本目录 CMake 声明的程序版本。 |
| CMake、交叉编译、EC200A、EG25G | CMake 生成构建规则；交叉编译是在开发主机上用目标设备工具链生成二进制。本目录交叉编译分支由 `QL_MODULE_PLATFORM` 选择 `EC200A` 或 `EG25G` 的宏、头文件和库，其他平台值会使配置失败。 |
| CAN、CAN ID、点表 | CAN 是控制器局域网；CAN ID 标识报文。本程序不直接打开 `/dev/can*`，而是通过本机 nanomsg 接收其他服务送来的 CAN 帧。`Pointsheet_info1.json` 按通道、CAN ID、位偏移、长度、字节序、缩放与上报间隔描述如何解析字段。 |
| nanomsg、SUB、PUB/SUB | nanomsg 是进程间消息库。这里的 `NN_SUB` 订阅 `127.0.0.1:16002`、`:16003`、`:16004` 上的 CAN0/1/2 帧；PUB/SUB 指发布者发送、订阅者接收的通信模式。实际 CAN 总线接入由其他服务负责。 |
| 场景、`libscene.so` | 场景是 `power_on`、`idle`、`work`、`drive` 等运行状态。`libscene.so` 导出 `judge_scene`，主程序用 `dlopen`/`dlsym` 加载，按 CAN ID `0x171`、`0x172` 的转速与速度判断场景；点表可按场景选择上报字段。 |
| MQTT、Mosquitto、Topic、QoS | MQTT 是发布订阅消息协议，Mosquitto 是本程序使用的客户端库；Topic 是主题路径，QoS 是消息交付等级。程序从 `/opt/conf_ext2.ini` 取得 Broker 地址，默认 `127.0.0.1:1883`，但配置可指向其他地址；默认 Schema 路径订阅获取 Schema 的主题。`mosquitto_publish` 调用成功不等于下游业务系统已确认处理。 |
| JSON、cJSON、Schema、CBOR、`JSONGZ` | JSON 是点表和通用配置格式，cJSON 用于解析和生成 JSON。代码内部的 Schema 项保存字段名、类型和 CBOR 索引；发布的 Schema 条目只写字段名与类型。CBOR 是紧凑二进制编码。`mtyp` 选择普通 JSON（0）、CBOR（1）或名为 `JSONGZ` 的 JSON 结构（2）。当前 `gz_work_data_generate()` 只返回未压缩的 JSON 字符串。 |
| `GeneralParam.json`、`Pointsheet_info1.json` | 默认编译路径先加载通用参数/Schema，再加载点表；两个文件都从可执行程序所在目录读取。`NOT_USED_SCHEMA=ON` 时跳过前者，但仍加载点表。源码目录内未提供这两个运行文件，部署时需另行提供。 |
| `NOT_USED_SCHEMA`、`USE_JINKE` | 两个本目录 CMake 选项和同名预处理宏。`NOT_USED_SCHEMA` 控制旧应用消息与事件路径，`USE_JINKE` 再控制金科专有事件和统计；两者都启用才会在 `main()` 启动金科相关线程。 |
| GPS、MCU、FEI4、ECD、SCR、NOx | GPS 提供定位；MCU 是微控制器；FEI4 是本项目旧路径中非道路国四相关模块名；ECD 是排放控制诊断，SCR 是选择性催化还原，NOx 是氮氧化物。这些代码或字段存在于可选扩展路径，不能据此推断默认构建已执行国四上报。 |
| `CPActive`、保活 | SDK 的进程管理接口；主程序登记 `can_client`，以 5 秒周期更新访问时间，登记超时值为 10 秒。它反映进程保活，不代表 CAN 或 MQTT 数据链路健康。 |

## 主要文件与职责

| 路径 | 职责 |
| --- | --- |
| `CMakeLists.txt`、`global_config.h.in` | 定义版本、平台、编译选项、依赖、`can_client`/`scene` 目标和安装位置。 |
| `main.c` | 初始化 Mosquitto、配置和进程保活，启动 CAN 接收、MQTT 和可选扩展线程。 |
| `src/can_mng/` | 保存共享上下文；从 `/opt/conf_ext2.ini` 读取 MQTT Broker 地址；定位程序目录并加载场景库。 |
| `src/can_server/` | 通过 nanomsg 订阅三个 CAN 通道，解析收到的帧并更新当前场景。 |
| `src/data_process/` | 解析通用参数与点表、提取 CAN 字段，按场景和周期生成两种 JSON 结构或 CBOR 数据及 Schema；`JSONGZ` 路径未实际压缩。 |
| `src/scene/`、`src/scene_lib/` | 动态加载场景库以及实现 `judge_scene`。 |
| `src/mosquitto/` | 连接配置的 MQTT Broker，发布数据/Schema，处理获取 Schema 的请求。 |
| `src/app_server/`、`src/fei4/`、`src/event/`、`src/queue/` | GPS/MCU 和金科事件等旧扩展代码；是否运行取决于编译选项与调用路径。 |
| `src/list/` | 数据规则使用的链表辅助代码。 |

## 启动与数据流

1. `main()` 初始化 Mosquitto 与共享状态。`can_mng_init()` 读取 `/proc/self/exe` 得到程序目录，尝试从该目录加载 `libscene.so` 和 `judge_scene`；随后从 `/opt/conf_ext2.ini` 读取 `[mgw]` 的 `local_ip`、`local_port`。配置文件打不开时主程序退出；键缺省时使用 `127.0.0.1:1883`。
2. 默认编译路径加载程序目录下的 `GeneralParam.json` 和 `Pointsheet_info1.json`；缺少或解析失败会退出。`NOT_USED_SCHEMA=ON` 时只加载点表。
3. 注册 `CPActive` 保活并启动线程。CAN 线程订阅本机三个通道，按点表解析字段，再经 `judge_scene` 更新场景；MQTT 线程连接配置的 Broker。
4. 默认路径连接后发布 CBOR Schema，并订阅 `local/data/get_schema`；收到请求时再次发布 Schema。按点表配置的间隔和 `mtyp` 生成数据，分别发布到 `local/data/general_data`、`local/data/gz_general_data` 或 `local/data/cbor_general_data`。其中 `gz_general_data` 目前仍发送普通 JSON 字节。
5. 同时启用 `NOT_USED_SCHEMA`、`USE_JINKE` 时，主程序另启动应用消息、日期检查与 FEI4 统计线程，并可向 `local/data/event` 发布事件。默认构建不启动这些线程。

### 点表解析与上报条件

- `Pointsheet_info1.json` 的顶层 `mver`、`sver`、`pver`、`mtyp` 分别提供机器版本、Schema 版本、点表版本和消息类型；`sce` 是场景列表，`dev` 保存 CAN 规则，`app` 保存应用侧数据项。`dev[].slot` 作为 CAN 通道索引（0～2），`dev[].msg` 包含按 CAN ID 匹配的规则；`app[].nm` 只接受 `can_client`。依据：`src/data_process/data_process.c:756` 起。
- 最外层规则的 `val` 用作 CAN ID 匹配键；虽然 `rls` 子规则也保存 `val`，当前递归提取代码没有再次比较子规则的值。`pts` 是最终字段。字段常用键：`nm` 名称、`ofs` 位偏移、`len` 位宽、`bo` 字节序、`sign` 符号、`sc` 比例、`pofs` 物理值偏移、`min`/`max` 限幅、`ptyp` 值类型、`sce` 适用场景、`scems` 上报间隔（毫秒）、`udif` 变化阈值。具体 JSON 必须与该设备型号点表一致；本源码目录未附带金科专用点表。
- CAN 消息处理先按 `slot` 和 CAN ID 查找规则，再按字段位偏移、字节序和有符号位读取原始值；数值字段计算 `原始值 × sc + pofs` 并按 `min`/`max` 限幅。上报时还要求当前场景、间隔匹配且该字段收到过消息。`udif=0` 时允许周期上报；非零时数值变化达到阈值或字符串变化才再次上报；首次收到的值可上报。依据：`set_common_data_item()` 与 `check_data_item_value_json/cbor()`。
- 当前 `scene_judge.c` 的硬编码规则使用 `0x171` 判断发动机转速、`0x172` 判断行驶速度：速度大于 0 为 `drive`，否则转速等于 0 为 `power_on`、不超过 800 为 `idle`、再高为 `work`。实际场景与点表列出的 `sce` 必须对应；该规则不能自动识别所有金科车型。

### MQTT 消息与交付边界

| `mtyp` | 发布主题 | 源码生成内容 |
| --- | --- | --- |
| 0 `MSG_TYPE_JSON` | `local/data/general_data` | `body.id` 为空字符串；`body.items[0]` 含毫秒时间戳 `ts` 和 `properties`。 |
| 1 `MSG_TYPE_CBOR` | `local/data/cbor_general_data` | CBOR 字节序列含协议前缀、流水号、Schema 版本、时间戳和字段索引映射。 |
| 2 `MSG_TYPE_JSONGZ` | `local/data/gz_general_data` | `body.things[0].id` 为空字符串，`items[0]` 含 `ts`、`properties`；源码未压缩。 |

数据主题的发布调用使用 QoS 0；Schema 主题 `local/data/schema` 使用 QoS 1，获取 Schema 的订阅也请求 QoS 1。`mosquitto_publish()` 返回成功只说明客户端接纳发布请求，不能据此认定 Broker 下游已经处理。源码没有历史数据持久化或业务应答闭环；数据字段没有满足条件时不生成工作数据包。

### 可选金科扩展

同时开启 `NOT_USED_SCHEMA`、`USE_JINKE` 后，`0x215` CAN 帧被解析为钻孔工单事件：工单号、计划深度、实际深度、耗时和成功标志入队，再按 `DrillingCompletionEvent` JSON 发布到 `local/data/event`。上一工单号和日期保存在 `/media/sdcard/jk_evt_no`，日期检查线程约每 10 秒检查一次，以日号变化重置工单号。旧路径还从 GPS/MCU 本机消息更新应用数据，并启用 FEI4 统计；具体报文处理以 `src/app_server/`、`src/fei4/` 为准。此分支依赖目标设备消息与存储资源，不能直接套用默认路径的点表/Schema 行为。

## 配置与运行依赖

| 资源 | 源码约定 |
| --- | --- |
| `/opt/conf_ext2.ini` | `[mgw]` 下的 `local_ip`、`local_port` 指定 MQTT Broker；默认地址是 `127.0.0.1:1883`，配置也可为其他地址。文件缺失时启动失败。 |
| 程序目录中的 `libscene.so` | `judge_scene` 动态库；主程序按自身实际安装目录拼出完整路径。 |
| 程序目录中的 `GeneralParam.json` | 默认路径的通用参数与 Schema 内容；启用 `NOT_USED_SCHEMA` 后不加载。 |
| 程序目录中的 `Pointsheet_info1.json` | CAN 点表、场景、消息格式和间隔；两条路径均需加载。源码目录未附金科专用实例，不能把仓库其他型号的点表当作本项目配置。 |
| `tcp://127.0.0.1:16002`～`16004` | 三个 CAN 通道的 nanomsg SUB 地址；需要其他设备进程发布 CAN 帧。 |
| MQTT `local/data/*` | 数据和 Schema 的主题名；名称中的 `local` 不限定 Broker 的部署位置，定义见 `src/mosquitto/client_mosquitto.h`。 |

构建要求 CMake 3.11、C99。CMake 查找 nanomsg 1.2、OpenSSL 1.1.1、cJSON 1.7.15、appmng 1.0、Mosquitto 2.0.15，并链接 cn-cbor、tbox-common、pthread 等。`can_client` 与 `libscene.so` 安装到安装前缀的 `opt/`；若改动运行位置，应使配置 JSON 和场景库与可执行文件同目录。交叉编译需设置 `QL_MODULE_PLATFORM=EC200A` 或 `EG25G` 及对应 SDK 环境。本目录没有自动化测试目标；本文依据静态源码与 Git 信息整理，未在目标设备验证编译和端到端上报。

## 排查提示

- 先确认实际构建选项、`/opt/conf_ext2.ini`、同目录的运行文件、本机 CAN 消息服务及配置地址上的 MQTT Broker，再看点表中的通道、CAN ID、场景与间隔。`mtyp` 决定发布格式，不同 Topic 不应混为同一消息流。
- `can_mng_init()` 未检查 `load_libscene()` 的返回值；若库加载失败，后续 CAN 帧处理仍会调用 `judge_scene` 指针。排查 CAN 线程异常时应先检查 `libscene.so` 是否存在及符号是否可加载。
- 当前源码的 `can_mng.c` 无条件使用 `can_data_t` 和 `can_data_init()`，而 `can_data_t` 在头文件中受 `NOT_USED_SCHEMA` 与 `USE_JINKE` 宏保护。默认选项下可能出现类型不可见的编译错误；实际构建前应核对所选宏组合和编译结果。
- 点表解析对 `dev[].slot`、`sce`、`scems` 等数组索引没有系统性的范围检查；错误点表可能造成越界或错误数据。部署配置前应检查通道是否为 0～2、场景与间隔项是否超过头文件的 `SCENE_MAX=10`，并确认字段偏移与 CAN 帧结构匹配。
- 主程序复用同一个 `pthread_t tid`，末尾只 `pthread_join` 最后赋值的线程 ID。该调用不能视为等待所有工作线程；线程启动返回值也未逐项检查。
