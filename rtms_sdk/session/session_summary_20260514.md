# modem_mng 开发会话总结

**日期：2026-05-14**  
**分支：develop/rtms_sdk_v1.3_20240408**  
**涉及平台：EC200A、EG25**  
**参考上游：`open_dial_for_artery/src/dial/dial.c`**

---

## 背景

`modem_mng` 是一个 4G/LTE 网络拨号守护进程，支持三个 Quectel 模组平台（EC200A、EG25、IMX6ULL），通过编译宏选择。本次会话在两个平台上完成了三项独立任务：指示灯逻辑补全、EG25 分级恢复启用与修复、以及 roamlink 移植质量审查。

---

## 一、指示灯（LED）逻辑分析与修复

### 修复前的状态

**EC200A**（`dialer_ec200a.cpp`）：
- 有完整的 `currentLEDState` 状态机（-1=初始/2=亮/0=灭）
- ping 成功 `currentLEDState = 2`，连续失败 `currentLEDState = 0`
- 但 roamlink 通道切换时**缺少过渡闪灯**

**EG25**（`eg25/dial/dial.c`）：
- 声明了 `lastLEDState` 变量但**完全未使用**
- 断网不灭灯、联网不亮灯、切换不闪灯——整个 LED 逻辑缺失

### 修复内容

**EG25 新增 8 处控灯：**

| 位置 | 触发条件 | 动作 |
|---|---|---|
| 心跳 `net_ok = true` | ping 成功 | `LEDControl_controlLight(..., 1)` 亮灯 |
| `dial_stat_net_connected` 掉线 | SDK 回调断网 | `LEDControl_controlLight(..., 0)` 灭灯 |
| 心跳 ping 失败超阈值 | `ping_fail_count >= PING_FAIL_THRESHOLD` | `LEDControl_controlLight(..., 0)` 灭灯 |
| Policy1 SIM→Roamlink 回切 | 稳定 300s 触发 | `LEDControl_blinkLight(...)` 闪灯 |
| Policy2 Roamlink→SIM 回切 | 稳定 300s 触发 | `LEDControl_blinkLight(...)` 闪灯 |
| Roamlink 通道超时切 SIM | `roamlink_connect/fail_timeout` | `LEDControl_blinkLight(...)` 闪灯 |
| FORCE_ROAMLINK 重启服务 | 超时重启 | `LEDControl_blinkLight(...)` 闪灯 |
| SIM 失败 3min 切 Roamlink | `downtime >= ROAMLINK_SWITCH_TIMEOUT` | `LEDControl_blinkLight(...)` 闪灯 |

**EC200A 新增 5 处过渡闪灯：**

| 切换场景 | 代码位置 |
|---|---|
| Roamlink 通道超时 → 切 SIM（策略1/2） | 守护块 `roamlink_connect/fail_timeout` |
| FORCE_ROAMLINK 服务重启 | 守护块 else 分支 |
| Policy1 SIM 稳定 → 回切 Roamlink | ping 成功回切逻辑 |
| Policy2 Roamlink 稳定 → 回切 SIM | ping 成功回切逻辑 |
| SIM 失败 3min → 切 Roamlink | ping 失败切换逻辑 |

---

## 二、EG25 分级恢复（L1/L2/L3）启用与修复

### 背景

EG25 的三级恢复代码被 `#if 0` 整体禁用。在启用前先审查逻辑，发现 4 个问题。

### 阈值设计

两平台阈值不同，原因是 EC200A 有 **SDK L0 自动重连机制**（每 25s 自动尝试，持续约 5min），EG25 无此机制，需要应用层更早介入。

| 级别 | 动作 | EG25 触发阈值 | EC200A 触发阈值 |
|---|---|---|---|
| L1 | 软重拨（stop + start） | 60s | 5min（L0 窗口之后）|
| L2 | RF 重置（CFUN=0→1） | 5min | 10min |
| L3 | 硬重启（CFUN=1,1 + exit） | 30min | 35min |

### EC200A 的 L2 为何不用 `restart_cfun_safe()`

`restart_cfun_safe()` 内置 `MIN_CFUN_INTERVAL = 600s`（10min）最小间隔限制，与 L2 的 5min 冷却冲突——会静默跳过 L2。因此 L2 直接调用 `AT+CFUN=0 / AT+CFUN=1`，代价是失去 `MAX_CFUN_CALLS = 10` 次数上限保护，但通过 L2 冷却时间（5min）隐式限制了频率。

### 修复的 4 个问题

#### 问题1（高优先级）：无首次连接保护，启动期 L1 干扰初始 PDP 建立

**场景**：EC200A 持久模式（retry_count ≥ 3，fast-fail 已关闭）下，从启动就 ping 失败，5min 后 L1 触发 `ql_data_call_stop + start`，打断正在建立的 PDP。

**修复（EC200A）**：添加 `has_connected_once` 变量，首次 ping 成功后置 1，L1/L2/L3 入口加守卫：

```cpp
// 声明
int has_connected_once = 0;

// ping 成功路径
has_connected_once = 1;  // 解锁 L1/L2/L3

// L1/L2/L3 入口
if (!is_roamlink_active && enable_policy_recovery == 1 && has_connected_once) {
```

**EG25 同步修复**（上一会话已完成，本次验证对齐）：
- 声明 `has_connected_once = 0`（line 485）
- 心跳 ping 成功置 1（line 632）
- 启动已联网路径置 1（line 523）
- L1/L2/L3 入口守卫（line 803）

**快速失败模式（retry < 3）不受影响**：fast-fail 在 PDP 建立后 60s 超时退出，L1 需 5min，两者不重叠。

#### 问题2（低优先级）：阈值与 EC200A 不一致

EG25（60s/5min/30min）比 EC200A（5min/10min/35min）更激进。  
**处理**：代码中加注释说明差异原因（SDK L0 机制），不修改阈值。

#### 问题3（低优先级）：L2 无次数上限

L2 最多能触发次数：`30min / 5min = 6` 次。  
**处理**：可接受，不修复。

#### 问题4（低优先级）：`last_recovery_ts` 网络恢复时未清零

**问题**：断网 → L2 触发（记录 `last_recovery_ts = T`）→ 网络恢复（`last_recovery_ts` 未清零）→ 再次断网，新故障距 T 仅 3.7min 就可能触发 L2（比 5min 早）。

**修复**：ping 成功路径清零 `last_recovery_ts`，与 EC200A 的 `last_l2_ts = 0` 对齐：

```c
start_fail_ts    = 0;
last_l1_ts       = 0;
last_recovery_ts = 0;   // 新增：清除 L2 节流
sim_error_active = 0;
```

---

## 三、Roamlink 移植质量审查

### 架构概述

Roamlink 是一种虚拟 SIM 卡服务（RBMaster 进程），通过云端虚拟号段替代物理 SIM。系统支持 4 种策略：

| 策略值 | 名称 | 行为 |
|---|---|---|
| 1 | `PREFER_ROAMLINK` | 优先 Roamlink，失败切 SIM，SIM 稳定 300s 切回 |
| 2 | `PREFER_SIM` | 优先 SIM，失败切 Roamlink，Roamlink 稳定 300s 切回 |
| 3 | `FORCE_ROAMLINK` | 强制 Roamlink，超时只重启服务，不切 SIM |
| 4 | `FORCE_SIM` | 强制物理 SIM，启用 L1/L2/L3，不切换 |

### Probe 四种情况处理

启动时调用 `roamlink_probe()` 判断 Roamlink 可用性：

```
PROBE_OK           → roamlink_available = true，按 policy 启动
PROBE_NO_PACKAGE   → FORCE_SIM（RBMaster 未安装）
PROBE_CONF_MISSING → FORCE_SIM（/opt/conf.ini 缺失，等 factoryApp 预置）
PROBE_LICENSE_MISSING:
  ├─ backup 恢复成功 → re-probe → OK: 正常使用 / 失败: FORCE_SIM
  └─ backup 失败 → license_pending 模式（先走 SIM，等 RBMaster 下载 license）
```

### Roamlink 通道状态机（EC200A 与 EG25 一致）

```
启动 (PREFER/FORCE_ROAMLINK)
  └─ roamlink_start_ts = now, is_roamlink_active = true
        │
        ├─ 300s 内 ping 通 → 正常监控
        │     ├─ ping 断 300s → 切 SIM (策略1/2) 或 重启 (策略3)
        │     └─ 策略2 稳定 300s → 回切 SIM
        │
        └─ 300s 内从未 ping 通 → 同上

SIM 通道
  └─ SIM 失败 3min → 切 Roamlink (策略1/2)
        └─ 策略1 SIM 稳定 300s → 回切 Roamlink
```

### License Pending 模式

```
进入条件：PROBE_LICENSE_MISSING 且 backup 恢复失败
工作流程：
  1. FORCE_SIM 让物理 SIM 联网
  2. 首次 SIM ping 通后，启动 RBMaster（让其利用 SIM 下载 license）
  3. 每 60s 轮询 license 文件
  4. 出现 → backup + reboot
  5. 超过 300s → 放弃，保持 FORCE_SIM 继续运行
```

### 对比审查结论（8 项）

| # | 审查项 | EC200A | EG25 | 结论 |
|---|---|---|---|---|
| 1 | `roamlink_last_ping_ts` 守卫 | ✅ 有 `is_roamlink_active` 条件 | ✅ | 已正确 |
| 2 | 通道切换后 `ping_fail_count = 0` | ✅ 所有主要切换点均有 | ✅ | 已正确 |
| 3 | license 备份 re-probe | ✅ lines 1590-1602 | ✅ | 已正确 |
| 4 | `license_check_ts` 初始化 | ✅ 声明时 `= 0` | ✅ | 已正确 |
| 5 | FORCE_ROAMLINK 完整性 | 启动时始终 `is_roamlink_active=true` | 失败时保持 `false` | 两种设计等效 |
| **6** | **`has_connected_once` 守卫** | **❌ 缺失** | **✅** | **已修复** |
| 7 | `roamlink_starting` 专用状态 | ping 替代（1.5s 粒度） | ping 替代（30s 粒度） | 功能等效 |
| 8 | `rx_packets` 业务层监控 | ❌ 未实现 | ❌ 未实现 | 暂不修复 |

### 问题5 设计差异详解

**EG25 启动策略**：`roamlink_start_service()` 失败时 `is_roamlink_active` 保持 `false`，SIM→Roamlink 切换条件包含 `FORCE_ROAMLINK`，作为安全网。

**EC200A 启动策略**：无论是否成功，始终 `is_roamlink_active = true`，利用守护块（`roamlink_connect_timeout`）负责后续处理：

```cpp
/* 无论启动成功或失败，均标记 active + 记录时间戳，
 * 让守护块负责后续超时重试（FORCE_ROAMLINK）或切回SIM（PREFER_ROAMLINK）*/
is_roamlink_active = true;
roamlink_start_ts  = tnow;
```

两种设计均正确，行为等效：EC200A 的方式更早进入守护逻辑，EG25 的方式保留了更明确的状态语义。

### 问题7：`roamlink_starting` 等待状态

参考实现有独立状态 + TCP 测试（到 18.196.0.17:22）每 10s 检测。  
两平台用常规 ping 替代：

| 实现 | 检测方式 | 检测间隔 | 超时 |
|---|---|---|---|
| 参考 | TCP 测试 | 10s | `ROAMLINK_CONNECT_WAIT_SEC` |
| EC200A | ICMP ping 8.8.8.8 | 1.5s | 300s |
| EG25 | ICMP ping 8.8.8.8 | 30s（心跳） | 300s |

EC200A 粒度（1.5s）优于参考实现（10s），EG25（30s）略粗但不影响正确性。**结论：功能等效，不修复。**

### 问题8：`rx_packets` 业务层监控

**检测的故障模式**：  
虚拟 SIM 注册失败时，Roamlink 隧道层可达（TCP 测试通过），但云端拒绝 SIM 业务，`rmnet_data*` 接口无实际流量，`rx_packets` 不增长。

**两平台现状**：`dial_status` 中 `roamlink_rx_packets` 字段写死为 0，未实现此检测。

**为何 ping 基本能覆盖**：  
虚拟 SIM 注册失败通常导致无有效 IP 或无默认路由，ping 8.8.8.8 也会失败，最终触发 `roamlink_connect_timeout`（300s），执行与 TCP 失败相同的切换或重启逻辑。

**唯一漏洞**：Roamlink 隧道路由允许 ping 通（测试地址可达）但选择性阻断通用业务流量。是否存在此场景取决于 RBMaster 的具体路由实现。

**实现成本**：需要接口名枚举（`rmnet_data*`）、计数读取（`/sys/class/net/`）、基准管理、无数据超时计时器，涉及 6~8 处改动。

**结论：若现场出现 Roamlink 显示连通但业务不通的现象，再实现；当前优先级低。**

---

## 四、关键时序参数汇总

### EC200A 主循环轮询间隔

| 阶段 | 间隔 |
|---|---|
| 主循环 tick | 50ms |
| SIM 状态检查（ST_SIM） | 800ms |
| 信号强度（ST_SIGNAL） | 800ms |
| Ping 检测（ST_PING） | 1500ms |
| 心跳日志 | 30s |
| 5min 扩展心跳 | 300s |

### Roamlink 超时阈值（两平台一致）

| 参数 | 值 | 含义 |
|---|---|---|
| `ROAMLINK_CONNECT_TIMEOUT` | 300s | 启动后从未 ping 通的超时 |
| `ROAMLINK_FAIL_TIMEOUT` | 300s | ping 通后再次断线的超时 |
| `ROAMLINK_SWITCH_TIMEOUT` | 180s | SIM 失败多久后切 Roamlink |
| `SIM_FALLBACK_RETRY` | 300s | 策略1：SIM 稳定多久后回切 |
| `ROAMLINK_FALLBACK_RETRY` | 300s | 策略2：Roamlink 稳定多久后回切 |
| `LICENSE_WAIT_TIMEOUT` | 300s | license 等待总超时 |
| `LICENSE_CHECK_INTERVAL` | 60s | license 文件轮询间隔 |

### EC200A Fast-Fail 机制

```
retry_count(/tmp/dial_retry_count):
  < 3  → 快速失败模式：PDP 建立后 60s 内无 ping 成功 → exit(1)
  ≥ 3  → 持久模式：禁用快速失败，L1/L2/L3 负责恢复
  
首次 ping 成功时：clear_retry_count()，is_fast_fail_mode = 0
```

---

## 五、修改文件汇总

| 文件 | 改动类型 | 主要内容 |
|---|---|---|
| `dialer_ec200a.cpp` | 新增功能 + bugfix | roamlink 完整移植；LED 切换闪灯（5处）；`has_connected_once` 守卫 |
| `eg25/dial/dial.c` | 新增功能 + bugfix | LED 完整控灯逻辑（8处）；L1/L2/L3 启用；`has_connected_once`；`last_recovery_ts` 清零；roamlink 通道守护 |
| `eg25/dial/dial.h` | 声明补充 | roamlink 相关结构体字段 |
| `ec200a/nw/nw.c` | bugfix | `strcat` 指针算术 bug 修复；`get_signal_strength` 改用 `fopen/fprintf/fclose` 替换 `system()` |
| `ec200a/test_utils/test_utils.h` | 重构 | 移除 `#ifdef __cplusplus` 守卫（该头文件仅被 C++ 编译单元包含） |
| `fault_report/SimSignalMonitor.cpp` | 联动改动 | 适配 EC200A SIM 回调机制调整 |
| `ec200a/apn/apn.c`、`eg25/apn/apn.c` | 配置 | APN 相关调整 |

---

## 六、遗留事项

| 事项 | 优先级 | 说明 |
|---|---|---|
| EG25 `rx_packets` 监控 | 低 | 参见问题8分析，视现场情况决定 |
| EC200A `rx_packets` 监控 | 低 | 同上 |
| `restart_cfun_safe()` 次数上限缺失 | 低 | L2 绕过了 MAX_CFUN_CALLS=10 保护；L2 本身 5min 冷却做了频率限制，可接受 |
