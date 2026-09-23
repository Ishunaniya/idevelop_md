---
name: project-roamlink-refactor
description: Roamlink 双卡切换重构（问题7/8）已完成，各文件改动位置
metadata:
  type: project
---

2026-05-19 完成。两个问题均已实现并确认无误。

## 问题7：dial_stat_roamlink_starting / roamlink_active 枚举状态

**EC200A**（只改状态文件）：
- `dialer_ec200a.cpp`：`roamlink_last_ping_ts == 0` 时输出 `"starting"`，否则 `"active"`

**EG25**（全重构）：
- `eg25/dial/dial.h`：枚举新增 17=`roamlink_starting`，18=`roamlink_active`；struct 新增 `roamlink_rx_packets`/`roamlink_no_data_timer`
- `eg25/dial/dial.c`：
  - 启动段 / SIM→Roamlink 切换 / reg_timeout_handler 均设 `dial_st = roamlink_starting`
  - 心跳 ping 成功驱动 `starting → active` 状态转移
  - 移除旧心跳 Roamlink 守护块
  - 新增 `case dial_stat_roamlink_starting:` 负责连通超时
  - 新增 `case dial_stat_roamlink_active:` 负责 ping 超时 + 业务层超时
- `status/dial_status.c`：`dial_eg25_stat_name()` 新增 case 17/18

## 问题8：rx_packets 业务层监控（EG25 only，EC200A 不处理）

已确认 EG25 目标硬件 Roamlink 激活后使用 `rmnet_data*` 接口。

- `roamlink/roamlink.h`：新增 `ROAMLINK_NO_DATA_TIMEOUT_SEC 120`
- `eg25/nw/nw.h`：声明 `nw_get_rmnet_rx_packets_sum()`
- `eg25/nw/nw.c`：实现，扫描 `/sys/devices/virtual/net/rmnet_data*/statistics/rx_packets`
- `eg25/dial/dial.c`：
  - `starting → active` 时初始化 rx_packets 基准
  - 心跳每 30s 检测增长，无增长打日志
  - `roamlink_active` case 新增业务层超时：120s 无增长按策略切 SIM 或重启服务
  - 状态文件 `roamlink_rx_packets` 改用实际值

**Why:** EC200A 用 ccinet0（共享接口，无法区分 Roamlink 流量），无法实现业务层监控。

**How to apply:** 若以后需要在 EC200A 上实现类似功能，需先确认 Roamlink 激活后是否出现独立接口。
