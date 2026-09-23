# BCM 参数页 — dataengine ↔ UI 对接说明

## 数据来源

| 项目 | 说明 |
|------|------|
| CAN 通道 | CAN0 |
| CAN ID | `0x18FD0291`（扩展帧） |
| 解析位置 | `core/RealtimeStatusManager.cpp` |
| 推送位置 | `vehicles/forklift/SkesQtBridge.cpp` |
| 推送周期 | 每次收到该 CAN 帧时触发更新 |

---

## BYTE0 故障码机制（理解背景）

BCM 每帧只能通过 BYTE0 发一个故障编号，多个故障时**轮流循环发送**：

```
例：K9 + K12 同时故障
时间轴: [9] [12] [9] [12] [9] [12] ...

例：全部恢复
时间轴: [0] [0] [0] ...
```

dataengine 内部用时间戳 map 将循环码**还原成每路独立布尔值**，超过 **5 秒**未再出现的故障码自动标记为已恢复，BYTE0=0 立即清空所有故障。

**UI 不需要关心这个机制**，直接读下面的字段即可。

---

## UI 可用字段（vehicle.xxx）

### 指示灯类字段 — 显示规则：`0 → 绿灯，非0 → 红灯`

#### K* 继电器状态（6路）

| QML 属性 | 类型 | 含义 | 0 | 非0 |
|----------|------|------|---|-----|
| `vehicle.k5`  | int | K5 继电器 | 正常（绿） | 故障（红） |
| `vehicle.k7`  | int | K7 继电器 | 正常（绿） | 故障（红） |
| `vehicle.k9`  | int | K9 继电器 | 正常（绿） | 故障（红） |
| `vehicle.k10` | int | K10 继电器 | 正常（绿） | 故障（红） |
| `vehicle.k11` | int | K11 继电器 | 正常（绿） | 故障（红） |
| `vehicle.k12` | int | K12 继电器 | 正常（绿） | 故障（红） |

#### F* 保险丝状态（20路，F5~F24）

| QML 属性 | 类型 | 含义 | 0 | 非0 |
|----------|------|------|---|-----|
| `vehicle.f5`  ~ `vehicle.f24` | int | 对应保险丝 | 正常（绿） | 故障（红） |

> 完整列表：f5、f6、f7、f8、f9、f10、f11、f12、f13、f14、f15、f16、f17、f18、f19、f20、f21、f22、f23、f24

---

### 数字/状态类字段

#### DI 数字输入（7路）

| QML 属性 | 类型 | 含义 | 备注 |
|----------|------|------|------|
| `vehicle.di1` | int | DI1 / 钥匙 ON | 1=有效 |
| `vehicle.di2` | int | DI2 | 1=有效 |
| `vehicle.di3` | int | DI3 | 1=有效 |
| `vehicle.di4` | int | DI4 | 1=有效 |
| `vehicle.di5` | int | DI5 | 1=有效 |
| `vehicle.di6` | int | DI6 | 1=有效 |
| `vehicle.di7` | int | DI7 | 1=有效 |

#### DO 数字输出（6路）

| QML 属性 | 类型 | 含义 | 备注 |
|----------|------|------|------|
| `vehicle.do1` | int | DO1 / 大灯 | 1=输出中 |
| `vehicle.do2` | int | DO2 | 1=输出中 |
| `vehicle.do3` | int | DO3 | 1=输出中 |
| `vehicle.do4` | int | DO4 | 1=输出中 |
| `vehicle.do5` | int | DO5 | 1=输出中 |
| `vehicle.do6` | int | DO6 | 1=输出中 |

#### 其他

| QML 属性 | 类型 | 含义 | 备注 |
|----------|------|------|------|
| `vehicle.heartbeat`      | int | BCM 心跳（BYTE2 bit7） | 0/1 交替变化 |
| `vehicle.c11Ratio`       | int | C11 输出比例（BYTE5）  | 0~255 |
| `vehicle.c12Ratio`       | int | C12 输出比例（BYTE6）  | 0~255 |
| `vehicle.bcmSoftVersion` | int | BCM 软件版本（BYTE7）  | 原始整数 |
| `vehicle.activeFaultCodes` | string | 当前活跃故障码 JSON 数组 | 如 `"[9,12]"` |
| `vehicle.fuseFaultCode`  | int | BYTE0 循环码当前瞬时值 | **UI 不使用此字段** |

---

## QML 使用示例

```qml
// 单个指示灯（绿/红）
Rectangle {
    width: 28; height: 28; radius: 14
    color: vehicle.f5 !== 0 ? "#ff3333" : "#00cc44"
}

// 继电器指示灯
Rectangle {
    width: 28; height: 28; radius: 14
    color: vehicle.k9 !== 0 ? "#ff3333" : "#00cc44"
}

// 显示当前所有故障码（调试用）
Text {
    text: "活跃故障: " + vehicle.activeFaultCodes
}
```

---

## 故障恢复逻辑（UI 无需处理）

| 情况 | dataengine 行为 | UI 表现 |
|------|----------------|---------|
| BYTE0 = 0 | 立即清空所有故障 | 全部变绿 |
| 某故障码 5 秒未出现 | 自动移除该故障 | 对应灯变绿 |
| 新故障码出现 | 加入活跃集合 | 对应灯变红 |
