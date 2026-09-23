# vehicleDataEngine 接口说明文档

> 基于 rawframe_process 源码（RSM_v4 / skes_v4），记录 CAN 解析、QT 下发命令及 realtime_status.json 完整字段结构。

---

## 一、CAN 通道说明

> ⚠️ 源码内部的 channel 编号与物理总线名称存在映射关系，务必注意区分。

| 需求表名称 | 源码 can_channel | CanCommandServer channel | 物理总线用途 |
|-----------|----------------|--------------------------|------------|
| CAN1 | 0（接收）| 0 → port 26002 | 有接收有下发（0x31A/0x18FD0291/0x585/0x0CFF\* 等接收；0x31D 下发）|
| CAN2 | 1（接收）| 1 → port 26003 | 仅接收（0x32A/0x32B/0x32C/0x32D/0x32E/0x33A）|

> ⚠️ **待确认**：`0x31D` 源码中 `FRAME_0x31D_CHANNEL = 1`，按上表会发到物理 CAN2，但需求表要求发到 CAN1。如有问题，将 `FRAME_0x31D_CHANNEL` 改为 `0` 即可。

---

## 二、0x31D 下发帧结构（CAN1，每 500ms 周期发送）

| 字节 | Bit | 字段名 | 说明 |
|------|-----|--------|------|
| BYTE0 | Bit0~1 | speedMode | 速度模式：00=E经济 01=P高速 10=S标准 |
| BYTE0 | Bit2 | fan | 驾驶室风扇：0=关 1=开 |
| BYTE0 | Bit3 | ptc | PTC加热：0=关 1=开 |
| BYTE0 | Bit4 | headlight | 前大灯：0=关 1=开 |
| BYTE0 | Bit5 | worklight | 后大灯：0=关 1=开 |
| BYTE0 | Bit6 | forkMonitor | 货叉监控：0=关 1=开 |
| BYTE1 | Bit0 | heightPreset1 | 高度定址1 触发脉冲（1000ms自复位）|
| BYTE1 | Bit1 | heightPreset2 | 高度定址2 触发脉冲（1000ms自复位）|
| BYTE1 | Bit2 | heightPreset3 | 高度定址3 触发脉冲（1000ms自复位）|
| BYTE1 | Bit3 | heightPreset4 | 高度定址4 触发脉冲（1000ms自复位）|
| BYTE1 | Bit4 | heightPreset5 | 高度定址5 触发脉冲（1000ms自复位）|
| BYTE1 | Bit5 | horizontalStart | 一键水平启动 触发脉冲（1000ms自复位）|
| BYTE1 | Bit6 | heightStop / height_stop_setting | 高度停止：QT主动发0/1，VEH持久化记忆，无自复位 |
| BYTE1 | Bit7 | horizontalStop / horizontal_stop_setting | 一键水平停止：QT主动发0/1，VEH持久化记忆，无自复位 |

---

## 三、QT 端下发命令列表

QT 通过 SKES 协议向 VEH 发送 JSON 报文，格式：

```json
{
  "version": "1.0",
  "id": "qt-client",
  "ts": 1234567890000,
  "action": "request",
  "type": "properties",
  "data": {
    "type": "cmd_control",
    "properties": [
      { "name": "speedMode", "data": 1 }
    ]
  }
}
```

### 3.1 持久化控制（掉电记忆，写入 0x31D BYTE0）

| name | data 值 | 功能 | 对应 Bit |
|------|---------|------|---------|
| `speedMode` | 0=E / 1=P / 2=S | 速度模式切换 | BYTE0 Bit0~1 |
| `fan` | 0/1 | 驾驶室风扇开关 | BYTE0 Bit2 |
| `ptc` | 0/1 | PTC加热开关 | BYTE0 Bit3 |
| `headlight` | 0/1 | 前大灯开关 | BYTE0 Bit4 |
| `worklight` | 0/1 | 后大灯开关 | BYTE0 Bit5 |
| `forkMonitor` | 0/1 | 货叉监控开关 | BYTE0 Bit6 |

### 3.2 持久化控制（掉电记忆，写入 0x31D BYTE1 Bit6/7，无自复位）

| name | data 值 | 功能 | 对应 Bit |
|------|---------|------|---------|
| `heightStop` | 0/1 | 高度停止 置位/清位 | BYTE1 Bit6 |
| `height_stop_setting` | 0/1 | 同 heightStop（语义等价）| BYTE1 Bit6 |
| `horizontalStop` | 0/1 | 一键水平停止 置位/清位 | BYTE1 Bit7 |
| `horizontal_stop_setting` | 0/1 | 同 horizontalStop（语义等价）| BYTE1 Bit7 |

### 3.3 脉冲控制（1000ms 后自动清零，写入 0x31D BYTE1 Bit0~5）

| name | data 值 | 功能 | 对应 Bit |
|------|---------|------|---------|
| `heightPreset1` | 无需 data | 高度定址1 触发 | BYTE1 Bit0 |
| `heightPreset2` | 无需 data | 高度定址2 触发 | BYTE1 Bit1 |
| `heightPreset3` | 无需 data | 高度定址3 触发 | BYTE1 Bit2 |
| `heightPreset4` | 无需 data | 高度定址4 触发 | BYTE1 Bit3 |
| `heightPreset5` | 无需 data | 高度定址5 触发 | BYTE1 Bit4 |
| `horizontalStart` | 无需 data | 一键水平启动 触发 | BYTE1 Bit5 |

### 3.4 仪表内部命令（不下发 CAN）

| name | data 值 | 功能 |
|------|---------|------|
| `brightness` | 0~100 | 屏幕亮度（不持久化）|
| `volume` | 0~100 | 音量（不持久化）|
| `language` | 0/1 | 语言切换 |

---

## 四、realtime_status.json 完整字段结构

### 4.1 顶层结构

```
realtime_status.json
├── statusIcons        ← nanomsg 订阅（AEB/BSD/DMS/雷达/GPS）写入
├── settings           ← 外部 settings 文件写入
└── chache
    ├── system         ← 仪表系统时间
    ├── mainScreen     ← 主界面显示字段
    ├── internal       ← 内部状态（开机/充电/工时计时）
    ├── ioStatus       ← IO状态 + IO输出界面字段
    ├── faultInfo      ← 故障信息界面字段
    ├── versionInfo    ← 版本信息界面字段
    └── runStatus      ← 运行状态界面字段
```

---

### 4.2 statusIcons（顶层）

| 字段名 | 类型 | 说明 | 数据来源 |
|--------|------|------|---------|
| `bsd` | int | BSD盲区预警状态 | nanomsg 订阅 |
| `Rgear` | int | 倒车档标志 | nanomsg 订阅 |
| `aeb` | int | AEB自动紧急制动状态 | nanomsg 订阅 |
| `dms` | int | DMS驾驶员监控状态 | nanomsg 订阅 |
| `radarStatus` | int | 雷达连接状态 | nanomsg 订阅 |
| `gpsStatus` | int | GPS连接状态 | nanomsg 订阅 |
| `gps` | int | GPS定位状态 | nanomsg 订阅 |
| `location` | int | 定位标志 | nanomsg 订阅 |
| `networkSignal` | int | 4G网络信号强度 0~4格 | nanomsg 订阅 |
| `view_nsat` | int | GPS可见卫星数 | nanomsg 订阅 |
| `sound_light_alarm` | int | 声光报警状态 | nanomsg 订阅 |

---

### 4.3 chache.system

| 字段名 | 类型 | 说明 | 需求 | 数据来源 |
|--------|------|------|------|---------|
| `systemDate` | string | 系统日期，格式 xxxx-xx-xx | 主界面 Seq1 | 仪表系统时间 |
| `systemTime` | string | 系统时间，格式 xx:xx:xx | 主界面 Seq2 | 仪表系统时间 |

---

### 4.4 chache.mainScreen（主界面）

| 字段名 | 类型 | 说明 | 需求 | CAN 来源 |
|--------|------|------|------|---------|
| `vehicleSpeed` | double | 车速，单位 km/h，分辨率 0.1 | Seq18/19 | CAN0 0x31A BYTE0 |
| `ready` | int | READY灯，0=不显示 1=显示 | Seq4 | CAN0 0x31A BYTE1 Bit0 |
| `wrench` | int | 维修扳手图标，0=不显示 1=显示 | Seq10 | CAN0 0x31A BYTE1 Bit1 |
| `seatBeltWarning` | int | 安全带警告，**反逻辑**：Bit=0时警告=1 | Seq7 | CAN0 0x31A BYTE1 Bit2 |
| `footBrake` | int | 脚制动图标，0=不显示 1=显示 | Seq5 | CAN0 0x31A BYTE1 Bit3 |
| `seatWarning` | int | 坐椅警告，**反逻辑**：Bit=0时警告=1 | Seq8 | CAN0 0x31A BYTE1 Bit4 |
| `handBrake` | int | 手刹图标，0=不显示 1=显示 | Seq6 | CAN0 0x31A BYTE1 Bit5 |
| `speedMode` | int | 速度模式，0=E经济 1=P高速 2=S标准 | Seq20 | CAN0 0x31A BYTE1 Bit6~7 |
| `forkWeight` | int | 货叉称重，单位 KG | Seq24 | CAN0 0x31A BYTE2~3 |
| `steeringAngle` | double | 驱动轮转向角度，单位°，分辨率 0.1，偏移 -1800 | Seq27/28 | CAN0 0x31A BYTE4~5 |
| `faultCount` | int | 故障数量，>0 时主界面显示故障图标并闪烁 | Seq11 | CAN0 0x31A BYTE6 |
| `batteryLevel` | int | 电量百分比 0~100 | Seq21/22 | CAN0 0x31A BYTE7 |
| `headLight` | int | 大灯指示，0=关 1=开 | Seq9 | CAN0 0x18FD0291 BYTE3 Bit0 |
| `driveDirection` | int | 行车方向原始值，<1000=左 / =1000=中位N / >1000=右 | Seq23 | CAN0 0x0CFF0008 BYTE4~5 |
| `forkTiltAngle` | int | 货叉倾角，单位°，int8有符号 -90°~+90° | Seq25 | CAN0 0x585 BYTE0 |
| `forkHeight` | int | 货叉高度（数据源待定）| Seq26 | 待定 |
| `totalMileage` | double | 总里程，单位 km，小数点后1位 | Seq29 | 仪表自身计算 |
| `singleMileage` | double | 单次里程，单次开机复位 | Seq30 | 仪表自身计算 |
| `totalWorkHours` | double | 总工时，单位 h，小数点后1位 | Seq33 | 仪表自身计算 |
| `singleWorkHours` | double | 单次工时，单次开机复位 | Seq34 | 仪表自身计算 |
| `cabinFan` | int | 驾驶室风扇状态，0=关 1=开 | Seq12 | QT下发/持久化 → 0x31D BYTE0 Bit2 |
| `frontLight` | int | 前大灯状态，0=关 1=开 | Seq13 | QT下发/持久化 → 0x31D BYTE0 Bit4 |
| `rearLight` | int | 后大灯状态，0=关 1=开 | Seq14 | QT下发/持久化 → 0x31D BYTE0 Bit5 |
| `forkMonitor` | int | 货叉监控状态，0=关 1=开 | Seq16 | QT下发/持久化 → 0x31D BYTE0 Bit6 |
| `oneClickLevel` | int | 一键水平状态，0=停止 1=启动 | Seq17 | QT下发 → 0x31D BYTE1 Bit5/7 |

---

### 4.5 chache.internal（内部状态）

| 字段名 | 类型 | 说明 | CAN 来源 |
|--------|------|------|---------|
| `powerOnSignal` | int | 钥匙ON信号，0=OFF 1=ON，工时计时触发条件 | CAN0 0x18FD0291 BYTE2 Bit0 |
| `chargingStatus` | int | 充电状态，0=未充电 非0=充电中，工时计时排除条件 | CAN0 0x18FD0291 BYTE5 Bit2~3 |
| `isWorkTimeCounting` | bool | 是否正在计工时（powerOn=1 且 charging=0）| 仪表内部逻辑 |
| `passwordVerified` | int | 密码校验结果，0=未验证 1=已验证，**重启复位不持久化** | QT下发密码验证 |

---

### 4.6 chache.ioStatus（IO状态 + IO输出界面）

#### AI 采样值（来自 CAN1，需求表 CAN2）

| 字段名 | 类型 | 说明 | 需求 | CAN 来源 |
|--------|------|------|------|---------|
| `accelPedalAi1` | int | 油门采样值1 | IO状态 1.1 | CAN1 0x32A BYTE1 |
| `angleSensorAi1` | int | 方向盘转向角度采样值1 | IO状态 1.2 | CAN1 0x32A BYTE2 |
| `brakePedalAi1` | int | 刹车踏板采样值1 | IO状态 1.3 | CAN1 0x32A BYTE3 |
| `thumbTelescopicAi` | int | 前后移采样值 | IO状态 1.4 | CAN1 0x32A BYTE4 |
| `thumbUpDAi` | int | 升降采样值 | IO状态 1.5 | CAN1 0x32A BYTE5 |
| `thumbTiltAi` | int | 倾斜采样值 | IO状态 1.6 | CAN1 0x32A BYTE6 |
| `thumbShiftAi` | int | 侧移采样值 | IO状态 1.7 | CAN1 0x32A BYTE7 |
| `accelPedalAi2` | int | 油门采样值2 | IO状态 1.9 | CAN1 0x32B BYTE1 |
| `angleSensorAi2` | int | 方向盘转向角度采样值2 | IO状态 1.10 | CAN1 0x32B BYTE2 |
| `brakePedalAi2` | int | 刹车踏板采样值2 | IO状态 1.11 | CAN1 0x32B BYTE3 |
| `telescopicPosAi` | int | 前后移编码器采样值 | IO状态 1.8 | CAN1 0x32B BYTE4 |
| `pressureValveAi` | int | 压力传感器采样值 | IO状态 1.12 | CAN1 0x32B BYTE5 |

#### DI 开关量（来自 CAN1 0x32C BYTE1）

| 字段名 | 类型 | 说明 | 需求 | CAN 来源 |
|--------|------|------|------|---------|
| `mastRaiseSwitch` | int | 门架上升到位开关，0=断 1=通 | IO状态 1.14 | CAN1 0x32C BYTE1 Bit0 |
| `mastLowerSwitch` | int | 门架下降到位开关，0=断 1=通 | IO状态 1.15 | CAN1 0x32C BYTE1 Bit1 |
| `attachmentRightSwitch` | int | 属具左移到位，0=断 1=通 | IO状态 1.16 | CAN1 0x32C BYTE1 Bit2 |
| `attachmentLeftSwitch` | int | 属具右移到位，0=断 1=通 | IO状态 1.17 | CAN1 0x32C BYTE1 Bit3 |
| `seatBeltSwitch` | int | 安全带开关信号，0=断 1=通 | IO状态 1.18 | CAN1 0x32C BYTE1 Bit4 |
| `opsSeatSwitch` | int | 座椅开关，0=断 1=通 | IO状态 1.19 | CAN1 0x32C BYTE1 Bit5 |
| `brakeSwitch` | int | 驻车开关，0=断 1=通 | IO状态 1.13 | CAN1 0x32C BYTE1 Bit6 |

#### DO 输出开关量（来自 CAN1 0x32C BYTE3）

| 字段名 | 类型 | 说明 | 需求 | CAN 来源 |
|--------|------|------|------|---------|
| `attachmentTiltShiftDO` | int | 倾斜开关控制 | IO输出 13 | CAN1 0x32C BYTE3 Bit0 |
| `liftDO` | int | 起升开关控制 | IO输出 10 | CAN1 0x32C BYTE3 Bit1 |
| `fanDO` | int | 散热风扇控制 | IO输出 14 | CAN1 0x32C BYTE3 Bit2 |
| `attachmentDO1` | int | 左移/前倾开关控制 | IO输出 11 | CAN1 0x32C BYTE3 Bit3 |
| `attachmentDO2` | int | 右移/后倾开关控制 | IO输出 12 | CAN1 0x32C BYTE3 Bit4 |

#### PWM 输出值（来自 CAN1 0x32C BYTE4~7）

| 字段名 | 类型 | 说明 | 需求 | CAN 来源 |
|--------|------|------|------|---------|
| `strPWM` | int | 转向阻尼PWM控制值 | IO输出 4 | CAN1 0x32C BYTE4 |
| `teleFPWM` | int | 前移比例阀PWM控制值 | IO输出 2 | CAN1 0x32C BYTE5 |
| `teleRPWM` | int | 后移比例阀PWM控制值 | IO输出 3 | CAN1 0x32C BYTE6 |
| `downPWM` | int | 下降比例阀PWM控制值 | IO输出 1 | CAN1 0x32C BYTE7 |

#### 电机状态机 & 承载轮PWM（来自 CAN0）

| 字段名 | 类型 | 说明 | 需求 | CAN 来源 |
|--------|------|------|------|---------|
| `strMotorState` | int | 转向电机状态机，Bit0=HMOE Bit1=中位信号 | IO输出 5 | CAN0 0x360 BYTE0 |
| `driveMotorState` | int | 行走1状态机（0=Init 1=Standby 2=TorqueCtrl 3=SpeedCtrl 5=Failure）| IO输出 6 | CAN0 0x0CFF0008 BYTE6 Bit0~3 |
| `pumpMotorState` | int | 油泵1状态机（同行走状态机编码）| IO输出 7 | CAN0 0x0CFF0009 BYTE6 Bit0~3 |
| `leftLoadWheelPWM` | int | 左承载轮PWM信号值 | IO输出 8 | CAN0 0x18FF6E17 BYTE5 |
| `rightLoadWheelPWM` | int | 右承载轮PWM信号值 | IO输出 9 | CAN0 0x18FF6E17 BYTE6 |

---

### 4.7 chache.faultInfo（故障信息界面）

| 字段名 | 类型 | 说明 | 需求 | CAN 来源 |
|--------|------|------|------|---------|
| `userErr` | int | 用户使用提醒故障号，实际显示值=原始值 | 故障信息 类型1 | CAN1 0x32D BYTE1 |
| `codeErr` | int | VCU故障码，实际显示值=原始值 | 故障信息 类型2 | CAN1 0x32E BYTE1 |

---

### 4.8 chache.versionInfo（版本信息界面）

| 字段名 | 类型 | 说明 | CAN 来源 |
|--------|------|------|---------|
| `vcuSoftVersion` | string | VCU软件版本号，ASCII字符串 | CAN1 0x33A BYTE1~3 |

---

### 4.9 chache.rfidData（RFID刷卡信息）

> 数据来源：CAN1（需求表 CAN2）0x4D1 / 0x4D2 帧  
> 推送属性名统一加 `rfid` 前缀，避免与其他分区重名

| 字段名 | 类型 | 说明 | 需求# | CAN 来源 |
|--------|------|------|-------|---------|
| `heartbeatStatus` | int | 心跳状态，0=指示灯灭 1=指示灯亮 | 1 | CAN1 0x4D1 byte0 bit0 |
| `workStatus` | int | 工作状态，0=待机 1=工作 | 2 | CAN1 0x4D1 byte0 bit1 |
| `faultStatus` | int | 故障状态，0=正常 1=解卡失败/IC卡未录入 2=加卡错误/已达最大加卡数 | 3 | CAN1 0x4D1 byte0 bit2~4 |
| `sequence` | int | 序号，仪表自增（每收到 0x4D1 帧 +1，重启归零） | 4 | 仪表自行维护 |
| `unlockCardId` | string | 解锁卡号，4字节 UID 格式化为大写十六进制，如 `"A1B2C3D4"` | 5 | CAN1 0x4D1 byte1~4 |
| `unlockTime` | string | 解卡时间，格式 `YYYY-MM-DD HH:MM:SS`，workStatus 0→1 跳变时仪表自动记录 | 6 | 仪表自行记录 |
| `unlockDuration` | int | 前一次解锁时长，单位秒（小端序） | 7 | CAN1 0x4D2 byte4~5 |

**Push 属性名对照表**（Qt 侧订阅 `/skes/filter` 时使用）：

| JSON 字段名 | Push property_name | data_type |
|------------|-------------------|-----------|
| `heartbeatStatus` | `heartbeatStatus` | int |
| `workStatus` | `workStatus` | int |
| `faultStatus` | `faultStatus` | int |
| `sequence` | `sequence` | int |
| `unlockCardId` | `unlockCardId` | string |
| `unlockTime` | `unlockTime` | string |
| `unlockDuration` | `unlockDuration` | int |

---

### 4.10 chache.runStatus（运行状态界面）

| 字段名 | 类型 | 说明 | 需求 | 偏移/分辨率 | CAN 来源 |
|--------|------|------|------|------------|---------|
| `driveMotorTargetSpeed` | int | 行走电机目标速度，单位 RPM | 3.1 | 偏移 -10000 | CAN0 0x0CFF08EF BYTE3~4 |
| `strMotorTargetSpeed` | int | 转向电机目标速度，单位 RPM | 3.2 | 偏移 -5000 | CAN0 0x359 BYTE4~5 |
| `pumpMotorTargetSpeed` | int | 油泵电机目标速度，单位 RPM | 3.3 | 偏移 -10000 | CAN0 0x0CFF09EF BYTE3~4 |
| `driveMotorTorque` | int | 行走电机扭矩，单位 NM | 3.4 | 偏移 -1023 | CAN0 0x0CFF0008 BYTE2~3 |
| `driveMotorSpeed` | int | 行走电机转速，单位 rpm | 3.5 | 偏移 -32767 | CAN0 0x0CFF0008 BYTE4~5 |
| `driveMotorTemp` | int | 行走电机温度，单位 ℃ | 3.6 | 偏移 -40 | CAN0 0x0CFF0108 BYTE4 |
| `driveIGBTTemp` | int | 行走IGBT温度，单位 ℃ | 3.7 | 偏移 -40 | CAN0 0x0CFF0108 BYTE5 |
| `strMotorCurrent` | double | 转向电机电流，单位 A | 3.8 | ×0.1 偏移 -500 | CAN0 0x359 BYTE0~1 |
| `strMotorSpeed` | int | 转向电机转速，单位 RPM | 3.9 | 偏移 -5000 | CAN0 0x360 BYTE4~5 |
| `pumpMotorTorque` | int | 油泵电机扭矩，单位 NM | 3.12 | 偏移 -1023 | CAN0 0x0CFF0009 BYTE2~3 |
| `pumpMotorSpeed` | int | 油泵电机转速，单位 rpm | 3.13 | 偏移 -32767 | CAN0 0x0CFF0009 BYTE4~5 |
| `pumpMotorTemp` | int | 油泵电机温度，单位 ℃ | 3.14 | 偏移 -40 | CAN0 0x0CFF0109 BYTE4 |
| `pumpIGBTTemp` | int | 油泵IGBT温度，单位 ℃ | 3.15 | 偏移 -40 | CAN0 0x0CFF0109 BYTE5 |

> ⚠️ **待实现**：3.10 转向电机温度、3.11 转向IGBT温度，需求表标注"待定"，CAN 帧未确认，暂未添加。

---

## 五、持久化文件字段说明

文件路径：`/tmp/forklift_persistent.json`

| 字段名 | 负责模块 | 说明 |
|--------|---------|------|
| `totalMileage` | RealtimeStatusManager | 总里程，每 60s 保存 |
| `totalWorkHours` | RealtimeStatusManager | 总工时，每 60s 保存 |
| `lastSaveTime` | RealtimeStatusManager | 上次保存时间戳 |
| `password` | SkesQtBridge | 开机密码，默认 "0000" |
| `speedMode` | SkesQtBridge | 速度模式（0/1/2）|
| `heightStopSetting` | SkesQtBridge | 高度停止使能（0/1）|
| `horizontalStopSetting` | SkesQtBridge | 一键水平停止使能（0/1）|

> `passwordVerified`（密码校验结果）**不持久化**，每次重启复位为 0，需重新验证。

---

## 六、SKES 推送字段汇总

VEH 每 **500ms** 通过 `/skes/filter` 主题向 QT 推送全量状态，包含以下字段（共约 80 个）：

- **系统类**：`systemDate` / `systemTime` / `networkSignal`
- **主界面类**（30个）：`ready` / `wrench` / `seatBeltWarning` / `footBrake` / `seatWarning` / `handBrake` / `headLight` / `faultCount` / `cabinFan` / `frontLight` / `rearLight` / `forkMonitor` / `oneClickLevel` / `vehicleSpeed` / `speedMode` / `batteryLevel` / `driveDirection` / `forkWeight` / `forkTiltAngle` / `forkHeight` / `steeringAngle` / `totalMileage` / `singleMileage` / `totalWorkHours` / `singleWorkHours` / `heightStopSetting` / `horizontalStopSetting`
- **安全感知类**：`aeb` / `bsd` / `dms` / `radarStatus` / `gpsStatus` / `view_nsat` / `Rgear` / `sound_light_alarm`
- **GPS类**：`latitude` / `longitude`
- **内部状态类**：`powerOnSignal` / `chargingStatus` / `isWorkTimeCounting` / `passwordVerified`
- **设置类**：`reverseGuideLine` / `warningSound` / `screenBrightness`
- **IO状态类**（12个AI + 7个DI）：`accelPedalAi1` ~ `pressureValveAi` / `mastRaiseSwitch` ~ `brakeSwitch`
- **IO输出类**（4个DO + 4个PWM + 5个电机）：`liftDO` ~ `fanDO` / `strPWM` ~ `downPWM` / `strMotorState` ~ `rightLoadWheelPWM`
- **故障信息类**：`userErr` / `codeErr`
- **版本信息类**：`vcuSoftVersion`
- **运行状态类**（13个）：`driveMotorTargetSpeed` ~ `pumpIGBTTemp`
- **RFID刷卡类**（7个）：`heartbeatStatus` / `workStatus` / `faultStatus` / `sequence` / `unlockCardId` / `unlockTime` / `unlockDuration`
