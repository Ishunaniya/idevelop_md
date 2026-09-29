# camera-app 仓库源码深度分析报告

> **作者**：AI 代码审计  
> **版本**：V5.6.6  
> **公司**：杭州美旅科技有限公司（Hangzhou Meari Technology Co., Ltd.）  
> **分析时间**：2026-03-06  
> **代码量**：约 41 万行代码，3101 个源文件（.c / .h / .cpp）

---

## 目录

1. [仓库概述](#1-仓库概述)
2. [整体架构](#2-整体架构)
3. [目录结构详解](#3-目录结构详解)
4. [各模块功能详析](#4-各模块功能详析)
5. [启动流程图](#5-启动流程图)
6. [数据流与通信架构](#6-数据流与通信架构)
7. [多平台与多SDK支持](#7-多平台与多sdk支持)
8. [安全漏洞分析](#8-安全漏洞分析)
9. [代码质量评估](#9-代码质量评估)
10. [总结与建议](#10-总结与建议)

---

## 1. 仓库概述

`camera-app` 是一款面向嵌入式 Linux 的 **IP 网络摄像头（IPC）固件应用程序**，由杭州美旅科技有限公司（品牌 Meari / PPStrong）开发，支持涂鸦（Tuya）和美旅自有云平台两套接入方案。固件运行于多种 MIPS/ARM SoC 平台之上，对外提供：

- 视频流推送（RTSP、RTMP、P2P）
- 云平台接入（涂鸦云、Meari 云、NKIT）
- ONVIF 标准协议
- HTTP REST API（80 端口）
- SD 卡本地录像
- PTZ 云台控制
- AI 功能（移动侦测 MD、人形检测 PD、宠物检测 PET、婴儿哭声检测 BCD）
- 蓝牙 / AP 配网
- OTA 在线升级

---

## 2. 整体架构

```
┌─────────────────────────────────────────────────────────┐
│                      应用层 (Application Layer)          │
│           meari_ipc / tuya_ipc (main entry, SDK胶水层)   │
├─────────────────────────────────────────────────────────┤
│                  云端接入层 (Cloud SDK Layer)             │
│    Tuya SDK (v4/v5)  │  Meari SDK  │  NKIT SDK          │
├──────────────────────┬──────────────────────────────────┤
│   协议服务组件层      │        device 功能层              │
│  (Components Layer)  │     (pps_device Layer)            │
│  ┌────────────────┐  │  ┌─────────┐  ┌──────────────┐  │
│  │ RTSP Server    │  │  │ 网络管理 │  │ 媒体管道控制  │  │
│  │ RTMP (SRS)     │  │  │ 升级OTA  │  │ PTZ云台      │  │
│  │ ONVIF v1/v2    │  │  │ 存储/录像│  │ 用户认证     │  │
│  │ REST API(HTTP) │  │  │ GPIO/LED │  │ 加密身份     │  │
│  │ Shell Server   │  │  │ 看门狗   │  │ 设备发现     │  │
│  │ Baby Monitor   │  │  │ 设备能力 │  │ ...          │  │
│  └────────────────┘  │  └─────────┘  └──────────────┘  │
├──────────────────────┴──────────────────────────────────┤
│                   公共基础层 (Common Layer)               │
│    EventHub(事件总线) │ DNS │ Wi-Fi扫描 │ 时间工具        │
├─────────────────────────────────────────────────────────┤
│                    HAL 硬件抽象层                         │
│    HiSilicon himix100  │  SigmaStar ssc333  │  x86(测试)  │
└─────────────────────────────────────────────────────────┘
           ↕ 硬件驱动（Kernel / Driver，不含于本仓库）
```

**核心设计思想**：通过 HAL 屏蔽底层差异，通过条件编译（`#ifdef CONFIG_TUYA_SUPPORT` / `CONFIG_MEARI_IPC_SDK` 等）在同一套代码中支持多种云平台与多种 SoC，通过插件表（`ppsdev_init_plugins_t[]`）驱动模块化启动。

---

## 3. 目录结构详解

```
camera-app/
├── README.md                   # 架构简介
├── 更新要点.md                 # 版本变更记录（V5.6.x 系列）
├── all.sh / auto.sh            # 多平台统一构建脚本
├── version/                    # 版本管理
│   ├── version                 # 主版本号：5.6.6
│   ├── gen_version.sh          # 版本号生成脚本
│   └── gen_git_info.sh         # Git commit 注入脚本
├── include/                    # 第三方/公共头文件
│   ├── curl/                   # libcurl HTTP 客户端
│   ├── zbar/                   # 二维码解码库
│   ├── onvif/                  # ONVIF 协议定义
│   ├── osal/                   # OS 抽象层（线程/内存/日志）
│   ├── util/                   # 通用工具（JSON/Base64/MD5/AES等）
│   ├── storage/                # 存储相关头文件
│   ├── pps_error.h             # 统一错误码定义
│   └── wpa_ctrl.h              # wpa_supplicant 控制接口
├── common/                     # 公共基础模块
│   ├── core/
│   │   ├── pps_eventhub.h      # 事件总线（发布/订阅）
│   │   ├── pps_rfctime.c/h     # RFC 时间格式处理
│   ├── net/
│   │   ├── iwscan.c/h          # Wi-Fi AP 扫描
│   │   ├── pps_dns.c/h         # DNS 并发解析
│   │   └── pps_net2.c/h        # 网络基础操作
├── pps_hal/                    # 硬件抽象层
│   ├── hwl_config.h            # 全局路径与配置宏定义
│   ├── arm-himix100-linux/     # 海思 HiSilicon HAL
│   └── ssc333/                 # 星宸 SigmaStar HAL
├── pps_device/                 # 设备功能核心层（最大模块）
│   ├── pps_device_board.c/h    # 硬件板级信息（型号/MAC/SN等）
│   ├── pps_device_encryption.c # 设备身份加密（flash 分区读写）
│   ├── pps_device_netmng.c     # 网络管理（Wi-Fi/AP/以太网）
│   ├── pps_device_media.c      # 媒体流管理（4.x API）
│   ├── pps_device_media_5.x.c  # 媒体流管理（5.x API）
│   ├── pps_device_ptz.c        # PTZ 云台控制（8342行，最大单文件）
│   ├── pps_device_upgrade.c    # OTA 固件升级
│   ├── pps_device_user.c       # 用户认证管理
│   ├── pps_device_watchdog.c   # 软件/硬件看门狗
│   ├── pps_device_sdcard.c     # SD 卡管理
│   ├── pps_device_discovery.c  # 设备局域网发现（ONVIF WS-Discovery）
│   ├── pps_device_capa.c       # 设备能力集（支持 252+ 型号）
│   ├── pps_device_led_ctrl.c   # LED 状态灯控制
│   ├── pps_device_gpio.c       # GPIO 事件（按键/PIR/电机）
│   ├── pps_device_rgb_light.c  # RGB 彩色灯控制
│   ├── pps_device_white_light.c# 白光灯控制
│   ├── pps_device_uart.c       # UART 通信
│   ├── pps_device_temp_humidity.c # 温湿度传感器
│   ├── pps_device_battery.c    # 电池管理
│   ├── pps_device_sleeping.c   # 休眠/离家模式
│   ├── pps_device_qrcode.c     # 二维码扫描（配网）
│   ├── pps_device_music.c      # 音乐播放
│   ├── pps_device_siren.c      # 报警声光控制
│   ├── pps_device_miniz.c/h    # zlib 压缩（miniz 嵌入实现，约380K）
│   ├── pps_device_watermark.c  # 视频水印/OSD
│   ├── pps_device_charm.c      # 门铃模块
│   ├── pps_wpa_cli.c           # wpa_supplicant CLI 接口
│   ├── pps_sdcard/             # SD 卡文件系统工具
│   │   ├── pps_sdcard.c        # SD 卡挂载/格式化
│   │   ├── pps_disk_fs.c       # 磁盘文件系统操作
│   │   └── pps_fsck/           # FAT32 文件系统检查（自实现 fsck）
│   └── pps_product/            # 产测初始化
├── components/                 # 功能组件层
│   ├── pps_rtsp/               # RTSP 服务器（端口 8554）
│   ├── pps_rtmp/               # RTMP 推流（SRS librtmp）
│   ├── pps_onvif/              # ONVIF v1 / v2 实现
│   ├── restful_server/         # HTTP REST API（Mongoose 框架，80端口）
│   ├── pps_shell/              # Unix 域套接字命令服务器
│   ├── pps_storage/            # 录像控制与 MP4 封装
│   ├── pps_record3/            # 录像引擎 v3（MP4 muxer/demuxer）
│   ├── pps_baby_module/        # 婴儿监护专用协议
│   ├── pps_nkit_module/        # NKIT 第三方云 SDK 适配
│   ├── pps_nvr_module/         # NVR 接入模块
│   ├── pps_local_music/        # 本地音乐播放器
│   ├── pps_conf_sync/          # 配置同步
│   └── prodtest/               # 产线测试模块
├── ipc_app/                    # 云平台应用层（最终可执行程序）
│   ├── meari_ipc/              # 美旅云平台应用
│   │   └── main/               # 主程序（ppsapp.c 入口）
│   └── tuya_ipc/               # 涂鸦云平台应用
│       ├── tuya_app/           # 涂鸦应用主体（DP处理/事件/媒体）
│       ├── tuya_sdk_a2/        # 涂鸦 SDK a2 平台包
│       ├── tuya_sdk_a4/        # 涂鸦 SDK a4 平台包
│       ├── tuya_sdk_c5/c7/c9/  # 涂鸦 SDK cx 系列平台包
│       └── tuya_sdk_*_5_x_sdk/ # 涂鸦 5.x SDK 包
├── tools/                      # 工具程序
│   ├── cmd_srv/                # 命令服务
│   ├── discovery/              # 局域网设备发现工具
│   ├── longsung_cm/            # 龙盛模块管理
│   ├── lookup_proc/            # 进程查找工具
│   ├── ppsconfig/              # 配置读写工具
│   ├── tpconfig/               # TP 产品参数配置
│   └── unpack/                 # 固件解包工具
└── doc/                        # 项目文档
    ├── 设备WEB API.doc          # HTTP API 文档
    ├── 硬件抽象层接口文档.docx   # HAL 接口规范
    ├── 设备外围IO驱动接口文档    # GPIO/外设驱动接口
    └── pps_device对接指南.md    # SDK 集成指南
```

---

## 4. 各模块功能详析

### 4.1 pps_device（核心设备层）

该层是整个项目的"心脏"，是规模最大、功能最全的模块。

#### 4.1.1 pps_device_board（板级管理）
- 从 Flash MTD 分区读取设备身份信息（`BOOT_PARAMS` 结构体，256字节）
- 信息包括：设备型号、MAC 地址（最多 2 个）、生产日期、序列号、OEM 代码、P2P ID、许可证、加密版本号等
- 初始化以太网（`eth0`）MAC 地址并 `ifconfig eth0 up`
- 初始化 WiFi 模块驱动（通过 `rmmod`/`insmod` 命令加载不同 WiFi 驱动）
- 支持的 WiFi 模块：RTL8188FU/8192FU/8733BU、ATBM603x/606x、高拓 6062 等

#### 4.1.2 pps_device_encryption（设备身份加密）
- 定义并解析 Flash 中的 `BOOT_PARAMS`（设备元信息）和 `EXT_PARAM`（扩展信息）
- 支持多种加密样式：TUTK_STYLE、LICENSE_ONLY_STYLE、TUYA2_STYLE、IDENTITY_STYLE 等
- 读取设备的 P2P ID、MAC、序列号、License Key、LDS 证书/密钥等
- 使用 XXTEA 算法对部分配置进行加密存储

#### 4.1.3 pps_device_netmng（网络管理）
这是仅次于 ptz 的第二大模块（约 97K），负责全部网络操作：
- Wi-Fi STA 模式（连接路由器）：调用 `wpa_supplicant` / `wpa_cli`
- AP 模式（热点配网）：调用 `hostapd` + `udhcpd`
- 蓝牙配网支持（通过 `pps_bt` 模块）
- 多 Wi-Fi 切换（最多支持 2 个 Wi-Fi 配置）
- 5GHz/2.4GHz 双频支持，含频段白名单配置（按国家/地区）
- 以太网（`eth0`）支持（条件编译 `PPS_NET_MANAGER_SUPPORT_ETH`）
- Ping 连通性检测、DNS 并发解析
- Wi-Fi 信号强度定期上报（间隔 1 天）

#### 4.1.4 pps_device_ptz（PTZ 云台控制）
- 规模最大的单文件（8342 行），负责云台的全部控制逻辑
- 支持步进电机（Step Motor）控制：上/下/左/右/全方位停止
- 巡视功能：一键巡视、定时巡视、噪音触发巡视
- 人形追踪（People Track）联动控制
- 极限位置检测与保护
- 预置点（Preset Point）管理

#### 4.1.5 pps_device_upgrade（OTA 升级）
- 支持 URL 升级（HTTP/HTTPS 下载固件包）
- 支持 SD 卡本地升级（扫描 `/mnt/mmc01/` 查找升级包）
- 升级流程：下载 → 写入 Flash → 校验 → 重启
- 固件包格式验证：检查 magic number (`0x5354524e`) 和基础 checksum
- 升级前卸载内核模块（音频、视频编码、ISP等）

#### 4.1.6 pps_device_user（用户认证）
- 管理最多 4 个账户（索引 0-3）
- 账户信息以静态全局变量方式存储（内存中）
- 密码使用简单字符偏移"加密"（非真正加密，详见安全章节）

#### 4.1.7 pps_device_watchdog（看门狗）
- 硬件看门狗：调用 HAL 接口 `PPS_HAL_WATCH_DOG_Ctrl`
- 软件看门狗：监控多个模块的"喂狗"心跳值
  - 监控项：CPU高负载、媒体流延迟、RESTful 服务器超时、SD卡超时、网络超时、快照超时等
- 自动重启：默认每 7 天定时重启，支持按平台差异化配置

#### 4.1.8 pps_sdcard / pps_fsck（SD 卡管理）
- 支持 FAT32 文件系统的挂载、格式化、fsck 检查（自实现）
- 容量支持扩展至 256GB
- fsck 在挂载前检查 FAT32 文件系统完整性
- 录像文件按日期目录分类存储

---

### 4.2 Components（服务组件层）

#### 4.2.1 RTSP 服务器（pps_rtsp）
- 监听端口：`8554`
- 支持主码流（Main）、子码流（Sub）、回放流（Playback）、语音对讲流
- 基于 Digest 认证（MD5）
- 检测客户端类型：VLC、ONVIF、NVR、FFMPEG、海思微型设备
- 支持 TCP/UDP 两种 RTP 传输模式

#### 4.2.2 RTMP 推流（pps_rtmp）
- 使用 SRS（Simple Realtime Server）librtmp 库
- 用于云录像和直播流推送

#### 4.2.3 ONVIF（pps_onvif）
- 实现 ONVIF v1（端口 8000）和 v2
- WS-Discovery 基于 UDP 组播（239.255.255.250:3702）和广播（255.255.255.255:3703）
- 支持：获取设备信息、媒体配置、PTZ 控制、事件订阅、重启/恢复出厂
- 断外网后 ONVIF 仍可本地运行（v5.6.5 修复）

#### 4.2.4 RESTful HTTP 服务器（restful_server）
- 基于 Mongoose 嵌入式 HTTP 框架
- 监听端口：`80`
- 提供 JSON 格式的 REST API
- 涵盖功能：设备信息、媒体控制、录像查询、网络配置、PTZ、OTA 升级等
- 认证方式：自定义 MD5 摘要认证

#### 4.2.5 Shell 服务器（pps_shell）
- 基于 Unix 域套接字（`/var/shell.socket`）
- 提供设备本地调试命令接口（`ppstool` 客户端）
- 支持命令：iqtool（图像质量调试）、pd（人形检测）、log_level、factory（恢复出厂）、track（人形追踪）、dog（看门狗）、ptz、bitrate、save_audio_video 等

#### 4.2.6 录像引擎（pps_record3 / pps_storage）
- 自实现 MP4 muxer/demuxer（支持 H.264/H.265 + AAC/G.711）
- 支持 MP4 文件修复（异常断电后）
- 录像计划管理（`pps_schedule`）
- 缩略图生成（`pps_record_thumbnail`）
- 回放流管理

#### 4.2.7 Baby Monitor（pps_baby_module）
- 专用私有协议（TCP）
- 客户端/服务端双向通信
- AES 加密的数据传输（`pps_aes.c`）
- 管道（pipe）数据传输机制

---

### 4.3 ipc_app（应用层）

#### 4.3.1 meari_ipc（美旅云应用）
- 主入口：`ppsapp.c` 的 `main()` 函数
- 采用**插件表驱动**的三阶段启动（core → user → after）
- 所有模块以结构体 `ppsdev_init_plugins_t` 注册，表明是否需要独立线程、优先级、栈大小
- 集成美旅 SDK（`mp_ipc_api.h`）进行云端 DP 数据点上报

#### 4.3.2 tuya_ipc（涂鸦云应用）
- 集成涂鸦 IPC SDK（v4.x/v5.x）
- DP 数据点管理（`pps_dp.c`）：设备状态与云端双向同步
- 事件上报（`pps_tuya_ipc_event.c`）：移动侦测、人形检测、门铃等
- 支持涂鸦网关模式（`tuya_gw_sdk`）
- 支持多平台 SDK 包（a2/a4/c302b/c5/c7/c9/c9-5x-bt）

---

## 5. 启动流程图

```
main()
  │
  ├── Phase 1: core（同步顺序执行）
  │     ├── pps_log_redirect_init()    # 日志重定向（DEBUG 模式）
  │     ├── pps_log_report_init()      # 日志上报到云端
  │     ├── pps_device_system_init()   # 内核版本通知、基础系统初始化
  │     ├── pps_register_signal_handlers() # 注册 SIGSEGV/SIGTERM 等信号处理
  │     ├── pps_watchdog_init()        [线程] 硬件/软件看门狗启动
  │     ├── pps_startup_log_init()     [线程] 启动日志记录
  │     ├── pps_device_board_init()    # 从 Flash 读取板级信息（型号/MAC/SN）
  │     ├── pps_device_user_info_init()# 初始化默认用户账户
  │     ├── pps_settings_init()        # 加载持久化配置
  │     ├── pps_white_light_init()     # 白光灯模块
  │     ├── pps_ptz_service_init()     [线程] PTZ 云台服务
  │     ├── pps_media_service_init()   # 媒体管道初始化（ISP/编码器/音频）
  │     ├── pps_watermark_loop()       [线程] 视频水印/OSD
  │     ├── pps_gpio_event_init()      [线程] GPIO 事件（按键/PIR/电机）
  │     ├── pps_led_init()             # LED 状态灯
  │     ├── pps_product_test_init()    # 产线测试（产测固件）
  │     ├── pps_device_sdcard_init()   [线程] SD 卡检测/挂载/fsck
  │     ├── pps_device_recorder_init() # 录像引擎初始化
  │     ├── pps_device_storage_init()  [线程] 录像调度与管理
  │     └── shell_server_open()        # Unix socket shell 服务
  │
  ├── Phase 2: user（等待基础初始化完成后执行）
  │     ├── pps_netmng_service_start() [线程] 网络管理主循环
  │     ├── pps_ap_v2_start()          [线程] AP 热点配网服务
  │     ├── pps_nkit_sdk_init()        [线程] NKIT SDK（条件编译）
  │     ├── pps_device_siren_ctrl()    [线程] 报警声光控制
  │     ├── pps_rgb_color_light_service() RGB 灯服务
  │     ├── pps_onvif_cfg_init()       # ONVIF 配置
  │     ├── pps_onvif_start1/2()       [线程×2] ONVIF 服务（WS-Discovery）
  │     ├── pps_rtsp_server_init()     [线程] RTSP 服务器（8554端口）
  │     ├── restful_webserver_init()   [线程] HTTP REST 服务器（80端口）
  │     ├── pps_device_discovery_init()[线程] 局域网发现（UDP 3702/3703）
  │     ├── pps_device_sleeping_init() # 离家/休眠模式
  │     ├── pps_temp_humidity_init()   # 温湿度传感器
  │     ├── pps_device_battery_init()  # 电池管理
  │     └── pps_pcr_rotation_init()    [线程] 视频方向自适应
  │
  └── Phase 3: after（所有 user 服务就绪后）
        ├── pps_uart_zoom_start()      [线程] 变焦串口控制（ZOOM）
        ├── pps_common_bt_init()       [线程] 蓝牙初始化
        ├── pps_mp_sdk_init()          [线程] Meari Cloud SDK 连接
        ├── pps_check_p2p_usage()      [线程] P2P 带宽自适应码率
        └── music_player_init_music_list() [线程] 音乐列表初始化

        ↓ 所有阶段完成后 main() 进入 while(1) sleep 永久等待
```

---

## 6. 数据流与通信架构

### 6.1 视频数据流

```
传感器(Sensor)
    ↓ ISP 处理（媒体 HAL）
编码器(H.264/H.265)
    ↓ pps_media 媒体管道
    ├──→ RTSP Server (port 8554)    ──→ NVR / VLC / FFMPEG
    ├──→ RTMP 推流                  ──→ 云端直播
    ├──→ P2P (Tuya/Meari/TUTK)     ──→ 手机 App
    ├──→ SD 卡录像 (MP4)            ──→ 本地存储
    └──→ 截图 (JPEG)               ──→ 报警推送
```

### 6.2 事件总线（EventHub）

```
事件生产者                     EventHub（pub/sub）         事件消费者
────────────                  ─────────────────          ───────────
PIR 传感器    →  PPS_EV_MD  →                      →   录像触发
移动侦测 AI   →  PPS_EV_DB  →   事件路由分发       →   云推送报警
SD 卡插拔     →  PPS_EV_SDCARD_MOUNTED →           →   录像初始化
Wi-Fi 连接    →  PPS_EV_STA_CONNECTED →            →   云平台接入
电池状态      →  PPS_EV_BATTERY →                  →   低电告警
```

主要事件类型（`PPS_EV_TYPE_E`）：
- `PPS_EV_MD` - 移动侦测
- `PPS_EV_DB_ALARM` - 数据库告警
- `PPS_EV_BCD` - 婴儿哭声检测
- `PPS_EV_SDCARD_*` - SD 卡系列事件（挂载/拔出/格式化/满容）
- `PPS_EV_STA_*` - Wi-Fi 连接状态
- `PPS_EV_MQTT_CONNECT` - MQTT 连接状态

### 6.3 网络协议栈

| 协议 | 端口 | 方向 | 用途 |
|------|------|------|------|
| HTTP REST | 80 | 双向 | 设备配置、控制 |
| RTSP | 8554 | 双向 | 视频预览（局域网） |
| ONVIF HTTP | 8000 | 双向 | ONVIF 服务 |
| ONVIF WS-Discovery | UDP 3702 | 组播 | 局域网设备发现 |
| 局域网发现 | UDP 3703 | 广播 | 设备发现（专有协议）|
| RTMP | 1935 | 外发 | 云端推流 |
| MQTT | 1883/8883 | 双向 | IoT 消息（涂鸦/Meari）|
| P2P | 动态 | 双向 | 手机直连视频 |
| Unix Socket | `/var/shell.socket` | 本地 | 调试 Shell |

### 6.4 配置文件分布

| 路径 | 内容 |
|------|------|
| `/etc/device_tp.conf` | 设备 TP 参数 |
| `/etc/passwd` | 系统账户（覆盖写） |
| `/home/cfg/wifi.info` | Wi-Fi 明文配置（遗留格式） |
| `/home/cfg/wifi_enc.json` | Wi-Fi 加密配置（新格式） |
| `/home/cfg/dev_settings.json` | 设备设置持久化 |
| `/home/cfg/alarm.cfg` | 告警配置 |
| `/home/cfg/device.token` | 设备云端 token |
| `/home/cfg/tutk_meari.cfg` | P2P 快速配置（JSON） |
| `/proc/mtd` | MTD Flash 分区表 |

---

## 7. 多平台与多SDK支持

### 7.1 支持的 SoC 平台

| 平台代号 | SoC | 厂商 | 备注 |
|---------|-----|------|------|
| C4 | HiSilicon Hi3518EV201 | 海思 | 老平台 |
| C101 | HiSilicon EV300 | 海思 | 含 uboot SD 卡驱动优化 |
| A2 | AnyKa V200/V330 | 安凯 | anykaV33x |
| A3 | AnyKa AV100/AV100N | 安凯 | AV100 1.08 驱动 |
| A4 | AnyKa + CV4001/CV5001 | 安凯 | 500W 像素适配 |
| C302 | Ingenic T31 | 君正 | MIPS |
| C302b | Ingenic T31（变体） | 君正 | MIPS |
| C404/C405 | Ingenic T40/T41 | 君正 | uclibc |
| C5 | SigmaStar SSC333 | 星宸 | ARM |
| C7 | SigmaStar（新） | 星宸 | ARM |
| C9 | （新平台）| - | 含蓝牙版本 c9_5_x_bt |

### 7.2 支持的 Wi-Fi 模块

RTL8188FU、RTL8192FU、RTL8733BU、ATBM603x、ATBM606x、高拓 6062/6063、Hi3873（海思 Wi-Fi+BT）等共 **16+ 种** Wi-Fi 模块。

### 7.3 支持的设备型号

通过 `pps_device_capa.c` 统计，共支持 **252+ 种** 设备型号，涵盖：

- **Mini 系列**：Mini 1 ~ Mini 21S（室内半球型）
- **Speed 系列**：Speed 2 ~ Speed 27S（速球型）
- **Bullet 系列**：Bullet 4Q ~ Bullet 14Q（枪型）
- **Baby 系列**：Baby 3Q ~ Baby 12S（婴儿监护）
- **NVR 系列**：8路/16路 NVR

### 7.4 云平台 SDK 版本（V5.6.6）

| SDK | 版本 |
|-----|------|
| 涂鸦 SDK（老平台） | 4.9.18 |
| 涂鸦 SDK（新平台） | 5.3.22 |
| Meari SDK | B14418 |
| 媒体库 anykaV33x | 4.6.147 |
| 媒体库 君正T31 | 4.6.145 |
| 媒体库 SigmaStar | 4.6.58 |
| 媒体库 AV100 | 5.1.59 |
| 媒体库 T41 | 5.1.62 |
| 媒体库 多方 | 5.1.71 |
| SD/录像库 | 1.2 |

---

## 8. 安全漏洞分析

> ⚠️ 以下漏洞均基于源码静态分析，按严重程度排序。

### 🔴 高危（Critical）

#### 漏洞 1：调试固件硬编码 Wi-Fi 凭据

**位置**：`ipc_app/meari_ipc/main/ppsapp.c`

```c
#ifdef CONFIG_DEBUG_FIRMWARE
int main(int argc, char *argv[])
{
    pps_device_system_init();
    pps_net_wifi_connect("MeariAP", "56565099");  // ← 硬编码凭据
    sleep(10);
    return 0;
}
#endif
```

**风险**：若调试固件被意外部署到生产环境，设备将自动连接名为 `MeariAP` 的热点，密码为 `56565099`。攻击者只需创建同名热点即可让设备自动接入攻击者网络，实现流量劫持。

---

#### 漏洞 2：内置后门账户（密码仅字符偏移混淆）

**位置**：`pps_device/pps_device_user.c`

代码中静态定义了 **4 个账户**，其中：

- **账户 0（管理员）**：用户名 `admin`，密码：
  - 大多数分支：`admin`
  - `ARCH_THIRD_BRANCH`：`056565099`（与 Flash magic 数相同）
- **账户 1（内置后门）**：用户名 `PpStRoNg`，密码 `#%&wL1@*tU123zv`

所谓"加密"仅是对每个字符加减一个常量偏移（ASCII_SPACE=32, ASCII_SPACE2=31），这是**安全上毫无意义的混淆**，而非真正的加密。任何能读取二进制的人均可立即解码。

该后门账户 `PpStRoNg` 在所有使用此代码的设备上**永久存在且密码固定**，可用于 RTSP 认证、HTTP REST API 认证、ONVIF 认证。

---

#### 漏洞 3：OTA 升级无签名验证

**位置**：`pps_device/pps_device_upgrade.c`

固件升级仅校验魔数（`0x5354524e`）和简单校验和，**不验证密码学签名**：

```c
/* check checksum */
// 仅有基础 magic + length + checksum 校验，无 RSA/ECDSA 签名
```

URL 校验仅检查前缀：
```c
if (url == NULL || strncmp(url, "http", 4)) {
    PPS_ERROR("upgrade url is invalid\n");
}
```

`strncmp(url, "http", 4)` 同时允许 `http://` 和 `https://`，不强制 TLS。攻击者若能进行中间人攻击（MITM），可推送恶意固件，完全控制设备。

---

### 🟠 中危（High）

#### 漏洞 4：潜在命令注入

**位置**：`pps_device/pps_device_netmng.c`

```c
// 将 gateway 直接拼入 ping 命令
sprintf(cmdline, "ping %s -w 6", gateway);
pps_system_cmd(cmdline);

// 将 country_code 拼入写文件命令
sprintf(cmdline, "echo 'country=%s' >> /etc/wpa_supplicant.conf",
        this_board->country_code);
pps_system_cmd(cmdline);
```

若 `gateway` 或 `country_code` 可被外部输入控制（例如通过 HTTP API 提交），并且**未经充分过滤**，则存在 Shell 命令注入风险。例如，传入 `gateway = "8.8.8.8; rm -rf /home/cfg"` 将执行额外的 Shell 命令。

---

#### 漏洞 5：Wi-Fi 密码日志泄露

**位置**：`pps_device/pps_device_netmng.c`

```c
PPS_INFO("ssid: %s\n", g_wifi_relate.wifi_info_swi[0].wifi_info.ssid);
PPS_VERBOSE("psk: %s\n", g_wifi_relate.wifi_info_swi[0].wifi_info.psk);
```

在 VERBOSE 日志级别下，Wi-Fi 明文密码会被打印到日志。若设备日志上传到云端（`pps_log_report_init` 开启），则 Wi-Fi 密码存在泄露到服务器的风险。

---

#### 漏洞 6：不安全的密码存储

密码认证通过 `pps_user_cherck_verify_user_char()` 进行，仅做字符偏移"加密"后的字符串比较，**不使用哈希（如 bcrypt/PBKDF2）**，且比较方式容易受到时序攻击（`strcmp`）。

---

### 🟡 低危（Medium）

#### 漏洞 7：大量使用不安全的字符串函数

代码中广泛使用 `strcpy`、`sprintf`（无长度限制）：

```c
// pps_device_upgrade.c
strcpy(sd_up.filename, name);  // 无长度检查

// pps_device_netmng.c
strcpy(ifr.ifr_name, "eth0");
strcpy(wifi_info->ssid, old_info.ssid);
```

虽然现有调用点部分有长度保护，但在大规模 C 代码中使用 `strcpy`/`sprintf` 是潜在缓冲区溢出的来源，尤其在处理外部网络输入时。

---

#### 漏洞 8：EXT_CFG_MAGIC 与产测密码相同

Flash 配置魔数 `0x56565099` 与产线测试 Wi-Fi 默认密码 `56565099`（`pps_device_netmng.h` 注释中标注为 `FACTORY_DEFAULT_MODE`）完全相同，说明两者来自同一默认值，属于敏感设计信息泄露。

---

#### 漏洞 9：HTTP REST API 无速率限制

RESTful 服务器（80端口）基于 Mongoose 实现，代码中未见速率限制或失败计数锁定机制，存在对管理员密码的暴力破解风险。

---

### 漏洞汇总

| 编号 | 类型 | 位置 | 严重程度 |
|------|------|------|---------|
| V1 | 硬编码调试凭据 | `ppsapp.c` | 🔴 Critical |
| V2 | 内置后门账户 + 弱混淆 | `pps_device_user.c` | 🔴 Critical |
| V3 | OTA 无签名验证 + HTTP | `pps_device_upgrade.c` | 🔴 Critical |
| V4 | 命令注入风险 | `pps_device_netmng.c` | 🟠 High |
| V5 | Wi-Fi 密码日志泄露 | `pps_device_netmng.c` | 🟠 High |
| V6 | 明文/弱加密密码比对 | `pps_device_user.c` | 🟠 High |
| V7 | 不安全字符串函数 | 多处 | 🟡 Medium |
| V8 | 敏感常量信息复用 | `pps_device_netmng.h` | 🟡 Medium |
| V9 | REST API 无限频保护 | `restful_server.c` | 🟡 Medium |

---

## 9. 代码质量评估

### 9.1 优点

1. **分层架构清晰**：HAL → Common → Device → Components → App 五层分离，职责明确。
2. **插件化启动**：三阶段插件表驱动初始化，模块可插拔，便于裁剪。
3. **EventHub 解耦**：事件总线有效解耦生产者与消费者，避免模块间直接依赖。
4. **多平台统一**：条件编译管理多 SoC 差异，一套代码支持 10+ 平台，工程化水平较高。
5. **代码注释**：核心结构体（如 `BOOT_PARAMS`）有详细的字节偏移注释，便于维护。
6. **看门狗完善**：软硬件双重看门狗保障长期稳定运行，覆盖多个关键模块。
7. **SD 卡自检**：集成 FAT32 fsck，增强存储可靠性。

### 9.2 不足与技术债务

1. **最大单文件 PTZ 8342 行**：`pps_device_ptz.c` 严重过大，应拆分为运动控制、巡视、追踪等子模块。
2. **历史遗留文件**：存在 `pps_device_media_old.c`（老版本媒体接口）、`pps_device_charm_old.c`、`pps_device_ptz_old.c`、`pps_device_watermark_old.c` 等大量遗留文件未清理。
3. **全局状态滥用**：大量使用全局静态变量（如 `g_wifi_relate`、`g_pps_user`、`this_board`），线程安全依赖锁，但部分地方缺乏充分保护。
4. **不安全 C 函数**：`strcpy`、`sprintf`、`strcat` 遍布全局，应全面迁移到 `strncpy`、`snprintf`、`strncat`。
5. **编译警告被抑制**：CMake 中有 `-Wno-unused` 和 `-Wno-unused-result`，掩盖了实际的代码质量问题。
6. **缺乏单元测试**：`test/` 目录内容极少（仅有 media 和 mongoose 的少量测试），项目整体缺乏系统化测试。
7. **硬件耦合**：部分代码直接调用 `pps_system_cmd()`（即 `system()`）执行 Shell 命令，HAL 抽象不够彻底。

---

## 10. 总结与建议

### 仓库定位

`camera-app` 是一款**工程化程度较高的商业级 IPC 固件**，支持广泛的硬件平台和两套主流云平台，代码经历了多年迭代（从 V2.x 到 V5.6.x），具备完整的设备生命周期管理能力（配网、运行、升级、监控）。

### 安全整改建议

1. **立即移除内置后门账户** `PpStRoNg`，或至少在生产固件中禁用。
2. **删除调试固件中的硬编码 Wi-Fi 凭据**，改用安全的测试凭据注入机制。
3. **OTA 升级必须强制 HTTPS**，并引入 RSA/ECDSA 固件签名验证。
4. **用户密码改用安全哈希**（如 SHA-256 + salt 或 bcrypt），彻底废弃字符偏移混淆。
5. **命令拼接改用白名单校验**，或使用 `execve()` 替代 `system()`，避免命令注入。
6. **Wi-Fi 密码禁止打印到日志**，或仅在严格受控的调试版本中允许。
7. **REST API 增加登录失败计数和锁定机制**，防暴力破解。

### 架构演进建议

1. 将 `pps_device_ptz.c`（8342行）拆分重构。
2. 清理所有 `*_old.c` 遗留文件，纳入版本控制归档分支。
3. 建立覆盖网络管理、升级、认证模块的集成测试套件。
4. 推动关键模块（认证、升级、命令执行）的安全代码审查制度化。

---

*本报告内容均基于源码静态分析，所有描述以代码事实为依据。*
