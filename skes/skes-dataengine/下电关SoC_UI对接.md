# 下电 / 关 SoC 需求 —— dataengine ↔ UI 对接文档

> 适用模块：`skes-dataengine`（数据端）与 `skes-ui/skes-main_fork-lift`（叉车主界面）
> 关联 CAN 帧：`CAN0 0x18FE7C03`（VCU 发送的上/下电指令）
> 关联 io_mng 命令/事件：`CMD_SHUT_SOC_SET(105)`、`EVENT_CODE_ABNORMAL_PWROFF(20)`

---

## 1. 需求概述

当 VCU 通过 `CAN0 0x18FE7C03` 的 `Byte0 BIT0` 发出**下电请求**时：

- **正常场景**：UI 显示一张**“关机中”图片**（弹窗/遮罩）。
- **异常场景**：MCU 检测到下电异常，经 io_mng 事件通知 dataengine，dataengine 把**错误码 INERR001** 推给 UI 显示（熄屏显示）。

> **BIT0 语义（重要）**：`CAN0 0x18FE7C03 Byte0 BIT0`：**`1` = 上电请求，`0` = 下电请求**。
> 该原始值已由 dataengine 解析为字段 **`vcuPowerCmd`**（`0下电 / 1上电`）并推送给 UI。
> 本需求以 **`BIT0==0`（即 `vcuPowerCmd==0`，下电请求）** 作为触发条件。

错误码展示内容：

| 字段 | 值 |
|---|---|
| 故障代码 | `INERR001` |
| 故障类型 | 整车故障 |
| 故障描述 | 下电信号异常故障 |

---

## 2. 整体数据流

```
                       ┌─────────── 正常场景 ───────────┐
CAN0 0x18FE7C03            vcuPowerCmd=0 (已有字段,周期推送)
 BIT0==0(下电请求) ───────────────────────────────────────► UI: vcuPowerCmd==0 时显示“关机中”图片
       │
       │ 同时下发
       ▼
 CMD_SHUT_SOC_SET(105,EN=1) ──► io_mng ──► MCU
       │                                    │
       │  ACK(ack==0=接受成功)              │ ← 仅 dataengine 内部记录，不推 UI
       ◄────────────────────────────────────┘
                       └─────────── 异常场景 ───────────┘
 MCU 检测下电异常 ──► EVENT_CODE_ABNORMAL_PWROFF(20) ──► io_mng 事件总线
                                                              │
                        dataengine 订阅到 ─► faultList 增加 INERR001 ─► UI 显示故障码
```

**要点**：
- 正常场景**不新增字段**，直接复用已有的 `vcuPowerCmd`（`==0` 即下电请求中）显示关机图。
- **错误码 INERR001 只由异常事件（EVENT 20）触发**；MCU 对命令的 ACK（接受成功与否）**不影响** UI 故障码显示，由 dataengine 内部处理。

---

## 3. dataengine 推送给 UI 的接口（契约）

推送走**现有属性推送通道**（与 `faultCount`、`faultList`、`vcuPowerCmd` 等同一套 `property_name` 机制，约 500ms 周期）。

### 3.1 `vcuPowerCmd` —— 关机中图片（正常场景，复用既有字段）

| 项 | 值 |
|---|---|
| property_name | `vcuPowerCmd`（**既有字段，无需 dataengine/UI 新增接收管道**） |
| 类型 | `int` |
| 取值 | `0` = 下电请求（显示“关机中”图片）；`1` = 上电请求（正常运行） |
| 来源 | `CAN0 0x18FE7C03 Byte0 BIT0` 原始值 |

> UI 侧的 `vcuPowerCmd` 属性、接收分发、`Q_PROPERTY` **均已存在并在用**（`VehicleController.h` + `Gadget_BmsParam.qml`），本需求 UI 只需新增一处“关机中”图片的 QML 绑定即可（见 4.1）。

### 3.2 `faultList` 中的整车故障 —— 错误码（异常场景）

`faultList` 是**既有字段**（JSON 数组字符串）。异常场景下，数组中会**新增一条**整车故障条目，与其它故障条目同形状：

```json
{
  "seq": 3,
  "type": "vehicleErr",
  "typeName": "整车故障",
  "code": "INERR001",
  "time": "2026-07-14 10:30:22"
}
```

| 字段 | 说明 |
|---|---|
| `type` | `"vehicleErr"`（本需求新增的故障类型标识/property_name） |
| `typeName` | `"整车故障"` |
| `code` | **字符串码** `"INERR001"`（类似 `bcmErr` 的字符串码，不是数字） |
| `time` | 首次发生时间 |

> **描述文案** `下电信号异常故障` **不在推送里**，由 UI 侧按 `code` 映射（与 `bcmErr`/`BCMERRF017` 一套约定），单一真相源在 UI，便于改文案/多语言。

**故障清除**：dataengine 在**下一次下电请求边沿**（BIT0 1→0）自动清除上一轮 INERR001；故障始终反映“最近一次下电”的结果。UI 无需主动清除，跟随 `faultList` 刷新即可。

---

## 4. UI 端需要做的事

### 4.1 “关机中”图片 —— **待 UI 实现（仅一处 QML 绑定）**

`vcuPowerCmd` UI 已在接收（属性/分发/Q_PROPERTY 都现成），因此**不需要**再加 C++ 接收管道，只需在 QML 里绑一张图：

```qml
Image {
    anchors.fill: parent
    source: "qrc:/assets/关机中.png"        // 替换为实际资源
    visible: vehicle.vcuPowerCmd === 0       // 0=下电请求 → 显示关机中；vehicle 为 VehicleController 实例
    z: 9999                                  // 置顶遮罩
}
```

> 具体样式（全屏遮罩 / 弹窗 / 文案 / 资源图）由 UI 决定，dataengine 只保证 `vcuPowerCmd` 的值。

### 4.2 `faultList` / INERR001 故障码显示 —— **已实现（无需重做）**

整车故障码的显示逻辑**已在 `VehicleController.cpp` 加好**（本次改动一部分），照 `bcmErr` 字符串码的套路：

- `formatFaultCodeText()`：`vehicleErr` 分支直接返回原始码 → 显示 `INERR001`
- `formatFaultContentText()`：`vehicleErr` 分支 → `vehicleFaultContent("INERR001")` → 显示 `下电信号异常故障`
- 辅助函数 `vehicleFaultContent()`：`"INERR001" → "下电信号异常故障"`（未来新增整车故障码在此扩展）

UI 侧的故障列表/横幅会自动带出该条，无需额外处理。若新增其它 `INERRxxx` 码，只需在 `vehicleFaultContent()` 增加映射。

---

## 5. 字段速查表

| 方向 | 名称 | 类型 | 取值/说明 | UI 状态 |
|---|---|---|---|---|
| dataengine → UI | `vcuPowerCmd`（既有） | int | 0=下电请求(显示关机中图)，1=上电请求 | 属性/分发已就绪，仅需加 QML 图（4.1） |
| dataengine → UI | `faultList`（既有） | string(JSON) | 异常时含 `{type:"vehicleErr",code:"INERR001",typeName:"整车故障",...}` | 故障码显示已实现（4.2） |

**本需求不新增任何推送字段**：正常场景复用 `vcuPowerCmd`，异常场景复用 `faultList`。

---

## 6. 附录：io_mng / MCU 侧约定（背景，非 UI 关注）

| 项 | 值 |
|---|---|
| 下发命令 | `CMD_SHUT_SOC_SET = 105`，载荷 `cmd_bytes[0]=EN(=1 执行)` + 3 字节保留 |
| 命令下发总线 | dataengine PUB → `tcp://127.0.0.1:26008`（io_mng CMD_SUB） |
| 命令回执总线 | dataengine SUB → `tcp://127.0.0.1:16008`（io_mng CMD_PUB） |
| ACK 判定 | 应答载荷 `cmd_settings_ack_t.ack`，**`ack==0` = MCU 接受成功**（仅内部日志，不推 UI） |
| 异常事件 | `EVENT_CODE_ABNORMAL_PWROFF = 20` |
| 事件总线 | dataengine SUB → `tcp://127.0.0.1:48000`（io_mng EVT_PUB，JSON 格式） |

> io_mng 侧改动：`cmd.h` 新增 `CMD_SHUT_SOC_SET=105`、`evt_proc.h` 新增 `EVENT_CODE_ABNORMAL_PWROFF=20`（命令转发/事件上报为通用逻辑，无需额外处理）。MCU 固件需支持接收 105、按 `ack==0` 应答、异常时上报事件 20。

---

*本文档随需求实现同步维护；字段/端口若有调整以 `RealtimeStatusManager.cpp`、`SkesQtBridge.cpp` 实际代码为准。*
