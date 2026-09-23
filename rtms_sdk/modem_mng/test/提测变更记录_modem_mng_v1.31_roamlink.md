# modem_mng v1.31 Roamlink 功能提测变更记录

**提测日期**：2026-05-22  
**版本号**：v1.31.0  
**平台**：EG25G（主测）/ EC200A（辅测）  
**分支**：`develop/rtms_sdk_v1.3_20240408_dc_switch`  
**提测人**：yuping.liu  
**涉及提交**：`3bc83208` → `9b9dc05c`（共 8 个提交）

---

## 一、变更概述

本次提测在 EG25 和 EC200A 双平台引入 **Roamlink 虚拟 SIM 双卡自动切换**功能，并在此基础上完成以下增强与修复：

| 类型 | 内容 | 提交 |
|------|------|------|
| 新功能 | Roamlink 双卡切换状态机（EC200A/EG25 双平台） | `3bc83208` |
| 重构 | 合并 roamlink 模块，统一双平台实现 | `c920ebcc` |
| 文档 | roamlink.h 接口注释完善 | `c41b787e` |
| 新功能 | EG25 Roamlink 状态机枚举化 + rx_packets 业务层监控 | `2484d2ff` |
| 新功能 + 修复 | EG25 启动/心跳诊断日志体系 + 120s 周期重拨 Bug 修复 | `bf0ec6d3` |
| 新功能 | EG25 心跳日志字段增强（CH / RL_FAIL / RX_PKT） | `45126949` |
| 修复 | EG25 L1/L2/L3 恢复逻辑修复 + roamlink_start_service 跳过无效 AT+COPS=0 | `7001af5f` |
| 修复 | EG25 nanomsg handler 查询 ICCID/IMSI 返回空 | `6f0e3eb4` + `9b9dc05c` |

---

## 二、新增功能详细说明

### 2.1 Roamlink 双卡切换状态机

#### 2.1.1 功能背景

RTMS 设备此前只依赖物理 SIM 联网。部分部署场景中物理 SIM 信号弱或运营商限制，需要通过 RBMaster 进程提供的虚拟 SIM（Roamlink）通道作为备用或主用网络。本次集成实现了物理 SIM 与虚拟 SIM 的自动切换，支持四种策略，并在两个平台（EG25G / EC200A）完全对齐。

#### 2.1.2 四种网络策略

策略通过配置文件 `/usrdata/network.ini` 的 `network_select` 字段指定，默认为 4（强制物理 SIM，不启用任何切换）。

| 策略值 | 宏名 | 说明 |
|--------|------|------|
| 1 | `NET_POLICY_PREFER_ROAMLINK` | 优先 Roamlink；Roamlink 失败后切换到物理 SIM，物理 SIM 稳定 300s 后自动回切 Roamlink |
| 2 | `NET_POLICY_PREFER_SIM` | 优先物理 SIM；物理 SIM 失败后切换到 Roamlink，Roamlink 稳定 300s 后自动回切物理 SIM |
| 3 | `NET_POLICY_FORCE_ROAMLINK` | 强制 Roamlink；通道超时只重启 RBMaster，绝不切换到物理 SIM |
| 4 | `NET_POLICY_FORCE_SIM` | 强制物理 SIM，不启用双卡切换（**默认值，老行为兼容**） |

#### 2.1.3 Roamlink 前置条件探测（probe）

程序启动时执行一次 `roamlink_probe()`，按顺序检查三个前置条件：

1. RBMaster 二进制存在且可执行（`/usrdata/roamlink/RBMaster`）
2. conf.ini 存在（`/opt/conf.ini`，由 factoryApp 预置）
3. License 文件存在且非空（`/usrdata/roamlink/etc/.pconfig/license.cer`）

| Probe 结果 | 处理行为 |
|-----------|---------|
| `PROBE_OK` | roamlink_available = true，按策略正常运行 |
| `PROBE_NO_PACKAGE` | 永久降级为 FORCE_SIM，不再尝试切换 |
| `PROBE_CONF_MISSING` | 降级为 FORCE_SIM，等待 factoryApp 预置后重启 |
| `PROBE_LICENSE_MISSING` | 先从备份路径恢复（`/data/ufs/license.cer`）；失败则进入 license_pending 模式 |

#### 2.1.4 License 自动下载机制

当 probe 返回 `LICENSE_MISSING` 且本地备份也不存在时，系统进入 `license_pending` 模式：

1. 临时强制物理 SIM，让设备先联网
2. 物理 SIM 联网后，延迟启动 RBMaster，由 RBMaster 利用现有网络下载 license
3. 每 60s 轮询一次 license 文件是否出现（`LICENSE_CHECK_INTERVAL_SEC = 60`）
4. License 出现 → 备份到 `/data/ufs/license.cer` → 重启系统（重启后 re-probe 结果为 OK）
5. 超过 300s 仍未出现 → 放弃 license_pending，保持 FORCE_SIM 继续运行

#### 2.1.5 切换状态机时序

**物理 SIM → Roamlink（主动切换）**
- 触发条件：物理 SIM 连续失联 ≥ 180s（`ROAMLINK_SWITCH_TIMEOUT_SEC`），且 roamlink_available == true，策略为 1/2
- 动作：`roamlink_start_master()` → `roamlink_start_service()` → 进入 `roamlink_starting` 状态 → 等待首次 ping 通（最长 300s）→ 进入 `roamlink_active`

**Roamlink 建链超时 / 再次失联**
- Roamlink 启动后 300s 内从未 ping 通，或建链后再次失联 300s：
  - 策略 1/2：停止 Roamlink → 切回物理 SIM，记录 `sim_fallback_ts`
  - 策略 3：不切物理 SIM，重启 RBMaster 服务，重新等待

**双向自动回切（策略 1/2）**
- 策略 1：在物理 SIM 备用且稳定 300s → 尝试切回 Roamlink（`SIM_FALLBACK_RETRY_SEC`）
- 策略 2：在 Roamlink 备用且稳定 300s → 尝试切回物理 SIM（`ROAMLINK_FALLBACK_RETRY_SEC`）

#### 2.1.6 Ping 抖动过滤器

新增 `ping_fail_count` 计数器，需连续 3 次（`PING_FAIL_THRESHOLD`）ping 失败才认定为真实断网并启动故障计时。单次偶发超时不触发切换或恢复动作。

---

### 2.2 EG25 Roamlink 状态机枚举化重构

**重构前**：Roamlink 通道状态仅用 `is_roamlink_active`（bool）表示，无法区分"已启动但从未 ping 通"和"已稳定运行"两种状态。

**重构后**：在 `dial_stat_enu` 中新增两个 Roamlink 专属枚举值：

```c
dial_stat_roamlink_starting = 17  // Roamlink 服务已启动，等待首次 ping 通
dial_stat_roamlink_active   = 18  // Roamlink 通道稳定运行中
```

状态转移路径：`roamlink_starting` → (ping OK) → `roamlink_active`

L1/L2/L3 分级恢复增加守卫，在 Roamlink 状态期间不触发物理 SIM 恢复动作，防止逻辑冲突。

---

### 2.3 EG25 业务层存活监控（rx_packets）

TCP/ICMP ping 通只证明网络层可达。Roamlink 虚拟 SIM 注册失败时 ping 可能仍通，但 `rmnet_data*` 接口的 `rx_packets` 不会增长，业务数据实际无法流转。

**新增函数**：`nw_get_rmnet_rx_packets_sum()`——遍历 `/sys/devices/virtual/net/rmnet_data*`，累计各接口 `statistics/rx_packets`。

**监控流程**：
- 进入 `roamlink_active` 时：记录初始基准值 `roamlink_rx_packets` + 时间戳 `roamlink_no_data_timer`
- 每次心跳（30s）：读取当前 rx_packets；有增长则更新基准，无增长则打印"无增长日志"
- 每秒检测：若距上次增长 > 120s（`ROAMLINK_NO_DATA_TIMEOUT_SEC`），策略 1/2 切回物理 SIM，策略 3 重启 RBMaster

---

### 2.4 EG25 启动诊断日志

SDK 初始化成功后立即打印硬件/固件快照，帮助快速定位设备环境问题。

| 日志标签 | 触发时机 | 内容 |
|----------|---------|------|
| `[MODEM]` | SDK 初始化成功 | 模组型号（AT+CGMM）、固件版本（AT+QGMR）、子版本（AT+CSUB）、CFUN 状态、温度（AT+QTEMP） |
| `[INIT]` | SIM 操作完成 | ICCID、IMSI、IMEI、PDP 上下文（AT+CGDCONT?，跳过空 APN） |
| `[CELL]` | CEREG 注册成功 | 服务小区快照（AT+QENG="servingcell"），同时初始化 `last_cell_id` |
| `[SDK]` | DataCall 回调 | 连接：profile/IP/GW/DNS（IPv4）；断开：profile/错误码 |

---

### 2.5 EG25 心跳日志增强

**30s `[HEARTBEAT]` 格式升级**，新增字段：

| 字段 | 说明 |
|------|------|
| `CH` | 当前通道：`SIM` 或 `ROAMLINK` |
| `DownTime` | 当前断网持续秒数 |
| `ConsecFail` | 连续 ping 失败次数 |
| `RL_FAIL` | Roamlink 切回物理 SIM 累计次数（仅 roamlink_available 时打印） |
| `RX_PKT` | rmnet_data* rx_packets 当前值（仅 Roamlink 通道时打印） |

Roamlink 通道时不打印 `REG` 字段（无诊断价值）。

**每 5 分钟**在心跳行后追加扩展字段：RSRP/RSRQ（dBm，AT+QENG 解析）、Cell ID、已分配 IP（AT+CGPADDR）。

**新增事件日志**：

| 标签 | 触发条件 | 内容 |
|------|---------|------|
| `[CELL CHANGE]` | 心跳检测到 Cell ID 变化 | 旧 Cell ID → 新 Cell ID + 完整 QENG 原始行 |
| `[DIAG]` | 故障计时器启动后首次心跳 | RSRP/RSRQ、IP（AT+CGPADDR）、温度、AT+CEER、AT+CGACT?（每次故障周期仅打一次） |
| `[RECOVERY Ln]` | 触发 L1/L2/L3 恢复前 | 错误原因（AT+CEER）+ PDP 激活状态（AT+CGACT?） |

---

### 2.6 拨号状态文件系统（/tmp/dial_status）

新增 `status/dial_status.h` / `status/dial_status.c`，每次心跳（30s）将完整运行状态原子写入 `/tmp/dial_status`（先写 `.tmp` 再 `rename`，防止读写撕裂）。

示例输出（`cat /tmp/dial_status`）：

```ini
[dial]
version=1.31.0
uptime=1234
state=roamlink_active
policy=1
policy_name=PREFER_ROAMLINK
channel=ROAMLINK
roamlink_available=1
license_pending=0

[sim]
iccid=89464283216100721879
csq=19
apn=internet.lte.cxn
profile_idx=1

[network]
status=1
type=2

[counters]
tcp_fail_count=0
roamlink_fail_count=0
roamlink_rx_packets=65

[roamlink]
state=active
biz_no_data_sec=0
rbmaster_pid=1505

[stats]
outage_count=0
last_outage_sec=0
total_outage_sec=0
current_outage_sec=0
```

---

## 三、Bug 修复说明

### 3.1 【已验证】EG25 每隔 120s 触发周期重拨

**问题根因**：`dial_stat_net_connected` 状态下，`nw_get_if_statistics_rx_packets()` 依赖 `p_dial_mng->p_ifaddrs->ifa_name` 读取接口流量，但 `nw_get_ifaddrs()` 返回的链表在函数内部已通过 `freeifaddrs()` 释放，形成**悬垂指针**，导致 rx_packets 始终读到 0，被误判为业务层断流，每 120s 触发一次不必要的重拨。

**修复方案**：改用 `nw_get_rmnet_rx_packets_sum()` 直接遍历 `/sys/devices/virtual/net/rmnet_data*` 接口目录读取 rx_packets，不依赖外部链表，彻底消除悬垂指针风险。

**涉及文件**：`eg25/nw/nw.c`、`eg25/nw/nw.h`

---

### 3.2 【已验证】EG25 roamlink_start_service 无条件发 AT+COPS=0

**问题描述**：每次调用 `roamlink_start_service()`（共 7 处调用点，包括切换和回切）都无条件发送 `AT+COPS=0`。若调制解调器已处于自动选网模式（mode=0），该命令会触发不必要的运营商重选，导致 `rmnet_data*` 接口短暂断开，MQTT TCP 连接随之断线（rtms_client 重连延迟 60-600s），断线期间 qos=0 的下发命令全部丢失。

**现场表现**：安装旧版 Roamlink 部署包后，平台下发的锁机/解锁命令延迟 3-5 分钟才能收到反馈。

**修复方案**：在发 `AT+COPS=0` 前先查询当前模式（`AT+COPS?`），若返回 `+COPS: 0`（已是自动模式）则跳过，仅在手动锁定运营商时才发送。查询失败（超时/错误）时安全降级为继续发送，行为与修改前一致。

**实测验证**：
```
[ROAMLINK] start_service: AT+COPS already auto, skip AT+COPS=0
[ROAMLINK] Ping OK in starting state, channel now active  (~57s 建链)
[HEARTBEAT] CH:ROAMLINK | RX_PKT 持续增长，DownTime:0s，RL_FAIL:0
```

**涉及文件**：`roamlink/roamlink.c`

---

### 3.3 【已验证】EG25 L1/L2/L3 分级恢复逻辑缺陷

**问题 1**：L2/L3 判断条件与 L1 并列（`if/else if`），导致在 L2 冷却期内仍可退化回 L1 继续重拨，L2 冷却节流逻辑形同虚设。

**问题 2**：L2 执行后未同步清零 `last_l1_ts`，下次 L1 节流计时起点计算错误。

**问题 3**：L1 在 REG 未就绪时仍尝试重拨（概率性无效），且无日志输出。

**问题 4**：`[DIAG]` / `[RECOVERY Ln]` 中 `cgact` 缓冲区为 128 字节，多 PDP 上下文激活状态下输出可能被截断。

**修复方案**：
- L2/L3 分支改为严格 `elif`：进入 L2 后不再退化执行 L1，等待 L2 冷却（5min）再重试
- L2 执行后同时清零 `last_l1_ts` 和 `last_recovery_ts`
- L1 执行前检查注册状态（`nw_reg_status_check()`），REG 未就绪时跳过并记录 `[RECOVERY L1] REG down, skip redial` 日志
- `cgact` 缓冲区从 128 字节扩容至 256 字节（诊断快照和恢复前快照两处均已修复）

**涉及文件**：`eg25/dial/dial.c`

---

### 3.4 【已验证】EG25 nanomsg handler 查询 ICCID/IMSI 返回空

**问题根因**：原实现在 nanomsg handler 初始化时调用 `QL_MCM_SIM_Client_Init` 二次初始化来获取 ICCID/IMSI，但该 Client 句柄生命周期与 dial_task 线程中的 SIM 客户端不同，导致 handler 端读取的 SIM 信息为空。

**修复方案**：废弃二次初始化方式，改为"写入-读取"分离：
- `dial_task` 在 `sim_op` 成功后调用 `set_eg25_iccid()` / `set_eg25_imsi()`，直接写入 handler 的成员变量
- `getIccid()` / `getImsi()` 直接返回已写入的值，不再执行任何 MCM 操作
- `dialer_eg25.c` 新增 `p_dial_mng->nano_handler = handler`，将 handler 指针传入 `dial_task` 使其可访问

**涉及文件**：`dialer_eg25.c`、`eg25/dial/dial.c`、`eg25/dial/dial.h`、`nanomsg_process_cinterface.h`、`nanomsg_process_wraper.cpp`、`nanomsg_process.cpp`

---

### 3.5 EG25 其他小缺陷修复

| 问题 | 根因 | 修复 |
|------|------|------|
| 启动后立即触发一次心跳 | `last_heartbeat` 初始化为 `0`，首次进入心跳块时 `now - 0` 必然 ≥ 30，无条件立即打印 | 改为 `time(NULL)` 初始化，对齐首次心跳触发时间 |
| `/tmp/dial_status` 中 `version` 字段始终输出 `"1.30"` | `dstat.version` 写死为字符串 `"1.30"`，未随版本宏同步 | 改用 `MODEM_MNG_VERSION_MAIN.SUB.PATCH` 宏动态生成；同步修正 `dialer_eg25.c` 的启动日志和 CPActive 注册版本字段 |
| `nw_get_ifaddrs()` 异常退出路径返回垃圾指针 | 函数内局部变量 `p_ifaddrs` 未初始化，异常分支直接 return，上层拿到随机地址 | 初始化为 `NULL`，确保异常路径返回空指针 |

**涉及文件**：`eg25/dial/dial.c`、`dialer_eg25.c`、`eg25/nw/nw.c`

---

### 3.6 EG25 注册超时保护

**问题描述**：物理 SIM 被运营商禁卡时，`nw_reg_status_check()` / `nw_at_get_cereg_stat()` 会无限等待注册，程序卡死在 `reg_check` / `cereg_check` 状态，永远无法切换到 Roamlink 通道。

**修复方案**：新增 `REG_CHECK_TIMEOUT_SECONDS = 300s` 超时保护，`reg_check` 和 `cereg_check` 共享同一计时器（`dial_timer`），超时后进入 `reg_timeout_handler`：

| 情况 | 处理 |
|------|------|
| 当前已在 Roamlink 通道 | SIM 注册失败属正常，仅重置计时器 |
| 策略 1/2 + roamlink 可用 + 非 license_pending | 停止 SIM data call → 启动 Roamlink |
| 策略 4 / roamlink 不可用 / license_pending | AT+CFUN=0 → AT+CFUN=1 重置射频 |

**涉及文件**：`eg25/dial/dial.h`、`eg25/dial/dial.c`

---

## 四、新增/修改文件清单

### 新增文件

| 文件 | 说明 |
|------|------|
| `roamlink/roamlink.h` | 双平台共用头文件：API 声明、四种策略常量、超时常量、路径宏 |
| `roamlink/roamlink.c` | 双平台共用实现：probe、license 备份恢复、master 进程管理、send_cmd、start/stop_service |
| `status/dial_status.h` | 拨号状态结构体定义 |
| `status/dial_status.c` | 拨号状态原子写入实现（先写 .tmp 再 rename） |

### 修改文件

| 文件 | 主要变动 |
|------|---------|
| `eg25/dial/dial.h` | `dial_stat_enu` 新增枚举值 17/18；`dial_mng_t` 新增 16 个 Roamlink/License/统计字段；新增 `REG_CHECK_TIMEOUT_SECONDS` 常量 |
| `eg25/dial/dial.c` | 集成双卡切换状态机；注册超时处理；license_pending 轮询；Roamlink 状态机枚举化重构；rx_packets 业务层监控；启动/心跳诊断日志体系；修复 rx_packets 悬垂指针；修复 ICCID/IMSI 写入路径 |
| `eg25/nw/nw.h` | 新增 `nw_get_rmnet_rx_packets_sum()` 声明 |
| `eg25/nw/nw.c` | 实现 `nw_get_rmnet_rx_packets_sum()`；修复 `nw_get_ifaddrs()` p_ifaddrs 未初始化 |
| `roamlink/roamlink.c` | roamlink_start_service 增加 AT+COPS? 预检查 |
| `dialer_eg25.c` | 新增 `p_dial_mng->nano_handler = handler`，将 nanomsg handler 指针传入 `dial_task` |
| `nanomsg_process_cinterface.h` | 新增 `set_eg25_iccid()` / `set_eg25_imsi()` 接口声明 |
| `nanomsg_process_wraper.cpp` | 实现 `set_eg25_iccid()` / `set_eg25_imsi()`，直接写入 handler 成员变量 |
| `nanomsg_process.cpp` | 补充 ICCID/IMSI 相关缺失实现 |
| `dialer_ec200a.cpp` | 集成 Roamlink 切换状态机；SDK 崩溃回调；SIM 状态异步回调；诊断函数；修复 pclose/fscanf 等缺陷 |
| `status/dial_status.c` | `dial_eg25_stat_name()` 新增 case 17→`"roamlink_starting"`，case 18→`"roamlink_active"` |
| `CMakeLists.txt` | 两平台指向顶层 `roamlink/`；EG25G 新增 `opt_iniparser`、`status` 编译路径 |

### 新增运行时文件

| 路径 | 说明 |
|------|------|
| `/tmp/dial_status` | INI 格式完整运行快照，每 30s 原子更新 |
| `/tmp/network_type` | 当前通道标记：`1`=物理SIM，`2`=Roamlink |
| `/usrdata/network.ini` | 策略配置（`network_select=1~4`），需预置 |
| `/data/ufs/license.cer` | Roamlink license 本地持久化备份（首次联网下载后自动创建） |

---

## 五、关键超时常量汇总

所有切换相关超时均集中定义在 `roamlink/roamlink.h`，两平台共用：

| 常量 | 值 | 含义 |
|------|----|------|
| `ROAMLINK_SWITCH_TIMEOUT_SEC` | 180s | 物理 SIM 失联多久后主动切换到 Roamlink |
| `ROAMLINK_CONNECT_TIMEOUT_SEC` | 300s | 发出 start 后等待首次 ping 通的最长时间 |
| `ROAMLINK_FAIL_TIMEOUT_SEC` | 300s | ping 通后再次失联超时，切回物理 SIM |
| `SIM_FALLBACK_RETRY_SEC` | 300s | 策略 1：物理 SIM 备用稳定后回切 Roamlink 的等待时间 |
| `ROAMLINK_FALLBACK_RETRY_SEC` | 300s | 策略 2：Roamlink 备用稳定后回切物理 SIM 的等待时间 |
| `ROAMLINK_NO_DATA_TIMEOUT_SEC` | 120s | 业务层：rmnet_data* rx_packets 无增长的切换阈值 |
| `LICENSE_WAIT_TIMEOUT_SEC` | 300s | license 下载等待总超时 |
| `LICENSE_CHECK_INTERVAL_SEC` | 60s | license 文件轮询间隔 |
| `PING_FAIL_THRESHOLD` | 3次 | 连续 ping 失败多少次才认定为真实断网 |
| `REG_CHECK_TIMEOUT_SECONDS` | 300s | EG25 注册等待超时（策略切换或 CFUN 重置） |

---

## 六、测试要点

### 6.1 环境前置条件

| 条件 | 要求 |
|------|------|
| 设备平台 | EG25G（主测），EC200A（辅测） |
| RBMaster 包 | `/usrdata/roamlink/RBMaster` 可执行 |
| conf.ini | `/opt/conf.ini` 已预置（由 factoryApp 下发） |
| License | `/usrdata/roamlink/etc/.pconfig/license.cer` 已存在（或首次联网自动下载） |
| 策略文件 | `/usrdata/network.ini` 可手动编辑 `network_select` 值 |

### 6.2 核心功能测试用例

#### TC-01：默认策略（FORCE_SIM，策略 4）
- 不创建 `/usrdata/network.ini`，或设置 `network_select=4`
- 预期：设备走物理 SIM，断网不切换到 Roamlink，L1/L2/L3 分级恢复正常工作
- 验证：`cat /tmp/dial_status` → `channel=SIM`，`cat /tmp/network_type` → `1`

#### TC-02：优先 Roamlink（策略 1）
- 设置 `network_select=1`，RBMaster 包、conf.ini、license 均就绪
- 预期：启动时直接走 Roamlink 通道，`state=roamlink_active`，ping 正常
- 验证：`cat /tmp/network_type` → `2`，`cat /tmp/dial_status` → `channel=ROAMLINK`

#### TC-03：策略 1 物理 SIM 备用 + 自动回切
- `network_select=1`，设备处于 Roamlink 通道运行中
- 模拟 Roamlink 断联 > 300s（或停止 RBMaster）
- 预期：切回物理 SIM，记录 `sim_fallback_ts`；物理 SIM 稳定 300s 后自动切回 Roamlink
- 验证：日志输出 `[ROAMLINK] Policy1: SIM stable ... trying Roamlink recovery`

#### TC-04：优先物理 SIM（策略 2）
- `network_select=2`，物理 SIM 联网正常
- 模拟物理 SIM 断联 > 180s
- 预期：切换到 Roamlink 通道；Roamlink 稳定 300s 后自动切回物理 SIM
- 验证：日志输出 `[ROAMLINK] Switched to Roamlink channel` 和 `Switched back to SIM channel`

#### TC-05：强制 Roamlink（策略 3）
- `network_select=3`，RBMaster 就绪
- 模拟 Roamlink 断联 > 300s
- 预期：不切物理 SIM，重启 RBMaster 服务后重新等待建链
- 验证：日志无 `Switched back to SIM`，有 `starting RBMaster`

#### TC-06：RBMaster 未安装（probe NO_PACKAGE）
- 删除 `/usrdata/roamlink/RBMaster`，设置任意非 FORCE_SIM 策略
- 预期：probe 返回 NO_PACKAGE，永久降级为 FORCE_SIM，走物理 SIM
- 验证：日志 `[ROAMLINK] probe NO_PACKAGE: RBMaster missing, force SIM`

#### TC-07：conf.ini 缺失（probe CONF_MISSING）
- 保留 RBMaster，删除 `/opt/conf.ini`
- 预期：probe 返回 CONF_MISSING，降级为 FORCE_SIM
- 验证：日志 `[ROAMLINK] probe CONF_MISSING`

#### TC-08：license 缺失 + 本地备份存在
- 删除正式路径 license，保留 `/data/ufs/license.cer`（备份）
- 预期：从备份恢复后 re-probe 为 OK，正常启用 Roamlink
- 验证：日志 `[ROAMLINK] license restored from backup`

#### TC-09：license 缺失 + 无备份（license_pending 模式）
- 同时删除正式路径和备份路径的 license
- 预期：进入 license_pending，强制 SIM 联网；SIM 联网后启动 RBMaster 下载 license；300s 内若 license 出现则 reboot；超时则保持 FORCE_SIM
- 验证：日志 `[ROAMLINK] license_pending mode`，`rbmaster_started`

#### TC-10：ping 抖动不触发切换
- 策略 1/2，设备处于 SIM 通道
- 制造 1-2 次 ping 失败（如临时断网后恢复）
- 预期：不触发双卡切换，不启动故障计时器
- 验证：日志无 `fault timer started`，`ConsecFail` < 3

#### TC-11：nanomsg 查询 ICCID/IMSI
- 设备正常联网后，通过 nanomsg 查询 ICCID 和 IMSI
- 预期：返回正确的 ICCID/IMSI（非空）
- 验证：`nanomsg_req` 查询结果非空字符串

#### TC-12：rx_packets 业务层监控
- 设备处于 `roamlink_active` 状态，rx_packets 持续增长
- 模拟 rx_packets 停止增长 > 120s（如 RBMaster 注册失败）
- 预期：策略 1/2 切回物理 SIM；策略 3 重启 RBMaster
- 验证：日志 `[ROAMLINK] Biz rx_packets no growth`

#### TC-13：启动诊断日志完整性
- 正常启动设备
- 预期：日志依次出现 `[MODEM]`、`[INIT]`、`[CELL]`、`[SDK]` 四类标签，内容字段无空白
- 验证：grep 各标签并检查内容

#### TC-14：注册超时保护（EG25 专有）
- 物理 SIM 被禁卡或运营商拒绝注册（stat=3）
- 策略 1/2：预期 300s 后切换到 Roamlink
- 策略 4：预期 300s 后执行 CFUN 重置
- 验证：日志 `reg_timeout_handler`

#### TC-15：启动时清理残留 RBMaster
- 设置策略 4，但上次运行遗留了 RBMaster 进程
- 预期：启动时检测到 RBMaster 存活，自动停止，等待 2s 后继续
- 验证：日志 `stopping residual RBMaster service`

### 6.3 回归验证

| 项目 | 说明 |
|------|------|
| 物理 SIM 基础拨号 | 策略 4 / roamlink 不可用时，物理 SIM 拨号流程与旧版行为一致 |
| L1/L2/L3 分级恢复 | FORCE_SIM 策略下断网恢复逻辑与旧版对齐 |
| nanomsg 接口 | IMEI/IP/CSQ/APN 等查询字段返回正确 |
| /tmp/dial_Status | 旧格式兼容（与 1.30 对比） |

---

## 七、已知遗留问题

| 编号 | 描述 | 影响 | 计划 |
|------|------|------|------|
| P-01 | rtms_client MQTT 在 Roamlink 启动期间的连接稳定性未验证（需设备侧和 rtms_client 侧日志同步采集） | 无法完整证明 AT+COPS=0 修复对 MQTT 的效果 | 下个测试周期补充 |
| P-02 | stub ev 循环中 `req_mcu_info()` 使用阻塞 `nn_send`，与锁机命令处理同线程，存在结构性阻塞风险 | stub 侧问题，不在 modem_mng 范围 | stub 侧单独修复 |
| P-03 | EC200A 平台 L3 硬重启仍为注释状态（30min 断网只记录 FATAL，不触发重启） | 策略 4 + L3 场景断网无法自愈 | 待确认是否需要启用 |

---

## 八、实机验证截图（参考）

设备日志（策略 1，Roamlink 首次建链全流程）：

```
[1970-01-01 09:21:14] === Dial Program Started ===
[1970-01-01 09:21:17] EG25 modem_mng Version: 1.31.0
[1970-01-01 09:21:17] [ROAMLINK] network_select = 1
[1970-01-01 09:21:17] [ROAMLINK] probe OK (RBMaster + conf.ini + license valid)
[1970-01-01 09:21:17] [ROAMLINK] probe OK, roamlink available, policy=1
[1970-01-01 09:21:19] [ROAMLINK] Policy requires Roamlink as initial channel. Starting RBMaster...
[1970-01-01 09:21:22] [ROAMLINK] start_service: AT+COPS already auto, skip AT+COPS=0
[1970-01-01 09:21:22] [ROAMLINK] send_cmd: sent "RBstartServiceMaster" to 127.0.0.1:5568
[1970-01-01 09:21:22] [ROAMLINK] start_service: Roamlink service started, network_type=2
[1970-01-01 09:22:19] [ROAMLINK] Ping OK in starting state, channel now active
[1970-01-01 09:22:19] [HEARTBEAT] CH:ROAMLINK | SIM:1 | CSQ:16 | Temp:38 | DownTime:0s | ConsecFail:0 | RX_PKT:43
[1970-01-01 09:22:49] [HEARTBEAT] CH:ROAMLINK | SIM:1 | CSQ:16 | Temp:38 | DownTime:0s | ConsecFail:0 | RX_PKT:65
[1970-01-01 09:23:19] [HEARTBEAT] CH:ROAMLINK | SIM:1 | CSQ:16 | Temp:38 | DownTime:0s | ConsecFail:0 | RX_PKT:92
```

设备状态（`cat /tmp/dial_status`）：

```ini
[dial]
version=1.31.0
state=roamlink_active
policy=1
policy_name=PREFER_ROAMLINK
channel=ROAMLINK
roamlink_available=1

[network]
status=1
type=2

[counters]
tcp_fail_count=0
roamlink_fail_count=0
roamlink_rx_packets=65

[roamlink]
state=active
rbmaster_pid=1505

[stats]
outage_count=0
```
