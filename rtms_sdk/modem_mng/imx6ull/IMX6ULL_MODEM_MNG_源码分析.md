# i.MX6ULL `modem_mng` 源码分析

## 1. 分析对象与证据边界

本文分析的源码仓库为 `/home/tronlong/lyp/code/rtms_sdk`，源码目录为 `apps/modem_mng/imx6ull/`，同时核对它编译时实际使用的 `apps/modem_mng/CMakeLists.txt`、`nanomsg_process.*`、`traffic_sql/`、`logging/` 等公共模块。核对时源码仓库分支为 `develop/rtms_sdk_v1.3_20240408_dc_switch`，提交为 `e15a52329cf9cf8e19983c92da6f1124fb3d76e7`。本文是对**该提交静态代码**的分析；设备驱动、模组固件、SIM/运营商网络和部署配置的真实表现需要在目标板上验证。

为便于查证，下文所说的路径均相对 `apps/modem_mng/`。本文把“函数存在”“当前执行路径会调用”“依赖设备才能确认”分别说明，不把头文件声明、注释或备用代码视作运行事实。

文中的流程图使用纯文本代码框：从上往下读主线，`├─`/`└─` 表示条件分支，`→` 表示下一状态。这样在不支持 Mermaid 的 Markdown 查看器中也能直接阅读；每张图旁的状态表给出更准确的条件与恢复落点。

## 2. 构建产物和模块边界

`apps/CMakeLists.txt` 纳入 `modem_mng`。在交叉编译且环境变量 `QL_MODULE_PLATFORM=MCIMX6Y2CVM08AB` 时，`CMakeLists.txt:220-239` 选择 C++11、`USE_IMX6U_DIAL` 宏和本平台的 7 个 `.cpp`，再加入公共的 nanomsg、流量、故障报告、INI 解析、日志和队列源码。目标名是 `modem_mng`，安装到安装前缀下的 `usr/bin/`；该平台构建后调用 `arm-linux-gnueabihf-strip`。`imx6ull/version.h` 定义展示版本 `rtms_imx6ull_1.25.1`，而 CMake 的 `project VERSION 1.0` 仅是工程元数据。

| 源文件或目录 | 当前职责与执行地位 |
| --- | --- |
| `imx6ull/main.cpp`、`version.h` | 进程入口、日志与版本写入、首次拨号等待超时选项。 |
| `imx6ull/service/modem_service.hpp` | 通用服务封装在本平台的实例化：CPActive、本机消息线程、拨号线程、流量监测、退出信号。 |
| `imx6ull/dial/dialer_base.hpp` | `Dialer` 虚接口和另一套 `dial_stat_enu`；实际主流程由下面的 `State` 状态机控制。 |
| `imx6ull/dial/imx6ull_dialer.cpp/.hpp` | 主状态机、AT 控制、SIM/注册/PDP 检查、恢复、心跳诊断、时区及 GPIO 电源操作。 |
| `imx6ull/device/modem_device.cpp/.hpp` | 使用 sysfs USB 拓扑发现同一模组的串口与网卡，并尝试注册 `option` 驱动 ID。 |
| `imx6ull/at/at_channel.cpp/.hpp`、`quectel_parser.cpp/.hpp` | 串口事务和 AT 文本解析。 |
| `imx6ull/nw/network_configurator.cpp/.hpp` | DHCP 进程管理、IP/路由/DNS 读取、绑定网卡的 TCP 外网探测。 |
| `imx6ull/state/modem_snapshot.hpp`、`platform_modem_state.cpp/.hpp` | 定义和输出蜂窝状态快照，供公共请求处理器读取。 |
| `imx6ull/led/imx_led.hpp` | 有 LED GPIO 类，但拨号主循环里的 LED 实例/调用被注释，当前联网状态不由该类显示。 |
| `nanomsg_process.*`、`nanomsg_process_wraper.cpp` | 本机蜂窝状态请求、网络事件发布及流量监测接口；属于公共模块。 |
| `traffic_sql/`、`fault_report/`、`logging/`、`cc_deque/`、`eg25/opt_iniparser/` | CMake 引入的公共流量/故障报告/日志/队列/INI 实现；存在于二进制不意味着各功能在 i.MX6ULL 主循环逐项启用。 |

构建脚本还要求 libev、nanomsg、OpenSSL、tbox-common、cJSON、uthash、appmng 和 LED 控制库等包，i.MX6ULL 目标显式链接 pthread、cJSON、rt、appmng、nanomsg、SQLite、stdc++ 等。目标平台运行还依赖 `udhcpc`、USB 串口驱动、sysfs GPIO、设备侧配置和本机消息对端；仅通过主机编译无法证明拨号链路正常。

## 3. 进程启动与线程关系

1. `main.cpp:10-40` 初始化日志，打印平台版本，解析 `RTMS_MODEM_INITIAL_DIAL_TIMEOUT_SECONDS`。合法整数范围为 0～86400 秒；缺失、0 或无效值使等待不设超时。用临时文件加重命名的方式写 `/tmp/dial_version`，失败只记录日志，然后进入 `runModemService()`。
2. `service/modem_service.hpp:67-108` 创建 CPActive 并登记 30 秒超时的进程项；初始化 `tcp://127.0.0.1:48001` 网络事件发布端和 `tcp://127.0.0.1:38001` nanomsg 请求端。初始化失败会返回错误。读取 `/etc/config/config.json` 的 `C020102` 到 `TrafficMonitor::PacketLimit`；捕获异常时设为 `0xffff`。
3. 同一服务启动 nanomsg 请求线程、拨号线程和退出监听线程。拨号线程调用 `Imx6uDialer::dial_loop()`；主线程等拨号器首次通过 `SUCCESS` 回调公布网卡名，才调用流量监测入口 `check_CCINet_FlowOverLimit()`。拨号状态机在公布网卡后仍持续运行，并非拨号一次后退出。
4. `SIGINT`/`SIGTERM` 只令监听线程设置拨号停止标记；主服务随后等待拨号线程和请求线程退出并调用 `log_close()`。若设置了首次等待超时，主线程会通知拨号器停止、等待其退出并返回 1；消息端初始化失败返回 -1。公共服务的正常退出路径未看到 `delete_cpactive(cpactive)`，该函数在早期初始化失败分支才调用，退出资源行为应按实际库版本核对。

```text
main：初始化日志、版本和首次拨号等待超时
  │
  ▼
runModemService：初始化 nanomsg 发布端和请求端
  ├─ 失败 ────────────────────────────────► 返回 -1
  └─ 成功
       │
       ▼
     启动请求线程、拨号线程、退出监听线程
       │
       ▼
     拨号状态机持续运行
       ├─ 收到 SIGINT/SIGTERM ──────────► 停止线程、关闭日志
       ├─ 首次进入 SUCCESS ─────────────► 发布网卡名、启动流量监测
       │                                    │
       │                                    └─► 持续检测/恢复，直到退出信号
       └─ 未进入 SUCCESS
            ├─ 等待超时已配置且到期 ─────► 停止拨号、返回 1
            └─ 未到期或未配置超时 ───────► 继续拨号状态机
```

上图的“首次进入 SUCCESS”只表示程序当时确认了网卡 IPv4、该网卡默认路由及预设 TCP 探测目标可达。它不证明云平台、MQTT 或业务服务器可用。

## 4. USB 发现、AT 串口与状态快照

`device/modem_device.cpp` 扫描 `/sys/class/tty` 下 `ttyUSB*`/`ttyACM*` 以及 `/sys/class/net`；沿设备符号链接向上找到 `idVendor` 匹配的同一 USB 根节点，只有同时找到 AT 候选串口和网卡才算 `valid()`。候选串口排序后逐个发送基础 `AT`，以实际响应选定控制口。默认 VID 为 `2c7c`、PID 为 `0901`，可由 `RTMS_MODEM_USB_VENDOR`、`RTMS_MODEM_USB_PRODUCT` 覆盖。程序尝试向 `option1/new_id` 或 `option/new_id` 注册 VID:PID；注册失败仅记日志，仍等待系统自动绑定。USB 枚举等待上限约 30 秒。发现的网卡名动态取得，不能固定写成 `usb0`。

`at/at_channel.cpp` 将选定串口以非阻塞方式打开，设原始模式、115200 波特率，事务由互斥锁串行化。每次事务先排空旧输入，再完整写出命令，读取到 `OK`、`ERROR` 或 CME/CMS 终止响应为止；默认事务超时 5 秒，部分建链命令使用 30 秒。读写失败或超时记录带请求类别的事件；拨号主循环消费这些事件。解析函数只识别源码规定的 AT 响应格式，模组固件格式变化可能返回“未知”，不能把“解析失败”等同于模组明确报告断网。

`ModemSnapshot` 缓存 IMSI、IMEI、ICCID、运营商、PLMN、APN、注册状态、失败原因、PDP 和 CSQ。初次成功注册或成功联网时，代码会通过 `ATI`、`CGMR`、`CPIN`、`CEREG`、`CGDCONT`、`CFUN`、`CGATT`、`QNWINFO` 等记录初始诊断，其中 IMEI/IMSI/ICCID 以原值写日志。共享日志前需要据此处理身份信息。

| AT 指令 | 本平台当前用途 |
| --- | --- |
| `AT`、`ATE0` | 前者确认候选串口能响应和故障时的基本控制面可用性；后者在 `WRITE_TO_MODEM` 关闭命令回显。 |
| `CPIN?`、`CEREG?` | 分别检查 SIM 是否 `READY`、分组网络是否注册；`CEREG` 的 1/5 才进入后续数据连接。 |
| `CGDCONT?`、`CGDCONT=1,...` | 查询 PDP 上下文；仅显式提供 APN 时写 CID 1。 |
| `QCFG="usbnet",1`、`QNETDEVCTL?`、`QNETDEVSTATUS?`、`QNETDEVCTL=3,1,1` | 尝试 USB 网络模式、读取 PDN 状态（第一种格式解析失败时回退第二种）、启动数据连接；模式命令被拒绝不会立即终止。 |
| `CGATT`、`CGACT`、`QNETDEVCTL=0` | L2 软恢复期间先去附着/停用 CID 1/停网络设备，再重新附着和激活。 |
| `CFUN=0/1`、`CFUN?` | L3 切换模组功能状态；问号形式用于诊断。 |
| `CSQ`、`QCSQ`、`QENG="servingcell"` | 读取一般信号、LTE 信号与服务小区；后两者主要进入周期诊断。 |
| `CIMI`、`GSN`、`QCCID`、`COPS?` | 取得 IMSI、IMEI、ICCID、当前运营商。`COPS=3,2` 仅把查询显示格式设为数字 PLMN，并非手动选择网络。 |
| `QLTS=1` | AT 可用且处于网络配置、连接检查或成功状态时获取时区；解析成功后尝试写入 `/usr/dial/tz.ini`。 |

## 5. 拨号状态机与正常路径

主循环实现位于 `dial/imx6ull_dialer.cpp:905-1815`，每轮调用 CPActive 刷新、执行时区与诊断逻辑，再进入 `switch(state)`，轮末通常休眠约 2 秒。启动时如果发现的网卡已有 IPv4、默认路由且绑定网卡探测成功，会从 `CONFIGURE_USB` 验证 AT 口并跳到 `SUCCESS`，复用现有连接；否则，如果 `/tmp/modem_started` 原先已存在，先要求模组断电，再从 `POWER_ON` 重试。该临时文件只表明之前运行过，不证明本次已联网。

| 状态 | 条件和实际动作 | 后续状态 |
| --- | --- | --- |
| `POWER_ON` | 经 GPIO 130 电源脚和 GPIO 128 PWRKEY 执行上电脉冲；失败安排硬件恢复重试。 | `CONFIGURE_USB` 或 `FAILURE_RETRY` |
| `CONFIGURE_USB` | 尝试 `option` 驱动 ID 注册，按 USB 拓扑找候选 AT 口/网卡，逐个发 `AT`；约 30 秒内未完成则报 USB 故障。 | 已有可用网络进 `SUCCESS`，否则 `WRITE_TO_MODEM`；故障进 `RECOVER_HARDWARE` |
| `WRITE_TO_MODEM` | `ATE0`，`CPIN?` 要求 SIM `READY`，`CEREG?` 要求注册值 1（本地网）或 5（漫游）；合法 APN 配置时写 `CGDCONT` 的 CID 1，尝试 `QCFG="usbnet",1`，查询/请求 `QNETDEVCTL` 数据连接。 | 注册等待进 `WAIT_REGISTRATION`；建链后进 `CONFIGURE_NETWORK`；按故障分类恢复 |
| `WAIT_REGISTRATION` | 反复查询 `CEREG?`，约 10 秒间隔记录 CSQ，等待上限约 120 秒。 | 注册成功回 `WRITE_TO_MODEM`，超时进 `RECOVER_CFUN` |
| `CONFIGURE_NETWORK` | 停旧 DHCP；最多 5 次检查 PDP 已连接且网卡存在，间隔约 2 秒；启动 `udhcpc`，在配置的时间窗等 IPv4 和默认路由。 | `CHECK_CONNECTION`，或者 `FAILURE_RETRY`/`RECOVER_PDP` |
| `CHECK_CONNECTION` | 先验 IPv4/默认路由，再以绑定网卡 TCP 探测外网目标；成功清计数、创建 `/tmp/dial_success`。 | `SUCCESS`；DHCP/路由故障回 `CONFIGURE_NETWORK` 或升 `RECOVER_PDP`；5 次外网探测失败进 `RECOVER_PDP` |
| `SUCCESS` | 发布网卡名给服务层；约 10 秒后再次检查连接。 | `CHECK_CONNECTION` |
| `FAILURE_RETRY` | 按截止时间等待，再返回记录的恢复状态。 | `retry_resume_state` |
| `CONFIG_ERROR` | 保存失败原因、删除联网标记；主循环继续停留于此并定期循环，代码没有自动修复配置错误。 | 仍为 `CONFIG_ERROR`，直到进程停止 |

```text
进入 dial_loop
  ├─ 原网卡的 IP、默认路由、TCP 探测均正常
  │    └─ CONFIGURE_USB（验证 AT）→ SUCCESS
  └─ 否 → POWER_ON（GPIO 上电）→ CONFIGURE_USB
                                   ├─ 超时 → RECOVER_HARDWARE
                                   └─ AT 与网卡可用
                                        ↓
                                   WRITE_TO_MODEM
                                     ├─ SIM 未就绪 → RECOVER_CFUN
                                     └─ SIM 就绪 → 查询 CEREG
                                                   ├─ 未注册且未超时
                                                   │    → WAIT_REGISTRATION → 重查
                                                   ├─ 注册超时 → RECOVER_CFUN
                                                   └─ 已注册（1/5）
                                                        ↓
                                                   配置/复用 CID1 PDP
                                                     ├─ 配置非法 → CONFIG_ERROR
                                                     ├─ PDP 失败 → RECOVER_PDP
                                                     └─ 成功 → CONFIGURE_NETWORK
                                                                  ↓
                                                             启动 DHCP
                                                               ├─ PDP/网卡失败
                                                               │    → RECOVER_PDP
                                                               └─ CHECK_CONNECTION
                                                                    ├─ 无 IP/路由
                                                                    │  未超限 → 重配 DHCP
                                                                    │  超限 → RECOVER_PDP
                                                                    ├─ TCP 连续 5 次失败
                                                                    │    → RECOVER_PDP
                                                                    └─ TCP 成功 → SUCCESS
                                                                                   └─ 约 10 秒后复查
```

该图省略所有中间重试等待节点，以便展示业务条件；每次重试的准确落点见第 6 节。`QCFG="usbnet",1` 被拒绝只写诊断，不直接作为配置错误。若没有 `RTMS_MODEM_APN`，`create_call()` 保留模组现有 CID 1，不写 `CGDCONT`；有 APN 时仅接受 `IP`、`IPV6`、`IPV4V6` 三种 PDP 类型，默认 `IPV4V6`。

## 6. 故障分类与分级恢复

`FailureClass` 是故障成因，`RecoveryLevel` 是采取的动作，二者不是同一个枚举。`enterRecovery()` 把 DHCP/路由送到 L1，把数据路径/PDP 送到 L2，把 SIM/注册送到 L3，把 AT/USB 送到 L4，把配置错误送到 `CONFIG_ERROR`。普通重试等待阶梯为 10、30、60、120、300、600 秒，超过范围保持 600 秒；部分路径用显式等待值覆盖。

| 级别 | 执行动作和升级条件 |
| --- | --- |
| L1 DHCP | 重启目标网卡 DHCP 并重新检查地址/路由。`CHECK_CONNECTION` 每次发现无 IPv4/默认路由先增加计数，默认上限为 3；计数为 1、2、3 时各回 `CONFIGURE_NETWORK` 重配一次，第 4 次失败升 L2。即首次配置之外最多 3 次此类重配。`udhcpc` **启动失败**另走 `FAILURE_RETRY → CONFIGURE_NETWORK`，该分支虽增加同一计数，却没有在这里比较上限，可能持续重试，不能套用“第 4 次升 L2”的结论。 |
| L2 PDP | 停 DHCP，`modem_init()` 依次 `CGATT=0`、`CGACT=0,1`、`QNETDEVCTL=0`，`start_call()` 再 `CGATT=1`、`CGACT=1,1`、`QNETDEVCTL=3,1,1`；成功回 DHCP。AT 仍可用但软重建达到默认 3 次时升 L3。 |
| L3 CFUN | 停 DHCP，经 `CFUN=0`、短暂等待、`CFUN=1` 切换射频功能。默认冷却 600 秒，仅保存在当前进程内的单调时钟值；成功后等待注册并回 `WRITE_TO_MODEM`。命令失败先用基础 `AT` 判断是否升级为 AT 故障。 |
| L4 硬件 | 停 DHCP，默认冷却 900 秒；保存时间戳后回 `POWER_ON`，由 `POWER_ON` 执行 GPIO 上电序列。时间戳写 `/tmp/modem_mng_imx6ull_hard_reset_ms`；本级分支本身不直接调用 `powerOffModule()`。当检测到已有启动记录但当前未联网时，进入主循环前另有一次 `powerOffModule()`。 |

遥测 AT 超时由 `AtSession` 记录请求类别，拨号主循环最多每 30 秒用一次基础 `AT` 确认；基础探测连续失败达到 `RTMS_MODEM_AT_UNRESPONSIVE_LIMIT`（默认 3）才按 AT 故障进入 L4。一次 `QCSQ` 或状态查询超时不足以证明控制面不可用。`CONFIG_ERROR` 没有自动回到正常状态的路径；修正环境变量通常还需重新启动进程。

```text
故障分类
  ├─ DHCP / 路由 ──────► L1：重新运行 DHCP
  │                       └─ 地址/路由仍失败且超限 ──► L2
  ├─ PDP / 数据通路 ──► L2：停止并重建 PDP
  │                       ├─ 软重建失败、AT 可用且超限 ──► L3
  │                       └─ 基础 AT 连续确认失败 ───────► L4
  ├─ SIM / 注册 ──────► L3：CFUN=0 → CFUN=1
  │                       └─ 基础 AT 连续确认失败 ───────► L4
  ├─ AT / USB ────────► L4：记录冷却时间 → POWER_ON → USB 重发现
  └─ 配置非法 ────────► CONFIG_ERROR（当前运行中不自动退出）
```

## 7. 网络判定、DHCP 与参数

`NetworkConfigurator` 对网卡名做长度和字符检查；通过 `/sys/class/net` 判断存在，通过 `getifaddrs()` 检查非零 IPv4，通过 `/proc/net/route` 查该网卡目标 `00000000` 且路由标志有效的默认路由。DHCP 用 `udhcpc -i <接口> -p /var/run/udhcpc.<接口>.pid -b -t 5 -T 3` 启动；停 DHCP 时先校验 PID 对应命令行及接口，再发 SIGTERM，必要时发 SIGKILL。网络探测使用 `SO_BINDTODEVICE` 绑定当前网卡、非阻塞 TCP 连接及 `select()`/`SO_ERROR` 判定，默认目标 `1.1.1.1:443`、`8.8.8.8:443`，无 DNS 解析。

| 环境变量 | 默认值与代码约束 |
| --- | --- |
| `RTMS_MODEM_USB_VENDOR`、`RTMS_MODEM_USB_PRODUCT` | 默认 `2c7c`、`0901`；注册 `option` 驱动时要求 4 位十六进制 ID。 |
| `RTMS_MODEM_APN`、`RTMS_MODEM_PDP_TYPE` | APN 缺失时保留模组现有上下文；提供 APN 时类型缺省 `IPV4V6`，允许 `IP`、`IPV6`、`IPV4V6`。无论 APN 是否提供，非空且非法的 PDP 类型均进入配置错误。 |
| `RTMS_MODEM_PROBE_ENDPOINTS` | 逗号分隔最多 4 个 IPv4 或 IPv4:端口，缺端口用 443；没有任何有效项才用两个默认目标。 |
| `RTMS_MODEM_DHCP_RECOVERY_LIMIT` | 默认 3，允许 1～10。 |
| `RTMS_MODEM_DHCP_ACQUIRE_SECONDS` | 默认 20，允许 10～60 秒。 |
| `RTMS_MODEM_PDP_RECOVERY_LIMIT`、`RTMS_MODEM_AT_UNRESPONSIVE_LIMIT` | 均默认 3，允许 1～10。 |
| `RTMS_MODEM_CFUN_COOLDOWN_SECONDS` | 默认 600，允许 30～86400 秒。 |
| `RTMS_MODEM_HARD_RESET_COOLDOWN_SECONDS` | 默认 900，允许 60～86400 秒。 |
| `RTMS_MODEM_INITIAL_DIAL_TIMEOUT_SECONDS` | 默认 0，即无限等待；合法范围 0～86400 秒。 |

`/etc/resolv.conf` 仅用于诊断/状态接口中的 DNS 字段；主拨号成功判断是 IP + 该接口默认路由 + TCP 探测。私网 APN 或网络防火墙若不允许默认公网目标，应配置部署环境能访问的探测地址，否则会触发数据通路故障恢复。这是从探测条件直接推出的部署影响，不能据此推断所有网络不可用。

## 8. 本机通信、流量、日志与文件副作用

`runModemService()` 初始化本地 nanomsg REP 端 `tcp://127.0.0.1:38001` 和网络事件发布端 `tcp://127.0.0.1:48001`。请求线程读取 JSON 帧，要求含 `ts`、`status.network` 数组；数组元素包含 `cellular` 时调用公共 `ModemReqHandler::getStatus()`，返回嵌套于 `status.network.cellular` 的状态。公共模块会输出型号、SIM/模组身份、注册、PLMN、IP、DNS、APN、流量等，并在本平台附加 `pdn_status` 和非空 `dial_failure`。`network_online` 在 i.MX6ULL 分支重新执行绑定网卡的 IP/路由/TCP 探测；请求还会通过公共 `getApn()` 调用 `refreshApn()`，若 APN 缓存为空或长度不足 5 且 AT 口已确认，就可能发送 `AT+CGDCONT?`。IMEI、IMSI、ICCID、CSQ、PLMN 等本平台字段主要读取拨号线程更新的快照，不应把每次状态请求都理解为一次完整的模组实时查询。

| `cellular` 字段 | i.MX6ULL 分支的主要来源与解释 |
| --- | --- |
| `model`、`md_sw_ver`、`md_hw_ver` | 公共处理器尝试从 `/etc/quectel-project-version` 读取型号和版本；`md_hw_ver` 为空时填字面值 `imx6`，不是一次实时模组型号查询。 |
| `imei`、`imsi`、`iccid` | 平台 `ModemSnapshot` 缓存；IMEI/IMSI 缺失时写 `000`，ICCID 缺失时写 IMSI。缺失时的这些回退值不是实际身份。 |
| `phonenum`、`lac`、`cellid` | 电话号码固定写 `186`；LAC/Cell ID 缺失时分别用源码占位值 `271a`、`0d176547`。 |
| `csq`、`operator`、`plmn`、`register` | CSQ、运营商、PLMN、注册描述来自平台快照及公共读取；运营商缺失用 `Unknown`，PLMN 缺失用 `99`。 |
| `network_type`、`network_online`、`pdn_status`、`dial_failure` | `network_type` 的公共函数固定返回 1；`network_online` 重新检查 IP、默认路由及绑定网卡的 TCP 可达性；`pdn_status` 与非空 `dial_failure` 来自平台快照。四者含义各不相同。 |
| `ip`、`gateway`、`dns1`、`dns2`、`apn` | IP 由公共代码调用系统 `ip addr show <接口>` 解析，DNS 读取 `resolv.conf`，APN 来自平台快照且缓存缺失或过短时可能查询 AT；本平台的 `gateway` 由 IP 最后一段改成 `1` 推算，而不是调用 `getGateway()` 读取内核路由。DNS 缺失时会填公共代码里的默认字面值。 |
| `tx`、`rx` | 来自流量监测器的历史发送/接收字节快照，输出前各除以 1024；字段名没有在响应中携带单位。 |

`TrafficMonitor` 对象在首次拨号成功前已构造，会先尝试恢复数据库中的记录；首次公布网卡后，`check_CCINet_FlowOverLimit()` 才启动后台线程：约每 10 秒读一次网卡收发字节，约每 6 次采样尝试持久化月流量和历史流量；数据库路径是 `/usrdata/traffic_record.db`，表为 `MonthlyTraffic`、`HistoryTraffic`。`C020102` 写入的 `PacketLimit` 虽以 Packet 命名，当前比较表达式实际上把它与月接收字节加发送字节之和比较，超过时按计数节流发布 `TrafficOverLimit` 事件；不能按变量名直接把它解释为报文个数。配置值为字符串时 `modemServiceReadInt()` 不赋值，静态初值 0 会保留；读取抛异常时才回退 `0xffff`。存储是否成功仍取决于设备时钟和数据库可用性（`service/modem_service.hpp:54-58,100-107`、`nanomsg_process.hpp:370-590`、`traffic_sql/sql_traffic.hpp:180-192`）。

公共 `TrafficMonitor` 还实现了 `check_CCINet_hasTraffic()` 和 `NoTraffic` 事件，但 `runModemService()` 当前只调用超流量监测入口，没有启动这条“无流量”线程；不能因类里有实现就认为 i.MX6ULL 进程会发布 `NoTraffic`。

**公共状态字段不是纯粹的硬件真值。** 例如没有 IMEI/IMSI 时写字符串 `000`；ICCID 缺失时回退 IMSI；电话号码当前固定 `186`；LAC、Cell ID、DNS 在缺失时填入源码内置占位值。非 RK3506J 分支的 `gateway` 由 IP 最后一段改成 `1` 得到，并非读取实际内核网关。分析状态应同时参考 `network_online`、`pdn_status`、IP/路由及现场报文，不要把占位字段当成模组真实上报。上述行为见 `nanomsg_process.cpp:52-220`。

| 路径/接口 | 实际用途和限制 |
| --- | --- |
| `/tmp/dial_version` | 原子替换写入当前展示版本 `rtms_imx6ull_1.25.1`。 |
| `/tmp/modem_started` | 启动记录：原先存在且当前未联网时，启动前执行一次模组断电。临时文件清除后历史信息消失。 |
| `/tmp/dial_success` | 本轮联网判定成功时创建，故障恢复开始时删除；不是持久化历史或业务服务器状态。 |
| `/tmp/modem_mng_imx6ull_hard_reset_ms` | L4 冷却时间的单调时钟毫秒值；用于同一次开机内避免频繁硬件恢复。 |
| `/usr/dial/tz.ini` | 在 `CONFIGURE_NETWORK`、`CHECK_CONNECTION`、`SUCCESS` 且 AT 可用时尝试 `AT+QLTS=1`，解析后尝试写 `dev:tz`；解析成功后本进程不再重复获取，写文件失败须另看日志。 |
| `/etc/config/config.json` | 读取 `C020102` 作为流量上限；读取异常回退 `0xffff`。 |
| `/usrdata/traffic_record.db` | 公共流量监测使用的 SQLite 文件，记录月流量与历史流量；与 `/tmp` 标记不同，设计上是持久记录。 |
| `/media/sdcard/dial_log` | 日志模块在 SD 卡挂载且满足空间检查时写文件，并按日期/空间做清理；不满足条件时退回控制台输出。首次检查门槛为 500 MiB，见 `logging/logger_sd.c`。 |
| `tcp://127.0.0.1:38001`、`48001` | 本机请求响应与网络事件通道。端到端工作需要相应本机对端；端口本身不连接公网。 |

## 9. 明确未启用、待验证与维护风险

- `LIST_OPERATOR`、`SELECT_OPERATOR` 在状态机中直接拒绝手动选网并进入配置错误；`COPS`/PLMN 查询用于观测，不能据保留的解析器或文件辅助函数认定当前会执行手动运营商选择。
- `parse_oper_list_info()`、`cc_deque_free_all()`、`read_plmn_from_file()`、`write_plmn_to_file()`、`isVendorPresent()` 和 `adapterAT()` 等辅助代码在该平台目录中有定义，但没有从当前 `main()` 到拨号主循环的调用链；`/usrdata/plmn` 不能因此被写成当前必读配置。公共 `fault_report/NetworkMonitor`、`SimSignalMonitor`、`SimMonitor` 源文件虽纳入 i.MX6ULL 构建，当前服务入口没有创建或启动这些类的实例。
- `imx6ull/led/imx_led.hpp` 只有实现，主循环的 LED 控制语句已注释；GPIO 130/128 的模组电源控制则在主路径实际调用。设备引脚与供电时序仍要按板级设计核对。
- `getStatus()` 有多个硬编码占位值，且公共模块的状态字段可能来自缓存；上报内容不能替代原始 AT 回复、内核路由或实际业务端到端测试。
- 初始日志会写出完整 IMEI、IMSI、ICCID；日志收集和分享时应脱敏。`/tmp` 下的标记和冷却记录并非跨系统重启的可靠持久数据。
- 源码目录没有 i.MX6ULL 专用单元测试；本文没有执行目标板上的 USB 枚举、AT 交互、GPIO 电源、SIM 注册、DHCP、外网探测或 nanomsg 对端联调。上述静态路径与条件均可从代码核查，实际超时、模组 AT 响应格式及外网可达性需要真机与部署环境验证。

## 10. 设备故障排查：先保留证据，再按状态定位

下面的检查顺序只使用查询和读取动作，不会主动发送 AT 控制命令、重启 DHCP、切换 CFUN 或操作 GPIO。设备命令以 BusyBox/Linux 常见工具为例；`ip`、`ss` 在目标镜像中可能不存在，遇到这种情况使用表中的 `/proc`、`/sys` 文件核对。`<IF>`、`<PID>` 必须替换为日志或设备上实际发现的值。先记录故障开始时间、是否反复出现、当时 SIM/运营商/APN/网络策略及最近的软件变更，随后按同一时间线对照日志。

```text
设备异常
  ├─ 无 modem_mng 进程 → 查启动错误、依赖库、nanomsg 端口
  └─ 进程存在 → 查最后一次 [STATE] 与第一条 [RECOVERY]
       ├─ 卡在 CONFIGURE_USB  → USB 拓扑 / AT 串口 / GPIO
       ├─ 卡在 WRITE_TO_MODEM / WAIT_REGISTRATION → SIM / CEREG
       ├─ 卡在 CONFIGURE_NETWORK → PDP 状态 / 网卡 / DHCP
       ├─ 卡在 CHECK_CONNECTION → IPv4 / 默认路由 / TCP 探测
       ├─ 卡在 CONFIG_ERROR → APN / PDP 类型等配置
       └─ 已进 SUCCESS 但业务异常 → 本机接口 / 业务端点另查
```

### 10.1 最小取证清单

| 先查什么 | 设备侧只读检查 | 解释与注意事项 |
| --- | --- | --- |
| 程序是否运行及版本 | `pidof modem_mng`；`cat /tmp/dial_version` | 无 PID 时先看进程管理器/启动脚本的退出记录；`dial_version` 是上次写出的展示版本，单凭文件存在不能证明当前进程还在运行。应将设备版本与本文分析的源码提交对应起来。 |
| 日志实际位置 | `find /media/sdcard/dial_log -type f -name 'dial_*.log'`；同时看启动控制台日志 | 文件名为 `dial_YYYYMMDD_HHMMSS.log`，按日期或 `unsynced/` 分目录；启动时 SD 卡未挂载或可用空间不足 500 MiB 时只输出到控制台，之后挂载 SD 卡不会自动补开文件日志。设备时间不准时不要只查当天文件夹。 |
| 最新状态及原因 | 按时间顺序查 `[STATE]`、`[RECOVERY]`、`[DEVICE]`、`[AT]`、`[PDP]`、`[DHCP]`、`[NET]` | `[RECOVERY]` 同时包含故障类别、恢复级别、下一状态和原因；只看最后一条日志容易把恢复动作误当成最初故障。 |
| 本机消息端口 | 若镜像提供 `ss`，查 `ss -ltn` 中的 38001、48001；同时看启动日志 | 两个端口是 nanomsg 绑定的本地 TCP 传输层端口。能监听只说明绑定成功，不说明对端请求格式正确或业务已收到事件。 |
| USB、串口、网卡 | `ls /dev/ttyUSB* /dev/ttyACM*`；`ls /sys/class/net`；`dmesg` 的 USB/串口记录 | 要核对 `[DEVICE]` 日志选出的 AT 口和网卡属于**同一 USB 拓扑**。节点存在不证明 AT 可响应，也不证明蜂窝网络已注册。 |
| 目标网卡、路由、DNS | `ip -4 addr show dev <IF>`；`ip -4 route show`；`cat /proc/net/route`；`cat /etc/resolv.conf` | 先看目标网卡有无非零 IPv4，再看是否有指向该接口的默认路由。DNS 字段不参与程序主探测，且公共状态接口可能填回退值。 |
| DHCP、数据库、临时标记 | `ps` 中查目标接口的 `udhcpc`；`ls -l /var/run/udhcpc.<IF>.pid /usrdata/traffic_record.db /tmp/dial_success /tmp/modem_started /tmp/modem_mng_imx6ull_hard_reset_ms` | PID 文件必须与真实 `udhcpc -i <IF>` 进程对应；`dial_success` 是阶段性标记，`modem_started` 仅是运行记录；数据库异常主要影响流量统计，不自动说明拨号失败。 |

读取日志时以**时间线**记录“最后一个正常状态 → 第一个失败状态 → 恢复动作 → 是否恢复成功”。`[HB30]` 是约 30 秒摘要，`[HB300]` 是约 5 分钟详细样本；`sample_age_ms` 过大表示缓存样本陈旧，不能把旧快照当作当前实测值。`[RECOVERY] downtime_s=...` 表示代码又一次探测成功；是否恢复业务仍需单独核对业务端点。

### 10.2 按症状定位第一处失败

| 现场症状或日志 | 先核对的事实 | 代码路径与下一步 |
| --- | --- | --- |
| 没有 PID，或启动后立即退出 | `/tmp/dial_version` 是否更新、控制台有无 `Failed to initialize EventPublisher` / `NanoReqHandler`、38001/48001 是否已被占用、依赖库是否缺失 | `runModemService()` 在两类 nanomsg 初始化失败时返回 -1。区分“进程没启动”和“进程在持续拨号但还没进入 SUCCESS”；可结合启动管理器的退出码及构建平台宏。 |
| `[POWER] power-on failed`、USB 枚举超时 | GPIO 130/128 的设备节点及板级供电、`dmesg` 中模组枚举、VID/PID、AT 串口和网卡是否属于同一 USB 设备 | `POWER_ON → CONFIGURE_USB`，30 秒内无可响应 AT 口和网卡则按 USB 故障进 L4。`option new_id` 写入失败只是一条诊断，若内核已自动绑定且 `[DEVICE] AT ready` 出现，就不应把它当根因。 |
| 有 USB 节点但 `[AT] open failed`、`read EOF` 或 `response timed out` | 日志里的实际 AT 口路径、权限、模组是否重新枚举导致节点变动、是否有别的进程占用串口 | `AtSession` 使用非阻塞 115200 原始串口并串行化事务。单条遥测超时先看后续 `basic AT probe succeeded`；连续基本 AT 失败达到阈值才进入 L4。现场手动 AT 调试应避免与运行中的 `modem_mng` 同时占用端口。 |
| `[SIM] SIM not ready` 或 `CEREG` 长时间不是 1/5 | SIM 是否在位和可用，日志中的 `CPIN`/`CEREG` 原始值、信号、运营商覆盖及漫游条件 | SIM 不就绪进 L3；注册等待最长约 120 秒，超时进 L3。`CEREG` 响应无法解析也会等待后恢复；区分模组报告未注册与解析格式不匹配。程序不会自动执行手动选网状态。 |
| `[APN]` 配置被拒、进入 `CONFIG_ERROR` | 启动环境中的 `RTMS_MODEM_APN`、`RTMS_MODEM_PDP_TYPE` 是否与设备运营商匹配；PDP 类型必须是 `IP`、`IPV6`、`IPV4V6` | 非空非法 PDP 类型或 `CGDCONT` 拒绝会进入 `CONFIG_ERROR`。此状态当前不会自动重试配置；修正配置后需要由现场启动管理流程重启进程。查询环境变量时不要把 APN 以外的设备密钥一起导出。 |
| `[PDP]` 状态未知、CID 1 启动被拒，或反复 L2/L3 | 比较 `QNETDEVCTL?`/`QNETDEVSTATUS?` 的日志与模组固件格式，核对 SIM 注册与 APN，确认网卡是否随数据连接出现 | `QCFG="usbnet",1` 被拒绝本身不会立即恢复；`QNETDEVCTL=3,1,1` 被拒或 PDP 未达到已连接才是建链失败。L2 依次去附着/停连接再重建；AT 不响应或达到软恢复上限会升级。不要在进程运行时另起拨号脚本争夺模组控制权。 |
| `[DHCP]` 启动失败，或 `[NET] ... no IPv4 address` / `no default route` | `udhcpc -i <IF>` 是否实际在运行、接口地址、`/proc/net/route`、DHCP 服务器/模组侧地址分配 | PDP 已连接只代表模组数据侧建立，不代表 Linux 网卡已取得地址。默认等 DHCP 约 20 秒；`CHECK_CONNECTION` 中地址/路由缺失时最多重配 3 次，第 4 次升 L2。若是 `udhcpc` **进程启动失败**，代码在 `FAILURE_RETRY` 后继续尝试启动，没有同样的上限判断；应先查可执行文件、权限、进程/PID 文件、资源错误，不能仅等待自动升级。仅凭 PID 文件或 `udhcpc started` 日志不能认定租约成功。 |
| 有 IPv4 和默认路由，但 `[NET] interface-bound internet probe failed` | `[NET] interface-bound probe endpoints=...` 的目标、现场 APN 是否私网、目标 IP:端口是否被运营商/防火墙允许 | 代码以 `SO_BINDTODEVICE` 做 TCP connect，默认测试两个公网 443 端口；失败累计 5 次按数据通路故障进入 L2。`ping`/DNS 成功与否不能替代该 TCP 条件。私网环境应核对部署用 `RTMS_MODEM_PROBE_ENDPOINTS`，先证实目标本来应可达，再判断程序故障。 |
| 反复出现 L3/L4、`cooling down` | `[RECOVERY]` 的 `class`、`reason`、`attempt`、`wait_s`，以及单调时钟时间戳文件 | 冷却期是代码有意延迟，不等于线程卡死。L3 默认 600 秒，L4 默认 900 秒；先追踪导致升级的最早失败，避免只盯最后的 GPIO 动作。断电或 `/tmp` 清理会改变 L4 时间戳的意义。 |
| `[TRAFFIC] sample/persist skipped` 或流量异常 | 目标网卡统计文件、`/usrdata/traffic_record.db` 是否可访问、设备时钟是否早于数据库记录、`C020102` 的实际类型和值 | 流量线程在首次 SUCCESS 后启动；数据库失败可退为内存计数，不代表蜂窝数据连接必坏。`PacketLimit` 实际与字节总量比较，公共状态 `tx/rx` 是字节除 1024 后的数。 |
| 本机状态接口不通、字段异常 | 程序是否监听 38001，REQ 客户端是否用 nanomsg 协议及合法 JSON；看 `[NANOMSG]` 接收/发送错误 | 38001 是 nanomsg `NN_REP`，不能用普通 TCP 文本或 `nc` 当协议客户端。`ts` 和 `status.network` 数组为解析前提，元素需包含 `cellular`。`gateway`、电话等字段有推算/占位值，不能单凭这些字段判断网络。 |

### 10.3 如何判断“程序问题”与外部条件

1. **先用相同版本重现路径。** 记录 `/tmp/dial_version`、启动配置和第一条 `[STATE]` 到失败点的完整时间线。本文依据的源码提交见第 1 节；设备二进制若来自其他提交，不能直接套用本文的阈值和状态跳转。
2. **把控制面、数据面、业务面分开验证。** `[DEVICE] AT ready` 只证明控制串口响应；`CEREG=1/5` 只证明注册；`pdn=1` 只证明模组数据连接；Linux 网卡 IPv4/默认路由及绑定网卡的 TCP connect 另需成立；云端或业务服务器还要单独验证。越靠后的条件失败，越不能反推前面一定失败。
3. **核对源码判定与原始证据是否冲突。** 例如原始 AT 响应可读但解析器给 `pdn=-1`，应检查 `quectel_parser.cpp` 对固件格式的匹配；网卡有地址但程序报无默认路由，应核对 `/proc/net/route` 中对应接口和路由标志；专网业务正常但公网探测失败，应核对探测目标。这样的“证据冲突”才支持把问题定位到程序判定逻辑，仍需复现或抓到足够日志。
4. **不要用占位值、陈旧标记或单条超时直接定根因。** 公共状态接口的 `phonenum`、部分 DNS、`gateway` 可由固定值或推算产生；`/tmp/dial_success` 可能是上一次成功的残留，恢复开始才删除；单次遥测 AT 超时不会立即硬复位。以同一时段的状态跳转、实时网卡/路由、基础 AT 确认和恢复日志相互印证。
5. **现场改动放在取证之后。** 重启进程、重跑 `udhcpc`、手动发 `CFUN`、改 APN/路由或直接切 GPIO 都会改变状态和证据，也可能与进程的恢复状态机并发。要做受控复现，先保存日志、现有配置和设备网络状态，再由现场操作流程安排单项改动，记录改动前后的同一组观察值。

### 10.4 值得优先核验的源码边界

| 代码现状 | 何时怀疑为程序侧问题 | 验证方向 |
| --- | --- | --- |
| `at/quectel_parser.cpp` 只解析明确列出的 `QNETDEVCTL`/`QNETDEVSTATUS` 文本格式 | 原始 AT 回包表示已连接，但日志反复给出 `pdn=-1` 或“status unavailable” | 按对应固件版本检查解析规则；现有日志若未保留完整原始响应，需在受控复现环境补采，不能仅凭 `pdn=-1` 认定模组未建链。 |
| `CHECK_CONNECTION` 以绑定网卡连接指定 IP:端口作为联网条件 | 设备专网业务可通，但默认公网 443 被策略阻断并触发 L2 恢复 | 对照 `[NET] interface-bound probe endpoints` 和现场允许的目标；在相同接口、相同目标端口复现，确认是探测目标选择问题还是数据链路问题。 |
| `service/modem_service.hpp` 读取到字符串类型的 `C020102` 时不更新 `PacketLimit` | 流量阈值配置看似存在，却没有产生预期 `TrafficOverLimit` | 核对 JSON 字段实际类型、进程启动日志及数据库计数；静态初值为 0，异常回退值为 `0xffff`，二者处理路径不同。 |
| `fetchAndSaveTimezone()` 在解析成功后立即置 `tz_fetched_`，随后才写 INI | 时区值已解析但 `/usr/dial/tz.ini` 未更新，之后也不再重试 | 核对 INI 写入错误日志、目录权限和磁盘状态；不要把 `[TZ] modem timezone=... saved` 单独当作写盘成功证明。 |
| `CONFIG_ERROR` 仅保持失败状态，循环不自动重新加载 APN/PDP 配置 | 现场修正启动环境后进程仍不拨号 | 对照 `[RECOVERY] class=CONFIGURATION` 的首次原因；在受控流程中用更新配置重新启动并比较状态跳转。 |
| `CONFIGURE_NETWORK` 中 `startDhcpBackground()` 失败直接安排重试，没有按 DHCP 上限升级 | `[DHCP] start failed` 长时间重复，却始终没有进入 L2 | 核对 `udhcpc` 是否存在、启动权限及 PID 文件/进程状态；若这些外部条件正常，再检查进程启动失败处理和是否需要上限升级。 |
| 公共 `getStatus()` 的 `gateway`、`phonenum`、部分身份/DNS 字段可由推算或占位值产生 | 本机接口回包与内核路由、SIM 身份或真实号码不一致 | 将回包与 `/proc/net/route`、原始 AT/设备配置逐项比对；修接口时追到 `nanomsg_process.cpp:52-220` 的字段赋值路径。 |

修改程序后至少要复核两个方向：失败场景能否在不误触发高一级恢复的前提下正确分类；恢复后 `SUCCESS`、`network_online`、实际业务端点是否一致。i.MX6ULL 目录没有现成专用自动化测试，解析器边界可先在主机用保存的 AT 回包做针对性测试，再在隔离目标板复现 USB、GPIO、SIM、DHCP 与本机 nanomsg 时序。不要把主机解析测试通过解释为现场链路已验证。

排障依据：`imx6ull/service/modem_service.hpp:67-190`、`imx6ull/dial/imx6ull_dialer.cpp:905-1815`、`imx6ull/device/modem_device.cpp`、`imx6ull/at/at_channel.cpp`、`imx6ull/nw/network_configurator.cpp`、`nanomsg_process.cpp:1247-1428`、`nanomsg_process.hpp:350-590`、`logging/logger_sd.c:25-28,388-423`。以上命令和判断用于定位故障，不替代目标设备上的实测。
