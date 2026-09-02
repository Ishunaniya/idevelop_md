# skes-aeb2vcu 与 vehicleDataEngine 全面对比分析

> 本文档对比分析两套源码的通信方式、数据组装、数据解析、数据上传与接收的全部差异。

---

## 一、系统定位与角色

| 维度 | skes-aeb2vcu | vehicleDataEngine (rawframe_process) |
|------|-------------|--------------------------------------|
| **核心角色** | SKES 感知结果 → CAN 转发器 | CAN 总线数据 → SKES/QT 展示桥接器 |
| **数据流方向** | SKES感知数据 **下行** → CAN 总线（控制VCU）| CAN 总线数据 **上行** → JSON → SKES → QT 显示 |
| **主要职责** | 订阅AEB/BSD/DMS报警事件，组装成CAN帧发给VCU | 接收CAN帧解析为结构化JSON，通过SKES推送给QT仪表 |
| **编程语言** | 纯 C（C99）| C++11/14 |
| **进程生命周期** | 两个 pthread 线程（轮询+CAN发送）| libev 事件循环 + 多线程（CAN接收+SKES推送）|

---

## 二、整体架构对比

### skes-aeb2vcu 数据流

```
skes-relay(19226) ──SUB──▶ skes-aeb2vcu
                              │  接收 filter/event topic
                              │  解析 AEB/BSD/DMS/故障 JSON
                              │  组装 can_frame_t 结构体
                              ▼
                    nanomsg PUB ──▶ CAN0 TX (port 26002)
                              │
                              ▼
                        VCU（0x4BE / 0x0CFF2003）
```

### vehicleDataEngine 数据流

```
CAN0 RX(16002) ──SUB──▶ can_receiver
CAN1 RX(16003) ──SUB──▶ can_receiver
                              │  updateRealtimeStatus_bycan()
                              │  解析各 CAN ID 写入 realtime_status.json
                              ▼
                    RealtimeStatusManager (内存JSON)
                              │
                              ▼
                    SkesQtBridge（每500ms）
                              │  buildAndPublishStatus()
                              │  组装 filter_table JSON
                              ▼
                    nanomsg PUB ──▶ skes-relay(19225)
                              │
skes-relay(19226) ◀──────────┘  广播给所有订阅者（QT仪表等）

QT 仪表 ──▶ skes-relay(19225) ──▶ vehicleDataEngine(SUB 19226)
              （cmd_control / query 下发命令）
```

---

## 三、通信方式对比

### 3.1 nanomsg 连接拓扑

| 项目 | skes-aeb2vcu | vehicleDataEngine |
|------|-------------|-------------------|
| **SKES SUB 连接** | `nn_connect` → `tcp://127.0.0.1:19226` | `nn_connect` → `tcp://127.0.0.1:19226` |
| **SKES PUB 连接** | `nn_connect` → `tcp://127.0.0.1:19225` | `nn_connect` → `tcp://127.0.0.1:19225` |
| **CAN RX 订阅** | ❌ 不直接接收 CAN 帧 | `nn_connect` → `tcp://127.0.0.1:16002/16003` (NN_SUB) |
| **CAN TX 发送** | `nn_connect` → `tcp://127.0.0.1:26002` (NN_PUB，直连) | 通过 `CanCommandServer` → `tcp://127.0.0.1:26002/26003` |
| **额外订阅** | 无 | `tcp://127.0.0.1:5555` 订阅 alarm 主题 |
| **连接模式** | 全部 `nn_connect`（客户端角色）| 全部 `nn_connect`（客户端角色）|
| **SKES 库层次** | 使用 **skes-linker 高层库**（封装订阅+回调）| 使用 **原始 nanomsg API**（手动收发+解析）|

### 3.2 SKES 协议层使用方式差异

**skes-aeb2vcu（高层库封装）**：
```c
// 初始化：直接用 skes-linker 库，自动处理订阅/回调注册
skes_linker_init(&handle_sub, "tcp://127.0.0.1:19226",
                 &handle_pub, "tcp://127.0.0.1:19225",
                 false, on_skes_msg_received, sre);

// 消息接收：库回调，topic已被库解码为枚举
static int on_skes_msg_received(skes_handle_t *handle,
                                skes_topic_type_t topic_type,  // 枚举值
                                const void *msg, int msg_len) {
    switch (topic_type) {
        case skes_topic_type_filter:  on_msg_filter(...);  break;
        case skes_topic_type_event:   on_msg_event(...);   break;
    }
}

// 消息发送：用库封装的 skes_msg_reply()
skes_msg_reply(handle_pub, skes_topic_type_heartbeat, data, strlen(data));
```

**vehicleDataEngine（原始 nanomsg 自己实现）**：
```cpp
// 自定义二进制帧格式：[1字节topic长度][N字节topic][4字节payload长度(LE)][payload JSON]
static std::string buildMsg(const std::string& topic, const json& payload) {
    uint8_t topic_len = (uint8_t)(topic.size() & 0xFF);
    // 手动拼接二进制帧
    frame.push_back((char)topic_len);
    frame.append(topic);
    // 小端序4字节长度
    uint32_t payload_len = (uint32_t)json_str.size();
    frame.push_back((char)(payload_len & 0xFF));
    ...
}

// 手动解析接收帧
static bool extractJson(const char* buf, int n, json& out, std::string& topic_out) {
    uint8_t topic_len = (uint8_t)buf[0];
    topic_out = std::string(buf + 1, topic_len);
    // 手动读4字节长度
    ...
}

// 手动订阅过滤
if (topic != SKES_TOPIC_FILTER) continue;  // 只处理 /skes/filter
```

> **关键区别**：skes-aeb2vcu 依赖 `skes-linker` 库屏蔽了底层二进制帧格式和 topic 路由，vehicleDataEngine 自己实现了同等功能，因此帧格式必须与 skes-relay 约定一致（首字节=topic长度的二进制协议）。

---

## 四、订阅主题（Topic）对比

| Topic | skes-aeb2vcu 是否处理 | vehicleDataEngine 是否处理 | 说明 |
|-------|--------------------|--------------------------|------|
| `/skes/filter` (`filter`) | ✅ **接收** — 解析 AEB/BSD/DMS 属性 | ✅ **发送** — 推送车辆状态给QT；**接收** — 处理QT命令 | 双方都用，但方向相反 |
| `/skes/event` (`event`) | ✅ **接收** — 解析 fault/alarm 事件 | ❌ 不处理 | aeb2vcu专用 |
| `heartbeat` | ✅ 定义了处理（`#if 0`禁用）| ❌ 不处理 | |
| `version` | ✅ 定义了处理（`#if 0`禁用）| ❌ 不处理 | |
| `dev_data` | ✅ 定义了处理（`#if 0`禁用）| ❌ 不处理 | |
| `location` | ✅ 定义了处理（`#if 0`禁用）| ❌ 不处理 | |
| `alarm`（port 5555）| ❌ 不处理 | ✅ **接收**（独立端口5555，topic前缀过滤）| vehicleDataEngine专用 |

---

## 五、JSON 消息格式对比

### 5.1 skes-aeb2vcu 接收的消息格式

**filter topic（`type=properties`，由感知模块发布）**：
```json
{
  "version": "1.0",
  "id": "ai-module",
  "ts": 1234567890,
  "action": "post",
  "type": "properties",
  "data": {
    "type": "AEB",
    "data": [
      { "property_name": "AEBActivateCmd", "data": 2 },
      { "property_name": "FlashCmd",       "data": 1 },
      { "property_name": "BrkCurCmd_Front","data": 800 }
    ]
  }
}
```

**event topic（`type=alarm`，由AI感知模块发布）**：
```json
{
  "version": "1.0",
  "action": "post",
  "type": "alarm",
  "data": [
    {
      "source": "bsd",
      "status": "triggered",
      "data": [
        {
          "camera": { "position": "rear" },
          "bounding_boxes": [
            {
              "detected_position": { "angle": 1.56, "distance": 1.84 }
            }
          ]
        }
      ]
    }
  ]
}
```

**event topic（`type=fault`，设备故障上报）**：
```json
{
  "type": "fault",
  "data": [
    {
      "source": "camera",
      "data": [ { "code": 1, "position": "left_side" } ]
    },
    {
      "source": "communication",
      "data": [ { "code": 1, "type": "lte" } ]
    }
  ]
}
```

---

### 5.2 vehicleDataEngine 发送/接收的消息格式

**VEH → QT 状态推送（filter_table，每500ms）**：
```json
{
  "version": "1.0",
  "id": "veh-engine",
  "ts": 1234567890000,
  "action": "post",
  "type": "properties",
  "data": {
    "type": "filter_table",
    "data": [
      { "property_name": "vehicleSpeed",  "dtype": "double", "data": 5.0 },
      { "property_name": "ready",         "dtype": "int",    "data": 1   },
      { "property_name": "batteryLevel",  "dtype": "int",    "data": 80  },
      { "property_name": "driveMotorTemp","dtype": "int",    "data": 45  }
    ]
  }
}
```

**QT → VEH 控制命令（cmd_control）**：
```json
{
  "version": "1.0",
  "id": "qt-client",
  "ts": 1234567890000,
  "action": "request",
  "type": "properties",
  "data": {
    "type": "cmd_control",
    "properties": [
      { "name": "speedMode", "data": 2 }
    ]
  }
}
```

**QT → VEH 查询（query）**：
```json
{
  "action": "request",
  "type": "properties",
  "data": {
    "type": "query",
    "property_name": "mainScreen.vehicleSpeed"
  }
}
```

---

### 5.3 JSON 解析库对比

| 项目 | skes-aeb2vcu | vehicleDataEngine |
|------|-------------|-------------------|
| **JSON 库** | `cJSON`（C语言轻量库）| `nlohmann::json`（C++ 现代库）|
| **解析风格** | `cJSON_GetObjectItem()` + `cJSON_GetStringValueEx()` 手动遍历 | `j["key"]` / `j.value("key", default)` 运算符重载 |
| **内存管理** | 手动 `cJSON_Delete()` 释放 | RAII 自动管理 |
| **错误处理** | 返回码 + NULL检查 | try/catch + `contains()` 检查 |
| **类型转换** | `cJSON_GetDoubleValue()` / `cJSON_GetInt32Value()` | 隐式转换 `(int)j["key"]` |

---

## 六、CAN 帧结构对比

两套代码使用**相同的** `can_frame_t` 物理结构（20字节）：

```
偏移  大小  字段
0     4     can_id       CAN 帧 ID（扩展帧时 bit31=1）
4     2     can_dlc      数据长度 0~8
6     2     rsv_ms       MCU 运行时间（aeb2vcu 定义为 rsv_ms）
8     8     data[8]      CAN 数据负载
16    4     timestamp    RTC 时间戳（vehicleDataEngine 定义）
```

> ⚠️ **细微差异**：  
> - aeb2vcu 的 `can_frame_t` 最后4字节命名为 `timestamp`  
> - vehicleDataEngine 的 `can_frame_t` 最后4字节命名为 `rsv[4]`  
> - 物理布局完全一致，可互通

### 扩展帧标志位

两者都用 `can_id | 0x80000000` 标记扩展帧：
```c
// aeb2vcu
frame.can_id = AEB_FAULT_CAN_ID | 0x80000000;  // 0x0CFF2003 扩展帧

// vehicleDataEngine
msg.id = can_id | 0x80000000;  // 扩展帧标识
```

---

## 七、数据组装对比

### 7.1 skes-aeb2vcu：组装 CAN 帧发给 VCU

aeb2vcu 的核心工作是将感知 JSON 数据**组装成结构化二进制 CAN 帧**：

```c
// 第一步：从 SKES filter 消息里提取属性到内存结构体
sre->aeb_fault.A2V_AEBActivateCmd = (uint8_t)data;  // 直接填字段
sre->aeb_fault.A2V_BrkCurCmd_Front = (uint16_t)data;

// 第二步：计算生命信号+校验和
chksum = calculate_checksum((unsigned char *)&sre->aeb_fault,
                             AEB_FAULT_CAN_ID,
                             sre->aeb_fault.A2V_LifeSignal);
sre->aeb_fault.A2V_LifeSignal++;   // 递增生命信号
sre->aeb_fault.Checksum = chksum;

// 第三步：内存拷贝到 CAN 帧
memcpy(frame.data, &sre->aeb_fault, sizeof(aeb_fault_t));  // 结构体直接拷
frame.can_id = AEB_FAULT_CAN_ID | 0x80000000;
frame.can_dlc = 8;

// 第四步：通过 nanomsg 发出
nn_send(can_nn_fd, &frame, sizeof(can_frame_t), 0);
```

**Gj_ack（国际版应答帧 0x4BE，每 100ms 发送）**：
```c
// 汇总 BSD 触发状态到 gj_ack 结构体
sre->gj_ack.bsd_rear_alarm  = sre->bsd_rear_trigger  ? 1 : 0;
sre->gj_ack.bsd_left_alarm  = sre->bsd_left_trigger  ? 1 : 0;
sre->gj_ack.bsd_right_alarm = sre->bsd_right_trigger ? 1 : 0;
sre->gj_ack.heartbeat++;  // 心跳递增

memcpy(frame.data, &sre->gj_ack, sizeof(Gj_ack_t));
frame.can_id = 0x4BE;  // 标准帧（无0x80000000）
```

**校验和算法**（aeb2vcu 独有）：
```c
static uint8_t calculate_checksum(unsigned char *frame_data, uint32_t can_id, uint8_t LifeSignal) {
    uint32_t checksum = 0;
    // 累加前7个数据字节
    for (int i = 0; i < 7; i++) checksum += frame_data[i];
    // 加生命信号低4位
    checksum += (LifeSignal & 0x0F);
    // 加CAN ID的4个字节
    checksum += (can_id & 0xFF) + ((can_id>>8)&0xFF) + ((can_id>>16)&0xFF) + ((can_id>>24)&0xFF);
    // 三段混合压缩到3bit
    return (((checksum >> 6) & 0x03) + (checksum >> 3) + checksum) & 0x07;
}
```

> vehicleDataEngine **没有校验和机制**，CAN 帧直接发送无额外校验字段。

---

### 7.2 vehicleDataEngine：组装 SKES JSON 推送

vehicleDataEngine 将 CAN 原始数据**组装成 JSON 属性表**：

```cpp
// 第一步：CAN 帧解析，写入内存 JSON（逐帧逐字节解析）
uint16_t speedRaw = can_data[0];
double vehicleSpeed = speedRaw * 0.1;
realtime_status["chache"]["mainScreen"]["vehicleSpeed"] = vehicleSpeed;

// 行走电机扭矩（偏移量处理）
uint16_t torqueRaw = can_data[2] | ((uint16_t)can_data[3] << 8);
int driveMotorTorque = (int)torqueRaw - 1023;
realtime_status["chache"]["runStatus"]["driveMotorTorque"] = driveMotorTorque;

// 第二步：每500ms定时器触发，构建 filter_table 数组
auto addProp = [&](const char* name, const char* dtype, const json& val) {
    json item;
    item["property_name"] = name;
    item["dtype"] = dtype;
    item["data"] = val;
    data_arr.push_back(item);
};

addProp("vehicleSpeed",    "double", cDbl("mainScreen", "vehicleSpeed"));
addProp("driveMotorTorque","int",    cInt("runStatus",  "driveMotorTorque"));
// ... 共约80个字段

// 第三步：组装外层消息
msg_json["version"] = "1.0";
msg_json["id"]      = "veh-engine";
msg_json["action"]  = "post";
msg_json["type"]    = "properties";
msg_json["data"]["type"] = "filter_table";
msg_json["data"]["data"] = std::move(data_arr);

// 第四步：序列化 + 二进制帧封装 + nanomsg 发送
std::string msg = buildMsg(SKES_TOPIC_FILTER, msg_json);
nn_send(s_pub_sock, msg.c_str(), msg.size(), NN_DONTWAIT);
```

---

## 八、数据解析对比

### 8.1 skes-aeb2vcu 的解析层次

```
原始 nanomsg 消息
    └─▶ skes-linker 库（自动提取topic、解包JSON）
            └─▶ skes_msg_header_parse()（提取 version/id/ts/msg_id/action/type/data）
                    ├─▶ type="properties" → on_msg_aeb_filter_parse()
                    │       └─▶ 遍历 data.data[] 数组，按 type+property_name 分发
                    │               "AEB" → aeb_fault 结构体字段
                    │               "CentralControlScreen" → gj_ack.MachineType
                    │               "FaceRecognition" → gj_ack.FaceRecognitionResult
                    └─▶ type="alarm" → on_msg_alarm_parse()
                    │       ├─▶ source="aeb" → 提取 rear 摄像头 angle/distance
                    │       └─▶ source="bsd" → 按 position 设置 bsd_*_trigger 标志
                    └─▶ type="fault" → on_msg_fault_parse()
                            ├─▶ source="camera"            → 按 position 查表
                            ├─▶ source="object_detection"  → 按 type 查表
                            ├─▶ source="communication"     → 按 type 查表
                            ├─▶ source="sound_light_alarm" → 直接赋值
                            └─▶ source="data"              → 直接赋值
```

### 8.2 vehicleDataEngine 的解析层次

```
原始 nanomsg 二进制帧
    └─▶ extractJson()（手动解析：topic长度+topic+4字节长度+JSON）
            └─▶ 判断 topic == "/skes/filter"
                    └─▶ nlohmann::json 解析
                            └─▶ action="request" + type="properties"
                                    ├─▶ dtype="cmd_control" → handleCommand()
                                    │       └─▶ 逐属性处理：speedMode/fan/headlight...
                                    │               → setPersistentBit() / setPulseBit()
                                    │               → CanTxFrameManager → 0x31D CAN帧
                                    └─▶ dtype="query" → handleQuery()
                                            └─▶ 按 section.field 路径查 realtime_status
                                                → 返回 response JSON

CAN 原始帧（can_receiver 线程）
    └─▶ real_manager->updateRealtimeStatus_bycan(can_id, can_data, channel)
            └─▶ 按 can_channel + can_id 匹配解析块
                    ├─▶ CAN0 0x31A → vehicleSpeed/ready/speedMode/batteryLevel...
                    ├─▶ CAN0 0x0CFF0008 → driveDirection/torque/speed/state
                    ├─▶ CAN1 0x32A → accelPedalAi1/angleSensorAi1...
                    └─▶ ...（共20+个帧解析块）
```

---

## 九、CAN 发送机制对比

| 项目 | skes-aeb2vcu | vehicleDataEngine |
|------|-------------|-------------------|
| **发送方式** | 单独 pthread，独立 `nn_socket(NN_PUB)` + `nn_connect(26002)` | `CanTxFrameManager` 单例 + `CanCommandServer`，通过 libev 定时器触发 |
| **发送频率** | 固定 100ms（`usleep(100000)`）| 帧级别配置：0x31D 固定 500ms 周期 |
| **帧管理** | 无帧管理，每次直接发送当前状态 | `CanTxFrameEntry` 表，支持持久位/脉冲位/掉电记忆 |
| **持久位** | 无 | `setPersistentBit()`：掉电持久化，重启恢复 |
| **脉冲位** | 无 | `setPulseBit(ms)`：N ms 后自动清零（如 1000ms）|
| **校验和** | ✅ 有（生命信号 + CAN ID 参与计算）| ❌ 无 |
| **生命信号** | ✅ `A2V_LifeSignal` 每次递增 | ❌ 无 |
| **多帧支持** | 单一帧（0x4BE，aeb_fault 被 `#if 0` 禁用）| 多帧（0x31D 等），可扩展 |
| **连接目标** | 直接 connect 到 CAN TX 端口 26002 | 通过 CanCommandServer 中转，支持多通道（26002/26003）|

---

## 十、CAN 接收机制对比

| 项目 | skes-aeb2vcu | vehicleDataEngine |
|------|-------------|-------------------|
| **是否直接接收 CAN 帧** | ❌ 不接收 CAN 总线数据 | ✅ 接收 CAN0(16002) + CAN1(16003）|
| **接收方式** | 无 | `nn_poll()` 轮询多路复用，`nn_recv()` 取帧 |
| **解析入口** | 无 | `updateRealtimeStatus_bycan(can_id, data, channel)` |
| **通道区分** | 无 | channel=0(CAN0) / channel=1(CAN1)，同ID不同通道含义不同 |

---

## 十一、线程模型对比

### skes-aeb2vcu（双线程）

```
main()
  ├─▶ pthread_create → do_sre_msg_poller()  线程1: SKES消息轮询
  │       └─▶ while(running): skes_run(handle_sub)  阻塞等待消息
  │               └─▶ on_skes_msg_received() 回调处理
  └─▶ pthread_create → do_can_msg_sender()  线程2: CAN帧定时发送
          └─▶ while(running): 组装帧 → nn_send() → usleep(100ms)
```

### vehicleDataEngine（libev事件驱动 + CAN线程）

```
main()
  ├─▶ pthread_create → start_can_server()   线程1: CAN接收（阻塞nn_poll）
  │       └─▶ nn_poll(CAN0,CAN1) → nn_recv() → updateRealtimeStatus_bycan()
  ├─▶ ev_run(loop)                           主线程: libev事件循环
  │       ├─▶ ev_timer → SkesQtBridge::onPubTimer()     每500ms推送
  │       ├─▶ ev_timer → CanTxFrameManager::timerCb()   帧发送定时器
  │       ├─▶ ev_io → handleSKESInput()                 SKES命令接收
  │       └─▶ ev_timer → NetworkStatusQuery             网络状态查询
  ├─▶ NanoSubscriber sub(19226)              SKES主订阅（带回调）
  └─▶ NanoSubscriber alarmSub(5555)          alarm专用订阅
```

---

## 十二、持久化机制对比

| 项目 | skes-aeb2vcu | vehicleDataEngine |
|------|-------------|-------------------|
| **持久化文件** | ❌ 无持久化 | ✅ `/tmp/forklift_persistent.json` |
| **持久化字段** | 无 | totalMileage / totalWorkHours / speedMode / password / heightStop / horizontalStop |
| **保存策略** | 无 | 每60s定时 `savePersistentData()`，read-merge-write（防止字段覆盖）|
| **恢复策略** | 无 | 启动时 `loadPersistentData()` 恢复，并同步到 CAN TX 帧 |
| **重启丢失** | 所有状态重置 | 关键控制状态恢复，`passwordVerified` 重置 |

---

## 十三、故障/报警处理对比

| 项目 | skes-aeb2vcu | vehicleDataEngine |
|------|-------------|-------------------|
| **故障来源** | SKES event topic（AI感知模块上报）| CAN 总线 0x32D/0x32E（VCU上报）|
| **故障类型** | 摄像头/声光报警/目标检测/数据/通信 共16种 | 用户提醒故障 / VCU故障码 |
| **故障存储** | `aeb_fault_code_t fault_code_list[]` 静态数组（内存中）| `realtime_status["chache"]["faultInfo"]` JSON字段 |
| **故障下发** | 转换为 CAN 帧字段 `Fault_Code` 发给VCU（被`#if 0`禁用）| 不下发，仅推送给QT显示 |
| **BSD报警** | 解析 bounding_boxes + detected_position，提取 angle/distance | 从 CAN0 0x0CFF0008 等帧提取 BSD 状态写入 statusIcons.bsd |
| **声光报警轮询** | 实现了 `bsd_rear/left/right` 优先级轮询逻辑（`#if 0`禁用）| 不处理 |

---

## 十四、核心差异总结

| 维度 | skes-aeb2vcu | vehicleDataEngine |
|------|-------------|-------------------|
| **数据流向** | SKES → VCU（感知控制）| CAN → SKES → QT（状态显示）|
| **SKES 使用方式** | 消费者（订阅感知结果）| 生产者（发布车辆状态）+ 消费者（接收QT命令）|
| **CAN 角色** | 纯发送（组装控制帧给VCU）| 纯接收（解析VCU状态帧）|
| **skes 协议层** | 高层库 skes-linker 封装 | 自行实现二进制帧协议 |
| **JSON 库** | cJSON（C）| nlohmann::json（C++）|
| **帧安全机制** | 生命信号 + 3bit校验和 | 无 |
| **帧周期控制** | 粗粒度：全部100ms固定周期 | 精细：每帧独立周期，支持脉冲/持久两种模式 |
| **状态持久化** | 无 | 60s自动持久化，重启恢复 |
| **线程模型** | 双pthread（轮询+发送）| libev事件循环 + CAN接收线程 |
| **扩展性** | 依赖 skes-linker 接口扩展 | 直接在解析函数中添加 CAN ID 分支 |
| **代码量级** | ~1000行（含大量`#if 0`）| ~3000行（RSM+SkesQtBridge+其他）|

---

## 十五、端口全景图

```
┌─────────────────────────────────────────────────────────┐
│                    nanomsg 端口全景                       │
├───────────────────┬────────────────────┬────────────────┤
│      端口          │       用途          │    谁绑定       │
├───────────────────┼────────────────────┼────────────────┤
│ 16002 (CAN0 RX)  │ CAN0 接收广播       │ CAN驱动/桥接层  │
│ 16003 (CAN1 RX)  │ CAN1 接收广播       │ CAN驱动/桥接层  │
│ 16004 (CAN2 RX)  │ CAN2 接收广播       │ CAN驱动/桥接层  │
├───────────────────┼────────────────────┼────────────────┤
│ 26002 (CAN0 TX)  │ CAN0 发送           │ CAN驱动/桥接层  │
│ 26003 (CAN1 TX)  │ CAN1 发送           │ CAN驱动/桥接层  │
│ 26004 (CAN2 TX)  │ CAN2 发送           │ CAN驱动/桥接层  │
├───────────────────┼────────────────────┼────────────────┤
│ 19225 (SKES IN)  │ 各模块发布消息入口   │ skes-relay     │
│ 19226 (SKES OUT) │ 广播给所有订阅者     │ skes-relay     │
├───────────────────┼────────────────────┼────────────────┤
│ 5555 (alarm)     │ alarm事件专用广播   │ AI感知模块      │
│ 27000 (QT cmd)   │ QT命令接收入口      │ CanCommandServer│
└───────────────────┴────────────────────┴────────────────┘

skes-aeb2vcu:
  connect → 19226 (SUB，接收感知结果)
  connect → 19225 (PUB，回复/上报，实际未使用)
  connect → 26002 (PUB，发送CAN控制帧到VCU)

vehicleDataEngine:
  connect → 16002 (SUB，接收CAN0帧)
  connect → 16003 (SUB，接收CAN1帧)
  connect → 19225 (PUB，推送车辆状态)
  connect → 19226 (SUB，接收QT命令)
  connect → 5555  (SUB，接收alarm事件)
  bind    → 27000 (PAIR，接收QT直接命令)
  connect → 26002 (PUB via CanCommandServer，发送0x31D等控制帧)
```

---

## 十六、协作关系

两套源码在同一系统中**互为上下游**：

```
AI感知模块（摄像头/雷达）
    │ 发布 event(alarm/fault) + filter(AEB/BSD属性)
    ▼
skes-relay (19225/19226)
    │
    ├──▶ skes-aeb2vcu
    │       │ 解析感知报警 → 组装 Gj_ack CAN帧
    │       ▼
    │     CAN0 TX(26002) → 物理CAN总线 → VCU
    │
    └──▶ vehicleDataEngine（SkesQtBridge）
            │ 接收QT命令 → 解析 → setPersistentBit
            ▼
          CAN0 TX(26002) → 物理CAN总线 → VCU(0x31D)

VCU → 物理CAN总线
    │
    ├──▶ CAN0 RX(16002) → vehicleDataEngine（解析状态帧）
    │       → realtime_status JSON
    │       → SKES filter_table 推送
    │       ▼
    │     QT仪表（显示）
    │
    └──▶ CAN1 RX(16003) → vehicleDataEngine（解析IO/故障帧）
```

> **简言之**：skes-aeb2vcu 负责把"AI看到了什么"翻译成"VCU应该怎么刹车"；vehicleDataEngine 负责把"VCU当前状态是什么"翻译成"仪表盘应该显示什么"。两者共享同一套 CAN 总线和 SKES 消息总线，但数据流方向完全相反。
