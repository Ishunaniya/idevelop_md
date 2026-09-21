# Delta Spec: dial-status transition reason

## ADDED Requirement: expose the most recent state transition reason

`/tmp/dial_status` 的 `[dial]` 节 SHALL 包含 `last_transition_reason` 键。该键值 SHALL 是稳定的小写下划线标识；未知或尚未发生转移时值为 `startup`。

### Scenario: initial startup

**GIVEN** `dial_mng_new()` 已创建状态机且尚未完成任何状态转移

**WHEN** 写出第一次 `/tmp/dial_status`

**THEN** `[dial]` 包含 `last_transition_reason=startup`

### Scenario: SIM data call becomes connected

**GIVEN** 状态机处于 `dial_stat_wait_for_connect`

**WHEN** 数据连接成功并转入 `dial_stat_net_connected`

**THEN** `last_transition_reason` 为 `data_call_connected`

### Scenario: SIM health check triggers Roamlink fallback

**GIVEN** 状态机处于 `dial_stat_net_connected` 且策略允许回退到 Roamlink

**WHEN** 触发既有的 SIM 到 Roamlink 切换并转入 `dial_stat_roamlink_starting`

**THEN** `last_transition_reason` 为 `sim_health_check_failed`

### Scenario: Roamlink becomes usable

**GIVEN** 状态机处于 `dial_stat_roamlink_starting`

**WHEN** 既有 TCP 连通性检测成功并转入 `dial_stat_roamlink_active`

**THEN** `last_transition_reason` 为 `roamlink_connected`

### Scenario: Roamlink fallback triggers SIM re-entry

**GIVEN** 状态机处于 Roamlink 路径且策略允许回退到 SIM

**WHEN** 既有故障/超时逻辑转入 `dial_stat_sim_init` 或 `dial_stat_init`

**THEN** `last_transition_reason` 为 `roamlink_fallback_to_sim`

## Compatibility

新增键不得改变现有键的名称、含义、顺序要求或默认值。读取 `/tmp/dial_status` 的兼容读方可忽略未知键。
