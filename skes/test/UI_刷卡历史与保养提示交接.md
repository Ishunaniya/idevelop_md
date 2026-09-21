# UI 刷卡历史与保养提示交接

更新日期：2026-09-02

## 本次通信行为变更

`/skes/filter` 的 `action=post`、`type=properties`、`data.type=filter_table`
状态包不再每 500ms 重复发送全部字段。

- 每 500ms 构建当前状态并与上一次**成功发送**的状态比较；只发送值或类型发生变化的 `data.data` 项。
- 启动后的首个状态包，以及此后每 5 秒，发送一次完整状态快照。间隔可由 dataengine 车型配置中的 `skes_full_sync_interval_sec` 调整。
- 增量包中未出现的字段表示“值未变化”，UI 必须保留该字段的当前显示值，不能当作 0、空字符串或删除。
- 发送失败时 dataengine 不更新比较基线，下一轮会再次发送未成功交付的变化。

## 刷卡历史

### 状态推送范围

周期状态包仍发送当前刷卡状态：`heartbeatStatus`、`workStatus`、`faultStatus`、`sequence`、`unlockCardId`、`unlockTime`、`unlockDuration`。

`rfidHistory` 不再出现在周期状态包或 5 秒全量快照中。这样刷卡历史增长不会让每个状态包反复携带最多 100 条历史记录。

### 获取历史的接口

刷卡历史页进入（或用户点击刷新）时，UI 通过 `/skes/filter` 发送一次属性查询；`msg_id` 必填且应由 UI 唯一生成：

```json
{
  "version": "1.0",
  "id": "",
  "ts": 0,
  "msg_id": "<unique-id>",
  "action": "request",
  "type": "properties",
  "data": {
    "type": "query",
    "data": [
      {
        "index": -1,
        "property_name": "rfidData.rfidHistory",
        "data_type": "string"
      }
    ]
  }
}
```

响应为 `action=response`、`type=properties`、`data.type=query`，并回显相同的 `msg_id`。`data.data` 中对应项的规则如下：

- `property_name`：`rfidData.rfidHistory`
- `data_type`：`string`
- `value`：JSON 数组序列化后的字符串；UI 需要再执行一次 JSON 解析。
- `result`：`0` 表示成功；非 0 时保留已有列表或显示查询失败状态。

数组元素字段为 `cardId`、`unlockTime`、`unlockDuration`。后端只在 RFID 工作状态发生 `0 → 1` 的真实解锁事件时新增记录，不会把心跳报文当作刷卡记录；历史最多保留最近 100 条，超出时删除最旧项，并且会持久化。

UI 不应再等待或缓存周期状态中的 `rfidHistory`。列表数据应以该查询响应整体替换，而不是与旧列表按位置拼接。

## 保养提示

保养相关字段继续位于周期状态包中，且遵循上方“增量字段缺失即未变化”的规则：

| 字段 | 类型 | 含义 |
| --- | --- | --- |
| `maintenanceAlert` | `int`（0/1） | 是否显示保养提醒 |
| `maintenancePending` | `int`（0/1） | 是否存在尚未确认的保养事项 |
| `nextMaintenanceHours` | `double` | 下一个保养节点工时 |
| `faultList` | `string` | JSON 数组字符串；保养待确认时其中包含 type `8` 的保养故障项 |

后端以 `maintenancePending` 为唯一状态源：首次提醒节点为 300h，确认后按 1200h 间隔计算下一节点；`maintenanceAlert` 与 `faultList` 中的 type `8` 项会与它同步。以上状态会持久化，重启后仍可由首次/定期全量同步恢复。

## UI 验收项

1. 页面初始化后能正确应用首个完整状态包；后续收到部分 `data.data` 时，仅更新其中列出的属性。
2. 刷卡历史页进入和手动刷新均发送一次 `rfidData.rfidHistory` 查询，按 `msg_id` 匹配响应并解析 `value` 字符串。
3. 连续 5 秒无状态变化时，UI 收到的完整状态快照不会重置刷卡历史列表；该列表只由查询响应更新。
4. 保养状态从未提示变为待确认时，UI 能更新三个保养字段并刷新 `faultList`；确认保养后能清除提示并显示新的 `nextMaintenanceHours`。
5. UI 重连或漏掉某个增量包后，最多等待一个全量同步周期（默认 5 秒）即可恢复状态。
