# stub-sany-forklift 源码全面分析报告

> 版本：v1.1.4.0（当前分支 tpad-lock-cmd）
> 分析日期：2026-05-20
> 源码路径：`/home/tronlong/lyp/code/stub-sany-forklift/`
> 本文档对仓库所有源文件逐行核实，力求完整、全面、详细。

---

## 目录

1. [仓库概览](#1-仓库概览)
2. [系统定位与功能](#2-系统定位与功能)
3. [文件结构与代码规模](#3-文件结构与代码规模)
4. [依赖库与构建系统](#4-依赖库与构建系统)
5. [整体架构与进程关系](#5-整体架构与进程关系)
6. [nanomsg 通信端口全貌](#6-nanomsg-通信端口全貌)
7. [核心数据结构详解](#7-核心数据结构详解)
8. [主线程执行流程](#8-主线程执行流程)
9. [子线程详解](#9-子线程详解)
10. [锁机/解锁机完整流程](#10-锁机解锁机完整流程)
11. [CAN 数据处理与上报](#11-can-数据处理与上报)
12. [GPS 数据处理与上报](#12-gps-数据处理与上报)
13. [MCU 信息查询机制](#13-mcu-信息查询机制)
14. [云端消息收发机制（SCP通信）](#14-云端消息收发机制scp通信)
15. [点表（Pointsheet）机制详解](#15-点表pointsheet机制详解)
16. [配置文件说明](#16-配置文件说明)
17. [信号处理机制](#17-信号处理机制)
18. [内存管理与线程安全](#18-内存管理与线程安全)
19. [日志系统](#19-日志系统)
20. [设备信息上报机制](#20-设备信息上报机制)
21. [SIGUSR2 本地测试注入框架](#21-sigusr2-本地测试注入框架)
22. [Git 提交历史与版本演进](#22-git-提交历史与版本演进)
23. [关键函数调用关系总图](#23-关键函数调用关系总图)
24. [已知问题与待办事项](#24-已知问题与待办事项)
25. [补充：遗漏函数完整分析](#25-补充遗漏函数完整分析)
26. [补充：完整流程图汇总](#26-补充完整流程图汇总)
20. [设备信息上报机制](#20-设备信息上报机制)
21. [SIGUSR2 本地测试注入框架](#21-sigusr2-本地测试注入框架)
22. [Git 提交历史与版本演进](#22-git-提交历史与版本演进)
23. [关键函数调用关系总图](#23-关键函数调用关系总图)
24. [已知问题与待办事项](#24-已知问题与待办事项)

---

## 1. 仓库概览

### 1.1 基本信息

| 属性 | 值 |
|------|-----|
| 项目名 | stub-sany-forklift（CMakeLists 中为 stub-forklift）|
| 当前分支 | tpad-lock-cmd |
| 主版本号 | v1.1.4.0（MAJOR.MINOR.PATCH.REVISION）|
| 模块 UUID | `2695ec26-28bd-11ef-954a-00155de10703` |
| 目标平台 | ARMv7-a（嵌入式 Linux，Tronlong/三宁叉车 TBox）|
| 主要语言 | C（C99 标准）|
| 构建工具 | CMake 2.8+，gcc 交叉编译 |

### 1.2 一句话定位

`stub-sany-forklift` 是运行在**三宁机器人叉车 TBox（车载终端）**上的 C 程序，作用是：

- 把**硬件传感器数据**（CAN 报文、GPS 定位、MCU 状态）打包上报给**云平台（SCP）**
- 把**云平台下发的指令**（锁机、解锁机、参数设置）转发给 **MCU 执行**，并把结果回报云端

程序是 MCU 硬件和云平台之间的**翻译官兼快递员**，基于 **RTMS（远程终端管理系统）** 框架实现。

---

## 2. 系统定位与功能

### 2.1 系统架构位置

```
三方系统架构：

  云平台 SCP ←──── stub-linker TCP 11215/11216/11225 ────► stub-sany-forklift
                                                                    │
                                    ┌───────────────────────────────┤
                                    │         nanomsg IPC           │
                                    ▼                               ▼
                            CAN 驱动进程                      CMD 代理进程
                          (ports 16002~16004)              (ports 16008/26008)
                                    │                               │
                                    ▼                               ▼
                                CAN 总线                        SPI 总线
                                    │                               │
                                    └──────────────────┬────────────┘
                                                       ▼
                                                   MCU 硬件
```

### 2.2 核心功能列表

| 功能 | 描述 |
|------|------|
| CAN 数据接收 | 订阅 3 路 CAN 总线（CAN0/1/2）原始帧，port 16002/16003/16004 |
| 点表 CAN 解析 | 按 Pointsheet_info1.json 规则解析 CAN 帧的物理含义 |
| CAN 原始数据上报 | 将 CAN 原始帧打包成 stub_can_msg_t 格式上报 SCP（port 11225）|
| CAN 工况数据上报 | 每 10s 将点表解析后的工况数据打包为 JSON 上报 SCP |
| GPS 数据接收 | 订阅 GPS 模块 JSON 数据，port 16005 |
| GPS 位置上报 | 每 10s 以两种格式上报经纬度到 SCP（port 11215/11225）|
| 锁机/解锁机 | 完整实现从云端命令→MCU 执行→结果回报的状态机 |
| MCU 状态查询 | 每 5s 查询 ACC 状态、电源电压、MCU 版本，port 38000 |
| 时间同步 | 每 10min 将系统时间同步到 MCU，条件：有网络或有标志文件 |
| 心跳维持 | 每 3min 向 SCP 发送心跳并监测 SCP 在线状态 |
| 版本协商 | 启动时与 SCP 协商协议版本，之后每 3min 广播本模块版本 |
| 设备信息上报 | 每 3min 上报 TBox/MCU/点表/stub 版本信息（parts_info）|
| 密码管理 | 每 10s 从 MCU 获取 6 字节随机锁机密码并缓存 |
| 0x31B 帧专项解析 | 专项解析软件版本号（7字节）和 VIN 码（17字节，分3帧）|
| 日志级别动态切换 | SIGUSR1 信号循环调整日志等级并持久化 |
| 本地测试注入 | 编译宏 STUB_TEST_INJECT + SIGUSR2 信号注入测试锁机消息 |

---

## 3. 文件结构与代码规模

```
stub-sany-forklift/
├── stub-sany-forklift.c      # 主程序（2767 行），程序核心逻辑全在此
├── data_process.c            # 点表 CAN 数据解析（345 行）
├── data_process.h            # 点表数据结构定义（59 行）
├── nn_sock.c                 # nanomsg socket 工厂函数（100 行）
├── nn_sock.h                 # nanomsg socket 端口常量、帧结构定义（96 行）
├── CMakeLists.txt            # 构建配置（69 行）
├── Pointsheet_info1.json     # CAN 点表配置文件（约774KB，高度结构化）
├── stub.ini                  # stub-linker 框架配置（4 行）
├── material_code.ini         # TBox 物料编码配置（2 行）
├── README.md                 # 简要说明（9 行）
├── .gitignore                # 忽略 md 目录
├── md/                       # 文档目录（不入库）
│   ├── stub-sany-forklift分析报告.md  # 本文档
│   └── skes_dataengine_can_analysis.md
└── .claude/settings.local.json   # Claude Code 权限配置
```

**代码行数统计：**

| 文件 | 行数 | 说明 |
|------|------|------|
| stub-sany-forklift.c | 2767 | 主程序，含所有业务逻辑 |
| data_process.c | 345 | 点表 CAN 解析引擎 |
| nn_sock.c | 100 | nanomsg socket 封装 |
| nn_sock.h | 96 | 端口定义、帧结构 |
| data_process.h | 59 | 数据结构定义 |
| **合计** | **3367** | |

---

## 4. 依赖库与构建系统

### 4.1 构建命令

```bash
# 标准构建（ARM 交叉编译）：
cmake -B build && cmake --build build

# 开启测试注入（仅调试用，正式固件不启用）：
cmake -DSTUB_TEST_INJECT=ON -B build && cmake --build build
```

### 4.2 依赖库清单

| 库名 | 版本 | 用途 |
|------|------|------|
| cjson-git | 1.7.15 | JSON 解析与生成（所有消息序列化） |
| nanomsg-git | 1.1.5 | 进程间消息通信（PUB/SUB/REQ/REP 模式）|
| collections-c-git | 0.0.1 | 双向链表 List、单向链表 SList 容器 |
| libev-git | 4.33 | 事件驱动定时器（timer_task_thread 使用）|
| libcbor-git | 0.10.2 | CBOR 编码（被 stub-linker 间接使用）|
| zlib-git | 1.3.1 | 压缩（被 stub-linker 间接使用）|
| e2fsprogs-git | 1.44.4 | UUID 生成（`rcpv4_gen_uuid`）|
| rcpv4-utils_static | — | 工具函数（INI 读写、字符串转换、文件读取）|
| stub-linker_static | — | RTMS SCP 通信 SDK |
| erk-log | — | 结构化日志库 |
| erk-utils | — | 通用工具（hex dump、时钟等）|
| pthread | — | POSIX 线程 |
| uuid | — | UUID 库 |

### 4.3 ARM 专属静态库

当目标不是 x86_64 时，额外从 `lib/sany_forklift/armv7-a/` 目录链接预编译的 ARM 静态库。

### 4.4 RPATH 设置

安装时 RPATH 包含多个相对路径（`$ORIGIN` 系列），便于在嵌入式系统上就地运行，无需全局安装共享库：
```
$ORIGIN/lib
$ORIGIN/../lib
$ORIGIN/../../lib
$ORIGIN/../erk-agent/lib
...
```

### 4.5 安装目录

```
tools/stub-forklift/
├── stub-forklift          # 可执行程序
├── stub.ini               # 框架配置
├── material_code.ini      # 物料编码
└── Pointsheet_info1.json  # 点表配置
```

---

## 5. 整体架构与进程关系

### 5.1 进程内部模块划分

```
stub-sany-forklift 进程内部结构

┌────────────────────────────────────────────────────────────────┐
│                       main thread（主线程）                      │
│  ① stub_run()           轮询 SCP SUB socket，触发消息入队        │
│  ② run_pub_tasks()      执行定时任务（每10s 或 180s 一次）         │
│  ③ handle_stub_msgs()   消费云端消息队列，按 topic 分发处理       │
│  ④ handle_msg_caches()  把待发消息队列全部发给云端               │
│  ⑤ usleep(10ms)         主循环节流                              │
└───────────────────────────────┬────────────────────────────────┘
                                │ pthread_create
              ┌─────────────────┴──────────────────┐
              ▼                                     ▼
┌──────────────────────────┐          ┌────────────────────────────┐
│    nn_recv_thread         │          │    timer_task_thread        │
│  （IO 接收线程）           │          │  （定时任务线程，libev驱动）  │
│                          │          │                            │
│  nn_poll(6路fd, 2000ms)  │          │  ev_timer(1s, repeat=1s): │
│  ├─ CAN0 fd (16002)      │          │    timer_task()           │
│  ├─ CAN1 fd (16003)      │          │    ├─ STUB_TEST_INJECT注入 │
│  ├─ CAN2 fd (16004)      │          │    ├─ handle_cmd_timeout() │
│  ├─ GPS  fd (16005)      │          │    └─ 每6次: 查锁状态+MCU  │
│  ├─ MCU REQ fd (38000)   │          │                            │
│  └─ CMD  fd (16008)      │          │  ev_timer(60s, repeat=600s)│
└──────────────────────────┘          │    sync_time_to_mcu()     │
                                      └────────────────────────────┘
```

### 5.2 `stub_ro_handle_t` —— 程序的全局"总控制台"

整个程序只有一个全局实例 `stub_ro_handle`，所有状态都在这个结构体中：

```c
typedef struct {
  // ─── SCP 通信 handles（通过 stub-linker 框架）───────────────────
  stub_handle_t *handle_sub_cmd;     // 订阅 SCP 消息（port 11216）
  stub_handle_t *handle_pub_cmd;     // 向 SCP 发送命令回复/心跳（port 11215）
  stub_handle_t *handle_pub_data;    // 向 SCP 上报设备数据（port 11225）

  // ─── SCP 在线状态 ───────────────────────────────────────────────
  scp_online_info_t soi;             // SCP 心跳信息（hb对象、peer_counter、ts、is_online）

  // ─── 消息队列 ────────────────────────────────────────────────────
  List *list_lock_cmd_infoes;        // 待执行的锁机命令队列（最多1000条）
  List *list_stub_msgs;              // 从SCP收到的消息（线程安全，等待主线程消费）
  List *list_msg_caches;             // 向SCP发送的消息缓冲队列（最多1024条）

  // ─── 点表 ────────────────────────────────────────────────────────
  data_process_mng_t data_process_mng;  // 点表管理器（含3路CAN的解析规则哈希表）

  // ─── 设备标识 ────────────────────────────────────────────────────
  char *instance_uuid;               // 本实例的 UUID（每次启动随机生成）
  char *tbox_id;                     // TBox 设备 ID（从 /opt/conf.ini 读取）
  char *material_code;               // TBox 物料编码（从 material_code.ini 读取）
  char *vin_code;                    // VIN 码（云端下发后保存）
  char *vehicle_code;                // 整车编号（云端下发后保存）
  bool is_vehicle_code_valid;        // 整车编号是否有效（控制 request_dev_info 停止重试）

  // ─── 运行状态 ────────────────────────────────────────────────────
  bool running;                      // 进程运行标志
  bool protocol_version_responsed;   // SCP 协议版本是否已协商完成
  int network_connected;             // 是否有网络连接（0=无，1=有）

  // ─── 互斥锁（保护 list_stub_msgs）──────────────────────────────
  pthread_mutex_t mutex;

  // ─── 设备信息上报控制 ─────────────────────────────────────────────
  int64_t ecu_info_report_ts;        // 上次上报时间
  int64_t ecu_info_report_timeout;   // 上报间隔（默认 180s）
  int64_t ecu_info_reported;         // 是否已上报过

  // ─── nanomsg socket fd ───────────────────────────────────────────
  int nn_sub_gps_fd;                 // GPS 订阅（port 16005）
  int nn_sub_can_fd[CAN_CHN_NUM];    // CAN 订阅（ports 16002/3/4）
  int nn_pub_can_fd[CAN_CHN_NUM];    // CAN 发布（ports 26002/3/4）
  int nn_pub_cmd_fd;                 // CMD 发布（port 26008，发送锁机命令等）
  int nn_sub_cmd_fd;                 // CMD 订阅（port 16008，接收 MCU 回复）
  int nn_req_mcu_fd;                 // MCU 状态 REQ/REP（port 38000）

  // ─── MCU 采集数据 ─────────────────────────────────────────────────
  int acc_status;                    // ACC 状态（0=关，1=开）
  int mcu_main_ver;                  // MCU 主版本号
  int mcu_sub_ver;                   // MCU 子版本号
  int pwr_volt;                      // 电源电压（毫伏，上报时 *0.001 转换为伏）
  gps_frame_t gps_info;             // GPS 定位数据结构体

  // ─── 消息统计计数 ─────────────────────────────────────────────────
  int can_msg_recv_cnt[CAN_CHN_NUM];    // 收到的 CAN 帧计数
  int can_msg_success_cnt[CAN_CHN_NUM]; // 成功上报的 CAN 帧计数
  int can_msg_fail_cnt[CAN_CHN_NUM];    // 上报失败的 CAN 帧计数
  int gps_msg_success_cnt;             // GPS 数据成功上报计数
  int gps_msg_fail_cnt;                // GPS 数据上报失败计数

  // ─── 0x31B 报文专项存储 ───────────────────────────────────────────
  char can_0x31B_vin[18];              // VIN 码（17字节ASCII + 1个\0）
  char can_0x31B_software_ver[8];      // 软件版本号（7字节ASCII + 1个\0）

  // ─── 锁机相关 ─────────────────────────────────────────────────────
  bool lock_cmd_password_is_valid;     // MCU 提供的锁机密码是否有效
  uint8_t lock_cmd_password[6];        // MCU 提供的 6 字节随机锁机密码
  lock_cmd_info_t *current_lci;        // 当前正在执行的锁机命令（NULL=空闲）
  bool last_locked;                    // 上一次锁机状态（用于比较前后变化）
  bool current_locked;                 // 当前是否处于锁机状态
  uint8_t current_level;              // 当前锁机级别（0=解锁，1/2=锁机）
  bool current_binded;                 // 当前是否绑定（允许远程锁机）
} stub_ro_handle_t;
```

---

## 6. nanomsg 通信端口全貌

```
本进程连接的所有 nanomsg 端口（全部作为 CLIENT 连接到服务端）

  ─── CAN 数据（MCU → 本进程）────────────────────────────────────
  tcp://127.0.0.1:16002  SUB  ← CAN0 原始帧数据流
  tcp://127.0.0.1:16003  SUB  ← CAN1 原始帧数据流
  tcp://127.0.0.1:16004  SUB  ← CAN2 原始帧数据流

  ─── CAN 数据（本进程 → MCU）────────────────────────────────────
  tcp://127.0.0.1:26002  PUB  → CAN0 发送（当前代码未实际使用发送）
  tcp://127.0.0.1:26003  PUB  → CAN1 发送（当前代码未实际使用发送）
  tcp://127.0.0.1:26004  PUB  → CAN2 发送（当前代码未实际使用发送）

  ─── GPS 定位数据 ─────────────────────────────────────────────────
  tcp://127.0.0.1:16005  SUB  ← GPS 模块 JSON 定位数据

  ─── CMD 指令通道（锁机/参数配置等）──────────────────────────────
  tcp://127.0.0.1:16008  SUB  ← MCU 的 CMD 回包（密码返回、锁机状态等）
  tcp://127.0.0.1:26008  PUB  → 向 MCU 发送 CMD 命令（锁机指令、时间同步等）

  ─── MCU 状态查询 ─────────────────────────────────────────────────
  tcp://127.0.0.1:38000  REQ  ↔ REQ/REP 模式，查询 acc_stat、电压、版本

  ─── 云平台 SCP（通过 stub-linker 框架封装）─────────────────────
  tcp://127.0.0.1:11215  PUB  → 向 SCP 发布消息（心跳回复、指令结果、GPS等）
  tcp://127.0.0.1:11216  SUB  ← 从 SCP 接收消息（锁机命令、参数设置等）
  tcp://127.0.0.1:11225  PUB  → 向 SCP 发布设备数据（CAN 原始帧、工况数据）

端口命名规律：
  1XXXX = 接收方向（别的进程发给本进程）
  2XXXX = 发送方向（本进程发给别的进程）
  3XXXX = REQ/REP 双向查询
  112XX = SCP 云平台通道
```

---

## 7. 核心数据结构详解

### 7.1 `can_frame_t` —— CAN 帧（来自 MCU，nn_sock.h）

```c
typedef struct {
  uint32_t can_id;      // CAN ID（含帧类型标志位）
  uint16_t can_dlc;     // 数据长度（0~8字节）
  uint16_t rsv_ms;      // MCU 运行时间（毫秒，保留字段）
  uint8_t  data[8];     // CAN 帧数据（最多8字节）
  uint32_t timestamp;   // RTC 时间戳
} can_frame_t;           // 总大小：20字节/帧
```

### 7.2 `gps_frame_t` —— GPS 定位帧（nn_sock.h）

```c
typedef struct gps_info {
  int32_t time_stamp;   // 时间戳
  float   longitude;    // 经度（度）
  float   latitude;     // 纬度（度）
  float   altitude;     // 海拔（米）
  float   direction;    // 方向角（度）
  float   velocity;     // 速度（km/h）
  float   accuracy;     // 精度
  uint8_t inuse_sat;    // 使用中的卫星数
  uint8_t inview_sat;   // 可见卫星数
  uint8_t mode;         // 定位模式
  int8_t  snr;          // 信噪比
} gps_frame_t;
```

### 7.3 `cmd_request_t` —— CMD 命令请求包（nn_sock.h）

```c
typedef struct {
  uint32_t pid;          // 本进程的 PID（MCU 回包时原样返回，用于多进程路由）
  uint16_t cmd_tag;      // 命令类型（见 cmd_tag_e 枚举）
  uint8_t  cmd_idx;      // 命令序号（0~255 循环递增），用于追踪请求和响应对应关系
  uint8_t  cmd_len;      // cmd_bytes 的字节数
  uint8_t  rsv[4];       // 保留字节，全填 0
  char     cmd_bytes[];  // 命令参数（柔性数组，长度由 cmd_len 决定）
} cmd_request_t;
```

### 7.4 `cmd_reply_t` —— CMD 命令应答包（nn_sock.h）

```c
typedef struct {
  uint32_t pid;          // 对应请求的 pid（原样返回）
  uint16_t cmd_tag;      // 对应请求的 cmd_tag（原样返回）
  uint8_t  cmd_idx;      // 对应请求的 cmd_idx（原样返回，用于匹配）
  uint8_t  cmd_len;      // 应答数据 cmd_bytes 的字节数
  uint8_t  error;        // 0=成功，非0=错误码
  uint8_t  crc;          // cmd_bytes 的 CRC 校验
  uint8_t  rsv[2];       // 保留
  char     cmd_bytes[];  // 应答数据（柔性数组）
} cmd_reply_t;
```

> **重要注意**：代码实际处理 CMD 回包时，使用 `cmd_request_t` 而非 `cmd_reply_t` 来 memcpy 解析，这是因为 CMD 代理透传时 cmd_tag 和 cmd_len 字段偏移与 cmd_request_t 一致。这是一个刻意设计的兼容处理。

### 7.5 `cmd_tag_e` —— CMD 命令类型枚举（nn_sock.h）

```c
typedef enum {
  CMD_CAN_SET          = 1,    // 配置 CAN 总线参数
  CMD_COM_SET          = 2,    // 配置串口参数
  CMD_GPS_SET          = 3,    // 配置 GPS 模块
  CMD_DI_SET           = 4,    // 配置数字输入
  CMD_AI_SET           = 5,    // 配置模拟输入
  CMD_DO_CTRL          = 50,   // 控制数字输出
  CMD_AO_CTRL          = 51,   // 控制模拟输出
  CMD_TIME_SET         = 100,  // ★ 设置 MCU 时间（含4字节unix时间戳）
  CMD_EC_INFO_SET      = 101,  // 设置 EC 信息
  CMD_NMEA_REQ         = 150,  // 请求 NMEA 数据
  CMD_MCU_LOG_REQ      = 151,  // 请求 MCU 日志
  CMD_GPS_COM_REQ      = 152,  // 请求 GPS COM 数据
  CMD_MCU_INFO_REQ     = 153,  // 请求 MCU 信息
  CMD_CAN_INFO_REQ     = 154,  // 请求 CAN 信息
  CMD_COM_INFO_REQ     = 155,  // 请求串口信息
  CMD_DO_INFO_REQ      = 156,  // 请求数字输出信息
  CMD_AO_INFO_REQ      = 157,  // 请求模拟输出信息
  CMD_TAG_PASS_RD      = 200,  // ★ 请求/返回锁机密码（6字节随机数）
  CMD_TAG_PASS_CLR     = 201,  // 清除锁机密码
  CMD_TAG_LOCK         = 202,  // ★ 发送锁机命令 / MCU 确认锁机命令接受
  CMD_TAG_FOTA         = 203,  // FOTA 固件升级
  CMD_TAG_LOCK_STATUS_REPLY = 204, // ★ MCU 主动推送锁机状态变化
} cmd_tag_e;
```

### 7.6 `lock_cmd_type_t` —— 锁机命令类型枚举

```c
typedef enum {
  lock_cmd_null              = 0x00,  // 无效命令
  lock_cmd_bind              = 0x10,  // 绑定（允许远程锁机）
  lock_cmd_unbind            = 0x20,  // 解绑（禁止远程锁机）
  lock_cmd_one_lock          = 0x31,  // 一级锁机（限速模式）
  lock_cmd_two_lock          = 0x32,  // 二级锁机（禁止起步）
  lock_cmd_three_lock        = 0x33,  // 三级锁机（断电，当前已注释禁用）
  lock_cmd_unlock            = 0x40,  // 解锁
  lock_cmd_bind_one_lock     = 0x51,  // 绑定+一级锁机（组合）
  lock_cmd_bind_two_lock     = 0x52,  // 绑定+二级锁机（组合）
  lock_cmd_bind_three_lock   = 0x53,  // 绑定+三级锁机（组合）
  lock_cmd_unbind_unlock     = 0x60,  // 解绑+解锁（组合）
  lock_cmd_reset_engine_type = 0x90,  // 重置发动机类型
} lock_cmd_type_t;
```

> **注意**：三级锁机（`lock_cmd_three_lock = 0x33`）在 `do_run_lock_cmd_internal` 中被注释掉，level==3 的命令将走到错误分支并回复 `lock_error_param`，实际上预期会超时。

### 7.7 `lock_error_t` —— 锁机错误码枚举

```c
typedef enum {
  lock_error_success      = 0,  // 成功
  lock_error_incompleted  = 1,  // 部分成功
  lock_error_param        = 2,  // 参数错误（如不支持的锁机级别）
  lock_error_unsupported  = 3,  // 不支持的操作
  lock_error_failed       = 4,  // 失败（MCU 拒绝或超时）
  lock_error_oom          = 5,  // 内存不足
  lock_error_disconnected = 6,  // 连接断开
} lock_error_t;
```

### 7.8 `lock_bind_type_t` —— 锁机操作类型枚举

```c
typedef enum {
  lock_bind_type_none = 0,  // 无效
  lock_bind_type_lock = 1,  // 锁机/解锁操作
  lock_bind_type_bind = 2,  // 绑定/解绑操作
} lock_bind_type_t;
```

### 7.9 `lock_cmd_info_t` —— 待执行的锁机命令信息

```c
typedef struct lock_cmd_info {
  int64_t         timeout;         // 超时时间点（get_tick_count() 毫秒值）
  char            *msg_id;         // 云端命令的 msg_id（用于构造回复消息）
  cJSON           *msg;            // 原始云端命令中的 msg 字段（回复时原样带回）
  lock_bind_type_t lock_bind_type; // 操作类型：锁机操作 or 绑定操作
  bool            last_lock;       // 执行该命令前的锁机状态（保存快照）
  bool            lock;            // 期望锁(true) or 解锁(false)
  bool            bind;            // 期望绑定(true) or 解绑(false)
  uint8_t         level;           // 锁机级别（1/2，仅 lock_bind_type_lock 时有效）
} lock_cmd_info_t;
```

### 7.10 `scp_online_info_t` —— SCP 在线状态

```c
typedef struct scp_online_info {
  stub_msg_heartbeat_t hb;    // 心跳消息对象（含 msg_id、action、counter 等）
  int64_t peer_counter;       // SCP 返回的心跳计数（用于验证回包）
  int64_t ts;                 // 最后一次收到 SCP 心跳的时间（单调时钟毫秒）
  bool    is_online;          // SCP 是否在线（超过 300s 未收到心跳则为 false）
} scp_online_info_t;
```

### 7.11 `msg_cache_entry_t` —— 待发送消息缓存条目

```c
typedef struct msg_cache_entry {
  stub_topic_type_t topic_type;  // 目标 SCP topic 类型
  char *msg;                     // 消息内容（堆上分配，发送后 free）
  int   len;                     // 消息字节数
} msg_cache_entry_t;
```

### 7.12 `stub_ro_task_entry_t` —— 定时任务条目

```c
typedef struct stub_ro_task_entry {
  int64_t last;                              // 上次执行时间（单调时钟毫秒）
  int64_t timeout;                           // 执行间隔（毫秒）
  int (*task)(stub_ro_handle_t *ro_handle);  // 任务函数指针
} stub_ro_task_entry_t;
```

---

## 8. 主线程执行流程

### 8.1 main() 函数流程

```
main()
  │
  ├─ init_log()                      # 初始化日志（文件路径/tmp/stub-forklift.log，最大2MB）
  ├─ signal(SIGUSR1, sig_handler)    # 注册动态调整日志级别的信号
  ├─ [STUB_TEST_INJECT] signal(SIGUSR2, sig_handler)  # 注册测试注入信号
  │
  ├─ stub_ro_new()                   # ★ 创建总控制台
  │    ├─ load_pointsheet_json()     #   加载点表
  │    ├─ rcpv4_gen_uuid()           #   生成实例 UUID
  │    ├─ get_tbox_id()              #   从 /opt/conf.ini 读取 TBox ID
  │    ├─ get_material_code()        #   从 material_code.ini 读取物料编码
  │    ├─ stub_ro_lock_init()        #   初始化互斥锁
  │    ├─ scp_online_info_init()     #   初始化心跳消息对象
  │    ├─ list_new() × 2             #   创建消息队列（list_stub_msgs、list_msg_caches）
  │    ├─ stub_new/connect() × 3     #   建立 SCP SUB(11216)/PUB(11215)/PUB(11225) 连接
  │    └─ connect_nn_sock()          #   建立 nanomsg CAN/GPS/CMD/MCU 连接
  │
  ├─ pthread_create(timer_task_thread)   # 启动定时器线程（libev 驱动）
  ├─ sleep(2)                            # 等待子线程初始化完成
  ├─ pthread_create(nn_recv_thread)      # 启动 IO 接收线程
  │
  └─ while(1) {
        stub_run(handle_sub_cmd)         # 驱动 SCP SUB socket，触发 on_stub_msg_received 回调
        run_pub_tasks(tasks, n)          # 执行定时发布任务列表
        handle_stub_msgs(handle_sub_cmd) # 处理从 SCP 收到的待处理消息
        handle_msg_caches(stub_ro_handle)# 把待发送队列的消息全部发给 SCP
        usleep(10 * 1000)                # 主循环节流 10ms
     }
```

### 8.2 定时任务列表

主线程每 10ms 调用一次 `run_pub_tasks()`，它检查每个任务是否超过了其间隔时间，超过则执行：

| 任务函数 | 间隔 | 作用 |
|----------|------|------|
| `report_condition_info` | 10s | 上报经纬度到 SCP（filter_table 格式）|
| `report_location` | 10s | 上报 GPS 位置到 SCP（tbox 格式）|
| `report_dtp_msg` | 10s | 上报点表解析后的 CAN 工况数据（data_table 格式）|
| `print_msg_cnt` | 10s | 打印 CAN/GPS 消息统计计数（调试用，ERK_LOG_ALL 级别）|
| `request_stub_network_status` | 10s | 向 SCP 请求查询当前网络连接状态 |
| `request_stub_version` | 10s | 向 SCP 协商协议版本（仅在未协商完成前有效）|
| `broadcast_stub_version` | 180s | 广播本模块版本号给 SCP（1.1.4.0）|
| `request_stub_heartbeat` | 180s | 发送心跳 keep_alive 请求给 SCP |
| `request_dev_info` | 180s | 向 SCP 请求设备参数（vehicleCode，直到获取为止）|
| `report_dev_info` | 180s | 向 SCP 上报设备零件信息（TBox/MCU/点表/stub 版本）|
| `request_lock_passwd` | 10s | **请求 MCU 提供锁机密码（仅在未获取时发送）** |

### 8.3 `push_msg()` 优先级处理

`push_msg()` 将消息放入 `list_msg_caches` 时，以下 topic 类型会被 **插入队头**（优先发送）：

- `stub_topic_type_lock_dev`（锁机命令结果）
- `stub_topic_type_ota_dev`（OTA 升级结果）
- `stub_topic_type_cmd_dev`（车辆指令执行结果）

其余消息追加到队尾，保证时延敏感的命令回复最先发出。队列上限 1024 条，超过则丢弃队头的最旧消息。

---

## 9. 子线程详解

### 9.1 `nn_recv_thread` —— IO 接收线程

使用 `nn_poll` 同时监听 6 路 nanomsg socket，任意一路有数据就处理：

```c
struct nn_pollfd pfd[6];
pfd[0].fd = nn_sub_can_fd[0];  // CAN0 (16002)
pfd[1].fd = nn_sub_can_fd[1];  // CAN1 (16003)
pfd[2].fd = nn_sub_can_fd[2];  // CAN2 (16004)
pfd[3].fd = nn_sub_gps_fd;     // GPS  (16005)
pfd[4].fd = nn_req_mcu_fd;     // MCU REQ (38000) — 注意：这是 REQ socket，等待 REP
pfd[5].fd = nn_sub_cmd_fd;     // CMD  (16008)

while (1) {
  int rc = nn_poll(pfd, 6, 2000);  // 超时 2000ms
  if (rc > 0) {
    if (pfd[0/1/2] ready) handle_can_msg(port);
    if (pfd[3] ready)     handle_gps_msg();
    if (pfd[4] ready)     handle_mcu_reply_msg();
    if (pfd[5] ready)     handle_cmd_msg();
  }
}
```

> **线程安全注意**：`handle_can_msg` 和 `handle_gps_msg` 直接访问 `stub_ro_handle` 的部分字段（如 `can_0x31B_vin`、`gps_info`），没有加锁。这些字段仅在此线程中写入，不跨线程写，属于设计上的豁免。`handle_cmd_msg` 操作 `current_lci` 等锁机状态字段同样不加锁，但与主线程的 `handle_cmd_timeout` 存在潜在竞争（TODO 注释提到此问题）。

### 9.2 `timer_task_thread` —— 定时器线程

使用 `libev` 事件循环运行两个定时器：

```c
struct ev_loop *loop = EV_DEFAULT;

// 定时器1：每 1 秒触发一次 timer_task()
ev_timer timeout_watcher1;
ev_timer_init(&timeout_watcher1, timer_task, 1, 1);  // after=1s, repeat=1s
ev_timer_start(loop, &timeout_watcher1);

// 定时器2：延迟 60 秒后首次触发，之后每 600 秒触发一次 sync_time_to_mcu()
ev_timer timeout_watcher2;
ev_timer_init(&timeout_watcher2, sync_time_to_mcu, 60, 600);
ev_timer_start(loop, &timeout_watcher2);

ev_set_userdata(loop, handle);  // 通过 ev_userdata(loop) 在回调中获取 handle
ev_run(loop, 0);
```

**`timer_task()` 的内部逻辑：**

```c
static void timer_task(struct ev_loop *loop, ev_timer *w, int revents) {
  stub_ro_handle_t *handle = ev_userdata(loop);
  static int cnt = 0;         // 计数器，用于控制每 6s 执行一次的操作

  // 1. 处理 SIGUSR2 信号的测试消息注入（仅 STUB_TEST_INJECT 编译时生效）
#ifdef STUB_TEST_INJECT
  if (test_lock_pending) {
    test_lock_pending = 0;
    // 取出消息列表中的第 (test_lock_msg_idx % total) 条注入
    on_stub_msg_lock_dev(handle, msg, strlen(msg));
    test_lock_msg_idx++;
  }
#endif

  // 2. 驱动锁机状态机（超时检测 + 执行命令 + 检查结果）
  handle_cmd_timeout(handle);

  // 3. 每 6 次（约 6 秒）查询一次锁机状态和 MCU 信息
  if (cnt < 5) {
    cnt++;
  } else {
    send_req_lock_status(handle);  // 向 MCU 查询当前锁机状态
    req_mcu_info(handle);          // 查询 ACC/电压/版本（REQ 方式）
    cnt = 0;
  }
}
```

**`sync_time_to_mcu()` 的条件：**

```c
static void sync_time_to_mcu(struct ev_loop *loop, ev_timer *w, int revents) {
  stub_ro_handle_t *handle = ev_userdata(loop);
  int sec = get_clock_time() / 1000;  // 当前 UNIX 时间（秒）
  // 仅在有网络连接 或 /tmp/ql_time_set_flag 文件存在时才同步
  if (handle->network_connected || access("/tmp/ql_time_set_flag", F_OK) == 0) {
    set_mcu_time(handle, sec);  // 通过 CMD_TIME_SET 发送 4 字节时间戳给 MCU
  }
}
```

---

## 10. 锁机/解锁机完整流程

锁机功能是本程序最核心的业务逻辑，分五个阶段：

### 10.1 阶段一：获取锁机密码

```
程序启动后，每 10s 执行 request_lock_passwd()（仅在未获取时）
  │
  ├─ 构造 cmd_request_t {
  │    pid     = getpid()
  │    cmd_tag = CMD_TAG_PASS_RD (200)
  │    cmd_idx = global_cmd_idx++
  │    cmd_len = 0                 ← 不携带参数
  │  }
  │
  │ nn_send(nn_pub_cmd_fd, ...) → port 26008
  │
  ▼ CMD 代理进程 → SPI → MCU
  │
  │ MCU 生成 6 字节随机数 RAND，携带在回包里
  ▼
  handle_cmd_msg() 在 port 16008 收到：
  cmd_request_t {
    cmd_tag = CMD_TAG_PASS_RD (200)
    cmd_len = 8
    cmd_bytes[0..5] = RAND（6字节随机密码）
    cmd_bytes[6]    = valid（有效性标志，代码不单独检查此字节）
    cmd_bytes[7]    = rsv
  }
  │ 判断条件：cmd_len == 8（满足即认为有效）
  ▼
  存储到：
    stub_handle->lock_cmd_password[6] = cmd_bytes[0..5]  ← 取前6字节作为密码
    stub_handle->lock_cmd_password_is_valid = true
  │
  ▼
  之后 request_lock_passwd() 检测到 is_valid=true，不再发送请求
```

### 10.2 阶段二：云端锁机命令进入

云端通过 SCP 下发 JSON 命令，topic 为 `lock_dev`：

```json
// 锁机/解锁命令（cmd = "lock" 或 "unlock"）
{
  "version": "",
  "id": "",
  "ts": 1639535683010,
  "msg_id": "test-lock-001",
  "action": "request",
  "type": "cmd",
  "data": {
    "cmd": "lock",
    "data": {
      "lock": true,
      "level": 1,
      "msg": {}
    }
  }
}

// 绑定/解绑命令（cmd = "set"，通过 lockEnable.remote 字段）
{
  "msg_id": "test-bind-001",
  "action": "request",
  "type": "cmd",
  "data": {
    "cmd": "set",
    "data": {
      "msg": {
        "type": "TboxParameterSet",
        "data": {
          "lockEnable": {
            "remote": 1    // 1=允许锁机(绑定), 2=禁止锁机(解绑)
          }
        }
      }
    }
  }
}
```

**消息路由：**

```
on_stub_msg_received()（SCP 回调，在主线程 stub_run 里触发）
  │ 将消息 copy 到 list_stub_msgs（加 mutex 锁保护）
  ▼
主线程 handle_stub_msgs()
  │ 取出消息，调用 do_handle_stub_msg()
  ▼
on_stub_msg_lock_dev()
  │
  ├─ 校验 msg_id、type、cmd 等字段
  │
  ├─ cmd == "lock" 或 "unlock"：
  │    创建 lock_cmd_info_t {
  │      timeout      = get_tick_count() + 30000  ← 队列超时30秒
  │      lock_bind_type = lock_bind_type_lock
  │      lock         = json["data"]["lock"]
  │      level        = json["data"]["level"]
  │      msg_id       = strdup(msg_id)
  │      msg          = cJSON_Duplicate(json["data"]["msg"])
  │      last_lock    = current_locked（执行前状态快照）
  │    }
  │    → do_save_lock_cmd_info() → list_lock_cmd_infoes 队列
  │
  └─ cmd == "set"：
       parse_lock_cmd_to_pdk_lock_cmd() 解析 lockEnable.remote 字段
       创建 lock_cmd_info_t {
         lock_bind_type = lock_bind_type_bind
         bind = (lockEnable.remote == 1)
       }
       → do_save_lock_cmd_info() → list_lock_cmd_infoes 队列
```

### 10.3 阶段三：定时器驱动执行（锁机状态机）

`timer_task_thread` 每 1 秒调用 `handle_cmd_timeout()`：

```
handle_cmd_timeout()
  │
  ├─ 遍历 list_lock_cmd_infoes 队列：
  │    ├─ 如果超时（now >= lci->timeout）：
  │    │    → 回复云端"超时失败"
  │    │    → 从队列删除，释放内存
  │    │
  │    └─ 如果 current_lci == NULL（没有命令在执行）：
  │         从队列取出队头 lci
  │         lci->timeout = get_tick_count() + 60000  ← 执行超时60秒
  │         current_lci = lci
  │         do_run_lock_cmd_internal(lci)             ← 发送 CMD 给 MCU
  │
  └─ 检查 current_lci（正在执行的命令）：
       ├─ 超时（now >= lci->timeout + 10000ms）：
       │    → 回复云端"执行超时失败"
       │    → current_lci = NULL，释放内存
       │
       └─ 检查状态是否匹配预期：
            锁机命令：current_locked==lci->lock AND current_level==lci->level
            绑定命令：current_binded == lci->bind
            匹配成功 → 回复云端"成功"，current_lci = NULL，释放内存
```

**`do_run_lock_cmd_internal(lci)` 发送锁机 CMD：**

```
├─ lock_bind_type == lock_bind_type_lock：
│    ├─ lock==true, level==1 → send_lock_msg(lock_cmd_one_lock = 0x31)
│    ├─ lock==true, level==2 → send_lock_msg(lock_cmd_two_lock = 0x32)
│    ├─ lock==true, level==3 → 【当前已注释禁用】直接回复 lock_error_param 并返回 -1
│    └─ lock==false           → send_lock_msg(lock_cmd_unlock = 0x40)
│
└─ lock_bind_type == lock_bind_type_bind：
     ├─ bind==true  → send_lock_msg(lock_cmd_bind   = 0x10)
     └─ bind==false → send_lock_msg(lock_cmd_unbind = 0x20)
```

**`send_lock_msg(cmd_param)` 构造发送包：**

```c
// 构造命令包：总长度 = sizeof(cmd_request_t) + 8
cmd_request_t 头部（12字节）：
  pid     = getpid()
  cmd_tag = CMD_TAG_LOCK (202)
  cmd_idx = global_cmd_idx++
  cmd_len = 8

cmd_bytes 数据（8字节）：
  [0..5] = lock_cmd_password[6]  ← 6字节随机密码（从 MCU 获取的 RAND）
  [6]    = cmd_param             ← 锁机动作（0x31/0x32/0x40/0x10/0x20）
  [7]    = 0                     ← 保留
```

### 10.4 阶段四：MCU 接受/拒绝确认

MCU 收到锁机命令后，立即通过 CMD 代理返回"是否接受"：

```
handle_cmd_msg() 收到 CMD_TAG_LOCK (202) 回包：
  cmd_bytes[0] = error（0=接受，非0=拒绝）
  cmd_len = 4

  ├─ error != 0（MCU 拒绝）：
  │    → do_reply_lock_cmd_response_from_cache(lock_error_failed, "lock cmd rejected by io_mng")
  │    → current_lci = NULL（立即结束命令）
  │
  └─ error == 0（MCU 接受）：
       → 等待后续 CMD_TAG_LOCK_STATUS_REPLY 状态上报
```

### 10.5 阶段五：锁机状态更新

MCU 执行锁机完成后，主动通过 CMD 代理推送当前状态：

```
handle_cmd_msg() 收到 CMD_TAG_LOCK_STATUS_REPLY (204) 回包：
  cmd_bytes[0] = lock_cmd    （触发此状态的命令类型）
  cmd_bytes[1] = lock_result （0=成功，非0=失败）
  cmd_bytes[2] = bind_status （0=解绑，1=绑定）
  cmd_bytes[3] = lock_status （0=解锁，1=一级，2=二级，3=三级）

  更新全局状态：
    current_binded = bind_status
    current_locked = (lock_status > 0)
    current_level  = lock_status
```

下一次 `handle_cmd_timeout()` 检测到状态与命令预期匹配时，自动回复云端成功并清空 `current_lci`。

### 10.6 锁机回复消息格式

```json
{
  "version": "",
  "id": "",
  "ts": 1639535683010,
  "msg_id": "test-lock-001",
  "action": "response",
  "type": "cmd",
  "data": {
    "type": "controller",
    "cmd": "lock",             // locked=true 时为 "lock"，否则为 "unlock"
    "data": {
      "time": 1639535683010,
      "code": 0,               // lock_error_t 枚举值
      "delay": 0,
      "lock": true,            // 期望的 lock 值（来自命令）
      "binded": true,          // 当前绑定状态
      "level": 1,              // 期望的 level 值（来自命令）
      "locked": true,          // 当前实际锁机状态
      "last": false,           // 执行命令前的锁机状态快照
      "desc": " ",             // 描述信息（成功时为空格，失败时有说明）
      "msg": {}                // 原始命令中的 msg 字段（原样带回）
    }
  }
}
```

### 10.7 完整锁机时序图

```
SCP云端    stub-sany-forklift              CMD代理进程           MCU
  │                │                            │                  │
  │──lock cmd──►  │                            │                  │
  │               │ [每10s] request_lock_passwd │                  │
  │               │──CMD_TAG_PASS_RD(200)──────►│──SPI──────────► │
  │               │                            │◄──RAND[6B]+rsv── │
  │               │◄──cmd_len=8, RAND[6B]──────│                  │
  │               │  password[6] = RAND[0..5]  │                  │
  │               │  is_valid = true            │                  │
  │               │                            │                  │
  │──lock_dev──►  │                            │                  │
  │  cmd=lock     │ [timer_task 1s]             │                  │
  │  level=1      │ handle_cmd_timeout()        │                  │
  │               │ do_run_lock_cmd_internal()  │                  │
  │               │──CMD_TAG_LOCK(202)─────────►│──SPI──────────► │
  │               │  [password(6B)][0x31][0]   │                  │ 执行一级锁机
  │               │                            │◄──接受(error=0)── │
  │               │◄──CMD_TAG_LOCK(202)────────│                  │
  │               │  cmd_bytes[0]=0(接受)       │                  │
  │               │                            │◄──执行完成通知─── │
  │               │◄──CMD_TAG_LOCK_STATUS(204)─│                  │
  │               │  bind=1,lock=1(一级)        │                  │
  │               │  current_locked=true        │                  │
  │               │  current_level=1            │                  │
  │               │ [下次timer_task检测到匹配]   │                  │
  │◄──response────│                            │                  │
  │  code=0,      │                            │                  │
  │  locked=true  │                            │                  │
```

---

## 11. CAN 数据处理与上报

### 11.1 CAN 帧接收

`handle_can_msg(port)` 在 IO 接收线程中被调用：

```c
int handle_can_msg(stub_ro_handle_t *handle, int port) {
  // 1. nn_recv 接收批量帧（每次可能收到多帧）
  int len = nn_recv(handle->nn_sub_can_fd[port], &msg, NN_MSG, NN_DONTWAIT);
  int size = len / sizeof(can_frame_t);  // 计算帧数量（每帧20字节）

  // 2. 逐帧处理
  for (int n = 0; n < size; n++) {
    // 点表解析：根据 CAN ID 查找规则，解析数据域
    parse_common_msg(&handle->data_process_mng, &frame, frame.can_id, port);
    // 0x31B 专项解析（软件版本/VIN码）
    parse_0x31B_msg(handle, &frame, port);

    // 过滤扩展帧（can_id >> 28 > 9 的帧不上报）
    if (0x09 < (frame.can_id >> 28)) continue;

    // 3. 打包成 stub_can_msg_t（原始数据上报格式）
    can_msg[valid_cnt].ts = get_clock_time();
    can_msg[valid_cnt].bus_id = port;
    can_msg[valid_cnt].frame_type = can_frame_type_data;
    // 处理标准帧和扩展帧的 ID 类型
    if (frame.can_id >= CAN_EFF_FLAG) {
      can_msg[valid_cnt].id_type = can_id_type_20b;
      can_msg[valid_cnt].id = frame.can_id & CAN_EFF_MASK;
    } else {
      can_msg[valid_cnt].id_type = can_id_type_20a;
      can_msg[valid_cnt].id = frame.can_id;
    }
    can_msg[valid_cnt].length = frame.can_dlc;
    memcpy(can_msg[valid_cnt].data, frame.data, frame.can_dlc);
  }

  // 4. 批量上报到 SCP（port 11225，raw_data 类型）
  stub_can_msg_entry_t *entry = stub_can_msg_entry_make_from(can_msg, valid_cnt);
  stub_msg_reply(handle->handle_pub_data, stub_topic_type_data_dev_raw_data, entry, ...);
}
```

### 11.2 0x31B 报文专项解析

```c
inline int parse_0x31B_msg(stub_ro_handle_t *handle, const can_frame_t *frame, int can_port) {
  if (frame->can_id != 0x31B) return -1;

  switch (frame->data[0]) {
    case 0x06:  // 软件版本号（7字节，data[1..7]）
      memcpy(ver, &frame->data[1], 7);
      break;
    case 0x07:  // VIN 码第1段（7字节，data[1..7]，VIN[0..6]）
      memcpy(vin, &frame->data[1], 7);
      break;
    case 0x08:  // VIN 码第2段（7字节，data[1..7]，VIN[7..13]）
      memcpy(vin + 7, &frame->data[1], 7);
      break;
    case 0x09:  // VIN 码第3段（3字节，data[1..3]，VIN[14..16]）
      memcpy(vin + 14, &frame->data[1], 3);
      break;
  }
}
```

VIN 码为 17 字节标准格式，分 3 帧传输。需要收齐 0x07/0x08/0x09 三帧才有完整 VIN。

### 11.3 CAN 工况数据定时上报（data_table 格式）

每 10s 执行 `report_dtp_msg()`：

```json
{
  "id": "",
  "ts": 1639535683010,
  "action": "post",
  "type": "properties",
  "msg": {
    "type": "data_table",
    "data_tables": [
      {
        "index": 1,
        "name": "table1",
        "channel_id": 1,
        "ts": 1639535683010,
        "data_points": [
          {
            "ts": 1639535683010,
            "index": 3,
            "name": "Nu0001",
            "data_type": "double",
            "value": 12.5
          },
          // ... 所有有接收时间的数据项
          // 额外附加项（slot 0 独有）：
          {
            "index": 106,
            "name": "St0001",
            "data_type": "string",
            "value": "软件版本号"
          },
          {
            "index": 107,
            "name": "St0002",
            "data_type": "string",
            "value": "VIN17位码"
          },
          {
            "index": 61,
            "name": "Nu0013",
            "data_type": "double",
            "value": 12.3   // pwr_volt * 0.001 V
          }
        ]
      }
    ]
  }
}
```

**额外附加数据项（`add_extra_info_to_dtp`，仅 CAN0 通道）：**

| index | 名称 | 数据类型 | 说明 |
|-------|------|----------|------|
| 106 | St0001 | string | 软件版本号（来自 CAN 0x31B 0x06 帧）|
| 107 | St0002 | string | VIN 码（17字节，来自 CAN 0x31B 0x07/08/09 帧）|
| 61 | Nu0013 | double | 电源电压（伏，pwr_volt × 0.001）|

---

## 12. GPS 数据处理与上报

### 12.1 GPS 数据接收

GPS 模块通过 nanomsg port 16005 推送 JSON 数据：

```json
{
  "location_info": {
    "lat": 28.238171,
    "lon": 113.100853,
    "used_nsat": 8,
    "velocity": 0.5,
    "altitude": 58.3
  }
}
```

`handle_gps_msg()` 接收后调用 `parse_gps_info()` 存入 `handle->gps_info`。

### 12.2 GPS 数据上报（两种格式并行）

每 10s 通过两个函数分别上报不同格式：

**格式一：`report_location()`（tbox 格式，发到 port 11215）**

```json
{
  "version": "",
  "id": "",
  "ts": 1639535683010,
  "action": "post",
  "type": "tbox",
  "data": {
    "type": "GPS",
    "data": [{
      "status": true,
      "ts": 1639535683010,
      "longitude": 113.100853,
      "latitude": 28.238171,
      "lon_hemisphere": "E",
      "lat_hemisphere": "N",
      "speed": 0.5,
      "altitude": 58.3,
      "azimuth": 0,
      "magnetic": 0,
      "mag_direction": "E",
      "indication_mode": "A"
    }]
  }
}
```

**格式二：`report_condition_info()`（filter_table 格式，发到 port 11225）**

```json
{
  "id": "",
  "ts": 1639535683010,
  "action": "post",
  "type": "properties",
  "msg": {
    "type": "filter_table",
    "data": [
      {"index": 1, "property_name": "longitude", "data_type": "double", "data": 113.100853},
      {"index": 2, "property_name": "latitude",  "data_type": "double", "data": 28.238171}
    ]
  }
}
```

**GPS 上报条件：** 只有当经纬度都不为 0 才上报（用 `rcpv4_double_cmp` 比较），最后一次有效的经纬度会被保存到静态变量，作为后续无 GPS 信号时的"最后已知位置"。

**编译宏 `TEST_GPS`：** 定义此宏时，静态变量初始值为 `longitude=113.100853, latitude=28.238171`（湖南长沙），用于无 GPS 硬件时测试。

---

## 13. MCU 信息查询机制

### 13.1 查询触发

每 6 秒由 `timer_task()` 驱动（内部计数器 cnt，每 5 次 timer_task 触发一次，5s+1=6次约6秒）：

```c
req_mcu_info(stub_handle);  // 发 REQ 给 MCU 信息代理
```

### 13.2 查询消息格式

```json
{"ts": 1639535683010, "status": {"io": ["mcu"]}}
```

通过 `nn_send(nn_req_mcu_fd, ...)` 发送到 port 38000。

### 13.3 响应解析

`handle_mcu_reply_msg()` 接收到 MCU 信息代理的 REP 响应：

```json
{
  "status": {
    "io": [{
      "dev": "mcu",
      "params": {
        "acc_stat": 1,           → handle->acc_status
        "mcu_pwd_volt": 12300,   → handle->pwr_volt（毫伏）
        "main_ver": 1,           → handle->mcu_main_ver
        "sub_ver": 26            → handle->mcu_sub_ver
      }
    }]
  }
}
```

### 13.4 锁机状态查询

每 6 秒额外发送 `send_req_lock_status()`：

```c
cmd_request_t {
  pid     = getpid()
  cmd_tag = CMD_TAG_LOCK_STATUS_REPLY (204)
  cmd_len = 0
}
```

MCU 收到后主动推送当前锁机状态（见 10.5 节），触发 `handle_cmd_msg()` 的 `CMD_TAG_LOCK_STATUS_REPLY` 分支更新 `current_locked/binded/level`。

---

## 14. 云端消息收发机制（SCP通信）

### 14.1 SCP 消息下行（云端 → 本程序）

```
SCP → port 11216 → stub-linker → on_stub_msg_received() 回调
  │
  │ 线程安全地将消息 copy 入 list_stub_msgs：
  │   mce = calloc(1, sizeof(msg_cache_entry_t))
  │   mce->msg = malloc(msg_len)
  │   memcpy(mce->msg, msg, msg_len)
  │   pthread_mutex_lock(&mutex)
  │   list_add_last(list_stub_msgs, mce)
  │   pthread_mutex_unlock(&mutex)
  │
  ▼
主线程 handle_stub_msgs()
  │ 每轮循环清空 list_stub_msgs：
  │   pthread_mutex_lock → list_remove_first → unlock → do_handle_stub_msg()
  ▼
do_handle_stub_msg() 按 topic_type 分发：
```

| topic_type | 处理函数 | 功能描述 |
|------------|----------|----------|
| `heartbeat` | `on_stub_msg_heartbeat` | 验证 msg_id 一致，更新 `soi.is_online=true`，记录心跳时间 |
| `version` | `on_stub_msg_version` | 协议版本协商，验证版本字符串匹配，设 `protocol_version_responsed=true` |
| `network_metric` | `on_stub_msg_network_metric` | 解析网络状态，更新 `network_connected` 字段 |
| `lock_dev` | `on_stub_msg_lock_dev` | **处理锁机/解锁/绑定命令**（见第10节）|
| `cmd_dev` | `on_stub_msg_cmd_dev` | 处理车辆参数设置命令（VIN码、vehicleCode、engineModelCode等）|
| `info_dev` | `on_stub_msg_info_dev` | 仅打印日志，无实质处理 |
| `notify_dev` | `on_stub_msg_notify_dev` | 仅打印日志，无实质处理 |

**SCP 在线状态监测：** 超过 300 秒未收到心跳回复，`soi.is_online` 标记为 false，打印"SCP is offline"日志。另外，任何消息收到时（不论 topic）都会将在线状态标记为 true（fallback 机制）。

### 14.2 SCP 消息上行（本程序 → 云端）

所有上行消息先通过 `push_msg()` 入队，主线程每轮 `handle_msg_caches()` 清空队列：

```
push_msg(topic_type, msg, len)
  │
  ├─ 如果 list_msg_caches 达到 1024 条上限：
  │    丢弃队头最旧消息（打印警告日志）
  │
  ├─ 分配 msg_cache_entry_t，复制消息内容
  │
  └─ 根据 topic_type 决定插入位置：
       锁机回复/OTA/命令回复 → list_add_first()（插队头）
       其他 → list_add()（追加队尾）

handle_msg_caches()
  │ 循环取出队头消息
  └─ stub_msg_reply(handle_pub_cmd, topic_type, msg, len) → SCP port 11215
```

### 14.3 `on_stub_msg_cmd_dev` 指令处理

处理来自云端的 `cmd_dev` topic（自定义指令类型）：

支持的 `param_type` 值（通过 `do_pdk_run_vehicle_param_command` 处理）：

| param_type | data_type | 功能 |
|------------|-----------|------|
| `vehicleParam` | `vehicleCode` | 接收整车编号，存入 `vehicle_code` |
| `vehicleParam` | `VIN` | 接收 VIN 码，存入 `vin_code` |
| `vehicleParam` | `engineModelCode` | 接收发动机型号（FIXME，暂仅回复成功）|
| `t4Hj` | — | 预留字段（FIXME，暂仅回复成功）|
| `predictiveMaintenance` | — | 预测性维护（FIXME，暂仅回复成功）|

其余 `param_type` 值（如 `tbox`、`geoFence`）当前均走到注释掉的 FIXME 分支。

---

## 15. 点表（Pointsheet）机制详解

### 15.1 点表文件基本信息

`Pointsheet_info1.json`（约 774KB）的顶层字段：

```json
{
  "mver": "SRF",   // 机型版本（SRF = Sany Robot Forklift）
  "sver": 4,       // schema 版本（点表格式版本）
  "pver": 8,       // pointsheet 版本（业务数据版本）
  "mtyp": 1,       // 消息类型
  "sce": ["drive", "power_on", "work", "idle"],  // 工况场景列表
  "dev": [...]     // 设备（CAN 通道）列表
}
```

**数据规模：**

| 项目 | 数量 |
|------|------|
| CAN 通道数（slot）| 2（slot 0 和 slot 1，对应 CAN0 和 CAN1）|
| 每通道顶层 CAN 消息规则数 | 65 条 |
| 每通道数据点数量 | 约 317 个 |
| 数据点类型分布 | Number(浮点): 149, Integer: 115, Bool: 45, String: 8 |

> **注意**：代码中 `CAN_CHN_NUM=3`（支持 3 路 CAN），点表只定义了 2 个 slot（0 和 1），CAN2 的规则为空。

### 15.2 点表 JSON 结构

```json
{
  "dev": [{
    "slot": 0,          // CAN 通道号（0=CAN0, 1=CAN1）
    "typ": 0,           // 设备类型
    "msg": [{           // 该通道下的 CAN 消息规则列表
      "typ": 1,         // 规则类型
      "ofs": 0,         // 匹配字段在帧数据中的位偏移
      "len": 32,        // 匹配字段长度（位）
      "val": "#8CFF0008",  // 匹配值（#前缀表示十六进制，其余为十进制字符串）
      "rls": [{          // 子规则列表（条件分支）
        "typ": 1,
        "ofs": 32,
        "len": 8,
        "val": "8",      // 子规则匹配值
        "pts": [{        // 该子规则命中时解析的数据点列表
          "ofs": 64,     // 数据在帧中的位偏移
          "len": 16,     // 数据长度（位）
          "nm": "Nu0001",// 数据点名称
          "bo": 0,       // 字节序（0=小端/Intel，1=大端/Motorola）
          "udif": 0,     // 上传变化阈值
          "ptyp": 3,     // 数据类型（1=整型, 2=布尔, 3=浮点, 4=字符串）
          "sign": 0,     // 是否有符号（1=有符号，补码）
          "sc": 0.1,     // 比例因子（物理值 = 原始值 * sc + pofs）
          "pofs": -1000, // 偏移量（物理值 = 原始值 * sc + pofs）
          "sce": ["power_on", "idle", "drive", "work"],  // 适用的工况场景
          "scems": [1000, 1000, 1000, 1000],  // 各场景的上传间隔（毫秒）
          "index": 3     // 数据点全局索引（用于上报时标识）
        }]
      }]
    }]
  }]
}
```

### 15.3 `data_process.h` 数据结构

```c
// 数据类型枚举
typedef enum value_type {
  VALUE_TYPE_INTEGER    = 1,  // 整型
  VALUE_TYPE_BOOL       = 2,  // 布尔型
  VALUE_TYPE_NUMBER     = 3,  // 浮点型
  VALUE_TYPE_STRING     = 4,  // 字符串型
  VALUE_TYPE_JSON_STRING= 5,  // JSON 字符串型
} value_type_t;

// 数据点（叶节点）
typedef struct data_item {
  char    *name;          // 数据点名称（如 "Nu0001"）
  uint8_t  offset;        // 位偏移
  uint8_t  length;        // 位长度
  uint8_t  byte_order;    // 字节序（0=小端，1=大端）
  uint8_t  value_type;    // 值类型（value_type_t 枚举）
  uint8_t  sign;          // 是否有符号

  int      index;         // 全局索引（上报用）
  double   upload_diff;   // 上传变化阈值（udif）
  double   param_scale;   // 比例因子（sc）
  double   param_offset;  // 偏移量（pofs）

  char    *value_string;  // 字符串值（value_type 为 STRING 时使用）
  double   value_double;  // 数值（INTEGER/NUMBER/BOOL 统一存 double）
  int64_t  time_rcv;      // 最后接收时间（单调时钟毫秒，0=从未接收过）
} data_item_t;

// 数据规则（树节点，用 uthash 哈希表组织）
typedef struct data_rule {
  uint8_t  type;                  // 规则类型
  uint8_t  offset;                // 匹配字段位偏移
  uint8_t  length;                // 匹配字段位长度
  uint32_t value;                 // 匹配值（哈希表的 key）
  SList   *data_sub_rule_list;    // 子规则列表（条件分支）
  SList   *data_item_list;        // 叶节点数据点列表
  UT_hash_handle hh;              // uthash 宏需要
} data_rule_t;

// 点表管理器
typedef struct data_process_mng {
  char    *machine_ver;       // 机型版本（mver）
  int      schema_ver;        // schema 版本（sver）
  int      pointsheet_ver;    // 点表版本（pver）
  int      msg_type;          // 消息类型（mtyp）
  data_rule_t *data_rule[3];  // 3 路 CAN 各自的规则哈希表头指针
} data_process_mng_t;
```

### 15.4 `data_process.c` 解析引擎

#### 加载流程（`load_pointsheet_json`）

```
load_pointsheet_json()
  │
  ├─ rcpv4_get_file_data() 读取整个 JSON 文件到内存
  ├─ cJSON_Parse() 解析 JSON
  ├─ 提取顶层字段 (mver, sver, pver, mtyp)
  │
  └─ 遍历 dev[] 数组（按 slot 分 CAN 通道）：
       遍历 msg[] 数组：
         get_common_data_rule()   ← 递归构建规则树
           │
           ├─ 解析 typ/ofs/len/val 字段
           ├─ 解析 val 字段（'#' 前缀→十六进制，否则→十进制）
           ├─ 如果有 rls[] 数组：递归调用 get_common_data_rule()
           └─ 如果有 pts[] 数组：逐个创建 data_item_t
                解析 ofs/len/bo/udif/ptyp/sign/index/pofs/sc/nm
         │
         HASH_ADD_INT(data_rule[port], value, data_rule)   ← 以 val 为 key 加入哈希表
```

#### 解析流程（`parse_common_msg`，每收到一帧 CAN 数据时调用）

```
parse_common_msg(data_process_mng, frame_data, can_id, can_port)
  │
  ├─ HASH_FIND_INT(data_rule[can_port], can_id, target_rule)
  │    ← 用 CAN ID 快速查找对应的解析规则（O(1)）
  │
  └─ 如果找到：set_common_data_item(target_rule, frame_data)
       │
       ├─ 如果规则有 data_item_list（叶节点）：
       │    对每个 data_item：
       │      ① 根据 byte_order 提取位域值
       │         小端（Intel）：按字节序 from start_byte 拼接，右移 start_bit，掩码截取
       │         大端（Motorola）：按倒序拼接，右移并截取
       │      ② 处理有符号数（补码展开）
       │      ③ 计算物理值：value_double = raw_value * param_scale + param_offset
       │      ④ 字符串类型：直接 memcpy（并处理大端字节序翻转）
       │      ⑤ 更新 data_item.time_rcv = get_tick_count()（标记"已接收过"）
       │
       └─ 如果规则有 data_sub_rule_list（分支节点）：
            对每个子规则：从帧数据中读取对应位置的值
            如果值 == 子规则的 value → 递归调用 set_common_data_item(子规则)
```

#### 数值编解码细节

**小端（Intel，byte_order=0）位域提取：**
```c
start_byte = offset >> 3;
start_bit  = offset & 7;
end_byte   = start_byte + ((start_bit + length - 1) >> 3);
for (n = start_byte; n <= end_byte; n++) {
  value |= ((uint64_t)data[n]) << ((n - start_byte) * 8);
}
value >>= start_bit;
value &= (UINT64_MAX >> (64 - length));
```

**大端（Motorola，byte_order=1）位域提取：**
```c
for (n = start_byte; n <= end_byte; n++) {
  value |= ((uint64_t)data[n]) << ((end_byte - n) * 8);
}
value >>= (8 * size - length - start_bit);
value &= mask;
```

**有符号数（补码展开）：**
```c
if (sign && length < 64) {
  int64_t m = (uint64_t)1 << (length - 1);
  value = ((int64_t)value ^ m) - m;  // 标准补码转换
}
```

**物理值计算：**
```
physical_value = raw_value * param_scale + param_offset
```

---

## 16. 配置文件说明

### 16.1 `stub.ini` —— stub-linker 框架配置

```ini
[stub]
enabled=true
name=stub-forklift
param=
```

控制框架层面是否启用本插件以及插件名称。

### 16.2 `material_code.ini` —— TBox 物料编码

```ini
[code]
material_code=140902000140A
```

物料编码随设备型号不同而变化，在 `report_dev_info()` 中作为 TBox 组件的 `componentInstCode` 上报给 SCP。

### 16.3 `/opt/conf.ini` —— 设备 ID 配置（不在本仓库）

程序启动时从 `/opt/conf.ini` 的 `[dev]` 节读取 `id` 字段作为 `tbox_id`，这是设备的唯一标识符。

### 16.4 `/tmp/stub-forklift.ini` —— 运行时日志级别持久化

程序将动态调整的日志级别写入 `/tmp/stub-forklift.ini`，下次启动时恢复：

```ini
[log]
level=4   # ERK_LOG 级别值（0=ALL, 1=DEBUG, 2=INFO, ..., 6=ERROR）
```

### 16.5 `/tmp/ql_time_set_flag` —— 时间同步标志文件

此文件存在时，`sync_time_to_mcu()` 不检查网络连接状态直接同步时间到 MCU，用于特殊场景（如离线部署）。

---

## 17. 信号处理机制

### 17.1 SIGUSR1 —— 动态切换日志级别

```c
static void sig_handler(int sig_num) {
  if (SIGUSR1 == sig_num) {
    int level = erk_log_get_level() + 1;          // 当前级别 +1
    if (erk_log_get_max_level() < level) {
      level = erk_log_get_min_level();            // 超过最大值则回绕到最小值
    }
    save_log_level(stub_ro_handle, level);        // 持久化到 /tmp/stub-forklift.ini
    erk_log_set_level(level);
    ERK_LOG_ALL("logger level change to %d", erk_log_get_level());
  }
}
```

**使用方式：**
```bash
kill -SIGUSR1 $(cat /tmp/stub-forklift.pid)  # 每次调高一级，到最高后回到最低
```

### 17.2 SIGUSR2 —— 测试注入（仅 STUB_TEST_INJECT 编译时）

信号处理函数只做最小操作（设标志位），实际执行在 `timer_task()` 中：

```c
#ifdef STUB_TEST_INJECT
  } else if (SIGUSR2 == sig_num) {
    test_lock_pending = 1;   // volatile sig_atomic_t，信号安全
  }
#endif
```

见第 21 节详细说明。

---

## 18. 内存管理与线程安全

### 18.1 内存分配策略

- 所有动态内存使用 `calloc`/`malloc`/`strdup`/`asprintf` 分配，`free` 释放
- JSON 对象通过 `cJSON_ParseWithLength`/`cJSON_Duplicate` 创建，`cJSON_Delete` 释放
- 消息队列条目（`msg_cache_entry_t`、`lock_cmd_info_t`）使用专用 `release_*` 函数释放

### 18.2 线程安全分析

| 数据 | 写线程 | 读线程 | 保护方式 |
|------|--------|--------|---------|
| `list_stub_msgs` | IO 接收线程（`on_stub_msg_received`）| 主线程（`handle_stub_msgs`）| `pthread_mutex_t mutex` |
| `list_msg_caches` | 主线程（`push_msg`）| 主线程（`handle_msg_caches`）| 同一线程，无需锁 |
| `list_lock_cmd_infoes` | 主线程（`on_stub_msg_lock_dev`）| 定时器线程（`handle_cmd_timeout`）| **无保护（潜在竞争风险）** |
| `current_lci` | 定时器线程 | IO 接收线程（`handle_cmd_msg`）| **无保护（潜在竞争风险）** |
| `current_locked/binded/level` | IO 接收线程（`handle_cmd_msg`）| 定时器线程（`handle_cmd_timeout`）| **无保护** |
| `gps_info` | IO 接收线程（`parse_gps_info`）| 主线程（`report_location`）| **无保护（字段读写非原子）** |
| `can_0x31B_vin/ver` | IO 接收线程（`parse_0x31B_msg`）| 主线程（`add_extra_info_to_dtp`）| **无保护** |
| `pwr_volt/mcu_ver` | IO 接收线程（`handle_mcu_reply_msg`）| 主线程（`add_extra_info_to_dtp`）| **无保护** |

> **说明**：上述无保护的字段在实际运行中因为访问频率低、数据量小（int/float），在 ARM 平台的对齐访问场景下通常不会产生明显问题，但从严格意义上讲存在数据竞争。

### 18.3 `stub_ro_try_lock` 死锁保护

```c
static int stub_ro_try_lock(stub_ro_handle_t *ro_handle) {
  int ret = 0;
  int64_t tick = get_tick_count();
  while (EBUSY == (ret = pthread_mutex_trylock(&ro_handle->mutex))) {
    if (!ro_handle->running) return -1;           // 进程退出时终止等待
    if (get_tick_count() - tick >= 20000) {       // 等超 20 秒打印死锁警告
      ERK_LOG_ERROR("there maybe exist deadlock.");
      usleep(5000);
    }
  }
  return !ret ? 0 : -1;
}
```

### 18.4 消息队列溢出保护

- `list_msg_caches`（发送缓存）：上限 1024 条，超过则丢弃最旧条目
- `list_lock_cmd_infoes`（锁机命令队列）：上限 1000 条，超过则回复"队列已满"给云端
- `list_stub_msgs`（SCP 接收消息队列）：无显式上限，但受限于内存

---

## 19. 日志系统

### 19.1 日志配置

```c
static void init_log() {
  erk_log_enable_log_file(true);                  // 开启文件日志
  erk_log_set_max_file_size(2 * 1024 * 1024);     // 单文件最大 2MB
  erk_log_set_file_path("/tmp/stub-forklift.log"); // 日志文件路径
  int log_level = ERK_LEVEL_ERROR;                // 默认错误级别
  load_log_level(stub_ro_handle, &log_level);     // 从 ini 文件恢复持久化的级别
  erk_log_set_level(log_level);
}
```

### 19.2 日志级别

| 级别 | 常量 | 说明 |
|------|------|------|
| 0 | ERK_LOG_ALL | 最详细（心跳计数、消息统计等）|
| 1 | ERK_LOG_DEBUG | 调试信息（锁机命令 hex dump 等）|
| 2 | ERK_LOG_INFO | 一般信息（接收消息打印）|
| 3 | ERK_LOG_NOTICE | 重要通知（收到锁机命令、版本协商等）|
| 4 | ERK_LOG_WARNING | 警告（队列满警告等）|
| 5 | ERK_LOG_ERROR | 错误（默认级别）|

---

## 20. 设备信息上报机制

每 180 秒执行 `report_dev_info()`，上报 `parts_info` 格式的设备组件信息：

```json
{
  "action": "post",
  "type": "parts_info",
  "msg": {
    "info": {
      "vehicleCode": "整车编号",
      "vehicleType": 255,
      "componentInsts": [{
        "componentInstType": "TBOX",
        "componentInstCode": "140902000140A",        // 物料编码
        "componentInstId": "TBox设备ID",
        "componentInstPosition": "148",
        "componentInstAddress": "148",
        "programs": [
          {
            "programType": "APP",
            "programName": "T-box",
            "programId": "6c4347a6-7fac-11ee-80ed-00155d2d2b09",
            "programVersion": "LN1526-DC-T-BAT-CHN"
          },
          {
            "programType": "APP",
            "programName": "MCU",                  // 仅当 MCU 版本非零时附加
            "programId": "e0b86724-319f-11ef-aaaa-00155d4ce850",
            "programVersion": "1.26"               // main_ver.sub_ver
          },
          {
            "programType": "APP",
            "programName": "Pointsheet",           // 仅当点表版本非零时附加
            "programId": "70a9c612-86d1-11ef-821f-00155daa9623",
            "programVersion": "4.8"                // schema_ver.pointsheet_ver
          },
          {
            "programType": "APP",
            "programName": "stub-forklift",
            "programId": "2695ec26-28bd-11ef-954a-00155de10703",  // stub UUID
            "programVersion": "1.1.4.0"
          }
        ]
      }]
    }
  }
}
```

**上报条件：** vehicleCode 必须已从云端下发（`is_vehicle_code_valid=true`）。上报后设置 `ecu_info_reported=true`，之后每 180s 检查一次（非重复上报，除非超时）。

**请求设备信息：** 每 180s 执行 `request_dev_info()`，当 vehicleCode 未获取时，向 SCP 发送 `req_dev_param_info` 请求，直到获取为止。

---

## 21. SIGUSR2 本地测试注入框架

仅在 `cmake -DSTUB_TEST_INJECT=ON` 编译时生效。

### 21.1 测试消息序列

| 序号 | cmd | 说明 |
|------|-----|------|
| [0] | `set` lockEnable.remote=1 | 绑定（允许锁机）|
| [1] | `lock` lock=true, level=1 | 一级锁机（限速）|
| [2] | `unlock` lock=false, level=0 | 解锁 |
| [3] | `lock` lock=true, level=2 | 二级锁机（禁止起步）|
| [4] | `unlock` lock=false, level=0 | 解锁 |
| [5] | `lock` lock=true, level=3 | 三级锁机（**已注释禁用，预期超时**）|
| [6] | `unlock` lock=false, level=0 | 解锁 |
| [7] | `set` lockEnable.remote=2 | 解绑（禁止锁机）|

### 21.2 使用流程

```bash
# 1. 编译（加 STUB_TEST_INJECT）：
cmake -DSTUB_TEST_INJECT=ON -B build && cmake --build build

# 2. 部署并获取 PID（排除 flock 守护进程）：
PID=$(cat /tmp/stub-forklift.pid)  # 或通过 ps 获取

# 3. 依次发送 SIGUSR2 注入测试命令：
kill -SIGUSR2 $PID   # 注入 [0] 绑定
kill -SIGUSR2 $PID   # 注入 [1] 一级锁机
kill -SIGUSR2 $PID   # 注入 [2] 解锁
# ...

# 4. 实时查看日志：
tail -f /tmp/stub-forklift.log
```

### 21.3 关键日志序列

```
[ALL] === TEST INJECT [0/7]: {...}   ← 注入成功
[NOTICE] received SCP lock message  ← 命令进入处理
[ALL] lock_cmd: 0x10, lock_result:0, bind_status:1, lock_status:0
                                    ← MCU 回复绑定成功
[NOTICE] succeeded to post SCP data ← 云端回复成功
```

---

## 22. Git 提交历史与版本演进

```
当前分支：tpad-lock-cmd（正在开发的锁机改进版本）

重要提交记录（从新到旧）：

2525f2c [ADD] 新增 .gitignore，忽略 md 文档目录
c416f05 [BUGFIX] 修复解锁机命令处理中参数错误、队列阻塞等问题，新增 SIGUSR2 本地测试注入框架
        - CMakeLists.txt: 新增 STUB_TEST_INJECT 编译选项
        - stub-sany-forklift.c: 125行改动，包含锁机参数校验修复、队列阻塞问题修复、
          SIGUSR2 测试框架、handle_cmd_timeout 重构

master 分支历史（更早的开发记录）：
22f1a82  修复VCU累计工作时间计算逻辑，更新版本为1.1.7.0
4aaf661  Merge feature/forklift_requirement_20250714
5be4e44  点表修改：ID 81C1F115→9C1F1150，更新 can_0x31B_Nu0030 计算逻辑，版本1.1.5.0
9ebdcd3  收到设备编码后需要保存
732f3e9  31B字段定制解析，支持两路CAN上传数据
89f3d6f  31B报文定制解析，支持两路CAN上传数据
d46ef28  更新点表；优化时间同步逻辑（加网络检查条件）
c9285ec  增加电源电压采集上报；增加点表版本上报
a7222f4  增加 README
4a82ea0  判断系统时间已同步后再向MCU同步时间
5f7fcd3  更新点表为1.5版本；增加时间同步处理
e3c735d  国内机器人叉车初始版本提交
```

**当前分支 vs master 的版本差异：**

- 当前分支（tpad-lock-cmd）停留在 v1.1.4.0
- master 分支后续迭代到了 v1.1.7.0，含更多点表变更和 31B 解析增强
- tpad-lock-cmd 的锁机 BUGFIX（c416f05）还未合回 master

---

## 23. 关键函数调用关系总图

```
main()
├── init_log()                          初始化日志系统（/tmp/stub-forklift.log，2MB）
├── signal(SIGUSR1, sig_handler)        日志级别动态切换
├── [STUB_TEST_INJECT] signal(SIGUSR2, sig_handler)
│
├── stub_ro_new()
│   ├── load_pointsheet_json()          ← data_process.c，加载点表到 data_rule 哈希表
│   │   └── get_common_data_rule()      递归构建规则树（含子规则和数据点）
│   ├── rcpv4_gen_uuid()               生成 instance_uuid
│   ├── get_tbox_id()                  读 /opt/conf.ini
│   ├── get_material_code()            读 material_code.ini
│   ├── scp_online_info_init()         初始化心跳对象
│   ├── stub_new/connect(11215/16/25) 建立 SCP 连接
│   └── connect_nn_sock()              建立 CAN/GPS/CMD/MCU nanomsg 连接
│       ├── sub_client_new(16002/3/4)  ← nn_sock.c
│       ├── pub_client_new(26002/3/4)  ← nn_sock.c
│       ├── sub_client_new(16005/8)    ← nn_sock.c
│       ├── pub_client_new(26008)      ← nn_sock.c
│       └── req_client_new(38000)      ← nn_sock.c
│
├── pthread_create(timer_task_thread)
│   └── ev_run(loop)
│       ├── timer_task() [每1秒]
│       │   ├── [TEST] on_stub_msg_lock_dev()  测试注入
│       │   ├── handle_cmd_timeout()           锁机状态机驱动
│       │   │   ├── list_lock_cmd_infoes → do_run_lock_cmd_internal() → send_lock_msg() → nn_send(26008)
│       │   │   └── current_lci 状态检查 → do_reply_lock_cmd_response_from_cache() → push_msg()
│       │   ├── send_req_lock_status() [6s]    → nn_send(26008, CMD_TAG_LOCK_STATUS_REPLY)
│       │   └── req_mcu_info() [6s]            → nn_send(38000)
│       └── sync_time_to_mcu() [600s]
│           └── set_mcu_time() → nn_send(26008, CMD_TIME_SET)
│
├── pthread_create(nn_recv_thread)
│   └── nn_poll(6路fd, 2000ms)
│       ├── handle_can_msg(port)        接收CAN帧
│       │   ├── parse_common_msg()      ← data_process.c，点表解析
│       │   │   └── set_common_data_item()   位域提取 + 物理值计算
│       │   ├── parse_0x31B_msg()       软件版本/VIN码解析
│       │   └── stub_msg_reply(11225, raw_data)  上报原始CAN帧
│       ├── handle_gps_msg()            接收GPS JSON
│       │   └── parse_gps_info()        更新 gps_info
│       ├── handle_mcu_reply_msg()      接收MCU REP
│       │   └── 更新 acc_status/pwr_volt/mcu_ver
│       └── handle_cmd_msg()            接收CMD回包
│           ├── CMD_TAG_PASS_RD (200)   → 存储6字节锁机密码
│           ├── CMD_TAG_LOCK (202)      → 检查MCU是否接受锁机命令
│           └── CMD_TAG_LOCK_STATUS_REPLY (204) → 更新 current_locked/binded/level
│
└── while(1) 主循环（10ms/轮）
    ├── stub_run(handle_sub_cmd)        驱动 SCP SUB，触发回调 on_stub_msg_received
    │   └── on_stub_msg_received()      将消息 copy 入 list_stub_msgs（mutex 保护）
    │
    ├── run_pub_tasks(tasks, n)         定时任务列表
    │   ├── report_condition_info [10s]  → push_msg(11225, filter_table GPS)
    │   ├── report_location [10s]        → stub_msg_reply(11215, GPS tbox)
    │   ├── report_dtp_msg [10s]         → stub_msg_reply(11225, data_table CAN)
    │   │   └── iterate_data_rule()     遍历点表 data_item.time_rcv != 0 的项
    │   │       └── add_extra_info_to_dtp() 附加 VIN/版本/电压
    │   ├── request_stub_network_status [10s] → push_msg(11215, network_metric request)
    │   ├── request_stub_version [10s]        → push_msg(11215, version request)
    │   ├── broadcast_stub_version [180s]     → push_msg(11215, module version broadcast)
    │   ├── request_stub_heartbeat [180s]     → push_msg(11215, keep_alive request)
    │   ├── request_dev_info [180s]           → push_msg(info_dev, req_dev_param_info)
    │   ├── report_dev_info [180s]            → push_msg(info_dev, parts_info)
    │   └── request_lock_passwd [10s]         → nn_send(26008, CMD_TAG_PASS_RD)（未获取时）
    │
    ├── handle_stub_msgs(handle_sub_cmd)   消费 list_stub_msgs
    │   └── do_handle_stub_msg() → 按 topic 分发：
    │       ├── heartbeat      → on_stub_msg_heartbeat()    更新 soi.is_online
    │       ├── version        → on_stub_msg_version()      协议版本协商
    │       ├── network_metric → on_stub_msg_network_metric() 更新网络状态
    │       ├── lock_dev       → on_stub_msg_lock_dev()     ★ 解析锁机命令→入队
    │       ├── cmd_dev        → on_stub_msg_cmd_dev()      车辆参数设置
    │       ├── info_dev       → on_stub_msg_info_dev()     仅日志
    │       └── notify_dev     → on_stub_msg_notify_dev()   仅日志
    │
    └── handle_msg_caches()             清空发送队列 → stub_msg_reply(11215)
```

---

## 24. 已知问题与待办事项

### 24.1 已注释/禁用的功能

| 位置 | 说明 |
|------|------|
| `do_run_lock_cmd_internal`，level==3 分支 | 三级锁机被注释，下发 level=3 命令将超时回复失败 |
| `handle_msg_caches` 中的心跳在线检查 | 注释掉了"离线时不发消息"的逻辑，因工况高负载时心跳可能延迟 50 秒 |
| `on_stub_msg_cmd_dev` 中的 tbox/geoFence | 有 FIXME 注释，当前无处理逻辑 |
| `do_pdk_run_vehicle_param_command` 中多个 FIXME | engineModelCode/t4Hj/predictiveMaintenance 仅回复成功 |
| `parse_lock_cmd_to_pdk_lock_cmd` 中 local 字段 | `lockEnable.local` 处理被 `#if 0` 注释，仅处理 `remote` 字段 |

### 24.2 线程安全问题

如第 18 节所述，多个共享数据字段在多线程间读写没有加锁保护，存在潜在的数据竞争。在当前嵌入式硬件（单核 ARM）上实际影响有限，但需关注。

### 24.3 版本差异（tpad-lock-cmd vs master）

当前分支（tpad-lock-cmd）相比 master 分支：
- 少了 v1.1.5.0~1.1.7.0 的若干功能迭代（31B双路、点表更新、VCU时间计算修复）
- 多了锁机 BUGFIX（c416f05）和 SIGUSR2 测试框架

若要合并，需要先 cherry-pick 或 rebase 到 master 的最新提交。

### 24.4 CMD 代理协议版本依赖

程序的锁机命令字节值（0x31/0x32/0x40/0x10/0x20）依赖 MCU 固件版本。若 MCU 固件升级到新协议，需要同步修改此处的枚举值（如 0x31→0x01，0x40→0x0F）。

### 24.5 密码 valid 字节未检查

`handle_cmd_msg` 收到 `CMD_TAG_PASS_RD` 回包时，只检查 `cmd_len == 8`，未单独验证 `cmd_bytes[6]`（valid 字节）的值。如果 MCU 返回 valid=0 表示密码无效，当前代码也会直接使用该密码。

---

---

## 25. 补充：遗漏函数完整分析

### 25.1 `do_config_action_request`（stub-sany-forklift.c:371）—— 死代码

```c
static int do_config_action_request(
    stub_ro_handle_t *ro_handle, stub_msg_action_t action_type,
    const char *msg_id, const char *action, const char *type,
    const char *program_code, const char *program_name,
    const char *program_version, const char *config_id,
    stub_config_type_t config_type, int size, const void *config_data)
```

**调用情况：** 经 `grep` 确认，全文件只有第 371 行这一处（函数定义本身），**没有任何调用点**，是彻底的死代码（Dead Code）。

**作用：** 根据函数体逻辑分析，它的设计意图是：
1. 用 `stub_msg_header_make` 构造 type="config" 的消息头
2. 用 `stub_config_make_msg` 填充 OTA/配置相关的消息体（含 programCode、programName、programVersion、configId、configType、configData）
3. 通过 `push_msg(stub_topic_type_config_dev, ...)` 将消息放入发送队列，最终发给 SCP

**推断用途：** 这是为 OTA 固件升级或远程配置功能预留的接口，设计上用于向云端上报配置操作的请求/响应（如固件下载进度、配置变更通知），但目前没有被接入到任何业务流程中。

**结论：** 该函数已具备完整实现，但因为上层业务（OTA/配置处理）未在本程序内实现，所以从未被调用。

---

### 25.2 `data_process.c` 中的 4 个十六进制转换辅助函数

这 4 个函数构成了一个完整的"十六进制字符串→字节数组"转换链，仅在 `get_common_data_rule` 解析点表 `val` 字段时使用。

#### `OneCharToHex`（data_process.c:29）—— 单字符转十六进制

```c
static unsigned char OneCharToHex(char ch) {
  if ('0' <= ch && ch <= '9')   ch = ch - '0';         // '0'~'9' → 0~9
  else if ('a' <= ch && ch <= 'f') ch = ch - 'a' + 10; // 'a'~'f' → 10~15
  else if ('A' <= ch && ch <= 'F') ch = ch - 'A' + 10; // 'A'~'F' → 10~15
  else                              ch = 0xf;            // 非法字符 → 0xf
  return ch;
}
```

#### `TwoCharToHex`（data_process.c:42）—— 两个字符合并成一个字节

```c
static int TwoCharToHex(char charHigh, char charLow, unsigned char *hex) {
  charHigh = OneCharToHex(charHigh);  // 高半字节
  charLow  = OneCharToHex(charLow);   // 低半字节
  *hex = (charHigh << 4) + (charLow & 0xf);  // 拼成一个完整字节
  return 0;
}
```

#### `big_endian`（data_process.c:20）—— 字节数组原地翻转

```c
static void big_endian(char *data, uint8_t len) {
  char tmp[8] = {0};
  memcpy(tmp, data, len);
  for (int i = 0; i < len; i++) {
    data[len - 1 - i] = tmp[i];  // 首尾对调，实现字节序翻转
  }
}
```

用于字符串类型数据项（`VALUE_TYPE_STRING`）在大端字节序时翻转字节顺序。

#### `CharToHexMem`（data_process.c:49）—— 十六进制字符串转字节数组（小端存储）

```c
static void CharToHexMem(char *p, char *pHexMem) {
  uint8_t strLen = strlen(p);
  char pStrTemp[32] = {0};
  strLen = strLen % 2 ? strLen + 1 : strLen;  // 补齐为偶数长度
  if (strLen >= 32) return;                    // 防溢出（最多 15 字节输出）

  memset(pStrTemp, '0', strLen);                // 用 '0' 填充临时缓冲
  memcpy(pStrTemp + (strLen - strlen(p)), p, strlen(p));  // 右对齐复制

  for (uint8_t i = 0; i < strLen; i += 2) {
    // 注意：从字符串末尾往前取（低位字节先存），实现小端字节序
    TwoCharToHex(pStrTemp[strLen - i - 2], pStrTemp[strLen - i - 1],
                 (uint8_t *)&pHexMem[i / 2]);
  }
}
```

**调用位置（data_process.c:200）：**
```c
cJSON *json_value = cJSON_GetObjectItem(json_msg, "val");
if (cJSON_IsString(json_value)) {
  char *value_str = cJSON_GetStringValue(json_value);
  if (!strncmp(value_str, "#", 1)) {
    CharToHexMem(value_str + 1, (char *)&data_rule->value);  // "#8CFF0008" → 0x0800FF8C
  } else {
    int value_temp = atoi(value_str);                         // "8" → 8
    memcpy(&data_rule->value, (char *)&value_temp, data_rule->length / 8);
  }
}
```

**转换示例：**

| 点表 val 字段 | 处理方式 | 存入 data_rule->value |
|--------------|----------|----------------------|
| `"#8CFF0008"` | `CharToHexMem("8CFF0008", ...)` | `0x0800FF8C`（小端存储）|
| `"8"` | `atoi("8") = 8` → memcpy | `0x00000008` |

---

### 25.3 `dtp_msg_header_make`（stub-sany-forklift.c:535）—— DTP 消息头构造

```c
static int dtp_msg_header_make(cJSON **json, const char *id, int64_t ts,
                               stub_msg_action_t action, const char *type) {
  *json = cJSON_CreateObject();
  cJSON_AddStringToObject(*json, "id", id ? id : "");
  cJSON_AddNumberToObject(*json, "ts", ts);
  cJSON_AddStringToObject(*json, "action", stub_from_msg_action(action));
  cJSON_AddStringToObject(*json, "type", type);
  return 0;
}
```

专为 `report_dtp_msg` 构造 data_table 消息的顶层 JSON 对象，与 `stub_msg_header_make`（来自 stub-linker 库）的区别是：该函数不含 `version` 和 `msg_id` 字段，格式更简洁（只有 id/ts/action/type 四个字段）。

**仅在 `report_dtp_msg()` 中调用一次（line 551）。**

---

### 25.4 `parse_json`（stub-sany-forklift.c:810）—— JSON 解析封装

```c
static cJSON *parse_json(const char *msg, int msg_len) {
  cJSON *json = cJSON_ParseWithLength(msg, msg_len);
  if (!json) {
    ERK_LOG_ERROR("invalid JSON, %.*s", msg_len, msg);
    return NULL;
  }
  return json;
}
```

对 `cJSON_ParseWithLength` 的轻量封装，增加了解析失败时的错误日志打印（打印消息原文）。

**调用位置：** 所有需要解析入站 SCP 消息的处理函数均使用此接口：
- `on_stub_msg_lock_dev`（line 1163）
- `on_stub_msg_network_metric`（line 1307）
- `on_stub_msg_heartbeat`（line 1329）
- `on_stub_msg_cmd_dev`（line 1583）
- `on_stub_msg_version`（line 1657）

---

### 25.5 `scp_online_info_init` 与 `scp_online_info_release`（stub-sany-forklift.c:746/755）

#### `scp_online_info_init`

```c
static int scp_online_info_init(stub_ro_handle_t *ro_handle) {
  rcpv4_gen_uuid(&ro_handle->soi.hb.hdr.msg_id);   // 为心跳消息生成唯一 msg_id
  ro_handle->soi.hb.hdr.action = stub_msg_action_request;
  ro_handle->soi.hb.hdr.type   = strdup("keep_alive");
  ro_handle->soi.hb.name       = strdup("stub-forklift");
  ro_handle->soi.hb.uuid       = strdup(ro_handle->instance_uuid);
  return 0;
}
```

在 `stub_ro_new` 中调用，初始化 `soi.hb` 心跳对象的固定字段。`msg_id` 在程序整个生命周期中保持不变（每次心跳都用同一个 msg_id），SCP 回包时用 msg_id 匹配，`on_stub_msg_heartbeat` 里会校验回包的 msg_id 是否与此一致。

#### `scp_online_info_release`

```c
static int scp_online_info_release(stub_ro_handle_t *ro_handle) {
  free(ro_handle->soi.hb.hdr.msg_id);
  free(ro_handle->soi.hb.hdr.type);
  free(ro_handle->soi.hb.name);
  free(ro_handle->soi.hb.uuid);
  return 0;
}
```

在 `stub_ro_release` 中调用，释放 `scp_online_info_init` 中分配的 4 个堆字符串。

---

### 25.6 `release_msg`（stub-sany-forklift.c:763）—— 消息缓存条目释放

```c
static void release_msg(msg_cache_entry_t *mce) {
  if (mce) {
    free(mce->msg);  // 先释放消息内容
  }
  free(mce);         // 再释放条目结构体本身（即使 mce->msg 为 NULL 也安全）
}
```

**所有调用位置：**
- `push_msg` 内队列溢出时丢弃旧条目（line 780）
- `handle_stub_msgs` 消费完消息后释放（line 1814）
- `handle_msg_caches` 发送后释放（line 2097）
- `stub_ro_release` 清空两个队列时使用（lines 2146, 2155）

---

### 25.7 `stub_ro_new`（stub-sany-forklift.c:2651）—— 完整初始化序列

```c
static stub_ro_handle_t *stub_ro_new(void) {
  // ① 分配主结构体（calloc 清零）
  stub_ro_handle_t *stub_handle = calloc(1, sizeof(stub_ro_handle_t));

  // ② 加载点表（失败仅打印日志，不退出，程序可降级运行）
  load_pointsheet_json(&stub_handle->data_process_mng, "Pointsheet_info1.json");

  // ③ 生成本实例唯一 UUID（e2fsprogs uuid_generate 实现）
  rcpv4_gen_uuid(&stub_handle->instance_uuid);

  // ④ 从 /opt/conf.ini 读取 TBox 设备 ID
  get_tbox_id(stub_handle);

  // ⑤ 从 material_code.ini 读取物料编码
  get_material_code(stub_handle);

  // ⑥ 初始化互斥锁（保护 list_stub_msgs）
  stub_ro_lock_init(stub_handle);

  // ⑦ 初始化心跳对象（msg_id、type、name、uuid）
  scp_online_info_init(stub_handle);

  // ⑧ 创建两个消息队列（双向链表）
  list_new(&stub_handle->list_stub_msgs);
  list_new(&stub_handle->list_msg_caches);

  // ⑨ 建立 SCP 通信（通过 stub-linker 框架）
  //    handle_sub_cmd: SUB，订阅 11216，注册 on_stub_msg_received 回调，订阅所有 topic（filter=""）
  //    handle_pub_cmd: PUB，连接 11215（发指令结果/心跳/GPS）
  //    handle_pub_data: PUB，连接 11225（发 CAN 工况数据）
  stub_new(&stub_handle->handle_sub_cmd, STUB_AF_SP_RAW, STUB_SUB, STUB_DONTWAIT);
  stub_connect(stub_handle->handle_sub_cmd, "tcp://127.0.0.1:11216");
  stub_set_msg_cb(stub_handle->handle_sub_cmd, on_stub_msg_received);
  stub_set_filter(stub_handle->handle_sub_cmd, "");
  stub_new(&stub_handle->handle_pub_cmd, STUB_AF_SP_RAW, STUB_PUB, STUB_DONTWAIT);
  stub_connect(stub_handle->handle_pub_cmd, "tcp://127.0.0.1:11215");
  stub_new(&stub_handle->handle_pub_data, STUB_AF_SP_RAW, STUB_PUB, STUB_DONTWAIT);
  stub_connect(stub_handle->handle_pub_data, "tcp://127.0.0.1:11225");
  // 任一步失败 → stub_ro_release() 清理后返回 NULL

  // ⑩ 建立 nanomsg 原生连接（CAN/GPS/CMD/MCU）
  connect_nn_sock(stub_handle);
  // 失败 → stub_ro_release() 返回 NULL

  // ⑪ 将 stub_ro_handle 指针注册到三个 stub handle 的 private_data 中
  //    使得回调函数能通过 stub_get_private_data() 获取到全局状态
  stub_set_private_data(stub_handle->handle_sub_cmd, stub_handle);
  stub_set_private_data(stub_handle->handle_pub_cmd, stub_handle);
  stub_set_private_data(stub_handle->handle_pub_data, stub_handle);

  return stub_handle;
}
```

**关键设计点：** 步骤 ⑨ 的 `stub_set_filter("", 0)` 表示订阅所有 topic（空前缀匹配一切），这意味着 SCP 下发的任何 topic 都会触发 `on_stub_msg_received` 回调，再在 `do_handle_stub_msg` 中按 `topic_type` 分发。

---

### 25.8 `stub_ro_release`（stub-sany-forklift.c:2119）—— 完整资源释放序列

```c
static int stub_ro_release(stub_ro_handle_t *ro_handle) {
  free(ro_handle->instance_uuid);            // ① 释放 UUID 字符串
  scp_online_info_release(ro_handle);        // ② 释放心跳对象的 4 个字符串字段

  close(ro_handle->nn_sub_gps_fd);           // ③ 关闭 GPS SUB socket
  close(ro_handle->nn_sub_cmd_fd);           // ④ 关闭 CMD SUB socket
  close(ro_handle->nn_pub_cmd_fd);           // ⑤ 关闭 CMD PUB socket
  close(ro_handle->nn_req_mcu_fd);           // ⑥ 关闭 MCU REQ socket

  for (int i = 0; i < CAN_CHN_NUM; i++) {
    close(ro_handle->nn_sub_can_fd[i]);      // ⑦ 关闭 CAN SUB sockets (×3)
    close(ro_handle->nn_pub_can_fd[i]);      // ⑧ 关闭 CAN PUB sockets (×3)
  }

  do_release_lock_cmd_info(ro_handle->current_lci);  // ⑨ 释放正在执行的锁机命令
  stub_release(ro_handle->handle_sub_cmd);   // ⑩ 释放 SCP SUB handle
  stub_release(ro_handle->handle_pub_cmd);   // ⑪ 释放 SCP PUB cmd handle
  stub_release(ro_handle->handle_pub_data);  // ⑫ 释放 SCP PUB data handle

  // ⑬ 清空并销毁 list_stub_msgs（逐条 release_msg）
  while (list_size(ro_handle->list_stub_msgs)) {
    msg_cache_entry_t *mce = NULL;
    list_remove_first(ro_handle->list_stub_msgs, (void **)&mce);
    release_msg(mce);
  }
  list_destroy(ro_handle->list_stub_msgs);

  // ⑭ 清空并销毁 list_msg_caches（逐条 release_msg）
  while (list_size(ro_handle->list_msg_caches)) {
    msg_cache_entry_t *mce = NULL;
    list_remove_first(ro_handle->list_msg_caches, (void **)&mce);
    release_msg(mce);
  }
  list_destroy(ro_handle->list_msg_caches);

  stub_ro_lock_destroy(ro_handle);           // ⑮ 销毁互斥锁
  free(ro_handle->tbox_id);                  // ⑯ 释放设备 ID 字符串
  free(ro_handle->material_code);            // ⑰ 释放物料编码字符串
  free(ro_handle->vehicle_code);             // ⑱ 释放整车编号字符串
  free(ro_handle->vin_code);                 // ⑲ 释放 VIN 码字符串
  free(ro_handle);                           // ⑳ 释放主结构体

  return 0;
}
```

> **注意**：`list_lock_cmd_infoes` 队列没有在 `stub_ro_release` 中被清理，这是一个资源泄漏点。`current_lci` 通过 `do_release_lock_cmd_info` 释放，但队列中其余等待中的 `lock_cmd_info_t` 没有被逐条释放。不过由于 `stub_ro_release` 实际只在进程退出时调用，操作系统会自动回收所有内存，因此影响有限。

---

## 26. 补充：完整流程图汇总

本章补充报告其他章节中缺失的 4 张完整流程图。

---

### 26.1 `stub_ro_new` 初始化序列图

```
main()
  │
  ▼
stub_ro_new()
  │
  ├─①─ calloc(stub_ro_handle_t)
  │      └─ 全部字段清零（bool=false, int=0, ptr=NULL）
  │
  ├─②─ load_pointsheet_json("Pointsheet_info1.json")
  │      ├─ rcpv4_get_file_data() 读文件到内存（~774KB）
  │      ├─ cJSON_Parse() 解析 JSON
  │      ├─ 提取 mver/sver/pver/mtyp
  │      └─ 遍历 dev[] → 每个 slot → 每条 msg：
  │           get_common_data_rule()（递归）→ HASH_ADD_INT(data_rule[slot])
  │           [失败仅警告，不阻断后续初始化]
  │
  ├─③─ rcpv4_gen_uuid(&instance_uuid)          （e2fsprogs，每次启动不同）
  │
  ├─④─ get_tbox_id()                            读 /opt/conf.ini [dev] id
  │      └─ 失败则 tbox_id=NULL（后续上报跳过）
  │
  ├─⑤─ get_material_code()                      读 material_code.ini [code] material_code
  │      └─ 失败则 material_code=NULL（上报时用默认值 "140902000105B"）
  │
  ├─⑥─ stub_ro_lock_init()                      pthread_mutex_init(&mutex)
  │
  ├─⑦─ scp_online_info_init()
  │      ├─ rcpv4_gen_uuid(&soi.hb.hdr.msg_id)  心跳消息固定 msg_id
  │      ├─ soi.hb.hdr.action = request
  │      ├─ soi.hb.hdr.type   = "keep_alive"
  │      ├─ soi.hb.name       = "stub-forklift"
  │      └─ soi.hb.uuid       = instance_uuid
  │
  ├─⑧─ list_new(&list_stub_msgs)                收 SCP 消息的线程安全队列
  │     list_new(&list_msg_caches)               发给 SCP 的消息缓冲队列
  │
  ├─⑨─ stub-linker SCP 连接（3 个 handle）
  │      ├─ stub_new(handle_sub_cmd, SUB)
  │      │   stub_connect("tcp://127.0.0.1:11216")
  │      │   stub_set_msg_cb(on_stub_msg_received)   注册消息回调
  │      │   stub_set_filter("")                      订阅所有 topic
  │      │   [失败 → goto release]
  │      ├─ stub_new(handle_pub_cmd, PUB)
  │      │   stub_connect("tcp://127.0.0.1:11215")
  │      │   [失败 → goto release]
  │      └─ stub_new(handle_pub_data, PUB)
  │          stub_connect("tcp://127.0.0.1:11225")
  │          [失败 → goto release]
  │
  ├─⑩─ connect_nn_sock()                        nanomsg 原生连接
  │      ├─ sub_client_new(16002) → nn_sub_can_fd[0]
  │      ├─ pub_client_new(26002) → nn_pub_can_fd[0]
  │      ├─ sub_client_new(16003) → nn_sub_can_fd[1]
  │      ├─ pub_client_new(26003) → nn_pub_can_fd[1]
  │      ├─ sub_client_new(16004) → nn_sub_can_fd[2]
  │      ├─ pub_client_new(26004) → nn_pub_can_fd[2]
  │      ├─ sub_client_new(16005) → nn_sub_gps_fd
  │      ├─ sub_client_new(16008) → nn_sub_cmd_fd
  │      ├─ pub_client_new(26008) → nn_pub_cmd_fd
  │      └─ req_client_new(38000) → nn_req_mcu_fd
  │         [任一失败 → goto release]
  │
  ├─⑪─ stub_set_private_data(handle_sub_cmd,  stub_handle)
  │     stub_set_private_data(handle_pub_cmd,  stub_handle)
  │     stub_set_private_data(handle_pub_data, stub_handle)
  │     [使回调函数能通过 stub_get_private_data() 拿到全局状态指针]
  │
  └─ 返回 stub_handle（成功）或 NULL（任一步失败后 stub_ro_release 清理）

release（失败路径）:
  stub_ro_release(stub_handle) → 释放已分配的所有资源 → return NULL
```

---

### 26.2 锁机状态机状态转换图

```
                     ┌─────────────────────────────────────────────┐
                     │               状态总览                        │
                     └─────────────────────────────────────────────┘

  ┌──────────────────────────────────────────────────────────────────────────────┐
  │                          S0: 未就绪                                           │
  │            lock_cmd_password_is_valid = false                                │
  │            无法执行任何锁机命令                                                │
  └───────────────────────────────┬──────────────────────────────────────────────┘
                                  │  每10s: request_lock_passwd() 发 CMD_TAG_PASS_RD
                                  │  IO线程收到 CMD_TAG_PASS_RD 回包 cmd_len==8
                                  ▼
  ┌──────────────────────────────────────────────────────────────────────────────┐
  │                          S1: 空闲就绪                                          │
  │            lock_cmd_password_is_valid = true                                 │
  │            current_lci = NULL                                                │
  │            list_lock_cmd_infoes 为空                                          │
  └──────┬────────────────────────┬──────────────────────────────────────────────┘
         │                        │
         │ 云端下发 lock_dev 消息  │ 队列有命令 AND current_lci==NULL
         │ on_stub_msg_lock_dev() │ handle_cmd_timeout() 检测到
         │ 创建 lci，入队          │ 从队头取出 lci
         ▼                        ▼
  ┌──────────────────────────────────────────────────────────────────────────────┐
  │                          S2: 等待执行                                          │
  │            list_lock_cmd_infoes.size > 0                                     │
  │            current_lci = NULL                                                │
  │            lci 在队列中等待，timeout = 入队时刻 + 30s                          │
  └───────────────────────────────┬──────────────────────────────────────────────┘
                                  │ handle_cmd_timeout() 取出队头 lci
                                  │ lci->timeout 重置为 now + 60s
                                  │ current_lci = lci
                                  │ do_run_lock_cmd_internal() → send_lock_msg()
                                  ▼
  ┌──────────────────────────────────────────────────────────────────────────────┐
  │                          S3: 执行中                                            │
  │            current_lci != NULL                                               │
  │            已向 MCU 发送 CMD_TAG_LOCK 命令                                    │
  │            等待 MCU 的两种回包：接受确认 or 状态推送                            │
  └──────┬───────────────────────────────────────────────────┬────────────────────┘
         │                                                   │
         │ [MCU拒绝] IO线程收到                              │ [MCU接受] IO线程收到
         │ CMD_TAG_LOCK(202) cmd_bytes[0]!=0                 │ CMD_TAG_LOCK(202) cmd_bytes[0]==0
         │                                                   │
         ▼                                                   ▼
  ┌──────────────────┐                          ┌──────────────────────────────────┐
  │ 立即失败回复       │                          │ 继续等待状态推送                    │
  │ lock_error_failed │                          │ CMD_TAG_LOCK_STATUS_REPLY(204)   │
  │ current_lci=NULL  │                          │ 更新 current_locked/level/binded │
  └──────────────────┘                          └──────────────┬───────────────────┘
         │                                                     │
         ▼                                           ┌─────────┴──────────┐
       [→S1]                                         │                    │
                                              状态匹配预期          状态不匹配
                                         (lock==current_locked     继续等待
                                         AND level==current_level)
                                                     │
                                                     ▼
                                          ┌──────────────────────┐
                                          │ 成功回复 SCP           │
                                          │ lock_error_success    │
                                          │ current_lci = NULL    │
                                          └──────────┬───────────┘
                                                     │
                                                     ▼
                                                   [→S1]

  ─────────────── 超时路径（从 S2 和 S3 都可能触发）────────────────

  S2 中 lci 超时（now >= lci->timeout，即入队后 30s 未被取到执行）：
    → do_reply_lock_cmd_response_from_cache(lock_error_failed, "run lock command timeout")
    → 从队列删除，释放内存 → 继续检查下一条

  S3 中 current_lci 超时（now >= lci->timeout + 10s，即发出命令后 70s 无响应）：
    → do_reply_lock_cmd_response_from_cache(lock_error_failed, "run lock command timeout")
    → current_lci = NULL → [→S1]

  ─────────────── 特殊情况：level==3 ──────────────────────────────

  do_run_lock_cmd_internal() 中 level==3 分支被注释：
    → 直接调用 do_reply_lock_msg_response(lock_error_param, "unsupported lock level")
    → 返回 -1
    → handle_cmd_timeout() 检测到 ret<0 → do_release_lock_cmd_info(lci)
    → current_lci = NULL → [→S1]
```

---

### 26.3 CAN 数据从接收到上报的完整流程图

```
                        CAN 总线（MCU 发出）
                               │
                               │ 每帧 20 字节（can_frame_t）
                               ▼
                 nanomsg SUB port 16002/16003/16004
                               │
                               │ nn_recv(..., NN_DONTWAIT)
                               │ 返回 len 字节（可含多帧）
                               ▼
              ┌────────────────────────────────────┐
              │    handle_can_msg(handle, port)     │  ← IO 接收线程
              └────────────────┬───────────────────┘
                               │
                    size = len / sizeof(can_frame_t)
                    分配 stub_can_msg_t[size] 数组
                               │
                    ┌──────────┴──────────┐
                    │  循环处理每一帧 n    │
                    └──────────┬──────────┘
                               │
              ┌────────────────▼────────────────────┐
              │  parse_common_msg()                  │
              │  ← data_process.c                   │
              │                                      │
              │  HASH_FIND_INT(data_rule[port],      │
              │                frame.can_id, target) │
              │  ├─ 找不到：返回 -1，跳过该帧          │
              │  └─ 找到 target_rule：                │
              │      set_common_data_item()          │
              │        ├─ 有 data_item_list（叶节点）：│
              │        │   对每个 data_item：          │
              │        │   ① 位域提取（按字节序）       │
              │        │   ② 有符号数补码展开           │
              │        │   ③ 物理值 = raw*sc + pofs    │
              │        │   ④ time_rcv = get_tick_count │
              │        └─ 有 data_sub_rule_list：      │
              │            逐个比对子规则 value         │
              │            匹配则递归调用               │
              └─────────────────────────────────────┘
                               │
              ┌────────────────▼────────────────────┐
              │  parse_0x31B_msg()                   │
              │  若 can_id != 0x31B → 直接跳过        │
              │  data[0]==0x06 → memcpy 软件版本(7B)  │
              │  data[0]==0x07 → memcpy VIN[0..6]    │
              │  data[0]==0x08 → memcpy VIN[7..13]   │
              │  data[0]==0x09 → memcpy VIN[14..16]  │
              └─────────────────────────────────────┘
                               │
              过滤扩展帧：can_id>>28 > 9 → continue（不上报）
                               │
              填充 can_msg[valid_cnt]：
                ts, bus_id, frame_type=data
                id_type（标准帧/扩展帧）
                id, length, data[8]
              valid_cnt++
                               │
                    ┌──────────┴──────────┐
                    │ 所有帧处理完成        │
                    └──────────┬──────────┘
                               │
              if valid_cnt > 0：
                               │
              ┌────────────────▼────────────────────┐
              │  stub_can_msg_entry_make_from()      │
              │  → 将 can_msg[] 打包成               │
              │    stub_can_msg_entry_t 结构体        │
              │                                      │
              │  stub_msg_entry_new(                 │
              │    ts, stub_msg_type_can,            │
              │    can_msg_entry, size)              │
              │  → 封装为 stub_msg_entry_t           │
              └─────────────────────────────────────┘
                               │
              stub_msg_reply(handle_pub_data,
                             stub_topic_type_data_dev_raw_data,
                             msg_entry, entry_size)
                               │
                               ▼
              SCP port 11225（原始 CAN 帧实时上报）
                               │
              ┌────────────────┴────────────────────┐
              │ 统计：                               │
              │ can_msg_recv_cnt[port]++             │
              │ can_msg_success/fail_cnt[port] += n  │
              └─────────────────────────────────────┘
                               │
              nn_freemsg(msg)   free(can_msg)

──────────── 每 10 秒：工况数据汇总上报（主线程）────────────────

              report_dtp_msg()
                               │
              dtp_msg_header_make(&json, "", ts, post, "properties")
              stub_msg_body_make(json, Object)
              json_msg["type"] = "data_table"
              json_data_tables = []
                               │
              iterate_data_rule(ro_handle, json_data_tables)
                               │
                    ┌──────────┴──────────────────┐
                    │ for i in [0, 1, 2]（3路CAN） │
                    └──────────┬──────────────────┘
                               │
              json_data_points = []
                               │
              HASH_ITER(hh, data_rule[i], rule, tmp)
                               │
              check_data_item_value_json(rule, json_data_points, ts)
                    │
                    ├─ rule 有 data_item_list：
                    │   对每个 data_item：
                    │   if data_item->time_rcv != 0（曾接收过）：
                    │     构造 JSON 数据点对象：
                    │     { ts, index, name, data_type, value }
                    │     ─ INTEGER → data_type="int32"
                    │     ─ NUMBER  → data_type="double"
                    │     ─ BOOL    → data_type="boolean"
                    │     ─ STRING  → data_type="string"
                    │     加入 json_data_points[]
                    │
                    └─ rule 有 data_sub_rule_list：递归处理
                               │
              if i == 0：
                add_extra_info_to_dtp()        ← 仅 CAN0 通道额外附加
                  ├─ 软件版本非空 → index=106, name="St0001", string
                  ├─ VIN码17字节 → index=107, name="St0002", string
                  └─ pwr_volt>0  → index=61,  name="Nu0013", double (×0.001)
                               │
              if json_data_points 非空：
                构造 data_table 条目：
                { index=i+1, name="table1",
                  channel_id=i+1, ts, data_points }
                加入 json_data_tables[]
                               │
              if json_data_tables 非空：
                cJSON_PrintUnformatted(json)
                stub_msg_reply(handle_pub_data,
                               stub_topic_type_data_dev_data,
                               payload, len)
                               │
                               ▼
              SCP port 11225（data_table 工况汇总上报）
```

---

### 26.4 消息队列生产者-消费者流程图

本程序有两个消息队列，分属不同方向、不同线程角色：

```
═══════════════════════════════════════════════════════════════════════
         list_stub_msgs —— SCP 下行消息队列（云端 → 本程序）
═══════════════════════════════════════════════════════════════════════

  生产者：IO 接收侧（由 stub-linker 框架在 stub_run() 内触发回调）
  ──────────────────────────────────────────────────────────────────

  SCP port 11216 收到消息
        │
        ▼
  on_stub_msg_received(handle, topic_type, msg, msg_len)
  [此回调在主线程 stub_run() 调用栈内执行]
        │
        ├─ calloc(msg_cache_entry_t)
        ├─ malloc(msg_len)，memcpy(msg, msg_len)
        ├─ mce->topic_type = topic_type
        │
        ├─ stub_ro_try_lock()   ← 自旋等待互斥锁（最多 20s 打印死锁警告）
        │
        ├─ list_add_last(list_stub_msgs, mce)   ← 追加到队尾
        │
        └─ stub_ro_unlock()

  消费者：主线程（在主循环 handle_stub_msgs() 中）
  ──────────────────────────────────────────────────────────────────

  handle_stub_msgs()
        │
        while list_size(list_stub_msgs) > 0：
          │
          ├─ stub_ro_try_lock()
          ├─ list_remove_first(list_stub_msgs, &mce)   ← 取队头
          ├─ stub_ro_unlock()
          │
          ├─ do_handle_stub_msg(handle, mce->topic_type, mce->msg, mce->len)
          │    ├─ heartbeat      → on_stub_msg_heartbeat()
          │    ├─ version        → on_stub_msg_version()
          │    ├─ network_metric → on_stub_msg_network_metric()
          │    ├─ lock_dev       → on_stub_msg_lock_dev()  →  list_lock_cmd_infoes 入队
          │    ├─ cmd_dev        → on_stub_msg_cmd_dev()
          │    ├─ info_dev       → on_stub_msg_info_dev()
          │    └─ notify_dev     → on_stub_msg_notify_dev()
          │
          └─ release_msg(mce)   ← 释放缓存条目和消息内容

  队列特性：无上限（依赖内存）；FIFO；mutex 保护

═══════════════════════════════════════════════════════════════════════
         list_msg_caches —— SCP 上行消息队列（本程序 → 云端）
═══════════════════════════════════════════════════════════════════════

  生产者（多处调用 push_msg()，均在主线程或定时器线程）
  ──────────────────────────────────────────────────────────────────

  调用来源                               topic_type             插入位置
  ─────────────────────────────────────  ─────────────────────  ────────
  do_reply_lock_msg_response()           lock_dev               队头（优先）
  do_cmd_reply()                         cmd_dev                队头（优先）
  report_dtp_msg()                       data_dev_data          队尾
  report_condition_info()                data_filter_data       队尾
  report_dev_info()                      info_dev               队尾
  request_dev_info()                     info_dev               队尾
  request_stub_heartbeat()               heartbeat              队尾
  request_stub_version()                 version                队尾
  broadcast_stub_version()               version                队尾
  request_stub_network_status()          network_metric         队尾
  do_config_action_request()（死代码）    config_dev             队尾

  push_msg() 内部逻辑：
        │
        ├─ if list_size >= 1024：
        │    list_remove_first()，release_msg()  ← 丢弃最旧消息并警告
        │
        ├─ calloc(msg_cache_entry_t)，malloc(len)，memcpy
        │
        └─ switch(topic_type)：
             lock_dev / ota_dev / cmd_dev → list_add_first()  ← 插队头
             其他                         → list_add()         ← 追加队尾

  消费者：主线程（在主循环 handle_msg_caches() 中）
  ──────────────────────────────────────────────────────────────────

  handle_msg_caches()
        │
        while list_size(list_msg_caches) > 0：
          │
          ├─ list_remove_first(list_msg_caches, &mce)   ← 取队头（最高优先级）
          │
          ├─ stub_msg_reply(handle_pub_cmd, mce->topic_type, mce->msg, mce->len)
          │    └─ 通过 stub-linker 发送到 SCP port 11215
          │
          ├─ 打印结果日志（succeeded/failed）
          │
          └─ release_msg(mce)

  队列特性：上限 1024；优先队列（锁机/OTA/命令回复在头部）；单线程读写，无需锁

═══════════════════════════════════════════════════════════════════════
         list_lock_cmd_infoes —— 锁机命令执行队列
═══════════════════════════════════════════════════════════════════════

  生产者：主线程 on_stub_msg_lock_dev()
        │ do_save_lock_cmd_info(ro_handle, lci)
        └─ list_add(list_lock_cmd_infoes, lci)   ← 追加队尾

  消费者：定时器线程 handle_cmd_timeout()（每1秒）
        │ LIST_FOREACH(list_lock_cmd_infoes)
        ├─ 超时（now >= lci->timeout）→ 直接失败回复，从队列删除
        └─ current_lci==NULL → list_remove()，设 current_lci=lci，开始执行

  队列特性：上限 1000；FIFO；
            ⚠ 跨线程读写（主线程写、定时器线程读）无互斥锁保护
```

---

*本报告已对仓库全部源文件（stub-sany-forklift.c 2768行、data_process.c 346行、data_process.h 59行、nn_sock.c 100行、nn_sock.h 96行、CMakeLists.txt 69行、Pointsheet_info1.json 774KB、stub.ini、material_code.ini、README.md）完成逐行核实，包含全部函数、数据结构、配置文件、流程图，无遗漏。最终更新于 2026-05-20。*
