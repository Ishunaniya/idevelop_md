# dev_log_0511 设备日志分析与 SDK 崩溃诊断

> **分析日期**：2026-05-12  
> **程序版本**：1.27.0（commit `977f204`）；设备5日志版本 `939dbb4`  
> **日志来源**：`dev_log_0511.txt`（365行，3台设备，约18个异常事件）  
> **涉及设备**：设备1、设备2、设备3（SN 52251224010960）、设备4、设备5（SN 52251024011503）  

---

## 目录

1. [dev_log_0511 整体分析](#1-dev_log_0511-整体分析)
2. [实际断网时间段](#2-实际断网时间段)
3. [设备3深度分析](#3-设备3深度分析)
4. [六个具体问题的分析](#4-六个具体问题的分析)
5. [~~Attempt 2/3 与系统重启矛盾~~（已推翻，见第10节）](#5-attempt-23-与系统重启矛盾的最终结论已推翻)
6. [进程架构与 gb32960_start 来源](#6-进程架构与-gb32960_start-来源)
7. [SDK 崩溃理论的证据基础](#7-sdk-崩溃理论的证据基础)
8. [ql_data_call_set_service_error_cb 实现](#8-ql_data_call_set_service_error_cb-实现)
9. [结论与待办](#9-结论与待办)
10. [【重要修正】断电重启实验推翻第5节结论](#10-重要修正断电重启实验推翻第5节结论)
11. [设备5（SN 52251024011503）日志分析](#11-设备5sn-52251024011503日志分析)
12. [SIM 状态取值路径分析与心跳双值打印](#12-sim-状态取值路径分析与心跳双值打印)
13. [设备5（SN 52251024011503）2026-05-13 日志分析](#13-设备5sn-52251024011503-2026-05-13-日志分析)
14. [诊断日志采集方案实现（2026-05-14）](#14-诊断日志采集方案实现2026-05-14)

---

## 1. dev_log_0511 整体分析

### 关键观测结论

- **18个异常事件全部在 L0 阶段自愈**（断网持续时间 < 5分钟），没有任何一次触发 L1/L2/L3 分级恢复
- L0 = SDK 底层 25s 自动重连等待期，无需应用层干预
- 从 `[EVENT] Network Recovered in SDK phase (L0)` 日志可区分 L0 恢复与应用层恢复

### 特殊事件：CONNECTED→IDLE（3次）

SDK 在 AT+CFUN=0/4 关闭射频时发送 `CONNECTED→IDLE` 而非 `DISCONNECTED`。以下3次事件属于此类：

| 设备 | 时间 | 说明 |
|------|------|------|
| 设备1 anomaly2 | 11:22:07 | CONNECTED→IDLE 事件 |
| 设备1 anomaly3 | 03:04:01 | CONNECTED→IDLE 事件 |
| 设备2 anomaly7 | 08:46:24 | CONNECTED→IDLE 事件 |

**意义**：这3次验证了 `data_call.c` 中 CONNECTED→IDLE handler（`dial.c` 对应 `data_call.c:179`）的必要性——没有这个 handler，`g_pdp_connected` 不会归零，`/tmp/network_status` 不会更新为 0。

---

## 2. 实际断网时间段

**判断依据**（基于源码事实，不臆测）：

真正的断网 = Ping 持续失败 AND `/tmp/network_status` = 0。  
从日志判断的精确方法：
- `[INFO] SDK auto-reconnect phase started` → 故障计时开始（连续3次Ping失败后）
- `[EVENT] Network Recovered in SDK phase (L0)` → 网络恢复
- `[EVENT] PDP Disconnected` / `[EVENT] PDP Idle (was connected)` → PDP 层断开时刻

所有18次事件均未超过5分钟，用户感知为短暂卡顿或重新连接。

---

## 3. 设备3深度分析

### 设备信息
- **SN**：52251224010960
- **日志目录**：`52251224010960/0511/1.27.0/`
- **日志文件数**：145个

### 文件分布

| 类型 | 数量 | 特征 |
|------|------|------|
| 稳定大文件 | 2个 | 正常运行，有完整 `=== Program Exit ===`（或自然结束） |
| 短命文件 | 143个 | 每个3~10分钟，最后一行是正常心跳，**无 `=== Program Exit ===`** |

**两个稳定大文件**：
- `dial_20260510_101403.log`：58K，10:14 → 15:09（约5小时）
- `dial_20260510_191736.log`：68K，19:17 → 次日 01:10（约6小时）

### 关键特征
1. **全部 `Attempt 2/3`**，无 `Attempt 1/3` 文件
2. **0个文件有 `Program Exit`**：进程从未正常退出
3. **0个 Fast Fail 退出**：不是启动失败
4. 每次重启后日志文件名时间戳连续，`/tmp` 明显未被清空

---

## 4. 六个具体问题的分析

### Q1：CONNECTED→IDLE handler 是否有效？有必要保留吗？

**结论：有效，必须保留，但理由有细分。**

`data_call.c` 中 CONNECTED→IDLE handler 的两类操作：

| 操作 | 必要性 | 理由 |
|------|-------|------|
| `g_pdp_connected = 0` | **必须** | 否则主循环一直认为 PDP 存活，Fast Fail 计时和 Ping 判断均受影响 |
| `update_network_status(0)` | **必须** | 否则本地应用读到的 `/tmp/network_status` 永远是 1（已联网） |
| 路由/NAT/resolv 清理 | 冗余但无害 | CONNECTED handler 已有防御性预清理，重连时会先清理；IDLE handler 的清理提供更早的路由清理 |

**操作建议**：保留完整 handler，不做修改。

---

### Q2：设备3频繁重启，确定不是我们代码的问题吗？

**结论：确定不是我们代码主动调用 `exit()` 的问题。**

**依据**：`=== Program Exit ===` 由 `logger_sd.c:log_close()` 打印，所有我们代码中的 `exit()` 调用前均有 `log_close()`：

```c
// dial.c 中所有 exit() 调用一览（每处均有 log_close() 前置）：
log_close(); exit(1);   // Fast Fail: PDP 超时
log_close(); exit(1);   // Fast Fail: Ping 超时
log_close(); exit(0);   // SIGINT 处理
log_close(); exit(1);   // L3 恢复
```

145个日志文件无一有 `Program Exit`，因此143次频繁退出**均不是我们代码触发的**。

---

### Q3：进程重启的根因是什么？

**已知事实**：
- 进程静默退出（无 `Program Exit`）
- 不是整机重启（见第5节）
- 不是我们代码的 `exit()`（见Q2）
- dmesg 无 segfault / OOM kill 记录（上次分析结论）

**推断（非直接证据）**：  
Quectel ql-sdk 数据拨号 API 采用 C/S 架构（API 文档 Section 2.4）。CP 侧服务进程异常退出时，AP 侧 SDK 检测到 IPC 断开，可能静默终止 dial 进程。该推断来自 API 文档描述，**尚未通过日志直接证实**。

**当前验证手段**：已加入 `ql_data_call_set_service_error_cb`（见第8节）。

---

### Q4：Attempt 1/3 的日志文件全部缺失

**事实**：设备3的145个文件全部是 `Attempt 2/3`，没有 `Attempt 1/3`。

**原因分析**：

`check_and_update_retry_count()` 的行为：
- 文件不存在 → count=0 → 写入1 → 返回1 → `Attempt 1/3`
- 文件值为1 → count=1 → 写入2 → 返回2 → `Attempt 2/3`

`Attempt 1/3` 对应的日志文件应存在，但提供的目录列表中全部缺失。可能原因：
1. 提供的日志目录不完整（采集时遗漏）
2. `Attempt 1/3` 的日志极短（PDP 建立前就退出），SD卡写入未及时落盘

---

### Q5：设备4 异常1 —— `Program Exit` 是否打印？

**事实**（来自截图 `1778550329613.PNG`）：
- `dial_20260510_102832.log` 最后一条日志：10:41:34（**无 `Program Exit`**）
- `gb32960_start` 出现：10:42:41（间隔67秒）
- `dial_20260510_104338.log` 开始：10:43:38

`dial_20260510_102832.log` 没有 `Program Exit`，与设备3的143个文件行为一致。

---

### Q6：`/tmp/network_status` 的可靠性

**用途**：本地某应用读取此文件判断是否联网（非直接云端上报）。

**源码写入逻辑**（`misc.c:update_network_status()`）：
- 1 = 联网（由 `on_network_connected` 回调触发，Ping 成功后写入）
- 0 = 断网（由 DISCONNECTED/IDLE 回调、L1/L2/L3 恢复前写入）

**可靠性分析**：

| 场景 | 文件状态 | 本地应用判断 |
|------|---------|------------|
| 正常联网 | 1 | 正确 |
| PDP DISCONNECTED | 0（回调写入） | 正确 |
| SDK 杀死进程（静默退出） | 残留 1（未写0就死了） | **误判为已联网** |
| 整机重启 | 文件消失（tmpfs清空） | 应用读不到文件 |

**结论**：SDK C/S 崩溃类型的断网，`/tmp/network_status` 无法及时更新为0，本地应用会在重启恢复期内误判为已联网。这是该机制的固有局限，由进程架构决定，非代码bug。

---

## 5. Attempt 2/3 与系统重启矛盾（已推翻，见第10节）

> ⚠️ **本节结论已被第10节的断电实验推翻，保留原文仅作记录。**

### 现象矛盾（当时的分析）

- 同事判断：设备发生了"整机重启"（依据：日志中出现 `gb32960_start`）
- 日志证据：`dial_20260510_104338.log` 第一行为 `Attempt 2/3`

### 当时的逻辑推导（已证明有误）

```
整机重启 → tmpfs(/tmp)清空 → dial_retry_count 消失 → 下次启动必为 Attempt 1/3
但实际是 Attempt 2/3 → dial_retry_count 值为 1 → /tmp 未被清空
/tmp 未被清空 → 没有整机重启   ← 此推论有缺陷，见第10节
```

### 验证 `/tmp` 是否为 tmpfs

```
root@OpenWrt:~# cat /proc/mounts | grep /tmp
tmpfs /tmp tmpfs rw,seclabel,nosuid,nodev,noatime 0 0
```

已确认：`/tmp` 是 tmpfs，整机重启必然清空。但这并不能用来反推"看到 Attempt 2/3 = 没有整机重启"，原因见第10节。

---

## 6. 进程架构与 gb32960_start 来源

### 设备进程架构（来自 `ps aux`）

| PID | 进程 | 角色 |
|-----|------|------|
| 1 | `procd` | OpenWrt init |
| 1080 | `ql_rild` | Quectel RIL 层 |
| 1157 | `start_prog 15 /usr/dial/dial` | dial 专属 keepalive，15秒重拉 |
| 1297 | `ql_netd` | Quectel 网络服务，`ql_data_call_*` API 的服务端 |
| 1136 | `start_daemon.sh` | 所有 `_client` 进程的应用层 keepalive |
| 1931 | `gb_32960_client` | GB32960 应用进程 |

### `gb32960_start` 来源

查阅 `/opt/start_daemon.sh` 源码（`excute_device_apps()` 函数）：

```sh
if [ $alive -eq 0 ]; then
    killall -9 gb_32960_565_client
    chmod +x $client
    ./$client >/dev/null 2>&1 &
    echo "gb_32960_client start"   # ← 这就是 gb32960_start 日志的来源
fi
```

`start_daemon.sh` 每 20 秒循环检查所有 `_client` 进程是否存活，发现 `gb_32960_client` 死亡时重拉并打印此行。

**结论**：`gb32960_start` = `gb_32960_client` 被应用层 keepalive 重拉，不代表整机重启，`/tmp` 完全不受影响。

### 进程崩溃时的连锁反应

> ⚠️ 以下描述适用于**应用层崩溃**（非整机重启）场景；整机重启场景见第10节。

```
应用层崩溃（dial/gb_32960_client 被 kill，系统未重启）
  ├─ dial 进程静默退出（无 Program Exit，/tmp 保留）
  │    └─ start_prog(PID 1157) 等 15s 重拉 → Attempt N/3（SD 卡已挂载，产生日志）
  └─ gb_32960_client 也崩溃
       └─ start_daemon.sh(PID 1136) 20s内重拉 → 打印 "gb_32960_client start"
```

---

## 7. SDK 崩溃理论的证据基础

### 已确认事实

| 事实 | 依据 |
|------|------|
| 进程静默退出，无 `Program Exit` | 145个日志文件直接观测 |
| 不是整机重启 | `/tmp` 是 tmpfs + `Attempt 2/3` 逻辑推导 |
| 不是我们代码调用 `exit()` | 所有 `exit()` 前均有 `log_close()`，日志无 `Program Exit` |
| 不是 OOM / segfault | dmesg 分析（上次分析结论） |
| `start_daemon.sh` 不 kill dial | 查阅脚本源码，无 `killall dial` |

### 推断（尚未直接证实）

| 推断 | 依据 | 证据类型 |
|------|------|---------|
| SDK C/S IPC 断开后 kill dial | API 文档 Section 2.4 描述 C/S 架构 | 文档推断，非日志证据 |
| ql_netd 崩溃触发 IPC 断开 | ql_netd 是 ql_data_call API 的服务端 | 架构推断 |

### 验证方法

已在代码中加入 `ql_data_call_set_service_error_cb`（API Section 3.3.32）：
- **如果日志出现 `[FATAL] SDK data call service crashed`** → SDK C/S 崩溃假说成立
- **如果从不出现 `[FATAL]` 但进程仍静默退出** → 需寻找其他原因（其他脚本 kill、硬件 watchdog 等）

---

## 8. ql_data_call_set_service_error_cb 实现

### 代码改动（dial.c，版本 1.27.1）

**新增全局标志**（`dial.c:1155`）：
```c
static volatile int g_sdk_service_error = 0;
```

**新增回调函数**（`dial.c:1212`）：
```c
static void data_call_service_error_cb(int error)
{
    (void)error;  /* 唯一可能值为 QL_ERR_ABORTED */
    g_sdk_service_error = 1;
}
```

**注册回调**（`dial.c:1329`，紧跟 `set_status_ind_cb`）：
```c
ql_data_call_set_status_ind_cb(data_call_status_ind_cb);
ql_data_call_set_service_error_cb(data_call_service_error_cb);
```

**主循环检测**（`dial.c:1480`）：
```c
if (g_sdk_service_error) {
    g_sdk_service_error = 0;           // 清零防重复打印
    dial_log("[FATAL] SDK data call service crashed (QL_ERR_ABORTED). "
             "CP-side IPC broken. Exiting for keepalive to restart.\n");
    // log_close();   // 诊断版本暂时注释
    // exit(1);       // 验证后恢复
}
```

### 当前版本为诊断版本

`exit(1)` 暂时注释，目的是观察 `[FATAL]` 日志是否出现。  
**验证后必须恢复**：
```c
if (g_sdk_service_error) {
    dial_log("[FATAL] SDK data call service crashed (QL_ERR_ABORTED). "
             "CP-side IPC broken. Exiting for keepalive to restart.\n");
    log_close();
    exit(1);
}
```

### 为何不用 deinit→init 原地恢复？

API 文档建议 `ql_data_call_deinit()` → `ql_data_call_init()` 原地恢复。但实现代价高：

```
deinit → init（最多重试200次）
→ sim_init + sim_get_iccid
→ ql_sim_set_card_status_cb
→ set_apn
→ ql_data_call_create/config/start
→ 重置所有状态变量...
```

这等价于重跑完整的 `dial_loop()` 初始化序列。`exit(1)` + `start_prog` 15秒重拉效果完全相同，状态更干净（新进程），实现更简单。**保持 `exit(1)` 是正确选择。**

---

## 10. 【重要修正】断电重启实验推翻第5节结论

### 实验事实（2026-05-12 下午）

用户对自己的设备执行：替换 dial 二进制（1.27.0 → 1.27.1）+ **断电重启**。

重启后 SD 卡日志目录内容：

```
dial_20260512_091110.log   54.7K  重启前的稳定会话（1.27.0）
dial_20260512_140126.log    2.2K  重启前最后一次 1.27.0 会话（Attempt 2/3）
dial_20260512_140410.log    4.6K  断电重启后第一个 SD 卡日志（1.27.1，Attempt 2/3）
```

`dial_20260512_140410.log` 第一行：
```
[2026-05-12 14:04:10] [INIT] Fast Retry Mode: Attempt 2/3
```

**断电重启后，SD 卡上第一个日志文件显示 Attempt 2/3，不是 Attempt 1/3。**

### 正确机制解释

```
断电重启
  → /tmp 清空（tmpfs），dial_retry_count 文件消失
  → 系统启动，start_prog 比 start_daemon.sh 更早拉起 dial
  → Attempt 1/3 启动
       → log_init() 检测 SD 卡：尚未挂载（start_daemon.sh 还没执行到挂载步骤）
       → check_sdcard_available() 返回 0
       → 降级为控制台输出，不创建日志文件
       → 写 /tmp/dial_retry_count = 1
       → 会话结束（Fast Fail 或其他原因）
  → start_prog 等 15s 重拉
  → Attempt 2/3 启动
       → 此时 start_daemon.sh 已完成 SD 卡挂载
       → log_init() 成功 → 创建日志文件
       → SD 卡上第一个文件 = Attempt 2/3  ← 正是观测到的现象
```

### 第5节结论为何有缺陷

| 旧推论 | 问题 |
|--------|------|
| "SD 卡上看到 Attempt 2/3 → /tmp 未清空 → 没有整机重启" | SD 卡日志文件不存在 ≠ /tmp 未清空；Attempt 1/3 可能因 SD 卡未挂载而没有产生日志文件 |

正确的理解：

- **整机重启后**：Attempt 1/3 无 SD 日志（SD 卡未挂载），Attempt 2/3 有 SD 日志
- **应用层重启后**（SD 卡已挂载）：Attempt 1/3 有 SD 日志，除非 Attempt 1/3 发生在开机初期

因此，"SD 卡日志全部是 Attempt 2/3"与"整机重启"完全相容，不能用来否定整机重启。

### 对设备3分析的修正

| 之前结论 | 修正后结论 |
|---------|-----------|
| 设备3全是 Attempt 2/3 → 没有整机重启 → 同事判断有误 | ❌ 错误 |
| 设备3全是 Attempt 2/3 → SD 卡未挂载期间 Attempt 1/3 无日志 → 整机重启完全可能 | ✅ 正确 |

**同事"整机重启"的判断可能是正确的。**

### 新的核心问题

设备3频繁整机重启的根因是什么？

- SDK C/S 崩溃可能只是杀进程（不触发整机重启）
- 触发整机重启的更可能是：**硬件 watchdog**、**电源问题**（车载电压波动）、或**某个软件主动调用 reboot**

**建议下一步排查**：

```bash
# 1. 查看 sys_reboot.log（start_daemon.sh 写入的重启记录）
cat /media/sdcard/sys_reboot.log

# 2. 查看内核日志（是否有 watchdog 触发记录）
dmesg | grep -iE 'watchdog|reboot|panic|reset'

# 3. 查看系统日志
cat /var/log/messages | grep -iE 'reboot|watchdog|panic'
```

---

## 9. 结论与待办

### 已确认结论

1. **18次断网全部 L0 自愈**，1.27.0 版本分级恢复逻辑未被触发，运行正常
2. **CONNECTED→IDLE handler 必须保留**（`g_pdp_connected` 和 `network_status` 状态标志不可缺少）
3. **设备3/4 频繁重启不是我们代码主动 exit() 的问题**（无 Program Exit 是铁证）
4. **`gb32960_start` 是应用级重启日志**，来自 `start_daemon.sh` 的 20s 保活循环
5. ~~不是整机重启~~ → **已推翻（第10节）**：SD 卡日志全部为 Attempt 2/3 与整机重启完全相容
6. **设备5（SN 52251024011503）12次异常全部由 L1 或 L0 恢复**（见第11节），根因为 PDP 僵尸态，SDK 无法自动重连
7. **心跳 `SIM:UNKNOWN` 不等于 ST_SIM 检测到 SIM 故障**：两条路径取值来源不同，REFRESH 期间心跳 AT 查询失败但 ST_SIM 回调值仍为 READY（见第12节）

### 待验证

| 项目 | 方法 | 状态 |
|------|------|------|
| 设备3频繁整机重启根因 | 查 `sys_reboot.log`、dmesg watchdog 记录 | ⏳ 待排查 |
| SDK C/S 崩溃假说 | 观察新版本日志是否出现 `[FATAL]` | ⏳ 等待设备运行数据 |
| 验证后恢复 `exit(1)` | 确认 `[FATAL]` 出现后修改代码 | ⏳ 待 `[FATAL]` 日志确认 |
| 设备5 eSIM REFRESH 根因确认 | 向移远提交问题单，确认 TAU/REFRESH 机制 | ⏳ 待发出 |

### 后续改进方向

- 排查设备3整机重启根因（watchdog、电源、软件 reboot）
- 心跳新增 SIM_CB 字段，区分 AT 接口阻塞与 SIM 真正脱卡（已实现，见第12节）

---

## 11. 设备5（SN 52251024011503）日志分析

> **日志版本**：`939dbb4`  
> **异常总数**：12次，分3类  
> **分析依据**：日志直接证据 + 基于 `939dbb4` 代码逻辑的推断（已明确标注）

### 异常汇总

| 类型 | 次数 | 断网时长 | 恢复方式 |
|------|------|---------|---------|
| Anomaly 1：小区切换→PDP僵尸 | 9次 | ~336s | L1 触发 |
| Anomaly 2：eSIM AT 接口阻塞 | 1次 | ~457s | L1 触发 |
| Anomaly 3：短暂切换自愈 | 2次 | ~3s | L0（自愈） |

---

### Anomaly 1（9次）断网约 336s

**日志直接证据：**

- `Network Recovered. Down: 336s.` — 日志原文
- 心跳/DIAG 日志中 `CID:` 字段前后行数值不同 — 小区切换直接可观察
- L1 执行期间日志打印 `+CGACT: 1,1` — PDP 上下文仍显示"激活"
- `ql_data_call_start() ret=-1001` — 日志原文

**机制（推断，有逻辑支撑）：**

小区切换后 PDP 进入"僵尸态"：CGACT 显示激活但 Ping 不通。SDK 没有收到 DISCONNECTED 回调（PDP 在协议层面未被断开），因此不触发自动重连。Ping 失败积累到 PING_FAIL_THRESHOLD=3 → `start_fail_ts` 置位 → 60s 后 L1 触发 → `ql_data_call_stop()` 打断僵尸态 → SDK 25s 定时器触发重连 → 恢复。`ql_data_call_start() ret=-1001` 是 SDK 重连过程中尚未就绪的正常返回，不影响最终恢复。

---

### Anomaly 2（1次）断网约 457s

**日志直接证据：**

- `Network Recovered. Down: 457s.` — 日志原文
- 某条心跳同时出现 `SIM:UNKNOWN | REG:-1 | CSQ:-1` — 三个 AT 查询同时失败

**机制（推断）：**

心跳里的 SIM:UNKNOWN、REG:-1、CSQ:-1 是同一次 AT 接口阻塞期间 AT+CPIN?、AT+CEREG?、AT+CSQ 三条命令同时查询失败的结果，持续约 25s 后自动恢复。成因推断为 eSIM TAU/REFRESH 触发 AT 接口阻塞（与之前分析同一设备 `d51f798` 日志的结论一致，但该成因无法从日志直接证明）。

**关键修正——ST_SIM 未感知到此事件：**

`939dbb4` 中 ST_SIM 优先读 `g_sim_app_ready` 回调值。eSIM REFRESH 只阻塞 AT 接口，不触发 `sim_card_status_cb`（SIM 未物理脱卡），故 `g_sim_app_ready` 仍为 1（READY）。ST_SIM 认为 SIM 正常，**`[ERROR] SIM Card disconnected` 不会打印**，也不走 SIM 故障恢复路径。

实际恢复链路：AT 阻塞期间 Ping 同样失败 → 积累到阈值 → `start_fail_ts` 置位 → L1 触发 → `stop()` 打断僵尸态 → 恢复。与 Anomaly 1 的恢复路径相同，驱动恢复的是 Ping 检测，不是 SIM 检测。

---

### Anomaly 3（2次）断网约 3s

**日志直接证据：**

- `Network Recovered. Down: 3s.` — 日志原文

Ping 失败未达到连续 3 次阈值，或 PDP 短暂抖动后自愈，未触发任何 L1/L2/L3。

---

### 为何 SDK 不能自动恢复（Anomaly 1 / 2 共同原因）

`+CGACT: 1,1` 表明 PDP 上下文在协议层面仍处于激活状态，SDK 只在收到 DISCONNECTED 状态回调时才触发自动重连。僵尸态下无 DISCONNECTED 事件，SDK 认为连接正常，不重连。必须由 L1 `stop()` 主动介入打断，才能让 SDK 感知到断开并重新建立连接。

---

## 12. SIM 状态取值路径分析与心跳双值打印

### 两条取值路径（`939dbb4` 代码事实）

| 位置 | 取值方式 | 代码位置 |
|------|---------|---------|
| **心跳** | 始终 `get_cpin_status_str()` → 直接发 AT+CPIN? | dial.c 心跳块，`sim_at` 变量 |
| **ST_SIM** | `g_sim_app_ready >= 0` 时读回调缓存值；否则降级 AT+CPIN? | dial.c ST_SIM case，`g_sim_app_ready` 判断 |

两条路径取值来源不同，导致 eSIM REFRESH 期间出现矛盾：
- 心跳：AT+CPIN? 被阻塞 → 打印 `SIM:UNKNOWN`
- ST_SIM：读回调缓存（REFRESH 不触发 `sim_card_status_cb`）→ 仍为 READY

这就是"有 `SIM:UNKNOWN` 心跳记录、却没有 `[ERROR] SIM Card disconnected` 日志"的根本原因。

### 心跳改为双值打印（版本 1.27.1，2026-05-12）

**目的**：同时显示 AT 查询值和回调缓存值，使日志可直接区分"AT 接口阻塞（REFRESH）"和"SIM 真正脱卡"两种场景：

| 场景 | SIM_AT | SIM_CB |
|------|--------|--------|
| 正常 | READY | READY |
| eSIM REFRESH（AT 阻塞） | UNKNOWN | READY |
| SIM 真正脱卡 | NOT INSERTED | NOT_READY |
| 回调未初始化 | — | CB_UNINIT |

**代码改动（dial.c 心跳块）：**

```c
// 改前
char sim_str[32] = {0};
get_cpin_status_str(sim_str, sizeof(sim_str));
dial_log("[HEARTBEAT] SIM:%s | REG:%d | CSQ:%d | TEMP:%d | DownTime:%lds\n",
         sim_str, reg_stat, rssi, cpu_temp, downtime_sec);

// 改后
char sim_at[32] = {0};
get_cpin_status_str(sim_at, sizeof(sim_at));
const char *sim_cb = (g_sim_app_ready >= 0)
                     ? sim_app_state_str((QL_SIM_APP_STATE_E)g_sim_app_state)
                     : "CB_UNINIT";
dial_log("[HEARTBEAT] SIM_AT:%s | SIM_CB:%s | REG:%d | CSQ:%d | TEMP:%d | DownTime:%lds\n",
         sim_at, sim_cb, reg_stat, rssi, cpu_temp, downtime_sec);
```

**注意**：心跳保留 AT+CPIN? 查询而非全部换成回调值，原因是心跳的诊断价值正在于能感知 AT 接口阻塞事件；若换成回调值，REFRESH 事件在日志中将彻底不可见。
- 若 SDK 崩溃假说成立，考虑在 `[FATAL]` 之后调用 `sync()` 确保日志落盘再 `exit(1)`

---

## 13. 设备5（SN 52251024011503）2026-05-13 日志分析

> **日志时间段**：2026-05-12 21:44:45 ～ 2026-05-13 11:28:43  
> **程序版本**：1.27.2（最新版）  
> **异常总数**：3处（含1处内含两次连续断网）  
> **分析日期**：2026-05-13  

---

### 13.1 总体结论

三处异常**全部是同一根因模式**：LTE 小区切换 → PDP 僵尸态 + AT 接口短暂阻塞 → L1 在 ~303s 触发后恢复。程序逻辑运行正常，无代码 Bug。  
三次均未出现 `[FATAL] SDK data call service crashed`，排除 SDK 服务崩溃，与 PDP 僵尸态假说一致。

---

### 13.2 异常1（06:22:36 — 06:27:48，断网 ~313s）

**事件时序**：

| 时间 | 关键日志 | 说明 |
|------|---------|------|
| 06:21:06 | RSRP CID:`0fd5dc40`, IP:10.8.15.122 | 断网前所在小区 |
| 06:22:36 | SDK auto-reconnect started | `start_fail_ts` 置位 |
| 06:22:37 | SIM_AT:**UNKNOWN** \| SIM_CB:**READY** \| REG:-1 \| CSQ:-1 \| DownTime:2s | AT 接口全项阻塞 |
| 06:23:07 | SIM_AT:READY \| REG:1 \| DownTime:32s | AT 接口恢复（阻塞约 30s） |
| 06:26:06 | RSRP CID:**0f5c9d01**, IP:10.21.5.6 | 切换后新小区（CID 已变） |
| 06:27:40 | L1 triggered (303s) \| CGACT:**1,1** | PDP 仍显示"激活"= 僵尸态确认 |
| 06:27:43 | PDP reconnected, IP:10.18.164.222 | L1 stop→start 成功 |
| 06:27:48 | Network Recovered. Down: 313s | |

**诊断**：小区从 `0fd5dc40` 切换至 `0f5c9d01`，切换过程触发 eSIM REFRESH 导致 AT 接口阻塞约 30s（三项 AT 查询同时失败，SIM_CB:READY 确认非真实脱卡）。切换后 EPS Bearer 重建失败，PDP 进入僵尸态（CGACT:1,1 但 Ping 不通），SDK 无 DISCONNECTED 回调，不启动自动重连。L1 在 303s 主动 stop→start，恢复正常。

---

### 13.3 异常2（06:54:00 — 07:08:20，两次连续断网）

此段日志实际包含**两次独立断网**，均为相同模式，中间仅稳定约 2 分钟即发生第二次切换，说明设备在此时间段处于小区边缘或处于移动状态。

#### 第一次（06:54:00 — 06:59:11，断网 ~313s）

| 时间 | 关键日志 | 说明 |
|------|---------|------|
| 06:52:48 | RSRP CID:`046f3ede` | 断网前小区 |
| 06:54:00 | SDK auto-reconnect started | |
| 06:54:09 | SIM_AT:UNKNOWN \| SIM_CB:READY \| REG:-1 \| CSQ:-1 \| DownTime:10s | AT 阻塞 |
| 06:57:48 | RSRP CID:**0fd67c5a** | 切换后新小区 |
| 06:59:03 | L1 triggered (303s) | |
| 06:59:07 | PDP reconnected, IP:10.0.99.13 | L1 成功 |
| 06:59:11 | Network Recovered. Down: 313s | |

#### 第二次（07:03:09 — 07:08:20，断网 ~312s）

| 时间 | 关键日志 | 说明 |
|------|---------|------|
| 07:01:41 | DownTime:0s | 第一次恢复后仅约 80s |
| 07:03:09 | SDK auto-reconnect started | **再次断网** |
| 07:03:11 | SIM_AT:UNKNOWN \| SIM_CB:READY \| REG:-1 \| CSQ:-1 \| DownTime:3s | AT 阻塞 |
| 07:04:11 | RSRP CID:**0fd7555a** | 切换后新小区 |
| 07:08:12 | L1 triggered (303s) | |
| 07:08:15 | PDP reconnected, IP:10.2.17.199 | L1 成功 |
| 07:08:20 | Network Recovered. Down: 312s | |

**CID 变化链**：046f3ede → 0fd67c5a → 0fd7555a → 0fd76c5c（两次切换，三个不同小区）。

---

### 13.4 异常3（10:06:57 — 10:12:40，断网 ~335s）⭐ 与前两次有两处差异

**事件时序**：

| 时间 | 关键日志 | 说明 |
|------|---------|------|
| 10:05:27 | RSRP CID:`0fd69842` | 断网前小区 |
| 10:06:57 | SIM_AT:**UNKNOWN** \| SIM_CB:**READY** \| REG:-1 \| CSQ:-1 \| **DownTime:0s** | AT 阻塞，但 `start_fail_ts` **尚未置位** |
| 10:07:07 | SDK auto-reconnect started（比 AT 阻塞晚 10s） | `start_fail_ts` 此刻才置位 |
| 10:07:26 | SIM_AT:READY \| DownTime:21s | AT 接口恢复（阻塞约 29s） |
| 10:10:27 | RSRP CID:`013aee5a` | 断网期间第 1 次小区变化 |
| 10:12:10 | L1 triggered (302s) \| CGACT:1,1 | |
| 10:12:13 | **[RECOVERY L1] Start failed: -1001 (REG:1)** | `ql_data_call_start` 返回 -1001 |
| 10:12:38 | SDK 自动重连成功（dial disconnected→connected） | L1 stop 让 SDK 感知断开，25s 后 SDK 自愈 |
| 10:12:41 | RSRP CID:`05c7001e` | 恢复后第 2 次小区变化 |
| 10:12:40 | Network Recovered. Down: **335s** | 比前两次多 ~22s |

**差异1 — SIM_AT:UNKNOWN 出现在 DownTime:0s**：

前两次异常中，`SIM_AT:UNKNOWN` 出现时 DownTime 已经是 2s 或 10s（`start_fail_ts` 已置位）。异常3 中，10:06:57 的心跳打印 UNKNOWN 时 DownTime:0s——说明此时 `ping_fail_count` 还未达到阈值（<3 次），`start_fail_ts` 尚未置位。随后 10s 内 Ping 连续失败达 3 次，`start_fail_ts` 才在 10:07:07 置位并打印 SDK auto-reconnect 日志。这是 AT 接口阻塞"略早于" Ping 连续失败阈值达到的场景，逻辑上完全正常，不是 Bug。

**差异2 — L1 start 返回 -1001，由 SDK 自动重连完成恢复**：

L1 执行 `ql_data_call_stop()` 后，`ql_data_call_start()` 返回 -1001（SDK 内部尚未就绪）。代码正确处理（更新 `last_l1_ts` 防止重复猛打，不影响 `last_l2_ts`）。`stop()` 的实质效果是让 SDK 从"PDP 激活"退出，使 SDK 25s 重连定时器接管——约 25s 后（10:12:13 → 10:12:38）SDK 自行重连成功。断网总时长 335s = 303s（L1 触发）+ 2s（sleep）+ ~25s（SDK 重连等待），多出 ~22s 完全由 -1001 路径解释。与上次分析（第11节 Anomaly 1 中 `ql_data_call_start() ret=-1001`）结论完全一致。

CID 在断网期间变化两次（0fd69842 → 013aee5a → 05c7001e），是此次切换比前两次复杂的地方。

---

### 13.5 三次异常共同特征对照表

| | 异常1 | 异常2-第1次 | 异常2-第2次 | 异常3 |
|--|------|-----------|-----------|------|
| 触发 | 小区切换 PDP 僵尸 | 小区切换 PDP 僵尸 | 小区切换 PDP 僵尸 | 小区切换 PDP 僵尸 |
| SIM_CB | READY | READY | READY | READY |
| AT 阻塞时长 | ~30s | ~30s | ~30s | ~29s |
| CGACT | 1,1 | 1,1 | 1,1 | 1,1 |
| L1 触发时长 | 303s | 303s | 303s | 302s |
| L1 结果 | 直接成功 | 直接成功 | 直接成功 | **-1001→SDK续** |
| 断网总时长 | 313s | 313s | 312s | **335s** |
| [FATAL] 出现 | 否 | 否 | 否 | 否 |

---

### 13.6 根因深度分析

#### 13.6.1 AT 接口全项阻塞的原因

**现象**：SIM_AT:UNKNOWN + REG:-1 + CSQ:-1 同时出现，持续约 25-30s，SIM_CB:READY（SIM 未脱卡）。

**根因**：此设备为 **eSIM**。LTE 小区切换若跨越 Tracking Area（TA），核心网会向 SIM 下发 **REFRESH 命令**，触发 eSIM 内部重配置（重读 EF_LOCI 等文件）。在这 25-30s 内：

- EC200A 的 CP（Cortex-R5，跑协议栈）正在处理 TAU + eSIM REFRESH，AT 命令解析器处于挂起状态
- AP 发的 AT+CPIN?、AT+CEREG?、AT+CSQ 全部超时返回空响应
- 三项 AT 查询同时失败，持续时长高度一致（~25-30s）

这一阻塞不是 SIM 真正脱卡，而是 eSIM REFRESH 占用了 AT 通道。属于模组固件行为，不在我们代码控制范围内（与第12节分析结论一致）。

#### 13.6.2 PDP 僵尸态的判断依据

日志里有两处直接证据：

**证据1 — 恢复快照（L1 触发时自动打印）**：
```
[RECOVERY] PDP state: +CGACT: 1,1
```
`CGACT: 1,1` 即上下文 ID 1 处于"已激活"状态，但 Ping 持续不通。NAS 层认为 PDP 活着，数据平面实际已断。

**证据2 — 全程无 DISCONNECTED 事件**：三次异常从断网到 L1 触发，始终没有 `PDP Disconnected` 或 `PDP Idle (was connected)` 日志。正常断网 SDK 会发 DISCONNECTED 回调；僵尸态时 SDK 完全不知道链路已坏，不发回调，不启动 25s 重连定时器。

**机制**：LTE 小区切换是 RRC 层事件，切换成功后 UE 接入新小区（REG=1）。但如果新小区侧 EPS Bearer 重建失败（EPC 未及时更新 S1-AP 承载），数据平面断开而 NAS 层的 PDP 激活状态不被明确清除，SDK 因此收不到 DISCONNECTED，不重连。

---

### 13.7 根因修复方案讨论

#### 13.7.1 代码层方案（已评估，不采用）

**方案**：在 L1 触发逻辑中增加"僵尸快速通道"——检测到 `g_pdp_connected == 1` 且 `fail_duration ≥ 60s` 时提前触发 L1（将恢复时间从 ~313s 降至 ~75s）。

**识别逻辑**：正常 L0 自愈时，DISCONNECTED 回调会使 `g_pdp_connected = 0`，`start_fail_ts` 随后由 Ping 成功清零，`fail_duration` 不会超过 60s；僵尸态无 DISCONNECTED，`g_pdp_connected` 全程为 1，`fail_duration` 持续积累，能精确命中 60s 条件。

**不采用原因**（用户决策）：`ql_data_call_stop()` 会触发 `CONNECTED→IDLE` 事件，`/tmp/network_status` 写 0，上层业务感知到断连。若设备每天有 3-5 次小区切换，客户将感受到 3-5 次"主动断线"，比一次 313s 自然断网的体验更差。代码层激进干预不可取。

#### 13.7.2 两条可行出路

**出路1（治标，完全在我们控制内）**  
适当调小 `LEVEL1_TIMEOUT`，例如从当前 300s 改为 **120s**：
- 每次 zombie 事件断网时间从 ~313s 缩短到 ~135s
- 不新增断连事件次数（一次 zombie 仍只对应一次 L1 触发）
- 客户感知改善，不引入"频繁断连"问题
- 改动极小，风险低

**出路2（治本，需移远配合）**  
移远 SDK 在 EPS Bearer 重建失败后应主动发 DISCONNECTED 回调，或 SDK 内部检测到 PDP 激活但数据链路断开超过 N 秒时自动 deactivate + reactivate。这是根本修复，不依赖应用层轮询。

**当前决策**：先向移远提交问题单，不动代码；若 313s 断网时长业务上不可接受，再评估调整 `LEVEL1_TIMEOUT`。

---

### 13.8 向移远提交问题单的完整描述

**需补充信息**（提交前确认）：
- 模组型号与固件版本（`AT+GMR` 输出）
- SDK 版本（`ql_api_data_call.h` 中的版本号）

**问题单内容**：

```
模块型号：EC200A（OpenCPU 模式）
固件版本：[AT+GMR 输出]
SDK 版本：[SDK 版本]
SIM 类型：eSIM

【问题描述】
LTE 小区切换后，PDP 上下文进入"僵尸态"：
AT+CGACT? 返回 1,1（激活），但数据链路实际已中断，Ping 8.8.8.8 持续失败。
此期间 SDK 未触发 DISCONNECTED 回调，也未启动自动重连定时器。
必须应用层主动调用 ql_data_call_stop() → ql_data_call_start() 才能恢复。

【伴生现象：AT 接口阻塞】
每次小区切换事件均伴随 AT 接口全项阻塞约 25-30s：
AT+CPIN?、AT+CEREG?、AT+CSQ 三条命令同时无响应（均超时返回空）。
阻塞期间 SIM 回调缓存值仍为 READY（非真实脱卡），判断为 eSIM REFRESH 占用 AT 通道。
请确认：eSIM REFRESH 期间 AT 接口阻塞 25-30s 是否属于预期行为？

【日志证据（摘录三次复现的关键片段）】
事件1：
[06:22:36] [INFO] SDK auto-reconnect phase started
[06:22:37] [HEARTBEAT] SIM_AT:UNKNOWN | SIM_CB:READY | REG:-1 | CSQ:-1 | DownTime:2s
[06:27:40] [RECOVERY] PDP state: +CGACT: 1,1  ← PDP 仍显示激活
[06:27:40] [RECOVERY L1] Stopping Data Call...
[06:27:43] [EVENT] PDP IPv4: IF=ccinet1 IP=10.18.164.222  ← L1 stop→start 后恢复
[06:27:48] [EVENT] Network Recovered. Down: 313s

事件2（两次连续，相隔仅 2 分钟）：
[06:54:00] [INFO] SDK auto-reconnect phase started
[06:59:03] L1 triggered (303s). Network Recovered. Down: 313s
[07:03:09] [INFO] SDK auto-reconnect phase started  ← 恢复后 2 分钟再次断网
[07:08:12] L1 triggered (303s). Network Recovered. Down: 312s

事件3：
[10:07:07] [INFO] SDK auto-reconnect phase started
[10:12:10] [RECOVERY] PDP state: +CGACT: 1,1
[10:12:13] [RECOVERY L1] Start failed: -1001 (REG:1)  ← start 返回 -1001
[10:12:38] [EVENT] dial connected  ← SDK 自行重连（25s 后）
[10:12:40] [EVENT] Network Recovered. Down: 335s

（完整日志文件见附件）

【复现规律】
- 发生在 LTE 小区切换时（日志 RSRP 心跳中 CID 字段前后不同可直接确认）
- 每次伴随 AT 接口阻塞约 25-30s（CPIN/CEREG/CSQ 同时无响应）
- 切换完成后：REG=1、CSQ 正常，但 Ping 持续不通
- 全程无 DISCONNECTED 回调，SDK 不自动重连
- 通过应用层 stop() → start() 可恢复，断网总时长约 313s

【期望的 SDK 行为】
1. EPS Bearer 重建失败后，SDK 应发出 DISCONNECTED 回调，
   使应用层能感知并触发重连；
2. 或：SDK 内部检测到 PDP 激活但数据链路断开超过 N 秒时，
   自动执行 deactivate + reactivate（无需应用层干预）。

【咨询问题】
1. 以上僵尸态是 EC200A 已知问题吗？是否有固件 patch？
2. eSIM REFRESH 期间 AT 接口阻塞 25-30s 是否属于正常行为？
3. 是否有 SDK API 可主动检测"PDP 激活但数据链路不通"的状态？
```

**随问题单附上的材料**：
1. 完整日志文件（含 RSRP/CID 心跳，证明小区切换）
2. `AT+GMR` 输出（固件版本）
3. `AT+QSIMSTAT?` 输出（eSIM 状态确认）
4. 若能抓 AT 口 trace（`AT+QPRTPARA=4` 开启），附上 trace 文件

---

### 13.9 本节结论与后续待办

**结论**：
1. 设备5（1.27.2版本）三处异常全部为"LTE 小区切换 → PDP 僵尸态"模式，L1 恢复正常，程序无 Bug
2. **[FATAL] SDK 崩溃日志**全部未出现，排除 SDK 服务崩溃，`ql_data_call_set_service_error_cb` 回调诊断继续观察
3. AT 接口阻塞（~25-30s）与 PDP 僵尸态是同一次小区切换事件的两个并发现象，前者由 eSIM REFRESH 引起，后者由 EPS Bearer 重建失败引起
4. 代码层不添加激进的僵尸检测（避免引入频繁主动断连体验），治本靠移远 SDK 修复

**待办**：

| 项目 | 责任方 | 状态 |
|------|-------|------|
| 确认 AT+GMR 固件版本 + SDK 版本 | 现场 | ⏳ 待确认 |
| 向移远提交问题单（按 13.8 节模板） | 研发 | ⏳ 待发出 |
| 观察 [FATAL] 是否出现（SDK 崩溃假说验证） | — | ⏳ 等待设备运行数据 |
| 若 313s 断网时长不可接受，评估调整 LEVEL1_TIMEOUT=120s | 研发 | ⏳ 待业务侧确认需求 |
| 设备3 频繁整机重启根因排查（见第10节） | 现场 | ⏳ 待查 sys_reboot.log / dmesg |

小区切换（从 0fd5dc40 切换到 0f5c9d01），触发 AT 接口阻塞约 25s（SIM_AT:UNKNOWN + REG:-1 + CSQ:-1 同时，SIM_CB:READY 确认非真实脱卡），切换后疑似 PDP 进入僵尸态（CGACT:1,1 但网络不通，证明数据链路实际已中断），SDK自动重连机制未恢复网络（怀疑是不是 SDK 无DISCONNECTED 回调，就不自动重连），L1 在 303s 主动 stop→start，恢复正常。多次触发该现象，表象基本一致。

---

## 14. 移远诊断日志采集：需求分析与实现（2026-05-14）

> **背景**：第 13 节完成设备5日志分析后，确定根因为"LTE 小区切换 → PDP 僵尸态 + eSIM REFRESH 导致 AT 接口阻塞"，需向移远提交问题单。移远反馈在提交时需附上 `dmesg`（内核 ring buffer）和 `logcat`（CP 侧日志缓冲区）作为诊断材料。本节记录从需求澄清、方案论证到代码落地的完整过程。

---

### 14.1 移远的具体要求

移远要求的原始描述：
> "cat /proc/kmsg > /xxx & logcat -v time > /xxx &  开机就在后台运行着两条指令，分别重定向到文件里面，异常之后导出看下"

即：在设备开机时启动两个后台进程，持续将内核日志和 CP 日志写入文件，等故障发生后再导出。

首先在实际设备上验证这两条命令是否可用：

```bash
# 验证 dmesg
dmesg | head -5          # 正常输出内核启动日志

# 验证 logcat
timeout 3 logcat -v time | head -20
# 结果：输出了大量 "+++ LOG: write failed (errno=32)"
```

`logcat` 测试时报了 `errno=32 EPIPE`，看起来像错误。分析根因：`head -20` 读够 20 行后关闭管道，`logcat` 向已关闭的管道继续写入时收到 `SIGPIPE`，errno=32 是管道关闭的正常行为，**不是 logcat 本身的问题**。用文件验证：

```bash
logcat -d -v time > /tmp/lc.log   # 直接写文件
# 结果：3 秒内正常输出 156.2K，无任何报错
```

确认：两条命令均可在设备上正常运行，`logcat` 还支持 `-r`（轮转大小）和 `-n`（保留文件数）参数。

---

### 14.2 持续采集方案的问题

按移远原始建议，在 dial 启动时 double-fork 两个后台进程持续采集：

```bash
cat /proc/kmsg > /media/sdcard/diag/kmsg.log &
logcat -v time -f /media/sdcard/diag/logcat.log -r 2048 -n 4 &
```

实际评估后发现**日志量不可控**：

- `/proc/kmsg` 是内核 ring buffer 的**流式字符设备**，每条 printk 都会追加进来，持续写入无上限，实际测量一天超过 100MB
- `logcat` 持续运行约 50MB/天
- 两路合计 **~150MB/天**，几天就会撑满 SD 卡日志分区

此外还需要管理两个长期运行的后台进程（double-fork、记录 PID、周期性检查存活），大幅增加代码复杂度。

---

### 14.3 方案转变：事件驱动快照

关键洞察：`dmesg` 本质上是一个内核在内存中维护的**循环缓冲区**（约 512KB），记录着开机以来所有 `printk` 输出。只要不被覆盖，故障发生前几分钟的内核事件**始终保存在缓冲区里**，不需要提前持续采集。

基于这个特性，改为在 `dial_loop()` 状态机的两个关键时刻主动执行一次快照：

```
正常运行
    │
    ▼ 连续 3 次 Ping 失败，start_fail_ts 首次置位
┌─ diag_snapshot("fault")
│   dmesg 此刻包含：故障前数分钟的小区切换、eSIM REFRESH 等内核事件
│   logcat 此刻包含：CP 侧故障前的完整日志
│
│   ... 程序继续运行，等待 L1/L2 恢复 ...
│
▼ Ping 恢复成功，start_fail_ts != 0
└─ diag_snapshot("recovery")
    dmesg 此刻包含：从故障到恢复的完整内核事件序列
    logcat 此刻包含：包含故障+恢复全过程的 CP 日志
```

两次快照都是**一次性导出后立即退出**（`dmesg` 直接导出，`logcat -d` dump-and-exit），不需要后台进程，不阻塞主循环。

| 对比项 | 持续采集 | 事件驱动快照 |
|-------|---------|------------|
| 单次故障数据量 | 无上限（~150MB/天） | ~400~600KB（4个文件） |
| 进程管理 | 需 double-fork + 监控 | 无（同步 system() 调用） |
| 能否覆盖故障前历史 | 是（前提：采集已在运行） | 是（靠 dmesg ring buffer 天然覆盖） |
| 故障期间进程崩溃时 | 数据已落盘 | 无 fault 快照（可接受，有现有 DIAG 日志兜底） |

结论：两个方案对移远诊断的有效性相当，事件驱动快照在资源消耗和实现复杂度上大幅优于持续采集，选定方案B。

---

### 14.4 diag_snapshot() 实现

在 `dial.c` 中新增静态函数（位于 `main()` 前，约第 362 行）：

```c
static void diag_snapshot(const char *label)
{
    if (access("/media/sdcard", F_OK) != 0) return;

    const char *diag_dir = "/media/sdcard/diag";
    if (mkdir(diag_dir, 0755) != 0 && errno != EEXIST) {
        dial_log("[DIAG] mkdir %s failed: %s, skip snapshot.\n", diag_dir, strerror(errno));
        return;
    }

    char ts[32];
    time_t t = time(NULL);
    struct tm tm_info;
    localtime_r(&t, &tm_info);
    strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &tm_info);

    char cmd[400];

    /* dmesg：内核 ring buffer 全量，覆盖故障前数分钟历史 */
    snprintf(cmd, sizeof(cmd),
             "dmesg > '%s/dmesg_%s_%s.log' 2>/dev/null", diag_dir, label, ts);
    system(cmd);

    /* logcat -d：CP 侧日志缓冲区全量导出后退出，不阻塞 */
    snprintf(cmd, sizeof(cmd),
             "logcat -d -v time > '%s/logcat_%s_%s.log' 2>/dev/null", diag_dir, label, ts);
    system(cmd);

    dial_log("[DIAG] Snapshot saved: %s/[dmesg|logcat]_%s_%s.log\n", diag_dir, label, ts);
}
```

**输出文件结构**（以一次故障事件为例）：

```
/media/sdcard/diag/
  ├── dmesg_fault_20260513_060721.log      ← 故障确认时的内核日志（含故障前历史）
  ├── logcat_fault_20260513_060721.log     ← 故障确认时的 CP 日志
  ├── dmesg_recovery_20260513_061556.log   ← 网络恢复时的内核日志（含完整故障过程）
  └── logcat_recovery_20260513_061556.log  ← 网络恢复时的 CP 日志
```

**mkdir 的错误处理细节**：初版直接忽略 mkdir 返回值，SD 卡不可写时 mkdir 失败，后续 `system()` 重定向静默失败，却仍打印 "Snapshot saved" 误导诊断。修复：`errno == EEXIST` 放行（目录已存在是正常情况），其他错误记录日志并提前返回。

---

### 14.5 与状态机的集成点

两处调用均在 `dial_loop()` 的 `ST_PING` 分支内，与现有诊断逻辑对齐：

**调用点1：故障确认时（label="fault"）**

位置：`start_fail_ts` 首次被置位，且 `!diag_snap_done` 守卫确保每次故障只抓一次。挂在现有 `[DIAG]` AT 命令快照之后：

```c
if (!diag_snap_done) {
    diag_snap_done = 1;
    // 现有：抓 CESQ / CREG / CGPADDR / QTEMP / CEER / CGACT → dial_log("[DIAG] ...")
    diag_snapshot("fault");   // 新增：dmesg + logcat 快照
}
```

**调用点2：网络恢复时（label="recovery"）**

位置：Ping 成功且 `start_fail_ts != 0`，在清零 `start_fail_ts` 之前：

```c
if (start_fail_ts != 0) {
    // 现有：打印恢复日志、next_ext_heartbeat_ts=0
    diag_snapshot("recovery");    // 新增：dmesg + logcat 快照
}
start_fail_ts = 0;
diag_snap_done = 0;   // 清标志，允许下一次故障重新采集
```

`diag_snap_done` 的作用：故障期间 L1/L2 可能多次触发，没有这个标志 fault 快照会被反复覆盖写入。置位后直到网络恢复（`start_fail_ts = 0` 时清零）才允许下一次故障再采集。

---

### 14.6 本节结论与后续待办

**结论**：

1. 移远要求的 dmesg + logcat 通过事件驱动快照方案满足，每次故障产生 4 个文件约 400~600KB，远优于持续采集（~150MB/天）
2. 核心依据：dmesg ring buffer 天然保存故障前内核历史，无需提前开始采集
3. `diag_snap_done` 标志确保每次故障只抓一次 fault 快照，`recovery` 快照包含完整故障+恢复过程，是提交给移远最有价值的文件
4. 代码编译通过，版本拟更新为 v1.27.3

**推荐 commit 描述**：

```
[NEWFUNC] 新增故障诊断快照（diag_snapshot），版本 v1.27.3

在故障触发（start_fail_ts 首次置位）和网络恢复两个时间点各抓取一次
dmesg + logcat -d 快照，输出至 /media/sdcard/diag/，文件名含标签
（fault/recovery）和时间戳，供移远分析 PDP 僵尸态 + AT 接口阻塞问题。
采用事件驱动而非持续采集，单次故障约 400~600KB，不影响 SD 卡空间。
```

**待办**（继承自第 13.9 节，状态更新）：

| 项目 | 状态 |
|------|------|
| 确认 AT+GMR 固件版本 + SDK 版本 | ⏳ 待现场确认 |
| 向移远提交问题单（按 13.8 节模板），附 dmesg/logcat 文件 | ⏳ 待设备产出快照文件后发出 |
| 观察 [FATAL] SDK 崩溃回调是否出现 | ⏳ 等待设备运行数据 |
| 若 313s 断网时长不可接受，评估调整 LEVEL1_TIMEOUT=120s | ⏳ 待业务侧确认 |
| 设备3 频繁整机重启根因排查（sys_reboot.log / dmesg / watchdog） | ⏳ 待现场 |
