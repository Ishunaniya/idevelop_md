# open_dial 完整源码技术分析文档

> 生成时间：2026-05-27  
> 代码版本：V1.27.5（MAIN=1, SUB=27, PATCH=5）  
> 目标平台：Quectel EC200A / EG25（OpenCPU 模式，aarch64 嵌入式 Linux）  
> 分析范围：全部 C 源文件（dial.c / data_call.c / apn.c / sim.c / nw.c / logger_sd.c / misc.c / dial_reboot_conf.c / cc_deque/ / test_utils/）

---

## 目录

- [第一章：项目总览](#第一章项目总览)
- [第二章：头文件分析](#第二章头文件分析)
- [第三章：dial.c 完整分析](#第三章dialc-完整分析)
- [第四章：data_call 模块分析](#第四章data_call-模块分析)
- [第五章：APN 模块分析](#第五章apn-模块分析)
- [第六章：SIM 模块分析](#第六章sim-模块分析)
- [第七章：网络状态模块分析](#第七章网络状态模块分析)
- [第八章：SD卡日志模块分析](#第八章sd卡日志模块分析)
- [第九章：misc 工具函数库分析](#第九章misc-工具函数库分析)
- [第十章：持久化配置模块分析](#第十章持久化配置模块分析)
- [第十一章：cc_deque 双端队列分析](#第十一章cc_deque-双端队列分析)
- [第十二章：IPC 完整清单](#第十二章ipc-完整清单)
- [第十三章：并发安全分析](#第十三章并发安全分析)
- [第十四章：内存安全审计](#第十四章内存安全审计)
- [第十五章：已知 Bug 与修复历史](#第十五章已知-bug-与修复历史)
- [第十六章：改进建议](#第十六章改进建议)
- [第十七章：冷启动完整时序图](#第十七章冷启动完整时序图)
- [第十八章：关键流程时序图](#第十八章关键流程时序图)

---

## 第一章：项目总览

### 1.1 项目定位

`open_dial` 是运行在 Quectel 4G 模组（EC200A/EG25）嵌入式 Linux 系统上的**蜂窝网络自动拨号守护进程**。核心使命是确保蜂窝数据连接的持续可用性——在信号波动、PDP 异常、模组内部错误等各种条件下，自动检测并分级恢复网络连接。

### 1.2 硬件背景

- **SoC**：ASR1803（高通架构变种），单芯片集成 AP + CP
  - AP（Cortex-A7）：运行嵌入式 Linux，执行 `dial` 进程
  - CP（Cortex-R5）：运行蜂窝协议栈（LTE/5G 基带）
- **关键约束**：`AT+CFUN=1,1` 复位 CP 等于整颗 SoC 复位，Linux 必然跟着重启；因此 L3 恢复（`exit(1)`）会让保活脚本重拉进程，而非真正意义上的"模组重启"——在 EC200A 上其实是系统级重启。
- **目标设备**：eSIM 设备（无物理 SIM 槽位），测试时"断卡"用 `AT+CFUN=4` 替代

### 1.3 目录结构与文件清单

```
open_dial/
├── dial.c                        # ★ 主入口 + 核心状态机（约1200+行）
├── dial.h                        # dial_mng_t结构体、dial_stat_enu枚举
├── _public.h                     # 统一头文件汇总
├── misc.c                        # 通用工具函数（AT命令/进程管理/状态文件）
├── misc.h
├── logger_sd.c                   # SD卡日志（自动降级控制台）
├── logger_sd.h
├── Makefile                      # 交叉编译配置（aarch64-linux-gnu-gcc）
├── README.md                     # 版本历史
│
├── apn/
│   ├── apn.c                     # JSON格式APN数据库加载 + ICCID精确匹配
│   └── apn.h
│
├── data_call/
│   ├── data_call.c               # 数据连接状态回调 + 路由/DNS/NAT配置（235行）
│   └── data_call.h
│
├── nw/
│   ├── nw.c                      # 网络状态查询（信号/注册/RX流量统计）
│   └── nw.h
│
├── sim/
│   ├── sim.c                     # SIM卡初始化 + ICCID/IMSI读取
│   └── sim.h
│
├── reboot_conf/
│   ├── dial_reboot_conf.c        # 启动配置持久化 + 前置条件检测
│   └── dial_reboot_conf.h
│
├── cc_deque/                     # 双端队列实现（历史遗留，未被主流程使用）
│   ├── cc_deque.c
│   ├── cc_deque.h
│   ├── cc_common.c
│   └── cc_common.h
│
└── test_utils/                   # 调试菜单框架
    ├── test_utils.c
    └── test_utils.h
```

### 1.4 模块依赖 ASCII 图

```
dial.c（核心入口/状态机/分级恢复）
  │
  ├─── misc.c            AT命令执行、进程检测、网络状态文件写入
  │
  ├─── logger_sd.c       日志初始化和写入（SD卡 + 控制台双通道）
  │
  ├─── reboot_conf/      读写启动配置(/tmp/dial_reboot.conf)、CFUN限频保护
  │
  ├─── apn/apn.c         APN数据库加载(/usr/dial/apn.json) + ICCID精确匹配
  │
  ├─── sim/sim.c         SIM初始化(ql_sim_init)、ICCID读取
  │
  ├─── data_call/        连接状态回调处理、路由/NAT/DNS 配置与清理
  │
  ├─── nw/nw.c           信号强度、注册状态、RX统计
  │
  └─── cc_deque/         双端队列（当前未使用，历史遗留）
```

### 1.5 技术栈

| 类别 | 内容 |
|------|------|
| 编译器 | `aarch64-linux-gnu-gcc`（Quectel SDK 工具链交叉编译） |
| SDK | Quectel ql-sdk（`ql_data_call`、`ql_sim`、`ql_nw` 等头文件） |
| 第三方库 | `json-c`（APN JSON 解析）、`liblog`（Android-style log）、`libuci`、`libubox` |
| 外部工具 | `serial_atcmd`、`ping`、`ip`、`iptables`、`pgrep`、`logcat`、`dmesg` |
| 标准库 | POSIX（`ioctl`、`statvfs`、`sigaction`、`popen`、`fork`） |
| 内置数据结构 | `cc_deque`（双端队列，当前未使用） |

### 1.6 编译配置详解

**Makefile 关键点**：

```makefile
# SDK 配置引入（含工具链路径、CFLAGS、LDFLAGS）
-include /home/tronlong/lyp/SDK/EC200A/ql-ol-extsdk-ec200acntar02a04m2g_ocpu/sdk.mk

# 目标可执行文件名
DIAL_TARGET_EXE = dial

# 链接额外库（json解析、日志、UCI配置、ubox工具库）
-ljson-c -llog -lprop2uci -luci -lubox

# 源文件：递归查找当前目录所有.c文件（包含cc_deque、test_utils）
APP_SRC_FILES += $(shell find $(SRC_DIR)/ -name '*.c')

# 编译后自动拷贝到调试输出目录
cp dial ~/output_debug
```

**注意**：`find $(SRC_DIR)/ -name '*.c'` 会递归包含所有子目录的 .c 文件，包括 `cc_deque/`（虽然主流程未调用其函数，但代码仍被编译进二进制）。

### 1.7 版本号

- 当前版本：`V1.27.5`（MAIN=1, SUB=27, PATCH=5）
- 版本号格式：写入 `/tmp/dial_version`，格式为 `Version: 1.27.05\r\n`（三段式，PATCH 补零到两位）

---

## 第二章：头文件分析

### 2.1 dial.h — 核心类型定义

#### 宏定义

| 宏名 | 值 | 含义 |
|------|----|------|
| `DIAL_TIMEOUT_SECONDS` | 120 | 历史遗留超时常量（主流程已不直接使用） |
| `DILA_IFNAME_TIMEOUT_CNT` | 10 | 历史遗留，接口名等待计数 |
| `EXIT_COUNT_FILE` | `"/tmp/exit_count.txt"` | 异常退出计数文件路径 |
| `DODIAL_TIMEOUT_SECONDS` | 60 | PDP 建立超时（Fast Retry 阶段用），单位：秒 |

#### dial_stat_enu 枚举——历史状态机状态

| 枚举值 | 整数值 | 含义 |
|--------|-------|------|
| `dial_stat_none` | 0 | 初始/未知状态 |
| `dial_stat_call_init` | 1 | 调用 ql_data_call_init() 阶段 |
| `dial_stat_apn_init` | 2 | APN 初始化阶段 |
| `dial_stat_call_create` | 3 | 数据连接实例创建阶段 |
| `dial_stat_call_start` | 4 | 发起拨号阶段 |
| `dial_stat_stop` | 5 | 停止拨号 |
| `dial_stat_stop_cfun` | 6 | CFUN=0（关射频） |
| `dial_stat_start_cfun` | 7 | CFUN=1（开射频） |
| `dial_stat_wait_for_connect` | 8 | 等待连接建立 |
| `dial_stat_net_connected` | 9 | 网络已连接 |

> **注意**：此枚举为历史遗留，当前版本 `dial.c` 使用独立的 `enum Stage`（`ST_STATUS/ST_SIM/ST_SIGNAL/ST_PING/ST_RECOVERY`）替代，`dial_stat_enu` 主要保留在 `dial_mng_t` 结构体中。

#### dial_mng_t 结构体——历史管理结构体

```c
typedef struct {
    dial_stat_enu dial_st;          // 拨号状态（dial_stat_enu枚举）
    QL_NW_SIGNAL_STRENGTH_LEVEL_E level; // 信号强度等级（SDK枚举）
    // 以下字段已注释（历史遗留，不使用）：
    // struct ifaddrs *p_ifaddrs;   // 网络接口地址列表指针
    // uint64_t u64_if_rx_packets;  // 接口RX包计数
    // uint8_t rsv[2];              // 保留字节（对齐用）
    // char nw_node_name[NAME_MAX]; // 网络节点名（如 ccinet1）
    // int profile_idx;             // APN profile索引
    // sim_mng_t *p_sim_mng;        // SIM管理结构体指针
    struct timespec dial_timer;     // 拨号计时器（CLOCK_MONOTONIC）
} dial_mng_t;
```

**字段含义**：

| 字段 | 类型 | 含义 | 当前使用状态 |
|------|------|------|------------|
| `dial_st` | `dial_stat_enu` | 当前拨号状态枚举值 | 历史遗留，主流程不依赖 |
| `level` | `QL_NW_SIGNAL_STRENGTH_LEVEL_E` | 信号强度等级（NONE/POOR/MODERATE/GOOD/GREAT） | 历史遗留 |
| `dial_timer` | `struct timespec` | 计时器（用于超时检测） | 历史遗留 |

#### 外部声明

```c
extern dial_mng_t *dial_mng_new(void);  // 分配并初始化 dial_mng_t（历史遗留）
extern void *dial_task(void *arg);       // 历史线程函数（当前未使用）
int read_exit_count();                   // 读取 /tmp/exit_count.txt
void write_exit_count(int count);        // 写入 /tmp/exit_count.txt
```

### 2.2 _public.h — 统一头文件汇总

`_public.h` 是全局统一包含头文件，所有 .c 文件通过 `#include "_public.h"` 获得完整依赖。

#### 包含的标准库

| 头文件 | 功能 |
|--------|------|
| `<stdio.h>` | 标准I/O（printf/popen/fopen等） |
| `<stdarg.h>` | 可变参数（va_list，供dial_log使用） |
| `<sys/types.h>` | 基础类型（pid_t, off_t等）（包含两次，冗余） |
| `<dirent.h>` | 目录操作（opendir/readdir等） |
| `<unistd.h>` | POSIX API（fork/sleep/getpid等） |
| `<string.h>` | 字符串操作（strcmp/strncpy/memset等） |
| `<stdlib.h>` | 通用函数（malloc/free/exit/system等） |
| `<getopt.h>` | 命令行参数解析 |
| `<stdint.h>` | 固定宽度整型（uint64_t等） |
| `<errno.h>` | 错误码（errno全局变量） |
| `<fcntl.h>` | 文件控制（open/O_RDONLY等） |
| `<sys/stat.h>` | 文件状态（stat结构体） |
| `<signal.h>` | 信号处理（sigaction/SIGINT等） |

#### 包含的 Quectel SDK 头文件

| 头文件 | 提供的API |
|--------|----------|
| `ql-sdk/ql_type.h` | Quectel基础类型定义 |
| `ql-sdk/ql_data_call.h` | 数据连接管理API（ql_data_call_init等） |
| `ql-sdk/ql_sim.h` | SIM卡操作API（ql_sim_init/get_iccid等） |
| `ql-sdk/ql_nw.h` | 网络状态API（ql_nw_get_signal_strength等） |

#### 包含的项目内部头文件

| 头文件 | 说明 |
|--------|------|
| `dial.h` | 主结构体和枚举定义 |
| `apn.h` | APN模块接口 |
| `nw.h` | 网络状态模块接口 |
| `data_call.h` | 数据连接模块接口 |
| `test_utils.h` | 调试菜单接口 |

#### 全局宏定义

| 宏名 | 值 | 说明 |
|------|----|------|
| `LOG_TAG` | `"DIAL"` | Android-style log标签 |
| `LOG_NDEBUG` | 1 | 关闭DEBUG级别日志 |
| `DATA_CALL_APN_PUBLIC` | 6 | 默认APN标识号（历史遗留，当前主流程用ID=4） |
| `DATA_CALL_ID_PUBLIC` | 1 | 默认数据连接ID（历史遗留） |
| `APN_NAME_PUBLIC` | `"apnpublic"` | 默认APN名称（ICCID无匹配时回退） |

---

## 第三章：dial.c 完整分析

### 3.1 文件概述

`dial.c` 是整个项目的核心文件，承载以下全部职责：
- 程序入口（`main()`）
- 启动前置检查（ecm0 等待、单实例、快速连接检测）
- CP Dump 捕获（`diag_snapshot()`）
- 主状态机（`dial_loop()`）
- Fast Retry 机制
- 分级恢复逻辑（L0/L1/L2/L3）
- 心跳日志（30s 基础 + 5min 扩展）
- SIGINT 信号处理

### 3.2 宏定义完整表

| 宏名 | 值 | 类型 | 说明 |
|------|----|------|------|
| `PING_INTERVAL_MS` | 1500 | 时间(ms) | Ping检测间隔 |
| `SIM_INTERVAL_MS` | 800 | 时间(ms) | SIM状态检测间隔 |
| `SIG_INTERVAL_MS` | 800 | 时间(ms) | 信号检测间隔（当前透传）|
| `HEARTBEAT_INTERVAL_MS` | 30000 | 时间(ms) | 基础心跳日志间隔（30s） |
| `LEVEL1_TIMEOUT` | 300000 | 时间(ms) | L0结束/L1触发阈值（5min） |
| `LEVEL2_TIMEOUT` | 600000 | 时间(ms) | L2触发阈值（10min） |
| `LEVEL3_TIMEOUT` | 2100000 | 时间(ms) | L3触发阈值（35min） |
| `FAST_FAIL_TIMEOUT_MS` | 10000 | 时间(ms) | PDP就绪后Ping超时窗口（10s） |
| `PDP_WAIT_TIMEOUT_MS` | 60000 | 时间(ms) | PDP建立最长等待时间（60s） |
| `PING_FAIL_THRESHOLD` | 3 | 次数 | 连续失败次数才触发故障计时 |
| `VERSION_MAIN` | 1 | 版本 | 主版本号 |
| `VERSION_SUB` | 27 | 版本 | 次版本号 |
| `VERSION_PATCH` | 4 | 版本 | 补丁版本号 |

### 3.3 全局变量完整表

| 变量名 | 类型 | 初值 | 说明 | 写入方 | 读取方 |
|--------|------|------|------|--------|--------|
| `g_pdp_connected` | `volatile int` | 0 | PDP建立状态标志 | SDK回调线程（data_call.c） | 主循环 |
| `g_need_reset_fail_ts` | `volatile int` | 0 | 请求重置故障时间戳 | SDK回调线程（DISCONNECTED事件） | ST_PING开头 |
| `g_sigint_received` | `volatile int` | 0 | SIGINT信号标志 | 信号处理函数 | 主循环开头 |
| `g_sdk_service_error` | `volatile int` | 0 | SDK CP侧崩溃标志 | data_call_service_error_cb | 主循环开头 |
| `g_sim_app_ready` | `volatile int` | -1 | SIM就绪状态（-1=未初始化,0=非READY,1=READY） | sim_card_status_cb | ST_SIM / 心跳 |
| `g_sim_app_state` | `volatile int` | 0 | QL_SIM_APP_STATE_E整数值（供日志用） | sim_card_status_cb | 心跳 |
| `g_last_reg_stat` | `int` | -1 | 最近一次REG状态，供恢复逻辑判断 | ST_SIM | ST_RECOVERY |
| `g_if_name` | `char[64]` | "" | 当前数据连接网络接口名（如ccinet1） | data_call_status_ind_cb | 心跳/扩展心跳 |
| `enable_policy_recovery` | `int` | 1 | 0=禁用应用层恢复，仅靠SDK重连 | 无（硬编码） | ST_RECOVERY |

### 3.4 状态枚举

```c
enum Stage {
    ST_STATUS   = 0,  // 流转起点，立即无延迟跳转到ST_SIM
    ST_SIM      = 1,  // SIM卡状态检查：优先读SDK回调缓存值g_sim_app_ready，
                      // 无回调时降级为serial_atcmd at+cpin?，间隔800ms
    ST_SIGNAL   = 2,  // 信号强度检测（当前版本透传到ST_PING），间隔800ms
    ST_PING     = 3,  // Ping 8.8.8.8验证连通性，间隔1500ms
    ST_RECOVERY = 4   // 执行分级恢复操作（L1/L2/L3）
};
```

### 3.5 函数详细说明

#### 3.5.1 `get_uptime_ms()`

```c
uint64_t get_uptime_ms(void)
```

- **参数**：无
- **返回值**：系统开机以来的毫秒数（`uint64_t`）
- **逻辑**：
  1. 调用 `clock_gettime(CLOCK_MONOTONIC, &ts)` 获取单调时钟
  2. 返回 `ts.tv_sec * 1000 + ts.tv_nsec / 1000000`
- **用途**：所有超时计算的基础时间源（状态机间隔、故障持续时间等）

#### 3.5.2 `sigint_handler()`

```c
static void sigint_handler(int sig)
```

- **参数**：`sig` — 信号编号（SIGINT=2）
- **返回值**：无
- **逻辑**：仅将 `g_sigint_received = 1`（async-signal-safe）
- **安全性**：不调用任何非 async-signal-safe 函数，符合 POSIX 信号处理规范

#### 3.5.3 `diag_snapshot()`

```c
static void diag_snapshot(const char *label)
```

- **参数**：`label` — 快照标签（如 `"fault"` 或 `"recovery"`）
- **返回值**：无
- **逻辑**：
  1. 构造时间戳字符串（`YYYYMMDD_HHMMSS` 格式，通过 `localtime()` + `strftime()`）
  2. 创建目录 `/media/sdcard/diag/`（`mkdir -p`，通过 `system()`）
  3. 执行 `dmesg > /media/sdcard/diag/dmesg_<label>_<timestamp>.log`
  4. 执行 `logcat -d > /media/sdcard/diag/logcat_<label>_<timestamp>.log`
  5. 打印 `[DIAG] snapshot saved` 日志
- **用途**：在断网故障确认（`diag_snap_done==0`）和网络恢复时各保存一份诊断快照，供移远（Quectel）分析
- **控制变量**：每次故障周期内仅在 `ping_fail_count >= PING_FAIL_THRESHOLD` 且 `diag_snap_done==0` 时触发一次，恢复时再触发一次

#### 3.5.4 `test_can_ping_google()`

```c
static int test_can_ping_google(const char *iface)
```

- **参数**：`iface` — 网络接口名（NULL 表示不指定 `-I` 参数）
- **返回值**：1 = 可达，0 = 不可达
- **逻辑**：
  1. 构造命令：`ping -c 1 -W 3 [-I <iface>] 8.8.8.8`
  2. `popen()` 执行命令，读取输出
  3. 在输出中查找 `"ttl="` 字符串（不依赖返回码，防 SIGCHLD 干扰）
  4. `pclose()` 关闭管道
- **注意**：Ping 目标 `8.8.8.8` 硬编码，无法适配 Google DNS 不可达场景

#### 3.5.5 `wait_for_interface()`

```c
static int wait_for_interface(const char *ifname, int timeout_sec)
```

- **参数**：
  - `ifname` — 接口名（如 `"ecm0"`）
  - `timeout_sec` — 超时秒数
- **返回值**：0 = 成功，-1 = 超时
- **逻辑**：
  1. 创建 UDP socket（`AF_INET, SOCK_DGRAM`）
  2. 每秒调用 `ioctl(sock, SIOCGIFINDEX, &ifr)` 检查接口是否存在
  3. 成功返回 0，超时返回 -1（`exit(-1)` 触发保活脚本重拉）
- **用途**：确保 `ecm0`（USB ECM 接口，Quectel 模组 USB 网卡）已就绪才进行后续初始化

#### 3.5.6 `on_network_connected()`

```c
static void on_network_connected(void *ctx)
```

- **参数**：`ctx` — 用户上下文指针（当前传 NULL）
- **返回值**：无
- **逻辑**：
  1. 清除 Fast Retry 计数文件（`clear_retry_count()`）
  2. 写版本号到 `/tmp/dial_version`（格式：`Version: 1.27.04\r\n`）
  3. 打印 `[EVENT] Network Connected` 日志
- **触发时机**：首次 Ping 成功（`has_notified_connect==0`）时从主循环调用

#### 3.5.7 `main()`

```c
int main(int argc, char *argv[])
```

**启动序列（12步）**：

1. **等待 ecm0**：`wait_for_interface("ecm0", 30)` → 超时30s则 `exit(-1)`
2. **读取配置**：`read_config()` + `check_sim_status()` + `get_signal_csq()`
3. **单实例检查**：`check_pid_running(-1)`（`pgrep -x dial`），避免重复运行
4. **退出计数检查**：`read_exit_count()`，≥20 次调用 `restart_cfun()`（CFUN 保护）
5. **日志初始化**：`log_init()`（SD 卡挂载检测 + 空间检查）
6. **版本号写入**：写 `/tmp/dial_version`
7. **SIGINT 注册**：`sigaction(SIGINT, &sa, NULL)`（处理函数设 `g_sigint_received`）
8. **SIGCHLD 注册**：设置 `SA_NOCLDWAIT`，防止 `popen` 子进程产生僵尸进程
9. **快速连接检测**：`test_can_ping_google(NULL)` → 已联网则进入保活循环（每6s ping，10次失败后 exit）
10. **进入主循环**：`dial_loop(on_network_connected, NULL)`

#### 3.5.8 `dial_loop()`

```c
static void dial_loop(void (*on_connected_cb)(void *ctx), void *ctx)
```

**参数**：
- `on_connected_cb` — 首次连接成功回调函数指针
- `ctx` — 回调上下文

**初始化阶段**（进入 while 循环前）：

```
1. ql_data_call_init() 重试循环（最多200次，每100ms一次，每20次打印进度）
2. ql_data_call_set_status_ind_cb(data_call_status_ind_cb)   ← 注册连接状态回调
3. ql_data_call_set_service_error_cb(data_call_service_error_cb) ← 注册SDK崩溃回调
4. sim_init() + sim_get_iccid(iccid, QL_SIM_SLOT_1)
5. ql_sim_set_card_status_cb(sim_card_status_cb)             ← 注册SIM状态变化回调
6. ql_sim_get_card_info(QL_SIM_SLOT_1, &card_info)           ← 获取初始SIM状态
7. set_apn(iccid)                                             ← APN匹配+SDK配置
8. ql_data_call_create(4, "auto_network", 0)
9. ql_data_call_param_alloc() + 配置APN-ID/IP版本/重连间隔
10. ql_data_call_config(4, param)
11. ql_data_call_param_free(param)
12. ql_data_call_start(4)                                     ← 异步发起拨号
```

**主循环（50ms 轮询）**：

每轮依次执行：

```
A. SIGINT 检测：g_sigint_received → log + log_close() + exit(0)
B. SDK崩溃检测：g_sdk_service_error → [FATAL]日志 + exit(1)
C. 快速失败检测（is_fast_fail_mode=1时）：
   - PDP未建立超60s → "Fast Fail: PDP not established" → exit(1)
   - PDP建立后超10s Ping未通 → "Fast Fail: PDP up but no Ping" → exit(1)
D. 30s心跳日志（见3.7节）
E. 状态机流转（见3.6节）
```

#### 3.5.9 `data_call_service_error_cb()`

```c
static void data_call_service_error_cb(int error_code)
```

- **参数**：`error_code` — SDK 传入的错误码
- **返回值**：无
- **逻辑**：
  1. 打印 `[FATAL] ql_data_call service error: <error_code>` 日志
  2. 设置 `g_sdk_service_error = 1`（主循环下一轮检测后退出）
  3. （注：`exit(1)` 调用被注释，因尚未确认 SDK 自身是否会 kill 本进程）

#### 3.5.10 `sim_card_status_cb()`

```c
static void sim_card_status_cb(QL_SIM_SLOT_E slot, QL_SIM_APP_STATE_E app_state)
```

- **参数**：
  - `slot` — SIM 槽位（QL_SIM_SLOT_1）
  - `app_state` — SIM 应用状态枚举
- **逻辑**：
  1. 将 `app_state` 整数值写入 `g_sim_app_state`（供心跳日志打印原始值）
  2. 判断 `app_state == QL_SIM_APP_STATE_READY`：是则 `g_sim_app_ready = 1`，否则 `g_sim_app_ready = 0`

### 3.6 状态机完整分析

```
每轮 50ms 的轮询周期：

ST_STATUS (0)
  │ 无延迟
  ▼
ST_SIM (1)
  │ 检查距上次 SIM 检测是否 >= 800ms（last_sim_ts）
  │ ├─ 时间未到 → 跳过，不切换
  │ └─ 时间到 → 执行SIM检测：
  │      1. 若 g_sim_app_ready != -1（回调已初始化）→ 直接读 g_sim_app_ready
  │      2. 否则 → serial_atcmd at+cpin? 解析返回值
  │      SIM_READY → g_last_reg_stat = nw_get_data_reg_status()
  │      SIM非READY → 记录状态，sim_error_active=1
  │      SIM恢复READY且已过L0期 → 触发强制L1（跳到ST_RECOVERY后继续）
  │
  ▼ stage = ST_SIGNAL
ST_SIGNAL (2)
  │ 检查距上次信号检测是否 >= 800ms（last_sig_ts）
  │ 当前版本：直接透传（无实际操作）
  ▼
ST_PING (3)
  │ 检查距上次 Ping 是否 >= 1500ms（last_ping_ts）
  │
  │ 前置：若 g_need_reset_fail_ts==1 → 重置 start_fail_ts=0、ping_fail_count=0
  │         recovery_level=0、diag_snap_done=0（DISCONNECTED 事件触发）
  │
  │ 若 is_fast_fail_mode 且 PDP 已建立 → 在此执行 Ping（10s 窗口）
  │
  │ 调用 test_can_ping_google(g_if_name) 或 test_can_ping_google(NULL)
  │
  │ ── Ping 成功路径 ──────────────────────────────────────
  │   ping_fail_count = 0
  │   若 start_fail_ts != 0 → 计算恢复耗时，打印 [EVENT] Network Recovered
  │                            diag_snapshot("recovery")
  │   start_fail_ts = 0，recovery_level = 0，diag_snap_done = 0
  │   若 !has_notified_connect → on_network_connected()，has_notified_connect=1
  │   stage = ST_STATUS
  │
  │ ── Ping 失败路径 ──────────────────────────────────────
  │   ping_fail_count++
  │   若 ping_fail_count < PING_FAIL_THRESHOLD(3) → stage=ST_STATUS（等待累积）
  │   ping_fail_count >= 3：
  │     若 start_fail_ts == 0 → start_fail_ts = now（记录首次故障时刻）
  │                              diag_snapshot("fault")，diag_snap_done=1
  │     stage = ST_RECOVERY
  │
  ▼
ST_RECOVERY (4)
  │ 计算 fail_duration = now - start_fail_ts（故障持续时长）
  │
  │ ── L0 期（< LEVEL1_TIMEOUT = 5min）──────────────────
  │   不执行任何恢复操作，打印 [L0] 日志，SDK 自动重连
  │   stage = ST_STATUS
  │
  │ ── L1（>= 5min，REG≠0，recovery_level < 1）──────────
  │   检查 last_l1_ts 冷却（60s）
  │   REG=0 时跳过L1，打印跳过原因
  │   执行：ql_data_call_stop(4)
  │          sleep(2)
  │          ql_data_call_start(4)
  │   last_l1_ts = now，recovery_level = 1（若 >= L2_TIMEOUT 则已置2，不退回）
  │   stage = ST_STATUS
  │
  │ ── L2（>= 10min，或 REG=0 且 >= 5min）────────────────
  │   检查 last_l2_ts 冷却（正常5min，REG=0时90s）
  │   执行：serial_atcmd at+cfun=0
  │          sleep(3)
  │          serial_atcmd at+cfun=1
  │          sleep(10)
  │          ql_data_call_start(4)
  │   last_l2_ts = now，recovery_level = 2
  │   stage = ST_STATUS
  │
  └── L3（>= 35min）────────────────────────────────────────
      执行：serial_atcmd at+cfun=1,1（SoC全复位）
             sleep(10)
             log_close()
             exit(1) ← start_prog/保活脚本重拉
```

### 3.7 心跳日志分析

#### 基础心跳（每 30s）

输出内容（典型格式）：
```
[HB] UP=xxx REG=x CSQ=xx TEMP=xx SIM_AT=READY/UNKNOWN SIM_CB=x(x) DownTime=xxxxxms IF=ccinet1
```

| 字段 | 来源 | 说明 |
|------|------|------|
| `UP` | `/proc/uptime` | 系统开机时长（秒） |
| `REG` | `nw_get_data_reg_status()` | 数据网络注册状态（1=已注册,5=漫游） |
| `CSQ` | `serial_atcmd at+csq` | 信号强度（0-31,99=未知） |
| `TEMP` | `serial_atcmd at+qtemp` | 模块温度（排查热关机） |
| `SIM_AT` | `serial_atcmd at+cpin?` | SIM状态AT查询值（READY/NOT INSERTED等） |
| `SIM_CB` | `g_sim_app_ready(g_sim_app_state)` | SIM状态SDK回调缓存值（双值交叉验证，v1.27.2新增） |
| `DownTime` | `start_fail_ts` | 当前断网持续时长（0=已连接） |
| `IF` | `g_if_name` | 当前数据连接接口名 |

#### 扩展心跳（每 5min，联网后启动）

```
[HB-EXT] CESQ=... CELL=MCC/MNC/TAC/CID IP=x.x.x.x
```

| 字段 | AT命令 | 说明 |
|------|--------|------|
| `CESQ` | `AT+CESQ` | 扩展信号质量（RSRP/SINR/RSRQ，更精确） |
| `CELL` | `AT+CREG?` | 小区信息（MCC/MNC/TAC/CID，用于定位分析） |
| `IP` | `AT+CGPADDR` | 当前PDP IP地址 |

### 3.8 Fast Retry 机制详解

```
/tmp/dial_retry_count 不存在（或值=0）
  │ is_fast_fail_mode = true
  │ 第1次尝试（Attempt 1/3）
  │
  ├─ PDP等待：每50ms轮询g_pdp_connected，超过PDP_WAIT_TIMEOUT_MS(60s)
  │   → [FAIL] Fast Fail: PDP not established in 60s → exit(1)
  │
  ├─ PDP已建立（g_pdp_connected=1）：
  │   记录pdp_start_ts，进入Ping窗口（FAST_FAIL_TIMEOUT_MS=10s）
  │   → 10s内Ping通 → on_network_connected() → 清retry_count → 持久模式
  │   → 10s超时 → [FAIL] Fast Fail: PDP up but no Ping → exit(1)
  │
start_prog等待15s后重拉（第2次，retry_count=1）
  │ ...同上...
  │
第3次（retry_count=2）...同上...

retry_count >= 3：
  is_fast_fail_mode = false
  进入持久循环（never exit on timeout）
```

### 3.9 分级恢复时间轴

```
断网持续时间（v1.27+阈值）：
0 ────── 5min ──────── 10min ───────────────────── 35min
         │               │                          │
         L0 结束         L2 触发                   L3 触发
         L1 开始         射频重置                   模组重启
         │               │                          │
     stop/start      cfun=0→cfun=1              cfun=1,1
     60s冷却         5min冷却(正常)             exit(1)
     REG=0时跳过     90s冷却(REG=0)

L0（0~5min）：
  SDK reconnect_interval=25s 自动重连，程序不干预
  恢复则记录 "[EVENT] Network Recovered in SDK phase (L0)"

L1（5~10min，REG≠0）：
  每60s执行：ql_data_call_stop(4) → sleep(2) → ql_data_call_start(4)
  REG=0时跳过，直接进入L2（因start返回-1001无效）

L2（>10min 或 REG=0且>5min）：
  正常路径冷却5min：at+cfun=0 → sleep(3) → at+cfun=1 → sleep(10) → start
  REG=0路径冷却90s：同上操作但更快触发

L3（>35min）：
  at+cfun=1,1（SoC全复位）→ sleep(10) → exit(1)
  保活脚本重拉新进程，重新计时
```

---


---

## 第四章：data_call/data_call.c — 连接状态回调完整分析

### 4.1 模块职责

该模块是 SDK 连接状态事件的处理中心，负责：
- 监听 PDP 连接/断开状态变化
- 连接成功时配置系统路由、NAT、DNS
- 断开时精确清理上述配置，防止路由残留
- 维护全局 PDP 状态标志供主循环使用

### 4.2 全局变量

| 变量 | 类型 | 初始值 | 说明 |
|------|------|--------|------|
| `g_callid` | `int` | `DATA_CALL_ID_PUBLIC(1)` | 历史遗留，当前已不用 call_id 过滤 |
| `g_if_name` | `char[32]` | `{0}` | 当前连接的网卡名（如 ccinet1），断开时清零 |
| `g_pdp_connected` | `volatile int` | `0` | PDP 建立标志（1=已连接），由回调写入，主循环读取 |
| `dial_status[]` | `const char*[]` | 字符串数组 | 状态码到字符串的映射表（0-9） |

### 4.3 get_dial_status_msg(errcode)

将 SDK 状态码（0-9）映射为可读字符串，用于日志打印。超出范围返回 `"Unknown status"`。

### 4.4 data_call_status_ind_cb() — 核心回调完整分析

**函数签名**：`void data_call_status_ind_cb(int call_id, QL_NET_DATA_CALL_STATUS_E pre_call_status, ql_data_call_status_t *p_msg)`

**过滤逻辑**：
```c
if (p_msg == NULL || strcmp(p_msg->call_name, DIAL_CALL_NAME) != 0) return;
```
使用 `call_name`（"auto_network"）而非 `call_id` 过滤，原因：SDK 重拨后 `call_id` 会变化，但 `call_name` 在整个进程生命周期内保持稳定。

**① CONNECTED 路径（`QL_NET_DATA_CALL_STATUS_CONNECTED`）**：

```
1. g_pdp_connected = 1           → 通知 dial_loop PDP 已建立
2. system("echo 0 > /tmp/dial_Status")
3. update_network_status(1)      → 写 /tmp/network_status=1
4. strncpy(g_if_name, p_msg->device, 31)  → 记录网卡名

5. 防御性预清理（应对崩溃/重启后路由残留）：
   ip route del default dev <device> 2>/dev/null
   ip -6 route del default dev <device> 2>/dev/null
   循环删除旧 NAT：
     for (k=0; k<10 && system("iptables -t nat -D POSTROUTING -o <dev> -j MASQUERADE")==0; k++)

6. IPv4（has_addr=true）：
   - ip route add default via <gateway> dev <device>
   - iptables -t nat -A POSTROUTING -o <device> -j MASQUERADE
   - 写 /tmp/resolv_v4.conf：
       fprintf(fp, "nameserver %s\n", dnsp);  ← 纯 \n，无 \r
       fprintf(fp, "nameserver %s\n", dnss);
   - echo "" > /etc/resolv.conf
   - cat /tmp/resolv_v4.conf >> /etc/resolv.conf
   - 若 /tmp/resolv_v6.conf 存在：cat >> /etc/resolv.conf

7. IPv6（has_addr6=true）：
   - ip -6 route add default via <gw6> dev <device>
   - 写 /tmp/resolv_v6.conf（同上格式）
   - 重新合并 /etc/resolv.conf（先清空，再依次追加 v4/v6）
```

**② IDLE（was CONNECTED）路径**（`QL_NET_DATA_CALL_STATUS_IDLE` 且 `pre=CONNECTED`）：

AT+CFUN=0/4 关闭射频时，SDK 发 CONNECTED→IDLE 而非 DISCONNECTED。需同样清理：
```
g_pdp_connected = 0
echo 1 > /tmp/dial_Status
update_network_status(0)
ip route del default dev g_if_name
ip -6 route del default dev g_if_name
循环删 MASQUERADE 规则（同上）
g_if_name[0] = '\0'
unlink /tmp/resolv_v4.conf
unlink /tmp/resolv_v6.conf
```

**③ DISCONNECTED 路径**（`QL_NET_DATA_CALL_STATUS_DISCONNECTED`）：

```
g_pdp_connected = 0
echo 1 > /tmp/dial_Status
update_network_status(0)
if g_if_name 非空：
    ip route del default dev g_if_name（精确按接口名，不误删其他网卡路由）
    ip -6 route del default dev g_if_name
    循环删 iptables MASQUERADE 规则（上限 10 次防死循环）
    g_if_name[0] = '\0'
unlink /tmp/resolv_v4.conf  → 清理 DNS 临时文件，防止下次重连时混入过期 DNS
unlink /tmp/resolv_v6.conf
```

**注意**：v1.27 版本移除了 `g_need_reset_fail_ts` 字段（CLAUDE.md 中有记录，但当前代码中 DISCONNECTED 路径未见该字段写入），主循环通过 `g_pdp_connected` 变化间接感知断线。

### 4.5 flow_monitor_task() — 历史遗留，未使用

**功能**：监控网卡 RX 流量，120 秒内无收包则返回 `dial_stat_stop`。

**关键逻辑**：
```c
nw_get_if_statistics_rx_packets(&if_rx_packets, g_if_name)
若 rx_packets 有增长 → 重置计时器 dial_timer
diff(dial_timer, cur_timer).tv_sec > DIAL_TIMEOUT_SECONDS(120) → return dial_stat_stop
否则 usleep(4900ms) → return dial_stat_net_connected
```

**未使用原因**：早期设计通过 RX 流量判断网络质量，后改为 Ping 检测方案，更可靠且可感知单向断流。


---

## 第五章：apn/apn.c — APN 自适应模块完整分析

### 5.1 模块职责

从 `/usr/dial/apn.json` 加载运营商 APN 数据库，通过 SIM 卡 ICCID **精确匹配**正确的 APN 配置，并通过 SDK API 写入模组。

### 5.2 apn_obj_t 结构体

```c
typedef struct {
    char iccid[32];     // SIM ICCID（精确匹配键）
    char apn[32];       // APN 名称（如 "cmnet"）
    char usr_name[32];  // 用户名（可为空）
    char pwd[32];       // 密码（可为空）
} apn_obj_t;
```

### 5.3 apn.json 文件格式

```json
{
  "apn": [
    { "supplier": "中国移动", "iccid": "898600XXXXXXXXXXXXXXX",
      "apn": "cmnet", "usrname": "", "pwd": "" },
    { "supplier": "中国联通", "iccid": "898601XXXXXXXXXXXXXXX",
      "apn": "3gnet",  "usrname": "", "pwd": "" },
    { "supplier": "中国电信", "iccid": "898603XXXXXXXXXXXXXXX",
      "apn": "ctnet",  "usrname": "", "pwd": "" }
  ]
}
```

文件路径：`/usr/dial/apn.json`（由 `APN_JSON_NAME` 宏定义）。

### 5.4 apn_load_from_json() 完整分析

**参数**：`char *json_path`（JSON 文件路径），`int *p_apn_count`（输出：解析到的 APN 条数）

**返回值**：`apn_obj_t*` 数组指针（调用方负责 free），失败返回 NULL

**完整流程**：

```
1. fopen(json_path, "r")          → 打开文件
2. fseek(fp, 0, SEEK_END)         → 定位到文件尾
3. len = ftell(fp)                → 获取文件大小
4. rewind(fp)                     → 回到文件头
5. str = malloc(len + 1)          → +1 为 '\0' 预留空间（防止 json_tokener_parse 越界读）
6. fread(str, 1, len, fp)         → 读取全部内容
7. str[len] = '\0'                → 强制 null 结尾
8. obj = json_tokener_parse(str)  → 解析 JSON 根对象

9. json_object_object_get_ex(obj, "apn", &json_root)  → 获取 "apn" 数组
10. json_len = json_object_array_length(json_root)
11. p_apn_obj = malloc(sizeof(apn_obj_t) * json_len)  → 分配结果数组

12. 遍历每个元素（json_object_array_get_idx）：
    - 读取 "supplier"（仅注释，不存入结构体）
    - 读取 "apn"：json_object_object_get_ex → json_object_get_string
      → strncpy(p_apn_obj[i].apn, str, sizeof-1) + '\0'  ← 截断保护
    - 读取 "iccid"：同上写入 p_apn_obj[i].iccid
    - 读取 "usrname"：写入 p_apn_obj[i].usr_name
    - 读取 "pwd"：写入 p_apn_obj[i].pwd
    注意：json_object_object_get_ex 返回借用引用（borrowed reference），
         不得调用 json_object_put()，否则会导致 double-free 崩溃（v1.26 已修复）

13. json_object_put(obj)          → 正确释放根对象（修复内存泄漏）
14. free(str); fclose(fp)
15. return p_apn_obj
```

### 5.5 匹配逻辑演进

| 版本 | 匹配方式 | 问题 |
|------|----------|------|
| 老版本 | `strncmp(iccid, sim_iccid, len)` | 前缀相同时（如89860X）命中错误 APN |
| v1.26+ | `strcmp(iccid, sim_iccid)` | 精确完整匹配，无误匹配风险 |

### 5.6 set_apn() 完整逻辑

**参数**：`char *iccid`（当前 SIM 的完整 ICCID）

```
1. sprintf(json_path, "/usr/dial/%s", APN_JSON_NAME)
2. p_apn_obj = apn_load_from_json(json_path, &apn_obj_count)
3. if (apn_obj_count > 0 && p_apn_obj != NULL):
   for i in range(apn_obj_count):
     if strcmp(p_apn_obj[i].iccid, iccid) == 0:  ← 精确匹配
       填充 apn_cfg（apn_name/username/password/ip_ver=IPv4）
       break
   
4. 若找到匹配（i != apn_obj_count）：
   ql_data_call_set_apn_config(DATA_CALL_APN_PUBLIC=6, &apn_cfg)
   成功则 free(p_apn_obj) + return QL_ERR_OK

5. 若未找到匹配（i == apn_obj_count / apn_obj_count==0 / p_apn_obj==NULL）：
   使用默认 APN：
   apn_cfg.apn_name = APN_NAME_PUBLIC = "apnpublic"
   apn_cfg.ip_ver = QL_NET_IP_VER_V4
   ql_data_call_set_apn_config(DATA_CALL_APN_PUBLIC=6, &apn_cfg)

6. free(p_apn_obj); return ret
```

**注意**：`DATA_CALL_APN_PUBLIC = 6`（在 `_public.h` 定义），`DATA_CALL_ID_PUBLIC = 1`，两者不同，前者是 APN slot ID，后者是 call ID。


---

## 第六章：sim/sim.c — SIM 卡模块完整分析

### 6.1 模块职责

对 Quectel `ql_sim` SDK API 的薄封装，仅在程序启动时调用一次，用于获取 ICCID 供 APN 匹配使用。运行时 SIM 状态监控已改由 SDK 回调（`ql_sim_set_card_status_cb`）+ 主动查询（`ql_sim_get_card_info`）替代。

### 6.2 sim_init()

```c
int sim_init(void)
```
- 调用 `ql_sim_init()`
- 成功返回 `QL_ERR_OK`，失败返回 `-1`
- 仅打印日志，无重试逻辑（调用方 dial_loop() 在失败时 sleep(2) 后继续）

### 6.3 sim_deinit()

```c
int sim_deinit(void)
```
- 调用 `ql_sim_deinit()`，当前主流程未使用（清理时直接 exit）

### 6.4 sim_get_iccid()

```c
int sim_get_iccid(char *iccid)
```
- 硬编码 `slot = QL_SIM_SLOT_1`（不支持双卡切换，P1 潜在问题）
- 调用 `ql_sim_get_iccid(QL_SIM_SLOT_1, iccid, QL_SIM_ICCID_LENGTH+1)`
- ICCID 长度：`QL_SIM_ICCID_LENGTH`（通常为 20 位）
- 成功返回 `QL_ERR_OK`，失败返回 `-1`

### 6.5 sim_get_imsi()

```c
int sim_get_imsi(char *imsi)
```
- 同样硬编码 `QL_SIM_SLOT_1`
- `app_type` 硬编码为 `QL_SIM_APP_TYPE_UNKNOWN`（input=0 分支）
- 调用 `ql_sim_get_imsi(QL_SIM_SLOT_1, QL_SIM_APP_TYPE_UNKNOWN, imsi, QL_SIM_IMSI_LENGTH+1)`
- 实际在 dial.c 中已改用 AT 命令 `get_imsi_at_safe()`（AT+CIMI），更轻量

### 6.6 与 dial.c 的配合关系

```
启动阶段（一次性）：
  sim_init() → ql_sim_init()
  sim_get_iccid(iccid) → ql_sim_get_iccid()  → 供 set_apn() 使用

运行时 SIM 状态监控（替代 AT+CPIN? 轮询）：
  ql_sim_set_card_status_cb(sim_card_status_cb)   → SDK 变化时回调
  ql_sim_get_card_info(QL_SIM_SLOT_1, &card_info) → 主动获取初始状态
  → 写入 g_sim_app_ready / g_sim_app_state
  → ST_SIM 阶段读取标志，只有回调未触发时才降级为 AT+CPIN?
```

### 6.7 已知限制

- **单卡限制**：`QL_SIM_SLOT_1` 硬编码，无法自动切换到 Slot 2
- **无错误重试**：`sim_init` 失败后仅 sleep(2)，不会重试直到成功
- **IMSI 获取**：SDK 版本已不用，改用 AT 命令，减少对 ql_sim API 的依赖


---

## 第七章：nw/nw.c — 网络状态查询完整分析

### 7.1 模块概述

nw.c 是 Quectel ql_nw SDK 的完整封装层（1749 行），包含大量 SDK 测试代码（test_utils 菜单驱动）。真正被主流程调用的只有文件末尾几个函数。

### 7.2 被主流程实际调用的函数

#### get_signal_strength(QL_NW_SIGNAL_STRENGTH_LEVEL_E *level)

- 调用 `ql_nw_get_signal_strength(&info, level)` 获取多制式信号强度
- 调用 `internal_nw_get_signal_strength_level(*level, level_info, ...)` 将枚举转为字符串
- 执行 `echo <level_str> > /tmp/network_csq` 写入信号等级文件
- 返回 0 成功，-1 失败

**信号强度等级映射表**：

| SDK 枚举 | 写入字符串 |
|----------|-----------|
| `QL_NW_SIGNAL_STRENGTH_LEVEL_NONE` | `UNKNOWN` |
| `QL_NW_SIGNAL_STRENGTH_LEVEL_POOR` | `POOR` |
| `QL_NW_SIGNAL_STRENGTH_LEVEL_MODERATE` | `MODERATE` |
| `QL_NW_SIGNAL_STRENGTH_LEVEL_GOOD` | `GOOD` |
| `QL_NW_SIGNAL_STRENGTH_LEVEL_GREAT` | `GREAT` |

#### nw_get_if_statistics_rx_packets(uint64_t *p_rx_packets, char *if_name)

- 读取 `/sys/class/net/<if_name>/statistics/rx_packets`（`NW_IF_STATISTICS_RX_PACKETS_PATH` 宏）
- open → lseek(SEEK_END) 获取大小 → lseek(SEEK_SET) 回头 → read → strtoull 转换
- 成功返回 `true`，失败（文件不存在或大小异常）返回 `false`
- 被 `flow_monitor_task()` 调用（历史遗留）

#### nw_get_data_reg_status()

- 调用 `ql_nw_get_data_reg_status(&t_info)` 获取数据网络注册状态
- 打印 tech_domain、radio_tech、reg_state、MCC/MNC、CID、TAC 等详细信息
- 主流程中已改用 AT 命令 `get_cereg_status_safe()` 替代，更轻量

#### nw_mark_network_status(int net_status)（已废弃）

- 使用 `open(O_WRONLY|O_CREAT|O_TRUNC)` + `write()` 写 `/tmp/network_status`
- **问题**：缺少读取→比较→写入逻辑，每次都直接写，且无 `O_TRUNC` 在旧实现中可能不清零
- **已废弃**：统一改为调用 `misc.c` 的 `update_network_status()`，避免与其重复写入

### 7.3 其他函数概述（测试/历史遗留）

| 函数 | 说明 |
|------|------|
| `nw_voice_reg_event_ind_cb` | 语音注册事件回调，打印详细注册状态 |
| `nw_data_reg_event_ind_cb` | 数据注册事件回调 |
| `nw_signal_strength_event_ind_cb` | 信号强度变化事件回调 |
| `item_ql_nw_get_data_reg_status` | 测试菜单项：查询数据注册状态 |
| `item_ql_nw_get_signal_strength` | 测试菜单项：查询信号强度 |
| `nw_get_signal_strength` | 封装版，打印 LTE RSSI/RSRQ/RSRP/SNR |
| `nw_get_status` | 通过 call_id 查询 PDP 状态 |
| `nw_set_pref_nwmode_roaming` | 设置 LTE 模式 + 开漫游 |
| `internal_nw_get_signal_strength_level` | 枚举→字符串映射（私有） |

---

## 第八章：logger_sd.c — SD 卡日志完整分析

### 8.1 模块职责

提供带时间戳的双输出（控制台 + SD 卡文件）日志系统，支持自动降级（SD 卡不可用时仅控制台输出）。

### 8.2 配置宏

```c
#define SDCARD_PATH       "/media/sdcard"
#define LOG_DIR           "/media/sdcard/dial_log"
#define MIN_FREE_SPACE_MB 500
```

### 8.3 is_sdcard_mounted()

- 读取 `/proc/mounts`，逐行解析 `dev mnt fstype rest` 格式
- 比较挂载点字段是否等于 `"/media/sdcard"`
- 找到则返回 1，否则返回 0

### 8.4 check_sdcard_available()（内部函数）

```
1. is_sdcard_mounted() → 否则写 /tmp/sdcard_avl = "000" 并返回 0
2. statvfs(SDCARD_PATH, &stat) 检查文件系统
3. free_bytes = f_bavail × f_frsize
4. free_mb = free_bytes / (1024×1024)
5. free_mb < MIN_FREE_SPACE_MB(500) → 返回 0
6. 返回 1
```

### 8.5 check_sdcard_available_wait_30s()（内部函数）

- 最多等待 30 秒，每秒检查一次挂载状态 + 空间
- 每 5 秒打印一次等待进度
- 超时后打印具体失败原因（未挂载 / 空间不足）
- **注意**：`log_init()` 实际调用的是 `check_sdcard_available()`（不等待版本），不是此函数

### 8.6 log_init()

```
1. if g_log_fp != NULL: 防重复初始化，直接返回
2. check_sdcard_available() 失败 → 打印提示，仅控制台模式
3. mkdir -p /media/sdcard/dial_log（通过 system()）
4. 获取当前时间 → 生成文件名：
   /media/sdcard/dial_log/dial_YYYYMMDD_HHMMSS.log
5. fopen(filename, "w") 打开文件
6. 写入文件头：=== Dial Program Started [YYYY-MM-DD HH:MM:SS] ===
7. fflush() 立即落盘
```

### 8.7 dial_log(const char *fmt, ...)

```
1. 获取当前时间 → time_str = "YYYY-MM-DD HH:MM:SS"
2. printf("[%s] ", time_str) + vprintf(fmt, args) → 控制台输出
3. if g_log_fp 非 NULL：
   fprintf(g_log_fp, "[%s] ", time_str)
   va_start(args, fmt)（必须重新 start，va_list 只能遍历一次）
   vfprintf(g_log_fp, fmt, args)
   fflush(g_log_fp)  ← 关键：每次写入后立即刷新，防掉电丢失最新日志
```

### 8.8 log_close()

- 写入退出时间戳：`=== Program Exit [YYYY-MM-DD HH:MM:SS] ===`
- `fclose(g_log_fp)`，`g_log_fp = NULL`

### 8.9 log_to_file()（misc.c 中的版本）

当前版本为空函数（`#if 1` 分支），`#else` 分支保留了写 `/media/sdcard/call_fault.log` 的历史实现，已被 `dial_log()` 统一替代。

---

## 第九章：misc.c — 工具函数库完整分析

### 9.1 executeATCommand(const char *cmd)

**已不推荐使用，但仍有部分旧代码路径调用。**

- 动态分配：`result = malloc(1)`, `*result = '\0'`
- `popen(cmd, "r")` 打开管道
- 循环 `fgets` → `realloc` 扩容 → `strcat` 拼接
- `pclose(pipe)` 关闭
- **返回堆上分配的字符串，调用方必须 `free()`**
- **问题**：频繁 realloc、调用方内存管理责任不清，已逐步被 `exec_cmd_safe()` 替代

### 9.2 extractSignalValue(const char *input)

- 在输入字符串中查找 `"+CSQ: "` 前缀
- `sscanf` 提取后面的整数
- 配合 `executeATCommand` 使用（历史遗留）

### 9.3 checkSimCardStatus()

- 调用 `executeATCommand("serial_atcmd at+creg?")` 检查注册状态
  - 含 `+CREG: 0,1` 或 `+CREG: 0,5` → 注册正常
  - 否则返回 2（网络注册失败）
- 调用 `executeATCommand("serial_atcmd at+cpin?")` 检查 SIM
  - 含 `READY` → 返回 0（SIM 正常）
  - 否则返回 1
- **已被 dial.c 中的 `get_cpin_status_str()` + `get_cereg_status_safe()` 替代**

### 9.4 checkCommandSuccess(const char *cmd)

- 调用 `executeATCommand(cmd)`，检查输出是否含 `"OK"`
- 返回 1 成功，0 失败

### 9.5 isModuleOK()

- `executeATCommand("serial_atcmd at")` 检查模组是否响应
- 输出含 `"OK"` 则返回 1

### 9.6 read_exit_count() / write_exit_count(int count)

- 文件路径：`EXIT_COUNT_FILE = "/tmp/exit_count.txt"`（dial.h 定义）
- `read_exit_count()`：fopen → fscanf 读整数，文件不存在返回 0
- `write_exit_count(count)`：fopen("w") → fprintf 写整数

### 9.7 isIdleState()

判断当前是否处于空闲状态（无 WiFi 客户端连接）：

```
1. access("/sys/class/net/wlan0", F_OK) 失败 → 假设 idle，返回 1
2. runCommand("iw wlan0 info") → parseOutput 提取 "type " 后的字段
3. if type == "AP"：
     runCommand("hostapd_cli -i wlan0 status") → 提取 "num_sta[0]="
     num_sta > 0 → isIdle = 0（有客户端）
4. else（STA 模式）：
     runCommand("iw wlan0 link") → 若不含 "Not connected" → isIdle = 0
5. return isIdle
```

### 9.8 restartNetworkServices()

- 先调 `isIdleState()`，非 idle（有 WiFi 客户端）直接返回
- `killall -9 ql_rild`
- 双重 fork 启动 ql_rild（孙进程被 init 收养）：
  ```
  fork() → 子进程（中间层）
    → fork() → 孙进程 → execl("/usr/bin/ql_rild")
    → 子进程立即 _exit(0)（脱离父子关系）
  父进程 waitpid(pid1, NULL, 0)（立即回收中间进程，无僵尸）
  ```
- 同样方式双重 fork 启动 ql_netd

### 9.9 runCommand(const char *command, char *output, int outputSize)

- `popen(command, "r")` → 循环 `fgets` 追加到 output buffer
- 检查 `strlen(output) + strlen(line) < outputSize` 防溢出
- `pclose(fp)`

### 9.10 parseOutput(const char *output, const char *key, char *value, int valueSize)

- `strstr(output, key)` 定位 key
- 跳过 key 后的空格
- `strchr(start, '\n')` 找值结束位置
- `strncpy(value, start, len)` + `'\0'`

### 9.11 log_to_file()

当前实现为空函数（`#if 1` 分支），历史实现写 `/media/sdcard/call_fault.log`（`#else` 分支）。

### 9.12 readCallIdFromFile() / writeCallIdToFile()

- 读写 `/tmp/callid` 文件，持久化 `g_callid`
- **历史遗留**，当前 dial_loop 已固定使用 call_id=4，不再使用此机制

### 9.13 check_process(const char *process_name)

```c
snprintf(command, sizeof(command), "pgrep -x %s | wc -l", process_name);
```
- `-x` 精确匹配进程名（防止 `pgrep dial` 匹配到 `dialog` 等含子串进程）
- 返回进程实例数（0=未运行，≥1=运行中）
- 参数校验：process_name 为 NULL 或长度为 0 或 >64 时返回 -1

### 9.14 restart_ql_netd()

- `killall -9 ql_netd` + `sleep(3)`
- 单重 fork 启动 ql_netd（**注意**：此版本是单重 fork，子进程不被 init 收养，与 restartNetworkServices 的双重 fork 实现不同）
- 父进程不 waitpid，子进程成为孤儿进程，由 init 回收

### 9.15 read_status() / write_status() / update_network_status()

```c
// update_network_status(int new_status):
1. current_status = read_status()       // 读 /tmp/network_status
2. if current_status == new_status: 不写入（避免重复写，减少 flash 磨损）
3. else: write_status(new_status)       // 写新值
```

`NW_STATUS_PATH` = `"/tmp/network_status"`（misc.h 定义）

### 9.16 ql_netcall 相关函数（历史遗留）

`start_ql_netcall()`、`is_ql_netcall_running()`、`stop_ql_netcall()`、`restart_ql_netcall()` 均为历史遗留，当前版本不使用 ql_netcall 进程。

---

## 第十章：reboot_conf/dial_reboot_conf.c — 持久化配置完整分析

### 10.1 模块职责

- 持久化启动配置（uptime + restart_flag），防止循环重启
- 提供系统 uptime 读取
- 提供前置条件检测（信号、SIM、ql_netd）
- 提供 `restart_cfun()` 的频率保护（与 dial.c 中同名函数共享同一对文件）

### 10.2 Config 结构体

```c
typedef struct {
    int uptime;           // 【持久化】首次启动时的系统 uptime（秒）
    int restart_flag;     // 【持久化】是否已执行过系统重启（0=未重启，1=已重启）
    int first_disconnect; // 【运行时】首次断网标志（0=未断，1=已断）
    int signal_strength;  // 【运行时】CSQ 值（由 get_signal_csq() 填入）
    int sim_status;       // 【运行时】sim_init() 返回值（0=正常，-1=失败）
    int ql_netd_status;   // 【运行时】ql_netd 运行状态（0=运行，非0=异常）
} Config;
extern Config g_config;  // 全局实例
```

**持久化字段**在 `/tmp/dial_reboot.conf` 中以 `"uptime,restart_flag\n"` 格式存储。**运行时字段**不持久化，每次启动重新获取。

### 10.3 CONFIG_FILE 和 REBOOT_FILE

```c
#define CONFIG_FILE "/tmp/dial_reboot.conf"
#define REBOOT_FILE "/tmp/dial_reboot_flag"
#define THRESHOLD_TIME 3600  // 断网阈值（秒）
```

### 10.4 read_config()

```
1. fopen(CONFIG_FILE, "r") 打开
2. 若文件不存在（NULL）：
   g_config.uptime = get_uptime()
   g_config.restart_flag = 0
   g_config.first_disconnect = 0
   write_config()  → 创建初始配置文件
   return
3. fscanf(file, "%d,%d", &uptime, &restart_flag)
4. g_config.first_disconnect = 0（每次启动重置）
```

### 10.5 write_config()

- `fopen(CONFIG_FILE, "w")`
- `fprintf(file, "%d,%d\n", uptime, restart_flag)`

### 10.6 get_uptime()

- `fopen("/proc/uptime", "r")` → `fscanf(fp, "%f", &uptime)` → 返回 `(int)uptime`
- 取浮点数的整数部分（/proc/uptime 格式：`<uptime_seconds> <idle_seconds>`）

### 10.7 get_signal_csq()

- `popen("serial_atcmd at+csq", "r")` → 查找 `"+CSQ:"` → `sscanf` 提取信号值
- 返回 0-31（正常）或 -1（失败）

### 10.8 check_sim_status()

- 调用 `sim_init()` + `sim_get_iccid()`（仅用于检测 SIM 是否可用）
- 结果写入 `g_config.sim_status`

### 10.9 check_pre_conditions()

- 检查 `signal_strength > 20 && sim_status >= 0 && ql_netd_status == 0`
- 全部满足返回 1，否则返回 0（当前主流程未调用此函数）

### 10.10 handle_dial_up_down()（当前未使用）

- 检查前置条件 → 检查 restart_flag → 若断网时长 > THRESHOLD_TIME(3600s) → 执行 perform_reboot()

### 10.11 perform_reboot()

- 写 `1` 到 `/tmp/dial_reboot_flag` → sleep(10) → `system("reboot")`（当前未被调用）

### 10.12 print_config()

打印 g_config 所有字段到控制台，用于启动日志诊断。

---

## 第十一章：cc_deque/ — 双端队列（历史遗留）

### 11.1 来源与背景

来自开源项目 **Collections-C**（Srđan Panić），LGPL 许可证。实现了通用的循环缓冲区双端队列，原本可能用于事件队列设计，后因改用直接回调模式而未被主流程使用。

### 11.2 核心数据结构

```c
struct cc_deque_s {
    size_t   size;       // 当前元素个数
    size_t   capacity;   // 缓冲区容量（始终为 2 的幂）
    size_t   first;      // 队头索引
    size_t   last;       // 队尾索引（指向下一个写入位置）
    void   **buffer;     // void* 指针数组（循环缓冲区）
    
    // 可注入的内存分配器（支持自定义 allocator）
    void *(*mem_alloc)  (size_t size);
    void *(*mem_calloc) (size_t blocks, size_t size);
    void  (*mem_free)   (void *block);
};
```

**默认容量**：8（`DEFAULT_CAPACITY`），扩容因子：2（`DEFAULT_EXPANSION_FACTOR`）

**容量约束**：始终为 2 的幂（`upper_pow_two()` 对齐），便于用位运算实现循环索引。

### 11.3 CC_DequeConf 配置结构

```c
typedef struct {
    size_t capacity;     // 初始容量
    void *(*mem_alloc)(size_t);
    void *(*mem_calloc)(size_t, size_t);
    void  (*mem_free)(void*);
} CC_DequeConf;
```

`cc_deque_conf_init()` 用默认值（malloc/calloc/free + DEFAULT_CAPACITY）初始化。

### 11.4 主要操作函数

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `cc_deque_new(CC_Deque **d)` | 用默认配置创建新队列 | `CC_OK` / `CC_ERR_ALLOC` |
| `cc_deque_new_conf(conf, d)` | 用自定义配置创建 | `CC_OK` / `CC_ERR_INVALID_CAPACITY` / `CC_ERR_ALLOC` |
| `cc_deque_destroy(deque)` | 销毁队列（不释放元素） | void |
| `cc_deque_destroy_cb(deque, cb)` | 销毁队列并对每个元素调用 cb | void |
| `cc_deque_add_first(d, elem)` | 从头部插入 | `CC_OK` / `CC_ERR_ALLOC` |
| `cc_deque_add_last(d, elem)` | 从尾部插入 | `CC_OK` / `CC_ERR_ALLOC` |
| `cc_deque_remove_first(d, out)` | 移除头部元素 | `CC_OK` / `CC_ERR_OUT_OF_RANGE` |
| `cc_deque_remove_last(d, out)` | 移除尾部元素 | `CC_OK` / `CC_ERR_OUT_OF_RANGE` |
| `cc_deque_get_first(d, out)` | 读取头部不移除 | `CC_OK` / `CC_ERR_OUT_OF_RANGE` |
| `cc_deque_get_last(d, out)` | 读取尾部不移除 | 同上 |
| `cc_deque_get_at(d, idx, out)` | 按索引读取 | `CC_OK` / `CC_ERR_OUT_OF_RANGE` |
| `cc_deque_size(d)` | 返回元素个数 | `size_t` |
| `cc_deque_capacity(d)` | 返回当前容量 | `size_t` |
| `cc_deque_contains(d, elem)` | 线性查找是否存在 | `bool` |
| `cc_deque_copy_shallow(d, out)` | 浅拷贝 | `CC_OK` / `CC_ERR_ALLOC` |
| `cc_deque_iter_init / iter_next` | 正向迭代器 | — |
| `cc_deque_diter_init / diter_next` | 反向迭代器 | — |

### 11.5 内部扩容机制

```
expand_capacity(deque):
  new_capacity = deque->capacity * DEFAULT_EXPANSION_FACTOR(2)
  realloc buffer 到 new_capacity
  copy_buffer：将循环缓冲区线性化（处理 first > last 的绕回情况）
```

### 11.6 cc_common.c

提供 `cc_common.h` 中声明的基础工具：
- `t_get_int()` / `t_get_float()` / `t_get_string()`：从 stdin 安全读取输入
- `T_ARRAY_SIZE(arr)` 宏：`sizeof(arr)/sizeof(arr[0])`
- `diff(start, end)` 函数：计算两个 `timespec` 的差值

### 11.7 为何未使用

早期设计可能计划用双端队列缓冲 SDK 事件，后因：
1. SDK 回调本身是异步通知模型，直接写 volatile 标志更简单
2. 单核平台无真正并发，不需要线程安全队列
3. 连接状态不需要排队，最新状态覆盖即可


---

## 12. IPC 与通信协议完整清单

### 12.1 /tmp 状态文件完整清单

| 文件路径 | 读/写 | 内容格式 | 创建者 | 读取者 | 说明 |
|---------|-------|---------|-------|-------|------|
| `/tmp/network_status` | R/W | `1` 或 `0` | misc.c `update_network_status()` | 外部监控进程 | 1=已联网，0=未联网；读→比较→写，避免重复写入 |
| `/tmp/network_csq` | W | `POOR`/`MODERATE`/`GOOD`/`GREAT`/`UNKNOWN` | nw.c `get_signal_strength()` | 外部监控进程 | 信号强度等级字符串 |
| `/tmp/dial_Status` | W | `0` 或 `1` | data_call.c 回调 | 外部监控进程 | 0=拨通(CONNECTED)，1=断开(DISCONNECTED) |
| `/tmp/dial_version` | W | `Version: 1.27.04\r\n` | dial.c `main()` 启动时写入 | 外部监控进程 | 三段式格式：MAIN.SUB.PATCH（两位补零） |
| `/tmp/dial_retry_count` | R/W | `0`/`1`/`2`（整数字符串） | dial.c `check_and_update_retry_count()` | dial.c `dial_loop()` | Fast Retry 计数；成功后 `clear_retry_count()` 删除文件 |
| `/tmp/exit_count.txt` | R/W | 整数（如 `5`） | misc.c `write_exit_count()` | misc.c `read_exit_count()`、dial.c `main()` | 异常退出次数；≥20 触发 `restart_cfun()` |
| `/tmp/cfun_count.txt` | R/W | 整数（如 `3`） | dial.c `restart_cfun()` | dial.c `restart_cfun()` | CFUN 切换调用次数；上限 10 次 |
| `/tmp/cfun_last_call.txt` | R/W | 系统 uptime 秒数（如 `3600`） | dial.c `restart_cfun()` | dial.c `restart_cfun()` | 上次 CFUN 调用的系统时间；两次间隔 <600s 则跳过 |
| `/tmp/dial_reboot.conf` | R/W | `uptime,restart_flag\n`（如 `120,0`） | reboot_conf/ `write_config()` | reboot_conf/ `read_config()` | 持久化启动配置；uptime=开机时系统秒数 |
| `/tmp/resolv_v4.conf` | W | `nameserver x.x.x.x\nnameserver y.y.y.y\n` | data_call.c CONNECTED 路径 | 系统 resolver | 仅 LF 换行（无 CRLF），DISCONNECTED 时 unlink |
| `/tmp/resolv_v6.conf` | W | `nameserver xxxx::\n`（IPv6 DNS） | data_call.c CONNECTED 路径 | 系统 resolver | DISCONNECTED 时 unlink |
| `/tmp/sdcard_avl` | W | `"000"` | logger_sd.c `log_init()` | 外部监控进程 | SD 卡不可用时写入；不存在则表示 SD 可用 |
| `/tmp/callid` | R/W | 整数（如 `4`） | misc.c `writeCallIdToFile()` | misc.c `readCallIdFromFile()` | data call ID 持久化（历史遗留，当前主流程不依赖） |
| `/etc/resolv.conf` | W | `nameserver x.x.x.x\nnameserver xxxx::\n` | data_call.c CONNECTED 路径（合并 v4+v6） | 系统 DNS 解析器 | 由 `resolv_v4.conf` + `resolv_v6.conf` 合并写入；**断开时不清空**（P3 残留问题） |
| `/usr/dial/apn.json` | R | JSON 数组格式，见第 5 章 | apn/apn.c | apn/apn.c | APN 数据库；文件不存在则使用默认 APN `apnpublic` |
| `/media/sdcard/dial_log/dial_YYYYMMDD_HHMMSS.log` | W | 带时间戳的文本日志 | logger_sd.c `dial_log()` | 人工查阅 | SD 卡运行日志；文件名含启动时间戳 |
| `/media/sdcard/diag/dmesg_YYYYMMDD_HHMMSS.log` | W | dmesg 内核日志文本 | dial.c `diag_snapshot()` | 移远工程师分析 | 故障确认或网络恢复时触发；最多保留 5 个最新文件 |
| `/media/sdcard/diag/logcat_YYYYMMDD_HHMMSS.log` | W | Android logcat 系统日志 | dial.c `diag_snapshot()` | 移远工程师分析 | 与 dmesg 同时写入，命名规则相同 |
| `/proc/uptime` | R | `秒.厘秒 空闲秒` | 内核 | reboot_conf/ `get_system_uptime()`、dial.c | 读第一字段（系统运行总秒数） |
| `/proc/mounts` | R | `设备 挂载点 类型 选项` | 内核 | logger_sd.c `check_sdcard_available()` | 检查 `/media/sdcard` 是否已挂载 |
| `/sys/class/net/<if>/statistics/rx_packets` | R | 整数（十进制字节数） | 内核网络栈 | nw.c `nw_get_if_statistics_rx_packets()` | 网卡收包统计，供 flow_monitor_task 使用（历史遗留） |

### 12.2 AT 命令完整清单

所有 AT 命令均通过 `popen("serial_atcmd <cmd>", "r")` 调用（misc.c `executeATCommand()`），不直接操作串口。

| AT 命令 | 用途 | 响应格式 | 解析方式 | 调用位置 |
|---------|------|---------|---------|---------|
| `AT` | 模块在线检测 | `\r\nOK\r\n` | `strstr(result, "OK")` | misc.c `isModuleOK()` |
| `AT+CSQ` | 获取信号强度 | `+CSQ: <rssi>,<ber>\r\nOK` | `sscanf(csqPos+6, "%d", &val)` | misc.c `extractSignalValue()`、dial.c `get_signal_csq()` |
| `AT+CEREG?` | 数据网络注册状态 | `+CEREG: <n>,<stat>\r\nOK` | `sscanf(pos+9, "%*d,%d", &stat)` | dial.c `get_reg_stat_safe()`、misc.c `checkSimCardStatus()` |
| `AT+CPIN?` | SIM 卡状态（降级查询） | `+CPIN: READY\r\nOK` 或 `+CPIN: SIM PIN\r\nOK` | `strstr(resp, "READY")` | dial.c `get_cpin_safe()`、misc.c `checkSimCardStatus()` |
| `AT+CREG?` | CS 网络注册状态（旧版，已被 CEREG 取代） | `+CREG: <n>,<stat>` | `strstr(resp, "+CREG: 0,1")` | misc.c `checkSimCardStatus()`（历史遗留） |
| `AT+QIACT?` | PDP 上下文激活状态 | `+QIACT: <id>,<state>,<type>,<ip>` | `strstr` 检查是否有激活条目 | dial.c `get_pdp_context_safe()` |
| `AT+CGACT?` | PDP 激活状态（恢复前诊断快照） | `+CGACT: <cid>,<state>` | 整串保存到快照文件 | dial.c `ST_RECOVERY` 前的诊断段 |
| `AT+CEER` | 最后一次呼叫错误原因 | `+CEER: <report>` | 整串保存到快照文件 | dial.c `ST_RECOVERY` 前的诊断段 |
| `AT+QGMR` | 固件版本（Quectel 私有命令） | 纯版本号文本行（无 `+` 前缀，如 `EC200ACNAAR01A01M08_OCPU`） | `sscanf(buf, "%s", fw)` 取第一词 | dial.c `get_fw_version_safe()` |
| `AT+QTEMP` | 模块温度（排查热关机） | `+QTEMP: "label","temp_val"` | 整串打印到心跳日志 | dial.c 30s 心跳 Part A |
| `AT+CESQ` | 扩展信号质量（RSRP/SINR/RSRQ/RSSI） | `+CESQ: <rxlev>,<ber>,<rscp>,<ecno>,<rsrq>,<rsrp>` | 整串打印到扩展心跳 | dial.c 5min 扩展心跳 Part A2 |
| `AT+CREG=2` + `AT+CREG?` | 小区信息（MCC/MNC/TAC/CID，详细模式） | `+CREG: 2,<stat>,<tac>,<cid>,<act>` | 整串打印到扩展心跳 | dial.c 5min 扩展心跳 Part A2 |
| `AT+CGPADDR` | 当前 PDP 上下文 IP 地址 | `+CGPADDR: <cid>,<ip>` | 整串打印到扩展心跳 | dial.c 5min 扩展心跳 Part A2 |
| `AT+QSIMSTAT?` | SIM 卡硬件插入状态（物理层检测） | `+QSIMSTAT: <en>,<ins>` | `sscanf(resp, "+QSIMSTAT: %*d,%d", &ins)` | dial.c `ST_SIM` SIM 断开诊断路径 |
| `AT+CFUN=0` | 关闭射频（飞行模式） | `OK` | `strstr(result, "OK")` | dial.c `ST_RECOVERY L2`、reboot_conf/ `restart_cfun()` |
| `AT+CFUN=1` | 开启射频（退出飞行模式） | `OK` | `strstr(result, "OK")` | dial.c `ST_RECOVERY L2`、reboot_conf/ `restart_cfun()` |
| `AT+CFUN=1,1` | 模块软重启（触发 ASR1803 SoC 整体复位，Linux 必跟着重启） | 无响应（SoC 复位，进程被杀） | 不解析（发完即 `exit(1)`） | dial.c `ST_RECOVERY L3` |
| `AT+CIMI` | 读取 IMSI（15 位） | IMSI 数字字符串 + `OK` | `sscanf` 提取 15 位数字 | dial.c `dial_loop()` 初始化段 IMSI 采集 |
| `AT*ZCGMR` | 读取 AP 侧固件版本（ASR1803 AP/Linux 层固件，Quectel 私有扩展命令） | `*ZCGMR: "OW21.02_asr1803p401_rls211_1.057.030_..."` | `strstr(line, "*ZCGMR:")` 提取引号内版本字符串 | dial.c `get_zcgmr_safe()`，v1.27.5 新增，`print_init_info()` 内打印 `[INIT] AP_FW:` |
| `AT*CGMR` | 读取 CP 侧固件版本（ASR1803 CP/蜂窝协议栈，含 CP SDK、LWG modem、RF API 三段以 `::` 分隔） | `*CGMR: "FALCON_CP_SDK1.057.030X_Linux::FALCON_LWG_M13_..."` | `strstr(line, "*CGMR:")` 提取引号内版本字符串 | dial.c `get_acgmr_safe()`，v1.27.5 新增，`print_init_info()` 内打印 `[INIT] CP_FW:` |

### 12.3 SDK API 完整调用清单

#### ql_data_call 系列

| API 函数 | 调用位置（文件:行号范围） | 参数说明 | 返回值语义 |
|---------|----------------------|---------|----------|
| `ql_data_call_init()` | dial.c `dial_loop()` 初始化段 | 无参数 | `QL_ERR_OK`(0)=成功；`-1001`=SDK 服务未就绪（需重试） |
| `ql_data_call_set_status_ind_cb(cb)` | dial.c `dial_loop()` 初始化段 | `cb`=`data_call_status_ind_cb` 函数指针 | 注册连接状态变化回调 |
| `ql_data_call_set_service_error_cb(cb)` | dial.c `dial_loop()` 初始化段 | `cb`=`data_call_service_error_cb` 函数指针 | 注册 CP 侧 SDK 服务崩溃回调（v1.27.1 新增） |
| `ql_data_call_create(id, name, flag)` | dial.c `dial_loop()` 初始化段 | `id=4`；`name="auto_network"`；`flag` 默认 0 | 创建数据连接实例（不发起拨号） |
| `ql_data_call_param_alloc()` | dial.c `dial_loop()` 初始化段 | 无参数 | 返回参数结构体指针 |
| `ql_data_call_set_apn_id(param, id)` | dial.c `dial_loop()` 初始化段 | `id=1`（标准 APN 槽位） | 配置 APN 槽位 |
| `ql_data_call_set_ip_version(param, ver)` | dial.c `dial_loop()` 初始化段 | `ver=IPV4`（当前仅 IPv4） | 配置 IP 版本类型 |
| `ql_data_call_set_reconnect_interval(param, ms)` | dial.c `dial_loop()` 初始化段 | `ms=25000`（25 秒） | 配置 SDK 底层自动重连间隔 |
| `ql_data_call_config(id, param)` | dial.c `dial_loop()` 初始化段 | `id=4`；`param` 为上面配置好的参数 | 将参数配置应用到连接实例 |
| `ql_data_call_param_free(param)` | dial.c `dial_loop()` 初始化段 | 释放参数结构体内存 | — |
| `ql_data_call_start(id)` | dial.c `dial_loop()` 初始化段 + `ST_RECOVERY` L1/L2 | `id=4` | 发起异步拨号；立即返回，结果通过回调通知 |
| `ql_data_call_stop(id)` | dial.c `ST_RECOVERY L1` | `id=4` | 停止数据连接（L1 前执行） |
| `ql_data_call_set_apn_config(id, cfg)` | apn/apn.c `set_apn()` | `id=4`；`cfg` 含 apn/username/password | 配置连接使用的 APN 信息（供运营商鉴权） |

#### ql_sim 系列

| API 函数 | 调用位置 | 参数说明 | 返回值语义 |
|---------|---------|---------|----------|
| `ql_sim_init()` | sim/sim.c `sim_init()`、reboot_conf/ 前置检测 | 无参数 | `0`=成功；非 0=失败 |
| `ql_sim_set_card_status_cb(cb)` | dial.c `dial_loop()` 初始化段 | `cb`=SIM 状态变化回调（v1.27 新增，替代 AT+CPIN? 轮询） | 卡状态变化时通知（插入/拔出/READY）；初始状态不触发，需主动查询 |
| `ql_sim_get_card_info(slot, &info)` | dial.c `dial_loop()` 初始化段 | `slot=QL_SIM_SLOT_1`；`info` 含 `app_state` 等字段 | 主动获取当前 SIM 状态（v1.27，弥补回调不触发初始状态的问题） |
| `ql_sim_get_iccid(slot, buf, len)` | sim/sim.c `sim_get_iccid()` | `slot=QL_SIM_SLOT_1`；`buf`=20 字节缓冲；`len=20` | 读取 ICCID（20 位字符串）；成功返回 0 |
| `ql_sim_get_imsi(slot, type, buf, len)` | sim/sim.c `sim_get_imsi()` | `slot=QL_SIM_SLOT_1`；`type=QL_SIM_3GPP`；`buf`=15 字节缓冲 | 读取 IMSI（15 位字符串）；成功返回 0 |

#### ql_nw 系列

| API 函数 | 调用位置 | 参数说明 | 返回值语义 |
|---------|---------|---------|----------|
| `ql_nw_get_signal_strength(info)` | nw.c `get_signal_strength()` | `info` 为输出结构体（含多制式信号强度枚举） | `0`=成功；写入 LTE/5G 信号等级枚举值 |
| `ql_nw_get_data_reg_status(stat)` | nw.c `nw_get_data_reg_status()` | `stat` 为输出结构体 | 读取 PS 域网络注册状态 |

### 12.4 进程间通信拓扑图（ASCII）

```
┌────────────────────────────────────────────────────────────────────────────┐
│                              open_dial 进程                                  │
│                                                                              │
│  ┌──────────────────────────────────────────┐   ┌──────────────────────┐   │
│  │ 主线程（单线程轮询，50ms/周期）               │   │  SDK 内部回调线程      │   │
│  │                                          │   │                      │   │
│  │  main()                                  │   │  data_call_status_   │   │
│  │    └── dial_loop()                       │   │  ind_cb()            │   │
│  │          ├── 状态机（ST_STATUS~RECOVERY）  │   │  ├─ g_pdp_connected  │   │
│  │          ├── 心跳日志（30s）               │◄──┤  ├─ g_if_name        │   │
│  │          ├── 扩展心跳（5min）              │   │  └─ g_need_reset_    │   │
│  │          ├── Ping（popen子进程）           │   │     fail_ts          │   │
│  │          └── SIGINT/崩溃检测             │   │                      │   │
│  │                                          │   │  sim_card_status_cb  │   │
│  │                                          │◄──┤  ├─ g_sim_app_ready  │   │
│  │                                          │   │  └─ g_sim_app_state  │   │
│  │                                          │   │                      │   │
│  │                                          │◄──┤  data_call_service_  │   │
│  │                                          │   │  error_cb()          │   │
│  │                                          │   │  └─ g_sdk_service_   │   │
│  │                                          │   │     error            │   │
│  └──────────────────────────────────────────┘   └──────────────────────┘   │
│                    │                                                         │
│                    │ popen/system 子进程                                     │
│                    ▼                                                         │
│  ┌─────────────────────────┐   ┌────────────────┐   ┌──────────────────┐  │
│  │ serial_atcmd            │   │ ping 8.8.8.8   │   │ ip / iptables    │  │
│  │ (AT 命令代理工具)         │   │ (网络连通检测)   │   │ (路由/NAT 配置)   │  │
│  └────────────┬────────────┘   └────────────────┘   └──────────────────┘  │
│               │                                                              │
└───────────────┼──────────────────────────────────────────────────────────────┘
                │ AT 口（/dev/ttyS2 或 USB ACM）
                ▼
         ┌─────────────────────────────────────────────┐
         │               ASR1803 CP 侧                  │
         │  蜂窝协议栈（Cortex-R5，LTE/5G Modem 固件）   │
         │  ← at+cfun=0/1/1,1 控制 RF/SoC 复位          │
         └─────────────────────────────────────────────┘

/tmp 状态文件（IPC 中枢）
  ┌──────────────────┐     ┌──────────────────────────────────────────────┐
  │  open_dial（写）  │────▶│ /tmp/network_status (0/1)                   │
  │                  │────▶│ /tmp/dial_Status (0/1)                      │
  │                  │────▶│ /tmp/network_csq (信号强度字符串)              │
  │                  │────▶│ /tmp/dial_version (版本号)                    │
  │                  │────▶│ /tmp/dial_retry_count (0/1/2)               │
  │                  │────▶│ /tmp/exit_count.txt (退出计数)                │
  └──────────────────┘     └───────────────────────┬──────────────────────┘
                                                   │
                                                   ▼（外部进程读取）
                                        ┌──────────────────────────┐
                                        │ 上层监控/路由器管理程序     │
                                        │ （如 luci/netifd 等）       │
                                        └──────────────────────────┘

ql-sdk 通信路径（内核驱动层）
  open_dial
      │ ql_data_call_start()
      │ ql_sim_get_iccid()
      │ ql_nw_get_signal_strength()
      ▼
  ql-sdk 库（/usr/lib/libql*.so）
      │ ioctl / 私有驱动接口
      ▼
  ASR1803 AP 内核驱动 ──► CP 侧（蜂窝协议栈）
```


---

## 13. 并发安全分析

### 13.1 线程模型

`open_dial` 是**单主线程 + SDK 回调线程**的混合模型：

- **主线程**：`main()` → `dial_loop()`，50ms 轮询节拍，单线程，顺序执行状态机。所有业务逻辑（Ping、AT 命令、路由配置、Fast Retry）均在此线程。
- **SDK 回调线程**：由 Quectel ql-sdk 内部创建和管理，负责在连接状态变化、SIM 状态变化、SDK 服务错误时调用注册的回调函数。
- **popen 子进程**：Ping 和 AT 命令通过 `popen()` 创建子进程执行，主线程阻塞等待 `pclose()` 返回，属于同步调用，不引入额外线程。

```
┌───────────────────────────────────────────────────────┐
│ 进程地址空间                                            │
│                                                        │
│  主线程 (POSIX thread, 50ms 轮询)                       │
│  ┌────────────────────────────────────────────┐        │
│  │ dial_loop()  →  状态机  →  Ping (阻塞)      │        │
│  └──────────────────────────┬─────────────────┘        │
│                             │ 读取 volatile 全局变量    │
│                     ┌───────┴──────────────────────┐   │
│  SDK 回调线程 ──写─▶ │  g_pdp_connected             │   │
│                     │  g_need_reset_fail_ts         │   │
│                     │  g_sim_app_ready              │   │
│                     │  g_sim_app_state              │   │
│                     │  g_sdk_service_error          │   │
│                     │  g_if_name（char 数组，见下文） │   │
│                     └──────────────────────────────┘   │
└───────────────────────────────────────────────────────┘
```

### 13.2 volatile 变量完整列表

| 变量 | 类型 | 初始值 | 写入方 | 读取方 | 语义 |
|------|------|-------|-------|-------|------|
| `g_pdp_connected` | `volatile int` | 0 | SDK 回调线程（`data_call_status_ind_cb`） | 主线程（`dial_loop()`） | PDP 连接状态；1=已连接，0=未连接 |
| `g_need_reset_fail_ts` | `volatile int` | 0 | SDK 回调线程（DISCONNECTED 路径） | 主线程（`ST_PING` 开头） | 1=需重置故障计时器；主线程读后清零 |
| `g_sigint_received` | `volatile int` | 0 | 信号处理函数 `sig_handler()`（异步信号上下文） | 主线程（每轮轮询开头） | SIGINT 信号标志；捕获后设 1，主线程执行清理退出 |
| `g_sdk_service_error` | `volatile int` | 0 | SDK 回调线程（`data_call_service_error_cb`） | 主线程（每轮轮询开头） | CP 侧 SDK 服务崩溃标志；置 1 后主线程打日志并退出 |
| `g_sim_app_ready` | `volatile int` | -1 | SDK 回调线程（`sim_card_status_cb`） | 主线程（`ST_SIM`） | -1=回调未初始化，0=SIM 非 READY，1=SIM READY |
| `g_sim_app_state` | `volatile int` | 0 | SDK 回调线程（`sim_card_status_cb`） | 主线程（心跳日志） | `QL_SIM_APP_STATE_E` 整数值，用于日志打印 |

**注意**：`g_if_name` 是 `char[64]` 字符数组（非 `volatile`），由 `data_call_status_ind_cb` 的 CONNECTED 路径通过 `strncpy` 写入，主线程在 DISCONNECTED 路径清理路由时读取。这是一个潜在的 race condition（见 13.4）。

### 13.3 无 mutex 的合理性分析

代码不使用任何互斥锁，依赖 `volatile` 关键字防止编译器优化掉对共享变量的读取。在目标平台上可接受的原因：

1. **单核 ARM（ASR1803，Cortex-A7）**：不存在真正的硬件并行执行，"同时写入"不会发生。即使没有 mutex，CPU 每次只执行一个线程。

2. **int 类型原子性**：在 ARM 架构上，32 位 `int` 的读写是原子操作（单条 LDR/STR 指令），不会出现读到中间状态（半写）的情况。

3. **访问模式简单**：SDK 回调线程只写入（set 为 0 或 1），主线程只读取并在特定时机清零——是简单的生产者-消费者模式，无复杂的读-改-写序列。

4. **`volatile` 禁止缓存**：`volatile` 确保每次访问都从内存读取，而非 CPU 寄存器缓存，防止编译器将循环展开为"永真"或"永假"。

5. **嵌入式实时要求**：引入 mutex 会带来不确定的锁等待延迟，影响 50ms 轮询节拍的实时性。在状态简单、访问频率不高（50ms/次）的场景下，`volatile` 足够。

### 13.4 潜在 Race Condition 风险点

| 风险编号 | 涉及变量 | 场景描述 | 危害程度 | 缓解措施 |
|---------|---------|---------|---------|---------|
| RC-1 | `g_if_name` (char[64]) | SDK 回调线程在 CONNECTED 时写入新接口名，而主线程恰在 DISCONNECTED 路径 `ip route del default dev g_if_name` 中读取 | 低（多核才真正危险；单核上 strncpy 与 route del 之间不会被抢占执行） | 单核天然互斥；若移植到多核平台需加 mutex |
| RC-2 | `g_pdp_connected` | 主线程检查后（`if (g_pdp_connected)`），SDK 回调将其置 0，主线程基于旧状态继续执行 | 低（bool 翻转，下一轮 50ms 即可感知） | 可接受的最终一致性 |
| RC-3 | `g_need_reset_fail_ts` | 主线程读取 1 后执行重置，回调线程又写 1（两次 DISCONNECTED 快速连发） | 低（两次重置效果等同于一次，`start_fail_ts=0` 是幂等操作） | 无需额外保护 |
| RC-4 | `g_sim_app_ready` | SDK 回调连续写入两次不同状态（如 NOT_READY → READY），主线程可能错过中间态 | 极低（SIM 状态变化极慢，不会在 50ms 内发生两次） | 可接受 |
| RC-5 | `g_sigint_received` (信号处理函数) | 信号处理函数在非 async-signal-safe 上下文中（已修复：仅设标志不调用 free/fprintf）  | 已修复（见 Bug #13） | 信号处理函数内仅赋值，符合 async-signal-safe 规范 |

**总结**：现有并发模型在目标单核平台上是安全且足够的。若未来移植到多核 SoC（如 Cortex-A53 SMP），需为 `g_if_name` 字符串操作和 `g_pdp_connected` 状态转换添加 `pthread_mutex`。


---

## 14. 内存安全审计

### 14.1 动态内存分配点

| 位置 | 分配方式 | 分配大小 | 释放位置 | 安全性评估 |
|------|---------|---------|---------|----------|
| misc.c `executeATCommand()` | `malloc(1)` + `realloc` 累积 | 随 popen 输出动态增长 | 调用方负责 `free(result)` | ⚠️ 调用方须记得 free；文档化不足，但 V1.26 已改为大多数调用传入 buffer |
| apn/apn.c `apn_load_from_json()` | `malloc(file_size + 1)` | JSON 文件大小 + 1 字节 | 函数内 `free(buf)` | ✅ 同函数内分配和释放，无泄漏风险 |
| apn/apn.c `apn_load_from_json()` | `calloc(count, sizeof(apn_obj_t))` | APN 条目数量 × 结构体大小 | 调用结束后 `free(apn_list)` | ✅ 有 `free`；但若 JSON 解析中途失败（count=0 或 alloc 失败），早期 return 路径需检查是否遗漏 free |
| ql_data_call_param_alloc() | SDK 内部分配 | SDK 内部管理 | `ql_data_call_param_free(param)` | ✅ SDK 配套 alloc/free，成对调用 |

**注意**：V1.26 后大多数 AT 命令调用已改为调用方传入 `char buf[256]` 等静态 buffer，`executeATCommand` 动态分配版本的调用点已大幅减少，降低了内存泄漏风险。

### 14.2 Buffer 边界检查

| 函数/位置 | 操作 | 检查机制 | 风险评估 |
|---------|------|---------|---------|
| misc.c `executeATCommand()` + `realloc` | 累积 popen 输出 | `realloc` 动态扩容，无硬上限 | ⚠️ 若命令输出异常大（如 AT 响应循环），可能 OOM；实际 AT 响应通常 <1KB，风险极低 |
| misc.c `runCommand()` | `fgets` + `strcat` 到 `output` | `strlen(output)+strlen(line) < outputSize` 防溢出 | ✅ 有边界检查，超出时打日志并 break |
| misc.c `check_process()` | `snprintf(command, sizeof(command), ...)` | `sizeof(command)=256`；`process_name` 长度 ≤64 已检查 | ✅ snprintf 有 size 参数，不会溢出 |
| data_call/data_call.c DNS 写入 | `strncpy(dns, info->addr.dnsp, sizeof(dns)-1)` + `\0` 补齐 | `sizeof(dns)-1` 截断保护 | ✅ 防止超长 DNS 字符串截断 |
| apn/apn.c 字段读取 | `strncpy(obj.apn, json_str, sizeof(obj.apn)-1)` | `sizeof(apn_obj_t.apn)=32`，-1 保留 \0 | ✅ 截断保护；超长 APN 被截断但不崩溃 |
| dial.c AT 响应解析（get_reg_stat_safe 等） | `sscanf(buf+offset, "%*d,%d", &val)` | sscanf 格式化提取，不涉及 strcpy | ✅ 无 buffer 溢出风险 |
| dial.c 心跳日志（snprintf 构建字符串） | `snprintf(line, sizeof(line), ...)` | 均带 `sizeof(line)` 参数 | ✅ 安全 |
| logger_sd.c `dial_log()` | `vsnprintf(buf, sizeof(buf), fmt, args)` | `sizeof(buf)` 限制 | ✅ 安全，超长消息被截断 |

### 14.3 popen / pclose 配对审计

代码中所有 `popen` 均有对应 `pclose`，无泄漏：

| 调用位置 | popen 用途 | pclose 调用 | 异常路径关闭 |
|---------|----------|------------|-----------|
| misc.c `executeATCommand()` | 执行 AT 命令 | 循环后 `pclose(pipe)` | realloc 失败时也先 `pclose(pipe)` 再 return NULL ✅ |
| misc.c `runCommand()` | 执行任意命令 | `pclose(fp)` | 循环 break 后也会 `pclose` ✅ |
| misc.c `check_process()` | pgrep -x | `pclose(fp)` | fscanf 后直接 pclose，无异常路径 ✅ |
| reboot_conf/ `read_config()` | 历史遗留 popen（已改 fopen） | — | 已替换为 fopen/fclose，无 popen 泄漏 ✅ |

**V1.26 修复**：`reboot_conf/` 中曾用 `popen` 打开文件但用 `fclose` 关闭（Bug #15），已改为 `fopen`/`fclose`。

### 14.4 json-c 引用计数管理

json-c 采用引用计数（`json_object_put` 减引用，到 0 时 free 对象树）：

```c
// apn/apn.c apn_load_from_json() 正确写法（V1.26 修复后）
json_object *root = json_tokener_parse(buf);         // 创建根对象，引用计数=1
json_object *apn_array;
json_object_object_get_ex(root, "apn", &apn_array);  // 借用引用，计数不变

for (int i = 0; i < count; i++) {
    json_object *entry = json_object_array_get_idx(apn_array, i);  // 借用
    json_object *field;
    json_object_object_get_ex(entry, "iccid", &field);  // 借用
    // V1.26 修复前：错误地 json_object_put(field)，导致借用引用被释放，后续访问崩溃
    // V1.26 修复后：不调用 put，借用引用由父对象管理
    strncpy(obj.iccid, json_object_get_string(field), sizeof(obj.iccid)-1);
}

json_object_put(root);  // 正确：最终只释放根对象，整棵树递归 free ✅
```

**修复的 Bug**：
- V1.26 前：对每个 `json_object_get_ex` 返回的借用引用调用 `json_object_put`，导致根对象下的子对象被提前释放，后续的 `json_object_get_string` 访问已释放内存（use-after-free）。
- V1.26 后：仅在函数末尾对根对象调用一次 `json_object_put`，整棵树一次性释放。

### 14.5 文件描述符管理

| 位置 | 操作 | 是否关闭 |
|------|------|---------|
| misc.c `read_exit_count()` / `write_exit_count()` | `fopen`/`fclose` | ✅ 每次用完立即 fclose |
| reboot_conf/ `read_config()` / `write_config()` | `fopen`/`fclose` | ✅ 成对关闭 |
| logger_sd.c `dial_log()` | 使用全局 `g_log_fp`，`log_close()` 关闭 | ✅ 进程退出前 log_close() |
| apn/apn.c `apn_load_from_json()` | `fopen`/`fclose` 读 JSON 文件 | ✅ 读完立即 fclose |
| dial.c `diag_snapshot()` | `popen("dmesg", "r")` / `pclose()` | ✅ 每个 popen 对应 pclose |

**潜在问题**：`logger_sd.c` 的 `g_log_fp` 在 `log_init()` 中打开，正常路径由 `log_close()` 关闭。若程序被 `kill -9` 强杀（非 SIGINT），`g_log_fp` 未关闭，但 fflush 策略已确保最近一次 `dial_log()` 的数据已写盘，无数据丢失风险。


---

## 15. 已知 Bug 与修复历史

### 15.1 V1.26 修复链（全部已验收）

| # | 严重程度 | 文件 | 问题描述 | 修复方案 |
|---|---------|------|---------|---------|
| 1 | ★★★★ 严重 | `dial.c` | Fast Retry 计时从 `dial_loop()` 入口开始，PDP 建立前 Ping 必然失败，导致启动时误判为"网络不通"而频繁 exit | 引入 `g_pdp_connected` 标志，PDP 就绪后才开始 10s Ping 窗口；额外独立设置 60s PDP 等待超时 |
| 2 | ★★★★ 严重 | `dial.c ST_RECOVERY case 3` | L3 恢复代码全部注释，网络持续断开 30min 后无任何操作，永远不能自愈 | 取消注释，实际执行 `AT+CFUN=1,1` 并 `exit(1)`，保活脚本重拉进程 |
| 3 | ★★★★ 严重 | `data_call/data_call.c` | DISCONNECTED 后不删旧路由，重连后 `ip route` 有多条 default，路由选择混乱，必须重启系统才能恢复上网 | DISCONNECTED 时按 `g_if_name` 精确执行 `ip route del default dev <if>` |
| 4 | ★★★★ 严重 | `data_call/data_call.c` | `call_id` 过滤导致 SDK 重拨（call_id 变化）后的 CONNECTED/DISCONNECTED 事件被忽略，回调永远不触发 | 改用 `call_name`（`"auto_network"`）过滤，SDK 重拨后 call_name 不变 |
| 5 | ★★★★ 严重 | `data_call/data_call.c` | `iptables -t filter -F` 清空整个 filter 表，所有防火墙规则（包括用户自定义）被删除 | 改为循环删除指定 device 的 NAT MASQUERADE 规则（最多 10 次循环），不动 filter 表 |
| 6 | ★★★ 重要 | `dial.c ST_PING` | 单次 Ping 失败立即启动故障计时，偶发网络抖动导致误判，触发不必要的 L1 恢复 | 引入 `PING_FAIL_THRESHOLD=3`，连续失败 3 次才启动 `start_fail_ts` |
| 7 | ★★★ 重要 | `dial.c ST_RECOVERY case 2` | L2 射频重置无任何频率限制，可能每 1.5s 连续触发，对硬件射频产生不必要冲击 | 引入 `last_l2_ts` 冷却节流（正常路径 5min，REG=0 路径 90s） |
| 8 | ★★★ 重要 | `data_call/data_call.c` | DNS 字符串含 `\r\n`（Windows CRLF），写入 `/etc/resolv.conf` 后 `nameserver` 行末有 `^M`，glibc DNS 解析无法识别，导致 DNS 解析失败 | 改为纯 LF（`\n`），strip 掉 CR 字符 |
| 9 | ★★★ 重要 | `data_call/data_call.c` | 断开时不通知状态机重置 `start_fail_ts`，导致重连后 Ping 成功但故障计时器仍在继续，最终误触 L2/L3 | 设置 `g_need_reset_fail_ts=1`，状态机 `ST_PING` 开头检测并重置 |
| 10 | ★★★ 重要 | `apn/apn.c` | `strncmp(iccid, target, strlen(target))` 前缀匹配，ICCID 前 N 位相同的不同运营商会命中错误 APN | 改为 `strcmp` 精确匹配，完整 20 位比较 |
| 11 | ★★★ 重要 | `apn/apn.c` | `json_object_put` 误用于借用引用，子对象被提前释放，后续 `json_object_get_string` 访问已释放内存（use-after-free） | 仅对根对象调用一次 `json_object_put`，移除所有字段借用引用的 put 调用 |
| 12 | ★★★ 重要 | `misc.c` | SIGKILL 不可被捕获；原信号处理函数内调用 `free()` 和 `fprintf()`（非 async-signal-safe），导致信号处理时崩溃 | 改捕获 `SIGINT`，处理函数仅设 `volatile sig_atomic_t g_sigint_received=1`，主循环检测后执行清理 |
| 13 | ★★ 一般 | `misc.c check_process()` | `pgrep dial` 匹配所有含 "dial" 子串的进程（如 `dialog`、`dial_log`），单实例检查出错 | 改为 `pgrep -x dial` 精确匹配完整进程名 |
| 14 | ★★ 一般 | `misc.c executeATCommand()` | 返回堆分配的字符串，调用方责任不明确，存在多处泄漏 | 改为调用方传入静态 buffer，函数内部填充（V1.26 主要调用点已迁移） |
| 15 | ★★ 一般 | `reboot_conf/dial_reboot_conf.c` | `popen` 返回的 FILE* 用 `fclose` 而非 `pclose` 关闭，文件描述符泄漏 + 子进程无法回收 | 改为 `fopen`/`fclose`（不再需要 popen） |
| 16 | ★★ 一般 | `misc.c restartNetworkServices()` | fork 子进程执行 `execl`，但父进程未 `waitpid`，子进程变孤儿；若执行失败，子进程残留为僵尸进程 | 改用双重 fork：中间进程 fork 孙进程后立即 `_exit(0)`，孙进程被 init 收养，父进程 `waitpid` 中间进程即可 |
| 17 | ★★ 一般 | `nw.c` / `misc.c` | 两套 `/tmp/network_status` 写入接口并存（`nw_mark_network_status` 和 `update_network_status`），前者缺 `O_TRUNC`，可能写入短内容覆盖不完整 | 废弃 `nw_mark_network_status`，统一调用 `misc.c::update_network_status()`（读-比较-写） |
| 18 | ★★ 一般 | `dial.c` | `dial_loop()` 初始化时 `ql_data_call_init` 最多阻塞 20s，期间无任何日志输出，难以判断是否卡住 | 每 20 次重试打印一次进度，超时 / 成功均记录日志 |
| 19 | ★★ 一般 | `dial.c` | IPv6 路由 `ip -6 route del default dev <if>` 断开时未清理，导致路由残留 | DISCONNECTED 时同时执行 IPv6 default 路由清理 |

### 15.2 V1.27.x 新修复（待完整回归）

| # | 严重程度 | 提交 | 问题描述 | 修复方案 |
|---|---------|------|---------|---------|
| 20 | ★★★ 重要 | v1.27 | 恢复阈值 L1=60s/L2=5min/L3=30min 过于激进，SDK 还在自愈期（底层 25s 重连）就开始干预，导致干扰 SDK 自愈 | L0 期（0~5min）不干预；L1→5min，L2→10min，L3→35min（留足 SDK 自愈时间）|
| 21 | ★★★ 重要 | v1.27 | REG=0（注册丢失）时 L1（stop→start）连续失败（返回 -1001，因为未注册无法发起拨号），无节流保护 | REG=0 时跳过 L1，L0 期结束后直接触发 L2；L2 冷却从 5min 压缩为 90s |
| 22 | ★★★ 重要 | v1.27 | SIM 状态靠每 800ms 轮询 AT+CPIN? 高频 popen，增加 AT 端口压力，且 popen 开销大 | 改用 `ql_sim_set_card_status_cb` SDK 回调；主动查询 `ql_sim_get_card_info` 获取初始状态；回调未触发时降级为 AT 一次性查询 |
| 23 | ★★ 一般 | v1.27.1 | SDK CP 侧服务（ql_rild）崩溃时，`ql_data_call_start` 等 API 无响应，进程悬空，保活脚本无法重启 | 注册 `ql_data_call_set_service_error_cb`，崩溃时回调设 `g_sdk_service_error=1`，主循环打 [FATAL] 日志并退出（注：exit 调用当前被注释，待验证后启用） |
| 24 | ★ 轻微 | v1.27.2 | 心跳日志 SIM 状态仅显示 AT+CPIN? 查询值，无法与 SDK 回调状态交叉验证，诊断困难 | 心跳改为 `SIM_AT=<AT值>/SIM_CB=<回调值>` 双值输出 |
| 25 | ★ 轻微 | v1.27.3 | 断网故障发生时无诊断快照，移远技术支持需要 dmesg/logcat 但现场无法保留现场数据 | 新增 `diag_snapshot()` 函数，故障确认（ping_fail_count≥3）和恢复时分别写 dmesg+logcat 到 `/media/sdcard/diag/`，最多保留 5 个最新文件 |
| 26 | ★ 轻微 | v1.27.4 | CP dump（崩溃内存转储）写入路径 `/sdcard` 在 Linux 文件系统上不存在，转储丢失 | 新增 `setup_cp_dump_capture()`：将 `/media/sdcard/cp_dump` bind-mount 到 `/sdcard`，使 CP dump 写入 SD 卡持久化 |
| 27 | ★ 轻微 | v1.27.5 | 初始化日志未打印 AP/CP 固件版本，移远分析日志时无法确认双侧固件一致性 | 新增 `get_zcgmr_safe()`（`AT*ZCGMR`→AP 固件）、`get_acgmr_safe()`（`AT*CGMR`→CP 固件）；提取 `print_init_info()` 函数统一初始化日志，新增 `[INIT] AP_FW:` 和 `[INIT] CP_FW:` 输出 |
| 28 | ★ 轻微 | v1.27.5 | SIM 断卡诊断、SIM 恢复诊断、恢复前快照三段内联代码块散落在状态机内，可读性差且不可复用 | 提取为三个独立函数：`log_sim_disconnect_diag()`、`log_sim_recovery_diag()`、`log_recovery_snapshot()`；同时将 `get_operator_safe` / `get_cfun_safe` / `get_cgdcont_safe` 标记为 `__attribute__((unused))`（初始化日志精简，不再默认打印） |
| 29 | ★ 无 | v1.27.5 | CP dump 目录存在多个 dump 文件时，启动日志输出多行（每文件一行），日志臃肿 | 改为单行合并：`[CPDUMP] Found N existing CP dump(s): file1, file2, ...` |

### 15.3 潜在残留问题（未修复）

| # | 严重程度 | 文件 | 问题描述 | 建议 |
|---|---------|------|---------|------|
| P1 | ★★ 中 | `dial.c` | `test_can_ping_google` 的 Ping 目标 `8.8.8.8` 硬编码，在 Google DNS 不可达区域（如部分电信用户）会误判断网 | 从配置文件读取，或同时 Ping 多目标（`8.8.8.8` + `114.114.114.114`），有一个成功即视为联网 |
| P2 | ★★ 中 | `data_call/data_call.c` | DISCONNECTED 时仅删除 `/tmp/resolv_v4/v6.conf`，不清空 `/etc/resolv.conf`，断网期间 DNS 解析仍使用过期 DNS 服务器 | DISCONNECTED 时同步清空 `/etc/resolv.conf` 或写入空内容 |
| P3 | ★ 轻 | `dial.c` | L1/L2 恢复路径中的 `sleep(2)`/`sleep(3)`/`sleep(10)` 阻塞主线程（含心跳日志暂停），日志出现无法预期的空白段 | 将 sleep 替换为基于 `get_time_ms()` 的非阻塞等待计时，或引入恢复子状态机 |
| P4 | ★ 轻 | `dial.c` | `data_call_service_error_cb` 中的 `exit(1)` 当前被注释，SDK 崩溃仅打日志不退出，保活脚本无法触发重启 | 确认 ql-sdk 崩溃后是否会自动 kill 本进程；若不会，取消 exit 注释 |
| P5 | ★ 轻 | `sim/sim.c` | SIM 槽位硬编码为 `QL_SIM_SLOT_1`，不支持双卡场景 | 参数化槽位，支持外部配置 |


---

## 16. 改进建议

### 16.1 短期可改进项（代码层面，改动小、收益高）

#### 16.1.1 Ping 目标可配置化

**现状**：`test_can_ping_google()` 硬编码 `ping -c 1 -W 3 8.8.8.8`，无法适配 Google DNS 不可达场景（如部分运营商屏蔽 8.8.8.8）。

**改进方案**：
```c
// 从 /usr/dial/dial.conf 读取 ping 目标列表
// 优先级：配置文件 > 内置默认
#define PING_TARGETS_DEFAULT "8.8.8.8,114.114.114.114"
// 同时 ping 两个目标，任一成功即视为联网
```

**理由**：114.114.114.114 是中国三大运营商均可达的 DNS，与 8.8.8.8 互备，覆盖率更高。

---

#### 16.1.2 断开时清空 /etc/resolv.conf

**现状**：DISCONNECTED 时仅 `unlink("/tmp/resolv_v4.conf")` 和 `unlink("/tmp/resolv_v6.conf")`，`/etc/resolv.conf` 保留过期 DNS，断网期间的 DNS 请求仍会发往已不可达的 DNS 服务器，造成解析超时（默认 5s × 3 次 = 15s 延迟）。

**改进方案**：
```c
// data_call/data_call.c DISCONNECTED 路径末尾
FILE *resolv = fopen("/etc/resolv.conf", "w");
if (resolv) { fclose(resolv); }  // 清空内容
```

**理由**：空 resolv.conf 会让 glibc 立即返回 `EAI_AGAIN`，上层应用快速感知断网，而非等待 15s 超时。

---

#### 16.1.3 Sleep 改为非阻塞计时

**现状**：L1 恢复中 `sleep(2)`，L2 恢复中 `sleep(3)` + `sleep(10)` 均阻塞主线程，期间心跳日志停止，日志分析出现无法解释的空白段，且阻塞期间无法响应 SIGINT。

**改进方案**：引入恢复子状态机，使用 `get_time_ms()` 非阻塞等待：
```c
// ST_RECOVERY 内部
enum RecoverySubState { RS_STOP, RS_WAIT_AFTER_STOP, RS_START, RS_WAIT_CFUN1 };
static uint64_t recovery_wait_ts = 0;
static enum RecoverySubState rs = RS_STOP;

case RS_WAIT_AFTER_STOP:
    if (get_time_ms() - recovery_wait_ts >= 2000) rs = RS_START;
    break;
```

**理由**：消除阻塞后，50ms 轮询节拍全程保持，SIGINT 可即时响应，心跳日志连续无断档。

---

#### 16.1.4 启用 SDK 崩溃退出

**现状**：`data_call_service_error_cb` 中：
```c
dial_log("[FATAL] SDK service error, exiting...\n");
// log_close();   // 被注释
// exit(1);        // 被注释
```
SDK 崩溃后进程继续运行但拨号功能完全失效，保活脚本无法重启。

**待确认事项**：ql-sdk 崩溃（ql_rild 进程 coredump）后是否会向 dial 进程发 SIGKILL？若否，取消注释 `exit(1)` 即可；若是，现有代码已足够。

---

#### 16.1.5 心跳日志增加 fast_retry 状态

**现状**：心跳日志不包含 `is_fast_fail_mode` 和 `g_dial_retry_count` 的当前值，难以实时判断进程处于哪个阶段。

**改进方案**：30s 心跳中追加：
```c
dial_log("... FastRetry=%d/RetryMode=%s\n", retry_count,
         is_fast_fail_mode ? "FAST" : "PERSISTENT");
```

---

### 16.2 长期架构改进方向

#### 16.2.1 双 SIM 卡支持

**现状**：`QL_SIM_SLOT_1` 硬编码，所有 SIM API 仅操作 slot 1。

**改进方向**：
- 参数化 `QL_SIM_SLOT_E slot`，通过配置文件或命令行选择主用 slot
- 在 L2/L3 恢复前尝试切换 slot（若硬件支持），提供更高可用性
- `apn_load_from_json` 基于切换后 slot 的 ICCID 重新匹配 APN

---

#### 16.2.2 健康状态 JSON 导出

**现状**：状态通过多个 `/tmp` 单值文件（`network_status`、`network_csq` 等）分散暴露，上层监控需要读多个文件。

**改进方向**：定期（如每 30s）将所有状态原子地写入单个 JSON 文件：
```json
{
  "timestamp": "2026-05-27T10:30:00",
  "pdp_connected": 1,
  "stage": "ST_PING",
  "fail_duration_s": 0,
  "recovery_level": 0,
  "signal_level": "GOOD",
  "csq": 20,
  "sim_ready": 1,
  "fast_retry_mode": false,
  "retry_count": 0,
  "l1_count": 2,
  "l2_count": 1,
  "last_recovery_ts": "2026-05-27T09:15:00"
}
```

**理由**：上层监控一次 read 即可获取所有状态，且 JSON 格式便于 luci/netifd 等工具解析。

---

#### 16.2.3 配置文件驱动（可配置化）

**现状**：大量时间常量（`LEVEL1_TIMEOUT`、`LEVEL2_TIMEOUT`、`FAST_FAIL_TIMEOUT_MS` 等）硬编码在源码中，不同部署场景（如信号极差区域需要更激进的 L3）无法运行时调整。

**改进方向**：创建 `/usr/dial/dial.conf`：
```ini
[timeout]
pdp_wait_ms = 60000
fast_fail_ms = 10000
level1_ms = 300000
level2_ms = 600000
level3_ms = 2100000

[ping]
targets = 8.8.8.8,114.114.114.114
fail_threshold = 3

[recovery]
enable_policy = 1
l1_cooldown_s = 60
l2_cooldown_s = 300
```

---

#### 16.2.4 IPv6 双栈优先策略

**现状**：`ql_data_call_set_ip_version` 设置为 IPv4 only，即使网络支持 IPv6 也不使用；DNS 合并写入顺序（v4 优先 v6）未经验证是否符合 glibc 解析优先级。

**改进方向**：
- 改为 `IPV4V6` 双栈模式
- 建立 IPv6/IPv4 路由优先级策略（RFC 6724 地址选择规则）
- 确认 `/etc/resolv.conf` 中 IPv6 nameserver 格式正确（`nameserver ::1` vs `nameserver 2001:...`）


---

## 17. 完整冷启动时序图

```
时间轴（纵轴=时间推移，单位秒）

保活脚本 (start_prog)
    │
    │  dial 进程启动
    ▼
t=0  ┌──────────────────────────────────────────────────────────────────────┐
     │ main() 开始                                                           │
     │  1. 写 /tmp/dial_version "Version: 1.27.04\r\n"                      │
     │  2. 注册 SIGCHLD 处理（回收孙进程，避免僵尸）                           │
     └──────────────────────────────────────────────────────────────────────┘
                         │
     ┌─────────────── wait_for_interface("ecm0") ────────────────────────┐
     │  每 1s 轮询 ioctl(SIOCGIFINDEX, "ecm0")                            │
     │  超时：30s → exit(-1) → 保活脚本重拉                               │
     └────────────────────────────────────────────────────────────────────┘
                         │ ecm0 出现
t=?  ┌──────────────────────────────────────────────────────────────────────┐
     │ read_config()        → 读 /tmp/dial_reboot.conf                     │
     │ check_sim_status()   → sim_init() + ql_sim 初始化                   │
     │ get_signal_csq()     → AT+CSQ 读信号（写 /tmp/network_csq）          │
     │ check_pid_running(-1)→ pgrep -x dial | wc -l > 1 → exit(单实例)    │
     │ read_exit_count()    → 读 /tmp/exit_count.txt                       │
     │   exit_count >= 20? → restart_cfun()                               │
     │     restart_cfun():   AT+CFUN=0 → sleep(5) → AT+CFUN=1            │
     │                        检查：次数上限10次 + 600s间隔                 │
     └──────────────────────────────────────────────────────────────────────┘
                         │
     ┌─────────────── log_init() ─────────────────────────────────────────┐
     │  1. 检查 /proc/mounts 中 /media/sdcard 是否存在                     │
     │  2. statvfs() 检查剩余空间 ≥ 500MB                                  │
     │  3. 创建 /media/sdcard/dial_log/dial_YYYYMMDD_HHMMSS.log            │
     │  SD 不可用 → 降级为控制台输出，写 /tmp/sdcard_avl = "000"            │
     └────────────────────────────────────────────────────────────────────┘
                         │
     ┌─────────────── setup_cp_dump_capture() ────────────────────────────┐
     │  1. 检查 /sdcard 是否已 bind-mount（stat st_dev 比较）               │
     │  2. 未挂载：mkdir -p /sdcard + mount --bind /media/sdcard/cp_dump   │
     │  目的：CP dump 写 /sdcard 时自动落到 SD 卡                           │
     └────────────────────────────────────────────────────────────────────┘
                         │
     ┌─────────────── test_can_ping_google(NULL) ──────────────────────────┐
     │  Ping 8.8.8.8 一次（TTL 检测）                                       │
     │  ├─ 成功（已有网络）：进入保活循环                                     │
     │  │    while(1): ping 每 6s，连续失败 10 次 → exit(1) → 重拉          │
     │  └─ 失败（无网络）：进入 dial_loop()                                  │
     └────────────────────────────────────────────────────────────────────┘
                         │ 进入 dial_loop()
t=?  ┌──────────────────────────────────────────────────────────────────────┐
     │ ql_data_call_init() 重试循环（最多 200 次，每次 100ms，约 20s）        │
     │  返回 -1001 → 继续重试；每 20 次打印进度                              │
     │  返回 0    → 成功，继续                                              │
     │  超时 200 次 → 打日志但继续（不退出）                                  │
     └──────────────────────────────────────────────────────────────────────┘
                         │
     ┌──────────── 注册所有回调 ──────────────────────────────────────────┐
     │  ql_data_call_set_status_ind_cb(data_call_status_ind_cb)          │
     │  ql_data_call_set_service_error_cb(data_call_service_error_cb)    │
     │  ql_sim_set_card_status_cb(sim_card_status_cb)                    │
     │  ql_sim_get_card_info(QL_SIM_SLOT_1, &info)  ← 主动获取初始状态    │
     └────────────────────────────────────────────────────────────────────┘
                         │
     ┌──────────── SIM & 网络信息采集 ────────────────────────────────────┐
     │  sim_get_iccid()   → 读 ICCID（20位）                              │
     │  AT+CIMI           → 读 IMSI（15位）                               │
     │  AT+QGMR           → 读固件版本                                    │
     │  AT+CEREG?         → 读注册状态（g_last_reg_stat）                  │
     │  set_apn(iccid)    → JSON 精确匹配 → ql_data_call_set_apn_config   │
     │                      未匹配 → 默认 APN "apnpublic"                 │
     └────────────────────────────────────────────────────────────────────┘
                         │
     ┌──────────── 创建并启动数据连接 ────────────────────────────────────┐
     │  ql_data_call_create(4, "auto_network", 0)                        │
     │  ql_data_call_param_alloc() → 配置 apn_id=1, IPv4, reconnect=25s  │
     │  ql_data_call_config(4, param)                                    │
     │  ql_data_call_param_free()                                        │
     │  ql_data_call_start(4)  ← 异步，立即返回                           │
     └────────────────────────────────────────────────────────────────────┘
                         │
                         │ [async] SDK 内部发起拨号
                         ▼
     ┌──────────── data_call_status_ind_cb: CONNECTED ────────────────────┐
     │  g_if_name = "ccinet1"（或其他接口名）                              │
     │  g_pdp_connected = 1                                               │
     │  ip route del default dev <old_dev>（清旧路由）                    │
     │  ip route add default via <gw> dev ccinet1                        │
     │  iptables -t nat -A POSTROUTING -o ccinet1 -j MASQUERADE          │
     │  写 /tmp/resolv_v4.conf（LF 换行）                                 │
     │  合并写 /etc/resolv.conf                                           │
     │  /tmp/network_status = 1 / /tmp/dial_Status = 0                   │
     └────────────────────────────────────────────────────────────────────┘
                         │
     ┌──────────── 主循环检测 g_pdp_connected=1 ──────────────────────────┐
     │  [INIT] "PDP connected, start Ping window (10000ms)."              │
     │  check_and_update_retry_count() → is_fast_fail_mode=true（若 <3次）│
     └────────────────────────────────────────────────────────────────────┘
                         │
     ┌──────────── Fast Retry Ping 窗口（≤10s）────────────────────────────┐
     │  ST_PING → test_can_ping_google()                                  │
     │  ├─ 成功：                                                          │
     │  │    on_network_connected() 回调                                   │
     │  │    clear_retry_count() → 删 /tmp/dial_retry_count               │
     │  │    is_fast_fail_mode = false（进入持久监控模式）                   │
     │  │    [INIT] "Network is up! Entering persistent monitoring."       │
     │  └─ 失败 >10s：exit(1) → 保活脚本重拉（下次 attempt 2/3 或 3/3）    │
     └────────────────────────────────────────────────────────────────────┘
                         │ 进入持久监控
     ┌──────────── 持久监控循环（永不退出，50ms/周期）────────────────────────┐
     │  每 30s：心跳日志                                                   │
     │    "[HB] SIM_AT=READY/SIM_CB=1 REG=1 CSQ=20 TEMP=35 DownTime=0s"  │
     │  每 5min：扩展心跳（联网后启动）                                     │
     │    "[EXT] CESQ/小区信息/IP地址"                                      │
     │  持续：ST_STATUS→ST_SIM→ST_SIGNAL→ST_PING 循环                    │
     │  故障时：分级恢复（见 §3 状态机）                                    │
     └────────────────────────────────────────────────────────────────────┘
```


---

## 18. 关键流程时序图

### 18.1 断网三级恢复完整时序（v1.27 阈值）

```
时间轴（横轴=故障持续时间，从 start_fail_ts 开始）

t=0      t=5min   t=10min         t=35min
│        │        │               │
▼        ▼        ▼               ▼
─────────┬────────┬───────────────┬──────────────────────────────
         │        │               │
 [L0]    │ [L1]   │  [L2]         │ [L3]
SDK自愈期 │ 软重拨期│  射频重置期   │ 模块重启

────────────────────────────────────────────────────────────────────

[L0 期：0~5min，不干预，等 SDK 自动重连]
                                          SDK 内部每 25s 尝试重连
  Ping 失败 ────►────►────►────►────►── （ping_fail_count 累积到 3 才启动计时）
                                          └─ 恢复 → "[EVENT] Network Recovered in SDK phase (L0)"
                                              ├─ clear start_fail_ts
                                              ├─ clear recovery_level
                                              └─ diag_snapshot("recovery")

[L1：>5min，REG≠0（已注册网络）]
  t=5min ─────────────────────────────────────────────────────────────────────
         [ALARM] L1 recovery triggered (fail=5m, level=1)
         ql_data_call_stop(4)
         sleep(2)  ← 阻塞主线程 2s
         ql_data_call_start(4)
         last_l1_ts = now  ← 60s 内不重复触发
         │
         │ (60s 后可再次触发)
         │
         t≈6min [ALARM] L1 recovery...（若 Ping 仍失败）
         ... 以此类推直到 t=10min

  [特殊：REG=0（注册丢失）→ 跳过 L1，直接等 L2]
         REG=0 时 L1 也会失败（ql_data_call_start 返回 -1001）
         所以检测到 REG=0 → 跳过 stop/start，更新 last_l1_ts

[L2：>10min（正常路径），或 >5min（REG=0 路径）]
  t=10min ────────────────────────────────────────────────────────────────────
         [ALARM] L2 recovery triggered (fail=10m, level=2)
         diag_snapshot("before_l2")          ← 先保存快照
         executeATCommand("serial_atcmd at+ceer")   ← 记录错误原因
         executeATCommand("serial_atcmd at+cgact?") ← 记录 PDP 状态
         executeATCommand("serial_atcmd at+cfun=0") ← 关闭射频（飞行模式）
         sleep(3)                                    ← 等待射频关闭
         executeATCommand("serial_atcmd at+cfun=1") ← 开启射频
         sleep(10)                                   ← 等待网络重注册
         ql_data_call_start(4)
         last_l2_ts = now  ← 5min 内不重复触发（REG=0 路径：90s）
         last_l1_ts = now  ← 同时更新 l1 计时（避免 l2 后立即触发 l1）
         │
         t≈15min [ALARM] L2 recovery...（若 Ping 仍失败，5min 冷却后）
         ... 以此类推直到 t=35min

[L3：>35min]
  t=35min ───────────────────────────────────────────────────────────────────
         [ALARM] L3 recovery triggered (fail=35m, level=3)
         executeATCommand("serial_atcmd at+cfun=1,1")
         └─ ASR1803 SoC 整体复位
            CP（蜂窝协议栈）复位
            AP（Linux）复位
         sleep(10)  ← 实际上无意义（Linux 已重启，sleep 被打断）
         exit(1)   ← 由保活脚本重拉进程，并触发完整冷启动流程
```

### 18.2 SIM 瞬断恢复特殊路径

```
正常运行（Ping 通，start_fail_ts=0）
       │
       │  [SIM 瞬断]（物理接触不良、ESD 等）
       ▼
sim_card_status_cb:
  g_sim_app_ready = 0
  g_sim_app_state = NOT_READY（或 SIM_ABSENT）

主循环 ST_SIM 检测：
  SIM_CB=NOT_READY → "[WARN] SIM disconnected (cb_state=...)"
  sim_error_active = 1

（此时 Ping 可能仍在超时计时，若 start_fail_ts=0 则重新开始）

       │ SIM 插回/恢复
       ▼
sim_card_status_cb:
  g_sim_app_ready = 1
  g_sim_app_state = READY

主循环 ST_SIM 检测：
  sim_error_active=1 → "[INFO] SIM recovered"
  sim_error_active = 0

SIM 恢复后的 L0/L1 判断：
  ├─ start_fail_ts=0（Ping 未超过 3 次失败）→ 不执行 L1，等 SDK 重建连接
  └─ start_fail_ts>0 且 fail_duration > LEVEL1_TIMEOUT → 立即强制 L1
       ql_data_call_stop(4)
       sleep(2)
       ql_data_call_start(4)
       "[INFO] SIM recovered during L1+ phase, forcing L1 restart"
```

### 18.3 Fast Retry 三次失败 → 持久模式完整时序

```
start_prog 启动 dial（第 1 次）
       │
       ▼
check_and_update_retry_count():
  /tmp/dial_retry_count 不存在 → count=0 → is_fast_fail_mode=true
  写入 count=1

dial_loop() 初始化... ql_data_call_start(4)

─── 场景 A：PDP 一直建不起来 ─────────────────────────────────────────
                    │ g_pdp_connected 持续为 0
                    │ 超过 60s（PDP_WAIT_TIMEOUT_MS）
                    ▼
       "[INIT] Fast Fail: PDP not established in 60s. Exiting..."
       exit(1)

─── 场景 B：PDP 建起但 Ping 不通 ────────────────────────────────────
       │ g_pdp_connected=1，启动 Ping 窗口（10s）
       │
       │ Ping 持续失败...超过 10s
       ▼
       "[INIT] Fast Fail: PDP up but no Ping success in 10s. Exiting..."
       exit(1)

─── 以上两种情况均触发 exit(1) ─────────────────────────────────────

保活脚本 sleep(15s) 后重拉（第 2 次）
  count=1 → is_fast_fail_mode=true，打印 "Attempt 2/3"
  写入 count=2
  ...同上，若失败 → exit(1)

保活脚本 sleep(15s) 后重拉（第 3 次）
  count=2 → is_fast_fail_mode=true，打印 "Attempt 3/3"
  写入 count=3
  ...同上，若失败 → exit(1)

保活脚本 sleep(15s) 后重拉（第 4 次）
  count=3 → is_fast_fail_mode=false（进入持久模式）
  打印 "Retry count >= 3, entering persistent loop mode"
  ← 不再因 PDP 超时或 Ping 失败而 exit
  ← 状态机进入分级恢复流程（L0→L1→L2→L3）

─── 持久模式成功拨通 ──────────────────────────────────────────────
  Ping 成功 → clear_retry_count()（删 /tmp/dial_retry_count）
  ← 下次断网后重启，Fast Retry 从头开始（attempt 1/3）

三次 Fast Retry 总耗时估算：
  每次：PDP 超时 60s（或 Ping 超时 10s）+ 保活等待 15s ≈ 25s（Ping 失败场景）
  3 次 ≈ 75s 后进入持久模式
```

### 18.4 diag_snapshot() 诊断快照时序

```
触发条件一：ping_fail_count 首次 >= PING_FAIL_THRESHOLD(3)
       且 diag_snap_done = 0（当前故障周期未快照）

触发条件二：网络恢复（Ping 成功且 start_fail_ts > 0）

─── 快照写入流程 ────────────────────────────────────────────────────
       │
       ▼
diag_snapshot(label)：  label="fault" 或 "recovery"
  1. 获取当前时间戳 → 生成文件名前缀 "YYYYMMDD_HHMMSS"
  2. 执行 dmesg → 写 /media/sdcard/diag/dmesg_<label>_<ts>.log
  3. 执行 logcat -d → 写 /media/sdcard/diag/logcat_<label>_<ts>.log
  4. cleanup_dir_keep_newest("/media/sdcard/diag/", 5)：
       ├─ 扫描目录所有文件，按 mtime 排序
       └─ 保留最新 5 个，删除更旧的（防止 SD 卡被日志填满）

─── 多次快照间隔保护 ────────────────────────────────────────────────
  diag_snap_done = 1（fault 快照后设置）
  恢复快照触发后 → diag_snap_done = 0（为下一个故障周期复位）
  ← 保证每个故障周期只生成一次 fault 快照 + 一次 recovery 快照
```

---

## 19. 总结

`open_dial` 是一个在资源受限嵌入式 Linux（ASR1803 SoC，单核 Cortex-A7）上运行的蜂窝网络守护进程，核心设计哲学是**可靠性优先、渐进式恢复**。

**架构亮点**：
1. **双重保障机制**：SDK 底层 25s 自动重连（L0 期）+ 应用层分级恢复（L1/L2/L3）互补，`enable_policy_recovery=0` 可切换为纯 SDK 模式
2. **Fast Retry 快速失败**：启动时 3 次快速试探，失败超过阈值则转入持久模式，避免反复重启无效循环
3. **事件驱动 + 轮询混合**：SDK 回调驱动连接状态变化，主循环轮询驱动 Ping 检测和状态机，两者通过 `volatile` 标志解耦
4. **诊断能力**：CP dump 捕获 + diag_snapshot 快照 + 双值心跳日志，为远程问题分析提供充分数据

**主要演进轨迹（V1.26→V1.27.5）**：
- V1.26：修复 Fast Retry 误判、路由残留、DNS CRLF、call_id 过滤、json-c 内存、L3 失效等 19 个严重 Bug
- V1.27：优化分级阈值（5/10/35min），SIM 改 SDK 回调，REG=0 快速 L2 路径
- V1.27.1：SDK 崩溃回调诊断
- V1.27.2：心跳双值 SIM 状态
- V1.27.3：diag_snapshot 快照机制
- V1.27.4：CP dump bind-mount 持久化
- V1.27.5：新增 AP/CP 双侧固件版本打印（`AT*ZCGMR` / `AT*CGMR`），重构初始化日志为 `print_init_info()`，提取三个诊断辅助函数，精简初始化日志输出项

**待解决**：Ping 目标可配置化（P1）、断开时清空 resolv.conf（P2）、sleep 改非阻塞（P3）、SDK 崩溃 exit 启用（P4）是优先级最高的短期改进项。

<!-- GENERATION_COMPLETE: 2026-05-27 -->

