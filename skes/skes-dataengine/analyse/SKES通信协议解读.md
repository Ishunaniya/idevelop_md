# SKES 通信协议解读指南

> 基于《SKES的通信规范 v0.8》整理，面向初学者的完整解析

---

## 一、先搞清楚：这个系统到底是什么？

**SKES**（SKy-Eyes System，天眼系统）是一套**车载智能感知系统**，集成了：

- 📷 摄像头（多路，含DMS驾驶员监控）
- 🤖 AI算法（AEB自动紧急制动、BSD盲区检测、目标检测等）
- 📡 工况数据采集（CAN总线数据）
- 📍 GPS位置信息

这套协议规范的是**系统内部各模块之间如何通信**，类似于规定了"各个部件说话的语言"。

---

## 二、系统架构：三个核心角色

```
┌─────────────────────────────────────────────────────────┐
│                     SKES 系统                            │
│                                                         │
│   ┌──────────┐   pub(port:19225)   ┌──────────────┐    │
│   │          │ ──────────────────> │              │    │
│   │   sep    │                     │  skes-relay  │    │
│   │(功能程序) │ <────────────────── │  (消息中转站) │    │
│   │          │   pub(port:19226)   │              │    │
│   └──────────┘                     └──────────────┘    │
│                                                         │
│  sep 的例子：                                            │
│    - 摄像头处理程序                                       │
│    - CAN数据采集程序                                      │
│    - GPS上报程序                                         │
└─────────────────────────────────────────────────────────┘
```

| 角色 | 全称 | 作用 |
|------|------|------|
| **skes-relay** | 消息中转站 | 接收所有消息并转发（广播），类似一个消息总线 |
| **sep** | Skes-EndPoint（端点程序） | 各功能模块，既能发消息也能收消息 |
| **skes-linker** | 通信库 | 对nanomsg的封装，开发时直接调用这个库 |

### ⚠️ 重要注意点

skes-relay 工作在**广播模式**，每个 sep 都会收到所有消息。因此每个 sep 收到消息后**必须先判断这条消息是不是发给自己的**，否则可能误操作（特别是OTA升级场景，后果很严重）。

---

## 三、底层通信：nanomsg 是什么？

协议底层使用 **nanomsg** 库，它是一个高性能消息传输库（类似 ZeroMQ，但更简单）。

这里用到的通信模式是 **PUB/SUB（发布/订阅）**：

```
发布者(PUB) ──推送消息──> 订阅者(SUB)
```

类比理解：就像微信公众号，公众号发文章（PUB），关注者自动收到（SUB）。

### 端口规定

| 端口 | 方向 | 用途 |
|------|------|------|
| **19225** | sep → skes-relay | sep 向中转站发消息（用 nn_connect） |
| **19226** | skes-relay → sep | 中转站向 sep 推消息（用 nn_connect） |

> skes-relay 用 `nn_bind` 绑定这两个端口；所有 sep 用 `nn_connect` 连接。

---

## 四、消息格式：一条消息长什么样？

### 4.1 物理帧结构（二进制层面）

每条nanomsg消息由4段组成：

```
┌──────────┬──────────────┬──────────────┬──────────────────┐
│ 1 byte   │   n bytes    │   4 bytes    │    m bytes       │
│topic size│    topic     │ payload size │    payload       │
│(主题长度) │   (主题名)    │  (内容长度)   │   (实际内容)      │
└──────────┴──────────────┴──────────────┴──────────────────┘
```

**例子**：发送位置消息 `/skes/location`（主题长14字节，内容1024字节）

```
0E  /skes/location  00 04 00 00  [1024字节的payload内容]
↑                  ↑
14的十六进制        1024的小端序表示
```

> **小端序**：数字低位放前面。1024 = 0x00000400，小端存储为 `00 04 00 00`

### 4.2 Topic（主题）命名规则

- 使用 Linux 路径风格，`/` 分隔
- 只用可打印字符，不含空格

**当前所有 Topic：**

| Topic | 用途 |
|-------|------|
| `/skes/version` | 版本信息 |
| `/skes/heartbeat` | 心跳保活 |
| `/skes/dev/data` | 工况数据（JSON格式） |
| `/skes/dev/data/raw` | 工况数据（二进制格式，高性能） |
| `/skes/location` | GPS位置信息 |
| `/skes/event` | 设备事件（报警、故障、USB检测等） |
| `/skes/camera` | 摄像头数据（目标检测结果） |
| `/skes/filter` | 数据过滤/查询 |

### 4.3 Payload（内容）的通用JSON结构

所有 JSON 格式的消息都共用这个外层框架：

```json
{
  "version": "1.0",        // 协议版本号
  "id": "uuid-xxxx",       // 发送方的sep ID（未知时填""）
  "ts": 1639535683010,     // 当前时间戳（毫秒，UTC）
  "msg_id": "uuid-yyyy",   // 消息唯一ID（仅request/response时必填）
  "action": "post",        // 传输方式（见下表）
  "type": "CAN",           // 数据类型（每个topic有自己的type值）
  "data": { ... }          // 实际数据内容
}
```

**action 字段说明：**

| action值 | 含义 |
|----------|------|
| `post` | 单向推送（最常用），不需要回复 |
| `broadcast` | 广播到所有订阅者 |
| `request` | 请求，需要对方回复（必须有msg_id） |
| `response` | 对request的回复（msg_id与请求保持一致） |

---

## 五、各类数据详解

### 5.1 工况数据（CAN总线数据）

**Topic：** `/skes/dev/data`（JSON）或 `/skes/dev/data/raw`（二进制，推荐）

**方向：** sep → skes-relay

CAN数据的payload示例及字段说明：

```json
{
  "version": "1.0",
  "id": "sep-uuid",
  "ts": 1639535683010,
  "action": "post",
  "type": "CAN",
  "data": [
    {
      "bus_id": 0,           // CAN总线编号（0=CAN0, 1=CAN1...）
      "frame_type": "data",  // 帧类型: data/remote/error/overload
      "id_type": "20B",      // 帧ID类型: 20A=标准帧, 20B=扩展帧
      "id": 2596985277,      // CAN帧ID（符合CAN2.0B规范）
      "ts": 1639535683010,   // 这帧数据发生的时间（毫秒）
      "size": 8,             // 原始CAN数据字节数（非Base64字符串长度）
      "data": "AAECAwQFBgc=" // CAN原始8字节数据，用Base64编码
    }
  ]
}
```

> **为什么要 Base64？** JSON 只能传文本，CAN 的原始数据是二进制，Base64 是把二进制转成文本的标准方法。`AAECAwQFBgc=` 解码后就是 `00 01 02 03 04 05 06 07` 这8个字节。

---

### 5.2 位置信息（GPS）

**Topic：** `/skes/location`

**方向：** sep → skes-relay

```json
{
  "version": "1.0",
  "id": "sep-uuid",
  "ts": 1639535683010,
  "action": "post",
  "type": "tbox",
  "data": {
    "type": "GPS",
    "data": [
      {
        "status": 1,               // 0=定位无效, 1=定位有效
        "ts": 1639535683010,
        "longitude": 113.5527217,  // 经度（度分格式）
        "latitude": 28.7713283,    // 纬度（度分格式）
        "lon_hemisphere": "E",     // E=东经, W=西经
        "lat_hemisphere": "N",     // N=北纬, S=南纬
        "speed": 0.0,              // 地面速率
        "altitude": 0.0,           // 海拔高度（米）
        "azimuth": 0.0,            // 方位角（度）
        "magnetic": 0.0,           // 磁偏角（0.0~180.0度）
        "mag_direction": "E",      // 磁偏角方向: E或W
        "indication_mode": "A"     // A=自主定位, D=差分, E=估算, N=无效
      }
    ]
  }
}
```

---

### 5.3 事件上报（/skes/event）

事件上报通过 `type` 字段区分三大类：

```
/skes/event
    ├── type: "alarm"  → 告警事件（AEB触发、DMS报警等）
    ├── type: "event"  → 普通事件（USB插拔等）
    └── type: "fault"  → 故障事件（摄像头无信号、通信失败等）
```

#### 5.3.1 告警事件（alarm）结构

每个告警事件的公共字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| `source` | string | 来源：`aeb`/`bsd`/`dms`/`radar`/`camera`/`sound_light_alarm` |
| `ts` | int64 | 事件时间戳（毫秒） |
| `code` | int | 0=正常，非0=错误码 |
| `desc` | string | 描述信息 |
| `status` | string | `normal`/`triggered`/`disabled`/`malfunction` |
| `data` | array/object | 各source有自己的data格式 |

**AEB（自动紧急制动）data 字段：**

```json
"data": [
  {
    "alarm": { "trigger": true },      // true=触发报警
    "camera": {
      "id": 1,
      "position": "left_side"          // 摄像头位置：left_side/right_side/rear/front
    },
    "bounding_boxes": [               // 目标的边界框（矩形的4个角点）
      {
        "points": [
          {
            "direction": "top_left",
            "coords": {
              "world": { "x": 10.5, "y": 15.1 }, // 物理世界坐标（米）
              "pixel": { "x": 100, "y": 200 }     // 像素坐标
            }
          }
          // ... top_right, bottom_left, bottom_right
        ]
      }
    ],
    "detected_position": {
      "origin_coords": { ... },        // 原点坐标
      "angle": 18.9,                   // 极坐标角度（以摄像头为原点）
      "distance": 12.3,                // 距离（米）
      "coords": { ... }                // 检测到的人的位置坐标
    },
    "image": {
      "format": "jpg",
      "width": 16,
      "height": 16,
      "raw_data": "FFD8FF..."          // 图像十六进制数据
    }
  }
]
```

**DMS（驾驶员监控）data 字段：**

```json
"data": {
  "states": [
    "face_not_detected",   // 未检测到人脸
    "eyes_closed",         // 闭眼（疲劳）
    "yawning",             // 打哈欠
    "distraction",         // 注意力分散
    "using_phone",         // 使用手机
    "smoking",             // 吸烟
    "out_of_memory"        // 系统资源不足
  ]
}
```

#### 5.3.2 故障事件（fault）的 source 分类

| source值 | 故障类型 | data中关键字段 |
|----------|----------|--------------|
| `camera` | 摄像头无画面 | `position`（位置）, `desc: "no frame"` |
| `sound_light_alarm` | 声光报警器故障 | `position`, `desc` |
| `object_detection` | AI检测模块故障 | `type`（seat_departure/personnel_intrusion/fatigue_detection） |
| `data` | 数据传输失败 | `desc: "failed to transmit"` |
| `communication` | 通信模块故障 | `type`（rs485/lte/gps/bluetooth/ota/ntp） |

---

### 5.4 摄像头目标检测（/skes/camera）

**Topic：** `/skes/camera`

**方向：** 双向（sep ↔ skes-relay）

```json
{
  "ts": 1754375092976,
  "type": "object_detection",
  "data": [
    {
      "type": "bsd",              // 检测类型（bsd/aeb/dms等）
      "camera": { "id": "0" },   // 摄像头ID
      "models": {
        "algorithms": "yolo",    // 使用的算法
        "version": "5"           // 模型版本
      },
      "ts": 1754376690290,
      "results": [               // 检测结果数组
        {
          "class_id": 0,         // 目标类别ID
          "class_name": "person",// 目标类别名称
          "bounding_box": {
            "left": 0, "top": 0,
            "width": 100, "height": 100
          },
          "confidence": 0.8      // 置信度（0~1，越高越可信）
        }
      ],
      "image": {
        "format": "jpg",
        "width": 800, "height": 600,
        "raw_data": ""           // 图像十六进制数据（包含文件头）
      }
    }
  ]
}
```

---

### 5.5 数据过滤查询（/skes/filter）

**Topic：** `/skes/filter`

这是一个**双向请求-响应**机制，用于：
- sep 向 skes-relay 推送特定工况属性（如 AEB 控制指令）
- sep 向 skes-relay 查询某个属性的当前值

**三种交互模式：**

```
1. sep 推送工况 (action=post, data.type=filter_table)
   sep ─────────────────────────────────> skes-relay

2. sep 查询工况 (action=request, data.type=query)
   sep ─── 请求 ──> skes-relay ─── 回复 ──> sep

3. sep 推送AEB指令 (action=post, data.type=AEB)
   sep ──> skes-relay ──> sep（广播给所有sep）
```

**AEB 属性表：**

| property_name | 含义 |
|---------------|------|
| `AEBActivateCmd` | AEB制动控制信号 |
| `FlashCmd` | AEB触发灯光闪烁控制信号 |
| `AlarmCmd` | AEB原车报警灯控制信号 |
| `BrkCurCmd_Front` | AEB前桥制动阀目标电流 |
| `BrkCurCmd_Rear` | AEB后桥制动阀目标电流 |

---

## 六、数据类型速查表

| 类型名 | 字节数 | 说明 |
|--------|--------|------|
| `boolean` | 1 | 布尔值（false/true） |
| `int8` | 1 | 有符号整数 |
| `int16` | 2 | 有符号整数 |
| `int32` | 4 | 有符号整数 |
| `int64` | 8 | 有符号整数（时间戳用这个） |
| `uint8` | 1 | 无符号整数 |
| `uint16` | 2 | 无符号整数 |
| `uint32` | 4 | 无符号整数（CAN ID用这个） |
| `uint64` | 8 | 无符号整数 |
| `float` | 4 | 单精度浮点 |
| `double` | 8 | 双精度浮点 |
| `string` | 变长 | UTF-8字符串 |
| `binary` | 变长 | 二进制数据，JSON中用Base64编码 |

---

## 七、理解消息流转的完整例子

以**AEB触发报警**为例，完整流程如下：

```
1. AEB模块检测到危险
       ↓
2. sep（AEB程序）构建 /skes/event 消息
   {type:"alarm", data:[{source:"aeb", status:"triggered", ...}]}
       ↓
3. sep 通过 port 19225 pub 给 skes-relay
       ↓
4. skes-relay 通过 port 19226 广播给所有 sep
       ↓
5. 各个sep收到消息，先检查是否是发给自己的
   - 日志sep：记录这个报警
   - 声光sep：触发声光报警
   - 上报sep：上报到云端
```

---

## 八、如何扩展这个协议

### 8.1 新增 Topic

当有新的功能模块时，只需新增对应的 topic：

```
/skes/radar       # 雷达原始数据（示例）
/skes/ota         # OTA升级相关（示例）
/skes/config      # 配置下发（示例）
/skes/log         # 日志上报（示例）
```

**新增 Topic 的设计步骤：**
1. 确定数据方向（sep→relay、relay→sep、或双向）
2. 确定 action 类型（post/request-response）
3. 定义 type 字段的值（用来区分这个topic下的不同数据类型）
4. 设计 data 对象的字段结构
5. 更新文档，通知所有相关的 sep 开发者

### 8.2 新增事件 source

在 `/skes/event` 下新增新的告警来源，只需在 data 数组中添加新的 source 对象：

```json
// 例如新增 "lidar"（激光雷达）作为source
{
  "source": "lidar",
  "ts": 1639535683010,
  "code": 0,
  "desc": "",
  "status": "triggered",
  "data": [
    {
      "distance": 2.5,       // 障碍物距离（米）
      "angle": 45.0,         // 方向角
      "point_cloud": "..."   // 点云数据（Base64）
    }
  ]
}
```

### 8.3 新增故障 source

在 `type: "fault"` 下新增：

```json
{
  "source": "lidar",         // 新增激光雷达故障
  "ts": 1652173828,
  "code": 1,
  "desc": "激光雷达故障",
  "data": [
    {
      "code": 1,
      "desc": "no point cloud data"
    }
  ]
}
```

### 8.4 新增摄像头检测类型

在 `/skes/camera` 下，`data[].type` 目前有 `bsd`、`aeb`、`dms`，可以新增：

```json
{
  "type": "fire_detection",  // 新增火焰检测
  "camera": { "id": "2" },
  "models": {
    "algorithms": "yolov8",
    "version": "1"
  },
  "results": [
    {
      "class_id": 10,
      "class_name": "fire",
      "bounding_box": { "left": 100, "top": 50, "width": 200, "height": 150 },
      "confidence": 0.92
    }
  ]
}
```

### 8.5 扩展 /skes/filter 的 AEB 属性

在 AEB 属性表中新增属性只需在 `data.data` 数组里添加新的 property 条目：

```json
{
  "index": -1,
  "property_name": "SteeeringCmd",   // 新增转向控制
  "data_type": "double",
  "data": 15.0
}
```

---

## 九、开发集成时的注意事项

### 必须遵守的规则

1. **每条消息必须有 `id` 字段**，即使填空字符串，未知的id值会导致消息被丢弃
2. **时间戳单位是毫秒**（不是秒！），使用计算机元年（1970-01-01）以来的UTC时间
3. **收到广播消息后，必须先判断是否是发给自己的**，特别是OTA场景
4. **JSON传输时去掉不必要的空格和换行**（压缩传输，节省带宽）
5. **request消息必须有 `msg_id`**，response回复时 `msg_id` 必须与request完全一致
6. **二进制数据要用Base64编码**后放入JSON字符串字段

### 性能建议

- 工况数据（CAN）优先使用 `/skes/dev/data/raw`（二进制格式），比JSON性能高很多
- 图像数据量大，`raw_data` 字段建议按需传输，不要每帧都携带

### 代码集成

如果你的项目使用 C/C++，直接使用 **skes-linker 库**即可，它封装了 nanomsg 的所有细节。如果需要自行实现：

```c
// 伪代码：发送一条消息到 skes-relay
char topic[] = "/skes/event";
uint8_t topic_len = strlen(topic);

// 组装帧：[1字节topic长度][topic][4字节payload长度][payload]
// topic size 和 payload size 均为小端序
```

---

## 十、总结：协议结构一览图

```
SKES 通信协议
│
├── 传输层：nanomsg (PUB/SUB)
│   ├── 端口19225：sep → skes-relay
│   └── 端口19226：skes-relay → sep
│
├── 帧格式：[topic_size(1B)][topic(nB)][payload_size(4B)][payload(mB)]
│
└── Topic 体系：
    ├── /skes/version       版本
    ├── /skes/heartbeat     心跳
    ├── /skes/dev/data      工况(JSON)
    ├── /skes/dev/data/raw  工况(二进制，推荐)
    ├── /skes/location      GPS位置
    ├── /skes/event         事件上报
    │   ├── type=alarm      告警（AEB/BSD/DMS/radar/camera）
    │   ├── type=event      普通事件（USB等）
    │   └── type=fault      故障（摄像头/通信/检测模块）
    ├── /skes/camera        摄像头目标检测
    └── /skes/filter        数据过滤/属性查询
        ├── action=post     推送属性
        ├── action=request  查询属性
        └── action=response 回复查询
```

---

*文档基于 SKES通信规范 v0.8 整理，版权归树根科技所有。*
