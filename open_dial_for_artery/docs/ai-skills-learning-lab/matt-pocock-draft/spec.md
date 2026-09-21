# Spec to publish (演练版)

为 `open_dial` 的 `/tmp/dial_status` 添加只增不破坏兼容性的 `[dial].last_transition_reason`。支持 `startup`、`data_call_connected`、`sim_health_check_failed`、`roamlink_connected` 与 `roamlink_fallback_to_sim` 五个值。字段反映最近一次被纳入范围的实际状态转移，不影响拨号决策。

发布到 issue 跟踪系统前，必须由维护者确认状态文件现有读方可忽略新增键。
