# mpc-base 仓库深度分析报告

> 作者：James | 创建时间：2021 年起 | 分析时间：2026 年 2 月

---

## 一、仓库概述

`mpc-base` 是一套专为嵌入式 **NVR（Network Video Recorder，网络录像机）** 产品打造的 **C/C++ 中间件框架**，提供从硬件抽象、媒体处理、AI 智能识别、网络通信到低功耗电源管理的全套能力。它既是产品业务逻辑的基础设施，也是上层 UI 应用和外部客户端程序之间的统一接口层。

### 核心定位

```
┌────────────────────────────────────────────────┐
│         上层应用 (UI / App / 客户端)            │
├─────────────────────┬──────────────────────────┤
│  netclient（外部接口）│  modclient（内部接口）   │
├─────────────────────┴──────────────────────────┤
│              mpc-base 核心框架层                │
│  base / ipcm / dsp / ai / aov / nets / param   │
├────────────────────────────────────────────────┤
│           Hardware / OS / SDK 层               │
│   HAL / MCU / LED / PTZ / WiFi / 4G / BLE     │
└────────────────────────────────────────────────┘
```

---

## 二、支持平台

| 平台标识   | 说明                         | 构建脚本       |
|------------|------------------------------|----------------|
| `t32zmc`   | MIPS 架构嵌入式 SoC（无 4G） | build/t32zmc   |
| `t32zmc_4g`| MIPS 架构嵌入式 SoC（有 4G） | build/t32zmc_4g|
| `t41zm`    | Ingenic T41 平台              | build/t41zm    |
| `x86`      | x86 Linux（PC 调试用）        | build/x86      |

编译方式：`source x86 && make all`，输出目录为 `bin/$(ARCH)` 和 `lib/$(ARCH)`。

---

## 三、目录结构总览

```
mpc-base/
├── build/          # 各平台编译配置（交叉编译工具链、FLAGS）
├── bin/            # 编译产出的可执行文件
├── lib/            # 编译产出的静态/动态库
├── inc/            # 外部依赖头文件
├── doc/            # 开发文档（Development_Log.md、架构图）
│
├── mod/            # 核心业务模块（主体）
│   ├── base/       # 系统入口、总控 API
│   ├── ipcm/       # IP Camera 管理（通道管理核心）
│   ├── dsp/        # 媒体码流处理（解码/编码/帧管理）
│   ├── ai/         # AI 智能侦测（人形、烟火等）
│   ├── aov/        # AOV 任务调度（低功耗管理）
│   ├── nets/       # 私有协议网络服务端
│   ├── rpcapi/     # RPC 跨进程通信（Server + Client）
│   ├── param/      # 系统参数持久化管理
│   ├── event/      # 事件分发系统
│   ├── sys/        # 系统服务（看门狗、升级、时间）
│   ├── rtsps/      # RTSP 流媒体服务器
│   ├── preview/    # 实时预览
│   ├── playback/   # 录像回放
│   ├── 4g/         # 4G 模块管理
│   ├── lte/        # LTE 模块管理
│   ├── wlan/       # WiFi 模块管理
│   ├── wlan_client/# WiFi 客户端管理
│   ├── wired/      # 有线网络管理
│   ├── net_access/ # 网络接入状态管理
│   ├── net_wireless/# 无线网络心跳管理
│   ├── bluetooth/  # BLE 蓝牙管理
│   ├── mcu/        # 单片机通信管理
│   ├── nkit/       # 设备配对服务（UDP/Kit）
│   ├── discovery/  # 设备发现（局域网）
│   ├── hal/        # 硬件抽象层（MTD/加密/串口）
│   ├── ptz/        # PTZ 云台控制
│   ├── led/        # LED 状态灯控制
│   ├── button/     # 按键事件处理
│   ├── speaker/    # 喇叭/音频输出
│   ├── sound/      # 声音侦测
│   ├── ftp/        # FTP 定时抓图上传
│   ├── qrcode/     # 二维码生成/解析
│   └── upgrade/    # OTA 固件升级
│
├── modclient/      # 内部接口（同进程，无鉴权）
├── netclient/      # 外部接口（Socket，需用户鉴权）
│
└── util/           # 内部工具库
    ├── auth/       # Base64 + MD5 加密
    ├── cjson/      # JSON 解析（cJSON + s2j）
    ├── libnvrtools/# 基础工具（线程/锁/调试/网络/文件）
    │   ├── comm/   # 公共工具集合
    │   ├── file/   # 文件读写
    │   └── net/    # iKCP 可靠 UDP 协议
    ├── cyclone/    # 异步网络库（epoll/kqueue/select）
    ├── mem_trace/  # 内存泄漏追踪（含 MIPS backtrace）
    ├── cmd_srv/    # POSIX 命令执行服务
    └── uallsyms/   # 用户态符号表工具
```

---

## 四、核心架构设计

### 4.1 整体分层架构

```
┌──────────────────────────────────────────────────────────┐
│                     应用层接口                            │
│  ┌─────────────────────┐  ┌────────────────────────────┐ │
│  │    netclient         │  │       modclient            │ │
│  │（外部 Socket 接口）  │  │   （内部静态库接口）       │ │
│  │  需鉴权 / UI / APP   │  │   无鉴权 / 同进程调用      │ │
│  └─────────────────────┘  └────────────────────────────┘ │
├──────────────────────────────────────────────────────────┤
│                    业务逻辑层                              │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ │
│  │  base  │ │  ipcm  │ │ param  │ │ event  │ │  sys   │ │
│  └────────┘ └────────┘ └────────┘ └────────┘ └────────┘ │
├──────────────────────────────────────────────────────────┤
│                    媒体处理层                              │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐            │
│  │  dsp   │ │  ai    │ │ rtsps  │ │preview │            │
│  │码流处理│ │智能识别│ │RTSP流  │ │实时预览│            │
│  └────────┘ └────────┘ └────────┘ └────────┘            │
├──────────────────────────────────────────────────────────┤
│                  电源与任务调度层（AOV）                   │
│  ┌─────────────────────────────────────────────────────┐ │
│  │  nv_task（任务调度） + nv_battery（电源管理）        │ │
│  │  nv_heart_beat（心跳） + nv_timer（定时器）          │ │
│  └─────────────────────────────────────────────────────┘ │
├──────────────────────────────────────────────────────────┤
│                    网络通信层                              │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ │
│  │ nets │ │ 4g   │ │ wlan │ │ wired│ │  BLE │ │nkit  │ │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ │
├──────────────────────────────────────────────────────────┤
│                    硬件抽象层（HAL）                       │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐          │
│  │ hal  │ │ mcu  │ │ ptz  │ │ led  │ │button│          │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘          │
└──────────────────────────────────────────────────────────┘
```

---

## 五、核心模块深度解析

### 5.1 base 模块 —— 系统总入口

**文件**: `mod/base/`

base 是整个框架的启动入口和统一对外 API 聚合层。

**主要职责**:
- `nv_start_base()` / `nv_stop_base()` —— 系统启停总控
- 暴露设备信息接口（UUID、PID、AuthKey、固件版本）
- 提供通道级别的快捷控制接口（MIC、翻转、OSD、日夜模式、移动侦测等）
- 系统重启/关机/延迟重启封装

**设备配对模式**（`PPS_PAIR_TYPE`）:

```
LOCAL_CONNECT_MODE → 本地连接
QRCODE_MODE        → 二维码配网
AP_MODE            → AP 热点配网
BLE_MODE           → 蓝牙配网
```

---

### 5.2 ipcm 模块 —— IP 摄像机管理核心

**文件**: `mod/ipcm/`

这是整个框架最复杂、功能最丰富的核心模块，负责管理所有 IP Camera（IPC）通道的连接、配置、控制、能力集管理。

**内部架构（Factory 工厂模式）**:

```
nvipcm.cc（通道管理主控）
    │
    ├── nvfactory.cc（抽象工厂接口）
    │       │
    │       └── nvfactorydef.cc（具体工厂实现：Meari协议 / 本地协议）
    │
    └── nvlocal.cc（本地通道管理）
        nvipcmconfig.cc（通道配置持久化）
```

**通道配置结构** (`S_NV_IP_CONFIG`):

```c
typedef struct _NV_IP_CONFIG_ {
    char ip[64];        // IP 地址或 URL
    int  port;          // 端口
    int  protocol;      // 协议类型
    int  factory;       // 厂商类型（Meari / Onvif 等）
    char name[64];      // 用户名
    char pass[64];      // 密码
    char dev[64];       // 设备名称
    char serial[64];    // 设备序列号
    char dev_pid[64];   // 设备真实 PID
    char ty_pid[64];    // TUYA 绑定 PID
    char ty_devId[64];  // TUYA 绑定 UUID
} S_NV_IP_CONFIG;
```

**IPC 能力集位掩码系统**（每个 uint8_t 的 bit 位代表一项能力）:

| 字段               | 含义                         |
|--------------------|------------------------------|
| `video_encode`     | H264/H265 编码支持           |
| `audio_encode`     | AAC/PCM/G711U 音频格式       |
| `motion_detect`    | 移动侦测/区域侦测/PIR 支持   |
| `ptz`              | 云台 P/T/Z/预置点/追踪       |
| `audio_intercom`   | 单向/双向对讲                |
| `day_night`        | 日夜切换/白光/红外           |
| `person_detect`    | 人形侦测/画框/日夜过滤       |
| `sound_light`      | 声光警示/警笛/白灯           |
| `aov_fps`          | AOV 单帧间隔能力（1s~30s）   |
| `work_mode`        | 省电/性能/自定义低功耗模式   |

**异步消息队列设计**（`E_IPCM_MSG_TYPE_t`）:

所有对 IPC 的控制命令（约 44 种）均通过内部消息队列异步执行，避免阻塞主线程：

```
API 调用层
    │  nv_ipcm_set_xxx()
    ↓
消息队列入队（E_IPCM_MSG_XXX）
    │  IPCM_MSG_MAX_QU = 16
    ↓
ipcm 工作线程处理
    │
    ↓
下发到具体 IPC 设备（私有协议/Meari协议/ONVIF）
```

---

### 5.3 AOV 模块 —— 低功耗任务调度系统

**文件**: `mod/aov/`

AOV（Always-On Video）是专为**电池供电摄像机**设计的核心电源管理和任务调度系统，是整个框架最具特色的部分之一。

**任务状态机**:

```
                    nv_task_start()
                          │
                          ↓
                       [ idle ]
                          │
                    resume_type触发
                          ↓
                      [ ready ]
                          │
                    调度器执行一次
                          ↓
                    [ running(once) ]
                          │
                   running_cb() == true
                          ↓
                      [ running ]  ←── 持续运行
                          │
                    suspent_pre()
                          ↓
                      [ suspend ]
                          │
                      suspent()
                          ↓
                       [ idle ]  ──→ 系统休眠
```

**任务类型定义** (`TASK_NAME_E`):

| 任务               | 唤醒类型     | 说明                     |
|--------------------|--------------|--------------------------|
| `TASK_N_MAIN_VENC` | 总是唤醒     | 主码流实时编码           |
| `TASK_N_AENC`      | 总是唤醒     | 音频实时编码             |
| `TASK_N_DISK`      | 按需唤醒     | SD 卡录像存储            |
| `TASK_N_SPEAKER`   | 按需唤醒     | 喇叭播放                 |
| `TASK_N_ETH`       | 按需唤醒     | 以太网通信               |
| `TASK_N_4G`        | 按需唤醒     | 4G 模块通信              |
| `TASK_N_CPU`       | 随系统启动   | 系统级连续任务           |
| `TASK_N_WiFi`      | 按需唤醒     | WiFi 通信                |
| `TASK_N_BLE`       | 按需唤醒     | 蓝牙通信                 |

**触发器类型** (`TRIGIGGER_E`):

```
TRIG_NONE         → 无动作
TRIG_MORE_TIME    → 延长系统持续运行时间（处理码流/事件）
TRIG_REAL_TIME_ENC→ 触发视音频实时编码
TRIG_DISK         → 触发存储模块
TRIG_SPEAKER      → 触发喇叭模块
TRIG_NET          → 触发网络模块（4G/WiFi/HaLow 统一）
TRIG_MCU          → 触发单片机数据回复
TRIG_PTZ          → 触发 PTZ 追踪
TRIG_SOUND        → 强制使能警笛
TRIG_LIGHT        → 强制使能补光灯
TRIG_BLE          → 触发蓝牙模块
```

**电源管理设计** (`nv_battery`):

- 低功耗模式判断：`nv_battery_get_is_low_power()`
- 运行数据统计：运行时间、IOT 远程时间、报警次数、信号强度、电量
- 周期数据记录：以天为单位记录 7 天历史运行数据

**心跳系统** (`nv_heart_beat`):

- 唯一系统心跳任务（最大间隔 150 秒，可配置）
- 心跳超时则触发看门狗重启
- 用于判断系统健康状态和 IoT 平台保活

---

### 5.4 dsp 模块 —— 媒体码流处理

**文件**: `mod/dsp/`

**支持的帧类型** (`E_NV_FRAME_TYPE`):
- 视频：JPEG、H264-I、H264-P、H265-I、H265-P、YUV420SP
- 音频：PCM、G711A、G711U、AAC、OPUS

**核心接口**:

```c
nv_start_dsp(chan_max)            // 启动 DSP 模块（支持多通道）
nv_dsp_data_pro(header, buf, sz) // 处理一帧数据
nv_dsp_read_net_frame(...)        // 读取网络发送帧
nv_dsp_get_next_I_frame(...)      // 查找下一个 I 帧（用于流起始）
nv_dsp_require_send_net_frame(...)// 请求发送帧（抢占机制）
```

**码流类型定义**:

```
NV_STREAM_MAIN         = 1   // 主码流（混合 or AOV套装）
NV_STREAM_SUB          = 2   // 子码流
NV_STREAM_MAIN_REAL_TIME = 3 // 主码流实时流（套装）
NV_STREAM_IMG          = 4   // 图片流
NV_STREAM_MAIN_AOV     = 8   // 主码流AOV流（低功耗单帧）
NV_STREAM_EVENT        = 9   // 事件流
```

**网络缓冲区** (`nvnetbuf`): 为每个通道/码流维护循环缓冲区，支持多消费者（多客户端同时拉流）。

---

### 5.5 ai 模块 —— 智能识别

**文件**: `mod/ai/`

**能力**:
- 人形侦测（`hl_pps_nn_persondet`）
- 通用目标侦测（`hl_pps_nn_commondet`：人/车/宠物/包裹/烟火）
- 智能事件判断（`nvIntelligence`）

**生命周期**:

```c
nv_start_intelligence()    // 启动 AI 推理引擎
nv_intelligence_open(chan) // 开启指定通道智能
nv_intelligence_set_param(chan) // 更新参数（灵敏度等）
nv_intelligence_check_running() // 检查是否正在发生智能事件
nv_suspent_intelligence()  // 挂起（节省功耗）
nv_stop_intelligence()     // 停止
```

AI 支持编译时裁剪：定义 `NV_NO_AI` 宏后所有接口变为空函数，适配不含 NPU 的平台。

---

### 5.6 事件系统 —— event 模块

**文件**: `mod/event/`

事件系统是模块间解耦通信的核心，采用发布-订阅模式。

**主要事件类型** (`E_EVENT_MSG_TYPE_t`，共 40+ 种):

| 事件类型                          | 说明                   |
|-----------------------------------|------------------------|
| `E_EVENT_MSG_RECORD`              | 录像状态变化           |
| `E_EVENT_MSG_WIFI`                | WiFi 状态变化          |
| `E_EVENT_MSG_IPC_CONNECT`         | IPC 连接状态变化       |
| `E_EVENT_MSG_ALARM_MOTION_DECT`   | 移动侦测报警（含图）   |
| `E_EVENT_MSG_ALARM_NN_PERSON_DECT`| 人形侦测报警（含图）   |
| `E_EVENT_MSG_ALARM_NN_PET_DECT`   | 宠物侦测报警           |
| `E_EVENT_MSG_ALARM_NN_CAR_DECT`   | 车辆侦测报警           |
| `E_EVENT_MSG_ALARM_NN_FIRE_DECT`  | 火灾侦测报警           |
| `E_EVENT_MSG_HDCHG`               | 硬盘变动               |
| `E_EVENT_MSG_NVR_UPGRADE_PROGRESS`| NVR 升级进度           |
| `E_EVENT_MSG_MQTT_ONLINE_OFFLINE` | MQTT 平台上下线        |
| `E_EVENT_MSG_FTP_SNAP`            | FTP 定时抓图触发       |

**消息推送** (`nvmsgpush`): 将报警事件推送到 IoT 云平台（TUYA 等）。

**UI 消息** (`nvuimsg`): 将系统事件通知到本地显示界面。

---

### 5.7 rpcapi 模块 —— 跨进程 RPC 通信

**文件**: `mod/rpcapi/`

分为 Server 和 Client 两侧，实现进程间通信（IPC）。

**RPC Server 侧**（`rpcApiServer`）:
```c
nv_init_rpc_sever(chan_max, disk_max)  // 初始化，分配通道/磁盘资源
nv_start_rpc_sever()                   // 启动 RPC 服务
nv_stop_rpc_sever()                    // 停止
```

**RPC 核心接口**（`nvRpcManager.h` 暴露约 50+ 个跨进程调用）:
- 录像控制：StartRecord、StopRecord、StartMotionRecord、StartEventRecord
- 存储管理：GetDiskNum、FormatDisk、GetDiskInfoAll
- 录像搜索：searchRecord、getSearchResult
- 回放控制：playbackCreate、playbackPlay、playbackSeek
- 系统日志：nv_write_log

**RPC Client 侧**（`rpcApiClient`）:
- `nvbaserpc`：基础 RPC 通信底层
- `nvShareBuf`：基于 `memfd` 的共享内存大数据传输
- `nvnetbuf`：网络缓冲区管理
- `StorageRPC`：存储专用 RPC 接口封装

---

### 5.8 nets 模块 —— 私有协议网络服务

**文件**: `mod/nets/`

基于 cyclone 异步网络库实现的 TCP 服务器，处理客户端（App/UI）的私有协议连接。

```c
nv_start_netserver()   // 启动服务
nv_stop_netserver()    // 停止服务
```

内部通过 `nvnetnode`（连接节点管理）和 `nvnetop`（网络操作）处理多客户端并发连接。

---

### 5.9 param 模块 —— 配置参数管理

**文件**: `mod/param/`

统一管理 NVR 所有持久化配置，包含 10 个子模块：

| 子模块           | 说明                               |
|------------------|------------------------------------|
| `nvparam`        | 参数初始化/默认值/工厂恢复         |
| `nvchanparam`    | 通道参数（IPC 连接配置）           |
| `nvnetparam`     | 网络参数（IP/DNS/端口）            |
| `nvuser`         | 用户账户管理（最多 16 用户）       |
| `nvability`      | 设备能力集配置                     |
| `nvgbparam`      | 国标（GB28181）协议参数            |
| `nvptzparam`     | PTZ 预置点参数                     |
| `nvfixparam`     | 固定参数（型号、序列号等）         |
| `nvnormalset`    | 通用配置（语言/分辨率/时间格式等） |
| `nvsysreset`     | 系统恢复出厂设置                   |

---

### 5.10 sys 模块 —— 系统服务

**文件**: `mod/sys/`

| 组件              | 功能                                          |
|-------------------|-----------------------------------------------|
| `nvsystem`        | 系统初始化、重启、关机、版本管理              |
| `nvsystime`       | 系统时间管理、NTP 同步、时区配置              |
| `nvwatchdog`      | 软件看门狗（超时自动重启）                    |
| `nvcustomer`      | 客户定制化参数管理                            |
| `nvfactorymodel`  | 产测工厂模式                                  |

---

### 5.11 网络接入层

#### 4G/LTE 模块（`mod/4g/` 和 `mod/lte/`）

| 模块               | 功能                           |
|--------------------|--------------------------------|
| `nv_4g` / `nv_lte_ctl` | 4G/LTE 总控：拨号连接管理  |
| `nv_4g_at` / `nv_lte_at` | AT 命令收发（TTY 串口）  |
| `nv_4g_apn` / `nv_lte_apn` | APN 配置管理            |
| `nv_4g_upgrade` / `nv_lte_upgrade` | 模块固件升级     |
| `nv_lte_heartbeat` | LTE 心跳保活                   |

支持**双 SIM 卡**（`SIM_NUM = 2`）。

#### WiFi 模块（`mod/wlan/`）

- `nv_wifi`：WiFi 连接状态管理
- `nv_wifi_op`：WiFi 操作（扫描/连接/断开）
- `nvipchannel`：IPC WiFi 信道管理
- `pps_nkit_wlan`：NKit 配网协议实现
- `pps_wpa_cli`：wpa_supplicant 命令行接口
- `iwlib/iwpriv`：无线工具库

#### Bluetooth 模块（`mod/bluetooth/`）

- `nv_ble_msg`：BLE 消息协议处理
- `nv_bt_at`：蓝牙 AT 命令
- `nv_common_bt`：蓝牙通用控制

---

### 5.12 modclient 与 netclient —— 双接口设计

```
┌───────────────────────────────────────────────────────┐
│                    调用者视角                          │
│                                                       │
│  ┌──────────────────┐     ┌─────────────────────────┐ │
│  │    modclient      │     │       netclient         │ │
│  │                   │     │                         │ │
│  │  同进程内部调用   │     │  跨网络外部调用         │ │
│  │  静态库链接       │     │  TCP Socket 连接        │ │
│  │  无鉴权           │     │  用户名/密码鉴权        │ │
│  │  直接调用函数     │     │  私有二进制协议         │ │
│  │  (Unix Socket     │     │  支持 Windows/Linux     │ │
│  │   规划中)         │     │  客户端 SDK             │ │
│  └──────────────────┘     └─────────────────────────┘ │
│           │                         │                  │
│           └──────────┬──────────────┘                 │
│                      ↓                                 │
│               mpc-base 核心框架                        │
└───────────────────────────────────────────────────────┘
```

**netclient** 提供完整的 C SDK，支持跨平台（Windows `__declspec(dllexport)` / Linux），实现包括：
- 连接管理、用户鉴权
- 通道预览/回放
- 参数读写
- 事件接收（移动侦测、AI 报警等）
- 错误码体系（`E_SDK_RET_CODE`）

---

## 六、工具库详解（util/）

### 6.1 libnvrtools —— 基础工具集

| 工具          | 说明                                    |
|---------------|-----------------------------------------|
| `nvthreadhelp`| 线程创建/销毁封装（pthread）            |
| `nvmutexhelp` | 互斥锁/读写锁/条件变量封装              |
| `nvmsgqueue`  | 线程安全消息队列                        |
| `ThreadQueue` | C++ 模板消息队列                        |
| `nvdebughelp` | 分级日志系统（`NV_LOG_LEVEL`）          |
| `nvsysloghelp`| syslog 封装                             |
| `nvnethelp`   | 网络工具（IP 解析、端口扫描等）         |
| `nvparamhelp` | 参数持久化 R/W                          |
| `nvcrc32help` | CRC32 校验                              |
| `nvfile`      | 文件读写封装                            |
| `pps_thread`  | 轻量级线程封装                          |
| `pps_debug`   | 调试打印工具                            |
| `ikcp`        | iKCP 可靠 UDP 协议（低延迟传输）        |
| `pps_c911`    | 911 紧急呼叫支持                        |

### 6.2 cyclone —— 异步网络库

`cyclone` 是一个完整的跨平台异步 TCP 网络框架，支持三种 I/O 多路复用机制：

```
cyEvent（事件循环）
    ├── cye_looper_epoll.cpp    → Linux epoll
    ├── cye_looper_kqueue.cpp   → macOS/BSD kqueue
    └── cye_looper_select.cpp   → POSIX select（兼容所有平台）

cyNetwork（TCP 网络）
    ├── cyn_tcp_server          → 多线程 TCP 服务器
    │   ├── master_thread       → 接受连接（Accept）
    │   └── work_thread         → 处理读写（epoll）
    └── cyn_tcp_client          → TCP 客户端

cyCrypt（加密）
    ├── cyr_rijndael            → AES 加密
    ├── cyr_dhexchange          → Diffie-Hellman 密钥交换
    ├── cyr_adler32             → Adler32 校验
    └── cyr_xorshift128         → 随机数生成
```

### 6.3 mem_trace —— 内存调试

支持两种架构的调用栈回溯：
- `mips/backtrace.c`：MIPS 架构专用（解析 MIPS 指令集回溯栈帧）
- `other/backtrace.c`：其他架构（使用 `backtrace()` 标准接口）

功能：内存分配/释放跟踪，检测内存泄漏，输出分配时的调用堆栈。

### 6.4 cjson —— JSON 处理

- `cJSON`：轻量级 JSON 解析器（Dave Gamble 著）
- `s2j`：JSON ↔ 结构体自动映射宏库，简化序列化/反序列化

---

## 七、关键数据流分析

### 7.1 视频流处理流程

```
IPC 设备（网络摄像机）
    │ 私有协议/RTSP/iKCP
    ↓
ipcm 模块（通道连接管理）
    │ 接收码流数据帧
    ↓
dsp 模块（nvdsp_data_pro）
    │ 解析帧头（FsFrame_t）
    │ 写入环形缓冲区（nvnetbuf）
    ↓
┌──────────────────────────────┐
│      码流分发                 │
│  ┌──────────┐ ┌───────────┐  │
│  │  AI 模块  │ │ RTSP 服务 │  │
│  │ 人形/烟火 │ │ 推流给客户│  │
│  └──────────┘ └───────────┘  │
│  ┌──────────┐ ┌───────────┐  │
│  │ 预览推流  │ │ 录像存储  │  │
│  │（私有协议）│ │（SD/HDD） │  │
│  └──────────┘ └───────────┘  │
└──────────────────────────────┘
```

### 7.2 AI 报警事件处理流程

```
dsp 模块输出 YUV420SP 帧
    │
    ↓
ai 模块（nvIntelligence）
    │ 神经网络推理（hl_pps_nn_persondet 等）
    │ 检测到人/车/宠物/烟火
    ↓
触发 AOV TRIG_REAL_TIME_ENC（延长系统运行时间）
    │
    ↓
event 系统（E_EVENT_MSG_ALARM_NN_XXX）
    │
    ├── nvmsgpush → IoT 云平台推送报警（含图片）
    ├── nvuimsg   → 本地 UI 显示报警
    ├── ipcm      → 触发 IPC 端声光警示
    └── dsp       → 触发录像（StartEventRecord）
```

### 7.3 低功耗唤醒流程（AOV）

```
系统处于休眠（AOV idle 状态）
    │
    ├── PIR 触发（人体红外传感器）
    │       ↓
    │   MCU 唤醒主 SoC
    │
    ├── 定时器到期（nv_timer）
    │       ↓
    │   系统 RTC 唤醒
    │
    └── 4G 网络消息
            ↓
        TRIG_NET 触发
                
        所有情况统一进入：
            ↓
        nv_task 调度器执行 resume 流程
            │
            ├── venc 任务启动 → 编码 1 帧 → AI 推理
            ├── 若有事件 → TRIG_MORE_TIME → 延长运行
            └── 无事件 → 完成任务 → suspend → 系统休眠
```

### 7.4 配对/连接流程

```
新设备首次联网
    │
    ├── QR Code 扫描
    │       ↓
    │   nkit 模块（nv_kit_server）
    │   UDP 广播扫描 + 配网信息下发
    │
    ├── BLE 配网
    │       ↓
    │   bluetooth 模块
    │   BLE AT 命令传递 WiFi 凭证
    │
    └── AP 热点模式
            ↓
        wlan 模块（pps_nkit_wlan）
        HTTP 接口传递 WiFi 凭证
        
        统一完成后：
            ↓
        discovery（pps_nkit_discovery）
        局域网设备发现广播
            ↓
        nets 建立私有协议 TCP 连接
            ↓
        ipcm 注册通道，开始取流
```

---

## 八、编译构建体系

### Makefile 编译目标

```makefile
make fw      → 仅编译工具库（util/）
make client  → 编译工具库 + netclient
make all     → 编译全部（util + netclient + mod + modclient）
make clean   → 清理所有构建产物
```

### 平台环境变量

```bash
source build/x86       # 设置 x86 开发环境
source build/t41zm     # 设置 Ingenic T41 交叉编译环境
# 设置后环境变量：
# NV_CPU_ARCH=x86/t41zm/t32zmc
# NV_DEV_TYPE=平台类型
```

### 编译宏控制

| 宏定义            | 作用                         |
|-------------------|------------------------------|
| `NV_NO_MEDIA`     | 禁用媒体处理（精简编译）     |
| `NV_NO_AI`        | 禁用 AI 模块                 |
| `IPC_SYS_BACKUP`  | 启用备份编译模式             |

---

## 九、接口设计规范

### 9.1 C/C++ 混合接口风格

所有模块均采用 `C` 接口导出（`extern "C"`），内部以 C++ 实现：

```c
#ifdef __cplusplus
extern "C" {
#endif
// 接口声明
extern int nv_xxx_function(params...);
#ifdef __cplusplus
}
#endif
```

这保证了：
- C++ 内部可使用 STL、RAII、多态等特性
- 对外 ABI 与纯 C 代码完全兼容
- 可编译为静态库供 C 代码链接

### 9.2 返回值规范

```c
#define NV_SUCCESS        (0)    // 成功
#define NV_FAILURE        (-1)   // 通用失败
#define NV_RETRY          (-2)   // 需重试（回放过快）
#define NV_NO_DEVICE      (-3)   // 无设备
#define NV_EXCEED_LIMITED (-4)   // 超出上限
#define NV_NO_SPACE       (-5)   // 空间不足
```

### 9.3 通道索引规范

- 最大通道数：`NV_CHAN_MAX = 4`（静态上限，实际通道数运行时决定）
- 通道索引：`[0, chan_max)`
- 码流类型：主码流（1）、子码流（2）

---

## 十、安全与加密

| 组件                        | 算法/技术               |
|-----------------------------|-------------------------|
| `util/auth/nv_md5`          | MD5 用户密码哈希        |
| `util/auth/nv_base64`       | Base64 编码（配置传输） |
| `mod/hal/pps_xxtea`         | XXTEA 加密（配置文件）  |
| `mod/hal/pps_device_encryption` | 设备级加密         |
| `cyclone/cyCrypt/rijndael`  | AES-128/256 加密        |
| `cyclone/cyCrypt/dhexchange`| DH 密钥交换（网络层）   |
| `mod/qrcode`                | 二维码（含 miniz 压缩） |

---

## 十一、典型使用场景

### 场景一：4 通道 NVR 系统初始化

```c
// 1. 初始化参数
nv_init_param();

// 2. 启动系统基础服务
nv_start_base();

// 3. 启动 IP 摄像机管理（4 通道）
nv_start_ipcm(4);

// 4. 启动媒体处理
nv_start_dsp(4);

// 5. 启动 AI 智能
nv_start_intelligence();

// 6. 启动网络服务（UI/客户端连接）
nv_start_netserver();

// 7. 启动 RPC 服务（存储等跨进程）
nv_init_rpc_sever(4, 2);
nv_start_rpc_sever();

// 8. 添加通道配置
S_NV_IP_CONFIG cfg = {"192.168.1.100", 9527, ...};
nv_ipcm_set_ipconfig(0, cfg);
```

### 场景二：电池相机 AOV 工作模式

```c
// 注册 AI 智能任务
nv_task_register("ai_task",
    TASK_N_MAIN_VENC,   // 总是随编码唤醒
    TRIG_REAL_TIME_ENC, // 检测到目标后延长编码
    ai_check_running,   // 检测条件：AI 正在运行
    ai_loop_callback,   // 任务主体
    0, 5, NULL);

// 注册存储任务
nv_task_register("disk_task",
    TASK_N_DISK,        // 按需唤醒
    TRIG_MORE_TIME,     // 缓冲区有数据延长
    disk_check_running, // 检测条件：缓冲区非空
    disk_loop_callback,
    0, 3, NULL);

// 启动任务调度
nv_task_start(1);
```

---

## 十二、总结

### 技术亮点

1. **完整的低功耗管理体系**：AOV 模块将任务调度与电源管理深度融合，是电池相机产品的核心竞争力。

2. **工厂模式的协议抽象**：ipcm 通过 nvfactory 支持多种 IPC 协议（Meari 私有协议、ONVIF 标准协议），扩展新协议只需添加新工厂实现。

3. **双接口客户端设计**：modclient（内部无鉴权）+ netclient（外部有鉴权）的组合，兼顾了开发便利性和产品安全性。

4. **编译时能力裁剪**：通过宏定义（`NV_NO_MEDIA`、`NV_NO_AI`）支持按需裁剪，适应不同硬件配置和产品形态。

5. **异步事件驱动架构**：ipcm 控制命令队列 + event 发布订阅 + cyclone 异步网络，整体架构非阻塞，适合实时嵌入式系统。

### 局限与规划方向（源码注释中已记录）

- `modclient` 目前仅支持同进程静态库，**规划支持 Unix Socket 跨进程调用**
- 存储任务 A/B 双缓冲同步机制还在完善中
- pRTP2（私有 RTP 协议2）依赖以太网，首次获取 IP 需要 5~10 秒

---

*本报告基于 mpc-base 仓库源码全量分析，所有结论均来自源码事实。*
