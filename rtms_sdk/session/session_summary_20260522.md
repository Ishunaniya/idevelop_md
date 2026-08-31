# 会话总结

**日期**：2026-05-22  
**平台**：EG25G（`modem_mng` + `open_dial_for_artery` 对比）  
**分支**：`develop/rtms_sdk_v1.3_20240408_dc_switch`

---

## 一、测试背景

设备运行 `modem_mng` v1.31，策略 `network_select=1`（PREFER_ROAMLINK）。测试者执行：

```bash
iptables -I OUTPUT -p icmp -j DROP   # 模拟断网
```

观察到日志序列：

```
[HEARTBEAT] Ping failed 3 consecutive times, fault timer started
[HEARTBEAT] CH:ROAMLINK | ... | DownTime:30s | ConsecFail:4 | RX_PKT:844
[ROAMLINK] Biz layer no data for 131s, switching to SIM            # BIZ 层超时先触发
[RECOVERY L1] REG down, skip redial (downtime=62s)                 # 不应出现
[RECOVERY L1] REG down, skip redial (downtime=124s)                # 不应出现
[ROAMLINK] policy=1: SIM down 185s, switching to Roamlink          # 正确路径
```

---

## 二、TC-EG25-RL-ACT-01 测试用例问题分析

测试用例目标：验证 `roamlink_active` 阶段 **ping 层 300s 超时**切回 SIM。

### 问题 1：测试方法与目标不符（BIZ 层 120s 必然先于 ping 层 300s 触发）

`iptables -I OUTPUT -p icmp -j DROP` 同时造成两个效果：

1. ICMP 全部丢弃 → ping 失败，`roamlink_last_ping_ts` 停止更新
2. 设备几乎无出向流量 → `rmnet_data*` 入向流量随之停止 → **rx_packets 不增长**

BIZ 层超时 = **120s**，ping 层超时 = **300s**，BIZ 层必然抢先触发，ping 层 300s 根本等不到。

**正确的测试方法**：要测 ping 层 300s，必须在**保持 rmnet_data\* rx_packets 持续增长**（维持背景数据流量）的前提下再屏蔽 ICMP。

用例应拆分为两个独立用例：

| 用例目标 | 屏蔽方式 | 等待时间 |
|---|---|---|
| BIZ 层超时切 SIM | `iptables -I INPUT -i rmnet_data0 -j DROP`，ping 保持正常 | 120s |
| ping 层超时切 SIM | 维持业务流量（rx_packets 持续增长），再屏蔽 ICMP | 300s |

### 问题 2：代码 Bug — L1/L2/L3 缺少 policy 守卫

**位置**：`eg25/dial/dial.c`，L1/L2/L3 分级恢复入口（原第 1154 行）。

**根因**：注释写明"启用条件：策略4（NET_POLICY_FORCE_SIM）"，但实际代码缺少 policy 检查，导致策略 1/2/3 下只要 `dial_st` 不是 Roamlink 状态，L1/L2/L3 照样触发。

```c
// 修复前
if (!net_ok && start_fail_ts > 0 && has_connected_once &&
    p_dial_mng->dial_st != dial_stat_roamlink_starting &&
    p_dial_mng->dial_st != dial_stat_roamlink_active) {

// 修复后
if (!net_ok && start_fail_ts > 0 && has_connected_once &&
    p_dial_mng->dial_st != dial_stat_roamlink_starting &&
    p_dial_mng->dial_st != dial_stat_roamlink_active &&
    p_dial_mng->network_policy == NET_POLICY_FORCE_SIM) {
```

**影响**：BIZ 层切回 SIM 后（`is_roamlink_active=false`），policy=1/2/3 也进入 L1/L2/L3 分支。最严重的情况：policy=1 下 Roamlink 切换失败后，L2 会错误执行 `AT+CFUN=0/1`，与 Roamlink 180s 回切逻辑冲突。

**已修复**：`eg25/dial/dial.c` 第 1157 行。

---

## 三、策略 1/2 Roamlink ↔ SIM 切换逻辑梳理

### modem_mng（EG25）

**连通性探测**：ICMP ping（`test_can_ping_google()`），每 30s 心跳执行。

#### 策略 1（PREFER_ROAMLINK）— 以 Roamlink 为首选

启动后直接进入 `roamlink_starting`。

**Roamlink → SIM（三条路径，均设 `sim_fallback_ts=now`）**：

| 触发点 | 条件 |
|---|---|
| `roamlink_starting` 超时 | `roamlink_start_ts` 起 > 300s 未 ping 通 |
| `roamlink_active` ping 层超时 | 距上次 ping 通 > 300s |
| `roamlink_active` BIZ 层超时 | rx_packets 无增长 > 120s |

**SIM 备用期（`net_connected`）**：

| 条件 | 动作 |
|---|---|
| ping 通 + `sim_fallback_ts` 起 >= 300s | **主动回切 Roamlink**（首选通道恢复） |
| ping 失败 + `downtime_sec >= 180s` | **再次切 Roamlink** |

#### 策略 2（PREFER_SIM）— 以 SIM 为首选

启动后走 SIM 路径。

**SIM → Roamlink**：ping 失败 + `downtime_sec >= 180s` → 切 Roamlink，`roamlink_fallback_ts=now`。

**Roamlink → SIM（三条路径，均设 `sim_fallback_ts=now`，`roamlink_fallback_ts=0`）**：

| 触发点 | 条件 |
|---|---|
| `roamlink_starting` 超时 | > 300s 未 ping 通 |
| `roamlink_active` ping 层超时 | 距上次 ping 通 > 300s |
| `roamlink_active` BIZ 层超时 | rx_packets 无增长 > 120s |
| `roamlink_active` 稳定 300s（主动回切） | ping 通 + `roamlink_fallback_ts` 起 >= 300s |

回到 SIM（首选通道）后：SIM 正常则维持，SIM 再次失败 180s 才重新切 Roamlink。

---

### open_dial_for_artery

**连通性探测**：TCP 连通性测试（`nw_tcp_connectivity_test`），**不用 ICMP ping**。  
**切换触发方式**：失败**次数**达阈值，无时间等待。

#### 策略 1（PREFER_ROAMLINK）

启动后进入 `roamlink_starting`，TCP 通则进入 `roamlink_active`。

**Roamlink → SIM（三条路径，均设 `sim_fallback_timer=now`）**：

| 触发点 | 条件 |
|---|---|
| `roamlink_starting` 超时 | > `ROAMLINK_CONNECT_WAIT_SEC` 未 TCP 通 |
| `roamlink_active` TCP 失败 | 失败 >= `ROAMLINK_FAIL_THRESHOLD` 次 |
| `roamlink_active` BIZ 层超时 | rx_packets 无增长 >= `ROAMLINK_NO_DATA_TIMEOUT_SEC` |

**SIM 备用期（`net_connected`）**：

| 条件 | 动作 |
|---|---|
| TCP 通 + `sim_fallback_timer` 起 >= `SIM_FALLBACK_RETRY_SEC` | **主动回切 Roamlink** |
| TCP 失败 >= `TCP_FAIL_THRESHOLD` 次 | **再切 Roamlink**，`roamlink_fallback_timer=now` |

#### 策略 2（PREFER_SIM）

启动后走 SIM 路径，TCP 失败 >= `TCP_FAIL_THRESHOLD` 次 → 切 Roamlink，`roamlink_fallback_timer=now`。

**Roamlink → SIM（三条路径）**：

| 触发点 | 条件 |
|---|---|
| `roamlink_starting` 超时 | > `ROAMLINK_CONNECT_WAIT_SEC` |
| `roamlink_active` TCP 失败 | 失败 >= `ROAMLINK_FAIL_THRESHOLD` 次 |
| `roamlink_active` BIZ 层超时 | rx_packets 无增长 >= `ROAMLINK_NO_DATA_TIMEOUT_SEC` |
| `roamlink_active` 稳定回切 | TCP 通 + `roamlink_fallback_timer` 起 >= `ROAMLINK_FALLBACK_RETRY_SEC` |

---

### 两仓库关键差异

| 维度 | modem_mng (EG25) | open_dial_for_artery |
|---|---|---|
| 连通性探测 | ICMP ping | TCP 连通性测试 |
| SIM→Roamlink 切换触发 | 时间阈值（`downtime_sec >= 180s`） | 次数阈值（TCP 失败 >= N 次） |
| Roamlink→SIM 切换触发 | 时间阈值（距上次 ping 通 > 300s） | 次数阈值（TCP 失败 >= N 次） |
| L1/L2/L3 分级恢复 | 存在（仅 policy=4，已修复守卫缺失 bug） | **不存在** |
| Roamlink→SIM 后初始化路径 | 直接 `dial_stat_reg_check` | `sim_initialized` 为 true 走 `sim_init`，否则走 `dial_stat_init` |
| `roamlink_fallback_timer` 在 Roamlink 失败→SIM 时 | 显式清零 | 不清零（保留旧值） |

`roamlink_fallback_timer` 处理方式不同，但**功能一致**：该计时器只在 `roamlink_active` 状态下被检查；切回 SIM 后状态机不在 `roamlink_active`，清零与否均无影响；下次重新进入 Roamlink 时计时器会被重新赋值。

---

## 四、本次实际代码修改

| 文件 | 位置 | 修改内容 |
|---|---|---|
| `eg25/dial/dial.c` | 第 1157 行 | L1/L2/L3 入口补加 `p_dial_mng->network_policy == NET_POLICY_FORCE_SIM` 守卫 |
