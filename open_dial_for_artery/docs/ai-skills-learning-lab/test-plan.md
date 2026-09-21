# 验证计划

## 先失败，再实现

为每个 fixture 写一个最小检查：它应在字段尚未实现时因键缺失或值不匹配而失败。不要先修改实现再补验证。

## 验收场景

1. 首次写出状态文件时出现 `startup`。
2. `wait_for_connect → net_connected` 后出现 `data_call_connected`。
3. 既有 SIM 健康检查引发的 `net_connected → roamlink_starting` 出现 `sim_health_check_failed`。
4. `roamlink_starting → roamlink_active` 出现 `roamlink_connected`。
5. 允许回退的 Roamlink 路径进入 SIM 初始化时出现 `roamlink_fallback_to_sim`。
6. 旧键仍存在，新增键不会使简单 INI 读方失败。
7. 输出仍走临时文件再 `rename()`，不得直接覆盖最终状态文件。

## 环境限制

当前仓库未配置单元测试，且交叉编译依赖 OpenNPC SDK。本演练只定义验证证据；正式实现时应根据可用 SDK 运行构建，并选择 host-side 状态写出测试或硬件/SDK mock 方案。
