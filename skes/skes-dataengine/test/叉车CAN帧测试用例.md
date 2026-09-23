# 叉车 chache 层 CAN 帧测试用例

> 测试环境：`vehicleDataEngine` 进程运行中，`skes-relay` 运行中
> 测试工具：`skes_qt_bridge_tester`（以下简称 **tester**）
> CAN 注入工具：`cansend`（注意文档 CAN1=源码 CAN0，文档 CAN2=源码 CAN1）
>
> **通用测试步骤**：先开一个终端持续订阅，再开另一个终端注入 CAN 帧
> ```bash
> # 终端1：持续订阅推送（贯穿所有测试）
> ./skes_qt_bridge_tester subscribe
> ```
>
> ⚠️ **数据帧格式**：`cansend <channel> <CANID>#<8字节16进制>`，每帧必须是 16 个十六进制字符（8 字节）
> ⚠️ **扩展帧**：CAN ID 超过 3 位十六进制（> 0x7FF）时，`cansend` 使用 8 位 ID 格式自动识别为扩展帧

---

## 一、mainScreen — 主界面核心状态

### Frame-01  CAN0 0x31A — 主要车辆状态（综合帧）

**协议说明**：

| 字节 | 位域 | 字段 | 分辨率/单位 | JSON 路径 |
|------|------|------|-------------|-----------|
| BYTE0 | — | 车速 | ×0.1 km/h | `mainScreen.vehicleSpeed` |
| BYTE1 Bit0 | — | READY 灯 | 0/1 | `mainScreen.ready` |
| BYTE1 Bit1 | — | 维修扳手 | 0/1 | `mainScreen.wrench` |
| BYTE1 Bit2 | — | 安全带警告（**反逻辑**） | bit=0→警告=1 | `mainScreen.seatBeltWarning` |
| BYTE1 Bit3 | — | 脚制动 | 0/1 | `mainScreen.footBrake` |
| BYTE1 Bit4 | — | 坐椅警告（**反逻辑**） | bit=0→警告=1 | `mainScreen.seatWarning` |
| BYTE1 Bit5 | — | 手刹 | 0/1 | `mainScreen.handBrake` |
| BYTE1 Bit6~7 | — | 速度模式 | 0=E/1=P/2=S | `mainScreen.speedMode` |
| BYTE2~3 | 小端 | 货叉称重 | KG | `mainScreen.forkWeight` |
| BYTE6 | — | 故障数量 | 个 | `mainScreen.faultCount` |
| BYTE7 | — | 电池电量 | % | `mainScreen.batteryLevel` |

**CAN 注入 — 车速测试**：
```bash
# BYTE0=50 → 5.0 km/h，其余字段清零（注意安全带/坐椅反逻辑此时输出警告=1）
cansend can0 031A#32 00 00 00 08 07 00 00

# BYTE0=100 → 10.0 km/h
cansend can0 031A#64 00 00 00 08 07 00 00

# BYTE0=0 → 静止
cansend can0 031A#00 00 00 00 08 07 00 00
```

**CAN 注入 — BYTE1 指示灯测试**：
```bash
# BYTE1=0x01：仅 READY 亮（Bit0=1）
cansend can0 031A#00 01 00 00 08 07 00 00

# BYTE1=0x02：仅 维修扳手（Bit1=1）
cansend can0 031A#00 02 00 00 08 07 00 00

# BYTE1=0x04：安全带反逻辑测试（Bit2=1 → seatBeltWarning=0 不报警）
cansend can0 031A#00 04 00 00 08 07 00 00

# BYTE1=0x08：脚制动踩下（Bit3=1）
cansend can0 031A#00 08 00 00 08 07 00 00

# BYTE1=0x10：坐椅反逻辑测试（Bit4=1 → seatWarning=0 不报警）
cansend can0 031A#00 10 00 00 08 07 00 00

# BYTE1=0x20：手刹拉起（Bit5=1）
cansend can0 031A#00 20 00 00 08 07 00 00

# BYTE1=0x40：速度模式 P 高速（Bit6=1, Bit7=0）
cansend can0 031A#00 40 00 00 08 07 00 00

# BYTE1=0x80：速度模式 S 标准（Bit7=1, Bit6=0）
cansend can0 031A#00 80 00 00 08 07 00 00
```

**CAN 注入 — 货叉称重测试**：
```bash
# BYTE2~3 小端 = 500 KG（0x01F4 → BYTE2=0xF4, BYTE3=0x01）
cansend can0 031A#00 00 F4 01 08 07 00 00
# ↑ 注意下方正确写法（不含空格）：
cansend can0 031A#00 00 F4 01 08 07 00 00

# BYTE2~3 小端 = 1200 KG（0x04B0 → BYTE2=0xB0, BYTE3=0x04）
cansend can0 031A#00 00 B0 04 08 07 00 00

# BYTE2~3 = 0 KG
cansend can0 031A#00 00 00 00 08 07 00 00
```

**CAN 注入 — 故障数量测试**：
```bash
# BYTE6=0：无故障
cansend can0 031A#00 00 00 00 08 07 00 00

# BYTE6=3：3个故障
cansend can0 031A#00 00 00 00 08 07 03 00

# BYTE6=255：最大故障数
cansend can0 031A#00 00 00 00 08 07 03 FF
# ↑ 注意8字节写法：
cansend can0 031A#00 00 00 00 08 07 FF 00
```

> 纠正：BYTE6 是第7个字节，BYTE7 是第8个字节，正确帧格式为：
> `031A# [B0][B1][B2][B3][B4][B5][B6][B7]`

**CAN 注入 — 电池电量测试**：
```bash
# BYTE7=75：75%电量
cansend can0 031A#00 00 00 00 08 07 00 4B

# BYTE7=20：20%低电量
cansend can0 031A#00 00 00 00 08 07 00 14

# BYTE7=100：满电
cansend can0 031A#00 00 00 00 08 07 00 64
```

**tester 逐字段验证**：
```bash
./skes_qt_bridge_tester query mainScreen.vehicleSpeed
./skes_qt_bridge_tester query mainScreen.ready
./skes_qt_bridge_tester query mainScreen.wrench
./skes_qt_bridge_tester query mainScreen.seatBeltWarning
./skes_qt_bridge_tester query mainScreen.footBrake
./skes_qt_bridge_tester query mainScreen.seatWarning
./skes_qt_bridge_tester query mainScreen.handBrake
./skes_qt_bridge_tester query mainScreen.speedMode
./skes_qt_bridge_tester query mainScreen.forkWeight
./skes_qt_bridge_tester query mainScreen.steeringAngle
./skes_qt_bridge_tester query mainScreen.faultCount
./skes_qt_bridge_tester query mainScreen.batteryLevel
```

**预期结果**：

| BYTE1 Bit2（安全带原始）| seatBeltWarning | 含义 |
|------------------------|-----------------|------|
| 0 | 1 | **显示**安全带警告（反逻辑） |
| 1 | 0 | 不显示 |

| BYTE1 Bit4（坐椅原始）| seatWarning | 含义 |
|-----------------------|------------|------|
| 0 | 1 | **显示**坐椅警告（反逻辑） |
| 1 | 0 | 不显示 |

| BYTE1 Bit6~7 | speedMode | 显示 |
|-------------|----------|------|
| 00 | 0 | E 乌龟（经济档）|
| 01 | 1 | P 兔子（高速档）|
| 10 | 2 | S 标准档 |

---

### Frame-02  CAN0 0x18FD0291 — 保险盒状态（故障码 / DI / DO / 比例输出 / BCM版本）

**帧结构**（29位扩展帧，8字节）：

| 字节 | 字段 | 说明 |
|------|------|------|
| BYTE0 | 故障循环码 | 保险/继电器故障循环上报，0=全部正常 |
| BYTE1 | — | 未解析，填 0x00 |
| BYTE2 | DI1~7 + 心跳 | Bit0~6=DI1~7，Bit7=心跳 |
| BYTE3 | DO1~6 + 大灯 | Bit0=DO1（同时驱动 headLightStatus），Bit1~5=DO2~6 |
| BYTE4 | — | 未解析，填 0x00 |
| BYTE5 | C11 比例 | 0~255 |
| BYTE6 | C12 比例 | 0~255 |
| BYTE7 | BCM 软件版本 | U8 整数 |

**协议说明**：

| 字节 | 位域 | 字段 | JSON 路径 |
|------|------|------|-----------|
| BYTE0 | — | 故障循环码 | `fuseBoxStatus.fuseFaultCode` / `fuseBoxStatus.activeFaultCodes` / `fuseBoxStatus.k5~k12` / `fuseBoxStatus.f5~f24` |
| BYTE2 Bit0 | — | DI1（钥匙ON）| `fuseBoxStatus.di1` / `internal.powerOnSignal` |
| BYTE2 Bit1~6 | — | DI2~DI7 | `fuseBoxStatus.di2~di7` |
| BYTE2 Bit7 | — | 心跳 | `fuseBoxStatus.heartbeat` |
| BYTE3 Bit0 | — | DO1 + 大灯反馈 | `fuseBoxStatus.do1` / `mainScreen.headLightStatus` |
| BYTE3 Bit1~5 | — | DO2~DO6 | `fuseBoxStatus.do2~do6` |
| BYTE5 | — | C11 输出比例 | `fuseBoxStatus.c11Ratio` |
| BYTE6 | — | C12 输出比例 | `fuseBoxStatus.c12Ratio` |
| BYTE7 | — | BCM 软件版本 | `versionInfo.bcmSoftVersion` |

> ⚠️ **注意事项**：
> 1. `0x18FD0291` 是 29 位扩展帧，`cansend` 自动识别（ID > 0x7FF）
> 2. BYTE0 为循环故障码机制：停发超过 5 秒则对应故障自动恢复，测试时需持续发帧或在查询前快速连发
> 3. BYTE1 / BYTE4 代码中未解析，填 `0x00` 即可
> 4. 帧至少需要 4 字节才触发解析，BYTE5~7 需对应字节存在时才解析

---

**一、BYTE0 — 保险 / 继电器故障循环码**

```bash
# ── K 继电器单故障 ──────────────────────────────────────
# K5 故障（BYTE0=5，持续发，间隔 <5s）
for i in $(seq 1 5); do cansend can0 18FD0291#05 00 00 00 00 00 00 00; sleep 0.2; done
./skes_qt_bridge_tester query fuseBoxStatus.fuseFaultCode    # → 5
./skes_qt_bridge_tester query fuseBoxStatus.k5               # → true
./skes_qt_bridge_tester query fuseBoxStatus.activeFaultCodes # → [5]

# K7 故障（BYTE0=7）
for i in $(seq 1 5); do cansend can0 18FD0291#07 00 00 00 00 00 00 00; sleep 0.2; done
./skes_qt_bridge_tester query fuseBoxStatus.k7               # → true

# K9 故障（BYTE0=9）
for i in $(seq 1 5); do cansend can0 18FD0291#09 00 00 00 00 00 00 00; sleep 0.2; done
./skes_qt_bridge_tester query fuseBoxStatus.k9               # → true

# K10 故障（BYTE0=10=0x0A）
for i in $(seq 1 5); do cansend can0 18FD0291#0A 00 00 00 00 00 00 00; sleep 0.2; done
./skes_qt_bridge_tester query fuseBoxStatus.k10              # → true

# K11 故障（BYTE0=11=0x0B）
for i in $(seq 1 5); do cansend can0 18FD0291#0B 00 00 00 00 00 00 00; sleep 0.2; done
./skes_qt_bridge_tester query fuseBoxStatus.k11              # → true

# K12 故障（BYTE0=12=0x0C）
for i in $(seq 1 5); do cansend can0 18FD0291#0C 00 00 00 00 00 00 00; sleep 0.2; done
./skes_qt_bridge_tester query fuseBoxStatus.k12              # → true

# ── F 保险丝单故障（F6~F24，不与 K 重叠部分）─────────────
# F6 故障（BYTE0=6）
for i in $(seq 1 5); do cansend can0 18FD0291#06 00 00 00 00 00 00 00; sleep 0.2; done
./skes_qt_bridge_tester query fuseBoxStatus.f6               # → true

# F8 故障（BYTE0=8）
for i in $(seq 1 5); do cansend can0 18FD0291#08 00 00 00 00 00 00 00; sleep 0.2; done
./skes_qt_bridge_tester query fuseBoxStatus.f8               # → true

# F13 故障（BYTE0=13=0x0D）
for i in $(seq 1 5); do cansend can0 18FD0291#0D 00 00 00 00 00 00 00; sleep 0.2; done
./skes_qt_bridge_tester query fuseBoxStatus.f13              # → true

# F24 故障（BYTE0=24=0x18，最大编号）
for i in $(seq 1 5); do cansend can0 18FD0291#18 00 00 00 00 00 00 00; sleep 0.2; done
./skes_qt_bridge_tester query fuseBoxStatus.f24              # → true

# ── F 与 K 共享编号（同一码号同时触发 kN 和 fN）──────────
# F5+K5 同时故障（BYTE0=5 在 K 和 F 两个集合中都有）
for i in $(seq 1 5); do cansend can0 18FD0291#05 00 00 00 00 00 00 00; sleep 0.2; done
./skes_qt_bridge_tester query fuseBoxStatus.k5               # → true
./skes_qt_bridge_tester query fuseBoxStatus.f5               # → true

# ── 多故障循环（K9 + K12 轮流发）────────────────────────
for i in $(seq 1 10); do
  cansend can0 18FD0291#09 00 00 00 00 00 00 00; sleep 0.1
  cansend can0 18FD0291#0C 00 00 00 00 00 00 00; sleep 0.1
done
./skes_qt_bridge_tester query fuseBoxStatus.k9               # → true
./skes_qt_bridge_tester query fuseBoxStatus.k12              # → true
./skes_qt_bridge_tester query fuseBoxStatus.k5               # → false（未注入，正常）
./skes_qt_bridge_tester query fuseBoxStatus.activeFaultCodes # → [9,12]
./skes_qt_bridge_tester query fuseBoxStatus.fuseFaultCode    # → 9 或 12（最后收到的帧值）

# ── 所有故障立即清空（BYTE0=0）────────────────────────
cansend can0 18FD0291#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query fuseBoxStatus.activeFaultCodes # → []
./skes_qt_bridge_tester query fuseBoxStatus.k9               # → false
./skes_qt_bridge_tester query fuseBoxStatus.k12              # → false

# ── 单个故障超时自动恢复（5 秒超时机制）──────────────────
# 先注入 K9+K12，再只持续发 K9，停发 K12 → K12 约 5 秒后自动淘汰
for i in $(seq 1 5); do
  cansend can0 18FD0291#09 00 00 00 00 00 00 00; sleep 0.1
  cansend can0 18FD0291#0C 00 00 00 00 00 00 00; sleep 0.1
done
# 从此只发 K9，停发 K12
for i in $(seq 1 60); do cansend can0 18FD0291#09 00 00 00 00 00 00 00; sleep 0.1; done
./skes_qt_bridge_tester query fuseBoxStatus.k12              # → false（超时已淘汰）
./skes_qt_bridge_tester query fuseBoxStatus.k9               # → true（仍活跃）
./skes_qt_bridge_tester query fuseBoxStatus.activeFaultCodes # → [9]
```

---

**二、BYTE2 — DI1~DI7 数字输入 + 心跳**

```bash
# DI1=1（Bit0，钥匙ON，BYTE2=0x01）
cansend can0 18FD0291#00 00 01 00 00 00 00 00
./skes_qt_bridge_tester query fuseBoxStatus.di1             # → 1
./skes_qt_bridge_tester query internal.powerOnSignal        # → 1（同步写入）

# DI2=1（Bit1，BYTE2=0x02）
cansend can0 18FD0291#00 00 02 00 00 00 00 00
./skes_qt_bridge_tester query fuseBoxStatus.di2             # → 1

# DI3=1（Bit2，BYTE2=0x04）
cansend can0 18FD0291#00 00 04 00 00 00 00 00
./skes_qt_bridge_tester query fuseBoxStatus.di3             # → 1

# DI4=1（Bit3，BYTE2=0x08）
cansend can0 18FD0291#00 00 08 00 00 00 00 00
./skes_qt_bridge_tester query fuseBoxStatus.di4             # → 1

# DI5=1（Bit4，BYTE2=0x10）
cansend can0 18FD0291#00 00 10 00 00 00 00 00
./skes_qt_bridge_tester query fuseBoxStatus.di5             # → 1

# DI6=1（Bit5，BYTE2=0x20）
cansend can0 18FD0291#00 00 20 00 00 00 00 00
./skes_qt_bridge_tester query fuseBoxStatus.di6             # → 1

# DI7=1（Bit6，BYTE2=0x40）
cansend can0 18FD0291#00 00 40 00 00 00 00 00
./skes_qt_bridge_tester query fuseBoxStatus.di7             # → 1

# 心跳=1（Bit7，BYTE2=0x80）
cansend can0 18FD0291#00 00 80 00 00 00 00 00
./skes_qt_bridge_tester query fuseBoxStatus.heartbeat       # → 1

# 全部同时置1（BYTE2=0xFF）
cansend can0 18FD0291#00 00 FF 00 00 00 00 00
./skes_qt_bridge_tester query fuseBoxStatus.di1             # → 1
./skes_qt_bridge_tester query fuseBoxStatus.di7             # → 1
./skes_qt_bridge_tester query fuseBoxStatus.heartbeat       # → 1

# 全部清零（BYTE2=0x00）
cansend can0 18FD0291#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query fuseBoxStatus.di1             # → 0
./skes_qt_bridge_tester query fuseBoxStatus.heartbeat       # → 0
```

---

**三、BYTE3 — DO1~DO6 数字输出 + 大灯反馈**

```bash
# DO1=1 + 大灯ON（Bit0，BYTE3=0x01）
cansend can0 18FD0291#00 00 00 01 00 00 00 00
./skes_qt_bridge_tester query fuseBoxStatus.do1             # → 1
./skes_qt_bridge_tester query mainScreen.headLightStatus    # → 1（Seq9 大灯指示）

# DO2=1（Bit1，BYTE3=0x02）
cansend can0 18FD0291#00 00 00 02 00 00 00 00
./skes_qt_bridge_tester query fuseBoxStatus.do2             # → 1

# DO3=1（Bit2，BYTE3=0x04）
cansend can0 18FD0291#00 00 00 04 00 00 00 00
./skes_qt_bridge_tester query fuseBoxStatus.do3             # → 1

# DO4=1（Bit3，BYTE3=0x08）
cansend can0 18FD0291#00 00 00 08 00 00 00 00
./skes_qt_bridge_tester query fuseBoxStatus.do4             # → 1

# DO5=1（Bit4，BYTE3=0x10）
cansend can0 18FD0291#00 00 00 10 00 00 00 00
./skes_qt_bridge_tester query fuseBoxStatus.do5             # → 1

# DO6=1（Bit5，BYTE3=0x20）
cansend can0 18FD0291#00 00 00 20 00 00 00 00
./skes_qt_bridge_tester query fuseBoxStatus.do6             # → 1

# DO1~DO6 全部置1（BYTE3=0x3F），大灯ON
cansend can0 18FD0291#00 00 00 3F 00 00 00 00
./skes_qt_bridge_tester query fuseBoxStatus.do1             # → 1
./skes_qt_bridge_tester query fuseBoxStatus.do6             # → 1
./skes_qt_bridge_tester query mainScreen.headLightStatus    # → 1

# 大灯OFF（DO1=0，BYTE3=0x00）
cansend can0 18FD0291#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query mainScreen.headLightStatus    # → 0
```

---

**四、BYTE5 — C11 输出比例**

```bash
# C11=0%（BYTE5=0x00）
cansend can0 18FD0291#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query fuseBoxStatus.c11Ratio        # → 0

# C11=50%（BYTE5=0x32）
cansend can0 18FD0291#00 00 00 00 00 32 00 00
./skes_qt_bridge_tester query fuseBoxStatus.c11Ratio        # → 50

# C11=100%（BYTE5=0x64）
cansend can0 18FD0291#00 00 00 00 00 64 00 00
./skes_qt_bridge_tester query fuseBoxStatus.c11Ratio        # → 100

# C11=最大（BYTE5=0xFF）
cansend can0 18FD0291#00 00 00 00 00 FF 00 00
./skes_qt_bridge_tester query fuseBoxStatus.c11Ratio        # → 255
```

---

**五、BYTE6 — C12 输出比例**

```bash
# C12=0%（BYTE6=0x00）
cansend can0 18FD0291#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query fuseBoxStatus.c12Ratio        # → 0

# C12=50%（BYTE6=0x32）
cansend can0 18FD0291#00 00 00 00 00 00 32 00
./skes_qt_bridge_tester query fuseBoxStatus.c12Ratio        # → 50

# C12=100%（BYTE6=0x64）
cansend can0 18FD0291#00 00 00 00 00 00 64 00
./skes_qt_bridge_tester query fuseBoxStatus.c12Ratio        # → 100

# C12=最大（BYTE6=0xFF）
cansend can0 18FD0291#00 00 00 00 00 00 FF 00
./skes_qt_bridge_tester query fuseBoxStatus.c12Ratio        # → 255
```

---

**六、BYTE7 — BCM 软件版本**

```bash
# BCM版本=0（BYTE7=0x00）
cansend can0 18FD0291#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query versionInfo.bcmSoftVersion    # → 0

# BCM版本=1（BYTE7=0x01）
cansend can0 18FD0291#00 00 00 00 00 00 00 01
./skes_qt_bridge_tester query versionInfo.bcmSoftVersion    # → 1

# BCM版本=42（BYTE7=0x2A）
cansend can0 18FD0291#00 00 00 00 00 00 00 2A
./skes_qt_bridge_tester query versionInfo.bcmSoftVersion    # → 42

# BCM版本=255（BYTE7=0xFF）
cansend can0 18FD0291#00 00 00 00 00 00 00 FF
./skes_qt_bridge_tester query versionInfo.bcmSoftVersion    # → 255
```

---

**七、综合联动测试**

```bash
# 全字段同时注入：K9故障 + 钥匙ON(DI1) + 大灯ON(DO1) + C11=80%(0x50) + C12=60%(0x3C) + BCM版本=3
cansend can0 18FD0291#09 00 01 01 00 50 3C 03
./skes_qt_bridge_tester query fuseBoxStatus.fuseFaultCode   # → 9
./skes_qt_bridge_tester query fuseBoxStatus.k9              # → true
./skes_qt_bridge_tester query fuseBoxStatus.di1             # → 1
./skes_qt_bridge_tester query internal.powerOnSignal        # → 1
./skes_qt_bridge_tester query fuseBoxStatus.do1             # → 1
./skes_qt_bridge_tester query mainScreen.headLightStatus    # → 1
./skes_qt_bridge_tester query fuseBoxStatus.c11Ratio        # → 80
./skes_qt_bridge_tester query fuseBoxStatus.c12Ratio        # → 60
./skes_qt_bridge_tester query versionInfo.bcmSoftVersion    # → 3
```

**tester 逐字段验证**：
```bash
./skes_qt_bridge_tester query fuseBoxStatus.fuseFaultCode
./skes_qt_bridge_tester query fuseBoxStatus.activeFaultCodes
./skes_qt_bridge_tester query fuseBoxStatus.k5
./skes_qt_bridge_tester query fuseBoxStatus.di1
./skes_qt_bridge_tester query fuseBoxStatus.di7
./skes_qt_bridge_tester query fuseBoxStatus.heartbeat
./skes_qt_bridge_tester query fuseBoxStatus.do1
./skes_qt_bridge_tester query fuseBoxStatus.do6
./skes_qt_bridge_tester query mainScreen.headLightStatus
./skes_qt_bridge_tester query fuseBoxStatus.c11Ratio
./skes_qt_bridge_tester query fuseBoxStatus.c12Ratio
./skes_qt_bridge_tester query versionInfo.bcmSoftVersion
./skes_qt_bridge_tester query internal.powerOnSignal
```

---

### Frame-03  CAN0 0x585 — 货叉倾角

**协议说明**：

| 字节 | 类型 | 字段 | 单位 | JSON 路径 |
|------|------|------|------|-----------|
| BYTE0 | int8_t 有符号 | 货叉倾角 | ° | `mainScreen.forkTiltAngle` |

**CAN 注入**：
```bash
# BYTE0=0x1E（+30°，前倾）
cansend can0 585#1E 00 00 00 00 00 00 00

# BYTE0=0xE2（-30°，后倾，补码：0xE2 = -30）
cansend can0 585#E2 00 00 00 00 00 00 00

# BYTE0=0x00（水平 0°）
cansend can0 585#00 00 00 00 00 00 00 00

# BYTE0=0x5A（+90°，最大前倾）
cansend can0 585#5A 00 00 00 00 00 00 00

# BYTE0=0xA6（-90°，最大后倾，补码：0xA6 = -90）
cansend can0 585#A6 00 00 00 00 00 00 00
```

**tester 验证**：
```bash
./skes_qt_bridge_tester query mainScreen.forkTiltAngle
```

**预期结果**：正值前倾，负值后倾，范围 -90°~+90°

---

### Frame-04  CAN0 0x0CFF0008 — 行车方向

**协议说明**：

| 字节 | 字节序 | 字段 | 说明 | JSON 路径 |
|------|--------|------|------|-----------|
| BYTE4~5 | 小端 | driveDirection | <1000=左/=1000=N/> 1000=右 | `mainScreen.driveDirection` |

> 注：此帧同时写入 runStatus 电机数据，详见 Frame-08

**CAN 注入**：
```bash
# BYTE4~5 小端 = 500（0x01F4 → BYTE4=0xF4, BYTE5=0x01）→ 左箭头（倒车）
# BYTE2~3 扭矩零点：raw=1023=0x03FF（BYTE2=0xFF, BYTE3=0x03）
cansend can0 0CFF0008#00 00 FF 03 F4 01 00 00

# BYTE4~5 小端 = 1000（0x03E8 → BYTE4=0xE8, BYTE5=0x03）→ N 中位
cansend can0 0CFF0008#00 00 FF 03 E8 03 00 00

# BYTE4~5 小端 = 1500（0x05DC → BYTE4=0xDC, BYTE5=0x05）→ 右箭头（前进）
cansend can0 0CFF0008#00 00 FF 03 DC 05 00 00
```

**tester 验证**：
```bash
./skes_qt_bridge_tester query mainScreen.driveDirection
```

**预期结果**：

| driveDirection | 显示 |
|----------------|------|
| < 1000 | 左箭头（倒车方向）|
| = 1000 | N 档中位 |
| > 1000 | 右箭头（前进方向）|

---

## 二、internal — 内部工况状态

### Frame-05  CAN0 0x0CFF7C03 — 充电状态

**协议说明**：

| 字节 | 位域 | 字段 | 说明 | JSON 路径 |
|------|------|------|------|-----------|
| BYTE5 Bit2~3 | — | chargingStatus | 0=未充/1=充中/2=完成/3=预留 | `internal.chargingStatus` |
| （联动）| — | isWorkTimeCounting | powerOn=1且charging=0时为true | `internal.isWorkTimeCounting` |

**CAN 注入**：
```bash
# 未充电（BYTE5 Bit2~3=00 → BYTE5=0x00）
cansend can0 0CFF7C03#00 00 00 00 00 00 00 00

# 充电中（BYTE5 Bit2~3=01 → BYTE5=0x04）
cansend can0 0CFF7C03#00 00 00 00 00 04 00 00

# 充电结束（BYTE5 Bit2~3=10 → BYTE5=0x08）
cansend can0 0CFF7C03#00 00 00 00 00 08 00 00
```

**tester 验证**：
```bash
./skes_qt_bridge_tester query internal.chargingStatus
./skes_qt_bridge_tester query internal.isWorkTimeCounting
```

**工时计时联动测试**：
```bash
# 第一步：钥匙 ON（先发 0x18FD0291）
cansend can0 18FD0291#00 00 01 00 00 00 00 00

# 第二步：确认未充电（发 0x0CFF7C03 BYTE5=0x00）
cansend can0 0CFF7C03#00 00 00 00 00 00 00 00

# 预期：isWorkTimeCounting=true（工时计时中）
./skes_qt_bridge_tester query internal.isWorkTimeCounting

# 第三步：接入充电
cansend can0 0CFF7C03#00 00 00 00 00 04 00 00

# 预期：isWorkTimeCounting=false（充电暂停工时）
./skes_qt_bridge_tester query internal.isWorkTimeCounting
```

**预期结果**：

| powerOnSignal | chargingStatus | isWorkTimeCounting |
|--------------|----------------|--------------------|
| 0 | 0 | false |
| 1 | 0 | **true**（计时中）|
| 1 | 1 | false（充电暂停）|
| 0 | 1 | false |

---

## 三、runStatus — 电机运行状态

### Frame-06  CAN0 0x359 — 转向电机电流 / 目标速度

**协议说明**：

| 字节 | 字节序 | 字段 | 公式 | 单位 | JSON 路径 |
|------|--------|------|------|------|-----------|
| BYTE0~1 | 小端 | strMotorCurrent | raw×0.1 - 500 | A | `runStatus.strMotorCurrent` |
| BYTE4~5 | 小端 | strMotorTargetSpeed | raw - 5000 | RPM | `runStatus.strMotorTargetSpeed` |

**CAN 注入**：
```bash
# 电流=0A（raw=5000=0x1388, BYTE0=0x88, BYTE1=0x13），目标速度=0RPM（raw=5000, BYTE4=0x88, BYTE5=0x13）
cansend can0 359#88 13 00 00 88 13 00 00

# 电流=+10A（raw=5100=0x13EC, BYTE0=0xEC, BYTE1=0x13），目标速度=+300RPM（raw=5300=0x14B4, BYTE4=0xB4, BYTE5=0x14）
cansend can0 359#EC 13 00 00 B4 14 00 00
cansend can0 359#EC 13 00 00 B4 14 00 00

# 电流=-10A（raw=4900=0x1324, BYTE0=0x24, BYTE1=0x13），目标速度=-300RPM（raw=4700=0x125C, BYTE4=0x5C, BYTE5=0x12）
cansend can0 359#24 13 00 00 5C 12 00 00
cansend can0 359#24 13 00 00 5C 12 00 00
```

**tester 验证**：
```bash
./skes_qt_bridge_tester query runStatus.strMotorCurrent
./skes_qt_bridge_tester query runStatus.strMotorTargetSpeed
```

---

### Frame-07  CAN0 0x360 — 转向电机状态机 / 实际转速

**协议说明**：

| 字节 | 位域 | 字段 | 说明 | JSON 路径 |
|------|------|------|------|-----------|
| BYTE0 Bit0 | — | strMotorState（HMOE位）| 回零完成标志 | `ioStatus.strMotorState` |
| BYTE0 Bit1 | — | strMotorState（中位信号）| 中位到位 | `ioStatus.strMotorState` |
| BYTE1 | — | steerMcuErr（转向MCU故障码）| 0=正常，非0=故障码 | `faultInfo.steerMcuErr` |
| BYTE2~3 | 小端 | steeringAngle | (raw-1200)×0.1°，° | `mainScreen.steeringAngle` |
| BYTE4~5 | 小端 | strMotorSpeed | raw - 5000，RPM | `runStatus.strMotorSpeed` |

**CAN 注入 — 状态机 / 转速测试**：
```bash
# BYTE0=0x01（HMOE=1，中位=0），转速=0RPM（raw=5000=0x1388, BYTE4=0x88, BYTE5=0x13），转向角=0°（raw=1200=0x04B0, BYTE2=0xB0, BYTE3=0x04）
cansend can0 360#01 00 B0 04 88 13 00 00

# BYTE0=0x03（HMOE=1，中位=1），转速=+500RPM（raw=5500=0x157C, BYTE4=0x7C, BYTE5=0x15）
cansend can0 360#03 00 B0 04 7C 15 00 00

# BYTE0=0x02（HMOE=0，中位=1），转速=-500RPM（raw=4500=0x1194, BYTE4=0x94, BYTE5=0x11）
cansend can0 360#02 00 B0 04 94 11 00 00

# BYTE0=0x00（均为0），转速=0RPM
cansend can0 360#00 00 B0 04 88 13 00 00
```

**CAN 注入 — 驱动轮转向角测试**：
```bash
# BYTE2~3 小端 = 1200（0x04B0）→ (1200-1200)×0.1 = 0.0°（正前方）
# BYTE0=0x03 保持HMOE+中位，转速=0RPM（BYTE4=0x88, BYTE5=0x13）
cansend can0 360#03 00 B0 04 88 13 00 00

# BYTE2~3 小端 = 1300（0x0514 → BYTE2=0x14, BYTE3=0x05）→ (1300-1200)×0.1 = +10.0°
cansend can0 360#03 00 14 05 88 13 00 00

# BYTE2~3 小端 = 1100（0x044C → BYTE2=0x4C, BYTE3=0x04）→ (1100-1200)×0.1 = -10.0°
cansend can0 360#03 00 4C 04 88 13 00 00

# BYTE2~3 小端 = 2100（0x0834 → BYTE2=0x34, BYTE3=0x08）→ (2100-1200)×0.1 = +90.0°（最大右转）
cansend can0 360#03 00 34 08 88 13 00 00

# BYTE2~3 小端 = 300（0x012C → BYTE2=0x2C, BYTE3=0x01）→ (300-1200)×0.1 = -90.0°（最大左转）
cansend can0 360#03 00 2C 01 88 13 00 00
```

**CAN 注入 — 转向MCU故障码（BYTE1）**：
```bash
# BYTE1=0：无故障（正常运行，BYTE0=0x03 保持HMOE+中位，BYTE2~3=中位角，BYTE4~5=0RPM）
cansend can0 360#03 00 B0 04 88 13 00 00
./skes_qt_bridge_tester query faultInfo.steerMcuErr      # → 0

# BYTE1=1：故障码1（BYTE0/BYTE2~5 同上）
cansend can0 360#03 01 B0 04 88 13 00 00
./skes_qt_bridge_tester query faultInfo.steerMcuErr      # → 1

# BYTE1=5：故障码5
cansend can0 360#03 05 B0 04 88 13 00 00
./skes_qt_bridge_tester query faultInfo.steerMcuErr      # → 5

# BYTE1=0：恢复正常
cansend can0 360#03 00 B0 04 88 13 00 00
./skes_qt_bridge_tester query faultInfo.steerMcuErr      # → 0
```

**tester 验证**：
```bash
./skes_qt_bridge_tester query ioStatus.strMotorState
./skes_qt_bridge_tester query runStatus.strMotorSpeed
./skes_qt_bridge_tester query mainScreen.steeringAngle
./skes_qt_bridge_tester query faultInfo.steerMcuErr
```

---

### Frame-08  CAN0 0x0CFF0008 — 行走电机扭矩 / 转速 / 状态机

**协议说明**（与行车方向共帧）：

| 字节 | 字节序 | 字段 | 公式 | 单位 | JSON 路径 |
|------|--------|------|------|------|-----------|
| BYTE2~3 | 小端 | driveMotorTorque | raw - 1023 | NM | `runStatus.driveMotorTorque` |
| BYTE4~5 | 小端 | driveMotorSpeed | raw - 32767 | RPM | `runStatus.driveMotorSpeed` |
| BYTE6 Bit0~3 | — | driveMotorState | 状态机值 | — | `ioStatus.driveMotorState` |

> 注：BYTE4~5 同时作为 `driveDirection`（mainScreen）和 `driveMotorSpeed`（runStatus）两用

**CAN 注入**：
```bash
# 扭矩=0NM（raw=1023=0x03FF, BYTE2=0xFF, BYTE3=0x03），方向=N（BYTE4~5小端1000=0x03E8），状态机=0
cansend can0 0CFF0008#00 00 FF 03 E8 03 00 00

# 扭矩=+100NM（raw=1123=0x0463, BYTE2=0x63, BYTE3=0x04），方向=右（BYTE4~5=1500=0x05DC→DC05），状态机=2
cansend can0 0CFF0008#00 00 63 04 DC 05 02 00

# 扭矩=-100NM（raw=923=0x039B, BYTE2=0x9B, BYTE3=0x03），方向=左（BYTE4~5=500=0x01F4→F401），状态机=1
cansend can0 0CFF0008#00 00 9B 03 F4 01 01 00
```

**tester 验证**：
```bash
./skes_qt_bridge_tester query runStatus.driveMotorTorque
./skes_qt_bridge_tester query runStatus.driveMotorSpeed
./skes_qt_bridge_tester query ioStatus.driveMotorState
```

---

### Frame-09  CAN0 0x0CFF0009 — 油泵电机扭矩 / 转速 / 状态机

**协议说明**：

| 字节 | 字节序 | 字段 | 公式 | 单位 | JSON 路径 |
|------|--------|------|------|------|-----------|
| BYTE2~3 | 小端 | pumpMotorTorque | raw - 1023 | NM | `runStatus.pumpMotorTorque` |
| BYTE4~5 | 小端 | pumpMotorSpeed | raw - 32767 | RPM | `runStatus.pumpMotorSpeed` |
| BYTE6 Bit0~3 | — | pumpMotorState | 状态机值 | — | `ioStatus.pumpMotorState` |

**CAN 注入**：
```bash
# 扭矩=0NM（raw=1023=0x03FF, BYTE2=0xFF, BYTE3=0x03），转速=0RPM（raw=32767=0x7FFF, BYTE4=0xFF, BYTE5=0x7F），状态机=0
cansend can0 0CFF0009#00 00 FF 03 FF 7F 00 00

# 扭矩=+50NM（raw=1073=0x0431, BYTE2=0x31, BYTE3=0x04），转速=+500RPM（raw=33267=0x81F3, BYTE4=0xF3, BYTE5=0x81），状态机=3
cansend can0 0CFF0009#00 00 31 04 F3 81 03 00

# 扭矩=-50NM（raw=973=0x03CD, BYTE2=0xCD, BYTE3=0x03），转速=0RPM，状态机=0
cansend can0 0CFF0009#00 00 CD 03 FF 7F 00 00
```

**tester 验证**：
```bash
./skes_qt_bridge_tester query runStatus.pumpMotorTorque
./skes_qt_bridge_tester query runStatus.pumpMotorSpeed
./skes_qt_bridge_tester query ioStatus.pumpMotorState
```

---

### Frame-10  CAN0 0x0CFF0108 — 行走电机温度 / IGBT 温度

**协议说明**：

| 字节 | 字段 | 公式 | 单位 | JSON 路径 |
|------|------|------|------|-----------|
| BYTE4 | driveMotorTemp | raw - 40 | ℃ | `runStatus.driveMotorTemp` |
| BYTE5 | driveIGBTTemp | raw - 40 | ℃ | `runStatus.driveIGBTTemp` |

**CAN 注入**：
```bash
# 电机温度=25℃（raw=65=0x41），IGBT温度=25℃（raw=65=0x41）
cansend can0 0CFF0108#00 00 00 00 41 41 00 00

# 电机温度=80℃（raw=120=0x78），IGBT温度=60℃（raw=100=0x64）
cansend can0 0CFF0108#00 00 00 00 78 64 00 00

# 电机温度=-10℃（raw=30=0x1E），IGBT温度=-10℃
cansend can0 0CFF0108#00 00 00 00 1E 1E 00 00

# 电机温度=0℃（raw=40=0x28），IGBT温度=0℃
cansend can0 0CFF0108#00 00 00 00 28 28 00 00
```

**tester 验证**：
```bash
./skes_qt_bridge_tester query runStatus.driveMotorTemp
./skes_qt_bridge_tester query runStatus.driveIGBTTemp
```

---

### Frame-11  CAN0 0x0CFF0109 — 油泵电机温度 / IGBT 温度

**协议说明**：

| 字节 | 字段 | 公式 | 单位 | JSON 路径 |
|------|------|------|------|-----------|
| BYTE4 | pumpMotorTemp | raw - 40 | ℃ | `runStatus.pumpMotorTemp` |
| BYTE5 | pumpIGBTTemp | raw - 40 | ℃ | `runStatus.pumpIGBTTemp` |

**CAN 注入**：
```bash
# 泵电机温度=25℃（raw=65=0x41），泵IGBT温度=25℃
cansend can0 0CFF0109#00 00 00 00 41 41 00 00

# 泵电机温度=90℃（raw=130=0x82），泵IGBT温度=70℃（raw=110=0x6E）
cansend can0 0CFF0109#00 00 00 00 82 6E 00 00
cansend can0 0CFF0109#00 00 00 00 82 6E 00 00

# 泵电机温度=0℃（raw=40=0x28），泵IGBT温度=0℃
cansend can0 0CFF0109#00 00 00 00 28 28 00 00
```

**tester 验证**：
```bash
./skes_qt_bridge_tester query runStatus.pumpMotorTemp
./skes_qt_bridge_tester query runStatus.pumpIGBTTemp
```

---

### Frame-12  CAN0 0x0CFF08EF — 行走电机目标速度

**协议说明**：

| 字节 | 字节序 | 字段 | 公式 | 单位 | JSON 路径 |
|------|--------|------|------|------|-----------|
| BYTE3~4 | 小端 | driveMotorTargetSpeed | raw - 10000 | RPM | `runStatus.driveMotorTargetSpeed` |

**CAN 注入**：
```bash
# 目标速度=0RPM（raw=10000=0x2710, BYTE3=0x10, BYTE4=0x27）
cansend can0 0CFF08EF#00 00 00 10 27 00 00 00

# 目标速度=+500RPM（raw=10500=0x2904, BYTE3=0x04, BYTE4=0x29）
cansend can0 0CFF08EF#00 00 00 04 29 00 00 00

# 目标速度=-500RPM（raw=9500=0x251C, BYTE3=0x1C, BYTE4=0x25）
cansend can0 0CFF08EF#00 00 00 1C 25 00 00 00
cansend can0 0CFF08EF#00 00 00 1C 25 00 00 00
cansend can0 0CFF08EF#00 00 00 1C 25 00 00 00
```

**tester 验证**：
```bash
./skes_qt_bridge_tester query runStatus.driveMotorTargetSpeed
```

---

### Frame-13  CAN0 0x0CFF09EF — 油泵电机目标速度

**协议说明**：

| 字节 | 字节序 | 字段 | 公式 | 单位 | JSON 路径 |
|------|--------|------|------|------|-----------|
| BYTE3~4 | 小端 | pumpMotorTargetSpeed | raw - 10000 | RPM | `runStatus.pumpMotorTargetSpeed` |

**CAN 注入**：
```bash
# 目标速度=0RPM（raw=10000=0x2710, BYTE3=0x10, BYTE4=0x27）
cansend can0 0CFF09EF#00 00 00 10 27 00 00 00

# 目标速度=+1000RPM（raw=11000=0x2AF8, BYTE3=0xF8, BYTE4=0x2A）
cansend can0 0CFF09EF#00 00 00 F8 2A 00 00 00

# 目标速度=-1000RPM（raw=9000=0x2328, BYTE3=0x28, BYTE4=0x23）
cansend can0 0CFF09EF#00 00 00 28 23 00 00 00
```

**tester 验证**：
```bash
./skes_qt_bridge_tester query runStatus.pumpMotorTargetSpeed
```

---

## 四、ioStatus — IO 输入 / 输出状态

### Frame-14  CAN0 0x18FF6E17 — 左 / 右承载轮 PWM

**协议说明**：

| 字节 | 字段 | JSON 路径 |
|------|------|-----------|
| BYTE5 | leftLoadWheelPWM（0~255）| `ioStatus.leftLoadWheelPWM` |
| BYTE6 | rightLoadWheelPWM（0~255）| `ioStatus.rightLoadWheelPWM` |

**CAN 注入**：
```bash
# 左PWM=100，右PWM=150（BYTE5=0x64, BYTE6=0x96）
cansend can0 18FF6E17#00 00 00 00 00 64 96 00

# 左PWM=200，右PWM=200（BYTE5=0xC8, BYTE6=0xC8）
cansend can0 18FF6E17#00 00 00 00 00 C8 C8 00
cansend can0 18FF6E17#00 00 00 00 00 C8 C8 00
cansend can0 18FF6E17#00 00 00 00 00 C8 C8 00

# 左PWM=0，右PWM=0（全停）
cansend can0 18FF6E17#00 00 00 00 00 00 00 00
```

**tester 验证**：
```bash
./skes_qt_bridge_tester query ioStatus.leftLoadWheelPWM
./skes_qt_bridge_tester query ioStatus.rightLoadWheelPWM
```

---

### Frame-15  CAN1 0x32A — 模拟量输入信号（1）

**协议说明**（CAN1 对应源码 can_channel=1）：

| 字节 | 字段 | JSON 路径 |
|------|------|-----------|
| BYTE1 | accelPedalAi1（油门踏板通道1）| `ioStatus.accelPedalAi1` |
| BYTE2 | angleSensorAi1（转角传感器通道1）| `ioStatus.angleSensorAi1` |
| BYTE3 | brakePedalAi1（刹车踏板通道1）| `ioStatus.brakePedalAi1` |
| BYTE4 | thumbTelescopicAi（前后移手柄）| `ioStatus.thumbTelescopicAi` |
| BYTE5 | thumbUpDAi（升降手柄）| `ioStatus.thumbUpDAi` |
| BYTE6 | thumbTiltAi（倾斜手柄）| `ioStatus.thumbTiltAi` |
| BYTE7 | thumbShiftAi（侧移手柄）| `ioStatus.thumbShiftAi` |

**CAN 注入**：
```bash
# 所有通道中间值128（BYTE0=0，其余=0x80）
cansend can1 32A#00 80 80 80 80 80 80 80

# 油门全踏（BYTE1=0xFF），其余中间值
cansend can1 32A#00 FF 80 80 80 80 80 80

# 升降手柄全拉（BYTE5=0xFF），其余中间值
cansend can1 32A#00 80 80 80 80 FF 80 80

# 全部归零
cansend can1 32A#00 00 00 00 00 00 00 00
```

**tester 验证**：
```bash
./skes_qt_bridge_tester query ioStatus.accelPedalAi1
./skes_qt_bridge_tester query ioStatus.angleSensorAi1
./skes_qt_bridge_tester query ioStatus.brakePedalAi1
./skes_qt_bridge_tester query ioStatus.thumbTelescopicAi
./skes_qt_bridge_tester query ioStatus.thumbUpDAi
./skes_qt_bridge_tester query ioStatus.thumbTiltAi
./skes_qt_bridge_tester query ioStatus.thumbShiftAi
```

---

### Frame-16  CAN1 0x32B — 模拟量输入信号（2）

**协议说明**：

| 字节 | 字节序 | 字段 | 公式 / 说明 | JSON 路径 |
|------|--------|------|------------|-----------|
| BYTE1 | — | accelPedalAi2（油门踏板通道2）| 0~255 | `ioStatus.accelPedalAi2` |
| BYTE2 | — | angleSensorAi2（转角传感器通道2）| 0~255 | `ioStatus.angleSensorAi2` |
| BYTE3 | — | brakePedalAi2（刹车踏板通道2）| 0~255 | `ioStatus.brakePedalAi2` |
| BYTE4 | — | telescopicPosAi（前后移编码器）| 0~255 | `ioStatus.telescopicPosAi` |
| BYTE5 | — | pressureValveAi（液压压力传感器）| 0~255 | `ioStatus.pressureValveAi` |
| BYTE6~7 | 小端 uint16 | forkHeight（货叉高度）| mm÷1000=m，如 0x0BB8=3000→3.0m | `mainScreen.forkHeight` |

**CAN 注入**：
```bash
# 所有通道中间值128
cansend can1 32B#00 80 80 80 80 80 00 00

# 油门通道2全踏，刹车=0，液压压力满量程（BYTE5=0xFF）
cansend can1 32B#00 FF 80 00 00 80 FF 00
cansend can1 32B#00 FF 80 00 80 80 FF 00

# 前后移编码器=200（BYTE4=0xC8）
cansend can1 32B#00 80 80 80 C8 80 00 00

# 全部归零
cansend can1 32B#00 00 00 00 00 00 00 00
```

**CAN 注入 — 货叉高度（BYTE6~7）**：
```bash
# forkHeight=0m（BYTE6=0x00, BYTE7=0x00）
cansend can1 32B#00 80 80 80 80 80 00 00
./skes_qt_bridge_tester query mainScreen.forkHeight      # → 0.0

# forkHeight=1.5m（1500mm=0x05DC, 小端 BYTE6=0xDC, BYTE7=0x05）
cansend can1 32B#00 80 80 80 80 80 DC 05
./skes_qt_bridge_tester query mainScreen.forkHeight      # → 1.5

# forkHeight=3.0m（3000mm=0x0BB8, 小端 BYTE6=0xB8, BYTE7=0x0B）
cansend can1 32B#00 80 80 80 80 80 B8 0B
./skes_qt_bridge_tester query mainScreen.forkHeight      # → 3.0

# forkHeight=5.0m（5000mm=0x1388, 小端 BYTE6=0x88, BYTE7=0x13）
cansend can1 32B#00 80 80 80 80 80 88 13
./skes_qt_bridge_tester query mainScreen.forkHeight      # → 5.0
```

**tester 验证**：
```bash
./skes_qt_bridge_tester query ioStatus.accelPedalAi2
./skes_qt_bridge_tester query ioStatus.angleSensorAi2
./skes_qt_bridge_tester query ioStatus.brakePedalAi2
./skes_qt_bridge_tester query ioStatus.telescopicPosAi
./skes_qt_bridge_tester query ioStatus.pressureValveAi
./skes_qt_bridge_tester query mainScreen.forkHeight
```

---

### Frame-17  CAN1 0x32C — 数字输入 / 输出 + PWM 输出

**协议说明**：

| 字节 | 位域 | 字段 | JSON 路径 |
|------|------|------|-----------|
| BYTE1 Bit0 | — | mastRaiseSwitch（门架上升到位）| `ioStatus.mastRaiseSwitch` |
| BYTE1 Bit1 | — | mastLowerSwitch（门架下降到位）| `ioStatus.mastLowerSwitch` |
| BYTE1 Bit2 | — | attachmentRightSwitch（属具左移到位）| `ioStatus.attachmentRightSwitch` |
| BYTE1 Bit3 | — | attachmentLeftSwitch（属具右移到位）| `ioStatus.attachmentLeftSwitch` |
| BYTE1 Bit4 | — | seatBeltSwitch（安全带开关）| `ioStatus.seatBeltSwitch` |
| BYTE1 Bit5 | — | opsSeatSwitch（操作坐椅开关）| `ioStatus.opsSeatSwitch` |
| BYTE1 Bit6 | — | brakeSwitch（驻车开关）| `ioStatus.brakeSwitch` |
| BYTE3 Bit0 | — | attachmentTiltShiftDO（属具侧移DO）| `ioStatus.attachmentTiltShiftDO` |
| BYTE3 Bit1 | — | liftDO（提升DO）| `ioStatus.liftDO` |
| BYTE3 Bit2 | — | fanDO（风扇DO）| `ioStatus.fanDO` |
| BYTE3 Bit3 | — | attachmentDO1（属具DO1）| `ioStatus.attachmentDO1` |
| BYTE3 Bit4 | — | attachmentDO2（属具DO2）| `ioStatus.attachmentDO2` |
| BYTE4 | — | strPWM（转向阻尼PWM）| `ioStatus.strPWM` |
| BYTE5 | — | teleFPWM（前移比例阀PWM）| `ioStatus.teleFPWM` |
| BYTE6 | — | teleRPWM（后移比例阀PWM）| `ioStatus.teleRPWM` |
| BYTE7 | — | downPWM（下降比例阀PWM）| `ioStatus.downPWM` |

**CAN 注入**：
```bash
# 所有 DI 开关全开（BYTE1=0x7F），DO 全关，PWM 全零
cansend can1 32C#00 7F 00 00 00 00 00 00

# 安全带已系（BYTE1 Bit4=1 → 0x10），驻车开关ON（Bit6=1 → 0x40），合计0x50
cansend can1 32C#00 50 00 00 00 00 00 00

# 提升DO=1（BYTE3 Bit1=1 → 0x02），风扇DO=1（Bit2=1 → 0x04），合计0x06
cansend can1 32C#00 00 00 06 00 00 00 00

# 转向PWM=128，前移PWM=100，后移PWM=80，下降PWM=60（BYTE4~7）
cansend can1 32C#00 00 00 00 80 64 50 3C
cansend can1 32C#00 00 00 00 80 64 50 3C

# 全字段非零联合测试：DI=0x3F，DO=0x1F，strPWM=50，teleFPWM=60，teleRPWM=70，downPWM=80
cansend can1 32C#00 3F 00 1F 32 3C 46 50
cansend can1 32C#00 3F 00 1F 32 3C 46 50
```

**tester 验证**：
```bash
./skes_qt_bridge_tester query ioStatus.mastRaiseSwitch
./skes_qt_bridge_tester query ioStatus.mastLowerSwitch
./skes_qt_bridge_tester query ioStatus.seatBeltSwitch
./skes_qt_bridge_tester query ioStatus.opsSeatSwitch
./skes_qt_bridge_tester query ioStatus.brakeSwitch
./skes_qt_bridge_tester query ioStatus.attachmentTiltShiftDO
./skes_qt_bridge_tester query ioStatus.liftDO
./skes_qt_bridge_tester query ioStatus.fanDO
./skes_qt_bridge_tester query ioStatus.strPWM
./skes_qt_bridge_tester query ioStatus.teleFPWM
./skes_qt_bridge_tester query ioStatus.teleRPWM
./skes_qt_bridge_tester query ioStatus.downPWM
```

---

## 五、faultInfo — 故障码

### Frame-18  CAN1 0x32D — 用户提醒故障码

**协议说明**：

| 字节 | 字段 | JSON 路径 |
|------|------|-----------|
| BYTE1 | userErr（用户提醒故障码）| `faultInfo.userErr` |

**CAN 注入**：
```bash
# userErr=0：无提醒
cansend can1 32D#00 00 00 00 00 00 00 00

# userErr=5：5号用户提醒
cansend can1 32D#00 05 00 00 00 00 00 00

# userErr=255：最大值
cansend can1 32D#00 FF 00 00 00 00 00 00
```

**tester 验证**：
```bash
./skes_qt_bridge_tester query faultInfo.userErr
```

---

### Frame-19  CAN1 0x32E — VCU 故障码

**协议说明**：

| 字节 | 字段 | JSON 路径 |
|------|------|-----------|
| BYTE1 | vcuErr（VCU 系统故障码）| `faultInfo.vcuErr` |

**CAN 注入**：
```bash
# vcuErr=0：无故障
cansend can1 32E#00 00 00 00 00 00 00 00

# vcuErr=3：3号故障
cansend can1 32E#00 03 00 00 00 00 00 00

# vcuErr=128：高位故障
cansend can1 32E#00 80 00 00 00 00 00 00
```

**tester 验证**：
```bash
./skes_qt_bridge_tester query faultInfo.vcuErr
```

**双故障码联合测试**：
```bash
# 同时注入两路故障码
cansend can1 32D#00 05 00 00 00 00 00 00    # userErr=5
cansend can1 32E#00 03 00 00 00 00 00 00    # vcuErr=3
./skes_qt_bridge_tester query faultInfo.userErr    # → 5
./skes_qt_bridge_tester query faultInfo.vcuErr    # → 3
```

---

## 六、versionInfo — 版本信息

### Frame-20  CAN1 0x33A — VCU 软件版本号

**协议说明**：

| 字节 | 类型 | 字段 | JSON 路径 |
|------|------|------|-----------|
| BYTE1~3 | ASCII（3字节）| vcuSoftVersion | `versionInfo.vcuSoftVersion` |

**CAN 注入**：
```bash
# 版本号 "V26"（BYTE1=0x56='V', BYTE2=0x32='2', BYTE3=0x36='6'）
cansend can1 33A#00 56 32 36 00 00 00 00

# 版本号 "A01"（BYTE1=0x41='A', BYTE2=0x30='0', BYTE3=0x31='1'）
cansend can1 33A#00 41 30 31 00 00 00 00

# 版本号 "---"（BYTE1~3=0x2D '-'）
cansend can1 33A#00 2D 2D 2D 00 00 00 00

# 版本号全零（预期返回空字符串或 null 字符）
cansend can1 33A#00 00 00 00 00 00 00 00
```

**tester 验证**：
```bash
./skes_qt_bridge_tester query versionInfo.vcuSoftVersion
```

**预期结果**：返回 3 字节 ASCII 组成的字符串，如 `"V26"`

---

## 综合联调测试

### T-01  0x31A 全字段一帧注入

注入一帧包含所有 0x31A 字段的完整数据，批量验证：

```bash
# BYTE0=80（8.0km/h）
# BYTE1=0x43：Bit0=READY, Bit1=扳手, Bit6=高速P（0x40+0x02+0x01=0x43）
# BYTE2~3：forkWeight=300KG（0x012C, 小端BYTE2=0x2C, BYTE3=0x01）
# BYTE4~5：已移出（转向角现由CAN0 0x360 BYTE2~3提供）
# BYTE6=2（2个故障）
# BYTE7=64（64%电量）
cansend can0 031A#50 43 2C 01 00 00 02 40

# 注入转向角+5°（raw=1250=0x04E2, 小端BYTE2=0xE2, BYTE3=0x04）至 CAN0 0x360
cansend can0 360#03 00 E2 04 88 13 00 00

./skes_qt_bridge_tester query mainScreen.vehicleSpeed    # → 8.0
./skes_qt_bridge_tester query mainScreen.ready           # → 1
./skes_qt_bridge_tester query mainScreen.wrench          # → 1
./skes_qt_bridge_tester query mainScreen.seatBeltWarning # → 1（Bit2=0，反逻辑）
./skes_qt_bridge_tester query mainScreen.speedMode       # → 1（高速P）
./skes_qt_bridge_tester query mainScreen.forkWeight      # → 300
./skes_qt_bridge_tester query mainScreen.steeringAngle   # → 5.0（来自0x360）
./skes_qt_bridge_tester query mainScreen.faultCount      # → 2
./skes_qt_bridge_tester query mainScreen.batteryLevel    # → 64
```

---

### T-02  工时计时完整链路

```bash
# 1. 钥匙 ON
cansend can0 18FD0291#00 00 01 00 00 00 00 00
./skes_qt_bridge_tester query internal.powerOnSignal    # → 1

# 2. 确认未充电
cansend can0 0CFF7C03#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query internal.chargingStatus  # → 0
./skes_qt_bridge_tester query internal.isWorkTimeCounting  # → true

# 3. 等待 10 秒，查询工时变化
sleep 10
./skes_qt_bridge_tester query mainScreen.totalWorkHours

# 4. 接入充电，确认工时停止
cansend can0 0CFF7C03#00 00 00 00 00 04 00 00
./skes_qt_bridge_tester query internal.isWorkTimeCounting  # → false
```

---

### T-03  CAN1 IO 状态全帧联合

```bash
# 注入 0x32A：油门踩半（BYTE1=0x80），转角中位（BYTE2=0x80）
cansend can1 32A#00 80 80 80 80 80 80 80

# 注入 0x32B：液压压力=200（BYTE5=0xC8）
cansend can1 32B#00 80 80 80 80 C8 00 00
cansend can1 32B#00 80 80 80 80 C8 00 00
cansend can1 32B#00 80 80 80 80 C8 00 00

# 注入 0x32C：安全带已系+坐椅有人（BYTE1=0x30），提升DO=1（BYTE3=0x02），strPWM=100
cansend can1 32C#00 30 00 02 64 00 00 00
cansend can1 32C#00 30 00 02 64 00 00 00
cansend can1 32C#00 30 00 02 64 00 00 00

# 批量查询
./skes_qt_bridge_tester query ioStatus.accelPedalAi1    # → 128
./skes_qt_bridge_tester query ioStatus.pressureValveAi  # → 200
./skes_qt_bridge_tester query ioStatus.seatBeltSwitch   # → 1
./skes_qt_bridge_tester query ioStatus.opsSeatSwitch    # → 1
./skes_qt_bridge_tester query ioStatus.liftDO           # → 1
./skes_qt_bridge_tester query ioStatus.strPWM           # → 100
```

---

### T-04  故障码 + 版本号 + subscribe 推送验证

```bash
# 后台开启订阅
./skes_qt_bridge_tester subscribe &
SUB_PID=$!

# 注入故障码
cansend can1 32D#00 07 00 00 00 00 00 00    # userErr=7
cansend can1 32E#00 02 00 00 00 00 00 00    # vcuErr=2

# 注入版本号
cansend can1 33A#00 56 32 37 00 00 00 00    # "V27"

# 等一个推送周期
sleep 1

# 查询
./skes_qt_bridge_tester query faultInfo.userErr           # → 7
./skes_qt_bridge_tester query faultInfo.vcuErr           # → 2
./skes_qt_bridge_tester query versionInfo.vcuSoftVersion  # → "V27"

kill $SUB_PID
```

**预期**：每 500ms 收到一条 filter_table 推送，JSON 中 `faultInfo` 和 `versionInfo` 随注入帧同步更新

---

## 附：CAN 帧速查表

| CAN 通道 | CAN ID | 类型 | 关联 JSON 路径 |
|----------|--------|------|----------------|
| CAN0 | 0x31A | 标准帧 | mainScreen（速度/指示灯/称重/角度/故障/电量）|
| CAN0 | 0x18FD0291 | 扩展帧 | fuseBoxStatus（故障码/DI/DO/C11/C12）/ mainScreen.headLightStatus / versionInfo.bcmSoftVersion / internal.powerOnSignal |
| CAN0 | 0x0CFF7C03 | 扩展帧 | bmsData（电压/电流/SOC/继电器/充电状态）/ internal.chargingStatus / isWorkTimeCounting |
| CAN0 | 0x0CFF7D03 | 扩展帧 | bmsData.cellVoltMax / cellVoltMin / cellTempMax / cellTempMin |
| CAN0 | 0x0CFF7F03 | 扩展帧 | bmsData.insulation |
| CAN0 | 0x18FE7C03 | 扩展帧 | bmsData.vcuPowerCmd |
| CAN0 | 0x0CFF0008 | 扩展帧 | mainScreen.driveDirection / runStatus（行走电机）/ ioStatus.driveMotorState |
| CAN0 | 0x359 | 标准帧 | runStatus.strMotorCurrent / strMotorTargetSpeed |
| CAN0 | 0x360 | 标准帧 | ioStatus.strMotorState / runStatus.strMotorSpeed / mainScreen.steeringAngle / faultInfo.steerMcuErr |
| CAN0 | 0x585 | 标准帧 | mainScreen.forkTiltAngle |
| CAN0 | 0x0CFF0009 | 扩展帧 | runStatus（油泵电机）/ ioStatus.pumpMotorState |
| CAN0 | 0x0CFF0108 | 扩展帧 | runStatus.driveMotorTemp / driveIGBTTemp |
| CAN0 | 0x0CFF0109 | 扩展帧 | runStatus.pumpMotorTemp / pumpIGBTTemp |
| CAN0 | 0x0CFF08EF | 扩展帧 | runStatus.driveMotorTargetSpeed |
| CAN0 | 0x0CFF09EF | 扩展帧 | runStatus.pumpMotorTargetSpeed |
| CAN0 | 0x18FF6E17 | 扩展帧 | ioStatus.leftLoadWheelPWM / rightLoadWheelPWM |
| CAN1 | 0x32A | 标准帧 | ioStatus（7路模拟量输入1）|
| CAN1 | 0x32B | 标准帧 | ioStatus（5路模拟量输入2）/ mainScreen.forkHeight |
| CAN1 | 0x32C | 标准帧 | ioStatus（DI开关 + DO输出 + 4路PWM）|
| CAN1 | 0x32D | 标准帧 | faultInfo.userErr |
| CAN1 | 0x32E | 标准帧 | faultInfo.vcuErr |
| CAN0 | 0x33A | 标准帧 | versionInfo.vcuSoftVersion |
| CAN0 | 0x0CFF0208 | 扩展帧 | faultInfo.driveMcuErr |
| CAN0 | 0x0CFF0209 | 扩展帧 | faultInfo.pumpMcuErr |
| CAN0 | 0x0CFF0308 | 扩展帧 | versionInfo.driveMcuSoftVersion |
| CAN0 | 0x0CFF0309 | 扩展帧 | versionInfo.pumpMcuSoftVersion |
| CAN0 | 0x0CFF030A | 扩展帧 | versionInfo.steerMcuSoftVersion |
| CAN0 | 0x0CFF8203 | 扩展帧 | versionInfo.bmsSoftVersion |
| CAN1 | 0x33B + 0x33C + 0x33D | 标准帧 | versionInfo.vehicleVin（3帧拼合）/ versionInfo.vcuWorktime（0x33D BYTE6）|
| CAN1 | 0x4D1 | 标准帧 | rfidData（心跳/工作/故障/卡号/解锁时间/序号）|
| CAN1 | 0x4D2 | 标准帧 | rfidData.unlockDuration |

---

## 七、mainScreen — 里程 / 工时（Seq29/30/33/34 补充）

> 这两项字段由 VEH 内部定时器计算累积，不直接来自单一 CAN 帧，  
> 而是由 **0x31A 提供车速触发**、**0x18FD0291 + 0x0CFF7C03 触发工时计时**。

### Frame-21  Seq29/30 — 总里程 / 单次里程

**计算逻辑**（源自源码 updateDateTime 定时器，每 1 秒触发）：
- `vehicleSpeed > 0.01 km/h` 时累加：`Δdistance = vehicleSpeed × (1s/3600)`
- `totalMileage` 每 60 秒持久化到 `/data/forklift_persistent.json`
- `singleMileage` 不持久化，重启归零

**测试步骤 — 验证累积正确性**：
```bash
# 1. 查询当前基准值
./skes_qt_bridge_tester query mainScreen.totalMileage
./skes_qt_bridge_tester query mainScreen.singleMileage

# 2. 注入持续车速 10 km/h（BYTE0=100=0x64）
cansend can0 031A#64 00 00 00 00 00 00 00

# 3. 等待 10 秒后查询（理论增量 = 10×10/3600 ≈ 0.028 km）
sleep 10
./skes_qt_bridge_tester query mainScreen.totalMileage
./skes_qt_bridge_tester query mainScreen.singleMileage

# 4. 注入车速 0（停车），等待 5 秒，确认里程不再增加
cansend can0 031A#00 00 00 00 00 00 00 00
sleep 5
./skes_qt_bridge_tester query mainScreen.totalMileage
# 应与上一次查询值相同
```

**边界测试 — 停车不累加**：
```bash
# 注入 vehicleSpeed = 0.05 km/h（BYTE0=0，低于阈值0.01时不累加？实测确认）
# 注意：BYTE0=0 → vehicleSpeed=0.0，确认不累加
cansend can0 031A#00 00 00 00 00 00 00 00
sleep 5
./skes_qt_bridge_tester query mainScreen.singleMileage
# 预期：值不变
```

**持久化验证**：
```bash
# 1. 记录当前 totalMileage 值
./skes_qt_bridge_tester query mainScreen.totalMileage

# 2. 重启 vehicleDataEngine（等待 60s 确保持久化写入后再重启）
sleep 60
kill $(pgrep vehicleDataEngine)
sleep 3
cd ~/cameras && ./vehicleDataEngine &
sleep 2

# 3. 查询重启后恢复值
./skes_qt_bridge_tester query mainScreen.totalMileage   # → 应恢复重启前的值
./skes_qt_bridge_tester query mainScreen.singleMileage  # → 应归零（不持久化）
```

**预期结果**：

| 条件 | totalMileage | singleMileage |
|------|-------------|---------------|
| vehicleSpeed=10 km/h，运行 3600s | +10 km | +10 km |
| vehicleSpeed=0（停车）| 不变 | 不变 |
| 重启后 | **恢复**（持久化）| **归零**（不持久化）|

---

### Frame-22  Seq33/34 — 开机工时 / 单次工时

**计算逻辑**：
- 条件：`powerOnSignal=1`（0x18FD0291）且 `chargingStatus=0`（0x0CFF7C03）
- 每 1 秒累加 1/3600 小时
- `totalWorkHours` 每 60 秒持久化，`singleWorkHours` 不持久化

**测试步骤 — 工时正常计时**：
```bash
# 1. 发钥匙 ON 信号（0x18FD0291 BYTE2 Bit0=1）
cansend can0 18FD0291#00 00 01 00 00 00 00 00

# 2. 确认未充电（0x0CFF7C03 BYTE5 Bit2~3=00）
cansend can0 0CFF7C03#00 00 00 00 00 00 00 00

# 3. 查询计时标志
./skes_qt_bridge_tester query internal.powerOnSignal    # → 1
./skes_qt_bridge_tester query internal.chargingStatus  # → 0
./skes_qt_bridge_tester query internal.isWorkTimeCounting  # → true

# 4. 等待 10 秒，查询工时增量（理论 ≈ 0.00278 小时）
sleep 10
./skes_qt_bridge_tester query mainScreen.totalWorkHours
./skes_qt_bridge_tester query mainScreen.singleWorkHours
```

**测试步骤 — 充电时工时暂停**：
```bash
# 接入充电（BYTE5=0x04，Bit2=1）
cansend can0 0CFF7C03#00 00 00 00 00 04 00 00

./skes_qt_bridge_tester query internal.chargingStatus      # → 1
./skes_qt_bridge_tester query internal.isWorkTimeCounting  # → false

# 等待 10 秒，确认工时不增加
sleep 10
./skes_qt_bridge_tester query mainScreen.totalWorkHours
# 值应与充电前相同
```

**测试步骤 — 钥匙 OFF 时工时暂停**：
```bash
# 钥匙 OFF（BYTE2 Bit0=0）
cansend can0 18FD0291#00 00 00 00 00 00 00 00

./skes_qt_bridge_tester query internal.powerOnSignal       # → 0
./skes_qt_bridge_tester query internal.isWorkTimeCounting  # → false

sleep 5
./skes_qt_bridge_tester query mainScreen.totalWorkHours
# 值不变
```

**持久化验证**：
```bash
# 1. 确认工时大于 0
./skes_qt_bridge_tester query mainScreen.totalWorkHours

# 2. 等待持久化周期后重启（至少 60s）
sleep 65
kill $(pgrep vehicleDataEngine)
sleep 3
cd ~/cameras && ./vehicleDataEngine &
sleep 2

# 3. 重启后查询
./skes_qt_bridge_tester query mainScreen.totalWorkHours   # → 恢复重启前的值
./skes_qt_bridge_tester query mainScreen.singleWorkHours  # → 归零
```

**预期结果**：

| powerOnSignal | chargingStatus | isWorkTimeCounting | 工时累加 |
|--------------|----------------|--------------------|---------|
| 0 | 0 | false | ❌ 不累加 |
| 1 | 0 | true | ✅ 累加 |
| 1 | 非0 | false | ❌ 充电暂停 |
| 0 | 非0 | false | ❌ 不累加 |

---

## 八、已知 Qt 端问题备注

### Bug-Qt-01  driveDirection=1000 显示右箭头

**现象**：
```bash
cansend can0 0CFF0008#00 00 FF 03 E8 03 00 00
./skes_qt_bridge_tester query mainScreen.driveDirection
# → 1000（VEH 端正确）
# Qt 界面显示：右箭头 ❌（应显示 N 档中位）
```

**根因**：VEH 端逻辑正确，`driveDirection=1000` 已按协议写入 JSON。  
问题在 **Qt 端判断逻辑**，可能将 `>= 1000` 判断为右箭头，而非 `> 1000`。

**协议约定**（源码注释）：
```
< 1000  →  左箭头（倒车）
= 1000  →  N 档中位
> 1000  →  右箭头（前进）
```

**修复建议**：Qt 端将判断条件从 `>= 1000` 改为 `> 1000`（或 `== 1000` 单独判断中位）。  
VEH 端**无需修改**。

---

## 九、bmsData — BMS 电池管理系统

> 涉及 4 条 CAN0 帧，所有字段通过 `bmsData.*` 路径查询。
> 注：`0x0CFF7C03` 同时更新 `internal.chargingStatus` / `internal.isWorkTimeCounting`，工时联动测试见 Frame-05。

---

### Frame-23  CAN0 0x0CFF7C03 — BMS 主帧（电压 / 电流 / SOC / 继电器 / 充电状态）

> 至少需要 8 字节才触发解析。

**协议说明**：

| 字节 | 位域 | 字段 | 公式 / 说明 | JSON 路径 |
|------|------|------|------------|-----------|
| BYTE0~1 | 小端 uint16 | voltage | raw×0.1=V（直接传真实值，double）| `bmsData.voltage` |
| BYTE2~3 | 小端 uint16 | current | raw×0.1−1000=A（偏移1000，直接传真实值，double）| `bmsData.current` |
| BYTE4 | — | soc | raw×0.4 = %（分辨率0.4），存整数 | `bmsData.soc` |
| BYTE5 Bit0~1 | — | bmsSelfCheck | 0=自检中/1=完成/2=失败 | `bmsData.bmsSelfCheck` |
| BYTE5 Bit2~3 | — | chargingMode | 0=未充/1=充中/2=完成 | `bmsData.chargingMode` |
| BYTE5 Bit4~5 | — | chargeConnection | 0=未连/1=直流/2=交流 | `bmsData.chargeConnection` |
| BYTE5 Bit6 | — | chargeActivate | 0=无A+信号/1=有A+信号 | `bmsData.chargeActivate` |
| BYTE5 Bit7 | — | bmsHvOff | 0=不请求/1=请求切断高压 | `bmsData.bmsHvOff` |
| BYTE6 Bit0~1 | — | relayHeat | 0=断开/1=闭合/2=故障 | `bmsData.relayHeat` |
| BYTE6 Bit2~3 | — | relayCharge | 0=断开/1=闭合/2=故障 | `bmsData.relayCharge` |
| BYTE6 Bit4~5 | — | relayMainPos | 0=断开/1=闭合/2=故障 | `bmsData.relayMainPos` |
| BYTE6 Bit6~7 | — | relayMainNeg | 0=断开/1=闭合/2=故障 | `bmsData.relayMainNeg` |
| BYTE7 Bit0~1 | — | relayDC | 0=断开/1=闭合/2=故障 | `bmsData.relayDC` |
| BYTE7 Bit4~5 | — | relayPrecharge | 0=断开/1=闭合/2=故障 | `bmsData.relayPrecharge` |
| BYTE7 Bit6 | — | onWakeup | 0=无/1=ON档唤醒 | `bmsData.onWakeup` |

---

**一、BYTE0~1 — 电池总电压**

```bash
# 电压=0V（raw=0）
cansend can0 0CFF7C03#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query bmsData.voltage           # → 0.0

# 电压=48.0V（raw=480=0x01E0 → BYTE0=0xE0, BYTE1=0x01）
cansend can0 0CFF7C03#E0 01 00 00 00 00 00 00
./skes_qt_bridge_tester query bmsData.voltage           # → 48.0

# 电压=60.0V（raw=600=0x0258 → BYTE0=0x58, BYTE1=0x02）
cansend can0 0CFF7C03#58 02 00 00 00 00 00 00
./skes_qt_bridge_tester query bmsData.voltage           # → 60.0

# 电压=80.0V（raw=800=0x0320 → BYTE0=0x20, BYTE1=0x03）
cansend can0 0CFF7C03#20 03 00 00 00 00 00 00
./skes_qt_bridge_tester query bmsData.voltage           # → 80.0
```

---

**二、BYTE2~3 — 工作电流（无符号，偏移1000）**

> 换算：实际电流(A) = raw×0.1 − 1000（直接传真实值，double）。
> 反推：raw = (实际电流 + 1000) × 10。

```bash
# 电流=0A（raw=10000=0x2710 → BYTE2=0x10, BYTE3=0x27）
cansend can0 0CFF7C03#00 00 10 27 00 00 00 00
./skes_qt_bridge_tester query bmsData.current           # → 0.0

# 电流=+100A（raw=11000=0x2AF8 → BYTE2=0xF8, BYTE3=0x2A）
cansend can0 0CFF7C03#00 00 F8 2A 00 00 00 00
./skes_qt_bridge_tester query bmsData.current           # → 100.0

# 电流=-50A（raw=9500=0x251C → BYTE2=0x1C, BYTE3=0x25）
cansend can0 0CFF7C03#00 00 1C 25 00 00 00 00
./skes_qt_bridge_tester query bmsData.current           # → -50.0

# 电流=+200A（raw=12000=0x2EE0 → BYTE2=0xE0, BYTE3=0x2E）
cansend can0 0CFF7C03#00 00 E0 2E 00 00 00 00
./skes_qt_bridge_tester query bmsData.current           # → 200.0

# raw=0（边界，实际=-1000A）
cansend can0 0CFF7C03#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query bmsData.current           # → -1000.0
```

---

**三、BYTE4 — SOC（分辨率0.4）**

> 换算：SOC(%) = raw×0.4（向下取整）；反推：raw = SOC/0.4。

```bash
# SOC=0%（raw=0）
cansend can0 0CFF7C03#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query bmsData.soc               # → 0

# SOC=50%（raw=125=0x7D）
cansend can0 0CFF7C03#00 00 00 00 7D 00 00 00
./skes_qt_bridge_tester query bmsData.soc               # → 50

# SOC=80%（raw=200=0xC8）
cansend can0 0CFF7C03#00 00 00 00 C8 00 00 00
./skes_qt_bridge_tester query bmsData.soc               # → 80

# SOC=100%（raw=250=0xFA）
cansend can0 0CFF7C03#00 00 00 00 FA 00 00 00
./skes_qt_bridge_tester query bmsData.soc               # → 100
```

---

**四、BYTE5 — BMS状态标志位**

```bash
# ── bmsSelfCheck（Bit0~1）────────────────────────────────
# 自检中（Bit0~1=00, BYTE5=0x00）
cansend can0 0CFF7C03#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query bmsData.bmsSelfCheck      # → 0

# 自检完成（Bit0~1=01, BYTE5=0x01）
cansend can0 0CFF7C03#00 00 00 00 00 01 00 00
./skes_qt_bridge_tester query bmsData.bmsSelfCheck      # → 1

# 自检失败（Bit0~1=10, BYTE5=0x02）
cansend can0 0CFF7C03#00 00 00 00 00 02 00 00
./skes_qt_bridge_tester query bmsData.bmsSelfCheck      # → 2

# ── chargingMode（Bit2~3）────────────────────────────────
# 未充电（Bit2~3=00, BYTE5=0x00）
cansend can0 0CFF7C03#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query bmsData.chargingMode      # → 0

# 充电中（Bit2~3=01 → BYTE5=0x04）
cansend can0 0CFF7C03#00 00 00 00 00 04 00 00
./skes_qt_bridge_tester query bmsData.chargingMode      # → 1

# 充电完成（Bit2~3=10 → BYTE5=0x08）
cansend can0 0CFF7C03#00 00 00 00 00 08 00 00
./skes_qt_bridge_tester query bmsData.chargingMode      # → 2

# ── chargeConnection（Bit4~5）────────────────────────────
# 未连接（Bit4~5=00, BYTE5=0x00）
cansend can0 0CFF7C03#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query bmsData.chargeConnection  # → 0

# 直流连接（Bit4~5=01 → BYTE5=0x10）
cansend can0 0CFF7C03#00 00 00 00 00 10 00 00
./skes_qt_bridge_tester query bmsData.chargeConnection  # → 1

# 交流连接（Bit4~5=10 → BYTE5=0x20）
cansend can0 0CFF7C03#00 00 00 00 00 20 00 00
./skes_qt_bridge_tester query bmsData.chargeConnection  # → 2

# ── chargeActivate（Bit6）────────────────────────────────
# 有A+充电激活信号（Bit6=1 → BYTE5=0x40）
cansend can0 0CFF7C03#00 00 00 00 00 40 00 00
./skes_qt_bridge_tester query bmsData.chargeActivate    # → 1

# ── bmsHvOff（Bit7）──────────────────────────────────────
# BMS请求切断高压（Bit7=1 → BYTE5=0x80）
cansend can0 0CFF7C03#00 00 00 00 00 80 00 00
./skes_qt_bridge_tester query bmsData.bmsHvOff          # → 1

# ── 充电场景综合：充电中+直流连接+有激活信号+自检完成（BYTE5=0x55）
# Bit0=1(自检完成) | Bit2=1(充中) | Bit4=1(直流) | Bit6=1(激活) = 0x01|0x04|0x10|0x40=0x55
cansend can0 0CFF7C03#00 00 00 00 00 55 00 00
./skes_qt_bridge_tester query bmsData.bmsSelfCheck      # → 1
./skes_qt_bridge_tester query bmsData.chargingMode      # → 1
./skes_qt_bridge_tester query bmsData.chargeConnection  # → 1
./skes_qt_bridge_tester query bmsData.chargeActivate    # → 1
./skes_qt_bridge_tester query bmsData.bmsHvOff          # → 0
```

---

**五、BYTE6 — 继电器状态（主正 / 主负 / 充电 / 加热）**

```bash
# 全部断开（BYTE6=0x00）
cansend can0 0CFF7C03#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query bmsData.relayHeat         # → 0
./skes_qt_bridge_tester query bmsData.relayCharge       # → 0
./skes_qt_bridge_tester query bmsData.relayMainPos      # → 0
./skes_qt_bridge_tester query bmsData.relayMainNeg      # → 0

# 主正闭合（Bit4~5=01 → BYTE6=0x10）
cansend can0 0CFF7C03#00 00 00 00 00 00 10 00
./skes_qt_bridge_tester query bmsData.relayMainPos      # → 1

# 主正故障（Bit4~5=10 → BYTE6=0x20）
cansend can0 0CFF7C03#00 00 00 00 00 00 20 00
./skes_qt_bridge_tester query bmsData.relayMainPos      # → 2

# 主负闭合（Bit6~7=01 → BYTE6=0x40）
cansend can0 0CFF7C03#00 00 00 00 00 00 40 00
./skes_qt_bridge_tester query bmsData.relayMainNeg      # → 1

# 充电继电器闭合（Bit2~3=01 → BYTE6=0x04）
cansend can0 0CFF7C03#00 00 00 00 00 00 04 00
./skes_qt_bridge_tester query bmsData.relayCharge       # → 1

# 加热继电器闭合（Bit0~1=01 → BYTE6=0x01）
cansend can0 0CFF7C03#00 00 00 00 00 00 01 00
./skes_qt_bridge_tester query bmsData.relayHeat         # → 1

# 主正+主负+充电+加热全部闭合（BYTE6=0x55）
# Bit0~1=01(加热) | Bit2~3=01(充电) | Bit4~5=01(主正) | Bit6~7=01(主负) = 0x01|0x04|0x10|0x40=0x55
cansend can0 0CFF7C03#00 00 00 00 00 00 55 00
./skes_qt_bridge_tester query bmsData.relayHeat         # → 1
./skes_qt_bridge_tester query bmsData.relayCharge       # → 1
./skes_qt_bridge_tester query bmsData.relayMainPos      # → 1
./skes_qt_bridge_tester query bmsData.relayMainNeg      # → 1
```

---

**六、BYTE7 — 继电器状态（DC / 预充）+ ON唤醒**

```bash
# 全部断开（BYTE7=0x00）
cansend can0 0CFF7C03#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query bmsData.relayDC           # → 0
./skes_qt_bridge_tester query bmsData.relayPrecharge    # → 0
./skes_qt_bridge_tester query bmsData.onWakeup          # → 0

# DC继电器闭合（Bit0~1=01 → BYTE7=0x01）
cansend can0 0CFF7C03#00 00 00 00 00 00 00 01
./skes_qt_bridge_tester query bmsData.relayDC           # → 1

# DC继电器故障（Bit0~1=10 → BYTE7=0x02）
cansend can0 0CFF7C03#00 00 00 00 00 00 00 02
./skes_qt_bridge_tester query bmsData.relayDC           # → 2

# 预充继电器闭合（Bit4~5=01 → BYTE7=0x10）
cansend can0 0CFF7C03#00 00 00 00 00 00 00 10
./skes_qt_bridge_tester query bmsData.relayPrecharge    # → 1

# ON档唤醒（Bit6=1 → BYTE7=0x40）
cansend can0 0CFF7C03#00 00 00 00 00 00 00 40
./skes_qt_bridge_tester query bmsData.onWakeup          # → 1

# DC闭合+预充闭合+ON唤醒（BYTE7=0x01|0x10|0x40=0x51）
cansend can0 0CFF7C03#00 00 00 00 00 00 00 51
./skes_qt_bridge_tester query bmsData.relayDC           # → 1
./skes_qt_bridge_tester query bmsData.relayPrecharge    # → 1
./skes_qt_bridge_tester query bmsData.onWakeup          # → 1
```

---

**七、0x0CFF7C03 全字段综合注入**

```bash
# 场景：48V / +100A / SOC=80% / 充电中+直流连接+激活 / 全继电器闭合 / ON唤醒
# BYTE0~1: voltage 实际48.0V → raw=480=0x01E0 → 0xE0 0x01
# BYTE2~3: current 实际+100A → raw=100×10+10000=11000=0x2AF8 → 0xF8 0x2A
# BYTE4: soc=80% → raw=200=0xC8
# BYTE5: bmsSelfCheck=1(0x01)|chargingMode=1(0x04)|chargeConnection=1(0x10)|chargeActivate=1(0x40) = 0x55
# BYTE6: relayHeat=1(0x01)|relayCharge=1(0x04)|relayMainPos=1(0x10)|relayMainNeg=1(0x40) = 0x55
# BYTE7: relayDC=1(0x01)|relayPrecharge=1(0x10)|onWakeup=1(0x40) = 0x51
cansend can0 0CFF7C03#E0 01 F8 2A C8 55 55 51

./skes_qt_bridge_tester query bmsData.voltage           # → 48.0
./skes_qt_bridge_tester query bmsData.current           # → 100.0
./skes_qt_bridge_tester query bmsData.soc               # → 80
./skes_qt_bridge_tester query bmsData.bmsSelfCheck      # → 1
./skes_qt_bridge_tester query bmsData.chargingMode      # → 1
./skes_qt_bridge_tester query bmsData.chargeConnection  # → 1
./skes_qt_bridge_tester query bmsData.chargeActivate    # → 1
./skes_qt_bridge_tester query bmsData.bmsHvOff          # → 0
./skes_qt_bridge_tester query bmsData.relayHeat         # → 1
./skes_qt_bridge_tester query bmsData.relayCharge       # → 1
./skes_qt_bridge_tester query bmsData.relayMainPos      # → 1
./skes_qt_bridge_tester query bmsData.relayMainNeg      # → 1
./skes_qt_bridge_tester query bmsData.relayDC           # → 1
./skes_qt_bridge_tester query bmsData.relayPrecharge    # → 1
./skes_qt_bridge_tester query bmsData.onWakeup          # → 1
```

**预期结果**：

| BYTE5 | bmsSelfCheck | chargingMode | chargeConnection | chargeActivate | bmsHvOff |
|-------|-------------|-------------|-----------------|---------------|---------|
| 0x00 | 0(自检中) | 0(未充) | 0(未连) | 0 | 0 |
| 0x55 | 1(完成) | 1(充中) | 1(直流) | 1 | 0 |
| 0x28 | 0(自检中) | 2(完成) | 2(交流) | 0 | 0 |
| 0x80 | 0 | 0 | 0 | 0 | 1(请求切断) |

| BYTE6 | relayHeat | relayCharge | relayMainPos | relayMainNeg |
|-------|----------|------------|-------------|-------------|
| 0x00 | 0(断) | 0(断) | 0(断) | 0(断) |
| 0x55 | 1(合) | 1(合) | 1(合) | 1(合) |
| 0xAA | 2(故障) | 2(故障) | 2(故障) | 2(故障) |

| BYTE7 | relayDC | relayPrecharge | onWakeup |
|-------|--------|--------------|---------|
| 0x00 | 0(断) | 0(断) | 0 |
| 0x51 | 1(合) | 1(合) | 1 |
| 0x02 | 2(故障) | 0 | 0 |

---

### Frame-24  CAN0 0x0CFF7D03 — BMS 单体电压 / 温度

> 至少需要 6 字节才触发解析。

**协议说明**：

| 字节 | 字节序 | 字段 | 说明 | JSON 路径 |
|------|--------|------|------|-----------|
| BYTE0~1 | 小端 uint16 | cellVoltMax | 最高单体电压，raw×0.001=V（double）| `bmsData.cellVoltMax` |
| BYTE2~3 | 小端 uint16 | cellVoltMin | 最低单体电压，raw×0.001=V（double）| `bmsData.cellVoltMin` |
| BYTE4 | — uint8 | cellTempMax | 最高单体温度，raw−40=℃（偏移-40）| `bmsData.cellTempMax` |
| BYTE5 | — uint8 | cellTempMin | 最低单体温度，raw−40=℃（偏移-40）| `bmsData.cellTempMin` |

**CAN 注入 — 单体电压测试**（换算 V = raw×0.001；反推 raw = V×1000）：
```bash
# cellVoltMax=3.6V(raw=3600=0x0E10 → BYTE0=0x10, BYTE1=0x0E)
# cellVoltMin=3.2V(raw=3200=0x0C80 → BYTE2=0x80, BYTE3=0x0C)
cansend can0 0CFF7D03#10 0E 80 0C 00 00 00 00
./skes_qt_bridge_tester query bmsData.cellVoltMax       # → 3.6
./skes_qt_bridge_tester query bmsData.cellVoltMin       # → 3.2

# cellVoltMax=4.2V(raw=4200=0x1068)，cellVoltMin=3.0V(raw=3000=0x0BB8)
cansend can0 0CFF7D03#68 10 B8 0B 00 00 00 00
./skes_qt_bridge_tester query bmsData.cellVoltMax       # → 4.2
./skes_qt_bridge_tester query bmsData.cellVoltMin       # → 3.0

# 全零（raw=0，0V）
cansend can0 0CFF7D03#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query bmsData.cellVoltMax       # → 0.0
./skes_qt_bridge_tester query bmsData.cellVoltMin       # → 0.0
```

**CAN 注入 — 单体温度测试**（换算 ℃ = raw−40；反推 raw = ℃+40）：
```bash
# cellTempMax=25℃(raw=65=0x41)，cellTempMin=10℃(raw=50=0x32)
cansend can0 0CFF7D03#00 00 00 00 41 32 00 00
./skes_qt_bridge_tester query bmsData.cellTempMax       # → 25
./skes_qt_bridge_tester query bmsData.cellTempMin       # → 10

# cellTempMax=60℃(raw=100=0x64)，cellTempMin=0℃(raw=40=0x28)
cansend can0 0CFF7D03#00 00 00 00 64 28 00 00
./skes_qt_bridge_tester query bmsData.cellTempMax       # → 60
./skes_qt_bridge_tester query bmsData.cellTempMin       # → 0

# 负温边界：cellTempMin=-10℃(raw=30=0x1E)
cansend can0 0CFF7D03#00 00 00 00 28 1E 00 00
./skes_qt_bridge_tester query bmsData.cellTempMax       # → 0
./skes_qt_bridge_tester query bmsData.cellTempMin       # → -10

# 全字段综合：cellVoltMax=4.2V, cellVoltMin=3.0V, cellTempMax=55℃, cellTempMin=20℃
# BYTE0~1小端4200=0x1068, BYTE2~3小端3000=0x0BB8, BYTE4=55+40=95=0x5F, BYTE5=20+40=60=0x3C
cansend can0 0CFF7D03#68 10 B8 0B 5F 3C 00 00
./skes_qt_bridge_tester query bmsData.cellVoltMax       # → 4.2
./skes_qt_bridge_tester query bmsData.cellVoltMin       # → 3.0
./skes_qt_bridge_tester query bmsData.cellTempMax       # → 55
./skes_qt_bridge_tester query bmsData.cellTempMin       # → 20
```

---

### Frame-25  CAN0 0x0CFF7F03 — BMS 绝缘阻值

> 至少需要 8 字节，仅解析 BYTE6~7（小端 uint16），单位 KΩ。

**协议说明**：

| 字节 | 字节序 | 字段 | 单位 | JSON 路径 |
|------|--------|------|------|-----------|
| BYTE6~7 | 小端 uint16 | insulation | KΩ | `bmsData.insulation` |

**CAN 注入**：
```bash
# 绝缘=0 KΩ（短路/故障）
cansend can0 0CFF7F03#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query bmsData.insulation        # → 0

# 绝缘=500 KΩ（raw=500=0x01F4 → BYTE6=0xF4, BYTE7=0x01）
cansend can0 0CFF7F03#00 00 00 00 00 00 F4 01
./skes_qt_bridge_tester query bmsData.insulation        # → 500

# 绝缘=1000 KΩ（raw=1000=0x03E8 → BYTE6=0xE8, BYTE7=0x03）
cansend can0 0CFF7F03#00 00 00 00 00 00 E8 03
./skes_qt_bridge_tester query bmsData.insulation        # → 1000

# 绝缘=65535 KΩ（最大 uint16，BYTE6=0xFF, BYTE7=0xFF）
cansend can0 0CFF7F03#00 00 00 00 00 00 FF FF
./skes_qt_bridge_tester query bmsData.insulation        # → 65535
```

> ⚠️ BYTE0~5 源码未解析，填 `0x00` 即可；绝缘阻值仅取 BYTE6~7。

---

### Frame-26  CAN0 0x18FE7C03 — VCU 上电指令

> 至少需要 1 字节，仅取 BYTE0 Bit0。

**协议说明**：

| 字节 | 位域 | 字段 | 说明 | JSON 路径 |
|------|------|------|------|-----------|
| BYTE0 Bit0 | — | vcuPowerCmd | 0=下电请求/1=上电请求 | `bmsData.vcuPowerCmd` |

**CAN 注入**：
```bash
# 下电请求（Bit0=0，BYTE0=0x00）
cansend can0 18FE7C03#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query bmsData.vcuPowerCmd       # → 0

# 上电请求（Bit0=1，BYTE0=0x01）
cansend can0 18FE7C03#01 00 00 00 00 00 00 00
./skes_qt_bridge_tester query bmsData.vcuPowerCmd       # → 1

# 切换测试：先下电再上电
cansend can0 18FE7C03#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query bmsData.vcuPowerCmd       # → 0
cansend can0 18FE7C03#01 00 00 00 00 00 00 00
./skes_qt_bridge_tester query bmsData.vcuPowerCmd       # → 1
```

---

**tester 逐字段验证**：
```bash
./skes_qt_bridge_tester query bmsData.voltage
./skes_qt_bridge_tester query bmsData.current
./skes_qt_bridge_tester query bmsData.soc
./skes_qt_bridge_tester query bmsData.insulation
./skes_qt_bridge_tester query bmsData.cellVoltMax
./skes_qt_bridge_tester query bmsData.cellVoltMin
./skes_qt_bridge_tester query bmsData.cellTempMax
./skes_qt_bridge_tester query bmsData.cellTempMin
./skes_qt_bridge_tester query bmsData.relayMainPos
./skes_qt_bridge_tester query bmsData.relayMainNeg
./skes_qt_bridge_tester query bmsData.relayPrecharge
./skes_qt_bridge_tester query bmsData.relayDC
./skes_qt_bridge_tester query bmsData.relayHeat
./skes_qt_bridge_tester query bmsData.relayCharge
./skes_qt_bridge_tester query bmsData.bmsHvOff
./skes_qt_bridge_tester query bmsData.chargeActivate
./skes_qt_bridge_tester query bmsData.chargeConnection
./skes_qt_bridge_tester query bmsData.chargingMode
./skes_qt_bridge_tester query bmsData.bmsSelfCheck
./skes_qt_bridge_tester query bmsData.vcuPowerCmd
./skes_qt_bridge_tester query bmsData.onWakeup
```

---

## 十、rfidData — RFID 刷卡信息

> 涉及 2 条 CAN1 帧，所有字段通过 `rfidData.*` 路径查询。
> 数据由 RFID 读卡模块主动上报，无需主动轮询。

---

### Frame-27  CAN1 0x4D1 — RFID 心跳 / 工作 / 故障状态 + 解锁卡号

**协议说明**（至少 5 字节才触发解析）：

| 字节 | 位域 | 字段 | 说明 | JSON 路径 |
|------|------|------|------|-----------|
| BYTE0 Bit0 | — | heartbeatStatus | 0=灯灭/1=灯亮 | `rfidData.heartbeatStatus` |
| BYTE0 Bit1 | — | workStatus | 0=待机/1=工作 | `rfidData.workStatus` |
| BYTE0 Bit2~4 | — | faultStatus | 0=正常/1=解卡失败·IC卡未录入/2=加卡错误·已达上限 | `rfidData.faultStatus` |
| BYTE1~4 | — | unlockCardId | 4字节 UID，十六进制字符串（如 `"12345678"`）| `rfidData.unlockCardId` |
| （副作用）| — | unlockTime | workStatus **0→1 跳变**时自动记录当前时间 | `rfidData.unlockTime` |
| （副作用）| — | sequence | 每收到一帧 0x4D1 自增 1（用于验证帧是否到达）| `rfidData.sequence` |
| （副作用）| — | rfidHistory | 仅在 workStatus 0→1 时新增成功解锁记录；最多 100 条 | `rfidData.rfidHistory` |

> ⚠️ **注意**：`unlockTime` 仅在 workStatus 发生 **0→1 跳变**时更新；重复发送 workStatus=1 不会再次更新时间。

---

**一、BYTE0 Bit0 — heartbeatStatus**

```bash
# 心跳亮（Bit0=1，BYTE0=0x01），卡号全零
cansend can1 4D1#01 00 00 00 00 00 00 00
./skes_qt_bridge_tester query rfidData.heartbeatStatus   # → 1

# 心跳灭（Bit0=0，BYTE0=0x00）
cansend can1 4D1#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query rfidData.heartbeatStatus   # → 0
```

---

**二、BYTE0 Bit1 — workStatus 及 unlockTime 跳变**

```bash
# ── 待机→工作跳变（0→1），unlockTime 应更新 ────────────────
# 先确认处于待机（workStatus=0）
cansend can1 4D1#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query rfidData.workStatus        # → 0
./skes_qt_bridge_tester query rfidData.unlockTime        # → ""（空）

# 发工作状态（Bit1=1，BYTE0=0x02），带卡号 12345678
cansend can1 4D1#02 12 34 56 78 00 00 00
./skes_qt_bridge_tester query rfidData.workStatus        # → 1
./skes_qt_bridge_tester query rfidData.unlockTime        # → 非空时间戳（如 "2026-05-26 10:58:46"）
./skes_qt_bridge_tester query rfidData.unlockCardId      # → "12345678"

# ── 重复发 workStatus=1，unlockTime 不再更新 ─────────────
sleep 2
cansend can1 4D1#02 AB CD EF 01 00 00 00
./skes_qt_bridge_tester query rfidData.unlockTime        # → 与上次相同（未更新）
./skes_qt_bridge_tester query rfidData.unlockCardId      # → "ABCDEF01"（卡号已更新）

# ── 工作→待机（1→0）再跳变回工作（0→1），时间戳应刷新 ────
cansend can1 4D1#00 00 00 00 00 00 00 00   # 归待机
sleep 1
cansend can1 4D1#02 FF FF FF FF 00 00 00   # 再次刷卡进入工作
./skes_qt_bridge_tester query rfidData.unlockTime        # → 新的时间戳（与第一次不同）
./skes_qt_bridge_tester query rfidData.unlockCardId      # → "FFFFFFFF"
```

---

**三、BYTE0 Bit2~4 — faultStatus**

```bash
# faultStatus=0（正常，Bit2~4=000，BYTE0=0x00）
cansend can1 4D1#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query rfidData.faultStatus       # → 0

# faultStatus=1（解卡失败 / IC卡未录入，Bit2=1 → BYTE0=0x04）
cansend can1 4D1#04 00 00 00 00 00 00 00
./skes_qt_bridge_tester query rfidData.faultStatus       # → 1

# faultStatus=2（加卡错误 / 已达最大数，Bit3=1 → BYTE0=0x08）
cansend can1 4D1#08 00 00 00 00 00 00 00
./skes_qt_bridge_tester query rfidData.faultStatus       # → 2

# 心跳亮（Bit0=1）+ 故障=1（Bit2=1）= 0x01|0x04 = 0x05
cansend can1 4D1#05 00 00 00 00 00 00 00
./skes_qt_bridge_tester query rfidData.heartbeatStatus   # → 1
./skes_qt_bridge_tester query rfidData.faultStatus       # → 1
```

**预期结果**：

| BYTE0 Bit2~4 | faultStatus | 含义 |
|-------------|------------|------|
| 000 | 0 | 正常 |
| 001 | 1 | 解卡失败 / IC卡未录入 |
| 010 | 2 | 加卡错误 / 已达最大数 |

---

**四、BYTE1~4 — unlockCardId 卡号格式**

```bash
# 典型卡号（BYTE1=0x12, BYTE2=0x34, BYTE3=0x56, BYTE4=0x78）
cansend can1 4D1#00 12 34 56 78 00 00 00
./skes_qt_bridge_tester query rfidData.unlockCardId      # → "12345678"

# 全 FF 卡号
cansend can1 4D1#00 FF FF FF FF 00 00 00
./skes_qt_bridge_tester query rfidData.unlockCardId      # → "FFFFFFFF"

# 全零卡号
cansend can1 4D1#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query rfidData.unlockCardId      # → "00000000"

# 混合卡号（BYTE1=0xDE, BYTE2=0xAD, BYTE3=0xBE, BYTE4=0xEF）
cansend can1 4D1#00 DE AD BE EF 00 00 00
./skes_qt_bridge_tester query rfidData.unlockCardId      # → "DEADBEEF"
```

**预期结果**：卡号固定为 8 个大写十六进制字符，与 BYTE1~4 顺序一致（非小端转换）。

---

**五、sequence 自增验证**

```bash
# 查询当前基准 sequence
./skes_qt_bridge_tester query rfidData.sequence          # → N（记录当前值）

# 连发 3 帧
cansend can1 4D1#01 00 00 00 00 00 00 00
cansend can1 4D1#01 00 00 00 00 00 00 00
cansend can1 4D1#01 00 00 00 00 00 00 00

# 验证 sequence 增加了 3
./skes_qt_bridge_tester query rfidData.sequence          # → N+3
```

---

### Frame-28  CAN1 0x4D2 — RFID 解锁时长

**协议说明**（至少 6 字节才触发解析）：

| 字节 | 字节序 | 字段 | 说明 | JSON 路径 |
|------|--------|------|------|-----------|
| BYTE4~5 | 小端 uint16 | unlockDuration | 前一次解卡时长 | `rfidData.unlockDuration` |

> ⚠️ BYTE0~3 源码未解析，填 `0x00` 即可；仅取 BYTE4~5。

**CAN 注入**：
```bash
# unlockDuration=0（BYTE4=0x00, BYTE5=0x00）
cansend can1 4D2#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query rfidData.unlockDuration    # → 0

# unlockDuration=30（BYTE4=0x1E, BYTE5=0x00）
cansend can1 4D2#00 00 00 00 1E 00 00 00
./skes_qt_bridge_tester query rfidData.unlockDuration    # → 30

# unlockDuration=255（BYTE4=0xFF, BYTE5=0x00）
cansend can1 4D2#00 00 00 00 FF 00 00 00
./skes_qt_bridge_tester query rfidData.unlockDuration    # → 255

# unlockDuration=1000（0x03E8 小端，BYTE4=0xE8, BYTE5=0x03）
cansend can1 4D2#00 00 00 00 E8 03 00 00
./skes_qt_bridge_tester query rfidData.unlockDuration    # → 1000

# unlockDuration=65535（uint16 最大值，BYTE4=0xFF, BYTE5=0xFF）
cansend can1 4D2#00 00 00 00 FF FF 00 00
./skes_qt_bridge_tester query rfidData.unlockDuration    # → 65535
```

---

### 综合联动测试 — 完整刷卡流程

```bash
# 1. 初始状态：心跳亮，待机，无故障
cansend can1 4D1#01 00 00 00 00 00 00 00
./skes_qt_bridge_tester query rfidData.heartbeatStatus   # → 1
./skes_qt_bridge_tester query rfidData.workStatus        # → 0
./skes_qt_bridge_tester query rfidData.faultStatus       # → 0

# 2. 刷卡成功，进入工作状态（0→1 跳变），卡号 A1B2C3D4
cansend can1 4D1#03 A1 B2 C3 D4 00 00 00   # 0x01|0x02=0x03（心跳亮+工作）
./skes_qt_bridge_tester query rfidData.workStatus        # → 1
./skes_qt_bridge_tester query rfidData.unlockCardId      # → "A1B2C3D4"
./skes_qt_bridge_tester query rfidData.unlockTime        # → 非空时间戳

# 3. 收到解锁时长（60单位）；同时回填最近一条历史记录
cansend can1 4D2#00 00 00 00 3C 00 00 00
./skes_qt_bridge_tester query rfidData.unlockDuration    # → 60
./skes_qt_bridge_tester query rfidData.rfidHistory       # → 最后一项 cardId=A1B2C3D4、unlockDuration=60

# 4. 工作结束，回到待机
cansend can1 4D1#01 00 00 00 00 00 00 00
./skes_qt_bridge_tester query rfidData.workStatus        # → 0

# 5. 刷卡失败（IC卡未录入，故障=1，心跳亮），workStatus 保持 0
cansend can1 4D1#05 00 00 00 00 00 00 00   # 0x01|0x04=0x05（心跳亮+故障1）
./skes_qt_bridge_tester query rfidData.faultStatus       # → 1
./skes_qt_bridge_tester query rfidData.workStatus        # → 0
./skes_qt_bridge_tester query rfidData.unlockTime        # → 与步骤2相同（未跳变，不更新）
```

**tester 逐字段验证**：
```bash
./skes_qt_bridge_tester query rfidData.heartbeatStatus
./skes_qt_bridge_tester query rfidData.workStatus
./skes_qt_bridge_tester query rfidData.faultStatus
./skes_qt_bridge_tester query rfidData.sequence
./skes_qt_bridge_tester query rfidData.unlockCardId
./skes_qt_bridge_tester query rfidData.unlockTime
./skes_qt_bridge_tester query rfidData.unlockDuration
./skes_qt_bridge_tester query rfidData.rfidHistory
```

### 刷卡历史验证

> `rfidHistory` 只记录成功解锁事件（CAN1 0x4D1 的 `workStatus` 0→1），而非每个心跳帧。查询结果是 JSON 字符串，需要观察数组最后一项。

```bash
# 前置：先发待机帧，确保下一帧可构成 0→1 跳变
cansend can1 4D1#01 00 00 00 00 00 00 00

# 第 1 次成功刷卡，随后补时长 60
cansend can1 4D1#03 A1 B2 C3 D4 00 00 00
cansend can1 4D2#00 00 00 00 3C 00 00 00
./skes_qt_bridge_tester query rfidData.rfidHistory
# → 数组最后一项：{"cardId":"A1B2C3D4","unlockTime":"...","unlockDuration":60}

# 重复工作帧只更新实时 RFID 状态，不新增历史记录
cansend can1 4D1#03 A1 B2 C3 D4 00 00 00
./skes_qt_bridge_tester query rfidData.rfidHistory
# → 历史条数不变

# 归待机后再解锁第 2 张卡，历史新增一项
cansend can1 4D1#01 00 00 00 00 00 00 00
cansend can1 4D1#03 11 22 33 44 00 00 00
cansend can1 4D2#00 00 00 00 78 00 00 00
./skes_qt_bridge_tester query rfidData.rfidHistory
# → 数组最后两项依次为 A1B2C3D4/60、11223344/120
```

**持久化验证**：记录历史后重启 dataengine，再执行 `query rfidData.rfidHistory`，记录应仍存在。保留上限为 100 条，写入第 101 条时最旧记录应被淘汰。

---

## 十一、faultInfo 补充 — MCU 故障码

> 本节补充来自行走 / 油泵 MCU 的故障码帧，写入 `faultInfo.*` 并同步更新 `faultInfo.faultList`。

---

### Frame-29  CAN0 0x0CFF0208 — 行走MCU故障码

**协议说明**（至少 6 字节才触发解析）：

| 字节 | 字节序 | 字段 | 说明 | JSON 路径 |
|------|--------|------|------|-----------|
| BYTE4~5 | 小端 uint16 | driveMcuErr | 0=正常，非0=故障码 | `faultInfo.driveMcuErr` |

**CAN 注入**：
```bash
# driveMcuErr=0（无故障，BYTE4=0x00, BYTE5=0x00）
cansend can0 0CFF0208#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query faultInfo.driveMcuErr      # → 0

# driveMcuErr=1（BYTE4=0x01, BYTE5=0x00）
cansend can0 0CFF0208#00 00 00 00 01 00 00 00
./skes_qt_bridge_tester query faultInfo.driveMcuErr      # → 1

# driveMcuErr=100（BYTE4=0x64, BYTE5=0x00）
cansend can0 0CFF0208#00 00 00 00 64 00 00 00
./skes_qt_bridge_tester query faultInfo.driveMcuErr      # → 100

# driveMcuErr=1000（0x03E8 小端, BYTE4=0xE8, BYTE5=0x03）
cansend can0 0CFF0208#00 00 00 00 E8 03 00 00
./skes_qt_bridge_tester query faultInfo.driveMcuErr      # → 1000

# 恢复正常
cansend can0 0CFF0208#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query faultInfo.driveMcuErr      # → 0
```

> ⚠️ BYTE0~3 源码未解析，填 `0x00` 即可。非0 故障码同时会被 `updateFaultEntry()` 写入 `faultInfo.faultList`，恢复 0 后从 faultList 移除。

---

### Frame-30  CAN0 0x0CFF0209 — 油泵MCU故障码

**协议说明**（至少 6 字节才触发解析）：

| 字节 | 字节序 | 字段 | 说明 | JSON 路径 |
|------|--------|------|------|-----------|
| BYTE4~5 | 小端 uint16 | pumpMcuErr | 0=正常，非0=故障码 | `faultInfo.pumpMcuErr` |

**CAN 注入**：
```bash
# pumpMcuErr=0（无故障）
cansend can0 0CFF0209#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query faultInfo.pumpMcuErr       # → 0

# pumpMcuErr=2（BYTE4=0x02, BYTE5=0x00）
cansend can0 0CFF0209#00 00 00 00 02 00 00 00
./skes_qt_bridge_tester query faultInfo.pumpMcuErr       # → 2

# pumpMcuErr=500（0x01F4 小端, BYTE4=0xF4, BYTE5=0x01）
cansend can0 0CFF0209#00 00 00 00 F4 01 00 00
./skes_qt_bridge_tester query faultInfo.pumpMcuErr       # → 500

# 恢复正常
cansend can0 0CFF0209#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query faultInfo.pumpMcuErr       # → 0
```

---

### faultInfo.faultList / faultCount / maintenanceAlert — 派生字段验证

> 以上各路故障码（userErr/vcuErr/steerMcuErr/driveMcuErr/pumpMcuErr/bmsErr）注入后，`updateFaultEntry()` 自动维护 `faultInfo.faultList` 数组和 `faultInfo.faultCount`。

```bash
# 1. 注入多路故障，观察 faultList 聚合
cansend can1 32D#00 05 00 00 00 00 00 00         # userErr=5（故障类型1）
cansend can1 32E#00 03 00 00 00 00 00 00         # vcuErr=3（故障类型2）
cansend can0 0CFF0208#00 00 00 00 02 00 00 00    # driveMcuErr=2（故障类型3）
# bmsErr=2（byte3 bit0~1=2，一级故障；其余字节随意非零触发）
cansend can0 0CFF8003#01 00 00 02 00 00 00 00    # bmsErr=2（故障类型6）

./skes_qt_bridge_tester query faultInfo.userErr       # → 5
./skes_qt_bridge_tester query faultInfo.vcuErr        # → 3
./skes_qt_bridge_tester query faultInfo.driveMcuErr   # → 2
./skes_qt_bridge_tester query faultInfo.bmsErr        # → 2
./skes_qt_bridge_tester query faultInfo.faultList     # → 含4条故障记录的数组
./skes_qt_bridge_tester query faultInfo.faultCount    # → 4

# 2. 清除其中一路
cansend can1 32D#00 00 00 00 00 00 00 00         # userErr=0（清除）
./skes_qt_bridge_tester query faultInfo.faultList     # → 剩3条
./skes_qt_bridge_tester query faultInfo.faultCount    # → 3

# 3. 全部清除
cansend can1 32E#00 00 00 00 00 00 00 00
cansend can0 0CFF0208#00 00 00 00 00 00 00 00
cansend can0 0CFF8003#00 00 00 00 00 00 00 00    # bmsErr=0（全字节清零）
./skes_qt_bridge_tester query faultInfo.faultList     # → []
./skes_qt_bridge_tester query faultInfo.faultCount    # → 0
```

**保养提示与清除验证**（首次 300h，后续固定每 1200h）：

> 前置：使用新的/已复位的持久化数据，使 `faultInfo.nextMaintenanceHours=300`。若设备已有保养状态，请先查询该字段，并把总工时设置到该节点或更高值。

```bash
# 1. 总工时达到首次节点，触发并锁存保养提示
./skes_qt_bridge_tester set_total_work_hours 300
sleep 1
./skes_qt_bridge_tester query faultInfo.maintenanceAlert      # → true
./skes_qt_bridge_tester query faultInfo.maintenancePending    # → true
./skes_qt_bridge_tester query faultInfo.nextMaintenanceHours  # → 300
./skes_qt_bridge_tester query faultInfo.faultList             # → 含 type="maintenanceAlert"、code=8000

# 2. 即使等待多个定时周期，提示与故障条目仍保持一致
sleep 2
./skes_qt_bridge_tester query faultInfo.maintenanceAlert      # → true
./skes_qt_bridge_tester query faultInfo.faultList             # → 仍含保养提示条目

# 3. 模拟 UI 点击“清除”
./skes_qt_bridge_tester clear_maintenance
./skes_qt_bridge_tester query faultInfo.maintenanceAlert      # → false
./skes_qt_bridge_tester query faultInfo.maintenancePending    # → false
./skes_qt_bridge_tester query faultInfo.faultList             # → 不含 maintenanceAlert 条目
./skes_qt_bridge_tester query faultInfo.nextMaintenanceHours  # → 1500
```

**超期清除验证**：若当前总工时为 2800h 时清除，下一节点应推进至 3900h，而不是再次提示 300/1500/2700h。重启 dataengine 后，清除状态和下一节点均应保持。

---

## 十二、versionInfo 补充 — MCU版本 / BMS版本 / VIN码

---

### Frame-31  CAN0 0x0CFF0308 — 行走MCU软件版本

**协议说明**（至少 3 字节）：

| 字节 | 字段 | 说明 | JSON 路径 |
|------|------|------|-----------|
| BYTE2 | driveMcuSoftVersion | U8 整数版本号 | `versionInfo.driveMcuSoftVersion` |

```bash
# 版本=0（BYTE2=0x00）
cansend can0 0CFF0308#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query versionInfo.driveMcuSoftVersion   # → 0

# 版本=1（BYTE2=0x01）
cansend can0 0CFF0308#00 00 01 00 00 00 00 00
./skes_qt_bridge_tester query versionInfo.driveMcuSoftVersion   # → 1

# 版本=26（BYTE2=0x1A）
cansend can0 0CFF0308#00 00 1A 00 00 00 00 00
./skes_qt_bridge_tester query versionInfo.driveMcuSoftVersion   # → 26

# 版本=255（BYTE2=0xFF）
cansend can0 0CFF0308#00 00 FF 00 00 00 00 00
./skes_qt_bridge_tester query versionInfo.driveMcuSoftVersion   # → 255
```

---

### Frame-32  CAN0 0x0CFF030A — 转向MCU软件版本 / 转向电机温度 / IGBT温度

**协议说明**（软件版本至少 3 字节，温度需至少 7 字节）：

| 字节 | 字段 | 说明 | JSON 路径 |
|------|------|------|-----------|
| BYTE2 | steerMcuSoftVersion | U8 整数版本号 | `versionInfo.steerMcuSoftVersion` |
| BYTE5 | strMotorTemp | raw−40，分辨率1，℃ | `runStatus.strMotorTemp` |
| BYTE6 | strIGBTTemp | raw−40，分辨率1，℃ | `runStatus.strIGBTTemp` |

> ⚠️ BYTE5/BYTE6 温度字段需 data_length >= 7 才解析；帧只有 3 字节时仅更新软件版本，温度保持旧值。

**CAN 注入 — 软件版本**：
```bash
# 版本=5（BYTE2=0x05），温度字节=0（raw 0 → -40℃）
cansend can0 0CFF030A#00 00 05 00 00 00 00 00
./skes_qt_bridge_tester query versionInfo.steerMcuSoftVersion   # → 5

# 版本=100（BYTE2=0x64）
cansend can0 0CFF030A#00 00 64 00 00 00 00 00
./skes_qt_bridge_tester query versionInfo.steerMcuSoftVersion   # → 100
```

**CAN 注入 — 转向电机温度 / IGBT温度（BYTE5/BYTE6）**：
```bash
# strMotorTemp=25℃（raw=65=0x41），strIGBTTemp=30℃（raw=70=0x46），版本=5
cansend can0 0CFF030A#00 00 05 00 00 41 46 00
./skes_qt_bridge_tester query runStatus.strMotorTemp            # → 25
./skes_qt_bridge_tester query runStatus.strIGBTTemp             # → 30

# strMotorTemp=0℃（raw=40=0x28），strIGBTTemp=0℃
cansend can0 0CFF030A#00 00 05 00 00 28 28 00
./skes_qt_bridge_tester query runStatus.strMotorTemp            # → 0
./skes_qt_bridge_tester query runStatus.strIGBTTemp             # → 0

# strMotorTemp=80℃（raw=120=0x78），strIGBTTemp=60℃（raw=100=0x64）
cansend can0 0CFF030A#00 00 05 00 00 78 64 00
./skes_qt_bridge_tester query runStatus.strMotorTemp            # → 80
./skes_qt_bridge_tester query runStatus.strIGBTTemp             # → 60

# strMotorTemp=-10℃（raw=30=0x1E），strIGBTTemp=-10℃
cansend can0 0CFF030A#00 00 05 00 00 1E 1E 00
./skes_qt_bridge_tester query runStatus.strMotorTemp            # → -10
./skes_qt_bridge_tester query runStatus.strIGBTTemp             # → -10
```

---

### Frame-33  CAN0 0x0CFF0309 — 油泵MCU软件版本

**协议说明**（至少 3 字节）：

| 字节 | 字段 | 说明 | JSON 路径 |
|------|------|------|-----------|
| BYTE2 | pumpMcuSoftVersion | U8 整数版本号 | `versionInfo.pumpMcuSoftVersion` |

```bash
# 版本=3（BYTE2=0x03）
cansend can0 0CFF0309#00 00 03 00 00 00 00 00
./skes_qt_bridge_tester query versionInfo.pumpMcuSoftVersion    # → 3

# 版本=200（BYTE2=0xC8）
cansend can0 0CFF0309#00 00 C8 00 00 00 00 00
./skes_qt_bridge_tester query versionInfo.pumpMcuSoftVersion    # → 200
```

---

### Frame-34  CAN0 0x0CFF8203 — BMS软件版本

**协议说明**（至少 5 字节）：

| 字节 | 字段 | 说明 | JSON 路径 |
|------|------|------|-----------|
| BYTE4 | bmsSoftVersion | U8 整数版本号 | `versionInfo.bmsSoftVersion` |

```bash
# 版本=0（BYTE4=0x00）
cansend can0 0CFF8203#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query versionInfo.bmsSoftVersion        # → 0

# 版本=10（BYTE4=0x0A）
cansend can0 0CFF8203#00 00 00 00 0A 00 00 00
./skes_qt_bridge_tester query versionInfo.bmsSoftVersion        # → 10

# 版本=255（BYTE4=0xFF）
cansend can0 0CFF8203#00 00 00 00 FF 00 00 00
./skes_qt_bridge_tester query versionInfo.bmsSoftVersion        # → 255
```

---

### Frame-35~37  CAN1 0x33B + 0x33C + 0x33D — 车辆VIN码 + VCU工时

**协议说明**（三帧拼合，各帧 8 字节，BYTE0 为序号）：

| 帧 ID | BYTE0（序号）| 字段 | 说明 | JSON 路径 |
|-------|-------------|------|------|-----------|
| 0x33B | 7 | VIN 第1~7位 | BYTE1~7 ASCII | `versionInfo.vehicleVin`（三帧全收后写入）|
| 0x33C | 8 | VIN 第8~14位 | BYTE1~7 ASCII | 同上 |
| 0x33D | 9 | VIN 第15~17位 | BYTE1~3 ASCII | 同上 |
| 0x33D | 9 | vcuWorktime | BYTE6 U8（小时，分辨率1）| `versionInfo.vcuWorktime` |

> ⚠️ 三帧全部收到（序号 7/8/9 均匹配）后才写入 vehicleVin，顺序无关但必须齐全。

**CAN 注入 — VIN "LSVAC5431MW000001"**（17位，ASCII）：
```bash
# 0x33B：BYTE0=7，BYTE1~7 = 'L','S','V','A','C','5','4'
cansend can1 33B#07 4C 53 56 41 43 35 34

# 0x33C：BYTE0=8，BYTE1~7 = '3','1','M','W','0','0','0'
cansend can1 33C#08 33 31 4D 57 30 30 30

# 0x33D：BYTE0=9，BYTE1~3 = '0','0','1'；BYTE6=vcuWorktime=50h
cansend can1 33D#09 30 30 31 00 00 32 00

./skes_qt_bridge_tester query versionInfo.vehicleVin     # → "LSVAC5431MW000001"
./skes_qt_bridge_tester query versionInfo.vcuWorktime    # → 50
```

**CAN 注入 — 乱序测试（先发33D再发33B和33C）**：
```bash
cansend can1 33D#09 30 30 31 00 00 0A 00    # vcuWorktime=10
cansend can1 33B#07 4C 53 56 41 43 35 34
cansend can1 33C#08 33 31 4D 57 30 30 30
./skes_qt_bridge_tester query versionInfo.vehicleVin     # → "LSVAC5431MW000001"（乱序仍正确）
./skes_qt_bridge_tester query versionInfo.vcuWorktime    # → 10
```

**CAN 注入 — 全零VIN（全 ASCII '0'=0x30）**：
```bash
cansend can1 33B#07 30 30 30 30 30 30 30
cansend can1 33C#08 30 30 30 30 30 30 30
cansend can1 33D#09 30 30 30 00 00 00 00
./skes_qt_bridge_tester query versionInfo.vehicleVin     # → "00000000000000000"
./skes_qt_bridge_tester query versionInfo.vcuWorktime    # → 0
```

**tester 逐字段验证**：
```bash
./skes_qt_bridge_tester query versionInfo.driveMcuSoftVersion
./skes_qt_bridge_tester query versionInfo.steerMcuSoftVersion
./skes_qt_bridge_tester query versionInfo.pumpMcuSoftVersion
./skes_qt_bridge_tester query versionInfo.bmsSoftVersion
./skes_qt_bridge_tester query versionInfo.vehicleVin
./skes_qt_bridge_tester query versionInfo.vcuWorktime
```

---

### Frame-38  CAN0 0x0CFF8003 — BMS故障状态（故障类型6）

**协议说明**（至少 7 字节，扩展帧）：

| 字节 | 位 | 说明 | JSON 路径 |
|------|-----|------|-----------|
| byte0 | bit0~1 | BMSERR1 单体电压过高 | — |
| byte0 | bit2~3 | BMSERR2 单体电压过低 | — |
| byte0 | bit4~5 | BMSERR3 总压过高 | — |
| byte0 | bit6~7 | BMSERR4 总压过低 | — |
| byte1 | bit0~7 | BMSERR5~8 温度/压差类 | — |
| byte2 | bit0~7 | BMSERR9~12 SOC/绝缘/功率类 | — |
| byte3 | bit0~1 | **总故障状态**（0正常/1预警/2一级/3二级）| `faultInfo.bmsErr` |
| byte3 | bit2~7 | BMSERR14~19 单项故障 | — |
| byte4 | bit0~7 | BMSERR20~27 通信/检测类 | — |
| byte5 | bit0~7 | BMSERR28~35 继电器类 | — |
| byte6 | bit0~7 | BMSERR36~43 传感器/充电类 | — |

**聚合规则**：byte0~byte6 任意位非零 → 有故障；`bmsErr` 取 byte3 bit0~1 作为严重等级（0~3）；总故障状态=0 但其他字节非零时，`bmsErr` 兜底置 1。全字节清零则故障自动恢复（退出 faultList）。

**CAN 注入 — 有故障**：
```bash
# byte3 bit0~1=1（预警），其他字节=0 → bmsErr=1
cansend can0 0CFF8003#00 00 00 01 00 00 00 00
./skes_qt_bridge_tester query faultInfo.bmsErr     # → 1
./skes_qt_bridge_tester query faultInfo.faultList  # → 含 type=6 BMS故障
./skes_qt_bridge_tester query faultInfo.faultCount # → 1

# byte3 bit0~1=2（一级故障），byte0 也有非零位
cansend can0 0CFF8003#01 00 00 02 00 00 00 00
./skes_qt_bridge_tester query faultInfo.bmsErr     # → 2

# byte3 bit0~1=3（二级故障），多字节非零
cansend can0 0CFF8003#FF 00 00 03 00 00 00 00
./skes_qt_bridge_tester query faultInfo.bmsErr     # → 3

# byte3=0 但 byte0 非零（兜底逻辑）→ bmsErr=1
cansend can0 0CFF8003#01 00 00 00 00 00 00 00
./skes_qt_bridge_tester query faultInfo.bmsErr     # → 1
```

**CAN 注入 — 故障恢复**：
```bash
# 全字节清零 → bmsErr=0，退出 faultList
cansend can0 0CFF8003#00 00 00 00 00 00 00 00
./skes_qt_bridge_tester query faultInfo.bmsErr     # → 0
./skes_qt_bridge_tester query faultInfo.faultList  # → 不含 type=6
./skes_qt_bridge_tester query faultInfo.faultCount # → 0（无其他故障时）
```

**预期结果**：

| byte3 bit0~1 | 其他字节 | bmsErr | 故障列表 |
|---|---|---|---|
| 0 | 全0 | 0 | 无 BMS 条目 |
| 0 | 任意非零 | 1 | 有 BMS 条目（code=1） |
| 1 | 任意 | 1 | 有 BMS 条目（code=1，预警）|
| 2 | 任意 | 2 | 有 BMS 条目（code=2，一级）|
| 3 | 任意 | 3 | 有 BMS 条目（code=3，二级）|

---

## 十三、命令驱动字段（Qt下发控制，不经过cansend注入）

> 以下字段由 Qt 通过 `cmd_control` 写入，tester 用对应 `set_*` 命令模拟 Qt 下发；VEH 处理后写 CAN 0x31D 帧并回写 JSON，通过 `query` 读回验证。

---

### mainScreen 命令驱动字段

| 字段 | tester 命令 | CAN 下发 | 说明 |
|------|------------|----------|------|
| cabinFan | `set_cabin_fan <0\|1>` | 0x31D BYTE0 | 驾驶室风扇开关 |
| headLight | `set_head_light <0\|1>` | 0x31D BYTE0 | 前大灯开关 |
| rearLight | `set_rear_light <0\|1>` | 0x31D BYTE0 | 后大灯开关 |
| forkMonitor | `set_fork_monitor <0\|1>` | 0x31D BYTE0 | 货叉监控开关 |
| oneTouchLeveling | `set_one_touch_leveling <0\|1>` | 0x31D BYTE1 Bit5 | 一键水平触发 |

```bash
# 驾驶室风扇开
./skes_qt_bridge_tester set_cabin_fan 1
./skes_qt_bridge_tester query mainScreen.cabinFan        # → 1

# 前大灯开
./skes_qt_bridge_tester set_head_light 1
./skes_qt_bridge_tester query mainScreen.headLight       # → 1

# 后大灯开
./skes_qt_bridge_tester set_rear_light 1
./skes_qt_bridge_tester query mainScreen.rearLight       # → 1

# 货叉监控开
./skes_qt_bridge_tester set_fork_monitor 1
./skes_qt_bridge_tester query mainScreen.forkMonitor     # → 1

# 一键水平（脉冲型，无记忆，VEH 500ms后自动归0）
./skes_qt_bridge_tester set_one_touch_leveling 1
./skes_qt_bridge_tester query mainScreen.oneTouchLeveling  # → 1（立即）
sleep 1
./skes_qt_bridge_tester query mainScreen.oneTouchLeveling  # → 0（归零后）

# 全部关闭
./skes_qt_bridge_tester set_cabin_fan 0
./skes_qt_bridge_tester set_head_light 0
./skes_qt_bridge_tester set_rear_light 0
./skes_qt_bridge_tester set_fork_monitor 0
```

---

### testCalibration / system 命令驱动字段

```bash
# 软件使能（无记忆，重启恢复0；CAN0 0x31D BYTE7 bit0）
./skes_qt_bridge_tester set_software_enable 1
./skes_qt_bridge_tester query testCalibration.softwareEnable   # → 1
./skes_qt_bridge_tester set_software_enable 0
./skes_qt_bridge_tester query testCalibration.softwareEnable   # → 0

# 禁用刷卡（掉电保持；仪表自身逻辑，无CAN帧）
./skes_qt_bridge_tester set_disable_card_swipe 1
./skes_qt_bridge_tester query testCalibration.disableCardSwipe # → 1（开机不弹刷卡窗）
./skes_qt_bridge_tester set_disable_card_swipe 0
./skes_qt_bridge_tester query testCalibration.disableCardSwipe # → 0

# 显示LOGO（掉电保持；仪表自身逻辑，无CAN帧）
./skes_qt_bridge_tester set_display_logo 1
./skes_qt_bridge_tester query testCalibration.displayLogo      # → 1（显示SANY LOGO）
./skes_qt_bridge_tester set_display_logo 0
./skes_qt_bridge_tester query testCalibration.displayLogo      # → 0

# 临时刷卡（掉电保持；CAN0 0x4D1 byte0 bit1，触发式发送）
./skes_qt_bridge_tester set_temp_card_swipe 1
./skes_qt_bridge_tester query testCalibration.tempCardSwipe    # → 1（已发0x4D1 bit1=1）
./skes_qt_bridge_tester set_temp_card_swipe 0
./skes_qt_bridge_tester query testCalibration.tempCardSwipe    # → 0（已发0x4D1 bit1=0）

# 屏幕亮度（掉电保持，0%→硬件最低亮度/不黑屏，100%→最亮）
./skes_qt_bridge_tester set_brightness 50
./skes_qt_bridge_tester query screenBrightness                 # → 50
./skes_qt_bridge_tester set_brightness 0
./skes_qt_bridge_tester query screenBrightness                 # → 0（硬件最低，非黑屏）
./skes_qt_bridge_tester set_brightness 100
./skes_qt_bridge_tester query screenBrightness                 # → 100

# 语言切换（掉电保持，0=中文/1=英文）
./skes_qt_bridge_tester set_language 0
./skes_qt_bridge_tester query language                         # → 0（中文）
./skes_qt_bridge_tester set_language 1
./skes_qt_bridge_tester query language                         # → 1（英文）
```

> ⚠️ `screenBrightness` / `language` 在 ARM 设备上掉电保持，重启后恢复设置值；x86 开发机上亮度写 sysfs 节点静默失败，语言同样生效但无实际界面切换。

---

### 总里程 / 总工时 修正命令（持久化数值覆盖）

> Qt 通过 `cmd_control`（`data_type=double`）下发修正值，VEH 调用 `setTotalMileage()` / `setTotalWorkHours()` **直接覆盖**累积值并**立即落盘** `/data/forklift_persistent.json`，重启后保持。区别于 Frame-21/22 的车速/工时自动累积：此命令是人工校准/换表后的强制改写。

| 字段 | tester 命令 | 单位 | 联动写入字段 |
|------|------------|------|--------------|
| totalMileage | `set_total_mileage <km>` | km | `mainScreen.totalMileage` + `versionInfo.meterTotalMileage` |
| totalWorkHours | `set_total_work_hours <hours>` | h | `mainScreen.totalWorkHours` + `versionInfo.meterWorkHours` |

**协议说明**（源码 `SkesQtBridge.cpp:595-623` / `RealtimeStatusManager.cpp:2189-2211`）：
- `data_type` 必须为 `double`（`set_total_mileage` / `set_total_work_hours` 已内置）
- 负值非法：桥层 `SkesQtBridge.cpp:597/612` 直接返回 `result=-3` 且不调用 setter（值不变）；setter 内部另有 `<0 → 0` 兜底
- 修正会**同时联动写入** `mainScreen.*` 与 `versionInfo.meter*` 两个字段，二者应一致
- 写入即同步持久化（非 60s 定时），断电不丢

**总里程修正测试**：
```bash
# 修正为 1234.50 km
./skes_qt_bridge_tester set_total_mileage 1234.5
./skes_qt_bridge_tester query mainScreen.totalMileage          # → 1234.5
./skes_qt_bridge_tester query versionInfo.meterTotalMileage    # → 1234.5（联动一致）

# 修正为 0（清零换表）
./skes_qt_bridge_tester set_total_mileage 0
./skes_qt_bridge_tester query mainScreen.totalMileage          # → 0.0

# 负值非法（被桥层拒绝，result=-3，值保持不变）
./skes_qt_bridge_tester set_total_mileage 100        # 先置 100
./skes_qt_bridge_tester set_total_mileage -50        # → 返回 result=-3
./skes_qt_bridge_tester query mainScreen.totalMileage          # → 100.0（未被改写）
```

**总工时修正测试**：
```bash
# 修正为 567.80 h
./skes_qt_bridge_tester set_total_work_hours 567.8
./skes_qt_bridge_tester query mainScreen.totalWorkHours        # → 567.8
./skes_qt_bridge_tester query versionInfo.meterWorkHours       # → 567.8（联动一致）

# 修正为 0
./skes_qt_bridge_tester set_total_work_hours 0
./skes_qt_bridge_tester query mainScreen.totalWorkHours        # → 0.0

# 负值非法（result=-3，值不变）
./skes_qt_bridge_tester set_total_work_hours 300
./skes_qt_bridge_tester set_total_work_hours -10              # → result=-3
./skes_qt_bridge_tester query mainScreen.totalWorkHours        # → 300.0
```

**持久化验证（修正值掉电保持）**：
```bash
# 1. 修正并确认
./skes_qt_bridge_tester set_total_mileage 8888.0
./skes_qt_bridge_tester set_total_work_hours 999.0

# 2. 立即重启（无需等 60s，修正命令已同步落盘）
kill $(pgrep vehicleDataEngine)
sleep 3
cd ~/cameras && ./vehicleDataEngine &
sleep 2

# 3. 查询恢复值
./skes_qt_bridge_tester query mainScreen.totalMileage         # → 8888.0（持久化恢复）
./skes_qt_bridge_tester query mainScreen.totalWorkHours       # → 999.0（持久化恢复）
```

**预期结果**：

| 命令 | 入参 | mainScreen | versionInfo 联动 | 落盘 | result |
|------|------|-----------|------------------|------|--------|
| set_total_mileage | ≥0 | totalMileage=入参 | meterTotalMileage=入参 | 立即 | 0 |
| set_total_mileage | <0 | 不变 | 不变 | 否 | -3 |
| set_total_work_hours | ≥0 | totalWorkHours=入参 | meterWorkHours=入参 | 立即 | 0 |
| set_total_work_hours | <0 | 不变 | 不变 | 否 | -3 |

> ⚠️ 修正后若车辆继续行驶/工作，Frame-21/22 的自动累积会在修正值基础上**继续叠加**（修正只改基准，不停止累积）。

