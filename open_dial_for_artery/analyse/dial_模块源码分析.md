# dial 模块源码深度分析

> 分析对象：`src/dial/dial.h`（209 行）+ `src/dial/dial.c`（1769 行）  
> 项目版本：open_dial_for_artery V1.29.07  
> 分析日期：2026-05-26

---

## 1. 模块概览

### 1.1 职责

`dial` 模块是整个 `open_dial` 程序的核心，承担以下职责：

1. **状态机驱动**：通过 `dial_task` 线程运行一个 19 状态的有限状态机，管理从上电到联网的完整生命周期。
2. **双通道管理**：同时支持物理 SIM 卡通道和 Roamlink 虚拟 SIM 通道，依据策略自动切换。
3. **健康检测**：通过 TCP 端到端连通性测试 + rx_packets 业务层保活，多维度判断链路质量。
4. **状态输出**：将运行状态原子写入 `/tmp/dial_status`，供外部监控工具读取。
5. **断网统计**：记录断网次数、时长，便于运营分析。

### 1.2 文件依赖

```
dial.c
├── dial.h          ── 本模块头文件（宏、枚举、结构体、全局变量声明）
├── at.h            ── AT 命令收发（Ql_SendAT，/dev/smd8）
├── nw.h            ── 网络层 API（TCP 测试、rx_packets、MCM 封装）
├── apn.h           ── APN 配置（从 /usr/dial/apn.json 读取）
├── tz.h            ── 时区读取（AT+QLTS=1 → /usr/dial/tz.ini）
├── seas_log.h      ── 结构化旋转日志（SD 卡）
├── roamlink/roamlink.h  ── RBMaster 生命周期 + 双通道切换
└── status/status.h      ── dial_status_t + dial_status_write()
```

### 1.3 线程分工

| 线程 | 入口 | 职责 |
|------|------|------|
| main thread | `main()` | 每 5s 调用 `nw_at_get_csq()` 更新 CSQ，写 `/tmp/network_csq` |
| dial_task thread | `dial_task()` | 状态机全部逻辑：拨号、切换、TCP 测试、状态输出 |

两线程共享 AT 串口 `smd_fd`，以 `pthread_mutex_t g_at_port_mutex` 保护。

---

## 2. dial.h 完整解析

### 2.1 宏定义全表

| 宏名 | 值 | 含义 | 使用状态 |
|------|----|------|---------|
| `SIM_CHECK_TIMEOUT_SECONDS` | 3600 | SIM 卡检测超时（1小时），超时后跳到 `stop_cfun` 复位 | `dial_stat_sim_check` |
| `DIAL_TIMEOUT_SECONDS` | 60 | rx_packets 无增长超时 / wait_for_connect 超时 | `net_connected`, `wait_for_connect` |
| `DIAL_CSQ_THREADHOLD` | 20 | CSQ 门限（未在状态机中判断，仅为参考值） | 未直接使用 |
| `DIAL_CSQ_INVALID` | 99 | CSQ 无效值标识 | 未直接使用 |
| `REG_CHECK_TIMEOUT_SECONDS` | 300 | 注册超时（`reg_check` + `cereg_check` 共享同一计时器），超时后按策略切换或 CFUN 复位 | `reg_check`, `cereg_check` |
| `TCP_TEST_INTERVAL_SECONDS` | 60 | SIM 通道 TCP 测试间隔 | `net_connected` |
| `TCP_FAIL_THRESHOLD` | 3 | SIM 通道连续 TCP 失败触发切换阈值 | `net_connected` |
| `ROAMLINK_CONNECT_WAIT_SEC` | 300 | 启动 Roamlink 后最长等待连通时间 | `roamlink_starting` |
| `ROAMLINK_CHECK_INTERVAL_SEC` | 30 | Roamlink 激活时 TCP 测试间隔 | `roamlink_active` |
| `ROAMLINK_FAIL_THRESHOLD` | 3 | Roamlink 连续 TCP 失败触发切换阈值 | `roamlink_active` |
| `ROAMLINK_NO_DATA_TIMEOUT_SEC` | 120 | Roamlink 业务层无数据超时（TCP 通但 rx_packets 不增长） | `roamlink_active` |
| `ROAMLINK_BIZ_LOG_INTERVAL_SEC` | 3600 | Roamlink 业务正常摘要日志间隔（1小时打一次，降噪） | `roamlink_active` |
| `SIM_FALLBACK_RETRY_SEC` | 300 | 策略1：SIM 备用通道稳定后尝试回切 Roamlink 的间隔 | `net_connected` |
| `ROAMLINK_FALLBACK_RETRY_SEC` | 300 | 策略2：Roamlink 备用通道稳定后尝试回切 SIM 的间隔 | `roamlink_active` |
| `OPER_NAME_LENGTH` | 64 | 运营商名称缓冲区长度 | `parse_oper_info_str` |
| `OPER_INFO_LENGTH` | 256 | 单条运营商信息字符串最大长度 | `parse_oper_list_info` |
| `AT_RETRY_NB` | 2 | AT 命令最大重试次数 | `list_oper`, `select_oper`, `stop_cfun`, `start_cfun` |
| `AUTO_REDIAL` | (宏定义，无值) | 定义时 `data_call.reconnect=true`（自动重拨），注释掉则 `false` | `dail_start_data_call` |
| `QUECTEL_AUTO_OPER_SELECT` | (注释掉) | 定义时启用自动运营商选择子路径（当前关闭） | 无效 |

> **注意**：`LICENSE_WAIT_TIMEOUT_SEC` 和 `LICENSE_CHECK_INTERVAL_SEC` 定义在 `roamlink/roamlink.h`，值分别为 300s（测试值，生产需改为 7200s）和 60s。

### 2.2 枚举 dial_stat_enu

```c
typedef enum {
    dial_stat_none,              // 0
    dial_stat_init,              // 1
    dial_stat_sim_init,          // 2
    dial_stat_sim_check,         // 3
    dial_stat_sim_op,            // 4
    dial_stat_reg_check,         // 5
    dial_stat_cereg_check,       // 6
    dial_stat_precondition_check,// 7
    dial_stat_pre_start_call,    // 8
    dial_stat_start_call,        // 9
    dial_stat_stop_call,         // 10  ⚠️ 死状态，switch 中无 case
    dial_stat_stop_cfun,         // 11
    dial_stat_start_cfun,        // 12
    dial_stat_list_oper,         // 13
    dial_stat_select_oper,       // 14
    dial_stat_wait_for_connect,  // 15
    dial_stat_net_connected,     // 16
    dial_stat_roamlink_starting, // 17
    dial_stat_roamlink_active,   // 18
} dial_stat_enu;
```

各状态语义：

| 状态 | 进入条件 | 状态内动作 | 正常退出 |
|------|---------|------------|---------|
| `none` | 初始状态 | 根据策略/license 决定首条路径 | → `init` 或 → `roamlink_starting` |
| `init` | none 分派 / Roamlink→SIM(未初始化) | 调用 `QL_MCM_NW_Client_Init` | → `sim_init` |
| `sim_init` | init 成功 / cfun 复位后 / Roamlink→SIM(已初始化,is_func_called=true) | `sim_op(SIM_OP_INIT)` | → `sim_check` |
| `sim_check` | sim_init 成功 | 轮询 `nw_get_sim_card_status()`，超时 3600s → `stop_cfun` | → `sim_op` |
| `sim_op` | sim_check 成功 | `sim_op_handler(get_iccid)` 读 ICCID | → `reg_check` |
| `reg_check` | sim_op 成功 / 各种断线/超时回跳 | `nw_reg_status_check()`，超时 300s → `reg_timeout_handler` | → `cereg_check` |
| `cereg_check` | reg_check 成功 | `AT+CEREG?` stat=1或5，超时 300s（共享 dial_timer）→ `reg_timeout_handler` | → `precondition_check` |
| `precondition_check` | cereg_check 成功 | `QL_Data_Call_Init_Precondition()` | → `pre_start_call` |
| `pre_start_call` | precondition 成功 | 懒加载 `apn_get_apn_obj(iccid)` + `apn_scan_idx()` + `QL_Data_Call_Set_Default_Profile` | → `start_call` |
| `start_call` | pre_start_call 设置完 profile | `dail_start_data_call()` | → `wait_for_connect` |
| `stop_call` | **⚠️ 死状态，永远不会被进入** | default: break | — |
| `stop_cfun` | sim_check 超时 / select_oper 失败 / reg_timeout 策略4 / cfun 复位流程 | `AT+CFUN=0`(15s) | → `start_cfun` |
| `start_cfun` | stop_cfun 成功 / select_oper 所有运营商失败 | `AT+CFUN=1`(15s) | → `sim_init` |
| `list_oper` | wait_for_connect 超时且 is_oper_select=true | `AT+COPS=?`(180s)，解析填充 deque | → `select_oper` |
| `select_oper` | list_oper 成功（deque 非空） | `AT+COPS=1,2,<PLMN>`(30s)，依次尝试 deque 中的运营商 | → `stop_cfun` |
| `wait_for_connect` | start_call 成功 | 轮询 `nw_get_connect_state()`，超时 60s → 重新路由 | → `net_connected` |
| `net_connected` | wait_for_connect 成功 | TCP 测试 + rx_packets 保活 + 切换判断 | → `reg_check` 或 → `roamlink_starting` |
| `roamlink_starting` | none分派 / SIM失败后切换 / 策略1回切 | 每 10s 一次 TCP 测试，超时 300s 切回 SIM 或重启 | → `roamlink_active` |
| `roamlink_active` | roamlink_starting TCP 通过 | 每 30s TCP 测试 + rx_packets 业务层检测 | → `sim_init/init` 或重启 Roamlink |

### 2.3 结构体 oper_info_t

```c
typedef struct {
    char oper_number[8];  // 运营商 PLMN 数字串，如 "46001"
} oper_info_t;
```

实际未被直接使用，运营商字符串通过 `CC_Deque` 以 `char*` 形式管理。

### 2.4 结构体 dial_mng_t（核心数据结构）

#### 基础连接字段

| 字段 | 类型 | 初始值 | 含义 |
|------|------|--------|------|
| `dial_st` | `dial_stat_enu` | `dial_stat_none` | 当前状态机状态 |
| `h_nw_client` | `nw_client_handle_type` | 0 | MCM NW 客户端句柄，`QL_MCM_NW_Client_Init` 后有效 |
| `p_ipv4_data_call_info` | `ql_data_call_info_s*` | NULL | 联网后的 IPv4 连接信息（IP、GW、DNS）|
| `p_ifaddrs` | `struct ifaddrs*` | NULL | 接口地址（浅拷贝，存在悬空指针风险，见缺陷章节）|
| `u64_if_rx_packets` | `uint64_t` | 0 | 上次读取的接口 rx_packets 值，用于增量比较 |
| `csq` | `uint8_t` | 0 | 由 main thread 每 5s 更新的信号强度 |
| `is_func_called` | `bool` | **true** | 数据呼叫门控标志（见 §5.1）|
| `rsv[2]` | `uint8_t[2]` | 0 | 保留字段，结构体对齐用 |
| `nw_node_name` | `char[NAME_MAX]` | "" | 网络接口名（如 rmnet_data0）|
| `profile_idx` | `int` | 0/-1 | APN profile 索引，`pre_start_call` 阶段设置，始终为 1 |
| `p_sim_mng` | `sim_mng_t*` | calloc | SIM 管理结构（ICCID、IMSI 等）|
| `dial_timer` | `struct timespec` | now | 多功能定时器（sim_check超时/注册超时/rx_packets超时）|
| `smd_fd` | `int` | `at_init()` | AT 串口 `/dev/smd8` 文件描述符 |
| `deque_oper` | `CC_Deque*` | 空队列 | 可用运营商 PLMN 列表（`list_oper` 填充，`select_oper` 消费）|
| `p_valid_oper` | `char*` | NULL | 当前选中的有效运营商（实际未使用）|
| `p_apn_obj` | `apn_obj_t*` | NULL | APN 对象（NULL = 下次拨号时重新查询）|

#### 策略与通道切换字段

| 字段 | 类型 | 初始值 | 含义 |
|------|------|--------|------|
| `network_select` | `int` | `roamlink_read_policy()` | 网络策略 1~4（1=PREFER_ROAMLINK, 2=PREFER_SIM, 3=FORCE_ROAMLINK, 4=FORCE_SIM）|
| `tcp_test_timer` | `struct timespec` | now | SIM 通道 TCP 测试计时，每 60s 触发 |
| `tcp_fail_count` | `int` | 0 | SIM 通道连续 TCP 失败次数，达 3 次触发切换 |
| `roamlink_timer` | `struct timespec` | now | 双重复用：`roamlink_starting` 下为等待超时计时；`roamlink_active` 下为测试间隔计时 |
| `roamlink_fail_count` | `int` | 0 | Roamlink 连续 TCP 失败次数，达 3 次触发切换 |
| `sim_initialized` | `bool` | false | `QL_MCM_NW_Client_Init` 是否已调用。Roamlink→SIM 切换时：true → 从 `sim_init` 重入（快速路径）；false → 从 `init` 重入（完整初始化）|
| `roamlink_available` | `bool` | probe 结果 | RBMaster+conf.ini+license 三者均就绪 |

#### License 等待字段

| 字段 | 类型 | 含义 |
|------|------|------|
| `license_pending` | `bool` | true = 处于首次激活等待。进入条件：probe()==LICENSE_MISSING 且备份恢复失败 |
| `license_wait_start` | `struct timespec` | 进入 license_pending 时记录，判断总超时（LICENSE_WAIT_TIMEOUT_SEC = 300s）|
| `license_check_timer` | `struct timespec` | 上次检测 license 文件的时间，每 60s 轮询一次 |

**license_pending 生命周期**：
```
probe() == LICENSE_MISSING
    └─ backup restore 失败
        ├─ license_pending = true
        ├─ network_select 强制 FORCE_SIM
        ├─ dial_stat_none → dial_stat_init（SIM 路径，不启动 RBMaster）
        ├─ SIM 联网（wait_for_connect 成功）→ roamlink_start_master()（rbmaster_started=true）
        └─ dial_task 主循环每 60s 轮询
            ├─ 出现 → roamlink_license_backup_and_reboot()（不返回）
            └─ 超时 300s → license_pending=false，永远 FORCE_SIM
```

#### Roamlink 业务层检测字段

| 字段 | 类型 | 含义 |
|------|------|------|
| `roamlink_rx_packets` | `uint64_t` | 进入 `roamlink_active` 时读取的 rx_packets 基准；每次有增长时更新 |
| `roamlink_no_data_timer` | `struct timespec` | 最后一次 rx_packets 有增长的时刻；距此超过 120s 判定业务层不工作 |

**设计背景**：TCP 连通性测试（到 18.196.0.17:22）只证明网络层可达，但 Roamlink 虚拟 SIM 注册失败时，rmnet_data* 接口无实际数据包。通过监控 rx_packets 增量来检测业务层真实工作状态。

#### rbmaster_started 字段

`bool rbmaster_started`，初始 false，仅在 `license_pending` 模式下使用。

**为什么不在 `dial_stat_none` 就启动 RBMaster**：RBMaster 启动后会立即向 Roamlink 云端建立连接，与物理 SIM 数据通道竞争，导致 `QL_Data_Call_Start` 完成后约 20s 内被踢断。正确时序是先让 SIM 稳定联网，再启动 RBMaster 利用现有网络下载 license。`rbmaster_started` 标志确保只在 SIM 首次进入 `net_connected` 时启动一次，后续断线重连不重复启动。

#### 回切计时器字段

| 字段 | 类型 | 零值语义 | 非零语义 |
|------|------|---------|---------|
| `sim_fallback_timer` | `struct timespec` | 未进入 SIM 备用通道 | 进入 SIM 备用的时刻，每 300s 尝试回切 Roamlink（策略1）|
| `roamlink_fallback_timer` | `struct timespec` | 未进入 Roamlink 备用通道 | 进入 Roamlink 备用的时刻，每 300s 尝试回切 SIM（策略2）|

两者均用 `memset(..., 0)` 初始化，以 `tv_sec == 0` 作为"尚未进入备用"的哨兵值。

#### 断网统计字段

| 字段 | 类型 | 含义 |
|------|------|------|
| `net_was_down` | `bool` | 上一状态是否为断网（边沿检测用）|
| `ever_connected` | `bool` | 曾经联网过（防止开机未联网就统计断网）|
| `net_down_since` | `struct timespec` | 本次断网开始时刻 |
| `outage_count` | `int` | 本次运行断网次数 |
| `last_outage_sec` | `long` | 上次断网持续秒数 |
| `total_outage_sec` | `long` | 本次运行累计断网秒数 |

边沿检测逻辑（在 `dial_status_update` 内执行）：
- 联网定义：`dial_stat_net_connected` 或 `dial_stat_roamlink_active`
- **联网→断网**：`net_down_since=now`，`outage_count++`，`net_was_down=true`
- **断网→联网**：计算 `last_outage_sec`，累加 `total_outage_sec`，`net_was_down=false`
- `ever_connected=false` 时，断网状态不触发统计

#### 日志降噪字段

`roamlink_biz_log_timer`：Roamlink 业务正常时，将每 30s 一条的摘要日志节流为每 3600s（1小时）一条。初始化为零值，首次触发时立即打一条，之后按 1 小时间隔。

### 2.5 全局变量

| 变量 | 定义位置 | 初始值 | 含义 |
|------|---------|--------|------|
| `g_dial_start_time` | `dial.c` | main 设置 | 进程启动时间，用于计算 uptime |
| `g_dial_version_str[16]` | `dial.c` | "unknown" | 版本字符串，main.c 写入 |
| `g_at_port_mutex` | `main.c`（extern） | 初始化 | AT 串口互斥锁，保护 smd_fd |

---

## 3. dial.c 函数逐一深度分析

### 3.1 `diff(start, end) → struct timespec`

```c
struct timespec diff(struct timespec start, struct timespec end)
```

计算两个 `CLOCK_MONOTONIC` 时间戳的差值，处理 `tv_nsec` 借位（当 `end.tv_nsec < start.tv_nsec` 时需从 `tv_sec` 借 1 秒 = 10⁹ ns）。

**全文调用点**：
- `dial_status_update`：计算 uptime、license_wait_sec、biz_no_data_sec、断网时长
- `dial_task`：所有定时器比较（reg 超时、TCP 测试间隔、Roamlink 等待等）

---

### 3.2 `dial_mng_new() → dial_mng_t*`

构造并初始化整个状态机管理结构，共 6 个阶段：

**阶段 1：基础分配**
```c
p_dial_mng = calloc(1, sizeof(dial_mng_t));
p_sim_mng  = calloc(1, sizeof(sim_mng_t));
p_dial_mng->smd_fd = at_init();           // 打开 /dev/smd8
cc_deque_new(&p_dial_mng->deque_oper);    // 初始化运营商队列
p_dial_mng->is_func_called = true;        // 门控初始为 true
```

**阶段 2：策略读取 + roamlink_probe()**

读取 `/usrdata/network.ini` 中的 `network_select`，然后执行 `roamlink_probe()` 处理四种情况：

| probe 结果 | 条件 | 处理动作 |
|-----------|------|---------|
| `PROBE_OK` | RBMaster+conf.ini+license 就绪 | `roamlink_available=true` |
| `PROBE_NO_PACKAGE` | RBMaster 不存在 | `roamlink_available=false`，`network_select=FORCE_SIM` |
| `PROBE_CONF_MISSING` | `/opt/conf.ini` 缺失 | `roamlink_available=false`，`network_select=FORCE_SIM`（等 factoryApp）|
| `PROBE_LICENSE_MISSING` | license 文件缺失/为空 | 先尝试备份恢复：成功→`roamlink_available=true`；失败→`license_pending=true`，`network_select=FORCE_SIM` |

`LICENSE_MISSING` 的备份恢复子路径：
```
roamlink_license_restore_from_backup()
    成功 → re_probe()
        PROBE_OK → roamlink_available=true
        PROBE_CONF_MISSING → FORCE_SIM（极端情况：两次 probe 之间 conf.ini 消失）
        其他 → FORCE_SIM（备份文件损坏）
    失败 → license_pending=true，clock_gettime(license_wait_start/check_timer)
```

**阶段 3：SIM 策略防呆（第一道保险）**

```c
if (network_select == PREFER_SIM || FORCE_SIM) {
    if (roamlink_is_master_running()) {
        roamlink_stop_service();
        sleep(2);  // 给模组切回物理 SIM 的时间
    }
}
```

**场景**：前一个 `dial` 实例在 Roamlink 通道运行时被 `killall`，模组底层仍处于虚拟 SIM 状态。不停服务则 `sim_op_handler` 会读到虚拟 SIM 的 ICCID（如 `8985203...F`），APN 数据库无法匹配，导致卡在 `start_call`。

**阶段 4：所有新字段初始化**

```c
clock_gettime(CLOCK_MONOTONIC, &p_dial_mng->tcp_test_timer);
clock_gettime(CLOCK_MONOTONIC, &p_dial_mng->roamlink_timer);
p_dial_mng->tcp_fail_count      = 0;
p_dial_mng->roamlink_fail_count = 0;
p_dial_mng->sim_initialized     = false;
p_dial_mng->license_pending     = false;  // 可能在阶段2被置 true
p_dial_mng->roamlink_rx_packets = 0;
clock_gettime(CLOCK_MONOTONIC, &p_dial_mng->roamlink_no_data_timer);
p_dial_mng->rbmaster_started    = false;
memset(&p_dial_mng->sim_fallback_timer,      0, ...);  // 零值=未进入备用
memset(&p_dial_mng->roamlink_fallback_timer, 0, ...);
p_dial_mng->net_was_down      = false;
p_dial_mng->ever_connected    = false;
p_dial_mng->outage_count      = 0;
memset(&p_dial_mng->roamlink_biz_log_timer, 0, ...);  // 零值=首次立即打
```

**阶段 5：写入初始 network_type**

```c
if (PREFER_ROAMLINK || FORCE_ROAMLINK)
    roamlink_mark_network_type(NETWORK_TYPE_ROAMLINK);  // /tmp/network_type = 2
else
    roamlink_mark_network_type(NETWORK_TYPE_SIM);        // /tmp/network_type = 1
```

---

### 3.3 `dial_init(p_dial_mng) → bool`

```c
bool dial_init(dial_mng_t *p_dial_mng) {
    if (0 != QL_MCM_NW_Client_Init(&p_dial_mng->h_nw_client))
        return false;
    // sim_op(SIM_OP_INIT, NULL);  // 注释掉，SIM 初始化移至 dial_stat_sim_init
    return true;
}
```

- 成功后在 `dial_stat_init` case 中设置 `sim_initialized=true`
- 整个程序生命周期只调用一次（由 `sim_initialized` 标志保证）

---

### 3.4 `dial_data_call_state_callback(state)`

数据呼叫状态变化的异步回调（由 SDK 内部线程调用）：

- **CONNECTED**：记录 IP/GW/DNS 日志，调用 `nw_mark_network_status(1)`
- **DISCONNECTED**：记录 profile_idx 和 err 日志，调用 `nw_mark_network_status(0)`

**为何不在回调里改 `dial_st`**：回调由 SDK 线程调用，与 `dial_task` 无同步机制。状态机的状态迁移必须在 `dial_task` 内通过 `nw_get_connect_state()` 轮询驱动，避免竞态。

---

### 3.5 `dail_start_data_call(p_dial_mng) → bool`

`is_func_called` 门控详解（是整个拨号重试机制的核心）：

```c
bool dail_start_data_call(dial_mng_t *p_dial_mng) {
    // 门控1：只有 is_func_called=true 时才调用 QL_Data_Call_Init
    if (p_dial_mng->is_func_called) {
        if (QL_Data_Call_Init(dial_data_call_state_callback))
            exit(0);  // Init 失败视为致命错误
    }
    // 门控2：只有 is_func_called=true 时才发起 QL_Data_Call_Start
    if (p_dial_mng->is_func_called) {
        data_call.profile_idx = p_dial_mng->profile_idx;
        data_call.ip_family   = QL_DATA_CALL_TYPE_IPV4;
        data_call.reconnect   = true;  // AUTO_REDIAL
        if (0 == QL_Data_Call_Start(&data_call, &err)) {
            p_dial_mng->is_func_called = false;  // 成功后关门
            ret = true;
        }
    }
    p_dial_mng->is_func_called = false;  // 末尾无条件关门
    return ret;
}
```

**`is_func_called` 完整生命周期**：

| 时刻 | 值 | 操作 |
|------|-----|------|
| `dial_mng_new()` 初始化 | **true** | 允许下次拨号时走 Init+Start 完整流程 |
| `dail_start_data_call` 成功 | false | Start 成功，不再重复 Init |
| `dail_start_data_call` 末尾 | false | 无论是否进入 Init/Start 分支，最终都关门 |
| `dial_stat_net_connected` 断线 | true | 断线后需要重新 Init+Start |
| `wait_for_connect` 超时 | true | 超时后需要重新 Init+Start |
| `roamlink→SIM` 切换（已初始化路径） | true | 切换后走 sim_init，但 start_call 时需要重新 Init |

> **⚠️ 注意**：`start_call` 状态调用 `dail_start_data_call` 失败时，函数末尾的无条件 `is_func_called=false` 会把门控关掉，导致下次循环进入 `start_call` 时 `is_func_called=false`，函数直接返回 false 而不重试。本质上 `start_call` 状态的重试是靠 `dial_task` 主循环的 `sleep(1)` + 下次 case 进入来驱动的，依赖状态没有被切换这一条件。

---

### 3.6 `dail_stop_data_call(profile_idx) → bool`

纯粹调用 `QL_Data_Call_Stop(profile_idx, QL_DATA_CALL_TYPE_IPV4)`，无副作用。

调用场景：
1. `wait_for_connect` 超时（清理悬挂呼叫）
2. SIM → Roamlink 切换前
3. `reg_timeout_handler` 中策略1/2 切换时

---

### 3.7 `parse_oper_info_str(p_oper_info, ...) → int`

解析单条运营商信息，如 `"2,\"CHN-UNICOM\",\"UNICOM\",\"46001\",2"`：

```c
// 解析过程（以 "2,"CHN-UNICOM","UNICOM","46001",2" 为例）：
// 迭代1: p_end→第1个','，temp="2"，       case 0: *p_stat=2，    select_idx=1
// 迭代2: p_end→第2个','，temp='"CHN-UNICOM"'，case 1: memcpy lg_oper，select_idx=2
// 迭代3: p_end→第3个','，temp='"UNICOM"'，  case 2: memcpy sh_oper，select_idx=3
// 迭代4: p_end→第4个','，temp='"46001"'，  case 3: memcpy nu_oper，select_idx=4
// 迭代5: strchr("2", ',') == NULL → return 4
//        ⚠️ case 4 (*p_act=...) 从未执行！act 字段永远是未初始化的栈值
```

**Bug 分析**：函数在找不到下一个逗号时返回 `select_idx`，最后一个字段（`act`，索引 4）的赋值分支永远无法到达。调用方 `parse_oper_list_info` 检查 `scanf_ret==4` 通过，但 `act` 始终是栈上未初始化值。由于 `act` 未被任何后续逻辑使用，不影响实际功能，属于潜在隐患。

---

### 3.8 `parse_oper_list_info(p_oper_deque, p_oper_list_str) → bool`

解析 `AT+COPS=?` 完整响应：
```
+COPS: (2,"CHN-UNICOM","UNICOM","46001",2),(1,"CHINA MOBILE","CMCC","46000",0),,(0-4),(0-2)
```

逻辑：
1. 定位 `+COPS:` 后的内容
2. 循环找 `(` `)` 对，提取每条运营商信息
3. 调用 `parse_oper_info_str` 解析
4. **只将 `stat==1`（available）的运营商的 numeric PLMN 加入 deque**（stat=0=forbidden，stat=2=current，stat=3=forbidden）
5. `calloc` 分配字符串存入 deque，由 `select_oper` 状态消费后 `free`

**缓冲区安全问题**：
- `memcpy(oper_info, p_start, p_end - p_start)` 未检查 `p_end - p_start` 是否超过 `OPER_INFO_LENGTH(256)`
- `memcpy(p_lg_oper, temp_msg, strlen(temp_msg))` 未检查是否超过 `OPER_NAME_LENGTH(64)`

---

### 3.9 `get_tz_after_connected(p_dial_mng) → bool`

```c
pthread_mutex_lock(&g_at_port_mutex);
Ql_SendAT(fd, "AT+QLTS=1", "OK", 1000, rsp_msg);
if (parse_tz_info(rsp_msg, &tz))
    save_tz_info(tz);  // 写 /usr/dial/tz.ini
pthread_mutex_unlock(&g_at_port_mutex);
```

- 1000ms 超时（较短，失败不影响联网流程）
- 加锁保护 AT 串口
- 只在 `wait_for_connect → net_connected` 转换时调用一次

---

### 3.10 `ipv4_data_call_info_init(p_dial_mng)`

```c
free(p_dial_mng->p_ipv4_data_call_info);
p_dial_mng->p_ipv4_data_call_info = nw_get_connect_ipv4_data_call_info(profile_idx);
if (p_ipv4_data_call_info != NULL) {
    free(p_dial_mng->p_ifaddrs);
    p_dial_mng->p_ifaddrs = nw_get_ifaddrs(inet_ntoa(...ip));
    if (p_ifaddrs != NULL)
        p_dial_mng->u64_if_rx_packets = 0;  // 清零 rx_packets 基准
}
```

**已知 Bug**：`nw_get_ifaddrs()` 返回的是浅拷贝（copy of `struct ifaddrs`），但内部调用了 `freeifaddrs()` 释放原始链表，导致 `ifa_name` 等字段成为悬空指针。`dial_status_update` 中访问 `p_ifaddrs->ifa_name` 时存在 UB。

---

### 3.11 `dial_status_update(p_dial_mng, version_str, cur_timer)`

从 `dial_mng_t` 抽取状态，填写 `dial_status_t`，调用 `dial_status_write()` 原子写入 `/tmp/dial_status`。

**完整字段填写逻辑**：

1. **version**：`strncpy(st.version, version_str, sizeof-1)`

2. **uptime**：`diff(g_dial_start_time, *cur_timer).tv_sec`

3. **state**：`dial_stat_name((int)dial_st)`（枚举值转字符串）

4. **policy / policy_name**：`dial_policy_name(network_select)`

5. **channel 字段判断**：
   ```
   roamlink_starting / roamlink_active → "ROAMLINK"
   net_connected                        → "SIM"
   none < state < net_connected         → "SWITCHING"
   其他                                  → "NONE"
   ```

6. **license_wait_sec**：仅 `license_pending=true` 时计算 `diff(license_wait_start, cur_timer).tv_sec`

7. **ICCID**：从 `p_sim_mng->sim_iccid` 读取

8. **APN**：从 `p_apn_obj->apn` 读取

9. **PLMN**：`open(NW_PLMN_PATH, O_RDONLY)` 读取 `/tmp/network_plmn`，去掉末尾 `\n`（`echo` 写入时带换行）

10. **net_status / net_type**：`open()` 分别读取 `/tmp/network_status` 和 `/tmp/network_type`

11. **IP / ifname**：从 `p_ipv4_data_call_info` 和 `p_ifaddrs` 读取

12. **counters**：`tcp_fail_count`, `roamlink_fail_count`, `roamlink_rx_packets`

13. **roamlink_state**：
    - `roamlink_starting` → "starting"
    - `roamlink_active`   → "active"
    - 其他               → "none"

14. **biz_no_data_sec**：仅 `roamlink_active` 时计算 `diff(roamlink_no_data_timer, cur_timer)`

15. **roamlink_connect_wait_sec**：仅 `roamlink_starting` 时计算 `diff(roamlink_timer, cur_timer)`

16. **rbmaster_pid**：遍历 `/proc/*/cmdline`，查找包含 "RBMaster" 的进程

17. **断网统计边沿检测**（详见 §2.4 断网统计字段）

18. **stats**：`outage_count`, `last_outage_sec`, `total_outage_sec`, `current_outage_sec`（断网中时实时计算）

---

### 3.12 `dial_task(arg)` — 主循环与状态机

#### 3.12.1 循环前置逻辑（每次迭代都执行，不受状态影响）

**① License 等待轮询**

```c
if (p_dial_mng->license_pending) {
    // 每 60s 检查 license 文件
    if (lc_dif.tv_sec >= LICENSE_CHECK_INTERVAL_SEC) {
        license_check_timer = cur_timer;
        if (roamlink_license_appeared()) {
            roamlink_license_backup_and_reboot();  // 不返回！
        }
    }
    // 总超时检查
    if (lt_dif.tv_sec >= LICENSE_WAIT_TIMEOUT_SEC) {  // 300s
        license_pending = false;
        // network_select 保持 FORCE_SIM
    }
}
```

**② 状态文件周期刷新**

```c
static struct timespec s_status_timer;
static bool s_status_initialized = false;
if (!s_status_initialized) {
    s_status_initialized = true;
    dial_status_update(...);  // 首次立即写，进程启动后文件尽快可见
} else if (diff(s_status_timer, cur_timer).tv_sec >= 30) {
    s_status_timer = cur_timer;
    dial_status_update(...);  // 之后每 30s 刷新
}
```

状态切换点会额外触发 `dial_status_update`（立即可见）。

#### 3.12.2 `dial_stat_none`

```c
case dial_stat_none:
    // 1. license_pending 优先：走 SIM 路径，不启动 RBMaster
    if (license_pending) {
        dial_st = dial_stat_init;
        break;
    }
    // 2. Roamlink 策略
    if (PREFER_ROAMLINK || FORCE_ROAMLINK) {
        // 第二道防呆：roamlink_available 再次确认
        if (!roamlink_available) {
            network_select = FORCE_SIM;
            dial_st = dial_stat_init;
            break;
        }
        roamlink_start_master();
        roamlink_timer = cur_timer;
        roamlink_start_service(smd_fd);  // AT+COPS=0 + RBstartServiceMaster
        dial_st = dial_stat_roamlink_starting;
    } else {
        dial_st = dial_stat_init;
    }
```

#### 3.12.3 `dial_stat_init`

```c
case dial_stat_init:
    if (dial_init(p_dial_mng)) {  // QL_MCM_NW_Client_Init
        sim_initialized = true;   // 标记已初始化，后续 Roamlink→SIM 可走快速路径
        dial_st = dial_stat_sim_init;
    }
    // 失败无 sleep，依赖循环末尾 sleep(1) 重试
```

#### 3.12.4 `dial_stat_sim_init`

```c
case dial_stat_sim_init:
    if (E_QL_OK == sim_op(SIM_OP_INIT, NULL)) {
        dial_st = dial_stat_sim_check;
        dial_timer = cur_timer;  // sim_check 超时计时起点
    } else {
        sleep(2);
    }
```

#### 3.12.5 `dial_stat_sim_check`

```c
case dial_stat_sim_check:
    if (nw_get_sim_card_status(h_nw_client)) {
        dial_st = dial_stat_sim_op;
    } else {
        sleep(2);
    }
    // 超时检查（与 sim_init 的 dial_timer 起点）
    if (diff(dial_timer, cur_timer).tv_sec > SIM_CHECK_TIMEOUT_SECONDS) {  // 3600s
        dial_st = dial_stat_stop_cfun;
    }
```

#### 3.12.6 `dial_stat_sim_op`

```c
case dial_stat_sim_op:
    if (sim_op_handler(p_sim_mng, sim_op_stat_get_iccid)) {
        dial_timer = cur_timer;   // 注册超时计时起点（reg_check + cereg_check 共享）
        dial_st = dial_stat_reg_check;
    }
    // ⚠️ 已知 Bug：sim_op_handler 中 get_iccid case 缺少 break，会 fall-through 到 get_imsi
    // 副作用：同时读了 IMSI，但行为无害
```

#### 3.12.7 `dial_stat_reg_check`

```c
case dial_stat_reg_check:
    if (nw_reg_status_check(h_nw_client)) {
        dial_st = dial_stat_cereg_check;
    } else {
        if (diff(dial_timer, cur_timer).tv_sec > REG_CHECK_TIMEOUT_SECONDS) {  // 300s
            goto reg_timeout_handler;
        }
        sleep(2);
    }
```

#### 3.12.8 `dial_stat_cereg_check`

```c
case dial_stat_cereg_check:
    if (nw_at_get_cereg_stat(smd_fd)) {   // AT+CEREG? stat=1 or 5
        dial_st = dial_stat_precondition_check;
    } else {
        if (diff(dial_timer, cur_timer).tv_sec > REG_CHECK_TIMEOUT_SECONDS) {
            goto reg_timeout_handler;
        }
        sleep(10);  // 注意：比 reg_check 的 sleep(2) 更长
    }
```

**关键点**：`reg_check` 和 `cereg_check` 共享同一个 `dial_timer`（在 `sim_op` 状态设置），因此 300s 是两个状态的**合计**超时，不是各自独立的 300s。

#### 3.12.9 `dial_stat_precondition_check`

```c
case dial_stat_precondition_check:
    if (0 == QL_Data_Call_Init_Precondition()) {
        profile_idx = -1;   // -1 = 触发下一状态的 APN 查询
        dial_st = dial_stat_pre_start_call;
    }
    // 失败无 sleep，依赖循环末尾 sleep(1) 重试
```

#### 3.12.10 `dial_stat_pre_start_call`

```c
case dial_stat_pre_start_call:
    if (profile_idx == -1) {
        // 懒加载 APN 对象（p_apn_obj=NULL 时才查询，切换通道时被清空）
        if (p_apn_obj == NULL)
            p_apn_obj = apn_get_apn_obj(p_sim_mng->sim_iccid);
        if (p_apn_obj != NULL)
            profile_idx = apn_scan_idx(p_apn_obj);  // 始终返回 1
    } else {
        // profile_idx 已设置，配置 default profile
        QL_Data_Call_Set_Default_Profile(&profile);
        dial_st = dial_stat_start_call;
    }
```

逻辑分两拍：第一次进入（`profile_idx==-1`）查 APN；第二次进入（`profile_idx==1`）配置 profile 并推进。

#### 3.12.11 `dial_stat_start_call`

```c
case dial_stat_start_call:
    if (dail_start_data_call(p_dial_mng)) {
        dial_st = dial_stat_wait_for_connect;
        dial_timer = cur_timer;  // wait_for_connect 超时计时起点
    }
    // 失败：is_func_called 已被无条件置 false，下次进入仍返回 false
    // 实际靠 sleep(1) + 状态未变 来重试
```

#### 3.12.12 `dial_stat_wait_for_connect`

```c
case dial_stat_wait_for_connect:
    if (nw_get_connect_state(profile_idx) == 1) {
        get_tz_after_connected();      // AT+QLTS=1
        ipv4_data_call_info_init();    // 获取 IP/接口信息，重置 rx_packets
        nw_mark_network_status(1);
        dial_st = dial_stat_net_connected;

        // license_pending 模式：SIM 联网后首次启动 RBMaster
        if (license_pending && !rbmaster_started) {
            roamlink_start_master();
            rbmaster_started = true;
        }
        dial_status_update(...);  // 立即刷新状态文件
    } else {
        if (diff(dial_timer, cur_timer).tv_sec > DIAL_TIMEOUT_SECONDS) {  // 60s
            dail_stop_data_call(profile_idx);  // 清理悬挂呼叫
            is_func_called = true;             // 重置门控

            if (p_apn_obj->is_oper_select) {
                dial_st = dial_stat_list_oper;
            } else {
                // AT+COPS=0 清除手动运营商锁定（超时180s）
                Ql_SendAT(smd_fd, "AT+COPS=0", "OK", 180000, rsp_msg);
                // 注意：AT+COPS=0 完成后才重置 dial_timer，避免 AT 耗时侵占注册窗口
                dial_timer = cur_timer;
                dial_st = dial_stat_reg_check;
            }
        }
    }
```

#### 3.12.13 `dial_stat_net_connected`（核心监控状态）

**阶段 1：断线检测**
```c
if (nw_get_connect_state(profile_idx) <= 0) {
    nw_mark_network_status(0);
    dial_timer = cur_timer;   // 注册超时起点
    is_func_called = true;    // 断线后需要重新 Init+Start
    dial_st = dial_stat_reg_check;
    tcp_fail_count = 0;
}
```

**阶段 2：rx_packets 保活检测**
```c
nw_get_if_statistics_rx_packets(&if_rx_packets, ifa_name);
if (if_rx_packets != u64_if_rx_packets && if_rx_packets > u64_if_rx_packets) {
    dial_timer = cur_timer;   // 有数据包增长，刷新 rx_packets 超时计时器
}
u64_if_rx_packets = if_rx_packets;
```

**阶段 3：TCP 端到端测试（每 60s）**
```c
if (diff(tcp_test_timer, cur_timer).tv_sec >= TCP_TEST_INTERVAL_SECONDS) {
    tcp_test_timer = cur_timer;

    if (nw_tcp_connectivity_test("18.196.0.17", 22, 5)) {
        // ── TCP 通过 ──
        if (tcp_fail_count >= 2)
            LOG "recovered after %d failures";
        tcp_fail_count = 0;
        dial_timer = cur_timer;

        // 策略1 主通道回切：在 SIM 备用通道稳定 300s 后尝试回切 Roamlink
        if (network_select == PREFER_ROAMLINK
            && roamlink_available
            && sim_fallback_timer.tv_sec > 0
            && diff(sim_fallback_timer, cur_timer).tv_sec >= SIM_FALLBACK_RETRY_SEC) {
            // SIM → Roamlink 切换序列
            dail_stop_data_call(profile_idx);
            p_apn_obj = NULL;
            roamlink_start_master();
            roamlink_mark_network_type(NETWORK_TYPE_ROAMLINK);
            roamlink_timer = cur_timer;
            roamlink_start_service(smd_fd);
            roamlink_fail_count = 0;
            dial_st = dial_stat_roamlink_starting;
            dial_status_update(...);
        }
    } else {
        // ── TCP 失败 ──
        tcp_fail_count++;

        // 【关键修复】每次 TCP 失败都刷新 dial_timer，防止 rx_packets 超时（60s）
        // 在 TCP 连续 3 次失败（需要 3*60=180s）之前率先触发，把 tcp_fail_count 清零
        // 导致切换逻辑永远无法触发的 bug
        dial_timer = cur_timer;

        if (tcp_fail_count >= TCP_FAIL_THRESHOLD) {  // 3
            tcp_fail_count = 0;
            nw_mark_network_status(0);

            if (license_pending) {
                // License 下载中：SIM 失败只重拨，不切换（切了 RBMaster 无法下载 license）
                dial_timer = cur_timer;
                dial_st = dial_stat_reg_check;
            } else if (network_select == PREFER_SIM || PREFER_ROAMLINK) {
                // 策略1/2：切到 Roamlink 备用
                dail_stop_data_call(profile_idx);
                p_apn_obj = NULL;
                roamlink_start_master();  // 幂等，已运行则跳过
                roamlink_mark_network_type(NETWORK_TYPE_ROAMLINK);
                roamlink_timer = cur_timer;
                roamlink_fallback_timer = cur_timer;  // 记录进入 Roamlink 备用时刻
                roamlink_start_service(smd_fd);
                roamlink_fail_count = 0;
                dial_st = dial_stat_roamlink_starting;
                dial_status_update(...);
            } else {
                // 策略4 FORCE_SIM：只重拨
                dial_timer = cur_timer;
                dial_st = dial_stat_reg_check;
            }
        }
    }
}
```

**阶段 4：rx_packets 超时**
```c
if (diff(dial_timer, cur_timer).tv_sec > DIAL_TIMEOUT_SECONDS) {  // 60s
    nw_mark_network_status(0);
    dial_timer = cur_timer;
    dial_st = dial_stat_reg_check;
    tcp_fail_count = 0;
}
sleep(5);
```

#### 3.12.14 `dial_stat_list_oper`

```c
case dial_stat_list_oper:
    if (cc_deque_size(deque_oper) == 0) {
        Ql_SendAT(smd_fd, "AT+COPS=?", "OK", 180000, rsp_msg);  // 180s 超时
        // 失败：retry_nb++，达 AT_RETRY_NB(2) → dial_stat_reg_check
        parse_oper_list_info(deque_oper, rsp_msg);
    }
    if (cc_deque_size(deque_oper) != 0) {
        dial_st = dial_stat_select_oper;
    } else {
        dial_timer = cur_timer;
        dial_st = dial_stat_reg_check;
    }
```

#### 3.12.15 `dial_stat_select_oper`

```c
case dial_stat_select_oper:
    cc_deque_get_at(deque_oper, 0, (void**)&p_oper);
    sprintf(dial_oper_slecet_str, "AT+COPS=1,2,%s", p_oper);  // ⚠️ 无边界检查
    Ql_SendAT(smd_fd, dial_oper_slecet_str, "OK", 30000, rsp_msg);  // 30s
    // 失败：retry_nb++，达上限则移除当前运营商
    //   deque 空 → dial_stat_start_cfun
    //   deque 非空 → dial_stat_list_oper（重新扫描）
    // 成功：移除队首元素，free(p_oper) → dial_stat_stop_cfun
```

#### 3.12.16 `dial_stat_stop_cfun`

```c
case dial_stat_stop_cfun:
    Ql_SendAT(smd_fd, "AT+CFUN=0", "OK", 15000, rsp_msg);
    // 失败：retry_nb++，达上限 → dial_stat_reg_check
    // 成功：usleep(5000*1000) → dial_stat_start_cfun
```

#### 3.12.17 `dial_stat_start_cfun`

```c
case dial_stat_start_cfun:
    Ql_SendAT(smd_fd, "AT+CFUN=1", "OK", 15000, rsp_msg);
    // 失败：retry_nb++，达上限 → dial_stat_reg_check
    // 成功：usleep(5000*1000) → dial_stat_sim_init（注意：直接到 sim_init，非 reg_check）
```

#### 3.12.18 `dial_stat_roamlink_starting`

```c
case dial_stat_roamlink_starting:
    dif_timer = diff(roamlink_timer, cur_timer);

    if (nw_tcp_connectivity_test("18.196.0.17", 22, 5)) {
        // Roamlink 已连通
        nw_mark_network_status(1);
        roamlink_timer = cur_timer;
        roamlink_fail_count = 0;
        // 读取 rx_packets 基准值
        nw_get_rmnet_rx_packets_sum(&roamlink_rx_packets);
        roamlink_no_data_timer = cur_timer;
        dial_st = dial_stat_roamlink_active;
        dial_status_update(...);
    } else if (dif_timer.tv_sec > ROAMLINK_CONNECT_WAIT_SEC) {  // 300s
        // 超时
        nw_mark_network_status(0);
        if (PREFER_ROAMLINK || PREFER_SIM) {
            roamlink_stop_service();
            roamlink_mark_network_type(NETWORK_TYPE_SIM);
            tcp_fail_count = 0;
            p_apn_obj = NULL;
            sim_fallback_timer = cur_timer;   // 记录进入 SIM 备用时刻
            tcp_test_timer = now;
            if (sim_initialized) {
                is_func_called = true;
                dial_st = dial_stat_sim_init;   // 快速路径
            } else {
                dial_st = dial_stat_init;        // 完整初始化
            }
        } else {  // FORCE_ROAMLINK（策略3）
            roamlink_stop_service();
            sleep(5);
            roamlink_timer = cur_timer;
            roamlink_start_master();
            roamlink_start_service(smd_fd);
            // 留在 roamlink_starting
        }
    } else {
        // 等待中：日志降噪（前15s打一次，之后每60s打一次）
        if (dif_timer.tv_sec < 15 || (dif_timer.tv_sec % 60) < 11)
            LOG "waited %lds / %ds";
    }
    sleep(10);  // 每 10s 检查一次
```

#### 3.12.19 `dial_stat_roamlink_active`

```c
case dial_stat_roamlink_active:
    dif_timer = diff(roamlink_timer, cur_timer);
    if (dif_timer.tv_sec >= ROAMLINK_CHECK_INTERVAL_SEC) {  // 30s
        roamlink_timer = cur_timer;

        if (nw_tcp_connectivity_test("18.196.0.17", 22, 5)) {
            roamlink_fail_count = 0;
            nw_mark_network_status(1);

            bool did_switch = false;  // 防止同一次循环中触发两次切换

            // 策略2 主通道回切
            if (!did_switch && PREFER_SIM
                && roamlink_fallback_timer.tv_sec > 0
                && diff(roamlink_fallback_timer, ...).tv_sec >= ROAMLINK_FALLBACK_RETRY_SEC) {
                // Roamlink → SIM 切换
                roamlink_stop_service();
                roamlink_mark_network_type(NETWORK_TYPE_SIM);
                tcp_fail_count = 0;
                p_apn_obj = NULL;
                sim_fallback_timer = cur_timer;
                tcp_test_timer = now;
                if (sim_initialized) { is_func_called=true; dial_st=sim_init; }
                else dial_st = init;
                dial_status_update(...);
                did_switch = true;
            }

            // 业务层存活检测（rx_packets）
            if (!did_switch) {
                nw_get_rmnet_rx_packets_sum(&cur_rx);
                if (cur_rx > roamlink_rx_packets) {
                    // 有增长：更新基准，日志降噪（每1小时打一条摘要）
                    roamlink_rx_packets = cur_rx;
                    roamlink_no_data_timer = cur_timer;
                } else {
                    // 无增长
                    no_data_dif = diff(roamlink_no_data_timer, cur_timer);
                    if (no_data_dif.tv_sec >= ROAMLINK_NO_DATA_TIMEOUT_SEC) {  // 120s
                        // 业务层死亡：按与 TCP 失败相同策略处理
                        roamlink_fail_count = 0;
                        nw_mark_network_status(0);
                        if (PREFER_ROAMLINK || PREFER_SIM) {
                            // 切到 SIM
                            ...（同 TCP 失败切换序列）
                        } else {  // FORCE_ROAMLINK
                            roamlink_stop_service(); sleep(5); restart;
                        }
                    }
                }
                // rmnet_data* 不存在：跳过业务层检测
            }
        } else {
            // TCP 失败
            roamlink_fail_count++;
            if (roamlink_fail_count >= ROAMLINK_FAIL_THRESHOLD) {  // 3
                roamlink_fail_count = 0;
                nw_mark_network_status(0);
                if (PREFER_ROAMLINK || PREFER_SIM) {
                    // 切到 SIM（sim_initialized 决定路径）
                    roamlink_stop_service();
                    roamlink_mark_network_type(NETWORK_TYPE_SIM);
                    tcp_fail_count = 0;
                    p_apn_obj = NULL;
                    sim_fallback_timer = cur_timer;
                    tcp_test_timer = now;
                    if (sim_initialized) { is_func_called=true; dial_st=sim_init; }
                    else dial_st = init;
                    dial_status_update(...);
                } else {  // FORCE_ROAMLINK
                    roamlink_stop_service(); sleep(5);
                    roamlink_timer = cur_timer;
                    roamlink_start_master(); roamlink_start_service(smd_fd);
                    dial_st = dial_stat_roamlink_starting;
                }
            }
        }
    }
    sleep(5);
```

#### 3.12.20 `reg_timeout_handler`（goto 目标）

```c
reg_timeout_handler:
    nw_mark_network_status(0);
    if ((PREFER_ROAMLINK || PREFER_SIM) && roamlink_available && !license_pending) {
        // 策略1/2：注册超时，切到 Roamlink
        dail_stop_data_call(profile_idx);
        p_apn_obj = NULL;
        roamlink_start_master();
        roamlink_mark_network_type(NETWORK_TYPE_ROAMLINK);
        roamlink_timer = cur_timer;
        roamlink_fallback_timer = cur_timer;  // 记录进入 Roamlink 备用时刻
        roamlink_start_service(smd_fd);
        roamlink_fail_count = 0;
        dial_st = dial_stat_roamlink_starting;
        dial_status_update(...);
    } else {
        // 策略4 / roamlink 不可用 / license_pending：cfun 复位
        is_func_called = true;
        dial_st = dial_stat_stop_cfun;
    }
```

**注意**：`license_pending` 时跳过 Roamlink 切换，因为 Roamlink 本身没有 license 无法使用。

#### 3.12.21 循环末尾

```c
goto dial_loop_continue;
// ...（跳过 reg_timeout_handler）
dial_loop_continue:
    sleep(1);   // 每次循环最少等待 1s，防止 CPU 满载空转
```

---

## 4. 状态转移完整表

| 当前状态 | 触发条件 | 目标状态 | 关键操作 |
|---------|---------|---------|---------|
| none | license_pending | init | — |
| none | PREFER/FORCE_ROAMLINK + available | roamlink_starting | start_master + start_service |
| none | 其他 | init | — |
| init | QL_MCM_NW_Client_Init 成功 | sim_init | sim_initialized=true |
| sim_init | sim_op(INIT) 成功 | sim_check | dial_timer=now |
| sim_check | nw_get_sim_card_status 成功 | sim_op | — |
| sim_check | 超时 3600s | stop_cfun | — |
| sim_op | sim_op_handler(iccid) 成功 | reg_check | dial_timer=now |
| reg_check | nw_reg_status_check 成功 | cereg_check | — |
| reg_check | 超时 300s | reg_timeout_handler | goto |
| cereg_check | AT+CEREG stat=1/5 | precondition_check | — |
| cereg_check | 超时 300s（共享 dial_timer）| reg_timeout_handler | goto |
| precondition_check | QL_Data_Call_Init_Precondition 成功 | pre_start_call | profile_idx=-1 |
| pre_start_call | APN 查询完成 | start_call | QL_Data_Call_Set_Default_Profile |
| start_call | dail_start_data_call 成功 | wait_for_connect | dial_timer=now |
| wait_for_connect | 连接成功 | net_connected | get_tz + ipv4_info_init |
| wait_for_connect | 超时 60s + is_oper_select | list_oper | dail_stop + is_func_called=true |
| wait_for_connect | 超时 60s + !is_oper_select | reg_check | AT+COPS=0 + dial_timer=now |
| net_connected | 断线 | reg_check | is_func_called=true |
| net_connected | TCP 连续失败 3 次（策略1/2）| roamlink_starting | dail_stop + start_service |
| net_connected | TCP 连续失败 3 次（策略4）| reg_check | dial_timer=now |
| net_connected | TCP 通过 + 策略1 + 回切条件 | roamlink_starting | dail_stop + start_service |
| net_connected | rx_packets 超时 60s | reg_check | dial_timer=now |
| list_oper | AT+COPS=? 成功 + deque 非空 | select_oper | — |
| list_oper | deque 为空 或 AT 失败 | reg_check | dial_timer=now |
| select_oper | AT+COPS=1,2,PLMN 成功 | stop_cfun | free(p_oper) |
| select_oper | 所有运营商失败 | start_cfun | — |
| stop_cfun | AT+CFUN=0 成功 | start_cfun | sleep(5s) |
| stop_cfun | AT 失败达上限 | reg_check | — |
| start_cfun | AT+CFUN=1 成功 | sim_init | sleep(5s) |
| start_cfun | AT 失败达上限 | reg_check | — |
| roamlink_starting | TCP 通过 | roamlink_active | 读rx_packets基准 |
| roamlink_starting | 超时 300s（策略1/2）| sim_init 或 init | stop_service + sim_fallback_timer=now |
| roamlink_starting | 超时 300s（策略3）| roamlink_starting | stop→restart（留在本状态）|
| roamlink_active | TCP 通过 + 策略2 + 回切条件 | sim_init 或 init | stop_service |
| roamlink_active | TCP 通过 + rx_packets 无增长 120s | sim_init/init 或 restart | — |
| roamlink_active | TCP 失败 3 次（策略1/2）| sim_init 或 init | stop_service |
| roamlink_active | TCP 失败 3 次（策略3）| roamlink_starting | stop→restart |
| reg_timeout_handler | 策略1/2 + available + !license_pending | roamlink_starting | start_service |
| reg_timeout_handler | 其他 | stop_cfun | is_func_called=true |

---

## 5. 关键机制深度分析

### 5.1 is_func_called 门控机制

`is_func_called` 是整个拨号重试机制的核心标志，防止 `QL_Data_Call_Init` 被重复调用。

```
初始状态：is_func_called = true（允许 Init+Start）
    ↓
dail_start_data_call()
    ├─ 成功路径：内部置 false + 末尾置 false（双重）
    └─ 失败路径：内部不改 + 末尾置 false（末尾关门）
    ↓
断线/超时重置：is_func_called = true（重开门）
```

**关键差异**：`open_dial_for_artery` 版本在函数末尾有无条件 `is_func_called=false`，但 `rtms_sdk` 版本修复了这个问题（失败时保持 `is_func_called` 不变，方便重试）。

### 5.2 定时器体系（时序关系）

```
时间轴示例（SIM 路径正常联网后）：

T=0  进入 net_connected，tcp_test_timer=now，dial_timer=now（rx_packets 重置）
     |
T=5  每次循环读 rx_packets，有增长则 dial_timer=now
     |
T=60 TCP 测试（60s 间隔）
     ├─ 通过：tcp_fail_count=0，dial_timer=now
     └─ 失败：tcp_fail_count++，dial_timer=now（阻止 rx 超时抢先触发）
     |
T=120 第2次 TCP 测试
     |
T=180 第3次 TCP 测试，tcp_fail_count=3 → 切换
     （如果没有 dial_timer=now 修复，T=65 rx超时就会先触发）
```

**所有定时器一览**：

| 定时器 | 使用状态 | 触发周期/阈值 | 用途 |
|--------|---------|--------------|------|
| `dial_timer` | sim_check, reg_check/cereg, wait_for_connect, net_connected | 60/300/3600s | 多功能，由状态切换重置 |
| `tcp_test_timer` | net_connected | 每 60s | SIM 通道 TCP 测试 |
| `roamlink_timer` | roamlink_starting, roamlink_active | 等待300s/每30s | Roamlink 超时/测试 |
| `roamlink_no_data_timer` | roamlink_active | 无增长 120s | 业务层死亡检测 |
| `roamlink_biz_log_timer` | roamlink_active | 每 3600s | 日志降噪 |
| `sim_fallback_timer` | net_connected | 满 300s 触发回切 | 策略1 回切 Roamlink |
| `roamlink_fallback_timer` | roamlink_active | 满 300s 触发回切 | 策略2 回切 SIM |
| `license_wait_start` | dial_task 前置 | 总超时 300s | license_pending 生命周期 |
| `license_check_timer` | dial_task 前置 | 每 60s | license 文件轮询 |
| `s_status_timer` (static) | dial_task 前置 | 每 30s | 状态文件周期刷新 |

### 5.3 SIM ↔ Roamlink 切换序列对比

#### 切换场景 A：SIM → Roamlink（TCP 失败，策略1/2）

```
触发：tcp_fail_count >= 3（net_connected 状态内）
操作序列：
  1. tcp_fail_count = 0
  2. nw_mark_network_status(0)
  3. dail_stop_data_call(profile_idx)
  4. p_apn_obj = NULL           ← 切换后重新查 APN
  5. roamlink_start_master()    ← 幂等，策略2首次需要启动
  6. roamlink_mark_network_type(ROAMLINK)
  7. roamlink_timer = now
  8. roamlink_fallback_timer = now  ← 记录进入 Roamlink 备用时刻（供策略2回切计时）
  9. roamlink_start_service(smd_fd) ← AT+COPS=0 + RBstartServiceMaster
  10. roamlink_fail_count = 0
  11. dial_st = roamlink_starting
  12. dial_status_update()
```

#### 切换场景 B：SIM → Roamlink（策略1 定时回切，TCP 通过时）

```
触发：tcp 通过 + PREFER_ROAMLINK + sim_fallback_timer>0 + 已超 300s
操作序列：同场景 A，但无 roamlink_fallback_timer=now（非备用切换）
```

#### 切换场景 C：Roamlink → SIM（TCP 失败，策略1/2）

```
触发：roamlink_fail_count >= 3（roamlink_active 状态内）
操作序列：
  1. roamlink_fail_count = 0
  2. nw_mark_network_status(0)
  3. roamlink_stop_service()      ← RBstopServiceMaster + sleep(3)
  4. roamlink_mark_network_type(SIM)
  5. tcp_fail_count = 0
  6. p_apn_obj = NULL
  7. sim_fallback_timer = now     ← 记录进入 SIM 备用时刻（供策略1回切计时）
  8. tcp_test_timer = now         ← 防止立即触发 TCP 测试
  9. if (sim_initialized):
       is_func_called = true
       dial_st = sim_init          ← 快速路径（复用已有 NW Client）
     else:
       dial_st = init              ← 完整初始化
  10. dial_status_update()
```

#### 切换场景 D：Roamlink → SIM（业务层无数据，策略1/2）

```
触发：TCP 通过但 rx_packets 无增长超 120s
操作序列：同场景 C（完全相同的切换序列）
```

#### 切换场景 E：Roamlink → SIM（策略2 定时回切）

```
触发：TCP 通过 + PREFER_SIM + roamlink_fallback_timer>0 + 已超 300s
操作序列：同场景 C，但 did_switch=true 防止后续业务层检测再次触发
```

### 5.4 license_pending 完整时序图

```
T=0  probe() == LICENSE_MISSING，backup 恢复失败
     ├─ license_pending = true
     ├─ network_select → FORCE_SIM
     ├─ license_wait_start = now
     └─ license_check_timer = now

T=~60s  dial_stat_none → dial_stat_init（SIM 路径，不启动 RBMaster）
        理由：RBMaster 启动后会抢占模组通道，导致 SIM 联网失败

T=~120s SIM 联网（wait_for_connect 成功）
        ├─ rbmaster_started = false → roamlink_start_master()
        └─ rbmaster_started = true

dial_task 前置逻辑（每次循环）：
  T=~180s（第1次检查）：roamlink_license_appeared() → 否
  T=~240s（第2次检查）：... → 否
  T=~300s 总超时到期（若 license 未下载完）：
          license_pending = false，永远 FORCE_SIM

或：
  T=~某时刻 license 文件出现：
          roamlink_license_backup_and_reboot()  → 重启（不返回）
          重启后 probe() 成功 → 正常策略流程
```

### 5.5 rx_packets 超时与 TCP 失败竞态问题及修复

**无修复时的 bug 路径**：
```
T=0   进入 net_connected，dial_timer=now，tcp_test_timer=now
T=60  第1次 TCP 测试失败，tcp_fail_count=1，dial_timer 不更新
T=65  rx_packets 超时（dial_timer 超 60s），tcp_fail_count=0，→ reg_check
      （tcp_fail_count 被清零，切换逻辑永远无法积累到 3 次）
```

**修复后**：
```
T=0   进入 net_connected，dial_timer=now，tcp_test_timer=now
T=60  第1次 TCP 测试失败，tcp_fail_count=1，dial_timer=now（刷新！）
T=120 第2次 TCP 测试失败，tcp_fail_count=2，dial_timer=now
T=180 第3次 TCP 测试失败，tcp_fail_count=3 → 触发切换
```

`dial_timer = cur_timer` 在每次 TCP 失败时刷新，保证 rx_packets 超时窗口始终从最近一次 TCP 测试起算，使 TCP 3 次失败（180s）先于 rx_packets 超时（60s+180s=240s）触发。

### 5.6 did_switch 防二次触发机制

`roamlink_active` 的 TCP 通过分支中，策略2 回切和业务层检测可能在同一次 30s 循环中都满足条件：

```c
bool did_switch = false;

// ① 策略2 回切检查
if (!did_switch && PREFER_SIM && roamlink_fallback_timer>0 && 满300s) {
    // 执行切换...
    did_switch = true;  // 设置标志
}

// ② 业务层存活检测
if (!did_switch && ...) {
    // 若 did_switch=true，跳过，防止二次调用 roamlink_stop_service()
}
```

`did_switch` 在每次 30s 测试时创建，作用域限于本次 `if` 块，确保：
- 同一次 TCP 测试中只能触发一个切换动作
- `roamlink_stop_service()` 不会被调用两次
- `dial_st` 不会被二次覆盖

### 5.7 AT 命令完整清单

| AT 命令 | 超时(ms) | 调用位置 | 锁保护 | 用途 |
|---------|---------|---------|-------|------|
| `AT+QLTS=1` | 1000 | `get_tz_after_connected` | ✅ | 获取网络时区 |
| `AT+COPS=?` | 180000 | `dial_stat_list_oper` | ✅ | 扫描可用运营商列表 |
| `AT+COPS=1,2,<PLMN>` | 30000 | `dial_stat_select_oper` | ✅ | 手动选择运营商 |
| `AT+COPS=0` | 180000 | `wait_for_connect` 超时后 | ✅ | 清除手动运营商锁，恢复自动选网 |
| `AT+COPS=0` | (roamlink) | `roamlink_start_service` 内 | ✅ | 切 Roamlink 前清锁 |
| `AT+CFUN=0` | 15000 | `dial_stat_stop_cfun` | ✅ | 关闭射频（模组进入最小功能模式）|
| `AT+CFUN=1` | 15000 | `dial_stat_start_cfun` | ✅ | 恢复全功能（重新注册网络）|
| `AT+CSQ` | (nw.c) | main thread，每 5s | ✅ | 读取信号强度 |
| `AT+CEREG?` | (nw.c) | `dial_stat_cereg_check` | ✅（via nw.c）| 查询 PS 域注册状态 |

---

## 6. 代码缺陷与风险点

### 6.1 parse_oper_info_str：act 字段永不被赋值（Low）

**位置**：`dial.c:340-378`

**问题**：`parse_oper_info_str` 在找不到下一个逗号时立即返回 `select_idx`，导致最后一个字段（`act`，index=4）的赋值永远无法执行。调用方 `parse_oper_list_info` 中 `int act` 未初始化，`parse_oper_info_str` 的 `scanf_ret==4` 检查通过，但 `act` 是栈上垃圾值。

**影响**：当前代码不使用 `act` 做任何判断，功能无影响。但若未来需要按接入技术过滤（如只选 LTE），该字段将失效。

**修复建议**：
```c
// 在循环后额外处理最后一个字段
if (p_start && *p_start) {
    memset(temp_msg, 0, OPER_NAME_LENGTH);
    strncpy(temp_msg, p_start, OPER_NAME_LENGTH - 1);
    if (select_idx == 4) *p_act = atoi(temp_msg);
    select_idx++;
}
return select_idx;
```

### 6.2 dial_stat_stop_call：枚举死状态（Info）

**位置**：`dial.h:61`（枚举），`dial.c:787`（switch，无对应 case）

**问题**：`dial_stat_stop_call` 存在于枚举中，但 `dial_task` 的 `switch` 语句没有对应的 `case`，会进入 `default: break`，状态机永远卡死在此状态。

**影响**：目前没有任何代码路径会进入此状态，不影响正常运行。

**修复建议**：从枚举中移除，或添加对应的 `case`（停止数据呼叫后跳到 `reg_check`）。

### 6.3 p_ifaddrs 悬空指针（Medium）

**位置**：`nw.c` `nw_get_ifaddrs()`；`dial.c:478-483` `ipv4_data_call_info_init`

**问题**：`nw_get_ifaddrs()` 内部调用 `getifaddrs()` + 遍历 + `freeifaddrs()`，只浅拷贝了 `struct ifaddrs` 本身，但 `ifa_name` 是指向被释放内存的字符串指针。

**影响**：`dial_status_update` 中访问 `p_ifaddrs->ifa_name` 时为 UB，在大多数平台上不立即崩溃（被释放的内存往往短期内内容不变），但属于潜在安全漏洞。

**修复建议**：在 `nw_get_ifaddrs` 中用 `strdup` 深拷贝 `ifa_name`，并在 `free(p_ifaddrs)` 前先 `free(p_ifaddrs->ifa_name)`。

### 6.4 sprintf 无边界检查（Low）

**位置**：`dial.c:1267`

```c
char dial_oper_slecet_str[256] = {0};
sprintf(dial_oper_slecet_str, "AT+COPS=1,2,%s", p_oper);
```

**问题**：`p_oper` 来自 `cc_deque`，其内容由 `parse_oper_list_info` 中的 `numberic_oper` 填充，`numberic_oper` 是 64 字节缓冲区。PLMN 最长 6 位，前缀 `"AT+COPS=1,2,"` 长 13 字节，总共不超过 20 字节，实际不会溢出。但缺乏防御性检查。

**修复建议**：改用 `snprintf(dial_oper_slecet_str, sizeof(dial_oper_slecet_str), "AT+COPS=1,2,%s", p_oper)`。

### 6.5 retry_nb 跨状态共享（Low）

**位置**：`dial.c:709`（`int retry_nb = 0`）

**问题**：`retry_nb` 是 `dial_task` 函数内的局部变量，在 `list_oper`、`select_oper`、`stop_cfun`、`start_cfun` 四个状态中共享使用。各状态在成功或超出阈值时有 `retry_nb=0` 重置，但若状态切换不经过重置路径（如 `list_oper` 直接超时跳到 `reg_check`，再回来），残留的 `retry_nb` 可能导致第一次失败就被认为达到阈值。

**实际影响**：`list_oper` 和 `stop_cfun` 都有 `retry_nb=0` 的安全路径，概率较低，但属于设计隐患。

### 6.6 NW_TCP_TEST_HOST 单点故障（Design）

**位置**：`src/nw/nw.h`

**问题**：TCP 测试目标硬编码为 `18.196.0.17:22`（单一 IP）。若该服务器不可达（维护、封锁、路由变更），SIM 和 Roamlink 两个通道都会被判定为失败，触发无意义的反复切换（flip-flop）。

**修复建议**：配置多个测试目标（如 `8.8.8.8:53`、`114.114.114.114:53`），任意一个通过即认为联通。

### 6.7 nw_at_get_csq 和 nw_mark_plmn 使用 system()（Medium）

**位置**：`src/nw/nw.c`

**问题**：使用 `system("echo %d > /tmp/network_csq")` 形式写文件，当参数来自模组（mcc/mnc）时存在注入风险（虽然 mcc/mnc 通常为数字，但属于防御性编程缺陷）。

**修复建议**：改用 `open()/write()/close()` 直接写文件。

### 6.8 LICENSE_WAIT_TIMEOUT_SEC 生产值未更新（Pre-ship ⚠️）

**位置**：`src/roamlink/roamlink.h`

**当前值**：300s（测试用）

**生产值**：应为 7200s（2小时，给 license 足够的下载时间）

**风险**：若保持 300s，license 下载超时后永远 FORCE_SIM，设备无法激活 Roamlink。

---

## 7. 总结

### 7.1 设计亮点

1. **did_switch 防二次触发**：用局部 bool 变量优雅地解决了同一 TCP 测试周期内多条切换路径可能并发触发的问题，代码简洁且安全。

2. **dial_timer 多功能复用**：同一个 `dial_timer` 字段在不同状态承担不同语义（sim_check 超时、注册超时、rx_packets 超时），通过状态切换时的显式重置保证语义清晰。设计紧凑但注释充分。

3. **TCP 失败时的 dial_timer 刷新**（bug fix）：用一行 `dial_timer = cur_timer` 优雅地解决了 rx_packets 超时与 TCP 失败计数之间的竞态，无需引入额外状态或定时器。

4. **license_pending 延迟启动机制**：精确地识别"RBMaster 会干扰 SIM 联网"的时序依赖，通过 `rbmaster_started` 标志实现单次延迟触发，设计严谨。

5. **fallback_timer 零值哨兵**：利用 `tv_sec == 0`（`memset` 初始化为 0）作为"未进入备用通道"的哨兵值，避免引入额外的 bool 字段，内存紧凑。

6. **roamlink_biz_log_timer 日志降噪**：通过独立的日志计时器，将 Roamlink 业务正常时的每 30s 一条摘要日志节流为每小时一条，在不损失异常告警的前提下大幅降低日志噪音。

7. **状态文件原子写入**：`dial_status_write` 先写 `.tmp` 再 `rename()`，读取方永远不会看到半写状态。

### 7.2 改进建议

| 优先级 | 问题 | 建议 |
|--------|------|------|
| ⚠️ Pre-ship | `LICENSE_WAIT_TIMEOUT_SEC=300` | 改为 7200 |
| Medium | `p_ifaddrs` 悬空指针 | `nw_get_ifaddrs` 深拷贝 `ifa_name` |
| Medium | `nw_mark_plmn` 使用 `system()` | 改用 `open/write` |
| Low | `parse_oper_info_str` act 字段 bug | 补充最后字段解析 |
| Low | `dial_stat_stop_call` 死状态 | 移除枚举条目或补充 case |
| Low | `sprintf` 无边界检查 | 改用 `snprintf` |
| Low | `retry_nb` 跨状态共享 | 在状态机入口处统一重置，或改为每状态独立变量 |
| Design | TCP 测试单点 | 配置多目标 fallback |

<!-- GENERATION_COMPLETE: 2026-05-26 -->
