# skes-dataengine CAN 报文分析：上云 vs 不上云

> 基于对 `skes-dataengine` 全部源文件的直接代码分析。  
> 点表来源：`Pointsheet_info1_SRF.json` / `Pointsheet.xlsx`（两者内容一致）。  
> "上云"判断标准：CAN ID 存在于点表中（stub 会按点表解析并上报至云端）。

---

## 一、上云的 CAN 报文（共 17 个 ID，点表中存在）

| CAN ID | 总线 | 含义 | 来源文件 |
|--------|------|------|---------|
| `0x0CFF0008` | CAN0 | 行走电机扭矩 / 转速 / 状态机 | RealtimeStatusManager.cpp |
| `0x0CFF0009` | CAN0 | 油泵电机扭矩 / 转速 / 状态机 | RealtimeStatusManager.cpp |
| `0x0CFF0108` | CAN0 | 行走电机温度 / 行走 IGBT 温度 | RealtimeStatusManager.cpp |
| `0x0CFF0109` | CAN0 | 油泵电机温度 / 油泵 IGBT 温度 | RealtimeStatusManager.cpp |
| `0x0CFF0208` | CAN0 | 行走 MCU 故障码（BYTE4~5） | RealtimeStatusManager.cpp |
| `0x0CFF0209` | CAN0 | 油泵 MCU 故障码（BYTE4~5） | RealtimeStatusManager.cpp |
| `0x0CFF0308` | CAN0 | 行走 MCU 软件版本（BYTE2） | RealtimeStatusManager.cpp |
| `0x0CFF0309` | CAN0 | 油泵 MCU 软件版本（BYTE2） | RealtimeStatusManager.cpp |
| `0x0CFF030A` | CAN0 | 转向 MCU 软件版本（BYTE2） | RealtimeStatusManager.cpp |
| `0x0CFF08EF` | CAN0 | 行走电机目标速度（BYTE3~4，偏移 -10000，RPM） | RealtimeStatusManager.cpp |
| `0x0CFF09EF` | CAN0 | 油泵电机目标速度（BYTE3~4，偏移 -10000，RPM） | RealtimeStatusManager.cpp |
| `0x0CFF7C03` | CAN0 | BMS 主帧：总电压 / 工作电流 / SOC / 继电器状态 / 充电状态 | RealtimeStatusManager.cpp |
| `0x0CFF7D03` | CAN0 | BMS 单体：最高/最低单体电压、最高/最低单体温度 | RealtimeStatusManager.cpp |
| `0x0CFF7F03` | CAN0 | BMS 绝缘阻值（BYTE6~7，单位 KΩ） | RealtimeStatusManager.cpp |
| `0x0CFF8203` | CAN0 | BMS 软件版本（BYTE4） | RealtimeStatusManager.cpp |
| `0x18FE7C03` | CAN0 | VCU 上电指令（BYTE0 Bit0） | RealtimeStatusManager.cpp |
| `0x31A` | CAN0 | 车辆主状态：车速 / 电量 / 货叉称重 / 转向角 / 故障数量 / 指示灯位 | RealtimeStatusManager.cpp |

> **说明：** 点表中 CAN0 和 CAN1 均覆盖上述同组 ID，stub 两路同时采集。  
> skes-dataengine 仅在代码中明确解析了 CAN0 方向，CAN1 同协议帧由点表统一收录。

---

## 二、不上云的 CAN 报文（skes-dataengine 处理但点表中不存在）

### 2.1 PDO 控制帧（仪表 ↔ VCU 双向控制，非数据采集）

| CAN ID | 总线 | 含义 | 来源文件 |
|--------|------|------|---------|
| `0x111` | CAN0 | 仪表 → VCU PDO 写帧（参数下发） | VcuPdoManager.cpp |
| `0x222` | CAN0 | VCU → 仪表 PDO 应答帧（读写确认） | VcuPdoManager.cpp / CanHandlerFactory.cpp |

### 2.2 仪表主动发送帧（TX，仪表下发给 VCU 的控制命令）

| CAN ID | 总线 | 含义 | 来源文件 |
|--------|------|------|---------|
| `0x31D` | CAN1 | 仪表控制帧：一键调平 / 高度停车 / 软件使能 / 标定等 | SkesQtBridge.cpp / CanTxFrameManager |

### 2.3 IO 状态帧（仪表本地显示用，未收录进点表）

| CAN ID | 总线 | 含义 | 来源文件 |
|--------|------|------|---------|
| `0x32A` | CAN1 | IO 输入(1)：油门 / 方向盘角度 / 刹车 / 前后移 / 升降 / 倾斜 / 侧移采样值 | RealtimeStatusManager.cpp |
| `0x32B` | CAN1 | IO 输入(2)：油门2 / 方向盘2 / 刹车2 / 前后移编码器 / 液压压力 / 货叉高度 | RealtimeStatusManager.cpp |
| `0x32C` | CAN1 | DI 开关（门架到位 / 安全带 / 座椅 / 驻车）+ DO/AO 输出（比例阀 PWM）| RealtimeStatusManager.cpp |

### 2.4 故障码帧（仪表本地展示故障列表，未收录进点表）

| CAN ID | 总线 | 含义 | 来源文件 |
|--------|------|------|---------|
| `0x32D` | CAN1 | 用户提醒故障码（故障类型 1，BYTE1） | RealtimeStatusManager.cpp |
| `0x32E` | CAN1 | VCU 故障码（故障类型 2，BYTE1） | RealtimeStatusManager.cpp |
| `0x360` | CAN0 | 转向 MCU 故障码（故障类型 4，BYTE1）+ 转向电机状态机 / 转速 | RealtimeStatusManager.cpp |

### 2.5 版本 / VIN 帧（仪表版本信息界面，未收录进点表）

| CAN ID | 总线 | 含义 | 来源文件 |
|--------|------|------|---------|
| `0x33A` | CAN0 | VCU 软件版本号（ASCII 字符串，BYTE1~7） | RealtimeStatusManager.cpp |
| `0x33B` | CAN1 | VIN 码第 1~7 位（BYTE0=帧序号 7） | RealtimeStatusManager.cpp |
| `0x33C` | CAN1 | VIN 码第 8~14 位（BYTE0=帧序号 8） | RealtimeStatusManager.cpp |
| `0x33D` | CAN1 | VIN 码第 15~17 位 + VCU 工作时间（BYTE6，单位 H）| RealtimeStatusManager.cpp |

### 2.6 转向电机运行数据（未收录进点表）

| CAN ID | 总线 | 含义 | 来源文件 |
|--------|------|------|---------|
| `0x359` | CAN0 | 转向电机电流（BYTE0~1，×0.1，偏移 -500，A）+ 目标速度（BYTE4~5，偏移 -5000，RPM） | RealtimeStatusManager.cpp |
| `0x360` | CAN0 | 转向电机状态机（BYTE0，HMOE/中位信号）+ 转速（BYTE4~5，偏移 -5000，RPM） | RealtimeStatusManager.cpp |

### 2.7 货叉倾角（未收录进点表）

| CAN ID | 总线 | 含义 | 来源文件 |
|--------|------|------|---------|
| `0x585` | CAN0 | 货叉倾角（BYTE0，int8，-90°~+90°） | RealtimeStatusManager.cpp |

### 2.8 特定厂商 / 特殊功能帧

| CAN ID | 总线 | 含义 | 来源文件 |
|--------|------|------|---------|
| `0x18FF8906` | CAN0 | BSD 盲区检测（BYTE0 Bit7） | RealtimeStatusManager.cpp |
| `0x1C1F383B` | CAN0 | 三一履带吊 BSD（BYTE6 Bit3） | RealtimeStatusManager.cpp |
| `0x16D61B96` | CAN0 | 三一履带吊倒档判断（BYTE0：R1/R2/R3 档） | RealtimeStatusManager.cpp |
| `0x18A120EF` | CAN0 | 德塔机械档位 / 倒车（BYTE0 Bit2） | RealtimeStatusManager.cpp |
| `0x18FD0291` | CAN0 | 保险盒状态 / 钥匙 ON / 大灯 / DI1~7 / DO1~6 | RealtimeStatusManager.cpp |
| `0x18FF6E17` | CAN0 | 左 / 右承载轮 PWM（BYTE5 / BYTE6） | RealtimeStatusManager.cpp |

---

## 三、排除项（代码中出现但非实际 CAN 总线 ID）

| 值 | 出现位置 | 实际含义 |
|----|---------|---------|
| `0x7FFFFFFF` | RealtimeStatusManager.cpp | 29-bit 扩展帧清高位掩码，非 CAN ID |
| `0x80000000` | 通用 | 扩展帧标识位，非 CAN ID |
| `0x0A00` | skes_qt_bridge_tester.cpp 注释 | 故障码示例值，非 CAN ID |
| `0x79A1` | RealtimeStatusManager.cpp 注释 | 大端字节序示例数据，非 CAN ID |
| `0x123` / `0x124` | config_loader.cpp | 硬编码默认占位值（vehicle_speed / engine_temperature），非本项目叉车 CAN ID |

---

## 四、总结

| 类别 | 数量 |
|------|------|
| 上云（在点表中） | 17 个 ID |
| 不上云（仅本地仪表使用） | 21 个 ID |
| 排除（非 CAN ID） | 5 个值 |

**不上云的主要原因：**
- PDO 控制帧（`0x111` / `0x222` / `0x31D`）是仪表与 VCU 的双向控制通道，属于指令而非采集数据
- IO 状态、故障码、VIN、版本帧目前点表未收录，仅在仪表本地展示
- 转向电机运行数据（`0x359` / `0x360`）、货叉倾角（`0x585`）点表未收录
- 特定厂商帧（三一履带吊 BSD、德塔机械档位）为选配件，非标准配置
