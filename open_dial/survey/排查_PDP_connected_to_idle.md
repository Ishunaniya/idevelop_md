# 排查记录：`DataCall connected -> idle` 异常分析

> **状态**：进行中（根因未定论）
> **创建**：2026-06-02
> **涉及版本**：v1.28.1
> **关联代码**：`data_call/data_call.c`、`dial.c`、`at/at.c`
> **关联手册**：`md/ec200a_pdf/Quectel_EC200A-CN(TA)_QuecOpen_数据拨号API_参考手册_V1.0.0_Preliminary_20230411.pdf`

---

## 0. 本文档的纪律（重要）

本次排查的核心教训：**严格区分「事实」「推断」「未决」**，不把推断当事实。
- ✅ **事实** = 读了代码 / 手册原文 / 日志原文，可复核。
- ⚠️ **推断** = 基于事实的合理外推，但未被证据直接证明。
- ❓ **未决** = 现有证据无法判定，需要新数据。

继续排查时请保持同样纪律。

---

## 1. 问题现象（事实 · 来自现场日志）

SN 现场日志片段（2026-06-02 03:32 附近）：

```
03:32:25 [INFO]  SDK auto-reconnect phase started (interval=25s). L1 at 300s, L2 at 600s, L3 at 2100s.
03:32:26 [DIAG]  RSRP:-77 | RSRQ:-11 | CID:00000000 | IP:10.3.153.52 | CEER:"0 Unknown" | PDP:+CGACT: 1,1 | +CGACT: 2,0
03:32:27 [EVENT] DataCall[4/auto_network]: dial connected -> dial idle
03:32:27 [EVENT] PDP Idle (was connected): IF=ccinet1
03:32:27 [WARN]  Status updated to 0.
03:32:27 [EVENT] DataCall[4/auto_network]: dial idle -> dial connecting
03:32:28 [EVENT] DataCall[4/auto_network]: dial connecting -> dial connected
03:32:28 [WARN]  Status updated to 1.
03:32:28 [EVENT] PDP IPv4: IF=ccinet1 IP=10.0.76.96 GW=10.0.76.97 DNS=36.158.169.100/36.158.151.124
03:32:34 [DIAG]  Snapshot saved: /media/sdcard/dial_snap/[dmesg|logcat]_fault_20260602_033226.log
03:32:36 [EVENT] Network Recovered in SDK phase (L0). Down: 12s.
```

**用户最初的疑问**：为什么会出现 `dial connected -> dial idle`？当前程序处理是否有问题？最佳方案是什么？

---

## 2. 已查证的事实

### 2.1 日志字段含义（事实 · 读代码）

- **`CID` = 服务小区 Cell ID**，从 `+CREG: n,stat,"lac","ci",act` 第 4 字段（ci，十六进制）解析。
  来源：`at/at.c:505-524`（注释原文："服务小区 CID，十六进制"）。
  → 故 `CID:00000000` = 那一刻采样到的服务小区 ci 为 0。
- DIAG 整行由 `build_ext_line()` 拼装，打印点在 `dial.c:701-705`。

### 2.2 程序的分级恢复未被触发（事实 · 日志+代码）

- `SDK auto-reconnect phase started` = ping 失败时首次置 `start_fail_ts`，打印点 `dial.c:672-679`。
- `Network Recovered in SDK phase (L0). Down: 12s.` = `dial.c:630-632`。
- 全程**没有** `[RECOVERY L1]` / L2 / L3 日志 → 本次故障在 L0（前 5 分钟 SDK 自愈窗口）内自行恢复，
  应用层**没有**主动干预（没有调用 stop/start）。
  - L1 恢复路径会打印 `[RECOVERY L1] Stopping...` / `Restarting...`（`dial.c:778,784,789,797`），本段无此日志。

### 2.3 数据拨号 API 手册的状态定义（事实 · PDF §2.3，印刷第 11 页）

```
NONE        ：数据拨号实例不存在
CREATE      ：数据拨号实例被创建
IDLE        ：数据拨号实例已经被配置
CONNECTING  ：正在进行拨号
CONNECTED   ：数据拨号成功，获取到 IP 地址等信息
DISCONNECTED：数据拨号失败或者断开
DELETE      ：数据拨号实例被删除
```

### 2.4 ⭐ 官方状态机「图 2」的转换关系（事实 · PDF 印刷第 11 页，PDF page index 12）

这张图是本次排查最关键的证据。逐条读出的箭头与标签：

| 转换 | 标签 | 类型 |
|---|---|---|
| NONE → CREATE | `CREATE_REQ` | 请求 |
| CREATE → IDLE | `CONFIG_REQ` | 请求 |
| IDLE → CONNECTING | `START_REQ` | 请求 |
| CONNECTING → CONNECTED | `Data call succeed` | 事件（虚线） |
| CONNECTING → DISCONNECTED | `Data call failed` | 事件（虚线） |
| **CONNECTED → DISCONNECTED** | **`Network disconnect`** | **事件（虚线）** |
| **CONNECTED → IDLE** | **`STOP_REQ`** | **请求** |
| DISCONNECTED → CONNECTING | `Reconnect enable` | 事件（虚线，自动重连） |
| 各状态 → DELETE | `DELETE_REQ` | 请求 |

**手册语义的关键两条：**
1. **网络侧断开一个已连接的拨号 = `CONNECTED → DISCONNECTED`**（"Network disconnect"），**不经过 IDLE**。
2. **`CONNECTED → IDLE` 只由 `STOP_REQ` 触发**（即"有人发了停止请求"）。
3. 自动重连按图走 `CONNECTED → DISCONNECTED → CONNECTING`，**也不经过 IDLE**。

### 2.5 重连机制（事实 · PDF §2.2，印刷第 10 页）

- 自动重连模式：`DISABLE / NORMAL / MODE1 / MODE2`。
- 设置接口：`ql_data_call_param_set_reconnect_mode()` + `ql_data_call_param_set_reconnect_interval()`（§3.3.18 / §3.3.20）。
- 手册建议：**重连间隔 ≥20 秒**，频繁拨号可能被运营商拉黑。
- → 若将来要调 L0 行为，改的就是这两个接口。

### 2.6 回调使用约束（事实 · PDF §3.3.29）

- 状态变化回调里**禁止调用任何数据拨号接口**，**禁止耗时/阻塞操作**。
- ⚠️ 注意：现有 `data_call_status_ind_cb()` 内大量使用 `system()` 跑 `ip`/`iptables`/`cat`，
  严格说违反此约束（有阻塞风险）。这是既有实现，本次未动；后续重构可考虑改为异步/标志位驱动。

---

## 3. ⭐ 关键结论：代码注释被手册推翻

`data_call/data_call.c:182` 的注释：

```c
/* AT+CFUN=0/4 关闭射频时，SDK 发 CONNECTED→IDLE 而非 DISCONNECTED，
 * 需同样清理状态，否则 g_pdp_connected 永远不归零。 */
```

- **用户明确表示对该注释内容不确定。**（来源不明，可能当初是猜测写的）
- **图 2（§2.4）直接与之矛盾**：按手册，网络/射频断开应是 `CONNECTED → DISCONNECTED`，
  而 `CONNECTED → IDLE` 是 `STOP_REQ`（停止请求）所致。

**因此：**
- 现场这条 `CONNECTED → IDLE → CONNECTING → CONNECTED`，按手册语义，**形态对应「STOP_REQ 然后 START_REQ」（停了再拨）**，
  而**不是**一次单纯的网络瞬断（网络瞬断不经过 IDLE）。
- 注释的因果（"网络/CFUN 断开 → IDLE"）**不被手册支持**，应视为存疑/待修正。

---

## 4. 未决问题（需要新数据才能定论）

既然 `CONNECTED → IDLE` 在手册里 = "收到 STOP_REQ"，那么本次是**谁发的 STOP_REQ**？

- ❌ **不是应用层**：app 的 L1 stop+start 会打 `[RECOVERY L1]` 日志，本段没有。
- ❓ 剩下两种可能，**现有证据（手册+日志）无法区分**：
  1. **SDK 固件内部**在自动重连时自己走了一次 stop/start（固件实现 ≠ 理想图）；
  2. **固件实际行为与文档图有出入**（这版固件遇到网络丢承载就是报 IDLE，而非文档说的 DISCONNECTED）。

⚠️ 另一个未证实的推断：`CID:00000000` + `CEER:"0 Unknown"` **暗示**可能是网络侧瞬时丢小区/丢承载，
但"重选/网络主动拆除"是推断，不是事实——采样也可能正好落在状态空窗。

---

## 5. 下一步排查路径（按优先级）

### 路径 A（首选）：读故障快照，定位真实底层事件
- 文件：`/media/sdcard/dial_snap/dmesg_fault_20260602_033226.log`
  与 `/media/sdcard/dial_snap/logcat_fault_20260602_033226.log`
- 目标：看 CP/网络侧在 03:32:26~28 的真实事件——是网络下发 deactivate、模组 RLF、还是 SDK 内部 stop。
- 能把"谁发的 STOP_REQ"从「未决」变成「事实」。

### 路径 B：实测验证注释真伪
- 在设备上手动发 `AT+CFUN=0`（或 `AT+CFUN=4`），观察 `data_call_status_ind_cb` 回调上报的是
  `IDLE` 还是 `DISCONNECTED`。
- 直接证实/证伪 `data_call.c:182` 那条注释。

### 路径 C：复核 SDK 重连配置
- 在代码里搜索是否调用了 `ql_data_call_param_set_reconnect_mode/interval`，确认当前 L0 自动重连用的是
  NORMAL 还是 MODEx、间隔多少。日志里 app 自己打印的 `interval=25s` 是 **ping 检测间隔**，
  不要与 SDK 重连间隔混淆（需读代码确认 `dial.c:674` 那个 `%d` 实参到底是什么）。

---

## 6. 代码健壮性隐患（与根因独立，确认存在的真实问题）

读 `data_call/data_call.c:31-240` 确认：

### 隐患 A：IDLE 清理门控过窄
- 现状（`data_call.c:179-180`）：仅当 `call_status==IDLE && pre_call_status==CONNECTED` 才清理。
- 风险：若 SDK 报 `PARTIAL_V4_CONNECTED(0x4) → IDLE`、`CONNECTED → ERROR(0x8)`、
  `CONNECTED → DELETED(0x9)` 等路径，`if` 不命中 → `g_pdp_connected` 滞留为 1 + 路由/NAT 残留。
- 手册 §3.3.28.2 中 PARTIAL_V4/V6、ERROR、DELETED 都是合法状态，现在完全没处理。

### 隐患 B：清理代码三处重复
- CONNECTED 块开头的预清理、IDLE 分支、DISCONNECTED 分支，三段 `ip route del` / `iptables -D` / `unlink`
  几乎一字不差重复，改一处易漏其他两处。

### 建议改法（尚未实施，待根因确认后一并做）
把"下线判定"从"看 pre 是不是 CONNECTED"改为"看我方是否曾连接"，并抽公共 helper：

```c
/* 凡进入非连接态且我方之前是连着的，统一清理 */
else if (g_pdp_connected &&
         (call_status == QL_NET_DATA_CALL_STATUS_IDLE ||
          call_status == QL_NET_DATA_CALL_STATUS_DISCONNECTED ||
          call_status == QL_NET_DATA_CALL_STATUS_ERROR ||
          call_status == QL_NET_DATA_CALL_STATUS_DELETED)) {
    teardown_pdp_routes();   // 抽出三段重复逻辑
}
```
- 注意：保持 §3.3.29 的回调约束意识（理想上回调内不该跑阻塞 `system()`，但属既有实现，本次不强制改）。

---

## 7. 对原始问题的当前回答（截至本文档）

1. **为什么 connected -> idle？**
   - 事实层面：按官方图 2，`CONNECTED→IDLE` = `STOP_REQ`。本次形态是"停了再拨"，非单纯网络瞬断。
   - 谁发的 stop：**未决**（非 app；SDK 内部 or 固件偏离文档，待路径 A/B 定论）。
2. **PDF 有答案吗？**
   - 状态定义和图 2 有（已提取，见 §2.3/§2.4）；但"何时报 IDLE vs DISCONNECTED 的内部触发细节"
     需结合快照/实测，手册只给到状态机层面。
3. **当前程序处理有问题吗？**
   - 本次这条**未触发误恢复，IDLE 清理逻辑本身工作正常**（12s 内 L0 自愈）。
   - 但存在 §6 的健壮性隐患（门控窄、重复代码），以及 `data_call.c:182` 的**错误注释**待修正。
4. **最佳方案？**
   - 对这种 12s 瞬断本身：不加额外恢复（L0/定时恢复已闭环兜底）。
   - 待办：先定根因（路径 A/B），再修注释 + 按 §6 拓宽下线判定/抽 helper。

---

## 8. 待办清单（下次直接接手）

- [ ] **路径 A**：读 `/media/sdcard/dial_snap/{dmesg,logcat}_fault_20260602_033226.log`，定位真实底层事件。
- [ ] **路径 B**：设备上实测 `AT+CFUN=0`，确认回调报 IDLE 还是 DISCONNECTED。
- [ ] **路径 C**：读代码确认 SDK 重连模式/间隔，及 `dial.c:674` 的 `interval` 实参语义。
- [ ] 修正 `data_call.c:182` 的错误注释（确认根因后再定怎么写）。
- [ ] 实施 §6 改法：拓宽下线判定（覆盖 PARTIAL/ERROR/DELETED）+ 抽 `teardown_pdp_routes()` helper。
- [ ] 编译验证零 warning。
