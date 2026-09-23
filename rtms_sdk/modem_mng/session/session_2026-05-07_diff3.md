# 会话摘要：差异3 修复 — EG25 time_t → struct timespec（CLOCK_MONOTONIC）

**日期**：2026-05-07  
**操作者**：yuping.liu  
**涉及文件**：`eg25/dial/dial.h`、`eg25/dial/dial.c`  
**提交说明**：`[CHGLOG] 完善 Roamlink 双卡切换逻辑（EG25/EC200A），新增 status 模块`

---

## 一、会话背景

本会话是上一会话（Roamlink 双卡切换移植）的延续。上一会话已完成：
- 从 `open_dial_for_artery` 移植 Roamlink 双卡切换逻辑到 EG25 / EC200A 两个平台
- 新增 `eg25/roamlink/`、`ec200a/roamlink/` 平台适配模块
- 新增 `status/` 模块（输出 `/tmp/dial_status` 运行状态）
- 更新 `CMakeLists.txt` 纳入新模块

移植完成后对比参考代码发现三处结构性差异。本会话：
1. 先逐一**论述**三个差异能否实现及如何实现
2. 论述修改总共涉及多少处
3. 会话开始前用户先自行提交了当前工作区改动
4. 实现**差异3**（差异1/2 未在本会话实现）

---

## 二、三个差异的完整论述

### 差异1：能否为 EG25/EC200A 加入 `dial_stat_roamlink_starting` / `roamlink_active` 状态

参考代码（`open_dial_for_artery`）有两个独立状态：
- `dial_stat_roamlink_starting`：TCP 连通测试，每 10s 一次，300s 超时回落 SIM
- `dial_stat_roamlink_active`：TCP 测试每 30s；rx_packets 120s 无数据触发回落

**EG25 — 技术可行，有前提**

EG25 已有完整状态机（`dial_stat_enu` 枚举），扩展路径清晰，但核心前提是：
`eg25/nw/` 模块中必须存在 `nw_tcp_connectivity_test()` 和 `nw_get_rmnet_rx_packets_sum()` 两个函数。若存在则需以下 7 处改动：

1. `dial.h`：枚举新增两个值
2. `dial.h`：`dial_mng_t` 新增 `tcp_test_timer`、`roamlink_rx_packets`、`roamlink_no_data_timer`、`sim_fallback_timer`、`roamlink_fallback_timer`（均为 `struct timespec`）
3. `dial.c` `dial_stat_none`：policy 1/3 时绕过 SIM init 直接跳转 `roamlink_starting`（`sim_initialized` 此时仍 false）
4. `dial.c`：新增 `dial_stat_roamlink_starting` case
5. `dial.c`：新增 `dial_stat_roamlink_active` case
6. `dial.c`：heartbeat 现有 Roamlink 连通性检测迁移到新状态
7. `dial.c`：`reg_timeout_handler` 跳转目标改为 `roamlink_starting`

若函数不存在，当前 heartbeat + ICMP ping 方案已是最优替代，不必强加状态。

**EC200A — 不适用**

EC200A 是 SDK callback + 五阶段轮询架构（ST_STATUS/ST_SIM/ST_SIGNAL/ST_PING/ST_RECOVERY），无状态机。`is_roamlink_active` 布尔标志 + ST_PING/ST_RECOVERY 已在功能上等价于 Roamlink 活跃状态，强行加状态等于重写整个架构，代价远超收益。

---

### 差异2：EC200A 注册超时如何更好处理

现状：EC200A `ST_SIM` 阶段对注册失败只是无限重试，无超时机制（EG25 有 `reg_timeout_handler` 处理 300s 超时）。

可增量补充的方案（无需改变整体轮询架构）：

1. `dialer_ec200a.cpp`：添加 `uint64_t reg_fail_start_ms = 0` 局部变量
2. `dialer_ec200a.cpp` ST_SIM 阶段：注册失败时若 `reg_fail_start_ms == 0` 则记录 `getMonotonicMs()`；成功时清零
3. `dialer_ec200a.cpp` ST_SIM 或 ST_RECOVERY 判断：超过 300s → policy 1/2 且 `roamlink_available` 则切 Roamlink；否则触发 CFUN 重置

共 3 处修改点，均在 `dialer_ec200a.cpp`。**本会话未实现，留待后续。**

---

### 差异3：EG25 time_t → struct timespec（CLOCK_MONOTONIC）

**问题根源**

`dial_mng_t` struct 中四个时间字段使用 `time_t`（`time(NULL)`，CLOCK_REALTIME）：
```c
time_t roamlink_start_ts;
time_t roamlink_last_ping_ts;
time_t license_wait_start;
time_t license_check_timer;
```

而状态机核心计时器 `dial_timer` 已用 `struct timespec`（CLOCK_MONOTONIC）。

混用带来的具体风险：
- NTP 校时时钟向前跳变（如设备联网后同步到正确时间，时钟突然快进几分钟）：`now - roamlink_start_ts` 的值瞬间虚增，可能错误触发 300s Roamlink 超时回落
- NTP 时钟向后回拨：超时永远不触发，Roamlink 故障通道永远不切换

另外 license 监控块嵌在 30s heartbeat 节拍内，依赖 `time_t now`，导致只有 heartbeat 触发时才检测，精度差。

---

## 三、修改点全景分析

用户提问"总共要修改几点"后，按三个差异拆解如下：

| 差异 | 涉及文件 | 修改点数 | 前提条件 |
|------|----------|----------|----------|
| 差异1（Roamlink 状态机） | `dial.h` + `dial.c` | 7 | `nw_tcp_connectivity_test` 函数存在 |
| 差异2（EC200A 注册超时） | `dialer_ec200a.cpp` | 3 | 无 |
| 差异3（struct timespec） | `dial.h` + `dial.c` | 7（必要4 + 可选扩展roamlink_ts 3） | 无 |

**本会话实现差异3，共 7 处修改点：**

| # | 位置 | 内容概述 |
|---|------|----------|
| 1 | `dial.h` struct | 4 个 `time_t` 字段改为 `struct timespec` |
| 2 | `dial.c` `dial_mng_new()` | license / roamlink_ts 初始化 |
| 3 | `dial.c` 循环结构重整 | `clock_gettime` 前移 + license 监控块搬迁 |
| 4 | `dial.c` heartbeat `dstat.license_wait_sec` | `now - ts` 改为 `diff().tv_sec` |
| 5 | `dial.c` heartbeat 所有 Roamlink 时间戳赋值/比较 | ~8 处 `now` / `== 0` / `!= 0` 替换 |
| 6 | `dial.c` heartbeat `dstat.roamlink_connect_wait_sec` | `now - ts` 改为 `diff().tv_sec` |
| 7 | `dial.c` `reg_timeout_handler` | `time(NULL)` 改为 `cur_timer` |

---

## 四、实现详情

### 修改点1：dial.h — 四个字段类型变更

```c
/* 修改前 */
time_t roamlink_start_ts;      /* 发出 start 命令的时间（0=未启动） */
time_t roamlink_last_ping_ts;  /* Roamlink 通道上最近一次 ping 成功的时间（0=从未成功） */
bool   license_pending;
time_t license_wait_start;
time_t license_check_timer;

/* 修改后 */
struct timespec roamlink_start_ts;      /* 发出 start 命令的时间（tv_sec=0=未启动） */
struct timespec roamlink_last_ping_ts;  /* Roamlink 通道上最近一次 ping 成功的时间（tv_sec=0=从未成功） */
bool            license_pending;
struct timespec license_wait_start;
struct timespec license_check_timer;
```

---

### 修改点2：dial.c dial_mng_new() — 初始化

**license 字段（PROBE_LICENSE_MISSING 分支）：**
```c
/* 修改前 */
p_dial_mng->license_wait_start  = time(NULL);
p_dial_mng->license_check_timer = 0;

/* 修改后 */
clock_gettime(CLOCK_MONOTONIC, &p_dial_mng->license_wait_start);
memset(&p_dial_mng->license_check_timer, 0, sizeof(struct timespec));
```

**Roamlink 时间戳（函数末尾）：**
```c
/* 修改前 */
p_dial_mng->is_roamlink_active    = false;
p_dial_mng->roamlink_start_ts     = 0;
p_dial_mng->roamlink_last_ping_ts = 0;

/* 修改后 */
p_dial_mng->is_roamlink_active    = false;
memset(&p_dial_mng->roamlink_start_ts,     0, sizeof(struct timespec));
memset(&p_dial_mng->roamlink_last_ping_ts, 0, sizeof(struct timespec));
```

**循环前启动块（policy 1/3 初始启动 Roamlink）：**
```c
/* 修改前 */
p_dial_mng->is_roamlink_active    = true;
p_dial_mng->roamlink_start_ts     = time(NULL);
p_dial_mng->roamlink_last_ping_ts = 0;

/* 修改后 */
p_dial_mng->is_roamlink_active    = true;
clock_gettime(CLOCK_MONOTONIC, &p_dial_mng->roamlink_start_ts);
memset(&p_dial_mng->roamlink_last_ping_ts, 0, sizeof(struct timespec));
/* 注：此处在 while 循环前，cur_timer 尚未初始化，只能用 clock_gettime */
```

---

### 修改点3：dial.c — 循环结构重整（最核心改动）

**原结构问题**：`clock_gettime` 在 switch 前（heartbeat 之后），导致 license 监控块在 heartbeat 内只能用 `time_t now`，且每 30s 才执行一次。

```
/* 原结构 */
while(true) {
    // heartbeat（30s节拍）{
    //     ... ping检查、Roamlink守护 ...
    //     license监控(time_t now)  ← 每30s执行，依赖 time_t
    // }
    clock_gettime(CLOCK_MONOTONIC, &cur_timer);  // ← 仅供 switch 使用
    switch(dial_st) { ... }
    sleep(1);
}
```

**新结构**（对齐参考代码 `open_dial_for_artery`）：
```
/* 新结构 */
while(true) {
    clock_gettime(CLOCK_MONOTONIC, &cur_timer);  // ← 移到顶部，全局共享

    // license监控(cur_timer) ← 每次循环执行，内部有60s间隔守卫
    if (license_pending) {
        wait_elapsed = diff(license_wait_start, cur_timer).tv_sec;
        // 超时检查 / 文件检查
    }

    // heartbeat（30s节拍）{
    //     ... ping检查、Roamlink守护（全部用 cur_timer）...
    //     license监控已移走
    // }
    switch(dial_st) { ... }
    sleep(1);
}
```

**license 监控块移动后的代码（使用 struct timespec）：**
```c
if (p_dial_mng->license_pending)
{
    long wait_elapsed = diff(p_dial_mng->license_wait_start, cur_timer).tv_sec;

    if (wait_elapsed > LICENSE_WAIT_TIMEOUT_SEC)
    {
        dial_log("[ROAMLINK] license wait timeout (%lds), "
                 "giving up license_pending mode, stay FORCE_SIM\n", wait_elapsed);
        p_dial_mng->license_pending = false;
    }
    else if (p_dial_mng->license_check_timer.tv_sec == 0 ||
             diff(p_dial_mng->license_check_timer, cur_timer).tv_sec >= LICENSE_CHECK_INTERVAL_SEC)
    {
        p_dial_mng->license_check_timer = cur_timer;
        dial_log("[ROAMLINK] license check (%lds / %ds elapsed)...\n",
                 wait_elapsed, LICENSE_WAIT_TIMEOUT_SEC);
        if (roamlink_eg25_license_appeared()) {
            dial_log("[ROAMLINK] license file appeared! backing up and rebooting\n");
            roamlink_eg25_license_backup_and_reboot(); /* 不返回 */
        } else {
            dial_log("[ROAMLINK] license not yet available\n");
        }
    }
}
```

**改动的具体行为变化**：
- license 文件检查：从每 30s 检查一次改为每 1s 检查（内部 60s 守卫不变，频率实际不变）
- 总超时检测精度：从"最多晚 30s 发现"改为"最多晚 1s 发现"
- `cur_timer` 在 switch 里比原来"略旧"（早了 heartbeat 执行时间，通常 < 2s），对 120s/300s/3600s 超时阈值无实质影响

---

### 修改点4：heartbeat — dstat.license_wait_sec

```c
/* 修改前 */
dstat.license_wait_sec = p_dial_mng->license_pending
                        ? (long)(now - p_dial_mng->license_wait_start) : 0;

/* 修改后 */
dstat.license_wait_sec = p_dial_mng->license_pending
                        ? diff(p_dial_mng->license_wait_start, cur_timer).tv_sec : 0;
```

---

### 修改点5：heartbeat — Roamlink 时间戳所有赋值和比较（共 8 处）

**① ping 成功时更新 last_ping_ts：**
```c
/* 修改前 */ p_dial_mng->roamlink_last_ping_ts = now;
/* 修改后 */ p_dial_mng->roamlink_last_ping_ts = cur_timer;
```

**② policy1 回切 Roamlink 成功：**
```c
/* 修改前 */
p_dial_mng->roamlink_start_ts     = now;
p_dial_mng->roamlink_last_ping_ts = 0;
/* 修改后 */
p_dial_mng->roamlink_start_ts     = cur_timer;
memset(&p_dial_mng->roamlink_last_ping_ts, 0, sizeof(struct timespec));
```

**③ policy2 回切 SIM，清空 Roamlink 时间戳：**
```c
/* 修改前 */
p_dial_mng->roamlink_start_ts     = 0;
p_dial_mng->roamlink_last_ping_ts = 0;
/* 修改后 */
memset(&p_dial_mng->roamlink_start_ts,     0, sizeof(struct timespec));
memset(&p_dial_mng->roamlink_last_ping_ts, 0, sizeof(struct timespec));
```

**④ Roamlink 通道守护 — 超时判断比较（最关键的比较替换）：**
```c
/* 修改前 */
bool roamlink_connect_timeout =
    (p_dial_mng->roamlink_last_ping_ts == 0 &&
     p_dial_mng->roamlink_start_ts != 0 &&
     (now - p_dial_mng->roamlink_start_ts) > ROAMLINK_CONNECT_TIMEOUT_SEC);
bool roamlink_fail_timeout =
    (p_dial_mng->roamlink_last_ping_ts != 0 &&
     (now - p_dial_mng->roamlink_last_ping_ts) > ROAMLINK_CONNECT_TIMEOUT_SEC);

/* 修改后 */
bool roamlink_connect_timeout =
    (p_dial_mng->roamlink_last_ping_ts.tv_sec == 0 &&
     p_dial_mng->roamlink_start_ts.tv_sec != 0 &&
     diff(p_dial_mng->roamlink_start_ts, cur_timer).tv_sec > ROAMLINK_CONNECT_TIMEOUT_SEC);
bool roamlink_fail_timeout =
    (p_dial_mng->roamlink_last_ping_ts.tv_sec != 0 &&
     diff(p_dial_mng->roamlink_last_ping_ts, cur_timer).tv_sec > ROAMLINK_CONNECT_TIMEOUT_SEC);
```

**⑤ Roamlink 守护切回 SIM，清空时间戳：**
```c
/* 修改前 */
p_dial_mng->roamlink_start_ts     = 0;
p_dial_mng->roamlink_last_ping_ts = 0;
/* 修改后 */
memset(&p_dial_mng->roamlink_start_ts,     0, sizeof(struct timespec));
memset(&p_dial_mng->roamlink_last_ping_ts, 0, sizeof(struct timespec));
```

**⑥ FORCE_ROAMLINK 重启服务，更新 start_ts：**
```c
/* 修改前 */
p_dial_mng->roamlink_start_ts     = now;
p_dial_mng->roamlink_last_ping_ts = 0;
/* 修改后 */
p_dial_mng->roamlink_start_ts     = cur_timer;
memset(&p_dial_mng->roamlink_last_ping_ts, 0, sizeof(struct timespec));
```

**⑦ SIM→Roamlink 主动切换成功，记录 start_ts：**
```c
/* 修改前 */
p_dial_mng->roamlink_start_ts     = now;
p_dial_mng->roamlink_last_ping_ts = 0;
/* 修改后 */
p_dial_mng->roamlink_start_ts     = cur_timer;
memset(&p_dial_mng->roamlink_last_ping_ts, 0, sizeof(struct timespec));
```

---

### 修改点6：heartbeat — dstat.roamlink_connect_wait_sec

```c
/* 修改前 */
if (p_dial_mng->roamlink_last_ping_ts == 0 &&
    p_dial_mng->roamlink_start_ts != 0)
    dstat.roamlink_connect_wait_sec =
        (long)(now - p_dial_mng->roamlink_start_ts);

/* 修改后 */
if (p_dial_mng->roamlink_last_ping_ts.tv_sec == 0 &&
    p_dial_mng->roamlink_start_ts.tv_sec != 0)
    dstat.roamlink_connect_wait_sec =
        diff(p_dial_mng->roamlink_start_ts, cur_timer).tv_sec;
```

---

### 修改点7：reg_timeout_handler — Roamlink 启动时记录 start_ts

```c
/* 修改前 */
p_dial_mng->is_roamlink_active    = true;
p_dial_mng->roamlink_start_ts     = time(NULL);
p_dial_mng->roamlink_last_ping_ts = 0;

/* 修改后 */
p_dial_mng->is_roamlink_active    = true;
p_dial_mng->roamlink_start_ts     = cur_timer;
memset(&p_dial_mng->roamlink_last_ping_ts, 0, sizeof(struct timespec));
/* 注：cur_timer 在循环顶部已赋值，此处在 switch 之后通过 goto 跳入，cur_timer 仍有效 */
```

---

## 五、diff() 函数说明

`dial.c` 第 24 行已定义 `diff()` 函数，计算两个 `struct timespec` 之差：

```c
struct timespec diff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec - start.tv_nsec) < 0) {
        temp.tv_sec  = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    } else {
        temp.tv_sec  = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp;
}
```

所有超时判断只用 `.tv_sec`（秒级精度），纳秒部分不影响结果。

---

## 六、EC200A 是否需要同样修改

**结论：不需要。**

原因逐条说明：

1. **license 计时已是 monotonic**：EC200A 的 `license_wait_ts` / `license_check_ts` 是 `dial_loop()` 内部的 `uint64_t` 局部变量，使用 `getMonotonicMs()`（单调毫秒计时器），本质上等同于 CLOCK_MONOTONIC，本就不受 NTP 影响。

2. **没有写入 struct 的 time_t 字段**：EC200A 的 license 状态是函数局部变量，不是 struct 成员，不存在 EG25 那种 struct 字段混用两套时钟的问题。

3. **Roamlink 时间戳机制不同**：EC200A 的 Roamlink 连接超时通过 ST_PING/ST_RECOVERY 阶段里的 `uint64_t` + `getMonotonicMs()` 控制，同样是单调时钟。

差异3 是 EG25 `dial_mng_t` 设计时的历史遗留问题，与 EC200A 架构无关。

---

## 七、正确性验证

### 验证命令

```bash
# 检查四个字段是否有残留的 time_t 风格操作
grep -n "roamlink_start_ts\|roamlink_last_ping_ts\|license_wait_start\|license_check_timer" \
  eg25/dial/dial.c \
  | grep -v "struct timespec\|clock_gettime\|memset\|diff(\|\.tv_sec\|cur_timer"
# 输出为空 → 全部替换完毕

# 确认仅剩的两处 time(NULL) 均合理
grep -n "time(NULL)" eg25/dial/dial.c
# 470: task_start_ts = time(NULL)  → 进程 uptime，用墙钟合理
# 605: time_t now = time(NULL)     → heartbeat 30s 节拍判断，time_t 局部变量体系
```

### 逻辑不变性逐项确认

| 修改点 | 逻辑不变性说明 |
|--------|----------------|
| 类型变更 | 已同步更新全部引用，无遗漏 |
| `clock_gettime` 前移 | 对 switch 超时误差 < 2s，相对 120s/300s 阈值可忽略 |
| license 监控前移 | 内部 60s 守卫不变，检查频率实际不变，只是检测更及时 |
| `diff()` 替换 `now - ts` | 计算语义相同（终 - 始），结果等价 |
| `.tv_sec == 0` 替换 `== 0` | `memset(0)` 初始化保证 tv_sec 和 tv_nsec 均为 0，哨兵逻辑等价 |
| Roamlink 比较用 `cur_timer` | cur_timer 在循环顶部赋值，heartbeat 内偏差 < 2s，对 300s 超时无影响 |
| `reg_timeout_handler` 用 `cur_timer` | goto 跳转后 cur_timer 仍是本次循环的值，有效 |

### 唯一需关注的边界情况

`roamlink_start_ts.tv_sec != 0` 哨兵：CLOCK_MONOTONIC 从系统开机 0 开始，极端情况下（开机极短时间内）tv_sec 可能为 0。但守护进程启动、完成 probe 流程、再到首次调用 `start_service`，实际经过时间远大于 1s，此边界情况在实践中不会出现。

---

## 八、附：提交说明讨论

用户在实现差异3之前自行提交了当前工作区（Roamlink 移植成果）。提交说明经过一轮讨论后确定：

**初版（过于强调"新增"）：**
```
[ADD] 移植 Roamlink 双卡切换功能（EG25/EC200A）并新增 status 模块
```

**修正版（更准确反映"完善"性质）：**
```
[CHGLOG] 完善 Roamlink 双卡切换逻辑（EG25/EC200A），新增 status 模块

- 补全 probe 四态处理、license_pending 模式、注册超时自动切卡
- 修复 dial_timer 重置缺失、re-probe 失败路径、重复 start_service 等问题
- 新增 eg25/roamlink、ec200a/roamlink 平台适配模块
- 新增 status 模块（输出 /tmp/dial_status 运行状态）
- 更新 CMakeLists.txt 纳入新模块
```

使用 `[CHGLOG]` 而非 `[ADD]` 的原因：Roamlink 框架之前就有，本次是补全逻辑而非从零新增，与仓库其他 `[CHGLOG]` 提交风格一致。

---

## 九、附：ql_netcall 注释说明

`dial_task()` 函数 Roamlink 启动块之前有一段注释：

```c
// 旧逻辑中，这里会通过外部命令 ql_netcall -p 1 -r 5 -d 做拨号尝试，
// 现在统一改为仅使用本进程内部的拨号状态机，不再依赖 ql_netcall。
```

**含义**：`ql_netcall` 是 Quectel EG25 SDK 提供的命令行拨号工具。旧版本在状态机启动前曾用 `system("ql_netcall -p 1 -r 5 -d")` 做一次快速预拨（`-p 1` 指定 profile，`-r 5` 重试5次，`-d` 发起拨号）。当前版本已完全移除该调用，拨号全权由内部状态机（`dial_stat_start_call` → `dail_start_data_call()`）负责。

**是否影响执行**：不影响。该注释下方为空，注释本身仅是代码演变历史记录，无任何运行时代码被遗漏或替换错误。

---

## 十、遗留问题

| 差异 | 状态 | 后续步骤 |
|------|------|----------|
| 差异1（EG25 Roamlink 状态机扩展） | **未实现** | 先确认 `eg25/nw/` 是否有 `nw_tcp_connectivity_test()` 和 `nw_get_rmnet_rx_packets_sum()`，若存在则可实施 7 处改动 |
| 差异2（EC200A 注册超时处理） | **未实现** | `dialer_ec200a.cpp` 增量添加 `reg_fail_start_ms` 计时及切换逻辑，共 3 处 |
| 差异3（struct timespec） | **已完成** | — |
