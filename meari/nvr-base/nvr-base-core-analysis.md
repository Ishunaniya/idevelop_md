# nvr-base-core 源码深度分析报告

> **作者**：基于源码全量阅读自动生成  
> **仓库**：`nvr-base-core/nvr-base/`  
> **语言**：C / C++（混用，接口层大量使用 `extern "C"`）  
> **编译目标**：`libnvrbase.a`（静态库）+ `modclient`（调试工具可执行程序）

---

## 目录

1. [项目定位与整体概述](#1-项目定位与整体概述)
2. [目录结构总览](#2-目录结构总览)
3. [整体架构分析](#3-整体架构分析)
4. [系统启动流程](#4-系统启动流程)
5. [核心模块逐一解析](#5-核心模块逐一解析)
   - 5.1 base — 入口与总控
   - 5.2 ipcm — IP摄像机管理器
   - 5.3 dsp — 数字媒体处理
   - 5.4 ai — 人工智能推理
   - 5.5 event — 事件总线
   - 5.6 storage — 存储子系统
   - 5.7 rpcapi — 进程间 RPC 通信
   - 5.8 nets — 网络服务器
   - 5.9 param — 参数/配置管理
   - 5.10 sys — 系统管理
   - 5.11 hal — 硬件抽象层
   - 5.12 discovery — 设备发现
   - 5.13 rtsps — RTSP 推流服务
   - 5.14 speaker — 音频输出
   - 5.15 preview / playback / ui — 预览/回放/界面
   - 5.16 wlan / wired — 网络连接管理
   - 5.17 led / button / upgrade — 外设与升级
6. [工具库（util）解析](#6-工具库util解析)
7. [netclient — 网络客户端 SDK](#7-netclient--网络客户端-sdk)
8. [modclient — 调试命令行工具](#8-modclient--调试命令行工具)
9. [关键数据结构](#9-关键数据结构)
10. [外部依赖分析](#10-外部依赖分析)
11. [构建系统分析](#11-构建系统分析)
12. [模块依赖关系图](#12-模块依赖关系图)
13. [数据流图：视频流从IPC到存储/显示](#13-数据流图视频流从ipc到存储显示)
14. [数据流图：报警事件处理流程](#14-数据流图报警事件处理流程)
15. [设计模式与架构特点总结](#15-设计模式与架构特点总结)

---

## 1. 项目定位与整体概述

`nvr-base-core` 是一款**网络硬盘录像机（NVR）的核心中间件库**。它运行在 Linux 嵌入式平台上，向上为 UI 进程（QT界面）提供 RPC 服务，向下负责连接和管理多路 IP 摄像机（IPC）。

**产品背景**：
- 设备类型：NVR（Network Video Recorder，网络硬盘录像机）
- 平台：Linux 嵌入式（支持 ARM/MIPS/x86 多平台编译）
- 云平台：涂鸦（Tuya）智能平台（见 `nvsystem.h` 中的 UUID/AUTHKEY）
- 最大通道数：支持最多 32 路 IPC（代码中 `STOR_CHAN_NUMS_SUPPORT = 32`）
- 显示窗口：最大支持 36 路分屏预览（`WIN_NUMS_36 = 36`）

**核心职责**：

| 职责 | 说明 |
|------|------|
| IPC 接入 | 管理多路 IP 摄像机连接，支持 5 种接入协议 |
| 视频分发 | 接收 IPC 视频流，分发给存储、预览、AI、RTSP服务 |
| 录像存储 | 基于自研 STFS 文件系统写录像/图片/日志到硬盘 |
| 智能分析 | 对视频帧执行 NN 推理（人形/宠物/车辆/包裹检测） |
| 事件告警 | 统一的事件总线，驱动推送、录像、界面刷新 |
| 进程通信 | RPC Server 向 UI 进程提供查询和控制接口 |
| 网络服务 | RTSP 流媒体、设备发现、网络配置 |

---

## 2. 目录结构总览

```
nvr-base-core/
└── nvr-base/
    ├── Makefile                  # 顶层构建入口
    ├── mod/                      # 核心功能模块（编译为 libnvrbase.a）
    │   ├── Makefile
    │   ├── base/                 # ★ 总入口模块
    │   │   ├── inc/nv_api.h      # 对外 API 汇总头文件
    │   │   ├── inc/nv_base.h     # 主入口：nv_start_base / nv_stop_base
    │   │   └── src/nv_base.cc    # 启动序列实现
    │   │       nv_api_action.cc  # 各通道控制 API
    │   │       nv_api_param.cc   # 参数读写 API
    │   ├── ipcm/                 # ★ IP摄像机管理器
    │   │   ├── inc/nvipcm.h
    │   │   └── src/
    │   │       ├── nvipcm.cc     # 核心调度（4503行）
    │   │       ├── nvipcmtype.h  # 通道状态机定义
    │   │       ├── nvfactory.h/cc# 工厂模式协议分发
    │   │       ├── nvmeari.cc    # Meari 协议实现
    │   │       ├── nvonvif.cc    # ONVIF 协议实现
    │   │       ├── nvrtsp.cc     # RTSP 拉流实现
    │   │       ├── nvmnetsdk.cc  # NETSDK 协议实现
    │   │       ├── nvptrp2.cc    # PRTP v2 协议实现
    │   │       └── nvbattery.cc  # 电池摄像机管理
    │   ├── dsp/                  # 数字媒体处理（帧缓冲）
    │   ├── ai/                   # AI 智能检测
    │   ├── event/                # 事件总线
    │   ├── storage/              # 存储子系统（自研STFS文件系统）
    │   │   ├── inc/              # 接口头文件
    │   │   └── src/
    │   │       ├── nvDataDevice/ # 磁盘设备抽象（DirectIO/FileSystem/SD）
    │   │       ├── nvDataFileSystem/ # STFS文件系统（录像/图片/日志/OTA）
    │   │       └── nvDataUtil/   # 线程/消息/时间工具
    │   ├── rpcapi/               # RPC 进程间通信
    │   │   ├── rpcApiServer/     # RPC服务端（运行在本进程）
    │   │   └── rpcApiClient/     # RPC客户端库（供UI进程链接）
    │   ├── nets/                 # 网络服务器（远程接入）
    │   ├── param/                # 配置参数持久化
    │   ├── sys/                  # 系统级管理（watchdog/时间/升级/工厂）
    │   ├── hal/                  # 硬件抽象（MTD/串口IO/加密）
    │   ├── discovery/            # 局域网设备发现
    │   ├── rtsps/                # RTSP 服务端（对外推流）
    │   ├── speaker/              # 音频输出/对讲
    │   ├── preview/              # 视频预览分发
    │   ├── playback/             # 录像回放
    │   ├── ui/                   # UI 状态管理（屏幕/轮巡/MP4回放）
    │   ├── wlan/                 # Wi-Fi 管理
    │   ├── wired/                # 有线网管理
    │   ├── led/                  # LED 指示灯
    │   ├── button/               # 实体按键
    │   └── upgrade/              # OTA 固件升级
    ├── util/                     # 独立工具库
    │   ├── auth/                 # MD5 / Base64
    │   ├── cjson/                # cJSON 解析器
    │   ├── cstorage/             # C语言存储工具
    │   ├── mem_trace/            # 内存泄漏追踪
    │   ├── cyclone/              # 事件循环框架（第三方）
    │   ├── libnvrtools/          # NVR 通用工具
    │   ├── uallsyms/             # 符号表工具
    │   ├── simple_xml/           # 简易XML解析
    │   └── cmd_srv/              # 命令服务（非x86）
    ├── netclient/                # 网络客户端 SDK（跨平台，含Windows）
    │   ├── inc/netclient.h       # 对外统一接口
    │   └── src/netclient.cc
    └── modclient/                # 调试命令行工具（可执行程序）
        └── src/modclient.cc
```

---

## 3. 整体架构分析

### 3.1 宏观架构层次图

```
┌─────────────────────────────────────────────────────┐
│                  UI 进程（QT界面）                    │
│          使用 rpcApiClient 库调用 NVR 功能             │
└────────────────────────┬────────────────────────────┘
          RPC 共享内存/本地Socket通信
┌────────────────────────▼────────────────────────────┐
│              libnvrbase.a（本仓库）                   │
│                                                     │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────────┐  │
│  │ base │ │ipcm  │ │ dsp  │ │  ai  │ │ storage  │  │
│  │(总控)│ │(IPC管│ │(媒体 │ │(NN推 │ │(STFS录像 │  │
│  │      │ │理器) │ │缓冲) │ │理)   │ │/磁盘管理)│  │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────────┘  │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────────┐  │
│  │event │ │rpcapi│ │ nets │ │rtsps │ │ param    │  │
│  │(事件 │ │(RPC服│ │(网络 │ │(RTSP │ │(配置持久 │  │
│  │总线) │ │务器) │ │服务) │ │推流) │ │化)       │  │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────────┘  │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐               │
│  │ sys  │ │ hal  │ │wlan/ │ │preview│              │
│  │(系统)│ │(硬件)│ │wired │ │/playback│            │
│  └──────┘ └──────┘ └──────┘ └──────┘               │
└─────────────────────────────────────────────────────┘
          ↓ 接入协议（Meari/ONVIF/RTSP/NETSDK/PRTP）
┌─────────────────────────────────────────────────────┐
│          IP 摄像机集群（IPC，最多32路）               │
│  [IPC 0][IPC 1][IPC 2]...[IPC 31]                   │
│  Wi-Fi / POE / 有线 / 4G                             │
└─────────────────────────────────────────────────────┘
          ↓ 磁盘存储（SATA/USB/SD卡）
┌─────────────────────────────────────────────────────┐
│         存储设备（自研STFS格式分区）                   │
│  [OTA分区 768MB][录像分区（剩余全部空间）]              │
└─────────────────────────────────────────────────────┘
```

### 3.2 关键设计决策

| 设计决策 | 说明 |
|---------|------|
| **单进程多线程** | `libnvrbase.a` 在主进程中运行，每个 IPC 通道有独立连接线程 |
| **工厂模式解耦协议** | IPC 接入协议抽象为 `E_NV_FACTORY`，通过回调函数表实现多态 |
| **事件总线解耦模块** | `nv_event_trigger()` 发送消息，各模块无需直接调用，低耦合 |
| **自研文件系统** | STFS（Streaming Time FileSystem）直接 Direct I/O 写硬盘，不依赖 Linux FS |
| **RPC 分离 UI** | UI 进程通过 `rpcApiClient` 与主进程通信，进程隔离，UI 崩溃不影响录像 |
| **C/C++ 混用** | 底层存储/HAL 用 C，上层业务逻辑用 C++，接口统一用 `extern "C"` |

---

## 4. 系统启动流程

`nv_start_base()` 是整个系统的入口函数，按严格顺序初始化所有子系统：

```
nv_start_base()
│
├─ [1] nv_init_signals()          # 注册信号处理（SIGINT/SIGTERM/SIGUSR1等）
├─ [2] nv_start_watchdog()        # 启动看门狗，防止进程死锁
├─ [3] nv_init_hardbord()         # 初始化硬件板型识别（工厂模式/客户型号）
├─ [4] nv_init_param()            # 加载所有配置参数（从Flash/MTD读取）
├─ [5] nv_init_timezone()         # 设置系统时区
├─ [6] nv_init_media()            # 初始化媒体层（平台SDK）
│
├─ [7] nv_init_ai(chan_max)        # 初始化 AI 检测引擎（加载 NN 模型）
├─ [8] nv_init_event(chan_max)     # 初始化事件总线（分配消息队列）
├─ [9] nv_init_ai_event(chan_max)  # 初始化 AI 事件处理器
├─ [10] nv_init_userm(chan_max)    # 初始化用户管理模块
│
├─ [11] nv_start_dsp(chan_max)     # 启动 DSP 媒体缓冲区（为每路通道分配帧缓冲）
├─ [12] nv_start_ipcm(chan_max)    # ★ 启动 IPC 管理器（开始连接所有摄像机）
│
├─ [13] nv_init_rpc_sever()       # 初始化 RPC 服务端资源
├─ [14] nv_start_data_server()    # 启动存储服务（挂载磁盘、扫描文件系统）
├─ [15] nv_start_rpc_sever()      # RPC 服务端开始监听 UI 请求
│
├─ [16] nv_start_preview(chan_max) # 启动预览分发（视频画面路由）
├─ [17] nv_start_rtspserver()     # 启动 RTSP 服务器（供第三方接入）
│
├─ [18] nv_start_intelligence()   # 启动 AI 推理主循环
├─ [19] nv_start_netserver()      # 启动网络服务器（NETSDK/远程接入）
├─ [20] nv_start_preview_patrol() # 启动预览轮巡定时器
├─ [21] nv_start_backup()         # 启动备份服务（U盘剪辑）
└─ [22] nv_start_disocvery_sever()# 启动局域网设备发现服务
```

**启动原则**：严格按依赖顺序，任何一步失败则 `exit(EXIT_FAILURE)`，看门狗在超时后会重启进程。

---

## 5. 核心模块逐一解析

### 5.1 base — 入口与总控

**文件**：`mod/base/src/nv_base.cc`、`nv_api_action.cc`、`nv_api_param.cc`

**职责**：
- 提供系统级 API（`nv_start_base/nv_stop_base/nv_sys_reboot`）
- 封装所有通道控制接口（翻转、麦克风、日夜模式、移动侦测、电池相机等）
- 每个控制接口遵循 **"先保存配置 → 再下发给IPC"** 的双写原则

**典型 API 调用模式**：
```c
int nv_set_chan_daynight_mode(int chan, int mode) {
    nv_set_daynight_mode(chan, mode);      // 1. 持久化到 param 模块
    nv_ipcm_set_daynight_mode(chan, mode); // 2. 通过 ipcm 发给摄像机
}
```

**通道范围检查宏**：
```c
NV_CHECK(chan < 0 || chan > nv_abi_getchannum() - 1, return NV_FAILURE);
```
所有接口都有边界检查，防止越界。

---

### 5.2 ipcm — IP摄像机管理器

**文件**：`mod/ipcm/src/nvipcm.cc`（4503行，最核心文件）

这是整个系统中最复杂的模块，负责管理所有 IP 摄像机的连接生命周期。

#### 5.2.1 支持的 5 种接入协议

```c
typedef enum _E_NV_FACTORY_ {
    NV_FACTORY_MEARI  = 1,  // 私有 Meari 协议（主推协议）
    NV_FACTORY_ONVIF  = 2,  // 标准 ONVIF 协议
    NV_FACTORY_MEARI2 = 3,  // PRTP v2（Meari改进版）
    NV_FACTORY_RTSP   = 4,  // 标准 RTSP 协议
    NV_FACTORY_NETSDK = 5,  // NETSDK 协议（接入客户手机账号下的相机）
} E_NV_FACTORY;
```

#### 5.2.2 工厂模式协议分发

`nvfactory.h/cc` 实现了一个完整的 **策略/工厂模式**：

```
nv_factory_set_dev_flip(handle, factory, status)
         │
         ├─ if factory == MEARI  → nv_meari_set_flip(...)
         ├─ if factory == ONVIF  → nv_onvif_set_flip(...)
         ├─ if factory == PRTP2  → nv_ptrp2_set_flip(...)
         ├─ if factory == RTSP   → (不支持，返回失败)
         └─ if factory == NETSDK → nv_mnetsdk_set_flip(...)
```

每种操作都定义了对应的回调函数类型（如 `set_dev_flip_callback`），实现了 **面向接口编程**。

#### 5.2.3 通道状态机

每个 IPC 通道有独立状态机：

```
E_IPCM_NONE
    │ 开始连接
    ▼
E_IPCM_CONNECT  ←──────────┐
    │ 网络断开/密码错误      │ 重连成功
    ▼                       │
E_IPCM_ERR_CONN/ERR_RECV   │
E_IPCM_ERR_IP/ERR_PASS ────┘

── 电池相机扩展状态 ──
E_IPCM_CONNECT_SLEEP   (连接且休眠，低功耗模式)
E_IPCM_CONNECT_AWAKE   (连接且唤醒，正在工作)
E_IPCM_DISCONNECT      (下线)
```

状态变更时，通过 `nv_event_trigger(E_EVENT_MSG_IPC_CONNECT, ...)` 通知 UI 和其他模块。

#### 5.2.4 线程模型

```c
// 每个通道启动独立连接线程
static int nv_ipcm_connectall() {
    for (int chan = 0; chan < max; chan++) {
        nv_start_thread(nv_ipcm_pro_connect, 50, value);
    }
}
```

#### 5.2.5 设备能力集（Capability）

`S_NV_DEVICEINFO` 结构体记录了每台 IPC 的硬件能力，以 `uint8_t` 位图方式存储：

| 能力字段 | 含义 |
|---------|------|
| `video_encode_cap` | 支持的视频编码（H264/H265） |
| `ptz_cap` | 是否支持云台 |
| `person_detect_cap` | 是否支持人形检测 |
| `aov_fps_cap` | AOV低功耗帧率配置（bit位表示1/2/3/5/10/15/20/30/60秒） |
| `work_mode_cap` | 工作模式：省电/性能/自定义 |
| `bell_cap` | 是否支持门铃 |
| `sound_light_cap` | 是否支持声光报警 |
| `ipc_ai_detect_cap` | 智能检测开关（bit位：人形/宠物/车辆/包裹/布防时间） |

---

### 5.3 dsp — 数字媒体处理

**文件**：`mod/dsp/src/nvdsp.cc`、`nvdecode.cc`、`nvsnapshot.cc`

**职责**：

1. **帧缓冲管理**：为每路通道维护循环帧缓冲区（`NetFrameIndex`），供预览/存储/AI 消费
2. **帧接收**：`nv_dsp_data_pro(FsFrame_t* pframe_header, char* buffer, int size)` 接收来自 ipcm 的编码帧
3. **帧分发**：`nv_dsp_read_net_frame()` 供消费者（预览/录像/AI）读取帧
4. **解码器管理**：`nvdecode.cc` 管理硬件解码器资源（用于本地预览和 AI）
5. **截图**：`nvsnapshot.cc` 提供 JPEG 截图功能

**支持的帧类型**：

```c
typedef enum _E_NV_FRAME_TYPE_ {
    E_VIDEO_FRAME_JPEG,
    E_VIDEO_FRAME_H264_I, E_VIDEO_FRAME_H264_P,
    E_VIDEO_FRAME_H265_I, E_VIDEO_FRAME_H265_P,
    E_VIDEO_FRAME_YUV420SP,
    E_AUDIO_FRAME_PCM, E_AUDIO_FRAME_G711A,
    E_AUDIO_FRAME_G711U, E_AUDIO_FRAME_AAC,
    E_AUDIO_FRAME_OPUS,
} E_NV_FRAME_TYPE;
```

---

### 5.4 ai — 人工智能推理

**文件**：`mod/ai/src/nvIntelligence.cc`、`hl_pps_nn_persondet.c`、`hl_pps_nn_commondet.cc`

**职责**：基于 NN 模型对视频帧进行目标检测。

**支持的检测类型**：

| 检测类型 | 触发事件 |
|---------|---------|
| 人形检测（Person Detection） | `E_EVENT_MSG_ALARM_NN_PERSON_DECT` |
| 宠物检测（Pet Detection） | `E_EVENT_MSG_ALARM_NN_PET_DECT` |
| 车辆检测（Car Detection） | `E_EVENT_MSG_ALARM_NN_CAR_DECT` |
| 包裹检测（Package Detection） | `E_EVENT_MSG_ALARM_NN_PACKAGE_DECT` |
| 通用目标检测（Common Det） | `hl_pps_nn_commondet.cc` |

**检测结果数据结构**：
```c
typedef struct nv_status_box_s {
    float cx, cy;    // 目标中心坐标（归一化）
    float w, h;      // 目标宽高（归一化）
    float score;     // 置信度 0.0~1.0
    int label;       // 目标类别 ID
    int status;      // 目标框状态
} nv_status_box_t;

#define NV_MAX_BOX_NUM (20)  // 单帧最多检测20个目标
```

**推理流程**：
```
每路通道 NV_AIer
    │
    ├─ 从 preview 模块获取解码后的 YUV 帧
    ├─ 送入 NN 推理引擎（平台 SDK: pps_media_api）
    ├─ 获取检测结果 nv_objs_t
    └─ 触发 nv_aievent → nv_event_trigger() → 存储/推送
```

---

### 5.5 event — 事件总线

**文件**：`mod/event/src/nvevent.cc`、`nvaievent.cc`、`nvmsgpush.cc`、`nvnetmsg.cc`、`nvuimsg.cc`、`nvsoundlight.cc`

**职责**：系统内部的发布-订阅事件总线，实现模块解耦。

**事件类型（部分）**：

| 枚举值 | 含义 |
|--------|------|
| `E_EVENT_MSG_RECORD` | 录像状态更新 |
| `E_EVENT_MSG_WIFI` | WiFi 信号强度变化 |
| `E_EVENT_MSG_IPC_CONNECT` | IPC 连接状态变化 |
| `E_EVENT_MSG_PLAYBACK` | 回放进度更新 |
| `E_EVENT_MSG_HDCHG` | 硬盘插拔变动 |
| `E_EVENT_MSG_ALARM_MOTION_DECT` | 移动侦测报警（带图） |
| `E_EVENT_MSG_ALARM_NN_PERSON_DECT` | 人形侦测报警 |
| `E_EVENT_MSG_ALARM_NN_PET_DECT` | 宠物侦测报警 |
| `E_EVENT_MSG_ALARM_NN_CAR_DECT` | 车辆侦测报警 |
| `E_EVENT_MSG_ALARM_NN_PACKAGE_DECT` | 包裹侦测报警 |
| `E_EVENT_MSG_NVR_UPGRADE_PROGRESS` | NVR OTA 升级进度 |
| `E_EVENT_MSG_IPC_UPGRADE_PROGRESS` | IPC OTA 升级进度 |
| `E_EVENT_MSG_HD_FORMAT_PROGRESS` | 硬盘格式化进度 |
| `E_EVENT_MSG_MQTT_ONLINE_OFFLINE` | 涂鸦云 MQTT 上下线 |

**事件触发**：`nv_event_trigger(type, chan, stream, a, b, data)` 是核心函数，参数 a/b 携带附加信息，data 携带图片等二进制数据。

---

### 5.6 storage — 存储子系统

这是代码量最大的子系统之一，实现了完整的 **自研文件系统 STFS（Streaming Time FileSystem）**。

#### 5.6.1 STFS 磁盘布局

```
硬盘物理分区布局（LBA 地址）：
┌──────────────────────────────────────────────────┐
│ MBR (LBA 0)           - 512字节，标准主引导记录  │
├──────────────────────────────────────────────────┤
│ STFS_TABLE (LBA 2000) - 文件系统主目录表         │
├──────────────────────────────────────────────────┤
│ STFS_TABLE_BAK (LBA 4000) - 目录表备份           │
├──────────────────────────────────────────────────┤
│ OTA 分区 (LBA 6144, 768MB)                       │
│   - NVR OTA 文件（最大64MB）                      │
│   - IPC OTA 文件（最大32MB/台）                   │
├──────────────────────────────────────────────────┤
│ 录像分区 (LBA 2097152, 剩余全部空间)              │
│   - 录像文件（循环覆盖写）                        │
│   - 图片文件（事件截图）                          │
│   - 日志文件                                      │
│   - 录像索引（4096*2 字节/通道）                  │
└──────────────────────────────────────────────────┘
```

#### 5.6.2 存储子系统层次结构

```
nvDataService（顶层服务入口）
    │
    ├─ nvDiskManager（磁盘管理）
    │       └─ 支持 SATA/eSATA/USB/SDCard 四种接口
    │
    ├─ nvDataVideoRecord（视频录像服务）
    ├─ nvDataImageRecord（图片存储服务）
    ├─ nvDataLogSave（系统日志服务）
    ├─ nvDataPlayback（录像回放服务）
    ├─ nvDataBackup（U盘备份服务）
    ├─ nvDataOtaFile（OTA文件管理）
    └─ nvDataParam（参数存储服务）

存储设备抽象层：
    ├─ nvDirectIODev（Direct I/O，绕过页缓存直写）
    ├─ nvFileSystemDev（文件系统方式，用于FAT32/NTFS/exFAT备份盘）
    └─ nvSDFile（SD卡特殊处理）

STFS 文件系统层：
    ├─ nvRecordFile（录像文件，核心）
    ├─ nvImageFile（图片文件）
    ├─ nvEventFile（事件文件）
    ├─ nvLogFile（日志文件）
    └─ nvOtaFile（OTA升级文件）
```

#### 5.6.3 录像接口

```c
// RPC Manager 暴露的录像接口
int32_t StartRecord(int32_t channel, uint32_t stream);     // 手动录像
int32_t StopRecord(int32_t channel, uint32_t stream);
int32_t StartMotionRecord(int32_t channel, uint32_t stream); // 移动侦测录像
int32_t StartEventRecord(int32_t channel, uint32_t stream);  // AI事件录像

// 搜索与回放
int searchRecord(time_t tmStart, time_t tmEnd, int64_t chLst, ...);
int playbackPlayByTime(int taskId, time_t startTime, ...);
int playbackPlayByFile(int taskId, FILE_INFO* pFileInfo, ...);
```

#### 5.6.4 录像类型支持

- **全时录像**：`set_schedule_record_enable(true)`，按计划时段录
- **移动侦测录像**：由运动事件脉冲触发
- **AI事件录像**：由 NN 检测结果触发
- **循环录像**：磁盘满时自动覆盖最老文件 `set_cycle_record_enable(true)`
- **录像保留期**：可设置过期天数 `set_record_expired(days)`

---

### 5.7 rpcapi — 进程间 RPC 通信

**文件**：`mod/rpcapi/rpcApiServer/`、`rpcApiClient/`

这是 UI 进程与 NVR 主进程通信的核心机制。

#### 5.7.1 架构

```
UI 进程                           NVR 主进程（libnvrbase.a）
┌─────────────────────┐           ┌──────────────────────────┐
│  rpcApiClient 库    │           │  rpcApiServer            │
│  nvbaserpc.h 接口   │ ←共享内存/→│  nvRpcManager.cc         │
│  nvNetBuf 网络缓冲  │  本地Socket│  nvRpcApiServer.cc       │
└─────────────────────┘           └──────────────────────────┘
```

#### 5.7.2 客户端提供的接口类型

- **系统参数**：时区、日期格式、语言、密码复杂度、UI自定义
- **录像控制**：开始/停止录像、搜索录像、按时间/文件回放
- **磁盘管理**：格式化、挂载/卸载U盘、查询磁盘信息
- **日志读写**：`WriteLog()`、`readLog()`、`searchLogBegin()`
- **AI事件搜索**：`searchAiEvent()` 按时间/通道/事件类型搜索
- **备份任务**：`start_backup_task()`、`get_backup_task_info()`

#### 5.7.3 关键工具类

| 类 | 功能 |
|----|------|
| `CRingBuf` | 无锁环形缓冲区 |
| `CThread` | POSIX 线程封装 |
| `CAppTimer` | 应用层定时器 |
| `nvmemfd` | 匿名共享内存（跨进程数据传输） |
| `nvnetbuf` | 网络帧缓冲，管理多路视频帧索引 |
| `ShareBuf` | 共享内存缓冲区管理 |

---

### 5.8 nets — 网络服务器

**文件**：`mod/nets/src/nvnetserver.cc`、`nvnetnode.cc`、`nvnetop.cc`、`nv_common_search.cc`

**职责**：提供网络远程接入服务，支持手机 APP 通过 NETSDK 协议访问 NVR。

**启动接口**：
```c
int nv_start_netserver(void);   // 启动
int nv_stop_netserver(void);    // 停止
int nv_get_all_online_device(void); // 获取在线设备（1:本地UI搜索 2:APP搜索）
```

---

### 5.9 param — 参数/配置管理

**文件**：`mod/param/`

**配置分类**：

| 模块 | 文件 | 内容 |
|------|------|------|
| 基础参数 | `nvparam.cc` | 初始化/恢复出厂 |
| 通道参数 | `nvchanparam.cc` | 每路IPC的各种配置 |
| 网络参数 | `nvnetparam.cc` | IP/DNS/PPPoE等 |
| 用户管理 | `nvuser.cc` | 账号密码权限 |
| 预置点 | `nvpresetparam.cc` | PTZ预置点 |
| 系统重置 | `nvsysreset.cc` | 恢复出厂设置逻辑 |
| 能力集 | `nvability.cc` | 设备能力查询 `nv_abi_getchannum()` |
| LPC参数 | `nvlpcparam.cc` | 低功耗相机专用参数 |
| 普通设置 | `nvnormalset.cc` | 通用设置 |
| 参数帮助 | `nvparamhelp.cc` | 参数辅助计算 |

---

### 5.10 sys — 系统管理

**文件**：`mod/sys/src/`

| 文件 | 功能 |
|------|------|
| `nvwatchdog.cc` | 看门狗：启动、喂狗、退出喂狗 |
| `nvsystime.cc` | 系统时间：NTP同步、时区设置 |
| `nvcustomer.cc` | 客户定制化：不同品牌/型号配置 |
| `nvfactorymodel.cc` | 工厂模式：出厂测试、老化测试 |
| `nvappscan.cc` | APP扫码添加 IPC 功能 |

---

### 5.11 hal — 硬件抽象层

**文件**：`mod/hal/src/`

| 文件 | 功能 |
|------|------|
| `hwl_mtd.c` | MTD（Flash）读写：用于参数持久化 |
| `pps_hal_strnio.c` | 串口IO抽象：`PPS_HAL_STRNIO_Exit()` 退出时关闭 |
| `pps_device_encryption.c` | 设备加密（唯一设备标识/证书） |

---

### 5.12 discovery — 设备发现

**文件**：`mod/discovery/src/discovery.cc`、`pps_nkit_discovery.c`

**职责**：局域网内广播发现 NVR 设备，供手机 APP 和 PC 客户端搜索。实现了 `pps_nkit_discovery` 协议的服务端，使 NVR 可被局域网扫描发现。

---

### 5.13 rtsps — RTSP 推流服务器

**文件**：`mod/rtsps/src/`

一个自包含的轻量级 RTSP/RTP 服务器实现，包含完整的：
- `rtsp.c` — RTSP 协议解析
- `rtp.c` — RTP 打包
- `rtspservr.c` — TCP 服务端监听
- `mime.c` — MIME 类型
- `bufpool.h` — 内存池

**对外接口**：
```c
int nv_start_rtspserver(void);
int nv_rtsps_send_h264_by_chan(int chan, int stream, int data_type, ...);
```

---

### 5.14 speaker — 音频输出

**文件**：`mod/speaker/src/`

| 文件 | 功能 |
|------|------|
| `audio.cc` | 音频播放核心（PCM） |
| `local_speaker.cc` | 本地扬声器输出 |
| `speaker.cc` | 语音对讲，播放报警音 |

启动完成时调用 `nv_play_audio_up_finish()` 播放开机完成提示音（`Boot_up_finish.pcm`）。

---

### 5.15 preview / playback / ui — 预览回放界面

**preview**：管理视频预览分发，将 DSP 缓冲区的视频帧路由到对应的解码器/显示窗口。支持 1/4/9/16/25/36 路分屏预览。

**playback**：管理录像回放，支持：
- 按时间回放 `playbackPlayByTime()`
- 按文件回放 `playbackPlayByFile()`
- 快进/快退/逐帧 `playbackSetModeEx()`
- 搜索回放月历数据 `playbackGetRecInfoByMonth()`
- 拖动进度条 `playbackSeekTime()`

**ui**：管理界面状态：
- `nvpreviewscreen.cc` — 预览屏幕布局
- `nvplayscreen.cc` — 回放屏幕布局
- `nvpreviewpatrol.cc` — 预览轮巡（自动切换分屏）
- `nvbackup.cc` — U盘备份任务管理
- `nvmp4play.cc` — U盘 MP4 文件回放
- `nvscreenview.cc` — 屏幕视图管理

---

### 5.16 wlan / wired — 网络连接管理

**wlan**：
- `nv_wifi_op.c` — Wi-Fi 连接操作（基于 `wpa_cli`）
- `nvipchannel.cc` — IP通道分配
- `netconfig.c` — 网络配置（IP/网关/DNS）
- `mongoose.c` — 嵌入式 HTTP 服务器（Mongoose 框架）
- `http_client.c` — HTTP 客户端
- `iwlib.c` — 无线扩展库（iw工具）
- `aes.c` — AES 加密（Wi-Fi 密码）

**wired**：`nvwired.cc` 管理有线网络接口（eth0 外网，eth1 POE 内网）

---

### 5.17 led / button / upgrade — 外设与升级

**led**：
- `nv_led.c` — NVR 指示灯控制（状态灯/录像灯）
- `pps_device_led_ctrl.c` — 平台LED控制驱动封装

**button**：`nv_button.cc` — 物理按键处理（复位按钮）

**upgrade**：
- `nv_device_upgrade.cc` — NVR/IPC OTA 升级逻辑
- `pps_device_upgrade.c` — 平台升级接口封装

---

## 6. 工具库（util）解析

| 工具库 | 说明 |
|--------|------|
| `auth/` | MD5（`nv_md5.c`）和 Base64（`nv_base64.c`）实现，用于认证加密 |
| `cjson/` | 开源 cJSON 解析器 + `s2j`（struct to JSON）序列化工具 |
| `cstorage/` | C语言磁盘工具：文件管理、文件系统检测、视频记录辅助函数 |
| `mem_trace/` | 内存泄漏追踪，支持 MIPS/其他架构的 backtrace |
| `cyclone/` | 第三方事件循环框架（Cyclone），用于异步 I/O（`using namespace cyclone`） |
| `libnvrtools/` | NVR 通用工具函数集 |
| `simple_xml/` | 轻量级 XML 解析，用于 ONVIF 等协议 |
| `cmd_srv/` | 命令服务（非 x86 平台，嵌入式调试命令接口） |
| `uallsyms/` | 符号表访问工具（用于crash分析） |

---

## 7. netclient — 网络客户端 SDK

**文件**：`netclient/inc/netclient.h`、`netclient/src/netclient.cc`

**定位**：供**上位机（Windows/Linux PC或手机APP）**连接 NVR 的客户端 SDK，设计为跨平台（含 Windows `__stdcall` 声明）。

**功能**：
- 登录鉴权（TCP/P2P两种连接方式）
- MD5/明文 密码加密选项
- 完整的设备控制和录像回放接口
- 定义了标准 SDK 错误码（`E_SDK_RET_CODE`）

**说明**：这是一个独立的对外 SDK，编译后可供 PC 客户端软件或 APP 使用，不是 NVR 主进程的一部分。

---

## 8. modclient — 调试命令行工具

**文件**：`modclient/src/modclient.cc`

**定位**：开发调试用的命令行交互程序，直接链接 `rpcApiClient` 库，通过 RPC 接口验证主进程功能。

**菜单功能包括**：
- 获取系统启动信息（激活状态、开机向导）
- 时区/日期/时间/语言设置
- 用户账户管理（激活、密码修改）
- 声音侦测报警配置
- 录像管理（搜索、回放）
- OTA 升级测试
- GUI 功能测试
- 磁盘格式化

---

## 9. 关键数据结构

### 9.1 设备信息 S_NV_DEVICEINFO

管理单台 IPC 的完整状态和能力信息，存储在通道管理结构中。包含：固件版本、MAC地址、硬件平台、日夜状态、WiFi信号、电池电量、能力集位图等约 40 个字段。

### 9.2 通道配置 S_NV_IP_CONFIG

存储 IPC 的连接配置：IP地址、端口、用户名、密码、协议类型（`NV_CAM_DEF` 常电 / `NV_CAM_BATTERY` 电池）。

### 9.3 通道管理 S_NV_IP_CHAN

每路 IPC 通道的完整运行时状态：连接状态机、设备信息、配置参数、初始化句柄、电池相机辅助定时器等。

### 9.4 STFS 文件系统表 _STFS_TABLE_

磁盘分区元数据，记录各类文件的起始LBA、文件大小、文件数量，以及覆盖写序列号。

### 9.5 帧头 FsFrame_t

统一的媒体帧头，携带：通道号、流类型、帧类型、时间戳、数据长度等。

### 9.6 报警事件检测结果 nv_objs_t

AI 推理输出：包含目标数量和每个目标的位置（归一化坐标）、置信度、类别标签。

---

## 10. 外部依赖分析

仓库中代码依赖若干外部库，这些库**未包含在仓库内**（通过编译环境提供）：

| 外部依赖 | 用途 | 说明 |
|---------|------|------|
| `cy_core`（cyclone） | 事件循环 | `#include <cy_core.h>`，使用 `cyclone` namespace |
| `pps_media_api` | 平台媒体 SDK | `pps_media_api.h`，硬件编解码、AI推理接口 |
| `pps_msdk_manage` | 媒体SDK管理 | `pps_msdk_manage.h` |
| `pps_cmd_cli` | 命令行接口 | 嵌入式平台调试命令 |
| `pps_debug` | 调试日志 | 平台日志输出 |
| `pps_nkit_discovery` | 设备发现 | 网络发现协议 |
| `nvcommon.h` | 公共类型 | `NV_LOG`/`NV_CHECK` 等宏，应在另一个仓库 |
| `nvdebughelp.h` | 调试工具 | 线程帮助宏 |
| `nvfile.h` | 文件工具 | 文件操作帮助 |
| `avc_or_hevc_sps_parse` | SPS解析 | H.264/H.265 序列参数集解析 |
| **涂鸦 SDK** | 云平台接入 | UUID/AUTHKEY/MQTT，定义在 `nvsystem.h` |
| `wpa_supplicant/wpa_cli` | Wi-Fi 管理 | 系统进程，通过命令行调用 |

---

## 11. 构建系统分析

### 11.1 编译环境变量

```makefile
NV_DEV_TYPE   # 设备类型（NV_DEV_BASE/NV_DEV_TOUCH等）
NV_CPU_ARCH   # CPU架构（arm/mips/x86/aarch64）
NV_CC         # C编译器
NV_CXX        # C++编译器
NV_CFLAGS     # C编译选项
NV_LDFLAGS    # 链接选项
NV_HOME       # 仓库根目录
```

### 11.2 编译目标

```bash
make fw      # 仅编译工具库（util）
make client  # 编译工具库 + netclient
make all     # 编译全部（util + netclient + mod + modclient）
```

### 11.3 输出物

```
libnvrbase.a         # 主静态库（mod编译产物）
libnvrtools.a        # 工具库
libnetclient.so/a    # 网络客户端SDK
modclient            # 调试命令行工具
```

### 11.4 Git 版本信息嵌入

Makefile 将 Git 分支、提交时间、提交哈希、作者、是否有未提交改动注入编译宏，启动时打印：

```
libnvrbase Build:2024-01-15_main_a1b2c3d P James
```

---

## 12. 模块依赖关系图

```
                        ┌─────────────────┐
                        │   nv_base (入口) │
                        └────────┬────────┘
         ┌──────────────────┬────┴──────┬──────────────────────┐
         ▼                  ▼           ▼                       ▼
   ┌──────────┐      ┌──────────┐  ┌───────────┐        ┌───────────┐
   │   ipcm   │      │   dsp    │  │  storage  │        │  rpcapi   │
   │ IP摄像机 │─────▶│ 帧缓冲区 │─▶│(STFS存储) │◀───────│(RPC服务器)│
   │  管理器  │      └──────────┘  └───────────┘        └───────────┘
   └──┬───────┘            │                                   ▲
      │                    │                                   │
      │                    ├─▶ ┌──────────┐             ┌─────┴─────┐
      │                    │   │ preview  │             │rpcApiClient│
      │                    │   │(预览分发)│             │(UI进程使用)│
      │                    │   └──────────┘             └───────────┘
      │                    │
      │                    └─▶ ┌──────────┐
      │                        │    ai    │─▶ ┌──────────┐
      │                        │(NN推理)  │   │  event   │
      │                        └──────────┘   │(事件总线)│
      │                                       └────┬─────┘
      └───────────────────────────────────────────▶│
                                                   ▼
                         ┌──────────────────────────────┐
                         │ 事件消费者（多路并发处理）      │
                         │ • nvmsgpush  (云端推送/通知)  │
                         │ • nvuimsg    (UI 界面刷新)    │
                         │ • nvnetmsg   (网络消息)       │
                         │ • nvsoundlight(声光报警)      │
                         └──────────────────────────────┘

所有模块共同依赖：
  param（配置参数）、sys（系统服务）、hal（硬件抽象）
  util/cyclone（事件循环）、util/cjson（JSON）
```

---

## 13. 数据流图：视频流从IPC到存储/显示

```
                    IPC 摄像机（如：192.168.1.100）
                           │ H.264/H.265 视频流
                           │ G.711a/AAC 音频流
                           ▼
                  ┌─────────────────────┐
                  │   ipcm 接入层        │
                  │  (Meari/ONVIF/RTSP  │
                  │  /NETSDK/PRTP2)      │
                  └──────────┬──────────┘
                             │ FsFrame_t（帧头+数据）
                             ▼
                  ┌─────────────────────┐
                  │   dsp 帧缓冲区       │
                  │  nv_dsp_data_pro()  │
                  │  (循环帧缓冲区)      │
                  └──────────┬──────────┘
                             │
          ┌──────────────────┼─────────────────────┐
          ▼                  ▼                     ▼
  ┌───────────────┐  ┌──────────────┐   ┌─────────────────┐
  │  storage       │  │   preview   │   │      ai          │
  │ (写录像/写图片) │  │ (解码→显示)  │   │ (YUV帧→NN推理)  │
  │ nvDataVideo   │  │ pps_media   │   │ hl_pps_nn_*     │
  │ Record.cc     │  │ _api 解码器  │   └────────┬────────┘
  └───────────────┘  └──────────────┘            │ 检测结果
                                                  ▼
                                        ┌─────────────────┐
                     ┌──────────────────│   nv_aievent    │
                     │                  └─────────────────┘
                     ▼                          │
              ┌─────────────┐                  ▼
              │    rtsps    │          ┌─────────────────┐
              │ (RTSP推流)  │          │  nv_event_trigger│
              │ 供第三方接入 │          │  (事件总线广播)   │
              └─────────────┘          └────────┬────────┘
                                                │
                              ┌─────────────────┼────────────┐
                              ▼                 ▼            ▼
                       nvmsgpush          nvsoundlight   nvuimsg
                      (云端推送)          (声光报警)    (UI刷新)
```

---

## 14. 数据流图：报警事件处理流程

```
触发源：
  A. IPC 上报移动侦测
  B. AI 模块检测到人形/宠物等
  C. 系统内部状态变化

        A/B/C
          │
          ▼
  ┌───────────────┐
  │ nv_event_trigger│
  │ (type, chan,   │
  │  a, b, data)  │
  └───────┬───────┘
          │ 发送到事件队列
          ▼
  ┌───────────────────────────────┐
  │       event 模块分发           │
  ├───────────────────────────────┤
  │ E_EVENT_MSG_ALARM_*           │
  └──┬────────────┬──────────┬───┘
     │            │          │
     ▼            ▼          ▼
┌─────────┐ ┌──────────┐ ┌────────────────────┐
│nvmsgpush│ │nvsound   │ │ storage 存储        │
│涂鸦云推送│ │light     │ │StartEventRecord()  │
│（消息通知│ │声光报警器 │ │开始录像，保存截图   │
│  /推送）│ └──────────┘ └────────────────────┘
└─────────┘
     │
     ▼
 MQTT/涂鸦云
 ─────────────▶ 手机 APP 弹窗通知
```

---

## 15. 设计模式与架构特点总结

### 15.1 架构模式

| 模式 | 应用位置 | 说明 |
|------|---------|------|
| **工厂模式（Factory）** | `ipcm/nvfactory.h` | 5种IPC接入协议统一抽象，通过回调函数表实现多态分发 |
| **观察者/发布订阅（Observer）** | `event/nvevent.cc` | `nv_event_trigger` 是 publisher，各消费者模块是 subscriber |
| **代理模式（Proxy）** | `rpcapi` | rpcApiServer 是本进程代理，rpcApiClient 是远端代理 |
| **门面模式（Facade）** | `base/nv_base.h` | 将所有子系统封装为统一的外部 API 接口 |
| **分层架构（Layered）** | 整体 | 应用层→中间件层→HAL层→硬件，层次清晰 |
| **状态机（State Machine）** | `ipcm/nvipcmtype.h` | IPC 通道连接状态机，状态转换驱动事件 |

### 15.2 代码质量特征

- **双写一致性**：配置修改先持久化再下发，断电不丢失
- **参数边界保护**：`NV_CHECK` 宏全面覆盖入参检查
- **读写锁**：`pthread_rwlock` 保护通道状态，高并发安全
- **启动顺序依赖**：严格的初始化顺序，任意失败即退出
- **C/C++ 兼容**：`extern "C"` 接口，模块可被 C 代码调用
- **多平台支持**：通过 `NV_CPU_ARCH` 变量支持 ARM/MIPS/x86 编译

### 15.3 系统特色

1. **自研 STFS 文件系统**：绕过操作系统文件系统，Direct I/O 直写磁盘，提升录像写入性能，减少断电丢帧风险
2. **电池相机专项支持**：单独的唤醒/休眠状态机，低功耗相机的特殊管理逻辑（AOV模式、休眠唤醒命令）
3. **协议层完全抽象**：同一台 NVR 可同时接入私有协议、ONVIF标准协议、RTSP三方设备
4. **UI 进程隔离**：UI 崩溃不影响后台录像（两个进程通过 RPC 通信）
5. **OTA 闪存分区**：硬盘上预留 768MB OTA 分区，支持在线升级 NVR 固件和多台 IPC 固件

---

*本文档基于对 `nvr-base-core` 仓库全量源码（C/C++ 文件、头文件、Makefile）的静态分析生成，所有描述均基于代码事实。*
