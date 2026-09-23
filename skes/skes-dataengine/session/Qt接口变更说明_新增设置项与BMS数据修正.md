# Qt 接口变更说明 — 2026-06-03

> 对应分支：`main_ui`  
> 涉及模块：`skes-dataengine`（SkesQtBridge、RealtimeStatusManager）  
> 协议版本：SKES v0.8，Topic `/skes/filter`

---

## 目录

1. [新增 cmd_control 命令](#1-新增-cmd_control-命令)
2. [新增 filter_table 推送字段](#2-新增-filter_table-推送字段)
3. [BMS 数据类型 / 精度变更（Breaking）](#3-bms-数据类型--精度变更breaking)
4. [速度模式编码变更（Breaking）](#4-速度模式编码变更breaking)
5. [转向角算法修正](#5-转向角算法修正)
6. [filter_table 移除字段](#6-filter_table-移除字段)
7. [变更汇总速查表](#7-变更汇总速查表)

---

## 1. 新增 cmd_control 命令

以下三条命令通过 `action=request / type=properties / data.type=cmd_control` 下发，格式与已有命令（如 `speedMode`、`headLight`）完全一致。

### 1.1 禁用刷卡 `disableCardSwipe`

| 项目 | 说明 |
|------|------|
| property_name | `disableCardSwipe` |
| data_type | `int` |
| 取值 | `0` = 允许刷卡（开机显示刷卡弹窗）<br>`1` = 禁用刷卡（开机不显示刷卡弹窗） |
| 持久化 | **掉电保持**，重启后恢复上次设置 |
| CAN | 无（仪表自身逻辑） |

**下发示例：**
```json
{
  "action": "request",
  "type": "properties",
  "msg_id": "xxxx",
  "data": {
    "type": "cmd_control",
    "data": [
      { "property_name": "disableCardSwipe", "data_type": "int", "data": 1 }
    ]
  }
}
```

**响应示例：**
```json
{
  "action": "response",
  "type": "properties",
  "msg_id": "xxxx",
  "data": {
    "type": "cmd_control",
    "result": 0,
    "data": [
      { "property_name": "disableCardSwipe", "result": 0, "data_type": "int", "data": 1 }
    ]
  }
}
```

---

### 1.2 显示 LOGO `displayLogo`

| 项目 | 说明 |
|------|------|
| property_name | `displayLogo` |
| data_type | `int` |
| 取值 | `0` = 隐藏 SANY LOGO<br>`1` = 显示 SANY LOGO |
| 持久化 | **掉电保持** |
| CAN | 无（仪表自身逻辑） |

**下发示例：**
```json
{
  "action": "request",
  "type": "properties",
  "msg_id": "xxxx",
  "data": {
    "type": "cmd_control",
    "data": [
      { "property_name": "displayLogo", "data_type": "int", "data": 1 }
    ]
  }
}
```

---

### 1.3 临时刷卡 `tempCardSwipe`

| 项目 | 说明 |
|------|------|
| property_name | `tempCardSwipe` |
| data_type | `int` |
| 取值 | `0` = 停止模拟刷卡<br>`1` = 模拟刷卡（触发一次 CAN 发送） |
| 持久化 | **掉电保持**，重启后若之前为 ON，自动补发 CAN 帧 |
| CAN | CAN0（扩展帧）0x4D1，byte0 bit1：ON→置1发送，OFF→清0发送 |

> **注意**：0x4D1 为**触发式发送**，非周期帧。ON/OFF 切换时各发一次，不持续占用总线。

**下发示例：**
```json
{
  "action": "request",
  "type": "properties",
  "msg_id": "xxxx",
  "data": {
    "type": "cmd_control",
    "data": [
      { "property_name": "tempCardSwipe", "data_type": "int", "data": 1 }
    ]
  }
}
```

---

## 2. 新增 filter_table 推送字段

VEH 每 500ms 推送一次 `action=post / type=properties / data.type=filter_table`，新增以下三个字段：

| property_name | data_type | 说明 | 初始值 |
|---------------|-----------|------|--------|
| `disableCardSwipe` | `int` | 禁用刷卡状态（0/1） | 持久化恢复 |
| `displayLogo` | `int` | 显示 LOGO 状态（0/1） | 持久化恢复 |
| `tempCardSwipe` | `int` | 临时刷卡状态（0/1） | 持久化恢复 |

**Qt 处理要点：**
- 三个字段均直接读持久化值，首次推送时即为上次掉电前的状态，Qt 初始化界面时无需另发 query。
- 支持 `query` 单独查询：`testCalibration.disableCardSwipe` / `testCalibration.displayLogo` / `testCalibration.tempCardSwipe`。

---

## 3. BMS 数据类型 / 精度变更（Breaking）

> ⚠️ **以下字段的类型或值域发生变化，Qt 侧须同步修改解析逻辑，否则显示数值将错误。**

### 3.1 工作电流 `current`

| 项目 | 旧版 | 新版 |
|------|------|------|
| 推送类型 | `int`（×10，有符号原始） | `int`（×10，已含偏移换算） |
| CAN 原始解析 | `int16_t * 0.1` | `uint16_t * 0.1 - 1000.0` |
| 值域（推送值） | 取决于原始有符号解读 | `-10000` ~ `+9247`（对应 -1000.0A ~ +924.7A） |
| 推送单位 | 0.1A（×10 传整数） | 0.1A（×10 传整数，**方向不变**） |
| Qt 还原公式 | `value / 10.0` | `value / 10.0`（**公式不变，数值范围变化**） |

> 充电电流为负，放电为正。旧版因有符号解读错误，大电流时数值有误差，**新版修正后数值会与旧版不同**。

---

### 3.2 SOC `soc`

| 项目 | 旧版 | 新版 |
|------|------|------|
| CAN 原始解析 | `raw`（直接当 % 用） | `raw * 0.4`（分辨率 0.4%） |
| 推送类型 | `int` | `int` |
| 值域（推送值） | 0 ~ 255 | 0 ~ 100（最大 raw=250 → 100%） |
| Qt 直接显示 | 可能出现 > 100 | **直接显示，无需再转换** |

---

### 3.3 最高 / 最低单体电压 `cellVoltMax` / `cellVoltMin`

| 项目 | 旧版 | 新版 |
|------|------|------|
| 推送类型 | `int`（raw 值，Qt 需 ×0.1 换算） | **`double`**（单位 V，精度 0.001V） |
| 示例 | raw=3500 → Qt 显示 350.0V（错误） | 推送 3.500 → Qt 直接显示 3.500V |
| Qt 处理 | `value * 0.1` | **直接使用**，无需再乘系数 |

> ⚠️ 类型从 `int` 变为 `double`，Qt 须更新读取方式。

---

### 3.4 最高 / 最低单体温度 `cellTempMax` / `cellTempMin`

| 项目 | 旧版 | 新版 |
|------|------|------|
| 推送类型 | `int`（raw，Qt 需 ×0.1 换算） | `int`（已减偏移 -40，单位 ℃） |
| 示例 | raw=65 → Qt 显示 6.5℃（错误） | 推送 25 → Qt 直接显示 25℃ |
| Qt 处理 | `value * 0.1` | **直接使用，单位 ℃** |

---

## 4. 速度模式编码变更（Breaking）

> ⚠️ **`speedMode` 的编码含义发生变化，Qt 侧显示逻辑须同步修正。**

| 值 | 旧版含义 | 新版含义 |
|----|---------|---------|
| `0` | 低速 E（乌龟） | **经济 E** |
| `1` | 高速 P（兔子） | **标准 S** |
| `2` | 标准 S | **高速 P** |

Qt 侧下发 `speedMode` 命令和读取显示均需按新编码对应。

---

## 5. 转向角算法修正

`steeringAngle` 的 CAN 解析公式修正（对绝大多数数值影响极小，无需 Qt 改动）：

| 项目 | 旧版 | 新版 |
|------|------|------|
| 公式 | `(raw - 1200) * 0.1` | `raw * 0.1 - 1200` |
| 差异 | 对整数 raw 结果相同；对非整数中间值有微小误差 | 精度更准确 |

Qt 侧无需修改。

---

## 6. filter_table 移除字段

以下字段**不再推送**，Qt 侧若有引用请删除：

| property_name | 原说明 |
|---------------|--------|
| `fuseFaultCode` | 保险盒故障码（已移除推送） |
| `activeFaultCodes` | 保险盒活跃故障码列表（已移除推送） |

---

## 7. 变更汇总速查表

| 变更类型 | property_name | 影响 | Qt 是否需要修改 |
|---------|---------------|------|----------------|
| ✅ 新增 | `disableCardSwipe` | cmd_control + filter_table | 需新增 |
| ✅ 新增 | `displayLogo` | cmd_control + filter_table | 需新增 |
| ✅ 新增 | `tempCardSwipe` | cmd_control + filter_table | 需新增 |
| ⚠️ Breaking | `current` | 偏移修正，数值范围变化 | 需确认显示范围 |
| ⚠️ Breaking | `soc` | 值域收敛到 0~100 | 移除 Qt 侧 ÷255 等换算 |
| ⚠️ Breaking | `cellVoltMax` | `int`→`double`，单位 V | **必须修改读取类型** |
| ⚠️ Breaking | `cellVoltMin` | `int`→`double`，单位 V | **必须修改读取类型** |
| ⚠️ Breaking | `cellTempMax` | 已含偏移 -40，单位 ℃ | 移除 Qt 侧 ×0.1 换算 |
| ⚠️ Breaking | `cellTempMin` | 已含偏移 -40，单位 ℃ | 移除 Qt 侧 ×0.1 换算 |
| ⚠️ Breaking | `speedMode` | 编码 1↔2 互换 | **必须修改显示映射** |
| ❌ 移除 | `fuseFaultCode` | 不再推送 | 删除相关读取代码 |
| ❌ 移除 | `activeFaultCodes` | 不再推送 | 删除相关读取代码 |
| — | `steeringAngle` | 算法微调，值无实质变化 | 无需修改 |
