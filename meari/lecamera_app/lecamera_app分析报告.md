# lecamera_app 源码分析与核验报告

> 初版分析日期：2026-03-06；本次核验日期：2026-10-04～2026-10-05
> 源码目录：`/home/tronlong/lyp/meari/lecamera_app`
> 文档目录：`/home/tronlong/lyp/perCode/idevelop_md/meari/lecamera_app`
> 快照规模：1364 个文件，其中 199 个 `.c`、1097 个 `.h`、3 个 `.cc`
> 核验方式：文件清点、关键业务调用链静态复核、Makefile 干运行、三个原函数的隔离主机核验。
> 流程图补充日期：2026-10-07；按源码修正原图节点，保留已有分析与核验记录。

本次核验以本机源码快照为准。该目录没有 Git 元数据，不能提供提交号；文件内容与相对路径的组合 SHA-256 为 `3b3f40da450f397b98e9d2bd7e10ee6934b9cd6ff783c1b7d36ce4e262982c86`，计算方法见 [核验脚本](evidence/verify_source.py)。统计覆盖整个文件树，深入复核集中在构建、启动、事件、网络、配置、升级、存储、回放和管理接口。SDK 内部、BSP、真实硬件行为及整机安全影响没有完成运行验证，不能把静态检查写成“全部文件均已深度分析”。

[核验记录](evidence/verification.json)保存统计、构建干运行结果、提取函数的哈希和主机核验输出。主机程序使用虚构账户与 Flash 桩，没有访问设备或生产凭据。

---

## 目录

1. [仓库概述](#1-仓库概述)
2. [整体架构](#2-整体架构)
3. [目录结构详解](#3-目录结构详解)
4. [硬件平台矩阵](#4-硬件平台矩阵)
5. [云平台 SDK 体系](#5-云平台-sdk-体系)
6. [核心模块分析](#6-核心模块分析)
7. [系统初始化流程](#7-系统初始化流程)
8. [事件驱动机制](#8-事件驱动机制)
9. [网络管理流程](#9-网络管理流程)
10. [固件升级流程](#10-固件升级流程)
11. [生产工具链](#11-生产工具链)
12. [安全与可靠性问题核验](#12-安全与可靠性问题核验)
13. [代码质量评估](#13-代码质量评估)
14. [总结与建议](#14-总结与建议)
15. [构建配置与依赖核验](#15-构建配置与依赖核验)
16. [低功耗唤醒与休眠闭环](#16-低功耗唤醒与休眠闭环)
17. [媒体录像与回放链路](#17-媒体录像与回放链路)
18. [配置持久化与管理接口](#18-配置持久化与管理接口)
19. [核验方法与后续验证](#19-核验方法与后续验证)

---

## 1. 仓库概述

`lecamera_app` 是 Meari Technology 的一套**低功耗（LPC）IoT 摄像机应用固件**工程。其核心目标是：

- 在资源受限的嵌入式 SoC（64M RAM / 8–16M Flash）上运行完整的摄像机应用
- 包含海思、君正、GK 的适配目录；README 和配置还声明 Sigmastar，当前构建支持有缺口
- 对接 Meari、Tuya、DOT SDK，以及 NKIT 套装通信
- 支持 Wi-Fi、4G、套装（Relay/NVR 搭配）等多种**网络接入形式**

主 Makefile 的目标名为 `ppsapp`，LiteOS 入口为 `app_init()`，Linux 入口为 `main()`。工程设计包含固件打包入口，但当前快照不包含各平台 `pack/`、预编译 `.a/.so` 或同级 LiteOS BSP，不能据此认定当前目录可独立编译、打包和部署。完整依赖核验见第 15 章。

产品定位和硬件参数来自源码 [README.md](/home/tronlong/lyp/meari/lecamera_app/README.md:1)，入口与链接规则见 [Makefile](/home/tronlong/lyp/meari/lecamera_app/Makefile:1)。

---

## 2. 整体架构

### 2.1 分层架构图

```
┌──────────────────────────────────────────────────────────┐
│                     Cloud SDK Layer                      │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────┐  │
│  │ Meari SDK│  │ Tuya SDK │  │  DOT SDK │  │  NKIT  │  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └───┬────┘  │
├───────┴──────────────┴──────────────┴────────────┴───────┤
│                   Application Layer                       │
│  pps_app | pps_netmng | pps_event_mng | pps_storage     │
│  pps_led_ctrl | pps_playback | pps_syslink_msg           │
├──────────────────────────────────────────────────────────┤
│                    ppsdk / Device Layer                   │
│  ppsdev_sdk | ppsdev_config                              │
│  device_alarm | device_media | device_upgrade | device_ptz│
│  device_settings | device_sound | device_power | ...     │
├──────────────────────────────────────────────────────────┤
│                      Core Layer                           │
│  pps_eventhub | pps_statem | pps_net | pps_ssl | pps_dns │
├──────────────────────────────────────────────────────────┤
│                   Components Layer                        │
│  mbedtls | mongoose | restful | websrv | rtsp | rtmp     │
│  discovery | prodtest                                    │
├──────────────────────────────────────────────────────────┤
│                      HAL Layer                            │
│  pps_hal_board | pps_hal_net | pps_hal_flash             │
│  pps_hal_uart | pps_hal_motor | pps_hal_adc | ...        │
├──────────────────────────────────────────────────────────┤
│                     OSAL Layer                            │
│  pps_mqueue | pps_event | pps_timer                      │
├──────────────────────────────────────────────────────────┤
│              Platform / RTOS / OS Layer                   │
│  HuaweiLiteOS (b6/b9)  │  Linux (b8/b81/b82)            │
└──────────────────────────────────────────────────────────┘
```

### 2.2 架构设计关键特点

| 特点 | 说明 |
|------|------|
| **平台抽象** | `arch/` 隔离 SoC HAL；上层仍有平台条件编译和媒体实现差异，移植需核对宏、库和 ABI |
| **SDK 编译选择** | `support_*` 与 `CONFIG_*` 决定 SDK 源码及库；不是运行时热插拔，部分回调为单槽注册 |
| **事件驱动** | 全局事件总线（eventhub）+ 状态机（statem）驱动异步流程 |
| **线程模型** | eventhub、状态机、事件管理、告警、录像、SDK 等异步执行；`pps_netmng_start()` 是启动选择函数，会返回，网络事件由状态机线程处理 |
| **回调接口** | HAL、网络、云 SDK 通过注册回调与上层解耦 |

---

## 3. 目录结构详解

```
lecamera_app/
├── arch/                          # 硬件架构适配层
│   ├── arm-himix100-liteos/       # b6: 海思 hi3518ev300 + LiteOS
│   ├── mips-zeratul-linux/        # b8/b81/b82: 君正 T31 + Linux
│   └── arm-gkmix100-liteos/       # b9: GK7202v300 + LiteOS
│       ├── drivers/               # GPIO/SPI/PWM/按键等驱动
│       ├── hal/                   # HAL 接口实现
│       ├── osal/                  # OS 抽象（mqueue/event/timer）
│       ├── hi3861l/               # 海思 Wi-Fi 模组子系统
│       ├── meari_sdk/             # 平台 Meari SDK 头文件
│       ├── tuya_sdk/              # 平台 Tuya SDK 头文件
│       ├── meari_cloud/           # 平台 Meari Cloud SDK 头文件
│       └── zbar/                  # 二维码扫描库头文件
│
├── core/                          # 平台无关核心库
│   ├── pps_eventhub.c/h           # 发布-订阅事件总线
│   ├── pps_statem.c/h             # 通用状态机框架
│   ├── pps_net.c/h                # TCP/非阻塞 socket 封装
│   ├── pps_ssl.c/h                # mbedTLS TLS 封装
│   ├── pps_dns.c/h                # DNS 缓存模块
│   └── make_svn.c/h               # 版本信息
│
├── device/                        # 设备功能模块层
│   ├── pps_device_alarm.c/h       # 告警事件管理（PIR/门铃/防拆）
│   ├── pps_device_board.c/h       # 板级信息管理
│   ├── pps_device_media.c/h       # 音视频媒体服务
│   ├── pps_device_upgrade.c/h     # OTA 固件升级
│   ├── pps_device_settings.c/h    # 设备配置参数 CRUD
│   ├── pps_device_ptz.c/h         # 电机/云台控制
│   ├── pps_device_power.c/h       # 电源/电池管理
│   ├── pps_device_led.c/h         # LED 状态指示
│   ├── pps_device_sound.c/h       # 音频播放
│   ├── pps_device_charm.c/h       # 门铃铃声管理
│   ├── pps_device_flight.c/h      # 飞行/勿扰模式
│   ├── pps_device_lock.c/h        # 电池锁管理
│   ├── pps_device_qrcode.c/h      # 二维码配网解析
│   ├── pps_device_time.c/h        # 系统时间/时区
│   ├── pps_device_user.c/h        # 用户账号管理
│   └── setting_opc.c/h            # 属性操作执行器（opc=operation callback）
│
├── apps/                          # 应用层（云 SDK 对接）
│   ├── pps_app.c                  # 主入口 main/app_init
│   ├── pps_netmng.c/h             # 网络连接状态机管理
│   ├── pps_event_mng.c/h          # 事件管理（PIR/录像触发）
│   ├── pps_storage.c/h            # SD 卡录像存储管理
│   ├── pps_playback.c/h           # 视频回放
│   ├── pps_led_ctrl.c/h           # LED 业务控制
│   ├── pps_syslink_msg.c/h        # MCU 通信（syslink 协议）
│   ├── pps_cmd.c/h                # 调试命令行接口
│   ├── meari/                     # 自研 Meari 云对接
│   ├── tuya/                      # Tuya 涂鸦云对接
│   ├── dot/                       # 移动 CMCC DOT 对接
│   └── nkit/                      # NKIT 对接
│
├── ppsdk/                         # SDK 接口层（云 SDK 调用设备能力的桥梁）
│   ├── ppsdev_sdk.c/h             # 设备能力统一接口（reboot/reset/info）
│   └── ppsdev_config.c/h          # 配置参数统一接口
│
├── components/                    # 第三方/自研中间件组件
│   ├── mbedtls/                   # mbedTLS 头文件和部分 library 源码
│   ├── mbedtls_mts/               # MTS 定制版 mbedTLS 头文件
│   ├── mongoose/                  # Mongoose HTTP 服务器库
│   ├── restful/                   # RESTful HTTP 服务（生产/调试，端口8090）
│   ├── websrv/                    # Web SDK 服务（二次封装）
│   ├── rtsp/                      # RTSP 推流客户端 + librtsp
│   ├── rtmp/                      # RTMP 推流
│   ├── discovery/                 # 局域网设备发现（UDP 广播）
│   └── prodtest/                  # 产线测试服务
│
├── include/                       # 全局头文件
│   ├── osal/                      # OSAL 类型/线程/互斥/信号量接口
│   ├── util/                      # AES/Base64/MD5/SHA1/DES3/HMAC/cJSON
│   ├── storage/                   # SD 卡/录像 API 接口
│   ├── pps_product_def.h          # 产品宏定义
│   ├── pps_util.h                 # 通用工具
│   └── bootparams.h               # 启动参数/Flash 布局定义
│
├── tools/                         # 开发/生产辅助工具
│   ├── dispatch/                  # 设备信息写入工具（生产）
│   ├── encrypt/                   # 固件加/解密工具
│   ├── pack/                      # 固件打包/解包工具
│   ├── hi_lzma/                   # LZMA 压缩工具
│   ├── battery/                   # 电池配置工具
│   ├── wifi_test/                 # Wi-Fi 测试工具
│   ├── mcu/                       # MCU 固件解析工具
│   ├── mtd/                       # MTD flash 布局文档
│   ├── iperf/                     # iperf 网络测速
│   ├── reboot/                    # 重启工具
│   ├── cmd_srv/                   # 命令服务进程
│   └── devinfo/                   # 设备信息查询工具
│
├── scripts/                       # 编译构建脚本
│   ├── buildconf/                 # 各平台/厂商组合配置文件（.conf）
│   ├── build.env                  # 默认构建组合
│   └── gen_version.sh             # 版本号生成脚本
│
├── test/                          # 测试代码
│   ├── hichannel/                 # HiChannel 通信测试
│   ├── pps_connect_noblock.c      # 非阻塞连接测试
│   └── pps_hardware_test.c        # 硬件功能测试
│
├── Makefile                       # 主构建文件
├── mkprj                          # 项目构建入口脚本
└── README.md                      # 工程使用指南
```

---

## 4. 硬件平台矩阵

| 代号 | SoC | 架构 | RAM | Flash | OS | Wi-Fi 方案 |
|------|-----|------|-----|-------|----|-----------|
| **b6** | 海思 hi3518ev300 | ARM | 64M | 8M | HuaweiLiteOS | hi3861L（UART syslink） |
| **b7** | Sigmastar（README 声明） | ARM | 64M | 16M | README 未明确 | 配置开启 hi3861L；缺主构建适配 |
| **b8** | 君正 T31zl | MIPS | 64M | 8M | Linux (zeratul) | 默认 hi3861L；厂商配置可覆盖 |
| **b81** | 君正 T31zl | MIPS | 64M | 8M/16M | Linux (zeratul) | 默认 hi3861L；厂商配置可覆盖 |
| **b82** | 君正 T31zx | MIPS | 128M | 8M | Linux (zeratul) | 默认 hi3861L；厂商配置可覆盖 |
| **b9** | GK7202v300 | ARM | 64M | 8M | HuaweiLiteOS | 默认 hi3861L；HGIC 取决于厂商配置及依赖 |

### 4.1 三套 arch 适配对比

| 功能 | arm-himix100-liteos | mips-zeratul-linux | arm-gkmix100-liteos |
|------|--------------------|--------------------|---------------------|
| **OSAL** | LiteOS mqueue/event | POSIX pthread | LiteOS mqueue/event |
| **网络** | lwIP | socket | socket |
| **Flash** | MTD flash 直写 | MTD + jffs2 | MTD flash 直写 |
| **Wi-Fi** | 默认 hi3861L | 默认 hi3861L；neutral_tx 选择 HGIC | 默认 hi3861L；厂商配置可覆盖 |
| **MCU 通信** | syslink（串口协议） | 串口 UART | syslink / 串口 |

---

## 5. 云平台 SDK 体系

### 5.1 SDK 对接架构

```text
┌──────────────────────────────────────────────────────────┐
│ ppsdk / device：设备能力、配置、媒体、存储、告警、升级     │
└──────────────────────────┬───────────────────────────────┘
                           ↕ 设备 API / 注册回调
       ┌───────────────────┼──────────────────┬─────────────────┐
       ↕                   ↕                  ↕                 ↕
┌───────────────┐  ┌────────────────┐  ┌──────────────┐  ┌──────────────┐
│ apps/meari    │  │ apps/tuya      │  │ apps/dot     │  │ apps/nkit    │
│ 旧 SDK /     │  │ Tuya 4.x / 5.x │  │ DOT 对接代码 │  │ NKIT 对接代码│
│ Meari Cloud  │  │                │  │              │  │              │
└───────┬───────┘  └────────┬───────┘  └──────┬───────┘  └──────┬───────┘
        ↕                  ↕                 ↕                 ↕
  对应 SDK 头文件和外部库（由 support_*、分支和平台配置选择）
        ↕                  ↕                 ↕                 ↕
  Meari 云服务         Tuya 云服务        DOT 服务          NVR / 中继
```

上图各列表示可选择的对接路径；是否编入及能否组合以构建配置为准。Meari 两种 SDK 复用应用代码，但头文件和库不同；告警、二维码等公共回调槽位不会自动隔离。

```text
设备能力 / 配置 / 媒体 / 存储（device、ppsdk）
              ↕ 回调注册与设备 API
应用对接代码（apps/meari、tuya、dot、nkit）
              ↕ 各 SDK 的平台头文件和外部库
云端服务，或 NKIT 对接的 NVR / 中继
```

此图表达调用边界；各 SDK 的实际协议、认证与传输实现需以对应库和版本为准。

### 5.2 SDK 选择与可核验能力

| 路径 | 构建选择 | 可见对接代码 | 核验边界 |
|------|----------|--------------|----------|
| Meari 旧 SDK | `support_meari_sdk=y` | `pps_meari_sdk.c`、`pps_iot_handle.c`；链接 `mp_ipc_sdk/PPCS_API` | 外部 SDK 实现未包含，不能把应用 TLS 结论扩展到全部 SDK 内部链路 |
| Meari Cloud | `support_meari_cloud=y` | 复用 Meari 应用代码，增加 `CONFIG_MEARI_CLOUD_SUPPORT`；链接 MTS、WebRTC、SRTP 等库 | 与旧 SDK 使用不同头文件、库路径；相关预编译库不在快照中 |
| Tuya | `support_tuya_config=y`，分支 `tuya4.x/tuya5.x` | Wi-Fi/wired HAL、stream、IoT handler、OTA、云存储等应用对接 | 两个版本选择不同 SDK 目录与库，配网格式也不能与 Meari 合并描述 |
| DOT | `support_dot_config=y`，分支 `dot` | LwM2M、HTTP 下载、OTA；非 recover 版本增加 CMCC `.cc` 源码 | recover 配置关闭普通媒体，需同时核对 CFLAGS 与 CPPFLAGS |
| NKIT | `support_meari_nkit=y` | NVR/中继对接、发现、媒体和 NVR 下发升级 | 属于套装通信对接，不能仅按独立云平台理解；当前默认组合就是 NKIT |

源码依据：[Makefile SDK 分支](/home/tronlong/lyp/meari/lecamera_app/Makefile:200)、[SDK 注册入口](/home/tronlong/lyp/meari/lecamera_app/apps/pps_app.c:350)。原功能对比表中“各平台协议、WebRTC、配网方式”等未逐项验证的概括不再作为确定结论。

Echo Show RTSP 与 Chromecast RTMP 由独立的 `support_pps_echoshow` / `support_pps_chromecast` 决定，不能推断每个 Meari 构建均包含这些功能。`pps_event_alarm_init()` 最终保存一个 `alarm_ctx.handler`，二维码回调也只有一个槽位；组合配置必须避免互相覆盖，不能称为自动隔离的运行时插件体系。

---

## 6. 核心模块分析

### 6.1 事件总线（pps_eventhub）

**设计模式**：发布/订阅（Pub/Sub）

```
定义的事件类型（pps_eventhub_e，43 个，含 TEST，不含 MAX 哨兵）：
  系统类：  REBOOT / SUSPEND / RESET
  按键类：  KEYDOWN / KEYDOWN_LONG
  告警类：  MOTION_DETECT / BELL_CALL / TAMPER_ALARM
  网络类：  NET_CONFIG / NET_CONNECT / NET_CONNECTED /
            NET_DISCONNECT / NET_DISCONNECTED / NET_CHANGED
  存储类：  SDCARD_MOUNTED / SDCARD_DISK_FULL / SDCARD_FORMATED
  录像类：  RECORD_START / RECORD_STOP
  图片类：  UPLOAD_PIC_START / UPLOAD_PIC_FINISHED / UPLOAD_PIC_FAIL
  SDK类：   SDK_INIT / SDK_ACTIVE_START / SDK_ACTIVE_FINISHED
  媒体类：  STREAMING_START / STREAMING_END / PLAY_VOICEMAIL
```

**实现机制**：

- 单消息队列（mqueue，配置消息数64），单派发线程
- 最多32个订阅者（数组固定分配）
- 发布者调用 `pps_eventhub_publish()` 投入队列
- 派发线程在持有 eventhub 互斥锁时，依数组顺序调用所有非空订阅者；订阅不按事件类型过滤，由回调自行筛选
- 消息只复制 `event` 与 `void *data` 指针值，不深拷贝数据；发布后数据生命周期由调用者保证
- 队列满时返回 `PPS_ERR_FULL`，发送失败返回 `PPS_ERR_WRITEFAIL`；不能默认事件必达
- Linux 队列实现是 System V IPC，`timeout_ms` 未实现、`cur_count` 无并发保护；64 是软件配置值，不能当作内核严格容量

依据：[派发实现](/home/tronlong/lyp/meari/lecamera_app/core/pps_eventhub.c:48)、[发布实现](/home/tronlong/lyp/meari/lecamera_app/core/pps_eventhub.c:279)、[Linux 队列](/home/tronlong/lyp/meari/lecamera_app/arch/mips-zeratul-linux/osal/pps_mqueue.c:43)。

### 6.2 状态机（pps_statem）

**设计目的**：管理 Wi-Fi 连接等异步状态转换

```text
事件投递 → 当前实例消息队列 → 实例线程
                              │
                              ▼
                  查找当前 state 对应的 callback
                              │
                              ▼
                  业务处理并按条件 set_current_state

网络状态转换摘录：
DISCONNECTED ── NET_CONFIG，初始化成功 ──▶ WAIT_CONFIG
DISCONNECTED ── NET_CONNECT，Wi-Fi 回调成功 ──▶ CONNECTING
WAIT_CONFIG  ── NET_CONNECT，回调成功 ──▶ CONNECTING
CONNECTING   ── NET_CONNECTED ──▶ CONNECTED
CONNECTING / CONNECTED ── NET_DISCONNECT 或 NET_DISCONNECTED ──▶ DISCONNECTED
```

```
状态机节点：最多4个并发状态机
每个状态机：最多8个 [state + callback] 动作对
每实例消息队列：配置消息数12（后端语义另见 OSAL）

实际 netmng 注册状态：
  PPS_NET_DISCONNECTED / PPS_NET_WAIT_CONFIG /
  PPS_NET_CONNECTING / PPS_NET_CONNECTED
```

每个状态机实例有自己的队列和线程。收到事件时，只执行当前状态对应的一个回调；状态转换由业务回调调用 `pps_statem_set_current_state()` 完成，框架不包含自动转换表。原文中的 `DISCONNECTING` 没有注册到网络状态机。详见 [pps_statem.c](/home/tronlong/lyp/meari/lecamera_app/core/pps_statem.c:95)、[网络动作表](/home/tronlong/lyp/meari/lecamera_app/apps/pps_netmng.c:741)。

### 6.3 网络核心（pps_net / pps_ssl）

**pps_net**：非阻塞 TCP 连接封装

- 使用 `fcntl(O_NONBLOCK)` + `select()` 实现带超时的连接
- 实际 `hints.ai_family=AF_INET`，此连接入口限定 IPv4，不能因注释写 IPv6 就判定支持 IPv6
- `pps_net_connect()` 返回 fd，失败为负数，不能按“0 成功”判断；`pps_net_write()` 直接调用 `send()`，调用者需要处理短写
- 第一次有效 socket 的连接失败后会 `break`，没有继续尝试 DNS 返回的后续地址

依据：[pps_net_connect](/home/tronlong/lyp/meari/lecamera_app/core/pps_net.c:132)。

**pps_ssl**：mbedTLS TLS 封装

- 结构体封装 `mbedtls_ssl_context` 所有上下文
- 可见的三个创建调用传入 `(NULL, 0, 0)`；`authmode` 固定为 OPTIONAL
- 即便传入 CA 和验证标志，初始化也未保存 CA，验证结果非零的分支没有返回失败；只调整调用参数不足以修复
- hostname 仍通过 `mbedtls_ssl_set_hostname()` 设置，但设置主机名不等于拒绝证书验证失败

完整缺陷链及调用范围见第 12.2 节。

### 6.4 HAL 层关键接口

```c
// 实际 Flash 接口名（以平台头文件为准）
pps_flash_read();
pps_flash_write();
pps_flash_erase();
pps_flash_file_read();
pps_flash_file_write();
dev_encrypt_read();
dev_encrypt_write();
```

`dev_encrypt_read/write` 用于设备生产/授权信息，XXTEA 分两段处理各 1024 字节。不能将此实现套用到所有运行配置。

### 6.5 设备配置系统（dev_cfg / ppsdev_config）

运行参数使用 `dev_cfg_param_t`，包含网络、用户、告警、工作模式、媒体等配置；头文件标注约 8 KiB，实际 ABI 尺寸应以目标编译器和宏组合确认。`dev_cfg_lock/unlock()` 使用互斥锁，并非读写锁。`dev_cfg_init()` 先读取配置，失败后建立默认值，再应用补丁并保存全局指针。

`write_cfg_data()` 更新字节累加和并尝试写两份配置。Linux 文件 HAL 使用普通 `fread/fwrite`，路径为 `/home/cfg/config`、`/home/cfg/config_bak`，这里没有 XXTEA 加密调用。上层 JSON 属性通过 `setting_opc` 执行即时设备操作，并持久化到结构体；详细流程与备份风险见第 18 章。

依据：[配置读写](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_board.c:674)、[初始化](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_board.c:935)、[Linux 文件 HAL](/home/tronlong/lyp/meari/lecamera_app/arch/mips-zeratul-linux/hal/pps_hal_flash.c:553)。

### 6.6 OTA 升级（pps_device_upgrade）

```text
固件包布局（描述项中的 offset / file_len 决定数据位置）：
┌──────────────────────────────────────────────────────────┐
│ 固定头：64 字节                                          │
│ Magic = 0x5354524e；头累加和；header_length；file_nums    │
│ 平台 / 厂商 / 版本等字段                                 │
├──────────────────────────────────────────────────────────┤
│ N 个文件描述：每项 44 字节                               │
│ filename[32] + offset[4] + file_len[4] + checksum[4]      │
├──────────────────────────────────────────────────────────┤
│ 文件 1 数据                                              │
│ 文件 2 数据                                              │
│ ……（文件名映射到各平台分区；不限定统一分区列表）         │
└──────────────────────────────────────────────────────────┘
  固定头及文件描述：固定 XOR 混淆；文件数据：原样存放
  设备解析器接受 N = 1～16；头部尺寸要求为 64 + 44 × N

升级概览：
云 SDK / NVR / SD / HTTP 等入口
        │ 各入口接收并提交升级数据
        ▼
pps_device_upgrade → 解混淆并检查包头 → 按文件校验和查分区
        │
        ▼
逐项 Flash 擦写（64 KiB 步长）→ 必要时升级 MCU
        │
        ▼
入口负责后续清理 / 重启（URL 路径见第 10 章）
```

包头由 `tools/pack/pack.h` 定义：固定头 64 字节，每个文件描述 44 字节（文件名 32 字节、offset、file_len、checksum 各 4 字节）。固件 Magic 为 **`0x5354524e`（STRN）**；`0x56565099` 出现在生产信息等其他结构，不能混用。

打包工具把文件数据原样写入包，**只对包头做固定 XOR 混淆**。头校验覆盖偏移 12 开始的头数据，文件数据使用字节累加和。这里没有 CRC 或数字签名验证，校验不能证明发布者身份。分区由文件名动态查表，不应将某张 Flash 布局视为全部硬件统一格式。

依据：[pack.h](/home/tronlong/lyp/meari/lecamera_app/tools/pack/pack.h:30)、[打包过程](/home/tronlong/lyp/meari/lecamera_app/tools/pack/pack.c:119)、[设备解析](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_upgrade.c:203)。升级时序、失败路径和内存边界见第 10、12 章。

---

## 7. 系统初始化流程

```
main() / app_init()
│
├─ 1. pps_system_init()
│   ├─ board_driver_init()          # GPIO/SPI/PWM/按键驱动
│   ├─ pps_watchdog_init()          # 看门狗（防止死机）
│   ├─ pps_eventhub_init()          # 创建事件总线线程
│   ├─ pps_statem_init()            # 初始化状态机框架
│   ├─ start_syslink_service()      # 启动 MCU UART 通信
│   ├─ pps_board_init()             # 读取 Flash 启动参数（UUID/SN/MAC）
│   ├─ dev_cfg_init()               # 加载 Flash 配置参数
│   ├─ pps_event_mng_init()         # 事件管理模块初始化
│   ├─ board_adc_light_init()       # ADC 光线传感器
│   ├─ pps_media_service_init()     # 音视频媒体服务
│   │   └─ [thread] pps_media_service_start
│   ├─ board_rootfs_init()          # rootfs 挂载
│   └─ pps_upgrade_mcu_check/mcu()  # 检查并升级 MCU 固件
│
├─ 2. pps_device_init()
│   ├─ tm_systime_init()            # NTP 时间同步
│   ├─ pps_device_sound_init()      # 音频播放初始化
│   ├─ ppsdev_init_config()         # 应用配置加载
│   ├─ pps_flight_init()            # 飞行/勿扰模式
│   ├─ pps_device_ptz_init()        # 电机/云台
│   ├─ pps_netmng_register()        # 注册网络回调（Wi-Fi/4G）
│   ├─ pps_netmng_init()            # 网络管理初始化
│   └─ SDK 初始化（按编译宏）
│       ├─ pps_meari_sdk_init()     # Meari
│       ├─ pps_tuya_sdk_init()      # Tuya
│       ├─ dot_sdk_init()           # DOT [独立线程]
│       └─ pps_nkit_sdk_init()      # NKIT
│
└─ 3. pps_service_init()   → 启动各服务线程
    ├─ [thread] pps_led_service_init        # LED 状态机
    ├─ [thread] pps_event_alarm_handler     # 告警处理
    ├─ [thread] pps_event_mng               # 事件管理主循环
    ├─ [thread] pps_netmng_start            # 启动网络选择，会返回；持续事件处理在 statem
    ├─ [thread] pps_storage_record_loop     # 录像线程（512K栈）
    ├─ [thread] pps_storage_disk_init       # SD 卡挂载
    ├─ [thread] pps_meari_sdk_start         # Meari SDK 主循环（512K栈）
    ├─ [thread] pps_tuya_sdk_start          # Tuya SDK 主循环（1M栈）
    ├─ [thread] tuya_upgrade_time           # Tuya 升级时间检查
    ├─ [thread] tuya_cloud_storage          # Tuya 云存储（1M栈）
    ├─ [thread] tuya_status_sync            # Tuya 状态同步
    ├─ [thread] cmcc_user_login             # DOT 登录
    ├─ [thread] pps_nkit_sdk_start          # NKIT（512K配置栈）
    └─ init_product_test()                 # 工厂模式才启动 HTTP / discovery
```

### 7.1 初始化成功与服务可用的区别

`main/app_init` 按 system → device → service 顺序执行。watchdog、eventhub、statem、syslink、board、配置、媒体初始化和 rootfs 等主要失败会使 system 返回错误；ADC 光照、媒体线程创建、PTZ 和 MCU 升级等部分失败只记录日志。`pps_service_init()` 对大量初始化/线程创建失败也只记录日志，最后仍返回 `PPS_SUCCESS`。

因此主入口进入常驻 `sleep(10000)` 循环，只能说明入口流程走到末尾，不能证明告警、网络、录像或 SDK 线程全部启动成功。没有统一的逐服务健康状态与失败回滚；线程栈设置值也不等于实际常驻内存占用。

源码依据：[system 初始化](/home/tronlong/lyp/meari/lecamera_app/apps/pps_app.c:132)、[device 初始化](/home/tronlong/lyp/meari/lecamera_app/apps/pps_app.c:251)、[service 初始化](/home/tronlong/lyp/meari/lecamera_app/apps/pps_app.c:425)、[主入口](/home/tronlong/lyp/meari/lecamera_app/apps/pps_app.c:547)。

---

## 8. 事件驱动机制

### 8.1 PIR 告警的实际调用链

以下文本流程图可直接阅读，无需安装图表渲染插件：

```text
MCU syslink 消息 / 启动唤醒原因
        │
        ▼
eventhub 发布 MOTION_DETECT
        │ 派发线程依次调用订阅者，回调自行筛选事件
        ├─ event_pre_subscriber：记录唤醒事件
        ├─ 其他订阅者：按各自事件分支处理
        └─ event_mng_subscriber
                │
                ▼
        event_mng_schedule 业务过滤
        工厂模式 / PIR 开关 / 休眠等待 / 云台运动 /
        告警计划 / 20 秒限频 / 录像忙 / PIR 电源
                ├─ 未通过 → 本次告警不下发
                └─ 通过
                    │
                    ▼
        pps_event_send_alarm_msg → pir_event
                    │ 唤醒独立告警线程
                    ▼
        pps_event_alarm_handler
                    ├─ 发布 UPLOAD_PIC_START
                    └─ 调用所选 SDK 的 alarm handler
                            ├─ event_snapshot：抓图、按配置做人形检测
                            │       └─ set_record_flag：本地录像 / 云存储
                            └─ 上传结果事件：FINISHED / FAIL
                                    └─ 更新休眠等待 / 缓存补传状态
```

PIR 的重要可见入口是 `pps_syslink_msg.c` 中的 MCU 回调以及 `pps_event_mng()` 对启动唤醒原因的补发。原先从 HAL radar 直接画到所有告警消费者的图省略了关键调度层。

`event_mng_schedule()` 先处理工厂模式、PIR 开关、休眠倒计时、云台运动、告警计划、20 秒限频、录像忙和 PIR 电源状态。通过后调用 `pps_event_send_alarm_msg()`，由独立告警线程调用注册的 SDK handler；并非事件总线直接完成云上传。PIR 的录像启动位于抓图/检测回调中，门铃和防拆分支则会更早调用 `set_record_flag()`。

依据：[MCU 事件入口](/home/tronlong/lyp/meari/lecamera_app/apps/pps_syslink_msg.c:154)、[业务过滤](/home/tronlong/lyp/meari/lecamera_app/apps/pps_event_mng.c:811)、[抓图与录像联动](/home/tronlong/lyp/meari/lecamera_app/apps/pps_event_mng.c:494)、[告警线程](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_alarm.c:370)。

### 8.2 网络状态机转换

```text
┌─────────────────┐
│  DISCONNECTED   │
└────────┬────────┘
         │ NET_CONFIG，配网初始化成功
         ▼
┌─────────────────┐
│   WAIT_CONFIG   │
└────────┬────────┘
         │ NET_CONNECT，停扫码，连接回调成功
         ▼
┌─────────────────┐
│   CONNECTING    │
└────────┬────────┘
         │ NET_CONNECTED
         ▼
┌─────────────────┐
│    CONNECTED    │
└─────────────────┘

补充边与失败分支：
DISCONNECTED ── NET_CONNECT，Wi-Fi 回调成功 ──▶ CONNECTING
WAIT_CONFIG ── NET_CONNECT，回调失败 ──▶ DISCONNECTED
WAIT_CONFIG ── NET_SCAN_AP，扫描失败 ──▶ 3 秒定时器 → 再次扫描
WAIT_CONFIG ── NET_SCAN_AP，扫描成功 ──▶ 保存凭据 → 提交 NET_CONNECT
DISCONNECTED ── NET_CONFIG，初始化失败 ──▶ 保持 DISCONNECTED
DISCONNECTED ── NET_CONNECT，回调失败 ──▶ 保持 DISCONNECTED
DISCONNECTED ── NET_CONNECT，4G 回调成功 ──▶ CONNECTED
DISCONNECTED ── NET_CONNECTED 通知 ──▶ CONNECTED
CONNECTING / CONNECTED ── NET_DISCONNECT 或 NET_DISCONNECTED ──▶ DISCONNECTED
CONNECTED ── NET_RECONNECT，执行重连回调 ──▶ DISCONNECTED
```

图中状态名省略 `PPS_NET_`，事件名省略 `PPS_EVENTHUB_`。WAIT_CONFIG 的连接成功分支进入 CONNECTING；不存在单独注册的 CONFIGING 或 DISCONNECTING 状态。

| 当前状态 | 事件 | 动作与目标状态 |
|----------|------|----------------|
| DISCONNECTED | NET_CONFIG | 配网初始化成功后进入 WAIT_CONFIG |
| DISCONNECTED | NET_CONNECT | 连接回调成功后，Wi-Fi 进入 CONNECTING；4G 分支可直接进入 CONNECTED |
| WAIT_CONFIG | NET_CONNECT | 停止二维码扫描，调用连接回调；成功进入 CONNECTING，失败进入 DISCONNECTED |
| WAIT_CONFIG | NET_SCAN_AP | 有网关凭据则扫描；失败启动 3 秒重试定时器，成功提交连接 |
| CONNECTING | NET_CONNECTED | 处理产线 IP、DNS 与提示音，进入 CONNECTED |
| CONNECTING / CONNECTED | NET_DISCONNECT | 调用断开回调，直接进入 DISCONNECTED |
| CONNECTING / CONNECTED | NET_DISCONNECTED | 进入 DISCONNECTED |
| CONNECTED | NET_RECONNECT | 调用重连回调，然后进入 DISCONNECTED |

这是注册动作表的摘录，不是完整设备重连策略。网络模块实际只有四个注册状态；配网等待与电源超时分别由相关模块维护，不能补画一个源码不存在的统一“配置超时→重连”边。

依据：[网络回调与状态处理](/home/tronlong/lyp/meari/lecamera_app/apps/pps_netmng.c:512)、[注册表](/home/tronlong/lyp/meari/lecamera_app/apps/pps_netmng.c:741)。

---

## 9. 网络管理流程

### 9.1 启动网络选择的优先级

```text
pps_netmng_start()
    │
    ├─ 当前已连接？是 → 发布 NET_CHANGED，设为 CONNECTED → 返回
    │               否
    ▼
等待 3 秒供 SD 挂载
    ├─ 正在升级？是 → 返回
    │           否
    ▼
按下面顺序选择第一个满足条件的分支：
    ├─ 产线 SD 网络标记存在
    │       └─ 设置产线类型 / meari0 接口 → init_product_test
    ├─ 网络类型为 4G
    │       └─ 直接返回；由 4G 初始化和通知路径维护状态
    ├─ 产线工具文件存在，且上下文 SSID 非空
    │       └─ 使用该 Wi-Fi 配置提交连接
    ├─ 板级支持工厂模式，且当前处于工厂模式
    │       └─ 使用固定产线热点配置提交连接
    └─ 普通路径
            ├─ Tuya 构建 → 此处普通 Wi-Fi 逻辑被条件编译排除
            └─ 非 Tuya 构建 → 读取已有 Wi-Fi 配置
                    ├─ SSID 非空 → 提交连接
                    └─ SSID 为空
                            ├─ DOT 构建 → 此处配网逻辑被排除
                            └─ 非 DOT 构建 → 请求配网
                                    └─ 有网关凭据 → 发布 NET_SCAN_AP
    │
    ▼
返回执行结果；后续连接状态由异步事件更新
```

`pps_netmng_start()` 检查当前网络状态；若已经连接，发布 NET_CHANGED 并返回。否则等待 3 秒供 SD 卡挂载，检查是否正在升级，再依次选择以下路径：

1. 检测产线 SD 网络标记：设置工厂类型、接口 `meari0` 并初始化生产测试。
2. 4G 网络类型：直接返回，持续状态由 4G 初始化及通知路径处理。
3. 存在产线工具文件且上下文已有 SSID：使用该配置连接。
4. 板级能力允许工厂模式，且当前确实处于工厂模式：连接固定产线热点。
5. 普通非 Tuya 路径：读取已有 Wi-Fi 配置并连接；没有配置且非 DOT，则发布配网事件。存在加密区网关凭据时再发起网关 AP 扫描。

因此“上电检查 Flash，没有 SSID 就统一扫描二维码”只适用于部分普通构建。该函数不是常驻网络主循环。依据：[启动选择实现](/home/tronlong/lyp/meari/lecamera_app/apps/pps_netmng.c:947)。

### 9.2 二维码配网与配置保存

```text
二维码识别结果 → pps_qrcode_get_string 公共入口
    │
    ├─ 匹配产线二维码且解析成功
    │       └─ 设置工厂类型 → 初始化生产测试 → 提交 Wi-Fi 连接
    └─ 非产线二维码 → 调用当前注册的 SDK 二维码回调
            ├─ Meari / NKIT
            │       └─ 解析并适配 s/p/t 字段
            │               → 保存 Wi-Fi 与 token
            │               → pps_netmng_wifi_connect
            │               → NET_CONNECT → 当前状态的连接回调
            │               → 等待网络通知更新状态
            ├─ Tuya
            │       └─ 按需补齐大括号 → 对应版本 / 宏下的 SDK 配网调用
            └─ DOT
                    └─ DOT 自有二维码及网络管理流程
```

二维码首先经过公共产线格式识别，再交给当前注册的 SDK 回调。Meari 与 NKIT 都接受特定 `s/p/t` 字段格式并做新旧格式适配，随后保存 Wi-Fi 与 token，再调用 `pps_netmng_wifi_connect()`；配置保存发生在连接成功之前。不能统一称为严格 JSON，也不能写成连接成功后才保存。

Tuya 回调自行分配 `length+4` 缓冲区，必要时补 `{}`，并在相应编译条件下调用 Tuya direct-connect。DOT 有自己的二维码和网络管理路径。公共产线解析器使用 `strstr()` 和分隔符截取；各 SDK 格式及边界检查需要分别核验。

依据：[公共回调](/home/tronlong/lyp/meari/lecamera_app/apps/pps_netmng.c:303)、[Meari 回调](/home/tronlong/lyp/meari/lecamera_app/apps/meari/pps_meari_sdk.c:2525)、[NKIT 回调](/home/tronlong/lyp/meari/lecamera_app/apps/nkit/pps_nkit_sdk.c:159)、[Tuya 回调](/home/tronlong/lyp/meari/lecamera_app/apps/tuya/pps_tuya_sdk.c:885)。

### 9.3 网络后端和 DNS

`pps_device_init()` 通过 `#if/#elif` 按 hi3861L → HGIC → 4G 的顺序注册网络回调。4G 复用了 `wifi_connect_cb` 等字段名，但会设置 `PPS_NETWORK_TYPE_4G`，并注册设备初始化、反初始化与网络事件通知，不能认为 Wi-Fi 与 4G 的实际时序完全相同。

`netmng_wifi_check_dns()` 仅在 `__HuaweiLite__` 下把 `8.8.8.8` 调为首选 DNS，Linux 下函数为空。DNS 缓存配置还会注册 `ppsdev_dns_analysis` 到公共地址解析封装。公网 DNS、MCU DNS 缓存与系统 resolver 必须分别排查，不能把 LiteOS 行为推广到 Linux。

依据：[网络注册](/home/tronlong/lyp/meari/lecamera_app/apps/pps_app.c:296)、[DNS 调整](/home/tronlong/lyp/meari/lecamera_app/apps/pps_netmng.c:112)、[DNS 注册](/home/tronlong/lyp/meari/lecamera_app/apps/pps_app.c:345)。

---

## 10. 固件升级流程

### 10.1 升级入口

| 来源 | 可见入口 | 说明 |
|------|----------|------|
| Meari URL | `pps_device_url_upgrade_firmware()` | 注册 SDK 下载回调，独立线程下载到共享升级缓冲区 |
| Tuya OTA | Tuya 下载/完成回调 | 使用 `pps_device_upgrade_ctrl()` 获取缓冲区，再提交设备升级 |
| DOT | `dot_ota_upgrade.c` | 自有下载和缓存控制，最终调用设备升级 |
| NKIT / NVR | `pps_nvr_firmware_upgrade_fun()` | 分包接收 NVR 升级数据，最终提交实际收包长度 |
| SD 卡 | `pps_sd_cmd_upgrade()`、`ppsdev_firmware_upgrade_by_file()` | 挂载回调检查本地升级文件，包含工厂及普通文件规则 |
| HTTP 工具 | `/flash/upgrade/*`、`/devices/firmware_upgrade` | 工厂 HTTP 服务启动后提供相关路径；处理器权限需逐项核对 |

依据：[URL 升级](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_upgrade.c:683)、[Tuya OTA](/home/tronlong/lyp/meari/lecamera_app/apps/tuya/pps_tuya_sdk.c:707)、[DOT OTA](/home/tronlong/lyp/meari/lecamera_app/apps/dot/dot_ota_upgrade.c:90)、[NVR 升级](/home/tronlong/lyp/meari/lecamera_app/apps/nkit/pps_nkit_handle.c:1178)、[SD 文件升级](/home/tronlong/lyp/meari/lecamera_app/apps/pps_storage.c:203)。

### 10.2 通用升级的实际时序

URL 升级路径的完整分支图如下；其他入口的接收方式见 10.1 节：

```text
pps_device_url_upgrade_thread
    ├─ 未注册下载回调 → 返回失败
    ▼
upgrade_ctrl(1)：设置升级标志，停扫码 / DSP，申请缓冲区
    ├─ 初始化失败 → 返回失败
    ▼
调用已注册下载器，回调将数据放入升级缓冲区
    ├─ 下载失败 ──────────────────────────────────────┐
    └─ 下载成功                                      │
            ▼                                        │
    pps_device_upgrade(buf, size)                    │
    （URL 线程传容量 size，而非实际接收长度）        │
            │                                        │
            ▼                                        │
    解锁电池 → 卸载 rootfs → 按后端擦除 Wi-Fi 配置   │
            │                                        │
            ▼                                        │
    upgrade_file_parse                               │
            ├─ 解码固定头，读取 header_length        │
            ├─ 分配并解码完整头                      │
            ├─ 检查文件数 / 头大小                   │
            ├─ 新格式比较平台；旧格式跳过            │
            ├─ 检查 Magic 0x5354524e / 头累加和      │
            ├─ 第一遍：文件累加和 / 分区查表，统计大小
            └─ 第二遍：再查文件 / 分区，逐项擦写 Flash
                    │                                │
                    ▼                                │
    检查 MCU 包版本，必要时通过 syslink 升级 MCU      │
            │                                        │
            ├─ 返回成功 ─────────────────────────────┤
            └─ 返回失败，记录日志 ───────────────────┤
                                                     ▼
    upgrade_ctrl(0)：释放缓冲区，清除标志，进度设为 100
            │
            ▼
    sync → 等待 3 秒 → 发布 REBOOT

解析器内的特别分支：
    文件累加和失败 / 分区名未知 → continue，跳过该项
    Flash 擦除错误被忽略；连续十次写失败后仍推进 offset
    因此返回成功、进度 100 和重启都不能证明全部分区写入正确
```

```text
upgrade_ctrl(1)
  → 设置升级状态、清零进度
  → 停止二维码/媒体 DSP，申请 BOOT_UPGRADE_SIZE 缓冲区
  → 部分 Linux 配置卸载 /app
接收升级包
  → pps_device_upgrade(buf, size)
  → 解锁电池、卸载 rootfs、按后端擦除 Wi-Fi 配置
  → 解混淆头，读取 header_length / file_nums
  → 检查文件数、头部大小、平台条件、Magic、头累加和
  → 按每项 offset / file_len 检查数据累加和
  → 按文件名查分区，逐项擦除及写入
  → 检查 Flash 中 MCU 包版本，必要时通过 syslink 升级 MCU
upgrade_ctrl(0)
  → 释放 DSP 缓冲区，清除升级状态，进度设置为 100
URL 路径再 sync、等待 3 秒并发布 REBOOT
```

平台比较只在新格式分支执行；`debug != 1 && major_version == 0` 被当作旧格式，跳过平台比较。解析器没有使用 `factory_id` 实施厂商匹配，也没有完整签名或防降级机制。`upgrade_boot()` 使用 **64 KiB** 擦写步长，并按最后剩余数据调整写入大小；`BACKUP_WRITE_UNIT=32 KiB` 只是未使用的宏。Linux HAL 内部还会回读比较。

没有找到原报告所写的 `pps_upgrade_verify()`，也没有通用流程“更新 bootparams 切换 A/B 镜像”的证据；recover 标志操作不能等同于完整 A/B 回滚。升级文件包含哪些分区取决于包内文件描述与平台查表。

依据：[设备升级主流程](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_upgrade.c:561)、[包解析](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_upgrade.c:203)、[分区写入](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_upgrade.c:137)、[Linux 写后校验](/home/tronlong/lyp/meari/lecamera_app/arch/mips-zeratul-linux/hal/pps_hal_flash.c:473)。

### 10.3 失败行为不能按成功流程理解

文件校验失败或分区名查找失败时，解析器 `continue`，最终仍可能返回成功。`upgrade_boot()` 连续十次写失败后推进 offset 并重置重试计数，函数最后返回 0；擦除返回值也被忽略。URL 路径下载或设备升级失败后仍清理并重启。`pps_upgrade_end()` 无条件设置进度 100，因此“100%”也不能作为固件写入正确的证据。

URL 线程向 `pps_device_upgrade()` 传入的是缓冲区容量 `size`，没有使用实际接收量 `g_upg_offset`；同时解析器缺少 offset/length 的包内边界检查。升级过程应先完成全部边界、分区、身份及完整性校验，再进入破坏性擦写阶段。相关问题与主机核验见第 12.3 节。

---

## 11. 生产工具链

### 11.1 dispatch（设备信息烧写工具）

产线将设备唯一标识信息从数据库导出为 JSON，通过 `dispatch` 工具写入 Flash：

```json
{
  "model": "LP100",
  "devType": 2,
  "prodNo": "SN2024001",
  "macAddr": "AA:BB:CC:DD:EE:FF",
  "p2p_id": "ABCDEF123456",
  "licence_id": "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx",
  "oem_authkey": "xxxxxxxx",
  "encryptVer": 2
}
```

`dispatch.c` 生成 `Encrypt-*.bin` 文件，XXTEA 对前两块各 1024 字节做处理，设备端由 `dev_encrypt_read/write` 对应读取。这是生产信息保护机制，不能据此认定所有运行配置都加密，也不能认定抵抗拆机读取或具有消息认证。

依据：[dispatch 工具](/home/tronlong/lyp/meari/lecamera_app/tools/dispatch/dispatch.c:36)、[输出与处理](/home/tronlong/lyp/meari/lecamera_app/tools/dispatch/dispatch.c:209)。`dispatch.py` 使用 Python 2 风格语法，并依赖数据库和外部工具；当前快照没有 `PPSDeviceDB.db`，生产流程不能直接复现。

### 11.2 pack / unpack 与构建脚本

```text
mkprj package
    │
    ▼
build_project → 生成版本信息 → make -j5
    │
    ▼
Makefile：链接 ppsapp → strip → 复制至所选平台 pack/app
    │
    ▼
按 b6 / b8* / b9 选择 arch/<平台>/pack
    │
    ▼
pack.sh <平台> <版本> <厂商> <构建类型>  [快照缺失]
    └─ 厂商为 prodtest 时，继续调用 packimg_project

mkprj packimg / packimg_project
    │
    ▼
同样选择平台 pack 目录
    │
    ▼
buildimg.sh platform=... out=camera-... factory=...  [快照缺失]

可见的独立工具 tools/pack/pack.c：
    输入文件 → 构建文件描述 / 累加和 → XOR 包头 → 输出升级包
    （因 pack.sh 缺失，无法确认主打包脚本如何调用此工具）
```

`tools/pack/pack.c` 提供文件打包、头描述和字节累加和实现，`unpack.c` 提供对应解包。`mkprj package` 先构建应用，再进入平台 `pack/` 执行 `pack.sh`；`mkprj packimg` 调用 `buildimg.sh`。当前快照三个平台都没有 `pack/`，因此不能核验最终镜像布局、实际输出名或 `$TFTPBOOT` 拷贝动作。README 中的流程属于工程设计说明。

依据：[package 入口](/home/tronlong/lyp/meari/lecamera_app/mkprj:213)、[packimg 入口](/home/tronlong/lyp/meari/lecamera_app/mkprj:187)。

### 11.3 buildconf 的组合方式

```text
scripts/build.env（平台 / 厂商 / 分支 / 构建类型）
    │
    ▼
platform_<平台>.conf
    │ 先设置平台能力与 support_*
    ▼
factory_<厂商>.conf
    │ 可覆盖此前 support_* 设置
    ▼
分支选择（meari / tuya4.x / tuya5.x / dot 等）
    │
    ▼
构建类型（release / debug）
    │
    ▼
Makefile 按最终值选择 ARCH_DIR、源码、宏、SDK 头文件与库

当前配置：b8 + neutral_nkit + meari + release
    → mips-zeratul-linux 适配 + NKIT 对接
    → 完整链接与打包仍依赖快照外的库和平台 pack 文件
```

平台配置先 include，厂商配置后 include，后者可以覆盖前者的 `support_*`。分支进一步设置正式/开发服务宏、Tuya 版本或 DOT，构建类型设置 release/debug。不能把平台 × 厂商 × 分支菜单的笛卡尔积视为全部可构建产品。

当前 `scripts/build.env` 是 `b8 / neutral_nkit / meari / release`，主版本文件为 `3.0.0`；Makefile 的 `SDK_VERSION=3.3.1` 是另一个变量，不能混为应用版本。完整组合与缺失依赖见第 15 章。

---

## 12. 安全与可靠性问题核验

本章区分“源码已确认”“隔离主机核验已复现”和“整机影响待验证”。修复优先级是工程排查顺序，不代表已经完成远程利用验证或 CVSS 评分。HTTP 问题尤其受工厂模式、服务监听和网络可达性约束。

### 12.1 用户认证接受空输入与正确凭据前缀

**源码已确认，原函数主机核验已复现。** `pps_verify_user()` 使用输入字符串长度决定 `memcmp()` 长度，没有要求输入长度与保存的账户、密码长度一致，也没有拒绝空字符串。

```c
usr_size = (strlen(username) > PPS_MAX_USERNAME) ? PPS_MAX_USERNAME : strlen(username);
psk_size = (strlen(password) > PPS_MAX_PASSWDLEN) ? PPS_MAX_PASSWDLEN : strlen(password);
if (memcmp(usr_str, username, usr_size) == 0 &&
    memcmp(psk_str, password, psk_size) == 0) {
    return 0;
}
```

空输入比较长度为 0，两项都会相等；正确用户名/密码的短前缀也可相等。HTTP `basic_auth()` 解码后以 `:` 拆分，再调用这个函数，因此它是管理认证链中的实际缺陷。是否在整机上可达还要确认工厂模式和 Mongoose 事件分派顺序。

使用虚构账户 `audituser/examplepass` 的主机输出为：

```text
full=0 wrong=-1 empty=0 prefix=0 empty_password=0
```

这里 0 表示认证成功，-1 表示失败。修复应拒绝空输入、校验完整长度、清空每个账户的解码缓冲区，并完整比较凭据。不能仅增加 HTTP 头格式检查。

依据：[pps_verify_user](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_user.c:118)、[Basic 认证调用](/home/tronlong/lyp/meari/lecamera_app/components/restful/restful_server.c:81)、[主机核验记录](evidence/verification.json)。

### 12.2 TLS 证书验证缺少失败闭环，传入 CA 也未保存

**源码已确认；CA 初始化问题主机核验已复现。** 三个可见创建调用在 Meari、DOT HTTP 请求和 RTSP 客户端中，均传 `(NULL, 0, 0)`。但根因不止调用参数：

1. `pps_ssl_init()` 使用 `calloc()` 清零结构体，随后判断 `if (ssl->ca_cert)`，而不是判断输入 `ca_cert`；因此传入 CA 也不会执行 `strdup()`。
2. `pps_ssl_connect()` 固定使用 `MBEDTLS_SSL_VERIFY_OPTIONAL`。
3. 即使验证标志设为 1，`mbedtls_ssl_get_verify_result()` 非零时也没有返回失败，相关返回语句被注释。

```c
// 初始化时 ssl->ca_cert 已被清零
if (ssl->ca_cert) {
    ssl->ca_cert = strdup(ca_cert);
}
```

传入测试 CA 字符串并设置两项验证标志后，主机输出：

```text
ca_saved=0 verify_host=1 verify_peer=1
```

修复需要同时保存/验证 CA、使用 REQUIRED、拒绝验证失败并清理失败连接；仅改为 `pps_ssl_init(ca_cert, 1, 1)` 仍不足。host 已经由 `mbedtls_ssl_set_hostname()` 设置，不能将问题写成“完全没有 hostname”。这些结论针对可见 `pps_ssl` 封装，不证明 Tuya 或其他外部 SDK 的所有 TLS 路径都存在同样问题。

依据：[TLS 握手与验证](/home/tronlong/lyp/meari/lecamera_app/core/pps_ssl.c:251)、[CA 初始化](/home/tronlong/lyp/meari/lecamera_app/core/pps_ssl.c:365)、[Meari 调用](/home/tronlong/lyp/meari/lecamera_app/apps/meari/pps_meari_sdk.c:186)、[DOT 调用](/home/tronlong/lyp/meari/lecamera_app/apps/dot/dot_util/pps_requests.c:340)、[RTSP 调用](/home/tronlong/lyp/meari/lecamera_app/components/rtsp/librtsp/rtsp_client.c:584)。

### 12.3 升级解析边界与写入错误传播

**源码已确认；持续写失败仍返回成功已在主机桩复现。**

| 位置 | 可见行为 | 可能后果及验证边界 |
|------|----------|-------------------|
| `upgrade_file_parse()` 初始解码 | 仅要求 size>0，就读取固定 64 字节头 | 短包可能越界读 |
| header_length | 从未验证的头取长度，分配后按该长度解码 | 长度超过实际包可越界读；过小分配还可能导致后续字段访问越界 |
| 文件描述数组 | 检查原始 size 足够容纳重新计算的头，但没有证明已分配头缓冲区足够 | 不能把后面的头长度检查当作完整保护 |
| offset/file_len | 在累加和计算前没有检查包内范围、加法溢出或重叠 | 任意偏移或大长度可能越界访问 |
| file_name | 定长 32 字节，随后用于 `%s`、查表及 `strcmp()` | 缺少显式 NUL 终止检查 |
| 文件校验或查分区失败 | `continue`，整体仍可能成功 | 形成部分更新或“未写任何分区仍成功” |
| `upgrade_boot()` | 擦除结果被忽略；写入十次失败后推进 offset，重置计数，最后返回 0 | 上层无法可靠判定固件写入失败 |
| URL 下载结束 | 把缓冲区容量传给解析器，没有传实际接收量 | 真实包长度边界失效；应使用 g_upg_offset 并核验总长度 |
| URL 版本字段 | 仅检查 version 非 NULL，没有实际版本比较 | 不能证明提供防降级保护 |
| 退出状态 | `pps_upgrade_end()` 设置 100；失败后 URL 线程仍重启 | 进度与重启不是成功证据 |

对 `upgrade_boot()` 提取原函数，以 HAL 桩令所有写入失败，输出：

```text
success_return=0 all_writes_fail_return=0 write_attempts=10 erase_attempts=10
```

桩不调用真实 Flash，延时被替换为空操作；输出证明该函数的错误传播问题，不证明某块硬件发生了写失败。建议先完整验证全部文件和分区，再擦写；校验失败应整包失败，擦写错误应逐层上报，最终成功条件应包括写后验证。当前解析器可见的是字节累加和与固定头混淆，没有发布者签名验证。

依据：[包解析](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_upgrade.c:203)、[写入重试](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_upgrade.c:137)、[URL 长度传递](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_upgrade.c:683)、[进度结束](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_upgrade.c:364)。

### 12.4 二维码与 HTTP 输入长度检查不完整

**源码已确认，越界后果尚未用整机或 sanitizer 验证。** 公共产线二维码、Meari 和 NKIT 回调都存在 256 字节 `data_fix`：允许 `length==256`，冒号开头时向 `data_fix+1` 复制 length 字节，随后写 `data_fix[length+1]`。即使 length==255，追加字符也会越过数组末端。非冒号路径若复制满 256 字节，也失去 NUL 终止保证。

公共 `parse_qrcode_info()` 将分隔符间的长度直接用于 SSID、密码转换与 token 的 memcpy，没有目标容量参数；`convert_qrcode_data()` 还读取 `in[i+1]`。风险出现在进入云 SDK 前的公共检查中，并非只有某个云版本才解析二维码。

HTTP `ppsdev_get_webss()` 按请求 method、URI、query 的源长度写入固定数组，未分别与 8/64/64 字节容量比较；body 只拒绝大于 4096，恰好等于容量时没有字符串终止空间。`basic_auth()` 的 Base64 目标没有容量参数，用户名/密码复制也无目标长度限制。`web_ppsdev_set_wifi_cfg()` 和 URL 升级的 strcpy 同样缺少输入长度边界。

建议按每个目标字段定义容量和最大输入长度，在复制前验证，保留 NUL 空间；对解析函数传入长度和容量。不能简单把 strcpy 全改为 strncpy 就认定安全，因为源码已经存在按外部长度调用 strncpy 的问题。

依据：[公共二维码](/home/tronlong/lyp/meari/lecamera_app/apps/pps_netmng.c:235)、[Meari 适配](/home/tronlong/lyp/meari/lecamera_app/apps/meari/pps_meari_sdk.c:2525)、[NKIT 适配](/home/tronlong/lyp/meari/lecamera_app/apps/nkit/pps_nkit_sdk.c:159)、[HTTP 请求转换](/home/tronlong/lyp/meari/lecamera_app/components/restful/restful_server.c:283)、[Wi-Fi POST](/home/tronlong/lyp/meari/lecamera_app/components/websrv/ppsdev_server.c:396)。

### 12.5 两处明确的锁等待风险

**源码已确认，未执行并发整机复现。**

- `pps_eventhub_deinit()` 持有 eventhub mutex，发送唤醒消息后调用 `pthread_join()`；派发线程接收后也要获取同一把 mutex。若销毁线程先拿锁、派发线程随后处理唤醒消息，就形成 join 等线程、线程等锁的互相等待。
- `pps_playback_start()` 在线程创建失败分支先锁 `playback_mutex`，再调用 `pps_playback_ctx_clear()`；后者再次锁同一把由 `PTHREAD_MUTEX_INITIALIZER` 初始化的锁。Linux 默认非递归锁下会自锁。

eventhub 还在持锁状态调用所有订阅者，若回调内再次订阅/退订，会重复获取同一把锁；慢回调也阻塞后续事件。这是接口使用约束和改造重点，不代表每个现有回调都实际触发了死锁。

依据：[eventhub 销毁](/home/tronlong/lyp/meari/lecamera_app/core/pps_eventhub.c:157)、[派发锁](/home/tronlong/lyp/meari/lecamera_app/core/pps_eventhub.c:67)、[回放失败路径](/home/tronlong/lyp/meari/lecamera_app/apps/pps_playback.c:392)、[回放清理](/home/tronlong/lyp/meari/lecamera_app/apps/pps_playback.c:73)、[POSIX 互斥封装](/home/tronlong/lyp/meari/lecamera_app/include/osal/pps_osal_mutex.h:28)。

### 12.6 Linux 消息队列的隔离与并发计数

**源码已确认。** `pps_mqueue_create()` 使用 `ftok("/", g_mqueue_cnt)` 和 `msgget(..., IPC_CREAT|0666)`，未使用 IPC_EXCL；key 来自进程内从 0 开始的计数。软件 `cur_count` 在生产者和消费者间无锁增减，`timeout_ms` 没有实现，底层 `msgsnd/msgrcv` 为阻塞调用。

同机其他进程在权限允许时可能碰到相同队列，异常退出遗留队列也需要处理；软件满队列判断与内核队列内容可能不一致。需按部署权限、IPC namespace 和重启策略验证实际影响。不能把“64 个消息、500 ms 超时”描述为 Linux 后端的严格保障。

依据：[Linux mqueue](/home/tronlong/lyp/meari/lecamera_app/arch/mips-zeratul-linux/osal/pps_mqueue.c:43)。LiteOS 使用另一套实现，不能照搬本节结论。

### 12.7 敏感数据输出与工厂服务暴露条件

**敏感输出已确认；是否能从部署设备取到数据受运行条件约束。** Wi-Fi 密码/token 出现在公共和 SDK 配网日志、产线网络日志中。Linux `SIGUSR1` 处理器直接 printf Wi-Fi 密码，没有 release 条件保护。Basic 认证也打印解码后的凭据。

release 默认日志级别为 ERROR，但命令行 `loglevel=` 可以覆盖；printf 不受该日志级别控制。因此不能把 release 默认抑制 WARN 等同于移除敏感输出，也不建议为了脱敏而全面禁用故障日志。

管理服务支持 Basic 和 Digest，当前绑定字符串为 `8090`，未配置 HTTPS。它由 `init_product_test()` 在工厂模式下启动。`/devices/wifi` 的 POST 在接收鉴权代码中被明确豁免；该路径可设置 SSID/PSK/token 并连接网络，风险需结合工厂服务可达性评估。

原报告声称“GET Wi-Fi API 在 8090 返回密码”不准确：`web_ppsdev_get_wifi_cfg()` 的确构造明文 `psk`，但当前路由表的 wifi 项只有 POST setter，没有挂 GET getter。因此本快照不能据此认定该 GET 泄漏接口可达。

依据：[日志级别](/home/tronlong/lyp/meari/lecamera_app/apps/pps_app.c:65)、[SIGUSR1 输出](/home/tronlong/lyp/meari/lecamera_app/apps/pps_app.c:99)、[服务启动门槛](/home/tronlong/lyp/meari/lecamera_app/components/prodtest/pps_product_test.c:2590)、[配网鉴权豁免](/home/tronlong/lyp/meari/lecamera_app/components/restful/restful_server.c:375)、[未挂载 getter 与路由表](/home/tronlong/lyp/meari/lecamera_app/components/websrv/ppsdev_server.c:441)。

### 12.8 运行配置、默认账户与生产信息应分开评估

运行配置路径没有统一 XXTEA 加密。默认用户在 `pps_device_user.c` 中用逐字节取反保存，认证时再还原，这属于可逆编码；账户默认值与是否能被修改、出厂清除应单独审查。报告不再次抄录生产密码。

生产信息 XXTEA 路径使用源码中的密钥材料，缺少可见的消息认证步骤。风险依据是可见的密钥管理与可逆存储实现，不是原文“已知明文可直接恢复任意 XXTEA 密钥”的推断。替换算法前应确定每设备密钥、密钥存放、完整性、防回滚和断电写入方案，不能只把 XXTEA 换成未指定模式的 AES 就认定修复完成。

配置读取还信任 `data->length` 参与累加和，未先验证长度；备份读写与短读问题见第 18.1 节。

依据：[账户默认值与解码](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_user.c:20)、[运行配置校验](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_board.c:674)、[生产信息加解密](/home/tronlong/lyp/meari/lecamera_app/arch/mips-zeratul-linux/hal/pps_hal_flash.c:735)。

### 12.9 Linux 信号处理

注册 `signal(SIGKILL, signal_handler)` 无法实现 SIGKILL 清理，返回值也未检查。源码问题还包括在异步信号处理器内调用 printf、日志、配置读取和 eventhub 等复杂函数；配置读取包含锁，处理器可能打断持锁线程，再尝试同一锁。

建议只在处理器内设置适当的信号标志或通知常驻线程，由正常线程执行输出与清理；SIGKILL 情况靠恢复策略处理。该路径只用于 Linux，LiteOS 分支注册用户命令。

依据：[处理器和注册](/home/tronlong/lyp/meari/lecamera_app/apps/pps_app.c:88)。

### 12.10 撤回缺乏证据的旧结论

| 原结论 | 本次核验修正 |
|--------|--------------|
| 固定 pers 就是固定随机种子，能够预测 TLS 密钥 | pers 是附加个性化数据；seed 同时调用 `mbedtls_entropy_func`。熵源质量需要针对目标库和平台验证，固定 pers 本身不能证明可预测 |
| malloc/free 次数不同证明内存泄漏 | 不成立。一个分配可对应多个错误分支 free，跨模块所有权和 pps_* 包装也会改变词法计数；需追踪分配生命周期或运行监测 |
| 572 处字符串 API 都是不安全漏洞 | 统计口径不明，且函数出现次数不等于实际调用或漏洞。重新给出明确词法统计，按输入来源和目标容量逐项审查 |
| Web 只用 Basic、所有设备都开 8090 | 源码也支持 Digest；可见启动条件为工厂模式 |
| 已对全部 1364 个文件完成深度静态分析 | 仅文件清点覆盖全部文件树；关键调用链经过复核，外部库和整机行为有明确边界 |

DRBG 的源码依据：[调用](/home/tronlong/lyp/meari/lecamera_app/core/pps_ssl.c:235)、[reseed 中收集熵再加入 additional](/home/tronlong/lyp/meari/lecamera_app/components/mbedtls/library/ctr_drbg.c:280)。不再保留原文以 MAC 地址、时间戳直接替代熵源的建议。

---

## 13. 代码质量评估

### 13.1 已核验的结构与约束

工程通过 arch/HAL、device、ppsdk 和 apps 实现平台与业务分层，统一回调减少 SDK 对设备细节的直接依赖。业务事件与阻塞上传分开执行，录像/回放也有独立服务和接口。这些结构有利于复用，但不足以证明“上层零修改”“全部 SDK 不相互影响”或“移植成本很低”。

当前较明显的维护成本来自跨模块共享状态、不同平台 OSAL 语义差异、编译宏组合、单槽回调注册和错误传播不一致。应先建立配置可构建性与关键失败路径验证，再扩展功能。

### 13.2 本次词法统计

口径：遍历全部 `.c/.h/.cc`，用 `\b函数名\s*\(` 匹配出现次数；包括声明、定义、注释和条件编译未启用内容，没有预处理或 AST 分析。因此这是代码审查线索，不是活跃调用量、漏洞数或内存泄漏数量。

| 名称 | 全快照词法次数 | apps 目录词法次数 |
|------|----------------|------------------|
| strcpy | 253 | 91 |
| sprintf | 219 | 55 |
| strcat | 33 | 0 |
| snprintf | 232 | 81 |
| malloc | 121 | 20 |
| free | 301 | 59 |

strcpy/sprintf/strcat 三项合计 505；与初版 572 的口径不能直接比较。`snprintf` 是否安全仍受容量、格式与返回值处理影响。

### 13.3 文件清点

| 目录 | 全部文件 | .c | .h |
|------|----------|----|----|
| apps | 75 | 27 | 45 |
| arch | 905 | 69 | 834 |
| components | 212 | 52 | 145 |
| core | 12 | 6 | 6 |
| device | 36 | 18 | 18 |
| include | 35 | 0 | 35 |
| ppsdk | 4 | 2 | 2 |
| scripts | 28 | 0 | 0 |
| test | 6 | 4 | 1 |
| tools | 47 | 21 | 11 |
| 根目录文件 | 4 | 0 | 0 |
| 合计 | 1364 | 199 | 1097 |

另有 3 个 `.cc`，均为 DOT/CMCC 适配。arch 中大量头文件来自 SDK 和平台接口，不能把总文件数等同于自研业务复杂度。事件枚举有 43 项（含 TEST、不含 MAX），最大订阅槽 32，状态机实例 4，每实例动作槽 8。

统计脚本与完整结果：[verify_source.py](evidence/verify_source.py)、[verification.json](evidence/verification.json)。

---

## 14. 总结与建议

### 14.1 工程定位

这是具有多平台、多 SDK 和工厂工具入口的低功耗摄像机应用源码快照。核心能力包括 MCU 唤醒联动、网络管理、告警抓图与检测、本地/云录像、回放、升级和设备配置。实际产品行为由平台/厂商/分支宏、运行能力字段、工作模式和外部库共同决定。

当前证据不足以保证全部组合可构建，也不能以目录结构和注释格式证明工业运行可靠性。三个已复现的函数缺陷及多处静态确认的错误路径，应作为维护入口。

### 14.2 修复与验证优先级

| 优先级 | 工作 | 可检查的完成条件 |
|--------|------|------------------|
| P0 | 修复用户认证完整比较 | 空输入、空密码、短前缀、长输入失败；完整正确凭据成功；真实 HTTP 链验证 |
| P0 | 完整修复 TLS 验证链 | CA 被保存；不可信/过期/错误主机证书失败；正确证书成功；失败资源清理 |
| P0 | 升级先验证后擦写，错误逐层传播 | 短包、异常头长度、越界分区、校验错、擦除错、连续写错均失败；禁止混合分区更新 |
| P1 | 修复二维码和 HTTP 字段边界 | 对容量边界、非终止字符串、异常转义、长 URI 等验证，无越界 |
| P1 | 修复 eventhub 销毁与回放失败锁路径 | 并发销毁、回调重入约束、线程创建失败注入可结束 |
| P1 | 配置短读、校验长度与断电持久化 | 损坏/缺失主备配置可恢复，写入失败不能报告成功 |
| P1 | 确认构建组合及依赖清单 | b7/x86 无效选项被明确处理；SDK/HAL 宏一致；工具链、BSP、库、pack 完整 |
| P1 | 敏感输出脱敏与工厂服务门槛 | printf、SIGUSR1、HTTP 认证日志均不输出密码；产线退出后服务状态可验证 |
| P2 | Linux IPC、服务健康状态与信号处理 | 队列隔离/异常退出恢复、初始化失败上报、正常线程处理信号 |
| P2 | 验证熵源、每设备密钥与升级身份 | 依据目标库配置和设备实测，不以更换算法名代替方案 |

此表是源码核验产生的维护建议，本次只修改文档与核验材料，没有实施源码修复。

---

## 15. 构建配置与依赖核验

### 15.1 配置如何生效

```text
mkprj init_env
  → 非 config 子命令先 source scripts/build.env
  → 未指定时选择 platform、branch、factory、release/debug
  → 读取 scripts/version
  → 写回 build.env，并创建 .out/<platform-id>/<branch>/<type>
build_project
  → 导出 VCAMERA_*，生成 version.h，执行 make -j5
Makefile
  → 选择 ARCH_DIR / BSP config.mk
  → include platform_<type>.conf
  → include factory_<factory>.conf
  → 按 support_* 加入源文件、CONFIG_* 和链接库
  → 按 branch/type 加入对应宏
  → 链接、strip、复制到 arch/.../pack/app，移除根目录临时目标
```

`mkprj config` 会写配置并创建目录，不能作为只读核验命令。本次使用 `make -n`，显式指定平台 ID、输出目录与构建变量，没有执行编译、strip、复制或清理动作。

依据：[mkprj 环境处理](/home/tronlong/lyp/meari/lecamera_app/mkprj:111)、[构建函数](/home/tronlong/lyp/meari/lecamera_app/mkprj:163)、[Makefile 选择与 include](/home/tronlong/lyp/meari/lecamera_app/Makefile:15)。

### 15.2 当前默认值与菜单声明

当前 `build.env`：b8、neutral_nkit、meari、release。应用版本 `scripts/version` 为 3.0.0。主 Makefile 自带缺省值仍为 b6-neutral、Release、apis-cn-hangzhou，与 mkprj 的小写 release/debug 和菜单值不同；直接 make 不读取 build.env，不能将两者混为默认配置。

菜单平台有 b6/b7/b8/b81/b82/b9；脚本分支有 meari、meari-develop、tuya4.x、tuya5.x、dot。Meari 厂商菜单 8 项、Tuya 2 项、DOT 4 项，共 14 个 factory 配置。另有 platform_x86.conf 和 Makefile x86 分支，但没有实际 arch/x86。README 的旧菜单不能代替当前脚本。

源码依据：[默认环境](/home/tronlong/lyp/meari/lecamera_app/scripts/build.env:1)、[当前 branch 菜单](/home/tronlong/lyp/meari/lecamera_app/mkprj:40)、[配置文件目录](/home/tronlong/lyp/meari/lecamera_app/scripts/buildconf/all_platform.conf:1)。

### 15.3 平台 × 厂商的覆盖关系

| 厂商配置 | SDK / 主要能力 | 网络覆盖 |
|----------|----------------|----------|
| neutral_std / neutral_develop | Meari 旧 SDK、AOS、Echo Show、Chromecast | 保留平台默认 hi3861L |
| m_neutral_std / m_neutral_develop | Meari Cloud、AOS、Chromecast | 保留平台默认 hi3861L |
| neutral_nkit | NKIT；关闭 Meari、AOS、Echo Show、Chromecast | 保留平台默认 hi3861L |
| neutral_tx | NKIT、HGIC FMAC、serial | 关闭 hi3861L，启用 HGIC |
| neutral_4g | Meari Cloud、4G、serial、RTC | 关闭 hi3861L，启用 4G |
| prodtest | Meari 旧 SDK、AOS、产测宏 | 保留平台默认 hi3861L |
| tuya2_general | Tuya | 保留平台默认 hi3861L |
| tuya2_4g | Tuya、4G、serial、RTC | **没有关闭平台默认 hi3861L** |
| dot_std / dot_meari_std | DOT，非 recover 增加 CMCC C++ | 保留平台默认网络设置 |
| dot_recover / dot_meari_recover | DOT recover、关闭普通 media、启用 SSL 宏 | 保留平台默认网络设置 |

`tuya2_4g` 的宏冲突尤其值得核对：平台配置开启 hi3861L，而厂商配置未覆盖为 n；apps 的网络注册是 hi3861L 优先的 `#elif` 链。因此菜单名“4G”不能证明实际选择 4G 网络后端。建议建立互斥约束并验证预处理结果，而不是只看文件名。

同时，当前 4G、HGIC、NKIT 的相关目录集中在 mips 平台；其他平台组合即使菜单可以选，也可能缺 HAL、serial 或 SDK 依赖。没有自动覆盖全部组合的可构建性检查。

依据：[tuya2_4g](/home/tronlong/lyp/meari/lecamera_app/scripts/buildconf/factory_tuya2_4g.conf:1)、[b8 平台默认](/home/tronlong/lyp/meari/lecamera_app/scripts/buildconf/platform_b8.conf:1)、[注册优先级](/home/tronlong/lyp/meari/lecamera_app/apps/pps_app.c:296)。

### 15.4 干运行结果与缺失依赖

| 核验组合 | make -n 结果 | 可以得出的结论 |
|----------|---------------|----------------|
| b8 / neutral_nkit / meari / release | exit 0，生成命令序列 | Makefile 能展开此组合，不代表编译或链接成功 |
| b7 / neutral_std / meari / release | exit 2，缺 `/config.mk` | b7 无 ARCH_DIR 分支，配置菜单与主构建不一致 |
| b6 / neutral_std / meari / release | exit 2，缺同级 liteos_v5.0.1.3/config.mk | BSP 未包含 |
| b9 / neutral_std / meari / release | exit 2，缺同级 liteos_gk020/config.mk | BSP 未包含 |
| x86 / neutral_std / meari / release | exit 2，缺 arch/x86/config/config.mk | x86 是未完整落地的配置入口 |

当前 PATH 未找到 `mips-linux-uclibc-gnu-gcc`；文件清点没有 `.a/.so`；三个 arch 都缺 pack 目录。因此 b8 干运行成功也无法证明完整固件能构建。未执行整机编译或试图自动替换工具链。

Makefile 为 LiteOS 选择 BSP config、LTO 与平台库，为 b8 系列选择 MIPS/Linux 与 `--gc-sections`。外部依赖至少涉及 OSAL/util/storage、媒体、NN/IMP、Meari/Tuya/DOT/NKIT、TLS 及 BSP。mbedtls 目录包含部分源码，但主应用 Makefile 按库链接；MTS 目录仅有头文件，不能当作完整可编译 TLS 实现。

依据：[MIPS 工具链配置](/home/tronlong/lyp/meari/lecamera_app/arch/mips-zeratul-linux/config/config.mk:1)、[链接与复制规则](/home/tronlong/lyp/meari/lecamera_app/Makefile:395)、[干运行证据](evidence/verification.json)。

---

## 16. 低功耗唤醒与休眠闭环

### 16.1 唤醒来源与角色分工

应用 SoC 与 MCU/Wi-Fi 模块通过 syslink 交换启动原因、网络状态、电池、PIR、时间、云连接信息和升级消息。hi3861L 分支与 serial/HC32F 分支分别有不同传输实现，启动时获取 bootinfo、注册回调并创建 keepalive 任务。

`pps_event_mng()` 将 `pps_get_wakeup_type()` 与预订阅回调暂存的 wakeup_event 合并，针对 KEY/PIR/TAMPER/WIFI_PACKET 等原因补发业务事件或延长活动窗口。启动原因不只是日志字段，而是决定告警、录像和休眠计时的输入。

依据：[syslink 初始化](/home/tronlong/lyp/meari/lecamera_app/apps/pps_syslink_msg.c:1201)、[启动信息处理](/home/tronlong/lyp/meari/lecamera_app/apps/pps_syslink_msg.c:446)、[唤醒调度](/home/tronlong/lyp/meari/lecamera_app/apps/pps_event_mng.c:1284)。

### 16.2 电源活动窗口

事件模块将 `PPS_PM_TIMEOUT_SCALE` 设为 2，正常 tick 周期为 500 ms。各电源事件有 SLEEP_MODE、ACTIVE_MODE 或 ACTIVE_TIMEOUT_MODE；计时值按 scale 换算。循环包含其他等待，以下是配置窗口而非硬实时承诺。

| 活动原因 | 配置窗口 |
|----------|----------|
| 普通 sleep 等待 | 10 秒 |
| 初始化 | 30 秒 |
| 网络连接 | 60 秒；device_class 为 4G 时部分路径扩展到 180 秒 |
| 配网 | 180 秒 |
| SDK 初始化 | 普通 120 秒；DOT 180 秒 |
| SDK 激活 | 40 秒 |
| Wi-Fi 包唤醒 | 15 秒 |
| 门铃呼叫等待 | 60 秒 |
| SD 格式化 | 600 秒 |

`pps_event_tick()` 递减超时，到期转 SLEEP 并触发 timeout callback；ACTIVE 状态可延长总 sleep 等待。工厂模式或特定设备类型持续重置 sleep 等待。升级期间循环延迟并保持活动，因此“休眠失败”排查必须查看具体事件状态，而不能只查一项总 timeout。

依据：[计时常量](/home/tronlong/lyp/meari/lecamera_app/apps/pps_event_mng.c:47)、[tick](/home/tronlong/lyp/meari/lecamera_app/apps/pps_event_mng.c:1186)、[格式化窗口](/home/tronlong/lyp/meari/lecamera_app/apps/pps_event_mng.c:1172)。

### 16.3 进入待机与兜底

录像活动由本地录像或云存储任一标志表示。事件管理有 record_timeout 兜底，设置为录像时长的 6 倍再乘 scale；超过等待窗口不应据此认定所有 SDK 上传都已正常退出。

总 sleep 等待低于 0 或 PM_EVENT_PWRON 为 SLEEP 后，应用决定 enable_keepalive：低功耗模式或 shutdown 时为 0，否则为 1。随后停止对应 SDK 服务、反初始化 SD、恢复日夜模式，按条件调整 PIR 曝光补偿和统计信息；HGIC 分支先进入 Wi-Fi sleep，然后向 MCU 发 standby 命令。

standby 后等待 10 秒；若循环仍继续，累积 retry_times，达到 3 次调用 `board_mcu_power_ctrl()`。这说明应用依赖 MCU/硬件真正完成电源状态转换，不是单纯 Linux 线程休眠，也不能证明失败后能完整恢复已关闭服务。

依据：[录像标志联动](/home/tronlong/lyp/meari/lecamera_app/apps/pps_event_mng.c:242)、[休眠退出过程](/home/tronlong/lyp/meari/lecamera_app/apps/pps_event_mng.c:1331)、[standby 协议](/home/tronlong/lyp/meari/lecamera_app/apps/pps_syslink_msg.c:621)。

### 16.4 重启与恢复出厂

REBOOT 由早期 event_pre_subscriber 调用 `system_reboot()`；先标记 reboot_flag、设置 LED、等待升级与 SD 格式化结束，再 sync、通知 MCU 并调用 HAL 重启。该订阅者执行在 eventhub 持锁回调上下文，因此等待会阻塞事件派发。

RESET 走单独的配置重置、媒体/SDK和 MCU 相关处理，不能只等同于删除一个文件；升级、配置持久化、SD 与 MCU 命令的交互需在目标产品验证。依据：[电源关闭准备](/home/tronlong/lyp/meari/lecamera_app/apps/pps_event_mng.c:152)、[重启/重置](/home/tronlong/lyp/meari/lecamera_app/apps/pps_event_mng.c:177)。

---

## 17. 媒体录像与回放链路

### 17.1 媒体实现由构建选择

启用 support_media_config 时，b8 系列加入 `pps_device_media_zeratul.c`，链接 ppsmedia/IMP/NN 等；其他平台加入 `pps_device_media.c`，链接媒体、AAC 和 zbar。二维码模块随媒体能力启用。上层通过 `ppsdev_media_*` 注册视频、音频、AAC、抓图、人形检测和对讲回调；硬件编码、NN 与缓存具体实现依赖外部库。

因此可以说明应用如何调用媒体 API，不能从本快照推断所有编码器缓存所有权、NN 准确率或实时性。DOT recover 关闭普通媒体，但还有 C++宏与 SDK 路径，需按整个组合检查。

依据：[媒体源码选择](/home/tronlong/lyp/meari/lecamera_app/Makefile:178)、[zeratul 媒体服务](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_media_zeratul.c:2259)。

### 17.2 录像数据路径

```text
媒体视频 / G711U / AAC 回调
  → 构造 pps_record_avframe_t
  → pps_record_write_avframe(group)
  → 外部 storage 组件的录像管理循环
SD 挂载 / 格式化 / 拔卡 / 满盘回调
  → 更新 sd_mount_done、发布 eventhub 事件
  → 控制 job 和回放，必要时检查 SD 升级文件
```

录像 group 使用 licence_id 与 serial_no 组合。视频轨为 H.264；通常使用 MP4、AAC 和 G711U，不同 Tuya OEM 分支选择 private 格式与 G711U。视频和音频回调过滤 `datetime < 1570983860` 的帧，因此未有效校时可能表现为录像无帧，不能直接判定 SD 写失败。

`pps_storage_record_init()` 注册视频通道 `100+0`、音频/AAC 通道 0，创建录像 job 并请求 I 帧。API 通道编号应按媒体接口约定理解，不能简单当作物理摄像头编号。

依据：[帧转换](/home/tronlong/lyp/meari/lecamera_app/apps/pps_storage.c:51)、[录像初始化](/home/tronlong/lyp/meari/lecamera_app/apps/pps_storage.c:473)。

### 17.3 SD 生命周期与录像延迟启动

SD 服务用 `/dev/mmcblk0`、挂载点 `/mnt/mmc01`。如果录像请求早于 SD 挂载完成，设置 sd_record_enable，挂载完成后再启动 job。全天录像与事件录像分别配置外部组件的模式、时长和循环规则。

挂载完成会发布 SDCARD_MOUNTED，并调用 `pps_sd_cmd_upgrade()`；拔卡开始先停止全部回放，再发布移除事件。REBOOT/RESET/NET_MOD_RESTART 的录像订阅者调用 `pps_recorder_del_job_all()`，但不同订阅者的顺序与阻塞重启回调还需结合 eventhub 时序核验。

依据：[SD 回调](/home/tronlong/lyp/meari/lecamera_app/apps/pps_storage.c:431)、[SD 初始化](/home/tronlong/lyp/meari/lecamera_app/apps/pps_storage.c:519)、[录像标志](/home/tronlong/lyp/meari/lecamera_app/apps/pps_storage.c:535)。

### 17.4 回放资源与失败路径

回放最多 4 个上下文，p2p_channel 为 0～3；每个上下文包含互斥锁、暂停/seek/mute 标志、开始时间、文件上下文与推流回调。帧缓冲配置为 256 KiB。`pps_playback_start()` 检查 SD 正常、时间有效、通道合法及 callback 非空；已有线程则转 seek，没有线程才创建推流任务。

回放停止并不等同于调用处同步 join 所有线程，资源释放时序应沿推流循环检查。线程创建失败时嵌套锁问题已在第 12.5 节列出。`group_name[64]` 从存储 group 用 strcpy 复制，也应核对设备授权/序列号的合计长度，而不是只审查 P2P 通道上限。

依据：[回放上下文](/home/tronlong/lyp/meari/lecamera_app/apps/pps_playback.c:30)、[回放启动](/home/tronlong/lyp/meari/lecamera_app/apps/pps_playback.c:339)、[全部停止](/home/tronlong/lyp/meari/lecamera_app/apps/pps_playback.c:423)。

### 17.5 告警图片与补传

event_snapshot 选择 fast 或普通抓图接口，对 PIR 进一步等待人形检测并联动声光/录像；校时不满足阈值时等待时间同步。图片上传完成发布耗时，业务订阅者更新 sleep 窗口并触发 ALARM_PIC_RETRAN；有正常 SD 和缓存图片且 PIR 未忙时重新提交告警。缓存图片最大数常量为 20。

这条路径解释了“PIR 已触发但没有上传”可能来自计划过滤、人形检测、抓图、校时、SDK 状态或存储忙，而不仅是网络故障。外部 SDK 的上传确认、重试和缓存文件具体格式仍需按产品验证。

依据：[抓图与人形检测](/home/tronlong/lyp/meari/lecamera_app/apps/pps_event_mng.c:494)、[结果与补传处理](/home/tronlong/lyp/meari/lecamera_app/apps/pps_event_mng.c:1052)。

---

## 18. 配置持久化与管理接口

### 18.1 双份运行配置的实际保证

`read_cfg_data()` 顺序读两个配置槽，检查 Magic、字节累加和和 use_flag，第一个有效槽即可使用；失败才尝试下一个。不是按版本序列选择最新副本，也没有可见原子提交协议。写入逐个 erase/write，希望成功写两份。

代码用 `2 ^ i` 判断位，而 C 的 `^` 是异或，不是幂。当前两槽 i=0/1 得到 2 和 3，第二项不是独立 bit，需澄清原设计并使用明确位掩码。

Linux `pps_flash_file_read/write()` 没有使用 start 偏移，按普通文件完整读取/覆盖；文件不存在时 read 返回成功，短读/短写只检查 `<0`，没有要求返回量等于请求 size，也没有 `ferror()` 检查。配置头 data->length 未先限定即用于累加和。高层检查 Magic 可以挡住部分缺失配置，却无法替代底层正确的短读和长度处理。

写前 erase 会删除 Linux 文件，再用 fopen("w") 创建；没有临时文件+原子替换、序列号或统一 flush/fsync 保证。双份存在不等于断电原子安全。持久化失败时 RAM 配置已修改的问题也应测试。

依据：[读写与掩码](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_board.c:674)、[Linux 文件 HAL](/home/tronlong/lyp/meari/lecamera_app/arch/mips-zeratul-linux/hal/pps_hal_flash.c:553)。

### 18.2 JSON 属性与底层配置

`ppsdev_init_config()` 由 `dev_cfg_param_t` 生成/解析 JSON 属性树。`prop_root_cbs`、子表和 setting_opc 表达属性访问模式、读取回调与设备动作。`ppsdev_set_all_config()` 持 g_config_lock，递归应用属性，再序列化并调用 `ppsdev_config_save()`；保存函数再持 dev_cfg 锁更新结构体和存储。

这表示 JSON 属性不是独立的另一份文件配置，也不是所有 setter 自动具有事务性。即时设备动作、RAM 属性树和持久化成功可能不一致；上层没有使用保存结果替换返回的属性设置结果，需分别检查错误传播。

依据：[属性根表](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_settings.c:110)、[保存](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_settings.c:328)、[整体设置](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_settings.c:796)、[初始化](/home/tronlong/lyp/meari/lecamera_app/device/pps_device_settings.c:841)。

### 18.3 工厂 HTTP 与发现服务

`pps_service_init()` 调用 init_product_test，但该函数在非工厂模式直接返回。工厂模式下注册媒体测试回调和 eventhub 订阅，启动 HTTP（配置栈 512 KiB）与 discovery 线程。公共二维码与 SD 产线标记可设置工厂类型，因此“非 prodtest 编译”不能自动证明这些服务永远不会启动。

HTTP 端口配置为 8090，支持 Basic/Digest 鉴权路径，`/devices/wifi` POST 有明确鉴权豁免。插件端点单独注册，最终授权还需要结合 Mongoose 事件生命周期与各处理器工厂判断检查；不能从一个公共回调推出所有路由权限一致。

| 路由族 | 可见作用 | 主要核验点 |
|--------|----------|------------|
| /sys/info、/sys/reboot、/sys/factory_reset | 运行信息、重启、重置 | /sys/info 中 cpu=10、mem=20 为固定值，不是实际监测结果 |
| /devices | 设备信息、settings、密码、升级、抓图、网络、Wi-Fi、PTZ | 按 web_ppsdev_handler_list 的 HTTP method 槽分派；wifi 无 GET getter |
| /flash/encryption、/flash/identity | 生产授权和身份信息 | 工厂状态、输入字段长度、授权边界 |
| /flash/upgrade/* | 接收升级/查询进度 | 包完整性、失败传播、是否可在量产模式访问 |
| /product_test、/pir_test、/battery_config | 产线与传感器测试 | 工厂门槛与异常输入 |

发现组件使用组播地址 239.255.255.250、端口 3702，以及广播相关端口 3703；还存在向 6969 发送的路径。只能确认发现/回复实现，不能由 SOAP 字符串推断完整 ONVIF 认证与协议兼容性。

依据：[工厂服务入口](/home/tronlong/lyp/meari/lecamera_app/components/prodtest/pps_product_test.c:2590)、[HTTP 端点与 bind](/home/tronlong/lyp/meari/lecamera_app/components/restful/restful_server.c:902)、[占位运行指标](/home/tronlong/lyp/meari/lecamera_app/components/restful/restful_server.c:481)、[设备路由](/home/tronlong/lyp/meari/lecamera_app/components/websrv/ppsdev_server.c:531)、[发现配置](/home/tronlong/lyp/meari/lecamera_app/components/discovery/discovery.c:24)。

### 18.4 排障定位顺序

| 现象 | 优先检查 |
|------|----------|
| 初始化失败或重启循环 | system 返回点、syslink bootinfo、配置有效性、rootfs、watchdog；不要只看 main 是否进入循环 |
| Wi-Fi 不连接 | 实际网络后端宏、工厂/SD 标记、已保存 SSID、NET_CONNECT 是否入队、连接回调与状态机 |
| 已连网但 SDK 不在线 | SDK 初始化返回、所选库版本、DNS 缓存、服务分支、TLS 失败；网络 CONNECTED 不代表云激活完成 |
| PIR 有事件无告警 | 工厂模式、PIR 开关、计划、PTZ、20 秒限频、power state、SDK handler、抓图/人形检测/校时 |
| SD 没有录像 | 挂载标志、帧 datetime、record_mic_enable、group job、事件/全天模式、存储组件返回值 |
| 长时间不休眠 | ACTIVE/ACTIVE_TIMEOUT 电源事件、升级状态、format 状态、录像/云存储标志、工厂模式 |
| 升级 100% 后异常 | 真实接收长度、每个分区校验/查表/写入结果、MCU ACK；100% 状态不能证明写入成功 |

这些是源码定位入口，不是已经验证的设备故障诊断结果。现场日志应脱敏后保留事件编号、状态、长度、返回值和时间戳。

---

## 19. 核验方法与后续验证

### 19.1 可复查的材料

- [verify_source.py](evidence/verify_source.py)：只读清点源码、计算快照指纹与词法次数、执行五个 make -n 组合，提取三个原函数编译隔离主机程序。
- [verification.json](evidence/verification.json)：保留命令参数、退出码、缺失依赖、词法统计、函数哈希及主机输出。

从本文档目录复核：

```sh
python3 evidence/verify_source.py /home/tronlong/lyp/meari/lecamera_app > /tmp/lecamera-verification.json
```

树哈希按相对路径字典序遍历每个文件，将 UTF-8 相对路径、NUL 与该文件 SHA-256 的 32 字节摘要依次送入 SHA-256。忽略目录时间、权限等元数据；文件增加或内容改变会改变指纹。当前源码树指纹与核验起点一致。

主机程序直接从当前快照提取函数体，替换外围类型、配置获取、HAL 和延时；没有改写被核验函数的控制逻辑。断言要求观察到文中描述的行为，并包含正确凭据/错误凭据与写成功的对照。它们是缺陷复现材料，不是修复后的回归测试；将来修复源码后断言需要调整。

### 19.2 已完成与未完成

| 项目 | 本次状态 |
|------|----------|
| 全文件树统计与指纹 | 已完成，1364 文件；源码文件内容未修改 |
| 构建/启动/事件/网络/配置/升级/存储/回放/HTTP 关键路径静态复核 | 已完成，定位见各章 |
| 五个构建组合 Makefile 干运行 | 已完成；仅 b8 所测组合可展开命令 |
| 空/前缀认证、CA 初始化、写入持续失败原函数主机核验 | 已完成并复现 |
| 整个工程交叉编译、链接、打包 | 未完成：工具链、BSP、预编译库与 pack 缺失 |
| TLS 握手与网络证书失败实测 | 未执行 |
| eventhub 并发销毁、回放线程创建失败动态复现 | 未执行，依据静态锁路径 |
| 升级/二维码/HTTP 解析 sanitizer 或 fuzz | 未执行，边界问题按静态证据描述 |
| 真实 Flash 写入、断电恢复、MCU/4G/HGIC、SDK 上线、功耗实测 | 未执行，需要目标设备及完整依赖 |

### 19.3 面向后续维护的验证用例

认证应覆盖空字段、正确前缀、最大长度、不同账户、完整正确值以及 HTTP Basic/Digest 两条路径；TLS 应覆盖有效、不可信、过期、主机名不匹配证书。升级应覆盖 1～63 字节短包、header_length 太小/太大、文件描述越界、文件名未终止、分区未知、坏校验、擦写失败、下载截断与并发请求。

配置应覆盖主配置丢失、备份有效、两份损坏、短读/短写、长度损坏与写入途中断电；事件应覆盖满队列、多生产者、销毁时机、阻塞订阅者与发布 data 生命周期；回放应覆盖 SD 拔卡、重复 start/seek/stop、四通道边界和线程创建失败。低功耗应分别测 PIR、门铃、Wi-Fi 唤醒、SDK 激活、SD 格式化、升级和 standby 失败兜底。

完成这些验证后，才可把本报告中的静态风险提升为已确认的产品故障、或将修复记录为目标设备通过。
