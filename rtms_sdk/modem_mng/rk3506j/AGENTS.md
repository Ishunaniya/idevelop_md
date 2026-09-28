# modem_mng / rk3506j 项目说明

## 项目位置与用途

- **源码仓库**：`/home/tronlong/lyp/code/rtms_sdk`；项目根目录：`apps/modem_mng/`；RK3506J 平台实现：`apps/modem_mng/rk3506j/`。**本文档仓库**：`/home/tronlong/lyp/perCode/idevelop_md`；本文档位置：`rtms_sdk/modem_mng/rk3506j/AGENTS.md`。文档用于理解和维护源码，不参与编译。
- 整理本文档时，源码仓库分支为 `develop/rtms_sdk_v1.3_20240408_dc_switch`，提交为 `e15a52329cf9cf8e19983c92da6f1124fb3d76e7`；文档仓库分支为 `master`，编写前提交为 `4c54edf`。这些分支属于各自的**整个 Git 仓库**，不是 `rk3506j/` 子目录的独立分支；切换分支或更新提交后，应重新核对本文与源码。
- 本平台代码构建并安装设备侧进程 `modem_mng`。进程在 RK3506J 主控上管理外接移远 EG912 或 EC200A 蜂窝模组，识别 USB 设备、AT 串口和数据网卡，检查 SIM/注册状态，配置或复用 PDP 数据连接，取得主机 IP，监测联网并执行故障恢复；同时通过本机消息接口提供网络状态和请求服务。
- 顶层 `apps/CMakeLists.txt` 的 `WITH_RTMS_CORE` 默认 `OFF`，启用后才纳入 `modem_mng`；`apps/modem_mng/CMakeLists.txt` 在交叉编译且 `QL_MODULE_PLATFORM=RK3506J` 时选择这里的 C++14 源码，生成的程序安装到 `usr/bin/modem_mng`。`rk3506j` 是平台实现目录，`RK3506J` 是构建选择值，`modem_mng` 是进程名。`version.h` 中当前运行时显示版本为 `rtms_rk3506j_1.28.6`；CMake 的 `project(modem_mng VERSION 1.0)` 是工程元数据。

### 相关技术与名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| RTMS SDK、Git 分支、提交 | `rtms_sdk` 是包含多个设备应用的源码仓库。分支是仓库的开发线，提交哈希标识本文核对的源码快照；文档仓库的 `master` 不代表源码仓库也在 `master`。 |
| RK3506J、交叉编译、CMake | RK3506J 是目标主控平台。交叉编译是在开发主机上使用目标架构工具链生成设备程序；CMake 按 `QL_MODULE_PLATFORM=RK3506J` 选择源文件、依赖和安装规则。 |
| EG912、EC200A、USB 枚举、VID/PID | EG912 和 EC200A 是本平台支持的外接移远蜂窝模组。USB 枚举指内核识别模组并生成串口、网卡；VID/PID 是 USB 厂商/产品标识。程序从同一 USB 拓扑关联 AT 口和网卡，并可向 `option` 串口驱动注册设备 ID；实际节点取决于模组、内核和枚举结果。 |
| AT 指令、AT 口、URC、端口所有权 | AT 指令经 `/dev/ttyUSB*` 控制或查询模组；URC 是模组异步上报。`at_channel` 负责串行化 AT 会话，进程启动时还要取得 AT 口所有权，避免另一进程同时控制。EG912 通常区分主 AT 口与辅助口，EC200A 通常使用 `ttyUSB1`；以实际发现和配置为准。 |
| SIM、`CPIN`、`CEREG`、`COPS`、PLMN | SIM 提供蜂窝用户身份；`AT+CPIN?` 查询卡状态，`AT+CEREG?` 查询网络注册，`AT+COPS` 查询或选择运营商。PLMN 是移动国家码与移动网络码组成的运营商标识。注册成功还不等于主机已联网。 |
| APN、PDP、CID、`CGACT`、`QNETDEVCTL` | APN 是运营商数据接入点；PDP 是模组的分组数据上下文；CID 是上下文编号。代码默认围绕 CID 1，通过 `CGACT` 检查/激活上下文，并用移远 `QNETDEVCTL` 控制或查询模组到主机的数据通路。 |
| ECM、DHCP、`udhcpc`、默认路由 | ECM 是 USB 以太网数据接口模式；`udhcpc` 为模组网卡取得 IPv4 参数。主机还需有匹配网卡的默认路由，才能把普通外发流量交给该接口。DHCP 成功、PDP 已激活和外网可达是不同状态。 |
| MQTT、保留网络、网络唤醒 | MQTT 是发布/订阅消息协议。目标场景中 MCU 可关闭 RK/Linux，而模组仍保持 PDP 和模组内 AT MQTT 会话；RK 再启动时，程序查询 `QMTCONN`、`QMTCFG=pdpcid`、`CEREG`、`CGACT` 和 `QNETDEVCTL`，尝试复用已存在的连接，避免破坏远控下行。网络唤醒事件采用本机 nanomsg 消息，但当前入口不依赖该事件判断冷启动。 |
| AT 端口缓存、`/opt/modem_at.state` | 发现并验证模组类型、USB 身份和 AT 端口后，程序可保存映射供后续启动加速；复用前仍核对 USB 拓扑和 AT 响应。旧缓存不能代替现场枚举检查。 |
| 联网探测、分级恢复 | 程序检查接口、IP、路由并尝试探测外部地址；成功只说明指定路径可达。故障恢复先处理 DHCP/主机网卡，再尝试 AT 数据连接恢复、模组重置，必要时按条件切换运营商或重新进入恢复循环；高影响操作可能打断现有远控会话。 |
| PWRKEY、`CFUN`、硬件恢复 | PWRKEY 是模组电源控制引脚；`CFUN` 控制模组功能/射频状态。它们会影响蜂窝注册与连接，因此复用保留网络的启动路径会限制破坏性操作；物理断电还受 `MODEM_PHYSICAL_POWER_CYCLE` 配置控制。 |
| nanomsg、REQ/REP、PUB/SUB、CPActive | nanomsg 用于本机进程间消息。`38001` 是请求/应答接口，`48001` 是网络事件发布接口；CPActive 是 `appmng` 的进程活动登记/刷新机制。接口可用或进程活跃不等于蜂窝链路可用。 |
| `C020102`、SQLite、流量监测 | `/etc/config/config.json` 的 `C020102` 被读为流量上限，读取失败时回退 `0xffff`。公共 `traffic_sql`/`TrafficMonitor` 模块使用 SQLite 支持流量记录；该配置的业务单位应以设备配置约定核对。 |

## 主要文件与职责

| 源码路径（相对 `apps/modem_mng/`） | 职责 |
| --- | --- |
| `CMakeLists.txt`、`rk3506j/version.h` | 选择 RK3506J 构建、依赖、安装位置及运行时版本。 |
| `rk3506j/main.cpp`、`rk3506j/service/modem_service.hpp` | 进程入口、AT 所有权、日志/LED、CPActive、nanomsg 服务、流量监测及退出处理。 |
| `rk3506j/dial/rk3506j_dialer.cpp/.hpp` | EG912/EC200A 拨号状态机、保留网络复用、联网检查和故障恢复。 |
| `rk3506j/startup/retained_network_probe.cpp/.hpp`、`network_wake_event_receiver.cpp/.hpp` | 解析模组保留状态；解析网络唤醒事件。当前入口绕过事件等待，以模组状态为启动依据。 |
| `rk3506j/device/modem_device.cpp/.hpp`、`at_port_cache.cpp/.hpp` | USB 拓扑发现、`option` 驱动绑定、已验证 AT 端口映射缓存。 |
| `rk3506j/at/at_channel.cpp/.hpp`、`quectel_parser.cpp/.hpp` | AT 事务、响应与 URC 解析。 |
| `rk3506j/nw/network_configurator.cpp/.hpp`、`state/` | DHCP、IP/路由/探测及供其他模块读取的状态快照。 |
| `rk3506j/scripts/` | 历史启动与诊断脚本；RK3506J 生产安装规则不安装这些脚本，拨号及 AT 所有权由 `modem_mng` 管理。 |

## 启动与联网流程

1. `main()` 初始化日志并记录版本，取得 AT 口进程所有权，初始化网络 LED，将启动模式设为 `UNKNOWN`，然后进入公共服务。未收到网络唤醒事件不被当作冷启动证据。
2. 公共服务登记 CPActive（30 秒超时），在后台尝试建立 `tcp://127.0.0.1:48001` 发布端和 `tcp://127.0.0.1:38001` 请求端；早期回环接口未就绪时按约 1 秒间隔重试，拨号继续进行。它还读取流量上限并启动拨号线程。
3. 拨号器先识别同一 USB 模组对应的 AT 口与网卡，可复用经校验的端口缓存。每次 Linux 启动先查询模组内 MQTT、网络注册、PDP 和数据通路状态；约 10 秒的复用窗口内，仅允许对已有 PDP 执行必要的 `QNETDEVCTL` 接入、网卡启用和 DHCP，随后检查 IP、路由和绑定网卡的外网探测。
4. 若发现可复用的模组连接但主机侧接入失败，先进入软件恢复，硬件恢复暂受保护；未发现可复用连接时，才准备完整拓扑并按模组类型分派。EG912 使用专门的精简拨号路径，EC200A 使用完整拨号状态机，模组类型未知时走通用完整 AT 流程。确认 AT/SIM/注册状态后，按配置处理 APN/PDP、激活数据通路、运行 DHCP，并检查主机网络和外部可达性。成功时写入 `/tmp/dial_success`；失败时进入分级恢复。
5. 服务将发现的网卡交给本机请求接口和流量监测。收到 `SIGINT`/`SIGTERM` 时请求拨号线程退出，停止监测和消息线程，并关闭网络 LED。

## 配置与运行边界

| 位置或选项 | 当前源码约定 |
| --- | --- |
| `QL_MODULE_PLATFORM=RK3506J` | 选择本平台 C++14 构建；目标程序安装到 `usr/bin/modem_mng`。交叉编译须配套 RK3506J 工具链和依赖库。 |
| `WITH_RTMS_CORE=ON` | 顶层应用构建开关，默认 `OFF`；需要它把 `apps/modem_mng/` 纳入构建。 |
| `MODEM_TYPE`、`MODEM_USB_VENDOR`、`MODEM_USB_PRODUCT` | 可显式选择 EG912/EC200A 类型及 USB 标识；默认发现仍须与实际设备枚举和驱动绑定核对。 |
| `MODEM_AT_PORT`、`MODEM_AT_PRIMARY`、`MODEM_AT_AUX` | 覆盖自动发现的 AT 口。板上常见布局为 EG912 主口 `ttyUSB0`、辅助口 `ttyUSB5`，EC200A 主口 `ttyUSB1`；固定节点前先核实同一 USB 拓扑，避免把其他串口当作蜂窝模组。 |
| `MODEM_NET_IFACE` | 覆盖自动发现的数据网卡。代码优先使用同一模组 USB 拓扑发现的网卡，板级回退值为 `eth1`；网卡名及路由应在目标设备核对。 |
| `MODEM_PDP_APN`、`MODEM_PDP_TYPE` | 可覆盖运营商接入点和 PDP 类型；未指定 APN 时保留模组原有 PDP 配置。指定 APN 而不指定类型时，代码使用 `IPV4V6`；实际设置前会校验字段并考虑保留连接保护。 |
| `MODEM_PHYSICAL_POWER_CYCLE` | 仅值为 `1`、`true` 或 `TRUE` 时允许物理 PWRKEY 关机动作；代码还要求模组下电到再次上电至少间隔 300 秒。涉及休眠后保留连接的设备，应结合实际电源时序验证。 |
| `/etc/config/config.json`、`C020102` | 流量上限配置；读取失败时程序使用 `0xffff`。 |
| `/tmp/modem_mng_at.lock`、`/opt/modem_at.state` | 前者用于进程级 AT 口所有权，后者保存经验证的 AT 端口映射；启动异常时分别核对进程占用和缓存对应的 USB 身份。 |
| `/tmp/dial_success`、`/tmp/dial_version`、`/var/run/udhcpc.<网卡名>.pid` | 分别是当前拨号成功标记、版本展示文件和 DHCP 客户端进程文件；前两者位于临时目录，不是持久化历史记录。 |
| `223.5.5.5` | 源码中的外网 ICMP 探测目标，调用时指定蜂窝网卡；探测失败可能是目标地址或网络策略所致，不能单独证明 PDP 未激活。 |
| `tcp://127.0.0.1:38001`、`tcp://127.0.0.1:48001` | 本机 nanomsg 请求端与事件发布端；启动早期不可用会重试，不应据此直接认定拨号失败。 |

## 构建与核对

- `apps/modem_mng/CMakeLists.txt` 提供 `MODEM_MNG_BUILD_RK3506J_UNIT_TESTS` 开关，覆盖 AT 通道、AT 端口缓存、USB 设备发现和保留网络状态解析；交叉编译所得 ARM 测试程序需在 RK3506J 或适当仿真环境运行，默认不安装到生产镜像。
- 该 CMake 还声明 libev、nanomsg、OpenSSL、tbox-common、cJSON、uthash、appmng、ledgpio-control 等依赖，并链接 pthread、SQLite 等库。依赖版本及工具链路径以对应源码提交和构建环境为准。
- 本文按上述两个仓库的当前分支和静态源码整理，未在目标板验证模组固件、USB 枚举、SIM、APN、PWRKEY 时序、网络唤醒与云端 MQTT 的端到端行为。实际调试时先核对源码提交、板卡接线和日志中的模组类型、AT 口、网卡及保留连接判断。
