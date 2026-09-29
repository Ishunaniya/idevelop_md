# nvr-mearicloud 仓库源码深度分析报告

> 版权所有：杭州美瑞智联科技有限公司 (Meari Technology Co., Ltd)  
> 分析基于仓库实际源码，不作主观推断。

---

## 目录

1. [项目概述](#1-项目概述)
2. [整体架构](#2-整体架构)
3. [目录结构详解](#3-目录结构详解)
4. [核心模块分析](#4-核心模块分析)
   - 4.1 [入口与生命周期 (main.c)](#41-入口与生命周期-mainc)
   - 4.2 [云端SDK接口 (mnsdk)](#42-云端sdk接口-mnsdk)
   - 4.3 [设备注册模块 (nvr_register)](#43-设备注册模块-nvr_register)
   - 4.4 [实时预览模块 (nvr_preview)](#44-实时预览模块-nvr_preview)
   - 4.5 [回放模块 (nvr_playback)](#45-回放模块-nvr_playback)
   - 4.6 [DP数据点模块 (nvr_dp)](#46-dp数据点模块-nvr_dp)
   - 4.7 [告警/移动侦测模块 (nvr_motion_detect)](#47-告警移动侦测模块-nvr_motion_detect)
   - 4.8 [双向对讲模块 (nvr_speak)](#48-双向对讲模块-nvr_speak)
   - 4.9 [OTA固件升级模块 (nvr_upgrade)](#49-ota固件升级模块-nvr_upgrade)
   - 4.10 [本地RESTful服务 (nvr_restful_server)](#410-本地restful服务-nvr_restful_server)
   - 4.11 [设备发现模块 (nvr_discovery)](#411-设备发现模块-nvr_discovery)
   - 4.12 [板级初始化 (nvr_device_board)](#412-板级初始化-nvr_device_board)
5. [RPC 通信层](#5-rpc-通信层)
6. [工具库层 (mod/util)](#6-工具库层-modutil)
7. [多平台支持与编译系统](#7-多平台支持与编译系统)
8. [关键数据流图](#8-关键数据流图)
   - 8.1 [启动初始化流程](#81-启动初始化流程)
   - 8.2 [实时预览数据流](#82-实时预览数据流)
   - 8.3 [告警推送数据流](#83-告警推送数据流)
   - 8.4 [回放控制流程](#84-回放控制流程)
   - 8.5 [DP配置下发流程](#85-dp配置下发流程)
9. [线程模型](#9-线程模型)
10. [安全机制](#10-安全机制)
11. [关键常量与配置参数汇总](#11-关键常量与配置参数汇总)
12. [依赖的外部预编译库](#12-依赖的外部预编译库)
13. [代码质量与特点观察](#13-代码质量与特点观察)
14. [总结](#14-总结)

---

## 1. 项目概述

**nvr-mearicloud** 是一个运行在嵌入式 NVR（Network Video Recorder，网络录像机）设备上的**云端接入应用程序**，由美瑞智联（Meari Technology）开发，目标是将其自研 NVR 硬件接入 Meari 云平台，实现：

- 手机 APP 远程实时预览（P2P 视频流）
- 录像回放检索与播放
- 双向对讲（语音对讲）
- 告警推送（移动侦测、人形/车辆/宠物 AI 检测、PIR 等）
- DP 数据点云端配置管理（设备属性的读/写）
- 固件 OTA 在线升级
- 本地 HTTP RESTful 管理接口

该应用程序以 **独立进程** 形式运行，通过 **RPC（Remote Procedure Call）共享内存机制** 与底层 NVR 基础系统（nvr-base）进行交互。它本身不直接操作硬件，而是依赖底层 NVR OS 提供的接口获取视频流、存储、摄像头控制等能力。

---

## 2. 整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Meari Cloud Platform                         │
│            (HTTPS REST + WebRTC P2P + MQTT/长连接)                   │
└─────────────────────────┬───────────────────────────────────────────┘
                          │  mnsdk (闭源预编译库)
┌─────────────────────────▼───────────────────────────────────────────┐
│                     nvr-mearicloud  (本仓库)                          │
│                                                                       │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │                   mod/meari  (业务逻辑层)                    │    │
│  │  main.c | nvr_register | nvr_preview | nvr_playback          │    │
│  │  nvr_dp | nvr_motion_detect | nvr_speak | nvr_upgrade        │    │
│  │  nvr_restful_server | nvr_discovery | nvr_device_board       │    │
│  └──────────────────────┬──────────────────────────────────────┘    │
│                          │                                            │
│  ┌───────────────────────▼─────────────────────────────────────┐    │
│  │                   mod/rpc  (RPC 客户端层)                    │    │
│  │  nv_meari_rpc.h | nv_meari_preveiw.h | nv_meari_playback.h  │    │
│  │  nv_meari_speak.h | nv_meari_pushmsg.h | nv_meari_com_def.h │    │
│  └──────────────────────┬──────────────────────────────────────┘    │
│                          │                                            │
│  ┌───────────────────────▼─────────────────────────────────────┐    │
│  │                  mod/util  (工具库层)                         │    │
│  │  pps_osal (OS抽象) | pps_util (JSON/HTTP/加密/网络/日志)     │    │
│  └─────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘
                          │  共享内存 / nvbaserpc
┌─────────────────────────▼───────────────────────────────────────────┐
│              nvr-base (底层NVR OS，闭源，预编译)                      │
│  视频编解码 | 存储管理 | 摄像头控制 | 文件系统 | 网络配置              │
└─────────────────────────────────────────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────────────┐
│                 硬件平台 (INGENIC / Sigmastar / MTK)                   │
│  IPC 摄像头通道 × N | 硬盘 | 网口 | WiFi                              │
└─────────────────────────────────────────────────────────────────────┘
```

**架构特点：**
- **分层设计**：业务逻辑、RPC通信、工具库三层清晰分离。
- **闭源SDK依赖**：Meari 云端接入能力封装在预编译的 `libmnsdk.a` 和 `libmearisdk.a` 中，本仓库不含其源码。
- **跨平台**：通过条件编译和配置脚本支持5种嵌入式芯片平台。
- **进程间通信**：本进程与底层 NVR 系统通过 `nvbaserpc`（共享内存 RPC）通信，而非直接驱动硬件。

---

## 3. 目录结构详解

```
nvr-mearicloud/
├── Makefile                  # 顶层 Makefile，分发到子目录
├── auto_build.sh             # 自动化编译脚本（多平台）
├── README.md                 # 简要说明
├── .gitignore
│
├── inc/                      # 公共头文件（对外暴露的SDK接口）
│   ├── mnsdk.h               # Meari云端SDK核心API（回调类型、结构体、函数声明）
│   └── mnsdk_meari.h         # Meari SDK 初始化扩展参数结构体
│
├── config/                   # 各平台编译环境变量配置脚本
│   ├── x86                   # x86 (调试用)
│   ├── ss661q                # Sigmastar ss661q (ARM)
│   ├── t41                   # INGENIC T41
│   ├── mipsel_a1x            # INGENIC A1x (MIPS小端)
│   ├── mipsel_a1nt           # INGENIC A1nt (MIPS小端)
│   └── MTK7628               # MediaTek MT7628 (MIPS)
│
├── lib/                      # 预编译第三方/自研静态库（按平台分目录）
│   ├── INGENIC_A1/           # libmnsdk.a, libmearisdk.a, libmbedtls.a, libsrtp2.a, libWebrtcClient.a ...
│   ├── INGENIC_A1NT/
│   ├── INGENIC_T41/
│   ├── sigmastar/
│   └── MTK7628/
│
├── build/                    # 编译输出目录（按平台存放编译结果）
│   ├── INGENIC_A1/
│   ├── INGENIC_A1_BASE/
│   ├── INGENIC_T41/
│   └── sigmastar/
│
└── mod/                      # 源码主体
    ├── Makefile              # 模块Makefile（编译规则、链接指令）
    │
    ├── meari/                # 业务逻辑主模块
    │   ├── inc/              # 模块内部头文件
    │   │   ├── nvr_common.h      # 公共宏/结构体/枚举
    │   │   ├── nvr_device_board.h
    │   │   ├── nvr_discovery.h
    │   │   ├── nvr_dp.h          # DP数据点定义（400+ 行的DP ID常量）
    │   │   ├── nvr_media.h
    │   │   ├── nvr_motion_detect.h
    │   │   ├── nvr_playback.h
    │   │   ├── nvr_preview.h
    │   │   ├── nvr_register.h
    │   │   ├── nvr_restful_server.h
    │   │   ├── nvr_speak.h
    │   │   ├── nvr_ssl.h
    │   │   ├── nvr_upgrade.h
    │   │   ├── nvr_util.h
    │   │   └── pps_device_time.h
    │   └── src/              # 模块源文件（9357行C代码）
    │       ├── main.c            (363行) 程序入口
    │       ├── nvr_register.c    (703行) 设备注册
    │       ├── nvr_dp.c          (4436行) DP数据点处理（最大文件）
    │       ├── nvr_preview.c     (612行) 实时预览推流
    │       ├── nvr_playback.c    (718行) 录像回放
    │       ├── nvr_motion_detect.c(193行) 告警侦测
    │       ├── nvr_speak.c       (84行)  双向对讲
    │       ├── nvr_upgrade.c     (233行) OTA升级
    │       ├── nvr_restful_server.c(368行) 本地HTTP服务
    │       ├── nvr_discovery.c   (523行) 设备发现
    │       ├── nvr_device_board.c(191行) 板级初始化
    │       ├── nvr_media.c       (52行)  媒体格式转换
    │       ├── nvr_util.c        (515行) 通用工具函数
    │       └── pps_device_time.c (366行) 时间管理
    │
    ├── rpc/                  # RPC接口声明层（纯头文件，无src）
    │   └── inc/
    │       ├── nv_meari_rpc.h        # 主RPC接口（摄像头控制、存储、设备信息等）
    │       ├── nv_meari_com_def.h    # 通道编号换算宏
    │       ├── nv_meari_preveiw.h    # 预览流RPC
    │       ├── nv_meari_playback.h   # 回放RPC
    │       ├── nv_meari_speak.h      # 对讲RPC
    │       ├── nv_meari_pushmsg.h    # 推送消息RPC
    │       └── nv_meari_searchrecord.h # 录像检索RPC
    │
    └── util/                 # 工具库（源码 + 头文件）
        ├── inc/
        │   ├── pps_osal/     # OS抽象层 (14个头文件)
        │   └── pps_util/     # 实用工具库 (39个头文件)
        └── src/              # 工具库源码 (11个.c文件)
            ├── pps_mongoose.c  (5353行) 嵌入式HTTP服务器
            ├── pps_netconfig.c (3483行) 网络配置
            ├── pps_cJSON.c              JSON解析
            ├── pps_md5.c                MD5
            ├── pps_base64.c             Base64
            ├── pps_requests.c           HTTP请求封装
            └── ...
```

---

## 4. 核心模块分析

### 4.1 入口与生命周期 (main.c)

`main.c` 是整个应用程序的控制中枢，负责所有模块的启动序列编排与退出清理。

**启动序列（按代码顺序）：**

```
main()
  │
  ├─ nvr_init_signals()          # 注册信号处理（SIGINT/SIGTERM → nvr_exit_handler）
  ├─ set_media_info()            # 初始化媒体参数（分辨率/码率/编码类型）
  ├─ nv_tuya_wait_nvr_start()    # 阻塞等待底层NVR基础系统就绪
  ├─ nv_tuya_init_share()        # 初始化RPC共享内存
  ├─ nv_tuya_init_nvr_ipclist()  # 初始化子设备（IPC摄像头）参数列表
  ├─ nv_tuya_start_preview()     # 启动预览流缓冲区
  ├─ nv_tuya_start_speak()       # 启动对讲缓冲区
  ├─ sleep(2)                    # 等待稳定
  ├─ init_play_id()              # 初始化回放会话ID管理
  ├─ pps_device_board_init()     # 板级硬件初始化
  ├─ restful_webserver_init()    # 启动本地HTTP服务（80端口）
  │
  └─ 云端注册循环（while(1)）
      ├─ pps_device_get_token()      # 从文件(/home/cfg/device.token)读token
      ├─ pps_register_init()         # 向Meari云注册设备，获取context（含签名、服务器域名等）
      ├─ nvr_mnsdk_init()            # 填充mnsdk_settings_t配置
      └─ mnsdk_init()                # 初始化云端SDK（成功则break出循环）
           失败 → sleep(5) → retry
  │
  ├─ nvr_init_dp_data()          # 初始化DP数据点（向云端同步设备属性）
  ├─ pthread_create(check_channel_thread)    # 通道连接状态检测线程
  ├─ pthread_create(motion_detect_thread)   # 告警侦测线程
  ├─ pthread_create(online_status_thread)   # APP在线状态检测线程
  │
  └─ while(1) sleep(60)          # 主线程守护循环
```

**退出清理（nvr_exit_handler）：**

```
nvr_exit_handler()
  ├─ nvr_deinit_dp_data()    # 释放DP数据
  ├─ live_stop_all()         # 停止所有预览推流线程
  ├─ playback_release()      # 释放回放资源
  ├─ mnsdk_release()         # 释放云端SDK
  └─ pthread_cancel(3个线程)
```

**关键数据结构：**

```c
// 全局上下文，存储注册后的设备信息、签名、服务器地址等
static meari_context_t g_meari_context;

// 主线程管理的3个后台线程句柄
typedef struct {
    pthread_t check_channel_thread;   // 通道连接状态检测
    pthread_t motion_detect_thread;   // 告警检测
    pthread_t online_status_thread;   // APP在线状态检测
} main_thread_t;
```

---

### 4.2 云端SDK接口 (mnsdk)

`mnsdk`（Meari Network SDK）是一个**闭源预编译静态库**（`libmnsdk.a`），封装了 Meari 云平台的全部网络通信逻辑。本仓库只包含其头文件 `inc/mnsdk.h`。

**设备类型定义：**

| 宏 | 值 | 含义 |
|---|---|---|
| `MNSDK_DEVICE_TYPE_IPC` | 0 | 常电摄像机 |
| `MNSDK_DEVICE_TYPE_SNAP` | 1 | 电池摄像机 |
| `MNSDK_DEVICE_TYPE_DOORBELL` | 2 | 门铃 |
| `MNSDK_DEVICE_TYPE_NVR` | 3 | 网络录像机 |

> 注意：main.c 中实际传入 `MNSDK_DEVICE_TYPE_IPC`（值0），并非 NVR 类型，这可能是一个历史遗留问题。

**媒体流类型：**

| 宏 | 含义 |
|---|---|
| `MNSDK_STREAM_TYPE_PREVIEW` | 实时预览流 |
| `MNSDK_STREAM_TYPE_PLAYBACK` | 录像回放流 |

**编码类型：** H264、H265、G711.A、G711.U、AAC

**视频码流类型：** 主码流（高清）、超清码流、辅码流

**核心回调函数（5个）：**

```c
// 1. 接收媒体流（APP → 设备，如对讲音频）
mnsdk_stream_received_callback
    参数: channel, stream_type, frame_type, encode_type, video_type, data, length, params

// 2. 接收数据命令（APP → 设备，如回放控制命令）
mnsdk_data_received_callback
    参数: channel, sid, data, length, params

// 3. 接收配置请求（APP获取/下发设备配置）
mnsdk_config_request_received_callback
    参数: sid, contact, content, content_length

// 4. 接收配置通知（云端推送无需响应的配置变更）
mnsdk_config_notify_received_callback
    参数: sid, contact, content, content_length

// 5. 事件推送响应（告警推送后的云端确认）
mnsdk_event_publish_response_received_callback
    参数: channel, sid, eventid, result
```

**核心API：**

```c
int mnsdk_init(mnsdk_settings_t *settings);           // 初始化SDK
int mnsdk_release();                                   // 释放SDK
int mnsdk_stream_send(channel, sid, stream_type, frame_type, encode_type, video_type, stream, length, params); // 发送媒体流
int mnsdk_data_send(channel, sid, data, length, params);  // 发送数据
int mnsdk_config_response(sid, contact, content, content_length); // 响应配置请求
int mnsdk_config_sync_request(sid, content, content_length);       // 同步配置请求
int mnsdk_event_publish_request(sid, eventid, params, content, content_length); // 发布事件（告警）
int mnsdk_server_is_connected();                      // 检查云端连接状态
```

---

### 4.3 设备注册模块 (nvr_register)

负责设备向 Meari 云平台完成注册鉴权，获取后续通信所需的 token 和服务器地址。

**核心函数：`pps_register_init(meari_settings_t*, meari_context_t*)`**

**注册流程：**

```
pps_register_init()
  │
  ├─ 读取本地存储的 token（/home/cfg/device.token）
  ├─ 构造注册请求（包含 licenceid、mac、firmware_version、capability 等）
  ├─ 计算签名
  │   ├─ mnsdk_md5()    → MD5 摘要
  │   └─ mnsdk_sha1()   → HMAC-SHA1 签名
  ├─ HTTPS POST 请求至 Meari 服务器
  │   mnsdk_ssl_request() → uni_ssl_request() (mbedTLS)
  └─ 解析响应 JSON
      ├─ 提取 signature（设备签名）
      ├─ 提取 server_domain（服务器域名）
      ├─ 提取 gwUrl（网关URL，用于在线状态检测）
      └─ 填充 meari_context_t
```

**注册接口示例（来自代码注释）：**
```
GET /v10/devices/binding_state?partnerId=8&deviceID=ppsc614742a2cedb443a&t=1451606410&sign=3b2242d71a75dddee350db11367adbb2
Host: apis-us-west.meari.com.cn
```

**加密算法：**
- MD5（`pps_md5`）：用于生成设备标识摘要
- HMAC-SHA1（`pps_sha1` + `pps_hmac`）：用于接口签名
- Base64（`pps_base64`）：签名结果编码
- mbedTLS：HTTPS 通信的 TLS 实现

---

### 4.4 实时预览模块 (nvr_preview)

实现 APP 通过云端 P2P 观看 NVR 各通道实时视频流的功能。

**关键结构体：**

```c
// 每路预览会话的上下文
typedef struct {
    int channel;       // IPC通道号（0~N-1）
    int sid;           // P2P会话ID（1~36，最多36个并发用户）
    int stream;        // 码流类型（主码流/辅码流）
    pthread_t thread;  // 推流线程
    int running;       // 线程运行标志
    ...
} preview_context_t;

// 支持最多 NVR_P2P_USER_NUM (36) 个并发预览会话
preview_context_t g_preview_context[NVR_P2P_USER_NUM];
```

**推流线程（thread_live_stream）核心逻辑：**

```
thread_live_stream(preview_context_t *pParam)
  │
  ├─ 循环: 从底层RPC获取帧数据
  │   nv_tuya_forceIframe(chan, stream)   // 首帧请求I帧
  │   │
  │   └─ 读取帧（通过nvFsFrame共享内存环形缓冲区）
  │       ├─ 视频帧：stream_type_change_mnsdk() → frame_type_change_mnsdk() → video_encode_change_mnsdk()
  │       │         → mnsdk_stream_send(MNSDK_STREAM_TYPE_PREVIEW, ...)
  │       └─ 音频帧：audio_change_2_msndk() [格式转换] → mnsdk_stream_send(...)
  │
  └─ 退出条件：pParam->running == 0 || 通道断线
```

**码流类型映射（通道编号 ↔ 码流）：**

来自 `nv_meari_com_def.h`：
```c
#define NV_TUYA_STREAMS_PER_CHAN (5)      // 每通道5个流槽位
#define NV_TUYA_CHN2CHAN(chn)  (chn / 5) // 通道号提取
#define NV_TUYA_CHN2STREAM(chn)           // 流类型提取（0→主，1→辅）
#define NV_TUYA_CHAN2CHN(chan, stream)     // 逆向映射
```

---

### 4.5 回放模块 (nvr_playback)

处理来自 APP 的录像检索与播放控制指令。

**命令集（由 `main.c` 的 `cmd_data_received_callback` 路由）：**

```c
typedef enum {
    MNSDK_PLAYBACK_CMD_SEARCH_BY_DAY   = 0,  // 按日检索
    MNSDK_PLAYBACK_CMD_SEARCH_BY_MONTH = 1,  // 按月检索
    MNSDK_PLAYBACK_CMD_SEARCH_BY_TIME  = 2,  // 按时间段检索
    MNSDK_PLAYBACK_CMD_SEARCH_SD_STATES= 3,  // 查询SD卡状态
    MNSDK_PLAYBACK_CMD_PLAY_START      = 4,  // 开始播放
    MNSDK_PLAYBACK_CMD_PLAY_PAUSE      = 5,  // 暂停
    MNSDK_PLAYBACK_CMD_PLAY_RESUME     = 6,  // 继续
    MNSDK_PLAYBACK_CMD_PLAY_SEEKTIME   = 7,  // 跳转时间点
    MNSDK_PLAYBACK_CMD_PLAY_STOP       = 8,  // 停止
} MNSDK_PLAYBACK_CMD_E;
```

**会话管理（play_id）：**

回放模块使用一个 `playbakck_paly_id_t g_playbakck_paly_id[36]` 数组管理最多36个并发回放会话，通过 `(channel, sid)` 二元组唯一标识一路回放。

```
add_play_id(channel, sid) → 分配 play_id（空闲槽位）
get_play_id(channel, sid) → 查找或新建 play_id
clean_play_id(play_id)    → 释放槽位
```

**playback 命令处理流：**

```
cmd_data_received_callback()
  │
  ├─ 检索命令（SEARCH_BY_DAY / SEARCH_BY_MONTH）
  │   → playback_search_request()
  │       → nv_tuya_get_rec_bymonth() / nv_tuya_get_rec_bymonth_utc()  [RPC]
  │       → 构造结果JSON
  │       → mnsdk_data_send() → APP
  │
  └─ 播控命令（PLAY_START / PAUSE / RESUME / SEEKTIME / STOP）
      → playback_control_request()
          ├─ PLAY_START → 启动回放线程，从NVR文件系统读帧 → mnsdk_stream_send()
          ├─ PLAY_PAUSE / RESUME → 控制推流速率标志
          ├─ PLAY_SEEKTIME → 重定位读帧位置
          └─ PLAY_STOP → 停止线程，释放play_id
```

**特殊标志：**
```c
reserved = playback_info.reserved[0];
// reserved[0] == 1: NVR回放可不限速快速推流（APP端WebRTC网页回放则正常速度）
```

---

### 4.6 DP数据点模块 (nvr_dp)

DP（Data Point）是 Meari 云平台的 IoT 属性模型，每个 DP 对应设备的一个可读/可写属性（类似涂鸦的DP点概念）。本模块是仓库中**代码量最大的模块（4436行）**，处理所有设备属性的云端同步。

**DP ID 定义举例（来自 nvr_dp.h）：**

| DP ID | 含义 |
|---|---|
| `"1"` | APP解绑用户 |
| `"50"` | 设备SN号码 |
| `"51"` | 固件名称 |
| `"52"` | 固件版本号 |
| `"54"` | 时区 |
| `"55"` | 设备能力级 |
| `"56"` | MAC地址 |
| `"61"` | licenseID |
| `"63"` | 设备型号 |
| `"64"` | 平台代号 |
| `"65"` | NVR在线时长(秒) |
| `"100"` | 当前WIFI名称 |
| `"104"` | 视频吞吐量 |
| `"141"` | IPC智能侦测开关 |
| `"143"` | IPC智能侦测灵敏度 |
| `"825"` | 主告警总开关 |
| ... | （共约100+个DP定义） |

**DP模块核心函数：**

```c
nvr_init_dp_data(int channels)     // 启动时向云端同步所有DP属性
pps_iot_config_request(sid, contact, content, len) // 处理云端配置请求/通知
nvr_dispose_event_id(channel, sid, eventid, result) // 处理告警推送响应
```

**DP数据处理流程：**

```
云端下发 config_request_received / config_notify_received
  │
  └─ pps_iot_config_request()
      ├─ 解析 JSON content（pps_cJSON）
      ├─ 根据 DP ID 路由到对应处理函数
      │   示例:
      │   "54" (时区) → nv_tuya_set_tz_minute() [RPC]
      │   "reboot"   → nv_tuya_restart() [RPC]
      │   "reset"    → nv_tuya_reset() [RPC]
      │   摄像头属性  → nv_tuya_set_chan_*() [RPC]
      │   ...
      └─ mnsdk_config_response() 响应云端
```

---

### 4.7 告警/移动侦测模块 (nvr_motion_detect)

`thread_md_proc` 线程持续监听底层 NVR 的事件消息队列，当检测到告警事件时向 Meari 云推送带图片的告警通知。

**支持的告警类型：**

| 底层事件类型 | 云端告警类型 |
|---|---|
| `E_EVENT_MSG_ALARM_MOTION_DECT` | `NOTIFICATION_NAME_MOTION`（移动侦测）|
| `E_EVENT_MSG_ALARM_NN_PERSON_DECT` | `NOTIFICATION_NAME_AI_PERSON`（AI人形）|
| `E_EVENT_MSG_ALARM_PIR` | `NOTIFICATION_NAME_PIR`（PIR红外）|
| `E_EVENT_MSG_ALARM_DB_DECT` | `NOTIFICATION_NAME_DECIBEL`（声音侦测）|
| `E_EVENT_MSG_ALARM_NN_PET_DECT` | `NOTIFICATION_NAME_PET`（宠物）|
| `E_EVENT_MSG_ALARM_NN_CAR_DECT` | `NOTIFICATION_NAME_CAR`（车辆）|
| `E_EVENT_MSG_ALARM_NN_PACKAGE_DECT` | `NOTIFICATION_NAME_PACKAGE`（包裹）|
| `E_EVENT_MSG_ALARM_DOORBELL` | （门铃，当前未映射）|

**告警推送流程：**

```
thread_md_proc()
  │
  └─ 监听事件队列（nvevent接口）
      │
      告警事件触发
      │
      nv_meari_alarm_do(alarm_parameter, chn, msgtype, snapshot_jpeg, len)
        │
        ├─ 图片大小过滤（len < 100KB 才推送）
        ├─ change_alarm_type_2_mnsdk() → 转换告警类型
        ├─ 构造 params JSON:
        │   {"type":N, "channel":N, "domain":"...", "deviceid":"...",
        │    "userid":"...", "secretkey":"..."}
        └─ mnsdk_event_publish_request(NULL, "alarm", params, jpeg_data, len)
            → 云端接收后向APP发推送通知（含缩略图预览）
```

**告警响应处理：**

```
event_push_response_received() → nvr_dispose_event_id()
  处理云端对告警推送的确认或错误响应
```

---

### 4.8 双向对讲模块 (nvr_speak)

支持 APP 用户通过云端与 NVR 连接的 IPC 摄像头进行双向语音对讲。

**接收对讲音频（APP → 设备）：**

```
mnsdk 回调 → stream_received_to_speak(channel, stream_type, frame_type, encode_type, ...)
  │
  └─ 判断 stream_type == MNSDK_STREAM_TYPE_PREVIEW（对讲流复用预览通道）
      │
      └─ nv_tuya_speak_send() [RPC] → 底层NVR播放音频到扬声器
```

**发送本地音频（设备 → APP）：**
本地麦克风音频由底层 NVR 采集后，通过预览推流线程（`thread_live_stream`）的音频帧路径随视频一起推送至 APP。

---

### 4.9 OTA固件升级模块 (nvr_upgrade)

支持通过 Meari 云下发 OTA 升级包，分段下载后通过 RPC 传送给底层 NVR 执行升级。

**升级流程：**

```
DP点收到升级指令（含URL）
  │
  └─ pps_download_ota(chan, url, &percent)
      │
      ├─ http_fopen(url)              // 打开HTTP下载流（pps_http_file）
      ├─ http_fsize()                 // 获取固件总大小
      └─ 循环下载（每次最大 MAX_DOWNLOAD_SIZE_ONCE）
          ├─ http_fread(buff, size)
          ├─ nv_tuya_send_ota_package(chan, buff, ret, total_len, offset) [RPC]
          ├─ percent = bytes * 100 / file_length
          └─ 失败重试 10 次
```

**多通道 OTA：**
NVR 不仅可以升级自身，还支持通过 `nv_tuya_send_ota_package(chan, ...)` 向指定通道的 IPC 子设备推送固件包，实现子摄像头的远程升级。

---

### 4.10 本地RESTful服务 (nvr_restful_server)

使用 `pps_mongoose`（Mongoose HTTP 嵌入式服务器）在设备 **80端口** 提供本地 HTTP RESTful 管理接口，主要供局域网内的管理工具使用。

**认证机制：**
HTTP Basic Auth，从 `/home/cfg/access_user.password` 文件中读取用户名密码，使用 `mg_base64_decode` 解码 Authorization 头。

**HTTP 方法支持：** GET、POST

**主要用途：**
- 查询设备状态
- 局域网下的设备配置
- 与 Meari APP 局域网直连模式配合

---

### 4.11 设备发现模块 (nvr_discovery)

实现 NVR 局域网设备发现功能（UDP 广播/多播），使 Meari APP 可以在局域网内自动发现 NVR 设备。

> 注：代码注释显示此功能"已从本进程移除，放到 base 库中去了"（`main.c` 中 `disocvery_server_init()` 已被注释）。但 `nvr_discovery.c` 源码（523行）仍保留在仓库中，说明可能仍作为库被 base 层调用或保留供参考。

---

### 4.12 板级初始化 (nvr_device_board)

负责读取设备硬件信息并对外暴露 `this_board` 全局指针：

```c
typedef struct {
    char *licenceid;   // 设备授权ID（来自硬件存储/文件系统）
    char *secretkey;   // 设备密钥
    char *mac;         // MAC地址
    ...
} device_board_t;

extern device_board_t *this_board;
```

`licenceid` 和 `secretkey` 是设备出厂时烧录的唯一标识，用于云端注册鉴权。`main.c` 在注册前校验这两个字段是否为空。

---

## 5. RPC 通信层

本仓库通过 `nvbaserpc`（底层预编译库）与 NVR 基础系统进行进程间通信。RPC 模块完全由头文件声明（`mod/rpc/inc/`），实现在预编译的 `lib/` 库中。

**通信机制：共享内存 + 信号量（典型嵌入式IPC方案）**

`nv_meari_rpc.h` 暴露了极其丰富的 API，按功能分类：

| 类别 | 代表函数 |
|---|---|
| 系统控制 | `nv_tuya_restart()`, `nv_tuya_reset()` |
| 通道连接状态 | `nv_tuya_get_ipc_connect(chan)`, `nv_tuya_get_ipc_awake(chan)` |
| 视频预览 | `nv_tuya_start_preview()`, `nv_tuya_forceIframe()` |
| 视频分辨率 | `nv_tuya_set_video_resolution()`, `nv_tuya_get_video_resolution()` |
| 录像检索 | `nv_tuya_get_rec_bymonth()`, `nv_tuya_get_rec_bymonth_utc()` |
| 存储管理 | `nv_get_hd_status()`, `nv_get_hd_storage()`, `nv_format_hd()` |
| 摄像头属性 | `nv_tuya_set_chan_flip_onoff()`, `nv_tuya_set_chan_daynight_mode()` 等20+个 |
| AI侦测 | `nv_tuya_set_chan_ai_dect_onoff()`, `nv_tuya_set_chan_ai_dect_human_onoff()` 等 |
| 对讲 | `nv_tuya_sys_speak_init()` |
| OTA | `nv_tuya_send_ota_package()`, `nv_tuya_sys_updatefw_do()` |
| 低功耗相机 | `nv_tuya_get_power_mode()`, `nv_tuya_send_batterycam_cmd_awake()` |
| 时间 | `nv_tuya_set_tz_minute()`, `pps_device_set_datetime()` |
| 门铃 | `nv_tuya_set_chan_doorbell_ring_exist()`, `nv_tuya_set_chan_chime_ring_tune()` |
| IPC绑定管理 | `nv_tuya_init_nvr_ipclist()`, `nv_tuya_scan_add_nvr_ipc()`, `nv_tuya_del_nvr_ipc()` |
| 设备信息 | `nv_tuya_get_nvr_pid()`, `nv_tuya_get_nvr_uuid()`, `nv_tuya_get_nvr_version()` |

---

## 6. 工具库层 (mod/util)

### pps_osal（OS 抽象层）

提供跨平台的操作系统原语封装，隐藏 POSIX/RTOS 差异：

| 头文件 | 功能 |
|---|---|
| `pps_osal_thread.h` | 线程创建/销毁 |
| `pps_osal_mutex.h` | 互斥锁 |
| `pps_osal_sem.h` | 信号量 |
| `pps_osal_msgq.h` | 消息队列 |
| `pps_osal_queue.h` | 通用队列 |
| `pps_osal_socket.h` | Socket封装 |
| `pps_osal_timer.h` | 定时器 |
| `pps_osal_mem.h` | 内存管理 |
| `pps_osal_log.h` | 日志 |
| `pps_osal_list.h` | 链表 |
| `pps_osal_time.h` | 时间 |
| `pps_osal_string.h` | 字符串 |
| `pps_osal_stdc.h` | 标准C扩展 |
| `pps_osal_type.h` | 类型定义 |

### pps_util（功能工具库）

| 模块 | 功能 |
|---|---|
| `pps_cJSON` | 轻量级 JSON 解析/生成 |
| `pps_mongoose` | 嵌入式 HTTP 服务器（5353行，核心）|
| `pps_md5` | MD5 哈希 |
| `pps_sha1` / `pps_hmac_sha1` | SHA1 / HMAC-SHA1 |
| `pps_base64` | Base64 编码/解码 |
| `pps_aes` | AES 对称加密 |
| `pps_des3` / `pps_d3des` | 3DES 加密 |
| `pps_xxtea` / `pps_xxtea_file` | XXTEA 加密 |
| `pps_crypt` | 加密综合接口 |
| `pps_ssl` | SSL 包装（mbedTLS） |
| `pps_requests` | HTTP 请求封装（GET/POST）|
| `pps_http_file` | HTTP 分块文件下载 |
| `pps_http_msg` | HTTP 消息解析 |
| `pps_netconfig` | 网络配置（WiFi/有线）|
| `pps_net` / `pps_net_base_operate` | 底层网络操作 |
| `pps_uri` | URI 解析 |
| `pps_curl` | libcurl 封装（可选）|
| `pps_cbuf` / `pps_circbuf` | 环形缓冲区 |
| `pps_mqueue` | POSIX 消息队列封装 |
| `pps_workqueue` | 工作队列（异步任务）|
| `pps_eventhub` | 事件总线 |
| `pps_cmd_cli` | 命令行接口 |
| `pps_slab` | Slab 内存分配器 |
| `pps_banner` | 启动 Banner 显示 |
| `pps_uptime` | 设备运行时长 |
| `pps_cpu_mem_usage` | CPU/内存使用率监控 |
| `pps_firmware_download` | 固件下载 |
| `pps_factory_check_info` | 产测信息 |
| `pps_common_util` | 通用工具函数集合 |
| `pps_iwlib` / `pps_iwscan` / `wireless` | 无线网络扫描 |
| `pps_getifaddrs` | 网络接口信息 |
| `pps_ipaddr` / `pps_netif_status` | IP 地址 / 网卡状态 |
| `pps_stdout_ctl` | 标准输出控制 |
| `pps_gunzip` | GZIP 解压 |
| `pps_c911` | 911 功能（未知具体用途）|
| `pps_lstLib` | 链表库（VxWorks风格）|
| `pps_checksum` | 校验和计算 |

---

## 7. 多平台支持与编译系统

### 支持的硬件平台

| 平台标识 | 芯片 | 架构 | 典型产品 |
|---|---|---|---|
| `INGENIC_A1` / `mipsel_a1x` | Ingenic T31/A1 | MIPS 小端 | 美瑞NVR A1系列 |
| `INGENIC_A1NT` / `mipsel_a1nt` | Ingenic A1NT | MIPS 小端 | 美瑞NVR A1NT系列 |
| `INGENIC_T41` / `t41` | Ingenic T41 | MIPS 小端 | 美瑞NVR T41系列 |
| `sigmastar` / `ss661q` | Sigmastar ss661q | ARM | 美瑞NVR SS系列 |
| `MTK7628` | MediaTek MT7628 | MIPS 小端 | 美瑞NVR MTK系列 |
| `x86` | x86 | x86_64 | 开发调试 |

### 编译系统

**顶层 Makefile 变量（由 config 脚本注入）：**

```makefile
NV_CPU_ARCH    # 平台标识（如 INGENIC_A1、sigmastar）
NV_CC          # C 编译器（交叉编译工具链）
NV_CXX         # C++ 编译器
NV_AR          # 静态库打包工具
NV_STRIP       # 符号剥离工具
NV_CFLAGS      # 编译选项
NV_LDFLAGS     # 链接选项
NV_LIBDIR      # 发布库目录
NV_INCDIR      # 发布头文件目录
NV_BINDIR      # 发布二进制目录
```

**编译目标：**
```
make all → 编译 mod/ → 输出
    nvr-mearicloud/build/{ARCH}/meari_nvr    (可执行二进制)
    nvr-mearicloud/lib/{ARCH}/libmeari_nvr.a (静态库，供base层调用)
```

**自动编译脚本 `auto_build.sh`：** 依次 source 各平台 config 脚本，执行 `make clean && make all`，批量输出所有平台产物。

**Git 信息注入（编译时追踪）：**
```makefile
CFLAGS += -DGIT_BRANCH=\"...\"
CFLAGS += -DGIT_TIME=\"...\"
CFLAGS += -DGIT_COMMIT=\"...\"
CFLAGS += -DGIT_AUTHOR=\"...\"
CFLAGS += -DGIT_DIRTY=\"...\"
```
启动时打印：`nvr-meariclound Build:2021-11-08_main_a1b2c3d P by zengyaowu`

---

## 8. 关键数据流图

### 8.1 启动初始化流程

```
┌─────────────────────────────────────────────────────┐
│ main()                                              │
│                                                     │
│  set_media_info()                                   │
│       ↓                                             │
│  nv_tuya_wait_nvr_start()  ← 等待 NVR base 就绪    │
│       ↓                                             │
│  nv_tuya_init_share()      ← 初始化共享内存RPC      │
│       ↓                                             │
│  nv_tuya_init_nvr_ipclist() ← 加载IPC通道列表      │
│       ↓                                             │
│  nv_tuya_start_preview()   ← 初始化视频缓冲区      │
│  nv_tuya_start_speak()     ← 初始化音频缓冲区      │
│       ↓                                             │
│  pps_device_board_init()   ← 读取licenceid/key     │
│       ↓                                             │
│  restful_webserver_init()  ← 启动本地HTTP服务      │
│       ↓                                             │
│  ┌─── while(1) ────────────────────────────────┐   │
│  │  pps_device_get_token()                      │   │
│  │       ↓                                      │   │
│  │  pps_register_init()  → HTTPS → Meari云     │   │
│  │       ↓ 成功                                 │   │
│  │  mnsdk_init()         → 建立云端长连接       │   │
│  │       ↓ 成功 → break                         │   │
│  │  失败 → sleep(5) → retry                     │   │
│  └──────────────────────────────────────────────┘   │
│       ↓                                             │
│  nvr_init_dp_data()       ← 同步设备属性到云端     │
│       ↓                                             │
│  创建3个后台线程                                    │
│  ┌─ check_channel_thread  ← 检测IPC连接状态        │
│  ├─ motion_detect_thread  ← 告警事件监听           │
│  └─ online_status_thread  ← APP在线状态检测        │
│       ↓                                             │
│  while(1) sleep(60)        ← 主线程守护             │
└─────────────────────────────────────────────────────┘
```

### 8.2 实时预览数据流

```
┌────────────┐    RPC(共享内存)    ┌─────────────────┐
│  IPC摄像头  │ ─────────────────→ │   NVR base      │
│ (H264/H265 │                    │ 环形缓冲区       │
│  G711A/PCM)│                    │ nvFsFrame       │
└────────────┘                    └────────┬────────┘
                                           │ 读帧
                                  ┌────────▼────────┐
                                  │ thread_live_    │
                                  │ stream()        │
                                  │ (每路一个线程)   │
                                  │  视频帧转换      │
                                  │  音频帧转换      │
                                  └────────┬────────┘
                                           │ mnsdk_stream_send()
                                  ┌────────▼────────┐
                                  │   libmnsdk.a    │
                                  │  (WebRTC P2P)   │
                                  └────────┬────────┘
                                           │ P2P / TURN中继
                                  ┌────────▼────────┐
                                  │   Meari云服务器  │
                                  └────────┬────────┘
                                           │
                                  ┌────────▼────────┐
                                  │   手机APP        │
                                  │  实时预览画面    │
                                  └─────────────────┘
```

### 8.3 告警推送数据流

```
┌────────────┐  事件消息队列  ┌──────────────────┐
│  IPC摄像头  │ ─────────────→│  thread_md_proc  │
│ (移动侦测   │               │  (告警侦测线程)   │
│  AI人形等)  │               └────────┬─────────┘
└────────────┘                        │
              ┌───────────────────────┘
              │ nv_meari_alarm_do()
              │  1. 判断告警类型
              │  2. 构造 params JSON
              │  3. 附带 JPEG 快照
              ▼
      mnsdk_event_publish_request(
          "alarm",
          params,          ← {"type":N,"channel":N,...}
          jpeg_data,       ← 报警截图(最大100KB)
          len
      )
              │
              ▼ HTTPS / MQTT
      Meari 云平台
              │
              ▼ APNs/GCM
      手机APP 推送通知
      (含缩略图预览)
```

### 8.4 回放控制流程

```
手机APP
  │ 发送回放命令 (binary data)
  ▼
mnsdk 回调: cmd_data_received_callback(channel, sid, data, length, params)
  │
  │ 解析 playback_info_t
  ├─ command = SEARCH_BY_DAY / SEARCH_BY_MONTH
  │     ↓
  │   playback_search_request()
  │     → nv_tuya_get_rec_bymonth() [RPC] → NVR文件系统
  │     → 构造录像列表JSON
  │     → mnsdk_data_send() → APP 显示录像列表
  │
  └─ command = PLAY_START
        ↓
      playback_control_request()
        → add_play_id(channel, sid)     ← 分配会话槽位
        → 启动回放推流线程
            从NVR文件系统读帧
            → mnsdk_stream_send(PLAYBACK, ...) → APP播放
              ↑ reserved[0]==1 时不限速推流
```

### 8.5 DP配置下发流程

```
手机APP（修改摄像头设置）
  │
  ▼ 云端下发
mnsdk 回调: config_request_received(sid, contact, json_content, len)
  │
  ▼
pps_iot_config_request()
  │
  ├─ 解析 JSON（pps_cJSON）
  │
  ├─ 路由 DP ID：
  │   "54" (时区)    → nv_tuya_set_tz_minute() [RPC]
  │   "141"(AI侦测)  → nv_tuya_set_chan_ai_dect_onoff() [RPC]
  │   "reboot"       → nv_tuya_restart() [RPC]
  │   摄像头翻转     → nv_tuya_set_chan_flip_onoff() [RPC]
  │   日夜模式       → nv_tuya_set_chan_daynight_mode() [RPC]
  │   ... (100+ 种DP)
  │
  └─ mnsdk_config_response(sid, contact, result_json, len) → 响应APP
```

---

## 9. 线程模型

应用程序运行时的线程全景：

```
主线程 (main)
│
├─ check_channel_thread    [后台] 轮询IPC通道连接状态，通知云端子设备上下线
│
├─ motion_detect_thread    [后台] 监听告警事件队列，触发告警推送
│
├─ online_status_thread    [后台] 轮询云端网关URL，检测APP在线状态
│
├─ HTTP服务线程             [后台] Mongoose HTTP服务器（本地80端口）
│
├─ live_main_0~N 线程      [动态] 每个(channel,用户)的主码流推流线程
│                                  在 APP 发起预览请求时创建，结束时销毁
│
└─ live_sub_0~N 线程       [动态] 辅码流推流线程（同上）
```

**并发控制：**
- `g_play_lock`（pthread_mutex）：保护回放会话ID分配
- `g_app_init_lock`（pthread_mutex）：保护初始化状态标志
- `g_upgrading_mutex`：防止并发OTA升级
- 预览线程通过 `pParam->running` 标志协调优雅停止
- 信号量 `g_sem_wakeup_ipc[]`：低功耗相机唤醒同步

---

## 10. 安全机制

| 机制 | 位置 | 说明 |
|---|---|---|
| HTTPS/TLS | `nvr_register.c`、`pps_ssl` | 使用 mbedTLS 进行云端注册通信加密 |
| HMAC-SHA1 签名 | `nvr_register.c` | 接口请求签名防篡改 |
| MD5 摘要 | `nvr_register.c` | 辅助签名计算 |
| AES 加密 | `pps_aes` | 媒体或配置数据加密（按需使用）|
| 3DES / XXTEA | `pps_des3`, `pps_xxtea` | 旧版兼容加密算法 |
| Basic Auth | `nvr_restful_server.c` | 本地HTTP接口保护 |
| Token 文件 | `/home/cfg/device.token` | 存储云端会话令牌 |
| 访问密码文件 | `/home/cfg/access_user.password` | 本地访问凭据 |
| AuthCode 硬编码 | `main.c` | SDK授权码（base64编码，320字节）|
| licenceid/secretkey | 硬件存储 | 设备唯一身份标识，出厂烧录 |
| SRTP | `libsrtp2.a` | WebRTC 媒体流加密传输 |

---

## 11. 关键常量与配置参数汇总

| 常量 | 值 | 说明 |
|---|---|---|
| `DEMO_NVR_SUB_DEV_NUM` | `nv_tuya_get_channums()` | 动态获取IPC通道数 |
| `NVR_P2P_USER_NUM` | 36 | 最大并发P2P用户数 |
| `NVR_HD_INFO_NUM` | 8 | 最大硬盘数量 |
| `DEVICE_TOKEN_FILE` | `/home/cfg/device.token` | Token持久化路径 |
| `ACCESS_PASSWORD_FILE` | `/home/cfg/access_user.password` | 本地密码文件路径 |
| `DEVICE_TZ_CONFIG` | `/etc/TZ` | 时区配置文件 |
| `DEVICE_SSR621Q_WAN_NETWORK` | `eth0` | Sigmastar平台WAN口 |
| `DEVICE_MTK7628_WAN_NETWORK` | `eth2.2` | MTK平台WAN口（VLAN）|
| `QR_TOKEN` | `"nvr"` | 二维码配网Token前缀 |
| `SIZE_BUFF` | 1024 | 通用缓冲区大小 |
| 告警图片大小上限 | 100 KB | 超过此大小不推送 |
| 告警推送最小间隔 | 60秒 | 防止频繁推送 |

---

## 12. 依赖的外部预编译库

所有库均为静态库（`.a`），按平台分目录存放在 `lib/`：

| 库文件 | 功能 | 开放源码？ |
|---|---|---|
| `libmnsdk.a` | Meari 云端网络SDK（核心）| ❌ 闭源 |
| `libmearisdk.a` | Meari SDK 扩展层 | ❌ 闭源 |
| `libmearisdk_mts.a` | Meari SDK MTS变体 | ❌ 闭源 |
| `libmbedtls.a` | TLS/SSL 加密库 | ✅ 开源（Apache 2.0）|
| `libsrtp2.a` | SRTP 媒体加密 | ✅ 开源（BSD）|
| `libWebrtcClient.a` | WebRTC 客户端 | ❌ 定制闭源 |
| `libWebrtcUtils.a` | WebRTC 工具库 | ❌ 定制闭源 |
| `nvbaserpc`（通过 `NV_LIBDIR`）| NVR基础系统RPC | ❌ 闭源 |
| `nvrecord`（通过 `NV_INCDIR`）| 录像文件系统 | ❌ 闭源 |

> **关键依赖链：** 本仓库的核心功能高度依赖 `libmnsdk.a` 和底层 `nvbaserpc`，这两个库均不开源，构成了 Meari 平台的核心商业保护屏障。

---

## 13. 代码质量与特点观察

**正面：**
- 源文件均有完整的文件头注释（版权、作者、日期、版本）
- 关键数据结构和函数有详细中文注释
- 模块划分清晰，单一职责原则执行较好
- 编译时注入 Git 信息，便于版本追溯
- 有详细的错误日志（`NV_LOG`宏，支持级别：TRACE/INFO/WARN/ERROR）

**值得关注：**
- `nvr_mearicloud` 中设备类型硬编码为 `MNSDK_DEVICE_TYPE_IPC`（0）而非 NVR 类型（3），代码注释标记为 `//for IPC`，与仓库定位不符，可能是遗留问题
- `main.c` 末尾存在测试代码（`test_playback_search`、`test_palyback_control`）在 while(1) 守护循环之后，实际上永远不会执行
- `nvr_discovery.c` 文件保留但功能已移至 base 层，形成死代码
- 头文件名 `nv_meari_preveiw.h` 存在拼写错误（`preveiw` → `preview`）
- SDK AuthCode 以明文 base64 字符串硬编码在 `main.c` 中

---

## 14. 总结

`nvr-mearicloud` 是一个**典型的嵌入式物联网设备云端接入中间件**，架构清晰、功能完整。其核心设计哲学是：

1. **将硬件抽象彻底下推给 NVR base 层**，本层只做业务逻辑和云端协议对接。
2. **将云端通信能力封装在闭源 SDK（libmnsdk）中**，本层只定义回调和调用 API，不关心 P2P/WebRTC/MQTT 细节。
3. **通过 RPC 解耦进程依赖**，使云端模块可以独立部署、升级而不影响底层 NVR 系统。

这种分层设计使得美瑞可以复用同一套云端接入代码，快速适配不同芯片平台（Ingenic/Sigmastar/MTK）的 NVR 产品，是嵌入式物联网产品线快速扩张的常见架构模式。

```
┌──────────────────────────────────────────────────────────┐
│                   核心价值链条                             │
│                                                          │
│  硬件摄像头 → NVR base(闭源) → nvr-mearicloud(本仓库)   │
│                                    ↓                     │
│                              libmnsdk(闭源)              │
│                                    ↓                     │
│                            Meari云平台(SaaS)             │
│                                    ↓                     │
│                            手机APP (C端)                 │
└──────────────────────────────────────────────────────────┘
```

---

*报告生成时间：2026-02-28 | 基于仓库 nvr-mearicloud 全部源文件分析*
