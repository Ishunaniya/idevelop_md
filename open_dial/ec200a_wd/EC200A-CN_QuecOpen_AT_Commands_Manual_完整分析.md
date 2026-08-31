# Quectel EC200A-CN(TA) QuecOpen AT 指令手册 完整分析

> **文档信息**
> - 产品系列：LTE Standard Module Series
> - 版本：V1.0.0
> - 日期：2022-08-26
> - 状态：Preliminary（预发布）
> - 总页数：58 页
> - 版本历史：
>   - 2021-12-15，Morris XIAO，创建文档
>   - 2022-08-26，Lyndsay XIE，预发布版本

---

## 目录

1. [简介](#1-简介)
2. [通用指令](#2-通用指令)
3. [(U)SIM 相关指令](#3-usim-相关指令)
4. [网络服务指令](#4-网络服务指令)
5. [分组域指令](#5-分组域指令)
6. [音频指令](#6-音频指令)
7. [硬件相关指令](#7-硬件相关指令)
8. [附录参考](#8-附录参考)

---

## 1 简介

本文档介绍 Quectel EC200A-CN(TA) 模块在 QuecOpen® 解决方案中所支持的 AT 指令集。

### 1.1 定义

| 符号 | 含义 |
|------|------|
| `<CR>` | 回车符 |
| `<LF>` | 换行符 |
| `<...>` | 参数名。尖括号本身不出现在命令行中 |
| `[...]` | 命令的可选参数，或 TA 信息响应的可选部分。方括号不出现在命令行中。若可选参数未给出，则新值等于上次值或默认值（另有说明除外）|
| **下划线** | 参数的默认值 |

### 1.2 AT 指令语法

所有命令行必须以 `AT` 或 `at` 开头，以 `<CR>` 结尾。信息响应和结果码始终以回车和换行字符开始和结束：`<CR><LF><response><CR><LF>`。在本文档的表格中，`<CR>` 和 `<LF>` 被有意省略。

EC200A-CN(TA) 模块支持的 AT 指令集是国际标准（3GPP TS 27.007、3GPP TS 27.005、ITU-T V.25ter）与 Quectel 自有 AT 指令的结合。

AT 指令从语法上分为三类：

#### 基本指令（Basic Command）

格式：`AT<x><n>` 或 `AT&<x><n>`

- `<x>` 是指令名，`<n>` 是该指令的参数
- 示例：`ATE<n>`，控制 DCE 是否将从 DTE 接收到的字符回显

#### S 参数语法（S Parameter Syntax）

格式：`ATS<n>=<m>`

- `<n>` 是 S 寄存器的索引号
- `<m>` 是要赋予的值

#### 扩展指令（Extended Command）

扩展指令可以在多种模式下操作：

**表 1：AT 指令类型**

| 指令类型 | 语法 | 说明 |
|----------|------|------|
| 测试指令 | `AT+<cmd>=?` | 测试对应写指令是否存在，并给出参数的类型、值或范围 |
| 读指令 | `AT+<cmd>?` | 查询对应写指令的当前参数值 |
| 写指令 | `AT+<cmd>=<p1>[,<p2>[,<p3>[...]]]` | 设置用户可定义的参数值 |
| 执行指令 | `AT+<cmd>` | 返回特定信息参数或执行特定动作 |

**多指令行**：用分号（`;`）分隔，只有第一条指令带 `AT` 前缀；指令可大小写混用。

**空格规则**：以下情况空格被保留，其余情况忽略：
- 引号字符串内
- 未加引号的字符串或数值参数内
- IP 地址内
- AT 指令名中直到 `=`、`?` 或 `=?` 为止

**其他规则**：
- 输入时至少需要一个回车；换行字符被忽略，因此可以在输入中使用回车/换行对
- 若 `AT` 令牌后未输入任何指令，返回 `OK`；若输入非法指令，返回 `ERROR`
- 可选参数除非有明确说明，否则必须提供到最后一个被输入的参数位置

### 1.3 AT 指令响应

当 AT 指令处理器处理完一行后，将输出 `OK`、`ERROR` 或 `+CME ERROR: <err>`，表示已准备好接受新指令。主动请求的信息响应在最终 `OK`、`ERROR` 或 `+CME ERROR: <err>` 之前发送。

响应格式如下：

```
<CR><LF>+CMD1:<parameters><CR><LF>
<CR><LF>OK<CR><LF>
```

或：

```
<CR><LF><parameters><CR><LF>
<CR><LF>OK<CR><LF>
```

### 1.4 支持的字符集

模块的 AT 指令接口默认使用 GSM 字符集，支持以下字符集（定义于 *3GPP TS 27.005*）：

- **GSM format**（GSM 格式）
- **UCS-2**
- **IRA**

字符集影响 SMS 和 SMS 小区广播消息的发送与接收，以及电话簿条目文本字段的输入和显示。

### 1.5 AT 指令端口

两个 USB 端口（USB modem 端口 和 USB AT 端口）均支持 AT 指令通信和数据传输。

### 1.6 主动上报码（URC）

URC（Unsolicited Result Code，主动上报码）作为上报消息，不是已执行 AT 指令的响应的一部分。EC200A-CN(TA) 在未被 TE 请求的情况下自动发出 URC，当特定事件发生时触发。

典型触发 URC 的事件包括：
- 来电（`RING`）
- 接收到短消息
- 高/低电压告警
- 高/低温度告警
- 等

### 1.7 AT 指令示例声明

本文档中的 AT 指令示例仅用于帮助理解指令的用法，不应视为 Quectel 关于如何设计程序流程或模块状态设置的推荐或建议。示例之间不存在相关性，也不应视为需按顺序执行。

---

## 2 通用指令

### 2.1 ATI — 显示 MT 标识信息

**功能说明**：该指令返回 MT（Mobile Terminal，移动终端）的标识信息文本。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 执行指令 | `ATI` | `Quectel`<br>`<objectID>`<br>`Revision: <revision>`<br><br>`OK` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | / |
| 参考标准 | V.25ter |

**参数说明**：

| 参数 | 类型 | 说明 |
|------|------|------|
| `<objectID>` | 字符串类型 | 设备类型标识符 |
| `<revision>` | 字符串类型 | MT 固件版本的标识文本 |

**示例**：

```
ATI
Quectel
EC200A-CNTA
Revision: EC200ACNTAR01A03M2G_OCPU

OK
```

---

### 2.2 AT+GSN — 请求国际移动设备标识（IMEI）

**功能说明**：该指令请求 ME（Mobile Equipment，移动设备）的 IMEI（International Mobile Equipment Identity，国际移动设备标识）号码，可用于标识单个 ME 设备。与 `AT+CGSN`（第 2.3 节）功能相同。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+GSN=?` | `OK` |
| 执行指令 | `AT+GSN` | `<IMEI>`<br><br>`OK` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | / |
| 参考标准 | V.25ter |

**参数说明**：

| 参数 | 类型 | 说明 |
|------|------|------|
| `<IMEI>` | 字符串类型 | ME 的 IMEI 号码 |

> **注意**：IMEI 对每台 ME 唯一，可用于标识 ME。

---

### 2.3 AT+CGSN — 请求国际移动设备标识（IMEI）

**功能说明**：请求 ME 的 IMEI 号码，与 `AT+GSN` 功能相同。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CGSN=?` | `OK` |
| 执行指令 | `AT+CGSN` | `<IMEI>`<br><br>`OK` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | / |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

| 参数 | 类型 | 说明 |
|------|------|------|
| `<IMEI>` | 字符串类型 | ME 的 IMEI 号码 |

> **注意**：IMEI 对每台 ME 唯一，可用于标识 ME。

---

### 2.4 ATE — 设置指令回显模式

**功能说明**：该指令控制 TA（Terminal Adapter，终端适配器）在 AT 指令模式下是否将从 TE（Terminal Equipment，终端设备）接收到的字符回显。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 执行指令 | `ATE<value>` | `OK` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | 指令立即生效；配置不保存 |
| 参考标准 | V.25ter |

**参数说明**：

| 参数 | 类型 | 说明 |
|------|------|------|
| `<value>` | 整数类型 | 是否回显从 TE 接收到的字符 |
| | `0` | 关闭（OFF） |
| | `1` | 开启（ON）（**默认值**） |

---

### 2.5 AT+CFUN — 设置 MT 功能

**功能说明**：该指令选择 MT 的功能级别 `<fun>`，也可以复位 MT。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CFUN=?` | `+CFUN: (list of supported <fun>s),(list of supported <rst>s)`<br><br>`OK` |
| 读指令 | `AT+CFUN?` | `+CFUN: <fun>`<br><br>`OK` |
| 写指令 | `AT+CFUN=<fun>[,<rst>]` | `OK`<br><br>若 MT 功能相关错误：<br>`+CME ERROR: <err>`<br>或<br>`ERROR` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 15 s（由网络决定） |
| 特性 | / |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

| 参数 | 类型 | 说明 |
|------|------|------|
| `<fun>` | 整数类型 | 功能级别 |
| | `0` | 最小功能 |
| | `1` | 完整功能（**默认值**） |
| | `3` | 禁止 MT 接收 RF 信号 |
| | `4` | 禁止 MT 发射和接收 RF 信号 |
| | `5` | 禁用 (U)SIM |
| | `6` | 关闭第二路接收（Second RX off） |
| `<rst>` | 整数类型 | 复位控制 |
| | `0` | 设置功能级别前不复位 MT（**默认值**） |
| | `1` | 设置功能级别前先复位 MT，复位后设备完全可用（仅适用于 `<fun>=1`） |
| `<err>` | — | MT 错误结果码，详见第 8.2 节 |

**示例**：

```
AT+CFUN=0                        //切换 UE 至最小功能
OK
AT+CPIN?
+CME ERROR: 13                   //(U)SIM 故障

AT+CFUN=1                        //切换 UE 至完整功能
OK
AT+CPIN?
+CPIN: READY

OK
AT+COPS?
+COPS: 0,2,"46000",7             //运营商已注册

OK
```

---

## 3 (U)SIM 相关指令

### 3.1 AT+CIMI — 请求国际移动用户标识（IMSI）

**功能说明**：该指令请求 IMSI（International Mobile Subscriber Identity，国际移动用户标识），用于 TE 标识附着于 MT 的 UICC（GSM 或 SIM）中的单个 SIM 卡或活动应用程序。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CIMI=?` | `OK` |
| 执行指令 | `AT+CIMI` | `<IMSI>`<br><br>`OK`<br><br>若有 MT 功能相关错误：<br>`+CME ERROR: <err>`<br>或<br>`ERROR` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | / |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

| 参数 | 类型 | 说明 |
|------|------|------|
| `<IMSI>` | 字符串类型 | 国际移动用户标识（不含双引号） |
| `<err>` | — | MT 错误结果码，详见第 8.2 节 |

**示例**：

```
AT+CIMI                          //查询附着于 MT 的 (U)SIM 的 IMSI 号码
460023210226023                  //附着于 MT 的 (U)SIM 的 IMSI 号码

OK
```

---

### 3.2 AT+CLCK — 设施锁定

**功能说明**：该指令对 MT 或网络设施 `<fac>` 进行锁定、解锁或查询状态。在设置或查询网络设施时可以中断。通常需要密码才能执行此类操作。PF、PN、PU、PP 和 PC 锁的出厂默认密码为 `"12341234"`。查询网络服务状态时（`<mode>=2`），若某业务类别未激活，仅在该业务类别的所有 `<class>` 均未激活时才返回"未激活"状态行（`<status>=0`）。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CLCK=?` | `+CLCK: (list of supported <fac>s)`<br><br>`OK` |
| 写指令 | `AT+CLCK=<fac>,<mode>[,<passwd>[,<class>]]` | 若 `<mode>≠2` 且设置成功：`OK`<br><br>若 `<mode>=2` 且设置成功：<br>`+CLCK: <status>[,<class>]`<br>`[+CLCK: <status>[,<class>]]`<br>`[…]`<br><br>`OK` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 5 s |
| 特性 | 指令立即生效；配置自动保存 |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

**`<fac>` — 字符串类型，设施锁定类型**：

| 值 | 说明 |
|----|------|
| `"SC"` | (U)SIM 卡锁（在 MT 开机时及发出此锁定指令时要求输入密码） |
| `"AO"` | BAOC，禁止所有呼出（见 *3GPP TS 22.088 条款 1*） |
| `"OI"` | BOIC，禁止呼出国际电话（见 *3GPP TS 22.088 条款 1*） |
| `"OX"` | BOIC-exHC，禁止除本国外的国际呼出（见 *3GPP TS 22.088 条款 1*） |
| `"AI"` | BAIC，禁止所有呼入（见 *3GPP TS 22.088 条款 2*） |
| `"IR"` | BIC-Roam，漫游时禁止呼入（见 *3GPP TS 22.088 条款 2*） |
| `"AB"` | 所有禁止服务（见 *3GPP TS 22.030*，仅适用于 `<mode>=0`） |
| `"AG"` | 所有呼出禁止服务（见 *3GPP TS 22.030*，仅适用于 `<mode>=0`） |
| `"AC"` | 所有呼入禁止服务（见 *3GPP TS 22.030*，仅适用于 `<mode>=0`） |
| `"FD"` | SIM 卡固定拨号（若当前会话中未完成 PIN2 认证，则 PIN2 为必需密码） |
| `"PF"` | 锁定手机至首次插入的 SIM/UICC 卡（PH-FSIM）（插入其他 SIM/UICC 时要求密码） |
| `"PN"` | 网络个性化（见 *3GPP TS 22.022*） |
| `"PU"` | 网络子集个性化（见 *3GPP TS 22.022*） |
| `"PP"` | 服务提供商个性化（见 *3GPP TS 22.022*） |
| `"PC"` | 企业个性化（见 *3GPP TS 22.022*） |

**`<mode>` — 整数类型，操作模式**：

| 值 | 说明 |
|----|------|
| `0` | 解锁 |
| `1` | 锁定 |
| `2` | 查询状态 |

**`<passwd>` — 字符串类型**：密码

**`<class>` — 整数类型**：信息类别之和（每类用一个整数表示）：

| 值 | 说明 |
|----|------|
| `1` | 语音 |
| `2` | 数据 |
| `4` | 传真 |
| `7` | 除短信外的所有电话业务（**默认值**） |
| `8` | 短消息业务 |
| `16` | 数据电路同步 |
| `32` | 数据电路异步 |

**`<status>` — 整数类型**：

| 值 | 说明 |
|----|------|
| `0` | 未激活 |
| `1` | 已激活 |

**示例**：

```
AT+CLCK="SC",2                   //查询 (U)SIM 卡状态
+CLCK: 0                         //(U)SIM 卡未激活

OK
AT+CLCK="SC",1,"1234"            //锁定 (U)SIM 卡，密码为 1234
OK
AT+CLCK="SC",2                   //查询 (U)SIM 卡状态
+CLCK: 1                         //(U)SIM 卡已激活

OK
AT+CLCK="SC",0,"1234"            //解锁 (U)SIM 卡，密码为 1234
OK
```

---

### 3.3 AT+CPIN — 输入 PIN 码

**功能说明**：该指令向 MT 发送操作所需密码（(U)SIM PIN、(U)SIM PUK、PH-SIM PIN 等），或查询 MT 是否需要密码才能操作。

若 PIN 需要输入两次，MT 自动重复 PIN。若无待处理的 PIN 请求，则不对 MT 采取任何操作，并向 TE 返回错误消息 `+CME ERROR`。

若所需 PIN 为 (U)SIM PUK 或 (U)SIM PUK2，则需要第二个参数 `<newpin>`，用于替换 (U)SIM 中的旧 PIN。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CPIN=?` | `OK` |
| 读指令 | `AT+CPIN?` | `+CPIN: <code>`<br><br>`OK`<br><br>若有 MT 功能相关错误：<br>`+CME ERROR: <err>`<br>或<br>`ERROR` |
| 写指令 | `AT+CPIN=<pin>[,<newpin>]` | `OK` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 5 s |
| 特性 | 指令立即生效；配置自动保存 |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

**`<code>` — 字符串类型（不含双引号），MT 所需密码类型**：

| 值 | 说明 |
|----|------|
| `READY` | MT 无待输入密码 |
| `SIM PIN` | MT 等待输入 (U)SIM PIN |
| `SIM PUK` | MT 等待输入 (U)SIM PUK |
| `SIM PIN2` | MT 等待输入 (U)SIM PIN2 |
| `SIM PUK2` | MT 等待输入 (U)SIM PUK2 |
| `PH-NET PIN` | MT 等待输入网络个性化密码 |
| `PH-NET PUK` | MT 等待输入网络个性化解锁密码 |
| `PH-NETSUB PIN` | MT 等待输入网络子集个性化密码 |
| `PH-NETSUB PUK` | MT 等待输入网络子集个性化解锁密码 |
| `PH-SP PIN` | MT 等待输入服务提供商个性化密码 |
| `PH-SP PUK` | MT 等待输入服务提供商个性化解锁密码 |
| `PH-CORP PIN` | MT 等待输入企业个性化密码 |
| `PH-CORP PUK` | MT 等待输入企业个性化解锁密码 |

| 参数 | 类型 | 说明 |
|------|------|------|
| `<pin>` | 字符串类型 | 密码。若所需密码为 PUK，`<pin>` 后必须跟 `<newpin>` |
| `<newpin>` | 字符串类型 | 若所需码为 PUK，则需要新 PIN |
| `<err>` | — | MT 错误结果码，详见第 8.2 节 |

**示例**：

```
//输入 PIN
AT+CPIN?
+CPIN: SIM PIN                   //等待输入 (U)SIM PIN

OK
AT+CPIN="1234"                   //输入 PIN
OK

+CPIN: READY
AT+CPIN?
+CPIN: READY                     //PIN 已输入

OK
//输入 PUK 和新 PIN
AT+CPIN?
+CPIN: SIM PUK                   //等待输入 (U)SIM PUK

OK
AT+CPIN="26601934","1234"        //输入 PUK 及新 PIN 密码
OK

+CPIN: READY
AT+CPIN?
+CPIN: READY                     //PUK 已输入

OK
```

---

### 3.4 AT+CPWD — 修改密码

**功能说明**：写指令为 `AT+CLCK` 定义的设施锁定功能设置新密码。测试指令返回可用设施及其密码最大长度的列表。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CPWD=?` | `+CPWD: list of supported (<fac>,<pwdlength>)s`<br><br>`OK` |
| 写指令 | `AT+CPWD=<fac>,<oldpwd>,<newpwd>` | `OK` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 5 s |
| 特性 | 指令立即生效；配置自动保存 |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

**`<fac>` — 字符串类型，设施锁定类型**（同 `AT+CLCK`）：

| 值 | 说明 |
|----|------|
| `"SC"` | (U)SIM 卡锁 |
| `"AO"` | BAOC（见 *3GPP TS 22.088 条款 1*） |
| `"OI"` | BOIC（见 *3GPP TS 22.088 条款 1*） |
| `"OX"` | BOIC-exHC（见 *3GPP TS 22.088 条款 1*） |
| `"AI"` | BAIC（见 *3GPP TS 22.088 条款 2*） |
| `"IR"` | BIC-Roam（见 *3GPP TS 22.088 条款 2*） |
| `"AB"` | 所有禁止服务（见 *3GPP TS 22.030*，仅适用于 `<mode>=0`） |
| `"AG"` | 所有呼出禁止服务（见 *3GPP TS 22.030*，仅适用于 `<mode>=0`） |
| `"AC"` | 所有呼入禁止服务（见 *3GPP TS 22.030*，仅适用于 `<mode>=0`） |
| `"P2"` | (U)SIM PIN2 |

| 参数 | 类型 | 说明 |
|------|------|------|
| `<pwdlength>` | 整数类型 | 密码最大长度 |
| `<oldpwd>` | 字符串类型 | 用户界面或指令所指定的该设施当前密码 |
| `<newpwd>` | 字符串类型 | 新密码 |

**示例**：

```
AT+CPIN?
+CPIN: READY

OK
AT+CPWD="SC","1234","4321"       //将 (U)SIM 卡密码从 "1234" 改为 "4321"
OK
//重启 MT 或重新激活 (U)SIM 卡
AT+CPIN?                         //等待输入 (U)SIM PIN
+CPIN: SIM PIN

OK
AT+CPIN="4321"                   //必须输入 PIN 才能定义新密码 "4321"
OK

+CPIN: READY
```

---

### 3.5 AT+CSIM — 通用 (U)SIM 访问

**功能说明**：该指令允许远程应用程序（在 TE 上）直接控制插入当前选定卡槽的 (U)SIM。TE 应在 GSM/UMTS 规定的帧内对 (U)SIM 信息进行处理。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CSIM=?` | `OK` |
| 写指令 | `AT+CSIM=<length>,<command>` | `+CSIM: <length>,<response>`<br><br>`OK`<br>或<br>`ERROR`<br><br>若有 MT 功能相关错误：<br>`+CME ERROR: <err>` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | 指令立即生效；配置自动保存 |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

| 参数 | 类型 | 说明 |
|------|------|------|
| `<length>` | — | 发送给 TE 的 `<command>` 或 `<response>` 中字符的长度（为命令或响应实际长度的两倍） |
| `<command>` | — | MT 传递给 (U)SIM 的命令，格式参见 *3GPP TS 51.011*（十六进制字符格式） |
| `<response>` | — | (U)SIM 传递给 MT 的响应，格式参见 *3GPP TS 51.011*（十六进制字符格式） |
| `<err>` | — | MT 错误结果码，详见第 8.2 节 |

---

### 3.6 AT+CRSM — 受限 (U)SIM 访问

**功能说明**：该指令提供对 (U)SIM 数据库更简便但更有限的访问。向 MT 传递 (U)SIM `<command>` 及其所需参数。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CRSM=?` | `OK` |
| 写指令 | `AT+CRSM=<command>[,<fileid>[,<P1>,<P2>,<P3>[,<data>[,<pathid>]]]]` | `+CRSM: <sw1>,<sw2>[,<response>]`<br><br>`OK`<br>或<br>`ERROR`<br><br>若有 MT 功能相关错误：<br>`+CME ERROR: <err>` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | 指令立即生效；配置自动保存 |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

**`<command>` — 整数类型，MT 传递给 (U)SIM 的命令**：

| 值 | 说明 |
|----|------|
| `176` | READ BINARY（读二进制） |
| `178` | READ RECORD（读记录） |
| `192` | GET RESPONSE（获取响应） |
| `214` | UPDATE BINARY（更新二进制） |
| `220` | UPDATE RECORD（更新记录） |
| `242` | STATUS（状态） |

| 参数 | 类型 | 说明 |
|------|------|------|
| `<fileid>` | 整数类型 | (U)SIM 基本数据文件的标识符，除 STATUS 指令外所有指令均为必填 |
| `<P1>`、`<P2>`、`<P3>` | — | MT 传递给 (U)SIM 的参数，除 GET RESPONSE 和 STATUS 外所有指令均为必填 |
| `<data>` | — | 写入 (U)SIM 的信息（十六进制字符格式） |
| `<pathid>` | — | SIM/UICC 基本文件的目录路径（十六进制格式，定义于 *ETSI TS 102 221*） |
| `<sw1>`、`<sw2>` | 整数类型 | (U)SIM 关于实际命令执行的信息，无论成功或失败均会返回 |
| `<response>` | — | 成功执行上一指令的响应（十六进制字符格式）。STATUS 和 GET RESPONSE 返回当前基本数据字段的信息（包括文件类型和大小，见 *3GPP TS 51.011*）。READ BINARY 或 READ RECORD 后返回请求数据。UPDATE BINARY 或 UPDATE RECORD 成功后不返回 `<response>` |
| `<err>` | — | 错误码，详见第 8.2 节 |

---

## 4 网络服务指令

### 4.1 AT+COPS — PLMN 选择

**功能说明**：返回当前运营商及其状态，允许自动或手动选择网络。

- **测试指令**：返回一组五个参数集合，每组代表网络中的一个运营商（包括：可用性整数、运营商长短字母格式名称、数字格式名称及接入技术）。运营商按顺序排列：归属网络、(U)SIM 中注册的网络、其他网络。
- **读指令**：返回当前模式和已选运营商。若无选定运营商，`<format>`、`<oper>` 和 `<AcT>` 省略。
- **写指令**：强制选择并注册 GSM/UMTS 网络运营商。若选定运营商不可用，不会选择其他运营商（`<mode>=4` 除外）。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+COPS=?` | `+COPS: [range of supported (<stat>,long alphanumeric <oper>,short alphanumeric <oper>,numeric <oper>[,<AcT>])s][,,(range of supported <mode>s),(range of supported <format>s)]`<br><br>`OK`<br><br>若有错误：`+CME ERROR: <err>` 或 `ERROR` |
| 读指令 | `AT+COPS?` | `+COPS: <mode>[,<format>[,<oper>][,<AcT>]]`<br><br>`OK`<br><br>若有错误：`+CME ERROR: <err>` 或 `ERROR` |
| 写指令 | `AT+COPS=<mode>[,<format>[,<oper>[,<AcT>]]]` | `OK`<br><br>若有错误：`+CME ERROR: <err>` 或 `ERROR` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 180 s（由网络决定） |
| 特性 | / |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

**`<stat>` — 整数类型**：

| 值 | 说明 |
|----|------|
| `0` | 未知 |
| `1` | 运营商可用 |
| `2` | 当前运营商 |
| `3` | 运营商禁止 |

**`<oper>` — 字符串类型**：按 `<format>` 格式的运营商

**`<mode>` — 整数类型，选择模式**：

| 值 | 说明 |
|----|------|
| `0` | 自动（`<oper>` 字段忽略）（**默认值**） |
| `1` | 手动（`<oper>` 字段必须提供，`<AcT>` 可选） |
| `2` | 从网络注销 |
| `3` | 仅设置 `<format>`（用于 `AT+COPS?` 读指令）；不尝试注册/注销（`<oper>` 和 `<AcT>` 字段忽略）；不适用于读指令响应 |
| `4` | 手动/自动（`<oper>` 字段需提供）；若手动选择失败，进入自动模式（`<mode>=0`） |

**`<format>` — 整数类型，运营商格式**：

| 值 | 说明 |
|----|------|
| `0` | 长字母数字格式 `<oper>`（最多 16 个字符）（**默认值**） |
| `1` | 短字母数字格式 `<oper>` |
| `2` | 数字格式 `<oper>`（GSM 位置区标识号） |

**`<AcT>` — 整数类型，接入技术**（值 3、4、5、6 仅出现在读指令响应中，当 MS 处于数据服务状态时）：

| 值 | 说明 |
|----|------|
| `0` | GSM |
| `2` | UTRAN |
| `3` | GSM w/EGPRS |
| `4` | UTRAN w/HSDPA |
| `5` | UTRAN w/HSUPA |
| `6` | UTRAN w/HSDPA and HSUPA |
| `7` | E-UTRAN |

| 参数 | 类型 | 说明 |
|------|------|------|
| `<err>` | — | MT 错误结果码，详见第 8.2 节 |

**示例**：

```
AT+COPS=?                        //列出所有当前网络运营商
+COPS: (1,"CHN-UNICOM","UNICOM","46001",2),(1,"CHN-UNICOM","UNICOM","46001",0),
(2,"CHN-UNICOM","UNICOM","46001",7),(1,"CHN-CT","CT","46011",7),
(3,"CHINA MOBILE","CMCC","46000",,),(0-4),(0-2)

OK
AT+COPS?                         //查询当前选定的网络运营商
+COPS: 0,2,"46000",7

OK
```

---

### 4.2 AT+CREG — 网络注册状态

**功能说明**：写指令控制主动上报码 `+CREG: <stat>` 的呈现方式（当 `<n>=1` 时，MT 的电路模式网络注册状态在 GERAN/UTRAN/E-UTRAN 中发生变化；当 `<n>=2` 时，网络小区在 GERAN/UTRAN/E-UTRAN 中发生变化，额外上报位置信息）。读指令返回结果码呈现状态和注册状态 `<stat>`。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CREG=?` | `+CREG: (range of supported <n>s)`<br><br>`OK` |
| 读指令 | `AT+CREG?` | `+CREG: <n>,<stat>[,<lac>,<ci>[,<AcT>]]`<br><br>`OK`<br><br>若有错误：`+CME ERROR: <err>` 或 `ERROR` |
| 写指令 | `AT+CREG=[<n>]` | `OK` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | 指令立即生效；配置不保存 |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

**`<n>` — 整数类型，是否启用网络注册 URC**：

| 值 | 说明 |
|----|------|
| `0` | 禁用网络注册主动上报码（**默认值**） |
| `1` | 启用网络注册主动上报码：`+CREG: <stat>` |
| `2` | 启用带位置信息的网络注册主动上报码：`+CREG: <stat>[,<lac>,<ci>[,<AcT>]]` |

**`<stat>` — 整数类型，电路模式注册状态**：

| 值 | 说明 |
|----|------|
| `0` | 未注册，MT 未在搜索新运营商 |
| `1` | 已注册，归属网络 |
| `2` | 未注册，MT 正在搜索新运营商 |
| `3` | 注册被拒绝 |
| `4` | 未知（例如，超出 GERAN/UTRAN/E-UTRAN 覆盖范围） |
| `5` | 已注册，漫游 |

| 参数 | 类型 | 说明 |
|------|------|------|
| `<lac>` | 字符串类型 | 两字节位置区域码（十六进制格式） |
| `<ci>` | 字符串类型 | 四字节 GERAN/UTRAN/E-UTRAN 小区 ID（十六进制格式） |
| `<AcT>` | 整数类型 | 服务小区接入技术（值同 AT+COPS） |
| `<err>` | — | MT 错误结果码，详见第 8.2 节 |

**示例**：

```
AT+CREG=2                        //激活扩展 URC 模式
OK
AT+CREG?
+CREG: 2,1,"550c","5a29c0b",7   //URC 上报 MT 已在 E-UTRAN 注册

OK
```

---

### 4.3 AT+CSQ — 信号质量

**功能说明**：执行指令返回接收信号强度指示 `<rssi>` 和信道误码率 `<ber>`。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CSQ=?` | `+CSQ: (list of supported <rssi>s),(list of supported <ber>s)`<br><br>`OK` |
| 执行指令 | `AT+CSQ` | `+CSQ: <rssi>,<ber>`<br><br>`OK`<br><br>若有错误：`+CME ERROR: <err>` 或 `ERROR` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | / |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

**`<rssi>` — 整数类型，接收信号强度指示**：

| 值 | 说明 |
|----|------|
| `0` | -113 dBm 或更低 |
| `1` | -111 dBm |
| `2–30` | -109 dBm 至 -53 dBm |
| `31` | -51 dBm 或更高 |
| `99` | 未知或无法检测 |

**`<ber>` — 整数类型，信道误码率（百分比）**：

| 值 | 说明 |
|----|------|
| `0–7` | 参见 *3GPP TS 45.008 子条款 8.2.4* 中的 RxQual 值表 |
| `99` | 未知或无法检测 |

| 参数 | 类型 | 说明 |
|------|------|------|
| `<err>` | — | MT 错误结果码，详见第 8.2 节 |

**示例**：

```
AT+CSQ=?
+CSQ: (0-31,99),(0-7,99)

OK
AT+CSQ
+CSQ: 28,99                      //接收信号强度指示为 28，信道误码率为 99

OK
```

---

### 4.4 AT+CPOL — 首选 PLMN 列表

**功能说明**：该指令编辑 SIM 卡或 UICC（GSM 或 USIM）中活动应用程序中带有接入技术的 PLMN 选择器列表。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CPOL=?` | `+CPOL: (list of supported <index>s),(range of supported <format>s)`<br><br>`OK` |
| 读指令 | `AT+CPOL?` | `+CPOL: <index1>,<format>,<oper1>[,<GSM_AcT1>,<GSM_Compact_AcT1>,<UTRAN_AcT1>,<E-UTRAN_AcT1>]`<br>`[+CPOL: <index2>,<format>,<oper2>[,...]]`<br>`[…]`<br><br>`OK` |
| 写指令 | `AT+CPOL=<index>[,<format>[,<oper>[,<GSM_AcT>,<GSM_Compact_AcT>,<UTRAN_AcT>,<E-UTRAN_AcT>]]]` | 若省略可选参数：删除指定 `<index>` 的运营商：`OK`<br>若指定了任何可选参数：编辑首选运营商列表：`OK` 或 `ERROR`<br>若有 MT 功能相关错误：`+CME ERROR: <err>` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | / |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

| 参数 | 类型 | 说明 |
|------|------|------|
| `<indexn>` | 整数类型 | 运营商在 (U)SIM 首选运营商列表中的顺序号 |
| `<format>` | 整数类型 | `0`=长字母数字 `<oper>`（**默认**）；`1`=短字母数字 `<oper>`；`2`=数字 `<oper>` |
| `<opern>` | 字符串类型 | 格式见 `AT+COPS` |
| `<GSM_AcTn>` | 整数类型 | GSM 接入技术：`0`=未选中；`1`=已选中 |
| `<GSM_Compact_AcTn>` | 整数类型 | GSM Compact 接入技术：`0`=未选中；`1`=已选中 |
| `<UTRAN_AcTn>` | 整数类型 | UTRAN 接入技术：`0`=未选中；`1`=已选中 |
| `<E-UTRAN_AcTn>` | 整数类型 | E-UTRAN 接入技术：`0`=未选中；`1`=已选中 |
| `<err>` | — | MT 错误结果码，详见第 8.2 节 |

> **注意**：接入技术选择参数 `<GSM_AcT>`、`<GSM_Compact_AcT>`、`<UTRAN_AcT>` 和 `<E-UTRAN_AcT>` 仅在 SIM 卡或 UICC 包含带接入技术的 PLMN 选择器时才需要。

---

### 4.5 AT+COPN — 读取运营商名称

**功能说明**：该指令从 MT 返回运营商名称列表。MT 存储器中每个有字母数字等效名称 `<alphan>` 的运营商代码 `<numericn>` 均会被返回。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+COPN=?` | `OK` |
| 执行指令 | `AT+COPN` | `+COPN: <numeric1>,<alpha1>`<br>`[+COPN: <numeric2>,<alpha2>]`<br>`[…]`<br><br>`OK`<br><br>若有 MT 功能相关错误：`+CME ERROR: <err>` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 取决于运营商名称数量 |
| 特性 | / |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

| 参数 | 类型 | 说明 |
|------|------|------|
| `<numericn>` | 字符串类型 | 数字格式运营商名称（见 `AT+COPS`） |
| `<alphan>` | 字符串类型 | 长字母数字格式运营商名称（见 `AT+COPS`） |
| `<err>` | — | MT 错误结果码，详见第 8.2 节 |

---

### 4.6 AT+CTZU — 自动时区更新

**功能说明**：写指令通过 NITZ 启用或禁用自动时区更新。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CTZU=?` | `+CTZU: (list of supported <onoff>s)`<br><br>`OK` |
| 写指令 | `AT+CTZU=<onoff>` | `OK`<br>或<br>`ERROR` |
| 读指令 | `AT+CTZU?` | `+CTZU: <onoff>`<br><br>`OK` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | / |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

**`<onoff>` — 整数类型，是否启用自动时区更新**：

| 值 | 说明 |
|----|------|
| `0` | 禁用通过 NITZ 自动更新时区 |
| `1` | 启用通过 NITZ 自动更新时区，并通过 URC 更新 GMT 时间（**默认值**） |

**示例**：

```
AT+CTZU?
+CTZU: 1

OK
AT+CTZU=?
+CTZU: (0,1)

OK
AT+CTZU=0
OK
AT+CTZU?
+CTZU: 0

OK
```

---

### 4.7 AT+CTZR — 时区上报

**功能说明**：该指令控制时区变更事件上报。启用上报后，MT 在时区变更时返回主动上报码 `+CTZV: <tz>` 或 `+CTZE: <tz>,<dst>,<time>`。配置自动存储到 Flash。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CTZR=?` | `+CTZR: (list of supported <reporting>s)`<br><br>`OK` |
| 写指令 | `AT+CTZR=<reporting>` | `OK`<br>或<br>`ERROR` |
| 读指令 | `AT+CTZR?` | `+CTZR: <reporting>`<br><br>`OK` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | 指令立即生效；配置自动保存 |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

**`<reporting>` — 整数类型，是否启用时区上报**：

| 值 | 说明 |
|----|------|
| `0` | 禁用时区变更事件上报（**默认值**） |
| `1` | 通过 URC `+CTZV: <tz>` 启用时区变更事件上报 |
| `2` | 通过 URC `+CTZE: <tz>,<dst>,<time>` 启用扩展时区和本地时间上报 |

**`<tz>` — 字符串类型**：

本地时区之和（本地时间与 GMT 之差，以小时的四分之一为单位）加上夏令时。格式为 `"±zz"`，是一个固定宽度的两位整数，范围 -48 到 +56。为保持固定宽度，-9 到 +9 之间的数字带前导零（如 `"-09"`、`"+00"`、`"+09"`）。

**`<dst>` — 整数类型**：`<tz>` 是否包含夏令时调整：

| 值 | 说明 |
|----|------|
| `0` | `<tz>` 不包含夏令时调整 |
| `1` | `<tz>` 包含 +1 小时（即 `<tz>` 中 4 个季度）的夏令时调整 |
| `2` | `<tz>` 包含 +2 小时（即 `<tz>` 中 8 个季度）的夏令时调整 |

**`<time>` — 字符串类型**：

本地时间，格式为 `"YYYY/MM/DD,hh:mm:ss"`（YYYY=年，MM=月，DD=日，hh=时，mm=分，ss=秒）。该参数可由网络在传递时区信息时提供，并在扩展时区上报的 URC 中呈现。

---

## 5 分组域指令

### 5.1 AT+CGATT — PS 附着或分离

**功能说明**：该指令将 MT 附着到分组域服务或从中分离。命令完成后，MT 保持在 V.250 命令状态。若 MT 已处于请求状态，指令被忽略并返回 `OK`。若无法达到请求状态，返回 `ERROR` 或 `+CME ERROR`。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CGATT=?` | `+CGATT: (list of supported <state>s)`<br><br>`OK` |
| 读指令 | `AT+CGATT?` | `+CGATT: <state>`<br><br>`OK` |
| 写指令 | `AT+CGATT=<state>` | `OK`<br>或<br>`ERROR`<br><br>若有 MT 功能相关错误：`+CME ERROR: <err>` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 140 s（由网络决定） |
| 特性 | / |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

**`<state>` — 整数类型，PS 附着状态**：

| 值 | 说明 |
|----|------|
| `0` | 已分离 |
| `1` | 已附着 |

> 其他值为保留值，写指令将返回 `ERROR`。

| 参数 | 类型 | 说明 |
|------|------|------|
| `<err>` | — | MT 错误结果码，详见第 8.2 节 |

**示例**：

```
AT+CGATT=1                       //附着 PS 服务
OK
AT+CGATT=0                       //从 PS 服务分离
OK
AT+CGATT?                        //查询当前 PS 服务状态
+CGATT: 0

OK
```

---

### 5.2 AT+CGDCONT — 定义 PDP 上下文

**功能说明**：该指令为特定上下文 `<cid>` 指定 PDP（Packet Data Protocol，分组数据协议）上下文参数。写指令的特殊形式（`AT+CGDCONT=<cid>`）将上下文 `<cid>` 的值设为未定义。不允许修改已激活的上下文定义。读指令返回每个已定义 PDP 上下文的当前设置。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CGDCONT=?` | `+CGDCONT: (range of supported <cid>s),<PDP_type>,<APN>,<PDP_addr>,(range of supported <d_comp>s),(range of supported <h_comp>s),(list of supported <IPv4AddrAlloc>s),(list of supported <request_type>s),(range of supported <P-CSCF_discovery>s),(list of supported <IM_CN_Signalling_Flag_Ind>s)`<br><br>`OK` |
| 读指令 | `AT+CGDCONT?` | `+CGDCONT: <cid>,<PDP_type>,<APN>,<PDP_addr>,<d_comp>,<h_comp>,<IPv4AddrAlloc>,<request_type>,<P-CSCF_discovery>,<IM_CN_Signalling_Flag_Ind>`<br>`[…]`<br><br>`OK` |
| 写指令 | `AT+CGDCONT=<cid>[,<PDP_type>[,<APN>[,<PDP_addr>[,<d_comp>[,<h_comp>[,<IPv4AddrAlloc>[,<request_type>[,<P-CSCF_discovery>[,<IM_CN_Signalling_Flag_Ind>]]]]]]]]]]` | `OK`<br>或<br>`ERROR` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | 指令立即生效；配置自动保存 |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

| 参数 | 类型 | 说明 |
|------|------|------|
| `<cid>` | 整数类型 | PDP 上下文标识符，对 TE-MT 接口本地有效，用于其他 PDP 上下文相关指令。允许值范围（最小值=1）由测试指令返回，范围：1–15 |
| `<PDP_type>` | 字符串类型 | 分组数据协议类型，EPS 服务仅支持 `"IP"`、`"IPv6"`、`"IPV4V6"`：<br>`"IP"` = IPv4 互联网协议（*IETF STD 5*）<br>`"PPP"` = 点对点协议（*IETF STD 51*）<br>`"IPV6"` = 互联网协议版本 6（*RFC 2460*）<br>`"IPV4V6"` = 处理双 IP 栈 UE 能力的虚拟 `<PDP_type>`（见 *3GPP TS 24.301*） |
| `<APN>` | 字符串类型 | 接入点名称，用于选择 GGSN 或外部分组数据网络的逻辑名称。若值为 null 或省略，则请求订阅值 |
| `<PDP_addr>` | 字符串类型 | 在适用于 PDP 的地址空间内标识 MT。若值为 null 或省略，可由 TE 在 PDP 启动过程中提供，否则将请求动态地址。可用 `AT+CGPADDR` 读取分配的地址 |
| `<d_comp>` | 整数类型 | 控制 PDP 数据压缩（仅适用于 SNDCP，见 *3GPP TS 44.065*）：`0`=关（**默认**）；`1`=开（制造商首选压缩）；`2`=V.42bis；`3`=V.44（当前不支持） |
| `<h_comp>` | 整数类型 | 控制 PDP 头部压缩（见 *3GPP TS 44.065* 和 *3GPP TS 25.323*）：`0`=关（**默认**）；`1`=开；`2`=RFC1144（仅适用于 SNDCP）；`3`=RFC2507；`4`=RFC3095（仅适用于 PDCP） |
| `<IPv4AddrAlloc>` | 整数类型 | 控制 MT/TA 如何请求获取 IPv4 地址信息：`0`=通过 NAS 信令分配 IPv4 地址（**默认**）；`1`=通过 DHCP 分配 IPv4 地址 |
| `<request_type>` | 整数类型 | PDP 上下文激活请求类型，参见 *3GPP TS 24.301* 和 *3GPP TS 24.008* |
| `<P-CSCF_discovery>` | 整数类型 | 影响 MT/TA 如何请求获取 P-CSCF 地址（见 *3GPP TS 24.229 附录 B 和 L*）：`0`=P-CSCF 地址发现不受 `AT+CGDCONT` 影响（**默认**）；`1`=通过 NAS 信令发现 P-CSCF 地址；`2`=通过 DHCP 发现 P-CSCF 地址 |
| `<IM_CN_Signalling_Flag_Ind>` | 整数类型 | 向网络指示 PDP 上下文是否仅用于 IM CN 子系统相关信令：`0`=UE 指示 PDP 上下文不仅用于 IM CN 子系统相关信令（**默认**）；`1`=UE 指示 PDP 上下文仅用于 IM CN 子系统相关信令 |

---

### 5.3 AT+CGACT — 激活或停用 PDP 上下文

**功能说明**：激活或停用由 `AT+CGDCONT` 定义的指定 PDP 上下文。命令完成后，MT 保持在 V.250 命令状态。若任何 PDP 上下文已处于请求状态，该上下文状态保持不变。若 MT 未 PS 附着时执行激活命令，MT 首先执行 PS 附着然后尝试激活指定上下文。若未指定 `<cid>`，则激活或停用所有已定义的 PDP 上下文。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CGACT=?` | `+CGACT: (list of supported <state>s)`<br><br>`OK` |
| 读指令 | `AT+CGACT?` | `+CGACT: <cid>,<state>`<br>`[…]`<br><br>`OK` |
| 写指令 | `AT+CGACT=<state>,<cid>` | `OK`<br>或<br>`NO CARRIER`<br><br>若有 MT 功能相关错误：`+CME ERROR: <err>` 或 `ERROR` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 150 s（由网络决定） |
| 特性 | / |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

**`<state>` — 整数类型，PDP 上下文激活状态**：

| 值 | 说明 |
|----|------|
| `0` | 已停用 |
| `1` | 已激活 |

> 其他值为保留值，写指令将返回 `ERROR`。

| 参数 | 类型 | 说明 |
|------|------|------|
| `<cid>` | 整数类型 | 特定 PDP 上下文定义（见 `AT+CGDCONT`） |
| `<err>` | — | MT 错误结果码，详见第 8.2 节 |

**示例**：

```
AT+CGDCONT=2,"IP","UNINET"       //定义 PDP 上下文
OK
AT+CGACT=1,2                     //激活 PDP 上下文
OK
AT+CGACT?                        //查询当前 PDP 上下文状态
+CGACT: 1,1
+CGACT: 2,1

OK
AT+CGACT=0,2                     //停用 PDP 上下文
OK
```

---

### 5.4 AT+CGPADDR — 显示 PDP 地址

**功能说明**：该指令返回指定上下文标识符的 PDP 地址列表。若未指定 `<cid>`，则返回所有已定义上下文的地址。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CGPADDR=?` | `+CGPADDR: (list of defined <cid>s)`<br><br>`OK` |
| 写指令 | `AT+CGPADDR[=<cid>[,<cid>[,…]]]` | `+CGPADDR: <cid>,<PDP_addr>`<br>`[…]`<br><br>`OK`<br>或<br>`ERROR` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | / |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

| 参数 | 类型 | 说明 |
|------|------|------|
| `<cid>` | 整数类型 | 特定 PDP 上下文定义（见 `AT+CGDCONT`） |
| `<PDP_addr>` | 字符串类型 | 在适用于 PDP 的地址空间内标识 MT。地址可以是静态或动态的。静态地址是定义上下文时由 `AT+CGDCONT` 设置的；动态地址是上次使用 `<cid>` 引用的上下文定义激活 PDP 上下文时分配的。若无可用地址，`<PDP_addr>` 省略 |

**示例**：

```
AT+CGDCONT=1,"IP","UNINET"       //定义 PDP 上下文
OK
AT+CGACT=1,1                     //激活 PDP
OK
AT+CGPADDR=1                     //显示 PDP 地址
+CGPADDR: 1,"10.76.51.180"

OK
```

---

### 5.5 AT+CGCLASS — GPRS 移动站类别

**功能说明**：该指令设置 MT 按指定操作模式运行。详情见 *3GPP TS 23.060*。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CGCLASS=?` | `+CGCLASS: (list of supported <class>s)`<br><br>`OK` |
| 读指令 | `AT+CGCLASS?` | `+CGCLASS: <class>`<br><br>`OK` |
| 写指令 | `AT+CGCLASS=<class>` | `OK`<br><br>若有错误：`+CME ERROR: <err>` 或 `ERROR` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | 指令立即生效；配置自动保存 |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

**`<class>` — 字符串类型，GPRS 移动站类别（功能按降序排列）**：

| 值 | 说明 |
|----|------|
| `A` | A/Gb 模式下的 Class-A 操作模式，或 CS/PS 操作模式（最高模式） |
| `B` | A/Gb 模式下的 Class-B 操作模式，或 CS/PS 操作模式 |
| `CG` | A/Gb 模式下仅 PS 模式的 Class-C 操作，或 PS 操作模式 |
| `CC` | A/Gb 模式下仅 CS 模式的 Class-C 操作，或 CS 操作模式（最低模式） |

| 参数 | 类型 | 说明 |
|------|------|------|
| `<err>` | — | 错误码，详见第 8.2 节 |

---

### 5.6 AT+CGREG — GPRS 网络注册状态

**功能说明**：该指令查询网络注册状态并控制主动上报码的呈现。当 `<n>=1` 且 MT 的 GPRS 网络注册状态在 GERAN/UTRAN 中发生变化时，发送 `+CGREG: <stat>`；当 `<n>=2` 时，发送 `+CGREG: <stat>[,<lac>,<ci>[,<AcT>,<rac>]]`。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CGREG=?` | `+CGREG: (range of supported <n>s)`<br><br>`OK` |
| 读指令 | `AT+CGREG?` | `+CGREG: <n>,<stat>[,<lac>,<ci>[,<AcT>],[<rac>]]`<br><br>`OK` |
| 写指令 | `AT+CGREG=<n>` | `OK`<br>或<br>`ERROR` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | 指令立即生效；配置不保存 |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

**`<n>` — 整数类型**：

| 值 | 说明 |
|----|------|
| `0` | 禁用网络注册主动上报码（**默认值**） |
| `1` | 启用网络注册主动上报码 `+CGREG: <stat>` |
| `2` | 启用带位置信息的网络注册主动上报码 `+CGREG: <stat>[,<lac>,<ci>[,<AcT>],[<rac>]]` |

**`<stat>` — 整数类型，GPRS 注册状态**：

| 值 | 说明 |
|----|------|
| `0` | 未注册。MT 未在搜索运营商。UE 处于 GMM 状态 GMM-NULL 或 GMM-DEREGISTERED-INITIATED。GPRS 服务已禁用，但允许用户请求附着 |
| `1` | 已注册，归属网络。UE 处于 GMM 状态 GMM-REGISTERED 或 GMM-ROUTING-AREA-UPDATING-INITIATED INITIATED（在归属 PLMN） |
| `2` | 未注册，但 MT 正尝试附着或搜索运营商注册。UE 处于 GMM 状态 GMM-DEREGISTERED 或 GMM-REGISTERED-INITIATED。GPRS 服务已启用，但当前无可用 PLMN，一旦有可用 PLMN 将立即执行 GPRS 附着 |
| `3` | 注册被拒绝。UE 处于 GMM 状态 GMM-NULL。GPRS 服务已禁用，UE 不允许按用户请求附着 GPRS |
| `4` | 未知（例如，超出 GERAN/UTRAN 覆盖范围） |
| `5` | 已注册，漫游 |

| 参数 | 类型 | 说明 |
|------|------|------|
| `<lac>` | 字符串类型 | 两字节位置区域码（十六进制格式，例如 `"00C3"` 等于 195 十进制） |
| `<ci>` | 字符串类型 | 四字节 GERAN/UTRAN 小区 ID（十六进制格式） |
| `<AcT>` | 整数类型 | 所选接入技术：`0`=GSM；`2`=UTRAN；`3`=GSM w/EGPRS；`4`=UTRAN w/HSDPA；`5`=UTRAN w/HSUPA；`6`=UTRAN w/HSDPA and HSUPA；`7`=E-UTRAN |
| `<rac>` | 字符串类型 | 一字节路由区域码（十六进制格式） |

---

### 5.7 AT+CGEREP — 分组域事件上报

**功能说明**：该指令启用或禁用从 MT 向 TE 发送主动上报码 `+CGEV: XXX`（当分组域 MT 或网络中发生某些事件时）。`<mode>` 控制该指令中指定的主动上报码的处理方式；`<bfr>` 控制当 `<mode>=1` 或 `2` 时对缓冲码的影响。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CGEREP=?` | `+CGEREP: (range of supported <mode>s),(list of supported <bfr>s)`<br><br>`OK` |
| 读指令 | `AT+CGEREP?` | `+CGEREP: <mode>,<bfr>`<br><br>`OK`<br>或<br>`ERROR` |
| 写指令 | `AT+CGEREP=<mode>[,<bfr>]` | `OK`<br>或<br>`ERROR` |
| 执行指令 | `AT+CGEREP` | `OK` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | 指令立即生效；配置自动保存 |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

**`<mode>` — 整数类型**：

| 值 | 说明 |
|----|------|
| `0` | 在 MT 中缓冲主动上报码；若 MT 结果码缓冲区已满，最旧的代码将被丢弃。不向 TE 转发任何代码（**默认值**） |
| `1` | 当 MT-TE 链路保留（例如在线数据模式）时丢弃主动上报码；否则直接转发给 TE |
| `2` | 当 MT-TE 链路保留（例如在线数据模式）时在 MT 中缓冲主动上报码，链路可用时冲洗给 TE；否则直接转发给 TE |

**`<bfr>` — 整数类型**：

| 值 | 说明 |
|----|------|
| `0` | 当指定 `<mode>` 1 或 2 时，清除本指令定义的主动上报码的 MT 缓冲区（**默认值**） |
| `1` | 当指定 `<mode>` 1 或 2 时，将本指令定义的主动上报码的 MT 缓冲区冲洗给 TE（`OK` 响应应在冲洗代码前发出） |

> **注意**：主动上报码及对应事件定义如下：
>
> 1. `+CGEV: REJECT <PDP_type>,<PDP_addr>`：网络请求 PDP 上下文激活时，MT 无法向 TE 报告并带有 `+CRING` URC 而自动拒绝（不适用于 EPS）
> 2. `+CGEV: NW REACT <PDP_type>,<PDP_addr>,[<cid>]`：网络请求上下文重激活（不适用于 EPS）
> 3. `+CGEV: NW DEACT <PDP_type>,<PDP_addr>,[<cid>]`：网络强制停用上下文
> 4. `+CGEV: ME DEACT <PDP_type>,<PDP_addr>,[<cid>]`：移动设备强制停用上下文
> 5. `+CGEV: NW DETACH`：网络强制分组域分离（所有活动上下文已停用，不单独报告）
> 6. `+CGEV: ME DETACH`：移动设备强制分组域分离（所有活动上下文已停用，不单独报告）
> 7. `+CGEV: NW CLASS <class>`：网络强制 MS 类别变更，报告最高可用类别（见 `AT+CGCLASS`）
> 8. `+CGEV: ME CLASS <class>`：移动设备强制 MS 类别变更，报告最高可用类别（见 `AT+CGCLASS`）
> 9. `+CGEV: PDN ACT <cid>`：上下文已激活，在 LTE 中表示 PDN 连接，在 GSM/UMTS 中表示主 PDP 上下文（见 `AT+CGDCONT`）
> 10. `+CGEV: PDN DEACT <cid>`：上下文已停用，在 LTE 中表示 PDN 连接，在 GSM/UMTS 中表示主 PDP 上下文（见 `AT+CGDCONT`）

---

### 5.8 AT+CGSMS — 选择 MO SMS 消息服务

**功能说明**：该指令指定 MT 发送 MO（Mobile Originated，移动始发）短消息时使用的服务或服务优先级。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CGSMS=?` | `+CGSMS: (range of currently available <service>s)`<br><br>`OK` |
| 读指令 | `AT+CGSMS?` | `+CGSMS: <service>`<br><br>`OK` |
| 写指令 | `AT+CGSMS=<service>` | `OK`<br><br>若有 MT 功能相关错误：`+CME ERROR: <err>` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | 指令立即生效；配置自动保存 |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

**`<service>` — 整数类型，使用的服务或服务优先级**：

| 值 | 说明 |
|----|------|
| `0` | 分组域 |
| `1` | 电路交换（**默认值**） |
| `2` | 分组域优先（若 GPRS 不可用则使用电路交换） |
| `3` | 电路交换优先（若电路交换不可用则使用分组域） |

| 参数 | 类型 | 说明 |
|------|------|------|
| `<err>` | — | MT 错误结果码，详见第 8.2 节 |

---

### 5.9 AT+CEREG — EPS 网络注册状态

**功能说明**：该指令查询网络注册状态并控制主动上报码的呈现。当 `<n>=1` 且 MT 的 EPS 网络注册状态在 E-UTRAN 中发生变化时，发送 `+CEREG: <stat>`；当 `<n>=2` 时，发送 `+CEREG: <stat>[,<tac>,<ci>[,<AcT>]]`。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CEREG=?` | `+CEREG: (range of supported <n>s)`<br><br>`OK` |
| 读指令 | `AT+CEREG?` | `+CEREG: <n>,<stat>[,<tac>,<ci>[,<AcT>]]`<br><br>`OK` |
| 写指令 | `AT+CEREG=[<n>]` | `OK`<br>或<br>`ERROR` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | 指令立即生效；配置不保存 |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

**`<n>` — 整数类型**：

| 值 | 说明 |
|----|------|
| `0` | 禁用网络注册主动上报码（**默认值**） |
| `1` | 启用网络注册主动上报码 `+CEREG: <stat>` |
| `2` | 启用带位置信息的网络注册主动上报码 `+CEREG: <stat>[,<tac>,<ci>[,<AcT>]]` |

**`<stat>` — 整数类型，EPS 注册状态**：

| 值 | 说明 |
|----|------|
| `0` | 未注册，MT 未在搜索运营商 |
| `1` | 已注册，归属网络 |
| `2` | 未注册，但 MT 正在尝试附着或搜索运营商 |
| `3` | 注册被拒绝 |
| `4` | 未知（例如，超出 E-UTRAN 覆盖范围） |
| `5` | 已注册，漫游 |

| 参数 | 类型 | 说明 |
|------|------|------|
| `<tac>` | 字符串类型 | 两字节跟踪区域码（十六进制格式） |
| `<ci>` | 字符串类型 | 四字节 E-UTRAN 小区 ID（十六进制格式） |
| `<AcT>` | 整数类型 | 所选接入技术：`0`=GSM；`2`=UTRAN；`3`=GSM w/EGPRS；`4`=UTRAN w/HSDPA；`5`=UTRAN w/HSUPA；`6`=UTRAN w/HSDPA and HSUPA；`7`=E-UTRAN |

---

## 6 音频指令

### 6.1 AT+CLVL — 扬声器音量级别

**功能说明**：该指令选择 MT 内部扬声器的音量级别。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+CLVL=?` | `+CLVL: (list of supported <level>s)`<br><br>`OK` |
| 读指令 | `AT+CLVL?` | `+CLVL: <level>`<br><br>`OK`<br>或<br>`ERROR` |
| 写指令 | `AT+CLVL=<level>` | `OK`<br>或<br>`ERROR`<br><br>若有 MT 功能相关错误：`+CME ERROR: <err>` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | 指令立即生效；配置不保存 |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

| 参数 | 类型 | 说明 |
|------|------|------|
| `<level>` | 整数类型 | 音量级别，制造商特定范围（最小值代表最低音量）。范围：0–5；**默认值：3** |
| `<err>` | — | MT 错误结果码，详见第 8.2 节 |

---

### 6.2 AT+VTS — DTMF 和音调生成

**功能说明**：该指令发送 ASCII 字符，使 MSC（Mobile Switching Center，移动交换中心）向远端用户发送 DTMF 音调。该指令只能在语音通话中使用。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+VTS=?` | `+VTS: (list of supported <DTMF_string>s),(range of supported <duration>s)`<br><br>`OK` |
| 写指令 | `AT+VTS=<DTMF_string>[,<duration>]` | `OK`<br>或<br>`ERROR`<br><br>若有 MT 功能相关错误：`+CME ERROR: <err>` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 取决于 `<DTMF_string>` 和 `<duration>` 的长度 |
| 特性 | / |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

| 参数 | 类型 | 说明 |
|------|------|------|
| `<DTMF_string>` | 字符串类型 | ASCII 字符，集合为 `0...9,#,*,A,B,C,D`，字符串需用引号括起（`"..."`）。同时发送多个音调时，两音调的时间间隔 `<interval>` 可由 `AT+VTD` 指定。字符串最大长度为 31 字节 |
| `<duration>` | — | 每个音调的持续时间，单位 10 ms，有容差。范围：300–600，**默认值：300**。若持续时间小于网络规定的最小时间，则使用网络规定时间。若省略此参数，`<duration>` 可由 `AT+VTD` 指定 |
| `<err>` | — | MT 错误结果码，详见第 8.2 节 |

**示例**：

```
ATD12345678900;                  //拨号
OK
//通话已接通
AT+VTS="1"                       //远端呼叫者可以听到 DTMF 音调
OK
AT+VTS="1234567890A"             //同时发送多个音调
OK
```

---

### 6.3 AT+VTD — 音调持续时间

**功能说明**：该指令设置 DTMF 音调的持续时间。也可以在同时发送多个音调时设置两个音调之间的时间间隔。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+VTD=?` | `+VTD: (range of supported <duration>s),(range of supported <interval>s)`<br><br>`OK` |
| 读指令 | `AT+VTD?` | `+VTD: <duration>,<interval>`<br><br>`OK` |
| 写指令 | `AT+VTD=<duration>[,<interval>]` | `OK`<br>或<br>`ERROR`<br><br>若有 MT 功能相关错误：`+CME ERROR: <err>` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | 指令立即生效；配置不保存 |
| 参考标准 | 3GPP TS 27.007 |

**参数说明**：

| 参数 | 类型 | 说明 |
|------|------|------|
| `<duration>` | 整数类型 | 音调持续时间，单位 1/10 秒，有容差。范围：300–600，**默认值：300**。若持续时间小于网络规定的最小时间，则使用网络规定时间 |
| `<interval>` | 整数类型 | 使用 `AT+VTS` 同时发送多个音调时，两音调的时间间隔。范围：300–600，**默认值：300**，单位：100 ms |
| `<err>` | — | MT 错误结果码，详见第 8.2 节 |

---

## 7 硬件相关指令

### 7.1 AT+QADC — 读取 ADC 值

**功能说明**：该指令读取 ADC（Analog-to-Digital Converter，模数转换器）通道的电压值。

| 指令类型 | 语法 | 响应 |
|----------|------|------|
| 测试指令 | `AT+QADC=?` | `+QADC: (list of supported <port>s)`<br><br>`OK` |
| 读指令 | `AT+QADC=<port>` | `+QADC: <status>,<value>`<br><br>`OK` |

| 属性 | 值 |
|------|----|
| 最大响应时间 | 300 ms |
| 特性 | / |

**参数说明**：

**`<port>` — 整数类型，ADC 通道号**：

| 值 | 说明 |
|----|------|
| `0` | ADC 通道 0 |
| `1` | ADC 通道 1 |

**`<status>` — 整数类型，ADC 值读取是否成功**：

| 值 | 说明 |
|----|------|
| `0` | 失败 |
| `1` | 成功 |

| 参数 | 类型 | 说明 |
|------|------|------|
| `<value>` | 整数类型 | 指定 ADC 通道的电压值，单位：mV |

---

## 8 附录参考

### 8.1 术语与缩写

**表 2：术语与缩写**

| 缩写 | 全称 |
|------|------|
| 3GPP | 3rd Generation Partnership Project（第三代合作伙伴项目） |
| ADC | Analog-to-Digital Converter（模数转换器） |
| APN | Access Point Name（接入点名称） |
| ASCII | American Standard Code for Information Interchange（美国信息交换标准码） |
| BAIC | Bar All Incoming Calls（禁止所有呼入） |
| BAOC | Bar All Outgoing Calls（禁止所有呼出） |
| BCD | Binary-Coded Decimal（二进制编码十进制） |
| BOIC | Bar Outgoing International Calls（禁止呼出国际电话） |
| BOIC-exHC | Bar Outgoing International Calls except to Home Country（禁止除本国外的国际呼出） |
| CS | Circuit Switching（电路交换） |
| DCE | Data Communication Equipment（数据通信设备） |
| DHCP | Dynamic Host Configuration Protocol（动态主机配置协议） |
| DTE | Data Terminal Equipment（数据终端设备） |
| DTMF | Dual-Tone Multifrequency（双音多频） |
| EGPRS | Enhanced General Packet Radio Service（增强通用分组无线服务） |
| EPS | Evolved Packet System（演进分组系统） |
| E-UTRAN | Evolved Universal Terrestrial Radio Access Network（演进通用地面无线接入网） |
| GGSN | Gateway GPRS Support Node（网关 GPRS 支持节点） |
| GMM | GPRS Mobility Management（GPRS 移动性管理） |
| GMT | Greenwich Mean Time（格林威治标准时间） |
| GPRS | General Packet Radio Service（通用分组无线服务） |
| GSM | Global System for Mobile Communications（全球移动通信系统） |
| HSDPA | High Speed Downlink Packet Access（高速下行分组接入） |
| HSUPA | High Speed Uplink Packet Access（高速上行分组接入） |
| IMEI | International Mobile Equipment Identity（国际移动设备标识） |
| IMSI | International Mobile Subscriber Identity（国际移动用户标识） |
| IP | Internet Protocol（互联网协议） |
| IPv4 | Internet Protocol version 4（互联网协议版本 4） |
| IPv6 | Internet Protocol version 6（互联网协议版本 6） |
| IRA | International Reference Alphabet（国际参考字母表） |
| ME | Mobile Equipment（移动设备） |
| MO | Mobile Originated（移动始发） |
| MS | Mobile Station（移动站） |
| MSC | Mobile Switching Center（移动交换中心） |
| MT | Mobile Terminal（移动终端） |
| NAS | Non-Access Stratum（非接入层） |
| NITZ | Network Identity and Time Zone / Network Informed Time Zone（网络标识和时区/网络通知时区） |
| P-CSCF | Proxy-Call Session Control Function（代理呼叫会话控制功能） |
| PDP | Packet Data Protocol（分组数据协议） |
| PDU | Packet Data Unit（分组数据单元） |
| PIN | Personal Identification Number（个人识别码） |
| PLMN | Public Land Mobile Network（公共陆地移动网络） |
| PPP | Point-to-Point Protocol（点对点协议） |
| PS | Packet Switching（分组交换） |
| PUK | PIN Unlock Key（PIN 解锁码） |
| RAT | Radio Access Technology（无线接入技术） |
| RF | Radio Frequency（射频） |
| RX | Receive（接收） |
| RxQual | Receive Quality（接收质量） |
| SM | Session Management（会话管理） |
| SMS | Short Message Service（短消息服务） |
| SMSC | Short Message Service Center（短消息服务中心） |
| SNDCP | SubNetwork Dependent Convergence Protocol（子网依赖汇聚协议） |
| TA | Terminal Adapter（终端适配器） |
| TE | Terminal Equipment（终端设备） |
| TFT | Traffic Flow Template（业务流模板） |
| TPDU | Transport Protocol Data Unit（传输协议数据单元） |
| UART | Universal Asynchronous Receiver/Transmitter（通用异步收发器） |
| USC-2 | Universal Character Set (UCS-2) Format（通用字符集 UCS-2 格式） |
| UE | User Equipment（用户设备） |
| UICC | Universal Integrated Circuit Card（通用集成电路卡） |
| UIM | User Identity Model（用户身份模型） |
| UMTS | Universal Mobile Telecommunications System（通用移动通信系统） |
| URC | Unsolicited Result Code（主动上报码） |
| USB | Universal Serial Bus（通用串行总线） |
| (U)SIM | (Universal) Subscriber Identity Module（（通用）用户身份模块） |
| VLR | Visiting Location Register（访问位置寄存器） |

---

### 8.2 移动终端错误结果码 +CME ERROR

最终结果码 `+CME ERROR: <err>` 表示与移动设备或网络相关的错误。`+CME ERROR: <err>` 最终结果码的操作方式类似于常规 `ERROR` 结果码：若 `+CME ERROR: <err>` 是命令行中任何指令的结果码，则命令行中的后续指令均不执行（既不返回 `ERROR` 也不返回 `OK`）。`<err>` 的格式可以是数字或详细描述。

**表 3：+CME ERROR 通用错误码汇总**

| 数值 `<err>` | 详细描述 `<err>` |
|------------|----------------|
| 0 | phone failure（手机故障） |
| 1 | no connection to phone（与手机无连接） |
| 2 | phone-adaptor link reserved（手机适配器链路保留） |
| 3 | operation not allowed（操作不允许） |
| 4 | operation not supported（操作不支持） |
| 5 | PH-SIM PIN required（需要 PH-SIM PIN） |
| 6 | PH-FSIM PIN required（需要 PH-FSIM PIN） |
| 7 | PH-FSIM PUK required（需要 PH-FSIM PUK） |
| 10 | SIM not inserted（SIM 卡未插入） |
| 11 | SIM PIN required（需要 SIM PIN） |
| 12 | SIM PUK required（需要 SIM PUK） |
| 13 | SIM failure（SIM 故障） |
| 14 | SIM busy（SIM 忙） |
| 15 | SIM wrong（SIM 卡错误） |
| 16 | incorrect password（密码错误） |
| 17 | SIM PIN2 required（需要 SIM PIN2） |
| 18 | SIM PUK2 required（需要 SIM PUK2） |
| 20 | memory full（内存已满） |
| 21 | invalid index（无效索引） |
| 22 | not found（未找到） |
| 23 | memory failure（内存故障） |
| 24 | text string too long（文本字符串过长） |
| 25 | invalid characters in text string（文本字符串中包含非法字符） |
| 26 | dial string too long（拨号字符串过长） |
| 27 | invalid characters in dial string（拨号字符串中包含非法字符） |
| 30 | no network service（无网络服务） |
| 31 | network timeout（网络超时） |
| 32 | network not allowed - emergency calls only（网络不允许，仅限紧急呼叫） |
| 40 | network personalization PIN required（需要网络个性化 PIN） |
| 41 | network personalization PUK required（需要网络个性化 PUK） |
| 42 | network subset personalization PIN required（需要网络子集个性化 PIN） |
| 43 | network subset personalization PUK required（需要网络子集个性化 PUK） |
| 44 | service provider personalization PIN required（需要服务提供商个性化 PIN） |
| 45 | service provider personalization PUK required（需要服务提供商个性化 PUK） |
| 46 | corporate personalization PIN required（需要企业个性化 PIN） |
| 47 | corporate personalization PUK required（需要企业个性化 PUK） |
| 901 | audio unknown error（音频未知错误） |
| 902 | audio invalid parameters（音频参数无效） |
| 903 | audio operation not supported（音频操作不支持） |
| 904 | audio device busy（音频设备忙） |

---

### 8.3 URC 汇总

**表 4：URC 汇总**

| 序号 | URC 显示 | 含义 | 触发条件 |
|------|---------|------|---------|
| 1 | `+CREG: <stat>` | MT 的注册状态 | `AT+CREG=1` |
| 2 | `+CREG: <stat>[,<lac>,<ci>[,<AcT>]]` | 小区邻居变化后，显示网络是否已指示 MT 注册，含位置区域码 | `AT+CREG=2` |
| 3 | `+CGREG: <stat>` | MT 的网络注册状态 | `AT+CGREG=1` |
| 4 | `+CGREG: <stat>[,<lac>,<ci>[,<AcT>],[<rac>]]` | MT 的网络注册及位置信息 | `AT+CGREG=2` |
| 5 | `+CEREG: <stat>` | MT 的 EPS 网络注册状态 | `AT+CEREG=1` |
| 6 | `+CEREG: <stat>[,<tac>,<ci>[,<AcT>]]` | MT 的 EPS 网络注册及位置信息 | `AT+CEREG=2` |
| 7 | `+CTZV: <tz>` | 时区上报 | `AT+CTZR=1` |
| 8 | `+CTZE: <tz>,<dst>,<time>` | 扩展时区上报 | `AT+CTZR=2` |
| 9 | `RING` | 来电 | N/A |
| 10 | `+CFUN: 1` | MT 的所有功能可用 | N/A |
| 11 | `+CPIN: <state>` | (U)SIM 卡 PIN 状态 | N/A |
| 12 | `POWERED DOWN` | 模块断电 | `AT+QPOWD` |
| 13 | `+CGEV: REJECT <PDP_type>,<PDP_addr>` | 网络请求 PDP 激活，被自动拒绝 | `AT+CGEREP=2,1` |
| 14 | `+CGEV: NW REACT <PDP_type>,<PDP_addr>,[<cid>]` | 网络请求 PDP 重激活 | `AT+CGEREP=2,1` |
| 15 | `+CGEV: NW DEACT <PDP_type>,<PDP_addr>,[<cid>]` | 网络强制停用上下文 | `AT+CGEREP=2,1` |

---

## 附录：AT 指令快速参考索引

| 指令 | 功能 | 所在章节 |
|------|------|---------|
| `ATI` | 显示 MT 标识信息 | 2.1 |
| `AT+GSN` | 请求 IMEI | 2.2 |
| `AT+CGSN` | 请求 IMEI（同 AT+GSN） | 2.3 |
| `ATE` | 设置指令回显模式 | 2.4 |
| `AT+CFUN` | 设置 MT 功能级别 | 2.5 |
| `AT+CIMI` | 请求 IMSI | 3.1 |
| `AT+CLCK` | 设施锁定/解锁/查询 | 3.2 |
| `AT+CPIN` | 输入/查询 PIN 码 | 3.3 |
| `AT+CPWD` | 修改密码 | 3.4 |
| `AT+CSIM` | 通用 (U)SIM 访问 | 3.5 |
| `AT+CRSM` | 受限 (U)SIM 访问 | 3.6 |
| `AT+COPS` | PLMN 选择 | 4.1 |
| `AT+CREG` | 网络注册状态（CS） | 4.2 |
| `AT+CSQ` | 信号质量 | 4.3 |
| `AT+CPOL` | 首选 PLMN 列表 | 4.4 |
| `AT+COPN` | 读取运营商名称 | 4.5 |
| `AT+CTZU` | 自动时区更新 | 4.6 |
| `AT+CTZR` | 时区上报 | 4.7 |
| `AT+CGATT` | PS 附着或分离 | 5.1 |
| `AT+CGDCONT` | 定义 PDP 上下文 | 5.2 |
| `AT+CGACT` | 激活/停用 PDP 上下文 | 5.3 |
| `AT+CGPADDR` | 显示 PDP 地址 | 5.4 |
| `AT+CGCLASS` | GPRS 移动站类别 | 5.5 |
| `AT+CGREG` | GPRS 网络注册状态 | 5.6 |
| `AT+CGEREP` | 分组域事件上报 | 5.7 |
| `AT+CGSMS` | 选择 MO SMS 消息服务 | 5.8 |
| `AT+CEREG` | EPS 网络注册状态 | 5.9 |
| `AT+CLVL` | 扬声器音量级别 | 6.1 |
| `AT+VTS` | DTMF 和音调生成 | 6.2 |
| `AT+VTD` | 音调持续时间 | 6.3 |
| `AT+QADC` | 读取 ADC 值 | 7.1 |

---

*文档来源：Quectel EC200A-CN(TA) QuecOpen AT Commands Manual V1.0.0 Preliminary 2022-08-26*
*分析整理：基于原始 PDF 全文（共 58 页）逐页提取，不遗漏任何内容*
