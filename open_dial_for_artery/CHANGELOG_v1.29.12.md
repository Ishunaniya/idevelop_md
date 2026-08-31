# Changelog — dial 拨号程序升级包

---

## v1.29.12

**固件提供日期**：2026-06-25
**升级包版本**：`roamlink_deploy_v1.29.12`
**对应仓库**：`open_dial_for_artery`（分支 `feature/dual_channel_switch`）+ `roamlink_deploy`
**版本号来源**：`main.c` 的 `MAIN_VERSION.SUB_VERSION.TEST_VERSION` 宏（1.29.12）

> 本文是开发交付的提测说明，描述当前版本「行为是什么样、怎么配置、怎么观察」的完整面貌。
> 测试场景与用例由测试侧依据本文自行设计，不在此罗列。

> **本版（v1.29.12）相对此前已发布提测的 v1.29.07 的行为变更摘要**（v1.29.8~v1.29.12 累计）：
> 1. **RBMaster 存活判定改为纯 `/proc` 判活**：移除了端口 5568 连通性验证——RBMaster 每条控制命令后会主动断开连接，端口本不持续监听，端口探测会把健康进程误判为僵尸，导致反复 `fork()`、进程堆积。
> 2. **`/tmp/network_type=0`（切换中）可观测窗口变长**：Roamlink→SIM 切换时，从下发 `RBstopServiceMaster` 起一直保持为 0，直到 SIM 重新拨号成功才写回 1，覆盖整段重拨窗口（而不是只在极短瞬间为 0）。
> 3. **license 下载超时清理**：超时且已启动过 RBMaster 时，新增停服务+重拨清理；此前 RBMaster 会一直占住通道，导致物理 SIM 拿到全 0 IP/DNS，域名解析直到重启才恢复。
> 4. **FORCE_ROAMLINK（策略3）license 缺失分支不再临时降级 FORCE_SIM**：该策略设备本无物理 SIM，降级没有意义，改为记录日志等待人工处理。
> 5. **`start_call` 失败不再永久卡死状态机**（高危修复）：`QL_Data_Call_Start()` 失败后状态机现在会继续重试，而不是卡死在 `dial_stat_start_call`（看门狗只管进程存活，发现不了这种"进程活着但卡死"的状态）。
> 6. **AT 层两处防护**：串口未打开时 `Ql_SendAT` 直接判失败（不再有栈越界写）；`+CME/+CMS ERROR`/`ERROR` 应答现在立即返回失败，不再被当无关 URC 丢弃并空等满超时（最长 180s，期间会阻塞 main 线程 CSQ 心跳，因为两个线程共享同一把串口锁）。
> 7. **RBMaster 子进程僵尸回收**：新增定向 `waitpid(WNOHANG)`，避免策略3 反复重启 Roamlink 时僵尸进程累积耗尽 PID 空间。
> 8. **时区文件防污染**：`AT+QLTS?` 返回空应答（拨通初期 NITZ 未下发时常见）时不再把未初始化栈内容写入 `/usr/dial/tz.ini`。
> 9. **APN 对象内存泄漏修复**：通道切换 / 300s 回切重试时 `p_apn_obj` 置 NULL 前未释放，每次切换泄漏一个 `apn_obj_t` + 运营商 deque；现统一走 `apn_obj_free()`。
> 10. **日志重构（v1.29.10 起）**：CSQ 不再每 5s 刷屏，改为 60s/300s 两级 `[HEARTBEAT]` + 启动一次性 `[INIT]`。

---

### 一、进程与职责

- `dial` 是**唯一负责拨号与 Roamlink 通道管理的进程**，由 `check_network.sh` 看门狗每 30s 检查存活、挂了就重启（看门狗不参与任何切换判断，纯保活，由 `/etc/rc5.d/S60start_check_network` 开机拉起）。
- 进程内两个线程，共享 AT 串口 fd（`/dev/smd8`），由 `g_at_port_mutex` 互斥保护：
  - **主线程**：5s 一拍，调用 `nw_at_get_csq()` 刷新 `p_dial_mng->csq` 并写 `/tmp/network_csq`。不再逐拍打日志，改为每 **60s** 打一条 `[HEARTBEAT]`（state/csq/tcp_fail/rl_fail，标量读取，不取通道/策略——通道可由 state 推出，策略全程不变），每 **300s** 打一条扩展版（同 tag，靠字段区分：cereg + 小区 cellid/pci/tac + 信号 rsrp/rsrq + ifname/ip/rx_packets）。IP/ifname 来自主线程自己对 `rmnet*` 的 `getifaddrs()` 扫描，不会跨线程读取 `dial_task` 共享的 `p_ifname`/`p_ipv4_data_call_info` 指针（避免 use-after-free）。
  - **`dial_task` 线程**：拨号状态机全流程——SIM/Roamlink 拨号、TCP 测试、通道切换、状态文件写出。每轮循环尾部 `sleep(1)` 兜底，各状态内部再按需 `sleep(2~10s)` 控制重试/轮询节奏（例如 `roamlink_starting` 每 10s 测一次、`net_connected`/`roamlink_active` 每 5s 跑一轮）；状态切换时额外打一行 `SEAS_LOG_INFO("state: %s -> %s", ...)`（裸字符串，无方括号标签）。
- 启动信息仅打印一次：`[INIT] policy=...`、IMEI（`AT+CGSN`）、固件版本（`AT+QGMR`）；首次选定 APN 时额外打一条 `[INIT] APN: ...`（`dial_stat_pre_start_call` 内，每次重新查表都会打一次，不仅限启动时）。
- `/tmp/dial_status` 每 **30s** 周期性刷新一次（不依赖状态是否变化），状态切换点会额外立即调用一次，所以实际可见延迟 ≤30s。
- **进程间通信**：不依赖 IPC，全部通过 `/tmp/network_*`、`/tmp/dial_status` 等文件落地，供外部脚本/监控读取。

---

### 二、四种网络策略（`/usrdata/network.ini`）

```ini
[network]
network_select = 4   # 1=优先Roamlink  2=优先SIM  3=强制Roamlink  4=强制SIM（默认）
```

| 值 | 名称 | 行为概述 |
|---|---|---|
| 1 | PREFER_ROAMLINK | 优先 Roamlink，失败切物理 SIM 备用，SIM 稳定 300s 后主动回切 Roamlink |
| 2 | PREFER_SIM | 优先物理 SIM，失败切 Roamlink 备用，Roamlink 稳定 300s 后主动回切 SIM |
| 3 | FORCE_ROAMLINK | 强制 Roamlink，失败只重启 Roamlink 服务，**永不切 SIM** |
| 4 | FORCE_SIM | 强制物理 SIM，失败只重拨，**永不切 Roamlink**（代码内 `NET_POLICY_DEFAULT`，文件读不到/非法时兜底） |

`network.ini` 仅在 `dial_mng_new()` 启动时读取一次，运行中不重读；改后需重启 `dial`（`killall dial`，看门狗会自动拉起，或等下次开机）才生效。

**启动时的额外保护**：若策略是 PREFER_SIM/FORCE_SIM 但发现 RBMaster 已在运行（例如上一个 `dial` 进程在 Roamlink 状态被强杀），会先调用 `roamlink_stop_service()` 再进状态机，否则模组会停留在虚拟 SIM 模式，`sim_op_handler()` 读到的是 Roamlink 的 ICCID，APN 库匹配不到。

---

### 三、通道切换状态机（核心行为逻辑）

#### 完整状态序列

```
dial_stat_none
  │ 决定初始通道（策略 / roamlink_available / license_pending）
  ▼
── SIM 路径 ──────────────────────────────────────────
dial_stat_init               QL_MCM_NW_Client_Init()（每次运行只调一次）
dial_stat_sim_init           sim_op(SIM_OP_INIT)
dial_stat_sim_check          nw_get_sim_card_status()；超时 3600s → stop_cfun
dial_stat_sim_op             读 ICCID
dial_stat_reg_check          nw_reg_status_check()（PS 域）；超时 300s（与 cereg_check 共享）
dial_stat_cereg_check        AT+CEREG? stat=1 或 5
dial_stat_precondition_check QL_Data_Call_Init_Precondition()
dial_stat_pre_start_call     apn_get_apn_obj(iccid) + 设默认 Profile
dial_stat_start_call         QL_Data_Call_Start()（失败不再卡死，见上方变更摘要第5点）
dial_stat_wait_for_connect   轮询连接状态；超时 60s：is_oper_select → list_oper，否则先发 AT+COPS=0（超时设了 180s，会阻塞这一轮，但不计入下面的 300s 注册超时窗口——AT+COPS=0 返回后才重置计时器开始算）→ reg_check
dial_stat_net_connected      SIM 通道监控

── 运营商选择子路径 ──────────────────────────────────
dial_stat_list_oper           AT+COPS=?（180s 超时）
dial_stat_select_oper          AT+COPS=1,2,<PLMN>（30s，2次重试）
dial_stat_stop_cfun / start_cfun  AT+CFUN=0 / 1 → 回 sim_init

── Roamlink 路径 ────────────────────────────────────
dial_stat_roamlink_starting   等 TCP 测试通过（10s 轮询，300s 超时）
dial_stat_roamlink_active     Roamlink 通道监控（30s TCP 测试）
```

#### SIM 通道监控（`dial_stat_net_connected`）

- 每拍：`nw_get_connect_state()` 掉线 → 立即跳 `reg_check`；rx_packets 60s 无增长（`dial_timer` 计时）→ 跳 `reg_check`。
- 每 **60s**（`TCP_TEST_INTERVAL_SECONDS`）：TCP 测试（连 `18.196.0.17:22`）
  - 成功：清零 `tcp_fail_count`；策略1 且 `sim_fallback_timer` 满 300s → 切回 Roamlink。
  - 失败：`tcp_fail_count++`，同时重置 `dial_timer`（防止与 rx_packets 超时赛跑）；满 **3 次**（`TCP_FAIL_THRESHOLD`）：license_pending 期保持 SIM（仅 reg_check，不切）；策略1/2 → 切 Roamlink；策略4 → 仅 reg_check 重拨 SIM。

#### Roamlink 通道监控

- `roamlink_starting`：每 10s TCP 测试，**300s**（`ROAMLINK_CONNECT_WAIT_SEC`）内不通 → 策略1/2 切回 SIM，策略3 重启 Roamlink 服务。
- `roamlink_active`：每 **30s**（`ROAMLINK_CHECK_INTERVAL_SEC`）TCP 测试
  - 成功：①策略2 且 `roamlink_fallback_timer` 满 300s → 切回 SIM（`did_switch=true`）；②否则检查 `rmnet_data*` rx_packets 总和，增长则更新基准，**120s**（`ROAMLINK_NO_DATA_TIMEOUT_SEC`）无增长（业务层假死，通道连着但收不到数据）→ 策略1/2 切 SIM，策略3 重启服务。
  - 失败：`roamlink_fail_count++`，满 **3 次**（`ROAMLINK_FAIL_THRESHOLD`）→ 策略1/2 切 SIM，策略3 重启服务。

#### 注册超时（`reg_check`/`cereg_check`，共享 **300s**（`REG_CHECK_TIMEOUT_SECONDS`）超时，由进入 reg_check 时启动/复用的 `dial_timer` 统一计时）

- 策略1/2 且 `roamlink_available` 且非 `license_pending` → 切 Roamlink。
- 否则 → `is_func_called=true`，走 `AT+CFUN=0`→`AT+CFUN=1` 复位后重试（本版本没有 EG25 平台那种 L1/L2/L3 分级恢复，注册超时统一走一次 cfun 复位，没有按断网时长分级）。

#### 通道切换的具体动作序列

**SIM → Roamlink**：①`dail_stop_data_call()` 停物理 SIM 数据呼叫 → ②`apn_obj_free(&p_apn_obj)` 释放并清 APN 缓存（强制下次重新查表）→ ③`roamlink_start_master()` 确保 RBMaster 存活（幂等）→ ④`roamlink_start_service()`：发 `AT+COPS=0` 清手动选网锁 + 发 `RBstartServiceMaster` + 写 `network_type=2` → ⑤记 `roamlink_timer=now`、`roamlink_fail_count=0`、`roamlink_fallback_timer=now`，进 `roamlink_starting`。

**Roamlink → SIM**：①`roamlink_stop_service()`：发 `RBstopServiceMaster` + 写 `network_type=0`（切换中）+ `sleep(3)` 让模组解除虚拟 SIM → ②`apn_obj_free(&p_apn_obj)` → ③记 `sim_fallback_timer=now`、`tcp_test_timer=now`、`tcp_fail_count=0` → ④`sim_initialized` 为真则走 `dial_stat_sim_init`（快速重入，跳过 MCM 初始化），否则走 `dial_stat_init`（完整初始化）→ ⑤SIM 重新走完整个状态机，**到 `dial_stat_wait_for_connect` 成功时才写 `network_type=1`**——这一步与①之间的整段窗口 `network_type` 一直是 0，时长即实际切换耗时。

#### 切换代价

物理 SIM ↔ Roamlink 切换有一段无服务窗口（v1.29.07 实测约 22~107s，取决于场景），`/tmp/network_type` 在整段切换/重拨窗口内为 `0`，不宜频繁触发；300s 回切节流即为此设计的一部分。

#### 完整切换触发表（逐条核对源码，覆盖所有 12 个触发点）

下表按代码里实际出现的顺序列出**所有**会改变 `dial_st` 到 Roamlink/SIM 之间，或重启 Roamlink 服务的触发点。"动作"列只列切换相关的关键调用，不重复 AT 指令本身。

| # | 所在状态 | 触发条件 | 适用策略 | 关键动作（按代码顺序） | 目标状态 |
|---|---|---|---|---|---|
| 1 | `dial_stat_none` | 启动时 `license_pending=true` | 1/2/3/4（已被探测临时降级为4） | 不启动 RBMaster，直接进 SIM 路径 | `dial_stat_init` |
| 2 | `dial_stat_none` | 策略∈{1,3} 且 `roamlink_available` | 1、3 | `roamlink_start_master()` → `roamlink_timer=now` → `roamlink_start_service()` | `dial_stat_roamlink_starting` |
| 2b | `dial_stat_none` | 策略∈{1,3} 但二次检查发现 `!roamlink_available`（极端竞态防呆） | 1、3 | 策略强改为 4 | `dial_stat_init` |
| 3 | `dial_stat_none` | 策略∈{2,4}，或 1 已不带 `roamlink_available` | 2、4 | 无 | `dial_stat_init` |
| 4 | `dial_stat_net_connected` | `nw_get_connect_state()<=0`（掉线） | 全部 | `is_func_called=true`（门控重置，下次重新 Init+Start）、`tcp_fail_count=0` | `dial_stat_reg_check`（不切通道，原地重拨 SIM） |
| 5 | `dial_stat_net_connected` | rx_packets 60s 无增长（独立于 TCP 测试的另一条超时线） | 全部 | `tcp_fail_count=0` | `dial_stat_reg_check`（不切通道） |
| 6 | `dial_stat_net_connected` | 60s TCP 测试**成功**，且策略1 + `roamlink_available` + `sim_fallback_timer>0` + 已等满 300s | 1 | `dail_stop_data_call()` → `apn_obj_free()` → `roamlink_start_master()`（RBMaster 可能已被策略3之类杀过，重新确保） → `mark_network_type(ROAMLINK)` → `roamlink_timer=now` → `roamlink_start_service()` → `roamlink_fail_count=0` → 立即 `dial_status_update()` | `dial_stat_roamlink_starting` |
| 7 | `dial_stat_net_connected` | 60s TCP 测试**连续失败 3 次**，且 `license_pending=true` | 任意（已被探测临时降级为4） | 仅 `dial_timer=now` | `dial_stat_reg_check`（不切，license 没到切了也用不了，且会打断 RBMaster 借用的 SIM 下载） |
| 8 | `dial_stat_net_connected` | 60s TCP 测试**连续失败 3 次**，非 license_pending | 1、2 | `dail_stop_data_call()` → `apn_obj_free()` → `roamlink_start_master()` → `mark_network_type(ROAMLINK)` → `roamlink_timer=now` →（仅策略2）`roamlink_fallback_timer=now` → `roamlink_start_service()` → `roamlink_fail_count=0` → `dial_status_update()` | `dial_stat_roamlink_starting` |
| 9 | `dial_stat_net_connected` | 同上失败 3 次，策略4 | 4 | 仅 `dial_timer=now` | `dial_stat_reg_check`（不切，永不切 Roamlink） |
| 10 | `reg_check`/`cereg_check` | 共享 300s 注册超时（`reg_timeout_handler`） | 1、2（且 `roamlink_available` 且非 `license_pending`） | `dail_stop_data_call()` → `apn_obj_free()` → `roamlink_start_master()` → `mark_network_type(ROAMLINK)` → `roamlink_timer=now` → `roamlink_fallback_timer=now`（无条件写，仅策略2会真正用到这个值） → `roamlink_start_service()` → `roamlink_fail_count=0` → `dial_status_update()` | `dial_stat_roamlink_starting` |
| 11 | `reg_check`/`cereg_check` | 同上超时，策略4 / `roamlink_available=false` / `license_pending=true` | 4 或受限场景 | `is_func_called=true` | `dial_stat_stop_cfun`（cfun 复位重试，不切通道） |
| 12 | `dial_stat_roamlink_starting` | 每 10s TCP 测试**成功** | 全部 | `roamlink_timer=now`、`roamlink_fail_count=0`、重置 rx_packets 基准（`nw_get_rmnet_rx_packets_sum`）、`roamlink_no_data_timer=now`、立即 `dial_status_update()` | `dial_stat_roamlink_active` |
| 13 | `dial_stat_roamlink_starting` | 300s 内未连通（超时） | 1、2 | `roamlink_stop_service()`（**不**杀进程）→ `tcp_fail_count=0` → `apn_obj_free()` → `sim_fallback_timer=now` → `tcp_test_timer=now` → 按 `sim_initialized` 决定走 `dial_stat_sim_init`（重入,跳过 MCM 初始化）或 `dial_stat_init`（完整初始化） | `dial_stat_sim_init` / `dial_stat_init` |
| 14 | `dial_stat_roamlink_starting` | 同上超时，策略3 | 3 | `roamlink_stop_service()` → `sleep(5)` → `roamlink_timer=now` → `roamlink_kill_master()`（**杀进程**） → `roamlink_start_master()` → `roamlink_start_service()`；**注意：此分支未调用 `dial_status_update()`**，状态文件要等 30s 周期刷新才会反映新一轮的 `roamlink_connect_wait_sec` | 留在 `dial_stat_roamlink_starting` |
| 15 | `dial_stat_roamlink_active` | 每 30s TCP 测试**成功**，且策略2 + `roamlink_fallback_timer>0` + 已等满 300s（`did_switch` 门闩，防止本轮再触发业务层检测） | 2 | `roamlink_stop_service()` → `tcp_fail_count=0` → `apn_obj_free()` → `sim_fallback_timer=now` → `tcp_test_timer=now` → 同 #13 的 sim_init/init 分支 → `dial_status_update()` | `dial_stat_sim_init` / `dial_stat_init` |
| 16 | `dial_stat_roamlink_active` | TCP 成功 + `!did_switch` + `rmnet_data*` rx_packets **120s 无增长**（业务层假死：TCP 通但虚拟SIM未真正收发数据） | 1、2 | 同 #13 的完整 SIM 切换动作（含 `dial_status_update()`） | `dial_stat_sim_init` / `dial_stat_init` |
| 17 | `dial_stat_roamlink_active` | 同上业务层假死，策略3 | 3 | `roamlink_stop_service()` → `sleep(5)` → `roamlink_timer=now` → 重置 rx_packets 基准 + `roamlink_no_data_timer=now` → `roamlink_kill_master()` → `roamlink_start_master()` → `roamlink_start_service()` → `dial_status_update()` | 留在/回到 `dial_stat_roamlink_starting` |
| 18 | `dial_stat_roamlink_active` | 30s TCP 测试**连续失败 3 次** | 1、2 | 同 #13 完整 SIM 切换（含 `dial_status_update()`） | `dial_stat_sim_init` / `dial_stat_init` |
| 19 | `dial_stat_roamlink_active` | 同上连续失败 3 次，策略3 | 3 | `roamlink_stop_service()` → `sleep(5)` → `roamlink_timer=now` → `roamlink_kill_master()` → `roamlink_start_master()` → `roamlink_start_service()`；**同样未调用 `dial_status_update()`** | 留在 `dial_stat_roamlink_starting` |

**核对中发现的两个小的可观测性瑕疵**（不影响功能正确性，仅状态文件可能滞后 ≤30s）：触发点 #14 和 #19（FORCE_ROAMLINK 因连接超时/TCP连续失败而重启服务）没有像同类的 #17（业务层假死重启）一样显式调用 `dial_status_update()`；由于状态机本身有 30s 周期性刷新兜底，实际影响是这两种场景下 `/tmp/dial_status` 的 `roamlink_connect_wait_sec`/`rbmaster_pid` 最多滞后一个刷新周期，不是功能性 bug。测试侧若专门盯 `/tmp/dial_status` 做切换耗时打点，知道这个滞后来源即可。

**几个容易问错的细节**：
- `sim_fallback_timer`/`roamlink_fallback_timer` 只在"进入"对应备用通道时刷新为当前时间，离开时不会主动清零；下次再次进入同一备用通道会被重新刷新，因此不存在"用旧值误判"的问题。
- `roamlink_fallback_timer=now` 在触发点 #10（reg_timeout 切到 Roamlink）里对策略1也会被无条件执行，但该值只在策略2的判断分支里被读取，对策略1而言是个无副作用的死写。
- 触发点 #13/#15/#16/#18 四处"切回 SIM"的动作序列完全一致（已在文档中合并描述为"同 #13"），只是触发条件不同，源码里是四处几乎相同的代码块，并非函数复用——阅读源码时注意这是重复代码而非共享函数。

---

### 四、License 与切换的关系

启动时探测 `roamlink_probe()`（`dial_mng_new()` 执行一次）：

| 探测结果 | 条件 | 处理 |
|---|---|---|
| `OK` | RBMaster + `/opt/conf.ini` + license 均就绪 | `roamlink_available=true`，按策略正常使用 Roamlink；若备份（`/data/ufs/license.cer`）缺失/空，顺手 `roamlink_license_write_backup()` 补写一份（防下次重刷后备份也没了） |
| `NO_PACKAGE` | RBMaster 二进制不存在 | 本次运行永久降级 FORCE_SIM |
| `CONF_MISSING` | `/opt/conf.ini` 缺失 | 降级 FORCE_SIM，不启 RBMaster，不等待；只能等 factoryApp 补上 conf.ini 后重启 `dial` 再重新探测 |
| `LICENSE_MISSING` | license 缺失/空 | 先尝试 `roamlink_license_restore_from_backup()`；恢复失败：策略3 仅记录日志等人工处理（不进 license_pending，没意义）；策略1/2/4 → `license_pending=true`，临时 FORCE_SIM |

**`license_pending` 完整流程**：
1. `dial_stat_none` → `dial_stat_init`（SIM 路径，**此时不启动 RBMaster**——提前启动会让 RBMaster 抢占模组，打断 SIM 数据呼叫）。
2. SIM 数据呼叫连上（`dial_stat_wait_for_connect` 成功）→ 调一次 `roamlink_start_master()`（`rbmaster_started` 标志位防止重连时重复调用），借已通的 SIM 网络下载证书。
3. 每 **60s**（`LICENSE_CHECK_INTERVAL_SEC`）轮询主路径文件，最长等 `LICENSE_WAIT_TIMEOUT_SEC`；`roamlink_license_appeared()` 用 2 秒间隔的双重 stat 确认文件写入已稳定才采信（防止读到下载中的半截文件）。
4. 出现 → `roamlink_license_backup_and_reboot()`：写备份 + `fsync` + 直接 reboot（无返回）；重启后探测变 `OK`，按策略正常运行（此时才真正用上 Roamlink）。
5. 超时仍未下到 → `license_pending=false`；若 `rbmaster_started=true`，先 `roamlink_stop_service()` + 重拨（`reg_check`）释放模组通道，再放弃 pending 保持 FORCE_SIM——**这一步是 v1.29.11 新增的**，此前 RBMaster 会一直占住通道，物理 SIM 拿到全 0 IP/DNS，域名解析直到重启才恢复。

⚠️ **`LICENSE_WAIT_TIMEOUT_SEC` 当前是测试值 300s，量产前必须改回 7200s**（2小时），300s 在弱网/云端高负载场景下可能不够真实下载时间，测试侧如果验证"超时放弃"场景请注意这只是为了缩短测试周期临时调小的。

**license_pending 期间注册超时不切 Roamlink**：因为 license 还没到，切了也连不上，统一走 cfun 复位重试，等 license 真正到位重启后再按策略走。

---

### 五、RBMaster 进程管理（`src/roamlink/roamlink.c`）

RBMaster 是闭源 Roamlink 守护进程（`/usrdata/roamlink/RBMaster`），监听 `127.0.0.1:5568`，命令通过 TCP 短连接发送：`"RBstartServiceMaster"`（虚拟 SIM 接管模组）/ `"RBstopServiceMaster"`（释放回物理 SIM）。

- **存活判定**：仅 `/proc/<pid>/cmdline` 扫描含 `RBMaster`（**v1.29.11 起不再做端口探测**——端口 5568 非持续监听，RBMaster 每条命令处理完就主动断开连接，端口探测会把健康进程误判为僵尸，导致反复 `fork()`、进程堆积）。
- **`roamlink_kill_master()`**：SIGTERM 全扫 `/proc` 下所有 RBMaster pid → `sleep(3)` → SIGKILL 扫残留 → `sleep(2)`。仅在 FORCE_ROAMLINK 三处重启路径调用，保证旧进程清干净再 `fork()` 新的，避免新旧进程同时占用端口/资源。
- **僵尸回收（v1.29.12 新增）**：`dial` 是长驻父进程，不会退出，`fork()` 出的 RBMaster 子进程死后（自崩或被 `kill_master` 杀）若无人 `wait()`，会永久滞留为僵尸——`init`（PID=1）只收孤儿，不收活着的父进程名下的尸体。策略3 每轮失败重启都会 +1 个僵尸，长期运行可耗尽 PID 空间。现记录 `g_rbmaster_child_pid`，在 `roamlink_start_master()` 入口与 `roamlink_kill_master()` 收尾各做一次定向 `waitpid(WNOHANG)`（不用 `waitpid(-1)`，避免抢收 main 线程 `system()` 调用产生的子进程）。
- **`roamlink_start_master()`**：先判活，未运行才 `fork()+exec()`，`sleep(3)` 等初始化完成。
- **license 备份**：`roamlink_license_write_backup()` 独立函数，`fsync` 落盘，供 `dial_mng_new()` 探测成功时补写、以及下载完成后的备份重启流程共用。

---

### 六、AT 层行为（`src/at/at.c`）

- `Ql_SendAT()` 入口增加 `smd_fd < 0` 防护：串口未打开时直接返回 -1，调用方按"AT 失败"分支处理；此前会走到 `FD_SET(-1)`（栈越界写）和 `select(0,..)` 空耗满超时。
- `+CME ERROR:`/`+CMS ERROR:`/`ERROR` 应答原本的判断逻辑嵌套写反，永不可达——错误应答被当无关 URC 丢弃，持锁空等满 `timeout_ms`（`AT+COPS=?` 场景最长 180s，期间会阻塞主线程 CSQ 心跳，因为两线程共享同一把 `g_at_port_mutex`）。现已独立分支：错误应答立即拷出文本、返回 1（与超时同值，所有调用方按非 0 判失败，行为不变，只是不再空等）。
- 应答拷贝统一夹到 `AT_MSG_LENGTH_MAX-1` 并补 `'\0'`，防止读满 1024 字节缓冲区时无终止符导致调用方 `strstr` 越界读。
- 命令已自带 `\r`/`\n` 时不再发出全 NUL（此前会把发送缓冲区清零却仍按原长度 `write()`）。

---

### 七、APN 选择（`src/apn/apn.c`）

读取 `/usr/dial/apn.json`，按 ICCID 前 6 位匹配 APN/用户名/密码；同一运营商前缀可配置多个候选 APN，通过 `cc_deque` 队列在失败时轮换。

`apn_scan_idx()` 第一行就 `return apn_set(1, p_apn_obj)`（强制 `profile_idx=1`，配合高通自动路由），下面 100+ 行的扫描逻辑是死代码，已知但不清理（高通平台固定行为）。

**内存泄漏修复（v1.29.12）**：`apn_get_apn_obj()` 分配的 `apn_obj_t`（含运营商 deque）此前在 8 处 `p_apn_obj=NULL` 之前未释放，每次通道切换 / 300s 回切重试都会泄漏一份；现统一走 `apn_obj_free()`（先 `cc_deque_destroy_cb(.., free)` 再释放对象本身）。**建议测试侧长跑策略1/2 反复切换场景时关注 `dial` 进程的 RSS 内存曲线是否平稳。**

---

### 八、状态文件 `/tmp/dial_status`

每 30s 自动刷新，状态切换时立即更新，原子写（先写 `.tmp` 再 `rename()`，避免读到半截文件）：

```ini
# dial status - auto generated by dial v1.29.12
# updated: 2026-06-25 10:05:38

[dial]
version=1.29.12
uptime=329
state=roamlink_active
policy=1
policy_name=PREFER_ROAMLINK
channel=ROAMLINK           # SIM / ROAMLINK / SWITCHING / NONE
roamlink_available=1
license_pending=0
license_wait_sec=0

[sim]
iccid=89464283216100721879
csq=21
apn=internet.lte.cxn
profile_idx=1
plmn=46001

[network]
status=1                   # 1=up, 0=down
type=2                      # 1=SIM, 2=Roamlink, 0=切换中
ip=10.88.197.109
ifname=rmnet_data0

[counters]
tcp_fail_count=0
roamlink_fail_count=0
roamlink_rx_packets=143

[roamlink]
state=active                # none / starting / active
biz_no_data_sec=25
roamlink_connect_wait_sec=0
rbmaster_pid=1516

[stats]
outage_count=2
last_outage_sec=22
total_outage_sec=134
current_outage_sec=0
```

断网统计用边沿检测：连接→断开的转换 `outage_count++`，断开→连接的转换计算本次断网时长；进程重启（`killall dial`）会清零统计。

---

### 九、运行时配置与状态文件一览

| 文件 | 用途 |
|---|---|
| `/usrdata/network.ini` | 网络策略（`network_select`），启动时读一次 |
| `/usr/dial/apn.json` | APN 配置，按 ICCID 前 6 位匹配 |
| `/opt/conf.ini` | RBMaster 运行前置依赖（factoryApp 下载），缺失则永久 FORCE_SIM |
| `/usrdata/roamlink/etc/.pconfig/license.cer` | license 主路径（RBMaster 实际读取） |
| `/data/ufs/license.cer` | license 备份（掉电持久化分区） |
| `/tmp/dial_status` | INI 格式实时状态（见上节），每 30s 刷新 |
| `/tmp/network_status` | `1`=连接 / `0`=断开 |
| `/tmp/network_type` | `1`=SIM / `2`=Roamlink / `0`=切换中（整段切换/重拨窗口期间为 0） |
| `/tmp/network_csq` | 信号强度整数，主线程每 5s 刷新 |
| `/tmp/network_plmn` | 当前运营商 PLMN（MCC+MNC） |
| `/usr/dial/tz.ini` | dial 从网络取到时区后写入（v1.29.12 修复了空应答污染问题） |
| `/media/sdcard/seas_log_dial.log` | 滚动日志，60MB 上限、绕回截断；仅当 sdcard 存在且剩余空间≥1GB 时才写（本版本日志系统不是 EG25 平台那种按天分目录+40天保留，是单文件滚动） |
| `/usrdata/check_network.log` | 看门狗脚本日志，1MB 自动滚动 |

---

### 十、日志关键字速查表（测试 grep 用）

本版本日志标签较少，未做精细分类（不同于 EG25 平台 modem_mng 的 `[STATE]`/`[RECOVERY L1-3]`/`[CFUN]`/`[OPER]` 等细粒度标签体系）：

| 关注点 | grep 关键字 | 示例 |
|---|---|---|
| 启动信息（仅一次） | `[INIT]` | `[INIT] policy=4` / `[INIT] IMEI: 865167064495881` / `[INIT] FW: ...` |
| 选定/重新查表 APN（每次查表都打，不仅限启动） | `[INIT] APN` | `[INIT] APN: internet.lte.cxn` |
| 心跳（60s 基础版） | `[HEARTBEAT]` | `[HEARTBEAT] state=net_connected csq=21 tcp_fail=0 rl_fail=0` |
| 心跳（300s 扩展版，同 tag） | `[HEARTBEAT]` | `[HEARTBEAT] cereg=1 cellid=... pci=... tac=... rsrp=-95dBm rsrq=-12dB ifname=rmnet_data0 ip=10.88.197.109 rx_packets=143` |
| Roamlink/license 相关 | `[ROAMLINK]` | `[ROAMLINK] license failed, stopping RBMaster to free SIM channel and redial`（目前代码里唯一一条带方括号的 ROAMLINK 日志，其余 roamlink 相关日志都是 `roamlink:`/`roamlink_xxx:` 裸前缀，不是 `[ROAMLINK]`） |
| 全零地址兜底（预留，**未启用**） | `[ZERO ADDR]` | 代码中已注释，等真机复现后再激活，目前 grep 不到 |

> **状态机流转**（每次状态变化都打，覆盖所有状态）：`SEAS_LOG_INFO("state: %s -> %s", ...)`，例如 `state: reg_check -> cereg_check`、`state: net_connected -> roamlink_starting`——裸字符串前缀 `state:`，不是方括号标签，grep `"state: "` 即可看到完整状态流转轨迹。
> 运营商选择、CFUN 操作、TCP 测试结果均有日志输出但同样是裸字符串（`roamlink_start:`/`roamlink_stop:`/`tcp_test:`/`AT+COPS=?` 等前缀），未加方括号标签；测试侧排障建议直接搜索这些前缀或状态名，而不要套用 EG25 平台 `[STATE]`/`[RECOVERY L1-3]`/`[CFUN]`/`[OPER]` 的标签习惯——本版本没有那一套。

---

### 十一、已知缺陷 / 需要测试侧知会的设计限制

| 位置 | 问题 | 影响 |
|---|---|---|
| `LICENSE_WAIT_TIMEOUT_SEC` | 当前 300s（测试值），**量产前必须改回 7200s** | 测试"超时放弃 license_pending"场景时请确认本批固件是否已改回生产值，避免和真实弱网下载耗时混淆 |
| `NW_TCP_TEST_HOST`（`18.196.0.17:22`） | 硬编码单一探测目标，无备用地址 | 若该地址在测试网络环境下不可达，SIM/Roamlink 两条通道会反复判定失败、来回切换（"双通道同时失活"误报），建议测试前先确认测试网络能访问该地址 |
| `nw.c` 的 `nw_at_get_csq()`/`nw_mark_plmn()` | 用 `system("echo ... > /tmp/...")` 写文件，理论上有 shell 注入风险（mcc/mnc 来自模组应答） | 影响面小（数据源是模组而非外部用户输入），暂未整改，测试侧无需专项验证 |
| `apn.c` 的 `apn_scan_idx()` | 第一行即 `return`，下方 100+ 行扫描逻辑是死代码 | 不影响行为，纯代码可读性问题 |

---

### 升级包文件清单

```
roamlink_deploy/
├── dial                     ← v1.29.12 拨号程序二进制（核心，本次升级唯一变化点）
├── check_network.sh         ← 看门狗脚本（不变）
├── network.ini              ← 策略配置文件（不变）
├── install_update.sh        ← 安装/升级脚本（不变）
├── start_check_network      ← 开机启动脚本（不变）
├── roamlink/                ← RBMaster 及依赖（不变）
└── licenses/                ← 各设备 license 文件（按需放置）
    └── {IMEI}_license.cer
```

### 升级方式

- **普通升级**（已安装 Roamlink，只升级 `dial`）：`install_update.sh` 检测到 `/usrdata/roamlink` 已存在，自动只替换 `dial`、`check_network.sh`、`network.ini` 三个文件，不动 Roamlink 组件和 license，重启生效（`FORCE_INSTALL=0`，默认）。
- **重装**（重置所有组件）：`FORCE_INSTALL=1`，清除 `/usrdata/roamlink` 和 `/data/ufs/license.cer` 后走完整安装流程。
- **首次安装**：`/usrdata/roamlink` 不存在时自动走完整安装流程。

---
