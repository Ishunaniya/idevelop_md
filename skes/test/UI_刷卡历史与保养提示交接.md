# UI 交接：刷卡历史与保养提示

> 对接范围：`skes-dataengine` 叉车版本
>
> 本文只说明“刷卡历史”和“保养提示”新增接口；既有 CAN 状态、故障列表和其他设置接口不受影响。

---

## 1. 对接结论

UI 需要完成两件事：

1. 在刷卡历史页查询/展示历史记录；
2. 在出现保养提示时显示“清除”按钮，并下发清除命令。

后端会保存最近 100 条刷卡解锁记录，并在重启后恢复。保养提示按固定工时节点 `300 → 1500 → 2700 …` 触发；清除后自动进入下一个尚未到达的节点。

---

## 2. 通信约定

Qt 仍使用现有的 `/skes/filter` 属性协议：

- 主动查询：`data.type = "query"`；
- 下发清除：`data.type = "cmd_control"`；
- 状态推送：`data.type = "filter_table"`；
- `msg_id` 由 UI 生成，并在应答中原样返回；
- 请求中的 `index` 固定为 `-1`。

### 2.1 查询路径与推送字段名

查询使用完整路径；`filter_table` 推送使用平铺的 `property_name`。两者不要混用。

| 功能 | 主动查询 `property_name` | `filter_table` 推送 `property_name` | 类型 |
|---|---|---|---|
| 刷卡历史 | `rfidData.rfidHistory` | `rfidHistory` | string，内容为 JSON 数组 |
| 保养提示是否显示 | `faultInfo.maintenanceAlert` | `maintenanceAlert` | bool / int（UI 按 0/1 使用） |
| 是否存在待清除保养 | `faultInfo.maintenancePending` | `maintenancePending` | bool / int（UI 按 0/1 使用） |
| 下一保养节点（h） | `faultInfo.nextMaintenanceHours` | `nextMaintenanceHours` | double |
| 故障列表 | `faultInfo.faultList` | `faultList` | 查询返回 JSON 数组；推送为内容是 JSON 数组的 string |

---

## 3. 刷卡历史页

### 3.1 首次进入页面查询

请求示例：

```json
{
  "version": "1.0",
  "id": "qt-instrument",
  "ts": 1770000000000,
  "msg_id": "rfid-history-001",
  "action": "request",
  "type": "properties",
  "data": {
    "type": "query",
    "data": [{
      "index": -1,
      "property_name": "rfidData.rfidHistory",
      "data_type": "string"
    }]
  }
}
```

成功响应项的 `value` 是 JSON 字符串。UI 需要再做一次 JSON 解析：

```json
[
  {
    "cardId": "A1B2C3D4",
    "unlockTime": "2026-09-02 10:30:00",
    "unlockDuration": 3600
  }
]
```

字段说明：

| 字段 | 类型 | 含义 |
|---|---|---|
| `cardId` | string | 4 字节 UID，8 位大写十六进制 |
| `unlockTime` | string | 解锁事件发生时间，格式 `YYYY-MM-DD HH:MM:SS` |
| `unlockDuration` | int | 后续 CAN1 `0x4D2` 填入的解锁时长；尚未收到时为 `0` |

空历史固定返回字符串 `"[]"`。后端按时间从旧到新保存，UI 如需“最新在前”可在本地倒序显示。

### 3.2 实时刷新

订阅到 `filter_table` 后，找到：

```json
{
  "property_name": "rfidHistory",
  "data_type": "string",
  "data": "[...]"
}
```

解析 `data` 后刷新当前历史页即可。建议页面进入时先主动查询一次，再监听推送，避免刚订阅时遗漏旧记录。

### 3.3 记录规则

- 只有 CAN1 `0x4D1` 的 `workStatus` 从 `0` 跳变为 `1` 才新增一条历史；心跳帧、重复的工作状态帧不会新增。
- CAN1 `0x4D2` 到达后补写最近一条记录的 `unlockDuration`。
- 最多保留 100 条；超出后删除最旧的一条。
- 历史会掉电保存；既有单条 `unlockCardId/unlockTime/unlockDuration` 数据会兼容迁移。

---

## 4. 保养提示与“清除”按钮

### 4.1 UI 显示规则

- `maintenancePending = 1` 时显示保养提示和“清除”按钮；
- `maintenanceAlert` 与 `maintenancePending` 由后端保持同步，通常可只用 `maintenancePending` 控制按钮可见性；
- 可展示 `nextMaintenanceHours`，例如“下次保养节点：1500 h”；
- `faultList` 中同时会出现 `type="maintenanceAlert"`、`code=8000` 的条目，故障页面无需自行拼接该条目。

### 4.2 清除命令

按钮文案使用 **“清除”**。点击后发送：

```json
{
  "version": "1.0",
  "id": "qt-instrument",
  "ts": 1770000000000,
  "msg_id": "maintenance-clear-001",
  "action": "request",
  "type": "properties",
  "data": {
    "type": "cmd_control",
    "data": [{
      "index": -1,
      "property_name": "clearMaintenanceAlert",
      "data_type": "int",
      "data": 1
    }]
  }
}
```

处理结果：

1. `maintenanceAlert` 变为 `0`；
2. `maintenancePending` 变为 `0`；
3. `faultList` 删除保养提示条目；
4. `nextMaintenanceHours` 推进到当前总工时之后的下一个固定节点；
5. 上述状态立即掉电保存。

例如首次 300h 提示在 300h 清除后，下次节点为 1500h；若到 2800h 才清除，则下次节点为 3900h。

UI 收到 `cmd_control` 成功应答（`result=0`）后，仍应以随后到达的 `filter_table` 状态为准关闭弹窗，而不是本地强制改状态。重复下发清除命令是幂等的，不会重复推进节点。

> `confirmMaintenance` 是后端兼容旧版本的别名；新 UI 不应使用它，应统一使用 `clearMaintenanceAlert`。

---

## 5. UI 验收清单

1. 进入刷卡历史页，可显示 `rfidData.rfidHistory` 返回的 JSON 数组；
2. 刷一张卡并收到 `0x4D2` 后，列表新增卡号、时间和时长；重启后记录仍存在；
3. 工时达到保养节点后，提示和“清除”按钮同时出现；
4. 点击“清除”后，提示消失，故障列表不再有 `maintenanceAlert`；
5. 清除后查询 `nextMaintenanceHours`，应为当前总工时之后的下一个 `1200h` 周期节点。
