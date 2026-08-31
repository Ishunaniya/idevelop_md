# EC200A vs EG25 Roamlink 调度逐行核对报告

**核对日期**：2026-05-30
**核对人**：yuping.liu
**分支**：`develop/rtms_sdk_v1.3_20240408_dc_switch`
**参考基线**：EG25 v1.31.x（已设备验证），EC200A（未设备验证）
**目的**：确认 EC200A 平台的 Roamlink 功能与已验证的 EG25 平台是否对齐，列出全部事实差异

---

## 一、核对结论（先看这里）

> **两平台在策略、切换触发、超时、双向回切、license_pending、ping 抖动过滤上全部对齐。
> 唯一的实质性差异是 EC200A 缺少「业务层 rx_packets 无增长监控」这一整块功能。
> 其余差异均为合理的平台架构差异（状态机 vs SDK 调用），不应也不需要对齐。**

依据的两条硬事实：

1. Roamlink 功能于 v1.31.0 提测时在 **EG25 / EC200A 双平台一起集成**（见《提测变更记录_modem_mng_v1.31_roamlink.md》第 35、325 行），底层 `roamlink/roamlink.c`、`roamlink.h` 为两平台共用。
2. 提测基线 `9b9dc05c` 之后的 4 个提交（`93fc716b` / `bd015202` / `58ddaac1` / `b1b5516c`）**全部是 EG25 专属**，一行未触及 `dialer_ec200a.cpp`、`ec200a/`；共享的 `roamlink.c` 提测后也再未改动。且这 4 个提交对 `eg25/dial/dial.c` 的改动中**几乎没有 roamlink 调度的实质改动**，多为通用拨号/SIM/APN/nw 的 bugfix。

---

## 二、架构背景

| 维度 | EG25 | EC200A |
|---|---|---|
| 入口 | `eg25/dial/dial.c`（C，状态机 enum） | `dialer_ec200a.cpp`（C++，`dial_loop` 单函数） |
| 通道状态表示 | 独立枚举 `dial_stat_roamlink_starting` / `dial_stat_roamlink_active` | `bool is_roamlink_active` + `uint64_t` 毫秒时间戳 |
| 网络注册 | AT 串口轮询（`nw_reg_status_check` / CEREG） | SDK 异步回调驱动 |
| AT 通道 | `Ql_SendAT(smd_fd, ...)` 直连串口 | `serial_atcmd` 外部工具 / SDK API |
| 数据网卡 | `rmnet_data*` | `ccinet0` |
| 底层 roamlink 接口 | 共用 `roamlink/roamlink.c`（内含 `#ifdef USE_EC200A_DIAL` / `USE_EG25_DIAL` 分支） | 同左 |

---

## 三、逐项核对明细（✅ 已对齐）

| 调度环节 | EG25 位置 | EC200A 位置 | 判定 |
|---|---|---|---|
| 初始通道（策略 1/3 启动直接走 Roamlink） | `dial.c:909-924` | `dialer_ec200a.cpp:1645-1660` | ✅ 一致 |
| SIM→Roamlink 切换（失联 ≥180s `ROAMLINK_SWITCH_TIMEOUT`） | `dial.c:1137-1162` | `2193-2209` | ✅ 一致 |
| 连接超时（启动后 300s 未首次 ping 通） | `dial.c:1818-1858` | `1729-1731` | ✅ 一致 |
| ping 层失联超时（ping 通后再失联 300s） | `dial.c:1869-1913` | `1732-1734` | ✅ 一致 |
| 策略 1：SIM 备用稳定 300s 回切 Roamlink | `dial.c:1043-1065` | `2085-2108` | ✅ 一致 |
| 策略 2：Roamlink 备用稳定 300s 回切 SIM | `dial.c:1066-1087` | `2109-2124` | ✅ 一致 |
| 策略 3：超时只重启 RBMaster 不切 SIM | `dial.c:1845-1856` | `1752-1762` | ✅ 一致 |
| license_pending + RBMaster 延迟启动下载 | `dial.c:944-972` | `1772-1791` + `2141-2145` | ✅ 一致 |
| ping 抖动过滤（连续失败 `PING_FAIL_THRESHOLD=3` 才计） | `dial.c:1089-1090` | `2150-2155` | ✅ 一致 |
| 启动时清理残留 RBMaster（策略 2/4） | `dial.c:139-149` | `1633-1639` | ✅ 一致 |
| probe 四态 + license 备份恢复 | `dial.c:66-153` | `1584-1640` | ✅ 一致 |
| **业务层 rx_packets 无增长（120s）触发切换** | `dial.c:1014-1037` / `1914-1960` | **完全缺失** | ❌ **EC200A 缺失** |

---

## 四、合理架构差异（**不应**对齐）

这些是两平台底层架构不同导致的实现差异，各自正确，强行"对齐"反而引入错误：

| 项 | EG25 | EC200A | 不动的理由 |
|---|---|---|---|
| 切回/重连机制 | 改 `dial_st` 重走状态机（`sim_init`/`reg_check`） | `ql_data_call_start/stop(g_call_id)` 直接调 SDK | 两边各自的拨号架构，都正确 |
| starting/active 区分 | 独立枚举两态 | `roamlink_last_ping_ts==0/!=0` 隐式区分 | 行为等价 |
| reg 超时切 Roamlink | `dial.c:1970-2018`（AT 注册轮询超时处理） | 无 | EG25 专有；EC200A 注册由 SDK 回调驱动，无需轮询超时 |
| SIM→Roamlink 条件含 `FORCE_ROAMLINK` | 含（`dial.c:1141`） | 不含（`2194`） | EC200A 策略 3 启动时无论成败均置 `is_roamlink_active=true`，该分支（要求 `!is_roamlink_active`）对策略 3 不可达，不含是自洽的；EG25 因启动失败时 `active=false` 才需要含 |

> 关于策略 3 启动失败的自愈路径差异：EG25 启动失败时 `is_roamlink_active` 保持 `false`，靠 SIM 通道 ping 失败后的切换分支（含 `FORCE_ROAMLINK`）重新切回 Roamlink；EC200A 启动无论成败均置 `true`，靠守护块超时重启服务。**两者殊途同归，都能自愈**，非缺陷。

---

## 五、唯一实质差异详情：业务层 rx_packets 无增长监控

### 5.1 功能描述（提测记录 2.3 节，EG25 已设备验证）

TCP/ICMP ping 通只证明网络层可达。Roamlink 虚拟 SIM 注册失败时 ping 可能仍通，但数据网卡的 `rx_packets` 不会增长，**业务数据实际无法流转**。

EG25 的监控流程：
- 进入 `roamlink_active`：记录初始基准 `roamlink_rx_packets` + 时间戳 `roamlink_no_data_timer`（`dial.c:1014-1016`）
- 每次心跳（30s）：读当前 rx_packets，有增长则更新基准/时间戳，无增长则打印告警日志（`dial.c:1021-1036`）
- 每个主循环 tick：若距上次增长 > `ROAMLINK_NO_DATA_TIMEOUT_SEC`(120s)，策略 1/2 切回 SIM、策略 3 重启 RBMaster（`dial.c:1914-1960`）

### 5.2 EC200A 现状

`dialer_ec200a.cpp` 的 Roamlink 守护块（`1728-1764`）**只有 ping 层两层超时**（connect timeout / fail timeout），**没有任何 rx_packets 业务层检测**。
grep 验证：`nw_get_rmnet_rx_packets_sum` / `roamlink_rx_packets` / `roamlink_no_data_timer` / `ROAMLINK_NO_DATA_TIMEOUT` 在 EC200A 出现次数均为 **0**。

**影响**：Roamlink 虚拟 SIM 注册失败但 ping 仍通的场景下，EG25 会在 120s 后切换，**EC200A 不会切换**（业务持续断流）。

### 5.3 照搬障碍（关键事实）

| 平台 | 读取函数 | 路径 |
|---|---|---|
| EG25 | `nw_get_rmnet_rx_packets_sum()`（`eg25/nw/nw.c:411`） | 遍历 `/sys/devices/virtual/net/rmnet_data*` 累加 |
| EC200A | `nw_get_if_statistics_rx_packets(&v, if_name)`（`ec200a/nw/nw.c:1553`，`data_call.c:229` 已在用） | `/sys/devices/virtual/net/ccinet0/statistics/rx_packets` |

**EC200A 没有 `rmnet_data` 接口，数据网卡是 `ccinet0`。** 因此 EG25 的遍历函数不能直接照搬，需改用 EC200A 现成的单接口读取方式（`g_if_name` / `"ccinet0"`）套上相同的监控逻辑（基准 / 120s 阈值 / 策略分支）。

---

## 六、风险与建议

### 6.1 风险

1. **EC200A 无设备验证环境**：给未验证平台新增"无数据自动切换"逻辑，若 `ccinet0` 的 `rx_packets` 读取或 120s 阈值有偏差，可能误判触发切换，反而劣于现状。
2. **实现非同源**：必须改用 `ccinet0` 读取方式，与 EG25 不是同一套代码，无法靠"照搬已验证代码"获得同等可信度。

### 6.2 建议

- roamlink 调度主体两平台已对齐，**无需改动**。
- 是否补"业务层 rx_packets 监控"由项目决定。若实现，建议：
  1. 用 EC200A 现成的 `nw_get_if_statistics_rx_packets(&v, "ccinet0")` 套 EG25 同款逻辑（进入 active 记基准 → 每心跳更新 → >120s 无增长按策略切换）；
  2. 用编译宏开关包裹，未验证环境下默认关闭（保守）；
  3. 待 EC200A 具备设备验证条件后，按提测记录 TC-12 用例验证后再默认启用。

---

## 七、核对方法（可复现）

```bash
# 1. 提测后 EG25 专属提交（确认未触及 EC200A）
git log --oneline 9b9dc05c..HEAD
git diff --stat 9b9dc05c..HEAD          # 仅 eg25/* 变动
git log --oneline 9b9dc05c..HEAD -- roamlink/roamlink.c   # 空 = 共享层未改

# 2. roamlink 子功能存在性对比
grep -c "nw_get_rmnet_rx_packets_sum\|roamlink_rx_packets\|roamlink_no_data_timer\|ROAMLINK_NO_DATA_TIMEOUT" \
     eg25/dial/dial.c eg25/nw/nw.c        # EG25 > 0
grep -c "<同上>" dialer_ec200a.cpp        # EC200A = 0

# 3. 数据网卡名差异
grep -n "rmnet_data" eg25/nw/nw.c          # EG25: rmnet_data*
grep -n "ccinet"     dialer_ec200a.cpp     # EC200A: ccinet0
```
