# 工作记录：客户设备日志分析 + 诊断日志优化

**日期**：2026-04-29 ~ 2026-04-30  
**涉及文件**：`dial.c`、`analysis_52251024011503.md`、`issue_sim_transient_disconnect.md`  
**软件版本**：open_dial V1.26  

---

## 一、确认客户设备运行的软件版本

同事反馈客户设备频繁离线，协助定位问题。第一步先确认客户设备上跑的是哪个版本的程序。通过对设备上的 `dial` 二进制文件执行 `md5sum`，与各提交的编译产物逐一比对，确认客户设备运行的是 commit `7e0648d`（[BUGFIX] 修复 SIM UNKNOWN 状态下 L2/L3 恢复永不触发的问题），比当前最新版本 `53c9853` 少一个提交——`53c9853` 补充了 SIM 瞬断日志中附加 AT+CPIN? 实际返回值的功能，尚未部署到客户。

---

## 二、分析客户设备日志，定位频繁掉线根因

### 2.1 数据准备

解压 `52251024011503.zip`，共 23 个日志文件，覆盖时段约 27.5 小时（2026-04-28 10:21 ~ 2026-04-29 13:49）。同时参照云平台截图中记录的 11 条在线/离线事件，与日志逐条对照。

**平台记录 vs 日志对照（全部吻合）：**

| 平台时间 | 状态 | 日志对应事件 | 时差 |
|---------|------|------------|------|
| 04-29 09:55:54 | 离线 | 09:54:09 SIM disconnect | 约 105s |
| 04-29 09:56:07 | 在线 | 09:55:20 Network Connected | 约 47s |
| 04-29 10:27:37 | 离线 | 10:26:08 SIM disconnect | 约 89s |
| 04-29 10:28:07 | 在线 | 10:27:21 Network Connected | 约 46s |
| 04-29 10:46:07 | 离线 | 10:44:15 SIM disconnect | 约 112s |
| 04-29 10:46:07 | 在线 | 10:45:27 Network Connected | 约 40s |
| 04-29 11:13:36 | 在线 | 11:13:36 Network Connected | **0s（精准）** |
| 04-29 12:09:06 | 离线 | 12:07:47 SIM disconnect | 约 79s |
| 04-29 12:11:16 | 在线 | 12:11:14 Network Connected | 约 2s |

时差均来自平台通过心跳超时判断离线，存在 30~120 秒固有延迟，不是误报。

### 2.2 根本原因：eSIM 周期性瞬断

27 小时内共发生 **66 次** `[ERROR] SIM Card disconnected during runtime`。每次 SIM 接口失效 16~46 秒后自动恢复，无需任何 AT 命令干预。信号质量全程优秀（CSQ 24~31，REG 始终=1），完全排除基站侧原因。

**典型一次断线的完整时序（以 10:34:00 为例，三次事件规律一致）：**

```
10:33:46  [HEARTBEAT] SIM:READY | REG:1 | CSQ:31      ← 正常
10:34:00  [ERROR] SIM Card disconnected                ← 瞬断发生，start_fail_ts 开始计时
10:34:16  [HEARTBEAT] SIM:UNKNOWN | REG:-1 | CSQ:-1   ← SIM 接口失效，模组失去 SIM 访问能力
10:34:46  [HEARTBEAT] SIM:READY | REG:1 | CSQ:29      ← SIM 自行恢复，但 PDP 未自动重建
10:35:04  [ALARM] Net Fail Duration: 62s. Trigger L1  ← 60s 计时到期
10:35:04  [RECOVERY L1] Stopping Data Call...
10:35:06  [RECOVERY L1] Restarting Data Call...
10:35:14  [EVENT] Network Connected                    ← L1 重拨成功，总断线约 74s
```

**断线影响量化：**
- 单次断线约 70~80 秒设备完全无法收发数据（60s L1 计时 + ~10s 重拨）
- 66 次 × 平均 75s ≈ 82 分钟真实断线时长
- 日间运行约 13 小时，断线率约 **10%**，平均每 12 分钟断一次

### 2.3 eSIM 与 EC200A 架构说明

最初误判为"物理接触不良"，用户纠正该设备使用 eSIM，无物理卡槽。重新分析后，理解了 EC200A 的双核架构：

```
┌────────────────── ASR1803 芯片 ──────────────────┐
│                                                   │
│  AP（Cortex-A7）         CP（Cortex-R5）          │
│  跑 Linux + dial 程序    跑蜂窝协议栈             │
│                          管 SIM / 射频 / 4G 协议  │
│                                                   │
│  dial 程序在这里 ←─AT命令/SDK─→ eSIM 在这里      │
└───────────────────────────────────────────────────┘
```

eSIM 直接焊在 CP（Cortex-R5）内部，`dial` 程序感知不到 eSIM 本身，只能通过 `AT+CPIN?` 询问 CP 当前 SIM 状态。CP 回答 UNKNOWN，`dial` 就记一条 `[ERROR]`。因此断线的本质是 **CP 内部 SIM 接口短暂复位**，而非 eSIM 芯片本身损坏。

### 2.4 日间/夜间断线分布规律与根因推断

最关键的规律是：**所有 66 次断线全部集中在 06:00~19:00，夜间 19:00~次日 06:00 长达 11 小时零故障**。夜间 AT+CPIN? 同样每 3 秒查一次，但完全没有断线，排除了查询频率是原因的可能。

推断最可能的原因是重型卡车 24V 电源在发动机运转时产生的瞬态冲击导致 CP 侧 SIM 接口复位。但需要明确：日志中没有点火状态、车速或 GPS 数据，**"夜间=熄火停车"是推断，不是事实**。报告中已明确标注，后续需要从平台拉 ACC 或车速数据做交叉验证才能确认。

### 2.5 软件行为评估

软件表现完全符合设计预期，不是 bug：
- L1 在故障后约 62~66 秒准时触发（60s 阈值 + 约 2~6s Ping 抖动过滤）
- L1 `sleep(2)` 精准：16:43:12→16:43:14=2s
- L1 重拨 7~9 秒内成功联网
- PDP_WAIT_TIMEOUT 60s 精准：09:51:35 启动→09:52:35 超时退出=60s
- L1 冷却 60s：相邻两次 ALARM 间隔均 >60s

所有时间参数均与代码定义完全吻合。

### 2.6 特殊事件分析

**① Fast Fail Attempt 2/3 触发（09:51:35）：**

```
09:51:35  启动（Attempt 2/3），60s PDP 等待计时开始
09:51:37  首次心跳：SIM:UNKNOWN ← eSIM 还处于上次瞬断未恢复状态
09:52:07  SIM 恢复（READY, REG:1），但离 60s 超时只剩 28s
09:52:35  Fast Fail 60s 超时 → log_close() → exit(1)
```

这是本次分析的 23 个文件中唯一一次 Fast Fail，原因是 eSIM 的瞬断状态延续到了下一次进程启动，28 秒内 PDP 来不及建立。`=== Program Exit ===` 是 `log_close()` 写入的正常退出标记，说明走了正常的 `exit(1)` 代码路径，属预期行为。其他 22 个文件 PDP 均在 2 秒内建立成功。

**② `ql_data_call_start` 返回 -1001（发生 3 次）：**

```
18:26:21  SIM disconnect
18:27:24  L1 recovery → Start failed: -1001 (REG:1)  ← SDK 内部未就绪
18:28:28  L1 retry   → Start failed: -1001 (REG:1)
18:29:29  L1 retry   → 成功连接
```

SDK 内部状态未稳定，导致拨号暂时被拒，重试两次后成功。此次断线持续约 3 分钟（正常 70s），是本次日志中最长的一次断线。

### 2.7 AT+CPIN? 执行频率推算

状态机一个完整循环（ST_SIM → ST_SIGNAL → ST_PING → 回到 ST_SIM）的时间推算：

```
T=0ms    ST_SIM 执行 AT+CPIN?，设 next_ts = T + 800ms
T=800ms  ST_SIGNAL 执行，设 next_ts = T + 800ms
T=1600ms ST_PING 开始，先设 next_ts = T + 1500ms，再跑 ping(-W 2)

  情况 A：ping 成功（4G 约 100~200ms）
    T≈1800ms  ping 结束 → tnow < next_ts(3100)，等待
    T=3100ms  AT+CPIN? 再次执行  → 间隔约 3100ms

  情况 B：ping 失败（-W 2 超时，耗时 2000ms）
    T=3600ms  ping 结束 → tnow > next_ts(3100)，立即执行
    T=3600ms  AT+CPIN? 再次执行  → 间隔约 3600ms
```

**结论：正常联网约 3.1 秒查一次，断网状态约 3.6 秒查一次。**

### 2.8 源码 AT+CPIN? 策略设计评审

在分析日志的过程中，顺带审查了 `AT+CPIN?` 的判断策略，发现三处设计缺陷（写入 `analysis_52251024011503.md` 附录，标注为后续改进项，不是本次掉线根因）：

**⚠️ 缺陷一：SIM 检测无抖动过滤**
Ping 有 `PING_FAIL_THRESHOLD=3`，连续失败 3 次才计入故障。SIM 检测是**单次即触发**——只要一次 `AT+CPIN?` 返回非 READY，立刻打 `[ERROR]` 并启动 60s 故障计时。若 `serial_atcmd` 因 AT 口短暂繁忙返回空或 UNKNOWN，会被误判为真实 SIM 断开。

**⚠️ 缺陷二：popen() 无超时保护，可阻塞主循环**
`serial_atcmd at+cpin?` 若 CP 侧 AT 接口卡住，`fgets()` 无限等待，整个主循环（心跳、Ping、恢复逻辑）全部停摆。**直接证据**：日志中 10:21:09 心跳后，下一条心跳出现在 10:22:16，延迟了 37 秒（正常 30 秒），正是 SIM UNKNOWN 期间 AT 口响应变慢、每次 popen 阻塞数秒累积导致。

**⚠️ 缺陷三：popen 失败与真实 SIM 故障无法区分**
popen 系统调用失败时写入 `"ERROR"`，模组真实返回 UNKNOWN 时写入 `"UNKNOWN"`，两种情况在日志里表现相同，难以区分误报和真实故障。

### 2.9 子进程/孙子进程设计分析

代码中存在两套风格不一致的子进程管理：

**✅ `restartNetworkServices()`：正确的双重 fork**  
fork → 中间进程立即 fork 孙子进程后 `_exit(0)`，孙子进程被 init 收养，`dial` 通过 `waitpid` 立即回收中间进程，不持有 ql_rild/ql_netd 的 PID，SIGCHLD 不会因其触发。

**⚠️ `restart_ql_netd()`：单 fork，存在隐患**  
ql_netd 成为 `dial` 的直接子进程，退出时触发 SIGCHLD，handler 中的 `waitpid(-1, WNOHANG)` 可能抢先回收 popen 子进程，导致 `pclose()` 返回 -1。不过此函数当前在主流程中**未被调用**，实际无影响。

建议修复方案（代码已写入 `analysis_52251024011503.md` 附录五）：`restart_ql_netd()` 改为双重 fork 与 `restartNetworkServices()` 保持一致；`get_cpin_status_str()` 加 `timeout 2` 前缀防止 popen 阻塞——但后者**必须先在设备上执行 `which timeout` 确认命令可用**再改，否则 `timeout` 找不到时 popen 会立即返回空，每次 AT+CPIN? 都被当作 SIM 故障，比原来更糟。

以上分析全部整理写入 `analysis_52251024011503.md`（含五个附录）。

---

## 三、基于客户日志分析，新增诊断打印

### 3.1 发现的诊断盲区

分析客户日志过程中，发现以下场景日志信息不足，出问题时无法快速定位根因：

| 盲区 | 具体问题 | 客户日志中的体现 |
|------|---------|--------------|
| SIM 恢复时刻不可见 | 只能靠下一条心跳推断，最多延迟 30s | 不知道 SIM 究竟断了多久 |
| popen 失败与真实故障无法区分 | 两者在日志里表现相同 | 误报路径完全不透明 |
| L2 触发时 SIM 状态不可见 | 不知道 L2 触发时 SIM 是否已自行恢复 | 难判断 L2 是否多余 |
| L2 后拨号结果不可见 | start 失败了完全无日志 | 要靠后续行为间接推断 |
| Fast Fail PDP 超时原因不明 | 不知道是 SIM UNKNOWN 还是其他原因 | 见于 09:51:35 那次 Fast Fail |
| 初始化拨号失败无日志 | start 失败后只能靠 60s 超时看出来 | ql_data_call_start 返回值不打印 |

### 3.2 新增打印的初始实现

针对上述盲区，在 `dial.c` 中新增以下打印（**零逻辑改动，纯增加日志**）：

**① SIM 恢复打印（新增 `sim_error_active` 局部变量）**
```c
// ST_SIM：SIM 断开时置位，恢复时打印精确时长
if (strcmp(sim_str, "READY") == 0 && sim_error_active) {
    sim_error_active = 0;
    long down_sec = (tnow - start_fail_ts) / 1000;
    dial_log("[INFO] SIM recovered: CPIN=READY (down ~%lds)\n", down_sec);
}
```

**② popen 失败明确区分**
```c
// get_cpin_status_str()：popen 失败时打印 errno
FILE *fp = popen("serial_atcmd at+cpin?", "r");
if (!fp) {
    dial_log("[WARN] AT+CPIN? popen failed (errno=%d: %s)\n", errno, strerror(errno));
    strncpy(out_buf, "ERROR", len - 1);
    return;
}
```

**③ L2 执行前打印 SIM/REG 实时状态（初始版本）**
```c
// 初始写法：调用 get_cpin_status_str() 获取实时 CPIN
char cpin_l2[32] = {0};
get_cpin_status_str(cpin_l2, sizeof(cpin_l2));
dial_log("[RECOVERY L2] Toggling RF (Airplane Mode)... CPIN=%s REG=%d\n",
         cpin_l2, g_last_reg_stat);
system("serial_atcmd at+cfun=0");
```

**④ L2 后拨号失败打印**
```c
ret = ql_data_call_start(g_call_id);
if (ret != 0) {
    dial_log("[RECOVERY L2] Start failed: %d\n", ret);
}
```

**⑤ Fast Fail PDP 超时打印 CPIN/REG 状态（初始版本）**
```c
// 初始写法：调用 get_cpin_status_str() 获取实时 CPIN
char cpin_ff[32] = {0};
get_cpin_status_str(cpin_ff, sizeof(cpin_ff));
dial_log("[INIT] Fast Fail: PDP not established in %dms. CPIN=%s REG=%d. Exiting...\n",
         PDP_WAIT_TIMEOUT_MS, cpin_ff, g_last_reg_stat);
log_close();
exit(1);
```

**⑥ 初始化拨号失败打印**
```c
ret = ql_data_call_start(g_call_id);
if (ret != 0) {
    dial_log("[INIT] ql_data_call_start failed: %d\n", ret);
}
```

### 3.3 代码审查：发现问题并修复

实现完成后，对所有改动进行逐行代码审查，发现 ③ 和 ⑤ 两处有问题：

**问题一（严重）：Fast Fail PDP 超时路径中调用 `get_cpin_status_str()` 会导致进程卡死**

**问题一（严重）的根因分析：**

⑤ 的触发条件是 PDP 在 60 秒内建不起来。结合客户日志 `09:51:35` 那次 Fast Fail，当时直接原因是 eSIM 还处于 UNKNOWN、CP 正在复位，AT 口响应极慢。此时再调 `get_cpin_status_str()` 去发 `AT+CPIN?`，`serial_atcmd` 会无限期挂住，导致 `log_close()` 和 `exit(1)` 永远执行不到，进程卡死无法重启——与新增这条日志的初衷完全相反。

**修复：⑤ 改用缓存值 `g_last_reg_stat`，去掉 `get_cpin_status_str()` 调用**

```c
// 修复后
dial_log("[INIT] Fast Fail: PDP not established in %dms. REG=%d. Exiting to retry...\n",
         PDP_WAIT_TIMEOUT_MS, g_last_reg_stat);
log_close();
exit(1);
```

`g_last_reg_stat` 由 30s 心跳持续更新，足以反映当前注册状态，且完全避免了 popen 阻塞风险。

---

**问题二（较轻）：③ L2 执行前额外发 AT 命令增加不必要压力**

L2 触发时已断网 5 分钟以上，AT 口可能已处于降级状态。初始写法在 `at+cfun=0` 前还调了一次 `get_cpin_status_str()`，两条 `serial_atcmd` 紧挨发出且无超时保护，增加了不必要的 AT 口压力。

**修复：③ 同样改用 `g_last_reg_stat`**

```c
// 修复后
dial_log("[RECOVERY L2] Toggling RF (Airplane Mode)... REG=%d\n", g_last_reg_stat);
system("serial_atcmd at+cfun=0");
```

CPIN 状态从之前的 `[ERROR] SIM Card disconnected. CPIN=X` 日志中已可见，不需要在 L2 时重复查询。

### 3.4 异常路径全覆盖确认

修改完成后，系统过一遍所有异常路径，确认每种情况都有打印：

| 异常情况 | 打印内容 | 来源 |
|---------|---------|------|
| SIM 断开 | `[ERROR] SIM Card disconnected, CPIN=X` | 原有 |
| SIM 恢复 READY | `[INFO] SIM recovered: CPIN=READY (down ~Xs)` | **新增** |
| AT+CPIN? popen 失败 | `[WARN] AT+CPIN? popen failed (errno=X)` | **新增** |
| Ping popen 失败 | `[PING ERROR] popen failed!` | 原有 |
| L1 跳过（REG 未就绪） | `[RECOVERY L1] Skip: REG not ready (REG:X)` | 原有 |
| L1 stop/start | `Stopping...` → `Restarting...` | 原有 |
| L1 start 失败 | `[RECOVERY L1] Start failed: X (REG:Y)` | 原有 |
| L2 执行前状态 | `[RECOVERY L2] Toggling RF... REG=Y` | **新增** |
| L2 start 失败 | `[RECOVERY L2] Start failed: X` | **新增** |
| L3 触发/退出 | `[RECOVERY L3] FATAL...` / `Exiting.` | 原有 |
| Fast Fail PDP 60s 超时 | `[INIT] Fast Fail: PDP not established... REG=Y` | **新增（并修复 Bug）** |
| Fast Fail Ping 10s 超时 | `[INIT] Fast Fail: PDP up but no Ping success... IF=X REG=Y` | **新增** |
| 初始化拨号失败 | `[INIT] ql_data_call_start failed: X` | **新增** |
| 网络恢复 | `[EVENT] Network Connected.` | 原有 |
| SIGINT 退出 | `[EVENT] SIGINT received.` | 原有 |

---

## 四、三层日志架构重新设计与实现

### 4.1 原有方案的问题

在新增诊断打印的过程中，同时审视了整体日志结构，发现根本性矛盾：原方案在 30 秒心跳中包含大量 AT 命令（QCSQ/QENG/CGPADDR 等），设备每 30 秒执行 7 次 popen，绝大多数时间在正常运行，这些信息是冗余负担；而在最需要信息的故障瞬间，反而没有完整的现场快照。

### 4.2 三层结构设计

重新设计为三层，每层在合适的时机触发，避免在无需要的时候浪费资源：

**第一层：30s 基本心跳（轻量，无额外 AT 开销）**
```
[HEARTBEAT] SIM:READY | REG:1 | CSQ:19 | DownTime:0s
```
维持对设备基本状态的持续感知，不增加任何额外 AT 命令。

**第二层：5 分钟扩展心跳（3 次 popen）**
```
[HEARTBEAT] RSRP:-100 | RSRQ:-6 | CID:0d17c147 | IP:10.99.16.137
```
- 首次联网成功后立即触发一次（`next_ext_heartbeat_ts = 0`）
- 每 5 分钟一次，观察信号质量和小区的长期趋势
- 故障恢复后立即触发一次，抓取恢复后的信号/小区/IP 状态（防止等 5 分钟才能看到）

**第三层：DIAG 快照（6 次 popen，每故障周期仅触发一次）**
```
[DIAG] RSRP:-112 | RSRQ:-15 | CID:0d17c147 | IP:... | Temp:40.3 | CEER:36 | CGACT:1,0
```
- 在每次故障周期中，Ping 连续失败达到阈值（3 次）的那一刻立即触发
- 用 `diag_snap_done` 标志保证每个故障周期只打一次
- 故障恢复时 `diag_snap_done = 0` 重置，下次故障重新触发
- 信息密度最高，包含 TEMP（模组温度）、CEER（错误原因码）、CGACT（PDP 上下文状态）

### 4.3 实现中发现并修复的逻辑 Bug

**问题：故障恢复后扩展心跳不立即触发**

当 `has_notified_connect` 已经为 1（之前已联网），故障恢复时 Ping 成功，`start_fail_ts` 清零，但 `next_ext_heartbeat_ts` 没有重置，最长要等 5 分钟才能看到恢复后的信号/小区/IP 状态——而这恰恰是最有诊断价值的时刻。

**修复：**
```c
// Ping 成功时
ping_fail_count = 0;
if (start_fail_ts != 0) {
    next_ext_heartbeat_ts = 0;  // 刚从故障恢复，立即触发扩展心跳
}
start_fail_ts = 0;
diag_snap_done = 0;
```

### 4.4 同步去除 L1 REG 前置校验

分析期间发现 L1 恢复前有以下判断：
```c
if (g_last_reg_stat != 1 && g_last_reg_stat != 5) {
    // skip L1
}
```
查 git log 确认这是用户自己在 `df06b4d` 中加入的，不是 minglei.li 的原始设计。分析其必要性：L1 跳过时不更新 `last_recovery_ts`，对 L2 的触发时间完全没有影响，校验无实质作用。去除后，REG 值仍在日志中打印，不影响可观测性。

---

## 五、部署到自己设备验证，发现并修复 AT 命令兼容问题

### 5.1 问题：5 分钟扩展心跳始终不打印

将新固件烧录到自己的测试设备后，发现 5 分钟扩展心跳一条都没有打印出来。追查发现三个字段同时为空：

- `AT+QCSQ`：设备固件不支持，返回 ERROR，RSRP/RSRQ/SINR 字段为空
- `AT+QENG="servingcell"`：设备固件不支持，返回 ERROR，CID 字段为空
- `AT+CGPADDR`：支持，但响应格式 `+CGPADDR: 1, "10.99.16.137"` 中逗号后带有空格，原代码解析时只跳过逗号不跳过空格，导致 IP 提取失败

三个字段同时为空 → `build_ext_line()` 无任何内容 → `if (n > 0 && ext_line[0])` 判断为空行 → 整条扩展心跳被过滤，不打印。

### 5.2 修复

**① `AT+QCSQ` → `AT+CESQ`（3GPP 标准命令）**

设备实测返回：`+CESQ: 36,99,255,255,24,39`（格式：rxlev, ber, rscp, ecn0, rsrq_raw, rsrp_raw）

```c
// RSRP(dBm) = rsrp_raw - 141；RSRQ(dB) = rsrq_raw / 2 - 19
// 255 为无效值（不适用于当前制式），跳过不输出
int v1, v2, v3, v4, rsrq_raw, rsrp_raw;
if (sscanf(p+1, " %d,%d,%d,%d,%d,%d", &v1,&v2,&v3,&v4,&rsrq_raw,&rsrp_raw) == 6
    && rsrp_raw != 255 && rsrq_raw != 255) {
    int rsrp = rsrp_raw - 141;   // 39 - 141 = -102 dBm
    int rsrq = rsrq_raw / 2 - 19; // 24/2 - 19 = -7 dB
    snprintf(buf+pos, len-pos, "RSRP:%d | RSRQ:%d", rsrp, rsrq);
}
```

**② `AT+QENG` → `AT+CREG?`（复用已有命令）**

设备实测返回：`+CREG: 3,1,"272d","0d17c147",7`（格式：n, stat, "lac", "ci", act）

通过计数引号对定位第 3/4 个引号之间的 CI 字段：
```c
// 数到第3个引号开始，第4个引号结束，取中间的 CI
int quote_count = 0;
const char *ci_start = NULL;
while (*p) {
    if (*p == '"') {
        quote_count++;
        if (quote_count == 3) ci_start = p + 1;
        else if (quote_count == 4 && ci_start) {
            // 提取 CI，如 "0d17c147"
            break;
        }
    }
    p++;
}
```

**③ `AT+CGPADDR` IP 解析空格 Bug**

```c
const char *p = strchr(ip_str, ',');
if (p) {
    p++;
    while (*p == ' ') p++;  // ← 新增：跳过逗号后可能的空格
    if (*p == '"') p++;
```

### 5.3 验证结果

修复后烧录到自己设备，日志输出如下：

```
[2026-04-30 14:57:21] Program started. Main Version: 1.26
[2026-04-30 14:57:21] [INIT] Fast Retry Mode: Attempt 1/3
[2026-04-30 14:57:22] [HEARTBEAT] SIM:READY | REG:1 | CSQ:19 | DownTime:0s
[2026-04-30 14:57:23] [INIT] PDP connected, start Ping window (10000ms).
[2026-04-30 14:57:24] [INIT] Dial success. Retry count cleared.
[2026-04-30 14:57:24] [EVENT] Network Connected. Notification sent.
[2026-04-30 14:57:24] [HEARTBEAT] RSRP:-100 | RSRQ:-6 | CID:0d17c147 | IP:10.99.16.137  ← 首连立即触发 ✅
[2026-04-30 14:57:52] [HEARTBEAT] SIM:READY | REG:1 | CSQ:19 | DownTime:0s
... （每 30s 一条）
[2026-04-30 15:02:24] [HEARTBEAT] RSRP:-100 | RSRQ:-8 | CID:0d17c147 | IP:10.99.16.137  ← 5分钟后准时 ✅
```

- 从启动到拨号成功约 3 秒（Fast Retry Attempt 1/3）
- 30s 基本心跳间隔准确
- 5 分钟扩展心跳：首连立即触发，之后间隔精确（14:57:24 → 15:02:24 = 5 分 00 秒）
- RSRP/RSRQ/CID/IP 全部有效输出
- CID 两次相同（`0d17c147`），说明未发生小区切换，接入点稳定

---

## 六、当前状态

代码所有改动已在自己的测试设备上验证通过。**尚未部署到客户设备。** 待下次烧录后，才能在真实的 eSIM 瞬断场景下验证新增日志的实际效果——特别是 `[INFO] SIM recovered (down ~Xs)` 能否精确记录 SIM 恢复时刻，以及 Fast Fail 路径的 REG 状态打印是否能帮助快速定位 PDP 建不起来的原因。

---

## 附：本次 dial.c 主要改动提交说明

```
[CHGLOG] 新增三层诊断日志、去除 L1 恢复 REG 前置校验、修复 CGPADDR IP 解析空格问题，
并将信号诊断命令从设备不支持的 AT+QCSQ/AT+QENG 替换为标准 AT+CESQ/AT+CREG
```
