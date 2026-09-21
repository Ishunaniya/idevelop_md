# Context: open_dial 状态诊断演练

| 术语 | 约定含义 |
|---|---|
| 状态机 | `dial_task` 中由 `dial_st` 驱动的拨号生命周期。 |
| 状态转移 | `dial_st` 从一个状态实际赋值为另一个状态的事件；不是每次循环。 |
| 转移原因 | 触发最近一次状态转移的稳定分类标识，不是完整日志。 |
| SIM 通道 | 物理 SIM 的数据拨号路径；已连接状态为 `dial_stat_net_connected`。 |
| Roamlink 通道 | 虚拟 SIM 路径；启动中/已连接状态为 `dial_stat_roamlink_starting`、`dial_stat_roamlink_active`。 |
| 状态文件 | `/tmp/dial_status`，由 `dial_status_write()` 原子替换写出。 |
| 读方 | 读取该 INI 风格状态文件的运维脚本、监控或人工诊断工具。 |

禁止把“网络连接失败”泛化为“应立即切换通道”：具体转移仍由既有 `network_select` 策略和失败计数决定。
