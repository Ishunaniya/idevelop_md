# skes-dataengine × Qt 侧联调文档

> 适用版本：`skes-dataengine` ≥ v0.1.0.3.alpha26  
> 文档面向：skes-ui (Qt5) 开发人员  
> 维护人：yuping.liu（dataengine 侧）

---

## 1. 通信基础

Qt 与 dataengine 之间通过 **TCP Socket** 交换 JSON 报文。  
dataengine 监听端口：**`9090`**（默认，可在 `config.json` 中配置）。

### 1.1 Qt → dataengine（下发）

```json
{
  "version": "1.0",
  "id": "qt-instrument",
  "ts": <Unix毫秒时间戳>,
  "msg_id": "<唯一消息ID，建议 uuid 或递增序号>",
  "action": "request",
  "type": "properties",
  "data": {
    "type": "cmd_control",
    "data": [
      {
        "index": -1,
        "property_name": "<属性名>",
        "data_type": "<int|double|string>",
        "data": <值>
      }
    ]
  }
}
```

- `data.data` 为数组，可一次下发多个属性。  
- `index` 固定填 `-1`（dataengine 侧忽略该字段）。

### 1.2 dataengine → Qt（应答）

```json
{
  "version": "1.0",
  "id": "skes-dataengine",
  "ts": <Unix毫秒时间戳>,
  "msg_id": "<与请求相同>",
  "action": "response",
  "type": "properties",
  "data": {
    "type": "cmd_control",
    "data": [
      {
        "property_name": "<属性名>",
        "result": "ok"
      }
    ]
  }
}
```

### 1.3 dataengine → Qt（周期推送 filter_table）

dataengine 每 **~500 ms** 主动推送一次完整的车辆状态树：

```json
{
  "action": "push",
  "type": "filter_table",
  "data": { ...完整状态树... }
}
```

Qt 可从该推送中读取任意字段的最新值，无需主动查询。

---

## 2. 新增属性一览

以下四个属性均在本次迭代中新增，支持 `cmd_control` 下发设置。

| 属性名 | data_type | 有效范围 | 掉电保存 | 说明 |
|---|---|---|---|---|
| `totalWorkHours` | `double` | ≥ 0.0 | ✅ | 强制修正总工作时间（单位：小时） |
| `totalMileage` | `double` | ≥ 0.0 | ✅ | 强制修正总里程（单位：km） |
| `screenBrightness` | `int` | 0 ~ 100 | ✅ | 屏幕亮度百分比（0 = 硬件最低亮度，非黑屏） |
| `language` | `int` | 0 = 中文, 1 = 英文 | ✅ | 界面语言 |

---

## 3. 属性详解

### 3.1 totalWorkHours — 总工作时间修正

用途：后台管理或服务工程师在界面上修改车辆累计工作时间（例如更换控制器后对齐历史数据）。

**下发示例（设置为 1234.5 小时）：**

```json
{
  "version": "1.0",
  "id": "qt-instrument",
  "ts": 1716700000000,
  "msg_id": "msg-001",
  "action": "request",
  "type": "properties",
  "data": {
    "type": "cmd_control",
    "data": [
      {
        "index": -1,
        "property_name": "totalWorkHours",
        "data_type": "double",
        "data": 1234.5
      }
    ]
  }
}
```

**读回（filter_table 推送中的路径）：**

```
data.chache.mainScreen.totalWorkHours    → double，单位 h
data.chache.versionInfo.meterWorkHours   → double，单位 h（仪表盘镜像字段）
```

**注意：**
- 下发后立即生效，dataengine 同步写入持久化文件 `/data/forklift_persistent.json`。
- 重启后自动恢复为最后一次设置的值。
- 负数会被截断为 0。

---

### 3.2 totalMileage — 总里程修正

用途：同上，修正车辆累计行驶里程。

**下发示例（设置为 5678.9 km）：**

```json
{
  "version": "1.0",
  "id": "qt-instrument",
  "ts": 1716700000000,
  "msg_id": "msg-002",
  "action": "request",
  "type": "properties",
  "data": {
    "type": "cmd_control",
    "data": [
      {
        "index": -1,
        "property_name": "totalMileage",
        "data_type": "double",
        "data": 5678.9
      }
    ]
  }
}
```

**读回（filter_table 推送中的路径）：**

```
data.chache.mainScreen.totalMileage         → double，单位 km
data.chache.versionInfo.meterTotalMileage   → double，单位 km（仪表盘镜像字段）
```

---

### 3.3 screenBrightness — 屏幕亮度

**Qt 侧使用 0~100 的百分比，dataengine 负责映射到硬件亮度值（sysfs 50~255），Qt 无需关心底层映射。**

| Qt 值 | 实际硬件值 | 说明 |
|---|---|---|
| 0 | 50 | 最低亮度（硬件下限，不黑屏） |
| 50 | 152 | 中等亮度 |
| 100 | 255 | 最高亮度 |

**下发示例（设置为 80%）：**

```json
{
  "version": "1.0",
  "id": "qt-instrument",
  "ts": 1716700000000,
  "msg_id": "msg-003",
  "action": "request",
  "type": "properties",
  "data": {
    "type": "cmd_control",
    "data": [
      {
        "index": -1,
        "property_name": "screenBrightness",
        "data_type": "int",
        "data": 80
      }
    ]
  }
}
```

**读回（filter_table 推送中的路径）：**

```
data.chache.system.screenBrightness   → int，0~100
```

**初始值读取（Qt 启动时）：**  
Qt 启动后等待第一次 `filter_table` 推送，从 `chache.system.screenBrightness` 读取当前值，用于初始化界面滑块位置。

**注意：**
- 下发后 dataengine 立即写入 sysfs，屏幕亮度实时生效。
- 超出 0~100 的值会被 clamp，不会报错。
- 属性名统一使用 `screenBrightness`（不是 `brightness`）。

---

### 3.4 language — 界面语言

| 值 | 含义 |
|---|---|
| 0 | 中文（默认） |
| 1 | 英文 |

**下发示例（切换为英文）：**

```json
{
  "version": "1.0",
  "id": "qt-instrument",
  "ts": 1716700000000,
  "msg_id": "msg-004",
  "action": "request",
  "type": "properties",
  "data": {
    "type": "cmd_control",
    "data": [
      {
        "index": -1,
        "property_name": "language",
        "data_type": "int",
        "data": 1
      }
    ]
  }
}
```

**读回（filter_table 推送中的路径）：**

```
data.chache.system.language   → int，0 或 1
```

**注意：**
- dataengine 仅负责持久化语言设置并通过 `filter_table` 通知 Qt 当前值。
- Qt 侧收到 `language` 字段变更后，负责自行切换界面语言资源（Qt Linguist / tr()）。
- 非 0/1 的值传入时，dataengine 会原样存储（不做 clamp），建议 Qt 侧只传 0 或 1。

---

## 4. 状态树路径速查

下表列出本次新增字段在 `filter_table` 推送的 `data` 对象中的完整路径：

| 字段 | JSON 路径 | 类型 | 说明 |
|---|---|---|---|
| 总工作时间 | `chache.mainScreen.totalWorkHours` | double | 主界面读值路径 |
| 总工作时间（仪表） | `chache.versionInfo.meterWorkHours` | double | 仪表盘版本信息路径 |
| 总里程 | `chache.mainScreen.totalMileage` | double | 主界面读值路径（km） |
| 总里程（仪表） | `chache.versionInfo.meterTotalMileage` | double | 仪表盘版本信息路径 |
| 屏幕亮度 | `chache.system.screenBrightness` | int | 0~100 |
| 语言 | `chache.system.language` | int | 0=中文, 1=英文 |

---

## 5. 主动查询（可选）

若 Qt 需要立刻拉取当前值（不等待下次 filter_table 推送），可发送 `query` 请求：

```json
{
  "version": "1.0",
  "id": "qt-instrument",
  "ts": 1716700000000,
  "msg_id": "msg-query-001",
  "action": "request",
  "type": "properties",
  "data": {
    "type": "query",
    "data": ["screenBrightness", "language", "totalWorkHours", "totalMileage"]
  }
}
```

dataengine 会立即返回包含上述字段当前值的 response 报文。

---

## 6. 联调测试工具

dataengine 提供命令行测试工具 `skes_qt_bridge_tester`，可在开发机上模拟 Qt 下发：

```bash
# 编译（在 skes-dataengine build 目录下）
make skes_qt_bridge_tester

# 设置总工作时间为 300 小时
./skes_qt_bridge_tester set_total_work_hours 300

# 设置总里程为 12345.6 km
./skes_qt_bridge_tester set_total_mileage 12345.6

# 设置屏幕亮度为 80%
./skes_qt_bridge_tester set_brightness 80

# 切换语言为英文
./skes_qt_bridge_tester set_language 1

# 订阅 filter_table 推送（验证字段变更）
./skes_qt_bridge_tester subscribe --full
```

---

## 7. 持久化说明

以上四个属性均持久化到 `/data/forklift_persistent.json`，设备断电重启后自动恢复。  
该文件由 dataengine 独占管理，**Qt 侧不应直接读写此文件**。（上电可读取语言设置）

```json
{
  "totalWorkHours": 1234.5,
  "totalMileage": 5678.9,
  "language": 1,
  "screenBrightness": 80
}
```

---

## 8. RFID 刷卡信息（rfidData）

本次迭代新增 RFID 刷卡模块接入，数据来自 CAN1 总线 0x4D1 / 0x4D2 报文，dataengine 解析后写入状态树并通过 `filter_table` 推送给 Qt。

### 8.1 字段说明

所有字段位于 `filter_table` 推送的 `data.chache.rfidData` 对象下：

| 字段名 | 类型 | 来源报文 | 说明 |
|---|---|---|---|
| `heartbeatStatus` | int | CAN1 0x4D1 byte0 bit0 | RFID 模块心跳指示灯状态：0=灯灭，1=灯亮 |
| `workStatus` | int | CAN1 0x4D1 byte0 bit1 | 工作状态：0=待机，1=工作（已刷卡解锁） |
| `faultStatus` | int | CAN1 0x4D1 byte0 bit2~4 | 故障状态，见下表 |
| `unlockCardId` | string | CAN1 0x4D1 byte1~4 | 当前解锁卡号，4字节 UID 十六进制字符串，如 `"A1B2C3D4"` |
| `sequence` | int | CAN1 0x4D1 | 收到 0x4D1 报文时自动 +1 的计数器 |
| `unlockTime` | string | 由 dataengine 生成 | `workStatus` 0→1 跳变时记录的本地时间，格式 `"YYYY-MM-DD HH:MM:SS"` |
| `unlockDuration` | int | CAN1 0x4D2 byte4~5 | **前一次**解卡持续时长（小端序，单位由 RFID 硬件协议定义） |

### 8.2 faultStatus 枚举

| 值 | 含义 |
|---|---|
| 0 | 正常 |
| 1 | 解卡失败 / IC 卡未录入 |
| 2 | 加卡错误 / 已达最大录入数 |

### 8.3 sequence 的用途

`sequence` 每次收到 0x4D1 报文时自增（不重置）。  
Qt 可通过对比前后两次 `filter_table` 推送中 `sequence` 的值来检测**是否有新刷卡事件**，无需逐字段对比。

```js
// 伪代码示例
if (newData.rfidData.sequence !== lastSequence) {
    lastSequence = newData.rfidData.sequence;
    // 有新刷卡，读取 unlockCardId / workStatus / faultStatus
}
```

### 8.4 unlockTime 说明

`unlockTime` 仅在 `workStatus` 由 0 跳变为 1 时更新（即每次成功刷卡解锁时刻），不随心跳持续刷新。  
格式示例：`"2026-05-26 14:30:00"`

### 8.5 filter_table 路径速查

```
data.chache.rfidData.heartbeatStatus   → int    心跳灯
data.chache.rfidData.workStatus        → int    工作状态
data.chache.rfidData.faultStatus       → int    故障码
data.chache.rfidData.unlockCardId      → string 卡号(8位十六进制)
data.chache.rfidData.sequence          → int    刷卡序号（变化即有新事件）
data.chache.rfidData.unlockTime        → string 最近解锁时间 "YYYY-MM-DD HH:MM:SS"
data.chache.rfidData.unlockDuration    → int    前次解锁时长
```

### 8.6 注意事项

- `rfidData` 字段**只读**，Qt 侧不支持通过 `cmd_control` 写入（RFID 数据来自 CAN 硬件，非可配置项）。
- 若 RFID 模块未接入或 CAN 总线无报文，所有字段保持初始值：数值字段为 `0`，字符串字段为 `""`。
- `unlockCardId` 在 `workStatus=0`（待机）期间可能保留上一次刷卡的卡号，Qt 应结合 `workStatus` 判断是否有效。

---

## 9. 常见问题

**Q：下发 totalWorkHours 后，filter_table 里读到的值没变？**  
A：dataengine 的 filter_table 推送间隔约 500ms，稍等一个周期即可。也可使用 `query` 立即查询。

**Q：screenBrightness 设置 0 会黑屏吗？**  
A：不会。dataengine 将 0% 映射到硬件值 50（sysfs 量程 0~255），是肉眼可见的最低亮度，不会导致黑屏。

**Q：重启后亮度/语言会不会恢复默认？**  
A：不会，两者均持久化。dataengine 启动时自动从 `/data/forklift_persistent.json` 读取并恢复，亮度还会同步写入 sysfs 立即生效。

**Q：totalWorkHours 和 totalMileage 是覆盖写还是增量加？**  
A：是**覆盖写**，即下发的值就是新的绝对值，不是在原有基础上加减。
