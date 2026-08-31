# eg25/dial 模块深度分析

> 分析对象：`eg25/dial/dial.h` + `eg25/dial/dial.c`（共 2046 行）  
> 版本：modem_mng v1.31.2（更新至 commit b1b5516）  
> 分析日期：2026-05-26（最后更新：2026-05-28）

---

## 1. 模块概览

### 1.1 定位与职责

`eg25/dial` 是 EG25 平台的**拨号主控模块**，是整个 `modem_mng` 进程的核心线程入口。它的职责覆盖从硬件初始化到网络连通维持的全生命周期：

- 驱动从"未初始化"到"网络已连接"的完整 SIM 拨号状态机（19 个状态）
- 网络连通性的持续监控（30s 心跳 + ping 抖动过滤 + 小区切换检测）
- 双卡切换：物理 SIM ↔ Roamlink 虚拟 SIM，按 4 种策略自动倒换
- L1/L2/L3 分级恢复（仅强制物理 SIM 策略下）
- 启动诊断（固件版本 / 温度 / CFUN / PDP 配置，SDK 初始化后一次性采集）
- 状态结构化写入 `/tmp/dial_status`（每 30s 刷新，供 nanomsg handler 等外部进程查询）

### 1.2 对外接口（`dial.h` 声明）

| 函数 | 调用方 | 说明 |
|---|---|---|
| `dial_mng_t *dial_mng_new(void)` | `dialer_eg25.c` | 分配并初始化 dial_mng_t，含 Roamlink probe |
| `void *dial_task(void *arg)` | `dialer_eg25.c`（pthread） | 主拨号线程，无限循环 |
| `bool get_tz_after_connected(dial_mng_t *)` | `dial_task` 内部，也在主循环顶部 | 查询时区，全局只执行一次 |
| `void ipv4_data_call_info_init(dial_mng_t *)` | `dial_task` 内 `wait_for_connect` 成功时 | 填充 IPv4 地址 + 网卡信息 |

### 1.3 依赖模块

| 头文件 | 模块职责 |
|---|---|
| `at.h` | AT 串口操作，`Ql_SendAT()`，`at_init()` |
| `nw.h` | 网络状态查询（SIM 在位、注册状态、CSQ、rmnet rx_packets）|
| `sim.h` | SIM 操作（`sim_op()`、`sim_op_handler()`，读 ICCID/IMSI）|
| `apn.h` | APN 查询（根据 ICCID 匹配）、profile_idx 扫描 |
| `tz.h` | 时区字符串解析（`parse_tz_info`）与系统时区写入（`save_tz_info`）|
| `roamlink.h` | Roamlink 双卡：probe/start/stop/policy/license 操作 |
| `dial_status.h` | `/tmp/dial_status` JSON 结构化写入 |
| `nanomsg_process_cinterface.h` | 向 nanomsg handler 写入 ICCID/IMSI（`set_eg25_iccid/imsi`）|
| `cc_deque.h` | 运营商列表双端队列（`AT+COPS=?` 结果存储）|
| `led_control_c_api.h` | LED 状态指示（网络通 → 常亮，断网/切换 → 闪烁）|
| `logger.h` / `logger_sd.h` | 内存日志（`LOG_I/E/D`）+ SD 卡持久日志（`dial_log`）|

---

## 2. 头文件完整解析（dial.h）

### 2.1 版本宏

```c
MODEM_MNG_VERSION_MAIN  = 1
MODEM_MNG_VERSION_SUB   = 31
MODEM_MNG_VERSION_PATCH = 2
// → "1.31.2"，写入 /tmp/dial_status 的 version 字段
```

### 2.2 超时常量

| 宏 | 值 | 含义 | 使用位置 |
|---|---|---|---|
| `DIAL_TIMEOUT_SECONDS` | 120 | 拨号等待连接超时；`net_connected` 下 rx_packets 静止超时 | `wait_for_connect`、`net_connected` case |
| `SIM_CHECK_TIMEOUT_SECONDS` | 3600 | SIM 检测持续失败 1 小时后触发 CFUN 重置 | `sim_check` case |
| `REG_CHECK_TIMEOUT_SECONDS` | 300 | 注册超时（`reg_check` + `cereg_check` 共享同一计时器） | 两个 check case + `reg_timeout_handler` |

注意：`DIAL_TIMEOUT_SECONDS` 在 `dial.c` 开头也有一次 `#define`（重复定义，值相同），以 `dial.c` 内的局部定义生效（头文件版本被 `.c` 版本覆盖）。

Roamlink 相关阈值均在 `roamlink.h` 中定义，`dial.c` 通过 `#include "roamlink.h"` 直接使用：
- `ROAMLINK_SWITCH_TIMEOUT_SEC`：SIM 断网多久切 Roamlink
- `ROAMLINK_CONNECT_TIMEOUT_SEC`：Roamlink starting/active 无 ping 多久算超时
- `ROAMLINK_NO_DATA_TIMEOUT_SEC`：业务层 rx_packets 无增长多久算超时
- `SIM_FALLBACK_RETRY_SEC`：PREFER_ROAMLINK 策略，SIM 备用稳定多久后尝试回切
- `ROAMLINK_FALLBACK_RETRY_SEC`：PREFER_SIM 策略，Roamlink 备用稳定多久后尝试回切
- `LICENSE_WAIT_TIMEOUT_SEC`（300）：license_pending 最长等待
- `LICENSE_CHECK_INTERVAL_SEC`（60）：license 文件轮询间隔

### 2.3 dial_stat_enu — 19 个状态逐一说明

| 状态 | 进入条件 | 本状态执行内容 | 正常出口 | 异常/超时出口 |
|---|---|---|---|---|
| `dial_stat_none` | 初始状态 | 无操作 | `init` | — |
| `dial_stat_init` | `none` 或 Roamlink 切回 SIM 且 `!sim_initialized` | `QL_MCM_NW_Client_Init` + `sim_op(SIM_OP_INIT)` + 打印固件信息 | `sim_init` | `modem_need_reset=true`，原地重试 |
| `dial_stat_sim_init` | `init` 成功；`start_cfun` 完成；Roamlink 切回 SIM 且 `sim_initialized` | `sim_op(SIM_OP_INIT, NULL)` | `sim_check`（重置 dial_timer）| `modem_need_reset=true`，sleep(2) 重试 |
| `dial_stat_sim_check` | `sim_init` 成功 | `nw_get_sim_card_status(h_nw_client)` | `sim_op`；更新 `sim_present=1` | `sim_present=0`；sleep(2)；超时 3600s → `stop_cfun` |
| `dial_stat_sim_op` | `sim_check` 成功 | `sim_op_handler(get_iccid)` 读 ICCID；**`sim_op_handler(get_imsi)` 显式读 IMSI**；读 IMEI（`AT+CGSN`）；读 PDP 配置；写入 nano_handler | `reg_check`（重置 dial_timer 为注册超时起点）| `modem_need_reset=true`，原地重试 |
| `dial_stat_reg_check` | `sim_op` 成功；断线重连；Roamlink 切回 SIM 回到此状态 | `nw_reg_status_check(h_nw_client)` | `cereg_check` | sleep(2)；超时 300s → `reg_timeout_handler` |
| `dial_stat_cereg_check` | `reg_check` 成功 | `nw_at_get_cereg_stat(smd_fd)`（AT+CEREG）；成功后读初始 Cell ID | `precondition_check` | sleep(10)；超时 300s（共享 dial_timer）→ `reg_timeout_handler` |
| `dial_stat_precondition_check` | `cereg_check` 成功 | `QL_Data_Call_Init_Precondition()` | `pre_start_call`（重置 profile_idx=-1）| `modem_need_reset=true`，原地重试 |
| `dial_stat_pre_start_call` | `precondition_check` 成功 | profile_idx==-1：`apn_get_apn_obj(iccid)` + `apn_scan_idx()`；profile_idx 有效：`QL_Data_Call_Set_Default_Profile()` | `start_call` | `modem_need_reset=true`（profile 查询分两拍，第二拍才转移）|
| `dial_stat_start_call` | `pre_start_call` 有效 profile | `dail_start_data_call()`（含 `QL_Data_Call_Init` + `QL_Data_Call_Start`）| `wait_for_connect`（记录 dial_timer）| 原地重试 |
| `dial_stat_stop_call` | **未使用**（定义保留）| — | — | — |
| `dial_stat_stop_cfun` | `select_oper` 完成；`sim_check` 超时；`reg_timeout_handler` 策略 4 | `AT+CFUN=0`，等 5s | `start_cfun` | 失败 2 次 → `reg_check` |
| `dial_stat_start_cfun` | `stop_cfun` 成功；`select_oper` 队列耗尽 | `AT+CFUN=1`，等 5s | `sim_init` | 失败 2 次 → `reg_check` |
| `dial_stat_list_oper` | `wait_for_connect` 超时且 `is_oper_select==true`；`select_oper` 失败后队列非空 | `AT+COPS=?`（超时 180s！）+ 解析结果入队 | `select_oper`（队列非空）| 失败 2 次 → `reg_check`；队列空 → `reg_check` |
| `dial_stat_select_oper` | `list_oper` 队列非空 | 取队首，`AT+COPS=1,2,<num>` | `stop_cfun`（成功选中后）| 失败 2 次 → 移除队首；队列空 → `start_cfun` |
| `dial_stat_wait_for_connect` | `start_call` 成功 | `nw_get_connect_state(profile_idx)` 轮询 | `net_connected`（连接成功，调用 `get_tz`、`ipv4_data_call_info_init`、`nw_mark_network_status(1)`）| 超时 120s → 先 stop，再判断 `is_oper_select` 决定去 `list_oper` 或 `reg_check` |
| `dial_stat_net_connected` | `wait_for_connect` 成功 | 每次循环：检查 `nw_get_connect_state()`；若已连接，读 rmnet rx_packets 与 dial_timer 配合检测数据停滞 | 无（持续运行）| SDK 报断线 → `reg_check`；120s rx_packets 无增长 → `reg_check`（**同时重置 dial_timer**，确保 reg_check 获得完整 300s 超时窗口）|
| `dial_stat_roamlink_starting` | 策略决策后 `roamlink_start_service()` 成功 | 仅检测连接超时（ping 成功由心跳推进状态）| （由心跳 ping 成功 → `roamlink_active`）| 超时：策略 1/2 → SIM 路径；策略 3 → 重启服务留在此状态 |
| `dial_stat_roamlink_active` | 心跳 ping 成功时从 `starting` 转入 | 检测 ping 超时 + 业务层 rx_packets 超时 | （持续运行，由心跳维持）| ping 超时或业务无数据超时：策略 1/2 → SIM；策略 3 → 重启回 `starting` |

### 2.4 dial_mng_t 结构体字段完整说明

#### 基础运行字段

| 字段 | 类型 | 初始值 | 含义与生命周期 |
|---|---|---|---|
| `dial_st` | `dial_stat_enu` | `dial_stat_none` | 状态机当前状态，状态机 switch 的核心 |
| `h_nw_client` | `nw_client_handle_type` | 0 | `dial_init()` 中由 `QL_MCM_NW_Client_Init` 写入；供 `nw_*` 函数传参 |
| `p_ipv4_data_call_info` | `ql_data_call_info_s *` | NULL | `ipv4_data_call_info_init()` 中 malloc 填充；断线时 free 后重新分配 |
| `p_ifname` | `char *` | NULL | `ipv4_data_call_info_init()` 中由 `nw_get_ifaddrs()` 返回 `strdup(ifa_name)` 填充；存储连接成功后的网卡名称字符串（如 `"rmnet_data0"`）；`free` 后重新分配 |
| `u64_if_rx_packets` | `uint64_t` | 0 | `net_connected` 状态下的 rmnet rx_packets 快照；每次读取后更新；静止 120s → 强制重拨 |
| `rsv[2]` | `uint8_t[2]` | 0 | 对齐保留字节，无实际使用 |
| `nw_node_name` | `char[NAME_MAX]` | "" | 保留字段，当前版本未见写入使用 |
| `profile_idx` | `int` | -1（`pre_start_call` 阶段）| 当前激活的 PDP profile；由 `apn_scan_idx()` 确定；`dail_start/stop_data_call()` 使用 |
| `p_sim_mng` | `sim_mng_t *` | calloc 分配，`sim_op_st=get_iccid` | SIM 管理结构，含 `sim_iccid[]/sim_imsi[]` 等字段 |
| `dial_timer` | `struct timespec` | 各状态单独重置 | **多用途超时计时器**：`sim_check` 起点（sim_init 成功时）；注册超时起点（`sim_op` 完成 / 断线重连 / 切换后）；`wait_for_connect` 起点；`net_connected` rx_packets 活跃更新 |
| `smd_fd` | `int` | `at_init()` 返回值 | AT 串口文件描述符；所有 `Ql_SendAT` 调用的第一个参数 |
| `deque_oper` | `CC_Deque *` | `cc_deque_new()` 分配 | 运营商列表队列；`list_oper` 填充，`select_oper` 消耗 |

#### 扩展功能字段

| 字段 | 类型 | 初始值 | 含义与生命周期 |
|---|---|---|---|
| `monitor` | `TrafficMonitorWrapper *` | NULL（外部注入）| 流量监控对象，`dial_task` 循环顶部调用 `cpactive_upt_atime()` 用的是 `cpa` |
| `cpa` | `void *` | NULL（外部注入）| cpactive 活跃保持对象；每次循环顶部 `cpactive_upt_atime(p_dial_mng->cpa)` 更新时间戳 |
| `nano_handler` | `void *` | NULL（外部注入）| `NanoReqHandlerWrapper *`；`sim_op` 成功后 `set_eg25_iccid/imsi()` 写入 ICCID/IMSI |
| `should_dial` | `bool` | `true`（`dial_task` 中设置）| 外部控制拨号开关；当前版本启动即置 true，未见外部修改逻辑 |
| `mutex` | `pthread_mutex_t` | `pthread_mutex_init` | 与 `cond` 配合的拨号控制锁；当前版本初始化后未见实际等待/通知 |
| `cond` | `pthread_cond_t` | `pthread_cond_init` | 条件变量；同上，预留接口 |
| `p_apn_obj` | `apn_obj_t *` | NULL | `pre_start_call` 阶段 `apn_get_apn_obj(iccid)` 填入；切换通道时 `p_apn_obj=NULL` 重置 |
| `is_func_called` | `bool` | `true` | **true = 下次 `dail_start_data_call()` 须重走 `QL_Data_Call_Init + Start`**；成功后置 false；L1/L2/断线重连时置 true |

#### 自检状态字段（`status_mutex` 保护）

| 字段 | 类型 | 含义 | 写入方 | 读取方 |
|---|---|---|---|---|
| `status_mutex` | `pthread_mutex_t` | 保护下面三个字段 | `dial_task` | `dial_task` / 外部查询线程 |
| `net_is_up` | `int` | 0=断网，1=已连接 | `set_net_status()` | nanomsg handler |
| `sim_present` | `int` | 0=无卡/未识别，1=有卡 | `set_sim_present()` | nanomsg handler |
| `signal_strength` | `int` | CSQ 值或 -1（未知）| `set_signal_strength()`（目前仅初始化为 -1，未见持续更新）| nanomsg handler |

#### Roamlink 双卡字段

| 字段 | 类型 | 初始值 | 含义 |
|---|---|---|---|
| `network_policy` | `int` | `roamlink_read_policy()` | 策略值（0=PREFER_SIM，1=PREFER_ROAMLINK，2=FORCE_ROAMLINK，3=FORCE_SIM）|
| `roamlink_available` | `bool` | probe 结果 | 启动时 probe 确定，运行期不变；false 时所有切换逻辑均跳过 |
| `is_roamlink_active` | `bool` | false | 当前通道是否为 Roamlink；影响心跳日志格式、L1/L2/L3 触发判断、`/tmp/dial_status` 内容 |
| `roamlink_start_ts` | `struct timespec` | 全零 | 发出 `roamlink_start_service()` 的时刻；tv_sec=0 表示未启动；`starting` 状态超时检测基准 |
| `roamlink_last_ping_ts` | `struct timespec` | 全零 | 最近一次 ping 成功时刻；`active` 状态超时检测基准；心跳 ping 成功时更新 |
| `roamlink_rx_packets` | `uint64_t` | 0 | 业务层 rmnet rx_packets 基准；进入 active 时初始化；每次有增长时更新 |
| `roamlink_no_data_timer` | `struct timespec` | 全零 | 上次 rx_packets 增长的时刻；业务层超时检测基准 |
| `sim_initialized` | `bool` | false → `dial_init()` 成功后置 true | 标记 `QL_MCM_NW_Client_Init` 是否已执行；Roamlink 切回 SIM 时用于判断跳到 `sim_init` 还是 `init` |

#### License 等待字段

| 字段 | 类型 | 含义 |
|---|---|---|
| `license_pending` | `bool` | true = probe 返回 LICENSE_MISSING 且备份恢复失败，等待 RBMaster 下载 license |
| `license_wait_start` | `struct timespec` | 进入 license_pending 模式时记录，计算总等待时长 |
| `license_check_timer` | `struct timespec` | 上次检查 license 文件的时刻（初始全零 = 首次立即检查）|
| `rbmaster_started` | `bool` | false → SIM 首次联网后 `roamlink_start_master()` 一次后置 true；防止重复启动 |

#### 断网统计字段

| 字段 | 类型 | 写入时机 | 用途 |
|---|---|---|---|
| `outage_count` | `int` | 每次 `start_fail_ts` 清零（网络恢复）时 +1 | `/tmp/dial_status` 中断网次数 |
| `last_outage_sec` | `long` | 同上，记录本次断网持续时长 | 状态输出 |
| `total_outage_sec` | `long` | 同上，累加 | 状态输出 |
| `roamlink_fail_count` | `int` | Roamlink 切换失败、ping 超时、业务层超时时 +1 | 心跳日志 `RL_FAIL` + 状态输出 |

---

## 3. 函数完整分析

### 3.1 `struct timespec diff(start, end)`

**分类**：工具函数  
**功能**：计算两个 `struct timespec` 的差值，处理纳秒借位。  
**关键逻辑**：若 `end.tv_nsec < start.tv_nsec`，秒数借一位（`tv_sec-1`），纳秒加 1e9。  
**调用关系**：贯穿 `dial_task` 所有超时判断，如 `diff(p_dial_mng->dial_timer, cur_timer).tv_sec > TIMEOUT`。  
**注意**：未做 `end < start` 的整体检查（单调时钟 `CLOCK_MONOTONIC` 下不会出现，但若误用 REALTIME 则有溢出风险）。

---

### 3.2 `dial_mng_t *dial_mng_new(void)`

**分类**：初始化  
**功能**：分配并完整初始化 `dial_mng_t`，是 `dial_task` 启动前的前置步骤。

**关键逻辑（按执行顺序）**：

1. `calloc(1, sizeof(dial_mng_t))` + `memset` 全清零（double 保险）
2. 初始化 `sim_mng_t`（`sim_op_st=sim_op_stat_get_iccid`）
3. `at_init()` 打开 AT 串口 → `smd_fd`
4. `roamlink_set_fd(smd_fd)` 将 AT 串口句柄注入 roamlink 模块
5. `cc_deque_new(&deque_oper)` 创建运营商队列
6. `is_func_called = true`（标记需要执行 `QL_Data_Call_Init`）
7. **Roamlink probe 决策**（见第 5.2 节详述）
8. 策略为 PREFER_SIM 或 FORCE_SIM 且 RBMaster 残留运行 → `roamlink_stop_service()` + `sleep(2)` 清理
9. `roamlink_mark_network_type(NETWORK_TYPE_SIM)` 标记初始网络类型
10. 清零所有 Roamlink 时间戳

**副作用**：打开 AT 串口、可能停止 RBMaster 进程、写入 `/tmp/network_type`。

---

### 3.3 `bool dial_init(dial_mng_t *p_dial_mng)`

**分类**：初始化  
**功能**：调用 SDK NW Client 初始化 + SIM 操作初始化。  
**关键逻辑**：
- `QL_MCM_NW_Client_Init(&h_nw_client)` 失败直接返回 false
- `sim_op(SIM_OP_INIT, NULL)` 初始化 SIM 操作状态机
- 成功时 `dial_task` 中将 `sim_initialized=true`（标记 NW Client 已初始化）

---

### 3.4 `void dial_data_call_state_callback(ql_data_call_state_s *state)`

**分类**：SDK 异步回调  
**功能**：SDK 数据呼叫状态变化时回调，打印连接信息并更新网络状态文件。  
**关键逻辑**：
- `QL_DATA_CALL_CONNECTED`：打印 IPv4 地址/网关/DNS 或 IPv6 地址；调用 `nw_mark_network_status(1)`
- 其他状态：打印错误码；调用 `nw_mark_network_status(0)`
- 此回调**不**直接改变 `dial_st`（状态迁移由 `dial_task` 主循环的 `nw_get_connect_state()` 轮询驱动）

**注意**：IPv6 分支使用 `inet_ntop` + `INET6_ADDRSTRLEN` 缓冲，IPv4 分支直接用 `inet_ntoa`（非线程安全，但此处在回调线程中单独调用，无明显竞态）。

---

### 3.5 `bool dail_start_data_call(dial_mng_t *p_dial_mng)`

**分类**：核心逻辑  
**功能**：启动数据呼叫，含防重复 Init 保护。

**关键逻辑**：
```
if (is_func_called):
    QL_Data_Call_Init(callback)  ← 失败则 exit(0)！
if (is_func_called):             ← 注意：Init 失败会 exit，所以第二个 if 成立说明 Init 成功
    QL_Data_Call_Start(&data_call, &err)
    成功 → is_func_called=false，返回 true
    失败 → 记录错误，返回 false（is_func_called 保持 true，下次重试仍走 Init+Start）
```

**重要细节**：
- `QL_Data_Call_Init` 失败调用 `exit(0)` —— 这是有意为之的"硬失败"设计，让守护进程重拉。
- `data_call.reconnect = true` 等价于原来的 `AUTO_REDIAL` 标志，让 SDK 在断线时自动重试。
- 两个 `if (is_func_called)` 块分开写而非合并到一个块内，是为了允许未来将 Init 和 Start 解耦（当前逻辑等价于一个块）。

---

### 3.6 `bool dail_stop_data_call(int profile_idx)`

**分类**：核心逻辑  
**功能**：停止指定 profile 的 IPv4 数据呼叫。  
**关键逻辑**：`QL_Data_Call_Stop(profile_idx, IPv4, &err)`，成功返回 true。  
**调用方**：L1 恢复（stop + start 软重拨）、`wait_for_connect` 超时清理、`reg_timeout_handler` 中清理悬挂 SIM 数据呼叫。

---

### 3.7 `int parse_oper_info_str(char *p_oper_info, int *p_stat, char *p_lg_oper, char *p_sh_oper, char *p_nu_oper, int *p_act)`

**分类**：工具函数  
**功能**：解析单条运营商信息字符串（逗号分隔的 5 字段）。  
**输入格式**：`"2,\"CHN-UNICOM\",\"UNICOM\",\"46001\",2"`  
**关键逻辑**：逐字符找逗号，按 `select_idx` 分配到 stat/长名/短名/数字编号/接入技术。  
**返回值**：成功解析的字段数（期望 4，含 4 个逗号）。  
**注意**：未做缓冲区大小检查，`memcpy(p_lg_oper, temp_msg, strlen(temp_msg))` 依赖调用方保证 `OPER_NAME_LENGTH` 足够；`p_nu_oper` 解析到第 3 个字段（0-indexed），即 MCC-MNC 数字编号。

---

### 3.8 `bool parse_oper_list_info(CC_Deque *p_oper_deque, char *p_oper_list_str)`

**分类**：工具函数  
**功能**：解析 `AT+COPS=?` 完整响应，将 `stat==1`（可用）的运营商数字编号入队。  
**输入格式**：`+COPS: (2,"CHN-UNICOM","UNICOM","46001",2),(1,"CHINA MOBILE","CMCC","46000",0),,(0-4),(0-2)`  
**关键逻辑**：
- `strstr("+COPS:")` 定位起始位置
- 逐一提取 `(...)` 括号内容，调用 `parse_oper_info_str`
- `scanf_ret == 4 && stat == 1` 才入队（过滤不可用运营商和空括号）
- 入队元素为 `calloc` 分配的数字编号字符串，由 `select_oper` 中 `free`

**副作用**：动态分配内存，调用方（`cc_deque_free_all`）负责释放。

---

### 3.9 `void cc_deque_free_all(CC_Deque *deque)`

**分类**：工具函数  
**功能**：释放队列中所有元素及队列结构本身。  
**关键逻辑**：循环 `cc_deque_remove_first` + `free(element)`；最后 `free(deque)`。  
**注意**：`deque = NULL` 只修改局部变量，不影响调用方的指针（C 值传递），是典型"无效置 NULL"写法，无实际危害但也无效果。

---

### 3.10 `bool test_can_ping_google()`

**分类**：工具函数  
**功能**：`ping -c 1 8.8.8.8` 检测网络连通性。  
**关键逻辑**：`system()` 调用，返回值 0 = ping 通，非 0 = 失败。  
**注意**：
- `system()` 会 fork+exec，每次调用开销不可忽略
- 在心跳（每 30s）和 `net_connected` 监控（每次循环）中都会调用，心跳用于故障计时，`net_connected` 的 rx_packets 才是真实断网检测主力
- 目标 `8.8.8.8` 硬编码，在 DNS 不通但路由通的场景仍有效（ping IP）

---

### 3.11 `double get_uptime_seconds()`

**分类**：工具函数  
**功能**：读 `/proc/uptime` 获取系统开机时长（秒）。  
**关键逻辑**：`fopen + fscanf("%lf")` 读取第一个浮点数（第二个是 idle time，忽略）。  
**调用方**：`reset_modem()` 中判断开机时间是否 > 2000s。

---

### 3.12 `void reset_modem()`

**分类**：模组复位（**当前版本为空操作**）  
**功能**：意图触发整机重启，但核心 `system("reboot -f")` 已注释，实际只打印日志。  
**保护逻辑**（即使有效也受此约束）：
1. 开机时间 < 2000s → 跳过（避免启动初期反复重启）
2. 距上次复位 < 300s → 跳过（5 分钟冷却）
**实际影响**：`modem_need_reset=true` 在多个状态设置后，每次循环检查此值并调 `reset_modem()`，但因函数体无实际操作，"重置"退化为状态机的自然重试。  
**风险**：若某状态持续失败（如 SDK 返回非零），主循环会每秒调用一次空操作，无法真正恢复。

---

### 3.13 `set_net_status / set_sim_present / set_signal_strength`

**分类**：状态更新工具  
**功能**：在 `status_mutex` 保护下更新三个自检状态字段。  
**注意**：`set_signal_strength` 在 `dial_task` 初始化时调用（置 -1），但**心跳循环中只读 `csq` 值用于日志，未调用 `set_signal_strength` 更新**，导致 `signal_strength` 字段一直为 -1（等待外部 nanomsg 查询时返回无效值）。

---

### 3.14 AT 命令诊断助手（11 个 static 函数）

这 11 个函数均为 `static`，仅供 `dial_task` 内部使用，模式高度一致：
`Ql_SendAT(fd, cmd, "OK", timeout_ms, rsp_buf)` → 字符串解析 → 写出参数。

| 函数 | AT 命令 | 超时(ms) | 解析方式 | 特殊说明 |
|---|---|---|---|---|
| `at_get_fw_version` | `AT+QGMR` | 2000 | 跳过 echo 行和 OK 行，取 4+ 字符非关键字行 | 固件版本如 `EG25GGBR07A08M2G` |
| `at_get_fw_sub` | `AT+CSUB` | 2000 | `strstr("SubEdition:")` 后截取到行末 | 子版本如 `V06` |
| `at_get_model` | `AT+CGMM` | 2000 | 同 fw_version 模式 | 型号如 `EG25-G` |
| `at_get_cfun` | `AT+CFUN?` | 2000 | `strstr("+CFUN:")` 后 `atoi()` | 返回 int，-1 表示失败 |
| `at_get_cgdcont` | `AT+CGDCONT?` | 3000 | 遍历所有 `+CGDCONT:` 行，跳过含 `,,` 的空 APN 条目，用 `\|` 拼接 | 最大 512 字节响应 |
| `at_get_temp` | `AT+QTEMP` | 2000 | 取 `+QTEMP:` 行到行末 | 完整原始行，调用方用 `strstr(":")` 取值部分 |
| `at_get_serving_cell` | `AT+QENG="servingcell"` | 1500 | 取 `+QENG:` 行到行末 | 完整 LTE 小区快照 |
| `at_parse_cell_id` | （解析函数，无 AT）| — | 逗号计数到第 6 个，取下一字段 | Cell ID 在 LTE 格式中是第 7 个逗号分隔字段（0-indexed）|
| `at_parse_rsrp_rsrq` | （解析函数，无 AT）| — | 逗号计数到第 13 个，`sscanf("%d,%d")` | LTE 格式第 13/14 字段为 RSRP/RSRQ |
| `at_get_cgpaddr` | `AT+CGPADDR` | 2000 | 遍历所有 `+CGPADDR:` 行，跳过 IP 为空或 `0.0.0.0` 的条目 | 跳过未激活 profile |
| `at_get_ceer` | `AT+CEER` | 2000 | `strstr("+CEER:")` 行到行末 | 最近呼叫/PDP 错误原因 |
| `at_get_cgact` | `AT+CGACT?` | 2000 | 遍历所有 `+CGACT:` 行，`\|` 拼接 | PDP 激活状态 |

**共性注意**：
- 所有函数均**不加锁**，调用方（心跳块）负责在 `g_at_port_mutex` 保护下调用（部分调用如诊断函数在心跳内，未见加锁，存在潜在竞态，见第 7 节）
- 超时设计偏短（多为 2000ms），适合周期性监控，但在信号差环境下可能频繁超时

---

### 3.15 `build_ext_diag_line`

**分类**：诊断输出  
**功能**：将 RSRP/RSRQ/Cell/IP/Temp/CEER/CGACT 格式化为一行诊断字符串，空字段自动跳过。  
**关键逻辑**：`APPEND` 宏在首字段后自动加 ` | ` 分隔符。  
**输出示例**：`RSRP:-95 RSRQ:-10 | Cell:1A2B3C | +CGPADDR: 1,"10.x.x.x" | +QTEMP:38 | +CEER: 33 | +CGACT: 1,0`

---

### 3.16 `dial_do_diag_snapshot`

**分类**：诊断输出  
**功能**：采集全套诊断信息（RSRP/RSRQ/IP/Temp/CEER/CGACT）并打印 `[DIAG]` 日志。  
**触发条件**（由调用方 `dial_task` 控制）：`!net_ok && start_fail_ts != 0 && !diag_snap_done`  
**副作用**：发送约 7 条 AT 命令（`serving_cell`、`cgpaddr`、`temp`、`ceer`、`cgact` 共 5 个 AT 查询），约 10-15ms 执行时间。  
**注意**：调用时**未持有 `g_at_port_mutex`**（心跳块外调用），若 AT 端口此时被其他线程使用，存在竞态。

---

### 3.17 `dial_log_heartbeat`

**分类**：诊断输出  
**功能**：输出 30s 基础心跳；每 5min 追加扩展字段（RSRP/RSRQ/Cell/IP）。

**基础心跳格式**：
```
[HEARTBEAT] CH:SIM | SIM:1 | REG:1 | CSQ:18 | Temp:38 | DownTime:0s | ConsecFail:0
[HEARTBEAT] CH:ROAMLINK | SIM:1 | CSQ:18 | Temp:38 | DownTime:0s | ConsecFail:0 | RL_FAIL:2 | RX_PKT:12345
```

**字段选择逻辑**：
- `REG`：仅 SIM 通道打印（Roamlink 时无意义）
- `RL_FAIL`：仅 `roamlink_available` 时打印
- `RX_PKT`：仅 `is_roamlink_active` 时打印

**扩展心跳**（5min 触发，追加到基础行）：
- 复用传入的 `cell_buf`（已在心跳块中由 `at_get_serving_cell` 读取，避免重复查询）
- 额外调用 `at_get_cgpaddr` 获取 IP

---

### 3.18 `void *dial_task(void *arg)`

**分类**：核心主线程  
**功能**：EG25 拨号主循环，无限运行（L3 触发时 `exit(1)` 退出）。  
详见第 4 节状态机深度解析。

---

### 3.19 `bool get_tz_after_connected(dial_mng_t *p_dial_mng)`

**分类**：工具函数  
**功能**：通过 `AT+QLTS=1` 查询网络时区并保存到系统，全局只执行一次。

**关键逻辑**：
```c
static bool has_run = false;  // 文件作用域静态变量
if (has_run) return true;     // 已执行过直接返回
pthread_mutex_lock(&g_at_port_mutex);
Ql_SendAT(smd_fd, "AT+QLTS=1", "OK", 1000, rsp_msg);
if (parse_tz_info(rsp_msg, &tz)) {
    save_tz_info(tz);
    has_run = true;
}
pthread_mutex_unlock(&g_at_port_mutex);
```

**调用位置**：
1. `dial_task` 主循环顶部（每次循环都调，`has_run` 保证只执行一次）
2. `wait_for_connect` 成功时
3. 启动时的预检 ping 成功子循环中

**注意**：
- `has_run` 是静态变量，**线程安全问题**：若多个线程同时调用（理论上不存在），存在竞态
- `AT+QLTS=1` 超时仅 1000ms，在网络刚连接时可能返回无效值（模组尚未同步网络时间）
- 一次成功后永远不再重试，若网络时区变化（漫游跨区）无法自动更新

---

### 3.20 `void ipv4_data_call_info_init(dial_mng_t *p_dial_mng)`

**分类**：网络信息初始化  
**功能**：连接成功后读取 IPv4 地址和网卡名，填充 `p_ipv4_data_call_info` 和 `p_ifname`。

**关键逻辑**：
1. 若 `p_ipv4_data_call_info` 非 NULL，先 `free` 旧值
2. `nw_get_connect_ipv4_data_call_info(profile_idx)` 获取 IPv4 数据呼叫信息
3. 若成功，`nw_get_ifaddrs(ip_str)` 返回 `strdup(ifa->ifa_name)`（`char *`），存入 `p_ifname`；重置 `u64_if_rx_packets=0`

> **v1.31.2 变更**：原字段为 `struct ifaddrs *p_ifaddrs`，`nw_get_ifaddrs()` 返回整个结构体拷贝——但 `ifaddrs` 内部有指针指向 `getifaddrs` 分配的内存，`freeifaddrs()` 后这些指针成悬空指针（use-after-free）。现改为仅 `strdup` 网卡名字符串，彻底规避此问题。

**调用位置**：`wait_for_connect` 成功时，仅在进入 `net_connected` 前调用一次（断线重连不重调）。

---

## 4. 状态机深度解析

### 4.1 完整状态转移表

| 当前状态 | 触发条件 | 目标状态 | 备注 |
|---|---|---|---|
| none | 首次循环 | init | 无条件 |
| init | `dial_init()` 成功 | sim_init | 打印固件诊断信息 |
| init | `dial_init()` 失败 | init | `modem_need_reset=true`，原地重试 |
| sim_init | `sim_op(SIM_OP_INIT)` 成功 | sim_check | 重置 `dial_timer` |
| sim_init | `sim_op(SIM_OP_INIT)` 失败 | sim_init | sleep(2) 后重试 |
| sim_check | `nw_get_sim_card_status()` 成功 | sim_op | `sim_present=1` |
| sim_check | 失败 | sim_check | `sim_present=0`，sleep(2) |
| sim_check | 失败持续 3600s | stop_cfun | `dial_timer` 超时 |
| sim_op | `sim_op_handler(get_iccid)` 成功，**随即显式调用 `sim_op_handler(get_imsi)`** | reg_check | 重置 `dial_timer`（注册超时起点）|
| sim_op | 失败 | sim_op | 原地重试 |
| reg_check | `nw_reg_status_check()` 成功 | cereg_check | — |
| reg_check | 失败 | reg_check | sleep(2) |
| reg_check | 失败持续 300s | reg_timeout_handler | goto 跳转 |
| cereg_check | `nw_at_get_cereg_stat()` 成功 | precondition_check | 读取初始 Cell ID |
| cereg_check | 失败 | cereg_check | sleep(10) |
| cereg_check | 失败持续 300s（共享 dial_timer）| reg_timeout_handler | goto 跳转 |
| precondition_check | `QL_Data_Call_Init_Precondition()` 成功 | pre_start_call | `profile_idx=-1` |
| precondition_check | 失败 | precondition_check | 原地重试 |
| pre_start_call | `profile_idx==-1` | pre_start_call | 第一拍：获取 APN 和 profile |
| pre_start_call | `profile_idx` 有效 | start_call | 第二拍：设置默认 profile |
| start_call | `dail_start_data_call()` 成功 | wait_for_connect | 记录 `dial_timer` |
| start_call | 失败 | start_call | 原地重试 |
| wait_for_connect | `nw_get_connect_state()` == 1 | net_connected | 读时区、读 IP、标记网络 up |
| wait_for_connect | 连接 120s 未成功 | list_oper / reg_check | `is_oper_select` 为 true → list_oper；否则 → reg_check |
| net_connected | `nw_get_connect_state()` ≤ 0 | reg_check | SDK 报断线，重置 `is_func_called` |
| net_connected | rx_packets 120s 无增长 | reg_check | 数据停滞检测；**同时重置 `dial_timer`**（v1.31.2 修复，确保注册有完整 300s 窗口）|
| list_oper | 队列有可用运营商 | select_oper | — |
| list_oper | 失败 2 次 / 队列空 | reg_check | — |
| select_oper | AT+COPS 选择成功 | stop_cfun | — |
| select_oper | 失败 2 次，移除当前 | list_oper / start_cfun | 队列非空 → list_oper；空 → start_cfun |
| stop_cfun | AT+CFUN=0 成功 | start_cfun | — |
| stop_cfun | 失败 2 次 | reg_check | — |
| start_cfun | AT+CFUN=1 成功 | sim_init | — |
| start_cfun | 失败 2 次 | reg_check | — |
| roamlink_starting | 心跳 ping 成功 | roamlink_active | 心跳驱动状态转移 |
| roamlink_starting | 超时（`ROAMLINK_CONNECT_TIMEOUT_SEC`），策略 1/2 | sim_init / init | `roamlink_stop_service()` |
| roamlink_starting | 超时，策略 3 | roamlink_starting | 重启服务，留在此状态 |
| roamlink_active | 持续 ping 成功 | roamlink_active | 更新 `roamlink_last_ping_ts` |
| roamlink_active | ping 超时（策略 1/2）| sim_init / init | `roamlink_stop_service()` |
| roamlink_active | ping 超时（策略 3）| roamlink_starting | 重启服务 |
| roamlink_active | 业务无数据超时（策略 1/2）| sim_init / init | `roamlink_stop_service()` |
| roamlink_active | 业务无数据超时（策略 3）| roamlink_starting | 重启服务 |

### 4.2 正常 SIM 拨号完整流程

```
进程启动
├── dial_mng_new() ← AT 初始化、Roamlink probe、策略确定
└── dial_task()
    │
    ├── [启动预检] test_can_ping_google() 成功？
    │   ├─ 是 → 进入轻量监控子循环（最多 10 次连续失败才跳出）
    │   └─ 否 → 继续
    │
    ├── [策略 PREFER/FORCE_ROAMLINK] → 直接启动 Roamlink，跳过 SIM 路径
    │
    └── [主循环 sleep(1)/次]
        │
        ├── cpactive_upt_atime()     ← 活跃保持
        ├── get_tz_after_connected() ← 尝试获取时区（has_run 保护）
        ├── [30s 心跳块]              ← 见第 5.5 节
        │
        └── [状态机]
            none → init
               QL_MCM_NW_Client_Init + sim_op(INIT)
               打印 Model/FW/SubEd/CFUN/Temp
            → sim_init
               sim_op(SIM_OP_INIT)
            → sim_check
               nw_get_sim_card_status()  [失败sleep(2)，超时1hr→stop_cfun]
            → sim_op
               sim_op_handler(get_iccid) → 读 ICCID
               sim_op_handler(get_imsi)  → 读 IMSI（显式调用）
               AT+CGSN → 读 IMEI
               写 nano_handler，打印 PDP 配置
            → reg_check
               nw_reg_status_check()  [失败sleep(2)，超时300s→reg_timeout_handler]
            → cereg_check
               nw_at_get_cereg_stat() [失败sleep(10)，超时300s→reg_timeout_handler]
               成功：读初始 Cell ID
            → precondition_check
               QL_Data_Call_Init_Precondition()
            → pre_start_call（两拍）
               第一拍：apn_get_apn_obj(iccid) → apn_scan_idx()
               第二拍：QL_Data_Call_Set_Default_Profile()
            → start_call
               QL_Data_Call_Init(callback) + QL_Data_Call_Start()
            → wait_for_connect
               轮询 nw_get_connect_state()  [超时120s]
               成功：get_tz / ipv4_info_init / nw_mark_network_status(1)
            → net_connected（持续运行）
               监控 nw_get_connect_state() + rx_packets 活跃性
```

### 4.3 reg_timeout_handler 路径

```
reg_check 或 cereg_check 超过 300s →
  nw_mark_network_status(0) + set_net_status(0)
  │
  ├── [策略 1/2 + roamlink_available + !license_pending]
  │   → dail_stop_data_call（清理悬挂呼叫）
  │   → p_apn_obj = NULL（重置 APN）
  │   → roamlink_start_master() + roamlink_start_service()
  │   → dial_st = roamlink_starting + is_roamlink_active = true
  │
  └── [策略 4 / 无 Roamlink / license_pending]
      → is_func_called = true
      → dial_st = stop_cfun（RF 重置后重走 sim_init）
```

### 4.4 wait_for_connect 超时路径

```
连接等待 120s 失败 →
  dail_stop_data_call(profile_idx)
  is_func_called = true
  重置 dial_timer

  if (p_apn_obj->is_oper_select)
      → list_oper（尝试手动选网）
  else
      → reg_check（重走注册流程）
```

---

## 5. 核心机制深度分析

### 5.1 定时器体系

模块使用两套计时器：

#### `struct timespec` + `CLOCK_MONOTONIC`（`diff()` 计算差值）

| 变量 | 初始值 | 重置时机 | 用于哪个超时 |
|---|---|---|---|
| `p_dial_mng->dial_timer` | `sim_init` 成功时 | sim_op 成功（注册超时起点）；断线重连；Roamlink 切回 SIM；`wait_for_connect` 超时 | SIM 检测 3600s；注册 300s；拨号等待 120s；rx_packets 活跃性 120s |
| `roamlink_start_ts` | `roamlink_start_service()` 成功时 | 切换到 SIM 时全零清除 | Roamlink starting 超时 |
| `roamlink_last_ping_ts` | 心跳 ping 成功时更新 | 切换到 SIM 时全零清除 | Roamlink active ping 超时 |
| `roamlink_no_data_timer` | 进入 active 时；rx_packets 有增长时 | 切换时全零清除 | 业务层无数据超时 |
| `license_wait_start` | 进入 license_pending 时 | 不重置 | license 等待总超时 300s |
| `license_check_timer` | 全零（首次立即检查）| 每次检查后更新为 cur_timer | license 轮询间隔 60s |
| `cur_timer` | 每次循环 `clock_gettime(CLOCK_MONOTONIC)` 更新 | — | 所有差值计算的"当前"端 |

#### `time_t` + `time(NULL)`（wall clock）

| 变量 | 初始值 | 重置时机 | 用于哪个超时 |
|---|---|---|---|
| `last_heartbeat` | `time(NULL)`（任务启动）| 每次心跳执行后 = now | 心跳 30s 间隔 |
| `next_ext_heartbeat` | 0（首次联网后才开始计时）| 每次扩展心跳后 = now + 300 | 扩展心跳 5min 间隔 |
| `start_fail_ts` | 0 | ping 连续失败 3 次时 = now；网络恢复时 = 0 | 断网时长计算；L1/L2/L3 阈值判断 |
| `last_l1_ts` | 0 | L1 执行后 = now；网络恢复时 = 0 | L1 节流 60s |
| `last_recovery_ts` | 0 | L2 执行后 = now；网络恢复时 = 0 | L2 节流 5min |
| `sim_fallback_ts` | 0 | 切到 SIM 备用时 = now；切回 Roamlink 成功时 = 0 | 策略 1 回切计时 |
| `roamlink_fallback_ts` | 0 | 切到 Roamlink 备用时 = now；切回 SIM 成功时 = 0 | 策略 2 回切计时 |
| `task_start_ts` | `time(NULL)`（任务启动）| 不重置 | uptime 计算（写入 dial_status）|

**关键设计**：`start_fail_ts` 的生命周期决定了 L1/L2/L3 的触发时机。它在 ping 连续失败 `PING_FAIL_THRESHOLD`(=3) 次后才启动，是整个故障计时的核心。

### 5.2 双卡切换完整机制

#### 四种策略行为对比

| 策略 | 初始通道 | SIM 断网 | Roamlink 故障 | 回切 |
|---|---|---|---|---|
| PREFER_SIM (0) | SIM | 超阈值切 Roamlink | 超时切 SIM | Roamlink 稳定 `ROAMLINK_FALLBACK_RETRY_SEC` → 回 SIM |
| PREFER_ROAMLINK (1) | Roamlink（启动时直接启动）| 超阈值切 Roamlink | 超时切 SIM | SIM 稳定 `SIM_FALLBACK_RETRY_SEC` → 回 Roamlink |
| FORCE_ROAMLINK (2) | Roamlink | 不适用（直接走 Roamlink 路径）| 重启服务，留在 starting | 无 |
| FORCE_SIM (3) | SIM | L1/L2/L3 恢复 | 不适用 | 无 |

#### probe 决策树

```
roamlink_probe()
├── PROBE_OK
│   → roamlink_available=true，使用 ini 策略
│
├── PROBE_NO_PACKAGE（RBMaster 二进制不存在）
│   → roamlink_available=false，永久 FORCE_SIM
│
├── PROBE_CONF_MISSING（/opt/conf.ini 不存在）
│   → roamlink_available=false，FORCE_SIM（等待 factoryApp 预置后重启）
│
├── PROBE_LICENSE_MISSING
│   ├── roamlink_license_restore_from_backup() 成功
│   │   └── re-probe
│   │       ├── OK → roamlink_available=true，正常使用
│   │       └── 失败 → roamlink_available=false，FORCE_SIM
│   └── 备份恢复失败
│       → license_pending=true，FORCE_SIM
│       → 记录 license_wait_start
│       [SIM 联网后]
│       → rbmaster_started=false → roamlink_start_master()（仅一次）
│       [主循环每秒检测]
│       → 每 60s 检测 license 文件是否出现
│           ├── 出现 → roamlink_license_backup_and_reboot()（不返回！）
│           └── 300s 超时 → license_pending=false，保持 FORCE_SIM
│
└── 其他未知结果
    → roamlink_available=false，FORCE_SIM
```

#### 运行时 SIM→Roamlink 切换步骤

触发条件（心跳块，每 30s 检查）：
```
!net_ok
&& start_fail_ts > 0
&& !is_roamlink_active
&& roamlink_available
&& (policy == PREFER_SIM || PREFER_ROAMLINK || FORCE_ROAMLINK)
&& downtime_sec >= ROAMLINK_SWITCH_TIMEOUT_SEC
```

执行步骤：
1. `LEDControl_blinkLight`（LED 闪烁，表示切换中）
2. `roamlink_start_master()` 启动 RBMaster 进程
3. `roamlink_start_service()` 发起切换请求
4. 成功：
   - `dial_st = roamlink_starting`
   - `is_roamlink_active = true`
   - `roamlink_start_ts = cur_timer`
   - `roamlink_last_ping_ts` 全零（等待首次 ping）
   - `roamlink_fallback_ts = now`（供策略 2 的回切计时）
   - `sim_fallback_ts = 0`
   - `start_fail_ts = 0`（清除故障计时，防止 L1/L2 误触发）
   - `ping_fail_count = 0`
5. 失败：`roamlink_fail_count++`，`start_fail_ts = now`（继续 SIM 故障计时）

#### 运行时 Roamlink→SIM 切换步骤（ping 超时）

触发条件（状态机 `roamlink_active` case）：
```
roamlink_last_ping_ts.tv_sec != 0
&& diff(roamlink_last_ping_ts, cur_timer).tv_sec > ROAMLINK_CONNECT_TIMEOUT_SEC
```

执行步骤（策略 1/2）：
1. `roamlink_fail_count++`
2. `roamlink_stop_service()`
3. `dial_st = sim_initialized ? dial_stat_sim_init : dial_stat_init`
4. `is_func_called = true`（重新走 Data Call Init）
5. `is_roamlink_active = false`
6. 清零 `roamlink_start_ts`、`roamlink_last_ping_ts`、`roamlink_rx_packets`、`roamlink_no_data_timer`
7. `sim_fallback_ts = time(NULL)`（供策略 2 的回切计时）
8. `roamlink_fallback_ts = 0`
9. `start_fail_ts = 0`，`ping_fail_count = 0`
10. 重置 `dial_timer`（注册超时起点）

### 5.3 L1/L2/L3 分级恢复机制

#### 触发条件（5 个 AND，缺一不可）

```c
!net_ok                              // 1. 当前网络不通
&& start_fail_ts > 0                 // 2. 故障计时器已启动（连续 3 次 ping 失败后）
&& has_connected_once                // 3. 本次运行曾经连通过（避免启动期干扰初始拨号）
&& dial_st != roamlink_starting      // 4. 不在 Roamlink 路径中（Roamlink 有自己的超时机制）
&& dial_st != roamlink_active        // 4. 同上
&& network_policy == NET_POLICY_FORCE_SIM  // 5. 强制 SIM 策略（其他策略走双卡切换）
```

#### 三级恢复执行详情

**L1（60s，软重拨）**：
```
触发：downtime_sec >= 60 && (!last_l1_ts || now - last_l1_ts >= 60)
例外：SIM 恢复（sim_error_active && sim_ok）→ 跳过 60s 等待，立即执行

执行前快照：at_get_ceer + at_get_cgact → [RECOVERY L1] LastErr/PDP 日志
检查注册：nw_reg_status_check()
  → REG down：跳过重拨（无意义），仅更新 last_l1_ts
  → REG ok：
      LEDControl_blinkLight（表示处理中）
      dail_stop_data_call(profile_idx)
      sleep(2)
      is_func_called = true
      dail_start_data_call()
      last_l1_ts = now
      sleep(3)
```

**L2（5min，RF 重置）**：
```
触发：downtime_sec >= 300 && (!last_recovery_ts || now - last_recovery_ts >= 300)
注：到达 L2 后不再退化为 L1（`else if` 结构保证互斥）

执行前快照：同 L1
LEDControl_blinkLight
g_at_port_mutex 加锁
AT+CFUN=0（超时 15s）→ 日志
解锁
sleep(2)
g_at_port_mutex 加锁
AT+CFUN=1（超时 15s）→ 日志
解锁
sleep(10)
is_func_called = true
dail_start_data_call()
last_recovery_ts = last_l1_ts = now
```

**L3（30min，进程退出）**：
```
触发：downtime_sec >= 1800

执行前快照：（包含在 recovery_level > 0 的判断块中）
[RECOVERY L3] FATAL 日志
sync()（刷新文件系统缓存）
g_at_port_mutex 加锁
AT+CFUN=1,1（超时 15s，模组重启）→ 日志
解锁
sleep(20)
[RECOVERY L3] Exiting 日志
log_close()
exit(1)  ← 进程退出，守护进程重拉
```

#### EG25 vs EC200A 阈值差异的技术原因

| 级别 | EG25 | EC200A | 原因 |
|---|---|---|---|
| L1 | 60s | 5min | EC200A 有 SDK 自动重连机制（每 25s 尝试，持续 5min），应用层等待 SDK 先行；EG25 无此机制，需更早介入 |
| L2 | 5min | 10min | 同上 |
| L3 | 30min | 35min | 基本对齐，EG25 略激进 |

### 5.4 license_pending 特殊模式完整流程

```
[启动] probe() = LICENSE_MISSING
    → 尝试 roamlink_license_restore_from_backup()
    → 失败
    → license_pending = true
    → network_policy = FORCE_SIM（临时强制 SIM）
    → 记录 license_wait_start

[运行期，主循环每次检查]
    if license_pending:
        计算 wait_elapsed = diff(license_wait_start, cur_timer).tv_sec
        
        if wait_elapsed > 300（LICENSE_WAIT_TIMEOUT_SEC）:
            license_pending = false（超时放弃，保持 FORCE_SIM）
        
        elif 首次 或 距上次检查 >= 60s:
            更新 license_check_timer
            if roamlink_license_appeared():
                roamlink_license_backup_and_reboot()  ← 不返回！执行 backup + reboot
            else:
                打印"license not yet available"日志

[SIM 联网后，wait_for_connect 成功时]
    if license_pending && !rbmaster_started:
        roamlink_start_master()  ← 启动 RBMaster 让其利用 SIM 网络下载 license
        rbmaster_started = true  ← 只启动一次

[SIM 后续断线重连]
    rbmaster_started = true → 不再重复启动 RBMaster
```

**关键约束**：
- license_pending 期间 `roamlink_available=false`，`reg_timeout_handler` 不走 Roamlink 路径
- license_pending 期间 L1/L2/L3 恢复正常工作（FORCE_SIM 下）
- `roamlink_license_backup_and_reboot()` 调用后直接 reboot，对调用栈无返回

### 5.5 心跳监控系统

#### 30s 心跳块执行序列

```
每 30s 执行（now - last_heartbeat >= 30）：

1. sim_ok    = nw_get_sim_card_status(h_nw_client)
   !sim_ok → sim_error_active = 1（标记 SIM 曾断开，L1 快速触发）

2. reg_ok    = nw_reg_status_check(h_nw_client)

3. csq       = nw_at_get_csq(smd_fd)

4. net_ok    = test_can_ping_google()  ← 最重要的判断
   ├─ true：
   │   LED 常亮
   │   has_connected_once = 1
   │   ping_fail_count = 0
   │   若 start_fail_ts 非零 → 记录断网时长，更新 outage 统计
   │   start_fail_ts = 0，last_l1_ts = 0，last_recovery_ts = 0，sim_error_active = 0
   │   [Roamlink 状态推进] starting→active（或更新 active 的 ping_ts）
   │   [双卡回切] 策略 1/2 的 SIM↔Roamlink 稳定回切
   │
   └─ false：
       ping_fail_count++
       if ping_fail_count >= 3 && start_fail_ts == 0:
           start_fail_ts = now（故障计时开始）
           LED 熄灭

5. downtime_sec = (start_fail_ts > 0) ? now - start_fail_ts : 0

6. 小区切换检测：
   at_get_serving_cell → at_parse_cell_id
   if cur_cell_id != last_cell_id → 打印 [CELL CHANGE] 日志，更新 last_cell_id

7. dial_log_heartbeat（输出 30s 心跳日志）

8. DIAG 快照：
   if !net_ok && start_fail_ts != 0 && !diag_snap_done:
       diag_snap_done = 1
       dial_do_diag_snapshot()

9. SIM→Roamlink 切换判断

10. L1/L2/L3 分级恢复判断（FORCE_SIM 策略）

11. dial_status_write(&dstat)（写入 /tmp/dial_status）
```

#### ping 抖动过滤器

`ping_fail_count`（函数作用域 int）：
- ping 通 → 立即清零
- ping 失败 → 累加
- 达到 `PING_FAIL_THRESHOLD`(=3) 且 `start_fail_ts==0` → 启动故障计时

意义：单次 ping 超时（网络抖动、临时拥塞）不会触发故障处理，必须连续 3 次失败才认定为真实断网。代价是故障确认延迟最多 3×30s=90s（每次 ping 在心跳块内执行，不影响主循环响应）。

#### DIAG 快照防重复机制

`diag_snap_done`（函数作用域 int）：
- 0 → 下次故障确认后触发快照
- 1 → 当前故障周期已采集，跳过
- 网络恢复时 → 置 0，下次故障重新触发

意义：避免每次心跳都发 5 条 AT 命令采集诊断信息，减少 AT 总线压力。

---

## 6. `/tmp/dial_status` 写入机制

每次 30s 心跳末尾执行 `dial_status_write(&dstat)`，字段说明：

| 字段 | 来源 | 说明 |
|---|---|---|
| `version` | `MODEM_MNG_VERSION_*` 宏 | "1.31.2" |
| `uptime_sec` | `now - task_start_ts` | 进程已运行秒数 |
| `state` | `dial_eg25_stat_name(dial_st)` | 状态名字符串 |
| `policy` / `policy_name` | `network_policy` | 数值 + 字符串 |
| `channel` | `is_roamlink_active` | "ROAMLINK" 或 "SIM" |
| `roamlink_available` | `roamlink_available` | 0/1 |
| `license_pending` / `license_wait_sec` | 对应字段 | license 等待状态 |
| `iccid` | `sim_mng->sim_iccid` | 若非空则写入 |
| `csq` | 心跳块读取的 csq | — |
| `profile_idx` | `profile_idx` | — |
| `apn` | `p_apn_obj->apn` | 若非空则写入 |
| `plmn` | 读 `/tmp/network_plmn` | 由其他模块写入此文件 |
| `net_status` | `net_ok ? 1 : 0` | 当次 ping 结果 |
| `net_type` | `is_roamlink_active` | NETWORK_TYPE_ROAMLINK / SIM |
| `ip` | `p_ipv4_data_call_info->v4.addr.ip` | 仅 SIM 通道且连接成功时 |
| `ifname` | `p_ifname`（`char *`，`strdup` 的网卡名）| 同上 |
| `tcp_fail_count` | `outage_count` | 本次运行断网次数 |
| `roamlink_fail_count` | `roamlink_fail_count` | Roamlink 切换失败次数 |
| `roamlink_rx_packets` | `roamlink_rx_packets` | Roamlink 业务层数据包数 |
| `roamlink_state` | `dial_st` | "none" / "starting" / "active" |
| `roamlink_connect_wait_sec` | `diff(roamlink_start_ts, cur_timer)` | 仅 starting 状态有值 |
| `biz_no_data_sec` | `downtime_sec` | 业务无数据秒数（近似）|
| `rbmaster_pid` | `dial_get_rbmaster_pid()` | RBMaster 进程 PID |
| `outage_count` / `last_outage_sec` / `total_outage_sec` / `current_outage_sec` | 对应字段 | 断网统计 |

**PLMN 字段特殊处理**：通过 `fopen("/tmp/network_plmn", "r")` + `fgets` 读取，剔除换行符。该文件由其他模块（nw 模块或 SIM 模块）写入，`dial.c` 只读取。

---

## 7. 互斥锁与并发安全分析

### 7.1 `g_at_port_mutex`（全局，`extern`）

**保护对象**：AT 串口 `smd_fd` 上的发送操作（`Ql_SendAT`）

**已加锁的调用位置**：
- `get_tz_after_connected()`：`AT+QLTS=1`
- `sim_op` case 中 `AT+CGSN`
- 心跳块 L2 恢复：`AT+CFUN=0/1`
- L3 恢复：`AT+CFUN=1,1`
- `stop_cfun` case：`AT+CFUN=0`
- `start_cfun` case：`AT+CFUN=1`
- `list_oper` case：`AT+COPS=?`
- `select_oper` case：`AT+COPS=1,2,<num>`

**未加锁的调用位置**（潜在竞态）：
- `dial_do_diag_snapshot()`：内部调用 5 个 AT 助手函数，均直接调 `Ql_SendAT` 无锁
- `dial_log_heartbeat()`：扩展心跳中调 `at_get_cgpaddr`（直接 `Ql_SendAT` 无锁）
- 启动诊断（`dial_stat_init` case）：`at_get_model/fw_version/fw_sub/cfun/temp`（无锁）

**风险评估**：若存在并发的 AT 操作线程（如 roamlink 模块通过 `smd_fd` 发 AT 命令），上述无锁调用会产生响应混乱。实际风险取决于 `roamlink` 模块是否共享同一 `smd_fd`（`roamlink_set_fd` 注入了同一 fd）。

### 7.2 `status_mutex`

**保护字段**：`net_is_up`、`sim_present`、`signal_strength`  
**写入方**：`dial_task` 主线程（`set_net_status`、`set_sim_present`、`set_signal_strength`）  
**读取方**：外部查询线程（nanomsg handler）  
**问题**：`signal_strength` 在初始化后（置 -1）未见持续更新，外部查询始终得到 -1。

### 7.3 `mutex + cond`

**当前状态**：在 `dial_task` 内初始化，`should_dial=true`，但未见任何 `pthread_mutex_lock/unlock` 或 `pthread_cond_wait/signal` 调用。这是预留的外部控制接口（未实现）。

---

## 8. 已知问题与风险点

### P1（高）`reset_modem()` 为空操作
`modem_need_reset=true` 在 `init`/`sim_init`/`sim_check`/`sim_op`/`precondition_check` 失败时设置，每次循环末尾调用 `reset_modem()`，但函数体内 `reboot` 已注释。**影响**：若 SDK 初始化卡死（如 AT 串口无响应），主循环会每秒调用一次空操作，无法真正恢复，只能依赖守护进程在 L3 触发 `exit(1)` 后重拉。

### P2（高）部分 AT 命令调用未加锁
`dial_do_diag_snapshot`、扩展心跳中的 `at_get_cgpaddr`、启动诊断的 5 个助手函数，均直接调用 `Ql_SendAT` 而不持有 `g_at_port_mutex`。若 Roamlink 模块通过同一 `smd_fd` 并发发 AT 命令，将产生响应字节流混乱。

### P3（中）`signal_strength` 字段不更新
`set_signal_strength(-1)` 在初始化时调用，但心跳循环虽然读取了 `csq` 值，却未调用 `set_signal_strength(csq)` 更新。外部通过 nanomsg 查询信号强度时始终得到 -1。

### P4（中）`dial_stat_stop_call` 状态未实现
枚举中定义了 `dial_stat_stop_call`，状态机 switch 中无对应 case，进入后走 `default: break`，实为死状态。当前代码路径中没有转移到此状态的逻辑，故无实际危害，但属于遗留死代码。

### P5（中）`get_tz_after_connected()` 使用静态变量且不在锁内
`static bool has_run` 在读写时未持有锁，若将来此函数被多线程调用（目前只有 dial_task 主线程，安全），存在 TOCTOU（time-of-check-time-of-use）竞态。更大的问题是 1000ms 超时可能在网络刚连接时拿不到有效时区，且后续无重试机制。

### P6（低）`AT+COPS=?` 期间主循环阻塞 180s
`list_oper` case 中 `Ql_SendAT(fd, "AT+COPS=?", "OK", 180000, ...)` 同步阻塞 180s。在此期间：
- 心跳不执行（30s 心跳、L1/L2/L3 判断均不触发）
- LED 状态不更新
- `/tmp/dial_status` 不刷新

### P7（低）`parse_oper_info_str` 无缓冲区大小检查
`memcpy(p_lg_oper, temp_msg, strlen(temp_msg))` 依赖调用方保证缓冲区足够（实际均为 `OPER_NAME_LENGTH=64`），`temp_msg` 来自 `strncpy(temp_msg, p_start, p_end-p_start)`，长度受限于 `OPER_NAME_LENGTH`，实际安全，但代码风格上存在隐患。

### ~~已修复~~ commit b1b5516（v1.31.2，2026-05-28）

以下问题在此提交中得到修复，不再作为开放风险：

| 原风险点 | 修复内容 |
|---|---|
| `p_ifaddrs` use-after-free（`nw.c`）| `nw_get_ifaddrs()` 改为返回 `strdup(ifa_name)`（`char *`），不再拷贝含内部指针的 `ifaddrs` 结构体；`dial.h` 中字段从 `struct ifaddrs *p_ifaddrs` 改为 `char *p_ifname` |
| `dial_stat_sim_op` IMSI 读取依赖 fall-through | `sim.c` 中 `sim_op_stat_get_iccid` case 补充 `break`；`dial.c` 中显式调用 `sim_op_handler(p_sim_mng, sim_op_stat_get_imsi)` |
| `net_connected` → `reg_check` 时未重置 `dial_timer` | `dial.c` 无数据超时路径补充 `p_dial_mng->dial_timer = cur_timer`，确保 `reg_check` 获得完整 300s 注册超时窗口 |
| `open(O_CREAT)` 缺权限参数（`nw.c`，POSIX UB）| `nw_mark_plmn`、`nw_at_get_csq`、`nw_mark_network_status` 三处补充 `0644` 权限参数 |
| `nw_reg_status_check` 在 LIMITED_SERVICE 下误判已注册 | 增加 `registration_state == E_QL_MCM_NW_SERVICE_FULL` 判断，排除紧急服务模式 |
| `select()` timeout 未在循环内重置 + `tv_usec` 单位错误（`at.c`）| 每次循环内重新赋值 `timeout.tv_sec/tv_usec`，并修复 `tv_usec = (ms % 1000) * 1000` 单位换算 |

---

### P8（低）`is_func_called` 双 if 块语义
```c
if (p_dial_mng->is_func_called) {
    QL_Data_Call_Init(...)  // Init 失败 → exit(0)
}
if (p_dial_mng->is_func_called) {  // 第二个 if
    QL_Data_Call_Start(...)
    成功 → is_func_called = false
}
```
第二个 `if (is_func_called)` 只在 Init 成功（否则 exit）的情况下才会执行，因此两个 if 块的效果等价于一个 if 块。注释说这是为了将来"将 Init 和 Start 解耦"，但当前逻辑无法实现这一点（Init 失败直接 exit，不会走到第二个 if）。

---

## 9. 设计总结

### 9.1 设计亮点

**1. 双层监控架构（状态机 + 心跳）正交设计**  
状态机负责"从零到连通"的顺序推进，心跳负责"连通后的持续维护"。两层独立工作，互不干扰。状态机专注功能，心跳专注监控，职责清晰。

**2. ping 抖动过滤器（PING_FAIL_THRESHOLD=3）**  
避免瞬时网络抖动触发级联恢复操作，减少不必要的 L1/L2 干扰，是经过实际场景验证的容错设计。

**3. Roamlink probe 决策在启动时一次性完成**  
`roamlink_available` 在 `dial_mng_new()` 中确定后运行期不变，避免了运行期重复 probe 引入的竞态和不确定性。

**4. `is_func_called` 防止 `QL_Data_Call_Init` 重复调用**  
SDK `QL_Data_Call_Init` 不能重复调用，此标志以最简洁的方式解决了这个约束，且在所有需要重新拨号的场景中（L1/L2/断线重连）正确置 true。

**5. DIAG 快照 + 小区切换日志**  
在故障发生第一时间采集 RSRP/RSRQ/IP/Temp/CEER/CGACT，结合小区变化记录，为事后分析提供了完整的网络状态快照，大幅降低现场排查难度。

### 9.2 改进建议

| 优先级 | 问题 | 建议 |
|---|---|---|
| P1 | `reset_modem()` 空操作 | 明确决策：彻底删除 modem_need_reset 逻辑，或实现真正的 CFUN 重置替代 reboot |
| P2 | AT 调用未加锁 | 将 `dial_do_diag_snapshot()`、扩展心跳 AT 查询、启动诊断均用 `g_at_port_mutex` 保护 |
| P3 | `signal_strength` 不更新 | 心跳中读取 csq 后调用 `set_signal_strength(csq)` |
| P5 | 时区查询可靠性 | 增加重试：首次失败（`!has_run`）时在下一个心跳周期内再试，而非永久放弃 |
| P6 | `AT+COPS=?` 阻塞 | 考虑在独立线程中执行，或将 180s 超时分拆为多次短超时轮询 |
| — | `should_dial`/mutex/cond 未使用 | 要么实现外部控制逻辑，要么清理未使用的字段，避免误导阅读者 |

---

<!-- GENERATION_COMPLETE: 2026-05-26 -->
<!-- LAST_UPDATED: 2026-05-28 commit b1b5516 v1.31.2 -->
