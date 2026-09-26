# cloud_client 项目说明

## 项目位置与用途

- Git 仓库根目录：`/home/tronlong/lyp/code/rtms_sdk`；源码目录：`apps/cloud_client/`；本文档目录：`/home/tronlong/lyp/perCode/idevelop_md/rtms_sdk/Jinke/cloud_client/`。这是 `rtms_sdk` 仓库内的应用，没有独立的 Git 分支。
- 整理本文档时，仓库分支为 `develop/rtms_sdk_v1.3_20240408`，提交短哈希为 `bc60961e`（由 `git branch --show-current`、`git rev-parse --short HEAD` 核对）。切换分支或更新源码后，应重新核对下文的配置与行为。
- 本目录构建设备侧进程 `cloud_client`：连接本地 MQTT 服务和远端云平台，转发实时数据与云端指令/回复，处理设备注册、云端对时，并尝试把离线工况写入 SD 卡上的 SQLite 数据库供恢复后补传。它是通信桥接进程；原始 CAN 采集和各类业务指令执行依赖其他进程。
- `apps/CMakeLists.txt` 中 `WITH_CLOUD_CLIENT` 默认 `OFF`，须显式启用才纳入 SDK 构建。本目录 `CMakeLists.txt` 声明项目版本 1.5，安装目标为安装前缀下的 `opt/cloud_client`。

### 相关技术与名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| RTMS SDK、Git 分支、CMake | `rtms_sdk` 是包含多个设备应用的源码仓库；分支表示整仓源码版本。CMake 根据顶层及本目录的 `CMakeLists.txt` 生成编译与安装规则，`WITH_CLOUD_CLIENT` 控制是否构建此应用。 |
| T-Box、`cloud_client` | T-Box 通常指车载或设备侧通信终端。本进程是 T-Box 的云通信桥接组件，具体硬件与部署位置由目标设备决定。 |
| MQTT、Broker、主题、QoS | MQTT 是发布/订阅消息协议；Broker 是消息服务端；主题是消息路由名称。程序分别连接本地与远端 Broker，使用 Mosquitto 客户端库；QoS 是 MQTT 的传输确认级别，实际发布级别因消息路径而异。 |
| 本地消息、远端消息 | 本地主题如 `local/data/general_data`、`local/data/gps_data` 来自设备其他进程；远端主题如 `v4/p/post/thing/live/json/1.0`、`v4/s/post/device/cmd/json/1.0` 用于云平台上报和下发。程序按主题映射或转发，并不是所有主题都在本进程解析执行。 |
| JSON、cJSON、CBOR、schema、gzip | JSON 是配置/指令/部分上报的文本格式，cJSON 用于解析和生成；CBOR 是二进制数据格式，schema 是其字段模型；gzip 是压缩格式。源码按本地主题选择 JSON、CBOR 或压缩后的云端主题；schema 上报受 `NOT_USED_SCHEMA` 条件编译控制。 |
| nanomsg、PUB/SUB、REQ/REP | nanomsg 用于本机进程间通信。程序订阅命令回复、发布 MCU 命令，并以 REQ/REP 向本机网络服务查询蜂窝网络状态；它与 MQTT 是两套不同的消息通道。 |
| 设备注册、认证信息、MD5 | 程序先尝试 `/opt/conf2.ini` 中已保存的认证（若存在），再尝试 `/opt/conf.ini` 的设备 ID/密钥；连接被拒且候选凭据都失败后，满足机器序列号和模型 ID 条件才启动注册。注册响应中的用户名和密码会保存到 `/opt/conf2.ini`。注册请求使用租户、模型和设备信息构造标识，并用 MD5 计算注册凭据；MD5 是此处协议约定中的摘要算法。 |
| 国内/海外域名、PLMN、MCC | `/opt/conf_ext2.ini` 的 `domain_select:type` 可选 `domestic`、`oversea`、`auto`。自动模式借助蜂窝网络 PLMN 或运营商信息判断区域；MCC 是 PLMN 中的移动国家码，源码将前缀 `460` 判作国内。不同平台宏使用不同的网络信息获取路径。 |
| SQLite、WAL、离线补传 | SQLite 是嵌入式数据库；WAL 是其预写日志模式。SD 卡可用时，程序把部分离线工况缓存到 `mqtt_history.db`，恢复连接且 schema 上报标志置位后才进入补传循环。删除以本地 `mosquitto_publish` 返回成功为条件，不等于云端业务确认。 |
| 云端对时、RTC、MCU | 程序通过 MQTT 请求云端时间，收到响应后设置系统时间，并按平台分支尝试同步 MCU 或设备 RTC。MCU 指微控制器；RTC 指实时时钟。首次成功对时还会触发 `can_client`、`alarm_client` 重启命令。 |
| `QL_MODULE_PLATFORM`、交叉编译 | 交叉编译指在开发主机上用目标设备工具链生成程序。CMake 按 `QL_MODULE_PLATFORM` 选择 SDK 头文件、库和编译宏；本目录支持 `EC200A`、`EG25G`、`AG35GL`、`MCIMX6Y2CVM08AB`。 |
| `CPActive`、保活 | `appmng` 的进程活动登记机制；程序注册 10 秒超时并约每 5 秒更新时间。它反映进程定期运行，不能单独证明本地或云端链路可用。 |

## 主要文件与职责

| 路径 | 职责 |
| --- | --- |
| `CMakeLists.txt`、`global_config.h.in` | 定义版本、平台宏、依赖、构建目标和安装规则。 |
| `main.c` | 单实例文件锁、信号处理、初始化、线程启动和 CPActive 保活。 |
| `src/cloud_dev/` | 设备状态和配置对象初始化、SD 卡/数据库准备、国内或海外域名选择。 |
| `src/common/` | 读取设备 ID、密钥、域名和认证配置；时间、压缩、MD5、设备型号及状态文件等辅助函数。 |
| `src/mosquitto/` | 本地、远端和注册 MQTT 客户端；主题订阅、实时转发、指令回复及历史补传。`client_shared.c` 提供客户端选项/连接辅助。 |
| `src/json_parse/` | 云端时间、注册结果和部分云端命令的 JSON 解析；标准信息与对时请求生成。 |
| `src/nn_sock/` | 本机 nanomsg 命令和网络信息通道。 |
| `src/db_manage/`、`src/sdcard_check/` | SQLite 离线缓存及 SD 卡挂载、可写和剩余空间检查。 |
| `src/base64/`、`src/md5/`、`src/posix/`、`src/sany_log/` | 编码、摘要、POSIX 平台和日志辅助实现。 |

## 启动与主要数据流

1. `main()` 使用 `/tmp/cloud_client.pid` 文件锁限制同名实例，注册 CPActive，写入初始云状态，初始化 Mosquitto，再读取设备配置并建立 nanomsg 连接。`/opt/conf.ini` 的 `dev:id` 和 `dev:secret` 缺失时初始化失败。随后启动 nanomsg 接收、本地 MQTT、注册、远端 MQTT、指令回复、历史补传和保活线程。
2. 本地 MQTT 地址和端口从 `/opt/conf_ext2.ini` 读取，字段未配置时分别回退到 `127.0.0.1`、`1883`；订阅工况、定位、schema、系统信息及各类回复主题。远端 MQTT 使用所选国内/海外 Broker 和认证信息；云端连接后订阅对时、指令、配置、文件、OTA、锁及实时数据相关主题。整个配置文件加载失败时，读取函数会报错，但 `dev_info_init()` 当前没有检查其返回值，不能把字段默认值理解为文件缺失时仍可正常连接。
3. 本地工况在远端在线且对时完成后进入实时发送队列；根据本地主题映射到云端 JSON、CBOR 或 gzip 主题。远端已连接但尚未完成对时时，本地回调直接丢弃 schema、系统信息以外的消息，既不进入实时队列，也不进入离线缓存。云端下发的多数业务消息转发给本地 MQTT；本进程直接处理对时、注册结果以及 `REBOOT`、`SCHEMA`、`CUSTOM` 等部分指令，并把本地回复发布回云端。
4. 远端离线时，部分本地工况进入内存历史队列；SD 卡检查通过时，队列达到 10 条会批量写入 SQLite；SD 卡不可用时只保留内存队列，达到 100 条会淘汰旧消息。连通并满足 schema 上报标志后，历史线程尝试从数据库和内存队列补传。告警等其他消息的离线存储在源码中仍标为 TODO。
5. 程序通过云端时间主题请求对时，首次成功同步后才开放实时工况发送；定时器还会继续请求对时。云连接状态写入 `/tmp/cloud_status`；`SIGINT`/`SIGTERM` 会执行客户端清理，`SIGALRM` 分支执行系统 `reboot`。

### 主要主题映射

| 本地输入或云端输入 | 本进程处理及输出 |
| --- | --- |
| `local/data/general_data`、`local/data/gps_data` | 实时映射到 `v4/p/post/thing/live/json/1.0`；历史补传映射到 `v4/p/post/thing/history/json/1.0`。 |
| `local/data/cbor_general_data` | 实时映射到 `v4/p/post/thing/live/cbor/1.1`；历史补传映射到 `v4/p/post/thing/history/cbor/1.1`。 |
| `local/data/gz_general_data` | 先用 gzip 压缩；成功时分别发实时/历史 `jsongz/1.1` 主题，压缩失败时回退到 JSON 1.1 主题。历史 JSON 1.1 的源码常量是 `v4/post/thing/history/json/1.1`（比相邻历史主题少 `/p/`），需与云平台协议核对。 |
| `local/data/schema`、云端 `v4/s/get/cloud/time/json/1.0` | 前者更新本地 schema 缓存，供远端上报；后者由本进程解析并设置时间。 |
| 云端 `v4/s/post/device/cmd/json/1.0` | 本进程解析 `REBOOT`、`SCHEMA`、`CUSTOM` 等命令及可选回复，并将原指令转发给本地 MQTT。其余订阅的 OTA、锁、文件、配置和实时数据命令主要走本地转发路径。 |
| 本地的 OTA、锁、文件、配置、实时数据及命令回复主题 | 本地客户端收到后排入回复队列，由回复线程以原主题发往远端；具体业务执行由产生回复的本地进程负责。 |

头文件还定义了 `local/data/event` 的历史映射，但本地 MQTT 连接回调没有订阅这一主题；仅凭映射函数不能认定告警离线补传已经生效。上述映射依据当前回调和发布分支，具体端到端数据格式还需与生产者及云平台协议核对。

实时工况和历史补传的发布调用使用 QoS 0；命令回复、对时、系统信息、标准信息及 schema 等路径使用 QoS 1。QoS 1 也仅提供 MQTT 协议层确认，不代表云端业务处理成功。实时发送队列最多保留约 100 条，达到上限时移除旧消息；离线队列在 SD 卡可用时满 10 条才写库，因此未凑满一批的消息仍在内存中，掉电后无法依靠数据库恢复。

## 配置与运行边界

| 位置或接口 | 当前源码约定 |
| --- | --- |
| `/opt/conf.ini` | `dev:id`、`dev:secret` 是启动必需的设备身份字段。密钥属于敏感信息，不应复制到文档或日志样例。 |
| `/opt/conf_ext2.ini` | `mgw:domain_oversea`、`mgw:domain_domestic`、`mgw:port`、`mgw:local_ip`、`mgw:local_port` 配置 Broker；`register:domain_oversea`、`register:domain_domestic`、`register:port`、`register:machinelink_tenant_id`、`register:rootcloud_tenant_id` 配置注册；`domain_select:type` 选择区域策略；`log:log_level` 控制日志级别。多数字段有代码默认值，但加载整个文件失败时的行为需另行处理。 |
| `/opt/conf2.ini`、`/opt/device_type.ini`、`/opt/device_model.json`、`/opt/machine_sn` | 分别涉及已保存的云连接认证、设备型号、型号映射和设备序列号；注册是否成功还取决于云端服务与这些数据。 |
| 本地 MQTT 服务 | 配置字段缺失时默认 `127.0.0.1:1883`，实际地址和端口来自配置。必须有本地 Broker 及产生工况、schema、系统信息和回复的其他进程，转发链路才有数据。 |
| nanomsg `16008`、`26008`、`38001` | 分别是命令回复 SUB、命令请求 PUB、网络信息 REQ 所连接的本机端口。源码还定义其他 CAN/GPS 端口常量，但本程序初始化时只连接这三个。 |
| `/media/sdcard/mqtt_history.db` | SQLite 离线缓存；使用前要求 SD 卡真实挂载、可写且空闲至少 500 MiB。数据库模块还有占用率和剩余空间检查。 |
| `/opt/cert/ca.crt`、`client.crt`、`client.key` | 远端 Broker 端口为 `8883` 且编译启用 `WITH_TLS` 时使用；本目录 CMake 定义了 `WITH_TLS`。其他端口不能仅凭该宏推断已启用 TLS。 |
| `/tmp/cloud_status`、`/tmp/system_is_rebooting` | 云状态文件中启动时写 `0`，断开/清理时写 `1`，远端连接成功时写 `2`；后者在处理重启指令时写 `1`。具体消费者需看设备其余进程。 |

## 构建与维护提示

- 本目录要求 CMake 3.11；顶层构建需启用 `WITH_CLOUD_CLIENT=ON`。查找 libev 4.33、nanomsg 1.2、OpenSSL 1.1.1、cJSON 1.7.15、appmng 1.0、tbox-common 1.0、Mosquitto 2.0.15，并链接 SQLite、zlib、pthread 等库。交叉编译还须对应平台 SDK 和环境变量。
- `NOT_USED_SCHEMA` 默认 `OFF`，远端发送线程会等待本地 schema 并尝试上报。历史补传线程无条件等待 `cbor_schema_report`，即使把 `NOT_USED_SCHEMA` 设为 `ON` 也要核对这条等待路径；不要仅按选项名称推断历史补传一定可运行。
- 国内/海外 `auto` 策略最长等待约 10 分钟；非蜂窝 SDK 宏路径通过本机 nanomsg 网络信息读取 PLMN/运营商，`EG25`、`AG35GL`、`EC200A` 路径调用模组 API 读取 MCC；超时或配置不合法时回退海外。`main()` 当前未检查 `select_domain_cfg()` 返回值，排查连接问题时应看日志中的实际所选域名。
- 历史线程直接调用数据库读取函数，而数据库句柄仅在 SD 卡检查通过后创建。无 SD 卡场景虽然设有内存队列，仍需在目标设备验证该线程的实际行为；维护时优先检查空句柄与补传条件。
- 数据库从 `mqtt_cache` 取一条时使用 `SELECT * ... LIMIT 1`，没有显式 `ORDER BY`；删除也按子查询 `LIMIT` 而未显式排序。不能把源码当前行为当作严格的先进先出保证。历史发布函数对未识别的主题初始返回值为成功码，若数据库里存在这样的主题，还可能被当作已发送而删除。
- 设备注册还要求 `/opt/machine_sn` 可读取且模型 ID 有效；`/opt/device_type.ini` 的型号通过 `/opt/device_model.json` 映射为模型 ID。若已有候选凭据未全部收到“连接拒绝”，注册线程可能继续等待，而不会立即注册。排查时区分网络连接失败、认证被拒和注册条件缺失。
- 当前源码会在部分调试/信息日志中输出设备密钥、MQTT 密码或注册口令；收集与共享日志时应脱敏。源码中的文件和主题路径是程序约定，实际服务地址、证书和数据生产者仍以部署设备为准。
- `mosquitto_publish` 返回成功只表示消息被客户端库接受；数据库行和内存队列会据此移除，不能据此认定云平台已处理消息。修改补传可靠性时，应联查 QoS、发布回调、数据库删除时机和断线恢复逻辑。
- 云端 `REBOOT` 指令会安排 10 秒后系统重启，对时首次成功会执行 `killall can_client alarm_client`。验证这些路径必须在可控设备环境进行。当前目录未见专用自动化测试目标；可以先核对 CMake 构建，再在具备本地 Broker、云服务、配置文件及 SD 卡的设备上验证转发、注册、对时和补传。
