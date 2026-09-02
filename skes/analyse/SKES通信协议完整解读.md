# SKES 通信协议完整解读手册
> 基于《SKES通信规范 v0.8》| 树根科技 | 面向开发者的深度解析

---

## 目录

1. [系统全景：SKES是什么](#一系统全景skes是什么)
2. [架构设计：三角色模型](#二架构设计三角色模型)
3. [通信层：nanomsg底层原理](#三通信层nanomsg底层原理)
4. [帧结构：消息的物理格式](#四帧结构消息的物理格式)
5. [Topic体系：消息的分类系统](#五topic体系消息的分类系统)
6. [Payload结构：消息的内容规范](#六payload结构消息的内容规范)
7. [各Topic深度解析](#七各topic深度解析)
8. [事件系统详解](#八事件系统详解)
9. [完整交互流程图](#九完整交互流程图)
10. [数据类型与编码规则](#十数据类型与编码规则)
11. [开发注意事项与陷阱](#十一开发注意事项与陷阱)
12. [扩展设计指南](#十二扩展设计指南)
13. [协议总览速查图](#十三协议总览速查图)

---

## 一、系统全景：SKES是什么

### 1.1 定义

**SKES**（SKy-Eyes System，天眼系统）是一套部署在车辆上的智能感知与数据处理系统。

它不是单一程序，而是由**多个功能程序协同工作**的分布式车载系统：

```
┌─────────────────────────────────────────────────────────────────────┐
│                         SKES 天眼系统                                │
│                                                                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐             │
│  │  📷 摄像头   │  │ 🤖 AI算法    │  │ 📡 CAN采集   │             │
│  │  多路视频流  │  │ AEB/BSD/DMS  │  │  工况数据    │             │
│  └──────────────┘  └──────────────┘  └──────────────┘             │
│                                                                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐             │
│  │  📍 GPS定位  │  │  📶 4G/LTE   │  │  🔊 声光报警  │             │
│  │  位置追踪   │  │  云端上报    │  │  输出控制    │             │
│  └──────────────┘  └──────────────┘  └──────────────┘             │
│                                                                     │
│              以上所有模块通过本协议互相通信                           │
└─────────────────────────────────────────────────────────────────────┘
```

### 1.2 这份协议解决什么问题？

想象有6个功能程序同时在运行，它们需要互相传递信息：

- GPS程序需要把位置告诉上报程序
- AEB程序检测到危险，需要触发声光报警程序
- CAN采集程序的数据需要发给AI算法程序

**如果没有统一协议**，每两个程序之间都要单独建立通信，6个程序就需要 C(6,2)=15 个通信链路，维护噩梦。

**有了SKES协议**，所有程序通过一个中心节点通信，只需要 6 条链路，而且格式统一。

```
没有协议（网状结构）：          有了协议（星型结构）：

A ─────── B                    A
│ ╲     ╱ │                   │
│  ╲   ╱  │              B ───┼─── skes-relay ─── D
│   ╲ ╱   │                   │
C    ╳    D                   C         E
│   ╱ ╲   │
│  ╱   ╲  │
E ─────── F                 (清晰，可维护)
(混乱，难维护)
```

---

## 二、架构设计：三角色模型

### 2.1 三个核心角色

| 角色 | 英文全称 | 中文职责 | 类比 |
|------|----------|----------|------|
| **skes-relay** | SKES Message Relay | 消息中转站，负责接收并广播所有消息 | 邮局/广播电台 |
| **sep** | Skes-EndPoint | 功能端点程序，发送或接收消息 | 寄件人/收件人 |
| **skes-linker** | SKES Linker Library | 通信封装库，开发时调用 | 快递单标准模板 |

### 2.2 skes-relay 的工作模式

```
                    ┌─────────────────────────┐
                    │       skes-relay         │
                    │                          │
sep-A ──pub──> port:19225 ──[接收]             │
sep-B ──pub──>            ──[接收]             │
sep-C ──pub──>            ──[接收]             │
                    │         ↓                │
                    │    [广播转发]             │
                    │         ↓                │
sep-A <──sub── port:19226 ──[发出]             │
sep-B <──sub──            ──[发出]             │
sep-C <──sub──            ──[发出]             │
                    └─────────────────────────┘
```

> **关键理解**：skes-relay 目前是**纯广播**模式。sep-A 发出的消息，sep-B 和 sep-C 也都能收到。这意味着每个 sep 必须自己判断"这条消息是不是我该处理的"。

### 2.3 sep 的两种行为

```
sep 的行为模式：

发布模式 (PUB)：sep 主动向 skes-relay 推送数据
┌─────┐   构建消息    ┌──────────┐   port:19225   ┌───────────────┐
│ sep │ ──────────> │nanomsg   │ ─────────────> │  skes-relay   │
│程序  │             │pub socket│                │               │
└─────┘             └──────────┘                └───────────────┘

订阅模式 (SUB)：sep 监听来自 skes-relay 的消息
┌───────────────┐   port:19226   ┌──────────┐   收到消息    ┌─────┐
│  skes-relay   │ ─────────────> │nanomsg   │ ──────────> │ sep │
│               │                │sub socket│              │程序  │
└───────────────┘                └──────────┘              └─────┘
```

### 2.4 端口绑定规则（重要）

```
端口所有权：

skes-relay（使用 nn_bind）：
  ├── 绑定 port:19225  ← 专门"接收" sep 发来的消息
  └── 绑定 port:19226  ← 专门"发出"消息给所有 sep

所有 sep（使用 nn_connect）：
  ├── 连接到 port:19225  ← 用于"发送"消息给 skes-relay
  └── 连接到 port:19226  ← 用于"接收"来自 skes-relay 的消息

规则：nn_bind 只能用于 skes-relay！
      其他所有程序一律使用 nn_connect！
```

---

## 三、通信层：nanomsg底层原理

### 3.1 nanomsg vs 其他方案

| 特性 | nanomsg | ZeroMQ | MQTT | HTTP |
|------|---------|--------|------|------|
| 实现语言 | C | C++ | 各语言 | 各语言 |
| 复杂度 | 低 | 中 | 中 | 低 |
| 性能 | 高 | 高 | 中 | 低 |
| 适合场景 | 嵌入式/本地进程 | 分布式 | 物联网 | Web |
| 是否需要Broker | 否（但此处relay充当） | 否 | 是 | 否 |

### 3.2 PUB/SUB 通信模型详解

```
PUB/SUB 工作原理：

Step 1: skes-relay 绑定两个 socket
┌─────────────┐
│ skes-relay  │
│  xsub:19225 │  ← 接收来自所有sep的pub消息（xsub可以聚合多个pub）
│  xpub:19226 │  ← 将消息分发给所有sub（xpub可以广播给多个sub）
└─────────────┘

Step 2: 每个 sep 连接两个 socket
┌──────┐
│sep-A │
│pub──>│────连接 19225  ← 发送时用这个
│sub<──│────连接 19226  ← 接收时用这个
└──────┘

Step 3: 消息流转
sep-A.pub ──发布消息──> xsub:19225 ──内部转发──> xpub:19226
                                                      │
                                          ┌───────────┼──────────┐
                                          ↓           ↓          ↓
                                       sep-A.sub  sep-B.sub  sep-C.sub
                                    (所有人都收到！)
```

### 3.3 主题过滤（Topic Filtering）

虽然所有消息都被广播，但 nanomsg 支持按主题前缀过滤，sep 可以只订阅自己关心的主题：

```
sep 订阅过滤示例：

订阅所有消息：   sub.subscribe("")         → 收到一切
订阅位置消息：   sub.subscribe("/skes/location") → 只收位置
订阅所有事件：   sub.subscribe("/skes/event")    → 只收事件

过滤原理：nanomsg 比较消息前缀，前缀匹配才投递给订阅者
```

---

## 四、帧结构：消息的物理格式

### 4.1 完整帧布局

```
nanomsg 消息帧（字节级视图）：

偏移量:  0          1        1+n        5+n       5+n+m
         │          │          │          │          │
         ▼          ▼          ▼          ▼          ▼
┌────────┬──────────────────┬────────────┬──────────────────────┐
│1 byte  │     n bytes      │  4 bytes   │       m bytes        │
│topic   │     topic        │  payload   │       payload        │
│ size   │   (字符串内容)    │   size     │       (消息内容)      │
└────────┴──────────────────┴────────────┴──────────────────────┘
   ↑                              ↑
小端序整数                      小端序整数
表示topic的字节长度             表示payload的字节长度
```

### 4.2 具体例子：发送 /skes/location 消息

```
场景：topic="/skes/location"(14字节), payload=某GPS数据(假设1024字节)

字节内容：
┌──┬─────────────────────┬────────────┬─────────────────────┐
│0E│/skes/location       │00 04 00 00 │{JSON内容...}         │
└──┴─────────────────────┴────────────┴─────────────────────┘
 ↑          ↑                  ↑               ↑
14的        "/skes/location"  1024的          实际GPS
十六进制    14个ASCII字节     小端序           JSON数据

小端序解析：
  1024 = 0x00000400
  小端存储 = 00 04 00 00  (低字节在前)
```

### 4.3 小端序（Little-Endian）详解

```
为什么用小端序？
  ARMv8/x86 CPU 原生就是小端，直接 memcpy 性能最好。

数字 → 小端序字节的转换：

  十进制   十六进制    小端(4字节)    大端(4字节)
    1     0x00000001  01 00 00 00    00 00 00 01
  256     0x00000100  00 01 00 00    00 00 01 00
 1024     0x00000400  00 04 00 00    00 00 04 00
65536     0x00010000  00 00 01 00    00 01 00 00

C语言写法（topic_size）：
  uint8_t topic_len = (uint8_t)strlen(topic);
  // 直接1字节，无端序问题

C语言写法（payload_size）：
  uint32_t payload_size = (uint32_t)payload_len;
  // x86/ARM 本身就是小端，直接 memcpy 即可
  memcpy(buf + 1 + topic_len, &payload_size, 4);
```

### 4.4 skes-linker 库帮你做了什么

```
手动实现（你自己写）：

  buf[0] = topic_len;                         // 1字节topic长度
  memcpy(buf+1, topic, topic_len);            // topic内容
  memcpy(buf+1+topic_len, &psize, 4);         // 4字节payload长度(小端)
  memcpy(buf+5+topic_len, payload, psize);    // payload内容
  nn_send(sock, buf, total_len, 0);           // 发送

使用 skes-linker（库帮你做）：

  skes_linker_publish(linker, "/skes/location", payload, psize);
  // ↑ 一行搞定，内部自动处理帧格式
```

---

## 五、Topic体系：消息的分类系统

### 5.1 Topic 设计原则

```
Topic 命名规则：
  ✅ /skes/location          使用 / 分隔
  ✅ /skes/dev/data/raw      支持多级路径
  ❌ /skes/dev data          不能含空格
  ❌ /skes\location          不能用反斜杠
  ❌ SKES/LOCATION           大小写敏感，请遵循现有风格
```

### 5.2 全量 Topic 总览

```
/skes
  │
  ├── /version              版本协商
  │     方向: 双向
  │     格式: 参见版本规范文档
  │
  ├── /heartbeat            心跳保活
  │     方向: 双向
  │     频率: 周期性发送（规范见心跳规范文档）
  │
  ├── /dev
  │     ├── /data           工况数据 (JSON格式)
  │     │     方向: sep → skes-relay
  │     │     type: "CAN"
  │     │
  │     └── /data/raw       工况数据 (二进制格式)
  │           方向: sep → skes-relay
  │           格式: 待定（二进制，性能最佳）
  │
  ├── /location             GPS位置信息
  │     方向: sep → skes-relay
  │     type: "tbox"
  │
  ├── /event                设备事件上报
  │     方向: sep → skes-relay
  │     type: "alarm" | "event" | "fault"
  │
  ├── /camera               摄像头数据
  │     方向: 双向(sep↔skes-relay)
  │     type: "object_detection"
  │
  └── /filter               数据过滤/属性查询
        方向: 双向(sep↔skes-relay)
        type: "properties" | "file"
```

### 5.3 各 Topic 数据方向汇总

```
sep ──────────────────────────> skes-relay（单向上行）：
    /skes/dev/data
    /skes/dev/data/raw
    /skes/location
    /skes/event

skes-relay ────────────────────> sep（单向下行）：
    （OTA包、DBC文件等，协议待完善）

sep <══════════════════════════> skes-relay（双向）：
    /skes/version
    /skes/heartbeat
    /skes/camera
    /skes/filter
```

---

## 六、Payload结构：消息的内容规范

### 6.1 Payload 的两种格式

```
Payload 格式选择：

JSON格式（大多数topic使用）
  ├── 优点: 可读性好，易调试，结构灵活
  ├── 缺点: 体积大，解析慢
  └── 适用: /skes/event, /skes/location, /skes/camera 等

二进制格式（仅 /skes/dev/data/raw 使用）
  ├── 优点: 体积极小，解析极快，CPU占用低
  ├── 缺点: 不可直接阅读，调试困难
  └── 适用: 高频工况数据（CAN帧率可达1000Hz）
```

### 6.2 JSON Payload 外层通用结构

```json
{
    "version": "1.0",
    "id":      "550e8400-e29b-41d4-a716-446655440000",
    "ts":      1639535683010,
    "msg_id":  "2841f290-e170-11ec-99d2-4b4f83e67c3f",
    "action":  "post",
    "type":    "CAN",
    "data":    { }
}
```

每个字段的详细说明：

```
字段详解：

"version"  string  必填
  └── 当前 payload 版本号，用于未来兼容性判断
  └── 示例: "1.0", "2.0"

"id"       string  必填（可为""）
  └── 发送方 sep 的唯一标识，建议用 UUID
  └── 空字符串""是允许的，但非空时用于追踪消息来源
  └── ⚠️ 未知/非法的id值可能导致整条消息被丢弃

"ts"       int64   必填
  └── 消息发送时的系统时间
  └── 单位: 毫秒（ms）
  └── 基准: 计算机元年（1970-01-01 00:00:00 UTC）
  └── ⚠️ 常见错误: 误用秒级时间戳（差1000倍）

"msg_id"   string  条件必填
  └── 仅当 action = "request" 或 "response" 时必须存在
  └── request: 填写一个新的唯一UUID
  └── response: 原封不动复制request中的msg_id
  └── 作用: 将请求与回复关联起来

"action"   string  必填
  ├── "post"      → 单向推送，不期待回复（最常用）
  ├── "broadcast" → 广播，不期待回复
  ├── "request"   → 发起请求，期待对方"response"回复
  └── "response"  → 对某个"request"的回复

"type"     string  必填
  └── 声明 data 字段内容的具体类型
  └── 不同 topic 下 type 有不同的合法值（见各章节）

"data"     object  必填
  └── 实际数据内容，结构由 type 决定
```

### 6.3 action 的交互时序图

```
post（单向推送）:

  sep-A                    skes-relay              sep-B
    │                          │                      │
    │──── action:post ────────>│                      │
    │     /skes/event          │──── 广播 ───────────>│
    │                          │     (sep-B自行判断    │
    │                          │      是否处理)        │
    │                          │                      │


request/response（请求-回复）:

  sep-A                    skes-relay              sep-B
    │                          │                      │
    │── action:request ───────>│                      │
    │   msg_id:"uuid-001"      │── 广播 ─────────────>│
    │   /skes/filter           │                      │
    │                          │                      │──┐ 处理
    │                          │                      │  │ 请求
    │                          │<── action:response ──│<─┘
    │<── 广播 ─────────────────│    msg_id:"uuid-001" │
    │    msg_id:"uuid-001"     │    (相同的msg_id)     │
    │    /skes/filter          │                      │
    │──┐ 匹配到对应的           │                      │
    │  │ 请求，处理回复         │                      │
    │<─┘                       │                      │
```

---

## 七、各Topic深度解析

### 7.1 工况数据：/skes/dev/data

#### 业务场景
车辆行驶时，CAN总线上每毫秒都有大量数据，这些原始CAN帧需要采集后上报。

#### 完整数据结构

```json
{
  "version": "1.0",
  "id": "sep-can-collector-uuid",
  "ts": 1639535683010,
  "action": "post",
  "type": "CAN",
  "data": [
    {
      "bus_id":     0,
      "frame_type": "data",
      "id_type":    "20B",
      "id":         2596985277,
      "ts":         1639535683010,
      "size":       8,
      "data":       "AAECAwQFBgc="
    },
    {
      "bus_id":     1,
      "frame_type": "data",
      "id_type":    "20A",
      "id":         0x123,
      "ts":         1639535683011,
      "size":       4,
      "data":       "AAQICBA="
    }
  ]
}
```

#### 字段详解

```
data数组中每个CAN帧对象：

"bus_id"      uint8   CAN总线编号
  ├── 0 = CAN0（第一路CAN总线）
  ├── 1 = CAN1（第二路CAN总线）
  └── 以此类推

"frame_type"  string  CAN帧类型（对应CAN协议规范）
  ├── "data"     → 数据帧（最常用，携带实际数据）
  ├── "remote"   → 远程帧（请求节点发送数据）
  ├── "error"    → 错误帧（总线错误检测）
  └── "overload" → 过载帧（节点处理能力不足）

"id_type"     string  CAN帧ID格式
  ├── "20A" → CAN2.0A 标准帧（11位ID，最大0x7FF）
  └── "20B" → CAN2.0B 扩展帧（29位ID，最大0x1FFFFFFF）

"id"          uint32  CAN帧的ID值
  └── 符合CAN2.0B规范

"ts"          int64   这帧数据实际采集到的时间（毫秒）
  └── ⚠️ 注意：这个ts是帧发生的时间，外层ts是消息发送时间

"size"        int32   原始CAN数据的字节数
  └── ⚠️ 不是Base64字符串的长度！是原始数据长度（1~8字节）

"data"        string  CAN报文数据，Base64编码
  └── 解码例子：
      "AAECAwQFBgc=" → Base64解码 → 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07
```

#### Base64 编码说明

```
为什么CAN数据要用Base64？

原因：JSON是文本格式，无法直接存放二进制字节（如0x00会被误认为字符串结束）
解决：将二进制数据用Base64编码成可打印ASCII字符

编码规则：每3个字节 → 4个ASCII字符
原始8字节 → Base64后约11个字符（8 × 4/3 ≈ 10.67，补齐到12）

实际例子：
  原始: 00 01 02 03 04 05 06 07   (8字节)
  B64:  "AAECAwQFBgc="            (12字符，尾部=是填充符)
```

---

### 7.2 位置信息：/skes/location

#### 业务场景
GPS模块持续输出GPRMC格式定位数据，需要封装后上报。

#### 完整数据结构

```json
{
  "version": "1.0",
  "id": "sep-gps-uuid",
  "ts": 1639535683010,
  "action": "post",
  "type": "tbox",
  "data": {
    "type": "GPS",
    "data": [
      {
        "status":           1,
        "ts":               1639535683010,
        "longitude":        113.5527217,
        "latitude":         28.7713283,
        "lon_hemisphere":   "E",
        "lat_hemisphere":   "N",
        "speed":            0.0,
        "altitude":         0.0,
        "azimuth":          0.0,
        "magnetic":         0.0,
        "mag_direction":    "E",
        "indication_mode":  "A"
      }
    ]
  }
}
```

#### 坐标系说明

```
经纬度格式：dd.mmmmmm（度分格式）

示例：longitude = 113.5527217
  解读：113度 + 0.5527217分
  注意：这不是"度"和"分秒"的组合，是直接的度数小数形式

经纬度合法范围：
  longitude（经度）: 0.0 ~ 180.0
  latitude（纬度）:  0.0 ~ 90.0

半球字段决定正负：
  lon_hemisphere = "E"（东经）或 "W"（西经）
  lat_hemisphere = "N"（北纬）或 "S"（南纬）

indication_mode（定位模式）：
  "A" → Autonomous，自主GPS定位（最常见）
  "D" → Differential，差分定位（精度更高）
  "E" → Estimated，航迹推算（无GPS信号时估算）
  "N" → Not valid，数据无效

status（定位状态）：
  0 → 定位无效（status=0时，坐标数据不可信）
  1 → 定位有效

字段不存在时的默认填充：
  float/double类型 → 填 0.0
  int类型          → 填 0
  string类型        → 填 ""
```

---

### 7.3 摄像头目标检测：/skes/camera

#### 业务场景
AI算法对摄像头画面进行分析，输出检测到的目标信息。

#### 完整数据结构

```json
{
  "ts":   1754375092976,
  "type": "object_detection",
  "data": [
    {
      "type": "bsd",
      "camera": {
        "id":      "0",
        "channel": "0"
      },
      "models": {
        "algorithms": "yolo",
        "version":    "5"
      },
      "ts":      1754376690290,
      "results": [
        {
          "class_id":    0,
          "class_name":  "person",
          "bounding_box": {
            "left":   0,
            "top":    0,
            "width":  100,
            "height": 100
          },
          "confidence": 0.8
        }
      ],
      "image": {
        "format":   "jpg",
        "width":    800,
        "height":   600,
        "raw_data": "FFD8FFE0..."
      }
    }
  ]
}
```

#### 边界框（Bounding Box）坐标系

```
图像坐标系（像素）：

(0,0) ────────────────────> X轴（left方向）
  │
  │        ┌──────────┐   ← top
  │        │          │
  │        │  目标区域 │   height
  │        │          │
  │        └──────────┘   ← top + height
  │        ↑          ↑
  │       left   left+width
  ↓
Y轴（top方向）

bounding_box字段含义：
  left   → 框的左边缘距图像左边界的像素数
  top    → 框的上边缘距图像上边界的像素数
  width  → 框的宽度（像素）
  height → 框的高度（像素）
```

#### confidence（置信度）说明

```
confidence 值域：0.0 ~ 1.0

理解：AI对这个检测结果"有多确信"

  0.0 ~ 0.5  → 低置信度，可能是误检，建议过滤
  0.5 ~ 0.7  → 中等置信度
  0.7 ~ 0.9  → 高置信度（示例中0.8属于此范围）
  0.9 ~ 1.0  → 极高置信度

实际工程中通常设一个阈值（如0.6），低于阈值的结果丢弃
```

#### image.raw_data 说明

```
raw_data 存储的是图像的十六进制字符串数据：

  "raw_data": "FFD8FFE000104A46494600..."
                ↑↑↑↑↑↑
                JPEG文件头标志（FF D8 FF 是所有JPEG文件的开头）

处理方法（C语言伪代码）：
  1. 将十六进制字符串转换为字节数组
     "FFD8" → { 0xFF, 0xD8 }
  2. 将字节数组写入文件，即可得到标准 JPEG 图片
  3. 或者直接传给图像解码库（如 libjpeg）
```

---

### 7.4 数据过滤：/skes/filter

#### 业务场景
这个Topic提供了一个属性查询和推送机制，有三种使用方式：

```
/skes/filter 的三种使用场景：

场景1：sep 主动推送工况数据给 skes-relay
        (action=post, data.type=filter_table)

  sep-CAN ──推送制动压力告警状态──> skes-relay
  sep-CAN ──推送AEB控制指令──────> skes-relay

场景2：sep 查询 skes-relay 上的属性值
        (action=request, data.type=query)

  sep-UI ──请求"当前制动压力告警是否触发"──> skes-relay
  sep-UI <──回复"当前值为false"──────────── skes-relay

场景3：通过 skes-relay 向其他 sep 推送 AEB 指令
        (action=post, data.type=AEB)

  sep-AEB ──发布AEB激活指令──> skes-relay ──广播──> sep-制动控制
```

#### 三种 data.type 的结构对比

**filter_table（推送过滤工况）：**
```json
{
  "action": "post",
  "type":   "properties",
  "data": {
    "type": "filter_table",
    "data": [
      {
        "index":         -1,
        "property_name": "ID0Xeff7e17_IC_BrakePressureAlarm",
        "data_type":     "boolean",
        "data":          false
      }
    ]
  }
}
```

**query（查询属性）+ 回复：**
```json
// 请求（sep → skes-relay）
{
  "action":  "request",
  "msg_id":  "uuid-001",
  "type":    "properties",
  "data": {
    "type": "query",
    "data": [
      {
        "index":         -1,
        "property_name": "ID0Xeff7e17_IC_BrakePressureAlarm",
        "data_type":     "boolean"
      }
    ]
  }
}

// 回复（skes-relay → sep，msg_id必须与请求一致）
{
  "action":  "response",
  "msg_id":  "uuid-001",
  "type":    "properties",
  "data": {
    "type": "query",
    "data": [
      {
        "ts":            1639535683010,
        "index":         -1,
        "property_name": "ID0Xeff7e17_IC_BrakePressureAlarm",
        "data_type":     "boolean",
        "value":         false,
        "result":        0
      }
    ]
  }
}
```

**AEB（推送AEB控制属性）：**
```json
{
  "action": "post",
  "type":   "properties",
  "data": {
    "type": "AEB",
    "data": [
      { "index": -1, "property_name": "AEBActivateCmd",  "data_type": "double", "data": 100.1 },
      { "index": -1, "property_name": "FlashCmd",        "data_type": "double", "data": 200.1 },
      { "index": -1, "property_name": "AlarmCmd",        "data_type": "double", "data": 300.1 },
      { "index": -1, "property_name": "BrkCurCmd_Front", "data_type": "double", "data": 50.1  },
      { "index": -1, "property_name": "BrkCurCmd_Rear",  "data_type": "double", "data": 10.1  }
    ]
  }
}
```

---

## 八、事件系统详解

### 8.1 事件分类总览

```
/skes/event
    │
    ├── type = "alarm"   告警事件（功能性报警）
    │     ├── source = "aeb"               自动紧急制动
    │     ├── source = "bsd"               盲区检测
    │     ├── source = "dms"               驾驶员行为监控
    │     ├── source = "radar"             雷达检测
    │     ├── source = "camera"            摄像头状态
    │     ├── source = "sound_light_alarm" 声光报警器
    │     └── source = "location"          位置相关告警
    │
    ├── type = "event"   普通事件（设备状态变化）
    │     └── source = "usb"               U盘插拔
    │
    └── type = "fault"   故障事件（系统异常）
          ├── source = "camera"            摄像头无画面
          ├── source = "sound_light_alarm" 声光报警器故障
          ├── source = "object_detection"  AI检测模块故障
          ├── source = "data"              数据传输故障
          └── source = "communication"     通信模块故障
```

### 8.2 事件公共字段与 status 枚举

```
每个事件对象的公共字段：

┌─────────────────────────────────────────────────────┐
│  字段       类型      含义                           │
├─────────────────────────────────────────────────────│
│  source     string   事件来源（aeb/bsd/dms等）       │
│  ts         int64    事件发生时间（毫秒，UTC）        │
│  code       int      0=正常/无错误，非0=具体错误码    │
│  desc       string   描述信息（可为空字符串）         │
│  status     string   当前状态（见下方枚举）           │
│  data       any      各source特有的数据结构           │
└─────────────────────────────────────────────────────┘

status 枚举值及适用场景：

  "normal"      → 系统正常运行（适用于所有source）
  "triggered"   → 报警已触发（AEB、BSD、DMS、USB等）
  "disabled"    → 功能被禁用（AEB被关闭等）
  "malfunction" → 设备故障（摄像头、声光灯、雷达等）
  "on"          → 开启状态
  "off"         → 关闭状态
```

### 8.3 AEB 事件完整字段解析

```
AEB 报警事件的完整 data 数组结构：

data[i]（每个检测目标）：
  │
  ├── alarm
  │     └── trigger: bool  是否触发制动报警
  │
  ├── camera
  │     ├── id: int         摄像头编号（1,2,3...）
  │     └── position: str  摄像头安装位置
  │                          "left_side"  左侧
  │                          "right_side" 右侧
  │                          "rear"       后方
  │                          "front"      前方
  │
  ├── bounding_boxes         检测目标的边界框（矩形）
  │     └── [每个框有4个点]
  │           每个点：
  │           ├── direction: str  点的方位
  │           │                    "top_left"     左上角
  │           │                    "top_right"    右上角
  │           │                    "bottom_left"  左下角
  │           │                    "bottom_right" 右下角
  │           └── coords
  │                 ├── world  物理世界坐标（单位：米）
  │                 │     ├── x: float  水平距离
  │                 │     └── y: float  纵向距离
  │                 └── pixel  图像像素坐标
  │                       ├── x: int
  │                       └── y: int
  │
  ├── detected_position      检测到的人员/障碍物位置
  │     ├── origin_coords   参考原点坐标（摄像头位置）
  │     ├── angle: float    极坐标角度（度）
  │     ├── distance: float 极坐标距离（米）
  │     └── coords          检测物体坐标（world+pixel）
  │
  └── image                 检测时刻的图像帧
        ├── format: str     图像格式（"jpg"）
        ├── width: int      图像宽度（像素）
        ├── height: int     图像高度（像素）
        └── raw_data: str   图像十六进制数据
```

#### AEB 坐标系可视化

```
摄像头视角（鸟瞰图）：

                    摄像头（参考原点）
                         *
                        /|
               angle  /  |
                    /    |
                  /      | 
      detected  /        |
      object  *──────────┘ distance
              ↑
              (detected_position.coords)

world 坐标系（实际物理空间）：
  x 轴 → 水平方向（向右为正）
  y 轴 → 纵向方向（向前为正）
  单位 → 米

pixel 坐标系（图像像素）：
  x 轴 → 图像水平方向（向右递增）
  y 轴 → 图像垂直方向（向下递增）
  (0,0) 在图像左上角
```

### 8.4 DMS 状态枚举详解

```
DMS（Driver Monitoring System，驾驶员监控系统）

states 数组包含当前检测到的所有异常状态：

  "face_not_detected"  → 未检测到人脸
                          原因: 驾驶员不在位，或遮挡
  
  "eyes_closed"        → 眼睛闭合
                          判断: 连续闭眼超过阈值时间则触发
  
  "yawning"            → 打哈欠
                          判断: 嘴部开合幅度超过阈值
  
  "distraction"        → 注意力分散
                          判断: 头部朝向偏离前方超过阈值
  
  "using_phone"        → 使用手机
                          判断: 手持矩形物体+头部低垂
  
  "smoking"            → 吸烟
                          判断: 手部到嘴部的重复动作
  
  "out_of_memory"      → 系统内存不足
                          注意: 这是系统状态，非驾驶员行为

⚠️ states 是数组，可以同时存在多个状态：
  例如：["eyes_closed", "yawning"] 表示同时检测到闭眼和打哈欠
```

### 8.5 故障事件（fault）source 详解

```
camera 故障：
  摄像头位置枚举：
    "left_side"  → 左侧摄像头（BSD）
    "right_side" → 右侧摄像头（BSD）
    "rear"       → 后方摄像头
    "front"      → 前方摄像头（AEB）
    "dms"        → 驾驶员监控摄像头

  故障类型（desc字段）：
    "no frame" → 没有视频画面

communication 故障子类型（type字段）：
    "rs485"     → RS485串口通信失败
    "lte"       → 4G/LTE移动网络拨号失败
    "gps"       → GPS定位失败
    "bluetooth" → 蓝牙连接失败
    "ota"       → OTA文件传输失败
    "ntp"       → NTP时间同步失败

object_detection 故障子类型（type字段）：
    "seat_departure"       → 离座检测故障
    "personnel_intrusion"  → 人员入侵检测故障
    "fatigue_detection"    → 疲劳检测故障
```

---

## 九、完整交互流程图

### 9.1 系统启动时序

```
系统启动顺序：

t=0  skes-relay 启动
       ├── nn_bind(19225)   ← 开始监听 sep 的消息
       └── nn_bind(19226)   ← 准备向 sep 广播

t=1  各 sep 启动
       ├── nn_connect(19225)  ← 连接发送端口
       ├── nn_connect(19226)  ← 连接接收端口
       └── 发送 /skes/version 进行版本协商

t=2  版本协商完成后
       └── 各 sep 开始定期发送 /skes/heartbeat

t=3  业务数据开始传输
       ├── CAN采集sep  周期性发送 /skes/dev/data
       ├── GPS sep     周期性发送 /skes/location
       └── AI sep      按需发送 /skes/event 和 /skes/camera
```

### 9.2 AEB 触发完整流程

```
场景：前方摄像头检测到障碍物，触发AEB制动

  摄像头(sep)      AI算法(sep)      skes-relay      制动控制(sep)    声光报警(sep)
      │                │                │                │                │
      │  共享内存写入   │                │                │                │
      │──图像帧+标定──>│                │                │                │
      │                │                │                │                │
      │                │──计算检测──>   │                │                │
      │                │                │                │                │
      │                │  /skes/camera  │                │                │
      │                │──目标检测结果──>│                │                │
      │                │                │──广播─────────>│                │
      │                │                │──广播──────────────────────────>│
      │                │                │                │                │
      │                │  /skes/event   │                │                │
      │                │──alarm/aeb─────>               │                │
      │                │  status:triggered               │                │
      │                │                │──广播─────────>│                │
      │                │                │──广播──────────────────────────>│
      │                │                │                │                │
      │                │  /skes/filter  │                │                │
      │                │──AEBActivateCmd>                │                │
      │                │  BrkCurCmd:50A │──广播─────────>│                │
      │                │                │                │                │
      │                │                │                │接收AEB指令      │
      │                │                │                │执行制动────>    │
      │                │                │                │                │
      │                │                │                │                │接收alarm
      │                │                │                │                │触发声光
```

### 9.3 sep 接收消息的处理逻辑

```
每个 sep 收到消息后的处理流程：

┌─── 接收到 nanomsg 消息 ───┐
│                           │
│  1. 解析帧头              │
│     提取 topic            │
│     提取 payload          │
│                           │
│  2. 判断 topic            │
│     这是 /skes/filter?    │─── 否 ──> 根据自己关心的topic处理
│     这是 /skes/heartbeat? │
│     ...                   │
│            │              │
│           是              │
│            ↓              │
│  3. 解析 payload          │
│     提取 id 字段          │
│     id 是发给我的吗?       │─── 否 ──> ⚠️ 丢弃，不处理！
│            │              │
│           是              │
│            ↓              │
│  4. 检查 action           │
│     post?    → 执行对应业务│
│     request? → 处理并response
│     response?→ 匹配msg_id │
│                 取出结果  │
└───────────────────────────┘
```

---

## 十、数据类型与编码规则

### 10.1 完整数据类型表

| 类型名 | 字节数 | 有符号 | 值域 | JSON中的表示 |
|--------|--------|--------|------|-------------|
| `boolean` | 1 | - | false/true | `false` / `true` |
| `int8` | 1 | ✅ | -128 ~ 127 | 数字 |
| `int16` | 2 | ✅ | -32768 ~ 32767 | 数字 |
| `int32` | 4 | ✅ | -2147483648 ~ 2147483647 | 数字 |
| `int64` | 8 | ✅ | -9.2×10¹⁸ ~ 9.2×10¹⁸ | 数字（时间戳） |
| `uint8` | 1 | ❌ | 0 ~ 255 | 数字 |
| `uint16` | 2 | ❌ | 0 ~ 65535 | 数字 |
| `uint32` | 4 | ❌ | 0 ~ 4294967295 | 数字 |
| `uint64` | 8 | ❌ | 0 ~ 1.8×10¹⁹ | 数字 |
| `float` | 4 | - | ±1.2×10⁻³⁸ ~ ±3.4×10³⁸ | 浮点数 |
| `double` | 8 | - | ±2.2×10⁻³⁰⁸ ~ ±1.8×10³⁰⁸ | 浮点数 |
| `string` | 变长 | - | UTF-8字符串 | `"..."` |
| `object` | 变长 | - | JSON对象 | `{...}` |
| `array` | 变长 | - | JSON数组 | `[...]` |
| `binary` | 变长 | - | 任意二进制 | Base64字符串 `"..."` |

### 10.2 时间戳规范

```
协议中所有时间戳统一标准：

  基准：Unix时间（1970-01-01 00:00:00 UTC）
  单位：毫秒（ms）
  类型：int64

实际值示例：
  1639535683010 ms
  = 1639535683.010 秒
  = 2021-12-15 02:54:43.010 UTC
  = 2021-12-15 10:54:43.010 北京时间（UTC+8）

C语言获取方法：
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  int64_t ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

⚠️ 常见错误：
  错误 → 使用 time(NULL) 得到的是秒级时间戳
  正确 → 使用毫秒级时间戳，需要 × 1000 或用 clock_gettime
```

### 10.3 UUID 规范

```
协议中 id 和 msg_id 字段建议使用 UUID v4：

格式：xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
示例：550e8400-e29b-41d4-a716-446655440000
      2841f290-e170-11ec-99d2-4b4f83e67c3f

C语言生成方法（使用libuuid）：
  #include <uuid/uuid.h>
  uuid_t uuid;
  char uuid_str[37];
  uuid_generate_random(uuid);
  uuid_unparse(uuid, uuid_str);
  // uuid_str = "550e8400-e29b-41d4-a716-446655440000"

Python生成方法：
  import uuid
  str(uuid.uuid4())
```

### 10.4 JSON 压缩传输

```
协议要求：JSON传输时去掉不必要的空格和换行

原始（可读）：
  {
      "version": "1.0",
      "id": "uuid",
      "ts": 1639535683010
  }

压缩（传输）：
  {"version":"1.0","id":"uuid","ts":1639535683010}

节省效果：
  原始: 约70字节
  压缩: 约48字节
  节省: ~30%

C语言压缩方法：
  使用 cJSON 库时：
    char *str = cJSON_PrintUnformatted(root);  // 不加格式化
  而不是：
    char *str = cJSON_Print(root);             // 有格式化（调试用）
```

---

## 十一、开发注意事项与陷阱

### 11.1 必须遵守的硬性规则

```
Rule 1: 广播消息必须先判断是否发给自己
  ❌ 错误：收到消息直接执行
  ✅ 正确：检查消息中的目标ID，只处理发给本sep的消息
  
  风险等级：🔴 极高
  后果：误触发OTA升级，可能导致系统损坏

Rule 2: 时间戳必须是毫秒级
  ❌ 错误：ts = time(NULL)              // 秒级，差1000倍
  ✅ 正确：ts = getMilliseconds()       // 毫秒级
  
  风险等级：🟡 中等
  后果：时序分析错误，事件排序混乱

Rule 3: id字段不能填入未知值
  ❌ 错误：id = "unknown"
  ✅ 正确：id = "" 或 id = "具体的sep-uuid"
  
  风险等级：🟡 中等
  后果：消息可能被skes-relay丢弃

Rule 4: request的msg_id必须唯一
  ❌ 错误：每次request都用固定的msg_id
  ✅ 正确：每次request生成新的UUID
  
  风险等级：🟡 中等
  后果：无法区分多个并发请求的回复

Rule 5: response必须原样返回request的msg_id
  ❌ 错误：response中生成新的msg_id
  ✅ 正确：response中的msg_id = request中的msg_id
  
  风险等级：🟡 中等
  后果：请求方无法匹配到回复

Rule 6: CAN数据的size字段是原始字节数
  ❌ 错误：size = strlen(base64_string)
  ✅ 正确：size = 原始CAN数据字节数（通常是8）
  
  风险等级：🟢 低
  后果：接收方解析时长度不匹配
```

### 11.2 性能优化建议

```
建议1：高频CAN数据使用二进制格式
  场景：CAN采集频率 > 100Hz
  方法：使用 /skes/dev/data/raw 而非 /skes/dev/data
  效果：CPU占用下降约60%，传输量下降约70%

建议2：图像数据按需携带
  场景：摄像头帧率30fps，但只有触发报警时才需要图像
  方法：正常时 raw_data = ""，报警时才填充图像数据
  效果：避免每帧都传输大量图像数据

建议3：data数组批量发送
  场景：短时间内有多个CAN帧需要上报
  方法：将多帧打包到一个消息的data数组中发送
  ❌ 低效：每个CAN帧单独发一条消息（频繁的nanomsg调用）
  ✅ 高效：100ms内的帧打包成一条消息发送

建议4：JSON压缩传输
  已在规范中要求，使用 cJSON_PrintUnformatted()
```

### 11.3 调试技巧

```
技巧1：订阅所有消息进行监控
  // 创建一个监控sep，订阅所有topic（不过滤）
  // 将收到的所有消息打印到日志
  sub.subscribe("")  // 空字符串表示接收所有

技巧2：区分两个ts字段
  外层 ts = 消息发送时的时间（消息层面）
  内层 ts = 数据实际产生的时间（数据层面）
  
  例如工况数据：
  外层ts: 采集程序打包并发送消息的时刻
  内层data[i].ts: 那条CAN帧实际从总线上采集到的时刻

技巧3：用msg_id追踪请求链路
  在日志中记录每个request的msg_id
  当response到来时，通过msg_id找到对应的request
  可以计算请求-响应的延迟时间

技巧4：验证Base64编码
  在线工具：https://base64.guru/converter/decode/hex
  将Base64字符串解码，比对是否符合预期的字节值
```

---

## 十二、扩展设计指南

### 12.1 新增 Topic 的完整流程

```
扩展步骤（以新增激光雷达 /skes/lidar 为例）：

Step 1: 确定通信需求
  ├── 数据方向？→ sep（激光雷达sep） → skes-relay
  ├── 触发方式？→ 周期性推送（100ms一次）
  ├── 数据量？ → 点云数据，较大，考虑压缩
  └── 是否需要回复？→ 否，单向推送

Step 2: 定义 Topic
  /skes/lidar   （遵循现有命名风格）

Step 3: 设计 data.type 枚举
  "point_cloud"  → 点云数据
  "obstacle"     → 障碍物检测结果

Step 4: 设计 data 对象结构（以point_cloud为例）
  {
    "version": "1.0",
    "id": "sep-lidar-uuid",
    "ts": 时间戳,
    "action": "post",
    "type": "point_cloud",
    "data": {
      "frame_id": 1001,          // 帧编号（递增）
      "scan_time_ms": 100,       // 扫描耗时（ms）
      "range_min": 0.1,          // 最小有效距离（米）
      "range_max": 30.0,         // 最大有效距离（米）
      "points": [
        {
          "x": 1.23, "y": 0.45, "z": 0.0,
          "intensity": 128,      // 反射强度(0~255)
          "ts": 时间戳
        }
      ]
    }
  }

Step 5: 在 topic 分类文档（5.4节）中登记
  添加: 激光雷达: /skes/lidar

Step 6: 通知所有 sep 开发者更新订阅逻辑
```

### 12.2 新增事件 source 的扩展

```
在 /skes/event 中新增 source（以新增 "lidar" 为例）：

alarm 类型下新增：
{
  "source": "lidar",
  "ts": 1639535683010,
  "code": 0,
  "desc": "",
  "status": "triggered",
  "data": [
    {
      "alarm": {
        "trigger": true,
        "type": "obstacle_too_close"  // 扩展：报警子类型
      },
      "distance": 0.5,               // 障碍物距离（米）
      "angle": 15.0,                 // 方向角（度）
      "velocity": -2.3               // 相对速度（m/s，负=靠近）
    }
  ]
}

fault 类型下新增：
{
  "source": "lidar",
  "ts": 1639535683010,
  "code": 1,
  "desc": "激光雷达故障",
  "data": [
    {
      "code": 1,
      "desc": "no point cloud data"   // 无点云数据
    },
    {
      "code": 2,
      "desc": "motor spin failed"     // 电机旋转故障
    }
  ]
}
```

### 12.3 新增 AEB 属性的扩展

```
在 /skes/filter 的 AEB 属性表中新增字段：

现有属性：
  AEBActivateCmd    AEB制动控制信号
  FlashCmd          AEB触发灯光闪烁控制信号
  AlarmCmd          AEB原车报警灯控制信号
  BrkCurCmd_Front   AEB前桥制动阀目标电流
  BrkCurCmd_Rear    AEB后桥制动阀目标电流

可扩展的新属性（示例）：
  SteeringCmd       AEB转向控制信号
  HornCmd           AEB喇叭控制信号
  BrkPressure_Front 前桥制动压力反馈值
  BrkPressure_Rear  后桥制动压力反馈值
  AEBLevel          AEB干预级别（0=预警, 1=轻制动, 2=紧急制动）

扩展格式（data数组中添加新条目）：
  {
    "index":         -1,
    "property_name": "SteeringCmd",
    "data_type":     "double",
    "data":          15.5
  }

注意：property_name 必须在 thing-profile.json 中先定义！
```

### 12.4 新增摄像头检测类型

```
在 /skes/camera 的 data[].type 中新增：

现有类型：
  "bsd"  → 盲区检测
  "aeb"  → 自动紧急制动摄像头
  "dms"  → 驾驶员监控

可扩展类型（示例）：
  "fire_detection"           → 火焰检测
  "seat_departure"           → 离座检测
  "personnel_intrusion"      → 人员入侵检测
  "license_plate"            → 车牌识别
  "fatigue_detection"        → 疲劳检测

示例：离座检测
{
  "type": "seat_departure",
  "camera": { "id": "4", "channel": "4" },
  "models": { "algorithms": "yolov8", "version": "1" },
  "ts": 1754376690290,
  "results": [
    {
      "class_id": 100,
      "class_name": "empty_seat",
      "bounding_box": { "left": 200, "top": 100, "width": 300, "height": 400 },
      "confidence": 0.95
    }
  ]
}
```

### 12.5 扩展通信故障类型

```
在 type="fault", source="communication" 的 data[].type 中新增：

现有类型：
  "rs485"     RS485串口
  "lte"       4G/LTE网络
  "gps"       GPS定位
  "bluetooth" 蓝牙
  "ota"       OTA升级
  "ntp"       时间同步

可扩展类型（示例）：
  "wifi"      WiFi连接
  "can"       CAN总线通信
  "ethernet"  有线网络

示例：
{
  "type": "wifi",
  "code": 1,
  "desc": "failed to connect to AP"
}
```

---

## 十三、协议总览速查图

### 完整架构与数据流向

```
┌────────────────────────────────────────────────────────────────────────┐
│                        SKES 通信协议总览                                │
├────────────────────────────────────────────────────────────────────────┤
│                                                                        │
│  物理层：nanomsg (PUB/SUB/XPUB/XSUB)                                  │
│  ┌──────────────┐                    ┌──────────────┐                 │
│  │              │ ──port:19225 pub──>│              │                 │
│  │  各 sep 程序  │                    │  skes-relay  │                 │
│  │              │<──port:19226 sub── │              │                 │
│  └──────────────┘                    └──────────────┘                 │
│                                                                        │
├────────────────────────────────────────────────────────────────────────┤
│  帧格式：[topic_size:1B][topic:nB][payload_size:4B,LE][payload:mB]     │
├────────────────────────────────────────────────────────────────────────┤
│                                                                        │
│  Topic 与数据方向：                                                     │
│                                                                        │
│  sep ──────────────────────────────────────> skes-relay               │
│    /skes/dev/data        工况JSON             action=post              │
│    /skes/dev/data/raw    工况二进制           action=post              │
│    /skes/location        GPS位置             action=post              │
│    /skes/event           事件上报            action=post              │
│      └── type=alarm   告警(aeb/bsd/dms/radar/camera/sound_light_alarm)│
│      └── type=event   普通事件(usb)                                    │
│      └── type=fault   故障(camera/communication/object_detection/data) │
│                                                                        │
│  sep <═══════════════════════════════════════> skes-relay             │
│    /skes/version         版本协商            request/response         │
│    /skes/heartbeat       心跳保活            broadcast/post           │
│    /skes/camera          目标检测            action=post              │
│      └── type=object_detection (bsd/aeb/dms等)                        │
│    /skes/filter          属性过滤/查询       post/request/response    │
│      └── data.type=filter_table  推送工况属性                          │
│      └── data.type=query         查询属性（需response）                 │
│      └── data.type=AEB           推送AEB控制指令                        │
│                                                                        │
├────────────────────────────────────────────────────────────────────────┤
│  Payload 通用外层：                                                     │
│    version(str) | id(str,UUID) | ts(int64,ms) |                       │
│    msg_id(str,仅request/response) | action(str) | type(str) | data    │
├────────────────────────────────────────────────────────────────────────┤
│  数据类型：boolean/int8/16/32/64/uint8/16/32/64/float/double/          │
│           string/object/array/binary(Base64)                           │
│  字符编码：UTF-8                                                        │
│  数值端序：小端（Little-Endian）                                         │
│  时间戳：  Unix时间，毫秒，UTC                                           │
└────────────────────────────────────────────────────────────────────────┘
```

### 快速索引：我要发什么消息？

| 我需要上报... | 用哪个Topic | action | type字段 |
|--------------|------------|--------|----------|
| CAN工况数据 | `/skes/dev/data` | post | "CAN" |
| 高频CAN数据 | `/skes/dev/data/raw` | post | 二进制 |
| GPS位置 | `/skes/location` | post | "tbox" |
| AEB触发报警 | `/skes/event` | post | "alarm"（source="aeb"） |
| BSD盲区警告 | `/skes/event` | post | "alarm"（source="bsd"） |
| DMS驾驶员异常 | `/skes/event` | post | "alarm"（source="dms"） |
| 摄像头故障 | `/skes/event` | post | "fault"（source="camera"） |
| 通信故障 | `/skes/event` | post | "fault"（source="communication"） |
| USB插入 | `/skes/event` | post | "event"（source="usb"） |
| AI目标检测结果 | `/skes/camera` | post | "object_detection" |
| 推送工况属性 | `/skes/filter` | post | "properties"（data.type="filter_table"） |
| 查询属性值 | `/skes/filter` | request | "properties"（data.type="query"） |
| AEB控制指令 | `/skes/filter` | post | "properties"（data.type="AEB"） |

---

*本文档基于 SKES通信规范 v0.8 整理 | 版权归树根科技所有*
*文档版本: v1.0 | 整理时间: 2026-02-26*
