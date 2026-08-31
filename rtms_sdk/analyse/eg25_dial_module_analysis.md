# `eg25/dial` 模块源码深度分析

> 版本：modem_mng v1.31.0  
> 文件：`dial.h` / `dial.c`（共 2046 行）  
> 分析日期：2026-05-26

---

## 一、模块定位

`eg25/dial` 是 EG25 平台的**拨号主控模块**，承担以下全部职责：

1. 驱动从"无状态"到"网络已连接"的完整状态机
2. 网络连通性持续监控（心跳 / DIAG 快照 / 小区切换检测）
3. 双卡切换（物理 SIM ↔ Roamlink 虚拟 SIM），按策略自动倒换
4. L1/L2/L3 分级恢复（仅强制物理 SIM 策略下）
5. 启动诊断（固件版本 / 温度 / CFUN / PDP 配置）
6. 结构化状态写入 `/tmp/dial_status`，供外部进程（nanomsg handler）查询

---

## 二、文件结构总览

```
eg25/dial/
├── dial.h   —— 常量、枚举、dial_mng_t 结构、公开函数声明
└── dial.c   —— 所有实现（工具函数 + AT 助手 + 状态机 + 心跳）
```

依赖的内部模块（`#include`）：

| 头文件 | 作用 |
|---|---|
| `at.h` | AT 串口操作，`Ql_SendAT()`，`at_init()` |
| `nw.h` | 网络状态查询（SIM/注册/CSQ/rmnet rx_packets）|
| `sim.h` | SIM 操作（`sim_op()`、`sim_op_handler()`）|
| `apn.h` | APN 查询、profile 扫描 |
| `tz.h` | 时区解析与保存 |
| `roamlink.h` | Roamlink 双卡切换（probe/start/stop/policy）|
| `dial_status.h` | `/tmp/dial_status` 结构化写入 |
| `nanomsg_process_cinterface.h` | 向 nanomsg handler 写入 ICCID/IMSI |
| `cc_deque.h` | 运营商列表双端队列 |
| `led_control_c_api.h` | LED 状态指示 |
| `logger.h` / `logger_sd.h` | 日志输出（内存 + SD 卡）|

---

## 三、常量与阈值

```c
DIAL_TIMEOUT_SECONDS       = 120    // 拨号等待连接超时（秒）
SIM_CHECK_TIMEOUT_SECONDS  = 3600   // SIM 检测失败，1 小时后触发 cfun 重置
REG_CHECK_TIMEOUT_SECONDS  = 300    // 注册超时（reg_check + cereg_check 共享），触发 Roamlink 切换或 cfun 重置
PING_FAIL_THRESHOLD        = 3      // 连续 ping 失败 N 次才认定为真实断网（抖动过滤）
```

Roamlink 相关阈值定义在 `roamlink.h`（本模块直接引用）：

| 常量 | 含义 |
|---|---|
| `ROAMLINK_SWITCH_TIMEOUT_SEC` | SIM 断网持续多久后切 Roamlink（心跳层判断） |
| `ROAMLINK_CONNECT_TIMEOUT_SEC` | Roamlink starting/active 无 ping 多久算超时 |
| `ROAMLINK_NO_DATA_TIMEOUT_SEC` | Roamlink active 业务层 rx_packets 无增长多久算超时 |
| `SIM_FALLBACK_RETRY_SEC` | PREFER_ROAMLINK 策略，SIM 备用稳定多久后尝试回切 Roamlink |
| `ROAMLINK_FALLBACK_RETRY_SEC` | PREFER_SIM 策略，Roamlink 备用稳定多久后尝试回切 SIM |
| `LICENSE_WAIT_TIMEOUT_SEC` | license_pending 模式最长等待时间（300s） |
| `LICENSE_CHECK_INTERVAL_SEC` | license_pending 轮询间隔（60s） |

---

## 四、数据结构

### 4.1 状态枚举 `dial_stat_enu`

```
dial_stat_none
dial_stat_init                — SDK NW Client 初始化
dial_stat_sim_init            — SIM 操作初始化
dial_stat_sim_check           — 检测 SIM 卡是否在位
dial_stat_sim_op              — 读取 ICCID/IMSI
dial_stat_reg_check           — AT+CREG 注册检测
dial_stat_cereg_check         — AT+CEREG 注册检测（4G 专属）
dial_stat_precondition_check  — QL_Data_Call_Init_Precondition
dial_stat_pre_start_call      — 获取 APN → 扫描 profile_idx
dial_stat_start_call          — 启动数据呼叫
dial_stat_stop_call           — （保留，未使用）
dial_stat_stop_cfun           — AT+CFUN=0（关闭 RF）
dial_stat_start_cfun          — AT+CFUN=1（开启 RF）
dial_stat_list_oper           — AT+COPS=? 获取可用运营商列表
dial_stat_select_oper         — AT+COPS=1,2,<num> 选择运营商
dial_stat_wait_for_connect    — 等待连接成功回调
dial_stat_net_connected       — 网络已连接，持续监控 rx_packets
dial_stat_roamlink_starting   — Roamlink 服务已启动，等待首次 ping 通
dial_stat_roamlink_active     — Roamlink 通道稳定运行
```

**重要**：已注释掉的旧版本枚举（文件头部）不包含 `cereg_check` 和两个 Roamlink 状态，是当前版本的演进记录。

### 4.2 核心结构 `dial_mng_t`（精简版在 `dial_mng_t_00`）

```c
// —— 基础字段（继承自 dial_mng_t_00）——
dial_stat_enu  dial_st;               // 当前状态机状态
nw_client_handle_type h_nw_client;   // NW Client 句柄（NW 模块）
ql_data_call_info_s  *p_ipv4_data_call_info; // IPv4 连接信息（连接成功后填入）
struct ifaddrs       *p_ifaddrs;      // 网卡地址信息
uint64_t             u64_if_rx_packets; // rmnet 接收包数基准（断网检测）
char                 nw_node_name[NAME_MAX]; // 网络节点名
int                  profile_idx;     // 当前激活的 PDP profile（由 APN 扫描确定）
sim_mng_t           *p_sim_mng;       // SIM 管理结构（ICCID/IMSI/状态）
struct timespec      dial_timer;      // 通用超时计时器（复用于多个状态）
int                  smd_fd;          // AT 串口文件描述符
CC_Deque            *deque_oper;      // 可用运营商队列（list_oper 填入，select_oper 消耗）

// —— 扩展字段（dial_mng_t 独有）——
TrafficMonitorWrapper *monitor;       // 流量监控（外部）
void                 *cpa;            // cpactive（活跃保持，外部）
void                 *nano_handler;   // NanoReqHandlerWrapper（写 ICCID/IMSI）
bool                 should_dial;     // 外部控制拨号开关（目前启动即为 true）
pthread_mutex_t      mutex;           // 拨号控制互斥锁
pthread_cond_t       cond;            // 拨号控制条件变量
apn_obj_t           *p_apn_obj;       // APN 对象（pre_start_call 阶段查询填入）
bool                 is_func_called;  // true = 下次 dail_start_data_call 须重走 Init+Start

// —— 自检状态（status_mutex 保护）——
pthread_mutex_t      status_mutex;
int                  net_is_up;       // 0/1
int                  sim_present;     // 0/1
int                  signal_strength; // CSQ 或 -1（未知）

// —— Roamlink 双卡切换 ——
int             network_policy;       // NET_POLICY_PREFER_SIM/ROAMLINK/FORCE_*
bool            roamlink_available;   // probe() 结果（启动时确定，运行时不变）
bool            is_roamlink_active;   // 当前通道是否为 Roamlink
struct timespec roamlink_start_ts;    // 发出 start 的时刻（用于 starting 超时）
struct timespec roamlink_last_ping_ts; // 最近一次 ping 成功时刻（用于 active 超时）
uint64_t        roamlink_rx_packets;  // 业务层 rx_packets 基准
struct timespec roamlink_no_data_timer; // 上次 rx_packets 增长时刻
bool            sim_initialized;      // QL_MCM_NW_Client_Init 是否已执行（切换时避免二次 init）

// —— License 下载等待 ——
bool            license_pending;      // 等待 RBMaster 下载 license 中
struct timespec license_wait_start;   // 开始等待的时刻
struct timespec license_check_timer;  // 上次检查 license 文件时刻

bool            rbmaster_started;     // license_pending 下 RBMaster 只启动一次

// —— 断网统计（写入 /tmp/dial_status）——
int   outage_count;          // 本次运行断网次数
long  last_outage_sec;       // 上次断网持续秒数
long  total_outage_sec;      // 累计断网秒数
int   roamlink_fail_count;   // Roamlink → SIM 切换次数
```

---

## 五、函数清单

### 5.1 初始化与管理

| 函数 | 说明 |
|---|---|
| `dial_mng_new()` | 分配 dial_mng_t，初始化 AT 端口，probe Roamlink，设置网络策略 |
| `dial_init()` | 调用 `QL_MCM_NW_Client_Init` + `sim_op(SIM_OP_INIT)`；失败返回 false |
| `ipv4_data_call_info_init()` | 连接成功后读取 IPv4 地址和网卡信息 |
| `get_tz_after_connected()` | 通过 `AT+QLTS=1` 查询时区并保存；全局标志确保只执行一次 |

### 5.2 数据呼叫

| 函数 | 说明 |
|---|---|
| `dail_start_data_call()` | 按 `is_func_called` 决定是否调用 `QL_Data_Call_Init`；成功后置 false |
| `dail_stop_data_call()` | 调用 `QL_Data_Call_Stop`；供 L1 恢复和 roamlink 切换清理用 |
| `dial_data_call_state_callback()` | SDK 异步回调：连接成功/断开时调用 `nw_mark_network_status()` 并打日志 |

### 5.3 AT 命令诊断助手（`static`，仅 `dial_task` 内部）

| 函数 | AT 命令 | 用途 |
|---|---|---|
| `at_get_fw_version()` | `AT+QGMR` | 固件版本，如 `EG25GGBR07A08M2G` |
| `at_get_fw_sub()` | `AT+CSUB` | 子版本，提取 `SubEdition:V06` 中的 `V06` |
| `at_get_model()` | `AT+CGMM` | 模组型号，如 `EG25-G` |
| `at_get_cfun()` | `AT+CFUN?` | RF 功能状态（返回整数，-1 失败）|
| `at_get_cgdcont()` | `AT+CGDCONT?` | PDP 上下文配置，跳过空 APN 条目 |
| `at_get_temp()` | `AT+QTEMP` | 模组温度 |
| `at_get_serving_cell()` | `AT+QENG="servingcell"` | 完整服务小区快照（原始 +QENG 行）|
| `at_parse_cell_id()` | （解析函数）| 从 QENG 行提取 Cell ID（逗号第 7 个字段）|
| `at_parse_rsrp_rsrq()` | （解析函数）| 从 QENG 行提取 RSRP/RSRQ（逗号第 13/14 字段）|
| `at_get_cgpaddr()` | `AT+CGPADDR` | PDP 已分配 IP（跳过未激活/0.0.0.0）|
| `at_get_ceer()` | `AT+CEER` | 最近呼叫/PDP 错误原因 |
| `at_get_cgact()` | `AT+CGACT?` | PDP 激活状态 |

### 5.4 诊断输出

| 函数 | 说明 |
|---|---|
| `build_ext_diag_line()` | 将 RSRP/RSRQ/Cell/IP/Temp/CEER/CGACT 格式化为一行，空字段跳过 |
| `dial_do_diag_snapshot()` | 采集全套诊断信息并打印 `[DIAG]` 日志（每次断网首次触发） |
| `dial_log_heartbeat()` | 30s 心跳日志；每 5min 追加 RSRP/RSRQ/Cell/IP 扩展字段 |

### 5.5 运营商操作

| 函数 | 说明 |
|---|---|
| `parse_oper_info_str()` | 解析单条运营商信息字符串（`,` 分隔，stat/长名/短名/MCC-MNC/接入技术）|
| `parse_oper_list_info()` | 解析 `AT+COPS=?` 完整响应，将可用运营商（stat==1）入队 |
| `cc_deque_free_all()` | 释放运营商队列所有元素及队列本身 |

### 5.6 模组复位

```c
void reset_modem()
```
- 保护条件：系统开机时间 > 2000s，且距上次复位 > 300s
- 实际动作：**打印日志后直接返回**（`system("reboot -f")` 已注释），是一个安全占位符
- `modem_need_reset` 在多个状态失败时置 true，但当前版本实际不执行重启

---

## 六、主循环 `dial_task()` 架构

`dial_task()` 是一个无限 `while(true)` 循环，**每秒执行一次**（`sleep(1)` 置于循环末尾）。内部分为两层并行逻辑：

```
每次循环 (1s)
├── [Layer 1] 每 30s 心跳块（time-based，wall clock）
│   ├── SIM / REG / CSQ 状态采集
│   ├── Ping 测试（抖动过滤：连续 3 次失败才计故障）
│   ├── 故障计时器 start_fail_ts 管理
│   ├── LED 状态更新
│   ├── Roamlink 状态推进（starting→active）
│   ├── 双卡回切逻辑（策略 1/2）
│   ├── 小区切换检测（CELL CHANGE 日志）
│   ├── 心跳日志（dial_log_heartbeat）
│   ├── DIAG 快照（首次断网触发一次）
│   ├── SIM→Roamlink 切换（策略 1/2/3，downtime 超阈值）
│   ├── L1/L2/L3 分级恢复（仅策略 4 FORCE_SIM）
│   └── /tmp/dial_status 更新
│
├── [Layer 2] 状态机 switch (p_dial_mng->dial_st)
│   └── 见第七节状态机详解
│
└── goto dial_loop_continue / reg_timeout_handler
```

**启动时额外逻辑**（循环前）：
1. 若 `test_can_ping_google()` 成功 → 进入轻量监控子循环（网络已通，最多等待 10 次连续 ping 失败后才进入主状态机）
2. 若策略为 PREFER/FORCE_ROAMLINK → 直接 `roamlink_start_service()`，状态设为 `roamlink_starting`，跳过 SIM 路径

---

## 七、状态机详解

### 7.1 正常拨号路径

```
none
 └→ init
      ├─[失败]→ modem_need_reset=true，原地重试
      └─[成功]→ 打印 Model/FW/SubEd/CFUN/Temp
               └→ sim_init
                    ├─[失败]→ modem_need_reset=true，sleep(2) 重试
                    └─[成功]→ sim_check
                               ├─[失败]→ modem_need_reset，sleep(2) 重试
                               ├─[超时 1hr]→ stop_cfun
                               └─[成功]→ sim_op
                                          ├─[失败]→ modem_need_reset，原地重试
                                          └─[成功]→ 打印 ICCID/IMSI/IMEI/PDP
                                                    写入 nano_handler
                                                    └→ reg_check
                                                         ├─[失败]→ sleep(2) 重试
                                                         ├─[超时 300s]→ reg_timeout_handler
                                                         └─[成功]→ cereg_check
                                                                    ├─[失败]→ sleep(10) 重试
                                                                    ├─[超时 300s]→ reg_timeout_handler
                                                                    └─[成功]→ 打印 CELL Init
                                                                              └→ precondition_check
                                                                                  └→ pre_start_call
                                                                                      └→ start_call
                                                                                          └→ wait_for_connect
                                                                                              ├─[超时 120s]→ reg_check 或 list_oper
                                                                                              └─[成功]→ net_connected（正常运行）
```

### 7.2 状态详细说明

#### `dial_stat_net_connected`

持续监控两件事：
1. `nw_get_connect_state(profile_idx)` ≤ 0 → 断线，重置 `is_func_called=true`，转 `reg_check`
2. `nw_get_rmnet_rx_packets_sum()` 若 120s 无增长 → 认定数据停滞，强制 `reg_check`

说明：检测维度是**数据包计数**（非 ping），与心跳层的 ping 检测是两套独立机制。

#### `dial_stat_list_oper`

- 仅当 `p_apn_obj->is_oper_select` 为 true 时才会进入（某些运营商需要手动选择）
- 发送 `AT+COPS=?`，超时高达 **180s**（扫描需要时间）
- 将 `stat==1`（可用）的运营商按数字编号入队

#### `dial_stat_select_oper`

- 取队列头，发 `AT+COPS=1,2,<num>` 选择运营商
- 成功后转 `stop_cfun`（做一次 RF 循环）
- 失败重试 2 次，超过则从队列移除，尝试下一个；队列空了转 `start_cfun`

#### `dial_stat_stop_cfun` / `dial_stat_start_cfun`

- 流程：`AT+CFUN=0` → 5s 等待 → `AT+CFUN=1` → 5s 等待 → `sim_init`
- `stop_cfun` 失败 2 次后直接转 `reg_check`（不强行卡死）
- 每次发送均使用 `g_at_port_mutex` 互斥保护

#### `dial_stat_roamlink_starting`

只做一件事：**超时检测**（正向推进由心跳 ping 成功触发）
- 超时 `ROAMLINK_CONNECT_TIMEOUT_SEC`：
  - 策略 1/2：`roamlink_stop_service()` → 转 SIM 路径（`sim_init` 或 `init`，取决于 `sim_initialized`）
  - 策略 3（FORCE_ROAMLINK）：重启服务，留在 `starting`，不降级

#### `dial_stat_roamlink_active`

监控两个超时：
1. **Ping 层**：`roamlink_last_ping_ts` 距今超过 `ROAMLINK_CONNECT_TIMEOUT_SEC`
2. **业务层**：`roamlink_no_data_timer` 距今超过 `ROAMLINK_NO_DATA_TIMEOUT_SEC`（rmnet rx_packets 无增长）

任一超时 → 策略 1/2 切 SIM，策略 3 重启服务回到 `starting`

### 7.3 `reg_timeout_handler`（goto 跳转目标）

注册超时（reg/cereg 超过 300s）后执行：

```
条件分支：
  策略 1/2 + roamlink_available + !license_pending
      → 切 Roamlink（start_master + start_service → roamlink_starting）

  否则（策略 4 / 无 Roamlink / license_pending）
      → is_func_called=true + 转 stop_cfun（RF 重置后重走 sim_init）
```

---

## 八、双卡切换（Roamlink）机制

### 8.1 四种网络策略

| 策略常量 | 值 | 初始通道 | 失败行为 | 回切 |
|---|---|---|---|---|
| `NET_POLICY_PREFER_SIM` | 0 | SIM | 断网超阈值切 Roamlink | 稳定 `ROAMLINK_FALLBACK_RETRY_SEC` 后回 SIM |
| `NET_POLICY_PREFER_ROAMLINK` | 1 | Roamlink | 连接超时切 SIM | 稳定 `SIM_FALLBACK_RETRY_SEC` 后回 Roamlink |
| `NET_POLICY_FORCE_ROAMLINK` | 2 | Roamlink | 超时重启服务，**不切 SIM** | — |
| `NET_POLICY_FORCE_SIM` | 3 | SIM | L1/L2/L3 分级恢复，**不切 Roamlink** | — |

策略来源：`roamlink_read_policy()` 读取 `/usrdata/network.ini`

### 8.2 启动时 `roamlink_probe()` 决策树

```
ROAMLINK_PROBE_OK
    → roamlink_available=true，使用 ini 文件策略

ROAMLINK_PROBE_NO_PACKAGE（RBMaster 不存在）
    → roamlink_available=false，强制 FORCE_SIM，永久

ROAMLINK_PROBE_CONF_MISSING（conf.ini 不存在）
    → roamlink_available=false，强制 FORCE_SIM

ROAMLINK_PROBE_LICENSE_MISSING
    ├─ 尝试 roamlink_license_restore_from_backup()
    │   ├─ 成功且 re-probe == OK → 正常使用
    │   └─ 失败 → FORCE_SIM（备份损坏或 conf.ini 消失）
    └─ 备份恢复失败
        → license_pending=true，FORCE_SIM
        → SIM 联网后启动 RBMaster 下载 license
        → 60s 轮询 300s 总超时，若 license 出现 → backup+reboot
        → 超时放弃，保持 FORCE_SIM
```

### 8.3 运行时切换触发点（心跳层，每 30s）

**SIM → Roamlink**（策略 1/2/3，`!is_roamlink_active`）：
```
!net_ok && start_fail_ts > 0 && downtime_sec >= ROAMLINK_SWITCH_TIMEOUT_SEC
```

**Roamlink 回切 SIM**（策略 1：PREFER_ROAMLINK，在 SIM 备用时）：
```
dial_st == net_connected && sim_fallback_ts > 0
&& (now - sim_fallback_ts) >= SIM_FALLBACK_RETRY_SEC
```

**SIM 回切 Roamlink**（策略 2：PREFER_SIM，在 Roamlink 备用时）：
```
dial_st == roamlink_active && roamlink_fallback_ts > 0
&& (now - roamlink_fallback_ts) >= ROAMLINK_FALLBACK_RETRY_SEC
```

### 8.4 `sim_initialized` 字段的作用

Roamlink 切换回 SIM 时，若 `QL_MCM_NW_Client_Init` 已执行过（`sim_initialized=true`），
直接进入 `dial_stat_sim_init`，跳过重复 init，避免 SDK 报错。

---

## 九、L1/L2/L3 分级恢复

**触发条件**（全部满足）：
- `!net_ok && start_fail_ts > 0`
- `has_connected_once == 1`（首次成功后才生效，避免干扰初始拨号）
- `dial_st != roamlink_starting && dial_st != roamlink_active`
- `network_policy == NET_POLICY_FORCE_SIM`

| 级别 | 阈值 | 操作 | 节流 |
|---|---|---|---|
| L1 | 60s | `dail_stop_data_call` + `is_func_called=true` + `dail_start_data_call` | `last_l1_ts` 60s 间隔 |
| L2 | 5min | `AT+CFUN=0` + sleep(2) + `AT+CFUN=1` + sleep(10) + 重拨 | `last_recovery_ts` 5min 间隔；L2 阶段不再退化回 L1 |
| L3 | 30min | `AT+CFUN=1,1` + sleep(20) + `log_close(); exit(1)` | 无（执行后进程退出）|

**特殊路径**：SIM 卡恢复时（`sim_error_active && sim_ok`）跳过 60s 等待，立即执行 L1。

**EG25 vs EC200A 阈值差异**：EG25 更激进（60s/5min/30min vs EC200A 5min/10min/35min），原因是 EC200A 有 SDK 自动重连（L0 期，每 25s 自动尝试），EG25 没有此机制。

---

## 十、心跳监控系统

### 10.1 触发频率

- **基本心跳**：每 30s（wall clock，`now - last_heartbeat >= 30`）
- **扩展心跳**（RSRP/RSRQ/Cell/IP）：首次联网后，每 5min 追加到心跳行

### 10.2 心跳日志格式

```
[HEARTBEAT] CH:SIM | SIM:1 | REG:1 | CSQ:18 | Temp:38 | DownTime:0s | ConsecFail:0
[HEARTBEAT] CH:SIM | SIM:1 | REG:1 | CSQ:18 | Temp:38 | DownTime:0s | ConsecFail:0 | RSRP:-95 RSRQ:-10 | Cell:1A2B3C | +CGPADDR: 1,"10.x.x.x"
[HEARTBEAT] CH:ROAMLINK | SIM:1 | CSQ:18 | Temp:38 | DownTime:0s | ConsecFail:0 | RL_FAIL:0 | RX_PKT:12345
```

说明：
- `CH`: 当前通道（SIM / ROAMLINK）
- `REG`: Roamlink 通道时不打印（无诊断价值）
- `RL_FAIL`: 仅 roamlink_available 时打印
- `RX_PKT`: 仅 Roamlink 通道时打印

### 10.3 小区切换检测

每 30s 调用 `at_get_serving_cell()` + `at_parse_cell_id()`，与 `last_cell_id` 比较：
```
[CELL CHANGE] 1A2B3C -> 4D5E6F | +QENG: "servingcell","NOCONN","LTE",...
```

### 10.4 DIAG 快照

每次断网确认后（`start_fail_ts` 被设置后的下一个心跳）打印一次：
```
[DIAG] RSRP:-95 RSRQ:-10 | Cell:1A2B3C | +CGPADDR: 1,"10.x.x.x" | +QTEMP:38 | +CEER: 33 | +CGACT: 1,0
```

`diag_snap_done` 标志保证每次连续断网只打印一次，网络恢复后重置。

---

## 十一、`/tmp/dial_status` 结构

每 30s 通过 `dial_status_write(&dstat)` 刷新：

| 字段 | 内容 |
|---|---|
| `version` | "1.31.0" |
| `uptime_sec` | 进程已运行秒数 |
| `state` | 当前状态机状态名 |
| `policy` / `policy_name` | 网络策略数值和名称 |
| `channel` | "SIM" 或 "ROAMLINK" |
| `roamlink_available` | probe 结果 |
| `license_pending` / `license_wait_sec` | license 等待状态 |
| `iccid` / `csq` / `profile_idx` / `apn` | SIM 信息 |
| `plmn` | 从 `/tmp/network_plmn` 读取 |
| `net_status` / `net_type` / `ip` / `ifname` | 网络信息 |
| `tcp_fail_count` / `roamlink_fail_count` / `roamlink_rx_packets` | 计数器 |
| `roamlink_state` | "none" / "starting" / "active" |
| `roamlink_connect_wait_sec` | starting 状态已等待秒数 |
| `biz_no_data_sec` | 业务无数据秒数（用断网持续时长近似） |
| `rbmaster_pid` | RBMaster 进程 PID |
| `outage_count` / `last_outage_sec` / `total_outage_sec` / `current_outage_sec` | 断网统计 |

---

## 十二、互斥锁使用模式

```
g_at_port_mutex（外部全局）
    └─ 所有 Ql_SendAT 调用均加锁：
       get_tz_after_connected / 心跳内 L2 AT+CFUN / stop_cfun / start_cfun
       / list_oper AT+COPS=? / select_oper AT+COPS=1 / sim_op 内的 AT+CGSN

p_dial_mng->mutex + cond
    └─ 外部控制 should_dial（目前仅 dial_task 初始化，未见外部等待/通知逻辑）

p_dial_mng->status_mutex
    └─ 保护 net_is_up / sim_present / signal_strength（供 nanomsg handler 等外部读取）
```

---

## 十三、已知设计细节与注意事项

### 1. `is_func_called` 标志的语义
```
true  = 需要调用 QL_Data_Call_Init（首次或重置后）
false = 已调用过，可跳过 Init 直接 Start（其实当前分支结构中 Init 和 Start 捆绑在一起）
```
实际上 `is_func_called` 在 `dail_start_data_call()` 中同时控制 `QL_Data_Call_Init` 和 `QL_Data_Call_Start` 两个调用（两个 `if (p_dial_mng->is_func_called)` 块），成功后置 `false` 防止重复 Init。

### 2. `reset_modem()` 是空操作
`modem_need_reset` 在多个状态失败时被置 `true`，但 `reset_modem()` 函数体中 `system("reboot -f")` 已注释，实际不做任何事。失败的"重置"依靠上层状态机重试实现。

### 3. `get_tz_after_connected()` 的全局静态标志
```c
static bool has_run = false;
```
函数使用文件作用域静态变量，**全进程生命周期只执行一次**。在主循环顶部和 `wait_for_connect` 成功时各调用一次，无害（第二次直接返回 true）。

### 4. `net_connected` 状态与心跳 ping 的双重检测
- `net_connected` case 使用 `nw_get_connect_state()` 和 `rx_packets` 检测
- 心跳层使用 `test_can_ping_google()` 检测
- 两者独立工作，`rx_packets` 120s 无增长会重新触发 `reg_check` 即使 SDK 认为连接正常

### 5. `dial_stat_stop_call` 状态定义但未使用
枚举中有 `dial_stat_stop_call`，但状态机 switch 中没有对应 case，是遗留定义。

### 6. `AT+COPS=?` 的 180s 超时
在 `list_oper` 状态中使用，扫描可用运营商本身需要模组向多个频段发起请求，耗时长，此超时是合理的，但期间主循环阻塞（`Ql_SendAT` 同步调用）。

### 7. L1 中检查注册状态后再重拨
```c
bool reg_live = nw_reg_status_check(p_dial_mng->h_nw_client);
if (!reg_live) {
    // 跳过软重拨，等注册恢复
}
```
防止在未注册时反复 stop+start 数据呼叫（无意义且可能破坏状态）。

---

## 十四、状态机转移速查表

| 当前状态 | 成功 → | 失败 → | 超时 → |
|---|---|---|---|
| none | init | — | — |
| init | sim_init | modem_reset（实为重试）| — |
| sim_init | sim_check | modem_reset + sleep(2) | — |
| sim_check | sim_op | modem_reset + sleep(2) | stop_cfun（1hr）|
| sim_op | reg_check | modem_reset | — |
| reg_check | cereg_check | sleep(2) 重试 | reg_timeout_handler（300s）|
| cereg_check | precondition_check | sleep(10) 重试 | reg_timeout_handler（300s）|
| precondition_check | pre_start_call | modem_reset | — |
| pre_start_call | start_call | — | — |
| start_call | wait_for_connect | 原地重试 | — |
| wait_for_connect | net_connected | — | reg_check 或 list_oper（120s）|
| net_connected | （循环监控）| reg_check（SDK断线）| reg_check（rx_packets 120s 无增长）|
| list_oper | select_oper | reg_check（2次重试后）| — |
| select_oper | stop_cfun | 移除当前运营商 + list_oper | — |
| stop_cfun | start_cfun | reg_check（2次失败）| — |
| start_cfun | sim_init | reg_check（2次失败）| — |
| roamlink_starting | （ping成功→active，由心跳触发）| — | SIM路径 或重启（策略相关）|
| roamlink_active | （ping持续更新）| — | SIM路径 或重启（策略相关）|

---

*分析完成，基于 `dial.h` + `dial.c` 全文逐行阅读，2026-05-26*
