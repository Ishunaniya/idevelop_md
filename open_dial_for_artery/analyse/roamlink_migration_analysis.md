# Roamlink 双卡切换逻辑移植分析报告

> 分析日期：2026-04-28  
> 仓库：`/home/tronlong/lyp/code/open_dial_for_artery`，分支：`feature/dual_channel_switch`  
> 基准提交：`9c1ec55432f4f741179e6b3dbb56752fe4631c4f`（双通道切换逻辑内置化）

---

## 一、提交列表

```
$ git log --oneline 9c1ec55432f4f741179e6b3dbb56752fe4631c4f^..HEAD

9d15833 [BUGFIX] 增加注册超时切换策略，修复断线重拨卡死及互斥锁死锁问题
e20da07 [CHGLOG] 增加变量值校验及异常处理并返回错误
3b5f41c [CHGLOG] 日志降频
bfc3098 [CHGLOG] 日志降频
86b46cb [BUGFIX] 增加/opt/conf.ini缺失判断
b1ca8d0 [BUGFIX] 系统测试问题修复
9c1ec55 [NEWFUNC] V1.29.7 双通道切换逻辑内置化
```

共 7 条提交，时间跨度 2026-03-18 至 2026-04-10。

---

## 二、逐提交 Roamlink 核心改动摘要

### 2.1 `9c1ec55` — [NEWFUNC] V1.29.7 双通道切换逻辑内置化（2026-03-18）

**新增文件**：`src/roamlink/roamlink.c`（628行）、`src/roamlink/roamlink.h`（157行）  
**修改文件**：`src/dial/dial.c`（+457行）、`src/dial/dial.h`（+38行）、`src/nw/nw.c`（+106行）

**核心改动**：

1. **新增 roamlink 模块**，包含以下函数：
   - `roamlink_probe()`：三态探测（OK / NO_PACKAGE / LICENSE_MISSING）
   - `roamlink_is_master_running()`：双重检测（/proc扫描 + 端口连通），解决僵尸进程误判
   - `roamlink_start_master()`：fork+exec 启动 RBMaster，chdir 到工作目录
   - `roamlink_send_cmd()`：非阻塞 TCP connect 替代 `echo | nc` 
   - `roamlink_start_service(smd_fd)`：AT+COPS=0 + RBstartServiceMaster + network_type=2
   - `roamlink_stop_service()`：RBstopServiceMaster + network_type=0 + sleep(3)
   - `roamlink_mark_network_type(type)`：写 /tmp/network_type
   - `roamlink_read_policy()`：读 /usrdata/network.ini，返回 1~4，越界时用默认值4
   - `roamlink_license_restore_from_backup()`、`roamlink_license_appeared()`、`roamlink_license_backup_and_reboot()`：license 自动激活流程

2. **`dial_mng_new()` 初始化**：读取策略、调用 `roamlink_probe()`、按三种探测结果设置 `roamlink_available` / `license_pending` / `network_select` 覆盖

3. **`dial_stat_none` 路由决策**：
   - `license_pending` → 先走 SIM（dial_stat_init）（此版本：直接启动 RBMaster，后续修正）
   - 策略1/3 + roamlink_available → 启动 RBMaster → roamlink_starting
   - 策略2/4 → dial_stat_init

4. **license 等待轮询**：在 dial_task 主循环顶部每次检查，每60s探测一次，超过 `LICENSE_WAIT_TIMEOUT_SEC`（此版本：7200s）放弃

5. **`dial_stat_roamlink_starting` 状态**：每10s TCP测试；通过 → roamlink_active；超时（300s）→ 策略1/2切SIM，策略3重启Roamlink

6. **`dial_stat_roamlink_active` 状态（初版）**：每30s TCP测试；失败3次策略1/2切SIM，策略3重启Roamlink

7. **`dial_stat_net_connected` TCP测试逻辑**：每60s TCP测试，失败3次策略1/2切Roamlink，策略4重拨SIM；添加 `dial_timer` 重置防止 rx_packets 超时抢先触发

8. **`nw_tcp_connectivity_test()`**：新增非阻塞 TCP connect 到 18.196.0.17:22

9. **`nw_get_rmnet_rx_packets_sum()`**：读取 `/sys/devices/virtual/net/rmnet_data*/statistics/rx_packets`

```c
/* roamlink.h 关键常量（此版本） */
#define LICENSE_WAIT_TIMEOUT_SEC    (7200)  /* 2 小时 */
#define ROAMLINK_CONNECT_TIMEOUT_SEC 300
```

---

### 2.2 `b1ca8d0` — [BUGFIX] 系统测试问题修复（2026-03-20）

**修改文件**：`src/dial/dial.c`（+548行）、`src/dial/dial.h`（+90行）、`src/nw/nw.c`（+63行）、新增 `src/status/status.c`（176行）、`src/status/status.h`（130行）

**核心改动**：

1. **启动清理**：`dial_mng_new()` 中，若策略为 SIM 类且 RBMaster 正在运行，先调 `roamlink_stop_service()` + `sleep(2)`，防止虚拟SIM残留导致ICCID无法匹配

2. **license_pending 流程修正**：RBMaster **不在** `dial_stat_none` 时立即启动，改为 SIM 进入 `net_connected` 后才启动（标志 `rbmaster_started` 防重复）：
   ```c
   /* SIM 联网后才启动 RBMaster */
   if (p_dial_mng->license_pending && !p_dial_mng->rbmaster_started)
   {
       SEAS_LOG_INFO("SIM connected, starting RBMaster for license download");
       roamlink_start_master();
       p_dial_mng->rbmaster_started = true;
   }
   ```

3. **fallback 计时器**：新增 `sim_fallback_timer`（进入SIM备用的时刻）和 `roamlink_fallback_timer`（进入Roamlink备用的时刻），零值=未进入备用模式

4. **策略1 回切 Roamlink**：在 `net_connected` TCP测试通过后，若 `sim_fallback_timer>0` 且已过 `SIM_FALLBACK_RETRY_SEC`（300s），切回 Roamlink：
   ```c
   if (p_dial_mng->network_select == NET_POLICY_PREFER_ROAMLINK
       && p_dial_mng->roamlink_available
       && p_dial_mng->sim_fallback_timer.tv_sec > 0)
   {
       struct timespec fallback_dif = diff(p_dial_mng->sim_fallback_timer, cur_timer);
       if (fallback_dif.tv_sec >= SIM_FALLBACK_RETRY_SEC)
       { /* 停SIM → 启Roamlink → roamlink_starting */ }
   }
   ```

5. **策略2 回切 SIM**：在 `roamlink_active` TCP测试通过后，`did_switch` 标志防止双次触发，若 `roamlink_fallback_timer>0` 且已过 `ROAMLINK_FALLBACK_RETRY_SEC`（300s），切回SIM

6. **业务层存活检测**（`dial_stat_roamlink_active`）：读 rmnet_data* rx_packets，无增长超过 `ROAMLINK_NO_DATA_TIMEOUT_SEC`（120s）则按失败处理；进入 `roamlink_active` 时读基准值、重置计时器

7. **sim_initialized 快速重入**：Roamlink→SIM 时判断 `sim_initialized`，true则设 `is_func_called=true` 跳到 `dial_stat_sim_init`，false则走完整 `dial_stat_init`

8. **旋转 SIM→Roamlink 切换完整序列**：`p_apn_obj=NULL`、`roamlink_start_master()`（幂等）、设 `roamlink_fallback_timer`

9. **状态输出模块**：新增 `dial_status_write()` 原子写入 `/tmp/dial_status`，30s周期刷新 + 状态切换点即时刷新；包含双通道状态、rx_packets、断网统计等全部字段

10. **LICENSE_WAIT_TIMEOUT_SEC 改为 300**（测试值）

---

### 2.3 `86b46cb` — [BUGFIX] 增加/opt/conf.ini缺失判断（2026-03-24）

**修改文件**：`src/roamlink/roamlink.c`、`src/roamlink/roamlink.h`、`src/dial/dial.c`

**核心改动**：

1. **probe 从三态扩展为四态**，新增 `ROAMLINK_PROBE_CONF_MISSING`：
   ```c
   typedef enum
   {
       ROAMLINK_PROBE_OK = 0,
       ROAMLINK_PROBE_NO_PACKAGE,
       ROAMLINK_PROBE_CONF_MISSING,     /* 新增 */
       ROAMLINK_PROBE_LICENSE_MISSING,
   } roamlink_probe_result_e;
   ```

2. **`roamlink_probe()` 新增检查2**：RBMaster存在后，先检查 `/opt/conf.ini` 是否存在，缺失则返回 `ROAMLINK_PROBE_CONF_MISSING`（在 license 检查之前，因为是更基础的前置条件）

3. **`dial_mng_new()` 处理 CONF_MISSING**：直接降级 FORCE_SIM，不启动RBMaster，等 factoryApp 下载 conf.ini 后 dial 重启

4. **恢复后重新 probe 逻辑精化**：备份恢复成功后，重新 probe 结果为 CONF_MISSING 时单独处理（极端情况：两次 probe 之间 conf.ini 消失）

```c
#define ROAMLINK_CONF_PATH           "/opt/conf.ini"
```

---

### 2.4 `bfc3098` — [CHGLOG] 日志降频（2026-03-24）

**修改文件**：`src/dial/dial.c`、`src/dial/dial.h`

**核心改动**（与双通道切换逻辑直接相关）：

1. **新增 `ROAMLINK_BIZ_LOG_INTERVAL_SEC`（3600s）** 和字段 `roamlink_biz_log_timer`：Roamlink biz OK 日志从每30s一条改为每1小时摘要一条

2. **roamlink_starting 等待日志限频**：前15s立即打，之后按60s间隔

---

### 2.5 `3b5f41c` — [CHGLOG] 日志降频（2026-03-25）

**修改文件**：`src/dial/dial.c`、`src/nw/nw.c`

**核心改动**：

1. TCP 测试恢复日志阈值从 `>0` 改为 `>=2`（SIM通道和Roamlink通道各一处）
2. `nw_tcp_connectivity_test()` 内成功路径日志注释掉，减少每次TCP成功都打一行的噪音

---

### 2.6 `e20da07` — [CHGLOG] 增加变量值校验及异常处理并返回错误（2026-04-08）

**修改文件**：`src/dial/dial.c`、`src/at/at.c`

**核心改动**（与双通道切换逻辑间接相关）：

1. **`at_init()` 错误处理**：`open(/dev/smd8)` 失败时记录日志并 `return -1`（之前默默返回无效fd）
2. **AT超时修正**：`timeout.tv_usec = (timeout_ms % 1000) * 1000`（原来缺少 *1000，毫秒/微秒转换错误）；在每次 select 循环前重置 timeout（`select()` 会修改 `timeval`）
3. **`dial_mng_new()` NULL检查**：`calloc` 失败时提前返回 NULL

---

### 2.7 `9d15833` — [BUGFIX] 增加注册超时切换策略，修复断线重拨卡死及互斥锁死锁问题（2026-04-10）

**修改文件**：`src/dial/dial.c`（+100行）、`src/dial/dial.h`（+10行）

**核心改动**：

1. **新增 `REG_CHECK_TIMEOUT_SECONDS`（300s）**：

   ```c
   #define REG_CHECK_TIMEOUT_SECONDS (300)
   ```

2. **`dial_stat_reg_check` / `dial_stat_cereg_check` 共享超时**：两个状态共用 `dial_timer` 计时，任意一个超过300s触发 `reg_timeout_handler`（goto label）

3. **`reg_timeout_handler`**：策略1/2 + roamlink_available + 非license_pending → 切换到Roamlink；否则 → cfun重置：
   ```c
   if ((policy == PREFER_ROAMLINK || policy == PREFER_SIM)
       && p_dial_mng->roamlink_available
       && !p_dial_mng->license_pending)
   {
       /* 切换到 Roamlink */
       roamlink_start_master();
       roamlink_mark_network_type(NETWORK_TYPE_ROAMLINK);
       p_dial_mng->roamlink_timer = cur_timer;
       p_dial_mng->roamlink_fallback_timer = cur_timer;
       roamlink_start_service(p_dial_mng->smd_fd);
       p_dial_mng->dial_st = dial_stat_roamlink_starting;
   }
   else
   {
       /* cfun 重置 */
       p_dial_mng->is_func_called = true;
       p_dial_mng->dial_st = dial_stat_stop_cfun;
   }
   ```

4. **`dial_timer` 注册超时起点统一**：所有跳转到 `reg_check` 的路径都补全 `p_dial_mng->dial_timer = cur_timer`（断线、rx_packets超时、TCP失败切SIM、list_oper失败、select_oper失败等）

5. **互斥锁死锁修复**：`wait_for_connect` 超时重拨时，AT+COPS=0 加了 `pthread_mutex_lock/unlock`（原来裸调Ql_SendAT不加锁）

6. **`is_func_called` 标志修复**：断线进入 `reg_check` 时补 `is_func_called=true`，防止 `dail_start_data_call` 内门控逻辑阻止重新发起呼叫

---

## 三、已实现逻辑清单

| 功能点 | 实现状态 | 所在提交 |
|--------|----------|----------|
| 四态 probe（OK/NO_PACKAGE/CONF_MISSING/LICENSE_MISSING） | ✅ 完整 | 9c1ec55 + 86b46cb |
| 策略读取（network.ini，越界默认4） | ✅ 完整 | 9c1ec55 |
| license_pending 流程（SIM先联网再启RBMaster） | ✅ 完整 | 9c1ec55 + b1ca8d0 |
| license 备份/恢复/备份后重启 | ✅ 完整 | 9c1ec55 |
| dial_stat_none 策略路由（1/2/3/4 + license_pending） | ✅ 完整 | 9c1ec55 + b1ca8d0 |
| dial_stat_roamlink_starting（10s TCP测试，300s超时） | ✅ 完整 | 9c1ec55 |
| dial_stat_roamlink_active（30s TCP测试，3次失败阈值） | ✅ 完整 | 9c1ec55 + b1ca8d0 |
| roamlink_active 业务层无数据检测（120s rx_packets） | ✅ 完整 | b1ca8d0 |
| 策略1 SIM备用→回切Roamlink（300s sim_fallback_timer） | ✅ 完整 | b1ca8d0 |
| 策略2 Roamlink备用→回切SIM（300s roamlink_fallback_timer） | ✅ 完整 | b1ca8d0 |
| 策略3 Roamlink失败只重启，不切SIM | ✅ 完整 | 9c1ec55 |
| 策略4 SIM失败只重拨，不切Roamlink | ✅ 完整 | 9c1ec55 |
| SIM→Roamlink 切换完整序列（stop_call/p_apn_obj=NULL/start_master/start_service/计时器重置） | ✅ 完整 | 9c1ec55 + b1ca8d0 |
| Roamlink→SIM 切换完整序列（stop_service/p_apn_obj=NULL/计时器重置/sim_initialized判断） | ✅ 完整 | b1ca8d0 |
| did_switch 标志防双次触发 | ✅ 完整 | b1ca8d0 |
| reg_check/cereg_check 超时切换策略 | ✅ 完整 | 9d15833 |
| RBMaster 僵尸进程检测（/proc + port双重验证） | ✅ 完整 | 9c1ec55 |
| 启动时清理残留 Roamlink 服务 | ✅ 完整 | b1ca8d0 |
| conf.ini 缺失（factoryApp未跑）降级FORCE_SIM | ✅ 完整 | 86b46cb |
| /tmp/dial_status 状态输出 | ✅ 完整 | b1ca8d0 |
| 互斥锁保护 AT 串口（含 Roamlink 相关 AT 命令） | ✅ 完整 | 9d15833 |
| AT 超时 tv_usec 计算修正（×1000） | ✅ 完整 | e20da07 |

---

## 四、缺失逻辑清单

| 缺失项 | 严重程度 | 说明 |
|--------|----------|------|
| `LICENSE_WAIT_TIMEOUT_SEC = 300`（测试值） | ⚠️ 预上线必改 | `b1ca8d0` 将原始值 7200（2小时）改为 300（5分钟），用于系统测试快速验证。生产前必须改回 7200，否则 license 下载窗口仅5分钟，极易超时导致设备永久降级 FORCE_SIM |
| `at_init()` 返回 -1 未在 `dial_mng_new()` 中检查 | 低 | `e20da07` 修复了 `at_init()` 使其返回 -1，但 `dial.c:55` 处 `p_dial_mng->smd_fd = at_init()` 未判断返回值。若 `/dev/smd8` 打开失败，所有 AT 命令（包括 AT+COPS=0）将对 fd=-1 操作，行为未定义 |

---

## 五、可移植性结论

**结论：双卡切换核心逻辑已在 `feature/dual_channel_switch` 分支完整实现，无需额外移植。**

理由：

1. **7条提交构成完整实现**：从 9c1ec55 的基础框架，经 b1ca8d0 的系统测试问题修复（fallback 计时器、RBMaster 延迟启动、业务层检测等），到 9d15833 的注册超时切换，各关键状态和策略已全部覆盖。

2. **设计对应 CLAUDE.md 描述**：四态 probe、四种策略、五个切换路径（SIM→RL、RL→SIM、reg超时→RL、RL超时→SIM、策略3重启）均已实现，计时器字段与文档一致。

3. **唯一预上线必改项**：`LICENSE_WAIT_TIMEOUT_SEC` 需由 300 改回 7200，此处是有意测试降值，不影响其他逻辑正确性。

4. **低优先级已知缺陷**：`at_init()` 返回值未检查，属于防御性编程缺失，不影响正常运行路径。
