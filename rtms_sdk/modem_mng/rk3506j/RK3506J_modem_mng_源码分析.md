# RK3506J `modem_mng` 源码分析

## 1. 分析基线与范围

- 源码：`/home/tronlong/lyp/code/rtms_sdk/apps/modem_mng/rk3506j/`；上层共用代码：同仓库 `apps/modem_mng/`。源码仓库分支 `develop/rtms_sdk_v1.3_20240408_dc_switch`，提交 `e15a52329cf9cf8e19983c92da6f1124fb3d76e7`。
- 本文档：`/home/tronlong/lyp/perCode/idevelop_md/rtms_sdk/modem_mng/rk3506j/`；文档仓库分支 `master`。以下为该提交的**静态代码行为**，不把代码意图当成真机结果。
- 覆盖本平台入口、服务线程、启动复用、两种拨号状态机、AT 通道、USB/端口缓存、网卡配置、监测与恢复、配置及构建测试。`nanomsg_process.*`、`traffic_sql/`、`logging/` 等公共实现只分析 RK3506J 实际调用的接口；云端协议、MCU 固件和 Linux 镜像配置不在本源码目录内。
- 文中“成功”按相应函数的返回或状态标记解释。`AT OK`、PDP 激活、主机获得 IP、ICMP 探测通过和业务云连接是五个不同层次，不可互相替代。

## 2. 编译后的组成与调用边界

顶层 `apps/CMakeLists.txt` 需启用 `WITH_RTMS_CORE` 才添加 `modem_mng`。交叉编译时环境变量 `QL_MODULE_PLATFORM=RK3506J` 选中 `apps/modem_mng/CMakeLists.txt` 的 RK3506J 分支：C++14、`USE_RK3506J_DIAL`、`USE_EG912_MINIMAL_DIAL`，生成 `modem_mng` 并安装到 `usr/bin/`。运行版本由 `version.h` 给出，当前是 `rtms_rk3506j_1.28.6`。该分支还编入 `nanomsg_process.cpp`、流量、日志和 `fault_report/` 等共用源码；链接 `nanomsg`、`appmng`、`cJSON`、SQLite、`ledgpio-control` 等库。`fault_report/` 的三个监测实现虽被编译，本平台入口和拨号循环没有实例化它们，不能据编译清单断言其监测线程在运行。`scripts/` 下脚本没有进入 RK3506J 的安装规则。依据：`apps/CMakeLists.txt`、`apps/modem_mng/CMakeLists.txt`、`rk3506j/version.h` 及本平台调用处。

| 文件 | 实际职责 |
| --- | --- |
| `main.cpp` | 日志/版本、AT 进程锁、网络 LED、启动模式初始化、进入服务。 |
| `service/modem_service.hpp` | CPActive、nanomsg 两个本机端点的后台重试、流量配置、拨号线程与退出协调。 |
| `dial/rk3506j_dialer.cpp/.hpp` | 启动复用、模组识别、EG912/EC200A 拨号、联网检测、恢复、诊断与 GPIO。 |
| `at/at_channel.cpp/.hpp` | AT 口进程锁、串口事务、统一超时、迟到响应重同步。 |
| `at/quectel_parser.cpp/.hpp` | CSQ、APN、运营商等 AT 响应字段解析及 COPS 选择校验。 |
| `device/modem_device.cpp/.hpp`、`at_port_cache.cpp/.hpp` | sysfs USB 拓扑定位、`option` 驱动 ID 注册、AT 端口映射缓存。 |
| `nw/network_configurator.cpp/.hpp` | 网卡、IPv4、默认路由、DNS、`udhcpc` 的启动/停止/查找。 |
| `startup/retained_network_probe.cpp/.hpp` | MQTT、PDP、网络注册和 QNETDEVCTL 响应解析与保留连接分类。 |
| `startup/network_wake_event_receiver.cpp/.hpp` | nanomsg 网络唤醒事件解析；**虽参与编译，当前 `main()` 没调用等待函数**。 |
| `state/` | IMEI、IMSI、ICCID、运营商、PLMN、APN、注册状态、CSQ 的线程安全快照入口。 |

## 3. 总体逻辑流程图

下图按 `main()`、`runModemService()` 和 `RK3506JDialer::dial_loop()` 的实际调用顺序绘制。使用普通文本图，Markdown 查看器无需 Mermaid 插件即可显示。`QNETDEVCTL` 在复用路径中的启动命令，仅用来将已有 PDP 暴露给 Linux；完整拨号会执行更多改动模组状态的命令。

```text
main() 进程启动
  │
  ├─ 初始化日志/版本/LED；取得 /tmp/modem_mng_at.lock
  │    └─ 失败 ───────────────────────────────────────────────→ 退出码 1
  │
  └─ 成功 → runModemService()
              ├─ CPActive、流量配置、nanomsg 端点后台重试
              └─ 拨号线程 dial_loop()
                    │
                    ├─ 禁止破坏性恢复；非破坏性 USB/AT 拓扑探测
                    └─ 查询 QMTCONN / QMTCFG pdpcid / CEREG / CGACT / QNETDEVCTL
                          │
                          ├─ MQTT 已连接，或网络注册且 PDP 活跃
                          │    └─ 尝试复用已有 CID：必要时 QNETDEVCTL 启动、
                          │       网卡启用、DHCP、IP/路由、ping
                          │         ├─ 接入且 ping 成功 → SUCCESS
                          │         ├─ 接入成功但 ping 失败 → CHECK_CONNECTION
                          │         └─ 主机接入失败 → SOFT_RECOVERY / REDIAL_AT
                          │                            （硬恢复暂禁）
                          │
                          └─ 无可复用连接 → 允许完整恢复；完整 USB/模组准备

  上述三种状态与完整准备结果，最终按模组类型进入状态机：
    EG912            → EG912 精简状态机
    EC200A / 未知类型 → 完整状态机

  状态机循环：成功时回调网卡并继续监测；失败时按对应路径恢复。
  SIGINT/SIGTERM 或首次拨号超时 → 请求退出、回收线程、关闭 LED。
```

### 3.1 进程与服务线程

1. `main()` 调 `log_init()`、输出版本；`AtChannel::acquireProcessOwnership()` 对 `/tmp/modem_mng_at.lock` 做非阻塞 `flock`，失败直接退出。然后建立网络 LED 对象，将启动模式固定设为 `UNKNOWN`，写 `/tmp/dial_version`，调用 `runModemService()`。这里**未调用** `NetworkWakeEventReceiver::waitForStartupMode()`；启动判断以模组内状态为依据。见 `main.cpp`、`at/at_channel.cpp`。
2. 服务注册 CPActive（30 秒）；`48001` 的事件 PUB 与 `38001` 的请求 REP 各在线程中尝试初始化，失败后约每秒重试。二者启动时可能因 `127.0.0.1` 尚不可用而失败，但不阻塞拨号线程。流量上限从 `/etc/config/config.json` 的 `C020102` 读取，解析抛异常时设 `0xffff`；字符串值的特殊处理见 5.5。见 `service/modem_service.hpp`。
3. 拨号线程调用 `dial.dial_loop()`，成功进入 `SUCCESS` 时通过回调公布实际网卡名；服务最多等待 360 秒得到首次回调。若超时，设置拨号退出标志；若拨号线程提前结束，则记错误。拿到网卡后调用流量监测。收到 `SIGINT`/`SIGTERM`，停止观察线程会设置退出标志。收尾时依次 join 拨号、停止流量监测、通知 nanomsg 线程、关闭 LED。服务返回值在首次拨号超时或提前结束时为 1，否则为 0。见 `service/modem_service.hpp`。

### 3.2 保留连接判定与复用

`dial_loop()` 先将“破坏性恢复”和“硬恢复”设为禁止，调用 `prepareCommonModemTopology(..., false)` 做最多约 4 秒的非破坏性拓扑准备；它不会按 PWRKEY。若 Linux 已启动足够久且未见原始 USB 设备，可在 600 毫秒宽限后提前结束此准备。即使未识别模组类型，也继续尝试使用可选中的 AT 口做通用只读查询。见 `rk3506j_dialer.cpp` 的 `dial_loop()`、`prepareCommonModemTopology()`。

| 查询 | 用途与判断 |
| --- | --- |
| `AT+QMTCONN?` | 查询模组内 MQTT 客户端 0；状态 `3` 被视为已连接，是最强的保留会话证据。 |
| `AT+QMTCFG="pdpcid",0` | 读取该客户端使用的 PDP CID；可解析到 1～15 时使用实值，否则回退 CID 1。 |
| `AT+QMTOPEN?` | 仅在 MQTT 状态为 3 时做诊断查询，不读取或消费下行缓存。 |
| `AT+CEREG?`、`AT+CGACT?` | 网络注册状态为 1 或 5，且目标 CID 的 PDP 活跃时，MQTT 即使没连也可视为可复用 PDP。 |
| `AT+QNETDEVCTL?` | 判断**指定 CID**的模组到主机数据通路是否已连接，不能把其他 CID 的记录当作目标。 |

分类优先级是 `QMTCONN=3 → connected-mqtt`；否则 `已注册且 PDP 活跃 → active-pdp`；否则 `none`。这是 `rk3506j_classify_retained_network()` 的准确条件，**MQTT 连接判定不额外要求 CEREG/CGACT 同时成功**。`CEREG` 解析要求完整行，超时产生的半截行不算注册成功。见 `startup/retained_network_probe.cpp`。

存在可复用连接时，约 10 秒窗口内：若 QNETDEVCTL 尚未连接，仅发送目标 CID 的 `AT+QNETDEVCTL=3,<cid>,1` 并查询结果；等待网卡出现，拉起接口；缺 IP 或默认路由时最多给一次 DHCP 约 3 秒；最后用该接口 ping `223.5.5.5`。ECM 接入或网卡出现失败，返回“有保留连接但接入失败”，先进入 `SOFT_RECOVERY/REDIAL_AT`，硬恢复保持禁止。主机接入成功但 ping 失败，进入 `CHECK_CONNECTION`；ping 成功则直接进入 `SUCCESS`。只有完全没有可复用连接时，才允许完整拓扑准备与硬恢复。见 `rk3506j_try_adopt_existing_network()`、`dial_loop()`。主机探测失败不能证明模组内 MQTT 已断；探测结果也不等于云端业务确认。

复用阶段使用 `QMTCFG` 报告的 CID（没有有效值时回退 1）；若复用失败进入 `REDIAL_AT`，重连函数却固定处理 CID 1。遇到日志 `MQTT PDP cid` 不等于 1 的设备，应把这个跨路径 CID 差异作为排查点，结合模组配置和实际连接状态判断影响，不能仅凭代码断言必然断线。见 `rk3506j_try_adopt_existing_network()`、`rk3506j_at_redial_modem()`。

## 4. 拨号与运行状态机

编译宏 `USE_EG912_MINIMAL_DIAL` 已在 RK3506J 分支定义。模组类型识别为 EG912 时进入 `dial_loop_eg912_minimal()`；识别为 EC200A 时走 `dial_loop()` 后半段的完整状态机；类型未知也走通用完整 AT 流程。两条路径共享启动复用、ECM 激活、网络配置、探测及恢复辅助函数，但运营商扫描只在完整路径。见 `rk3506j_dialer.cpp` 的 `dial_loop()`、`dial_loop_eg912_minimal()`。

```text
POWER_ON（准备 USB/模组）
  ├─ 成功 → WRITE_TO_MODEM（AT、SIM、注册、PDP、ECM）
  └─ 失败 → FAILURE_RETRY

WRITE_TO_MODEM
  ├─ 成功             → CONFIGURE_NETWORK
  ├─ shouldRetryAt()  → REDIAL_AT
  │   （AT 传输/拒绝、SIM、注册、PDP 配置或数据激活失败）
  └─ 其他失败         → FAILURE_RETRY（例如 AT 设备节点不可用）

REDIAL_AT（重建 CID1 数据通路）
  ├─ 成功 → CONFIGURE_NETWORK
  └─ 失败 → FAILURE_RETRY

CONFIGURE_NETWORK（网卡、DHCP、必要时静态地址回退）
  ├─ 网卡等待失败   → FAILURE_RETRY
  └─ 函数返回成功   → CHECK_CONNECTION

CHECK_CONNECTION（IP 与 ping 探测）
  ├─ 探测成功                 → SUCCESS
  ├─ 未到连续 5 次失败        → 留在 CHECK_CONNECTION
  └─ 连续 5 次失败            → 恢复阶梯，每组 5 次失败推进一级
       ├─ 第 1～2 轮          → CONFIGURE_NETWORK（DHCP）
       ├─ 第 3～4 轮          → REDIAL_AT
       ├─ 第 5～6 轮          → FAILURE_RETRY
       └─ 阶梯用尽
            ├─ EG912        → FAILURE_RETRY，重新开始恢复循环
            └─ 完整路径     → LIST_OPERATOR → SELECT_OPERATOR
                                  ├─ 历史/候选 PLMN 校验成功 → WRITE_TO_MODEM
                                  ├─ 扫描没有候选 → POWER_ON
                                  └─ 候选逐个用尽 → CONFIGURE_USB
                                                       └─ 兼容跳转 → WRITE_TO_MODEM

SUCCESS：标记成功、点亮 LED、回调网卡，等待 10 秒后返回 CHECK_CONNECTION。
FAILURE_RETRY：停止 DHCP；必要时开放硬恢复；尝试软关机，
               物理 PWRKEY 关机由配置决定；然后返回 POWER_ON。
```

图中 `CONFIGURE_NETWORK` 的“函数成功”只表示网卡等待完成和配置动作已尝试；该函数即使 DHCP/静态地址都未取得 IPv4，也会返回 `true`，后续由 `CHECK_CONNECTION` 的探测决定是否在线。`SUCCESS` 每次循环都回调网卡，并非仅首次回调。恢复阶梯是在**每组连续 5 次探测失败后推进一级**，不是每次单独失败都立即断开模组。依据：`rk3506j_prepare_data_network()`、两条状态机的 `CHECK_CONNECTION`/`SUCCESS`、`Rk3506jRedialContext::escalate()`。

### 4.1 完整拨号 AT 顺序

`WRITE_TO_MODEM` 先等 AT 设备节点及 `AT` 可用；冷启动可使用约 15 秒的功能就绪预算。然后读 `CFUN?`，必要时 `CFUN=1`；等待 `CPIN?` 为 `READY`；按 `MODEM_PDP_APN`/`MODEM_PDP_TYPE` 校验并配置 CID 1；等待 `CEREG?` 注册（最多约 30 秒）；检查/必要时执行 `CGACT=1,1`；发送 `QNETDEVCTL=3,1,1`，最终以 `QNETDEVCTL?` 查询目标 CID 已连接作为数据通路成功依据。`CFUN=1` 写超时后会再读状态，`CGACT?` 超时会做有界重试，`QNETDEVCTL` 写命令返回异常后仍查询真实连接状态，避免把单次应答不完整直接等同于未生效。见 `rk3506j_ecm_bringup_at()`、`rk3506j_ecm_activate_data()`。

`WRITE_TO_MODEM` 的失败去向依据 `Rk3506jBringupResult::shouldRetryAt()`，不能按日志里是否出现“AT”字样推断：

| 失败类型 | 下一状态 | 代码含义 |
| --- | --- | --- |
| `AtTransport`、`AtRejected`、`SimNotReady`、`Registration`、`PdpConfiguration`、`DataActivation` | `REDIAL_AT` | 即使根因在 SIM/注册/PDP，也先尝试 AT 侧重连；若重连失败，才进入 `FAILURE_RETRY`。 |
| `PortUnavailable` | `FAILURE_RETRY` | 等待的 AT 设备节点仍不可用，当前实现不先尝试 `REDIAL_AT`。 |

这张表只说明状态机的下一步，不表示 SIM 或注册问题必然能通过重拨修复。依据：`Rk3506jBringupFailure`、`Rk3506jBringupResult::shouldRetryAt()` 以及两个状态机的 `WRITE_TO_MODEM` 分支。

`MODEM_PDP_APN` 为空时保留模组现有 PDP 配置；非空时 PDP 类型缺省为 `IPV4V6`，只接受 `IP`、`IPV4`、`IPV6`、`IPV4V6`。CID 配置不一致时，代码需先停止 QNETDEVCTL 并停用该 CID 才改 `CGDCONT`；在保留会话保护期间禁止这样修改。见 `rk3506j_configure_pdp_profile()`。

### 4.2 EG912 与完整路径的差别

| 行为 | EG912 精简路径 | EC200A/未知完整路径 |
| --- | --- | --- |
| 首次完整拓扑已就绪 | 从 `WRITE_TO_MODEM` 开始 | 从 `WRITE_TO_MODEM` 开始 |
| 进入保留连接软恢复 | 从 `REDIAL_AT` 开始 | 从 `REDIAL_AT` 开始 |
| 网卡等待 | `30 × 2` 秒参数 | `5 × 2` 秒参数 |
| 连续 5 次探测失败后的阶梯 | DHCP 最多 2 轮、AT 重连最多 2 轮、模组恢复最多 2 轮；结束后重新进入恢复循环 | 同前三阶；最后尝试运营商选择 |
| 运营商选择 | `escalate(..., false)`，不进入 `LIST_OPERATOR` | 先试缓存 PLMN，再最多 3 次扫描 `AT+COPS=?`，逐个尝试并用 `AT+COPS?` 校验 |
| 身份和运营商刷新 | 网卡可用后按循环读取时间与缺失身份；周期更新运营商 | 网卡可用后尝试时间读取；缺失身份时读取 IMEI/IMSI/ICCID，联网成功后周期更新运营商 |

完整路径中的运营商手动选择发送 `AT+COPS=1,2,"<PLMN>",7`，但只有后续 `AT+COPS?` 返回与目标 PLMN 匹配的完整手选响应，才当作选择成功；单个命令的 `OK` 不足以确认运营商已切换。扫描没有任何候选时回 `POWER_ON`；进入 `SELECT_OPERATOR` 后若候选全部用尽，则转 `CONFIGURE_USB`，该兼容状态立即转 `WRITE_TO_MODEM`。见 `rk3506j_select_operator_and_query()`、`QuectelParser::selectedNumericOperator()`、`LIST_OPERATOR`/`SELECT_OPERATOR`。

### 4.3 网络配置与恢复边界

`network_iface()` 优先接受合法的 `MODEM_NET_IFACE`，否则使用同一 USB 模组发现的网卡；再考虑此前缓存的有效网卡，最后尝试 USB 网络驱动识别，板级回退名为 `eth1`。`NetworkConfigurator` 从 `getifaddrs` 看 IPv4、从 `/proc/net/route` 看目标网卡默认路由、从 `/etc/resolv.conf` 读 DNS。DHCP 通过 `udhcpc` 执行，查找进程时核对 `/proc/<pid>/cmdline` 的 `-i <网卡>`，不会仅凭 PID 文件杀任意进程。见 `network_iface()`、`nw/network_configurator.cpp`。

常规 `CONFIGURE_NETWORK` 先等待网卡，再运行一次前台 DHCP；无 IP 且允许破坏性恢复时，尝试解析 `AT+CGCONTRDP=1` 并调用 `ip addr replace` / `ip route replace` 配静态 IP 与默认路由，DNS 仍由系统管理。保留连接受保护时跳过此静态回退。`CHECK_CONNECTION` 用 `ping -I <网卡> -c 1 -W 2 223.5.5.5` 验证外部连通；成功写 `/tmp/dial_success`、点亮 LED、清零恢复计数，失败发布本机 `DIAL` 网络丢失事件。见 `rk3506j_prepare_data_network()`、`rk3506j_apply_static_ip_from_cgcontrdp()`、`test_can_ping_google()`。

`REDIAL_AT` 先确认 AT 口；只在 `QNETDEVCTL?` 明确显示 CID 1 ECM 已连接时发送停止命令，随后重新执行数据激活。`FAILURE_RETRY` 停止 DHCP，软恢复耗尽后才允许硬恢复，调用 `powerOffModule()` 再回 `POWER_ON`。关机先尝试 `QNETDEVCTL=0`、`CFUN=0`；只有 `MODEM_PHYSICAL_POWER_CYCLE` 为 `1`、`true` 或 `TRUE` 才附加 PWRKEY 物理关机。确认关机后才记录下电时刻，下次上电要求最少间隔 300 秒；AT 设备不存在或关机未确认时不设置这段冷却。见 `rk3506j_at_redial_modem()`、`powerOffModule()`、`waitBeforeModulePowerOn()`。

## 5. AT、USB 与状态实现细节

### 5.1 AT 事务可靠性

`AtSession` 用互斥锁串行化同一进程的 AT 操作，串口调用 `cfmakeraw()`，设置 115200 波特率、8 位数据位并关闭奇偶校验；代码没有显式修改停止位标志，因此不把它概括成必然的“8N1”。每次事务共用一个截止时间，覆盖清理输入、完整写入和读取响应。收到终止 `OK` 才是 `AtResult::ok()`，终止错误行则为失败；响应上限为 64 KiB。读取/写入超时会关闭句柄并标记端口需重同步，下次事务最多用 500 毫秒寻找 100 毫秒安静窗口，防止上次迟到应答混入下一命令。`/tmp/modem_mng_at.lock` 的 `flock` 只对遵守同一锁约定的进程有效，不能阻止任意外部脚本直接打开串口。见 `at/at_channel.cpp`、`at/at_channel.hpp`。

### 5.2 设备发现与缓存

`ModemDeviceLocator` 从 `/sys/class/tty` 与 `/sys/class/net` 追溯共同的 USB 根设备，要求匹配移远 VID（默认 `2c7c`）并把端口、网卡归于同一设备；可向 `option` 驱动的 `new_id` 注册 VID/PID（产品默认 `0901`），注册失败会记录诊断并继续扫描。模组类型通过同一 USB 拓扑的 ATI 响应识别，`MODEM_TYPE` 可显式覆盖；典型 EG912 主口/辅助口为 `ttyUSB0`/`ttyUSB5`，EC200A 主口为 `ttyUSB1`。节点号仍由当前 USB 枚举决定。见 `device/modem_device.cpp`、`rk3506j_identify_modem_family()`、`ensure_modem_at_ports()`。

`/opt/modem_at.state` 保存家族、VID/PID、USB 根路径、主/辅 AT 口。读取时限制为小型普通文件、限定字段和值格式，并与当前 USB 身份、拓扑及 AT 响应再次核对；用户显式设置 AT 口/家族时跳过缓存选择。写入使用临时文件、`fsync`、`rename`，只在验证并成功联网后保存。见 `device/at_port_cache.cpp`、`rk3506j_try_at_port_cache()`、`rk3506j_persist_at_port_cache_if_ready()`。

### 5.3 状态、日志与本机接口

`ModemSnapshot` 保存 IMSI、IMEI、ICCID、运营商、PLMN、APN、注册状态和 CSQ，`PlatformModemState::snapshot()` 提供公共请求处理代码读取。两个拨号循环都会刷新 CPActive，记录约 30 秒简版心跳与约 5 分钟详版心跳；详版包含网卡/IP/网关/路由/DNS、CEREG、PDP、CSQ、CPU 温度、COPS/CGDCONT/QENG、网卡流量计数。`fetchAndSaveTimezone()` 发 `AT+QLTS=1` 并尝试写 `/usr/dial/tz.ini`；解析成功就置 `tz_fetched_`，即使后续保存文件失败，本进程也不再重复获取。见 `state/`、`log_heartbeat()`、`fetchAndSaveTimezone()`。

本机 `tcp://127.0.0.1:38001` 是请求/应答服务，`48001` 是网络事件发布；`nanomsg_process.cpp` 的部分字段从 `PlatformModemState` 取快照。拨号循环以绑定蜂窝网卡的 **ICMP ping `223.5.5.5`** 判断探测是否成功；公共请求处理里的 `network_online` 则以绑定网卡的 **TCP 连接 `223.5.5.5:53`** 判断，两者不是同一指标。`/tmp/dial_success` 是本机探测成功后写入的运行态标记，失败路径会删除；保留连接复用的首次 ping 通过时，`[INTERNET-READY]` 由复用函数先记录，状态机随后才写成功标记。`/tmp/dial_version` 记录显示版本。网络 LED 在尝试拨号时闪烁、探测成功时常亮、故障恢复或退出时关闭；这些是代码动作，具体可见亮灯效果取决于板级驱动。见 `service/modem_service.hpp`、`main.cpp`、`nanomsg_process.cpp`、两个状态机。

### 5.4 本机状态请求的实际边界

`38001` 的 REP 处理器接收 JSON 帧，要求存在 `ts`、`status.network` 数组；数组中的字符串含 `cellular` 时才构造 `status.network.cellular` 回复。JSON 无法解析、缺字段、`network` 类型不对或数组项非字符串时会记节流的 `[NANOMSG] bad request` 日志并跳过该项/请求；普通接收超时表示没有客户端请求，不触发重绑。连续非超时接收失败达到 10 次才尝试重连；正常状态回复日志每约 300 秒汇总一次，因此不能按“每次请求都有一条日志”排查。见 `NanoReqHandler::processModemRequests()`。

状态中的 IMEI、IMSI、ICCID、CSQ、运营商、PLMN、注册状态和 APN 取当前平台快照，查询不会为这些字段额外轮询 AT；`ip` 读取已回调的网卡，尚未回调时可能缺失。`network_online` 做绑定网卡的 TCP 53 连接，回调前回退 `eth1`；`gateway` 的 RK3506J 分支却固定查询 `eth1` 的默认路由，若实际数据网卡另有名称，`gateway` 可缺失，即使 `network_online` 使用的是真实网卡。`lac`/`cellid` 在 RK3506J 分支没有现场小区查询，分别落到固定占位值 `271a`/`0d176547`；`phonenum` 固定为 `186`，`network_type` 固定为 `1`。这些值不能当作设备实时测量。`pdn_status`、`dial_failure` 两个字段仅在 `USE_IMX6U_DIAL` 分支添加，RK3506J 回复不提供。见 `ModemReqHandler::getStatus()`、`getCellInfo()`、`getGateway()`、`getNetworkType()`。

### 5.5 流量监测与持久化

服务在首次 `SUCCESS` 回调公布网卡后启动 `TrafficMonitor::check_CCINet_FlowOverLimit()`。监测线程约每 10 秒采一次该网卡的 RX/TX 字节计数，每 6 次循环尝试持久化到 `/usrdata/traffic_record.db` 的 `MonthlyTraffic`、`HistoryTraffic`；`tx`/`rx` 状态字段是历史累计字节数除以 1024。`C020102` 被直接当作**字节阈值数值**与当月 RX+TX 字节和比较；超过后按代码的节流频率发布 `TrafficOverLimit`，不会在此处停拨。配置解析抛异常时回退 `0xffff`；如果该键返回字符串，`modemServiceReadInt()` 不写入数值，进程初始静态值仍为 `0`，不应把所有格式异常都说成回退 `0xffff`。数据库不可用时记录 `[TRAFFIC] sample/persist skipped` 并继续内存统计；只有成功恢复或提交后才可把持久数据当作可信历史。`NoTraffic` 的独立监测函数虽存在，当前 RK3506J 服务没有调用。见 `service/modem_service.hpp`、`nanomsg_process.hpp`、`traffic_sql/sql_traffic.hpp`。

## 6. 构建、配置与验证入口

| 项目 | 代码依据与约束 |
| --- | --- |
| 构建选择 | `WITH_RTMS_CORE=ON`；`QL_MODULE_PLATFORM=RK3506J`；C++14。`CMakeLists.txt` 依赖目标 SDK、交叉编译器及第三方库。 |
| USB/串口覆盖 | `MODEM_TYPE`、`MODEM_USB_VENDOR`、`MODEM_USB_PRODUCT`、`MODEM_AT_PORT`、`MODEM_AT_PRIMARY`、`MODEM_AT_AUX`；应对应同一实际 USB 模组。 |
| 网卡/PDP | `MODEM_NET_IFACE`、`MODEM_PDP_APN`、`MODEM_PDP_TYPE`；未配置 APN 时保留模组配置。 |
| 流量 | `/etc/config/config.json` 的 `C020102`；代码将解析出的整数直接与当月 RX+TX 字节比较，配置异常及字符串值的处理差别见 5.5。 |
| 电源 | `MODEM_PHYSICAL_POWER_CYCLE` 默认为不执行物理关机；GPIO PWRKEY 编号在头文件中为 32，目标板接线须核对。 |
| 运行文件 | `/opt/modem_at.state`、`/tmp/modem_mng_at.lock`、`/tmp/dial_success`、`/tmp/dial_version`、`/var/run/udhcpc.<网卡>.pid`、`/usr/dial/tz.ini`。 |
| 单元测试 | `MODEM_MNG_BUILD_RK3506J_UNIT_TESTS` 默认 `OFF`；现有 `at_channel_test.cpp`、`at_port_cache_test.cpp`、`modem_device_test.cpp`、`retained_network_probe_test.cpp`。主机配置会注册对应 CTest；交叉构建的 ARM 测试目标不注册主机 CTest，安装另由 `MODEM_MNG_INSTALL_RK3506J_UNIT_TESTS` 决定。 |

本次核验用当前源码在 `/tmp` 单独编译并运行上述四个主机测试程序，四个退出码均为 0。其中 `retained_network_probe_test` 明确输出 `retained-network probe tests passed`；其余通过退出码判断。`logger_sd.c` 需作为 C 文件单独编译再链接 AT 通道测试。上述结果只验证测试覆盖的解析、缓存、设备发现和 PTY AT 通道行为，不覆盖真实模组、网卡、GPIO 与云服务。

## 7. 由代码可确认的限制与核查点

1. `NetworkWakeEventReceiver` 实现了订阅并识别事件码 13 的功能，但当前 `main()` 直接设 `UNKNOWN`；不能声称本版本靠收到事件 13 来区分冷启动/远控唤醒。
2. `rk3506j_prepare_data_network()` 在 DHCP 与静态地址都未成功时仍可返回 `true`；必须继续看实际 IP、路由和 `CHECK_CONNECTION` 探测。`/tmp/dial_success` 只是本机探测路径的状态标记，不是云端业务确认。
3. `QMTCONN=3` 单独足以进入 `connected-mqtt` 类别；该分类基于模组 AT 回应，不代表云端下行已被目标应用消费。启动复用路径没有发送 `QMTRECV`。
4. 代码中 `test_can_ping_google()` 的名字和部分失败日志沿用旧称，实际探测目标是 `223.5.5.5`；受限 APN、ICMP 被禁或目标不可达时，失败含义需结合路由和业务地址核对。
5. 完整路径的 `CONFIGURE_USB` 是兼容状态，常规新入口从 `POWER_ON` 的共用拓扑准备转到 `WRITE_TO_MODEM`；不应把它画成每次启动都会执行的额外 USB 阶段。
6. 源码没有在这里证明真机模组固件对每条 AT 指令的实际响应、`eth1` 的真实绑定、MCU 电源时序、运营商业务可达性或云端 MQTT 端到端效果。这些需要相应设备日志和联调结果，不能由静态代码推断。

## 8. 设备故障排查手册

本节用于判断“问题是否落在 `modem_mng` 的哪一层”，按当前源码中的日志、返回路径和文件编写。先收集证据，再判定故障点；不要把一次 ping 失败直接归因于 SIM、PDP 或程序本身。下面的现场命令均为**只读**，适用于设备具备相应 BusyBox/Linux 命令的情况；缺少某命令时读取同一 `/proc`、`/sys` 信息即可。

### 8.1 先固定同一次故障的证据

1. 记录设备时间、启动时间、程序版本和进程状态。`/tmp/dial_version` 是运行时版本文本，需与本文源码基线比较；源码 Git 分支名不会自动写进设备文件。若版本不同，应以设备实际版本的源码重新定位。
2. 查 `/media/sdcard/dial_log/<日期>/dial_*.log`；系统时间早于 2020 年时日志在 `unsynced/`。`logger_sd.c` 要求 SD 卡挂载且初始化时可用空间至少 500 MiB；否则 `dial_log()` 只向 stdout 输出，磁盘没有日志不等于进程没有运行。日志有按天清理及空间回收策略，发生故障后应及时保存相关时段。
3. 从同一启动时段依次找 `[SERVICE]`、`[STARTUP]`、`[FAST-BOOT]`、`[probe]`/`[AT-CACHE]`、`[EG912]` 或 `[FULL-DIAL]`、`[bringup]`、`[HB30]`/`[HB300]`。不要把前一次启动的日志与本次 `/tmp` 状态文件混用。

```sh
date
cat /proc/uptime
cat /tmp/dial_version
ps | grep '[m]odem_mng'
mount | grep ' /media/sdcard '
df -m /media/sdcard
find /media/sdcard/dial_log -type f -name 'dial_*.log'
```

日志路径和门槛来自 `logging/logger_sd.c`；版本文件来自 `main.cpp`/`logging/logger_sd.h`。`ps`、`mount`、`df` 的具体输出由设备镜像决定。收集日志给他人分析时，检查并遮盖 IMEI、IMSI、ICCID、APN、PLMN 等设备或网络标识。

### 8.2 第一轮定位：按“最后成功的一层”往下查

| 现场现象或日志 | 先核对什么 | 对应代码路径与下一步 |
| --- | --- | --- |
| 找不到进程，或没有 `/tmp/dial_version` | `ps`、启动器的标准输出/系统日志、程序文件是否存在；日志是否因 SD 卡不可用而只在 stdout。 | `main()` 在 AT 进程锁失败时直接返回 1；若根本没启动到 `main()`，需查启动器、动态库与镜像部署。版本文件写入失败会记 `[VERSION]`，单独缺文件不等于进程未启动。 |
| `[AT] ownership lock is held by another process` | 是否已有另一个 `modem_mng`，以及是否有遵守 `/tmp/modem_mng_at.lock` 的独立脚本占用。 | `AtChannel::acquireProcessOwnership()` 非阻塞加锁；排查并发启动原因。锁文件存在本身不能证明有人持锁，不能直接靠删除锁文件判断。 |
| 没有 SD 日志，但进程仍在 | 挂载点、可用空间、`[LogInit]` stdout。 | `log_init()` 初始化时检查 SD 卡及 500 MiB 门槛；失败走控制台输出。先找启动器收集的 stdout，再查日志介质。 |
| 反复 `[NANOMSG] ... unavailable`，拨号日志仍继续 | `127.0.0.1` 是否可用，本机 38001/48001 是否被其他进程占用；看是否随后出现 `ready`。 | `runModemService()` 的两个端点后台每秒重试，单独的 nanomsg 失败不会阻止拨号；若蜂窝已成功而本机请求失败，重点查本机 IPC/回环接口。 |
| `[SERVICE] dial timeout after 360s` | 在超时前最后一条 `[EG912]`/`[FULL-DIAL] state=...` 与失败原因；不要只看 360 秒这一总结果。 | 服务等的是 `SUCCESS` 回调；USB、AT、SIM、PDP、DHCP、探测任一层卡住都可造成总超时。按下表对应状态继续查。 |
| `[FAST-BOOT] retained=connected-mqtt`，随后保留连接失败 | `cid`、`qnet`、`ECM attach failed`、网卡是否出现；注意当时是否仍应保留模组 MQTT。 | `rk3506j_try_adopt_existing_network()` 根据模组内状态复用；若主机侧接入失败先走软件恢复，不能把 `QMTCONN=3` 当成 Linux 网卡已就绪。 |
| `retained=none`，但现场认为是远程唤醒 | QMTCONN、CEREG、CGACT 各自结果和 AT 超时；区分“模组确已断”与“查询不可用”。 | `rk3506j_classify_retained_network()` 只据这些 AT 可解析状态分类；当前入口不使用事件 13 做决定。需要模组侧和 MCU 启停时序证据，不能凭开机原因单独判程序误判。 |
| `[probe] modem family not identified` 或 `AT port unavailable` | USB VID/PID、原始 USB 是否出现、`/dev/ttyUSB*`、`/sys/class/tty`、缓存是否被拒绝。 | `prepareCommonModemTopology()`、`ensure_modem_at_ports()`；先确认同一 USB 设备下的 AT 口和网卡，再判断是枚举/驱动、端口选择还是 ATI 识别。 |
| `[AT-READY] ...` 超时或频繁 `response stream resynchronized` | AT 口是否匹配当前模组、USB 是否断续、响应是否只有 URC/半截内容、每次命令总耗时。 | `AtSession` 会在超时后关闭并重同步端口；需要区分 AT 传输失败与模组明确返回 `ERROR`。不要在服务运行时另开脚本抢发 AT 指令。 |
| `CPIN not ready`、`CEREG wait timed out` | SIM 状态、注册码、CFUN 状态、信号质量和运营商策略；看 `HB300` 的 `cops`/`serving_cell`。 | 分别落在 SIM 就绪或网络注册阶段，尚不能归因于 DHCP。源码 `rk3506j_ecm_activate_data()` 先等 SIM/注册，再激活数据；这些失败会被 `shouldRetryAt()` 导向 `REDIAL_AT`，并不证明 AT 串口本身坏了。 |
| `QNETDEVCTL did not reach connected state` | `CGACT` CID 1 是否活跃、`QNETDEVCTL?` 返回的目标 CID/状态、AT 命令是否执行完成。 | `rk3506j_ecm_activate_data()` 以**查询到目标 CID 已连接**为判定，不以启动命令 `OK` 单独判成功。 |
| 网卡出现但无 IPv4 或默认路由 | 实际接口名、`udhcpc` 是否属于该接口、`CGCONTRDP` 回退是否成功。 | `rk3506j_prepare_data_network()` 可能在 DHCP 失败后仍返回 `true`；继续查看 `CHECK_CONNECTION` 和 `HB300 ip/gw/route`，区分主机配置与 PDP 问题。 |
| `/tmp/dial_success` 存在，但业务仍报离线 | 看本次启动后的 `[INTERNET-READY]`、`[HB30] online`、业务地址可达性及 MQTT 状态。 | 标记只表示代码曾通过本机 ICMP 探测；同一 Linux 启动内重启进程时，旧 `/tmp` 标记可能在新进程首次失败处理前短暂保留。它不代表当前云端业务在线。 |
| `[HB30] online=1` 与 `network_online=0` 不一致 | 同一时刻的 ICMP 与 TCP 53 连通性、公共请求服务实际绑定的网卡。 | 状态机用 ICMP ping `223.5.5.5`，公共请求用 TCP `223.5.5.5:53`，协议不同；且网卡名未通过成功回调设置前，公共请求使用 `eth1` 回退值。先核对网络策略和接口名。 |
| 反复进入 `REDIAL_AT`/`FAILURE_RETRY` | 连续探测失败数、`redial stage`、`[POWER]`、恢复标志和模组类型。 | EG912 的阶梯末端重新走恢复循环；完整路径可进入 COPS 选择。`FAILURE_RETRY` 会尝试软关机，物理关机取决于配置与执行结果。 |
| `invalid MODEM_PDP_APN or MODEM_PDP_TYPE` 后反复重拨 | 实际启动环境里的 APN/PDP 类型是否符合代码校验，是否与模组当前 CID 1 配置一致。 | 这是配置校验失败，不等于串口或 SIM 损坏；`PdpConfiguration` 会走 `REDIAL_AT`，若配置始终不改，重试本身不能使非法值变合法。 |
| 关机后长时间停在上电前，或频繁 `[POWER]` | 是否有已确认的 `CFUN=0`/物理关机、300 秒冷却、AT 设备节点是否还在、`MODEM_PHYSICAL_POWER_CYCLE` 值。 | `waitBeforeModulePowerOn()` 在确认下电后按 300 秒间隔约束上电；物理关机开关只控制关机动作，不能把所有 PWRKEY 上电脉冲都归因于该开关。 |
| `38001` 无状态或 `48001` 无事件，但 ICMP 已通 | `[NANOMSG] ready/unavailable`、回环接口与消费者进程。 | 本机 IPC 与蜂窝数据面独立；`NetWork_EventPublisher` 可因初始化未完成而暂时无法发布，需按端点和消费者分别查。 |
| `38001` 可连接但请求无回复，或字段看似异常 | 请求 JSON 是否含 `ts`、`status.network` 数组及 `cellular` 字符串；查 `[NANOMSG] bad request`、`nn_send failed`、`status served OK`。 | 畸形请求会被跳过；RK3506J 的 `lac`、`cellid`、`phonenum`、`network_type` 含固定值，`gateway` 固定查 `eth1`，不能把这些字段直接当作实时链路证据。见 5.4。 |
| 流量显示为零、累计值回退或限额事件异常 | 首次 `SUCCESS` 回调是否发生、网卡计数是否可读、`[TRAFFIC] sample/persist skipped`、`/usrdata/traffic_record.db` 是否可用及设备时钟。 | 监测须在首次网卡回调后启动；采样/数据库故障不应当作确实零流量。`C020102` 与月累计 RX+TX 字节直接比较；详见 5.5。 |

### 8.3 各层只读检查及判读

**USB、AT 口与网卡。**先看 `MODEM_TYPE`/`MODEM_AT_*`/`MODEM_NET_IFACE` 是否由启动环境覆盖，再看 USB 枚举和 sysfs 关系。`/opt/modem_at.state` 是缓存，不是当前硬件的最终真相；日志 `[AT-CACHE] accepted` 表示本次又做了校验，`pending` 表示枚举可能尚未完成，`ignore` 表示内容或身份不符。`ttyUSB0/1/5` 是代码的板级布局与回退规则，不能仅凭节点号认定模组家族。进程运行时不要直接运行 `at2.sh`、`4g_bringup.sh`：这些脚本自己也声明不得与 `modem_mng` 并发控制 AT 口。依据：`device/modem_device.cpp`、`rk3506j_try_at_port_cache()`、`scripts/`。

```sh
ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
ls -l /sys/class/net
cat /opt/modem_at.state
cat /proc/mounts
```

**SIM/网络注册/PDP。**沿同一次启动日志按 `AT-READY basic` → `CFUN` → `CPIN` → `CEREG` → `CGACT` → `QNETDEVCTL` 找首次失败处；看 `status`（串口事务状态）、`ok`（是否完整终止 OK）、`response`（模组返回）。日志中的 `read_timed_out` 对应 `AtTransportStatus::ReadTimedOut`，明确的 `ERROR`、可解析的 `+CEREG` 和可解析的 `+QNETDEVCTL` 又是不同证据。要联查 `HB300` 的 `cereg`/`pdp`/`csq`/`cops`/`context`，但心跳是采样，不能倒推故障瞬间一定相同。只有在确认不会破坏保留 MQTT 会话、并具备设备操作条件时，才考虑人工 AT 干预；只读排查阶段以程序已有日志为主。依据：`rk3506j_ecm_activate_data()`、`log_heartbeat()`、`at/at_channel.cpp`。

**主机数据面。**在日志里先确定 `if=` 或 `[SERVICE] 4G interface=`，不要预设 `eth1`。对该网卡核对地址、默认路由、DHCP 进程和网卡收发计数；`HB300 traffic_valid=0` 表示该次计数样本不可用，不能当作流量为零。`HB300 pdp` 来自 QNETDEVCTL 查询，`ip`/`route` 来自主机；组合它们可把“模组数据通路未建好”和“Linux 网卡配置未完成”区分开。当前两个状态机调用 `log_heartbeat()` 时，`fail_streak` 和 `retry` 都传入同一个 `checkConnectionAttempts`，**不能把 `retry` 字段理解为 DHCP/AT/模组恢复阶梯计数**；恢复阶梯要看 `redial stage` 日志。依据：`network_iface()`、`nw/network_configurator.cpp`、`log_heartbeat()` 及两个状态机调用处。

```sh
ip -4 addr show
ip route show
cat /proc/net/route
ps | grep '[u]dhcpc'
cat /proc/net/dev
```

**联网探测与业务。**`[INTERNET-READY]` 在每次进程运行中最多记录一次，不是每次恢复成功都重复记录；后续应看 `[HB30] online`、`[HB300] probe` 和 `connectivity restored`。保留连接复用的首次 ping 成功会先打印 `[INTERNET-READY]`，随后状态机才写 `/tmp/dial_success` 并在 `SUCCESS` 回调网卡。若日志有该记录但标记缺失，应结合后续失败、进程重启与文件写入情况判读。`/tmp/dial_success`、网络 LED、`network_online` 各有不同判断条件。公共请求的 `network_online` 在网卡名尚未由 `SUCCESS` 回调设置时回退使用 `eth1`；若实际网卡不是 `eth1`，启动早期该字段可能与拨号器探测结果不同。若网卡 IP、路由和 ICMP 均正常，只有某业务域名、端口或云端 MQTT 不通，应进一步查业务端 DNS、证书、端口、模组内 MQTT 和下游进程日志；本文件不把这些业务失败归为拨号程序故障。依据：`rk3506j_try_adopt_existing_network()`、`rk3506j_announce_internet_ready()`、`test_can_ping_google()`、`nanomsg_process.cpp`、`runModemService()`。

### 8.4 按状态串起同一次故障

拿到一份完整拨号日志后，先按时间顺序找启动判定、状态转移、首次失败、恢复动作与下一次探测结果。下面的命令只筛选日志，不会发送 AT 指令；把文件名替换成故障当次的实际日志。若跨天或日志轮转，应按时间合并阅读相邻文件，不能只看最后一段。

```sh
grep -nE '\[FAST-BOOT\]|\[AT-CACHE\]|\[EG912\]|\[FULL-DIAL\]|\[bringup\]|\[HB30\]|\[HB300\]|\[INTERNET-READY\]|\[POWER\]|\[NANOMSG\]|\[SERVICE\]' '/media/sdcard/dial_log/<日期>/dial_<文件名>.log'
```

1. 先确认 `[FAST-BOOT] retained=...`：`connected-mqtt`/`active-pdp` 进入复用尝试，`none` 进入完整准备。若 AT 查询超时导致分类为 `none`，继续看同段 `status` 与原始响应，不能把 `none` 直接等同模组确实掉线。
2. 对照 `adopted ... action=... ip=... route=... internet=...`：`internet=1` 只证明当次指定网卡的 ICMP 探测；若 `ECM attach failed` 或 `interface ... is absent`，沿软恢复路径查；若已接入但 `internet=0`，先查 IP、路由和探测环境。
3. 找进入 `WRITE_TO_MODEM` 后第一次失败的 `bringup` 或 `AT-READY` 记录，按 AT 端口、CFUN、CPIN、PDP 配置、CEREG、CGACT、QNETDEVCTL 顺序归层。`REDIAL_AT` 是状态机选择的下一动作，不等于根因必在串口。
4. 若先有 `SUCCESS` 后掉线，连续看 `[HB30] online`、`fail_streak`、`redial stage` 与最终 `state=...`。每组五次失败推进一次恢复阶梯；`retry` 字段与 `fail_streak` 同源，不能当作阶梯轮数。
5. 最后核对业务报障的时间是否落在本程序探测失败窗口；若本程序持续 `online=1`，仍需核查业务地址、TCP/UDP 协议、DNS、模组 MQTT 与下游应用，不能仅凭 ICMP 成功排除这些层。

### 8.5 复现和提交问题时最有用的记录

给开发人员的故障记录至少包含：设备硬件/模组型号与固件、`/tmp/dial_version`、发生时间及 Linux 启动时长、是否有 MCU 断电/远控唤醒、同一时段完整拨号日志、首次失败的状态和 AT 原始响应摘要、实际 AT 口及网卡、IP/默认路由、`HB30`/`HB300`、恢复阶梯次数，以及业务侧失败的目标地址/协议。若问题只在休眠后发生，必须保留唤醒前后两段日志，并指出 `[FAST-BOOT] retained=...` 与 `adopted ... action=... internet=...` 的结果。没有这些证据时，只能定位到疑似层级，不能严谨断定根因属于本程序。
