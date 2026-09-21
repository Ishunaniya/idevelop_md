# Vertical-slice tickets (演练版)

## T1 — 确认外部契约与行为边界

验证状态文件现有格式、读方兼容性和四条状态转移的真实位置；完成后才能修改接口。

## T2 — 实现最小状态原因投影

在状态机数据、`dial_status_t` 和 INI 写出之间接通字段，并仅为规格列出的转移设置固定原因。依赖 T1。

## T3 — 验证与双轴评审

用 fixtures 验证默认、SIM 成功、SIM→Roamlink、Roamlink 成功和 Roamlink→SIM；审查不改变策略/超时，审查输出可安全忽略。依赖 T2。
