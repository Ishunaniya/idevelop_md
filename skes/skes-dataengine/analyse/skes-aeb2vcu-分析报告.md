# skes-aeb2vcu 仓库源码深度分析报告

> **项目名称**：skes-aeb2vcu  
> **版本**：0.1.0.0  
> **作者**：xuanxi.ren@irootech.com（ROOTCLOUD）  
> **创建日期**：2025-09-02  
> **分析日期**：2026-03-03  

---

## 目录

1. [项目概述](#1-项目概述)
2. [仓库结构](#2-仓库结构)
3. [整体架构](#3-整体架构)
4. [核心数据结构](#4-核心数据结构)
5. [通信架构详解](#5-通信架构详解)
6. [功能模块分析](#6-功能模块分析)
7. [完整数据流图](#7-完整数据流图)
8. [线程模型](#8-线程模型)
9. [消息协议格式](#9-消息协议格式)
10. [故障码系统](#10-故障码系统)
11. [BSD 声光告警状态机](#11-bsd-声光告警状态机)
12. [CAN 帧组装与校验](#12-can-帧组装与校验)
13. [编译构建系统](#13-编译构建系统)
14. [测试系统](#14-测试系统)
15. [设计亮点与问题分析](#15-设计亮点与问题分析)
16. [总结](#16-总结)

---

## 1. 项目概述

### 1.1 功能定位

`skes-aeb2vcu` 是一个**嵌入式 Linux 中间件进程**，全称为 **AEB (Automatic Emergency Braking) to VCU (Vehicle Control Unit) Bridge**。

它在叉车/工业车辆 ADAS（高级驾驶辅助系统）平台上承担**协议转换网关**的角色：

```
SKES 消息总线（JSON/nanomsg） ──► skes-aeb2vcu ──► CAN 总线（二进制帧/nanomsg）
         AI感知层                    本进程              车辆控制层
```

其核心职责是：
- **订阅** SKES 消息总线上的 AI 感知数据（告警、故障、属性过滤）
- **解析** JSON 格式消息，提取关键状态字段
- **打包** 为 CAN 总线二进制帧格式
- **定时发布** CAN 帧给车辆控制单元（VCU）

### 1.2 场景背景

本项目运行在 **RK3576 aarch64** 嵌入式平台上，服务于以下叉车 ADAS 功能：

| 功能 | 说明 |
|------|------|
| AEB | 自动紧急制动，接收来自摄像头的目标检测结果 |
| BSD | 盲区检测，监控后方、左侧、右侧行人告警 |
| DMS | 驾驶员监控，检测疲劳、分心、脱离驾驶位等 |
| 人脸识别 | 驾驶员身份验证 |
| 故障监控 | 摄像头、通信、目标检测等各子系统的故障上报 |

---

## 2. 仓库结构

```
skes-aeb2vcu-gj/
├── main.c                  # 程序入口，版本输出 + 生命周期管理
├── skes-aeb2vcu.h          # 公开 API 声明 + 所有数据结构定义
├── skes-aeb2vcu.c          # 核心业务逻辑（订阅处理 + CAN 发送）
├── CMakeLists.txt          # CMake 构建脚本
├── sep.ini                 # 服务进程配置文件（SEP 框架注册）
├── test.json               # 告警消息 JSON 样例（开发调试用）
├── tags                    # Vim ctags 符号表
└── tests/
    ├── test.cpp            # 测试程序：模拟 SKES 总线发布者
    └── CMakeLists.txt      # 测试子工程构建脚本
```

**文件职责一览：**

| 文件 | 代码行数（估） | 职责 |
|------|------------|------|
| `skes-aeb2vcu.h` | ~110 | 结构体定义、宏、API 声明 |
| `skes-aeb2vcu.c` | ~580 | 核心业务：消息解析 + CAN 发送线程 |
| `main.c` | ~50 | 进程入口：初始化、主循环、退出 |
| `tests/test.cpp` | ~120 | 集成测试：模拟上游消息发布 |

---

## 3. 整体架构

### 3.1 系统层次图

```
┌─────────────────────────────────────────────────────────────────┐
│                       车辆 ADAS 平台 (RK3576)                    │
│                                                                   │
│  ┌─────────────┐    ┌──────────────┐    ┌────────────────────┐  │
│  │  AI 感知层   │    │  SKES 消息   │    │   车辆控制层        │  │
│  │             │    │  总线        │    │                    │  │
│  │ 摄像头AI引擎 │──► │  (nanomsg    │    │  CAN 总线代理进程   │  │
│  │ AEB检测     │    │  PUB/SUB)    │    │  (can-proxy)       │  │
│  │ BSD检测     │    │              │    │                    │  │
│  │ DMS检测     │    │  Port 19226  │    │  Port 26002        │  │
│  │ 人脸识别    │    │  (SUB端)     │    │  (PUB端)           │  │
│  └─────────────┘    └──────┬───────┘    └────────┬───────────┘  │
│                             │                     ▲              │
│                             │    ┌────────────┐   │              │
│                             └──► │skes-aeb2vcu│ ──┘              │
│                                  │            │                  │
│                                  │ Port 19225 │ ◄── (pub回路)    │
│                                  └────────────┘                  │
│                                                                   │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              物理 CAN 总线 (CAN0)                             │ │
│  │          VCU ──── CAN0 ──── 制动控制器、报警器等              │ │
│  └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 进程内架构

```
skes-aeb2vcu 进程内部
┌─────────────────────────────────────────────────────┐
│  main thread                                         │
│  ├── skes_aeb2vcu_new()   // 初始化 entry 对象       │
│  ├── skes_aeb2vcu_run()   // 启动两个工作线程         │
│  └── 主循环（200ms轮询 is_running）                   │
│                                                      │
│  pthread: do_sre_msg_poller  (pid)                   │
│  ├── skes_run() 阻塞等待消息                          │
│  ├── on_skes_msg_received() 分发                      │
│  │   ├── on_msg_filter()   → 解析 properties         │
│  │   └── on_msg_event()    → 解析 fault / alarm      │
│  └── 更新 skes_aeb2vcu_entry_t 内部状态              │
│                                                      │
│  pthread: do_can_msg_sender  (can_pid)               │
│  ├── 每 100ms 执行一次                                │
│  ├── 读取 entry 中的最新状态                           │
│  ├── 组装 Gj_ack_t 帧（CAN ID: 0x4BE）               │
│  └── nn_send() 发送至 CAN 代理进程                    │
│                                                      │
│  共享状态 (skes_aeb2vcu_entry_t)                     │
│  ├── aeb_fault      : AEB 制动命令字段               │
│  ├── gj_ack         : 综合应答帧（国际版）            │
│  ├── fault_code_list: 16 个故障码槽位                 │
│  ├── angle/distance : AEB 目标位置信息               │
│  └── bsd_*_trigger  : BSD 三方向触发状态             │
└─────────────────────────────────────────────────────┘
```

---

## 4. 核心数据结构

### 4.1 主控入口结构体 `skes_aeb2vcu_entry_t`

```c
typedef struct skes_aeb2vcu_entry {
    bool              running;           // 运行标志
    pthread_t         pid;               // 消息轮询线程 ID
    skes_handle_t    *handle_sub;        // SKES 订阅句柄 (port 19226)
    skes_handle_t    *handle_pub;        // SKES 发布句柄 (port 19225)
    pthread_t         can_pid;           // CAN 发送线程 ID
    aeb_fault_t       aeb_fault;         // AEB CAN 帧状态（当前注释掉）
    aeb_fault_code_t *fault_code_list;   // 指向故障码表
    int32_t           angle;             // AEB 目标角度（×100，单位 cm角度）
    int32_t           distance;          // AEB 目标距离（×100，单位 cm）
    bool              bsd_rear_trigger;  // BSD 后方触发
    bool              bsd_left_trigger;  // BSD 左侧触发
    bool              bsd_right_trigger; // BSD 右侧触发
    Gj_ack_t          gj_ack;           // 综合应答帧（叉车国际版）
} skes_aeb2vcu_entry_t;
```

> **注意**：`handle_sub` 和 `handle_pub` 共用同一个 nanomsg 连接池，由 `skes-linker` 库封装管理。

### 4.2 AEB CAN 帧结构 `aeb_fault_t`（精确 8 字节，pragma pack 1）

本结构体存在两种车型变体，通过编译宏 `USE_NEWC_VEHICLE` 切换：

#### 标准版（`CAN_ID = 0x0CFF2003`，J1939 扩展帧）

```
Byte 0: [7:4]=rsv1  [3]=AlarmCmd  [2]=FlashCmd  [1:0]=AEBActivateCmd
Byte 1-2 (16bit): [15:12]=rsv2  [11:0]=BrkCurCmd_Front（前桥制动电流）
Byte 3-4 (16bit): [15:12]=rsv3  [11:0]=BrkCurCmd_Rear（后桥制动电流）
Byte 5: rsv4（保留）
Byte 6: Fault_Code（故障码）
Byte 7: [7:4]=Checksum  [3:0]=LifeSignal（生命信号计数器）
```

#### 新车版（`CAN_ID = 0x168B9664`，`USE_NEWC_VEHICLE` 定义时）

```
Byte 0: 同标准版
Byte 1-2: 同标准版
Byte 3-4: 同标准版
Byte 5: [7:2]=rsv4  [1:0]=VoiceAlert（声音告警 0-3）
Byte 6: Fault_Code
Byte 7: [7:4]=Checksum  [3:0]=LifeSignal
```

> 当前 CMakeLists.txt 通过 `add_definitions("-DUSE_NEWC_VEHICLE")` 激活新车版。

### 4.3 综合应答帧 `Gj_ack_t`（8字节，pragma pack 1）

```
Byte 0: MachineType（机型）
Byte 1-2: rsv1[2]（保留）
Byte 3: FaceRecognitionResult（人脸识别结果）
Byte 4: [7]=dms_camera_block  [6]=dms_sunglass_block  [5]=dms_out_of_seat
        [4]=dms_fengxin  [3]=dms_yawn  [2]=dms_close_eye  [1]=dms_smoke  [0]=dms_call
Byte 5: [7:5]=bsd_last_area  [4:1]=bsd_rcv  [0]=dms_no_seatbelt
Byte 6: [7:6]=bsd_right_alarm  [5:4]=bsd_left_alarm
        [3:2]=bsd_rear_alarm  [1:0]=bsd_front_alarm
Byte 7: heartbeat（心跳计数，每帧+1）
```

**当前发送 CAN ID：`0x4BE`（标准 11 位帧）**，帧周期 **100ms（10Hz）**。

### 4.4 故障码条目 `aeb_fault_code_t`

```c
typedef struct {
    int  code;            // 当前故障激活状态（0=正常，非0=故障）
    int  fault_code;      // 发给 VCU 的故障码字节值（如 0x01）
    char code_name[16];   // 内部编码名（如 "Y1201"）
    char source_name[64]; // 来源系统名（"camera","communication"...）
    char type_name[64];   // 子类型名（"left_side","rs485"...）
} aeb_fault_code_t;
```

### 4.5 CAN 帧传输结构 `can_frame_t`

```c
typedef struct {
    uint32_t can_id;      // CAN ID（bit31=扩展帧标志）
    uint16_t can_dlc;     // 数据长度（字节数，通常=8）
    uint16_t rsv_ms;      // MCU 上行时间戳（保留）
    uint8_t  data[8];     // CAN 数据域
    uint32_t timestamp;   // RTC 时间戳
} can_frame_t;
```

---

## 5. 通信架构详解

### 5.1 SKES 消息总线连接

| 方向 | 端口 | nanomsg 模式 | 角色 |
|------|------|------------|------|
| 订阅（接收）| `tcp://127.0.0.1:19226` | `NN_SUB` | 从总线接收 AI 感知消息 |
| 发布（发送）| `tcp://127.0.0.1:19225` | `NN_PUB` | 回复/转发消息（当前几乎不用） |

初始化代码：
```c
// skes_linker_init 内部封装了 nanomsg socket 的创建、连接和 topic 订阅
int ret = skes_linker_init(
    &(*sre)->handle_sub, "tcp://127.0.0.1:19226",  // SUB
    &(*sre)->handle_pub, "tcp://127.0.0.1:19225",  // PUB
    false,
    on_skes_msg_received,                          // 消息回调
    *sre                                           // 私有数据
);
```

### 5.2 CAN 总线发送连接

| 端口 | nanomsg 模式 | 说明 |
|------|------------|------|
| `tcp://127.0.0.1:26002` | `NN_PUB` | CAN0 发送通道（do_can_msg_sender 使用） |
| `26003` | 保留 | CAN1 发送（头文件中定义，当前未用） |
| `26004` | 保留 | CAN2 发送（头文件中定义，当前未用） |

CAN 接收通道（`16002~16004`）也在头文件中定义，但本进程只发送，不接收 CAN 帧。

### 5.3 SKES 消息帧格式

SKES 消息帧在 nanomsg 层的二进制布局（由 `tests/test.cpp` 揭示）：

```
+----------+-------------------+----------+-------------------+
| 1 byte   | topic_len bytes   | 4 bytes  | payload_len bytes |
+----------+-------------------+----------+-------------------+
| topic_len| topic string      | payload  | JSON payload      |
|          | e.g. "/skes/event"| length   |                   |
+----------+-------------------+----------+-------------------+
```

例如发布一条 event 消息：
```
[0x0B] [/skes/event] [0x00 0x00 0x01 0xF4] [{"version":"","type":"fault",...}]
```

`skes-linker` 库负责在接收端解析这个二进制协议头，并根据 topic 调用对应的 `on_skes_msg_received` 回调。

### 5.4 Topic 类型映射

```c
// 本进程当前激活订阅的 topic（来自 skes-linker 定义）：
skes_topic_type_filter  →  "/skes/filter"  // 属性过滤消息
skes_topic_type_event   →  "/skes/event"   // 事件消息（故障/告警）

// 以下 topic 在代码中已注释（#if 0 块）：
skes_topic_type_heartbeat
skes_topic_type_version
skes_topic_type_dev_data
skes_topic_type_dev_data_raw
skes_topic_type_location
```

---

## 6. 功能模块分析

### 6.1 生命周期管理 API

| 函数 | 功能 |
|------|------|
| `skes_aeb2vcu_new()` | 堆分配 entry，初始化 SKES 连接，挂载故障码表 |
| `skes_aeb2vcu_run()` | 启动消息轮询线程和 CAN 发送线程 |
| `skes_aeb2vcu_is_running()` | 查询运行状态（主循环用） |
| `skes_aeb2vcu_stop()` | 停止线程（`pthread_join` 等待） |
| `skes_aeb2vcu_release()` | 释放连接资源，`free` entry |

**生命周期时序：**

```
main()
  │
  ├─► skes_aeb2vcu_new()
  │       ├── malloc(entry)
  │       ├── skes_linker_init(sub=19226, pub=19225)
  │       └── fault_code_list = &fault_code_list[static]
  │
  ├─► skes_aeb2vcu_run()
  │       ├── pthread_create(do_sre_msg_poller)
  │       └── pthread_create(do_can_msg_sender)
  │
  ├─► while(running) { usleep(200ms); }
  │
  └─► skes_aeb2vcu_release()
          ├── skes_linker_uninit()
          └── free(entry)
```

### 6.2 消息处理模块

#### 6.2.1 `on_msg_filter()` — 属性过滤消息处理

处理 topic `/skes/filter`，消息结构：

```json
{
  "type": "properties",
  "data": {
    "type": "AEB",
    "data": [
      {"property_name": "AEBActivateCmd", "data": 1.0},
      {"property_name": "FlashCmd",       "data": 1.0},
      {"property_name": "AlarmCmd",       "data": 1.0},
      {"property_name": "BrkCurCmd_Front","data": 50.1},
      {"property_name": "BrkCurCmd_Rear", "data": 10.1}
    ]
  }
}
```

**支持的 type 及对应字段映射：**

| JSON `type` | `property_name` | 映射到结构体字段 |
|------------|----------------|----------------|
| `AEB` | `AEBActivateCmd` | `aeb_fault.A2V_AEBActivateCmd` |
| `AEB` | `FlashCmd` | `aeb_fault.A2V_FlashCmd` |
| `AEB` | `AlarmCmd` | `aeb_fault.A2V_AlarmCmd` |
| `AEB` | `BrkCurCmd_Front` | `aeb_fault.A2V_BrkCurCmd_Front` |
| `AEB` | `BrkCurCmd_Rear` | `aeb_fault.A2V_BrkCurCmd_Rear` |
| `CentralControlScreen` | `MachineType` | `gj_ack.MachineType` |
| `FaceRecognition` | `FaceRecognitionResult` | `gj_ack.FaceRecognitionResult` |

> **代码 Bug**：`on_msg_aeb_filter_parse` 中第二、三个 if 分支使用 `if` 而非 `else if`，导致同一消息会同时匹配所有 type 分支而不是只匹配一个。这是一个逻辑错误。

#### 6.2.2 `on_msg_event()` — 事件消息处理

处理 topic `/skes/event`，根据 `type` 字段分发：

```
on_msg_event()
    ├── type == "fault"  → on_msg_fault_parse()
    └── type == "alarm"  → on_msg_alarm_parse()
```

#### 6.2.3 故障解析分发链 `on_msg_fault_parse()`

```
on_msg_fault_parse(json_msg[])
    遍历数组，按 source 分发：
    ├── source == "camera"           → on_msg_fault_camera_parse()
    │       按 position 匹配故障码表
    ├── source == "sound_light_alarm"→ on_msg_fault_sound_parse()
    ├── source == "object_detection" → on_msg_fault_objdet_parse()
    │       按 type 匹配（seat_departure / personnel_instrusion / fatigue_detection）
    ├── source == "data"             → on_msg_fault_data_parse()
    └── source == "communication"   → on_msg_fault_comm_parse()
            按 type 匹配（rs485 / lte / gps / bluetooth / ota / ntp）
```

所有故障解析函数的**统一更新逻辑**：遍历 `fault_code_list[16]`，找到 source+type 匹配项，将从 JSON 中提取的 `code` 值写入 `fault_item->code`（0 = 正常，非0 = 故障激活）。

#### 6.2.4 告警解析分发链 `on_msg_alarm_parse()`

```
on_msg_alarm_parse(json_msg[])
    遍历数组，按 source 分发：
    ├── source == "aeb" → on_msg_alarm_parse_aeb()
    │       仅处理 rear 摄像头
    │       提取 detected_position.angle/distance
    │       × 100 转 cm 精度存入 sre->angle/distance
    └── source == "bsd" → on_msg_alarm_parse_bsd()
            status == "triggered" → 设置对应方向 trigger=true
            status == "normal"    → 全部 trigger=false
            position 映射：rear→bsd_rear_trigger
                          left_side →bsd_right_trigger（注意左右互换！）
                          right_side→bsd_left_trigger
```

> **代码疑点**：BSD 的 `left_side` 映射到 `bsd_right_trigger`，`right_side` 映射到 `bsd_left_trigger`，存在**左右方向颠倒**，可能是坐标系差异（摄像头视角 vs 车辆视角）也可能是 Bug，需结合实际安装方向确认。

---

## 7. 完整数据流图

### 7.1 SKES → CAN 完整数据流

```
┌─────────────────────────────────────────────────────────────────────┐
│  上游 SKES 发布者（AI 引擎 / 感知进程）                               │
└─────────────────────────────┬───────────────────────────────────────┘
                               │ nanomsg PUB → tcp:19226
                               ▼
                    ┌──────────────────────┐
                    │  SKES 消息总线代理    │
                    │  (skes-relay 进程)    │
                    └──────────┬───────────┘
                               │ 转发订阅消息
                               ▼
┌──────────────────────────────────────────────────────────────────────┐
│                      skes-aeb2vcu 进程                                │
│                                                                       │
│  ┌─────────────────────────────────┐                                  │
│  │  do_sre_msg_poller 线程         │                                  │
│  │                                  │                                  │
│  │  收到 /skes/filter 消息          │                                  │
│  │  └─ on_msg_filter()             │                                  │
│  │     └─ on_msg_aeb_filter_parse()│                                  │
│  │        ├─ AEB: 更新 aeb_fault   │                                  │
│  │        │   AEBActivateCmd       │                                  │
│  │        │   FlashCmd             │                                  │
│  │        │   AlarmCmd             │                                  │
│  │        │   BrkCurCmd_Front      │                                  │
│  │        │   BrkCurCmd_Rear       │                                  │
│  │        ├─ CentralControlScreen: │                                  │
│  │        │   MachineType          │                                  │
│  │        └─ FaceRecognition:      │                                  │
│  │            FaceRecognitionResult│                                  │
│  │                                  │                                  │
│  │  收到 /skes/event 消息（fault）  │                                  │
│  │  └─ on_msg_event()              │                                  │
│  │     └─ on_msg_fault_parse()     │                                  │
│  │        ├─ camera: 更新故障码表  │                                  │
│  │        │   [0x01~0x05] left/right/rear/front/dms                  │
│  │        ├─ sound_light_alarm:    │                                  │
│  │        │   [0x06]              │                                  │
│  │        ├─ object_detection:     │                                  │
│  │        │   [0x10~0x12]         │                                  │
│  │        ├─ data: [0x13]         │                                  │
│  │        └─ communication:        │                                  │
│  │            [0x14~0x19]         │                                  │
│  │                                  │                                  │
│  │  收到 /skes/event 消息（alarm）  │                                  │
│  │  └─ on_msg_event()              │                                  │
│  │     └─ on_msg_alarm_parse()     │                                  │
│  │        ├─ aeb: 更新 angle/dist  │                                  │
│  │        └─ bsd: 更新三方向触发   │                                  │
│  └─────────────────────────────────┘                                  │
│               │ 共享内存（无锁，潜在竞态）                              │
│               ▼                                                       │
│  ┌─────────────────────────────────┐                                  │
│  │  do_can_msg_sender 线程（10Hz） │                                  │
│  │                                  │                                  │
│  │  读取: bsd_rear/left/right_trigger                                 │
│  │        gj_ack.MachineType/FaceRecognitionResult                    │
│  │                                  │                                  │
│  │  写入 Gj_ack_t:                  │                                  │
│  │   gj_ack.bsd_rear_alarm  = bsd_rear_trigger  ? 1:0                │
│  │   gj_ack.bsd_left_alarm  = bsd_left_trigger  ? 1:0                │
│  │   gj_ack.bsd_right_alarm = bsd_right_trigger ? 1:0                │
│  │   gj_ack.heartbeat++              │                                │
│  │                                  │                                  │
│  │  组装 can_frame_t:               │                                  │
│  │   can_id  = 0x4BE                │                                  │
│  │   can_dlc = 8                    │                                  │
│  │   data    = memcpy(gj_ack)       │                                  │
│  │                                  │                                  │
│  │  nn_send(can_nn_fd, frame)        │                                  │
│  └─────────────────────────────────┘                                  │
└───────────────────────────────────────────────────────┬──────────────┘
                                                        │ nanomsg PUB → tcp:26002
                                                        ▼
                                           ┌─────────────────────┐
                                           │  CAN 总线代理进程    │
                                           │  (can-proxy)         │
                                           └──────────┬──────────┘
                                                      │
                                                      ▼
                                              物理 CAN0 总线
                                              CAN ID: 0x4BE
                                              (VCU / 制动控制器)
```

---

## 8. 线程模型

### 8.1 线程清单

| 线程 | 创建方式 | 职责 | 阻塞点 |
|------|---------|------|--------|
| `main` | 进程主线程 | 生命周期管理、状态监控 | `usleep(200ms)` |
| `do_sre_msg_poller` | `pthread_create` | 从 SKES 总线接收并解析消息 | `skes_run()` (nanomsg recv) |
| `do_can_msg_sender` | `pthread_create` | 组装并发送 CAN 帧 | `usleep(100ms)` |

### 8.2 线程间通信

两个工作线程通过 `skes_aeb2vcu_entry_t` 内的字段**直接共享内存**，当前代码**没有任何互斥锁或原子操作**保护。

```
msg_poller 线程  ─── 写 ──►  aeb_fault, gj_ack, bsd_*_trigger, angle, distance
can_sender 线程  ─── 读 ──►  同上字段
```

**潜在竞态条件**：在消息轮询线程更新 `bsd_rear_trigger` 的同时，CAN 发送线程也在读取它。在 aarch64 平台上，`bool` 的单字节读写是原子的，但对于 `gj_ack`（多字节结构体）和 `aeb_fault` 存在非原子更新风险。

### 8.3 线程停止机制

```c
skes_aeb2vcu_stop():
    sre->running = false;          // 设置停止标志
    pthread_join(sre->pid, NULL);  // 等待消息轮询线程退出
    pthread_join(sre->can_pid, NULL); // 等待 CAN 发送线程退出
```

`do_sre_msg_poller` 检查 `sre->running` 在主循环中；`do_can_msg_sender` 的 `while(sre->running)` 最大等待时间为 100ms。

---

## 9. 消息协议格式

### 9.1 Filter 消息（`/skes/filter`）

```json
{
    "version": "",
    "id": "",
    "ts": 1639535683010,
    "action": "post",
    "type": "properties",
    "data": {
        "type": "AEB",
        "data": [
            {"index": -1, "property_name": "AEBActivateCmd", "data_type": "double", "data": 1.0},
            {"index": -1, "property_name": "FlashCmd",       "data_type": "double", "data": 1.0},
            {"index": -1, "property_name": "AlarmCmd",       "data_type": "double", "data": 1.0},
            {"index": -1, "property_name": "BrkCurCmd_Front","data_type": "double", "data": 50.1},
            {"index": -1, "property_name": "BrkCurCmd_Rear", "data_type": "double", "data": 10.1}
        ]
    }
}
```

### 9.2 Event/Fault 消息（`/skes/event`，`type=fault`）

```json
{
    "version": "", "id": "", "ts": 1756457353839,
    "action": "post",
    "type": "fault",
    "data": [
        {
            "source": "camera",
            "ts": 1652173828, "code": 0, "desc": "camera fault",
            "data": [
                {"id": "", "code": 1, "position": "left_side", "desc": "no frame"}
            ]
        },
        {
            "source": "communication",
            "ts": 1652173828, "code": 0, "desc": "通信故障",
            "data": [
                {"type": "rs485", "code": 1, "desc": "failed to connect"}
            ]
        }
    ]
}
```

### 9.3 Event/Alarm 消息（`/skes/event`，`type=alarm`）

```json
{
    "version": "", "id": "", "ts": 12833043,
    "action": "post",
    "type": "alarm",
    "data": [
        {
            "source": "aeb",
            "status": "triggered",
            "data": [
                {
                    "camera": {"id": 2, "position": "rear"},
                    "bounding_boxes": [
                        {
                            "detected_position": {
                                "angle": 1.56,
                                "distance": 1.84
                            }
                        }
                    ]
                }
            ]
        },
        {
            "source": "bsd",
            "status": "triggered",
            "data": [
                {"camera": {"id": 2, "position": "right_side"}}
            ]
        }
    ]
}
```

### 9.4 消息头通用字段

| 字段 | 类型 | 说明 |
|------|------|------|
| `version` | string | 协议版本（当前为空） |
| `id` | string | 消息 UUID（当前为空） |
| `ts` | int64 | 时间戳（毫秒级） |
| `action` | string | `post`/`request`/`response`/`broadcast` |
| `type` | string | 消息业务类型 |
| `data` | array/object | 业务数据 |

---

## 10. 故障码系统

### 10.1 静态故障码表

程序启动时初始化 16 个静态故障码槽位：

| 索引 | fault_code | code_name | source_name | type_name |
|------|-----------|-----------|-------------|-----------|
| 0 | 0x01 | Y1201 | camera | left_side |
| 1 | 0x02 | Y1202 | camera | right_side |
| 2 | 0x03 | Y1203 | camera | rear |
| 3 | 0x04 | Y1204 | camera | front |
| 4 | 0x05 | Y1205 | camera | dms |
| 5 | 0x06 | Y1306 | sound_light_alarm | （空） |
| 6 | 0x10 | Y2410 | object_detection | seat_departure |
| 7 | 0x11 | Y2411 | object_detection | personnel_instrusion |
| 8 | 0x12 | Y2412 | object_detection | fatigue_detection |
| 9 | 0x13 | Y2513 | data | （空） |
| 10 | 0x14 | Y2616 | communication | rs485 |
| 11 | 0x15 | Y2617 | communication | lte |
| 12 | 0x16 | Y2618 | communication | gps |
| 13 | 0x17 | Y2619 | communication | bluetooth |
| 14 | 0x18 | Y2620 | communication | ota |
| 15 | 0x19 | Y2621 | communication | ntp |

### 10.2 故障码处理流程

```
收到 fault 事件
        │
        ▼
遍历 json_data[].source
        │
        ├── "camera"            → 按 position 匹配故障码表中 source_name=="camera"
        │                           写入 fault_item->code（0=恢复，非0=激活）
        │
        ├── "sound_light_alarm" → 匹配 source_name=="sound_light_alarm"
        │
        ├── "object_detection"  → 按 type 匹配 source_name=="object_detection"
        │
        ├── "data"              → 匹配 source_name=="data"
        │
        └── "communication"    → 按 type 匹配 source_name=="communication"
                                   （rs485/lte/gps/bluetooth/ota/ntp）

故障激活后，在 do_can_msg_sender（注释中）：
    轮询故障码表，取第一个非零故障，写入 aeb_fault.Fault_Code
    打包到 AEB_FAULT_CAN_ID 帧发送
```

> **注意**：故障码打包到 AEB 帧的逻辑目前在 `do_can_msg_sender` 中被 `#if 0` 注释掉，**当前版本实际上不发送 AEB 故障帧**，只发送 `Gj_ack_t` 帧（CAN ID 0x4BE）。

---

## 11. BSD 声光告警状态机

此逻辑位于 `do_can_msg_sender` 中，同样被 `#if 0` 包裹（当前版本未启用）。以下为设计逻辑分析：

### 11.1 状态变量

```
voice_index    : 0~2 轮询索引
voiceAlarmValue: 0=静默, 1=后方, 2=左侧, 3=右侧
```

### 11.2 告警优先级状态机

```
voiceAlarmValue == 0（静默状态）:
    │
    ├── 只有后方触发         → voiceAlarmValue=1, voice_index=1
    ├── 多方向 & voice_index==0 → voiceAlarmValue=1（后方轮到）
    ├── 只有左侧触发         → voiceAlarmValue=2, voice_index=2
    ├── 多方向 & voice_index==1 → voiceAlarmValue=2（左侧轮到）
    ├── 只有右侧触发         → voiceAlarmValue=3, voice_index=0
    └── 多方向 & voice_index==1 → voiceAlarmValue=3（右侧轮到）
                                    （注：此处可能有 Bug，条件与左侧相同）

voiceAlarmValue != 0（告警中）:
    下一周期强制 voiceAlarmValue=0  // 必须回到静默才能再次触发
    
设计原则：0→1→0→2（不允许 1→2 直接跳转）
```

### 11.3 当前实际行为（`#if 0` 已注释掉后）

```c
// 当前实际发送的 Gj_ack_t 中 BSD 字段：
sre->gj_ack.bsd_rear_alarm  = sre->bsd_rear_trigger  ? 1 : 0;
sre->gj_ack.bsd_left_alarm  = sre->bsd_left_trigger  ? 1 : 0;
sre->gj_ack.bsd_right_alarm = sre->bsd_right_trigger ? 1 : 0;
```

---

## 12. CAN 帧组装与校验

### 12.1 AEB 帧校验算法 `calculate_checksum()`

用于 AEB 故障帧（当前注释）的 4 位校验码计算：

```c
uint32_t checksum = 0;

// 步骤 1：累加前 7 个字节
for (int i = 0; i < 7; i++) {
    checksum += frame_data[i];
}

// 步骤 2：加生命信号低 4 位
checksum += (LifeSignal & 0x0F);

// 步骤 3：加 CAN ID 四个字节（小端分解）
checksum += (can_id & 0xFF)          // 低字节
          + ((can_id >> 8) & 0xFF)   // 中低字节
          + ((can_id >> 16) & 0xFF)  // 中高字节
          + ((can_id >> 24) & 0xFF); // 高字节

// 步骤 4：压缩为 3 位
final_checksum = (((checksum >> 6) & 0x03) + (checksum >> 3) + checksum) & 0x07;
```

这是一种简单的**模运算折叠校验**，将累加和通过位移折叠压缩为 3 位，填入 CAN 帧 Byte7 的高 4 位之一（实际上 `Checksum` 是 4 位域，只用了低 3 位）。

### 12.2 生命信号

`A2V_LifeSignal` 是一个 4 位计数器（0~15 循环），每发送一帧自增，VCU 通过检测它的连续递增来判断通信是否正常（心跳机制）。

### 12.3 当前实际发送帧

```
CAN ID: 0x4BE（标准帧，11位）
DLC:    8
数据:   Gj_ack_t 结构体内容（直接 memcpy）
周期:   100ms（10Hz）
```

扩展帧标识：头文件定义 `frame.can_id = AEB_FAULT_CAN_ID | 0x80000000`（bit31 作为扩展帧标志），但 `Gj_ack_t` 帧使用 `0x4BE` 标准帧 ID，没有设置 bit31。

---

## 13. 编译构建系统

### 13.1 CMake 配置

```cmake
project(skes-aeb2vcu)
set(CMAKE_CXX_STANDARD 11)
SET(CMAKE_BUILD_TYPE "debug")
add_definitions("-D_DEFAULT_SOURCE -D_GNU_SOURCE -DENABLE_SKES_LOG")
add_definitions("-DUSE_NEWC_VEHICLE")  # 启用新车版 CAN 格式
```

### 13.2 依赖库清单

| 库 | 版本 | 用途 |
|----|------|------|
| `cjson` | 1.7.15 | JSON 解析 |
| `nanomsg` | 1.1.5 | 进程间消息通信 |
| `skes-linker` | 0.0.1 | SKES 总线连接封装 |
| `skes-log` | 0.0.1 | 日志系统 |
| `skes-utils` | 内部 | 工具函数（时间、字符串等） |
| `curl` | 7.88.1 | HTTP（依赖传入，本进程未直接使用） |
| `openssl` | 1.1.1 | 加密（依赖传入） |
| `zlib` | 1.3.1 | 压缩（依赖传入） |
| `collections-c` | 0.0.1 | 容器库（依赖传入） |
| `pthread` | 系统 | 多线程 |
| `rt` | 系统 | 实时时钟 |

### 13.3 安装布局

```
apps/skes-aeb2vcu/
    skes-aeb2vcu        # 可执行文件
    sep.ini             # 服务注册配置

lib/
    libcjson.so*        # JSON 库
    libnanomsg.so*      # nanomsg 库
    libcurl.so*         # HTTP 库
    libssl.so*          # TLS 库
    libcrypto.so*       # 加密库
    libz.so*            # 压缩库
    libcollections-c.so # 容器库
    libskes-linker.so   # SKES 连接器
    libskes-log.so      # 日志库
```

### 13.4 RPATH 配置

使用相对路径 RPATH `$ORIGIN/.;$ORIGIN/lib;$ORIGIN/../lib;$ORIGIN/../../lib`，支持便携式部署而无需 `ldconfig`。

---

## 14. 测试系统

### 14.1 测试程序 `tests/test.cpp`

作为**独立可执行文件** `Aeb2VcuTest`，功能是**模拟 SKES 消息总线发布者**，用于对 `skes-aeb2vcu` 进行集成测试。

**测试数据轮播逻辑（每 5 秒一轮）：**

| 轮次 | Fault 消息 | Filter 消息 |
|------|-----------|------------|
| 0 | 摄像头故障触发（left_side, code=1） | AEB 制动激活（Activate=1, Flash=1） |
| 1 | 摄像头故障恢复（left_side, code=0） | AEB 制动释放（Activate=0） |
| 2 | RS485 通信故障触发（code=1） | AEB 制动激活 |
| 3 | RS485 通信故障恢复（code=0） | AEB 制动释放 |

### 14.2 测试程序帧封装格式

```c
// 自行实现 SKES 帧封装（与 skes-linker 协议兼容）
char *pSend = calloc(1, 1 + topic_len + 4 + payload_len + 1);
pSend[0] = topic_len;                           // 1 byte: topic 长度
memcpy(pSend+1, topic, topic_len);              // N bytes: topic 字符串
memcpy(pSend+1+topic_len, &payload_len, 4);     // 4 bytes: payload 长度（小端）
memcpy(pSend+1+topic_len+4, payload, payload_len); // M bytes: JSON 内容
nn_send(fd, pSend, 1+topic_len+4+payload_len, 0);
```

### 14.3 测试拓扑

```
Aeb2VcuTest ──PUB→ tcp:19225 ──SUB─► skes-aeb2vcu（handle_sub 连接到 19226）
```

**注意**：测试程序连接到 `19225`（主程序的 PUB 端口），而主程序 SUB 连接到 `19226`。这意味着测试程序与主程序的端口配置**实际上是直连还是经过转发器需要确认**。根据代码分析，skes-relay 可能在 19225 和 19226 之间做转发桥接。

---

## 15. 设计亮点与问题分析

### 15.1 设计亮点

| 方面 | 说明 |
|------|------|
| **关注点分离** | 消息接收线程和 CAN 发送线程独立，订阅层与 CAN 层解耦 |
| **车型可配置** | `USE_NEWC_VEHICLE` 宏实现双车型 CAN 协议切换，无需修改代码 |
| **故障码表驱动** | 静态表驱动设计，新增故障类型只需扩展表项 |
| **生命信号机制** | AEB 帧内置 4 位递增计数器，使 VCU 可检测通信超时 |
| **声音告警去抖** | BSD 声音告警状态机防止连续重复触发（0→1→0→2 设计） |
| **C/C++ 兼容** | 头文件用 `extern "C"` 保护，库接口可同时被 C 和 C++ 调用 |

### 15.2 问题与潜在风险

| 问题 | 位置 | 严重性 | 说明 |
|------|------|--------|------|
| **多线程竞态** | `skes_aeb2vcu_entry_t` 共享字段 | 中 | msg_poller 写，can_sender 读，无互斥保护 |
| **if 分支逻辑错误** | `on_msg_aeb_filter_parse()` | 低 | 第2/3个分支用 `if` 而非 `else if`，对同一消息会冗余匹配 |
| **BSD 方向颠倒** | `on_msg_alarm_parse_bsd()` | 待确认 | `left_side→bsd_right_trigger`，可能是坐标系差异 |
| **AEB 帧未发送** | `do_can_msg_sender` | 待确认 | AEB 故障帧发送逻辑被 `#if 0`，主要功能缺失或重构中 |
| **声光告警未发送** | `do_can_msg_sender` | 待确认 | VoiceAlert 逻辑被 `#if 0`，新车版功能缺失 |
| **内存泄漏风险** | skes_aeb2vcu_stop 未调用时 | 低 | 如进程异常终止，pthread 资源由 OS 回收 |
| **CAN 连接失败无重试** | `do_can_msg_sender` | 中 | `nn_connect` 失败直接 return，线程退出后无恢复机制 |
| **UUID 硬编码** | `get_uuid()` | 低 | 返回固定 UUID，多实例部署会冲突 |
| **`BrkCurCmd_Rear` 重复解析** | `on_msg_aeb_filter_parse()` | 低 | 同名 property 连续出现两次相同赋值语句 |

### 15.3 代码演进状态

代码中大量 `#if 0` 注释表明这是一个**正在迭代演进的版本**：

- **已注释掉**：heartbeat、version、dev_data、location 消息处理器
- **已注释掉**：AEB 故障码 → CAN 帧的完整映射逻辑
- **已注释掉**：BSD 声光告警状态机
- **已注释掉**：角度/距离辅助 CAN 帧（`0x00000001`）
- **当前激活**：仅保留最基础的 `Gj_ack_t` 帧发送（BSD 状态+心跳）

这说明项目可能处于从**完整功能版**精简为**国际版 Gj 协议**的过渡阶段。

---

## 16. 总结

### 16.1 架构总结

`skes-aeb2vcu` 是一个典型的**嵌入式 IPC 协议转换网关**，采用如下架构模式：

```
[JSON over nanomsg PUB/SUB]  →  [事件驱动解析]  →  [状态机聚合]  →  [定时 CAN 帧发送]
```

整体代码结构清晰，采用**生产者-消费者**线程模型，将异步 JSON 消息流转换为固定周期的 CAN 二进制帧输出，符合汽车电子系统对 CAN 报文周期性的基本要求。

### 16.2 核心技术栈

| 层次 | 技术选型 |
|------|---------|
| 进程间通信 | nanomsg（PUB/SUB 模式）|
| 消息格式 | JSON（cJSON 库解析）|
| 多线程 | POSIX pthread |
| CAN 总线 | 自定义 can_frame_t 结构体，通过 nanomsg 发送给 CAN 代理 |
| 构建系统 | CMake 2.8.12+ |
| 目标平台 | Linux aarch64（RK3576）|

### 16.3 与 SKES 平台的关系

本进程是 SKES（Smart Key Embedded System）消息平台的一个**叶节点消费者**：
- 它只**订阅**消息，不主动**发布**业务消息（pub 句柄当前基本闲置）
- 它将感知层的高级 JSON 语义转换为 VCU 能理解的低级 CAN 二进制语义
- 在整个系统中扮演 **AI 感知层 ↔ 车辆执行层** 的桥梁角色

---

*本报告基于仓库 `skes-aeb2vcu-gj`（commit 版本：tags 文件中的符号）的完整源码进行分析，所有结论均以代码事实为依据。*
