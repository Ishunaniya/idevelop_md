# 会话总结 — Bug-3 修复与策略 1 全链路验证

**日期**：2026-05-22  
**平台**：EG25G（`modem_mng`）  
**分支**：`develop/rtms_sdk_v1.3_20240408_dc_switch`

---

## 一、承接上次会话的遗留分析

### 策略 1 补充测试日志（`-p icmp -j DROP`）

**测试方法**：`iptables -I OUTPUT -p icmp -j DROP`（仅屏蔽 ICMP，TCP 不受影响）

**关键时间线**：

| 时刻 | 事件 |
|---|---|
| ~05:08:54 | 屏蔽 ICMP，Roamlink ping 开始失败 |
| 05:09:32–05:10:33 | ConsecFail 1→3，`start_fail_ts` 设置 |
| 05:11:04 | **BIZ 超时 131s → 切 SIM**（rmnet_data* rx_packets 停增，阈值 120s） |
| 05:11:05 | `stop_service` 成功（TCP 未屏蔽，RBMaster 正常停止）|
| 05:11:11 | `[INIT] ICCID: 89852019925010000888`（物理 SIM）|
| 05:11:24 起 | **SIM:1 REG:0**，持续整个 SIM 备用期 |
| 05:15:32 | downtime=186s ≥ 180s → **心跳路径触发回切 Roamlink** |
| 05:16:33 | Roamlink ping OK，稳定运行 |

**遗留问题**：SIM 备用期 REG:0，状态机卡在 `reg_check`，数据呼叫无法建立，  
300s 稳定回切路径从未触发，只靠 180s 心跳路径兜底。

---

## 二、根因分析

### Bug-3：策略 1 `h_nw_client` 未初始化

**策略 1 启动路径**（`dial.c` line 888–903，`while` 循环之前）：

```c
// 策略1/3：直接设置 dial_st，跳过 dial_stat_init
if (policy == PREFER_ROAMLINK || FORCE_ROAMLINK) {
    roamlink_start_master();
    roamlink_start_service();
    p_dial_mng->dial_st = dial_stat_roamlink_starting;  // ← 跳过 dial_stat_init
}
```

`dial_stat_init`（调用 `QL_MCM_NW_Client_Init`）从未执行 → `h_nw_client = 0`。

**三处切换路径全部硬写**（lines 1796 / 1844 / 1889）：

```c
p_dial_mng->dial_st = dial_stat_sim_init;  // 从不走 dial_stat_init
```

**结果链**：

```
h_nw_client = 0
  → QL_MCM_NW_GetRegStatus(0, ...) → data_registration_valid = false
  → nw_reg_status_check 始终返回 false（REG:0）
  → 状态机卡在 dial_stat_reg_check
  → SIM 备用期数据呼叫无法建立
  → net_ok 始终 false → 300s 稳定回切条件永远不满足
```

### 为何策略 2 无此问题

策略 2 启动走 `dial_stat_none → dial_stat_init`，`QL_MCM_NW_Client_Init` 在启动时就已正确调用，`h_nw_client` 有效，后续切换复用 handle，REG:1 正常。

### open_dial_for_artery 无此 Bug 的原因

三处切换点均有 `sim_initialized` 守卫（`src/dial/dial.c` lines 1408–1419 等）：

```c
if (p_dial_mng->sim_initialized)
    p_dial_mng->dial_st = dial_stat_sim_init;   // 复用已有 handle
else
    p_dial_mng->dial_st = dial_stat_init;        // 首次：调用 QL_MCM_NW_Client_Init
```

modem_mng 缺少这个守卫，本次修复即对齐此实现。

---

## 三、Bug-3 修复

**文件**：`eg25/dial/dial.c`，三处切换点

```c
// 修复前（lines 1796 / 1846 / 1893，三处相同）
p_dial_mng->dial_st = dial_stat_sim_init;

// 修复后
p_dial_mng->dial_st = p_dial_mng->sim_initialized
                      ? dial_stat_sim_init   // 复用已有 NW handle
                      : dial_stat_init;      // 首次：QL_MCM_NW_Client_Init
```

| 触发来源 | 代码位置 |
|---|---|
| `roamlink_starting` 超时 | line 1796 |
| `roamlink_active` ping 超时 | line 1846 |
| `roamlink_active` BIZ 超时 | line 1893 |

**效果**：
- 策略 1 首次 Roamlink→SIM：走 `dial_stat_init` → `QL_MCM_NW_Client_Init` → `sim_initialized=true`
- 后续切换：复用 handle，走 `dial_stat_sim_init`

---

## 四、license 下载逻辑核查

**结论：两仓库功能完全一致，无 Bug。**

| 关键点 | modem_mng | open_dial_for_artery |
|---|---|---|
| `LICENSE_WAIT_TIMEOUT_SEC` | 300s（`roamlink/roamlink.h:76`）| 300s（`roamlink.h:27`）|
| `LICENSE_CHECK_INTERVAL_SEC` | 60s（`roamlink/roamlink.h:77`）| 60s（`roamlink.h:28`）|
| Probe LICENSE_MISSING → 备份恢复失败 | `license_pending=true`，`FORCE_SIM` | 相同 |
| SIM 联网后才启动 RBMaster | ✅ `dial.c:1572`（`rbmaster_started` 保证只启动一次）| ✅ |
| 每 60s 轮询 license，出现则 backup+reboot | ✅ `dial.c:934-944` | ✅ |
| 超时 300s → 放弃，保持 FORCE_SIM | ✅ `dial.c:927-932` | ✅ |
| license 下载期间禁止切 Roamlink | 隐式：`roamlink_available=false` | 显式 `license_pending` 检查 |
| `reg_timeout_handler` 禁止切 Roamlink | 显式 `!license_pending`（`dial.c:1953`）| 显式 `!license_pending` |

**完整流程**：license 缺失 → 临时 FORCE_SIM → SIM 联网后启动 RBMaster 下载 → 下载成功 backup+reboot → 重启后 probe OK → 正常走配置策略。

---

## 五、设备验证

### 测试方案

```bash
# 策略1，Roamlink 激活运行中
iptables -I INPUT -i rmnet_data0 -j DROP   # 屏蔽 rmnet_data0 入向，触发 BIZ 超时
# 等待 BIZ 超时 → 切 SIM → 数据呼叫建立 → ping 通
iptables -D INPUT -i rmnet_data0 -j DROP   # 移除，SIM ping 恢复
# 等待 300s → 观察自动回切 Roamlink
```

### 关键日志及分析

| 时刻 | 日志 | 含义 |
|---|---|---|
| 05:46:50 | `Biz layer no data for 130s, switching to SIM` | BIZ 超时，`sim_fallback_ts` 记录此刻 |
| 05:46:56 | `[MODEM] Model: EG25` | **Bug-3 修复生效**：`dial_stat_init` 被调用（修复前无此日志）|
| 05:47:00 | `[INIT] ICCID: 89852019925010000888` | 物理 SIM 正确读取 |
| 05:47:08 | `[SDK] DataCall connected \| IP=202.175.36.16` | **SIM 数据呼叫成功**（修复前永远无法到达）|
| 05:47:19 | `CH:SIM \| REG:1` | **REG:1**（修复前全程 REG:0）|
| 05:48:24 | `ConsecFail:3` | iptables 仍在，SIM ping 路由短暂失败（测试副作用）|
| ~05:48:25 | 移除 iptables | |
| 05:48:47 | `Network recovered after 33s` | SIM ping 恢复，`net_ok=true`，300s 有效计时开始 |
| 05:52:58 | `DataCall disconnected profile=1 err=0x7d1` | **300s 稳定到期**（05:46:50+300s=05:51:50），回切触发，SDK 异步断开事件 |
| 05:53:08 | `DataCall connected profile=1 \| IP=172.20.164.2` | Roamlink 虚拟 SIM IP |
| 05:53:30 | `Ping OK in starting state, channel now active` | Roamlink 重新激活 |
| 05:53:30 | `CELL CHANGE D17C147 → 99D991` | 物理 SIM 小区 → Roamlink 虚拟小区 |
| 05:54:30 起 | `RL_FAIL:0`，RX_PKT 持续增长 | Roamlink 稳定运行 |

### 策略 1 全链路验证结论

```
Roamlink_active
  → BIZ 超时（130s）→ 切 SIM
  → dial_stat_init（Bug-3 修复）→ QL_MCM_NW_Client_Init
  → REG:1 → DataCall 建立（18s）
  → ping 通，sim_fallback_ts 计时
  → 300s 稳定 → 主动回切 Roamlink   ← 本次首次验证 ✅
  → Roamlink ping OK，RX_PKT 持续增长，稳定运行
```

---

## 六、本次代码变更汇总

| 文件 | 修改内容 | 版本 |
|---|---|---|
| `eg25/dial/dial.c` | Bug-3：三处 Roamlink→SIM 切换点增加 `sim_initialized` 守卫 | v1.32 |
| `CHANGELOG.md` | 新增 v1.32 条目，记录 Bug-3 根因与修复 | v1.32 |

### 建议提交说明

```
[modem_mng] [BUGFIX] EG25 修复策略1 Roamlink→SIM 切换后 REG:0 导致 SIM 备用期数据呼叫无法建立

- 根因：策略1启动跳过 dial_stat_init（h_nw_client=0），三处切换路径
  硬写 dial_stat_sim_init，QL_MCM_NW_GetRegStatus 返回
  data_registration_valid=false，nw_reg_status_check 始终 false (REG:0)，
  状态机卡在 reg_check，SIM 备用期数据呼叫无法建立，300s 回切路径无法触发

- 修复（dial.c）：roamlink_starting 超时 / ping 层超时 / BIZ 层超时
  三处切换点增加 sim_initialized 守卫——首次进入 SIM 走 dial_stat_init
  （完整 NW Client 初始化），后续复用 handle 走 dial_stat_sim_init；
  对齐 open_dial_for_artery 实现

- 验证（策略1）：BIZ 超时→切 SIM→REG:1→DataCall 建立→ping 通→
  300s 稳定→自动回切 Roamlink 全链路通过
```
