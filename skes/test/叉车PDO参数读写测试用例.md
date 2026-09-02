# 叉车 驱动设置/泵设置 PDO 参数读写测试用例

> **测试环境**：`vehicleDataEngine` 进程运行中，`skes-relay` 运行中
> **测试工具**：`skes_qt_bridge_tester`（以下简称 **tester**）
> **PDO 帧通道**：CAN0（源码 `can_channel=0`，即硬件 CAN1 口，注意接线对应关系）

---

## 前置准备

### CAN 总线初始化

```bash
ip link set can0 type can bitrate 250000
ip link set can0 up
ip link show can0   # 确认状态含 <UP,LOWER_UP>
```

### 终端布局（三终端模式）

```
终端A（监听）：candump can0 | grep -E "111|222"
终端B（发命令）：./skes_qt_bridge_tester set_drive_param / query ...
终端C（模拟VCU，无VCU时使用）：cansend can0 222#...
```

仅验证推送时可用两终端：
```bash
# 终端1：持续监听
./skes_qt_bridge_tester --full subscribe
# 终端2：执行测试命令
```

---

## 协议说明

### 帧格式速查

**0x111 发出帧**（dataengine → VCU，8字节标准帧）：

| 字节 | 字段名 | 读命令（cmd=1）| 写命令（cmd=0）|
|------|--------|---------------|---------------|
| BYTE0 | cmd | `01`（读）| `00`（写）|
| BYTE1 | addr | 参数地址 1~30 | 参数地址 1~30 |
| BYTE2 | val_L | `00` | U32 低字节 |
| BYTE3 | val_M1 | `00` | U32 次低字节 |
| BYTE4 | val_M2 | `00` | U32 次高字节 |
| BYTE5 | val_H | `00` | U32 高字节 |
| BYTE6 | — | `00` | `00` |
| BYTE7 | enable | 帧1=`01`，帧2=`00` | 帧1=`01`，帧2=`00` |

**0x222 应答帧**（VCU → dataengine，8字节）：

| 字节 | 写应答 | 读应答 |
|------|--------|--------|
| BYTE0 | `00`（写）| `01`（读）|
| BYTE1 | addr | addr |
| BYTE2~5 | 忽略 | 返回值（U32 小端）|
| BYTE6 | `00` | `00` |
| BYTE7 | `00`=成功 / `01`=失败 | 忽略 |

> ⚠️ **双帧规则**：每次操作必须连续发两帧，间隔 50ms，仅 BYTE7 不同（帧1=1，帧2=0）
> ⚠️ **tester 自动完成双帧 + 等待 VCU 应答 + 读回显**，无需手动发两次帧

### 协议握手时序

**PDO 写一个参数**（约 800ms）：

```
dataengine                         VCU
    │── 0x111 写帧1 (BYTE7=1) ──▶│
    │    等 50ms                   │
    │── 0x111 写帧2 (BYTE7=0) ──▶│
    │    等 300ms（ACK超时）        │
    │◀── 0x222 写应答 (BYTE7=0) ──│  ← 写成功
    │── 0x111 读帧1 (BYTE7=1) ──▶│  ← 写成功后立即读回显
    │    等 50ms                   │
    │── 0x111 读帧2 (BYTE7=0) ──▶│
    │    等 300ms                  │
    │◀── 0x222 读应答 (BYTE0=1) ──│  ← 返回 VCU 实际存储值
    │
  更新 cache → 回调 Qt 显示新值
```

**PDO 读一个参数**（约 500ms，后台静默）：

```
dataengine                         VCU
    │── 0x111 读帧1 (BYTE7=1) ──▶│
    │    等 50ms                   │
    │── 0x111 读帧2 (BYTE7=0) ──▶│
    │    等 150ms（后台短超时）     │
    │◀── 0x222 读应答 (BYTE0=1) ──│
    │
  更新 cache
```

---

## 一、驱动设置页（Drive Settings）

> **参数地址范围**：1~14（对应 `chache.driveSettings.*`）
> **上电时**：VEH 自动批量读全部参数填充 cache（后台，约 6~10 秒）
> **Qt 进入页面**：发 `page_enter` → VEH 立即推 cache 值 + 后台再刷新

### DS-01  进入驱动设置页（page_enter）

**tester 命令**：
```bash
# 进入驱动设置页：立即返回所有参数当前 cache 值
./skes_qt_bridge_tester page_enter drive_settings
```

**预期输出**（VCU 在线时，上电批量读完成后）：
```
[TESTER] dtype=page_enter  result=0 (成功)
---- 参数列表（addr / prop_name = value）----
  addr=01  ds_high_speed_max              = 12
  addr=02  ds_high_speed_accel            = 100
  addr=03  ds_high_speed_decel            = 100
  addr=04  ds_std_speed_max               = 8
  ...
```

**预期结果**：
- result=0，返回 14 个驱动参数；上电后首次进入值可能为 0（cache 尚未填充），约 10s 后刷新

**异常情况**：
- VCU 离线时：cache 值全为 0，后台读全部超时，仍正常返回（方案B）
- 超时 5s 无响应：检查 skes-relay 是否运行

---

### DS-02  读取单个参数（高速模式最大车速 addr=1）

**协议说明**：

| 地址 | 参数名 | 取值范围 | 单位 | JSON路径 |
|------|--------|----------|------|---------|
| 1 | `ds_high_speed_max` | 1~15 | km/h | `chache.driveSettings.highSpeedMax` |

**手动发 CAN 帧验证（观察 VCU→仪表 方向，需 CAN 总线分析仪）**：
```bash
# 仪表发读帧1（BYTE0=1读，BYTE1=1地址，BYTE7=1使能）
cansend can0 111#01 01 00 00 00 00 00 01
# 50ms后发读帧2（BYTE7=0）
cansend can0 111#01 01 00 00 00 00 00 00
# VCU 应在 0x222 回: 01 01 [val_L] [val_H] 00 00 00 [ack]
```

**tester 命令**（推荐，自动完成双帧+等待）：
```bash
./skes_qt_bridge_tester query_drive_settings
```

**tester 逐参数验证**：
```bash
./skes_qt_bridge_tester query chache.driveSettings.highSpeedMax
```

---

### DS-03  写入高速模式最大车速（addr=1）

**协议说明**：
- 写命令：BYTE0=0，BYTE1=1，BYTE2~5=值（小端），BYTE7=帧1:1/帧2:0
- 成功后 VEH 自动发读命令回显实际值

**tester 命令**：
```bash
# 写入高速最大车速=12 km/h
./skes_qt_bridge_tester set_drive_param ds_high_speed_max 12
```

**预期输出**：
```
[TESTER] PDO写操作：写+读回显共约需 800ms，等待中...
[TESTER] dtype=cmd_control  result=0 (成功)
  ds_high_speed_max                   result=0  readback=12 ✓
```

**手动 CAN 帧等价操作（value=12=0x0000000C）**：
```bash
# 写帧1: BYTE0=0(写), BYTE1=1(addr), BYTE2=12(0x0C), BYTE3~5=0, BYTE7=1
cansend can0 111#00 01 0C 00 00 00 00 01
# 写帧2（50ms后）: 同内容，BYTE7=0
cansend can0 111#00 01 0C 00 00 00 00 00
# VCU 0x222 写应答: 00 01 0C 00 00 00 00 00(BYTE7=0成功)
# 仪表随即发读帧1+读帧2
cansend can0 111#01 01 00 00 00 00 00 01
cansend can0 111#01 01 00 00 00 00 00 00
# VCU 0x222 读应答: 01 01 0C 00 00 00 00 00
```

**验证 cache 已更新**：
```bash
./skes_qt_bridge_tester query chache.driveSettings.highSpeedMax
# 预期: highSpeedMax = 12
```

**candump 终端预期完整输出**（写 highSpeedMax=12，addr=1，value=0x0000000C）：
```
t=0ms:      can0  111  [8]  00 01 0C 00 00 00 00 01   ← 写帧1（cmd=0写，addr=1，val=12，enable=1）
t=50ms:     can0  111  [8]  00 01 0C 00 00 00 00 00   ← 写帧2（enable=0）
t=50+Δms:   can0  222  [8]  00 01 xx xx xx xx 00 00   ← VCU写应答（BYTE7=0成功）
t=50+Δ:     can0  111  [8]  01 01 00 00 00 00 00 01   ← 读回显帧1（cmd=1读）
t=100+Δ:    can0  111  [8]  01 01 00 00 00 00 00 00   ← 读回显帧2
t=100+2Δ:   can0  222  [8]  01 01 0C 00 00 00 00 xx   ← VCU读回显（BYTE2=0x0C=12）
```
tester 打印：`result=1  readback_value=12`

**预期结果表**：

| 写入值 | VCU写ack(BYTE7) | readback | cache | Qt界面 |
|--------|----------------|----------|-------|--------|
| 12     | 0（成功）       | 12       | 12    | 显示12 |
| 999    | 1（拒绝/越界）  | 旧值     | 不变  | 回滚旧值 |
| VCU离线 | 无响应（超时）  | 旧值     | 不变  | 回滚旧值 |

---

### DS-04  驱动设置完整参数写入验证

**场景**：依次写入全部 14 个驱动参数，逐一确认 readback

```bash
# 高速模式
./skes_qt_bridge_tester set_drive_param ds_high_speed_max   12
./skes_qt_bridge_tester set_drive_param ds_high_speed_accel 120
./skes_qt_bridge_tester set_drive_param ds_high_speed_decel 120

# 标准模式
./skes_qt_bridge_tester set_drive_param ds_std_speed_max    8
./skes_qt_bridge_tester set_drive_param ds_std_speed_accel  100
./skes_qt_bridge_tester set_drive_param ds_std_speed_decel  100

# 低速模式
./skes_qt_bridge_tester set_drive_param ds_low_speed_max    4
./skes_qt_bridge_tester set_drive_param ds_low_speed_accel  80
./skes_qt_bridge_tester set_drive_param ds_low_speed_decel  80

# 转角电位器标定
./skes_qt_bridge_tester set_drive_param ds_steer_pot_left   1000
./skes_qt_bridge_tester set_drive_param ds_steer_pot_mid    2048
./skes_qt_bridge_tester set_drive_param ds_steer_pot_right  3000

# 承载轮制动
./skes_qt_bridge_tester set_drive_param ds_load_wheel_brake_en  1
./skes_qt_bridge_tester set_drive_param ds_load_wheel_brake_pct 50
```

**批量验证 — 写完后读页**：
```bash
./skes_qt_bridge_tester page_enter drive_settings
```

**逐参数 query 确认**：
```bash
./skes_qt_bridge_tester query chache.driveSettings.highSpeedMax    # → 12
./skes_qt_bridge_tester query chache.driveSettings.highSpeedAccel  # → 120
./skes_qt_bridge_tester query chache.driveSettings.highSpeedDecel  # → 120
./skes_qt_bridge_tester query chache.driveSettings.stdSpeedMax     # → 8
./skes_qt_bridge_tester query chache.driveSettings.stdSpeedAccel   # → 100
./skes_qt_bridge_tester query chache.driveSettings.stdSpeedDecel   # → 100
./skes_qt_bridge_tester query chache.driveSettings.lowSpeedMax     # → 4
./skes_qt_bridge_tester query chache.driveSettings.lowSpeedAccel   # → 80
./skes_qt_bridge_tester query chache.driveSettings.lowSpeedDecel   # → 80
./skes_qt_bridge_tester query chache.driveSettings.steerPotLeft    # → 1000
./skes_qt_bridge_tester query chache.driveSettings.steerPotMid     # → 2048
./skes_qt_bridge_tester query chache.driveSettings.steerPotRight   # → 3000
./skes_qt_bridge_tester query chache.driveSettings.loadWheelBrakeEn  # → 1
./skes_qt_bridge_tester query chache.driveSettings.loadWheelBrakePct # → 50
```

**预期**：每个参数的 `value` 与写入值一致（或 VCU 实际执行后的有效值）

---

### DS-05  写入失败回滚验证

**场景**：写入超出 VCU 允许范围的值，验证 Qt 界面回滚

```bash
# 记录当前高速最大车速
./skes_qt_bridge_tester query chache.driveSettings.highSpeedMax
# 假设当前值=12，现写入越界值（假设VCU限制最大15）
./skes_qt_bridge_tester set_drive_param ds_high_speed_max 999
```

**预期输出（VCU 拒绝时）**：
```
[TESTER] dtype=cmd_control  result=-6 (失败)
  ds_high_speed_max                   result=-6  readback=12 ✗(VCU拒绝/超时)
```

**VCU 拒绝时 VEH 额外动作**：自动发送 `action="post"`, `data.type="param_rollback"` 推旧值给 Qt

在 subscribe 终端可以观察到：
```
[TESTER] [响应 #N] ...action=post...param_rollback...ds_high_speed_max=12...
```

---

### DS-06  写入超时验证（VCU 离线）

**场景**：拔掉 CAN0 线或 VCU 掉电，模拟超时

```bash
# VCU离线时写参数（等待约1400ms：写超时×2次）
./skes_qt_bridge_tester set_drive_param ds_high_speed_max 10
```

**预期输出**：
```
[TESTER] PDO等待超时 5000ms
[TESTER] 命令执行失败（超时或无响应）
```

**subscribe 终端预期**：收到 `param_rollback` 推送（携带旧值）

---

## 二、泵设置页（Pump Settings）

> **参数地址范围**：15~30（对应 `chache.pumpSettings.*`）

### PS-01  进入泵设置页（page_enter）

```bash
./skes_qt_bridge_tester page_enter pump_settings
```

**预期**：返回 16 个泵参数，result=0

---

### PS-01B  批量读全部泵参数（query_pump_settings）

```bash
# 触发 16 组 0x111 读命令（addr=15~30），后台从 VCU 刷新所有泵参数
./skes_qt_bridge_tester query_pump_settings
```

**期望（终端A candump）**：看到 16 对 0x111 读帧（每对间隔 50ms，cmd=1）及 16 个 0x222 读应答

**读完后逐参数验证**：
```bash
./skes_qt_bridge_tester query chache.pumpSettings.liftMaxSpeed
./skes_qt_bridge_tester query chache.pumpSettings.tiltMaxSpeed
./skes_qt_bridge_tester query chache.pumpSettings.sideMaxSpeed
./skes_qt_bridge_tester query chache.pumpSettings.attachMaxSpeed
./skes_qt_bridge_tester query chache.pumpSettings.liftMaxHeight
./skes_qt_bridge_tester query chache.pumpSettings.forkLevelAngle
./skes_qt_bridge_tester query chache.pumpSettings.heightAddr1
./skes_qt_bridge_tester query chache.pumpSettings.weightSet
./skes_qt_bridge_tester query chache.pumpSettings.pressureCalib
./skes_qt_bridge_tester query chache.pumpSettings.liftBufHeight
./skes_qt_bridge_tester query chache.pumpSettings.dropBufHeight
```

---

### PS-02  起升最大速度（addr=15）

**协议说明**：

| 地址 | 参数名 | 取值范围 | JSON路径 |
|------|--------|----------|---------|
| 15 | `ps_lift_max_speed` | 0~100% | `chache.pumpSettings.liftMaxSpeed` |

**tester 命令**：
```bash
./skes_qt_bridge_tester set_pump_param ps_lift_max_speed 80
./skes_qt_bridge_tester query chache.pumpSettings.liftMaxSpeed
# 预期: liftMaxSpeed = 80
```

**手动 CAN 帧（addr=15=0x0F，value=80=0x00000050）**：
```bash
# 写帧1: BYTE0=0, BYTE1=0x0F, BYTE2=0x50, BYTE7=1
cansend can0 111#00 0F 50 00 00 00 00 01
# 写帧2（50ms后）
cansend can0 111#00 0F 50 00 00 00 00 00
```

---

### PS-03  起升最大高度（addr=19）

```bash
# 设置起升最大高度=3000（单位：mm 或协议定义单位）
./skes_qt_bridge_tester set_pump_param ps_lift_max_height 3000
./skes_qt_bridge_tester query chache.pumpSettings.liftMaxHeight
```

---

### PS-04  货叉一键调平角度（addr=20）

```bash
# 调平角度=0（水平）
./skes_qt_bridge_tester set_pump_param ps_fork_level_angle 0
# 调平角度=5
./skes_qt_bridge_tester set_pump_param ps_fork_level_angle 5
```

---

### PS-05  高度寻址1~5（addr=21~25）

```bash
# 高度寻址1
./skes_qt_bridge_tester set_pump_param ps_height_addr1 500
./skes_qt_bridge_tester set_pump_param ps_height_addr2 1000
./skes_qt_bridge_tester set_pump_param ps_height_addr3 1500
./skes_qt_bridge_tester set_pump_param ps_height_addr4 2000
./skes_qt_bridge_tester set_pump_param ps_height_addr5 2500

# 批量验证
./skes_qt_bridge_tester page_enter pump_settings
```

---

### PS-06  称重标定发送按钮（addr=26）

> **特殊说明**：该参数为触发型，写 1 触发标定动作，VCU 执行后返回应答

```bash
# 触发称重标定（写1）
./skes_qt_bridge_tester set_pump_param ps_weigh_calib_cmd 1
```

**预期**：
- VCU 收到写命令后执行标定动作
- 写 ack 返回 0=成功，VEH 再发读命令回显，readback 值由 VCU 决定

---

### PS-07  重量值设置（addr=27）

```bash
# 设置称重标定参考重量=500 KG
./skes_qt_bridge_tester set_pump_param ps_weight_set 500
./skes_qt_bridge_tester query chache.pumpSettings.weightSet
# 预期: weightSet = 500
```

---

### PS-08  压力值标定（addr=28）

```bash
# 设置标定压力值
./skes_qt_bridge_tester set_pump_param ps_pressure_calib 200
./skes_qt_bridge_tester query chache.pumpSettings.pressureCalib
```

---

### PS-09  上升缓冲高度 / 下降缓冲高度（addr=29/30）

```bash
./skes_qt_bridge_tester set_pump_param ps_lift_buf_height 100
./skes_qt_bridge_tester set_pump_param ps_drop_buf_height 80
./skes_qt_bridge_tester query chache.pumpSettings.liftBufHeight
./skes_qt_bridge_tester query chache.pumpSettings.dropBufHeight
```

---

### PS-10  泵设置完整参数写入验证

```bash
# 速度类
./skes_qt_bridge_tester set_pump_param ps_lift_max_speed    80
./skes_qt_bridge_tester set_pump_param ps_tilt_max_speed    60
./skes_qt_bridge_tester set_pump_param ps_side_max_speed    60
./skes_qt_bridge_tester set_pump_param ps_attach_max_speed  50

# 高度类
./skes_qt_bridge_tester set_pump_param ps_lift_max_height   3000
./skes_qt_bridge_tester set_pump_param ps_fork_level_angle  0

# 高度寻址
./skes_qt_bridge_tester set_pump_param ps_height_addr1 500
./skes_qt_bridge_tester set_pump_param ps_height_addr2 1000
./skes_qt_bridge_tester set_pump_param ps_height_addr3 1500
./skes_qt_bridge_tester set_pump_param ps_height_addr4 2000
./skes_qt_bridge_tester set_pump_param ps_height_addr5 2500

# 称重标定
./skes_qt_bridge_tester set_pump_param ps_weight_set      500
./skes_qt_bridge_tester set_pump_param ps_pressure_calib  200

# 缓冲高度
./skes_qt_bridge_tester set_pump_param ps_lift_buf_height 100
./skes_qt_bridge_tester set_pump_param ps_drop_buf_height 80

# 全页读取验证
./skes_qt_bridge_tester page_enter pump_settings
```

**逐参数 query 确认**：
```bash
./skes_qt_bridge_tester query chache.pumpSettings.liftMaxSpeed    # → 80
./skes_qt_bridge_tester query chache.pumpSettings.tiltMaxSpeed    # → 60
./skes_qt_bridge_tester query chache.pumpSettings.sideMaxSpeed    # → 60
./skes_qt_bridge_tester query chache.pumpSettings.attachMaxSpeed  # → 50
./skes_qt_bridge_tester query chache.pumpSettings.liftMaxHeight   # → 3000
./skes_qt_bridge_tester query chache.pumpSettings.forkLevelAngle  # → 0
./skes_qt_bridge_tester query chache.pumpSettings.heightAddr1     # → 500
./skes_qt_bridge_tester query chache.pumpSettings.heightAddr2     # → 1000
./skes_qt_bridge_tester query chache.pumpSettings.heightAddr3     # → 1500
./skes_qt_bridge_tester query chache.pumpSettings.heightAddr4     # → 2000
./skes_qt_bridge_tester query chache.pumpSettings.heightAddr5     # → 2500
./skes_qt_bridge_tester query chache.pumpSettings.weightSet       # → 500
./skes_qt_bridge_tester query chache.pumpSettings.pressureCalib   # → 200
./skes_qt_bridge_tester query chache.pumpSettings.liftBufHeight   # → 100
./skes_qt_bridge_tester query chache.pumpSettings.dropBufHeight   # → 80
```

---

## 三、状态机时序测试

### SM-01  双帧时序验证（示波器/CAN分析仪）

**测试目的**：验证 VEH 发出的两帧间隔精确为 50ms

**tester 触发**：
```bash
./skes_qt_bridge_tester set_drive_param ds_high_speed_max 12
```

**CAN 分析仪预期观察**：
```
t=0ms:     0x111 TX: 00 01 0C 00 00 00 00 01  (写帧1 BYTE7=1)
t=50ms:    0x111 TX: 00 01 0C 00 00 00 00 00  (写帧2 BYTE7=0)
t=50+Δms:  0x222 RX: 00 01 0C 00 00 00 00 00  (VCU写ack)
t=50+Δ:    0x111 TX: 01 01 00 00 00 00 00 01  (读帧1)
t=100+Δ:   0x111 TX: 01 01 00 00 00 00 00 00  (读帧2)
t=100+2Δ:  0x222 RX: 01 01 0C 00 00 00 00 00  (VCU读应答，BYTE2=12)
```

---

### SM-02  写失败后立即回滚（VCU NACK）

**测试目的**：VCU 写ack BYTE7=1（失败）时，VEH 不发读命令，直接回调失败

**CAN 分析仪场景**：
- VEH 发写双帧（0x111）
- 在 CAN 分析仪上注入 0x222 帧：`00 01 00 00 00 00 00 01`（写失败ack，BYTE7=1）

```bash
# 先后台注入VCU NACK（需CAN分析仪在线注入）
# 仅供实验室测试，需在50ms~350ms窗口内注入
# 在 subscribe 终端观察：
# 预期收到 param_rollback post 消息，携带 ds_high_speed_max=旧值
./skes_qt_bridge_tester --full subscribe &
./skes_qt_bridge_tester set_drive_param ds_high_speed_max 12
# 在CAN分析仪注入: cansend can0 222#00 01 00 00 00 00 00 01
```

---

### SM-03  写成功但读回显超时（降级逻辑）

**测试目的**：写ack正常（成功），但读ack超时，VEH 以写入值作为回显（降级策略）

**测试方法**（三终端）：
```bash
# 终端A：监听
candump can0

# 终端B：触发写
./skes_qt_bridge_tester set_drive_param ds_high_speed_max 12

# 终端C：只发写成功应答，不发读应答
cansend can0 222#00010000000000 00   # 写成功（BYTE7=0）
# 不发任何读应答，等约 700ms
```

**预期**：
- dataengine 日志：`[PdoMgr] read FAILED addr=1`（读超时）
- **降级策略**：以 `write_value=12` 作为 readback 值，cache 仍更新为 12
- tester 打印：成功，readback=12

```bash
./skes_qt_bridge_tester --full subscribe
./skes_qt_bridge_tester query chache.driveSettings.highSpeedMax    # → 12（降级写入）
```

---

### SM-04  并发写操作队列验证

**场景**：快速连续发两个写命令，验证队列串行执行

```bash
# 快速连续（不等待响应）
./skes_qt_bridge_tester set_drive_param ds_high_speed_max   12 &
./skes_qt_bridge_tester set_drive_param ds_high_speed_accel 120 &
wait
```

**预期**：两个操作各自独立完成，先入队先执行，互不干扰

在 subscribe 终端预期看到两个独立的 cmd_control 响应

---

## 四、上电流程测试

### PO-01  上电批量读验证

**测试目的**：验证 vehicleDataEngine 启动后自动读取全部 30 个参数

**步骤**：
```bash
# 1. 启动 vehicleDataEngine（VCU 在线）
cd ~/cameras && ./vehicleDataEngine &

# 2. 等待约 10 秒（30个参数 × 约200ms/个 = 6秒；全超时最坏21秒）
sleep 12

# 3. 查询驱动设置是否已填充
./skes_qt_bridge_tester page_enter drive_settings
# 预期：参数值为 VCU 实际值，不再全为 0

./skes_qt_bridge_tester page_enter pump_settings
# 预期：泵参数也正常填充
```

**预期时序日志**（vehicleDataEngine 控制台）：
```
[PdoMgr] trigger power-on batch bg-read (30 params)
[PdoMgr] dispatch op=READ addr=1 prop=ds_high_speed_max
[PdoMgr] TX 0x111 cmd=1 addr=1 val=0 enable=1
[PdoMgr] RX 0x222 cmd=1 addr=1 val=12 ack=0 state=4
[PdoMgr] cache updated: driveSettings.highSpeedMax = 12
...
```

---

### PO-02  VCU 离线时上电

**测试目的**：VCU 不在线时，上电批量读全部超时，系统正常继续运行

```bash
# VCU 离线（断开CAN总线）情况下启动
./vehicleDataEngine &
sleep 10
./skes_qt_bridge_tester page_enter drive_settings
# 预期：result=0，但所有 value=0（cache未填充）
# 后台读操作全部静默失败，不影响主流程
```

---

## 五、filter_table 推送验证

### FT-01  参数修改后推送同步

**测试目的**：写参数后，下一个 filter_table 周期（500ms）中包含更新后的值

```bash
# 1. 开启订阅
./skes_qt_bridge_tester --full subscribe &

# 2. 写参数
./skes_qt_bridge_tester set_drive_param ds_high_speed_max 12

# 3. 等待下一次 filter_table 推送（约500ms）
# 预期: subscribe终端打印出 ds_high_speed_max = 12
```

---

### FT-02  page_enter 后台刷新同步到推送

**测试目的**：进入页面后台重读参数，完成后自动在下次 filter_table 带出

```bash
# 1. 开启订阅监听
./skes_qt_bridge_tester --full subscribe &

# 2. 发 page_enter（立即返回 cache 值）
./skes_qt_bridge_tester page_enter drive_settings

# 3. 等待后台重读完成（约10秒）
sleep 12

# 4. 查看 filter_table 是否带出最新值
# 预期：subscribe终端中 ds_* 字段持续更新为VCU实际值
```

---

## 六、综合联调测试

### T-PDO-01  完整驱动设置工作流

```bash
# 1. 进入页面，获取当前值
./skes_qt_bridge_tester page_enter drive_settings

# 2. 修改高速最大车速
./skes_qt_bridge_tester set_drive_param ds_high_speed_max 15

# 3. 查询单字段确认
./skes_qt_bridge_tester query chache.driveSettings.highSpeedMax  # → 15

# 4. 修改标准模式最大车速
./skes_qt_bridge_tester set_drive_param ds_std_speed_max 10

# 5. 再次进入页面确认两项都更新
./skes_qt_bridge_tester page_enter drive_settings
# 预期：highSpeedMax=15, stdSpeedMax=10
```

---

### T-PDO-02  驱动+泵混合参数修改

```bash
# 场景：运营人员进入设置后同时调整驱动和泵参数

# 驱动参数
./skes_qt_bridge_tester set_drive_param ds_low_speed_max 3

# 泵参数
./skes_qt_bridge_tester set_pump_param  ps_lift_max_speed 70

# 验证（两个页面分别查）
./skes_qt_bridge_tester query chache.driveSettings.lowSpeedMax    # → 3
./skes_qt_bridge_tester query chache.pumpSettings.liftMaxSpeed    # → 70
```

---

### T-PDO-03  参数越界回滚 + subscribe 观察

```bash
# 开启订阅
./skes_qt_bridge_tester --full subscribe &
SUB_PID=$!

# 设定一个正常值
./skes_qt_bridge_tester set_drive_param ds_high_speed_max 12

# 尝试写越界值（假设 VCU 拒绝）
./skes_qt_bridge_tester set_drive_param ds_high_speed_max 9999

# 预期 subscribe 终端：
# 1. 先看到 cmd_control response result=-6
# 2. 随后看到 action=post type=param_rollback，ds_high_speed_max=12（旧值回滚）

# 等待并确认
sleep 2
./skes_qt_bridge_tester query chache.driveSettings.highSpeedMax   # → 应仍为12

kill $SUB_PID
```

---

### T-PDO-04  filter_table + PDO 联合验证

```bash
# 后台持续监听推送
./skes_qt_bridge_tester --full subscribe &
SUB_PID=$!

# 写入4个参数
./skes_qt_bridge_tester set_drive_param ds_high_speed_max  12
./skes_qt_bridge_tester set_drive_param ds_std_speed_max   8
./skes_qt_bridge_tester set_pump_param  ps_lift_max_speed  80
./skes_qt_bridge_tester set_pump_param  ps_lift_max_height 3000

# 等1秒，观察 filter_table 推送中上述参数是否更新
sleep 1

# 预期：subscribe 终端的 filter_table 推送中包含：
# ds_high_speed_max  = 12
# ds_std_speed_max   = 8
# ps_lift_max_speed  = 80
# ps_lift_max_height = 3000

kill $SUB_PID
```

---

## 七、附：PDO 参数地址速查表

### 驱动设置（CAN0 0x111 BYTE1 地址，对应 `chache.driveSettings`）

| 地址 | prop_name | cache_key | 说明 |
|------|-----------|-----------|------|
| 1 | `ds_high_speed_max` | highSpeedMax | 高速模式最大车速（1~15 km/h）|
| 2 | `ds_high_speed_accel` | highSpeedAccel | 高速模式加速度（10~300%）|
| 3 | `ds_high_speed_decel` | highSpeedDecel | 高速模式减速度（10~300%）|
| 4 | `ds_std_speed_max` | stdSpeedMax | 标准模式最大车速（1~10 km/h）|
| 5 | `ds_std_speed_accel` | stdSpeedAccel | 标准模式加速度 |
| 6 | `ds_std_speed_decel` | stdSpeedDecel | 标准模式减速度 |
| 7 | `ds_low_speed_max` | lowSpeedMax | 低速模式最大车速（1~5 km/h）|
| 8 | `ds_low_speed_accel` | lowSpeedAccel | 低速模式加速度 |
| 9 | `ds_low_speed_decel` | lowSpeedDecel | 低速模式减速度 |
| 10 | `ds_steer_pot_left` | steerPotLeft | 转角电位器左转值 |
| 11 | `ds_steer_pot_mid` | steerPotMid | 转角电位器中转值 |
| 12 | `ds_steer_pot_right` | steerPotRight | 转角电位器右转值 |
| 13 | `ds_load_wheel_brake_en` | loadWheelBrakeEn | 承载轮制动强制输出 0/1 |
| 14 | `ds_load_wheel_brake_pct` | loadWheelBrakePct | 承载轮制动比例值（1~100%）|

### 泵设置（地址 15~30，对应 `chache.pumpSettings`）

| 地址 | prop_name | cache_key | 说明 |
|------|-----------|-----------|------|
| 15 | `ps_lift_max_speed` | liftMaxSpeed | 起升最大速度 |
| 16 | `ps_tilt_max_speed` | tiltMaxSpeed | 倾斜最大速度 |
| 17 | `ps_side_max_speed` | sideMaxSpeed | 侧移最大速度 |
| 18 | `ps_attach_max_speed` | attachMaxSpeed | 属具最大速度 |
| 19 | `ps_lift_max_height` | liftMaxHeight | 起升最大高度 |
| 20 | `ps_fork_level_angle` | forkLevelAngle | 货叉一键调平角度 |
| 21 | `ps_height_addr1` | heightAddr1 | 高度寻址1 |
| 22 | `ps_height_addr2` | heightAddr2 | 高度寻址2 |
| 23 | `ps_height_addr3` | heightAddr3 | 高度寻址3 |
| 24 | `ps_height_addr4` | heightAddr4 | 高度寻址4 |
| 25 | `ps_height_addr5` | heightAddr5 | 高度寻址5 |
| 26 | `ps_weigh_calib_cmd` | weighCalibCmd | 称重标定发送按钮（触发型）|
| 27 | `ps_weight_set` | weightSet | 重量值设置 |
| 28 | `ps_pressure_calib` | pressureCalib | 压力值标定 |
| 29 | `ps_lift_buf_height` | liftBufHeight | 上升缓冲高度 |
| 30 | `ps_drop_buf_height` | dropBufHeight | 下降缓冲高度 |

---

## 八、无 VCU 手动模拟 0x222 应答测试

> 适用场景：VCU 硬件未就绪，但需验证 dataengine PDO 收发逻辑

**三终端布局**：
```
终端A：candump can0          ← 旁观所有帧
终端B：tester 发命令          ← 触发 0x111
终端C：cansend can0 222#...  ← 在收到 0x111 后人工回 0x222
```

---

### SIM-01  模拟写成功（以 highSpeedMax=12 为例）

```bash
# 终端B：触发写（有约 350ms 窗口发写应答）
./skes_qt_bridge_tester set_drive_param ds_high_speed_max 12

# 终端A 先看到（等写帧2发完后）：
#   can0  111  [8]  00 01 0C 00 00 00 00 01   ← 写帧1
#   can0  111  [8]  00 01 0C 00 00 00 00 00   ← 写帧2（50ms后）

# 终端C：在写帧2发完后 300ms 内发写成功应答
# BYTE0=0（写应答），BYTE1=1（addr匹配），BYTE7=0（成功）
cansend can0 222#00010000000000 00

# dataengine 收到后立即发读回显帧：
#   can0  111  [8]  01 01 00 00 00 00 00 01   ← 读帧1
#   can0  111  [8]  01 01 00 00 00 00 00 00   ← 读帧2（50ms后）

# 终端C：发读应答（BYTE0=1，BYTE1=1，BYTE2~5=12=0x0C小端）
cansend can0 222#01010C00000000 00

# tester 打印：result=1  readback_value=12
./skes_qt_bridge_tester query chache.driveSettings.highSpeedMax    # → 12
```

---

### SIM-02  模拟写失败（VCU NACK）

```bash
# 终端B
./skes_qt_bridge_tester set_drive_param ds_high_speed_max 999

# 终端C：等写帧2发完后发写失败应答（BYTE7=1 表示失败）
cansend can0 222#00010000000000 01

# tester 打印：result=0（失败），cache 保持旧值不变
./skes_qt_bridge_tester query chache.driveSettings.highSpeedMax    # → 旧值（未更新）
```

**注意**：dataengine 会同时发送 `param_rollback` 推送，subscribe 终端可观察到旧值回推。

---

### SIM-03  模拟批量读参数

```bash
# 终端B：触发批量读
./skes_qt_bridge_tester query_drive_settings

# 终端A 依次看到 addr=1~14 的读帧对（每对间隔 50ms）
# 终端C：为每个 addr 发读应答（以 addr=1 返回 12，addr=2 返回 8 为例）
cansend can0 222#01010C00000000 00    # addr=1，val=12
cansend can0 222#01020800000000 00    # addr=2，val=8
cansend can0 222#01030600000000 00    # addr=3，val=6
# ... 依次 addr=4~14，每条在对应读帧2发出后 150ms 内发送

# 全部应答后验证
./skes_qt_bridge_tester query chache.driveSettings.highSpeedMax    # → 12
./skes_qt_bridge_tester query chache.driveSettings.highSpeedAccel  # → 8
```

---

### SIM-04  超时不应答（观察重试日志）

```bash
# 终端B：触发写，不发任何 0x222
./skes_qt_bridge_tester set_drive_param ds_high_speed_max 12

# dataengine 日志预期（约 300ms 后）：
#   [PdoMgr] write ack timeout addr=1 retry=0
# 再次发写帧1+帧2（重试1次），再等 300ms：
#   [PdoMgr] write ack timeout addr=1 retry=1
#   [PdoMgr] write FAILED addr=1 prop=driveSettings.highSpeedMax
# tester 约 5 秒超时后打印：PDO 等待超时 5000ms
```

---

## 九、0x111 帧内容解码速查

> 用于与 candump 输出比对，快速判断帧正确性

**读 highSpeedMax（addr=1）**：
```
帧1：01 01 00 00 00 00 00 01
      │  │  └─── val=0 ──┘  └─ enable=1
      │  └─ addr=1
      └─ cmd=1（读）

帧2：01 01 00 00 00 00 00 00
      └─ enable=0（确认帧）
```

**写 highSpeedMax=12（addr=1，0x0C）**：
```
帧1：00 01 0C 00 00 00 00 01
      │  │  └─ val=12(小端) ┘  └─ enable=1
      │  └─ addr=1
      └─ cmd=0（写）

帧2：00 01 0C 00 00 00 00 00
```

**写 heightAddr3=1500（addr=23=0x17，0x5DC 小端=DC 05 00 00）**：
```
帧1：00 17 DC 05 00 00 00 01
帧2：00 17 DC 05 00 00 00 00
```

**写 liftMaxHeight=3000（addr=19=0x13，0xBB8 小端=B8 0B 00 00）**：
```
帧1：00 13 B8 0B 00 00 00 01
帧2：00 13 B8 0B 00 00 00 00
```

**写 steerPotMid=2048（addr=11=0x0B，0x800 小端=00 08 00 00）**：
```
帧1：00 0B 00 08 00 00 00 01
帧2：00 0B 00 08 00 00 00 00
```

---

## 十、已知问题与注意事项

### Issue-01  上电后首次 page_enter 返回全 0

**现象**：vehicleDataEngine 启动后 10 秒内进入设置页，所有参数值为 0

**根因**：上电批量读（30个参数串行）约需 6~10 秒（后台，VCU 在线时），首次 page_enter 取的是尚未填充的 cache

**处理**：正常现象，等待 10 秒后再进入页面；或等待 filter_table 推送刷新

---

### Issue-02  VCU 离线时 PDO 写操作等待 1400ms 后失败

**现象**：`set_drive_param` 命令执行约 1.4 秒后才报失败（重试一次）

**根因**：写ack超时300ms × 2次 = 600ms，加上帧间隔50ms × 2次 = 700ms，两次共1400ms

**处理**：正常超时行为，结果为失败，旧值已通过 param_rollback 推送给 Qt

---

### Issue-03  称重标定后 readback 值含义由 VCU 定义

**现象**：`ps_weigh_calib_cmd` 写 1 后，readback 值不一定是 1

**根因**：称重标定为动作型，VCU 可能将 readback 设置为标定结果码

**处理**：以 VCU 协议文档为准，tester 只做如实展示

---

### Issue-04  tester 并发发送 PDO 命令时 msg_id 可能混淆

**现象**：快速并发发两个 `set_drive_param` 时，收到响应的顺序不保证与发送顺序一致

**根因**：tester 是命令行工具，每次创建独立连接；VEH 侧 PDO 队列串行，但 tester 不等待对方队列

**处理**：生产中 Qt 端应在收到前一个 cmd_control response 后才发下一个；测试时请串行执行

