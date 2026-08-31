# open_dial 项目完整技术参考文档

> 本文档由 CLAUDE.md 拆分而来，承载详细参考内容。精简版约定见仓库根目录 `CLAUDE.md`。

> **版本**：V1.27.6（MAIN=1, SUB=27, PATCH=6）  
> **目标平台**：Quectel EC200A / EG25（OpenCPU 模式，aarch64 嵌入式 Linux）  
> **语言**：C（Quectel ql-sdk API）  
> **最后分析时间**：2026-05  

---

## 目录

- [A. 全局概览](#a-全局概览)
- [B. 深度逐模块分析](#b-深度逐模块分析)
- [C. 安全与 Bug 审计](#c-安全与-bug-审计)
- [D. IPC / 通信协议分析](#d-ipc--通信协议分析)
- [E. 完整文档](#e-完整文档)
- [F. 测试方案](#f-测试方案)

---

## A. 全局概览

### A.1 项目定位与核心功能

`open_dial` 是运行在 Quectel 4G/5G 模组（EC200A/EG25）Linux 系统上的**蜂窝网络自动拨号守护进程**，核心使命是确保蜂窝数据连接的持续可用性——在信号波动、PDP 异常、模组内部错误等各种条件下，自动检测并分级恢复。

**硬件背景**：EC200A 是 OpenCPU 模式，AP（Cortex-A7，跑 Linux/dial）和 CP（Cortex-R5，跑蜂窝协议栈）封装在同一颗 ASR1803 芯片里。`AT+CFUN=1,1` 复位 CP 等于整颗 SoC 复位，Linux 必然跟着重启。

### A.2 目录结构

```
open_dial/
├── main.c              # ★ 程序入口 main() + 启动期逻辑（网卡等待/单实例/信号/CFUN 限频/版本号宏）（405 行）
├── dial.c              # ★ 主状态机 + 分级恢复 + fast retry + SDK/SIM 回调 + 恢复日志（855 行）
├── dial.h              # dial_mng_t 结构体、状态枚举、共享全局/函数声明
├── _public.h           # 全局头文件汇总
├── misc.c              # 通用工具（AT 执行/进程/状态文件/连通性检测/目录清理/时间/CPU 温度）（699 行）
├── misc.h
├── logger_sd.c         # SD 卡日志（自动降级到控制台）
├── logger_sd.h
├── Makefile            # 交叉编译配置（aarch64-linux-gnu-gcc）
├── README.md           # 版本历史
│
├── at/
│   ├── at.c            # AT 访问层：serial_atcmd 发 AT + 解析（CSQ/CEREG/CESQ/版本/IMSI 等）（656 行）
│   └── at.h
│
├── diag/
│   ├── diag.c          # 故障诊断采集：dmesg/logcat 快照 + CP dump 绑定挂载（134 行）
│   └── diag.h
│
├── apn/
│   ├── apn.c           # JSON 格式 APN 数据库加载 + ICCID 精确匹配
│   └── apn.h
│
├── data_call/
│   ├── data_call.c     # 数据连接状态回调 + 路由/DNS/NAT 配置（235 行）
│   └── data_call.h
│
├── nw/
│   ├── nw.c            # 网络状态查询（信号/注册/RX 流量统计）
│   └── nw.h
│
├── sim/
│   ├── sim.c           # SIM 卡初始化 + ICCID/IMSI 读取 + SIM 状态枚举映射
│   └── sim.h
│
├── reboot_conf/
│   ├── dial_reboot_conf.c  # 启动配置持久化 + 前置条件检测
│   └── dial_reboot_conf.h
│
├── cc_deque/           # 双端队列实现（当前版本未被主流程使用）
│   ├── cc_deque.c / cc_deque.h
│   └── cc_common.c / cc_common.h
│
└── test_utils/         # 调试菜单框架
    ├── test_utils.c
    └── test_utils.h
```

### A.3 模块依赖关系

分层原则：**SDK 相关进按域模块（sim/nw/data_call/apn），AT 命令进 at/，故障诊断进 diag/，系统通用进 misc/**。

```
main.c（入口）
  ├─ dial.c          调用 dial_loop()；共享 p_dial_mng/g_sigint_received（声明见 dial.h）
  ├─ diag/           启动期 setup_cp_dump_capture()
  ├─ misc.c          test_can_ping_google() 等
  └─ reboot_conf/ + sim/ + logger_sd

dial.c（运行期编排：状态机 + 分级恢复 + 回调 + 恢复日志）
  ├─ at/at.c         ★AT 访问层：各 get_*_safe / build_ext_line / get_cpin_status_str
  ├─ diag/diag.c     ★诊断采集：diag_snapshot / teardown_cp_dump_capture
  ├─ misc.c          连通性检测/进程/状态文件/目录清理/时间/CPU 温度
  ├─ logger_sd.c     日志初始化和写入
  ├─ reboot_conf/    读写启动配置、CFUN 限频保护
  ├─ apn/apn.c       APN 数据库加载和 ICCID 匹配
  ├─ sim/sim.c       SIM 初始化、ICCID 读取、sim_app_state_str 枚举映射
  ├─ data_call/      连接状态回调、路由/NAT/DNS 设置
  └─ nw/nw.c         信号强度、注册状态

at/at.c  →  misc.c（exec_cmd_safe 为内部 helper，不依赖运行期状态）
diag/diag.c  →  misc.c（cleanup_dir_keep_newest）+ logger_sd
```

### A.4 关键入口文件与启动流程

入口：`main.c:main()`，启动后通过 12 个步骤初始化，最终调用 `dial.c:dial_loop()`（永不退出，快速失败模式除外）。完整流程见 [E.4 时序图](#e4-关键流程时序图)。

### A.5 技术栈与第三方库

| 类别 | 内容 |
|------|------|
| 编译器 | `aarch64-linux-gnu-gcc`（交叉编译） |
| SDK | Quectel ql-sdk（`ql_data_call`、`ql_sim`、`ql_nw`） |
| 第三方库 | `json-c`（APN JSON 解析） |
| 外部工具 | `serial_atcmd`、`ping`、`ip`、`iptables`、`pgrep` |
| 标准库 | POSIX（`ioctl`、`statvfs`、`sigaction`、`popen`） |
| 内置数据结构 | `cc_deque`（双端队列，当前未使用） |

---

## B. 深度逐模块分析

### B.1 主程序 dial.c

#### 模块职责

运行期编排核心（快速失败、心跳、状态机、分级恢复、SDK/SIM 回调、恢复日志）。程序入口 `main()` 及启动期初始化已拆至 `main.c`；AT 查询解析拆至 `at/`；故障诊断采集拆至 `diag/`；连通性检测/目录清理/时间/温度等通用工具拆至 `misc.c`。

#### 核心数据结构与关键变量

```c
// dial_loop() 内部关键状态变量（均为局部变量）
enum Stage stage;           // 当前状态机阶段
uint64_t start_fail_ts;     // 首次 Ping 失败的时间戳（ms，0=无故障）
uint64_t last_l1_ts;        // 上次 L1 尝试时间（含 skip/fail），防止每 1.5s 重复触发
uint64_t last_l2_ts;        // 上次 L2 执行时间，用于 L2 冷却节流（与 L1 独立）
int recovery_level;         // 当前触发的恢复等级（0/1/2/3）
int has_notified_connect;   // 是否已触发连接成功回调（0/1）
int is_fast_fail_mode;      // 是否处于快速失败模式
int ping_fail_count;        // 连续 Ping 失败次数（抖动过滤计数器）
int sim_error_active;       // SIM 曾发生断开，READY 恢复时打印日志用
int diag_snap_done;         // 当前故障周期已打过 DIAG 快照标志

// 全局变量（dial.c 文件域）
static volatile int g_pdp_connected = 0;     // PDP 建立状态（回调写入，主循环读取）
static volatile int g_sigint_received = 0;   // SIGINT 信号标志
static volatile int g_sdk_service_error = 0; // SDK CP 侧服务崩溃标志（data_call_service_error_cb 写入）
static volatile int g_sim_app_ready = -1;    // SIM 就绪标志（-1=回调未初始化, 0=非READY, 1=READY）
static volatile int g_sim_app_state = 0;     // QL_SIM_APP_STATE_E 整数值，供日志用
static int g_last_reg_stat = -1;             // 最近一次 REG 状态，供恢复逻辑判断
static char g_if_name[64];                   // 当前数据连接的网络接口名（如 ccinet1）
int enable_policy_recovery = 1;              // 0=禁用应用层恢复，仅靠 SDK 重连
```

#### 状态枚举

```c
enum Stage {
    ST_STATUS   = 0,  // 流转起点，立即跳 ST_SIM
    ST_SIM      = 1,  // 检查 SIM 卡（AT+CPIN?），间隔 800ms
    ST_SIGNAL   = 2,  // 信号检测（当前透传），间隔 800ms
    ST_PING     = 3,  // Ping 8.8.8.8 验证连通性，间隔 1500ms
    ST_RECOVERY = 4   // 执行分级恢复操作
};
```

#### 关键函数调用关系

```
main()
  ├─ wait_for_interface("ecm0", 30)   ← ioctl SIOCGIFINDEX 轮询
  ├─ read_config() / check_sim_status() / get_signal_csq()
  ├─ check_pid_running(-1)            ← pgrep dial 单实例检查
  ├─ restart_cfun()                   ← exit_count >= 20 时触发（有保护）
  ├─ log_init()                       ← SD 卡日志初始化
  ├─ test_can_ping_google(NULL)        ← 快速连接检测
  └─ dial_loop(on_network_connected, NULL)

dial_loop()
  ├─ ql_data_call_init()              ← 最多重试 200 次，每次 100ms
  ├─ ql_data_call_set_status_ind_cb() ← 注册连接状态回调
  ├─ ql_data_call_set_service_error_cb() ← 注册 SDK 崩溃回调（v1.27.1）
  ├─ sim_init() + sim_get_iccid()
  ├─ ql_sim_set_card_status_cb()      ← 注册 SIM 状态变化回调（v1.27）
  ├─ ql_sim_get_card_info()           ← 主动获取初始 SIM 状态
  ├─ set_apn(iccid)                   ← apn/apn.c
  ├─ ql_data_call_create/config/start()
  └─ while(1)  [50ms 轮询]
       ├─ SIGINT 检测
       ├─ SDK 崩溃检测（g_sdk_service_error）
       ├─ 快速失败检测（is_fast_fail_mode）
       ├─ 30s 心跳日志（SIM_AT/SIM_CB 双值/REG/CSQ/TEMP/DownTime）
       ├─ 5min 扩展心跳（CESQ/小区/IP，联网后才启动）（v1.27）
       └─ 状态机（ST_STATUS→ST_SIM→ST_SIGNAL→ST_PING→ST_RECOVERY）
```

#### 时间参数完整表

| 常量 | 值 | 说明 |
|------|----|------|
| `PING_INTERVAL_MS` | 1500ms | Ping 检测间隔 |
| `SIM_INTERVAL_MS` | 800ms | SIM 状态检测间隔 |
| `SIG_INTERVAL_MS` | 800ms | 信号检测间隔 |
| `HEARTBEAT_INTERVAL_MS` | 30000ms | 基础心跳日志间隔 |
| 扩展心跳间隔 | 5min | 信号质量+小区+IP，联网后启动 |
| `LEVEL1_TIMEOUT` | 30,000ms（30s，v1.28.8） | L0 结束阈值，同时为 L1 触发阈值 |
| `LEVEL2_TIMEOUT` | 330,000ms（5.5min，v1.28.8） | 触发 L2（射频重置）阈值 |
| `LEVEL3_TIMEOUT` | 1,830,000ms（30.5min，v1.28.8） | 触发 L3（进程 exit 重拉）阈值 |
| `L2_REG0_FAST_TIMEOUT_MS` | 90,000ms（90s，v1.28.8 新增） | REG=0 跳过 L1 直接触发 L2 的独立阈值（原复用 LEVEL1_TIMEOUT） |
| L1 冷却期 | 60s（`last_l1_ts`） | L1 节流，防止每 1.5s 重复触发 |
| L2 冷却期（正常路径） | 5min（`last_l2_ts`） | L2 节流 |
| L2 冷却期（REG=0） | 90s（`last_l2_ts`） | REG=0 时 L2 快速重触发 |
| `FAST_FAIL_TIMEOUT_MS` | 10,000ms | PDP 就绪后的 Ping 超时窗口 |
| `PDP_WAIT_TIMEOUT_MS` | 60,000ms | PDP 建立最长等待时间 |
| SDK `reconnect_interval` | 25s | SDK 底层自动重连间隔 |
| `PING_FAIL_THRESHOLD` | 3 | 连续失败次数才触发故障计时 |

#### 与其他模块的耦合点

- `data_call_status_ind_cb`（data_call.c）写入 `g_pdp_connected`、`g_need_reset_fail_ts`
- `apn/apn.c::set_apn()` 需要 ICCID 字符串
- `restart_cfun()` 读写 `/tmp/cfun_count.txt` 和 `/tmp/cfun_last_call.txt`

---

### B.2 数据连接模块 data_call/data_call.c

#### 模块职责

SDK 连接状态回调处理中心：连接成功时配置路由/NAT/DNS，断开时清理路由。

#### 核心回调：data_call_status_ind_cb

```
CONNECTED 时：
  ├─ update_network_status(1)              → /tmp/network_status = 1
  ├─ system("echo 0 > /tmp/dial_Status")
  ├─ 记录 g_if_name（如 "ccinet1"）
  ├─ 循环删除旧 NAT MASQUERADE 规则（最多 10 次）
  ├─ ip route del default dev <device>     ← 先删旧路由
  ├─ ip route add default via <gw> dev <device>
  ├─ iptables -t nat -A POSTROUTING -o <device> -j MASQUERADE
  ├─ 写 /tmp/resolv_v4.conf（主/副 DNS，带 \n 不带 \r）
  ├─ 合并到 /etc/resolv.conf
  └─ IPv6 同理（has_addr6 时）

DISCONNECTED 时：
  ├─ update_network_status(0)
  ├─ system("echo 1 > /tmp/dial_Status")
  ├─ ip route del default dev <g_if_name>  ← 精确按接口清理
  ├─ ip -6 route del default dev <g_if_name>
  ├─ 循环删除该接口所有 MASQUERADE 规则
  ├─ unlink("/tmp/resolv_v4.conf")
  ├─ unlink("/tmp/resolv_v6.conf")
  └─ g_need_reset_fail_ts = 1              ← 通知状态机重置故障计时器
```

#### call_id 过滤

当前版本按 `call_name`（如 `"auto_network"`）过滤，不再用 `call_id`（SDK 重拨后 call_id 会变化）。

#### flow_monitor_task（历史遗留，未在主流程调用）

读取 `/sys/class/net/<if>/statistics/rx_packets`，若 120s 内无收包则返回 `dial_stat_stop`。

---

### B.3 APN 自适应模块 apn/apn.c

#### 模块职责

从 `/usr/dial/apn.json` 加载运营商 APN 数据库，按 SIM ICCID **精确匹配**（`strcmp`，非前缀匹配）。

#### apn_obj_t 结构体

```c
typedef struct {
    char iccid[32];     // ICCID（精确匹配）
    char apn[32];       // APN 名称
    char usr_name[32];  // 用户名（可为空）
    char pwd[32];       // 密码（可为空）
} apn_obj_t;
```

#### apn.json 格式

```json
{
  "apn": [
    { "supplier": "中国移动", "iccid": "898600XXXXXXXXXXXXXXX", "apn": "cmnet", "usrname": "", "pwd": "" },
    { "supplier": "中国联通", "iccid": "898601XXXXXXXXXXXXXXX", "apn": "3gnet",  "usrname": "", "pwd": "" },
    { "supplier": "中国电信", "iccid": "898603XXXXXXXXXXXXXXX", "apn": "ctnet",  "usrname": "", "pwd": "" }
  ]
}
```

#### 匹配逻辑

`strcmp(apn_entry.iccid, sim_iccid)` 精确匹配（V1.26 修复了老版本 `strncmp` 前缀误匹配问题）。未匹配时回退到默认 APN `"apnpublic"`。

---

### B.4 网络状态模块 nw/nw.c

#### 模块职责

封装 Quectel SDK 网络查询 API，提供信号强度、注册状态、RX 流量统计。

#### 实际被主流程调用的函数

| 函数 | 说明 |
|------|------|
| `get_signal_strength(level)` | 获取信号强度等级，写 `/tmp/network_csq` |
| `nw_get_if_statistics_rx_packets()` | 读网卡 RX 统计（供 flow_monitor_task 用） |
| `nw_get_data_reg_status()` | 获取 LTE/5G 注册状态 |

#### 信号强度等级映射

| SDK 枚举 | 字符串 |
|---------|-------|
| `LEVEL_NONE` | UNKNOWN |
| `LEVEL_POOR` | POOR |
| `LEVEL_MODERATE` | MODERATE |
| `LEVEL_GOOD` | GOOD |
| `LEVEL_GREAT` | GREAT |

**注意**：`nw_mark_network_status()` 直接写 `/tmp/network_status`（已统一改为调用 `update_network_status()`，避免与 misc.c 重复写入）。

---

### B.5 SIM 卡模块 sim/sim.c

#### 模块职责

对 Quectel `ql_sim` API 的简单封装，仅用于启动时初始化和 ICCID 读取。

```c
int sim_init(void);                                        // ql_sim_init()
int sim_get_iccid(char *iccid, QL_SIM_SLOT_E slot);      // 参数化版本（V1.26）
int sim_get_imsi(char *imsi, QL_SIM_SLOT_E slot);
```

**限制**：默认使用 `QL_SIM_SLOT_1`，不支持双卡自动切换。运行时 SIM 检测改用 `serial_atcmd at+cpin?`（更轻量）。

---

### B.6 持久化配置模块 reboot_conf/

#### 模块职责

持久化启动配置，防止循环重启；提供 `restart_cfun()` 的频率保护逻辑。

#### Config 结构体

```c
typedef struct {
    int uptime;           // 启动时系统 uptime（秒），持久化
    int restart_flag;     // 是否已执行重启（0/1），持久化
    int first_disconnect; // 首次断网标志（运行时，不持久化）
    int signal_strength;  // CSQ 值（运行时）
    int sim_status;       // sim_init() 返回值（运行时）
    int ql_netd_status;   // ql_netd 状态（运行时）
} Config;
Config g_config;          // 全局配置
```

持久化格式：`/tmp/dial_reboot.conf` → `<uptime>,<restart_flag>\n`

#### restart_cfun() 双重保护

```c
// 调用上限 10 次（/tmp/cfun_count.txt）
// 两次调用间隔至少 600 秒（/tmp/cfun_last_call.txt）
// 超限时直接 return，不执行 AT+CFUN
```

---

### B.7 SD 卡日志模块 logger_sd.c

#### 模块职责

将运行日志写入 SD 卡，支持自动降级（SD 卡不可用时仅控制台输出）。

#### 初始化条件

1. `/media/sdcard` 在 `/proc/mounts` 中存在
2. `statvfs()` 检查剩余空间 ≥ 500MB
3. 创建 `/media/sdcard/dial_log/dial_YYYYMMDD_HHMMSS.log`

#### dial_log() 特性

- 同时输出到 `stdout` 和日志文件
- 每条日志前缀 `[YYYY-MM-DD HH:MM:SS]`
- 每次写入后立即 `fflush()`，防掉电丢数据

---

### B.8 工具函数库 misc.c

#### 关键函数

| 函数 | 说明 |
|------|------|
| `executeATCommand(cmd, buf, size)` | popen 执行命令，输出写入调用方 buffer（V1.26 改为传入 buffer，避免堆分配） |
| `update_network_status(status)` | 读→比较→写 `/tmp/network_status`，避免重复写入 |
| `check_process(name)` | `pgrep -x <name> | wc -l`，精确匹配（V1.26 加 `-x` 防止误匹配子串） |
| `isIdleState()` | 检查 wlan0 是否有 WiFi 客户端（AP/STA 模式感知） |
| `restart_ql_netd()` | 双重 fork 重启 ql_netd，子进程被 init 收养（V1.26 修复僵尸进程问题） |
| `read_exit_count()` / `write_exit_count(n)` | 读写 `/tmp/exit_count.txt` |

---

### B.9 分级故障恢复状态机

```
断网持续时间轴（v1.28.8+）：
0 ──── 30s ──────── 5.5min ────────────── 30.5min
       │               │                    │
     L0 结束          Level 2             Level 3
     Level 1          射频重置             进程重启
       │               │                    │
  ql_data_call_stop()  serial_atcmd        sync()
  sleep(2)             at+cfun=0           log_close()
  ql_data_call_start() sleep(3)            exit(1) ← start_prog 重拉
                       serial_atcmd        (仅 AP 进程重启，不复位 SoC，
                       at+cfun=1            v1.28.0 起弃用 cfun=1,1)
                       sleep(10)
                       ql_data_call_start()

L0 期（0~30s）：SDK 自动重连窗口，不干预
  └─ 期内恢复：记录 "[EVENT] Network Recovered in SDK phase (L0)"

L1（>30s，REG≠0）：冷却 60s，每次 stop→sleep(2)→start
  └─ REG=0 时跳过 L1，直接等 L2（L1 无效：start 返回 -1001）

L2（>5.5min，或 REG=0 且 >90s [L2_REG0_FAST_TIMEOUT_MS，v1.28.8 与
   LEVEL1_TIMEOUT 解耦；REG 仅 30s 心跳采样一次，等效要求 REG=0 持续
   ≥60s 才拉 cfun，防移动场景瞬时掉注册误触发]）：
  └─ REG=0 冷却 90s，正常路径冷却 5min
  └─ REG=0 且 30s~90s 窗口内：L1/L2 均不动作，等瞬时掉注册自愈

L3（>30.5min）：sync→log_close→exit(1) → start_prog 重拉
  （v1.28.0 起由 cfun=1,1 整机重启改为纯 exit，不影响同机业务）

（v1.28.3 起已移除"SIM 恢复→强制立即 L1"路径，SIM 断开/恢复仅作诊断
  标志，恢复统一由 Ping 计时路径按断网时长触发）

前提门控（v1.28.3+）：enable_policy_recovery == 1 && has_connected_once
```

### B.10 快速失败重试（Fast Retry）机制

```
/tmp/dial_retry_count 不存在 → 第 1 次（Attempt 1/3）
  PDP_WAIT_TIMEOUT (60s)：PDP 未建立 → exit(1)
  FAST_FAIL_TIMEOUT (10s)：PDP 建立后 ping 窗口 → exit(1)
  成功 → clear_retry_count()，进入持久循环

count=1 → 第 2 次（Attempt 2/3），同上
count=2 → 第 3 次（Attempt 3/3），同上
count≥3 → is_fast_fail_mode=false，进入持久循环模式（永不超时退出）

持久循环中拨通后 → clear_retry_count()，下次断网重启从头计
```

实测：每次 Fast Retry 耗时约 11s，加上 start_prog 等待 15s，3 次总计约 78s 后进入持久模式。

---

## C. 安全与 Bug 审计

### C.1 已修复问题（V1.26 修复链 + V1.27.x 新增）

| # | 文件 | 问题 | 修复方案 |
|---|------|------|---------|
| 1 | `dial.c` | Fast Retry 倒计时从 `dial_loop()` 入口开始，PDP 建立前 Ping 必然失败导致误判 | 引入 `g_pdp_connected` 标志，PDP 就绪后才开始 10s Ping 窗口；独立设置 60s PDP 等待超时 |
| 2 | `dial.c ST_RECOVERY case 3` | L3 恢复代码全部注释，30min 断网后无任何操作 | 取消注释，实际执行 `AT+CFUN=1,1` 并 `exit(1)` |
| 3 | `data_call/data_call.c` | `call_id` 过滤注释 + `iptables -t filter -F` 清空全部防火墙 | 改用 `call_name` 过滤；改为循环删除指定 device 的 NAT 规则，不动 filter 表 |
| 4 | `dial.c ST_PING` | 单次 Ping 失败立即触发故障计时，偶发抖动误判 | 引入 `PING_FAIL_THRESHOLD=3`，连续失败 3 次才启动计时 |
| 5 | `dial.c ST_RECOVERY case 2` | L2 射频重置无频率限制 | 引入 `l2_count` 计数器，上限 20 次（v1.26）；v1.27 改为 `last_l2_ts` 冷却节流，移除硬上限 |
| 6 | `data_call/data_call.c` | 断开时不清理路由，重连后路由叠加 | DISCONNECTED 时按 `g_if_name` 精确删除 default 路由（包含 IPv6） |
| 7 | `dial.c dial_loop()` | `ql_data_call_init` 最长阻塞 20s 无任何日志 | 每 20 次重试打印一次进度，超时/成功均记录 |
| 8 | `data_call/data_call.c` | DISCONNECTED 未通知状态机重置 `start_fail_ts` | 设置 `g_need_reset_fail_ts=1`，状态机在 ST_PING 开头读取并重置 |
| 9 | `apn/apn.c` | `json_object_put` 误用（借用引用被 put）+ 根对象永不释放 | 移除字段读取后的 `json_object_put`；末尾正确释放根对象 |
| 10 | `nw.c` / `misc.c` | 两套网络状态写入接口并存，`nw_mark_network_status` 缺 `O_TRUNC` | 统一调用 `update_network_status()`，废弃 `nw_mark_network_status` 独立实现 |
| 11 | `misc.c` | `executeATCommand` 返回堆内存，调用方责任不明 | 改为调用方传入 buffer，函数内部填充，无需 free |
| 12 | `apn/apn.c` | `strncmp` 前缀匹配导致 ICCID 前缀重叠时命中错误 APN | 改为 `strcmp` 精确匹配 |
| 13 | `misc.c` | `SIGKILL` 不可被捕获，信号处理函数内调用 `free()`（非 async-signal-safe） | 改捕获 `SIGINT`，处理函数仅设 `volatile sig_atomic_t g_sigint_received=1` |
| 14 | `misc.c:check_process()` | `pgrep dial` 会匹配 `dialog` 等含子串的进程 | 改为 `pgrep -x dial` 精确匹配 |
| 15 | `reboot_conf/dial_reboot_conf.c` | `popen` 返回的 FILE* 用 `fclose` 关闭（应用 `pclose`）| 统一改为 `pclose` |
| 16 | `misc.c:restartNetworkServices()` | fork 后子进程成为孤儿/僵尸进程风险 | 改用双重 fork，孙进程被 init(PID=1) 收养 |
| 17 | `data_call/data_call.c` | DNS 写入含 `\r\n`（CRLF），污染 `resolv.conf` 导致 DNS 解析失败 | 改为纯 `\n`（LF） |
| 18 | `dial.c` | `dial_loop` 中 L1 在 REG 未就绪时无节流，可能猛打 `ql_data_call_stop/start` | 检查 REG 状态，非 1/5 时跳过 L1，等待 L2 |
| 19 | `dial.c` | 恢复阈值 L1=60s/L2=5min/L3=30min 过于激进，SDK 还在自愈就干预 | L0 期（0~5min）不干预；L1→5min，L2→10min，L3→35min（v1.27） |
| 20 | `dial.c` | REG=0（注册丢失）时 L1 连续失败（-1001）却无法快速切到 L2 | REG=0 跳过 L1，L0 结束后直接触发 L2，冷却从 5min 压缩为 90s（v1.27） |
| 21 | `dial.c` | SIM 状态靠 800ms AT+CPIN? 轮询，高频 popen 增加 AT 端口压力 | 改用 `ql_sim_set_card_status_cb` SDK 回调；回调未触发时降级为 AT 一次性查询（v1.27） |
| 22 | `dial.c` | SDK CP 侧服务崩溃无感知，进程悬空无法重启 | 新增 `ql_data_call_set_service_error_cb`，崩溃时记录 [FATAL] 并退出（v1.27.1） |
| 23 | `dial.c` | 心跳日志 SIM 状态仅 AT 值，无法与 SDK 回调状态交叉验证 | 心跳改为 SIM_AT/SIM_CB 双值输出（v1.27.2） |
| 24 | `dial.c` | 断网故障无诊断快照，移远分析需要但无 dmesg/logcat 数据 | 新增 `diag_snapshot()`，故障确认和恢复时写 `/media/sdcard/dial_snap/`（v1.27.3） |
| 25 | `misc.c` | AT+CGMR 返回带冗余前缀文本，固件版本日志可读性差 | 改用 `AT+QGMR`（Quectel 私有命令），返回纯版本号（v1.27） |
| 26 | `dial.c` / `apn/apn.c` | **APN profile 不匹配（回归性 bug）**：`set_apn()` 把 APN 写入 profile `DATA_CALL_APN_PUBLIC`(=6)，但 `dial_loop()` 拨号 `param_set_apn_id` 误绑 `1`，SDK 自动新建空 APN 的 profile 1，apn.json 匹配出的 APN 全程不生效（仅靠模组自动 APN 兜底，专网卡会失败）。**史实**：首次提交 `afb6936` 的 `dial.c:134` 本来就绑 `DATA_CALL_APN_PUBLIC`(=6)（与 SDK sample 一致），是 `e92733c`（三级恢复重构）重写 dial_loop 时硬编码成 `1` 造成的回归，被自动 APN 掩盖至今 | ① `apn_id` 恢复为 `DATA_CALL_APN_PUBLIC`(=6)；② 配套把 `set_apn()` 默认分支的 APN 由 `"apnpublic"` 改为空串，未匹配卡走自动 APN，避免绑 6 后强制 apnpublic 拨不通消费级卡（v1.27.6） |
| 27 | `dial.c` | `ip_ver` 实参误写为 `0`(=`QL_NET_IP_VER_MIN`)，并非 `QL_NET_IP_VER_V4`(=0x1)，注释自相矛盾；现网靠 SDK 默认兜底未暴露 | 改为显式 `QL_NET_IP_VER_V4`（v1.27.6） |
| 28 | `dial.c` | SIM 断卡路径(ST_SIM)先于 Ping 路径置位 `start_fail_ts`，导致 ST_PING 中 `if(start_fail_ts==0)` 门控的 `diag_snapshot("fault")` 永不触发——SIM 引发的断网无 dmesg/logcat 快照 | fault 快照门控由 `start_fail_ts==0` 改为 `!diag_snap_done`，每个故障周期抓一次，不论由 Ping 还是 SIM 路径触发（v1.27.6） |
| 29 | `_public.h` / `misc.c` | 隐式声明 warning：`_public.h` 只引了 SDK 的 `ql_sim.h` 未引项目 `sim.h`（`sim_init`/`sim_get_iccid` 无原型）；`misc.c` 未引 `logger_sd.h`（`dial_log` 无原型） | `_public.h` 补 `#include "sim.h"`，`misc.c` 补 `#include "logger_sd.h"`（v1.27.6） |
| 30 | `logger_sd.c` | 编译 warning：`log_init()` 中 `int ret = system("mkdir -p")` 赋值后未读（检查被注释）；`check_sdcard_available_wait_30s()` 定义但未调用 | `ret` 恢复非致命判断（失败仅打诊断日志，由 fopen 兜底降级）；备用的 30s 等待变体标 `__attribute__((unused))` 保留。至此全工程零 warning（v1.27.6） |

### C.2 潜在残留问题

| # | 严重程度 | 文件 | 问题描述 |
|---|---------|------|---------|
| P1 | 低 | `sim/sim.c` | SIM 槽位默认 `QL_SIM_SLOT_1`，不支持双卡自动切换 |
| P2 | 低 | `cc_deque/` | 完整的双端队列实现未被主流程使用（历史遗留） |
| P3 | 低 | `data_call/data_call.c` | 断开时不清 `/etc/resolv.conf`（仅删 `resolv_v4/v6.conf` 临时文件），下次重连前 `/etc/resolv.conf` 含过期 DNS |
| P4 | 低 | `dial.c` | `test_can_ping_google` 目标硬编码为 `8.8.8.8`，无法适配 Google DNS 不可达场景 |
| P5 | 低 | `dial.c` | `data_call_service_error_cb` 中 `exit(1)` 被注释，SDK 崩溃仅打日志不退出，保活无法重启 |

### C.3 内存安全

- `executeATCommand` 已改为 buffer 传入模式，无动态分配风险
- `apn_load_from_json` 中 `malloc(file_size+1)` 读文件后加 `\0`，防 `json_tokener_parse` 越界
- `apn_obj_t` 字段读取改为 `strncpy(dst, src, sizeof(dst)-1)` + 补 `\0`，防止超长字段截断

### C.4 并发安全

程序为单线程模型，SDK 回调（`data_call_status_ind_cb`）在 SDK 内部线程触发，写入 `g_pdp_connected`、`g_need_reset_fail_ts` 等全局变量时均声明为 `volatile`。主循环与回调之间无 mutex，依赖 `volatile` + 原子写语义，在目标平台（单核 ARM）上可接受。

### C.5 安全漏洞

- `system()` 中的命令字符串（路由/iptables）由程序内部构造，不含用户输入，无命令注入风险
- APN 名称来自 JSON 文件（`/usr/dial/apn.json`），文件权限应设为 root 只读；若被篡改可注入任意 APN

---

## D. IPC / 通信协议分析

### D.1 IPC 拓扑图

```
┌─────────────────────────────────────────────────────────────────────┐
│                         open_dial 进程                               │
│                                                                       │
│  main()线程（单线程）         ←───────────→   SDK 回调线程            │
│  ├─ 状态机轮询（50ms）              volatile    data_call_status_ind_cb│
│  ├─ Ping（popen 子进程）        g_pdp_connected  ql_nw 事件回调       │
│  └─ AT 命令（popen 子进程）    g_need_reset_fail_ts                   │
│                                                                       │
└──────────┬──────────────────────────────────────────────────────────┘
           │
    ┌──────┴───────────────────────────────────────────────────┐
    │                  /tmp 状态文件（IPC 机制）                 │
    │  /tmp/network_status  ←→  外部监控进程（读取联网状态）      │
    │  /tmp/dial_Status     ←→  外部监控进程                     │
    │  /tmp/network_csq     ←   nw.c（信号强度）                 │
    │  /tmp/dial_version    ←   main()（版本号）                  │
    └───────────────────────────────────────────────────────────┘
           │
    ┌──────┴────────────────────────────┐
    │         两套通信路径               │
    │                                   │
    │  SDK API 路径（主路径）            │   AT 命令路径（辅助路径）
    │  ql_data_call_start()            │   popen("serial_atcmd at+csq")
    │  ql_sim_get_iccid()              │   popen("serial_atcmd at+cfun=0/1")
    │  ql_nw_get_signal_strength()     │   popen("serial_atcmd at+cereg?")
    │  → 直接与模组驱动通信              │   → 发往 CP 串口 AT 接口
    └───────────────────────────────────┘
```

### D.2 通信协议

#### AT 命令接口（辅助路径）

通过 `popen("serial_atcmd <cmd>", "r")` 调用系统工具 `serial_atcmd` 发送 AT 命令，读取响应字符串后用字符串解析提取值。

| AT 命令 | 用途 | 响应解析 |
|---------|------|---------|
| `AT+CSQ` | 信号强度 | `+CSQ: <rssi>,<ber>`，提取 rssi（0-31，99=未知） |
| `AT+CEREG?` | 数据网络注册状态 | `+CEREG: <n>,<stat>`，提取 stat（1=已注册，5=漫游） |
| `AT+CPIN?` | SIM 卡状态（降级用） | `+CPIN: READY` 或其他字符串 |
| `AT+QIACT?` | PDP 上下文状态 | `+QIACT: <id>,<state>,<type>,<ip>` |
| `AT+CGACT?` | PDP 激活状态（恢复前快照） | `+CGACT: <cid>,<state>` |
| `AT+CEER` | 最后一次错误原因（恢复前快照） | `+CEER: <text>` |
| `AT+QGMR` | 固件版本（Quectel 私有） | 纯文本行，无 `+` 前缀 |
| `AT+QTEMP` | 模块温度（排查热关机） | `+QTEMP: "label","temp_val"` |
| `AT+CESQ` | 扩展信号质量（RSRP/SINR/RSRQ） | 扩展心跳用 |
| `AT+CREG?` | 小区信息（MCC/MNC/TAC/CID） | 扩展心跳用 |
| `AT+CGPADDR` | 当前 PDP IP 地址 | 扩展心跳用 |
| `AT+QSIMSTAT?` | SIM 硬件插入状态（诊断用） | `+QSIMSTAT: <en>,<ins>` |
| `AT+CFUN=0` | 关闭射频（飞行模式） | OK |
| `AT+CFUN=1` | 开启射频（退出飞行模式） | OK |
| `AT+CFUN=1,1` | 模组软重启（触发整 SoC 重启） | 无响应（SoC 重启） |

#### SDK API 接口（主路径）

通过 Quectel ql-sdk C 函数直接与模组驱动通信，回调通过函数指针注册。

| SDK API | 调用时机 | 返回值 |
|---------|---------|-------|
| `ql_data_call_init()` | dial_loop 初始化 | `QL_ERR_OK`(0) / `-1001`(未就绪，需重试) |
| `ql_data_call_set_status_ind_cb(cb)` | 注册连接状态回调 | — |
| `ql_data_call_create(id, name, flag)` | 创建连接实例（id=4） | — |
| `ql_data_call_start(id)` | 发起拨号（异步，立即返回） | — |
| `ql_data_call_stop(id)` | 停止拨号 | — |

#### data_call_status_ind_cb 回调消息结构（ql_data_call_status_t）

| 字段 | 类型 | 说明 |
|------|------|------|
| `call_id` | int | 数据连接 ID（SDK 重拨后可能变化） |
| `call_name` | char[] | 连接名称（如 "auto_network"，稳定不变） |
| `call_status` | 枚举 | `CONNECTED`(6) / `DISCONNECTED`(7) 等 |
| `device` | char[] | 网络接口名（如 "ccinet1"） |
| `has_addr` | bool | 是否有 IPv4 地址 |
| `addr.addr` | char[] | IPv4 地址 |
| `addr.gateway` | char[] | 默认网关 |
| `addr.dnsp/dnss` | char[] | 主/副 DNS（LF 结尾，无 CR） |
| `has_addr6` | bool | 是否有 IPv6 地址 |

### D.3 状态机完整描述

```
main() 前置状态机（线性，串行执行）：
  WAIT_ECM0 → READ_CONFIG → SIM_INIT → CSQ_READ → SINGLE_INSTANCE_CHECK
  → EXIT_COUNT_CHECK → LOG_INIT → QUICK_PING → dial_loop()

dial_loop() 主状态机（50ms 非阻塞轮询）：

  ┌─────────────────────────────────────────────────────────┐
  │ 每轮开头：快速失败检测（is_fast_fail_mode 时）            │
  │   PDP 未建立：超过 60s → exit(1)                         │
  │   PDP 已建立：超过 10s Ping 未通 → exit(1)               │
  ├─────────────────────────────────────────────────────────┤
  │ 每轮开头：心跳日志（30s 周期）                            │
  ├─────────────────────────────────────────────────────────┤
  │                                                         │
  │  ST_STATUS ──立即──→ ST_SIM                              │
  │                        │                                │
  │                     800ms 间隔                          │
  │                     AT+CPIN?                            │
  │                        │                                │
  │                      ST_SIGNAL ──透传──→ ST_PING          │
  │                                           │              │
  │                                        1500ms 间隔       │
  │                                        test_can_ping_google()│
  │                                       /         \        │
  │                                  Ping 成功     Ping 失败  │
  │                                      │              │    │
  │                                清故障计时器    ping_fail_count++ │
  │                                清 recovery_level  │    │
  │                                首次成功触发回调  ≥3次? → start_fail_ts │
  │                                清 fast retry     计算 fail_duration │
  │                                diag_snapshot     L0<30s: 不干预  │
  │                                ("recovery")      L1(>30s,REG≠0): stop→start│
  │                                      │           L2(>5.5min/REG=0>90s) │
  │                                  ST_STATUS       L3(>30.5min): exit │
  │                                             ST_RECOVERY         │
  │                                                    │            │
  │                              L1: stop→sleep(2)→start            │
  │                              L2: cfun=0→sleep(3)→cfun=1→sleep(10)→start│
  │                              L3: sync→log_close→exit(1)         │
  │                                        └──→ ST_STATUS           │
  └─────────────────────────────────────────────────────────────────┘
```

### D.4 超时 / 重试 / 错误恢复机制

| 机制 | 触发条件 | 操作 | 频率保护 |
|------|---------|------|---------|
| SDK 自动重连（L0 期） | PDP 断开（0~30s） | SDK 内部每 25s 重连，程序不干预 | L0 期内 |
| L1 软重拨 | Ping 失败 > 30s 且 REG≠0 | stop→sleep(2)→start | 60s 冷却（`last_l1_ts`） |
| L2 射频重置（正常） | Ping 失败 > 5.5min | cfun=0→sleep(3)→cfun=1→sleep(10)→start | 5min 冷却（`last_l2_ts`） |
| L2 射频重置（REG=0） | REG=0 且 Ping 失败 > 90s（`L2_REG0_FAST_TIMEOUT_MS`） | 同上（跳过 L1） | 90s 冷却（`last_l2_ts`） |
| L3 进程重启 | Ping 失败 > 30.5min | sync→log_close→exit(1)（v1.28.0 起弃用 cfun=1,1） | 无（exit 后 start_prog 重拉） |
| ~~SIM 恢复强制 L1~~ | （v1.28.3 已移除）SIM 断开/恢复仅作诊断标志 | — | — |
| SDK 崩溃检测 | ql_data_call_service_error_cb 触发 | 记录 [FATAL] 日志并退出（keepalive 重启） | 无 |
| Fast Retry | 启动时 Ping 失败 | exit(1) 让 start_prog 重拉 | 上限 3 次，后转持久模式 |
| restart_cfun | exit_count ≥ 20 | cfun=0→sleep(5)→cfun=1 | 上限 10 次 + 600s 间隔 |

---

## E. 完整文档

### E.1 架构总览

open_dial 的架构围绕**三个设计原则**展开：

1. **可靠性优先**：Ping 检测用 `popen + TTL 字符串检测`，不依赖 `system()` 返回值（规避 SIGCHLD 干扰）；进程检查用 `pgrep -x` 精确匹配。

2. **渐进式恢复**：L0（0~30s SDK 自愈）→ L1（30s 软重拨）→ L2（5.5min 射频重置，REG=0 时 90s）→ L3（30.5min 进程 exit 重拉）梯度升级（v1.28.8 阈值），每级有冷却保护，避免频繁重置损伤硬件。

3. **双重保障**：SDK 底层 25s 自动重连 + 应用层分级策略互补，`enable_policy_recovery=0` 可切换为纯 SDK 模式。

### E.2 每个源文件功能说明

| 文件 | 行数 | 功能 |
|------|------|------|
| `main.c` | 405 | 程序入口 `main()`、网卡等待、单实例检查、信号处理、CFUN 限频、版本号宏、`dial_mng_new` |
| `dial.c` | 855 | 运行期编排：主状态机、分级恢复、Fast Retry 计数、SDK/SIM 回调、恢复日志、`print_init_info`、`dial_loop` |
| `dial.h` | 52 | `dial_mng_t` 结构体、状态枚举、共享全局/函数 extern 声明 |
| `_public.h` | — | 统一包含所有依赖头文件 |
| `at/at.c` | 656 | AT 访问层：`serial_atcmd` 发 AT + 解析（CSQ/CEREG/CESQ/CREG/版本/IMEI/IMSI/温度/CEER/CGACT 等）+ `build_ext_line` 格式化（`exec_cmd_safe` 为内部 helper） |
| `at/at.h` | 37 | at.c 导出函数声明 |
| `diag/diag.c` | 134 | 故障诊断采集：`diag_snapshot`（dmesg/logcat 快照）、`setup/teardown_cp_dump_capture`（CP dump 绑定挂载） |
| `diag/diag.h` | 14 | diag.c 三个函数声明 |
| `misc.c` | 699 | 通用工具：AT 执行、进程管理、状态文件、WiFi 感知、连通性检测 `test_can_ping_google`、目录清理 `cleanup_dir_keep_newest`、`now_ms`/`get_timestamp`/`get_cpu_temp` |
| `misc.h` | — | misc 函数声明 |
| `logger_sd.c` | — | SD 卡日志：挂载检测→空间检查→时间戳文件名→双输出 |
| `logger_sd.h` | — | `dial_log()`、`log_init()`、`log_close()` 声明 |
| `apn/apn.c` | — | JSON 解析、ICCID 精确匹配、APN 配置推送给 SDK |
| `data_call/data_call.c` | 235 | SDK 状态回调、路由/NAT/DNS 配置与清理 |
| `nw/nw.c` | — | SDK 网络查询封装：信号、注册状态、RX 统计 |
| `sim/sim.c` | — | SIM 初始化、ICCID/IMSI 读取（参数化 slot）、`sim_app_state_str` 枚举映射 |
| `reboot_conf/dial_reboot_conf.c` | — | 配置持久化、前置条件检查、CFUN 限频 |
| `cc_deque/` | — | 双端队列（历史遗留，未使用） |
| `test_utils/` | — | 调试菜单框架（仅开发调试用） |

### E.3 对外 API / 接口说明

#### 进程间状态接口（/tmp 文件）

| 文件路径 | R/W | 内容 | 创建者 |
|---------|-----|------|-------|
| `/tmp/network_status` | R/W | `1`=已连通，`0`=未连通 | misc.c |
| `/tmp/network_csq` | W | 信号强度等级字符串 | nw.c |
| `/tmp/dial_Status` | W | `0`=已拨通，`1`=断开 | data_call.c |
| `/tmp/dial_version` | W | `Version: 1.27.6\r\n`（三段式格式 `%d.%02d.%d`） | main.c main() |
| `/tmp/dial_retry_count` | R/W | Fast Retry 计数（0-3） | dial.c |
| `/tmp/exit_count.txt` | R/W | 异常退出次数（≥20 触发 CFUN） | misc.c |
| `/tmp/cfun_count.txt` | R/W | restart_cfun() 调用次数（上限 10） | dial.c |
| `/tmp/cfun_last_call.txt` | R/W | 上次 CFUN 调用的 uptime（秒） | dial.c |
| `/tmp/dial_reboot.conf` | R/W | `uptime,restart_flag` | reboot_conf/ |
| `/tmp/resolv_v4.conf` | W | IPv4 DNS（拨通后生成，断开后删除） | data_call.c |
| `/tmp/resolv_v6.conf` | W | IPv6 DNS（同上） | data_call.c |
| `/tmp/sdcard_avl` | W | SD 卡不可用时写 `"000"` | logger_sd.c |
| `/etc/resolv.conf` | W | 系统 DNS（从 resolv_v4/v6.conf 合并） | data_call.c |
| `/usr/dial/apn.json` | R | APN 数据库 | apn/apn.c |
| `/media/sdcard/dial_log/` | W | SD 卡运行日志目录（每次启动新建 `dial_<ts>.log`；启动时按总大小预算 `LOG_DIR_BUDGET_MB=64` 回收最旧文件） | logger_sd.c |
| `/media/sdcard/dial_snap/` | W | 诊断快照目录（`dmesg_<label>_<ts>.log`、`logcat_<label>_<ts>.log`，label=fault/recovery，故障/恢复时写入，保留最新 40 个） | dial.c `diag_snapshot()` |
| `/proc/uptime` | R | 系统运行时间 | reboot_conf/, dial.c |
| `/proc/mounts` | R | 挂载点（SD 卡检测） | logger_sd.c |
| `/sys/.../net/<if>/statistics/rx_packets` | R | 网卡 RX 统计 | nw.c |

#### SDK API 完整调用清单

**ql_data_call 系列**

| API | 调用位置 | 说明 |
|-----|---------|------|
| `ql_data_call_init()` | dial_loop() | 最多重试 200 次（-1001 时每次 100ms 间隔） |
| `ql_data_call_set_status_ind_cb()` | dial_loop() | 注册状态变化回调 |
| `ql_data_call_set_service_error_cb()` | dial_loop() | 注册 SDK CP 侧崩溃回调（v1.27.1） |
| `ql_data_call_create(id=4, name, flag)` | dial_loop() | 创建数据连接实例 |
| `ql_data_call_param_alloc/set_*/config/free()` | dial_loop() | 配置 APN ID=`DATA_CALL_APN_PUBLIC`(=6)、IP 版本=`QL_NET_IP_VER_V4`(=1)、重连间隔=25s（v1.27.6 修正，详见 C.1 #26/#27） |
| `ql_data_call_start(id)` | dial_loop(), ST_RECOVERY | 发起拨号（异步） |
| `ql_data_call_stop(id)` | ST_RECOVERY L1 | 停止拨号 |
| `ql_data_call_set_apn_config(id, cfg)` | apn/apn.c | 配置 APN 名称/账号/密码 |

**ql_sim 系列**

| API | 调用位置 | 说明 |
|-----|---------|------|
| `ql_sim_init()` | sim/sim.c, reboot_conf/ | 初始化 SIM 服务 |
| `ql_sim_set_card_status_cb(cb)` | dial_loop() | 注册 SIM 状态变化回调，替代 AT+CPIN? 轮询（v1.27） |
| `ql_sim_get_card_info(slot, &info)` | dial_loop() | 主动获取初始 SIM 状态（回调仅变化时触发） |
| `ql_sim_get_iccid(slot, iccid, len)` | sim/sim.c | 读取 ICCID（20 位） |
| `ql_sim_get_imsi(slot, type, imsi, len)` | sim/sim.c | 读取 IMSI（15 位） |

**ql_nw 系列**

| API | 调用位置 | 说明 |
|-----|---------|------|
| `ql_nw_get_signal_strength()` | nw.c | 多制式信号强度（含 5G NR） |
| `ql_nw_get_data_reg_status()` | nw.c | 数据网络注册状态 |

### E.4 关键流程时序图

#### 冷启动正常拨号时序

```
保活脚本启动 dial
    │
    ▼ (max 30s)
等待 ecm0 网卡（SIOCGIFINDEX 每秒轮询）
    │ 超时 30s → exit(-1)，保活脚本重拉
    ▼
read_config / sim_init / get_csq / 单实例检查 / SIGCHLD 注册
    │ exit_count ≥ 20 → restart_cfun()
    ▼
log_init（SD 卡检测→ dial_YYYYMMDD_HHMMSS.log）
    │ SD 卡不可用 → 降级为控制台
    ▼
test_can_ping_google（已联网？）
    ├─ 是 → 保活循环（每 6s ping，10 次失败后退出）
    └─ 否 → dial_loop()
              │
              ▼ (max 20s)
         ql_data_call_init（重试 200 次，每 2s 打印进度）
              │
              ▼ 注册回调
         ql_data_call_set_status_ind_cb / set_service_error_cb
         ql_sim_set_card_status_cb + ql_sim_get_card_info（获取初始状态）
              │
              ▼
         sim_get_iccid → set_apn（JSON 匹配 + SDK 配置）
              │
              ▼
         ql_data_call_create / config / start（异步）
              │
              ▼ SDK 回调
         data_call_status_ind_cb: CONNECTED
              ├─ g_pdp_connected = 1
              ├─ ip route add default via <gw> dev <dev>
              ├─ iptables -t nat -A POSTROUTING -o <dev> -j MASQUERADE
              └─ 写 resolv.conf / /tmp/resolv_v4.conf
              │
              ▼ 主循环检测到 g_pdp_connected=1
         "[INIT] PDP connected, start Ping window (10000ms)."
              │
              ▼ (< 10s)
         test_can_ping_google 成功
              │
              ▼
         on_network_connected() 触发、clear_retry_count()
         进入持久监控模式（永不超时退出）
```

#### 三级恢复时序（v1.27+）

```
Ping 首次失败 (t=0)
    │ ping_fail_count++ 未到 3 → 等待
    │ ping_fail_count ≥ 3 → start_fail_ts = t
    │
    ├─ t=0~30s: [L0] SDK 自动重连期，不干预（v1.28.8：由 5min 压缩）
    │   └─ 网络恢复 → "[EVENT] Network Recovered in SDK phase (L0)"
    │
    ├─ t=30s:   [ALARM] L1 recovery（REG≠0）
    │   └─ stop→sleep(2)→start；60s 内不重复触发（last_l1_ts）
    │   └─ REG=0 → 跳过 L1，30s~90s 窗口内不动作，等瞬时掉注册自愈
    │
    ├─ t=90s (REG=0，L2_REG0_FAST_TIMEOUT_MS) / t=5.5min (正常): [ALARM] L2 recovery
    │   └─ cfun=0→sleep(3)→cfun=1→sleep(10)→start
    │   └─ REG=0 冷却 90s，正常路径冷却 5min（last_l2_ts）
    │
    └─ t=30.5min: [ALARM] L3 recovery
        └─ sync→log_close→exit(1)→start_prog 重拉（v1.28.0 起弃用 cfun=1,1）
```

#### Fast Retry 时序

```
dial 启动（attempt 1/3）
    ├─ PDP 建立 < 60s → Ping 窗口 10s
    │     ├─ Ping 通 → 持久模式（成功）
    │     └─ Ping 超时 → exit(1)
    └─ PDP 未建立 > 60s → exit(1)

start_prog 等待 15s 后重拉（attempt 2/3）
...（同上）
attempt 3/3 后 → 进入持久循环（再不 exit）
```

### E.5 已知问题与改进建议

#### 短期改进

1. **Ping 目标可配置**：`8.8.8.8` 硬编码，无法适配 Google DNS 不可达场景，建议从配置文件读取，同时 Ping 多目标（如 `114.114.114.114`）。

2. **断开后清 resolv.conf**：当前仅删 `/tmp/resolv_v4/v6.conf`，不清 `/etc/resolv.conf`，断网期间 DNS 解析仍用过期服务器。建议 DISCONNECTED 时也清空 `/etc/resolv.conf`。

3. **L1/L2 中的 sleep 阻塞**：`sleep(3)`、`sleep(10)` 阻塞主线程（包括心跳日志）。可改用状态机计时替代阻塞 sleep。

4. **SDK 崩溃退出后仍被注释**：`data_call_service_error_cb` 中 `log_close()` / `exit(1)` 被注释，崩溃仅打日志不退出；待确认 SDK 自身是否会 kill 本进程后再决定是否启用。

#### 长期架构改进

1. **双 SIM 支持**：参数化 `QL_SIM_SLOT_E`，支持 slot 轮换。
2. **健康状态 JSON 导出**：将 `start_fail_ts`、`recovery_level`、Ping 成功率等写入状态文件，供外部监控读取。
3. **IPv6 优先策略**：梳理 v4/v6 路由设置优先级和 DNS 合并顺序。

### E.6 编译与部署

#### 编译

```bash
cd open_dial/
make clean
make  # 默认 aarch64-linux-gnu-gcc
# 或指定工具链
make CC=aarch64-linux-gnu-gcc
```

#### 部署检查清单

```bash
# 1. 确认 ecm0 网卡加载
ip link show ecm0

# 2. 确认 SDK 服务运行
pgrep ql_rild && pgrep ql_netd

# 3. 确认 serial_atcmd 可用
serial_atcmd at  # 应返回 OK

# 4. 创建 APN 配置（ICCID 必须完整，精确匹配）
mkdir -p /usr/dial
cat > /usr/dial/apn.json << 'EOF'
{
  "apn": [
    {"supplier":"移动","iccid":"89860012345678901234","apn":"cmnet","usrname":"","pwd":""},
    {"supplier":"联通","iccid":"89860112345678901234","apn":"3gnet","usrname":"","pwd":""}
  ]
}
EOF

# 5. 运行（由保活脚本管理）
/usr/bin/dial

# 6. 监控
cat /tmp/network_status         # 1=已连接
cat /tmp/dial_version           # Version: 1.27.6
tail -f /media/sdcard/dial_log/dial_*.log
ping -I ccinet1 -c 4 8.8.8.8
```

#### 保活脚本示例

```bash
#!/bin/sh
# /etc/init.d/start_prog (或等效)
while true; do
    /usr/bin/dial      # Fast Retry 前 3 次快速退出，第 4 次进持久模式
    EXIT_CODE=$?
    echo "dial exited ($EXIT_CODE), restarting in 15s..."
    sleep 15           # 与 Fast Retry 的 15s 等待对齐
done
```

---

## F. 测试方案

> 覆盖范围：`df06b4d` 起（含）的全部 BUGFIX 提交  
> 适用硬件：EC200A 模组 + 目标单板（**eSIM 设备，所有"断卡"场景用 `at+cfun=4` 替代**）  

### F.1 受测提交清单

**V1.26 修复链（已验收）**

| Commit | 说明 | 关键文件 |
|--------|------|---------|
| `7e0648d` | 修复 SIM UNKNOWN 状态下 L2/L3 恢复永不触发 | `dial.c` |
| `8c331aa` | 补提 dial 文件 | — |
| `6f7c1be` | 修复 persistent 模式下成功重连后 retry_count 未清零 | `dial.c` |
| `6c02b01` | 修复 Fast Retry 超时误判、call_id 过滤丢事件 | `dial.c`、`data_call/data_call.c` |
| `72888c3` | 路由清理精确化、补 IPv6 路由清理、统一 ip route | `data_call/data_call.c` |
| `ffea5dd` | 修复断网重连后路由残留导致必须重启系统 | `data_call/data_call.c` |
| `df06b4d` | 漏洞修复合集（APN、信号、抖动过滤、L1/L3、DNS 等） | 12 个文件 |

**V1.27.x 新功能（待完整回归）**

| Commit | 说明 | 关键文件 |
|--------|------|---------|
| `3ff9f9e` | REG=0 时跳过 L1 直接 L2（90s 冷却），缩短注册丢失恢复时长 | `dial.c` |
| `939dbb4` | L0 期 5min 不干预；L1→5min，L2→10min，L3→35min；SIM 状态改 SDK 回调 | `dial.c` |
| `90d0ab0` | 新增 `ql_data_call_set_service_error_cb`，诊断 SDK 崩溃 | `dial.c` |
| `d9bb1d7` | 心跳 SIM 状态改为双值输出（SIM_AT + SIM_CB） | `dial.c` |
| 未提交 | 新增 `diag_snapshot()`，故障/恢复时保存 dmesg+logcat | `dial.c` |

**重要**：V1.27.x 引入阈值变化，C-09 等涉及恢复等级的用例必须用最新 dial 重跑。

### F.2 测试前准备

```bash
# 烧录前基线快照
mkdir -p /tmp/before_upgrade
date > /tmp/before_upgrade/ts.txt
ip route show > /tmp/before_upgrade/route4.txt
ip -6 route show > /tmp/before_upgrade/route6.txt
iptables-save > /tmp/before_upgrade/iptables.txt
cp /etc/resolv.conf /tmp/before_upgrade/resolv.conf
ps > /tmp/before_upgrade/ps.txt
md5sum /usr/dial/dial > /tmp/before_upgrade/md5.txt
tar czf /media/sdcard/before_upgrade_$(date +%Y%m%d_%H%M).tar.gz -C /tmp before_upgrade

# 测试中常用观察命令
ip route show && ip -6 route show
iptables -t nat -L POSTROUTING -n -v
iptables -t filter -L -n -v
cat /tmp/dial_Status /tmp/network_status /tmp/dial_retry_count
cat /etc/resolv.conf
tail -F /media/sdcard/dial_log/*.log
```

### F.3 通用观察点（每个用例必检）

1. 能不能上网？`ping -I <wwan_if> 8.8.8.8 -c 3`
2. 路由干净？`ip route` 中只有一条 default via wwan*
3. NAT 规则干净？`iptables -t nat -L POSTROUTING` 只有一条 MASQUERADE
4. filter 表没被误清？原有自定义规则仍存在
5. DNS 正确？`/etc/resolv.conf` 与 `resolv_v4/v6.conf` 一致，无 `^M` 字符
6. 日志完整？有 `[INIT]` 系列日志，无 20s 静默

### F.4 核心测试用例

#### A 组：路由残留修复

**A-01 上电首次拨号**
- 断电 ≥ 30s → 上电 → 等 60s
- 预期：`ip route` 只有 1 条 default，NAT 1 条 MASQUERADE，ping 通

**A-02 软断网 → 恢复（30s）** ⭐ 核心 Bug 验证场景
```bash
serial_atcmd at+cfun=4 && sleep 30 && serial_atcmd at+cfun=1
```
- 预期：重连后路由仍只有 1 条，**不需要重启系统**即可 ping 通

**A-03 软断网 → 恢复（v1.28.8 起 L1 阈值为 30s，观察窗口可缩短）**
- 预期：最终必须拨通，路由/NAT 干净

**A-04 连续拔插 5 次（10s 断 / 20s 通）**
```bash
for i in 1 2 3 4 5; do
    serial_atcmd at+cfun=4; sleep 10
    serial_atcmd at+cfun=1; sleep 20
    echo "=== Round $i ===" && ip route show | grep default
    iptables -t nat -L POSTROUTING -n --line-numbers | grep MASQUERADE
done
```
- 预期：每轮 default 路由恒为 1 条（或断开态的 0 条），不累积

#### B 组：路由清理精确化

**B-01 filter 自定义规则不被清空**
```bash
iptables -I INPUT -p icmp -m comment --comment "TEST_B01" -j ACCEPT
serial_atcmd at+cfun=4; sleep 30; serial_atcmd at+cfun=1
iptables -t filter -L INPUT -n --line-numbers | grep TEST_B01
```
- 预期：`TEST_B01` 规则仍然存在

**B-02 其他网卡路由不被误删**（若有 eth0/br0）
- 预期：eth0 的 default 路由未被触动

**B-03 IPv6 路由清理**（仅 IPv6 APN 下有效）
- 断网后 `ip -6 route show default` 应为空

#### C 组：Fast Retry + call_id 修复

**C-01 init 进度日志**
- 预期：冷启动后看到 `[INIT] Waiting for ql_data_call_init...` → `data_call_init OK.`，无 20s 静默

**C-02 Fast Retry 正常拨通路径**
- 预期：`[INIT] Fast Retry Mode: Attempt 1/3` → `PDP connected, start Ping window` → 10s 内 ping 通 → 进持久模式

**C-03 Fast Retry PDP 建不起来（模拟无信号）**
```bash
serial_atcmd at+cfun=4
rm -f /tmp/dial_retry_count && killall dial
```
- 预期：60s 后 `Fast Fail: PDP not established in 60s`，连续 3 次后 `Entering persistent loop mode`

**C-04 Fast Retry PDP 建起但 Ping 不通**
```bash
iptables -I OUTPUT -p icmp -j DROP && iptables -I FORWARD -p icmp -j DROP
rm -f /tmp/dial_retry_count && killall dial
```
- 预期：`PDP connected, start Ping window (10000ms).` → 10s 后 `Fast Fail: PDP up but no Ping success`
- 清理：`iptables -D OUTPUT -p icmp -j DROP; iptables -D FORWARD -p icmp -j DROP`

**C-05 call_name 替代 call_id 过滤**
- 对比两次拨通的 call_id（可能不同），但两次均正常触发路由/NAT 设置

**C-07 MASQUERADE 循环清理**
```bash
# 注入 3 条重复规则
for i in 1 2 3; do iptables -t nat -A POSTROUTING -o ccinet1 -j MASQUERADE; done
serial_atcmd at+cfun=4; sleep 30; serial_atcmd at+cfun=1
iptables -t nat -L POSTROUTING -n   # 应只剩 1 条
```

**C-08 DNS 临时文件清理**
```bash
serial_atcmd at+cfun=4  # 等 DISCONNECTED
ls -la /tmp/resolv_v*.conf  # 应报 No such file
```

**C-09 三级恢复 L1/L2/L3 联合回归** ⭐⭐ 最重要用例

> 必须用 iptables DROP 而非 cfun=4：cfun=4 会使 REG=0，导致 L1 被跳过，且进程不稳定，start_fail_ts 不断归零，L3 永远无法触发。

```bash
# 1. 确认已拨通，无 dial_retry_count
ping -c 3 8.8.8.8
cat /tmp/dial_retry_count  # No such file

# 2. 阻断出站流量（保持 REG=1）
iptables -I OUTPUT -o ccinet1 -j DROP

# 3. 持续观察日志（约 30 分钟）
tail -f /media/sdcard/dial_log/*.log

# 4. L3 触发后清除规则
iptables -D OUTPUT -o ccinet1 -j DROP
```

预期时序（v1.28.8 阈值）：
- ~0~30s：`[L0]` SDK 自动重连期，无 [ALARM] 日志
- ~30s：`[ALARM] L1 recovery` → `Stopping Data Call` → `Restarting Data Call`（REG=1，不跳过）
- L1 每 ~60s 重复触发（直到 5.5min）
- ~5.5min：`[ALARM] L2 recovery` → `Toggling RF (Airplane Mode)`
- L2 每 ~5min 重复触发（直到 30.5min）
- ~30.5min：`[ALARM] L3 recovery` → `Network down ... secs` → sync → exit(1) → start_prog 重拉（v1.28.0 起不再 cfun=1,1）
- 清除 iptables 后新进程拨通
- （若制造 REG=0 场景：~90s 即触发 L2，`L2_REG0_FAST_TIMEOUT_MS=90s`，冷却 90s）

历史实测（v1.26，2026-04-21）：L1×5（62s/63s/124s/185s/246s）、L2×5（547s/847s/1148s/1448s/1749s）、L3×1（1800s）。  
**注意**：阈值已多次变更（v1.27: 5/10/35min → v1.28.8: 30s/5.5min/30.5min），C-09 需用当前版 dial 重跑才有效。

#### D 组：漏洞修复合集

**D-01 APN 精确匹配**：日志中命中的 apn_name 与 SIM ICCID **完全相同**的条目

**D-02 APN JSON 健壮性**：
- 正常 JSON → 拨通
- 畸形 JSON（语法错误）→ 进程不崩，用默认 APN
- 超长字段 → 截断后 printf 无乱码

**D-03 SIGINT 安全退出**
```bash
kill -INT $(pgrep -x dial)
# 预期：日志出现 "[EVENT] SIGINT received"，进程干净退出（非 coredump）
```

**D-04 Ping 抖动过滤**：单次/两次 ping 失败后 30s 内无 `[ALARM]` 日志

**D-07 DNS 换行符**
```bash
cat -A /etc/resolv.conf   # 行尾应为 $ 而非 ^M$
nslookup www.baidu.com    # 应能解析
```

### F.5 回归基线（时间紧张时只跑这 6 个）

| 用例 | 验证点 |
|------|-------|
| A-01 | 冷启动整链路通、日志完整 |
| A-02 | 核心路由 Bug 不回归 |
| C-02 | Fast Retry 两阶段计时正常 |
| C-07 | MASQUERADE 循环清理生效 |
| D-03 | SIGINT 安全退出不崩 |
| D-07 | DNS resolv.conf 无 ^M + nslookup 通 |

### F.6 Pass/Fail 总结模板

| 用例 | 结果 | 日志文件 | 备注 |
|------|------|---------|------|
| A-01 | ☐ PASS ☐ FAIL ☐ N/A | | 上电首次拨通 |
| A-02 | ☐ PASS ☐ FAIL ☐ N/A | | 软断 30s 重连，旧路由清理 |
| A-03 | ☐ PASS ☐ FAIL ☐ N/A | | 软断 5min 重连 |
| A-04 | ☐ PASS ☐ FAIL ☐ N/A | | 连续拔插 5 次无累积 |
| B-01 | ☐ PASS ☐ FAIL ☐ N/A | | filter 规则不被清 |
| B-02 | ☐ PASS ☐ FAIL ☐ N/A | | 其他网卡路由不误删 |
| B-03 | ☐ PASS ☐ FAIL ☐ N/A | | IPv6 路由清理 |
| C-01 | ☐ PASS ☐ FAIL ☐ N/A | | init 进度日志 |
| C-02 | ☐ PASS ☐ FAIL ☐ N/A | | Fast Retry 正常路径 |
| C-03 | ☐ PASS ☐ FAIL ☐ N/A | | PDP_WAIT 60s 超时 |
| C-04 | ☐ PASS ☐ FAIL ☐ N/A | | Ping 窗口 10s 超时 |
| C-05 | ☐ PASS ☐ FAIL ☐ N/A | | call_name 过滤 |
| C-07 | ☐ PASS ☐ FAIL ☐ N/A | | MASQUERADE 循环清理 |
| C-08 | ☐ PASS ☐ FAIL ☐ N/A | | DNS 临时文件清理 |
| C-09 | ✅ PASS（v1.26）/ 待重测（v1.27 阈值已变） | `dial_20260421_*.log` | 原：L1×5/L2×5/L3×1；v1.27 阈值变为 5/10/35min，需重跑 |
| D-01 | ☐ PASS ☐ FAIL ☐ N/A | | APN 精确匹配 |
| D-02 | ☐ PASS ☐ FAIL ☐ N/A | | APN JSON 健壮性 |
| D-03 | ☐ PASS ☐ FAIL ☐ N/A | | SIGINT 安全退出 |
| D-04 | ☐ PASS ☐ FAIL ☐ N/A | | Ping 抖动过滤 |
| D-05 | ✅ PASS | (见 C-09) | L1 REG=1 时正常执行 |
| D-06 | ✅ PASS | (见 C-09) | L3 真发 AT+CFUN=1,1 |
| D-07 | ☐ PASS ☐ FAIL ☐ N/A | | DNS 换行符修复 |

**验收标准**：A 组、C 组全部 PASS；B-01 必须 PASS；D-02/D-03/D-04/D-07 必须 PASS；D-05/D-06 通过 C-09 确认；其余允许以代码 review 替代真机验证。

### F.7 出问题时的排查抓手

| 现象 | 第一优先级检查 |
|------|-------------|
| 重连后 ping 不通 | `ip route` 是否有 default；`iptables -t nat` 是否有 MASQUERADE |
| 日志长时间静默 | `ps` 进程是否还在；`/tmp/dial_Status` 值；SD 卡是否满 |
| Fast Retry 无限退出拉起 | `AT+CSQ` 信号强度；`AT+CPIN?` SIM 状态 |
| resolv.conf 里混了旧 DNS | `ls -la /tmp/resolv_v*.conf` 时间戳；`cat -A /etc/resolv.conf` |
| 第二块网卡断网 | `ip route | grep default` 是否被误删（应按 dev 精确操作） |
| L3 触发但模组未重启 | 确认是 `AT+CFUN=1,1`（硬重启）而非 `AT+CFUN=0/1`（软重置） |

---

*本文档整合了 `open_dial_源码分析.md`、`open_dial_源码分析_详细版.md`、`open_dial_问题分析与修复方案.md`、`TEST_PLAN.md` 四份文档，基于 V1.26 全部源码及实际设备日志（C-09 真机测试于 2026-04-21）生成。*
