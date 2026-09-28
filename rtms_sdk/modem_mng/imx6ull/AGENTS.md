# modem_mng / imx6ull 项目说明

## 项目位置与用途

- 源码仓库为 `/home/tronlong/lyp/code/rtms_sdk`，源码根目录为 `apps/modem_mng/`，本平台的入口与实现位于 `apps/modem_mng/imx6ull/`。本文档位于另一个 Git 仓库 `/home/tronlong/lyp/perCode/idevelop_md` 的 `rtms_sdk/modem_mng/imx6ull/`，用于保存源码分析，不是编译输入。
- 编写本文时，**源码仓库**检出的分支是 `develop/rtms_sdk_v1.3_20240408_dc_switch`，提交是 `e15a52329cf9cf8e19983c92da6f1124fb3d76e7`；**文档仓库**检出的分支是 `master`，编写前提交是 `4c54edf`。Git 分支属于整个仓库，不属于 `imx6ull/` 子目录。切换源码分支或提交后，应重新核对本文所述行为。
- 该平台代码构建名为 `modem_mng` 的设备进程，运行在 NXP i.MX6ULL 主控上，管理通过 USB 接入的移远蜂窝模组：发现 AT 串口和网络接口、检查 SIM 与网络注册、配置 APN/PDP、建立数据连接、通过 DHCP 获取地址、检测外网，并在故障时分级恢复。它还向本机其他进程提供网络状态和请求接口。
- 顶层 `apps/CMakeLists.txt` 纳入 `modem_mng`；`apps/modem_mng/CMakeLists.txt` 在交叉编译且 `QL_MODULE_PLATFORM=MCIMX6Y2CVM08AB` 时选择 `imx6ull/` 源码。`imx6ull` 是平台目录名，`MCIMX6Y2CVM08AB` 是构建环境使用的平台值，二者不能互换。程序版本字符串来自 `imx6ull/version.h`，当前为 `rtms_imx6ull_1.25.1`；CMake 的 `project(modem_mng VERSION 1.0)` 是工程元数据，不是运行时展示版本。

### 相关技术与名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| RTMS SDK、Git 分支、提交 | `rtms_sdk` 是包含多个设备应用的源码仓库；分支表示整仓开发线，提交哈希标识本文核对的代码快照。本文档所在的 `idevelop_md` 是另一个仓库，其 `master` 不表示源码也在 `master`。 |
| i.MX6ULL、`MCIMX6Y2CVM08AB`、交叉编译 | i.MX6ULL 是设备主控平台；`MCIMX6Y2CVM08AB` 是本工程选择该平台代码的环境变量值。交叉编译是在开发主机上使用目标设备工具链生成 ARM 程序。 |
| CMake、`modem_mng`、平台目录 | CMake 读取构建脚本选出源文件和依赖，安装目标为 `usr/bin/modem_mng`。`imx6ull/` 是 `modem_mng` 的一种平台实现，不会生成名为 `imx6ull` 的独立进程。 |
| USB 枚举、VID/PID、`option` 驱动 | USB 枚举是内核识别模组并创建设备节点的过程；VID/PID 是厂商和产品 ID。代码默认使用移远 VID `2c7c`、PID `0901`，尝试写入 `option` 串口驱动的 `new_id` 节点，并从同一 USB 拓扑查找 AT 串口及网络接口。发现方式依赖目标内核驱动与模组实际枚举结果。 |
| AT 指令、AT 口、URC | AT 指令是主控经 `/dev/ttyUSB*` 或 `/dev/ttyACM*` 控制和查询模组的文本命令；URC 是模组主动上报。`at_channel` 串行化 AT 会话并处理响应，`quectel_parser` 解析移远响应；AT 可用不代表数据网络可用。 |
| SIM、`CPIN`、`CEREG`、`COPS`、PLMN | SIM 提供蜂窝用户身份；`AT+CPIN?` 查询卡状态；`AT+CEREG?` 查询分组网络注册；`AT+COPS` 关联运营商选择，PLMN 是由移动国家码和网络码组成的运营商标识。注册成功仍需激活数据连接。 |
| APN、PDP、CID、PDN | APN 是运营商数据接入点；PDP/PDN 指蜂窝分组数据连接上下文；CID 是上下文编号。仅显式设置 APN 时才改写 CID 1 的 APN/PDP 类型；随后通过移远 `QNETDEVCTL` 命令查询或启动数据连接。 |
| DHCP、`udhcpc`、默认路由 | `udhcpc` 为模组网络接口获取 IPv4 地址；默认路由决定未匹配目标的流量出口。进程同时检查地址和该接口的默认路由，不能只看 DHCP 进程是否存在。 |
| 外网探测、`SO_BINDTODEVICE` | 程序把 TCP 探测 socket 绑定到发现的蜂窝网卡，默认尝试 `1.1.1.1:443` 和 `8.8.8.8:443`。探测成功说明该接口到指定地址的 TCP 可达，不等于所有业务服务可用；探测目标可通过环境变量覆盖。 |
| L1 DHCP、L2 PDP、L3 CFUN、L4 硬件恢复 | 故障恢复阶梯：重新获取地址、重建数据连接、切换模组射频功能、通过设备 GPIO 对模组执行硬件电源操作。恢复级别按故障分类、尝试次数与冷却时间决定，硬件恢复会影响业务连接。 |
| nanomsg、REQ/REP、PUB/SUB | nanomsg 是本机进程间通信库。`NanoReqHandler` 通过本机 REQ/REP 端口回答模组/网络请求，`NetWork_EventPublisher` 发布网络事件；环回地址是本机接口，不是蜂窝运营商或云端地址。 |
| CPActive、流量监测、SQLite | CPActive 是 `appmng` 的进程活动登记/刷新机制；流量监测使用公共 `traffic_sql` 模块及 SQLite 保存或读取流量数据。进程活跃、流量记录和外网连通性是不同状态。 |
| `C020102`、JSON 配置 | `/etc/config/config.json` 中的 `C020102` 被读取为流量上限；读取失败时程序使用 `0xffff`。此键的业务单位及实际值须以设备配置和上游约定核对。 |
| IMSI、IMEI、ICCID、CSQ | IMSI 是移动用户标识，IMEI 是模组设备标识，ICCID 是 SIM 卡标识；CSQ 是 AT 信号质量读数。它们由状态快照或本机请求接口提供，不能互相替代，也不宜在共享日志中直接暴露完整身份值。 |
| CFUN、GPIO、冷却时间 | `CFUN` 是模组功能状态控制指令；GPIO 是主控的通用输入输出引脚，本平台用它控制模组电源和开机脚。冷却时间用于限制高影响恢复动作的频率。 |

## 主要文件与职责

| 源码路径（相对 `apps/modem_mng/`） | 职责 |
| --- | --- |
| `CMakeLists.txt` | 按 `QL_MODULE_PLATFORM` 选择平台源码、依赖、编译选项及安装目标。 |
| `imx6ull/main.cpp`、`imx6ull/version.h` | 初始化日志、写出显示版本，解析首次拨号超时，进入公共服务；定义运行时版本。 |
| `imx6ull/service/modem_service.hpp` | 注册 CPActive、启动 nanomsg 请求线程和拨号线程、读取流量配置、处理退出信号。 |
| `imx6ull/dial/imx6ull_dialer.cpp/.hpp` | 拨号状态机、SIM 与注册检查、数据连接、联网检查、日志采样和分级恢复；控制模组电源。 |
| `imx6ull/dial/dialer_base.hpp` | 定义平台拨号器继承的 `Dialer` 接口和状态枚举；具体 i.MX6ULL 行为在 `Imx6uDialer` 中。 |
| `imx6ull/device/modem_device.cpp/.hpp` | 按 USB 拓扑发现 AT 口与网络接口，并尝试向 `option` 驱动注册 VID/PID。 |
| `imx6ull/at/at_channel.cpp/.hpp`、`imx6ull/at/quectel_parser.cpp/.hpp` | 串口 AT 会话和移远响应解析。 |
| `imx6ull/nw/network_configurator.cpp/.hpp` | `udhcpc` 管理、IP/默认路由读取、绑定网卡的 TCP 可达性探测。 |
| `imx6ull/state/platform_modem_state.cpp/.hpp`、`modem_snapshot.hpp` | 保存并读取模组状态快照，供拨号和本机服务使用。 |
| `imx6ull/led/imx_led.hpp` | 提供 GPIO LED 操作类；拨号主循环中的 LED 对象和操作已被注释，不能据此认定当前会闪灯提示联网状态。 |
| `nanomsg_process.cpp/.hpp`、`traffic_sql/`、`fault_report/`、`logging/` | 跨平台的本机消息、流量、故障报告和日志模块。 |

## 启动、拨号与恢复

1. `main()` 初始化日志，将版本写到 `/tmp/dial_version`，建立 `Imx6uDialer`。首次拨号默认持续恢复；只有设置合法的 `RTMS_MODEM_INITIAL_DIAL_TIMEOUT_SECONDS`（0～86400 秒）才限制服务等待接口就绪的时间，其中 0 表示不超时。
2. 公共服务注册 CPActive（30 秒超时）、初始化 `tcp://127.0.0.1:48001` 的网络事件发布端及 `tcp://127.0.0.1:38001` 的请求端，然后读取流量配置，启动请求处理和拨号线程。收到 `SIGINT` 或 `SIGTERM` 时通知拨号停止并等待线程退出。
3. 拨号状态机先控制模组上电，等待最多约 30 秒完成 USB 枚举；按同一 USB 设备寻找可响应 `AT` 的串口及网络接口，避免把其他 USB 串口和网卡配对。接着用 `ATE0`、`CPIN`、`CEREG` 等检查模组与网络注册；注册等待上限约 120 秒。
4. 仅在显式设置 `RTMS_MODEM_APN` 时，代码才写入 CID 1 的 APN/PDP 类型；否则保留模组现有上下文。随后尝试 `AT+QCFG="usbnet",1`，再查询或请求 `AT+QNETDEVCTL=3,1,1` 建立数据连接。`usbnet` 命令被模组拒绝时只记诊断日志，不立即重置；PDP 状态和网络接口仍须确认。
5. 数据连接存在后，程序启动目标网卡上的 `udhcpc`，等待 IPv4 地址和默认路由，再以绑定网卡的 TCP 连接探测外网。成功时创建 `/tmp/dial_success`，进入持续监测；失败时删除该标记并按故障类型恢复。`/tmp/modem_started` 用于区分本次运行前是否已有启动记录，不等同于拨号成功。
6. DHCP/路由问题先重试本地网络配置；PDP 或数据通路问题尝试软重建；SIM/注册问题进入 CFUN 恢复；AT/USB 不可用时进入硬件恢复。重试次数和冷却时间可由环境变量调整。进程运行期间还会通过 CPActive 刷新活动时间，并周期记录网络与模组诊断信息。

### 诊断与代码边界

- `SUCCESS` 状态把发现的网卡名发布给公共服务，约 10 秒后重新进入 `CHECK_CONNECTION`；连续 5 次绑定网卡的 TCP 探测失败后按数据通路故障恢复。探测失败会发布 `DIAL` 类“network connection lost”事件。公网探测目标被网络策略阻断时，代码可能把业务专网仍可用的链路判断为故障。
- 诊断线程约每 30 秒输出一次 `[HB30]`，约每 5 分钟输出一次详细 `[HB300]`，包含注册/PDP/信号、IP、路由、DNS、探测时延及流量计数。AT 查询超时先作为诊断事件记录，由拨号主循环再用基础 `AT` 命令确认；单次遥测超时本身不应理解为立即执行模组复位。
- 当前 `LIST_OPERATOR`、`SELECT_OPERATOR` 状态进入时明确记录“manual operator selection is disabled”并转入配置错误；虽然源码保留了 `COPS`/PLMN 解析和文件辅助函数，不能把它们描述为当前自动或手动选网流程。拨号主循环在 `CONFIGURE_NETWORK`、`CHECK_CONNECTION`、`SUCCESS` 状态且 AT 可用时会调用 `fetchAndSaveTimezone()`；成功后通过 `AT+QLTS=1` 取得时区并写 `/usr/dial/tz.ini`，本次拨号循环内不再重复获取。
- `ModemSnapshot` 缓存 IMSI、IMEI、ICCID、运营商、PLMN、APN、注册状态、拨号失败原因、PDP 和 CSQ，供本机状态请求读取。快照字段可能尚未采到或来自上一次采样；判断当前链路时还须结合接口地址、默认路由与探测结果。
- `imx6ull/led/imx_led.hpp` 只提供 LED 类；实际模组上电/断电使用 `Imx6uDialer` 内的 GPIO 电源和 PWRKEY 操作。硬件恢复记录基于单调时钟的时间戳；临时文件丢失或设备重启后不能把它当成跨重启的永久恢复历史。

## 配置、接口与运行边界

| 位置或选项 | 当前源码约定 |
| --- | --- |
| `QL_MODULE_PLATFORM=MCIMX6Y2CVM08AB` | 交叉编译时选用 i.MX6ULL 分支，要求 C++11；构建后调用 `arm-linux-gnueabihf-strip`，安装到 `usr/bin/`。需配套目标架构工具链和链接库。 |
| `RTMS_MODEM_USB_VENDOR`、`RTMS_MODEM_USB_PRODUCT` | 覆盖默认 USB VID `2c7c`、PID `0901`；代码对 ID 格式做检查。设备实际 ID 与驱动绑定状态须在目标板核对。 |
| `RTMS_MODEM_APN`、`RTMS_MODEM_PDP_TYPE` | 控制 CID 1 的接入点与 PDP 类型。未显式给 APN 时保留模组现有 CID 1 上下文；PDP 类型非法会进入配置错误状态。 |
| `RTMS_MODEM_PROBE_ENDPOINTS` | 逗号分隔的 IPv4 地址或 `IPv4:端口`，最多接受 4 个；无有效覆盖时回退到默认两个 443 端口探测目标。外网限制或私网 APN 环境应按部署网络设置可达目标。 |
| `RTMS_MODEM_DHCP_RECOVERY_LIMIT`、`RTMS_MODEM_DHCP_ACQUIRE_SECONDS`、`RTMS_MODEM_PDP_RECOVERY_LIMIT`、`RTMS_MODEM_AT_UNRESPONSIVE_LIMIT` | 默认分别为 3 次、20 秒、3 次、3 次；源码限定了可接受范围，非法值回退默认值。 |
| `RTMS_MODEM_CFUN_COOLDOWN_SECONDS`、`RTMS_MODEM_HARD_RESET_COOLDOWN_SECONDS` | 默认分别为 600 秒、900 秒，限制高影响恢复动作频率。硬件恢复时间戳保存在 `/tmp/modem_mng_imx6ull_hard_reset_ms`。 |
| `/etc/config/config.json` | 从 `C020102` 读取流量限制；配置缺失或读取失败时使用 `0xffff`。文档不保存设备身份、密钥等现场配置。 |
| `/tmp/dial_version`、`/tmp/dial_success`、`/tmp/modem_started` | 分别是展示版本、当前拨号成功标记、启动记录标记；都位于临时目录，不应视为持久化历史。 |
| `/var/run/udhcpc.<网卡名>.pid`、`/proc/net/route`、`/etc/resolv.conf` | 用于 DHCP 进程定位、默认路由及 DNS 诊断；实际网卡名由 USB 拓扑发现，不应固定假设为 `usb0`。 |
| `tcp://127.0.0.1:38001`、`tcp://127.0.0.1:48001` | 分别用于本机 nanomsg 请求处理与网络事件发布；端口可用性还取决于其他本机进程的配对连接。 |
| `38001` 请求内容 | 公共请求处理器读取 JSON 中的 `ts`、`status.network` 数组；数组项包含 `cellular` 时生成包含蜂窝状态的应答。请求者须按当前协议提供这些字段；错误或未知项不会构成有效蜂窝状态请求。 |
| `/media/sdcard/dial_log` | 公共日志模块在 SD 卡已挂载且剩余空间符合要求时写日志，否则只输出到控制台；应以目标设备实际挂载和可写状态核对。 |

构建依赖在 `apps/modem_mng/CMakeLists.txt` 中声明，包括 libev、nanomsg、OpenSSL、tbox-common、cJSON、uthash、appmng、LED 控制库，并链接 pthread、SQLite 等。本文依据静态源码与 Git 状态整理，未在 i.MX6ULL 真机执行拨号、GPIO 复位或网络联调；现场表现还取决于模组固件、USB 驱动、SIM、APN、运营商和目标网络策略。
