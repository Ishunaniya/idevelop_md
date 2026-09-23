# Qt 接口变更说明 — 故障信息与转向电机温度

> 适用模块：`skes-dataengine` → `skes-main_fork-lift`
> 通信通道：`/skes/filter`，action=`post`，type=`properties`，data.type=`filter_table`

---

## 一、本次新增字段汇总

以下字段已加入 `filter_table` 周期推送（500ms），Qt 可直接从 `filter_table` 的 `data` 数组中按 `property_name` 取值，也可通过 `query` 接口按需查询。

### 1. BMS 故障严重等级

| property_name | data_type | 来源 CAN 帧 | 说明 |
|---|---|---|---|
| `bmsErr` | `int` | CAN0 `0x0CFF8003` byte3 bit0~1 | 0=正常，1=预警，2=一级故障，3=二级故障 |

**触发规则**：byte0~byte6 任意位非零即为有故障；`bmsErr` 取 byte3 bit0~1（总故障状态）作为等级；若总故障状态为 0 但其他字节有非零位，`bmsErr` 置 1。所有字节归零后自动恢复（故障从列表移除）。

BMS 43 个子故障位分布：

| 字节 | 位 | 故障编号 | 描述（部分） |
|---|---|---|---|
| byte0 | bit0~1 | BMSERR1 | 单体电压过高 |
| byte0 | bit2~3 | BMSERR2 | 单体电压过低 |
| byte0 | bit4~5 | BMSERR3 | 总压过高 |
| byte0 | bit6~7 | BMSERR4 | 总压过低 |
| byte1 | bit0~1 | BMSERR5 | 压差一致性故障 |
| byte1 | bit2~3 | BMSERR6 | 温差故障 |
| byte1 | bit4~5 | BMSERR7 | 温度过高 |
| byte1 | bit6~7 | BMSERR8 | 温度过低 |
| byte2 | bit0~1 | BMSERR9 | SOC过低 |
| byte2 | bit2~3 | BMSERR10 | 绝缘故障 |
| byte2 | bit4~5 | BMSERR11 | 回馈功率过高 |
| byte2 | bit6~7 | BMSERR12 | 放电功率过高 |
| byte3 | bit0~1 | BMSERR13 | **总故障状态**（即 `bmsErr` 值来源）|
| byte3 | bit2 | BMSERR14 | 低压供电过高 |
| byte3 | bit3 | BMSERR15 | 低压供电过低 |
| byte3 | bit4 | BMSERR16 | SOC跳变 |
| byte3 | bit5 | BMSERR17 | 充电座过温 |
| byte3 | bit6 | BMSERR18 | 加热温升异常 |
| byte3 | bit7 | BMSERR19 | MSD互锁故障 |
| byte4 | bit0~7 | BMSERR20~27 | VCU离线/BMS通信/热失控/传感器类 |
| byte5 | bit0~7 | BMSERR28~35 | 继电器粘连/失效类 |
| byte6 | bit0~7 | BMSERR36~43 | 传感器/充电/预充类 |

---

### 2. 转向电机温度

| property_name | data_type | 来源 CAN 帧 | 说明 |
|---|---|---|---|
| `strMotorTemp` | `int` | CAN0 `0x0CFF030A` BYTE5 | 转向电机温度，单位℃，偏移-40（raw 0→-40℃，raw 40→0℃） |
| `strIGBTTemp` | `int` | CAN0 `0x0CFF030A` BYTE6 | 转向 IGBT 温度，单位℃，偏移-40 |

---

## 二、faultInfo 故障信息页完整字段说明

故障信息通过两种方式推送给 Qt：

- **各错误码字段**（`userErr` / `vcuErr` 等）：直接携带当前故障码原始值，0 = 无故障
- **`faultList`**：所有当前活跃故障的聚合列表，按故障类型升序排列

### 2.1 各字段定义

| property_name | data_type | 说明 | 数据来源 |
|---|---|---|---|
| `faultCount` | `int` | 当前活跃故障总数（= `faultList` 数组长度） | 内部维护 |
| `faultList` | `string` | 活跃故障列表，JSON 数组序列化为字符串（见 2.2） | 内部维护 |
| `userErr` | `int` | 用户提醒故障码，0=正常，1~20 见故障码表 | CAN1 `0x32D` BYTE1 |
| `vcuErr` | `int` | VCU 故障码，0=正常，1~255 | CAN1 `0x32E` BYTE1 |
| `driveMcuErr` | `int` | 行走电机电控故障码，0=正常，非零见故障码表（DERR系列） | CAN0 `0x0CFF0208` BYTE4~5 小端 |
| `steerMcuErr` | `int` | 转向电机电控故障码，0=正常，非零见故障码表（SERR系列） | CAN0 `0x360` BYTE1 |
| `pumpMcuErr` | `int` | 油泵电机电控故障码，0=正常，非零见故障码表（PERR系列） | CAN0 `0x0CFF0209` BYTE4~5 小端 |
| `bmsErr` | `int` | BMS 故障严重等级，0=正常，1=预警，2=一级，3=二级 | CAN0 `0x0CFF8003` byte3 bit0~1（见第一节）|
| `bcmErr` | `int` | BCM 故障码（由其他模块写入，dataengine 不解析） | — |
| `maintenanceAlert` | `int` | 保养提示，0=无需保养，1=需要保养 | 内部逻辑：总工时≥300h首次触发，之后每1200h再次触发 |

> **注**：`faultList` 推送的是 JSON 数组的字符串形式，Qt 端需调用 `JSON.parse()` 或 `QJsonDocument::fromJson()` 解析。

---

### 2.2 faultList 数组元素结构

```json
[
  {
    "seq":      1,
    "type":     "driveMcuErr",
    "typeName": "行走MCU故障",
    "code":     36,
    "time":     "2026-05-29 10:23:45"
  },
  {
    "seq":      2,
    "type":     "bmsErr",
    "typeName": "BMS故障",
    "code":     2,
    "time":     "2026-05-29 10:24:01"
  }
]
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `seq` | int | 序号，从 1 开始，按下表排序号升序排列 |
| `type` | string | 故障类型代码（与各错误码字段名一致，见下表） |
| `typeName` | string | 故障类型名称 |
| `code` | int | 故障码（对应各控制器故障码表中的数值） |
| `time` | string | 该故障首次发生时间（格式：`YYYY-MM-DD HH:MM:SS`） |

**故障类型代码对应关系：**（`type` 即对应错误码字段名，排序号为列表固定排列顺序）

| 排序号 | type（代码） | typeName | 对应字段 |
|---|---|---|---|
| 1 | `userErr` | 用户提醒 | `userErr` |
| 2 | `vcuErr` | VCU故障 | `vcuErr` |
| 3 | `driveMcuErr` | 行走MCU故障 | `driveMcuErr` |
| 4 | `steerMcuErr` | 转向MCU故障 | `steerMcuErr` |
| 5 | `pumpMcuErr` | 油泵MCU故障 | `pumpMcuErr` |
| 6 | `bmsErr` | BMS故障 | `bmsErr` |
| 7 | `bcmErr` | BCM故障 | `bcmErr` |
| 8 | `maintenanceAlert` | 保养提示 | `maintenanceAlert` |

**faultList 维护规则：**
- 某类型故障码变为非零 → 该类型条目加入列表，记录首次发生时间
- 故障码变回 0 → 该类型条目从列表移除
- 列表始终按上表排序号升序排列，seq 重新连续编号
- 同一 type 同时只存在一条记录（code 更新但 time 不变）

---

## 三、Qt 端使用示例

### 3.1 从 filter_table 读取故障信息

```javascript
// filter_table push 回调中
for (const item of data) {
    if (item.property_name === "faultCount") {
        // 主界面故障数量角标
        updateFaultBadge(item.data);
    }
    if (item.property_name === "faultList") {
        // 故障列表页
        const list = JSON.parse(item.data);  // 解析 JSON 字符串
        renderFaultList(list);
    }
    if (item.property_name === "bmsErr" && item.data > 0) {
        // BMS 故障指示
        showBmsAlarm(item.data);  // 1=预警 2=一级 3=二级
    }
    if (item.property_name === "maintenanceAlert" && item.data === 1) {
        showMaintenanceDialog();
    }
}
```

### 3.2 单独查询故障信息

```json
{
  "action": "request",
  "type": "properties",
  "msg_id": "xxx",
  "data": {
    "type": "query",
    "data": [
      { "property_name": "faultInfo.faultCount",      "data_type": "int" },
      { "property_name": "faultInfo.faultList",       "data_type": "string" },
      { "property_name": "faultInfo.bmsErr",          "data_type": "int" },
      { "property_name": "faultInfo.maintenanceAlert","data_type": "int" }
    ]
  }
}
```

### 3.3 查询新增温度字段

```json
{
  "action": "request",
  "type": "properties",
  "msg_id": "xxx",
  "data": {
    "type": "query",
    "data": [
      { "property_name": "runStatus.strMotorTemp", "data_type": "int" },
      { "property_name": "runStatus.strIGBTTemp",  "data_type": "int" }
    ]
  }
}
```

---

## 四、变更文件列表

| 文件 | 变更内容 |
|---|---|
| `core/RealtimeStatusManager.cpp` | 新增 CAN0 `0x0CFF8003` BMS故障帧解析；`0x0CFF030A` 补充 BYTE5/BYTE6 转向温度解析 |
| `vehicles/forklift/SkesQtBridge.cpp` | filter_table 推送增加 `strMotorTemp`、`strIGBTTemp` |
| `profiles/forklift/realtime_status.json` | `runStatus` 增加 `strMotorTemp`、`strIGBTTemp` 初始值 |
