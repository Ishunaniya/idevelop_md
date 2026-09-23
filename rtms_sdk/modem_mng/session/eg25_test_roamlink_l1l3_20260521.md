# EG25 平台测试用例：Roamlink 状态机 & L1/L2/L3 分级恢复

**版本：v1.31**  
**日期：2026-05-21**  
**平台：EG25（`QL_MODULE_PLATFORM=EG25G`）**  
**源码基线：commit `45126949`**

---

## 一、测试环境常量速查

| 常量 | 值 | 来源 |
|---|---|---|
| `PING_FAIL_THRESHOLD` | 3（次连续失败） | `dial.c` |
| `L1` 阈值 | 60s | `dial.c: LEVEL1=60` |
| `L2` 阈值 | 300s（5min） | `dial.c: LEVEL2=5*60` |
| `L3` 阈值 | 1800s（30min） | `dial.c: LEVEL3=30*60` |
| L1 节流 | 60s（`last_l1_ts`） | `dial.c` |
| L2 节流 | 300s（`last_recovery_ts`） | `dial.c` |
| `ROAMLINK_SWITCH_TIMEOUT_SEC` | 180s（3min） | `roamlink.h` |
| `ROAMLINK_CONNECT_TIMEOUT_SEC` | 300s | `roamlink.h` |
| `ROAMLINK_NO_DATA_TIMEOUT_SEC` | 120s | `roamlink.h` |
| `SIM_FALLBACK_RETRY_SEC` | 300s | `roamlink.h` |
| `ROAMLINK_FALLBACK_RETRY_SEC` | 300s | `roamlink.h` |
| `REG_CHECK_TIMEOUT_SECONDS` | 300s | `dial.h` |
| `DIAL_TIMEOUT_SECONDS` | 120s（net_connected rx_packets 无增长超时） | `dial.h` |
| 心跳周期 | 30s | `dial.c` |
| 心跳扩展周期 | 5min（首次联网后） | `dial.c` |

> **注意**：`start_fail_ts`（故障计时起点）在连续第 `PING_FAIL_THRESHOLD=3` 次 ping 失败时设置，因此实际 L1/L2/L3 从断网算起的触发时间 = **ping 失败 3 次所需时间（约 60s）+ 对应阈值**。

---

## 二、运行时文件速查

| 路径 | 含义 |
|---|---|
| `/tmp/dial_status` | INI 格式状态快照，每 30s 原子更新 |
| `/tmp/network_type` | `1`=物理SIM，`2`=Roamlink |
| `/usrdata/network.ini` | 策略配置（`[network] network_select=1~4`） |
| `/usrdata/roamlink/RBMaster` | Roamlink 服务主程序 |
| `/usrdata/roamlink/etc/.pconfig/license.cer` | License 文件 |
| `/data/ufs/license.cer` | License 本地备份 |
| `/opt/conf.ini` | Roamlink 配置（factoryApp 下载） |
| `/sys/devices/virtual/net/rmnet_data*/statistics/rx_packets` | 业务层流量计数 |

---

## 三、观测命令

```bash
# 实时日志（EG25 日志写入 SD 卡，路径与 EC200A 相同）
tail -f /media/sdcard/dial_log/dial_*.log

# 结构化状态文件
cat /tmp/dial_status

# 当前通道
cat /tmp/network_type

# RBMaster 进程
ps aux | grep RBMaster

# Roamlink 控制端口
nc -z 127.0.0.1 5568 && echo "port OK" || echo "port CLOSED"

# rmnet_data* rx_packets（所有接口）
cat /sys/devices/virtual/net/rmnet_data*/statistics/rx_packets

# 模拟无 rx_packets 增长（丢弃入站流量）
iptables -I INPUT -i rmnet_data0 -j DROP

# 恢复
iptables -D INPUT -i rmnet_data0 -j DROP

# 模拟 ping 失败（丢弃 ICMP）
iptables -I OUTPUT -p icmp -j DROP
iptables -D OUTPUT -p icmp -j DROP
```

---

## 四、L1/L2/L3 分级恢复测试

> **前置条件（所有 L1/L2/L3 用例）**：
> - `network_select=4`（FORCE_SIM）
> - Roamlink 无需就绪（L1/L2/L3 只在非 Roamlink 通道时生效）
> - `has_connected_once=1`（至少 ping 通过一次；首次联网后自动置 1）
> - EG25 无 SDK 自动重连层（L0），L1 是第一道防线

---

### TC-EG25-L1-01：L1 软重拨（正常触发）

**目标**：验证 L1 在 `start_fail_ts + 60s` 时执行 `dail_stop_data_call + dail_start_data_call`，且只有 REG 存活时才执行。

**前提**：
- `network_select=4`，SIM 已联网，`has_connected_once=1`
- 断网模拟方式使 REG 保持正常（如仅 `iptables -I OUTPUT -p icmp -j DROP` 屏蔽 ping）

**步骤**：
1. 确认当前正常联网：`cat /tmp/dial_status | grep state` 显示 `net_connected`
2. 执行 `iptables -I OUTPUT -p icmp -j DROP`，使 ping 全部失败
3. 等待日志出现 3 次 ping 失败，`start_fail_ts` 被设置：
   ```
   [HEARTBEAT] Ping failed 3 consecutive times, fault timer started
   ```
4. 再等待约 60s（`LEVEL1`），观察日志出现 L1 前快照：
   ```
   [RECOVERY L1] LastErr: <CEER内容> | PDP: <CGACT内容>
   ```
5. 随后执行 L1 操作（无单独的 L1 触发日志，通过 `dail_stop_data_call/dail_start_data_call` 的底层日志识别）
6. 若 L1 后仍无法恢复（iptables 未解除），约 60s 后 L1 再次触发（`last_l1_ts` 节流）

**验证**：
```bash
# 确认 L1 前快照出现
grep "RECOVERY L1" /media/sdcard/dial_log/dial_*.log

# 解除 iptables 后网络应恢复，验证恢复日志
# grep "Network recovered" /media/sdcard/dial_log/dial_*.log
```

**预期状态**：
- `[RECOVERY L1] LastErr: ... | PDP: ...` 出现
- 解除 iptables 后出现 `[HEARTBEAT] Network recovered after XXs`，`start_fail_ts=0`，`last_l1_ts=0`，`last_recovery_ts=0`

---

### TC-EG25-L1-02：L1 因 REG 未就绪被跳过

**目标**：验证 `nw_reg_status_check()` 返回 false 时 L1 仅更新 `last_l1_ts`，不执行重拨。

**前提**：
- `network_select=4`，已联网，`has_connected_once=1`
- 拔出或屏蔽 SIM 信号，使注册状态丢失

**步骤**：
1. 拔出 SIM 卡（同时导致 ping 失败和注册失败）
2. 等待 3 次 ping 失败触发 `start_fail_ts`
3. 等待 60s（L1 阈值）
4. 观察日志：`[RECOVERY L1] LastErr: ... | PDP: ...` 快照出现，但**后续无重拨操作**（因 REG 不存活，L1 路径进入 `last_l1_ts=now` 分支，不调用 `dail_stop/start_data_call`）
5. 等待 300s（L2 阈值），L2 应触发（L2 不检查 REG）

**验证**：
```bash
# L1 快照出现但无重拨（60s 内检查）
grep "RECOVERY L1" /media/sdcard/dial_log/dial_*.log

# L2 触发（等待 300s 后）
grep "RECOVERY L2\|CFUN" /media/sdcard/dial_log/dial_*.log
# 应依次出现：
# [RECOVERY L2] LastErr: ... | PDP: ...
# [CFUN] Sending AT+CFUN=0 (stop RF)
# [CFUN] AT+CFUN=0 success, response: OK
# [CFUN] Sending AT+CFUN=1 (start RF)
# [CFUN] AT+CFUN=1 success, response: OK
```

**预期**：
- L1 快照出现，但 60s 内 `dail_start_data_call` 不被调用
- 300s 后 `[RECOVERY L2] LastErr: ... | PDP: ...` 出现，`AT+CFUN=0` / `AT+CFUN=1` 执行

---

### TC-EG25-L1-03：SIM 断卡恢复时立即触发 L1

**目标**：验证 `sim_error_active=1` → SIM 恢复时跳过 60s 等待立即 L1。

**前提**：
- `network_select=4`，已联网，`has_connected_once=1`

**步骤**：
1. 确认正常联网
2. 拔出 SIM 卡，等待心跳检测到 SIM 断卡（`sim_ok=0`），此时 `sim_error_active=1`
3. **等待少于 60s**（比如 30s），重新插入 SIM 卡
4. 下一个 30s 心跳到来时，`sim_ok=1` 且 `sim_error_active=1` → 立即触发 L1

**预期日志**：
```
[RECOVERY] SIM recovered, forcing immediate L1 recovery (skip 60s wait).
[RECOVERY L1] LastErr: ... | PDP: ...
```

**验证**：L1 在 SIM 恢复后的第一个心跳（≤30s）即触发，而非等待 `downtime_sec >= 60s`。

---

### TC-EG25-L2-01：L2 CFUN 重置（5min 断网）

**目标**：验证 `downtime_sec >= 300s` 时执行 `AT+CFUN=0` + `AT+CFUN=1`。

**前提**：
- `network_select=4`，已联网，`has_connected_once=1`
- 屏蔽 ping（`iptables -I OUTPUT -p icmp -j DROP`）且 REG 状态不定（L2 不检查 REG）

**步骤**：
1. 屏蔽 ping，等待 `start_fail_ts` 设置（3 次 ping 失败，约 60s）
2. 等待约 300s（L2 阈值）
3. 期间 L1 会先于 L2 触发（60s + 60s 节流），观察：
   ```
   [RECOVERY L1] LastErr: ... | PDP: ...
   ```
4. 300s 后，L2 触发（`last_recovery_ts=0` 或达到节流间隔 300s）：
   ```
   [RECOVERY L2] LastErr: ... | PDP: ...
   ```
5. 随后观察 CFUN 日志：
   ```
   [CFUN] Sending AT+CFUN=0 (stop RF)
   [CFUN] AT+CFUN=0 success, response: OK
   [CFUN] Sending AT+CFUN=1 (start RF)
   [CFUN] AT+CFUN=1 success, response: OK
   ```
6. CFUN=1 后进程从 `dial_stat_sim_init` 重走，自动重新联网

**验证**：
```bash
grep "RECOVERY L2\|CFUN" /media/sdcard/dial_log/dial_*.log
```

**时序说明**：L2 执行后 `last_recovery_ts = last_l1_ts = now`，300s 内不再触发 L2（即使断网持续）。

---

### TC-EG25-L2-02：L2 节流验证（网络恢复后 last_recovery_ts 清零）

**目标**：验证网络恢复时 `last_recovery_ts=0`，下次断网满 300s 后 L2 正常重新触发（不提前）。

**步骤**：
1. 断网 → L2 触发（记录时间 T1）
2. 解除 iptables，等待网络恢复
3. 观察恢复日志：`[HEARTBEAT] Network recovered after XXs`（此时 `last_recovery_ts=0`，`last_l1_ts=0`）
4. 再次断网（记录时间 T2）
5. 等待 `start_fail_ts` 设定后约 300s，L2 再次触发（时间 T3）

**验证**：T3 - T2（从第二次断网开始）应 ≈ 3 次 ping 失败时间 + 300s L2 阈值。  
**反例**：修复前 `last_recovery_ts` 未清零，第二次断网可能仅 3.7min 就触发 L2。

---

### TC-EG25-L3-01：L3 硬重启（30min 断网）

**目标**：验证 `downtime_sec >= 1800s` 时进程打印 FATAL 日志并 `exit(1)`。

**前提**：
- `network_select=4`，`has_connected_once=1`，断网后维持到 30min

**步骤**：
1. 屏蔽 ping，确认 `start_fail_ts` 设置
2. 等待 30min（期间 L1/L2 会多次触发）
3. 约 30min 后，L3 触发：
   ```
   [RECOVERY L3] FATAL: Network down 30mins. Sending AT+CFUN=1,1 then exit.
   [RECOVERY L3] Exiting. will reinitialize.
   ```
4. `sync()` 执行，`AT+CFUN=1,1`（timeout 15s），`sleep(20)`，`exit(1)`
5. 进程退出，等待 watchdog/进程守护重新拉起

**验证**：
```bash
grep "RECOVERY L3\|FATAL" /media/sdcard/dial_log/dial_*.log
# 确认进程已退出
ps aux | grep modem_mng
```

**注意**：L3 无节流保护（代码中无 `last_recovery_ts` 条件），一旦 `downtime_sec >= 1800` 每次 30s 心跳都触发，但 `exit(1)` 仅触发一次即退出。

---

### TC-EG25-L1-L3-04：`has_connected_once=0` 保护启动期

**目标**：验证从未 ping 通时 L1/L2/L3 不触发。

**前提**：
- 进程全新启动，启动时无网络（`test_can_ping_google()` 返回 false）
- `network_select=4`

**步骤**：
1. 确认 `has_connected_once=0`（观察：启动后日志无 `[HEARTBEAT] Network recovered`，无 L1/L2/L3）
2. 等待 5min（超过 L2 阈值）
3. 确认 L1/L2 **不触发**（即使 ping 一直失败，只要 `has_connected_once=0`）
4. 手动接通网络，ping 成功 → `has_connected_once=1`
5. 再次断网 → 满 60s 后 L1 正常触发

**验证**：启动后 5min 内日志中 **不出现** `RECOVERY L1/L2/L3`。

---

### TC-EG25-HC-01：进程重启时 `has_connected_once` 从实时 ping 结果初始化

**目标**：验证进程重启时若网络已通，`test_can_ping_google()` 在启动阶段立即返回 true，`has_connected_once=1` 立即生效，L1 在 60s 断网后正常触发。

**背景**：`has_connected_once` 是进程生命周期内的内存标志，重启后归零。但若启动时 ping 立即成功，`has_connected_once` 在第一次心跳前即置 1，避免重启后有不必要的 L1/L2/L3 保护期盲区。

**前提**：`network_select=4`，当前 SIM 网络通畅

**步骤**：
1. 确认网络正常联网：
   ```bash
   cat /tmp/dial_status | grep state   # net_connected
   ping -c 1 8.8.8.8 && echo "ping OK"
   ```
2. 停止当前 modem_mng 并立即重新启动（不断网，保持 SIM 连接）
3. 观察启动阶段日志：
   - 若初始化阶段 ping 成功，应快速出现：
     ```
     [HEARTBEAT] Network recovered after XXs
     ```
   - `has_connected_once` 置 1
4. 屏蔽 ping：
   ```bash
   iptables -I OUTPUT -p icmp -j DROP
   ```
5. 等待约 90s（3 次 ping 失败 ≈ 60s + LEVEL1=60s），观察 L1 触发：
   ```
   [HEARTBEAT] Ping failed 3 consecutive times, fault timer started
   [RECOVERY L1] LastErr: ... | PDP: ...
   ```
6. 恢复 iptables：
   ```bash
   iptables -D OUTPUT -p icmp -j DROP
   ```

**验证**：
```bash
# 重启后无需等待即有 has_connected_once=1 效果
grep "Network recovered\|RECOVERY L1" /media/sdcard/dial_log/dial_*.log | tail -5
```

**预期**：进程重启后网络已通时，L1 在 60s 断网后触发（而非被 `has_connected_once=0` 压制）。

---

## 五、Roamlink 启动与 Probe

---

### TC-EG25-RL-PROBE-01：Probe 在 `dial_mng_new()` 执行，policy 被覆盖

**目标**：验证 `roamlink_probe()` 在进程初始化时执行，RBMaster 缺失时强制 FORCE_SIM。

**前提**：`network_select=1`，`/opt/conf.ini` 和 `license.cer` 均就绪，删除 `/usrdata/roamlink/RBMaster`

**步骤**：
1. 写入策略配置：
   ```bash
   echo -e "[network]\nnetwork_select=1" > /usrdata/network.ini
   ```
2. 确认 conf.ini 和 license 已就绪，然后移除 RBMaster：
   ```bash
   mv /usrdata/roamlink/RBMaster /usrdata/roamlink/RBMaster.bak
   ```
3. 停止当前 modem_mng 并重新启动（使其重新执行 `dial_mng_new`）
4. 观察启动阶段日志，probe 在初始化时运行，应出现 NO_PACKAGE 相关日志
5. 等待 SIM 拨号完成（观察 `[SDK] DataCall connected`），确认走物理 SIM 路径
6. 等待 3min（超过 `ROAMLINK_SWITCH_TIMEOUT_SEC=180s`），确认 **不发生** Roamlink 切换
7. 验证状态文件：
   ```bash
   cat /tmp/dial_status | grep -E "policy|policy_name|roamlink_available|channel"
   # policy=4
   # policy_name=FORCE_SIM
   # roamlink_available=0
   # channel=SIM
   cat /tmp/network_type   # → 1
   ```
8. 恢复 RBMaster：`mv /usrdata/roamlink/RBMaster.bak /usrdata/roamlink/RBMaster`

**验证**：
```bash
# 日志中不出现任何 Roamlink 切换
grep "switching to Roamlink\|Initial channel: Roamlink" /media/sdcard/dial_log/dial_*.log  # 无输出
# 状态文件 policy 被强制覆盖为 4
cat /tmp/dial_status | grep policy_name  # FORCE_SIM
```

---

### TC-EG25-RL-START-01：策略1/3 启动时直接进入 Roamlink 通道

**目标**：验证 `network_select=1` 或 `3` 启动时走 `dial_stat_roamlink_starting`。

**前提**：Roamlink 完整就绪（RBMaster、conf.ini、license.cer），`network_select=1`

**步骤**：
1. 启动 modem_mng
2. 观察启动阶段日志：
   ```
   [ROAMLINK] Policy requires Roamlink as initial channel. Starting RBMaster...
   [ROAMLINK] Initial channel: Roamlink (starting)
   ```
3. 状态文件验证：
   ```bash
   cat /tmp/dial_status | grep -E "state|channel|network_type"
   # state=roamlink_starting
   # channel=ROAMLINK
   cat /tmp/network_type   # → 2
   ```

**如果启动时 RBMaster 启动失败**（`roamlink_start_service()` 返回 false）：
```
[ROAMLINK] Initial start failed, falling back to SIM
```
此时走 SIM 拨号路径。

---

### TC-EG25-RL-START-02：策略2 启动时走物理 SIM

**目标**：验证 `PREFER_SIM`（策略2）启动时以物理 SIM 为主通道，不启动 Roamlink。

**前提**：Roamlink 组件完整就绪（RBMaster、conf.ini、license.cer 均存在）

**步骤**：
1. 写入策略配置：
   ```bash
   echo -e "[network]\nnetwork_select=2" > /usrdata/network.ini
   ```
2. 停止当前 modem_mng 并重新启动（使 `dial_mng_new()` 重读配置）
3. 观察启动阶段日志，确认**不出现**以下任何一行：
   ```
   Initial channel: Roamlink
   [ROAMLINK] Policy requires Roamlink as initial channel
   ```
4. 等待 SIM 完成注册和拨号，直到出现：
   ```
   [CELL] Init: ...
   [SDK] DataCall connected | profile=...
   ```
5. 验证状态文件：
   ```bash
   cat /tmp/dial_status | grep -E "state|policy_name|channel"
   # state=net_connected
   # policy_name=PREFER_SIM
   # channel=SIM
   cat /tmp/network_type   # → 1
   ```
6. 等待 300s（超过 `SIM_FALLBACK_RETRY_SEC`），SIM 稳定后**不触发** Roamlink 回切尝试  
   （策略2 的回切逻辑只在 Roamlink 备用→SIM 恢复时运行，冷启动 SIM 不设 `roamlink_fallback_ts`）

**验证**：
```bash
# 启动后日志中不出现 Roamlink 切换
grep "Initial channel: Roamlink\|switching to Roamlink" /media/sdcard/dial_log/dial_*.log  # 无输出
# 通道为 SIM
cat /tmp/network_type   # 1
```

**预期**：
- 启动时不出现 `Initial channel: Roamlink`
- 走 SIM 注册路径，最终 `state=net_connected`，`channel=SIM`
- `cat /tmp/network_type` → `1`

---

## 六、Roamlink 状态机：roamlink_starting 阶段

---

### TC-EG25-RL-ST-01：首次 ping 通 → 转入 roamlink_active

**目标**：验证 `starting` 状态下 ping 成功触发 `starting → active` 转移，初始化 rx_packets 基准。

**前提**：进入 `dial_stat_roamlink_starting`（TC-EG25-RL-START-01 成功后）

**步骤**：
1. 等待 Roamlink 通道 ping 通（首次）
2. 观察日志：
   ```
   [ROAMLINK] Ping OK in starting state, channel now active
   ```
3. 状态文件变化：
   ```bash
   cat /tmp/dial_status | grep -E "state|roamlink_rx_packets"
   # state=roamlink_active
   # roamlink_rx_packets=<非零值（初始基准）>
   ```
4. 心跳格式验证（此时为 Roamlink 通道）：
   ```
   [HEARTBEAT] CH:ROAMLINK | SIM:1 | CSQ:XX | Temp:XX | DownTime:0s | ConsecFail:0 | RL_FAIL:0 | RX_PKT:XXXXX
   ```
   注意：**Roamlink 通道时 REG 字段不打印**

---

### TC-EG25-RL-ST-02：starting 阶段超时 300s → 策略1/2 切回 SIM

**目标**：验证 `roamlink_start_ts` 起 300s（`ROAMLINK_CONNECT_TIMEOUT_SEC`）内未 ping 通，策略1/2 切 SIM。

**前提**：`network_select=1`，已进入 `roamlink_starting`，但阻断 Roamlink ping（如 `iptables -I OUTPUT -p icmp -j DROP` 仅限 Roamlink 流量）

**步骤**：
1. 进入 `starting` 状态后，不让 ping 通过
2. 等待 300s
3. 状态机 `case dial_stat_roamlink_starting:` 检测到超时，触发：
   ```
   [ROAMLINK] Connect timeout (300s), switching to SIM
   ```
4. `roamlink_stop_service()` 执行，`is_roamlink_active = false`，`dial_st = dial_stat_reg_check`，`sim_fallback_ts = now`

**验证**：
```bash
cat /tmp/network_type           # → 1
cat /tmp/dial_status | grep state  # → reg_check 或后续状态
grep "Connect timeout" /media/sdcard/dial_log/dial_*.log
```

---

### TC-EG25-RL-ST-03：starting 阶段超时 300s → 策略3（FORCE_ROAMLINK）重启服务

**前提**：`network_select=3`，进入 `starting` 状态，阻断 ping

**步骤**：
1. 等待 300s 超时
2. 观察日志：
   ```
   [ROAMLINK] FORCE_ROAMLINK: connect timeout, restarting service
   ```
3. `roamlink_stop_service()` + sleep(5) + `roamlink_start_master()` + `roamlink_start_service()`，`roamlink_start_ts = cur_timer`（重置，留在 starting）

**验证**：
- `cat /tmp/network_type` 始终 `= 2`
- `cat /tmp/dial_status | grep state` 保持 `roamlink_starting`
- **不出现** `switching to SIM`

---

## 七、Roamlink 状态机：roamlink_active 阶段

---

### TC-EG25-RL-ACT-01：active 阶段 ping 失败 300s → 策略1/2 切回 SIM

**目标**：验证 `roamlink_last_ping_ts` 起 300s（`ROAMLINK_CONNECT_TIMEOUT_SEC`）无 ping 成功，切回 SIM。

**前提**：已进入 `roamlink_active`（TC-EG25-RL-ST-01 成功），`network_select=1`

**步骤**：
1. 屏蔽 ping：`iptables -I OUTPUT -p icmp -j DROP`
2. 等待 3 次连续 ping 失败 → `start_fail_ts = now`，同时 `roamlink_last_ping_ts` 停止更新
3. 继续等待，直到 `cur_timer - roamlink_last_ping_ts > 300s`
4. 状态机 `case dial_stat_roamlink_active:` 触发：
   ```
   [ROAMLINK] Channel fail timeout (300s), switching to SIM
   ```
5. `roamlink_stop_service()`，`dial_st = reg_check`，`is_roamlink_active = false`，`sim_fallback_ts = time(NULL)`

**验证**：
```bash
cat /tmp/network_type              # → 1
cat /tmp/dial_status | grep state  # → reg_check 或后续
```

---

### TC-EG25-RL-ACT-02：active 阶段超时 → 策略3（FORCE_ROAMLINK）重启回到 starting

**前提**：`network_select=3`，已进入 `roamlink_active`，屏蔽 ping

**步骤**：
1. 等待 `roamlink_last_ping_ts` 起 300s 超时
2. 观察日志：
   ```
   [ROAMLINK] FORCE_ROAMLINK: channel timeout, restarting service
   ```
3. `roamlink_stop_service()` + sleep(5) + `dial_st = roamlink_starting` + 重启服务

**验证**：
- `cat /tmp/network_type` 始终 `= 2`
- `cat /tmp/dial_status | grep state` 从 `roamlink_active` 变回 `roamlink_starting`
- **不出现** `switching to SIM`

---

### TC-EG25-RL-ACT-03：L1/L2/L3 在 roamlink_active 状态下被屏蔽

**目标**：验证 `dial_st == roamlink_active` 时 L1/L2/L3 分支不进入（代码条件：`dial_st != roamlink_starting && != roamlink_active`）。

**前提**：`network_select=3`（FORCE_ROAMLINK），已进入 `roamlink_active`，屏蔽 ping

**步骤**：
1. 屏蔽 ping，等待 `start_fail_ts` 设置
2. 等待 60s（L1 阈值）、300s（L2 阈值）
3. 观察日志：**不出现** `RECOVERY L1/L2`
4. 故障处理只通过 `case roamlink_active:` 中的 ping 层超时（300s）触发

**验证**：日志中 `RECOVERY L1` 和 `RECOVERY L2` 不出现；`AT+CFUN` 不被调用。

---

## 八、业务层 rx_packets 监控（仅 roamlink_active 状态）

---

### TC-EG25-BIZ-01：rx_packets 无增长 120s → 策略1/2 切 SIM

**目标**：验证 `roamlink_no_data_timer` 起 120s 内 rx_packets 无增长时，策略1/2 切回物理 SIM。

**背景**：ping 层可达不代表业务数据流通；Roamlink 虚拟 SIM 注册失败时 `rmnet_data*` 的 rx_packets 不会增长。

**前提**：`network_select=1` 或 `2`，已进入 `dial_stat_roamlink_active`，`roamlink_no_data_timer` 已初始化

**步骤**：
1. 进入 `roamlink_active` 状态后，让 ping 正常（保持 ICMP 可达），但屏蔽 `rmnet_data0` 入站：
   ```bash
   iptables -I INPUT -i rmnet_data0 -j DROP
   ```
2. 每 30s 心跳日志出现无增长警告：
   ```
   [ROAMLINK] Biz rx_packets no growth for XXs (threshold 120s)
   ```
3. 等待 `roamlink_no_data_timer` 起 120s，状态机 `case roamlink_active:` 中业务层超时触发：
   ```
   [ROAMLINK] Biz layer no data for XXs, switching to SIM
   ```
4. `roamlink_stop_service()`，`dial_st = reg_check`，`is_roamlink_active = false`，`sim_fallback_ts = time(NULL)`

**验证**：
```bash
cat /tmp/network_type   # → 1
grep "Biz layer no data" /media/sdcard/dial_log/dial_*.log
```

---

### TC-EG25-BIZ-02：rx_packets 无增长 120s → 策略3 重启服务

**目标**：验证 `FORCE_ROAMLINK`（策略3）下业务层 120s 无 rx_packets 增长时重启 Roamlink 服务，不回落 SIM。

**前提**：`network_select=3`，已进入 `dial_stat_roamlink_active`（TC-EG25-RL-ST-01 成功）

**步骤**：
1. 写入策略配置：
   ```bash
   echo -e "[network]\nnetwork_select=3" > /usrdata/network.ini
   ```
2. 确认当前状态为 `roamlink_active`：
   ```bash
   cat /tmp/dial_status | grep state   # roamlink_active
   cat /tmp/network_type               # 2
   ```
3. 保持 ping 可达（不屏蔽 ICMP），但屏蔽 `rmnet_data0` 入站流量：
   ```bash
   iptables -I INPUT -i rmnet_data0 -j DROP
   ```
4. 等待约 30s，观察心跳日志开始出现无增长警告：
   ```
   [ROAMLINK] Biz rx_packets no growth for XXs (threshold 120s)
   ```
5. 等待满 120s（`ROAMLINK_NO_DATA_TIMEOUT_SEC`），状态机触发：
   ```
   [ROAMLINK] FORCE_ROAMLINK: biz no data, restarting service
   ```
6. 随后 `roamlink_stop_service()` + sleep(5) + 重启 RBMaster + 服务，状态回到 `roamlink_starting`
7. 恢复 iptables：
   ```bash
   iptables -D INPUT -i rmnet_data0 -j DROP
   ```

**验证**：
```bash
grep "FORCE_ROAMLINK: biz no data" /media/sdcard/dial_log/dial_*.log
cat /tmp/network_type               # 始终为 2（不回落 SIM）
cat /tmp/dial_status | grep state   # roamlink_starting（重启后）
```

**预期**：
- `cat /tmp/network_type` 始终 `= 2`
- `dial_st` 从 `roamlink_active` 回到 `roamlink_starting`

---

### TC-EG25-BIZ-03：rx_packets 正常增长时不误触发

**前提**：`network_select=1`，已进入 `roamlink_active`，Roamlink 正常有数据流量

**步骤**：
1. 确认有实际数据流量（如运行持续 ping 或下载测试）
2. 每 30s 心跳：`roamlink_no_data_timer` 被刷新，**不出现** `Biz rx_packets no growth` 日志
3. 运行 300s，通道保持 `roamlink_active`

**验证**：
```bash
grep "Biz rx_packets no growth" /media/sdcard/dial_log/dial_*.log   # 无输出
cat /tmp/dial_status | grep state   # roamlink_active
```

---

### TC-EG25-BIZ-04：rx_packets 基准在 starting→active 转移时正确初始化

**目标**：验证首次 ping 通时 `roamlink_rx_packets` 等于当时的实际 rx_packets 总和。

**步骤**：
1. 进入 `roamlink_starting` 前，记录：
   ```bash
   cat /sys/devices/virtual/net/rmnet_data*/statistics/rx_packets
   ```
   记为 `V_before`
2. Roamlink 首次 ping 通，观察日志 `[ROAMLINK] Ping OK in starting state, channel now active`
3. 等待下一个 30s 心跳更新状态文件
4. 读取：
   ```bash
   cat /tmp/dial_status | grep roamlink_rx_packets
   ```
   应 ≥ `V_before`（即 `nw_get_rmnet_rx_packets_sum()` 读取时的实际值）
5. 产生流量后，再次读取，值应递增

---

### TC-EG25-BIZ-05：SIM 通道下不触发业务层监控

**目标**：验证业务层 rx_packets 监控只在 `dial_stat_roamlink_active` 下激活，SIM 通道（`net_connected`）下即使无流量也不误触发。

**前提**：`network_select=4`（FORCE_SIM），SIM 已联网（`state=net_connected`）

**步骤**：
1. 确认当前为 SIM 通道：
   ```bash
   cat /tmp/dial_status | grep -E "state|channel"   # state=net_connected, channel=SIM
   cat /tmp/network_type                             # 1
   ```
2. 屏蔽 `rmnet_data0` 入站，使 rx_packets 停止增长：
   ```bash
   iptables -I INPUT -i rmnet_data0 -j DROP
   ```
3. 等待 150s（超过 `ROAMLINK_NO_DATA_TIMEOUT_SEC=120s`）
4. 观察日志，确认 **不出现** 任何以下内容：
   ```
   Biz rx_packets no growth
   FORCE_ROAMLINK: biz no data
   Biz layer no data
   ```
5. 恢复 iptables：
   ```bash
   iptables -D INPUT -i rmnet_data0 -j DROP
   ```
6. 确认状态维持 `net_connected`，未发生通道切换

**验证**：
```bash
grep "Biz rx_packets no growth\|Biz layer no data" /media/sdcard/dial_log/dial_*.log   # 无输出
cat /tmp/dial_status | grep state   # net_connected（未变化）
cat /tmp/network_type               # 1（未切换）
```

**预期**：
- 日志中 **不出现** `Biz rx_packets no growth`
- 即使 `rmnet_data*` 接口无流量，也不触发切换（业务层监控仅在 `dial_stat_roamlink_active` 下激活）

---

## 九、双向回切逻辑

---

### TC-EG25-RL-BACK-01：策略1 SIM 备用 300s 后自动回切 Roamlink

**场景**：`PREFER_ROAMLINK`（策略1），Roamlink 失败切到 SIM 备用后，SIM 稳定 300s（`SIM_FALLBACK_RETRY_SEC`）触发回切。

**前提**：TC-EG25-RL-ST-02 完成（Roamlink 超时 → 已切回 SIM，`sim_fallback_ts = now`），`network_select=1`

**步骤**：
1. SIM 拨号成功，正常联网（`dial_st = net_connected`）
2. 等待 300s（SIM 备用稳定时间）
3. 下一个 ping 成功的心跳到来，检测到 `(now - sim_fallback_ts) >= 300s`，触发回切：
   ```
   [ROAMLINK] Policy1: SIM stable XXs, trying Roamlink recovery
   ```
4. 启动 RBMaster + service，成功则：
   ```
   [ROAMLINK] Switched back to Roamlink channel (starting)
   ```
   失败则：
   ```
   [ROAMLINK] Switch back failed, retry in 300s
   ```
   （`roamlink_fail_count++`，`sim_fallback_ts = now` 重置，300s 后再试）

**验证**：
```bash
cat /tmp/network_type              # → 2（回切成功后）
cat /tmp/dial_status | grep state  # → roamlink_starting 或 roamlink_active
```

---

### TC-EG25-RL-BACK-02：策略2 Roamlink 备用 300s 后自动回切 SIM

**场景**：`PREFER_SIM`（策略2），SIM 失败切到 Roamlink 备用后，Roamlink 稳定 300s（`ROAMLINK_FALLBACK_RETRY_SEC`）触发回切。

**前提**：已进入 `roamlink_active` 备用状态（`roamlink_fallback_ts` 已设置），`network_select=2`

**步骤**：
1. Roamlink 正常 ping 通（`dial_st = roamlink_active`）
2. 等待 300s
3. 下一次 ping 成功心跳，检测到 `(now - roamlink_fallback_ts) >= 300s`：
   ```
   [ROAMLINK] Policy2: Roamlink stable XXs, trying SIM recovery
   ```
4. `roamlink_stop_service()`，`dial_st = reg_check`，`is_roamlink_active = false`：
   ```
   [ROAMLINK] Switched back to SIM channel
   ```
5. SIM 重新注册拨号，重新联网

**验证**：
```bash
cat /tmp/network_type              # → 1
cat /tmp/dial_status | grep state  # → net_connected（SIM 联网后）
```

---

### TC-EG25-SIM-INIT-01：Roamlink→SIM 回落时 `sim_initialized` 跳过重复 init

**目标**：验证 Roamlink 回落到物理 SIM 时，因 `sim_initialized=true`，状态机直接从 `dial_stat_reg_check` 进入（跳过 `dial_stat_sim_init`），不重复调用 `QL_MCM_NW_Client_Init`，且 `[MODEM]` 启动诊断日志不再打印。

**背景**：`QL_MCM_NW_Client_Init` 重复调用会导致资源泄漏。`sim_initialized` 标志确保 Roamlink→SIM 切换时直接进 `reg_check` 而非 `sim_init`。

**前提**：`network_select=1` 或 `2`，已进入 `roamlink_active`，准备触发 Roamlink 超时回落 SIM（TC-EG25-RL-ACT-01）

**步骤**：
1. 确认当前为 `roamlink_active`，此时 `sim_initialized=true`（SIM 在 Roamlink 启动前已初始化）
2. 触发 Roamlink ping 层 300s 超时（参考 TC-EG25-RL-ACT-01），状态切回 SIM：
   ```
   [ROAMLINK] Channel fail timeout (300s), switching to SIM
   ```
3. 观察切回 SIM 后的状态机流转：
   - 状态应直接出现 `reg_check`（或后续 `cereg_check`），**不出现** `sim_init` 阶段
4. 明确验证 **不再打印** `[MODEM]` 开头的启动诊断日志（如 `[MODEM] Model: EG25-G`）：
   ```bash
   # 记录 Roamlink 回落时间戳后 grep
   grep "\[MODEM\] Model" /media/sdcard/dial_log/dial_*.log | tail -3
   # 最后一条应出现在进程启动阶段，不出现在回落后
   ```
5. SIM 完成注册和拨号，进入 `net_connected`

**验证**：
```bash
# Roamlink 回落后不出现 sim_init 阶段日志
grep "sim_init\|\[MODEM\] Model" /media/sdcard/dial_log/dial_*.log | tail -5
# 直接出现 reg_check 进程
grep "reg_check\|CEREG" /media/sdcard/dial_log/dial_*.log | tail -5
cat /tmp/network_type   # → 1（SIM 联网后）
```

---

## 十、SIM→Roamlink 主动切换（心跳驱动）

---

### TC-EG25-RL-SWITCH-01：SIM 断网 180s → 策略1/2 切 Roamlink

**目标**：验证心跳中 `downtime_sec >= ROAMLINK_SWITCH_TIMEOUT_SEC=180s` 时发起切换。

**前提**：`network_select=2`，Roamlink 完整就绪，SIM 已联网

**步骤**：
1. 屏蔽 ping（`iptables -I OUTPUT -p icmp -j DROP`）
2. 等待 3 次 ping 失败设置 `start_fail_ts`（约 60s 内）
3. 继续等待，`downtime_sec >= 180s`，触发切换：
   ```
   [ROAMLINK] policy=2: SIM down XXs, switching to Roamlink
   ```
4. 切换成功：
   ```
   [ROAMLINK] Switched to Roamlink channel (starting)
   ```
   `dial_st = roamlink_starting`，`is_roamlink_active = true`，`start_fail_ts = 0`

**验证**：
```bash
cat /tmp/network_type   # → 2
```

---

### TC-EG25-RL-SWITCH-02：切换失败时 `roamlink_fail_count` 递增并重置 `start_fail_ts`

**前提**：同上，但 `roamlink_start_service()` 失败（如阻断控制端口 5568）

**步骤**：
1. 同 TC-EG25-RL-SWITCH-01 条件，但 Roamlink 服务无法正常启动
2. 触发切换尝试时观察：
   ```
   [ROAMLINK] policy=2: SIM down XXs, switching to Roamlink
   [ROAMLINK] Switch failed
   ```
3. `roamlink_fail_count++`，`start_fail_ts = now`（重置为当前时刻）
4. 180s 后 `downtime_sec` 再次 ≥ 180s，再次尝试切换

**验证**：
```bash
cat /tmp/dial_status | grep roamlink_fail_count  # 递增
cat /tmp/network_type                            # 仍为 1
```

---

### TC-EG25-RL-SWITCH-03：策略3 初始启动失败→SIM 备用→SIM 断网 180s 再切回 Roamlink

**目标**：验证 `FORCE_ROAMLINK`（策略3）在初始 Roamlink 启动失败、临时走 SIM 备用后，SIM 断网超过 `ROAMLINK_SWITCH_TIMEOUT_SEC=180s` 时重新触发切换回 Roamlink。

**前提**：`network_select=3`，Roamlink 组件就绪但**首次启动会失败**（如临时阻断控制端口 5568）

**步骤**：
1. 写入策略配置：
   ```bash
   echo -e "[network]\nnetwork_select=3" > /usrdata/network.ini
   ```
2. 阻断 Roamlink 控制端口，使初始启动失败：
   ```bash
   iptables -I INPUT -p tcp --dport 5568 -j DROP
   ```
3. 停止并重新启动 modem_mng，观察日志：
   ```
   [ROAMLINK] Initial start failed, falling back to SIM
   ```
4. SIM 完成注册拨号，进入 `net_connected`（`network_type=1`）
5. 恢复 Roamlink 控制端口（使后续切换可以成功）：
   ```bash
   iptables -D INPUT -p tcp --dport 5568 -j DROP
   ```
6. 屏蔽 SIM ping：
   ```bash
   iptables -I OUTPUT -p icmp -j DROP
   ```
7. 等待 3 次 ping 失败设置 `start_fail_ts`，继续等待 `downtime_sec >= 180s`，触发切换：
   ```
   [ROAMLINK] policy=3: SIM down XXs, switching to Roamlink
   ```
8. 恢复 iptables：
   ```bash
   iptables -D OUTPUT -p icmp -j DROP
   ```

**验证**：
```bash
grep "Initial start failed\|policy=3: SIM down\|switching to Roamlink" /media/sdcard/dial_log/dial_*.log
cat /tmp/network_type   # → 2（切换成功后）
```

---

### TC-EG25-RL-FAILCNT：`roamlink_fail_count` 累计计数单调递增

**目标**：验证每次 Roamlink 切换失败（`switch failed`）时 `roamlink_fail_count` 递增，多轮失败后单调累加，重启后归零。

**前提**：`network_select=2`，Roamlink 服务反复启动失败（持续阻断控制端口 5568）

**步骤**：
1. 写入策略配置：
   ```bash
   echo -e "[network]\nnetwork_select=2" > /usrdata/network.ini
   ```
2. 阻断 Roamlink 控制端口：
   ```bash
   iptables -I INPUT -p tcp --dport 5568 -j DROP
   ```
3. 触发第一次切换失败（TC-EG25-RL-SWITCH-02 方式：SIM 断网 180s）：
   ```bash
   iptables -I OUTPUT -p icmp -j DROP
   ```
   等待切换尝试并失败：
   ```
   [ROAMLINK] Switch failed
   ```
4. 恢复 ping，等待 SIM 恢复联网，读取计数：
   ```bash
   iptables -D OUTPUT -p icmp -j DROP
   cat /tmp/dial_status | grep roamlink_fail_count  # 应为 1
   ```
5. 重复步骤 3-4 共 3 次（累计 3 轮失败）
6. 每轮读取 `roamlink_fail_count`，确认单调递增（1 → 2 → 3）
7. 重启 modem_mng，读取 `roamlink_fail_count`：
   ```bash
   cat /tmp/dial_status | grep roamlink_fail_count  # 应为 0（重启归零）
   ```
8. 恢复 iptables：
   ```bash
   iptables -D INPUT -p tcp --dport 5568 -j DROP
   ```

**验证**：
```bash
# 每轮失败后递增
cat /tmp/dial_status | grep roamlink_fail_count
# 重启后归零
```

---

## 十一、注册超时处理（`reg_timeout_handler`）

---

### TC-EG25-REG-01：注册超时 + 策略1/2 + Roamlink 可用 → 切 Roamlink

**目标**：验证 `reg_check` 或 `cereg_check` 超过 `REG_CHECK_TIMEOUT_SECONDS=300s` 后触发 `goto reg_timeout_handler`，策略1/2 切 Roamlink。

**前提**：`network_select=1` 或 `2`，Roamlink 完整就绪，`license_pending=false`，物理 SIM 注册失败（如禁卡或信号极差，CEREG=3 持续不通过）

**步骤**：
1. 进入 `dial_stat_reg_check`（可通过拔 SIM 再插回，或信号极差时触发）
2. `reg_check` / `cereg_check` 每次失败，`dial_timer` 计时
3. 超过 300s 后触发 `goto reg_timeout_handler`：
   ```
   [REG TIMEOUT] policy=X, switching to Roamlink
   ```
4. `dail_stop_data_call(profile_idx)` 清理悬挂呼叫
5. `roamlink_start_master()` + `roamlink_start_service()`
6. 成功：`dial_st = roamlink_starting`，`is_roamlink_active = true`，`dial_timer` 重置

**验证**：
```bash
grep "REG TIMEOUT" /media/sdcard/dial_log/dial_*.log
cat /tmp/network_type   # → 2
```

---

### TC-EG25-REG-02：注册超时 + 策略4 → CFUN 重置重走

**前提**：`network_select=4`，SIM 注册超时

**步骤**：
1. 同 TC-EG25-REG-01，等待 300s 超时
2. 触发 `goto reg_timeout_handler`：
   ```
   [REG TIMEOUT] policy=4, cfun reset and retry
   ```
3. `dial_st = dial_stat_stop_cfun`，随后：
   ```
   [CFUN] Sending AT+CFUN=0 (stop RF)
   [CFUN] AT+CFUN=0 success, response: OK
   [CFUN] Sending AT+CFUN=1 (start RF)
   [CFUN] AT+CFUN=1 success, response: OK
   ```
4. `dial_stat_start_cfun` 成功后进入 `dial_stat_sim_init`，重走完整拨号流程

**验证**：
```bash
grep "REG TIMEOUT\|CFUN" /media/sdcard/dial_log/dial_*.log
```

---

### TC-EG25-REG-03：Roamlink 激活期间物理 SIM 注册失败不触发 reg_timeout_handler

**目标**：验证 `roamlink_starting`/`roamlink_active` 状态下状态机不处于 `reg_check`/`cereg_check`，故 `reg_timeout_handler` 路径天然不可达。

**前提**：`network_select=1`，已进入 `roamlink_starting` 或 `roamlink_active`，物理 SIM 因 RBMaster 占用模组通道而注册失败

**步骤**：
1. 确认 `cat /tmp/dial_status | grep state` 为 `roamlink_starting` 或 `roamlink_active`
2. 等待 300s（`REG_CHECK_TIMEOUT_SECONDS`）
3. 观察日志

**预期**：日志中 **不出现** `[REG TIMEOUT]`，状态保持 `roamlink_starting`/`roamlink_active`，Roamlink 心跳正常。

---

## 十二、SIM 通道 net_connected 状态下的 rx_packets 超时重拨

---

### TC-EG25-ND-01：`net_connected` 状态 120s rx_packets 无增长 → 跳回 reg_check

**目标**：验证 `DIAL_TIMEOUT_SECONDS=120s` 无 `rmnet_data*` 新增 rx_packets 时，`dial_st` 从 `net_connected` 跳回 `reg_check`。

> 注意：此处用 `nw_get_rmnet_rx_packets_sum()`，而非旧版 `nw_get_if_statistics_rx_packets()`（后者使用悬垂指针 `p_ifaddrs->ifa_name`，已在 bf0ec6d3 中修复）。

**前提**：`network_select=4`，SIM 已联网（`dial_stat_net_connected`），PDP 连接保持，但屏蔽 rmnet_data0 入站

**步骤**：
1. 进入 `net_connected` 状态
2. `iptables -I INPUT -i rmnet_data0 -j DROP`
3. 等待 120s，`dial_timer` 超时：
   - 日志（内部）：`LOG_E("no data rx on net if, redial")`
   - `dial_st = reg_check`（重走注册→拨号流程）
4. 解除 iptables 后，PDP 重建，重新进入 `net_connected`

**验证**：
```bash
cat /tmp/dial_status | grep state   # 短暂离开 net_connected
```

**修复前的 bug 现象**（已修复）：旧版调用 `nw_get_if_statistics_rx_packets()` 传入 `p_ifaddrs->ifa_name`，而 `p_ifaddrs` 在 `nw_get_ifaddrs()` 内部被 `freeifaddrs()` 释放，导致读取失效，`rx_packets` 始终为 0，每隔 120s 就误判断为无增长触发重拨。

---

## 十三、心跳与诊断日志验证

---

### TC-EG25-HB-01：30s 心跳格式（SIM 通道）

**目标**：验证 SIM 通道下心跳每 30s 打印一次，字段含 `CH:SIM`、`REG`，断网时 `DownTime` 递增。

**前提**：`network_select=4`，SIM 已联网（`state=net_connected`）

**步骤**：
1. 确认 SIM 正常联网，等待至少 35s（保证至少一次心跳）
2. 查看心跳日志，格式应为：
   ```
   [HEARTBEAT] CH:SIM | SIM:1 | REG:1 | CSQ:22 | Temp:38 | DownTime:0s | ConsecFail:0
   ```
   若 `roamlink_available=1`，额外追加 `| RL_FAIL:0`：
   ```
   [HEARTBEAT] CH:SIM | SIM:1 | REG:1 | CSQ:22 | Temp:38 | DownTime:0s | ConsecFail:0 | RL_FAIL:0
   ```
3. 使用 iptables 屏蔽 ping，等待 60s，观察 `DownTime` 字段递增：
   ```bash
   iptables -I OUTPUT -p icmp -j DROP
   # 等待两个心跳周期
   ```
4. 观察连续两条心跳中 `DownTime:Xs` 的 X 值递增（间隔约 30s）
5. 恢复 iptables：
   ```bash
   iptables -D OUTPUT -p icmp -j DROP
   ```

**验证**：
```bash
grep "\[HEARTBEAT\]" /media/sdcard/dial_log/dial_*.log | tail -5
# 确认每隔约 30s 一条，CH:SIM，断网期间 DownTime 递增
```

**预期格式**：
```
[HEARTBEAT] CH:SIM | SIM:1 | REG:1 | CSQ:22 | Temp:38 | DownTime:0s | ConsecFail:0
```

当 `roamlink_available=1` 时追加：
```
[HEARTBEAT] CH:SIM | SIM:1 | REG:1 | CSQ:22 | Temp:38 | DownTime:0s | ConsecFail:0 | RL_FAIL:0
```

---

### TC-EG25-HB-02：30s 心跳格式（Roamlink 通道）

**目标**：验证 Roamlink 通道下心跳格式与 SIM 通道的差异：无 `REG` 字段，含 `RX_PKT` 字段。

**前提**：已进入 `dial_stat_roamlink_active`（TC-EG25-RL-ST-01 成功）

**步骤**：
1. 确认当前为 Roamlink 通道：
   ```bash
   cat /tmp/dial_status | grep state   # roamlink_active
   cat /tmp/network_type               # 2
   ```
2. 等待 35s，查看心跳日志，格式应为：
   ```
   [HEARTBEAT] CH:ROAMLINK | SIM:1 | CSQ:22 | Temp:38 | DownTime:0s | ConsecFail:0 | RL_FAIL:0 | RX_PKT:12345
   ```
3. 明确验证以下两点：
   - 日志中 **不含** `REG:` 字段（与 SIM 通道格式的关键区别）
   - `RX_PKT:` 字段存在，且值与 `/sys/devices/virtual/net/rmnet_data*/statistics/rx_packets` 总和对应
4. 产生少量流量后，等待下一个 30s 心跳，确认 `RX_PKT` 数值有所递增

**验证**：
```bash
grep "CH:ROAMLINK" /media/sdcard/dial_log/dial_*.log | tail -3
# 确认：无 REG 字段，含 RX_PKT 字段
grep "CH:ROAMLINK.*REG:" /media/sdcard/dial_log/dial_*.log   # 无输出
grep "CH:ROAMLINK.*RX_PKT:" /media/sdcard/dial_log/dial_*.log   # 有输出
```

**预期格式**：
```
[HEARTBEAT] CH:ROAMLINK | SIM:1 | CSQ:22 | Temp:38 | DownTime:0s | ConsecFail:0 | RL_FAIL:0 | RX_PKT:12345
```

**注意**：
- **REG 字段不打印**（Roamlink 通道时 REG 无诊断价值）
- `RL_FAIL` 字段仅在 `roamlink_available=true` 时追加
- `RX_PKT` 字段仅在 Roamlink 通道时追加

---

### TC-EG25-HB-03：5min 扩展心跳（首次联网后）

**目标**：验证 `has_connected_once=1` 后每 5min 一次扩展心跳，追加 RSRP/RSRQ/Cell/IP 字段。

**前提**：SIM 或 Roamlink 通道已联网（`has_connected_once` 在首次 ping 成功时置 1）

**步骤**：
1. 启动 modem_mng，等待首次 ping 成功。日志出现：
   ```
   [HEARTBEAT] Network recovered after XXs
   ```
   此时 `has_connected_once=1`
2. 记录当前时间，等待 5min（扩展心跳周期）
3. 等待扩展心跳，格式为在普通心跳行之后追加扩展字段：
   ```
   [HEARTBEAT] CH:SIM | SIM:1 | REG:1 | CSQ:22 | Temp:38 | DownTime:0s | ConsecFail:0 | RSRP:-95 RSRQ:-10 | Cell:1A2B3C4D | +CGPADDR: 10,"10.x.x.x"
   ```
4. 记录扩展心跳时间 T1，等待下一次扩展心跳 T2，验证 T2-T1 ≈ 300s（5min）
5. 确认普通 30s 心跳（无扩展字段）在两次扩展心跳之间正常出现（约 10 条）

**验证**：
```bash
# 过滤出含 RSRP 的扩展心跳行
grep "RSRP:" /media/sdcard/dial_log/dial_*.log | tail -3
# 确认两条之间间隔约 5min（观察时间戳）
# 同时确认普通心跳每 30s 一条
grep "\[HEARTBEAT\]" /media/sdcard/dial_log/dial_*.log | tail -20
```

**预期**：每 5min 在心跳后追加扩展字段：
```
[HEARTBEAT] CH:SIM | SIM:1 | REG:1 | ... | RSRP:-95 RSRQ:-10 | Cell:1A2B3C4D | +CGPADDR: 10,\"10.x.x.x\"
```

---

### TC-EG25-HB-04：故障首次确认时的 [DIAG] 快照

**目标**：验证 `start_fail_ts` 设置后的第一次心跳打印 `[DIAG]` 快照，且同一故障周期只打一次。

**步骤**：
1. 断网，等待 3 次 ping 失败（`start_fail_ts` 设置）
2. 下一次心跳打印：
   ```
   [DIAG] RSRP:-95 RSRQ:-10 | Cell:1A2B3C | +CGPADDR: ... | +QTEMP:38 | +CEER: ... | +CGACT: ...
   ```
3. 继续断网，下一个 30s 心跳：`[DIAG]` **不再重复**
4. 网络恢复后，`diag_snap_done` 重置，下次断网重新打印

---

### TC-EG25-HB-05：启动诊断日志（SDK 初始化完成后）

**目标**：验证 SDK 初始化成功后打印 `[MODEM]`、`[INIT]`、`[CELL]`、`[SDK]` 四类标签。

**步骤**：
1. 启动 modem_mng，观察初始化阶段日志：
   ```
   [MODEM] Model:   EG25-G
   [MODEM] FW:      EG25GGBR07A08M2G
   [MODEM] SubEd:   V06
   [MODEM] CFUN:    1 (full)
   [MODEM] Temp:    38
   ```
2. SIM 操作完成后：
   ```
   [INIT] ICCID: 8946028461XXXXXXXXXX
   [INIT] IMSI:  460028XXXXXXXXX
   [INIT] IMEI:  86XXXXXXXXXXXXX
   [INIT] PDP:   +CGDCONT: 1,"IP","cmnet",...
   ```
3. CEREG 注册成功后：
   ```
   [CELL] Init: +QENG: "servingcell","NOCONN","LTE","FDD",460,01,...
   ```
4. DataCall 连接成功时（回调中）：
   ```
   [SDK] DataCall connected | profile=1 | IP=10.x.x.x GW=10.x.x.1 DNS=...
   ```

---

### TC-EG25-HB-06：小区切换检测

**目标**：验证每 30s 心跳中 Cell ID 变化时打印 `[CELL CHANGE]`。

**步骤**：
1. 移动设备到不同基站覆盖区域（或使用切换工具）
2. 下一个 30s 心跳检测到 Cell ID 变化：
   ```
   [CELL CHANGE] 1A2B3C -> 4D5E6F | +QENG: "servingcell","NOCONN","LTE",...
   ```

---

## 附：关键代码路径与文件索引

| 功能 | 文件 | 关键行（commit `45126949` 基线） |
|---|---|---|
| L1/L2/L3 分级恢复 | `eg25/dial/dial.c` | ~1154~1213 |
| 心跳 ping / start_fail_ts | `eg25/dial/dial.c` | ~956~1073 |
| Roamlink starting case | `eg25/dial/dial.c` | ~1760~1798 |
| Roamlink active case（ping 层 + 业务层） | `eg25/dial/dial.c` | ~1806~1893 |
| ping OK → starting→active 转移 | `eg25/dial/dial.c` | ~988~1014 |
| SIM→Roamlink 主动切换 | `eg25/dial/dial.c` | ~1114~1138 |
| 策略1/2 回切逻辑 | `eg25/dial/dial.c` | ~1020~1063 |
| reg_timeout_handler | `eg25/dial/dial.c` | ~1903~1949 |
| nw_get_rmnet_rx_packets_sum | `eg25/nw/nw.c` | ~389~ |
| 心跳日志格式（CH/RL_FAIL/RX_PKT） | `eg25/dial/dial.c` | ~745~800 |
| 启动诊断日志（MODEM/INIT/CELL） | `eg25/dial/dial.c` | ~1289~1449 |
| dial_stat_enu 枚举 | `eg25/dial/dial.h` | 45~66 |
| 所有 Roamlink 超时常量 | `roamlink/roamlink.h` | 64~77 |
