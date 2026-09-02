# rootcloud-stp-sdk 仓库全面技术分析报告

> 分析时间：2026-04-22  
> 代码规模：177 个 C/H 源文件，约 115,000 行代码  
> 目标平台：aarch64 (RK3576) / armv7-a  
> 版本：ERK v0.8.1.16 / RCPv4 v0.17.12.128

---

## A. 全局概览

### A.1 项目定位（一句话）

**这是树根互联（RootCloud）工业 IoT 设备端 SDK，实现了设备通过 MQTT/STP 协议向云平台上报工况、位置、事件，并接收 OTA、命令下发、锁机解锁等控制指令的完整闭环。**

### A.2 目录结构与职责

```
rootcloud-stp-sdk/
│
├── CMakeLists.txt              # 根构建入口，组织所有子模块
├── build-rk3576.sh             # RK3576 aarch64 交叉编译脚本
├── cmake/
│   ├── toolchain_aarch64.cmake # aarch64 工具链（RK3576 Buildroot）
│   └── toolchain_armv7-a.cmake # armv7-a 工具链（老平台兼容）
│
├── erk-3rd-party/
│   └── stub-linker-git/        # ★ 核心 IPC 库
│       ├── stub-linker.c/h     # nanomsg pub/sub 封装，定义全部 topic 类型
│       ├── stub-msg.c/h        # 消息帧结构：帧头/payload 组装与解析
│       ├── stub-bind.c/h       # 绑定/换绑/更改标识协议封装
│       └── stub-config.c/h     # stub 配置（端口、地址等）
│
├── erk-core/                   # ★ SDK 核心框架
│   ├── erk/
│   │   ├── erk.c               # ERK 核心：libev 事件循环 + MQTT 连接管理
│   │   └── erk-instance.c      # ERK 实例：进程单例锁（PID 文件）
│   ├── rcpv4/                  # RCPv4 协议层（共 20+ 文件）
│   │   ├── rcpv4.c             # 协议主入口：插件加载/卸载，rcpv4_new/release
│   │   ├── rcpv4-cmd-lock.c    # 锁机命令处理（云平台→设备）
│   │   ├── rcpv4-cmd-cmd.c     # 自定义命令下发处理
│   │   ├── rcpv4-cmd-config.c  # 设备配置同步
│   │   ├── rcpv4-cmd-info.c    # 设备信息/自注册
│   │   ├── rcpv4-cmd-live.c    # 实时数据（工况查询）
│   │   ├── rcpv4-cmd-shadow.c  # 设备影子（属性同步）
│   │   ├── rcpv4-cmd-response.c# 命令回复公共逻辑
│   │   ├── rcpv4-aul.c         # OTA 文件下载（断点续传）
│   │   ├── rcpv4-ahrr.c        # 事件/告警上报
│   │   ├── rcpv4-adl.c         # 设备数据链路层
│   │   └── rcpv4-cmd-cache.c   # 命令超时缓存管理
│   ├── rcpv4-utils/            # 工具库（md5/sha256/aes/ecdsa/base64/download/iniparser）
│   ├── erk-log/                # 日志模块（带等级、文件/行号）
│   ├── erk-utils/              # 工具函数（时间戳/信号/UUID等）
│   └── profiles/               # 物模型配置文件
│       ├── thing-profile.json  # 物模型属性定义
│       ├── data-profile.json   # 数据格式定义
│       └── connection-profile.json # 连接参数模板
│
├── erk-plugins/                # ★ 插件层
│   └── plugins/
│       ├── scp-adapter/        # SCP2.0 适配器插件（最核心，~50000行）
│       │   ├── scp.c           # SCP 协议主实现（3009行）
│       │   ├── scp-adapter.c   # 插件主体：nanomsg↔RCPv4 桥接（7460行）
│       │   ├── scp-raw-data-filter.c # 原始工况数据过滤/存储（6675行）
│       │   ├── scp-rule-filter.c     # 规则过滤引擎（1361行）
│       │   ├── scp-cmd-hook.c        # 命令钩子（自定义命令扩展）
│       │   ├── scp-profile-sync.c    # 物模型同步
│       │   ├── scp-config-sync.c     # 配置参数同步
│       │   ├── scp-j1939-adapter.c   # J1939 协议适配
│       │   └── tools/          # 各设备型号的控制器托管程序（stub）
│       │       ├── stub-sany-crane/       # 三一重起履带吊 stub
│       │       ├── stub-sany-forklift/    # ★ 三一叉车 stub（含锁机逻辑）
│       │       ├── stub-sany-ota/         # 三一 OTA 专用 stub
│       │       ├── stub-simulator/        # 仿真测试用 stub
│       │       └── stub-rdexp-flash/      # LED 闪灯 Demo stub
│       └── virtual-adapter/    # 虚拟适配器插件（无 CAN 总线测试用）
│
├── erk-agent/                  # ★ 可执行程序入口
│   ├── main.c                  # 程序启动：初始化 rcpv4，运行事件循环
│   └── erk-agent.c             # 命令行参数解析、日志初始化、配置加载
│
└── erk-otad/                   # OTA 守护进程（独立进程，负责实际升级执行）
```

### A.3 模块划分与依赖关系

```
┌──────────────────────────────────────────────────────────────────┐
│                         erk-agent (main)                          │
│              rcpv4_new() → rcpv4_task_run() → ev_loop            │
└──────────────────┬───────────────────────────────────────────────┘
                   │ 加载插件（dlopen）
                   ▼
┌──────────────────────────────┐   ┌────────────────────────────┐
│     erk-core / rcpv4         │   │   erk-core / ERK           │
│  ┌─────────────────────────┐ │   │  ┌──────────────────────┐  │
│  │ rcpv4.c                 │ │   │  │ erk.c (libev loop)   │  │
│  │ rcpv4-cmd-lock.c        │◄├───┼──┤ erk_mqtt_t           │  │
│  │ rcpv4-cmd-cmd.c         │ │   │  │ MQTT pub/sub/connect │  │
│  │ rcpv4-aul.c (OTA下载)   │ │   │  └──────────────────────┘  │
│  └─────────────────────────┘ │   └────────────────────────────┘
│             │                │                │
│    device_adapter_t 接口      │          MQTT → CMS/ESMP
└──────────────┬───────────────┘
               │ 实现 device_adapter_t 虚函数表
               ▼
┌──────────────────────────────────────────────────────────────────┐
│                    scp-adapter (插件)                             │
│  scp-adapter.c ←→ stub-linker (nanomsg) ←→ 控制器托管程序(stub) │
│       │                                                          │
│  scp-raw-data-filter.c  (工况存储、压缩、上传)                    │
│  scp-rule-filter.c      (DBC 解析、J1939 规则引擎)               │
│  scp-profile-sync.c     (物模型下发与同步)                        │
│  scp-cmd-hook.c         (自定义命令透传)                          │
└──────────────────────────────────────────────────────────────────┘
               │
               ▼ nanomsg pub/sub (端口 11215/11216/11225)
┌──────────────────────────────────────────────────────────────────┐
│            控制器托管程序 (stub-sany-forklift 等)                  │
│  CAN socketcan ← CAN总线 → ECU/MCU                               │
│  锁机/解锁机命令 → SPI/CAN → MCU                                  │
└──────────────────────────────────────────────────────────────────┘
```

**依赖关系（从底到顶）：**

```
nanomsg → stub-linker-git
libev + umqtt → erk-core/erk
cJSON + collections-c + libssl → erk-core/rcpv4-utils
erk + rcpv4-utils + stub-linker → rcpv4
rcpv4 + DBC解析库 + j1939-adapter → scp-adapter插件
scp-adapter + rcpv4 → erk-agent（最终可执行文件）
```

### A.4 关键入口文件及启动流程

```
main() [erk-agent/main.c]
  │
  ├─1. init_env()          ← 设置 locale、工作目录
  ├─2. argv_handle()       ← 解析 -e endpoints.json -c config.json
  ├─3. init_log()          ← 初始化日志（文件+终端，等级可运行时 SIGUSR1 切换）
  ├─4. rcpv4_new()         ← 创建 RCPv4 句柄
  │     ├── 解析 endpoints.json（MQTT 地址/凭证）
  │     ├── 解析 config.json（设备 ID、物模型等）
  │     └── dlopen 加载插件（scp-adapter.so 或 virtual-adapter.so）
  │           └── 插件 init() → stub_init() → nn_bind(11215/11216/11225)
  │
  ├─5. rcpv4_task_new() + rcpv4_task_run()
  │     └── 启动事件循环线程（libev ev_loop）
  │           ├── MQTT 连接/重连（erk_mqtt_connect → auto reconnect）
  │           ├── 订阅云平台 topic（命令/OTA/配置/锁机）
  │           └── 定时器：心跳、统计计数器 dump（每10秒）
  │
  ├─6. erk_signal_init()   ← SIGUSR1（动态切换日志等级）、SIGPIPE（忽略）
  │
  └─7. while(running) usleep(200ms)  ← 主循环，仅做计数器 dump
        └── rcpv4_task_is_running() 返回 false 时退出

  ┌─── 后台线程（scp-adapter 内部）─────────────────────────────────┐
  │  stub_data_thread:  stub_run(handle_sub_data)  ← 接收工况(11225) │
  │  stub_cmd_thread:   stub_run(handle_sub_cmd)   ← 接收命令(11215) │
  │  raw_filter_thread: 原始数据压缩/写文件/上传                      │
  │  comp_thread:       数据压缩线程                                   │
  └──────────────────────────────────────────────────────────────────┘
```

### A.5 技术栈与第三方库

| 库/技术 | 版本/说明 | 用途 |
|---------|-----------|------|
| **libev** | 4.x | 异步事件循环（TCP/MQTT/定时器/信号） |
| **nanomsg** | 1.x | 本地 IPC pub/sub，stub-linker 的底层 |
| **umqtt** | 内嵌 | MQTT 3.1/3.1.1 客户端协议栈 |
| **cJSON** | 1.7.x | JSON 序列化/反序列化 |
| **collections-c** | — | HashTable/HashSet/Deque/List |
| **OpenSSL / mbedTLS** | — | TLS/MQTT加密，AES/ECDSA/SHA256 |
| **uthash** | 2.x | 嵌入式哈希表（宏实现） |
| **DBC 解析库** | 内嵌 | CAN 数据库文件解析（cantools_config/dbcmodel） |
| **j1939-adapter** | 内嵌 | J1939 PGN/DM 协议适配 |
| **iniparser** | 内嵌 | INI 配置文件解析 |
| socketCAN | Linux 内核 | CAN 总线硬件接口 |
| CMake | ≥2.8.12 | 构建系统 |
| aarch64-buildroot-linux-gnu | RK3576 | 交叉编译工具链（RK3576 目标） |

---

## B. 深度逐模块分析

### B.1 stub-linker（erk-3rd-party/stub-linker-git）

**职责：** nanomsg IPC 的高层封装，是 SCP2.0 插件与所有控制器托管程序（stub）之间唯一的通信介质。

**对外接口（关键 API）：**

```c
// 创建发布/订阅句柄，绑定或连接到指定地址（端口）
int stub_new(stub_handle_t **handle, int domain, int protocol,
             const char *addr, int flags);

// 发送一条消息（内部封装帧头：topic_size + topic + payload_size + payload）
int stub_send(stub_handle_t *handle, stub_topic_type_t topic,
              const void *data, int len);

// 轮询接收（阻塞或非阻塞，由 stub_opt_type_run_waitable 控制）
int stub_run(stub_handle_t *handle);

// 中断 stub_run 的阻塞（线程安全，内部用 socketpair 事件通知）
int stub_break(stub_handle_t *handle);

// 注册消息到达回调
int stub_set_on_receive(stub_handle_t *handle, stub_on_receive_t cb, void *userdata);

// 高层消息构造（用于 scp-adapter 调用）
stub_msg_entry_t* stub_msg_entry_new(uint64_t seq, const char *id, int64_t ts,
                                     int64_t msg_id, stub_msg_action_t action,
                                     stub_msg_type_t type, const void *data, int len);
```

**核心数据结构：**

```c
#pragma pack(push, 1)
// 帧头（紧凑内存布局，1字节对齐）
typedef struct stub_msg_header {
    uint8_t  topic_size;      // topic 字节数
    char     topic[topic_size]; // topic 字符串（可变长）
    uint32_t payload_size;    // payload 字节数（小端）
    // 后接 payload
} stub_msg_header_t;
#pragma pack(pop)

// stub_filter_msg_t：工况二进制帧的容器
typedef struct {
    stub_filter_msg_type_t type;  // raw / j1939_dm / cbor
    uint32_t size;
    uint8_t  padding[];           // 实际 payload（柔性数组）
} stub_filter_msg_t;
```

**端口绑定策略：**

| 句柄 | 协议 | 地址 | 绑定者 |
|------|------|------|--------|
| handle_sub_data | XSUB | tcp://127.0.0.1:11225 | 插件 bind，托管程序 connect |
| handle_sub_cmd  | XSUB | tcp://127.0.0.1:11215 | 插件 bind，托管程序 connect |
| handle_pub_cmd  | XPUB | tcp://127.0.0.1:11216 | 插件 bind，托管程序 connect |

**线程模型：** stub_break 通过内部 `socketpair` 的事件 fd 唤醒阻塞中的 stub_run，实现跨线程安全中断，无需 mutex。

---

### B.2 ERK 核心（erk-core/erk）

**职责：** 基于 libev 的单线程异步 MQTT 客户端框架，是整个 SDK 的网络底座。

**核心数据结构：**

```c
struct erk_handle {
    struct ev_loop  *loop;          // libev 事件循环实例
    HashTable       *signals;        // 信号处理器哈希表
    bool             running;
    void            *userdata;
    HashSet         *mqtt_connectors; // 持有的所有 erk_mqtt_t 集合
};

struct erk_mqtt {
    erk_handle_t    *handle;
    struct ev_timer *reconnect_timer; // 自动重连定时器
    struct ev_timer *publish_timer;   // 自动发布定时器
    erk_mqtt_conn_opt_t opt;          // MQTT 连接参数（含 TLS）
    struct umqtt_client *mqtt_handle; // umqtt 底层句柄
    char            *host;
    uint16_t         port;
    bool             auto_reconnect;
    // ...
};

struct erk_mqtt_mgr {
    erk_handle_t *handle;
    bool          running;
    erk_counter_t counter;           // TCP/MQTT 统计计数器
    // 所有回调：on_network_error/close/status/connect_ack/publish/subscribe_ack
};
```

**生命周期：**

```
erk_handle_new()
  └── ev_loop_new(EVFLAG_AUTO)

erk_mqtt_new(handle)
  └── 关联 erk_handle_t，加入 mqtt_connectors HashSet

erk_mqtt_connect(mqtt, host, port, opt)
  └── 异步 TCP 连接，ev_io 注册读写事件
        └── CONNACK 回调 → on_connect_ack()

erk_handle_run(handle, wait=true)   ← 主循环，阻塞直到 erk_handle_stop()
  └── ev_run(loop, EVRUN_ONCE 或 0)

erk_handle_stop()  ← 线程安全，内部 ev_break(EVBREAK_ALL)
```

**设计特点：** 整个 ERK 是无锁单线程模型，所有 API 只能在创建 erk_handle_t 的同一线程调用。

---

### B.3 RCPv4 协议层（erk-core/rcpv4）

**职责：** 树根互联私有云协议 v4 的设备端实现，管理插件生命周期、命令路由、OTA 流程、设备影子等。

**核心数据结构：**

```c
typedef struct rcpv4_handle {
    erk_mqtt_t     *mqtt;          // MQTT 连接
    erk_handle_t   *ev_handle;     // libev 事件循环
    device_adapter_t *adapter;     // 插件适配器（虚函数表）
    rcpv4_cmd_cache_t *cmd_cache;  // 命令超时队列
    // OTA、配置、锁机、影子...各子模块句柄
} rcpv4_handle_t;

// 命令结构（平台下发或 stub 上报）
typedef struct rcpv4_cmd {
    char           *msg_id;        // 消息 UUID
    char           *connection_id; // 设备 ID
    rcpv4_thing_type_t thing_type;
    stp_data_desc_t *dd;           // 数据描述：类型+指针+大小
    // ...
} rcpv4_cmd_t;
```

**关键子模块职责：**

| 文件 | 职责 |
|------|------|
| `rcpv4.c` | 插件 dlopen/dlsym 加载，MQTT topic 订阅/发布路由 |
| `rcpv4-cmd-lock.c` | 解析云平台锁机命令，验证签名，转发给插件 |
| `rcpv4-cmd-cmd.c` | 自定义命令（geoFence 等）的 request/response 处理 |
| `rcpv4-aul.c` | OTA 文件下载，支持断点续传，HTTP GET + MD5/SHA256 校验 |
| `rcpv4-cmd-cache.c` | 命令超时管理，防止无限等待 stub 回复 |
| `rcpv4-ahrr.c` | 事件/告警上报，包含 HJ 1322-2023 国四排放数据 |

**device_adapter_t 虚函数表（插件接口）：**

```c
typedef struct device_adapter {
    // 基础信息
    char* (*get_uuid)(device_adapter_t*, bool is_instance);
    int   (*set_name)(device_adapter_t*, const char *name);

    // 锁机
    int   (*on_lock)(device_adapter_t*, rcpv4_cmd_t *cmd);
    int   (*on_lock_status)(device_adapter_t*, rcpv4_device_info_lock_status_t*);

    // OTA
    int   (*on_ota)(device_adapter_t*, rcpv4_cmd_t *cmd);
    int   (*on_ota_progress)(device_adapter_t*, rcpv4_cmd_t*, int progress);

    // 命令
    int   (*on_cmd)(device_adapter_t*, rcpv4_cmd_t *cmd);
    int   (*on_info)(device_adapter_t*, rcpv4_cmd_t *cmd);
    int   (*on_config)(device_adapter_t*, rcpv4_cmd_t *cmd);
    int   (*on_bind)(device_adapter_t*, rcpv4_cmd_t *cmd);

    // 生命周期
    int   (*start)(device_adapter_t*, rcpv4_handle_t*);
    int   (*stop)(device_adapter_t*);
    int   (*release)(device_adapter_t*);
} device_adapter_t;
```

---

### B.4 scp-adapter 插件（erk-plugins/plugins/scp-adapter）

**职责：** 这是整个 SDK 的业务核心，负责：
1. 通过 stub-linker (nanomsg) 接收托管程序的工况/事件/状态数据
2. 经 DBC/J1939 规则引擎解析工况，过滤后上传
3. 将云平台下发的 OTA/命令/锁机/配置命令转发给对应 stub
4. 实现完整的锁机状态机（带 progress/status 多次上报机制）

**scp_adapter_t 核心字段（scp-adapter-internal.h）：**

```c
typedef struct scp_adapter {
    device_adapter_t da;           // 必须是第一个字段（C 的多态）

    // nanomsg 句柄
    stub_handle_t *handle_sub_data;  // 订阅工况数据（11225）
    stub_handle_t *handle_pub_data;  // 发布到工况通道
    stub_handle_t *handle_sub_cmd;   // 订阅命令通道（11215）
    stub_handle_t *handle_pub_cmd;   // 发布命令到 stub（11216）

    // DBC 相关
    DBCModel      *dbc_items[MAX_DBC_ITEM_COUNT]; // 最多 10 个 DBC 文件
    int            dbc_count;

    // 锁机状态
    scp_dev_lock_info_t        *lock_info;         // 当前进行中的锁机命令
    scp_dev_lock_status_info_t *lock_status_info;  // 锁机状态（带 progress）

    // OTA
    scp_ota_sh_task_info_t ota_sh_task_info;

    // 数据过滤
    scp_raw_data_filter_handle_t *rd_handle;  // 原始数据缓存/过滤

    // 计数器
    scp_counter_t counter;

    // 线程
    pthread_t      data_pid;        // 工况接收线程
    pthread_t      cmd_pid;         // 命令接收线程

    // 连接状态
    HashSet       *connection_ids;   // 在线的 stub connection_id 集合
    hash_stub_verson_t *stub_versions; // uthash：各 stub 版本信息
} scp_adapter_t;
```

**锁机状态机（scp-adapter.c）：**

```
                   ┌──────────────────────┐
                   │   IDLE（无 lock_info）│
                   └──────────┬───────────┘
                              │ on_lock() 收到云平台 lock/unlock 命令
                              ▼
                   ┌──────────────────────┐
                   │   PENDING（等待 stub）│  timeout=300s，retries=true
                   │   lock_info 非 NULL  │
                   └────┬──────────┬──────┘
          stub 回复      │          │  超时（300s）
       (response)        │          │
            ┌────────────┘          └────────────┐
            ▼                                    ▼
┌───────────────────────┐           ┌────────────────────────┐
│ progress+status 模式  │           │  超时回复 error 给平台  │
│ waiting → done        │           │  lock_info=NULL → IDLE  │
│（可多次上报进度）     │           └────────────────────────┘
└───────────┬───────────┘
            │ progress==100 && status=="done" 或 code!=0
            ▼
   ┌────────────────────┐
   │ 上报结果给云平台   │
   │ lock_info=NULL     │
   └────────────────────┘
```

---

### B.5 stub-sany-forklift（叉车托管程序）

**职责：** 三一叉车的控制器托管程序，实现：
1. SocketCAN 接收 CAN 帧，通过 stub-linker 发送给 scp-adapter
2. 接收 scp-adapter 的锁机/解锁命令，通过 CAN/SPI 发送给 MCU
3. 维护锁机状态、绑定状态

**锁机命令类型（lock_cmd_type_t）：**

```c
lock_cmd_null         = 0x00
lock_cmd_bind         = 0x10  // 绑定
lock_cmd_unbind       = 0x20  // 解绑
lock_cmd_one_lock     = 0x31  // 一级锁
lock_cmd_two_lock     = 0x32  // 二级锁
lock_cmd_three_lock   = 0x33  // 三级锁
lock_cmd_unlock       = 0x40  // 解锁
lock_cmd_bind_one_lock   = 0x51  // 绑定+一级锁
lock_cmd_bind_two_lock   = 0x52  // 绑定+二级锁
lock_cmd_bind_three_lock = 0x53  // 绑定+三级锁
lock_cmd_unbind_unlock   = 0x60  // 解绑+解锁
lock_cmd_reset_engine_type = 0x90 // 重置发动机类型
```

**与其他模块的耦合点：**
- 通过 `stub-linker` API（stub_new/send/run/break）与 scp-adapter 通信
- 通过 `socketcan` (`linux/can.h`) 与 CAN 总线硬件交互
- 通过 `erk-log` 输出日志
- 通过 `data_process.c` 做 CAN 数据的 DBC 解析

---

### B.6 scp-raw-data-filter（原始数据缓存模块）

**职责：** 工况数据的本地缓存、压缩（gzip）、文件分片存储、加密上传。

**线程模型（多线程，有 mutex）：**

```
主线程（scp-adapter）
   │  push to deque
   ▼
raw_pid 线程（stub-linker 工况接收）
   │  filter_mutex 保护 DBC 解析
   ▼
comp_pid 线程（gzip 压缩 + 写文件）
   │  sem_t 信号量触发
   ▼
export_task 线程（文件上传 HTTP）
   │  mutex + cond 控制
   ▼
erk-otad（上传成功后删除临时文件）
```

**关键同步原语：**
- `pthread_mutex_t filter_mutex`：保护 DBC 解析/filter 操作
- `pthread_mutex_t mutex`：保护数据队列
- `sem_t sem`：触发压缩线程开始工作
- `pthread_cond_t cond`：控制上传任务启停

---

## C. 安全与 Bug 审计

### C.1 潜在 Bug 列表

| # | 文件 | 行号 | 严重度 | 问题描述 |
|---|------|------|--------|---------|
| 1 | `rcpv4/rcpv4-cmd-lock.c` | 92 | 🔴 高 | **复制粘贴错误**：`if (json_delay)` 应为 `if (json_cts)`，导致 `json_cts` 为 NULL 时 `cts` 始终为 0，锁机时间戳校验逻辑完全失效 |
| 2 | `rcpv4/rcpv4.c` | 2722–2730 | 🟡 中 | `strcat(real_name, ...)` 动态拼接插件名，但 `real_name` 分配大小计算 `n = strlen(plugin_name)` 后没有为前缀 `lib` 和后缀 `.so` 预留足够空间，若输入插件名恰好不含 "lib" 前缀时可能堆溢出 |
| 3 | `tools/stub-sany-crane/stub-ota.c` | 770 | 🔴 高 | `strcpy(ota_cmd.package_name, oi->file_name)` 未检查 `file_name` 长度，若文件名超过 `package_name` 字段固定大小（取决于结构体定义），会发生**栈/堆缓冲区溢出** |
| 4 | `tools/stub-sany-ota/stub-ota.c` | 770 | 🔴 高 | 同上（两处代码完全相同，疑似 copy-paste） |
| 5 | `scp-adapter/scp-raw-data-filter.c` | 578, 612, 629 等 20+ 处 | 🟡 中 | `asprintf()` 返回值未检查（仅赋值给 `ret` 但随后忽略），在内存耗尽时 `filename/data` 指针为 NULL，后续直接解引用会导致**空指针崩溃** |
| 6 | `erk-core/erk/erk-instance.c` | 39–41 | 🟡 中 | `strcat(fuuid, "/tmp/")` + `strcat(fuuid, ".pid")`，但 `fuuid` 是定长数组，若 UUID 字符串过长会**栈溢出** |
| 7 | `rcpv4-utils/download.c` | 70–73 | 🟡 中 | 多次 `strcat` 拼接文件名，缓冲区大小依赖运行时参数，无越界保护 |
| 8 | `scp-adapter/scp-adapter.c` | 2630–2667 | 🟠 中 | `calloc(lock_info)` 成功后立即用到 `lock_info->entry.cmd`，但中间有 `le->cmd = rcpv4_cmd_dup(sa->lock_info->entry.cmd)`，若此时 `sa->lock_info->entry.cmd` 为旧值 NULL 会 crash |

### C.2 内存安全问题

**1. calloc 后未检查 NULL（scp-adapter.c 多处）：**

```c
// 第 1122 行
scp_timer_task_entry_t *task = calloc(1, sizeof(*task));
// ❌ 未检查 task == NULL，直接使用
task->...
```

**2. 锁机状态信息的 double-free 风险（scp-adapter.c）：**

```c
// 约第 2596 行
static int scp_lock_cmd_info_release(scp_dev_lock_info_t *lock_info) {
  if (!lock_info) return 0;
  rcpv4_cmd_release(lock_info->entry.cmd); // cmd 可能已被 release
  free(lock_info);
}
```
若 `rcpv4_cmd_release` 内部再次触发某回调并再次调用本函数，存在 double-free 风险。

**3. rd-export/rd-export.c 第 147 行：**

```c
strcpy(block, base); // base 来自外部，长度未校验
```

### C.3 并发/竞态条件风险

**1. filter_mutex 自旋等待（scp-raw-data-filter.c 第 318 行）：**

```c
while (EBUSY == (ret = pthread_mutex_trylock(&rd_handle->filter_mutex))) {
    // 自旋，无 usleep，在高频工况下 CPU 空转
}
```
应使用 `pthread_mutex_lock()` 阻塞等待，而非自旋。

**2. connection_ids HashSet 未加锁访问：**

`scp-adapter.c` 中 `sa->connection_ids` 在主线程和 stub_cmd 线程中均有读写操作，但未见一致的互斥保护，存在 data race。

**3. ota_event_type 字段跨线程读写（scp-adapter.c / virtual-adapter.c）：**

`ota_event_type` 在多线程中被设置和读取，未使用原子操作或 mutex 保护。

### C.4 错误处理缺失

**1. rcpv4.c 插件加载（dlopen/dlsym）：**

```c
// 未检查 dlsym 返回 NULL 的情况
adapter->on_lock = dlsym(handle, "on_lock");
// 若符号不存在，后续调用 adapter->on_lock() 会段错误
```

**2. nn_bind/nn_connect 失败后继续运行（stub-linker.c 第 152–175 行）：**

```c
int ret = nn_bind(handle->fd, addr);
if (ret < 0) {
    fprintf(stderr, "nn_bind: %s\n", nn_strerror(nn_errno()));
    // ❌ 仅打印错误，未返回错误码，程序继续运行
}
```
nanomsg 绑定失败后，所有后续的 pub/sub 都是静默失败的。

**3. cJSON 解析失败后未清理资源（rcpv4-cmd-lock.c 第 70 行）：**

```c
cJSON *json_lock = cJSON_GetObjectItem(json, "lock");
if (!json_lock) {
    cJSON_Delete(json); // ✅ 正确释放
    return -1;
}
// 但此后如果中途失败，有多处 cJSON_Delete(json) 缺失
```

### C.5 安全漏洞

**1. 硬编码 UUID（scp-adapter.c 第 281 行）：**

```c
return strdup("3413816a-ddba-11ec-aa98-b370a26e7c88");
// 所有未配置 instance_uuid 的设备共享同一 UUID
```

**2. OTA 签名验证仅支持 MD5（文档明确说明）：**

SDK 版本 0.3.5.98 之前只支持 MD5 签名，MD5 已不安全，存在碰撞攻击风险。

**3. 锁机签名验证逻辑（rcpv4-cmd-lock.c）：**

由于 Bug #1（`if (json_delay)` 条件判断错误），当 `json_cts` 为 NULL 时签名验证分支可能被跳过，导致**不验签**直接执行锁机命令。

---

## D. IPC / 通信协议分析

### D.1 IPC 拓扑图（完整消息流向）

```
云平台 (CMS/ESMP)
     │ MQTT publish/subscribe
     │ TLS TCP
     ▼
erk-agent (main process)
  ├── erk_mqtt_t (umqtt client, libev驱动)
  └── rcpv4_handle_t
        │
        │ device_adapter_t 虚函数调用
        ▼
scp-adapter.so (loaded by dlopen)
  ├── scp_adapter_t
  │     ├── handle_sub_data ─── nanomsg XSUB ← 11225 ← stub.pub（工况）
  │     ├── handle_sub_cmd  ─── nanomsg XSUB ← 11215 ← stub.pub（命令回复/状态）
  │     └── handle_pub_cmd  ─── nanomsg XPUB → 11216 → stub.sub（命令下发）
  │
  └── 内部线程
        ├── data_thread:  stub_run(sub_data)  → CAN数据 → DBC解析 → MQTT上报
        ├── cmd_thread:   stub_run(sub_cmd)   → 锁机结果/OTA进度 → MQTT上报
        ├── raw_filter_thread: 数据写文件
        └── comp_thread:       gzip压缩 + HTTP上传

控制器托管程序 (stub-sany-forklift, 独立进程)
  ├── nn_connect(11225) → pub CAN工况数据
  ├── nn_connect(11215) → pub 命令执行结果/状态
  └── nn_connect(11216) → sub 接收命令下发

物理层
  └── SocketCAN (CAN0/CAN1) ←→ ECU/MCU
```

### D.2 所有通信协议

#### nanomsg 帧格式（stub-linker）

```
+──────────────+──────────────+──────────────────+──────────────+
│ topic_size   │   topic      │   payload_size   │   payload    │
│   1 byte     │ n bytes      │   4 bytes (LE)   │ m bytes      │
+──────────────+──────────────+──────────────────+──────────────+
```

- **topic_size**：无符号单字节，表示 topic 字符串的字节数
- **topic**：ASCII 可打印字符串，例如 `/stub/lock/dev`
- **payload_size**：4字节小端无符号整数
- **payload**：JSON（UTF-8 压缩格式）或二进制（`stub_filter_msg_t`）

#### JSON Payload 通用头部

```json
{
  "version": "",
  "id":      "<设备UUID或空串>",
  "ts":      1639535683010,
  "msg_id":  "<UUID，request/response时必须>",
  "action":  "request|response|broadcast|post",
  "type":    "<业务类型字符串>",
  "data":    {}
}
```

#### CAN 数据帧（JSON）

```json
{
  "type": "CAN",
  "data": [{
    "bus_id":     0,
    "frame_type": "data",   // data/remote/error/overload
    "id_type":    "20B",    // 20A=标准帧, 20B=扩展帧
    "id":         2596985277,
    "ts":         1639535683010,
    "size":       8,         // 原始数据字节数（非base64长度）
    "data":       "AAECAwQFBgc="  // base64编码
  }]
}
```

#### 锁机命令帧（插件→托管程序）

```json
{
  "action": "request", "type": "cmd",
  "data": {
    "type": "controller",
    "cmd": "lock|unlock|set|get",
    "data": {
      "level": 2,
      "lock": true
    }
  }
}
```

#### 锁机结果帧（托管程序→插件）

```json
{
  "action": "response", "type": "cmd",
  "data": {
    "type": "controller",
    "cmd": "lock",
    "data": {
      "last":     true,      // 执行前状态
      "lock":     false,     // 本次命令
      "locked":   false,     // 当前状态（必须）
      "binded":   true,
      "level":    1,
      "progress": 50,        // 可选，0-100
      "status":   "waiting", // 可选，waiting/done
      "code":     0,
      "desc":     "",
      "tips":     ""
    }
  }
}
```

### D.3 锁机状态机

参见 B.4 中的状态机图。补充 progress/status 规则：

| 条件 | SDK行为 |
|------|---------|
| 同时有 `progress` + `status`，格式正确 | 保持会话，等待后续上报 |
| 不包含 `progress`/`status` | 立即关闭会话 |
| `progress > 100` | 立即关闭会话 |
| `progress < 0` | 重置为 0，继续等待 |
| `progress == 100` 且 `status == "done"` | 正常完成，关闭会话 |
| `code != 0` | 立即关闭会话（上报错误） |

### D.4 超时/重试/错误恢复机制

| 机制 | 位置 | 参数 |
|------|------|------|
| MQTT 自动重连 | `erk_mqtt_set_auto_reconnect` | 可配置间隔（ms） |
| 锁机命令超时 | `scp-adapter.c: lock_info->timeout` | 默认 300s |
| 命令缓存超时 | `rcpv4-cmd-cache.c` | 每条命令独立超时 |
| OTA 下载超时 | `rcpv4-aul.c` | 由平台下发 timeout 字段控制 |
| OTA 超时默认 | `rcpv4-cmd-lock.c` 注释 | 插件侧默认 10 分钟 |
| nanomsg 接收超时 | `stub_opt_type_run_timeout` | scp-adapter 中设为 2000ms |
| 换绑超时 | `rcpv4_cmd_t.timeout` | 平台指定，单位毫秒 |

---

## E. 完整技术参考文档

### E.1 架构总览

本仓库实现了一个**三层架构的工业 IoT 设备端 SDK**：

```
[云平台层]  CMS（MQTT接入）↔ ESMP（设备管理）
              ↕ MQTT/TLS
[SDK框架层] erk-agent → ERK(libev) → RCPv4 → device_adapter_t接口
              ↕ dlopen 插件机制
[插件适配层] scp-adapter → stub-linker(nanomsg) → 控制器托管程序(stub)
              ↕ SocketCAN
[物理设备层] CAN总线 → ECU/MCU/传感器
```

### E.2 所有源文件功能说明

#### 根目录

| 文件 | 功能 |
|------|------|
| `CMakeLists.txt` | 根构建脚本，管理所有子模块编译 |
| `build-rk3576.sh` | RK3576 aarch64 一键交叉编译脚本 |

#### cmake/

| 文件 | 功能 |
|------|------|
| `toolchain_aarch64.cmake` | aarch64-buildroot-linux-gnu 工具链定义 |
| `toolchain_armv7-a.cmake` | armv7-a（旧平台）工具链定义 |

#### erk-3rd-party/stub-linker-git/

| 文件 | 功能 |
|------|------|
| `stub-linker.h/c` | nanomsg IPC 高层封装：创建/发送/接收/中断 |
| `stub-msg.h/c` | 消息帧结构定义与构造/解析工具函数 |
| `stub-bind.h/c` | 绑定/换绑/更改标识协议消息的构造与解析 |
| `stub-config.h/c` | stub 配置加载（从 INI 文件读取端口、地址等） |

#### erk-core/erk/

| 文件 | 功能 |
|------|------|
| `erk.c` | libev 事件循环、MQTT 连接、回调注册 |
| `erk-instance.c` | 进程实例管理（PID 文件单例锁）|

#### erk-core/erk-log/

| 文件 | 功能 |
|------|------|
| `erk-log.c` | 多级别日志，支持文件+终端输出，SIGUSR1 动态切换等级 |

#### erk-core/rcpv4/

| 文件 | 功能 |
|------|------|
| `rcpv4.c` | 协议主入口：初始化/销毁，插件 dlopen，MQTT topic 路由 |
| `rcpv4-cmd-lock.c` | 解析云平台锁机命令，验证 MD5 签名，转发给插件 |
| `rcpv4-cmd-cmd.c` | 自定义业务命令（geoFence 等）处理 |
| `rcpv4-cmd-config.c` | 设备配置参数下发与同步 |
| `rcpv4-cmd-info.c` | 设备信息/自注册数据处理（parts_info/cell_site等） |
| `rcpv4-cmd-live.c` | 实时工况查询（query 类型命令） |
| `rcpv4-cmd-shadow.c` | 设备影子（属性期望值/上报值同步） |
| `rcpv4-cmd-response.c` | 通用命令响应构建工具函数 |
| `rcpv4-cmd-cache.c` | 命令超时缓存队列，防止无响应命令无限堆积 |
| `rcpv4-cmd-schema.c` | 命令 JSON Schema 校验 |
| `rcpv4-cmd-time.c` | 时间同步命令处理 |
| `rcpv4-cmd-file.c` | 文件传输命令处理 |
| `rcpv4-aul.c` | OTA 文件下载（HTTP GET，断点续传，校验） |
| `rcpv4-ahrr.c` | 事件/告警上报（含 HJ 1322-2023 国四数据） |
| `rcpv4-adl.c` | 设备数据链路：CAN/位置/工况数据的通用上报通道 |

#### erk-core/rcpv4-utils/

| 文件 | 功能 |
|------|------|
| `md5.c/aes.c/sha256.c/ecdsa.c/ecies.c` | 密码学工具 |
| `base64.c` | Base64 编解码 |
| `download.c` | HTTP 文件下载（断点续传） |
| `iniparser.c` | INI 配置文件解析 |
| `utils.c` | 通用工具（时间戳/UUID/字符串处理） |
| `dictionary.c` | 字典数据结构 |

#### erk-agent/

| 文件 | 功能 |
|------|------|
| `main.c` | 程序入口：初始化→启动→主循环→退出 |
| `erk-agent.c` | 命令行参数解析、日志初始化、配置路径加载 |
| `erk-agent.h` | 对外 API（版本号接口） |
| `app_info.ini` | 程序版本信息 |
| `config.json` | 设备配置模板（设备ID/物模型等） |
| `endpoints.json` | MQTT 接入点配置（broker地址/凭证） |
| `mappings.ini` | 信号映射配置 |

#### erk-plugins/plugins/scp-adapter/

| 文件 | 功能 |
|------|------|
| `scp.c` | SCP2.0 协议核心实现（3009行） |
| `scp-adapter.c` | 插件主体：nanomsg↔RCPv4 桥接，锁机/OTA/命令路由（7460行） |
| `scp-raw-data-filter.c` | 工况原始数据缓存/过滤/压缩/上传（6675行） |
| `scp-rule-filter.c` | DBC 规则过滤引擎（1361行） |
| `scp-cmd-hook.c` | 命令钩子：自定义命令扩展机制 |
| `scp-profile-sync.c` | 物模型文件同步（下发 DBC/profile 文件） |
| `scp-config-sync.c` | 配置参数同步处理 |
| `scp-j1939-adapter.c` | J1939 DM（诊断消息）协议适配 |

#### erk-plugins/plugins/scp-adapter/tools/

| 目录/文件 | 功能 |
|-----------|------|
| `stub-sany-crane/stub-sany-crane.c` | 三一履带吊控制器托管程序（274K，含 OTA、锁机） |
| `stub-sany-forklift/stub-sany-forklift.c` | 三一叉车控制器托管程序（83K，含多级锁机） |
| `stub-sany-ota/stub-sany-ota.c` | 三一 OTA 专用（含三一私有 OTA SDK libhqroot.so） |
| `stub-simulator/main.c` | 工况仿真器（读取 CAN 录制文件回放，用于测试） |
| `stub-rdexp-flash/` | LED 闪灯简单示例 stub |

#### erk-plugins/plugins/virtual-adapter/

| 文件 | 功能 |
|------|------|
| `virtual-adapter.c` | 无 CAN 总线的虚拟适配器，用于纯软件测试 |

### E.3 对外 API / 接口说明

#### RCPv4 公开 API（erk-core/include/rcpv4/rcpv4.h）

```c
// 创建 RCPv4 句柄（加载配置+插件）
int rcpv4_new(rcp_type_t type, const char *endpoints,
              const char *config, rcpv4_handle_t **handle);

// 释放 RCPv4 句柄（卸载插件、断开 MQTT）
int rcpv4_release(rcpv4_handle_t *handle);

// 版本信息
uint32_t rcpv4_version_major/minor/patch/revision(void);

// 获取 MQTT 句柄（用于信号绑定等）
erk_mqtt_t* rcpv4_get_mqtt(rcpv4_handle_t *handle);

// 统计计数器 dump（JSON 格式）
int rcpv4_counter_dump(rcpv4_handle_t *handle, int64_t interval_ms, char **data);

// 任务管理
rcpv4_task_t* rcpv4_task_new(rcpv4_handle_t *handle);
int rcpv4_task_run(rcpv4_task_t *task);   // 启动事件循环线程
int rcpv4_task_stop(rcpv4_task_t *task);
bool rcpv4_task_is_running(rcpv4_task_t *task);
int rcpv4_task_release(rcpv4_task_t *task);
```

#### stub-linker 公开 API

```c
// 创建 IPC 句柄
int stub_new(stub_handle_t **handle, int domain, int protocol,
             const char *addr, int flags);

// 发送消息
int stub_send(stub_handle_t *handle, stub_topic_type_t topic,
              const void *data, int len);

// 高层消息发送（含完整帧封装）
int stub_msg_reply(stub_handle_t *handle, stub_topic_type_t topic,
                   stub_msg_entry_t *entry, int len);

// 轮询接收（阻塞或非阻塞）
int stub_run(stub_handle_t *handle);

// 中断 stub_run
int stub_break(stub_handle_t *handle);

// 注册接收回调
int stub_set_on_receive(stub_handle_t *handle, stub_on_receive_t cb, void *userdata);

// 设置选项
int stub_set_option(stub_handle_t *handle, stub_opt_type_t type,
                    const void *val, const int *len);

// 释放
int stub_release(stub_handle_t *handle);
```

#### 插件 device_adapter_t 接口（需实现的虚函数表）

参见 B.3 节。

### E.4 关键流程时序图（文字版）

#### 锁机完整时序

```
云平台 ESMP      rcpv4/erk-agent    scp-adapter       stub-sany-forklift    MCU
    │                  │                 │                    │                │
    │──lock cmd─────►  │                 │                    │                │
    │  (MQTT pub)      │                 │                    │                │
    │              rcpv4_cmd_on_post_lock│                    │                │
    │                  │──on_lock()────► │                    │                │
    │                  │   (虚函数)       │                    │                │
    │                  │                 │─nanomsg pub(11216)─►                │
    │                  │                 │  /stub/lock/dev     │                │
    │                  │                 │  cmd=lock,level=2   │                │
    │                  │                 │                    │──CAN/SPI──────►│
    │                  │                 │                    │  锁机命令       │
    │                  │                 │                    │◄──ack──────────│
    │                  │                 │◄─nanomsg pub(11215)─                │
    │                  │                 │  progress=50,       │                │
    │                  │                 │  status=waiting     │                │
    │◄──progress 50─── │◄─update_lock()──│                    │                │
    │  (MQTT pub)       │                 │                    │                │
    │                  │                 │◄─nanomsg pub(11215)─                │
    │                  │                 │  progress=100,      │                │
    │                  │                 │  status=done,code=0 │                │
    │◄──locked result── │◄─update_lock()──│                    │                │
    │  (MQTT pub)       │                 │  lock_info=NULL     │                │
```

#### OTA 完整时序

```
云平台 ESMP      rcpv4             scp-adapter      stub          ECU
    │                │                  │              │            │
    │──OTA cmd──────►│                  │              │            │
    │  (含文件URL)    │                  │              │            │
    │             aul_new(url)          │              │            │
    │             aul_run() ─HTTP GET─► 文件服务器      │            │
    │             ←─── 文件数据 ─────────               │            │
    │             MD5/SHA256校验        │              │            │
    │                │──on_ota()───────►│              │            │
    │                │                  │─nanomsg pub──►            │
    │                │                  │ /stub/ota/dev │            │
    │                │                  │ (本地文件路径)│            │
    │                │                  │              │──TFTP/UDS─►│
    │                │                  │              │  升级包     │
    │                │                  │◄─nanomsg pub──            │
    │                │                  │ status=installing         │
    │                │                  │ progress=50               │
    │◄──OTA progress──│◄─on_ota_progress─│              │            │
    │                │                  │◄─nanomsg pub──            │
    │                │                  │ status=done,code=0        │
    │◄──OTA success───│◄─────────────────│              │            │
```

#### 工况数据上报时序

```
ECU/传感器      stub(socketcan)   scp-adapter(data线程)  rcpv4/MQTT    云平台CMS
    │                │                    │                  │              │
    │──CAN帧────────►│                    │                  │              │
    │                │──nanomsg pub──────►│                  │              │
    │                │  /stub/data/dev/   │                  │              │
    │                │  data/raw          │                  │              │
    │                │  (11225端口)        │                  │              │
    │                │                    │  DBC解析+过滤     │              │
    │                │                    │──rcpv4_adl_pub───►              │
    │                │                    │  (工况JSON/CBOR)  │              │
    │                │                    │                  │──MQTT pub───►│
    │                │                    │                  │  工况topic   │
    │                │                    │  （同时写本地文件）│              │
```

### E.5 已知问题与改进建议

#### 已知问题

1. **rcpv4-cmd-lock.c 第 92 行 copy-paste bug**：`if (json_delay)` 应为 `if (json_cts)`，导致锁机时间戳验证逻辑永远不执行。**建议立即修复**。

2. **stub-sany-crane/ota.c + stub-sany-ota/ota.c 第 770 行**：`strcpy` 到固定大小字段，存在缓冲区溢出。**建议改为 strncpy 或 strlcpy，并添加长度校验**。

3. **scp-raw-data-filter.c 多处 asprintf 返回值未检查**：OOM 时会触发空指针 crash。**建议统一添加返回值检查**。

4. **connection_ids HashSet 并发访问未加锁**：在高频工况下存在 data race。**建议对访问 connection_ids 的所有路径加 mutex**。

5. **nanomsg nn_bind 失败不返回错误**：程序静默继续运行，数据全部丢失。**建议 nn_bind/nn_connect 失败时返回错误并终止初始化**。

#### 改进建议

| 优先级 | 建议 |
|--------|------|
| 🔴 P0 | 修复 rcpv4-cmd-lock.c 第 92 行 `if (json_delay)` 错误 |
| 🔴 P0 | 修复 stub-ota.c strcpy 越界问题，改用 strlcpy + 长度校验 |
| 🟡 P1 | scp-raw-data-filter.c 中所有 `asprintf` 调用后添加 NULL 检查 |
| 🟡 P1 | connection_ids 所有访问路径加 mutex 保护 |
| 🟡 P1 | nn_bind/nn_connect 失败时做致命错误处理，避免静默运行 |
| 🟠 P2 | 将 scp-raw-data-filter.c 中的自旋 `pthread_mutex_trylock` 改为 `pthread_mutex_lock` |
| 🟠 P2 | 硬编码 UUID 应从配置文件或设备硬件（如 SN）派生，不能使用固定值 |
| 🟠 P2 | rcpv4.c 插件 dlsym 加载后需检查符号是否为 NULL，避免调用时段错误 |
| 🔵 P3 | 升级 OTA 文件签名从 MD5 到 SHA256（SDK 0.3.5.98+ 已支持，建议强制要求） |
| 🔵 P3 | strcat 拼接插件名改用 snprintf 并预先计算完整长度，避免溢出 |
| 🔵 P3 | 为 scp_adapter_t 中关键字段（ota_event_type等）使用原子操作或 mutex |

---

*本报告基于 repo-slim.zip（2026-04-14 打包）的源码分析，不含运行时行为验证。*  
*代码规模：177 C/H 文件，约 115,000 行（不含注释和空行约 80,000 行有效代码）。*
