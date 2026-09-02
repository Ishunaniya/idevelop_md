# ROOTCLOUD STP SDK 源码详解

> 版权 © 2016-2025 树根互联股份有限公司（ROOTCLOUD）  
> 本文档根据源码分析整理，供内部开发人员阅读。

---

## 目录

1. [项目定位与背景](#1-项目定位与背景)
2. [整体架构](#2-整体架构)
3. [核心层 erk-core](#3-核心层-erk-core)
   - 3.1 [ERK — 异步 MQTT 引擎](#31-erk--异步-mqtt-引擎)
   - 3.2 [STP — 三一传输协议](#32-stp--三一传输协议)
   - 3.3 [RCPv4 — 云端协议实现层](#33-rcpv4--云端协议实现层)
   - 3.4 [RCPv4-Utils — 工具库](#34-rcpv4-utils--工具库)
   - 3.5 [erk-log / erk-utils](#35-erk-log--erk-utils)
4. [插件层 erk-plugins](#4-插件层-erk-plugins)
   - 4.1 [插件系统设计](#41-插件系统设计)
   - 4.2 [scp-adapter — 三一 CAN 总线适配器](#42-scp-adapter--三一-can-总线适配器)
   - 4.3 [dtp-adapter — 通用设备适配器](#43-dtp-adapter--通用设备适配器)
   - 4.4 [virtual-adapter — 虚拟设备适配器](#44-virtual-adapter--虚拟设备适配器)
5. [可执行文件 erk-agent](#5-可执行文件-erk-agent)
6. [OTA 守护 erk-otad](#6-ota-守护-erk-otad)
7. [第三方依赖 erk-3rd-party](#7-第三方依赖-erk-3rd-party)
8. [配置文件体系](#8-配置文件体系)
9. [完整数据流](#9-完整数据流)
10. [关键设计模式](#10-关键设计模式)
11. [编译与交叉编译](#11-编译与交叉编译)
12. [工具集 tools](#12-工具集-tools)

---

## 1. 项目定位与背景

### 是什么

**ROOTCLOUD STP SDK**（代号 ERK）是树根互联为"三一新C项目"开发的**工业物联网边缘网关 SDK**，全部使用 C 语言实现，可运行在：

- x86-64 Linux（开发机/服务器）
- ARMv5te / ARMv7 / ARMv8（嵌入式 Linux 网关）

### 解决什么问题

三一重工的工程机械（叉车、起重机、挖掘机等）通过 **CAN 总线**采集运行数据，本 SDK 作为运行在车载/路侧网关上的中间件，负责：

1. **向下**：通过 J1939/SCP 协议解析 CAN 总线数据
2. **向上**：通过 MQTT 将数据上报到树根互联云平台（RootCloud）
3. **双向**：接收云端下发的远程指令（OTA 升级、配置下发、远程终端等）

### 版本信息

| 组件 | 版本 |
|------|------|
| STP 协议库 | v0.6.6.38 |
| RCPv4 协议库 | v0.17.12.128 |
| SDK 打包产物示例 | erk-agent_0.3.1.31_x86_64 |

---

## 2. 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                     ROOTCLOUD 云平台                              │
│                  (MQTT Broker / RCPv4 Server)                    │
└──────────────────────────┬──────────────────────────────────────┘
                           │ MQTT over TCP/TLS
                           │
┌──────────────────────────▼──────────────────────────────────────┐
│                        erk-agent (主进程)                         │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                  erk-core 核心协议栈                         │  │
│  │                                                            │  │
│  │  ┌──────────┐   ┌──────────┐   ┌───────────────────────┐  │  │
│  │  │   ERK    │   │   STP    │   │        RCPv4          │  │  │
│  │  │ 异步MQTT │◄──│ 协议编   │◄──│ 云端业务协议层         │  │  │
│  │  │  引擎    │   │  解码层  │   │ (命令/影子/OTA/配置等)  │  │  │
│  │  └──────────┘   └──────────┘   └───────────────────────┘  │  │
│  └──────────────────────────────────────────────────────────┬─┘  │
│                                                             │     │
│  ┌──────────────────────────────────────────────────────────▼─┐  │
│  │                    erk-plugins 插件层                        │  │
│  │                                                             │  │
│  │   ┌─────────────┐  ┌─────────────┐  ┌──────────────────┐  │  │
│  │   │ scp-adapter │  │ dtp-adapter │  │ virtual-adapter  │  │  │
│  │   │ (CAN/J1939) │  │ (通用设备)  │  │ (虚拟/测试)       │  │  │
│  │   └──────┬──────┘  └──────┬──────┘  └──────────────────┘  │  │
│  └──────────┼────────────────┼─────────────────────────────────┘  │
└─────────────┼────────────────┼──────────────────────────────────┘
              │                │
    ┌─────────▼──────┐  ┌──────▼────────┐
    │  CAN 总线设备   │  │  其他工业设备  │
    │ (J1939/SCP)    │  │ (Modbus等)    │
    │ 叉车/起重机等   │  │               │
    └────────────────┘  └───────────────┘
```

### 模块目录速览

```
rootcloud-stp-sdk/
├── erk-core/           # SDK核心：三层协议栈 + 工具
│   ├── erk/            # 第1层：ERK 异步MQTT引擎
│   ├── stp/            # 第2层：STP 协议编解码
│   ├── rcpv4/          # 第3层：RCPv4 业务协议（含OTA/影子/命令等）
│   ├── rcpv4-utils/    # 加密/下载/INI解析等工具库
│   ├── erk-log/        # 日志模块
│   ├── erk-utils/      # 通用工具
│   ├── include/        # 对外公开头文件
│   ├── profiles/       # 设备Profile配置（数据点定义）
│   ├── config.json     # SDK主配置
│   ├── endpoints.json  # MQTT云端连接配置
│   └── benchmark/      # 性能基准测试
│
├── erk-plugins/        # 设备适配插件
│   └── plugins/
│       ├── scp-adapter/    # 三一CAN/J1939适配器
│       ├── dtp-adapter/    # 通用设备适配器
│       └── virtual-adapter/# 虚拟设备（测试用）
│
├── erk-agent/          # 主进程可执行文件
├── erk-otad/           # OTA守护脚本
├── erk-3rd-party/      # 第三方依赖库源码
├── cmake/              # 交叉编译工具链配置
└── tools/              # 部署工具（Windows桌面端）
```

---

## 3. 核心层 erk-core

`erk-core` 是整个 SDK 的基础，实现了一套从 MQTT 传输到业务协议的**三层协议栈**：

```
应用/插件层
    ↕  rcpv4_handle_t API
第3层：RCPv4   ← 业务语义（命令/影子/OTA/配置/文件传输）
    ↕  stp_xxx API
第2层：STP     ← 数据序列化/反序列化（CBOR二进制格式）
    ↕  erk_mqtt_t API
第1层：ERK     ← MQTT 连接管理（libev + mosquitto）
    ↕  TCP/TLS
云端 MQTT Broker
```

---

### 3.1 ERK — 异步 MQTT 引擎

**源码位置**：`erk-core/erk/`  
**公开头文件**：`erk-core/include/erk/erk.h`

#### 定位

> ERK = **E**dgewise for **R**ootCloud **K**it

ERK 是 SDK 最底层的网络引擎，封装了 `libev`（事件循环）和 `mosquitto`（MQTT 客户端库），提供**单线程、非阻塞、无锁**的异步 MQTT 通信能力。

#### 四大核心对象

| 对象 | 类型 | 职责 |
|------|------|------|
| `erk_handle_t` | 引擎句柄 | 持有 libev 事件循环实例，调用 `erk_handle_run` 驱动整个 I/O 轮询 |
| `erk_mqtt_t` | 连接实体 | 代表一条 MQTT 连接，负责连接/断连/自动重连，管理 PUBLISH/SUBSCRIBE |
| `erk_mqtt_mgr_t` | 连接管理器 | 管理多条 `erk_mqtt_t`，分发 pub/sub 数据，回调用户例程 |
| `erk_instance_t` | 应用实例 | 控制整个应用生命周期，`erk_instance_run` 可阻塞或非阻塞运行 |

#### 关键特性

- **单线程可重入**：所有 ERK API 在同一线程内可重入，但不跨线程安全（与 libev 设计一致）
- **自动重连**：`erk_mqtt_t` 在网络抖动或 TCP 超时后自动重连 MQTT Broker
- **TLS 支持**：通过 `erk_mqtt_tls_opt_t` 配置双向 TLS，支持 CA 证书/客户端证书/私钥注入
- **多连接**：一个 `erk_handle_t` 可绑定多个 `erk_mqtt_t`，支持同时连接多个 Broker
- **遥测计数器**：内置 `erk_counter_t` 统计 TCP/MQTT 层面的收发包数

#### MQTT 连接参数（`erk_mqtt_conn_opt_t`）

```c
// 关键字段摘要：
connect_timeout   // 连接超时（秒）
clean_session     // MQTT Clean Session 标志
keep_alive        // 心跳间隔（秒）
client_id         // 客户端ID，RCPv4 中须等于 username
will_topic/message// 遗嘱消息（Last Will）
username/password // MQTT 认证
```

#### 源文件

| 文件 | 内容 |
|------|------|
| `erk.c` | 核心事件循环、连接状态机、pub/sub 分发 |
| `erk-instance.c` | 应用实例生命周期管理 |

---

### 3.2 STP — 三一传输协议

**源码位置**：`erk-core/stp/`  
**公开头文件**：`erk-core/include/stp/stp.h`

#### 定位

> STP = **S**ANY **T**ransmission **P**rotocol（三一传输协议）

STP 是三一与树根互联联合定义的**私有数据交换协议**，是 RCPv4 与 MQTT 之间的序列化层，使用 **CBOR**（Concise Binary Object Representation，RFC 7049）作为编码格式，相比 JSON 更紧凑，适合带宽受限的工业现场网络。

#### STP 定义的数据结构

**地理位置（GPS + 基站双模定位）**

```c
// GPS 定位（NMEA GPRMC 格式）
typedef struct stp_location_gprmc {
    int64_t time;              // UTC 时间戳（ms）
    float latitude/longitude;  // 纬度/经度
    float speed;               // 地面速度
    float azimuth;             // 地面方向角
    float magnetic;            // 磁偏角
    stp_positioning_status_t status; // 定位是否有效
    ...
} stp_location_gprmc_t;

// GPS 精度信息（NMEA GPGGA 格式）
typedef struct stp_location_gpgga {
    uint8_t star_num;          // 可见卫星数量
    float horizontal;          // 水平精度因子 (HDOP)
    float ant_altitude;        // 天线海拔高度
    ...
} stp_location_gpgga_t;

// 基站定位（LTE Cell）
typedef struct stp_location_cell {
    int mcc;    // 移动国家码
    int mnc;    // 移动网络码
    uint16_t lac; // 位置区码
    int cid;    // 基站小区ID
    ...
} stp_location_cell_t;
```

**数据点类型（`stp_data_type_t`）**

STP 定义了一套工业数据的类型系统，涵盖：
- 基本类型：`uint8/16/32/64`、`int8/16/32/64`、`float`、`double`、`bool`、`string`
- 结构类型：`array`（数组）、`map`（键值对）
- 位置类型：`gprmc`、`gpgga`、`gpgsv`、`cell`

**上报类型（`stp_thing_report_type_t`）**：控制数据上报策略（变化上报/周期上报/完整上报等）。

**Profile 文件描述**（`stp_profile_file_desc_t`）：描述需要从云端下载的 Profile 文件（含文件名、版本、校验算法、签名值）。

#### 源文件

| 文件 | 内容 |
|------|------|
| `stp.c` | STP 数据结构的构建/销毁/管理 |
| `stp_cbor.c` | STP ↔ CBOR 的编解码实现 |
| `stp_cbor_internal.h` | CBOR 编解码内部工具函数 |

---

### 3.3 RCPv4 — 云端协议实现层

**源码位置**：`erk-core/rcpv4/`  
**公开头文件**：`erk-core/include/rcpv4/rcpv4.h`

#### 定位

> RCPv4 = **R**ootCloud **P**rotocol **V**ersion **4**

RCPv4 是协议栈的最高层，也是整个 SDK 的"业务大脑"。它构建在 STP（数据序列化）和 ERK（MQTT 传输）之上，实现设备与云平台之间完整的双向通信业务逻辑。它同时也是插件系统的宿主，负责加载并调度所有设备适配插件。

RCPv4 兼容多个协议版本：`rcp_type_mqtt_bin_v2v3`（旧版二进制协议）、`rcp_type_mqtt_json_v2v3`（旧版JSON协议）、`rcp_type_v4`（当前主力版本）。

#### 核心句柄 `rcpv4_handle_t`

`rcpv4_handle_t` 是整个 RCPv4 运行时的中心对象，内部持有：
- `erk_mqtt_t` 的引用（MQTT 连接）
- 插件链表（加载的 `device_adapter_t`）
- 各子系统的句柄（OTA、影子、配置同步、缓存、性能监控等）
- `rcpv4_task_t` 异步任务队列

#### 子模块详解

**① 命令处理子系统（rcpv4-cmd-*）**

每个 `cmd` 子模块对应一类从云端下发的指令类型：

| 子模块文件 | 命令类型 | 功能说明 |
|-----------|---------|---------|
| `rcpv4-cmd-action.c/h` | ACTION | 执行一次性动作指令（如触发某个操作） |
| `rcpv4-cmd-config.c/h` | CONFIG | 接收云端下发的配置参数并应用 |
| `rcpv4-cmd-shadow.c/h` | SHADOW | 设备影子：维护设备期望状态与实际状态的同步 |
| `rcpv4-cmd-file.c/h` | FILE | 文件传输：接收云端推送的文件（如配置文件、规则包） |
| `rcpv4-cmd-live.c/h` | LIVE | 实时数据流：云端请求设备推送实时数据 |
| `rcpv4-cmd-info.c/h` | INFO | 设备基础信息上报（型号、版本、SN 等） |
| `rcpv4-cmd-time.c/h` | TIME | NTP 时钟同步：从云端校准设备时间 |
| `rcpv4-cmd-schema.c/h` | SCHEMA | Schema 定义管理 |
| `rcpv4-cmd-cache.c/h` | CACHE | 命令缓存处理 |
| `rcpv4-cmd-response.c/h` | RESPONSE | 统一指令响应封装 |
| `rcpv4-cmd-lock.c/h` | LOCK | 设备锁定/解锁控制（工程机械特有的远程锁机功能） |

**② OTA 升级子系统（rcpv4-ota-*）**

OTA（Over-the-Air）是 RCPv4 最复杂的子系统之一，实现了一套完整的**固件/软件远程升级状态机**：

```
云端下发 OTA 任务
        ↓
  DOWNLOAD（下载包）─── rcpv4-adl（异步下载器）+ curl
        ↓
  CHECK（完整性校验）── MD5 / SHA256 校验
        ↓
  UNZIP（解压缩）────── minizip
        ↓
  TRANS/DISTRIBUTE（分发到子设备）
        ↓
  PROGRAMMING（烧写/刷写）── 调用插件的 on_ota 回调
        ↓
  SETUP（安装）
        ↓
  WAIT_CONFIRM（等待设备确认）
        ↓
  DONE / ROLLBACK（完成 或 回滚）
```

OTA 子系统支持：
- 最多同时进行 5 个 OTA 任务（`ota_task_nums: 5`）
- 超时重试（默认超时 1800 秒，重试间隔 10 秒）
- 断点续传（`rcpv4_aul_t` 支持 `breakpoint_resume`）
- 完整的错误码体系（30+ 种错误类型，涵盖包损坏/网络弱/磁盘不足/解密失败等）
- 升级包加密验证（支持 MD5/SHA256）

| 文件 | 职责 |
|------|------|
| `rcpv4-ota.c/h` | OTA 状态机主逻辑，处理云端 OTA 命令 |
| `rcpv4-ota-cache.c/h` | OTA 任务状态持久化（断电续传） |
| `rcpv4-ota-file-table.c/h` | OTA 文件清单管理 |
| `rcpv4-ota-info.c/h` | OTA 进度/状态向云端上报 |

**③ 异步文件传输（rcpv4-adl / rcpv4-aul）**

- `rcpv4-adl`（Async Download Library）：异步 HTTP 下载器，支持进度回调、限速、超时
- `rcpv4-aul`（Async Upload Library）：异步上传器，支持断点续传、文件签名计算

**④ 设备影子（rcpv4-cmd-shadow）**

"设备影子"是 IoT 平台的标准概念：云端维护一份设备状态的"镜像"（Shadow），即使设备离线，云端也能保存期望状态，设备上线后自动同步。

RCPv4 影子功能：
- 定时上报影子（默认间隔 1800 秒）
- 关联 OTA 信息（OTA 进度写入影子）
- 校验 username 防止 Shadow 数据篡改

**⑤ 数据分组采样（rcpv4-grouping）**

负责将来自插件的海量数据点按**分组（Group）**进行采样和聚合后再上报，避免每个数据点单独上报导致的 MQTT 流量爆炸。

支持两种采样模式：
- `default`：默认采样
- `window`：滑动窗口采样（当前默认模式）

关键参数（来自 thing-profile.json 的 Group 定义）：
- `samplePeriod`：采样周期（ms），最小 50ms
- `reportPeriod`：上报周期（ms），最小 500ms，最大 1800000ms（30分钟）
- `fullCycleTime`：全量上报周期（ms）

**⑥ 离线缓存（rcpv4-hook 中的 Cache 部分）**

当 MQTT 断网时，RCPv4 将待发送的数据写入本地 **SQLite 数据库**缓存；网络恢复后自动重放。

配置参数（`config.json` 中）：
```json
"database": {
    "enabled": true,
    "query": { "rows": 1, "timeout": 0, "report_timeout": 200 }
}
```

**⑦ 数据转发（rcpv4-forwarder）**

将 RCPv4 接收或产生的数据转发到其他目标：
- **stub-linker**：通过 nanomsg pub/sub 转发到本机其他进程
- **mqtt-broker**：转发到另一个本地 MQTT Broker（如 mosquitto）

这使得 SDK 可以作为**本地数据总线**，同时为多个应用提供设备数据。

**⑧ 远程终端（rcpv4-rtty）**

集成 `erk-rtty` 库，实现通过云端对设备进行**远程 Shell 访问**（类似 SSH over MQTT）。

```c
rcpv4_rtty_run(proto_handle);            // 启动 RTTY 守护
rcpv4_rtty_update_server_address(...);   // 动态更新 RTTY 服务器地址
```

**⑨ 配置同步（rcpv4-config-sync）**

负责从云端同步 SDK 的运行时配置：
- 支持最大 20MB 的配置文件（`"max_file_size": "20M"`）
- 本地缓存上次同步的配置（`rcpv4-config-sync-cache`）

**⑩ 任务调度（rcpv4-task）**

`rcpv4_task_t` 是 RCPv4 的**内部异步任务队列**，所有跨子系统的异步操作（MQTT 消息处理、文件下载完成通知、定时任务、Profile 更新等）都通过它调度。

任务类型（`rcpv4_task_data_type_t`）：

| 任务类型 | 触发来源 |
|---------|---------|
| `mqtt` | 收到 MQTT 消息 |
| `adl` | 下载任务进度/完成 |
| `aul` | 上传任务完成 |
| `ahrr` | 重连后重放缓存 |
| `timer` | 定时器触发 |
| `profile` | Profile 文件更新 |
| `config_sync` | 配置同步触发 |
| `lib_unload` | 插件卸载 |

**⑪ 性能监控（rcpv4-perf）**

读取 `perf.json` 配置文件，采集并上报 SDK 自身的性能指标（CPU、内存、消息吞吐量等），供云端监控。

#### MQTT Payload 编码

RCPv4 支持多种 MQTT Payload 编码方式（配置项 `mqtt_payload.encoding`）：

| 编码 | 说明 |
|------|------|
| `default` | STP 原生 CBOR 二进制 |
| `cbor` | 显式 CBOR 模式 |
| `jsongz` | JSON + gzip 压缩 |

---

### 3.4 RCPv4-Utils — 工具库

**源码位置**：`erk-core/rcpv4-utils/`

这是一个内部工具集，为 RCPv4 和插件提供常用功能：

| 文件 | 功能 |
|------|------|
| `base64.c` | Base64 编解码 |
| `md5.c` | MD5 哈希计算（OTA 包校验） |
| `sha256.c` | SHA-256 哈希计算（OTA 包校验） |
| `aes.c` | AES 加密（包解密） |
| `ecdsa.c` | ECDSA 签名验证 |
| `ecies.c` | ECIES 椭圆曲线集成加密 |
| `download.c` | 基于 curl 的 HTTP 下载封装 |
| `iniparser.c` | INI 配置文件解析（用于 stub 工具的 .ini 配置） |
| `utils.c` | 时间、字符串、文件系统等通用工具 |
| `uthash.h`（头文件） | 轻量级哈希表宏库（第三方 uthash） |

---

### 3.5 erk-log / erk-utils

**源码位置**：`erk-core/erk-log/`、`erk-core/erk-utils/`

#### erk-log

SDK 统一日志模块，通过 `config.json` 配置：

```json
"log": {
    "level": 4,         // 日志级别（DEBUG/INFO/WARN/ERROR）
    "count": 5,         // 滚动日志文件数量
    "size": "20M",      // 单个日志文件最大大小
    "file": "logs/erk-agent.log"
}
```

#### erk-utils

通用工具函数集，包括时间处理、字符串操作、内存管理辅助函数等，供 ERK、STP、RCPv4 各层调用。同时定义了 GCC 分支预测宏 `erk_likely` / `erk_unlikely` 用于性能优化。

---

## 4. 插件层 erk-plugins

### 4.1 插件系统设计

RCPv4 采用**动态库（.so）插件架构**。每个插件：
1. 编译为独立的 `.so` 共享库
2. 运行时由 RCPv4 通过 `dlopen` 动态加载
3. 必须导出两个入口函数：

```c
// 插件初始化：返回设备适配器实例
device_adapter_t *device_adapter_create(rcpv4_handle_t *proto_handle);

// 插件释放：清理所有资源
int device_adapter_release(device_adapter_t *da);
```

**`device_adapter_t` 接口**是插件与 RCPv4 的契约，包含以下函数指针：

| 回调函数指针 | 调用时机 |
|------------|---------|
| `start` | 插件启动 |
| `stop` | 插件停止 |
| `execute_cmd` | 收到云端命令 |
| `notify_profile_update` | Profile 文件更新时 |
| `get_connection_id` | 获取设备连接ID列表 |
| `on_ota` | 收到 OTA 升级任务 |
| `on_running_status` | 运行状态变化（暂停/恢复） |

插件在 `config.json` 中声明并启用：

```json
"plugins": {
    "device": {
        "scp-adapter": { "enabled": true, "connection_ids": [""] },
        "dtp-adapter": { "enabled": false, "connection_ids": [""] }
    }
}
```

---

### 4.2 scp-adapter — 三一 CAN 总线适配器

**源码位置**：`erk-plugins/plugins/scp-adapter/`

这是 SDK 最核心的插件，专为三一工程机械的 CAN 总线数据采集而设计。

#### 支持的协议栈

```
三一工程机械 ECU（Electronic Control Unit）
    │
    │ CAN 总线（250kbps / 500kbps）
    ▼
can-data-link（CAN 硬件抽象层）
    │ J1939 PDU（SA + PGN + Data）
    ▼
j1939-adapter（J1939 协议解析）
    │
    ├── j1939-scp（SCP 2.0 自定义扩展）
    ├── DBC 文件解析（cantools/dbcreader）
    │
    ▼
scp-adapter（数据处理与上报）
    │
    ▼
rcpv4（上报到云端）
```

#### 核心子组件详解

**① CAN 数据链路层（`can-data-link.c/h`）**

CAN 硬件抽象层，定义了对 CAN 控制器的标准操作接口：

```c
int32_t can_hw_open(j1939_adapter_t *ja);    // 打开 CAN 设备
int32_t can_hw_close(j1939_adapter_t *ja);   // 关闭 CAN 设备
int32_t can_hw_poll(j1939_adapter_t *ja);    // 轮询 CAN 消息
int32_t can_msg_hw_recv(j1939_adapter_t *ja, j1939_adapter_msg_t *ja_msg); // 接收
int32_t can_msg_hw_send(j1939_adapter_t *ja, j1939_adapter_msg_t *ja_msg); // 发送
int32_t can_msg_hw_filter(j1939_adapter_t *ja, ...); // 硬件过滤
```

> **注意**：这是**开发者自定义区域**，文件注释明确说明"开发人员可以修改此部分代码，以便采集真实设备的数据"，然后转换为 J1939 格式与 scp-adapter 交互。

此外，`can-data-link.c` 也负责 GPS 位置数据的上报（`post_gps_location`）。

**② J1939 适配器（`scp-j1939-adapter.c/h`）**

封装 `j1939-adapter` 第三方库，实现 J1939 协议的解析：

J1939 PDU 结构（`j1939_adapter_msg_t`）：
```
┌──────┬──────┬──────┬──────┬──────┬────────────────────────┐
│  P   │ EDP  │  DP  │  PF  │  PS  │          SA            │
│(3bit)│(1bit)│(1bit)│(8bit)│(8bit)│         (8bit)         │
└──────┴──────┴──────┴──────┴──────┴────────────────────────┘
  优先级       数据页    PDU格式  PDU特定    源地址
                    └──── PGN = DP+PF+PS（参数组号）────┘
Data[0..1785]  // 最大 1785 字节数据载荷
```

scp-adapter 通过 `j1939_adapter_item_t` 维护每个设备节点（SA）的状态，包括 PGN/SPN 的映射关系、DBC 信号解析结果。

**③ DBC 文件解析（cantools/dbcreader）**

`.dbc`（Database CAN）文件是 CAN 总线信号定义的行业标准格式，描述每个 CAN ID 的信号名称、起始位、长度、缩放因子、单位等。

scp-adapter 通过 `cantools`/`dbcreader` 第三方库解析 DBC 文件，将原始 CAN 字节流解码为带物理含义的工程量值，再映射到 STP 数据点（SPN）。

从 connection-profile.json 可见，实际项目中 DBC 文件以 MD5 校验的方式从云端下载并管理版本。

**④ 规则过滤（`scp-rule-filter.c/h`）**

提供基于 HTTP REST 的动态规则注册/删除接口：
```json
"rule-filter": {
    "rule-add":   "http://127.0.0.1:18091/v1/rule/add",
    "rule-del":   "http://127.0.0.1:18091/v1/rule/del",
    "rule-clear": "http://127.0.0.1:18091/v1/rule/clear"
}
```
通过 nanomsg pub/sub 连接到本地的规则过滤服务，实现数据的动态过滤与路由。

**⑤ 原始数据缓存（`scp-raw-data-cache.c/h`）**

负责将原始 CAN 数据以 SHA256 签名的方式缓存到本地磁盘，并定期导出：

```json
"raw-data": {
    "retries": 3,
    "cycles": 200,
    "raw_path": "raw-data",
    "sign_method": "SHA256",
    "percent": 95,          // 磁盘使用率超过95%时停止缓存
    "export": {
        "copy-buffer": 1048576  // 每次导出 1MB
    }
}
```

**⑥ 配置同步（`scp-config-sync.c/h`）**

插件层面的配置同步，负责将云端下发的 scp-adapter 专用配置（数据点列表、采样参数等）同步到本地并应用。

**⑦ Profile 同步（`scp-profile-sync.c/h`）**

负责管理 Profile 文件的生命周期：
- 接收云端通知的 Profile 更新
- 下载新版 Profile 文件（含版本校验）
- 热切换 Profile（`scp_profile_update_step_t`：begin → ready → end）
- 清理旧版 Profile 文件

**⑧ 设备锁定（通过 `rcpv4-cmd-lock` 回调）**

工程机械特有的**远程锁机/解锁**功能：
- 支持多种锁定类型
- 上报锁定状态（周期/变化/完整上报）
- 处理解锁命令的成功/失败/超时

锁定状态上报策略（`lock-status-report` 配置）：
```json
"report-type": "complete-change",
"sample-period": 10000,
"full-cycle-time": 600000
```
含义：每 10 秒采样一次，发生变化时上报完整状态；无论如何每 10 分钟做一次全量上报。

**⑨ 任务调度参数**

```json
"task": {
    "cycles": 5,                  // 主循环周期（ms）
    "can_msg_cycles": 5,          // CAN 消息处理周期（ms）
    "can_msg_queue_max_size1": 30000  // CAN 消息队列最大长度
}
```

#### Stub 工具集（`tools/`）

scp-adapter 提供了一套 **stub（桩）程序**，模拟实际设备向 scp-adapter 发送 CAN 数据，用于开发调试：

| 工具 | 用途 |
|------|------|
| `stub-sany-forklift` | 模拟三一叉车 CAN 数据 |
| `stub-sany-crane` | 模拟三一起重机 CAN 数据 |
| `stub-sany-ota` | 模拟 OTA 升级流程 |
| `stub-simulator` | 通用模拟器 |
| `stub-demo` | 最简示例 |
| `stub-rdexp-flash` | RD Export 数据刷写工具 |

stub 工具通过 **stub-linker 协议**（基于 nanomsg TCP pub/sub）与 scp-adapter 通信：

```
stub-sany-forklift ──── nanomsg TCP ────► scp-adapter
  pub data: tcp://127.0.0.1:11226        sub data: tcp://127.0.0.1:11225
  sub cmd:  tcp://127.0.0.1:11215        pub cmd:  tcp://127.0.0.1:11216
```

---

### 4.3 dtp-adapter — 通用设备适配器

**源码位置**：`erk-plugins/plugins/dtp-adapter/`

> DTP = **D**evice **T**ransmission **P**rotocol

`dtp-adapter` 是比 scp-adapter 更通用的设备适配器，不绑定 CAN/J1939 协议，面向通过其他工业协议（如 Modbus RTU/TCP、RS-232、OPC UA 等）接入的设备。

#### 核心组件

| 文件 | 职责 |
|------|------|
| `dtp-adapter.c` | 插件主逻辑，实现 `device_adapter_t` 接口 |
| `dev-data-link.c/h` | 通用设备数据链路抽象层（可自定义实现） |
| `dtp.c` | DTP 协议数据处理 |
| `dtp-rule-filter.c/h` | 数据规则过滤 |

与 scp-adapter 的 `can-data-link.c` 类似，`dev-data-link.c` 也是**开发者自定义区域**，允许接入任意通信协议的设备。

---

### 4.4 virtual-adapter — 虚拟设备适配器

**源码位置**：`erk-plugins/plugins/virtual-adapter/`

纯软件实现的虚拟设备，用于：
- 开发和调试上层协议逻辑时不依赖真实硬件
- 测试 SDK 功能（Profile 加载、OTA 流程、配置同步等）

---

## 5. 可执行文件 erk-agent

**源码位置**：`erk-agent/`

`erk-agent` 是整个 SDK 的**主进程**，也是最终部署到设备上的可执行文件。

### 启动流程

```
erk-agent 启动
    │
    ├── 1. 读取 config.json（日志、MQTT 参数、插件列表）
    ├── 2. 读取 endpoints.json（云端 MQTT Broker 地址/认证）
    ├── 3. 初始化 erk-log 日志系统
    ├── 4. 创建 rcpv4_handle_t（初始化协议栈）
    ├── 5. 加载 profiles/（thing-profile、data-profile、connection-profile）
    ├── 6. dlopen 加载插件目录中启用的 .so 插件
    │      └── 调用各插件的 device_adapter_create()
    ├── 7. 建立 MQTT 连接（带 TLS 可选）
    ├── 8. 启动 rcpv4 任务队列
    └── 9. 进入 erk 事件循环（阻塞）
```

### 运行目录结构（安装后）

```
erk-root/
├── images/
│   └── erk-agent_0.3.5.100_armv7-a/   ← SDK 版本目录
│       ├── erk-agent                   ← 主可执行文件
│       ├── config.json                 ← SDK 主配置
│       ├── profiles/                   ← 设备 Profile
│       ├── plugins/                    ← 插件目录
│       │   └── scp-adapter/
│       │       ├── libscp-adapter.so
│       │       └── plugin.json
│       ├── logs/                       ← 日志目录
│       └── install.sh                  ← 安装脚本
└── endpoints.json                      ← MQTT 连接配置（独立于版本目录）
```

> **重要**：SDK 运行依赖相对路径，必须从 `erk-root` 目录（通过 `erk-daemon.sh`）间接启动，直接运行会因路径问题失败。

### 关键配置参数说明（config.json）

```json
{
  "rcpv4": {
    "multiple_access": {              // 多连接管理
      "enabled": true,
      "switch_timeout": 10000,        // 连接切换超时（ms）
      "reconnecting_timeout": 600000  // 重连超时（ms，10分钟）
    },
    "timing": {                       // 时钟同步
      "enabled": true,
      "interval": 86400,              // 同步间隔（秒，每天一次）
      "diff": 30000                   // 允许时差（ms，30秒内不同步）
    },
    "cell_site": {                    // 4G 基站信号质量监控
      "enabled": true,
      "rsrp": -108,                   // RSRP 阈值（dBm）
      "sinr": 13,                     // SINR 阈值（dB）
      "interval_time": 30             // 上报间隔（秒）
    }
  }
}
```

---

## 6. OTA 守护 erk-otad

**源码位置**：`erk-otad/`

这个目录不包含 C 代码，而是一组 **Shell 脚本**，构成 SDK 在设备上的**守护进程体系**：

| 脚本 | 职责 |
|------|------|
| `erk-daemon.sh` | SDK 主守护脚本，监控 erk-agent 进程，崩溃后自动重启 |
| `erk-otad.sh` | OTA 守护脚本，监控 OTA 升级流程 |
| `app-daemon.sh` | 应用层守护脚本 |
| `app-otad.sh` | 应用层 OTA 脚本 |
| `erk-stubd.sh` | stub 守护脚本（开发调试用） |
| `erk-redistributable.sh` | 可再发行组件脚本 |
| `install.sh` | SDK 安装脚本（仅能运行一次） |
| `make-version.sh` | 版本信息生成脚本 |
| `ota_progress` | OTA 进度记录文件 |
| `ota_update` | OTA 更新触发脚本 |
| `package_version` | 包版本信息 |
| `version` | SDK 版本号文件 |

---

## 7. 第三方依赖 erk-3rd-party

所有依赖库以**源码方式**内嵌到项目中，随 SDK 一起编译，确保不同平台的兼容性。

### 核心依赖

| 库名 | 版本目录 | 用途 |
|------|---------|------|
| `mosquitto-git` | `mosquitto-git/` | MQTT 客户端（ERK 的底层） |
| `libev-git` | `libev-git/` | 事件循环（ERK 的底层） |
| `openssl-git` | `openssl-git/` | TLS/SSL 加密通信 |
| `curl-git` | `curl-git/` | HTTP 下载（ADL/AUL 文件传输） |
| `libcbor-git` | `libcbor-git/` | CBOR 序列化（STP 协议） |
| `cjson-git` | `cjson-git/` | JSON 解析（配置文件/Profile） |
| `sqlite-git` | `sqlite-git/` | 离线数据缓存 |
| `nanomsg-git` | `nanomsg-git/` | IPC 通信（stub-linker/rule-filter） |
| `zlib-git` | `zlib-git/` | 压缩/解压（OTA 包解压、jsongz 编码） |
| `minizip-git` | `minizip-git/` | ZIP 格式 OTA 包解压 |

### 设备协议相关

| 库名 | 用途 |
|------|------|
| `j1939-scp-git` | J1939 CAN 协议栈（三一扩展版本 SCP 2.0/2.1） |
| `j1939-adapter-git` | J1939 PDU 解析适配层（自研） |
| `cantools-git` | DBC 文件解析（CAN 信号定义解码） |
| `libmodbus-git` | Modbus RTU/TCP 协议（工业现场总线） |

### 工具与通信

| 库名 | 用途 |
|------|------|
| `stub-linker-git` | stub 进程与 scp-adapter 之间的通信协议（自研，基于 nanomsg） |
| `dev-linker-git` | 设备数据链路协议（自研） |
| `hedge-linker-git` | 桥接链接库（自研） |
| `erk-rtty-git` | 远程 Shell 终端（基于 MQTT 的 SSH 替代方案） |
| `erk-tweaker-git` | 运行时参数调整工具（自研） |
| `erk-keyring-git` | 密钥管理（设备认证/证书存储） |
| `onion-git` | HTTP REST API 框架（rule-filter 服务） |

### 数据结构与工具

| 库名 | 用途 |
|------|------|
| `collections-c-git` | 通用数据结构（List/Deque/HashTable/HashSet） |
| `jq-git` | JSON 命令行处理工具（脚本中使用） |
| `glog-git` | Google 日志库（部分模块使用） |
| `googletest-git` | 单元测试框架（测试用） |
| `libumqtt-git` | 轻量级 MQTT 客户端 |
| `libuwsc-git` | 轻量级 WebSocket 客户端 |
| `e2fsprogs-git` | UUID 生成（设备唯一ID） |
| `rd_export` | RD 数据导出相关工具 |

---

## 8. 配置文件体系

SDK 的配置分为四个层次：

### 层次一：连接配置 `endpoints.json`

描述**云端 MQTT Broker** 的连接信息，独立于 SDK 版本目录，便于不同环境切换：

```json
{
  "endpoints": [{
    "enabled": true,
    "attribute": "default",
    "mqtt": {
      "host": "127.0.0.1",   // Broker 地址
      "port": 1883,
      "tls": {
        "enabled": false,     // 是否启用 TLS
        "verify": "peer",     // TLS 验证模式：none/peer
        "ca_file": "ca-cert.pem",
        "cert_file": "client-cert.pem",
        "key_file": "client-key.pem"
      },
      "connection_opts": {
        "keep_alive": 30,
        "username": "token-auth",  // 设备认证 Token
        "password": "demoToken"
      }
    }
  }]
}
```

### 层次二：SDK 主配置 `config.json`

控制 SDK 行为的核心配置，包括：日志级别、RCPv4 参数、插件列表、数据库缓存、OTA 设置、Profile 路径等（前文已有详细说明）。

### 层次三：Profile 文件（`profiles/`）

Profile 是设备数据点的**元数据描述**，由云端下发并管理版本，SDK 根据 Profile 知道"设备有哪些数据点、如何采样、如何上报"。

| 文件 | 内容 |
|------|------|
| `connection-profile.json` | 描述**设备连接信息**和 CAN 信号映射：Channel（CAN0）、设备列表、协议类型（SCPDBC）、DBC 文件（含 MD5 校验）、SPN 映射表（PGN→SPN→dataIndex→物理变量名） |
| `thing-profile.json` | 描述**物模型**：设备属性（Property）列表、名称、类型、精度、采样周期、上报周期、全量上报周期 |
| `data-profile.json` | 描述**数据采集配置**：数据格式、上报类型等 |

以 connection-profile.json 中的一个 SPN 映射条目为例：
```json
{
  "spn": 504328,
  "spIndexPgn": 1,
  "dataIndex": 1081,
  "dataVariable": "ID0X1ac8aaf2_Bat_Mes_Back",
  "dataType": "uint8"
}
```
表示：从 PGN=182442、SA=242 的 J1939 消息中，解析 SPN=504328 的信号，存入编号 1081 的数据点，变量名为 `ID0X1ac8aaf2_Bat_Mes_Back`，数据类型为 uint8。

### 层次四：插件配置 `plugin.json`

每个插件目录下的 `plugin.json` 描述插件的基本信息（名称、版本、发布者）和**运行时参数**（任务周期、stub-linker 地址、规则过滤器配置、原始数据缓存策略等）。

---

## 9. 完整数据流

### 上行数据流（设备 → 云端）

```
物理 CAN 总线
    │ Linux SocketCAN / 硬件驱动
    ▼
can_hw_recv()  [can-data-link.c]
    │ 原始 CAN 帧（29-bit CAN ID + 8字节数据）
    ▼
j1939-adapter  [j1939-adapter-git]
    │ 解析为 J1939 PDU（SA/PGN/Data）
    ▼
scp.c / DBC 解析 [scp.c + cantools]
    │ 根据 DBC 信号定义，解码为物理量（例: 电池电压=24.5V）
    │ 同时处理 J1939 诊断消息（DM1~DM14 故障码）
    ▼
scp-raw-data-cache [可选]
    │ 原始数据写本地磁盘（SHA256 签名）
    ▼
scp-rule-filter [可选]
    │ 规则过滤：不满足规则的数据点被丢弃
    ▼
scp-adapter 数据组装
    │ 根据 thing-profile.json 的分组定义，将数据点放入对应 Group
    ▼
rcpv4-grouping
    │ 按 samplePeriod 采样，按 reportPeriod 聚合打包
    ▼
STP 编码 [stp_cbor.c]
    │ 序列化为 CBOR 二进制（可选 gzip 压缩）
    ▼
ERK MQTT Publish
    │ 发布到 MQTT Topic（RCPv4 定义的 Topic 规范）
    ▼
ROOTCLOUD 云平台
```

### 下行数据流（云端 → 设备）

```
ROOTCLOUD 云平台（下发指令）
    │ MQTT Subscribe
    ▼
ERK MQTT 接收
    │
    ▼
rcpv4-task 任务队列
    │ 异步分发
    ▼
RCPv4 命令路由（按 Topic 类型）
    │
    ├── OTA 升级指令   → rcpv4-ota → ADL 下载包 → 插件 on_ota()
    ├── 配置下发       → rcpv4-cmd-config → 插件 execute_cmd()
    ├── 影子期望更新   → rcpv4-cmd-shadow → 插件 execute_cmd()
    ├── 远程动作       → rcpv4-cmd-action → 插件 execute_cmd()
    ├── 实时数据请求   → rcpv4-cmd-live → 立即采集并上报
    ├── 时钟同步       → rcpv4-cmd-time → 调整系统时间
    ├── 锁机命令       → rcpv4-cmd-lock → 插件处理锁定逻辑
    └── 文件推送       → rcpv4-cmd-file → AUL 上传 / ADL 下载
```

### stub 开发调试流（本机模拟）

```
stub-sany-forklift（模拟器进程）
    │ nanomsg pub/sub（stub-linker 协议）
    │ tcp://127.0.0.1:11226 (pub data)
    ▼
scp-adapter（SDK进程内的插件）
    │ tcp://127.0.0.1:11225 (sub data)
    ▼
（后续流程与真实 CAN 数据完全相同）
```

---

## 10. 关键设计模式

### 1. 单线程异步事件驱动

整个 ERK + RCPv4 运行在**单一线程**内，通过 libev 事件循环驱动。这意味着：
- 所有 MQTT 收发、定时器、文件 I/O 都是非阻塞的
- 不存在锁竞争，不会死锁
- 但也要求所有回调函数**不能阻塞**，否则会冻结整个事件循环

### 2. 插件化架构

插件通过 `device_adapter_t` 接口与核心解耦，新设备类型只需实现这个接口并编译为 `.so`，无需修改核心代码。Profile 文件的分离也意味着**同一套代码可以适配不同型号的设备**，只需替换 Profile 即可。

### 3. 多进程 IPC 分离

stub-linker（nanomsg pub/sub）将**数据采集进程**（stub/真实驱动）与**SDK 主进程**分离：
- stub 进程可以独立重启，不影响 SDK
- 支持多个 stub 进程同时向 SDK 发送数据
- 真实产品中，CAN 驱动程序可以是独立进程

### 4. Profile 驱动的数据模型

SDK 不硬编码任何数据点定义，所有设备属性完全由云端下发的 Profile 文件驱动。这使得**云端可以动态变更设备的数据采集策略**（采样率、过滤规则、数据点列表），无需更新 SDK 固件。

### 5. 离线缓存保障

SQLite 本地缓存 + OTA 断点续传，确保在不稳定的工业网络环境下（4G 信号弱、隧道断网等）数据不丢失。

---

## 11. 编译与交叉编译

### 本机 x86-64 编译

```bash
git clone <repo>
cd rootcloud-stp-sdk
mkdir build && cd build
cmake ..
make
```

编译产物位于 `build/install/` 和 `build/erk-agent/`。

### 交叉编译（以 ARMv7-a 为例）

```bash
# 初始化交叉编译工具链环境（不同工具链方式不同）
. /opt/ql-ol-crosstool/ql-ol-crosstool-env-init

# 或者手动 export
export CC="arm-linux-gnueabi-gcc"
export CXX="arm-linux-gnueabi-g++"
# ...

cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain_armv7-a.cmake
make
```

**注意**：不同架构的 build 目录必须互相隔离（rename 为 `build_armv7` 等），否则 CMake package 缓存会冲突。

### 打包

```bash
cmake ..              # 生成 .tar.gz 包
make package
# 产物：erk-agent_0.3.1.31_x86_64.tar.gz
```

### cmake 版本要求

- 最低 **3.21**（低于此版本 hedge-linker 编译会失败）
- 推荐 **3.24+**

---

## 12. 工具集 tools

### erk-kits（`tools/erk-kits/`）

基于 Python 的 **Windows 桌面部署工具**（GUI），功能：
- 通过 SSH/SCP（内嵌 `pscp.exe`）将 SDK 包部署到远程设备
- 图形化操作，简化现场运维人员的操作流程
- 打包为 Windows 可执行文件（`pyinstaller`）

### 性能压测（`tools/` 下的其他工具）

`tools/README.md` 描述了压力测试工具的集合，用于在开发阶段验证 SDK 在高频数据场景下的性能表现。

---

## 附录：关键缩写速查

| 缩写 | 全称 | 中文说明 |
|------|------|---------|
| ERK | Edgewise for RootCloud Kit | 边缘侧 MQTT 引擎 |
| STP | SANY Transmission Protocol | 三一传输协议 |
| RCP | RootCloud Protocol | 树根互联协议 |
| SCP | SANY CAN Protocol | 三一 CAN 协议 |
| DTP | Device Transmission Protocol | 设备传输协议 |
| OTA | Over-The-Air | 空中（远程）升级 |
| RTTY | Remote TTY | 远程终端 |
| ADL | Async Download Library | 异步下载库 |
| AUL | Async Upload Library | 异步上传库 |
| J1939 | — | SAE 工程机械 CAN 总线标准 |
| PGN | Parameter Group Number | J1939 参数组号 |
| SPN | Suspect Parameter Number | J1939 参数编号 |
| SA | Source Address | J1939 源地址（ECU 节点号） |
| DBC | Database CAN | CAN 信号定义文件格式 |
| CBOR | Concise Binary Object Representation | 紧凑二进制对象表示（RFC 7049） |
| RSRP | Reference Signal Received Power | 4G 参考信号接收功率 |
| SINR | Signal to Interference plus Noise Ratio | 信噪比 |

---

*文档生成日期：2026-04-14*  
*基于源码版本：commit 54538a2（增加仓库源码）*
