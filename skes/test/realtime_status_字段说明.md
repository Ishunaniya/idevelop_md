# realtime_status.json 字段说明

> 运行时 JSON 缓存，进程内实时维护，不直接落盘。
> 结构：顶层 `statusIcons` / `settings` + `chache` 子对象（包含来自 CAN/GPS/系统的所有状态）。

---

## 顶层 `statusIcons`（状态图标，来自各 SEP 模块汇聚）

| 字段 | 类型 | 来源 / 说明 |
|---|---|---|
| `aeb` | int | AEB 模块连接状态（SEP 心跳汇聚）0=无 1=连接正常 |
| `bsd` | int | BSD 模块状态（SEP 汇聚聚合值）|
| `alarmIndicator` | int | 报警指示灯（各 SEP alarm 触发后置1）|
| `workMode` | int | 工作模式（预留）|
| `seatBeltWarning` | int | 安全带报警等级（SEP alarm 汇聚）|
| `gpsStatus` | int | GPS 定位状态（GPS SEP 上报）0=无信号 1=定位中 3=已定位 |
| `view_nsat` | int | GPS 可见卫星数（GPS SEP 上报）|
| `networkSignal` | int | 4G 信号强度（Tbox/网络 SEP 上报）0-4 |
| `radarStatus` | int | 雷达模块状态（radar SEP 上报）255=未连接 |
| `sound_light_alarm` | int | 声光报警状态（alarm SEP 汇聚）0=无 1=触发 |
| `peoplewalking` | int | 行人检测报警（DMS/AEB SEP 上报）|
| `dms` | int | DMS 驾驶员监测状态（DMS SEP 上报）|
| `Rgear` | int | 倒档指示（CAN0 0x31A 解析）0=非倒档 1=倒档 |

---

## 顶层 `settings`（QT 可写的系统设置，VEH 侧实时生效，暂不持久化）

| 字段 | 类型 | 来源 / 说明 |
|---|---|---|
| `reverseGuideLine` | int | 倒车辅助线开关（QT 下发）0=关 1=开 |
| `warningSound` | int | 报警音量（QT `volume` 命令写入）0-100 |
| `screenBrightness` | int | 屏幕亮度（QT `brightness` 命令写入）0-100 默认50 |

---

## `chache.system`（系统时间/网络，定时更新）

| 字段 | 类型 | 来源 / 说明 |
|---|---|---|
| `systemDate` | string | 系统日期（Linux `localtime()` 每秒刷新）格式 YYYY-MM-DD |
| `systemTime` | string | 系统时间（Linux `localtime()` 每秒刷新）格式 HH:MM:SS |
| `networkSignal` | int | 4G 信号强度（同步自 statusIcons.networkSignal）|

---

## `chache.mainScreen`（主界面显示数据，来自 CAN0 各帧）

| 字段 | 类型 | 来源帧 | 说明 |
|---|---|---|---|
| `ready` | int | CAN0 0x31A BYTE0 bit0 | READY 就绪灯 0=熄灭 1=点亮 |
| `wrench` | int | CAN0 0x31A BYTE0 bit1 | 维修扳手指示灯 |
| `seatBeltWarning` | int | CAN0 0x31A BYTE0 bit2 | 安全带警告 |
| `footBrake` | int | CAN0 0x31A BYTE0 bit3 | 脚制动指示 |
| `seatWarning` | int | CAN0 0x31A BYTE0 bit4 | 坐椅警告 |
| `handBrake` | int | CAN0 0x31A BYTE0 bit5 | 手刹指示 |
| `headLight` | int | CAN0 0x31A BYTE0 bit6 | 大灯状态（CAN 回读）|
| `faultCount` | int | CAN0 0x31A BYTE6 | 故障计数 |
| `cabinFan` | int | CAN0 0x31A（或回读）| 驾驶室风扇状态 |
| `frontLight` | int | CAN0 0x31A | 前工作灯状态 |
| `rearLight` | int | CAN0 0x31A | 后工作灯状态 |
| `forkMonitor` | int | CAN0 0x31A | 货叉监控状态 |
| `oneClickLevel` | int | CAN0 0x31A / 内部脉冲 | 一键水平状态 0=停止 1=运行中 |
| `vehicleSpeed` | double | CAN0 0x31A BYTE1-2 | 车速 单位 km/h |
| `speedMode` | int | CAN0 0x31A / 回读 | 速度模式 0=低速E 1=高速P 2=标准S |
| `batteryLevel` | int | CAN0 0x31A BYTE3 | 电量百分比 0-100 |
| `driveDirection` | int | CAN0 0x31A BYTE4 | 行驶方向 0=前进 1=后退 1000=停止 |
| `forkWeight` | int | CAN0 0x31B | 货叉载重 单位 kg |
| `forkTiltAngle` | int | CAN0 0x31B | 货叉倾斜角度 单位 0.1° |
| `forkHeight` | int | CAN0 0x31A BYTE5 / 0x31B | 货叉高度 单位 mm |
| `steeringAngle` | double | CAN0 0x31B | 驱动轮转角 单位 ° |
| `totalMileage` | double | 持久化 + 运行累计 + Qt修正命令 | 总里程 单位 km；`set_total_mileage` 可强制覆盖并落盘 |
| `singleMileage` | double | 运行累计（本次开机） | 单次里程 单位 km |
| `totalWorkHours` | double | 持久化 + 运行累计 + Qt修正命令 | 总工时 单位 h；`set_total_work_hours` 可强制覆盖并落盘 |
| `singleWorkHours` | double | 运行累计（本次开机） | 单次工时 单位 h |

---

## `chache.internal`（内部逻辑状态，程序内部维护）

| 字段 | 类型 | 来源 / 说明 |
|---|---|---|
| `powerOnSignal` | int | CAN0 0x31A 钥匙ON信号 0=OFF 1=ON |
| `chargingStatus` | int | CAN0 0x31A 充电状态位 |
| `isWorkTimeCounting` | bool | 程序内部：车辆运动时为 true，静止时为 false |
| `passwordVerified` | int | 程序内部：密码验证状态 0=未验证 1=已验证（重启重置）|

---

## `chache.ioStatus`（IO 状态界面，来自 CAN1 各帧）

| 字段 | 类型 | 来源帧 | 说明 |
|---|---|---|---|
| `accelPedalAi1` | int | CAN1 0x32A BYTE1 | 加速踏板 AI1（模拟量） |
| `angleSensorAi1` | int | CAN1 0x32A BYTE2 | 角度传感器 AI1 |
| `brakePedalAi1` | int | CAN1 0x32A BYTE3 | 制动踏板 AI1 |
| `thumbTelescopicAi` | int | CAN1 0x32A BYTE4 | 拇指前移遥控 AI |
| `thumbUpDAi` | int | CAN1 0x32A BYTE5 | 拇指起升遥控 AI |
| `thumbTiltAi` | int | CAN1 0x32A BYTE6 | 拇指倾斜遥控 AI |
| `thumbShiftAi` | int | CAN1 0x32A BYTE7 | 拇指侧移遥控 AI |
| `accelPedalAi2` | int | CAN1 0x32B BYTE1 | 加速踏板 AI2 |
| `angleSensorAi2` | int | CAN1 0x32B BYTE2 | 角度传感器 AI2 |
| `brakePedalAi2` | int | CAN1 0x32B BYTE3 | 制动踏板 AI2 |
| `telescopicPosAi` | int | CAN1 0x32B BYTE4 | 前后移编码器 AI |
| `pressureValveAi` | int | CAN1 0x32B BYTE5 | 液压压力传感器 AI |
| `mastRaiseSwitch` | int | CAN1 0x32C BYTE1 bit0 | 门架上升开关 |
| `mastLowerSwitch` | int | CAN1 0x32C BYTE1 bit1 | 门架下降开关 |
| `attachmentRightSwitch` | int | CAN1 0x32C BYTE1 bit2 | 属具左移到位开关 |
| `attachmentLeftSwitch` | int | CAN1 0x32C BYTE1 bit3 | 属具右移到位开关 |
| `seatBeltSwitch` | int | CAN1 0x32C BYTE1 bit4 | 安全带开关 |
| `opsSeatSwitch` | int | CAN1 0x32C BYTE1 bit5 | 操作员坐椅开关 |
| `brakeSwitch` | int | CAN1 0x32C BYTE1 bit6 | 驻车开关 |
| `attachmentTiltShiftDO` | int | CAN1 0x32C BYTE3 bit0 | 属具倾斜侧移 DO |
| `liftDO` | int | CAN1 0x32C BYTE3 bit1 | 起升 DO |
| `fanDO` | int | CAN1 0x32C BYTE3 bit2 | 风扇 DO |
| `attachmentDO1` | int | CAN1 0x32C BYTE3 bit3 | 属具 DO1 |
| `attachmentDO2` | int | CAN1 0x32C BYTE3 bit4 | 属具 DO2 |
| `strPWM` | int | CAN1 0x32C BYTE4 | 转向阻尼 PWM |
| `teleFPWM` | int | CAN1 0x32C BYTE5 | 前移比例阀 PWM |
| `teleRPWM` | int | CAN1 0x32C BYTE6 | 后移比例阀 PWM |
| `downPWM` | int | CAN1 0x32C BYTE7 | 下降比例阀 PWM |

---

## `chache.faultInfo`（故障与保养提示）

| 字段 | 类型 | 来源 / 说明 |
|---|---|---|
| `faultCount` | int | 当前活跃故障条目数；由 `faultList` 自动汇总 |
| `faultList` | array | 当前故障列表；主动查询返回数组，`filter_table` 推送序列化为 JSON 字符串 |
| `userErr` / `vcuErr` / `driveMcuErr` / `steerMcuErr` / `pumpMcuErr` / `bmsErr` / `bcmErr` | int | 各 CAN 故障源的当前故障码或等级 |
| `maintenanceAlert` | bool | 保养提示是否显示；与 `maintenancePending` 同步 |
| `maintenancePending` | bool | 是否存在待清除的保养提示；为 true 时 UI 显示“清除”按钮 |
| `nextMaintenanceHours` | double | 下一保养固定工时节点（h）：300、1500、2700… |

> 保养提示由总工时触发：首次 300h，后续每 1200h。Qt 用 `clearMaintenanceAlert=1` 清除当前提示；清除后会推进到当前总工时之后的下一个节点。

## `chache.rfidData`（RFID 刷卡信息）

| 字段 | 类型 | 来源 / 说明 |
|---|---|---|
| `heartbeatStatus` | int | CAN1 0x4D1 BYTE0 bit0；0=灯灭，1=灯亮 |
| `workStatus` | int | CAN1 0x4D1 BYTE0 bit1；0=待机，1=工作/已解锁 |
| `faultStatus` | int | CAN1 0x4D1 BYTE0 bit2~4；0=正常，1=解卡失败，2=加卡错误 |
| `unlockCardId` | string | CAN1 0x4D1 BYTE1~4，8 位大写十六进制 UID |
| `sequence` | int | 每收到一帧 CAN1 0x4D1 自增，用于诊断 RFID 帧是否到达 |
| `unlockTime` | string | `workStatus` 0→1 时记录的本地解锁时间 |
| `unlockDuration` | int | CAN1 0x4D2 BYTE4~5 小端值 |
| `rfidHistory` | array | 最近 100 条成功解锁记录；每项含 `cardId`、`unlockTime`、`unlockDuration`；掉电保存 |

> 主动查询路径为 `rfidData.rfidHistory`，返回 JSON 字符串；`filter_table` 推送字段名为 `rfidHistory`。详见 [UI 刷卡历史与保养提示交接](UI_刷卡历史与保养提示交接.md)。

---

## `chache.versionInfo`（版本信息，来自 CAN1）

| 字段 | 类型 | 来源帧 | 说明 |
|---|---|---|---|
| `vcuSoftVersion` | string | CAN1 0x33A BYTE0-7 | VCU 软件版本字符串，如 "V1.0.0" 默认 "---" |

---

## `chache` 中预留空对象（待扩展）

| 字段 | 说明 |
|---|---|
| `runStatus` | 运行状态扩展区（预留）|
| `driveSettings` | 行驶参数扩展区（预留）|
| `pumpSettings` | 泵参数扩展区（预留）|
| `bmsData` | BMS 电池数据扩展区（预留）|
| `rfidData` | RFID 实时状态与最近 100 条解锁历史 |
| `fuseBoxStatus` | 保险盒状态扩展区（预留）|

---

## persistent.json 字段（落盘持久化配置，`/data/forklift_persistent.json`）

> RealtimeStatusManager 与 SkesQtBridge 共享该文件并采用读-合并-原子写方式保存各自字段；实时状态树仅在启动时从该文件恢复相关值。

| 字段 | 类型 | 说明 |
|---|---|---|
| `password` | string | 操作密码（默认 66999）|
| `speedMode` | int | 速度模式（QT 下发，重启恢复到 0x31D BYTE0 bit0-1）|
| `heightStopSetting` | int | 高度定址停止设置（QT 下发，重启恢复到 0x31D BYTE1 bit6）|
| `horizontalStopSetting` | int | 货叉一键水平停止设置（QT 下发，重启恢复到 0x31D BYTE1 bit7）|
| `totalMileage` | double | 总里程累计（RealtimeStatusManager 每 60s 定时写入；`set_total_mileage` 修正命令则立即覆盖落盘）|
| `totalWorkHours` | double | 总工时累计（RealtimeStatusManager 每 60s 定时写入；`set_total_work_hours` 修正命令则立即覆盖落盘）|
| `lastSaveTime` | int | 上次保存时间戳（RealtimeStatusManager 写入）|
| `unlockCardId` / `unlockTime` / `unlockDuration` | string / string / int | 兼容既有“最近一次解锁”字段 |
| `rfidHistory` | array | 最近 100 条成功解锁记录，超出后删除最旧记录 |
| `maintenancePending` | bool | 当前是否有待清除保养提示 |
| `nextMaintenanceHours` | double | 下一保养固定工时节点（h） |
