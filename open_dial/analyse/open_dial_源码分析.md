# open_dial 源码全面分析文档

> **版本**：V1.26（main=1, sub=26）  
> **目标平台**：嵌入式 Linux（Quectel 移远通信模组，如 EC200A、EG25）  
> **语言**：C（使用 Quectel ql-sdk API）  
> **核心功能**：4G/5G 蜂窝模组自动拨号守护进程，带自适应 APN、分级故障恢复、心跳监控

---

## 目录

1. [项目概述](#1-项目概述)
2. [目录结构](#2-目录结构)
3. [整体架构图](#3-整体架构图)
4. [模块详解](#4-模块详解)
   - [4.1 主程序 dial.c](#41-主程序-dialc)
   - [4.2 拨号状态机 dial_loop](#42-拨号状态机-dial_loop)
   - [4.3 APN 自适应模块 apn/](#43-apn-自适应模块-apn)
   - [4.4 数据连接模块 data_call/](#44-数据连接模块-data_call)
   - [4.5 网络状态模块 nw/](#45-网络状态模块-nw)
   - [4.6 SIM 卡模块 sim/](#46-sim-卡模块-sim)
   - [4.7 工具函数 misc.c](#47-工具函数-miscc)
   - [4.8 重启配置模块 reboot_conf/](#48-重启配置模块-reboot_conf)
   - [4.9 SD 卡日志模块 logger_sd.c](#49-sd-卡日志模块-logger_sdc)
5. [核心流程详解](#5-核心流程详解)
   - [5.1 启动流程](#51-启动流程)
   - [5.2 拨号状态机循环](#52-拨号状态机循环)
   - [5.3 分级故障恢复策略](#53-分级故障恢复策略)
   - [5.4 快速失败重试机制](#54-快速失败重试机制)
6. [关键数据结构](#6-关键数据结构)
7. [关键文件与路径速查表](#7-关键文件与路径速查表)
8. [外部依赖与 SDK 接口](#8-外部依赖与-sdk-接口)
9. [编译与部署](#9-编译与部署)
10. [设计亮点与注意事项](#10-设计亮点与注意事项)

---

## 1. 项目概述

`open_dial` 是一个运行在移远通信（Quectel）模组 Linux 系统上的**蜂窝网络自动拨号守护进程**。

**核心目标**：开机后自动完成蜂窝数据连接（4G/5G），并在断网时自动恢复，确保设备始终在线。

**主要能力**：

| 能力 | 说明 |
|------|------|
| 自适应 APN | 根据 SIM 卡 ICCID 自动匹配运营商 APN，从 `apn.json` 读取 |
| 自动拨号 | 使用 Quectel ql-sdk 的 `ql_data_call` API 建立数据连接 |
| 网络心跳监控 | 每 30 秒记录一次 SIM 状态、注册状态、信号强度、断网时长 |
| 分级故障恢复 | 断网 60s→软重拨；断网 5min→射频重置；断网 30min→模块重启 |
| 快速失败重试 | 前 3 次启动若 10 秒内未连上网则快速退出，让保活脚本拉起重试 |
| SD 卡日志 | 若 `/media/sdcard` 挂载且剩余 ≥500MB，则将运行日志写入 SD 卡 |
| 网络状态广播 | 将网络状态（0/1）写入 `/tmp/network_status` 供其他进程读取 |
| 单实例保证 | 通过 `pgrep` 检测自身进程数防止重复运行 |

---

## 2. 目录结构

```
open_dial/
├── dial.c                   # ★ 主程序入口 + 核心拨号循环
├── dial.h                   # 拨号管理结构体、状态枚举定义
├── _public.h                # 全局头文件（统一包含所有依赖）
├── Makefile                 # 构建脚本
├── README.md                # 版本历史说明
│
├── apn/
│   ├── apn.c               # APN 加载与配置（从 JSON 文件匹配 ICCID）
│   └── apn.h               # apn_obj_t 结构体定义
│
├── data_call/
│   ├── data_call.c         # 数据连接回调处理 + 路由/DNS 配置 + 流量监控
│   └── data_call.h
│
├── nw/
│   ├── nw.c                # 网络状态查询、信号强度、注册状态
│   └── nw.h
│
├── sim/
│   ├── sim.c               # SIM 卡初始化、ICCID/IMSI 读取
│   └── sim.h
│
├── misc.c                   # 通用工具函数（AT 命令、进程管理、状态文件读写）
├── misc.h
│
├── reboot_conf/
│   ├── dial_reboot_conf.c  # 配置持久化、信号强度读取、前置条件检查
│   └── dial_reboot_conf.h
│
├── logger_sd.c              # SD 卡日志（带挂载检测 + 空间检查 + 时间戳文件名）
├── logger_sd.h
│
├── test_utils/
│   ├── test_utils.c         # 测试菜单框架（用于调试）
│   └── test_utils.h
│
└── cc_deque/
    ├── cc_deque.c           # 双端队列实现
    ├── cc_deque.h
    ├── cc_common.c
    └── cc_common.h
```

---

## 3. 整体架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                        open_dial 进程                            │
│                                                                   │
│  main()                                                           │
│  ├─ 等待 ecm0 网卡就绪 (最多30s)                                  │
│  ├─ read_config() → 读取重启配置                                   │
│  ├─ check_sim_status() → SIM 初始化                               │
│  ├─ get_signal_csq() → 读取初始信号强度                            │
│  ├─ 单实例检查 (pgrep dial)                                        │
│  ├─ 读取 exit_count，若≥20 则执行 restart_cfun()                  │
│  ├─ log_init() → 初始化 SD 卡日志                                  │
│  ├─ 快速 Ping 检测：已联网则进入监控循环，掉线后才拨号              │
│  └─ dial_loop() ─────────────────────────────────────┐           │
│                                                       │           │
│  ┌────────────────────────────────────────────────────┼──────┐   │
│  │              拨号状态机 dial_loop()                 │      │   │
│  │                                                    ▼      │   │
│  │  初始化:                                                   │   │
│  │  ├─ ql_data_call_init() → 初始化数据拨号服务               │   │
│  │  ├─ ql_data_call_set_status_ind_cb() → 注册状态回调        │   │
│  │  ├─ sim_init() + sim_get_iccid() → 读取 ICCID             │   │
│  │  ├─ set_apn(iccid) → 从 apn.json 匹配并设置 APN           │   │
│  │  ├─ ql_data_call_create() → 创建数据连接实例               │   │
│  │  ├─ ql_data_call_config() → 配置参数(APN ID、重连间隔)     │   │
│  │  └─ ql_data_call_start() → 启动拨号                        │   │
│  │                                                            │   │
│  │  主循环 (每50ms轮询):                                       │   │
│  │  ├─ 快速失败检测 (前3次启动，10s超时)                       │   │
│  │  ├─ 心跳日志 (每30s: SIM/REG/CSQ/DownTime)                 │   │
│  │  └─ 业务状态机:                                            │   │
│  │     ST_STATUS → ST_SIM → ST_SIGNAL → ST_PING              │   │
│  │                                          │                 │   │
│  │                               Ping失败并超时               │   │
│  │                                          ↓                 │   │
│  │                                    ST_RECOVERY             │   │
│  │                              Level1/Level2/Level3          │   │
│  └────────────────────────────────────────────────────────────┘   │
│                                                                   │
│  data_call_status_ind_cb() (SDK回调)                              │
│  ├─ CONNECTED: 设置路由、iptables NAT、写 resolv.conf            │
│  └─ DISCONNECTED: 更新 /tmp/network_status = 0                   │
└─────────────────────────────────────────────────────────────────┘
          │                          │
          ▼                          ▼
   Quectel ql-sdk               /tmp/ 状态文件
   (ql_data_call.h              /tmp/network_status
    ql_sim.h                    /tmp/dial_version
    ql_nw.h)                    /tmp/exit_count.txt
                                /tmp/dial_retry_count
                                /tmp/cfun_count.txt
```

---

## 4. 模块详解

### 4.1 主程序 dial.c

`dial.c` 既是 **程序入口**，也包含了 **拨号循环主逻辑**（`dial_loop`）和所有 AT 命令查询辅助函数。

#### main() 流程

```
main()
 │
 ├─ 1. 等待 ecm0 网卡 (wait_for_interface("ecm0", 30))
 │      每秒轮询 ioctl(SIOCGIFINDEX)，超时30s返回-1退出
 │
 ├─ 2. read_config()       读取 /tmp/dial_reboot.conf 中的 uptime/restart_flag
 │   check_sim_status()    调用 sim_init() + sim_get_iccid()
 │   get_signal_csq()      通过 serial_atcmd at+csq 获取信号
 │   print_config()        打印当前配置状态
 │
 ├─ 3. 单实例保证
 │      check_pid_running(-1) → pgrep dial | wc -l >= 2 则退出
 │
 ├─ 4. 设置 SIGCHLD 处理器 (防止子进程变僵尸)
 │
 ├─ 5. 读取 exit_count (/tmp/exit_count.txt)
 │      若 exit_count >= 20 → restart_cfun() 重置射频，重置计数
 │
 ├─ 6. 写版本文件 write_version_log("/tmp/dial_version", 1, 26)
 │
 ├─ 7. log_init()           初始化 SD 卡日志
 │
 ├─ 8. 快速 Ping 检测
 │      if (test_can_ping_google(NULL)) {
 │          // 已连网：进入保持循环，每6s ping一次
 │          // 连续10次失败才退出循环，重新拨号
 │      }
 │
 └─ 9. dial_loop(on_network_connected, NULL)   进入拨号主循环
```

#### 关键辅助函数（dial.c 内）

| 函数 | 说明 |
|------|------|
| `test_can_ping_google(ifname)` | 用 popen+ping 判断网络连通性，检测 ttl= 字符串 |
| `get_csq_value_safe()` | 执行 `serial_atcmd at+csq`，解析 +CSQ: 值 |
| `get_cereg_status_safe()` | 执行 `serial_atcmd at+cereg?`，解析注册状态码 |
| `get_qiact_status_safe(ip_buf)` | 执行 `serial_atcmd at+qiact?`，检查 PDP 上下文激活状态 |
| `get_cpin_status_str(out_buf, len)` | 执行 `serial_atcmd at+cpin?`，返回 SIM 状态字符串 |
| `wait_for_interface(ifname, timeout)` | 阻塞等待网卡出现（ioctl轮询） |
| `restart_cfun()` | 发送 AT+CFUN=0/1，有调用次数上限（10次）和最小间隔（600s）保护 |

---

### 4.2 拨号状态机 dial_loop

`dial_loop` 是程序的核心，采用**非阻塞状态机**设计，主循环每 50ms 执行一次。

#### 状态定义

```c
enum Stage {
    ST_STATUS   = 0,   // 入口/过渡状态
    ST_SIM      = 1,   // 检查 SIM 卡状态
    ST_SIGNAL   = 2,   // 检查信号强度（当前为直通）
    ST_PING     = 3,   // Ping 测试网络连通性
    ST_RECOVERY = 4,   // 执行故障恢复动作
};
```

#### 状态转换图

```
         ┌──────────────────────────────────────────┐
         │                                          │
         ▼                                          │ 恢复完成
    ST_STATUS                                       │
         │                                     ST_RECOVERY
         ▼                                    ▲    │
      ST_SIM ──→ 无论结果 ──→ ST_SIGNAL        │    │
                                   │           │    │
                                   ▼           │    │
                               ST_PING         │    │
                              /        \       │    │
                        Ping成功    Ping失败    │    │
                          │          │         │    │
                    重置计时器   开始/累计计时   │    │
                    触发回调     超过阈值?───────┘    │
                     回ST_STATUS  (L1/L2/L3)         │
                                                     │
                              各级恢复执行后──────────┘
```

#### 时间阈值参数

| 参数 | 值 | 说明 |
|------|----|------|
| PING_INTERVAL_MS | 1500ms | Ping 检测间隔 |
| SIM_INTERVAL_MS | 800ms | SIM 检测间隔 |
| SIG_INTERVAL_MS | 800ms | 信号检测间隔 |
| HEARTBEAT_INTERVAL_MS | 30000ms | 心跳日志间隔 |
| LEVEL1_TIMEOUT | 60s | 软重拨触发阈值 |
| LEVEL2_TIMEOUT | 5min | 射频重置触发阈值 |
| LEVEL3_TIMEOUT | 30min | 模块重启触发阈值 |
| FAST_FAIL_TIMEOUT_MS | 10000ms | 快速失败超时 |
| reconnect_interval | 25s | SDK 底层自动重连间隔 |

---

### 4.3 APN 自适应模块 apn/

**功能**：根据 SIM 卡 ICCID 自动匹配运营商 APN 配置，避免硬编码。

#### apn_obj_t 结构体

```c
typedef struct {
    char iccid[32];     // SIM 卡 ICCID 前缀（用于匹配）
    char apn[32];       // APN 名称，如 "3gnet"
    char usr_name[32];  // 用户名（部分运营商需要）
    char pwd[32];       // 密码
} apn_obj_t;
```

#### apn.json 格式示例

```json
{
  "apn": [
    {
      "supplier": "中国移动",
      "apn": "cmnet",
      "iccid": "898600",
      "usrname": "",
      "pwd": ""
    },
    {
      "supplier": "中国联通",
      "apn": "3gnet",
      "iccid": "898601",
      "usrname": "",
      "pwd": ""
    }
  ]
}
```

- 文件路径：`/usr/dial/apn.json`
- 匹配逻辑：`strncmp(iccid前缀, 实际ICCID, 前缀长度)` 前缀匹配
- 若未匹配到：使用默认 APN `"apnpublic"`（`APN_NAME_PUBLIC`）

#### set_apn 流程

```
set_apn(iccid)
 ├─ 拼接路径 "/usr/dial/apn.json"
 ├─ apn_load_from_json() → 解析 JSON，返回 apn_obj_t 数组
 ├─ 遍历数组，strncmp ICCID 前缀匹配
 │   ├─ 命中 → 设置 apn_name / username / password
 │   └─ 未命中 → 使用 "apnpublic" 默认 APN
 └─ ql_data_call_set_apn_config(DATA_CALL_APN_PUBLIC, &cfg)
```

---

### 4.4 数据连接模块 data_call/

#### data_call_status_ind_cb（连接状态回调）

这是数据连接的核心回调，当 SDK 检测到连接状态变化时触发。

```
data_call_status_ind_cb(call_id, pre_status, p_msg)
 │
 ├─ if CONNECTED:
 │   ├─ update_network_status(1)
 │   ├─ 配置系统路由：ip ro add default via <gateway> dev <device>
 │   ├─ 配置 iptables NAT：iptables -t nat -A POSTROUTING -o <dev> -j MASQUERADE
 │   ├─ 写入 DNS：/tmp/resolv_v4.conf → 合并到 /etc/resolv.conf
 │   └─ (IPv6同理处理 addr6 / resolv_v6.conf)
 │
 └─ if DISCONNECTED:
     └─ update_network_status(0)
```

#### flow_monitor_task（流量监控）

监控网卡 rx_packets 统计，若 120 秒内无收包则返回 `dial_stat_stop`，触发重拨。流量统计读取路径：`/sys/devices/virtual/net/<ifname>/statistics/rx_packets`。

---

### 4.5 网络状态模块 nw/

提供对 Quectel ql-sdk 网络 API 的封装，主要用于信号和注册状态查询。

| 函数 | 说明 |
|------|------|
| `get_signal_strength(level)` | 调用 `ql_nw_get_signal_strength`，将等级写入 `/tmp/network_csq` |
| `nw_get_if_statistics_rx_packets` | 读取网卡 rx_packets 统计 |
| `nw_mark_network_status(status)` | 写 0/1 到 `/tmp/network_status` |
| 注册回调 | `nw_voice_reg_event_ind_cb`, `nw_data_reg_event_ind_cb`（事件通知） |

支持信号制式：GSM / WCDMA / TD-SCDMA / LTE / NR5G / CDMA / HDR

---

### 4.6 SIM 卡模块 sim/

对 Quectel `ql_sim` API 的简单封装。

| 函数 | 说明 |
|------|------|
| `sim_init()` | 调用 `ql_sim_init()`，返回 QL_ERR_OK 或 -1 |
| `sim_get_iccid(iccid)` | 读取 SIM1 的 ICCID，长度 QL_SIM_ICCID_LENGTH+1 |
| `sim_get_imsi(imsi)` | 读取 IMSI（UNKNOWN app_type 模式） |

**注意**：代码中 SIM 槽位和应用类型均为硬编码（SLOT_1 / APP_TYPE_UNKNOWN），如需适配双卡需修改。

---

### 4.7 工具函数 misc.c

提供全局通用的工具函数。

| 函数 | 说明 |
|------|------|
| `executeATCommand(cmd)` | popen 执行命令，动态拼接返回完整输出 |
| `extractSignalValue(input)` | 从字符串中解析 `+CSQ:` 后的信号值 |
| `checkSimCardStatus()` | AT+CREG? 和 AT+CPIN? 组合检查，返回 0/1/2 |
| `isModuleOK()` | 发送 AT，检查是否返回 OK |
| `check_process(name)` | pgrep name | wc -l，返回进程数量 |
| `isIdleState()` | 检测 wlan0 是否有 WiFi 客户端连接（AP/STA模式判断） |
| `restartNetworkServices()` | 仅在 WiFi 空闲时 fork 重启 ql_rild/ql_netd |
| `restart_ql_netd()` | 专门重启 ql_netd 守护进程 |
| `update_network_status(status)` | 读取 /tmp/network_status 当前值，仅在变化时写入 |
| `read_exit_count()` / `write_exit_count(n)` | 持久化异常退出计数到 /tmp/exit_count.txt |
| `runCommand(cmd, out, size)` | popen 运行命令，输出写入指定 buffer |
| `parseOutput(out, key, val, size)` | 从命令输出中提取 key=value 对 |

---

### 4.8 重启配置模块 reboot_conf/

持久化运行配置，防止频繁重启导致循环。

#### Config 结构体

```c
typedef struct {
    int uptime;           // 程序启动时的系统运行时间（秒）
    int restart_flag;     // 是否已执行过重启（0=未重启, 1=已重启）
    int first_disconnect; // 首次断网标志（运行时使用，不持久化）
    int signal_strength;  // 当前信号强度（CSQ值）
    int sim_status;       // SIM卡状态（ql_sim_init 返回值）
    int ql_netd_status;   // ql_netd 运行状态
} Config;
```

- 配置文件路径：`CONFIG_FILE`（通常为 `/tmp/dial_reboot.conf`）
- 格式：`uptime,restart_flag`（CSV）
- `check_pre_conditions()`：信号 > 20 且 SIM 正常且 ql_netd 运行才触发重启逻辑
- `perform_reboot()`：写重启标志文件 → sleep 10s → `system("reboot")`

---

### 4.9 SD 卡日志模块 logger_sd.c

**功能**：将拨号运行日志写入 SD 卡，便于离线排查问题。

#### 初始化逻辑

```
log_init()
 ├─ check_sdcard_available()
 │   ├─ is_sdcard_mounted()  → 扫描 /proc/mounts 查找 /media/sdcard
 │   └─ statvfs() 检查可用空间 >= 500MB
 │
 ├─ 若不可用 → 降级为仅控制台输出
 │
 └─ 若可用:
     ├─ mkdir -p /media/sdcard/dial_log
     └─ fopen("dial_YYYYMMDD_HHMMSS.log", "w")  按时间戳命名
```

#### dial_log 函数

```c
void dial_log(const char *fmt, ...);
```

- 同时输出到 stdout 和日志文件
- 每条日志前缀：`[YYYY-MM-DD HH:MM:SS]`
- 每次写入后立即 `fflush()`，防止掉电丢日志

---

## 5. 核心流程详解

### 5.1 启动流程

```
系统开机
    │
    ▼
保活脚本启动 dial 进程
    │
    ▼
等待 ecm0 网卡 (最多30秒)
    │ 不存在 → exit(-1)，保活脚本重新拉起
    │ 存在 ↓
    ▼
读取配置 + SIM初始化 + 读取信号强度
    │
    ▼
单实例检查 (pgrep dial >= 2 则退出)
    │
    ▼
读取 exit_count:
    ├─ >= 20: 执行 AT+CFUN=0/1 重置射频，重置计数为0
    └─ < 20: 继续
    │
    ▼
初始化日志系统 (SD卡/控制台)
    │
    ▼
快速Ping检测 (test_can_ping_google):
    ├─ 能Ping通: 进入网络保持循环 (每6s ping，10次失败后退出)
    └─ 不通: 直接进入 dial_loop()
    │
    ▼
dial_loop() ← 核心拨号循环（永不退出，除非快速失败模式）
```

### 5.2 拨号状态机循环

```
dial_loop() 初始化:
    ql_data_call_init() → SIM初始化 → set_apn() → 创建并启动数据连接

    每 50ms 执行一次主循环:
    ┌─────────────────────────────────────────────────────────┐
    │                                                         │
    │  [心跳日志] 每30s: 打印 SIM/REG/CSQ/断网时长            │
    │                                                         │
    │  [快速失败检测] 前3次启动 && 未连接成功:                  │
    │    if 已过10s → exit(1)，让保活脚本重拉                  │
    │                                                         │
    │  [业务状态机]:                                           │
    │                                                         │
    │  ST_STATUS ──→ ST_SIM                                   │
    │                  │                                       │
    │               检查CPIN状态                               │
    │               (非READY则记录故障开始时间)                 │
    │                  ↓                                       │
    │              ST_SIGNAL ──(直通)──→ ST_PING               │
    │                                      │                   │
    │                               test_can_ping_google()     │
    │                              /                \          │
    │                         成功                 失败        │
    │                          │                    │          │
    │                    清故障计时              记录首次失败时间 │
    │                    清恢复等级              计算累计断网时长 │
    │                    首次成功触发回调        判断恢复等级     │
    │                    清快速重试计数文件        │             │
    │                          │              [L1/L2/L3?]      │
    │                     ST_STATUS         有 → ST_RECOVERY   │
    │                                       无 → ST_STATUS     │
    │                                                          │
    │  ST_RECOVERY:                                            │
    │    case L1: ql_data_call_stop() → sleep(2) → start()    │
    │    case L2: AT+CFUN=0 → sleep(3) → AT+CFUN=1 → start() │
    │    case L3: sync() (实际重启被注释，仅记录日志)           │
    │    → ST_STATUS                                           │
    └─────────────────────────────────────────────────────────┘
```

### 5.3 分级故障恢复策略

```
断网持续时间线:
0s ─────────── 60s ─────────── 5min ─────────── 30min
               │               │               │
               Level 1         Level 2         Level 3
               软重拨           射频重置         模块重启(注)
               ↓               ↓               ↓
         stop data_call   AT+CFUN=0/1     system("reboot")
         sleep(2)         sleep(3+10)     (当前版本已注释，
         start data_call  start data_call  仅打印日志)

注：
- Level 1 冷却：相邻两次执行间隔 ≥ 60s
- Level 2 冷却：相邻两次执行间隔 ≥ 5min
- 可通过 enable_policy_recovery = 0 完全关闭策略，依赖 SDK 底层自动重连
- restart_cfun() 函数有独立保护：调用上限10次 + 最小间隔600s
```

### 5.4 快速失败重试机制

解决场景：开机后拨号失败，快速退出让保活脚本重新拉起尝试。

```
首次启动:  retry_count = 0, is_fast_fail_mode = true
  │ 10s内Ping通 → clear_retry_count(), 退出快速模式 → 正常运行
  │ 10s未通 → exit(1) → 保活脚本拉起

第2次启动: retry_count = 1, is_fast_fail_mode = true
  (同上)

第3次启动: retry_count = 2, is_fast_fail_mode = true
  (同上)

第4次启动: retry_count = 3 (>= MAX_FAST_RETRY_TIMES)
  → is_fast_fail_mode = false
  → 进入持久循环模式，永不因超时退出，完全依赖恢复策略

持久循环成功后: clear_retry_count() → 删除 /tmp/dial_retry_count
               下次断网重启时重新从第1次开始
```

---

## 6. 关键数据结构

### dial_mng_t（拨号管理对象）

```c
typedef struct {
    dial_stat_enu dial_st;              // 当前拨号状态（枚举）
    QL_NW_SIGNAL_STRENGTH_LEVEL_E level; // 当前信号强度等级
    struct timespec dial_timer;          // 最后一次收到数据包的时间戳
} dial_mng_t;
```

### apn_obj_t（APN 配置条目）

```c
typedef struct {
    char iccid[32];    // ICCID 前缀
    char apn[32];      // APN 名称
    char usr_name[32]; // 用户名
    char pwd[32];      // 密码
} apn_obj_t;
```

### Config（全局运行配置）

```c
typedef struct {
    int uptime;           // 系统启动时间（秒）
    int restart_flag;     // 重启标志（持久化）
    int first_disconnect; // 首次断网（运行时）
    int signal_strength;  // CSQ 信号强度
    int sim_status;       // SIM 状态
    int ql_netd_status;   // ql_netd 运行状态
} Config;
```

---

## 7. 关键文件与路径速查表

| 文件路径 | 读/写 | 说明 |
|---------|-------|------|
| `/tmp/network_status` | 读写 | 网络状态：0=断网，1=联网 |
| `/tmp/network_csq` | 写 | 信号强度等级字符串 |
| `/tmp/exit_count.txt` | 读写 | 异常退出计数，≥20触发射频重置 |
| `/tmp/dial_retry_count` | 读写 | 快速失败重试计数，≥3进入持久模式 |
| `/tmp/cfun_count.txt` | 读写 | AT+CFUN 调用次数（上限10次） |
| `/tmp/cfun_last_call.txt` | 读写 | AT+CFUN 最后调用时间（系统uptime秒数） |
| `/tmp/dial_version` | 写 | 当前版本号，如 `Version: 1.26` |
| `/tmp/dial_reboot.conf` | 读写 | 重启配置（uptime,restart_flag） |
| `/tmp/callid` | 读写 | 数据连接 call_id 持久化 |
| `/tmp/dial_Status` | 写 | 拨号状态：0=连接，1=断开 |
| `/tmp/resolv_v4.conf` | 写 | IPv4 DNS 配置（拨号成功后生成） |
| `/tmp/resolv_v6.conf` | 写 | IPv6 DNS 配置（拨号成功后生成） |
| `/etc/resolv.conf` | 写 | 系统 DNS（从 resolv_v4/v6.conf 合并） |
| `/usr/dial/apn.json` | 读 | APN 配置数据库 |
| `/proc/uptime` | 读 | 系统运行时间 |
| `/proc/mounts` | 读 | 挂载点检测（SD卡） |
| `/media/sdcard/dial_log/` | 写 | SD卡运行日志目录 |
| `/sys/.../net/<if>/statistics/rx_packets` | 读 | 网卡流量统计 |

---

## 8. 外部依赖与 SDK 接口

### Quectel ql-sdk API 使用汇总

| API | 功能 |
|-----|------|
| `ql_data_call_init()` | 初始化数据拨号服务（返回-1001时重试） |
| `ql_data_call_create(id, name, flag)` | 创建数据连接实例 |
| `ql_data_call_config(id, cfg)` | 配置拨号参数（APN ID、重连间隔） |
| `ql_data_call_start(id)` | 启动拨号 |
| `ql_data_call_stop(id)` | 停止拨号 |
| `ql_data_call_set_status_ind_cb(cb)` | 注册连接状态变化回调 |
| `ql_data_call_set_apn_config(id, cfg)` | 设置 APN 配置 |
| `ql_data_call_get_status(id, &sta)` | 查询连接状态 |
| `ql_sim_init()` | 初始化 SIM 服务 |
| `ql_sim_get_iccid(slot, iccid, len)` | 读取 ICCID |
| `ql_sim_get_imsi(slot, type, imsi, len)` | 读取 IMSI |
| `ql_nw_get_signal_strength(&info, &level)` | 获取信号强度 |
| `ql_nw_get_data_reg_status(&info)` | 获取数据网络注册状态 |
| `ql_nw_set_pref_nwmode_roaming(&info)` | 设置网络制式和漫游 |

### 第三方库依赖

| 库 | 用途 |
|----|------|
| `json-c` | 解析 apn.json 配置文件 |
| `cc_deque` | 双端队列（项目内置，当前版本暂未用于主流程） |

### 外部命令依赖

| 命令 | 用途 |
|------|------|
| `serial_atcmd at+csq` | 查询信号强度 |
| `serial_atcmd at+cereg?` | 查询网络注册状态 |
| `serial_atcmd at+cpin?` | 查询 SIM 卡状态 |
| `serial_atcmd at+qiact?` | 查询 PDP 上下文状态 |
| `serial_atcmd at+cfun=0/1` | 开关射频 |
| `ping -c 1 -W 2 8.8.8.8` | 网络连通性检测 |
| `ip ro add default via ...` | 添加默认路由 |
| `iptables -t nat ...` | 配置 NAT 转发 |
| `pgrep <name> | wc -l` | 进程数量检查 |
| `iw wlan0 info` | 查询 WiFi 模式 |
| `hostapd_cli -i wlan0 status` | 查询 WiFi 客户端数量 |

---

## 9. 编译与部署

### 编译

```bash
# 修改 Makefile 中的交叉编译工具链路径（aarch64 或 arm）
# 修改 APP_INCLUDE_DIRS 指向 ql-sdk 头文件路径

make
```

### 部署

```bash
# 1. 将编译产物 dial 复制到设备
adb push dial /usr/bin/dial
chmod +x /usr/bin/dial

# 2. 部署 APN 配置
adb push apn.json /usr/dial/apn.json

# 3. 配置保活脚本（在 /etc/init.d 或 crontab 中）
# 保活脚本需监控 dial 进程，退出后重新拉起
# 建议：用 start-stop-daemon 或 supervise
```

### 运行时检查

```bash
# 查看网络状态
cat /tmp/network_status

# 查看版本
cat /tmp/dial_version

# 查看信号强度
cat /tmp/network_csq

# 查看 SD 卡日志
ls /media/sdcard/dial_log/

# 手动测试 Ping
ping -c 1 -W 2 8.8.8.8
```

---

## 10. 设计亮点与注意事项

### 设计亮点

**1. Ping 结果可靠性优化**  
放弃 `system()` 的返回值检测（易受 SIGCHLD 影响），改用 `popen()` 读取输出内容，通过检测 `ttl=` 字符串判断 Ping 成功，彻底解决信号处理导致误判的问题。

**2. 分级故障恢复**  
不是简单地"失败就重启"，而是按断网时长分三个等级逐步升级恢复手段，尽量减少对系统的冲击。

**3. 频率保护机制**  
`restart_cfun()` 同时设置了调用次数上限（10次/系统运行周期）和最小间隔（600秒），防止频繁重置射频导致设备不稳定。

**4. WiFi 感知重启**  
在 `restartNetworkServices()` 中，先检查是否有 WiFi 客户端连接（AP/STA 模式），若有则跳过重启，避免重启 ql_netd 导致 bridge MAC 变化、WiFi 断连。

**5. SD 卡日志降级**  
`log_init()` 会自动检测 SD 卡挂载状态和可用空间，不满足条件时优雅降级为纯控制台输出，不影响主功能。

### 注意事项

| 项目 | 说明 |
|------|------|
| Level 3 恢复注释 | `dial_loop` 中 Level 3 的实际重启指令（`system("reboot")`）已被注释，当前版本仅打印告警日志，若需启用须手动取消注释 |
| 单卡硬编码 | SIM 槽位固定为 SLOT_1，不支持双卡设备，需要适配时修改 sim.c |
| `serial_atcmd` 依赖 | 多处 AT 命令通过 popen 调用 `serial_atcmd` 工具，该工具需在系统中存在 |
| APN 文件缺失 | 若 `/usr/dial/apn.json` 不存在，自动回退到默认 APN `"apnpublic"`，不会崩溃 |
| call_id 变化 | 注释中提到"重新拨号后 call_id 会改变"，部分回调中已注释掉对 call_id 的过滤判断，需关注多路拨号场景 |
| cc_deque 未用 | `cc_deque` 双端队列已包含在项目中但主流程未使用，可能是历史遗留或未来规划 |

---

