# 02 — dial.c + dial.h

## 一、文件职责概述

`dial.c` 是**运行期核心**：进入 `dial_loop()` 后永不退出，实现主状态机（ST_STATUS→ST_SIM→ST_SIGNAL→ST_PING→ST_RECOVERY）、分级恢复（L1/L2/L3）、Fast-Retry 快速失败、以及三个 SDK/SIM 回调。`dial.h` 定义状态枚举 `dial_stat_enu`、恢复档位 `RecoveryLevel`、运行期结构 `dial_mng_t`，并声明共享全局。

> 文件头注释仍写 `@file main_apn.c` / Quectel sample（`dial.c:1-21`），是历史遗留，与实际内容无关。

## 二、关键数据结构、全局变量、宏

### dial.h
| 名称 | 值/定义 | 含义 |
|---|---|---|
| `DIAL_TIMEOUT_SECONDS` | 120 | 声明存在，dial.c 未使用（`dial.h:10`） |
| `DILA_IFNAME_TIMEOUT_CNT` | 10 | 同上（`dial.h:11`） |
| `EXIT_COUNT_FILE` | `/tmp/exit_count.txt` | 退出计数文件（`dial.h:13`） |
| `DODIAL_TIMEOUT_SECONDS` | 60 | 声明存在，未在 dial.c 使用（`dial.h:15`） |
| `dial_stat_enu` | none/call_init/apn_init/call_create/call_start/stop/stop_cfun/start_cfun/wait_for_connect/net_connected | 旧状态枚举，dial_loop 实际用的是本文件内 `enum Stage`，此枚举仅 `p_dial_mng->dial_st` 赋值用（`dial.h:17-29`） |
| `RecoveryLevel` | NONE=0/L1=1/L2=2/L3=3 | 分级恢复档（`dial.h:32-37`） |
| `dial_mng_t` | `{dial_st, level, dial_timer}` | 大部分字段已注释掉（`dial.h:39-50`） |

### dial.c 全局/静态
| 名称 | 初值 | 含义 |
|---|---|---|
| `p_dial_mng` | NULL | 运行期状态指针，main 分配（`dial.c:54`） |
| `g_sigint_received` | 0 | SIGINT 标志（`dial.c:55`） |
| `g_dev_name[32]` | `"ccinet0"` | **声明存在但 dial_loop 内未使用**（`dial.c:59`） |
| `enable_policy_recovery` | 1 | 应用层策略总开关（`dial.c:65`） |
| `g_last_reg_stat` | -1 | 心跳缓存的 REG 状态（`dial.c:68`） |
| `g_sim_app_ready` | -1 | SIM READY: -1未初始化/0非READY/1READY（`dial.c:72`） |
| `g_sim_app_state` | 0 | SIM app_state 整数（`dial.c:73`） |
| `g_sdk_service_error` | 0 | CP 服务崩溃标志（`dial.c:75`） |
| `PING_FAIL_THRESHOLD` | 3 | ping 连续失败达此值才起计时（`dial.c:78`） |
| `RETRY_COUNT_FILE` | `/tmp/dial_retry_count` | Fast-Retry 计数文件（`dial.c:131`） |
| `MAX_FAST_RETRY_TIMES` | 3 | 最大快速重试次数（`dial.c:132`） |
| `FAST_FAIL_TIMEOUT_MS` | 10000 | PDP 建立后 Ping 成功窗口（`dial.c:133`） |
| `PDP_WAIT_TIMEOUT_MS` | 60000 | PDP 建立最长等待（`dial.c:134`） |

`enum Stage { ST_STATUS=0, ST_SIM, ST_SIGNAL, ST_PING, ST_RECOVERY }`（`dial.c:36`）。

## 三、逐函数分析

### 回调（SDK 内部线程触发，只写 volatile 标志）
- **`sim_card_status_cb(slot, p_info)`**（`dial.c:97-107`）：`p_info==NULL` → 置 `UNKNOWN`/ready=0；否则读 `p_info->app_3gpp.app_state`，READY 置 ready=1。注释强调不得在此调 SDK API。
- **`data_call_service_error_cb(int error)`**（`dial.c:114-118`）：忽略 error，仅 `g_sdk_service_error=1`。对应 CP 侧服务崩溃 QL_ERR_ABORTED。
- **`on_network_connected(void *user_data)`**（`dial.c:121-128`）：printf + `update_network_status(1)`。这是 main 传给 dial_loop 的 on_connected。

### Fast-Retry 计数
- **`check_and_update_retry_count()`**（`dial.c:139-169`）：读 `/tmp/dial_retry_count`，`>= 3` 返回 -1（不再快速退出）；否则 count++ 写回（`fflush`+`fsync`+`fclose`），返回新值。
- **`clear_retry_count()`**（`dial.c:173-179`）：文件存在则 `unlink` 并打日志。ping 成功时调用。

### 诊断辅助（popen 抓 AT/dmesg）
- **`log_sim_disconnect_diag()`**（`dial.c:182-194`）：popen `at+qsimstat?`、`at+cereg?`、`dmesg|grep sim`，各取一行，打 `[DIAG-SIM]`/`[DIAG-DMESG]`。取不到打 `TIMEOUT`/`(none)`。
- **`log_sim_recovery_diag()`**（`dial.c:197-207`）：popen `at+qsimstat?` + dmesg，打 `[DIAG-SIM-REC]`/`[DIAG-DMESG-REC]`。
- **`log_recovery_snapshot()`**（`dial.c:210-221`）：`get_ceer_safe`（最近错误）+ `get_cgact_safe`（PDP 态），非空则打 `[RECOVERY]`。
- **`print_init_info()`**（`dial.c:224-266`）：依次 `get_imei_safe/get_cgmr_safe/get_csub_safe/get_zcgmr_safe/get_acgmr_safe/get_imsi_at_safe/get_nw_mode_pref_safe`，打 IMEI/FW/SUB/AP_FW/CP_FW/IMSI/NW mode。Operator/CFUN/PDP cfg 三段被注释掉。

### `void dial_loop(void (*on_connected)(void*), void *user_data)`（`dial.c:269-876`）
永不正常返回（仅通过 exit 或 SIGINT 退出）。

#### 初始化段（`dial.c:271-394`）
1. Fast-Retry：`check_and_update_retry_count()`→`current_launch_count`；`is_fast_fail_mode = (count != -1)`；`start_loop_ts = now_ms()`（`dial.c:276-284`）。
2. 拨号参数：`apn_id=DATA_CALL_APN_PUBLIC(6)`、`ip_ver=QL_NET_IP_VER_V4`、`reconnect_interval=25`、`call_name="auto_network"`（`dial.c:293-298`）。**符合 CLAUDE.md 两条强约定**。
   - **注意**：注释写"设为20秒"，实际值 `25`（`dial.c:297`），注释与代码不符。
3. `ql_data_call_init()` 重试循环：最多 200 次，`ret==-1001` 时每次 `usleep(100ms)` 重试（每 20 次打进度），其它值 break（`dial.c:303-317`）。之后按 `ret != QL_ERR_OK` 打 OK/failed。
   - **潜在点**：若首次返回非 -1001 的错误码（如 -1),循环 break，`ret` 非 OK 只打日志不退出，继续走后续初始化。
4. `ql_data_call_set_status_ind_cb(data_call_status_ind_cb)`、`ql_data_call_set_service_error_cb(data_call_service_error_cb)`（`dial.c:324-325`）。
5. `sim_init()`：失败仅 `sleep(2)` 退避（`dial.c:327-332`）。
6. `sim_get_iccid(iccid)`，打 ICCID（`dial.c:333-335`）。
7. `ql_sim_set_card_status_cb(sim_card_status_cb)`（`dial.c:338`）。
8. 主动查一次 `ql_sim_get_card_info(QL_SIM_SLOT_1, &card_info)`，成功写 `g_sim_app_state/g_sim_app_ready` 并打初始态；失败告警将回落 AT+CPIN?（`dial.c:341-351`）。
9. `print_init_info()`（`dial.c:353`）。
10. `set_apn(iccid)`：OK/失败打日志（`dial.c:355-362`）。
11. `ql_data_call_create(g_call_id=4, "auto_network", 0)`（`dial.c:365`）。**注意：g_call_id 硬编码 4，而非 DATA_CALL_ID_PUBLIC(1)**（见潜在问题）。
12. `ql_data_call_param_alloc()`→ 设 apn_id、ip_version、`set_reconnect_mode(NORMAL)`（打 OK/FAIL）、`set_reconnect_interval({25,0},1)`、`ql_data_call_config(4, p_cfg)`（打 OK/FAIL）、`param_free`（`dial.c:367-388`）。alloc 失败告警。
13. `ql_data_call_start(g_call_id=4)`，非 0 打 failed（`dial.c:390-394`）。

#### 状态机变量（`dial.c:398-432`）
- 间隔常量：`PING_INTERVAL_MS=1500`、`SIM_INTERVAL_MS=800`、`SIG_INTERVAL_MS=800`、`HEARTBEAT_INTERVAL_MS=30000`（`dial.c:401-404`）。
- 阈值：`LEVEL1_TIMEOUT=5min`、`LEVEL2_TIMEOUT=10min`、`LEVEL3_TIMEOUT=35min`（`dial.c:408-410`）。**与 CLAUDE.md 分级恢复段一致**。
- 计时器：`start_fail_ts`、`last_l2_ts`、`last_l1_ts`、`recovery_level`、`ping_fail_count`、`has_notified_connect`、`has_connected_once`、`gated_notice_done`、`pdp_ready_ts`、`sim_error_active`（`dial.c:412-432`）。

#### 主循环 `while(1)`（`dial.c:434-875`）

**(0) SIGINT 处理**（`dial.c:436-445`）：`teardown_cp_dump_capture()` → free(p_dial_mng) → `log_close()` → `exit(0)`。

**(1) SDK 服务崩溃**（`dial.c:450-456`）：置位则清零并打 `[FATAL]` 日志。**注意：`log_close(); exit(1);` 两行被注释掉**（`dial.c:454-455`）——即当前**检测到 CP 服务崩溃只打日志，不主动退出**（与注释描述的"主动退出交 keepalive 重拉"行为不符，是当前实际行为的偏差，见潜在问题）。

**(2) Fast Fail 检测**（`dial.c:466-489`）：仅 `is_fast_fail_mode && !has_notified_connect` 时生效：
- `g_pdp_connected` 为真：首次记 `pdp_ready_ts`，`tnow-pdp_ready_ts > 10s` → `log_close(); exit(1)`。
- 否则：复位 `pdp_ready_ts=0`，`tnow-start_loop_ts > 60s` → `exit(1)`。

**(A) 30s 心跳**（`dial.c:494-520`）：`get_cpin_status_str`+回调态、`get_csq_value_safe`、`get_cereg_status_safe`（写 `g_last_reg_stat`）、`get_cpu_temp`、断网时长，打 `[HEARTBEAT]`。`reg_stat==3` 额外打 `[WARNING] Registration Denied`。

**(A2) 5min 扩展心跳**（`dial.c:527-545`）：仅 `has_notified_connect` 后生效。`get_cesq_safe/get_creg_safe/get_cgpaddr_safe` → `build_ext_line` → 打 `[HEARTBEAT]`。

**(B) 业务状态机 switch**（`dial.c:550-872`）：
- `ST_STATUS`：直接转 ST_SIM（`dial.c:551-553`）。
- `ST_SIM`（`dial.c:555-605`）：节流 `SIM_INTERVAL_MS`。优先用回调标志判 READY，回调未初始化则 `get_cpin_status_str` 回落。
  - `!sim_ready`：若 `has_notified_connect` 为真（即运行中掉卡）→ 打 `[ERROR] SIM disconnected` + `log_sim_disconnect_diag()`，置 `has_notified_connect=0`、`ping_fail_count=0`、`sim_error_active=1`（`dial.c:574-585`）。`start_fail_ts==0` 则置 `tnow`（`dial.c:586`）。
  - `sim_ready && sim_error_active`：打 `[INFO] SIM recovered` + `log_sim_recovery_diag()`，`sim_error_active=0`（`dial.c:587-600`）。注释说明"立即强制 L1"逻辑已移除（对齐 CLAUDE.md v1.28.3）。
  - 转 ST_SIGNAL。
- `ST_SIGNAL`（`dial.c:607-611`）：节流 `SIG_INTERVAL_MS` 后转 ST_PING。**本阶段实际不做任何信号读取动作**（仅节流+转移）。
- `ST_PING`（`dial.c:613-779`）：节流 `PING_INTERVAL_MS`。`test_can_ping_google(NULL)`：
  - **成功**（`dial.c:617-657`）：`ping_fail_count=0`；若 `start_fail_ts!=0` → 计算 down_sec，按 `< LEVEL1_TIMEOUT` 打 L0 恢复否则打普通恢复，`next_ext_heartbeat_ts=0`；`has_connected_once` 时 `diag_snapshot("recovery")`。随后清 `start_fail_ts=0`、`diag_snap_done=0`、`gated_notice_done=0`、`last_l1_ts=0`、`recovery_level=NONE`；`clear_retry_count()`；退出 fast_fail 模式；**`has_connected_once=1`**（解锁分级恢复）；若未通知则 `on_connected(user_data)`、`has_notified_connect=1`。
  - **失败**（`dial.c:658-777`）：`ping_fail_count++`，`< 3` 则转 ST_STATUS 跳过（`dial.c:663-667`）。达阈值：
    - `start_fail_ts==0` 时置 `tnow` 并打 SDK 自动重连期开始日志（`dial.c:670-678`）。
    - `!diag_snap_done` → 置 1，抓 6 项 AT（cesq/creg/cgpaddr/qtemp/ceer/cgact），`build_ext_line` 打 `[DIAG]`；`has_connected_once` 时 `diag_snapshot("fault")`（`dial.c:685-708`）。**门控用 `diag_snap_done`，符合 CLAUDE.md 约定**。
    - `fail_duration = tnow - start_fail_ts`；分级判断（见下）。

#### 分级判断（`dial.c:719-762`）—— **核心恢复逻辑**
门控条件：`enable_policy_recovery==1 && has_connected_once`（`dial.c:719`）。符合 CLAUDE.md v1.28.3。
- **L3**：`fail_duration > 35min` → RECOVERY_L3（`dial.c:723-725`）。
- **L2**：`fail_duration > 10min` 或 `(g_last_reg_stat==0 && fail_duration > 5min)`（`dial.c:732-733`）。冷却：`REG==0` 时 90s，否则 5min；`tnow-last_l2_ts > cooldown` 才置 L2（`dial.c:734-739`）。
- **L1**：`fail_duration > 5min && g_last_reg_stat != 0`（`dial.c:746`）；`tnow-last_l1_ts > 60s` 才置 L1（`dial.c:747-749`）。**REG=0 时跳过 L1**。
- **门控关闭自证日志**（`dial.c:755-762`）：`enable_policy_recovery==1 && !has_connected_once && fail_duration > 10min && !gated_notice_done` → 打 `[INFO] never-connected … gated`，每周期一次。符合 CLAUDE.md。
- 触发任一级 → 打 `[ALARM]`，转 ST_RECOVERY（`dial.c:771-776`）；否则转 ST_STATUS。

#### ST_RECOVERY（`dial.c:782-871`）
`log_recovery_snapshot()` 后按 recovery_level：
- **L1**（`dial.c:789-813`）：`ql_data_call_stop(4)` → `sleep(2)` → `ql_data_call_start(4)`。start 失败：`last_l1_ts=tnow` 后 break（不更新 last_l2_ts）。成功：`last_l1_ts=tnow`，`sleep(3)`。
- **L2**（`dial.c:815-841`）：`last_l2_ts=tnow`、`last_l1_ts=tnow`；`system("serial_atcmd at+cfun=0")` → `sleep(3)` → `system("serial_atcmd at+cfun=1")` → `sleep(10)` → `ql_data_call_start(4)`。ret==0 打 OK，ret==-1013 打"already connected"，其它打 failed。
  - **注意**：L2 用 `system("serial_atcmd at+cfun=0/1")`（外部命令），而非 `Ql_SendAT`；main.c 的 restart_cfun 用的是 `Ql_SendAT`。两条路径不同。
- **L3**（`dial.c:843-862`）：打 `[RECOVERY L3] FATAL` → `sync()` → `log_close()` → `exit(1)`。cfun=1,1 整机重启代码被注释（v1.28.0 起纯 exit，符合 CLAUDE.md）。
- 结束：`has_notified_connect=0`，转 ST_STATUS（`dial.c:869-870`）。

**循环尾**：`usleep(50ms)`（`dial.c:874`）。

## 四、关键分支/阈值定位表

| 位置 | 内容 |
|---|---|
| `dial.c:466-489` | Fast-Fail：PDP 已连 10s 无 ping / PDP 60s 未建立 → exit(1) |
| `dial.c:663` | ping 抖动过滤阈值 3 |
| `dial.c:719` | 分级恢复门控 `enable_policy_recovery && has_connected_once` |
| `dial.c:723/732/746` | L3>35min / L2>10min(或REG0>5min) / L1>5min(REG≠0) |
| `dial.c:734-736` | L2 冷却 REG0=90s / 正常=5min |
| `dial.c:747` | L1 节流 60s |
| `dial.c:755-762` | 门控关闭自证日志阈值 >10min |
| `dial.c:861` | L3 exit(1) |

## 五、潜在问题 / 需实测确认

1. **`g_call_id` 硬编码 4**（`dial.c:273,365,391,793,798,832` 等），而 `DATA_CALL_ID_PUBLIC=1`（_public.h:36）。create/config/start/stop 全用 4，内部自洽；但与宏定义的"公网 call id=1"不一致。**需确认 SDK 对 call_id 取值是否有约束、apn_id=6 与 call_id=4 组合是否为预期**（代码未体现 call_id 必须=1）。
2. **CP 服务崩溃处理被注释**（`dial.c:454-455`）：`data_call_service_error_cb` 置位后，主循环只打 `[FATAL]` 日志，`log_close()/exit(1)` 被注释掉，**实际不会主动退出**。与函数注释、CLAUDE.md 描述的"主动退出交 keepalive 重拉"不符。**需确认是有意保留还是遗漏**。
3. **注释与代码数值不符**：`reconnect_interval` 注释"设为20秒"实为 25（`dial.c:297`）。
4. **`ST_SIGNAL` 阶段空转**：只节流转移，未做任何信号采集（信号读取在心跳里做）（`dial.c:607-611`）。
5. **旧枚举 `dial_stat_enu` 与 `enum Stage` 并存**：状态机实际只用 `enum Stage`，`p_dial_mng->dial_st` 仅在 main 赋值 none，运行期不参与逻辑。
6. **`g_dev_name`（`dial.c:59`）声明后 dial_loop 未使用**。
7. Fast-Fail 的 60s/10s 窗口在 `is_fast_fail_mode && !has_notified_connect` 下才生效，退出 fast 模式后不再快速退出——长期驻留模式下 PDP 长期不建立不会 exit，只靠 L1/L2/L3（需 has_connected_once，从未连上则完全不动）。**需确认从未连上且长期无覆盖场景是否符合预期（安静等待 SDK 自连）**——CLAUDE.md 明确这是有意设计。
8. 所有 `system("serial_atcmd …")` / popen 诊断为阻塞外部进程调用，L2 期间主循环阻塞约 16s（sleep 3+10+命令耗时），期间不响应 SIGINT（需实测阻塞窗口对 SIGINT 及时性的影响）。
