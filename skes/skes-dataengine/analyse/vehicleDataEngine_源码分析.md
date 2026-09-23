# vehicleDataEngine 源码完全分析文档

> 适用版本：最新 zip（含 VcuPdoManager 模块）  
> 目标读者：代码能力较弱、希望完全理解整个仓库的开发者

---

## 目录

1. [整体架构概览](#1-整体架构概览)
2. [进程间通信全景图](#2-进程间通信全景图)
3. [数据流向全图](#3-数据流向全图)
4. [文件清单与职责一览](#4-文件清单与职责一览)
5. [核心数据结构：realtime_status.json](#5-核心数据结构realtime_statusjson)
6. [main.cpp — 程序入口与启动流程](#6-maincpp--程序入口与启动流程)
7. [RealtimeStatusManager.cpp — 实时状态管理器](#7-realtimestatusmanagercpp--实时状态管理器)
8. [SkesQtBridge.cpp — Qt仪表通信桥接](#8-skesqtbridgecpp--qt仪表通信桥接)
9. [CanTxFrameManager.cpp — CAN发送帧管理器](#9-cantxframemanagercpp--can发送帧管理器)
10. [CanCommandServer.cpp — CAN命令服务器](#10-cancommandservercpp--can命令服务器)
11. [VcuPdoManager.cpp — VCU参数读写状态机](#11-vcupdomanagercpp--vcu参数读写状态机)
12. [持久化文件说明](#12-持久化文件说明)
13. [关键场景完整流程](#13-关键场景完整流程)
14. [端口与地址速查表](#14-端口与地址速查表)
15. [常见问题速查](#15-常见问题速查)

---

## 1. 整体架构概览

```
┌─────────────────────────────────────────────────────────────────┐
│                      vehicleDataEngine 进程                       │
│                                                                   │
│  ┌──────────────┐   ┌──────────────────┐   ┌─────────────────┐  │
│  │  CAN接收线程  │   │   ev_loop主线程   │   │  GPS接收线程    │  │
│  │ (can_server) │   │  (libev事件驱动) │   │ (pthread)       │  │
│  └──────┬───────┘   └────────┬─────────┘   └────────┬────────┘  │
│         │                    │                       │           │
│  CAN帧  │             ┌──────┴──────────────┐        │GPS位置    │
│  解析   │             │  RealtimeStatusMgr  │◄───────┘           │
│  写入   │────────────►│  (全局JSON状态中心)  │                    │
│  JSON   │             └──────┬──────────────┘                    │
│         │                    │                                    │
│         │          ┌─────────┴──────────────────────────┐        │
│         │          │                                    │        │
│         │   ┌──────▼──────┐  ┌────────────┐  ┌────────▼─────┐  │
│         │   │SkesQtBridge │  │CanTxFrame  │  │CanCommandSvr │  │
│         │   │(Qt仪表桥接) │  │Manager     │  │(CAN命令服务) │  │
│         │   │             │  │(0x31D定时  │  │              │  │
│         │   │ 每500ms推送 │  │ 发送管理)  │  │              │  │
│         │   └──────┬──────┘  └──────┬─────┘  └──────┬───────┘  │
│         │          │                │                │           │
│         │    VcuPdo│Manager         │                │           │
│         │    (PDO参│数读写状态机)    │                │           │
└─────────┼──────────┼────────────────┼────────────────┼───────────┘
          │          │                │                │
    CAN总线        skes-relay       CAN总线           CAN总线
    (接收)        (nanomsg)        (0x31D发送)       (命令发送)
                     │
                   Qt仪表进程
```

**一句话概括：**  
`vehicleDataEngine` 是叉车仪表盘的数据引擎。它从 CAN 总线接收叉车实时数据，存入内存 JSON，再通过 skes-relay 推送给 Qt 仪表显示；同时接收 Qt 的控制命令，转成 CAN 帧发回叉车 VCU。

---

## 2. 进程间通信全景图

```
┌────────────────┐    nanomsg PUB/SUB    ┌────────────────┐
│   Qt 仪表进程   │◄──────────────────────│  skes-relay    │
│                │──────────────────────►│  (消息中转)    │
└────────────────┘                       └───────┬────────┘
                                                 │  nanomsg PUB/SUB
                                         ┌───────▼────────┐
                                         │ vehicleDataEngine│
                                         │  :19225 (接收)  │
                                         │  :19226 (发送)  │
                                         └────────────────┘

┌────────────────┐    nanomsg PUB        ┌────────────────┐
│  CAN驱动进程   │──────────────────────►│ vehicleDataEngine│
│  :16002 (CAN0) │◄──────────────────────│  CAN接收       │
│  :16003 (CAN1) │    nanomsg PUB        │  :26002 (发CAN0)│
│  :16005 (GPS)  │◄──────────────────────│  :26003 (发CAN1)│
└────────────────┘                       └────────────────┘

┌────────────────┐    UDP :9001          ┌────────────────┐
│  其他查询方    │◄──────────────────────│ vehicleDataEngine│
│  (调试/其他)   │──────────────────────►│  UDP服务       │
└────────────────┘                       └────────────────┘
```

---

## 3. 数据流向全图

### 3.1 数据进入方向（CAN → 内存 → Qt）

```
叉车 VCU/BMS/MCU
        │
        │ CAN总线
        ▼
  CAN驱动进程（板子）
  :16002 CAN0 | :16003 CAN1
        │
        │ nanomsg PUB（CanMessage结构体）
        ▼
 can_receiver（接收线程）
        │
        │ 调用 real_manager->updateRealtimeStatus_bycan()
        ▼
 RealtimeStatusManager（内存JSON）
 j["chache"]["mainScreen"]["vehicleSpeed"] = xxx
 j["chache"]["faultInfo"]["driveMcuErr"]  = xxx
 ...
        │
        │ ev_timer 每500ms触发
        ▼
 SkesQtBridge::buildAndPublishStatus()
 把 JSON 里的字段打包成 filter_table
        │
        │ nanomsg PUB → 19225
        ▼
      skes-relay
        │
        │ nanomsg SUB → 19226
        ▼
      Qt 仪表进程
      （显示速度/电量/故障/温度等）
```

### 3.2 控制命令方向（Qt → VEH → CAN）

```
Qt 仪表（用户按下"前大灯"按钮）
        │
        │ nanomsg PUB → skes-relay
        ▼
      skes-relay
        │
        │ nanomsg SUB ← 19226
        ▼
 SkesQtBridge::onQtCmd() 收到消息
 handleCmdControl() 解析 headLight=1
        │
        │ CanTxFrameManager::setPersistentBit(0x31D, 0, 4, 1)
        ▼
 CanTxFrameManager 帧缓存
 data[0] |= (1<<4)  ← Byte0 Bit4置1
        │
        │ ev_timer 每100ms tick，500ms周期到
        ▼
 CanCommandServer_sendCanMessage(CAN1, 0x31D, data, 8)
        │
        │ nanomsg PUB → 26003
        ▼
      CAN驱动 → CAN1总线 → 叉车 VCU
```

### 3.3 PDO参数读写方向（Qt ↔ VCU）

```
Qt 仪表（用户调整"高速最大速度"）
        │  cmd_control: driveSettings.highSpeedMax = 80
        ▼
 SkesQtBridge::handleCmdControl()
 VcuPdoManager::queueWrite(addr=1, val=80, ...)
        │
        │ 发CAN0 0x111 写帧1 (BYTE7=1)
        │ 等50ms
        │ 发CAN0 0x111 写帧2 (BYTE7=0)
        │ 等VCU应答 0x222 (最多300ms)
        ▼
 VCU回应 0x222 BYTE7=0（写成功）
        │
        │ 再发读帧 验证写入值
        ▼
 VCU回应 0x222 读应答（实际值）
        │
        │ 更新 cache，回调 Qt "写成功，当前值=80"
        ▼
 sendPropertiesResponse(msg_id, result=0, value=80)
        │ nanomsg → skes-relay → Qt
        ▼
Qt 仪表界面更新显示 80
```

---

## 4. 文件清单与职责一览

| 文件 | 职责 | 重要程度 |
|------|------|---------|
| `main.cpp` | 程序入口，启动所有模块，运行 ev_loop | ★★★★★ |
| `RealtimeStatusManager.cpp/.h` | CAN帧解析、全局状态JSON读写、里程/工时累计 | ★★★★★ |
| `SkesQtBridge.cpp/.h` | Qt仪表通信桥接（SKES v0.8协议） | ★★★★★ |
| `CanTxFrameManager.cpp/.h` | CAN发送帧缓存+周期发送+脉冲位管理 | ★★★★☆ |
| `CanCommandServer.cpp/.h` | 接收Qt直接CAN命令，发到总线 | ★★★☆☆ |
| `VcuPdoManager.cpp/.h` | VCU参数（驱动/泵设置）读写状态机 | ★★★★☆ |
| `can_receiver.cpp/.h` | CAN总线数据接收线程 | ★★★☆☆ |
| `realtime_status.json` | 全局状态的数据结构模板（初始值） | ★★★★★ |
| `forklift_persistent.json` | 持久化存储（密码、停止设置、里程工时） | ★★★★☆ |
| `NanoSubscriber.cpp/.h` | nanomsg 订阅器封装（用于alarm监听） | ★★☆☆☆ |
| `NetworkStatusQuery.cpp/.h` | 4G网络信号查询 | ★★☆☆☆ |
| `StatusRequestServer.cpp/.h` | HTTP状态查询服务器 | ★★☆☆☆ |
| `config_loader.cpp/.h` | Modbus配置加载 | ★★☆☆☆ |
| `skes_qt_bridge_tester.cpp` | 测试工具（模拟Qt端） | ★★★☆☆ |

---

## 5. 核心数据结构：realtime_status.json

这个 JSON 文件是整个程序的**"数据中枢"**。所有从 CAN 来的数据最终都写进这里；Qt 查询的所有数据也从这里读取。

```
realtime_status.json
│
├── statusIcons          ← 仪表盘图标状态（AEB/BSD/DMS/倒档等）
│
├── settings             ← 用户设置（亮度/音量/后视辅助线）
│
└── chache               ← 注意：是"chache"不是"cache"（原始拼写保留）
    │
    ├── mainScreen       ← 主界面数据（速度/电量/大灯/方向/里程/工时）
    │
    ├── internal         ← 内部状态（钥匙ON/充电/工时计时/密码验证）
    │
    ├── ioStatus         ← IO状态界面（油门/刹车/各路开关/PWM输出）
    │
    ├── faultInfo        ← 故障信息（故障数/故障列表/各MCU故障码）
    │
    ├── versionInfo      ← 版本信息（VCU软件版本/各MCU工时）
    │
    ├── runStatus        ← 运行状态（各电机转速/扭矩/温度）
    │
    ├── driveSettings    ← 行走参数（高/标/低速最大速度、加减速度等）
    │
    ├── pumpSettings     ← 油泵参数（起升/倾斜/侧移速度、高度地址等）
    │
    ├── bmsData          ← BMS电池数据（电压/电流/SOC/继电器状态）
    │
    ├── rfidData         ← RFID数据（暂空）
    │
    └── fuseBoxStatus    ← 保险盒状态（K继电器/F保险丝/DI/DO）
```

### 关键字段说明

| 字段路径 | 含义 | 数据来源 |
|---------|------|---------|
| `chache.mainScreen.vehicleSpeed` | 车速 km/h（分辨率0.1） | CAN0 0x31A BYTE0 × 0.1 |
| `chache.mainScreen.speedMode` | 速度模式 0=E/1=P/2=S | CAN0 0x31A BYTE1 Bit6~7 |
| `chache.mainScreen.driveDirection` | 行车方向 <9990=左/9990~10010=N/>10010=右 | CAN1 0x0CFF0008 BYTE4~5 |
| `chache.mainScreen.batteryLevel` | 电池电量 % | CAN0 0x31A BYTE7 |
| `chache.mainScreen.faultCount` | 故障计数 | CAN0 0x31A BYTE6 |
| `chache.mainScreen.totalMileage` | 总里程 km（持久化） | 累计计算 |
| `chache.mainScreen.totalWorkHours` | 总工时 h（持久化） | 累计计算 |
| `chache.internal.powerOnSignal` | 钥匙ON=1 | CAN0 0x18FD0291 BYTE2 Bit0 |
| `chache.faultInfo.faultList` | 故障列表（JSON数组字符串） | 多路CAN汇总 |
| `chache.bmsData.soc` | 电池 SOC % | CAN0 0x0CFF7C03 |

---

## 6. main.cpp — 程序入口与启动流程

### 启动顺序流程图

```
main()
  │
  ├─1. ensureVechicle_DataFileExists()
  │    把工程目录下的 realtime_status.json 覆盖到 /tmp/
  │    确保每次启动都用最新字段结构
  │
  ├─2. new RealtimeStatusManager(VEHICLE_REALTIME_STATUS_PATH)
  │    加载 /tmp/realtime_status.json 到内存
  │    从 forklift_persistent.json 恢复里程/工时
  │
  ├─3. ModbusConfigLoaderFactory::loadWithPriority()
  │    加载 modbus_config.json 传感器配置
  │
  ├─4. InitSensorStatusToFile()
  │    初始化 /tmp/SensorStatus（传感器超时状态文件）
  │
  ├─5. std::thread(start_can_server).detach()
  │    启动 CAN 接收线程（独立线程，接收CAN0/CAN1数据）
  │
  ├─6. std::thread(networkProbeThread).detach()
  │    启动网络探测线程（每5秒 ping 8.8.8.8）
  │
  ├─7. initializeChannels(manager, urls)
  │    初始化 nanomsg 订阅通道（Modbus数据接收）
  │    端口: 16006/16007/16030/16031
  │
  ├─8. real_manager->setupTimeoutChecker(loop)
  │    启动传感器超时检查定时器（每5秒）
  │
  ├─9. real_manager->setupDateTimeMileageTimer(loop)
  │    启动日期时间/里程/工时定时器（每1秒 + 每60秒持久化）
  │
  ├─10. GPS线程
  │     连接 nanomsg :16005，接收GPS位置数据
  │
  ├─11. NetworkStatusQuery::startPeriodicQuery()
  │     启动4G信号查询（每5秒，连接 :38001）
  │
  ├─12. SkesQtBridge::init()
  │     加载持久化设置（密码/停止设置）
  │     注册 CAN1 0x31D 帧到 CanTxFrameManager
  │
  ├─13. SkesQtBridge::start(loop, :19225, :19226, 0.5s)
  │     连接 skes-relay，注册 ev_io（接收Qt命令）
  │     注册 ev_timer（每500ms推状态）
  │     启动 CanTxFrameManager（每100ms tick）
  │     触发 VcuPdoManager 上电批量读（30个参数）
  │
  ├─14. CanCommandServer::start(loop, :27000, :26002, :26003)
  │     开始接收Qt直接CAN命令，转发到总线
  │
  ├─15. NanoSubscriber（订阅 alarm 主题）
  │
  └─16. ev_run(loop, 0)
        ★ 进入事件循环，永不退出 ★
        所有定时器、IO事件都在这里被驱动
```

### 逐行关键代码说明

```cpp
// 第182行：强制覆盖 realtime_status.json 到 /tmp/
// 原因：程序目录里的是最新版本，/tmp 里可能是旧版（字段不完整会导致崩溃）
ensureVechicle_DataFileExists();

// 第185行：创建全局状态管理器，real_manager 是全局指针
real_manager = new RealtimeStatusManager(VEHICLE_REALTIME_STATUS_PATH);

// 第205行：CAN接收线程独立运行，detach()意味着主线程不等它结束
std::thread(start_can_server, nullptr).detach();

// 第208行：网络探测线程，每5秒 ping 一次 Google DNS
// rc=0 表示网络可达，存入原子变量 g_network_signal=1；否则=3
std::thread(networkProbeThread).detach();

// 第298行：ev_run 是核心！进入 libev 事件循环
// 所有定时器回调、socket可读事件都在这里被触发
// 整个程序的"心跳"就是这一行
ev_run(manager.loop, 0);
```

### UDP服务器（第317-398行）

这是一个简单的 UDP 查询服务，监听 9001 端口：
- 收到查询字符串（如 `"vehicleSpeed"`）
- 从内存 JSON 找到对应字段值
- 返回 JSON 响应

主要用于外部调试工具或其他系统查询叉车状态。

---

## 7. RealtimeStatusManager.cpp — 实时状态管理器

这是整个程序**最重要的文件**，负责：
1. 维护全局状态 JSON（`realtime_status`）
2. 解析所有 CAN 帧并写入状态
3. 累计里程和工时
4. 管理故障列表

### 7.1 核心数据结构

```cpp
class RealtimeStatusManager {
    nlohmann::json realtime_status;  // ← 整个程序的数据中心，所有状态都在这里
    mutable std::mutex m_mutex;      // ← 保护 realtime_status 的互斥锁
    // ...
};
```

`realtime_status` 就是内存里的那个大 JSON，每次 CAN 帧来了就更新里面的字段，每次 Qt 查询就从里面读。

### 7.2 updateRealtimeStatus_bycan() — CAN帧解析（核心函数）

这个函数由 **CAN 接收线程**调用，每收到一帧 CAN 数据就调用一次。

```
参数：
  can_id      - 是哪个 CAN 帧的ID（如 0x31A）
  can_data    - 帧的8个字节数据
  data_length - 实际有效字节数
  can_channel - 0=CAN0, 1=CAN1
```

#### CAN0 帧解析一览

| CAN ID | 注释 | 解析字段 |
|--------|------|---------|
| `0x31A` (ch0) | 车辆主状态 | 车速、速度模式、READY/手刹/脚刹/安全带、货叉称重、转向角、故障数、电量 |
| `0x18FD0291` (ch0) | 保险盒+钥匙+大灯 | 钥匙ON信号、大灯指示、保险盒K/F继电器状态（大量字段） |
| `0x0CFF7C03` (ch0) | BMS主帧 | 电压/电流/SOC/绝缘/单体电压最值/温度最值/各继电器/充电状态 |
| `0x0CFF7D03` (ch0) | BMS单体 | 单体电压均值、温度均值 |
| `0x0CFF7F03` (ch0) | BMS绝缘 | 绝缘正/负阻值 |
| `0x18FE7C03` (ch0) | VCU上电指令 | vcuPowerCmd |
| `0x0CFF0008` (**ch1**) | 行走电机 | 行车方向、行走电机扭矩/转速/状态机 |
| `0x359` (ch0) | 转向电机 | 电流、目标速度 |
| `0x585` (ch0) | 货叉倾角 | forkTiltAngle |
| `0x0CFF0009` (ch0) | 油泵电机 | 扭矩/转速/状态机 |
| `0x0CFF0108` (ch0) | 行走电机温度 | driveMotorTemp, driveIGBTTemp |
| `0x0CFF0109` (ch0) | 油泵电机温度 | pumpMotorTemp, pumpIGBTTemp |
| `0x360` (ch0) | 转向电机状态/转速 | 状态机、转速、转向MCU故障码 |
| `0x0CFF08EF` (ch0) | 行走目标速度 | driveMotorTargetSpeed |
| `0x0CFF09EF` (ch0) | 油泵目标速度 | pumpMotorTargetSpeed |
| `0x18FF6E17` (ch0) | 承载轮PWM | leftLoadWheelPWM, rightLoadWheelPWM |
| `0x0CFF0208` (ch0) | 行走MCU故障 | driveMcuErr |
| `0x0CFF0209` (ch0) | 油泵MCU故障 | pumpMcuErr |

#### CAN1 帧解析一览

| CAN ID | 注释 | 解析字段 |
|--------|------|---------|
| `0x32A` (ch1) | IO输入1 | 油门/方向盘/刹车/前后移/升降/倾斜/侧移采样值 |
| `0x32B` (ch1) | IO输入2 | 油门2/方向盘2/刹车2/编码器/压力 |
| `0x32C` (ch1) | DI开关+DO输出 | 各路开关状态、PWM输出值 |
| `0x32D` (ch1) | 用户提醒故障 | userErr，写入 faultList |
| `0x32E` (ch1) | VCU故障码 | vcuErr，写入 faultList |
| `0x33A` (ch1) | VCU软件版本 | vcuSoftVersion（ASCII字符串） |

### 7.3 反逻辑处理（重要！）

CAN0 0x31A 中安全带和坐椅是**反逻辑**：

```cpp
// 安全带：bit=1 表示已系好 → 不显示警告；bit=0 表示未系 → 显示警告
int seatBeltRaw = (byte1 >> 2) & 0x1;
int seatBeltWarning = seatBeltRaw ? 0 : 1;  // 反转！

// 坐椅：bit=1 表示有人坐 → 不显示警告；bit=0 表示无人坐 → 显示警告
int seatRaw = (byte1 >> 4) & 0x1;
int seatWarning = seatRaw ? 0 : 1;  // 反转！
```

**Qt 端收到的值已经是处理后的**：`1=显示警告图标，0=不显示`，Qt 端无需再做反转。

### 7.4 故障列表管理 — updateFaultEntry()

故障不是简单写一个数字，而是维护一个**故障对象列表**：

```json
"faultList": [
  {
    "type": 1,              // 1=用户提醒 2=VCU故障 3=行走MCU 4=转向MCU 5=油泵MCU
    "type_name": "用户提醒",
    "code": 5,              // 故障码
    "time": "14:23:01"      // 发生时间
  }
]
```

规则：
- 收到某类故障码非0 → 如果该类型还没有，加入列表；如果已有，更新code和时间
- 收到故障码=0 → 从列表移除该类型
- `faultCount` 等于列表长度

### 7.5 里程和工时累计 — updateMileageAndWorkHours()

由 ev_timer **每秒**调用一次：

```
每秒判断：
  如果 vehicleSpeed > 0.01 km/h：
    delta_km = (speed / 3600.0) × 1秒
    totalMileage += delta_km         ← 总里程（持久化）
    singleMileage += delta_km        ← 单次里程（开机累计，不持久化）

  如果 钥匙ON=1 且 充电状态=0：
    delta_hours = 1/3600 小时
    totalWorkHours += delta_hours    ← 总工时（持久化）
    singleWorkHours += delta_hours   ← 单次工时（开机累计）
```

每60秒保存一次到 `/data/forklift_persistent.json`。

### 7.6 各MCU工时累计 — updateMileageAndWorkHours()

```
行走MCU工时：BYTE6 state == 3（SpeedControl，速度控制模式）时计时
转向MCU工时：0x360 BYTE0 Bit0==1（HMOE回中成功）时计时
油泵MCU工时：0x0CFF0009 BYTE6 state == 3 时计时
```

保养提示：
- 第一次保养：工时累计到300h
- 后续保养：每1200h一次

---

## 8. SkesQtBridge.cpp — Qt仪表通信桥接

### 8.1 通信协议格式（SKES v0.8二进制帧）

```
┌──────────┬──────────────┬─────────────┬──────────────────┐
│  1字节    │  N字节       │  4字节      │  M字节           │
│topic长度  │  topic字符串 │ payload长度 │  payload JSON    │
│(0x0C=12)  │ /skes/filter │ (小端序)    │  {...}           │
└──────────┴──────────────┴─────────────┴──────────────────┘
```

**举例**：topic="/skes/filter"（12字节），payload=`{"action":"post",...}`（假设100字节）

```
0C 2F 73 6B 65 73 2F 66 69 6C 74 65 72   ← topic
64 00 00 00                               ← payload长度100（小端序）
7B 22 61 63 74 69 6F 6E 22 2E 2E 2E 7D   ← JSON payload
```

### 8.2 消息类型分类

**VEH → Qt（主动推送，action=post）：**
```json
{
  "version": "1.0",
  "id": "",
  "ts": 1749000000000,
  "action": "post",
  "type": "properties",
  "data": {
    "type": "filter_table",
    "data": [
      {"index": -1, "property_name": "vehicleSpeed", "data_type": "double", "data": 5.2},
      {"index": -1, "property_name": "batteryLevel",  "data_type": "int",    "data": 80},
      ...
    ]
  }
}
```

**Qt → VEH（控制命令，action=request）：**
```json
{
  "version": "1.0",
  "id": "qt-instrument",
  "ts": 1749000000000,
  "msg_id": "2841f290-e170-11ec-99d2-4b4f83e67c3f",
  "action": "request",
  "type": "properties",
  "data": {
    "type": "cmd_control",
    "data": [
      {"index": -1, "property_name": "headLight", "data_type": "int", "data": 1}
    ]
  }
}
```

**VEH → Qt（命令响应，action=response）：**
```json
{
  "version": "1.0",
  "id": "",
  "ts": 1749000000000,
  "msg_id": "2841f290-e170-11ec-99d2-4b4f83e67c3f",  ← 原样回填
  "action": "response",
  "type": "properties",
  "data": {
    "type": "cmd_control",
    "result": 0,
    "message": "OK",
    "data": [
      {"property_name": "headLight", "result": 0}
    ]
  }
}
```

### 8.3 CAN1 0x31D 帧结构

这是 VEH 发给叉车 VCU 的控制帧，每 500ms 发一次：

```
BYTE 0（仪表控制位）:
  Bit0-1: 速度模式 (00=E低速, 01=P高速, 10=S标准)，无记忆，不持久化
  Bit2:   驾驶室风扇 cabinFan（无记忆）
  Bit3:   PTC加热 ptc（无记忆）
  Bit4:   前大灯 headLight（无记忆）
  Bit5:   后大灯 rearLight（无记忆）
  Bit6:   货叉监控 forkMonitor（无记忆）

BYTE 1（功能控制位）:
  Bit0-4: 高度定址预设1~5（脉冲位，1秒后自动归零）
  Bit5:   一键水平 oneTouchLeveling（无记忆，QT发0/1控制）
  Bit6:   高度定址停止设置 heightPositioning（带记忆，重启恢复）
  Bit7:   一键水平停止设置 horizontalStopSetting（带记忆，重启恢复）

BYTE 2-6: 保留（全0）

BYTE 7:
  Bit0:   密码验证标志 passwordVerified
```

**"带记忆"和"不带记忆"的区别：**
- 带记忆：写入 `forklift_persistent.json`，重启后 `restoreFrameFromSettings()` 恢复到帧里
- 不带记忆：只写帧缓存，重启后清零，需要 Qt 重新下发

### 8.4 handleCmdControl() — 支持的 property_name 完整列表

| property_name | Bit位置 | 持久化 | 说明 |
|--------------|---------|--------|------|
| `speedMode` | BYTE0 Bit0-1 | 否 | 0=E/1=P/2=S，范围检查 |
| `cabinFan` | BYTE0 Bit2 | 否 | 驾驶室风扇 |
| `ptc` | BYTE0 Bit3 | 否 | PTC加热 |
| `headLight` | BYTE0 Bit4 | 否 | 前大灯 |
| `rearLight` | BYTE0 Bit5 | 否 | 后大灯 |
| `forkMonitor` | BYTE0 Bit6 | 否 | 货叉监控 |
| `heightPreset1~5` | BYTE1 Bit0-4 | 否（脉冲） | 高度定址触发，1秒后自动清零 |
| `oneTouchLeveling` | BYTE1 Bit5 | 否 | 一键水平，Qt发0/1 |
| `heightPositioning` | BYTE1 Bit6 | **是** | 高度定址停止设置 |
| `horizontalStopSetting` | BYTE1 Bit7 | **是** | 一键水平停止设置 |
| `brightness` | 不涉及CAN | 否 | 屏幕亮度，写 settings.screenBrightness |
| `volume` | 不涉及CAN | 否 | 音量，写 settings.warningSound |
| `language` | 不涉及CAN | 否 | 语言设置 |
| `driveSettings.*` | PDO addr 1-14 | 写VCU | 行走参数，异步读写 |
| `pumpSettings.*` | PDO addr 15-30 | 写VCU | 油泵参数，异步读写 |

### 8.5 handleAuth() — 认证系统

密码存在 `forklift_persistent.json`，默认密码 `66999`。

| action | 说明 |
|--------|------|
| `verify` | 验证密码，成功则 `s_auth_verified=1`，写 BYTE7 Bit0 |
| `changePassword` | 旧密码验证后修改，新密码4-16位 |
| `getAuthStatus` | 查询当前认证状态 |
| `factoryReset` | 重置所有设置，恢复默认密码 |

### 8.6 buildAndPublishStatus() — 状态推送

每 500ms 触发一次，把 `realtime_status` 里的字段打包发给 Qt：

**暂时未推送（已注释）的字段：**
- system 区块：systemDate, systemTime, networkSignal
- statusIcons 区块：aeb, bsd, dms, radarStatus 等
- internal 区块：powerOnSignal, chargingStatus 等
- settings 区块：reverseGuideLine, warningSound 等

**正在推送的字段：** mainScreen全字段、ioStatus全字段、faultInfo全字段、runStatus全字段、versionInfo、GPS经纬度、bmsData全字段、fuseBoxStatus全字段、driveSettings/pumpSettings（PDO参数）。

### 8.7 PDO异步写的特殊回复逻辑

当 Qt 发送 PDO 参数修改时（如 `driveSettings.highSpeedMax=80`），VEH 侧的处理是**异步**的：

```
1. handleCmdControl 收到命令
2. 把写操作入队到 VcuPdoManager
3. 函数返回（不立即回复 Qt）
4. VcuPdoManager 状态机运行（CAN 通信）
5. VCU 应答成功 → 回调函数发 response(result=0, value=80)
6. VCU 应答失败 → 回调发 response(result=-6) + 再发 POST(param_rollback) 让 Qt 界面回滚
```

---

## 9. CanTxFrameManager.cpp — CAN发送帧管理器

### 9.1 解决了什么问题

CAN 总线上的 0x31D 帧需要**每 500ms 定期发送**，帧内容随时可能被多处代码修改（Qt 控制命令、密码验证等）。如果每次修改都立即发帧，会有：
- 时序问题（CAN总线繁忙）
- 并发问题（多线程同时修改）

`CanTxFrameManager` 提供一个**帧缓存 + 定时发送**的解决方案。

### 9.2 三种写入方式

```cpp
// 1. 持久位：设置某一位，永久有效直到下次修改
// 用于：大灯、风扇、速度模式等"状态型"控制
mgr.setPersistentBit(0x31D, 0, 4, 1);    // BYTE0 Bit4 置1（前大灯开）
mgr.setPersistentBit(0x31D, 0, 4, 0);    // BYTE0 Bit4 清0（前大灯关）

// 2. 带掩码的持久位：同时设置多个连续位
// 用于：速度模式（占2个bit）
mgr.setPersistentByteMask(0x31D, 0, 0x03, 0x01);  // BYTE0 的低2位设为 01（P档）

// 3. 脉冲位：置1后等 pulse_ms 毫秒自动清零
// 用于：高度定址触发（按下按钮 → 脉冲 → VCU执行 → 自动复位）
mgr.setPulseBit(0x31D, 1, 0, 1000);  // BYTE1 Bit0 置1，1000ms后自动清零
```

### 9.3 tick() — 核心定时函数（每100ms）

```cpp
void CanTxFrameManager::tick() {
    uint64_t ms = now_ms();
    for (auto* e : frames_) {
        // 步骤1：检查有没有脉冲位到期了
        for (auto it = pulse_bits.begin(); it != pulse_bits.end(); ) {
            if (ms >= it->expire_ms) {
                data[byte_idx] &= ~(1 << bit_idx);  // 清零
                it = pbs.erase(it);                   // 从列表移除
            }
        }
        // 步骤2：检查是否到了发送周期（500ms）
        if (时间差 >= period_ms) {
            // 发送当前帧数据
            CanCommandServer_sendCanMessage(channel, can_id, data, 8);
            last_send_ms = ms;  // 更新上次发送时间
        }
    }
}
```

**为什么tick是100ms但帧周期是500ms？**  
tick 越频繁，脉冲位归零越及时（最多延迟100ms）；帧发送由 `period_ms` 控制，两者独立。

### 9.4 扩展帧标识

```cpp
// 0x31D 是扩展帧（use_extended=true）
// 发送时在 CAN ID 最高位置1，表示29位扩展帧
uint32_t tx_id = e->use_extended
                 ? (e->can_id | 0x80000000u)   // 最高位置1
                 : e->can_id;                   // 标准帧不变
```

---

## 10. CanCommandServer.cpp — CAN命令服务器

### 10.1 职责

接收来自 Qt 的**直接 CAN 命令**（JSON格式），转发到指定 CAN 总线通道。

与 SkesQtBridge 的区别：
- `SkesQtBridge`：处理叉车控制逻辑（大灯/风扇/PDO参数），有业务封装
- `CanCommandServer`：透传任意 CAN 帧，不做业务判断，是"万能通道"

### 10.2 命令格式

```json
{
  "channel": 0,          // 0=CAN0, 1=CAN1
  "can_id": "0x18FF8906",
  "dlc": 8,
  "data": [1, 2, 3, 4, 5, 6, 7, 8]
}
```

也支持批量（JSON数组形式一次发多帧）。

### 10.3 CanCommandServer_sendCanMessage()

这个 C 接口函数是整个程序 CAN 发送的**统一出口**，被 CanTxFrameManager、VcuPdoManager 等多处调用：

```cpp
// 封装成20字节的结构体发给 CAN 驱动进程
struct {
    uint32_t can_id;    // CAN ID（最高位表示扩展帧）
    uint16_t can_dlc;   // 数据长度
    uint16_t rsv_ms;    // 保留
    uint8_t  data[8];   // 数据
    uint8_t  rsv[4];    // 保留
} msg;
nn_send(pub_sock, &msg, sizeof(msg), NN_DONTWAIT);
```

---

## 11. VcuPdoManager.cpp — VCU参数读写状态机

### 11.1 什么是PDO

PDO（Process Data Object）是叉车 VCU 内部存储的**可配置参数**，比如：
- 高速最大速度（addr=1）
- 低速加速度（addr=8）
- 起升最大高度（addr=19）

这些参数通过专用 CAN 帧（ID=0x111/0x222）读写，协议比较特殊——每次操作需要**发两帧**。

### 11.2 PDO参数表（共30个参数）

| addr | property_name | 含义 |
|------|--------------|------|
| 1-3 | driveSettings.highSpeed{Max,Accel,Decel} | 高速档最大速度/加速/减速 |
| 4-6 | driveSettings.stdSpeed{Max,Accel,Decel} | 标准档最大速度/加速/减速 |
| 7-9 | driveSettings.lowSpeed{Max,Accel,Decel} | 低速档最大速度/加速/减速 |
| 10-12 | driveSettings.steerPot{Left,Mid,Right} | 方向盘电位器左/中/右限位 |
| 13-14 | driveSettings.loadWheel{BrakeEn,BrakePct} | 承载轮制动使能/制动比例 |
| 15-18 | pumpSettings.{lift,tilt,side,attach}MaxSpeed | 起升/倾斜/侧移/属具最大速度 |
| 19 | pumpSettings.liftMaxHeight | 起升最大高度 |
| 20 | pumpSettings.forkLevelAngle | 货叉水平角度 |
| 21-25 | pumpSettings.heightAddr{1-5} | 高度定址1~5的高度值 |
| 26 | pumpSettings.weighCalibCmd | 称重标定指令（写触发） |
| 27 | pumpSettings.weightSet | 重量设定值 |
| 28 | pumpSettings.pressureCalib | 压力标定值 |
| 29-30 | pumpSettings.{lift,drop}BufHeight | 起升/下降缓冲高度 |

### 11.3 写操作时序（关键！）

```
┌──────────────────────────────────────────────────┐
│ 写操作时序（以 addr=1，val=80 为例）               │
│                                                    │
│  仪表                              VCU             │
│    │                                │              │
│    ├─ 写帧1: 0x111 ────────────────►│              │
│    │  BYTE0=0(写), BYTE1=1(addr),   │              │
│    │  BYTE2-5=80(值), BYTE7=1(使能) │              │
│    │                                │              │
│    │  等待 50ms...                  │              │
│    │                                │              │
│    ├─ 写帧2: 0x111 ────────────────►│              │
│    │  (同上，但 BYTE7=0)            │              │
│    │                                │              │
│    │          等待 VCU 应答...       │              │
│    │                                │              │
│    │◄─ 写应答: 0x222 ───────────────┤              │
│    │  BYTE0=0(写应答), BYTE1=1,     │              │
│    │  BYTE7=0(成功)/1(失败)         │              │
│    │                                │              │
│    │  如果成功，再做读回显验证...     │              │
│    ├─ 读帧1: 0x111 ────────────────►│              │
│    │  BYTE0=1(读), BYTE1=1, BYTE7=1 │              │
│    │                                │              │
│    │  等待 50ms...                  │              │
│    │                                │              │
│    ├─ 读帧2: 0x111 ────────────────►│              │
│    │  (同上，BYTE7=0)               │              │
│    │                                │              │
│    │◄─ 读应答: 0x222 ───────────────┤              │
│    │  BYTE0=1(读应答), BYTE1=1,     │              │
│    │  BYTE2-5=80(实际值)            │              │
│    │                                │              │
│    │ 更新 cache，回调 Qt 写成功      │              │
└──────────────────────────────────────────────────┘
```

### 11.4 状态机状态转换图

```
           ┌─────────────────────────────────────────────┐
           │               PDO_IDLE                       │
           │          （等待队列中的操作）                  │
           └──────────────┬──────────────────────────────┘
                          │ dispatch_next()
           ┌──────────────┴──────────────────────────────┐
           │                                              │
     写操作 ▼                                     读操作 ▼
  ┌─────────────────┐                    ┌─────────────────┐
  │发写帧1(BYTE7=1) │                    │发读帧1(BYTE7=1) │
  │PDO_WRITE_WAIT_  │                    │PDO_READ_WAIT_   │
  │FRAME2           │                    │FRAME2           │
  └────────┬────────┘                    └────────┬────────┘
           │ 50ms定时器到期                        │ 50ms定时器到期
           ▼                                      ▼
  ┌─────────────────┐                    ┌─────────────────┐
  │发写帧2(BYTE7=0) │                    │发读帧2(BYTE7=0) │
  │PDO_WRITE_WAIT_  │                    │PDO_READ_WAIT_   │
  │ACK              │                    │ACK              │
  └────────┬────────┘                    └────────┬────────┘
           │ 收到0x222写应答                       │ 收到0x222读应答
           │ BYTE7=0(成功)                         │ 更新cache，回调
           │       │                               │
           │  失败  │ 成功                          ▼
           │       ▼                          PDO_IDLE
           │ 转为读操作（回显验证）
           │
           └──► 读双帧 ──► 收到读应答 ──► 回调Qt ──► PDO_IDLE
```

### 11.5 线程安全设计

VcuPdoManager 面临一个关键问题：**CAN 帧从 CAN 接收线程到达，但状态机在 ev_loop 主线程**。

解决方案：**ev_async 机制**

```
CAN接收线程                ev_loop主线程
     │                          │
     │  收到 0x222 帧            │
     ▼                          │
 s_inbox_mutex.lock()           │
 s_inbox.push_back(frame)       │
 s_inbox_mutex.unlock()         │
     │                          │
 ev_async_send(s_async_watcher) │
     │                    触发唤醒▼
     │              on_async_222()
     │              从 s_inbox 取帧
     │              process_frame222()
     │              更新状态机
     └──────────────────────────┘
```

**好处：** 状态机操作全部在主线程，不需要给状态机加锁；只有 `s_inbox` 需要加锁（只做数据拷贝，很快）。

### 11.6 上电批量读

程序启动时（`SkesQtBridge::start()` 调用 `VcuPdoManager::triggerPowerOnRead()`），会把30个参数全部排队进行后台读：

```
上电 → 30个 queueBgRead() 入队 → 逐一读取（每个约200-600ms）
     → 更新 driveSettings/pumpSettings cache
     → 下次 Qt 查询时直接返回 cache 值
```

后台读的特点：
- 超时时间短（150ms vs 交互式300ms）
- 不重试（超时直接跳过，不影响启动）
- 只更新 cache，不通知 Qt

---

## 12. 持久化文件说明

### /data/forklift_persistent.json

```json
{
  "password": "66999",            // 操作密码
  "heightPositioning": 0,         // 高度定址停止设置（0x31D BYTE1 Bit6）
  "horizontalStopSetting": 0,     // 一键水平停止设置（0x31D BYTE1 Bit7）
  "totalMileage": 1234.5,         // 总里程（km）
  "totalWorkHours": 567.8,        // 总工时（h）
  "driveMcuWorkHours": 200.1,     // 行走MCU工时（h）
  "steerMcuWorkHours": 180.3,     // 转向MCU工时（h）
  "pumpMcuWorkHours":  150.7      // 油泵MCU工时（h）
}
```

**读写职责分工：**
- `SkesQtBridge`：负责 password, heightPositioning, horizontalStopSetting
- `RealtimeStatusManager`：负责 totalMileage, totalWorkHours, 各MCU工时

两者**共享同一个文件**，通过 `g_persist_file_mutex` 互斥锁协调，写时都是先读再合并写，避免互相覆盖。

---

## 13. 关键场景完整流程

### 场景1：用户按下"前大灯"按钮

```
① Qt 发送（nanomsg PUB → 19225 → skes-relay）:
  {"action":"request","type":"properties","msg_id":"uuid-abc",
   "data":{"type":"cmd_control","data":[
     {"property_name":"headLight","data_type":"int","data":1}
   ]}}

② skes-relay 转发 → nanomsg SUB ← 19226

③ SkesQtBridge::onQtCmd() 收到，调用 handleCmdControl()

④ prop == "headLight" 分支:
   v = 1
   mgr.setPersistentBit(0x31D, 0, 4, 1)  ← 帧缓存BYTE0 Bit4置1
   getRealtimeJson()["chache"]["mainScreen"]["headLight"] = 1  ← JSON更新

⑤ 发送响应（nanomsg PUB → 19225 → skes-relay → Qt）:
  {"action":"response","msg_id":"uuid-abc","data":{"type":"cmd_control","result":0}}

⑥ CanTxFrameManager tick（500ms后）:
   发送 CAN1 0x31D 帧，BYTE0=0x10（Bit4=1）

⑦ 叉车 VCU 收到 0x31D，点亮前大灯

⑧ 下次 buildAndPublishStatus（500ms内）:
   headLight=1 被包含在 filter_table 中推给 Qt
   Qt 界面显示大灯图标亮起
```

### 场景2：程序重启后恢复高度定址停止设置

```
① 上次运行：Qt 发送 heightPositioning=1
   → s_settings[SET_HEIGHT_STOP] = 1
   → setPersistentBit(0x31D, 1, 6, 1)
   → savePersistent() 写入 forklift_persistent.json

② 程序重启

③ SkesQtBridge::init()
   loadPersistent()
   → s_settings[SET_HEIGHT_STOP] = 1（从文件恢复）

④ restoreFrameFromSettings()
   → setPersistentBit(0x31D, 1, 6, 1)（写回帧缓存）

⑤ CanTxFrameManager 启动后每500ms发帧
   → 0x31D BYTE1 Bit6=1 正常发出
   → 叉车 VCU 知道停止设置有效
```

### 场景3：Qt 修改PDO参数（如调整高速最大速度为85）

```
① Qt 发送:
  {"action":"request","msg_id":"uuid-xyz","data":{"type":"cmd_control","data":[
    {"property_name":"driveSettings.highSpeedMax","data_type":"int","data":85}
  ]}}

② handleCmdControl() 识别 driveSettings. 前缀
   findPdoParam("driveSettings.highSpeedMax") → addr=1
   读旧值 old_val = cache["driveSettings"]["highSpeedMax"] = 80
   VcuPdoManager::queueWrite(addr=1, val=85, ...)
   can_rc = -99（标记异步），return 不立即回复

③ VcuPdoManager 状态机运行:
   发 CAN0 0x111 写帧1（addr=1, val=85, BYTE7=1）
   等50ms
   发 CAN0 0x111 写帧2（BYTE7=0）
   等VCU应答（最多300ms）

④ 情况A：VCU 回应 0x222 BYTE7=0（写成功）
   再发读帧验证回显
   VCU 回应 0x222 读应答（val=85）
   update_cache(addr=1, 85)  ← cache更新
   回调：sendPropertiesResponse("uuid-xyz", result=0, value=85)
   Qt 收到，界面显示 85

⑤ 情况B：VCU 回应 0x222 BYTE7=1（写失败）
   回调：sendPropertiesResponse("uuid-xyz", result=-6, value=80)
   同时 POST param_rollback: driveSettings.highSpeedMax=80
   Qt 收到，界面回滚显示 80
```

### 场景4：叉车发生行走MCU故障

```
① VCU 发出 CAN0 0x0CFF0208 帧
   BYTE4~5 = 0x0A 0x00（小端序故障码=10）

② CAN接收线程调用 updateRealtimeStatus_bycan()
   识别 can_id == 0x0CFF0208
   code = (data[5]<<8) | data[4] = 10
   realtime_status["chache"]["faultInfo"]["driveMcuErr"] = 10
   updateFaultEntry(3, 10)  ← 故障类型3，故障码10
   → faultList 中增加 {type:3, type_name:"行走MCU故障", code:10, time:"HH:MM:SS"}
   → faultCount = faultList.size() = +1

③ 下次 buildAndPublishStatus 推送:
   faultCount = 1
   driveMcuErr = 10
   faultList = "[{\"type\":3,\"code\":10,...}]"（JSON字符串）

④ Qt 仪表收到，显示故障图标，展示故障信息

⑤ 故障恢复：VCU 发 0x0CFF0208 BYTE4~5 = 0x00 0x00
   updateFaultEntry(3, 0)
   → faultList 中移除 type=3 的条目
   → faultCount 减1
   → Qt 故障图标消除
```

---

## 14. 端口与地址速查表

| 端口/地址 | 协议 | 方向 | 用途 |
|---------|------|------|------|
| `tcp://127.0.0.1:16002` | nanomsg PUB | CAN驱动→VEH | CAN0 原始帧接收 |
| `tcp://127.0.0.1:16003` | nanomsg PUB | CAN驱动→VEH | CAN1 原始帧接收 |
| `tcp://127.0.0.1:16005` | nanomsg SUB | GPS驱动→VEH | GPS 位置数据 |
| `tcp://127.0.0.1:16006/7/30/31` | nanomsg | Modbus→VEH | Modbus 传感器数据 |
| `tcp://127.0.0.1:19225` | nanomsg PUB | VEH→skes-relay | 状态推送/命令响应 |
| `tcp://127.0.0.1:19226` | nanomsg SUB | skes-relay→VEH | Qt 命令接收 |
| `tcp://127.0.0.1:26002` | nanomsg PUB | VEH→CAN驱动 | CAN0 发送 |
| `tcp://127.0.0.1:26003` | nanomsg PUB | VEH→CAN驱动 | CAN1 发送（含0x31D/0x111） |
| `tcp://127.0.0.1:27000` | nanomsg SUB | Qt→VEH | Qt直接CAN命令 |
| `tcp://127.0.0.1:38001` | TCP | 外部→VEH | 4G信号查询 |
| `UDP :9001` | UDP | 双向 | 状态查询服务 |
| `tcp://127.0.0.1:5555` | nanomsg SUB | 外部→VEH | alarm事件订阅 |

### CAN ID 速查

| CAN ID | 通道 | 方向 | 用途 |
|--------|------|------|------|
| `0x31A` | CAN0 | VCU→仪表 | 车辆主状态（速度/电量/方向等） |
| `0x31D` | **CAN1** | 仪表→VCU | 仪表控制帧（大灯/风扇/高度定址） |
| `0x18FD0291` | CAN0 | VCU→仪表 | 保险盒+钥匙+大灯状态 |
| `0x0CFF7C03` | CAN0 | BMS→仪表 | BMS主帧 |
| `0x0CFF0008` | **CAN1** | 行走MCU→仪表 | 行车方向/行走电机数据 |
| `0x360` | CAN0 | 转向MCU→仪表 | 转向电机状态/转速 |
| `0x32A/B/C` | CAN1 | VCU→仪表 | IO状态 |
| `0x32D/E` | CAN1 | VCU→仪表 | 故障码 |
| `0x33A` | CAN1 | VCU→仪表 | 软件版本 |
| `0x111` | CAN0 | 仪表→VCU | PDO参数读写请求 |
| `0x222` | CAN0 | VCU→仪表 | PDO参数读写应答 |

---

## 15. 常见问题速查

### Q1：程序启动后发现某些字段一直是0？

可能原因：
1. 对应 CAN 帧没有收到（VCU未上电、CAN 总线未连接）
2. CAN 帧的 `can_channel` 配置错误（如 0x0CFF0008 应该是 ch1 而非 ch0）
3. `realtime_status.json` 字段名拼写不一致

### Q2：Qt 发控制命令无响应？

排查步骤：
1. 确认 skes-relay 进程是否运行
2. 用 `skes_qt_bridge_tester subscribe` 确认 VEH 是否有推送
3. 检查 msg_id 是否为 UUID 格式（非 UUID 也能工作，但协议不规范）
4. 查看 VEH 日志是否有 `未知 property_name` 输出

### Q3：PDO参数写入后Qt界面没更新？

VcuPdoManager 是异步的，Qt 要等到回调才知道结果：
1. 写操作入队 → Qt 不立即收到 response
2. VCU 应答（最多 300ms × 2次） → 才发 response
3. 如果超过 600ms 还没收到 → VCU 可能未响应或CAN1未连接

### Q4：重启后大灯/风扇状态消失？

这是**设计行为**：BYTE0 的控制位（大灯/风扇/速度模式等）均**无掉电记忆**，重启后 Qt 端需要重新下发。只有 `heightPositioning` 和 `horizontalStopSetting`（BYTE1 Bit6/7）带记忆。

### Q5：forklift_persistent.json 被损坏怎么办？

程序有保护：读文件失败时使用默认值（密码=66999，停止设置=0，里程/工时=0）并重新写入一个干净的文件。直接删除 forklift_persistent.json 也可以达到重置效果。

### Q6：如何新增一个要推送给 Qt 的字段？

三步走：
1. 在 `realtime_status.json` 的对应 section 里加字段（给默认值）
2. 在 `RealtimeStatusManager::updateRealtimeStatus_bycan()` 里添加 CAN 帧解析
3. 在 `SkesQtBridge::buildAndPublishStatus()` 里加一行 `addProp(...)`

### Q7：如何新增一个 Qt 可控制的 CAN 位？

三步走：
1. 确定 0x31D 帧的哪个 BYTE/BIT
2. 在 `handleCmdControl()` 里添加 `prop == "新字段名"` 分支
3. 在 `buildAndPublishStatus()` 里用 `cInt` 从 JSON 读出值并 `addProp`

---

*文档生成时间：2026-03-20*  
*基于源码版本：最新zip（含VcuPdoManager模块）*
