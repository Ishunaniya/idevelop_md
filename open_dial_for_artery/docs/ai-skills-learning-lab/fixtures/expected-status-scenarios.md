# 状态文件演练样例

以下是要验证的**关键片段**，不是完整的 `/tmp/dial_status` 文件。

## 首次启动

```ini
[dial]
state=none
last_transition_reason=startup
```

## 物理 SIM 已连接

```ini
[dial]
state=net_connected
channel=SIM
last_transition_reason=data_call_connected
```

## SIM 健康检查失败，正在转向 Roamlink

```ini
[dial]
state=roamlink_starting
channel=ROAMLINK
last_transition_reason=sim_health_check_failed
```

## Roamlink 已连接

```ini
[dial]
state=roamlink_active
channel=ROAMLINK
last_transition_reason=roamlink_connected
```

## Roamlink 回退到 SIM

```ini
[dial]
state=sim_init
channel=SWITCHING
last_transition_reason=roamlink_fallback_to_sim
```
