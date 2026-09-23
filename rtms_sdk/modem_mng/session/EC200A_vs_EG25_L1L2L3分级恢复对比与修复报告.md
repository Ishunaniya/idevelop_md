# EC200A vs EG25 L1/L2/L3 分级恢复逐行比对与对齐修复报告

**日期**：2026-05-30
**分支**：`develop/rtms_sdk_v1.3_20240408_dc_switch`
**参考基线**：EG25 v1.31.x（已设备验证），EC200A（未设备验证）
**目的**：逐行比对两平台 L1/L2/L3 分级恢复的流程流转，确认 EC200A 是否存在 EG25 已修复的触发缺陷，并完成对齐修复

---

## 一、结论速览

1. **阈值与判级节奏差异属有意设计**，代码注释已说明（EC200A 有 SDK 25s 自动重连 L0 窗口，EG25 没有），不应"对齐"。
2. **EG25 在 `93fc716b` 修复了一个 L1/L2/L3 触发缺陷**：触发门控漏了 `network_policy == FORCE_SIM`，导致策略 1/2/3 的 SIM 通道也会触发分级恢复，与 Roamlink 切换抢动作。
3. **EC200A 功能上不会触发该缺陷**（靠三重隐式条件规避），但门控写法与 EG25 修复前一样松，存在维护隐患。
4. 本次已完成两项对齐修复：① `cgact` 诊断缓冲区 128→256；② L1/L2/L3 触发门控显式加 `network_policy == NET_POLICY_FORCE_SIM`。

---

## 二、总体差异（阈值 / 节奏 / 结构）

| 维度 | EG25（`eg25/dial/dial.c`） | EC200A（`dialer_ec200a.cpp`） | 性质 |
|---|---|---|---|
| L1 / L2 / L3 阈值 | 60s / 5min / 30min | 5min / 10min / 35min | 有意设计 |
| L0 期（SDK 自动重连） | 无 | 0~5min，SDK 每 25s 自动重连，应用层不介入 | 平台能力差异 |
| 判级频率 | 30s（心跳块内，`dial.c:978`） | ~1.5s（每个 ST_PING tick，`PING_INTERVAL_MS`） | 架构差异 |
| 执行结构 | 就地执行（心跳块内） | 独立 `ST_RECOVERY` 状态（`2264`） | 架构差异 |
| 触发门控（修复前） | 仅 `dial_st != roamlink_*` | `!is_roamlink_active && enable_policy_recovery`（`2227`） | 见第四节 |

阈值差异的设计依据（`dial.c:1174-1176` 注释）：EC200A 有 SDK 25s 自动重连 L0 窗口（0~5min），应用层等 SDK 先处理；EG25 无此机制，需更早介入（L1=60s）。

---

## 三、判级与执行流程逐项比对

### 3.1 判级逻辑

| 判级分支 | EG25 | EC200A | 判定 |
|---|---|---|---|
| 纯 downtime 升级（L3>L2>L1 严格 elif，不退化） | `dial.c:1191-1197` | `2233-2246` | ✅ 一致 |
| REG=0 时 L2 加速（fail>L1 即升 L2，冷却缩到 90s） | 无 | `2235-2242` | ⚠️ EC200A 独有 |
| SIM 恢复立即 L1 | 无条件立即 L1（`1186-1189`） | 仅 `fail_dur≥5min`（过 L0）才立即 L1（`2014-2024`） | ⚠️ 条件不同（各配合 L0） |
| L1 的 REG 检查 | 执行期检查，REG down 则 skip 软重拨（`1212-1215`） | 判级期要求 `REG!=0`，REG=0 直接走 L2 加速（`2243`） | ⚠️ 位置+激进度不同 |

### 3.2 执行动作

| 级别 | EG25 | EC200A |
|---|---|---|
| L1 软重拨 | stop→sleep2→start（`1218-1221`） | stop→sleep2→start（`2278-2288`） |
| L2 射频重置 | CFUN=0→sleep2→CFUN=1→sleep10→start；`last_recovery_ts=last_l1_ts=now`（`1224-1236`） | CFUN=0→sleep3→CFUN=1→sleep10→start；`last_l2_ts=last_l1_ts=tnow`；额外处理 `-1013 已连接`（`2293-2310`） |
| L3 硬重启 | sync→CFUN=1,1→sleep20→exit(1)（`1238-1246`） | sync→CFUN=1,1→sleep20→exit(1)（`2314-2320`） |
| 执行后回归 | 就地继续循环 | `has_notified_connect=0`→回 `ST_STATUS`（`2324-2325`） |

L2 中 EC200A 多出的 `-1013（已连接）` 分支，是配合 SDK 在射频重置期间可能已自动重连的合理处理，EG25 无 SDK 不需要。

### 3.3 提测记录 3.3 节 EG25 修复项在 EC200A 的对齐情况

| 提测 3.3 修复项 | EC200A 状态（修复前） | 本次处理 |
|---|---|---|
| L2 不退化回 L1（严格 elif） | ✅ 已对齐（`2235`） | — |
| L2 执行清零 `last_l1_ts` | ✅ 已对齐（`2294`） | — |
| L1 REG 未就绪不做无效重拨 | ✅ 等效（判级期 REG=0 走 L2） | — |
| `cgact` 缓冲区 128→256 防截断 | ❌ 未对齐（`2170` / `2270` 仍 128） | ✅ 本次已补 |

---

## 四、核心问题：EG25 `93fc716b` 的 L1/L2/L3 触发缺陷

### 4.1 EG25 的缺陷与修复

`93fc716b`（EG25 系统测试问题修复）的关键改动：

```diff
 if (!net_ok && start_fail_ts > 0 && has_connected_once &&
     p_dial_mng->dial_st != dial_stat_roamlink_starting &&
-    p_dial_mng->dial_st != dial_stat_roamlink_active) {
+    p_dial_mng->dial_st != dial_stat_roamlink_active &&
+    p_dial_mng->network_policy == NET_POLICY_FORCE_SIM) {
```

**缺陷**：修复前触发门控没有策略限制，策略 1/2 在 SIM 通道断网时，L1/L2/L3 与 SIM→Roamlink 切换同时生效。EG25 L1=60s 早于切换的 180s，会**先误触发 L1 软重拨**（`dail_stop/start_data_call`），与随后的切 Roamlink 抢动作。
**修复**：门控加 `network_policy == NET_POLICY_FORCE_SIM`，仅策略 4 才走 L1/L2/L3。

### 4.2 EC200A 是否存在同一缺陷

EC200A 触发门控（修复前，`2227`）同样**没有** `FORCE_SIM` 限制：

```c
if (!is_roamlink_active && enable_policy_recovery == 1 && has_connected_once) {
```

但**功能上不会触发该缺陷**，靠三重隐式条件规避：

1. **切换分支 `break` 跳过判级**：策略 1/2 在 SIM 通道 `fail_duration≥180s` 进入 SIM→Roamlink 切换分支，其末尾 `stage=ST_STATUS; break;`（`2217-2218`）直接跳出，走不到 L1/L2/L3 判级（`2227`）。
2. **阈值关系 L1(300s) > 切换(180s)**：未进切换分支时，EC200A L1=5min，180s 时已先切走；EG25 的坑恰恰是 L1=60s **早于**切换 180s。
3. **roamlink 不可用必降级策略 4**：probe 失败时 `network_policy` 强制改为 `FORCE_SIM`（`1591-1626`），不存在"策略 1/2 但 roamlink 不可用"；策略 3 启动即 `is_roamlink_active=true`，门控 `!is_roamlink_active` 失效。

逐策略验证：

| 策略 | 修复前是否会误触发 | 原因 |
|---|---|---|
| 4（FORCE_SIM） | 正常触发（符合预期） | — |
| 1 / 2 | 不会 | 180s 先切 Roamlink（break 跳过）；切换失败则 `start_fail_ts` 重置（`2214`），到不了 L1(300s) |
| 3（FORCE_ROAMLINK） | 不会 | `is_roamlink_active` 恒 true，门控失效 |

### 4.3 隐患

EC200A 是**靠隐式条件规避**，非显式门控。若调整 `ROAMLINK_SWITCH_TIMEOUT`（>300s）、删除切换分支的 `break`、或把 L1 阈值改到 <180s，立刻复现 EG25 当初的缺陷。EG25 已用一行策略门控彻底消除该风险。

---

## 五、本次对齐修复（EC200A）

| # | 项 | 位置 | 改动 |
|---|---|---|---|
| 1 | cgact 防截断（DIAG 快照） | `dialer_ec200a.cpp:2170` | `d_cgact[128]` → `[256]` |
| 2 | cgact 防截断（RECOVERY 快照） | `dialer_ec200a.cpp:2270` | `cgact_buf[128]` → `[256]` |
| 3 | L1/L2/L3 门控（SIM 恢复立即 L1） | `dialer_ec200a.cpp:2014` | 增加 `&& network_policy == NET_POLICY_FORCE_SIM` |
| 4 | L1/L2/L3 门控（主判级块） | `dialer_ec200a.cpp:2227` | 增加 `&& network_policy == NET_POLICY_FORCE_SIM` |

**说明**：
- `recovery_level` 的全部 4 个赋值点（`2020` SIM恢复L1、`2234` L3、`2242` L2、`2245` L1）均落在 `2014` / `2227` 这两个块内，门控覆盖完整。
- 保留原有 `enable_policy_recovery` 总开关，仅叠加策略门控，不破坏其语义。
- 门控改动**不改变任何现有正确行为**（策略 1/2/3 本就走不到 L1/L2/L3），仅把"碰巧不触发"变为"显式保证不触发"。
- `network_policy` / `NET_POLICY_FORCE_SIM` 在 `dial_loop` 作用域内均可用（`1633` 已在用同样判定），不引入新依赖。

**未对齐项（按设计保留，不动）**：阈值（60s/5min/30min vs 5min/10min/35min）、L0 期、SIM 恢复立即 L1 的触发条件、REG=0 时 L2 加速、判级节奏、执行结构——均为两平台架构/SDK 能力差异下的合理设计。

---

## 六、验证建议

EC200A 无设备验证环境，本次改动建议后续在设备上回归：

| 用例 | 场景 | 预期 |
|---|---|---|
| TC-01 | 策略 4，断网 5/10/35min | 依次触发 L1/L2/L3，`[RECOVERY Ln]` 日志正常，cgact 不截断 |
| TC-04 | 策略 1/2，SIM 断网 >180s | 切 Roamlink，**不出现** `[RECOVERY Ln]` 日志 |
| —— | 策略 3 | 全程 Roamlink，**不出现** `[RECOVERY Ln]` 日志 |

---

## 七、核对方法（可复现）

```bash
# 找到 EG25 的 L1/L2/L3 触发门控修复
git show 93fc716b -- eg25/dial/dial.c | grep -A2 -B2 "NET_POLICY_FORCE_SIM"

# 两平台 cgact 缓冲区大小
grep -n "cgact\[" eg25/dial/dial.c          # 738 / 1202 均为 256
grep -n "d_cgact\|cgact_buf" dialer_ec200a.cpp

# EC200A L1/L2/L3 触发门控与 recovery_level 赋值点
grep -n "enable_policy_recovery\|recovery_level = [123]" dialer_ec200a.cpp
```
