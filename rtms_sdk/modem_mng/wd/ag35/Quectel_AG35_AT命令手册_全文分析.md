# AG35-CET QuecOpen(SDK) AT 命令手册（中文）

> **文档元信息**
> - 原文档：`Quectel_AG35-CET_QuecOpen(SDK)_AT_Commands_Manual`（英文，本文为中文翻译转写）
> - 适用模块：LTE Standard 模块系列（AG35-CET）
> - 版本：1.0.0
> - 日期：2023-09-13
> - 状态：临时文件（Preliminary / Not Checked）
> - 作者：Lyndsay XIE
> - 总页数：91 页
>
> ⚠️ **平台提示**：本手册面向 **AG35-CET**。本项目 `open_dial` 运行在 **EC200A/EG25** 上，多数标准 3GPP/V.25ter 命令通用，但厂商私有命令（`AT+QCFG`、`AT+QNWINFO`、`AT+QADC` 等）及取值范围需对照实际平台核对。
>
> 📌 **通用约定**：返回码 `+CME ERROR: <err>` / `+CMS ERROR: <err>` 错误码见第 10.2 章；下划线标注的取值为**默认值**；`[...]` 为可选参数；`<...>` 为参数名。

---

## 文档历史

| 版本 | 日期 | 作者 | 描述 |
|---|---|---|---|
| - | 2023-09-13 | Lyndsay XIE | 文档创建 |
| 1.0.0 | 2023-09-13 | Lyndsay XIE | 临时版本 |

---

## 1 引言

Quectel AG35-CET 模块支持 QuecOpen® 方案；QuecOpen 是基于 Linux 系统的嵌入式开发平台，用于简化 IoV 应用的设计与开发。本文档适用于基于 SDK 构建环境的 QuecOpen 方案，介绍 AG35-CET 模块在 QuecOpen 方案下支持的 AT 命令集。

### 1.1 定义

- **\<CR>**：回车符（Carriage return）。
- **\<LF>**：换行符（Line feed）。
- **\<...>**：参数名。尖括号不出现在命令行中。
- **[...]**：命令的可选参数或 TA 信息响应的可选部分。方括号不出现在命令行中。命令中未给出可选参数时，新值等于其前值或默认值（除非另有说明）。
- **下划线**：参数的默认设置。

### 1.2 AT 命令语法

所有命令行必须以 `AT` 或 `at` 开头，以 `<CR>` 结尾。信息响应和结果码总是以回车符和换行符开始与结束：`<CR><LF><response><CR><LF>`。本文档表格中只展示命令和响应，`<CR>` 和 `<LF>` 被刻意省略。

模块支持的 AT 命令集是国际标准（*3GPP TS 27.007*、*3GPP TS 27.005*、*ITU-T V.25ter*）与 Quectel 自研 AT 命令的组合。AT 命令在语法上分三类："Basic"（基本）、"S Parameter"（S 参数）和 "Extended"（扩展）：

- **Basic（基本命令）**：格式为 `AT<x><n>` 或 `AT&<x><n>`，`<x>` 是命令，`<n>` 是参数。例如 `ATE<n>` 告诉 DCE（数据电路终接设备）是否将接收到的字符回显给 DTE（数据终端设备）。`<n>` 可选，省略时使用默认值。
- **S Parameter（S 参数命令）**：格式为 `ATS<n>=<m>`，`<n>` 是要设置的 S 寄存器索引，`<m>` 是要赋的值。
- **Extended（扩展命令）**：可有多种执行模式，见下表。

#### 表 1：AT 命令类型

| 命令类型 | 语法 | 描述 |
|---|---|---|
| 测试命令（Test） | `AT+<cmd>=?` | 测试相应命令是否存在，返回其参数的类型、取值或范围信息。 |
| 读命令（Read） | `AT+<cmd>?` | 查询相应命令的当前参数值。 |
| 写命令（Write） | `AT+<cmd>=<p1>[,<p2>[,<p3>[...]]]` | 设置用户可定义的参数值。 |
| 执行命令（Execution） | `AT+<cmd>` | 返回特定信息参数或执行特定动作。 |

- 单行可用分号 `;` 放置多条命令，此时只有第一条需 `AT` 前缀。命令大小写均可。
- 输入 AT 命令时空格应被忽略，但以下情况除外：引号字符串内（空格保留）；未加引号的字符串或数字参数内；IP 地址内；AT 命令名直到包含 `=`、`?` 或 `=?` 处。
- 输入时至少需一个回车符；换行符被忽略，因此可使用回车/换行对。
- `AT` 之后无命令则返回 `OK`；输入无效命令则返回 `ERROR`。可选参数除非明确说明，需提供到最后输入的参数。

### 1.3 支持的字符集

模块 AT 命令接口默认使用 GSM 字符集，并支持：**GSM**、**UCS-2**、**IRA**。这些字符集定义见 *3GPP TS 27.005*，影响短信和小区广播消息的收发以及电话簿条目文本字段的输入与显示。

### 1.4 AT 命令端口

两个 USB 端口（USB modem 端口与 USB AT 端口）支持 AT 命令通信和数据传输。

### 1.5 AT 命令示例声明

本文档中的 AT 命令示例用于帮助理解命令用法，**不应视为 Quectel 的推荐设计或建议**。一个命令可能提供多个示例，但这并不意味着示例之间存在关联或应按特定顺序执行。

---

## 2 通用命令（General Commands）

### 2.1 ATI — 显示 MT 标识信息

提供 MT 标识信息文本。

| 命令 | 响应 |
|---|---|
| 执行 `ATI` | `Quectel`<br>`<objectID>`<br>`Revision: <revision>`<br><br>`OK` |

- 最大响应时间：300 ms；参考：V.25ter。
- 参数：`<objectID>` 字符串，设备类型标识符；`<revision>` 字符串，MT 固件版本标识文本。

**示例**：
```
ATI
Quectel
AG35-CET
Revision: AG35CETCAR02A01M2G_OCPU

OK
```

### 2.2 AT+GSN — 请求国际移动设备识别码（IMEI）

请求 ME 的 IMEI 号，用于识别单个 ME 设备。与第 2.3 章 `AT+CGSN` 功能相同。

| 命令 | 响应 |
|---|---|
| 测试 `AT+GSN=?` | `OK` |
| 执行 `AT+GSN` | `<IMEI>`<br><br>`OK` |

- 最大响应时间：300 ms；参考：V.25ter。
- 参数：`<IMEI>` 字符串，ME 的 IMEI 号。
- **注**：IMEI 对每个 ME 唯一，可用于识别 ME。

### 2.3 AT+CGSN — 请求国际移动设备识别码（IMEI）

请求 ME 的 IMEI 号，与上面 `AT+GSN` 相同。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CGSN=?` | `OK` |
| 执行 `AT+CGSN` | `<IMEI>`<br><br>`OK` |

- 最大响应时间：300 ms；参考：3GPP TS 27.007。
- 参数：`<IMEI>` 字符串，ME 的 IMEI 号。
- **注**：IMEI 对每个 ME 唯一。

### 2.4 ATE — 设置命令回显模式

控制在 AT 命令模式下 TA 是否回显从 TE 接收的字符。

| 命令 | 响应 |
|---|---|
| 执行 `ATE<value>` | `OK` |

- 最大响应时间：300 ms；特性：命令立即生效，配置不保存；参考：V.25ter。
- 参数：`<value>` 整型，是否回显从 TE 接收的字符。`0` OFF；<u>`1` ON</u>。

### 2.5 AT+CFUN — 设置 MT 功能级别

选择 MT 的功能级别 `<fun>`，也可复位 MT。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CFUN=?` | `+CFUN: (支持的 <fun> 列表),(支持的 <rst> 列表)`<br>`OK` |
| 读 `AT+CFUN?` | `+CFUN: <fun>`<br><br>`OK` |
| 写 `AT+CFUN=<fun>[,<rst>]` | `OK`<br><br>若 MT 功能相关错误：`+CME ERROR: <err>` 或 `ERROR` |

- 最大响应时间：15 s，由网络决定；参考：3GPP TS 27.007。

**参数**：
- `<fun>` 整型：`0` 最小功能；<u>`1` 完整功能</u>；`3` 禁止 MT 接收 RF 信号；`4` 禁止 MT 收发 RF 信号；`5` 禁用 (U)SIM；`6` 第二路 RX 关闭。
- `<rst>` 整型：<u>`0` 设置 `<fun>` 功能级别前不复位 MT</u>；`1` 设置功能级别前先复位 MT，复位后完整功能（仅当 `<fun>=1` 时可用）。
- `<err>` MT 错误码，见第 10.2 章。

**示例**：
```
AT+CFUN=0           //切到最小功能
OK
AT+CPIN?
+CME ERROR: 13      //(U)SIM 失败
AT+CFUN=1           //切到完整功能
OK
AT+CPIN?
+CPIN: READY
OK
AT+COPS?
+COPS: 0,2,"46000",7   //运营商已注册
OK
```

---

## 3 (U)SIM 相关命令

### 3.1 AT+CIMI — 请求国际移动用户识别码（IMSI）

请求 IMSI，用于识别连接到 MT 的 UICC（GSM 或 SIM）中的 SIM 卡或活动应用。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CIMI=?` | `OK` |
| 执行 `AT+CIMI` | `<IMSI>`<br><br>`OK`<br><br>若错误：`+CME ERROR: <err>` 或 `ERROR` |

- 最大响应时间：300 ms；参考：3GPP TS 27.007。
- 参数：`<IMSI>` 不带双引号的字符串；`<err>` 错误码见 10.2。

**示例**：
```
AT+CIMI
460023210226023    //连接到 MT 的 (U)SIM 的 IMSI 号
OK
```

### 3.2 AT+CLCK — 设施锁（Facility Lock）

锁定、解锁或查询 MT 或网络设施 `<fac>`。设置/查询网络设施时可被中止。通常需要密码。PF/PN/PU/PP/PC 锁出厂默认密码为 `"12341234"`。查询网络服务状态（`<mode>=2`）时，仅当该服务对所有 `<class>` 都不活动时才返回"不活动"行（`<status>=0`）。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CLCK=?` | `+CLCK: (支持的 <fac> 列表)`<br>`OK` |
| 写 `AT+CLCK=<fac>,<mode>[,<passwd>[,<class>]]` | `<mode>≠2` 且设置成功：`OK`<br><br>`<mode>=2` 且成功：`+CLCK: <status>[,<class>]`<br>`[+CLCK: <status>[,<class>][...]]`<br>`OK` |

- 最大响应时间：5 s；特性：立即生效，自动保存；参考：3GPP TS 27.007。

**参数**：
- `<fac>` 字符串，设施锁类型：
  - `"SC"`：(U)SIM（锁当前选定卡槽的 SIM/UICC），开机及设此锁命令时需密码。
  - `"AO"` BAOC（禁止所有呼出，3GPP TS 22.088 clause 1）；`"OI"` BOIC（禁止国际呼出）；`"OX"` BOIC-exHC（禁止除归属国外的国际呼出）；`"AI"` BAIC（禁止所有呼入，clause 2）；`"IR"` BIC-Roam（漫游时禁止呼入，clause 2）。
  - `"AB"` 所有禁止服务（仅 `<mode>=0`）；`"AG"` 所有呼出禁止（仅 `<mode>=0`）；`"AC"` 所有呼入禁止（仅 `<mode>=0`）。
  - `"FD"`：SIM/UICC（GSM 或 USIM）固定拨号存储功能（若本次会话未做 PIN2 鉴权，需 PIN2 作为 `<passwd>`）。
  - `"PF"` 锁机到首张插入的 SIM/UICC（PH-FSIM）；`"PN"` 网络个性化；`"PU"` 网络子集个性化；`"PP"` 服务提供商个性化；`"PC"` 企业个性化（见 3GPP TS 22.022）。
- `<mode>` 整型：`0` 解锁；`1` 锁定；`2` 查询状态。
- `<passwd>` 字符串，密码。
- `<class>` 整型，信息类别之和：`1` 语音；`2` 数据；`4` 传真；<u>`7` 除短信外所有电话业务</u>；`8` 短信服务；`16` 数据电路同步；`32` 数据电路异步。
- `<status>` 整型，指定 `<class>` 当前状态：`0` 不活动；`1` 活动。

**示例**：
```
AT+CLCK="SC",2          //查询 (U)SIM 卡状态
+CLCK: 0                //不活动
OK
AT+CLCK="SC",1,"1234"   //锁定 (U)SIM 卡，密码 1234
OK
AT+CLCK="SC",2
+CLCK: 1                //活动
OK
AT+CLCK="SC",0,"1234"   //解锁 (U)SIM 卡，密码 1234
OK
```

### 3.3 AT+CPIN — 输入 PIN

向 MT 发送操作前必需的密码（(U)SIM PIN、(U)SIM PUK、PH-SIM PIN 等），或查询 MT 是否需要密码。

若 PIN 需输入两次，MT 自动重复 PIN。若无 PIN 请求挂起，则不对 MT 采取动作并返回 `+CME ERROR`。若所需 PIN 为 (U)SIM PUK 或 PUK2，需第二参数 `<newpin>` 替换 (U)SIM 中的旧 PIN。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CPIN=?` | `OK` |
| 读 `AT+CPIN?` | `+CPIN: <code>`<br><br>`OK`<br><br>若错误：`+CME ERROR: <err>` 或 `ERROR` |
| 写 `AT+CPIN=<pin>[,<newpin>]` | `OK` |

- 最大响应时间：5 s；特性：立即生效，自动保存；参考：3GPP TS 27.007。

**参数 `<code>`（不带双引号字符串）**：
- `READY` 不挂起任何密码；`SIM PIN` 等待 (U)SIM PIN；`SIM PUK` 等待 (U)SIM PUK；`SIM PIN2` 等待 PIN2；`SIM PUK2` 等待 PUK2；`PH-NET PIN`/`PH-NET PUK` 网络个性化密码/解锁密码；`PH-NETSUB PIN`/`PH-NETSUB PUK` 网络子集个性化；`PH-SP PIN`/`PH-SP PUK` 服务提供商个性化；`PH-CORP PIN`/`PH-CORP PUK` 企业个性化。
- `<pin>` 字符串，密码。若所请求密码是 PUK（如 (U)SIM PUK1、PH-FSIM PUK 等），则 `<pin>` 后须跟 `<newpin>`。
- `<newpin>` 字符串，所请求码为 PUK 时所需新密码。
- `<err>` 错误码见 10.2。

**示例**：
```
AT+CPIN?
+CPIN: SIM PIN          //等待 (U)SIM PIN
OK
AT+CPIN="1234"          //输入 PIN
OK
+CPIN: READY
AT+CPIN?
+CPIN: READY            //PIN 已输入
OK
// 输入 PUK 和 PIN：
AT+CPIN?
+CPIN: SIM PUK          //等待 (U)SIM PUK
OK
AT+CPIN="26601934","1234"   //输入 PUK 和新 PIN
OK
+CPIN: READY
```

### 3.4 AT+CPWD — 修改密码

写命令为 `AT+CLCK` 定义的设施锁功能设置新密码。测试命令返回各设施及其密码最大长度的配对列表。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CPWD=?` | `+CPWD: 支持的 (<fac>,<pwdlength>) 列表`<br>`OK` |
| 写 `AT+CPWD=<fac>,<oldpwd>,<newpwd>` | `OK` |

- 最大响应时间：5 s；特性：立即生效，自动保存；参考：3GPP TS 27.007。

**参数**：
- `<fac>` 字符串，设施锁类型：`"SC"` (U)SIM；`"AO"/"OI"/"OX"/"AI"/"IR"` 呼叫禁止类（同 `AT+CLCK`）；`"AB"/"AG"/"AC"`（仅 `<mode>=0`）；`"P2"` (U)SIM PIN2。
- `<pwdlength>` 整型，密码最大长度。
- `<oldpwd>` 字符串，从用户界面或命令设的设施密码；`<newpwd>` 字符串，新密码。

**示例**：
```
AT+CPIN?
+CPIN: READY
OK
AT+CPWD="SC","1234","4321"  //(U)SIM 卡密码从 "1234" 改为 "4321"
OK
// 重启 MT 或重新激活 (U)SIM 卡
AT+CPIN?
+CPIN: SIM PIN
OK
AT+CPIN="4321"              //须输入新密码 "4321"
OK
+CPIN: READY
```

### 3.5 AT+CSIM — 通用 (U)SIM 访问

允许 TE 上的远程应用直接控制当前卡槽中的 (U)SIM。TE 应在 GSM/UMTS 指定的框架内处理 (U)SIM 信息。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CSIM=?` | `OK` |
| 写 `AT+CSIM=<length>,<command>` | `+CSIM: <length>,<response>`<br>`OK` 或 `ERROR`<br>若错误：`+CME ERROR: <err>` |

- 最大响应时间：300 ms；特性：立即生效，自动保存；参考：3GPP TS 27.007。

**参数**：
- `<length>` 发送给 TE 的 `<command>` 或 `<response>` 中字符的长度（为实际命令/响应长度的两倍）。
- `<command>` MT 传给 (U)SIM 的命令，格式见 *3GPP TS 51.011*（十六进制字符格式）。
- `<response>` (U)SIM 传给 MT 的响应，格式同上。
- `<err>` 错误码见 10.2。

### 3.6 AT+CRSM — 受限 (U)SIM 访问

提供对 (U)SIM 数据库更简单但更受限的访问。向 MT 传输 (U)SIM `<command>` 及其所需参数。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CRSM=?` | `OK` |
| 写 `AT+CRSM=<command>[,<fileid>[,<P1>,<P2>,<P3>[,<data>[,<pathid>]]]]` | `+CRSM: <sw1>,<sw2>[,<response>]`<br>`OK` 或 `ERROR`<br>若错误：`+CME ERROR: <err>` |

- 最大响应时间：300 ms；特性：立即生效，自动保存；参考：3GPP TS 27.007。

**参数**：
- `<command>` 整型，MT 传给 (U)SIM 的命令：`176` READ BINARY；`178` READ RECORD；`192` GET RESPONSE；`214` UPDATE BINARY；`220` UPDATE RECORD；`242` STATUS。
- `<fileid>` 整型，(U)SIM 上基本数据文件的标识符。除 STATUS 外每条命令必填。
- `<P1>,<P2>,<P3>` 整型，MT 传给 (U)SIM 的参数。除 GET RESPONSE 与 STATUS 外每条命令必填，取值见 *3GPP TS 51.011*。
- `<data>` 应写入 (U)SIM 的信息（十六进制字符格式）。
- `<pathid>` SIM/UICC 上基本文件的目录路径，十六进制格式，定义见 *ETSI TS 102 221*。
- `<sw1>,<sw2>` 整型，(U)SIM 返回的命令执行信息，成功或失败两种情况下均返回。
- `<response>` 上一命令成功完成的响应（十六进制）。STATUS 和 GET RESPONSE 返回当前基本数据字段信息（含文件类型与大小，见 3GPP TS 51.011）；READ BINARY/RECORD 后返回所请求数据；UPDATE BINARY/RECORD 成功后不返回 `<response>`。
- `<err>` 错误码见 10.2。

---

## 4 网络服务命令

### 4.1 AT+COPS — PLMN 选择

返回当前运营商及其状态，允许自动或手动选网。

- **测试命令**返回五参数集，每组代表网络中出现的一个运营商：整型 `<stat>`（可用性）、长格式与短格式字母运营商名、数字格式运营商名、接入技术。任意格式可能不可用则为空。运营商列表顺序：归属网络、(U)SIM 引用网络、其他网络。
- **读命令**返回当前模式与当前选定运营商。若无选定运营商，`<format>`、`<oper>`、`<AcT>` 被省略。
- **写命令**强制尝试选择并注册到 GSM/UMTS 网络运营商。若选定运营商不可用，不选其他运营商（除非 `<mode>=4`）。所选运营商名格式应应用于后续读命令。

| 命令 | 响应 |
|---|---|
| 测试 `AT+COPS=?` | `+COPS: [(<stat>,长名 <oper>,短名 <oper>,数字 <oper>[,<AcT>])s][,,(支持的 <mode>s),(支持的 <format>s)]`<br>`OK`<br>或 `+CME ERROR: <err>` / `ERROR` |
| 读 `AT+COPS?` | `+COPS: <mode>[,<format>[,<oper>][,<AcT>]]`<br>`OK`<br>或 `+CME ERROR: <err>` / `ERROR` |
| 写 `AT+COPS=<mode>[,<format>[,<oper>[,<AcT>]]]` | `OK`<br>或 `+CME ERROR: <err>` / `ERROR` |

- 最大响应时间：180 s，由网络决定；参考：3GPP TS 27.007。

**参数**：
- `<stat>` 整型：`0` 未知；`1` 运营商可用；`2` 当前运营商；`3` 运营商被禁。
- `<oper>` 字符串，按 `<format>` 表示的运营商。
- `<mode>` 整型：<u>`0` 自动（忽略 `<oper>`）</u>；`1` 手动（须有 `<oper>`，`<AcT>` 可选）；`2` 从网络注销；`3` 仅设置 `<format>`（用于读命令，不尝试注册/注销，忽略 `<oper>` 和 `<AcT>`，读命令响应中不适用）；`4` 手动/自动（须有 `<oper>`，手动失败则进入自动 `<mode>=0`）。
- `<format>` 整型：<u>`0` 长格式字母名（最多 16 字符）</u>；`1` 短格式字母名；`2` 数字（GSM 位置区识别码）。
- `<AcT>` 整型，接入技术：`0` GSM；`2` UTRAN；`3` GSM w/EGPRS；`4` UTRAN w/HSDPA；`5` UTRAN w/HSUPA；`6` UTRAN w/HSDPA+HSUPA；`7` E-UTRAN。值 3/4/5/6 仅在 MS 处于数据服务态时出现于读命令响应，不用于写命令。
- `<err>` 错误码见 10.2。

**示例**：
```
AT+COPS=?    //列出所有当前网络运营商
+COPS: (1,"CHN-UNICOM","UNICOM","46001",2),(1,"CHN-UNICOM","UNICOM","46001",0),(2,"CHN-UNICOM","UNICOM","46001",7),(1,"CHN-CT","CT","46011",7),(3,"CHINA MOBILE","CMCC","46000",0),,(0-4),(0-2)
OK
AT+COPS?     //查询当前选定网络运营商
+COPS: 0,0,"46000",7
OK
```

### 4.2 AT+CREG — 网络注册状态

写命令控制 URC 呈现：`<n>=1` 时在 GERAN/UTRAN/E-UTRAN 电路域注册状态变化时上报 `+CREG: <stat>`；`<n>=2` 时网络小区变化时上报 `+CREG: <stat>[,[<lac>],[<ci>],[<AcT>]]`。

读命令返回结果码呈现状态及 `<stat>`（注册状态）。位置信息 `<lac>`、`<ci>`、`<AcT>` 仅当 `<n>=2` 且 MT 已注册时返回。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CREG=?` | `+CREG: (支持的 <n>s)`<br>`OK` |
| 读 `AT+CREG?` | `+CREG: <n>,<stat>[,<lac>,<ci>[,<AcT>]]`<br>`OK`<br>或 `+CME ERROR: <err>` / `ERROR` |
| 写 `AT+CREG=[<n>]` | `OK` |

- 最大响应时间：300 ms；特性：立即生效，配置不保存；参考：3GPP TS 27.007。

**参数**：
- `<n>` 整型，是否使能网络注册 URC：<u>`0` 禁用</u>；`1` 使能 `+CREG: <stat>`；`2` 使能带位置信息的 `+CREG: <stat>[,[<lac>],[<ci>],[<AcT>]]`。
- `<stat>` 整型，电路域注册状态：`0` 未注册，未在搜网；`1` 已注册，归属网络；`2` 未注册，正在搜网；`3` 注册被拒；`4` 未知（如脱离覆盖）；`5` 已注册，漫游。
- `<lac>` 字符串，两字节位置区码（十六进制）。
- `<ci>` 字符串，四字节小区 ID（十六进制）。
- `<AcT>` 整型，服务小区接入技术：`0` GSM；`2` UTRAN；`3` GSM w/EGPRS；`4` UTRAN w/HSDPA；`5` UTRAN w/HSUPA；`6` UTRAN w/HSDPA+HSUPA；`7` E-UTRAN。
- `<err>` 错误码见 10.2。

**示例**：
```
AT+CREG=2           //激活扩展 URC 模式
OK
AT+CREG?
+CREG: 2,1,"550c","5a29c0b",7   //URC 上报 MT 已注册到 E-UTRAN 网络
OK
```

### 4.3 AT+CSQ — 信号质量

执行命令返回 MT 的接收信号强度指示 `<rssi>` 和信道误码率 `<ber>`。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CSQ=?` | `+CSQ: (支持的 <rssi>s),(支持的 <ber>s)`<br>`OK` |
| 执行 `AT+CSQ` | `+CSQ: <rssi>,<ber>`<br>`OK`<br>或 `+CME ERROR: <err>` / `ERROR` |

- 最大响应时间：300 ms；参考：3GPP TS 27.007。

**参数**：
- `<rssi>` 整型，接收信号强度指示：`0` ≤-113 dBm；`1` -111 dBm；`2~30` -109 dBm 至 -53 dBm；`31` ≥-51 dBm；`99` 未知或不可测。
- `<ber>` 整型，信道误码率（百分比）：`0~7` 对应 *3GPP TS 45.008 subclause 8.2.4* 表中的 RxQual 值；`99` 未知或不可测。
- `<err>` 错误码见 10.2。

**示例**：
```
AT+CSQ=?
+CSQ: (0-31,99),(0-7,99)
OK
AT+CSQ
+CSQ: 28,99    //接收信号强度指示为 28，信道误码率为 99
OK
```

### 4.4 AT+CPOL — 首选 PLMN 列表

编辑 SIM 卡或 UICC（GSM 或 USIM）中带接入技术的 PLMN 选择器列表。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CPOL=?` | `+CPOL: (支持的 <index>s),(支持的 <format>s)`<br>`OK` |
| 读 `AT+CPOL?` | `+CPOL: <index1>,<format>,<oper1>[,<GSM_AcT1>,<GSM_Compact_AcT1>,<UTRAN_AcT1>,<E-UTRAN_AcT1>]`<br>`[+CPOL: <index2>,<format>,<oper2>[,...]]`<br>`OK` |
| 写 `AT+CPOL=<index>[,<format>[,<oper>[,<GSM_AcT>,<GSM_Compact_AcT>,<UTRAN_AcT>,<E-UTRAN_AcT>]]]` | 省略可选参数时删除指定 `<index>` 的运营商：`OK`<br>指定任意可选参数时编辑首选运营商列表：`OK` 或 `ERROR`<br>若错误：`+CME ERROR: <err>` |

- 最大响应时间：300 ms；参考：3GPP TS 27.007。

**参数**：
- `<indexn>` 整型，运营商在 (U)SIM 首选列表中的序号。
- `<format>` 整型：`0` 长格式字母名；`1` 短格式字母名；`2` 数字。
- `<opern>` 字符串，运营商；`<format>` 指明字母或数字格式（见 AT+COPS）。
- `<GSM_AcTn>` / `<GSM_Compact_AcTn>` / `<UTRAN_AcTn>` / `<E-UTRAN_AcTn>` 整型，各接入技术是否选中：`0` 未选；`1` 已选。
- `<err>` 错误码见 10.2。
- **注**：对于含带接入技术 PLMN 选择器的 SIM 卡/UICC，接入技术选择参数为必填。

### 4.5 AT+COPN — 读取运营商名称

返回 MT 中的运营商名称列表。每个在 MT 内存中有字母等价名 `<alphan>` 的运营商代码 `<numericn>` 被返回。

| 命令 | 响应 |
|---|---|
| 测试 `AT+COPN=?` | `OK` |
| 执行 `AT+COPN` | `+COPN: <numeric1>,<alpha1>`<br>`[+COPN: <numeric2>,<alpha2>[...]]`<br>`OK`<br>若错误：`+CME ERROR: <err>` |

- 最大响应时间：取决于运营商名称数量；参考：3GPP TS 27.007。
- 参数：`<numericn>` 数字格式运营商名（见 AT+COPS）；`<alphan>` 长字母格式运营商名；`<err>` 错误码见 10.2。

### 4.6 AT+CTZU — 自动时区更新

通过 NITZ 使能/禁用自动时区更新。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CTZU=?` | `+CTZU: (支持的 <onoff>s)`<br>`OK` |
| 读 `AT+CTZU?` | `+CTZU: <onoff>`<br>`OK` |
| 写 `AT+CTZU=<onoff>` | `OK` 或 `ERROR` |

- 最大响应时间：300 ms；参考：3GPP TS 27.007。
- 参数：`<onoff>` 整型，自动时区更新模式：`0` 禁用经 NITZ 自动更新；<u>`1` 使能经 NITZ 自动更新并更新 GMT 时间到 URC</u>。

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

### 4.7 AT+CTZR — 时区上报

控制时区变化事件上报。若使能，时区变化时 MT 返回 URC `+CTZV: <tz>` 或 `+CTZE: <tz>,<dst>,<time>`。配置自动存入 flash。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CTZR=?` | `+CTZR: (支持的 <reporting>s)`<br>`OK` |
| 读 `AT+CTZR?` | `+CTZR: <reporting>`<br>`OK` |
| 写 `AT+CTZR=<reporting>` | `OK` 或 `ERROR` |

- 最大响应时间：300 ms；特性：立即生效，自动保存；参考：3GPP TS 27.007。

**参数**：
- `<reporting>` 整型，时区上报模式：<u>`0` 禁用</u>；`1` 由 URC `+CTZV: <tz>` 使能时区变化事件上报；`2` 由 URC `+CTZE: <tz>,<dst>,<time>` 使能扩展时区与本地时间上报。
- `<tz>` 字符串，本地时区（本地时间与 GMT 之差，以 1/4 小时为单位）加夏令时之和，固定宽度两位整数，范围 -48 到 +56；-9 到 +9 范围以前导零表示，如 `-09`、`+00`、`+09`。
- `<dst>` 整型，`<tz>` 是否含夏令时调整：`0` 不含；`1` 含 +1 小时（4 个 1/4 小时）；`2` 含 +2 小时（8 个 1/4 小时）。
- `<time>` 字符串，本地时间，格式 `"YYYY/MM/DD,hh:mm:ss"`（年月日时分秒，整数表示）。该参数可由网络在发送时区信息时提供，并在扩展时区上报 URC 中呈现（若网络提供）。

### 4.8 AT+QNWINFO — 查询网络信息

查询所选接入技术、运营商、所选频段等网络信息。

| 命令 | 响应 |
|---|---|
| 测试 `AT+QNWINFO=?` | `OK` |
| 执行 `AT+QNWINFO` | `+QNWINFO: <AcT>,<oper>,<band>,<channel>`<br>`[+QNWINFO: <AcT>,<oper>,<band>,<channel>]`<br>`OK` |

- 最大响应时间：300 ms。

**参数**：
- `<AcT>` 字符串，所选接入技术：`"NONE"`、`"CDMA1X"`、`"CDMA1X AND HDR"`、`"CDMA1X AND EHRPD"`、`"HDR"`、`"HDR-EHRPD"`、`"GSM"`、`"GPRS"`、`"EDGE"`、`"WCDMA"`、`"HSDPA"`、`"HSUPA"`、`"HSPA+"`、`"TDSCDMA"`、`"TDD LTE"`、`"FDD LTE"`。
- `<oper>` 字符串，数字格式运营商。
- `<band>` 字符串，所选频段：`"GSM 450"`、`"GSM 480"`、`"GSM 750"`、`"GSM 850"`、`"GSM 900"`、`"GSM 1800"`、`"GSM 1900"`、`"WCDMA 2100"`、`"WCDMA 1900"`、`"WCDMA 1800"`、`"WCDMA 1700 US"`、`"WCDMA 850"`、`"WCDMA 800"`、`"WCDMA 2600"`、`"WCDMA 900"`、`"WCDMA 1700 JAPAN"`、`"WCDMA 1500"`、`"WCDMA 850 JAPAN"`、`"LTE BAND 1"~"LTE BAND 43"`、`"TDSCDMA BAND A~F"`。
- `<channel>` 整型，信道 ID。

**示例**：
```
AT+QNWINFO=?
OK
AT+QNWINFO
+QNWINFO: "FDD LTE","46001","LTE BAND 3",1650
OK
AT+QNWINFO
+QNWINFO: "CDMA1X","46003",283
+QNWINFO: "FDD LTE","46011","LTE BAND 1",75
OK
```

---

## 5 呼叫控制命令

### 5.1 ATA — 接听来电

将模块连接到由 `RING` URC 指示的来话语音或数据呼叫。

| 命令 | 响应 |
|---|---|
| 执行 `ATA` | TA 向远端摘机。<br>1) 数据呼叫建立成功：`CONNECT [<text>]`，TA 切到数据模式（`<text>` 仅当 ATX 的 `<value>`>0 时输出）；呼叫释放后回命令模式：`OK`<br>2) 语音呼叫建立成功：`OK`<br>3) 无法建立或被 TE 额外字符中止：`NO CARRIER` |

- 最大响应时间：90 s，由网络决定；参考：V.25ter。
- 参数：`<text>` 整型，数据速率，见 AT+CBST 的 `<speed>`。
- **注**：ATA 同一命令行后的任何附加命令被忽略；命令执行期间收到字符可中止，但握手等某些连接建立状态下可能无法中止。

**示例**：
```
RING                              //来电
AT+CLCC
+CLCC: 1,1,0,1,0,"",128           //LTE 模式下的 PS 呼叫
+CLCC: 2,1,4,0,0,"02154450290",129  //来电
OK
ATA                               //用 ATA 接听语音呼叫
OK
```

### 5.2 ATD — 发起呼叫

建立呼出语音、数据或传真呼叫，也可控制补充业务。

| 命令 | 响应 |
|---|---|
| 执行 `ATD<n>[<mgsm>][;]` | 无拨号音：`NO DIALTONE`；忙：`BUSY`；无法建立：`NO CARRIER`；语音连接成功：`OK` |

- 最大响应时间：5 s，由网络决定（AT+COLP=0）；参考：V.25ter。

**参数**：
- `<n>` 拨号数字串及可选 V.25ter 修饰符。拨号数字：`0-9`、`*`、`#`、`+`、`A`、`B`、`C`。以下 V.25ter 修饰符被忽略：`,`（逗号）、`T`、`P`、`!`、`W`、`@`。
- `<mgsm>` GSM 修饰符串：`I` 激活 CLIR（禁止向被叫呈现本机号码）；`i` 取消 CLIR（允许向被叫呈现本机号码）；`G` 仅本次呼叫激活闭合用户群调用；`g` 仅本次呼叫取消闭合用户群调用。
- `<;>` 建立语音呼叫时必需，呼叫后返回命令状态。
- **注**：收到 ATH 或字符时命令可被中止，但握手等状态下可能无法中止。活动语音呼叫期间用 ATD 发起第二个语音呼叫时，第一个呼叫自动置保持。任何时候可用 `AT+CLCC` 检查所有呼叫状态。

**示例**：
```
ATD10086;    //拨打对方号码
OK
```

### 5.3 ATH — 断开现有连接

断开电路交换数据呼叫或语音呼叫。`AT+CHUP` 也可用于断开语音呼叫。

| 命令 | 响应 |
|---|---|
| 执行 `ATH[n]` | `OK` |

- 最大响应时间：90 s，由网络决定；参考：V.25ter。
- 参数：`<n>` 整型：<u>`0` 从命令行断开现有呼叫并终止</u>。

### 5.4 AT+CVHU — 语音挂断控制

控制 ATH 是否可用于断开语音呼叫。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CVHU=?` | `+CVHU: (支持的 <mode>s)`<br>`OK` |
| 读 `AT+CVHU?` | `+CVHU: <mode>`<br>`OK` |
| 写 `AT+CVHU=<mode>` | `OK`<br>若错误：`ERROR` |

- 最大响应时间：300 ms；参考：3GPP TS 27.007。
- 参数：`<mode>` 整型：<u>`0` ATH 可用于断开语音呼叫</u>；`1` ATH 被忽略但返回 `OK`。

### 5.5 AT+CHUP — 挂断呼叫

取消处于 Active、Waiting、Held 状态的所有语音呼叫。数据呼叫断开请用 ATH。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CHUP=?` | `OK` |
| 执行 `AT+CHUP` | `OK`<br>若错误：`ERROR` |

- 最大响应时间：90 s，由网络决定；参考：3GPP 27.007。

**示例**：
```
RING        //来电
AT+CHUP     //挂断呼叫
OK
```

### 5.6 ATO — 从命令模式切换到数据模式

恢复连接并从命令模式返回数据模式。

| 命令 | 响应 |
|---|---|
| 执行 `ATO[n]` | 未成功恢复：`NO CARRIER`；成功恢复，TA 从命令模式回数据模式：`CONNECT [<text>]`；若错误：`ERROR` |

- 最大响应时间：300 ms；参考：V.25ter。
- 参数：`<n>` 整型：`0` 从命令模式切换到数据模式；`<text>` 整型，数据速率，见 AT+CBST 的 `<speed>`。

### 5.7 ATS0 — 设置自动应答前的振铃次数

控制来电的自动应答模式。

| 命令 | 响应 |
|---|---|
| 读 `ATS0?` | `<n>`<br>`OK` |
| 写 `ATS0=<n>` | `OK` 或 `ERROR` |

- 最大响应时间：300 ms；特性：立即生效；参考：V.25ter。
- 参数：`<n>` 整型：<u>`0` 禁用自动应答</u>；`1–255` 在指定振铃数后使能自动应答。
- **注**：若 `<n>` 过大，主叫方可能在自动应答前挂断。

**示例**：
```
ATS0=3      //设置 3 次振铃后自动应答
OK
RING        //来电
RING
RING        //三次振铃后自动应答
```

### 5.8 ATS7 — 连接完成超时

指定模块在应答/发起呼叫与建立连接之间允许的时间（秒）。若该时间内未建立连接，模块断开。

| 命令 | 响应 |
|---|---|
| 读 `ATS7?` | `<n>`<br>`OK` |
| 写 `ATS7=<n>` | `OK` 或 `ERROR` |

- 最大响应时间：300 ms；特性：立即生效；参考：V.25ter。
- 参数：`<n>` 整型：<u>`0` 禁用</u>；`1–255` 必须建立连接的秒数，否则呼叫断开。

### 5.9 AT+CBST — 选择承载业务类型

选择发起数据呼叫时使用的承载业务 `<name>`、数据速率 `<speed>` 和连接元素 `<ce>`。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CBST=?` | `+CBST: (支持的 <speed>s),(支持的 <name>s),(支持的 <ce>s)`<br>`OK` |
| 读 `AT+CBST?` | `+CBST: <speed>,<name>,<ce>`<br>`OK` |
| 写 `AT+CBST=[<speed>[,<name>[,<ce>]]]` | `OK` 或 `ERROR` |

- 最大响应时间：300 ms；特性：立即生效，配置不保存；参考：3GPP TS 27.007。

**参数**：
- `<speed>` 整型，数据速率（部分取值）：`0` 自动波特；`1` 300bps(V.21)；`2` 1200bps(V.22)；`3` 1200/75bps(V.23)；`4` 2400bps(V.22bis)；`5` 2400bps(V.26ter)；`6` 4800bps(V.32)；`7` 9600bps(V.32)；`12` 9600bps(V.34)；`14` 14400bps(V.34)；`15` 19200bps(V.34)；`16` 28800bps(V.34)；`17` 33600bps(V.34)；`34` 1200bps(V.120)；`36` 2400bps(V.120)；`38` 4800bps(V.120)；`39` 9600bps(V.120)；`43` 14400bps(V.120)；`47` 19200bps(V.120)；`48` 28800bps(V.120)；`49` 38400bps(V.120)；`50` 48000bps(V.120)；`51` 56000bps(V.120)；`65` 300bps(V.110)；`66` 1200bps(V.110)；`68` 2400bps(V.110/X.31)；`70` 4800bps；`71` 9600bps；`75` 14400bps；`79` 19200bps；`80` 28800bps；`81` 38400bps；`82` 48000bps；`83` 56000bps(V.110/X.31)；`84` 64000bps(X.31 flag stuffing)；`115` 56000bps(bit transparent)；`116` 64000bps(bit transparent)；`120` 32000bps(PIAFS32k)；`121` 64000bps(PIAFS64k)；`130` 28800bps(multimedia)；`131` 32000bps；`132` 33600bps；`133` 56000bps；`134` 64000bps(multimedia)。
- `<name>` 整型，承载业务：`0` 数据电路异步(UDI/3.1kHz modem)；`1` 数据电路同步(UDI/3.1kHz modem)；`2` PAD 接入异步(UDI)；`3` 包接入同步(UDI)；`4` 数据电路异步(RDI)；`5` 数据电路同步(RDI)；`6` PAD 接入异步(RDI)；`7` 包接入同步(RDI)。
- `<ce>` 整型，连接元素：`0` 透明；`1` 非透明；`2` 两者，透明优先；`3` 两者，非透明优先。

### 5.10 AT+CSTA — 选择地址类型

按 *3GPP TS 24.008* 选择后续 ATD 命令的号码类型。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CSTA=?` | `+CSTA: (支持的 <type>s)`<br>`OK` |
| 读 `AT+CSTA?` | `+CSTA: <type>`<br>`OK` |
| 写 `AT+CSTA=[<type>]` | `OK` |

- 最大响应时间：300 ms；特性：立即生效，配置不保存；参考：3GPP TS 27.007。
- 参数：`<type>` 整型，地址字节类型：<u>`129` 未知类型</u>；`145` 国际类型（含 `+` 字符）。

### 5.11 AT+CLCC — 列出当前呼叫

返回所有当前呼叫列表。命令成功但无可用呼叫时，不返回信息响应，仅 `OK`。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CLCC=?` | `OK` |
| 执行 `AT+CLCC` | `[+CLCC: <ccid1>,<dir>,<stat>,<mode>,<mpty>[,<number>,<type>[,<alpha>]]]`<br>`[+CLCC: <ccid2>,...]`<br>`[...]`<br>`OK`<br>若错误：`+CME ERROR: <err>` |

- 最大响应时间：300 ms；参考：（V.25ter / 3GPP）。

**参数**：
- `<ccidx>` 整型，呼叫标识号（见 *3GPP TS 22.030 subclause 4.5.5.1*）。
- `<dir>` 整型：`0` 主叫(MO)；`1` 被叫(MT)。
- `<stat>` 整型，呼叫状态：`0` Active；`1` Held；`2` Dialing(MO)；`3` Alerting(MO)；`4` Incoming(MT)；`5` Waiting(MT)。
- `<mode>` 整型，承载/电信业务：`0` 语音；`1` 数据；`2` 传真。
- `<mpty>` 整型：`0` 非多方（会议）呼叫成员；`1` 多方呼叫成员。
- `<number>` 字符串，号码（格式由 `<type>` 指定）。
- `<type>` 整型，地址字节类型（见 *3GPP TS 24.008 10.5.4.7*）：`129` 未知类型；`145` 国际类型（含 `+`）；`161` 国内类型。
- `<alpha>` 字符串，`<number>` 在电话簿中对应条目的字母表示。
- `<err>` 错误码见 10.2。

**示例**：
```
ATD10086;    //建立呼叫
OK
AT+CLCC
+CLCC: 1,1,0,1,0,"",128            //LTE 模式下的 PS 呼叫
+CLCC: 2,0,0,0,0,"10086",129       //MO 呼叫已建立并处于活动，已被应答
OK
```

### 5.12 AT+CRC — 设置来电指示扩展格式

控制是否使用来电指示扩展格式。使能时，来电用 URC `+CRING: <type>` 指示，替代普通的 `RING`。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CRC=?` | `+CRC: (支持的 <mode>s)`<br>`OK` |
| 读 `AT+CRC?` | `+CRC: <mode>`<br>`OK` |
| 写 `AT+CRC=[<mode>]` | `OK` |

- 最大响应时间：300 ms；特性：立即生效，配置不保存；参考：3GPP TS 27.007。

**参数**：
- `<mode>` 整型：<u>`0` 禁用扩展格式</u>；`1` 使能扩展格式。
- `<type>` 字符串：`ASYNC` 异步透明；`SYNC` 同步透明；`REL ASYNC` 异步非透明；`REL SYNC` 同步非透明；`FAX` 传真；`VOICE` 语音；`VOICE/ASYNC`、`VOICE/SYNC`、`VOICE/REL ASYNC`、`VOICE/REL SYNC`、`ALT VOICE/ASYNC`、`ALT VOICE/SYNC`、`ALT ASYNC/VOICE`、`ALT SYNC/VOICE ALT`、`ALT REL ASYNC/VOICE`、`ALT REL SYNC/VOICE`、`ALT VOICE/FAX`、`ALT FAX/VOICE` 等组合类型。

**示例**：
```
AT+CRC=1            //使能扩展格式
OK
+CRING: VOICE       //向 TE 指示语音类来电
ATH
OK
AT+CRC=0            //禁用扩展格式
OK
RING                //向 TE 指示来电
ATH
OK
```

### 5.13 AT+CRLP — 选择无线链路协议参数

选择发起非透明数据呼叫时使用的无线链路协议（RLP）参数。RLP 版本 0 和 1 共享同一参数集，读/测试命令对该集仅返回一行（其中 `<ver>` 可能不呈现）。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CRLP=?` | 多行 `+CRLP: (支持的 <iws>s),(支持的 <mws>s),(支持的 <T1>s),(支持的 <N2>s),<ver>`<br>`OK` |
| 读 `AT+CRLP?` | 多行 `+CRLP: <iws>,<mws>,<T1>,<N2>[,<ver>[,<T4>]]`<br>`OK` |
| 写 `AT+CRLP=[<iws>[,<mws>[,<T1>[,<N2>[,<ver>][,<T4>]]]]]` | `OK` |

- 最大响应时间：300 ms；特性：立即生效，配置不保存；参考：3GPP TS27.007。

**参数**：
- `<iws>` 整型，IWF 到 MS 窗口大小，范围 0–<u>61</u>。
- `<mws>` 整型，MS 到 IWF 窗口大小，范围 0–<u>61</u>。
- `<T1>` 整型，确认定时器 T1（10ms 单位），范围 39–255。
- `<N2>` 整型，重传次数，范围 1–255。
- `<ver>` 整型，RLP 版本号，范围 0–1。
- `<T4>` 整型，重排序周期，范围 3–255。

---

## 6 短信服务命令（SMS）

### 6.1 AT+CSMS — 选择消息服务

选择消息服务 `<service>`，返回 ME 支持的消息类型。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CSMS=?` | `+CSMS: (支持的 <service>s)`<br>`OK` |
| 读 `AT+CSMS?` | `+CSMS: <service>,<mt>,<mo>,<bm>`<br>`OK` |
| 写 `AT+CSMS=<service>` | `+CSMS: <mt>,<mo>,<bm>`<br>`OK`<br>若错误：`+CMS ERROR: <err>` |

- 最大响应时间：300 ms；参考：3GPP TS 27.005。

**参数**：
- `<service>` 整型，消息服务类型：<u>`0` 兼容 *3GPP TS 27.005 Phase 2 version 4.7.0*（不要求新命令语法的 Phase 2+ 特性可支持，如新 Phase 2+ 数据编码方案消息的正确路由）</u>；`1` 兼容 *3GPP TS 27.005 Phase 2+*（对应命令描述中说明 `<service>=1` 的要求）。
- `<mt>` 整型，被叫消息：`0` 不支持；<u>`1` 支持</u>。
- `<mo>` 整型，主叫消息：`0` 不支持；<u>`1` 支持</u>。
- `<bm>` 整型，广播消息：`0` 不支持；<u>`1` 支持</u>。
- `<err>` 错误码见 10.2。

**示例**：
```
AT+CSMS=?
+CSMS: (0,1)
OK
AT+CSMS=1       //设置消息服务类型为 1
+CSMS: 1,1,1
OK
AT+CSMS?
+CSMS: 1,1,1,1
OK
```

### 6.2 AT+CMGF — 消息格式

指定短信的输入输出格式。`<mode>` 指明用于发送、列表、读取、写入命令以及收到消息产生的 URC 的消息格式。可为 PDU 模式（使用整个 TP 数据单元）或 Text 模式（消息头和正文作为独立参数）。Text 模式用 AT+CSCS 指定的 `<chset>` 在 TA-TE 接口告知消息正文字符集。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CMGF=?` | `+CMGF: (支持的 <mode>s)`<br>`OK` |
| 读 `AT+CMGF?` | `+CMGF: <mode>`<br>`OK` |
| 写 `AT+CMGF[=<mode>]` | `OK` |

- 最大响应时间：300 ms；参考：3GPP TS 27.005。
- 参数：`<mode>` 整型：<u>`0` PDU 模式</u>；`1` Text 模式。

### 6.3 AT+CSCA — 短信中心地址

写命令在发送主叫短信时更新 SMSC 地址。Text 模式下，该设置用于写命令；PDU 模式下，仅当 `<pdu>` 中编码的 SMSC 地址长度为零时使用该设置。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CSCA=?` | `OK` |
| 读 `AT+CSCA?` | `+CSCA: <sca>,<tosca>`<br>`OK` |
| 写 `AT+CSCA=<sca>[,<tosca>]` | `OK` 或 `ERROR`<br>若错误：`+CMS ERROR: <err>` |

- 最大响应时间：300 ms；参考：3GPP TS 27.005。

**参数**：
- `<sca>` 短信中心地址。*3GPP TS 24.011* RP SC 地址 Address-Value 字段，字符串格式。BCD 数字（或 GSM 7 位默认字符）转换为当前选定 TE 字符集字符（见 AT+CSCS）；地址类型由 `<tosca>` 给出。
- `<tosca>` 短信中心地址类型。*3GPP TS 24.011* RP SC 地址 Type-of-Address 字节，整型（默认见 `<toda>`）。
- `<err>` 错误码见 10.2。

**示例**：
```
AT+CSCA="+8613800210500",145    //设置 SMSC 地址
OK
AT+CSCA?                         //查询 SMSC 地址
+CSCA: "+8613800210500",145
OK
```

### 6.4 AT+CPMS — 首选消息存储

选择用于读取、写入等的存储位置 `<mem1>`、`<mem2>`、`<mem3>`。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CPMS=?` | `+CPMS: (支持的 <mem1>s),(支持的 <mem2>s),(支持的 <mem3>s)`<br>`OK` |
| 读 `AT+CPMS?` | `+CPMS: <mem1>,<used1>,<total1>,<mem2>,<used2>,<total2>,<mem3>,<used3>,<total3>`<br>`OK` |
| 写 `AT+CPMS=<mem1>[,<mem2>[,<mem3>]]` | `+CPMS: <used1>,<total1>,<used2>,<total2>,<used3>,<total3>`<br>`OK`<br>若错误：`+CMS ERROR: <err>` |

- 最大响应时间：300 ms；参考：3GPP TS 27.005。

**参数**：
- `<mem1>` 字符串，从此存储读取和删除消息：`"SM"` (U)SIM 消息存储；`"SR"` 状态报告存储；<u>`"ME"` 移动设备消息存储</u>。
- `<mem2>` 字符串，消息写入和发送到此存储：`"SM"`；`"SR"`；<u>`"ME"`</u>。
- `<mem3>` 字符串，未设置路由到 PC（AT+CNMI）时，接收消息存于此存储：`"SM"`；`"SR"`；<u>`"ME"`</u>。
- `<usedx>` 整型，`<memx>` 中当前消息数。
- `<totalx>` 整型，`<memx>` 可存的消息总数。
- `<err>` 错误码见 10.2。

**示例**：
```
AT+CPMS?                          //查询当前短信存储
+CPMS: "ME",0,200,"ME",0,200,"ME",0,200
OK
AT+CPMS="SM","SM","SM"            //设置短信存储为 "SM"
+CPMS: 0,50,0,50,0,50
OK
AT+CPMS?
+CPMS: "SM",0,50,"SM",0,50,"SM",0,50
OK
```

### 6.5 AT+CMGD — 删除消息

从首选存储 `<mem1>`（见 AT+CPMS）位置 `<index>` 删除短信。若 `<delflag>` 存在且非 0，ME 忽略 `<index>` 并按以下 `<delflag>` 规则操作。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CMGD=?` | `+CMGD: (支持的 <index>s),(支持的 <delflag>s)`<br>`OK` |
| 写 `AT+CMGD=<index>[,<delflag>]` | `OK`<br>若错误：`+CMS ERROR:<err>` |

- 最大响应时间：300 ms（`<delflag>` 操作取决于被删消息存储）；参考：3GPP TS 27.005。

**参数**：
- `<index>` 整型，关联存储支持的位置号范围内的值。
- `<delflag>` 整型，删除标志：<u>`0` 删除 `<index>` 指定的消息</u>；`1` 删除 `<mem1>` 中所有已读消息，保留未读及已存主叫消息（无论是否发送）；`2` 删除所有已读及已发送主叫消息，保留未读及未发送主叫消息；`3` 删除所有已读、已发送、未发送主叫消息，保留未读消息；`4` 删除 `<mem1>` 中所有消息（含未读）。
- `<err>` 错误码见 10.2。

**示例**：
```
AT+CMGD=1       //删除 <index>=1 的消息
OK
AT+CMGD=1,4     //删除 <mem1> 中所有消息
OK
```

### 6.6 AT+CMGL — 列出消息

从首选存储 `<mem1>` 返回状态值为 `<stat>` 的消息。若消息状态为 "REC UNREAD"，则存储中状态变为 "REC READ"。不带状态值执行 `AT+CMGL` 时，上报状态为 "REC UNREAD" 的短信列表。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CMGL=?` | `+CMGL: (支持的 <stat>s)`<br>`OK` |
| 写 `AT+CMGL[=<stat>]` | Text 模式成功（不同消息类型）见下；PDU 模式：`+CMGL: <index>,<stat>,[<alpha>],<length><CR><LF><pdu>`...<br>`OK`<br>若错误：`+CMS ERROR: <err>` |
| 执行 `AT+CMGL` | 列出 `<mem1>` 中所有 "REC UNREAD" 消息，并将状态改为 "REC READ"。 |

Text 模式响应格式：
- SMS-SUBMIT/SMS-DELIVER：`+CMGL: <index>,<stat>,<oa/da>,[<alpha>],[<scts>][,<tooa/toda>,<length>]<CR><LF><data>`
- SMS-STATUS-REPORT：`+CMGL: <index>,<stat>,<fo>,<mr>,[<ra>],[<tora>],<scts>,<dt>,<st>`
- SMS-COMMAND：`+CMGL: <index>,<stat>,<fo>,<ct>`
- CBM 存储：`+CMGL: <index>,<stat>,<sn>,<mid>,<page>,<pages><CR><LF><data>`

- 最大响应时间：300 ms（`<stat>` 操作取决于存储）；参考：3GPP TS 27.005。

**参数 `<stat>`（PDU 模式整型 / Text 模式字符串）**：

| PDU | Text | 说明 |
|---|---|---|
| <u>0</u> | <u>"REC UNREAD"</u> | 收到的未读消息 |
| 1 | "REC READ" | 收到的已读消息 |
| 2 | "STO UNSENT" | 存储的未发送消息 |
| 3 | "STO SENT" | 存储的已发送消息 |
| 4 | "ALL" | 所有消息 |

**其他参数**（含 6.7 共用）：
- `<index>` 整型，存储支持的位置号范围内的值。
- `<da>` 字符串，目的地址（*3GPP TS 23.040* TP-Destination-Address）。BCD 数字（或 GSM 7 位默认字符）转换为当前选定 TE 字符集字符；地址类型由 `<toda>` 给出。
- `<oa>` 字符串，源地址（TP-Originating-Address），地址类型由 `<tooa>` 给出。
- `<alpha>` 字符串，`<da>` 或 `<oa>` 在 MT 电话簿中对应条目的字母表示；使用的字符集应为 AT+CSCS 选定的。
- `<scts>` 字符串，短信中心时间戳（TP-Service-Centre-Time-Stamp），时间字符串格式（见 AT+CMGR 的 `<dt>`）。
- `<toda>` 整型，目的地址类型（TP-Destination-Address Type-of-Address 字节）。
- `<tooa>` 整型，源地址类型（默认见 `<toda>`）。
- `<length>` 整型，消息长度：Text 模式为消息正文 `<data>` 字符数；PDU 模式为实际 TP 数据单元字节数（不含 RP 层 SMSC 地址字节）。
- `<data>` SMS 情况：*3GPP TS 23.040* TP-User-Data，Text 模式响应格式取决于 `<dcs>` 与 `<fo>`（GSM 7 位/8 位/UCS2 等编码转换规则见 *3GPP TS 27.005 Annex A*；"HEX" 字符集时每 7 位字符转两位 IRA 十六进制等）。CBS 情况：*3GPP TS 23.041* CBM 消息内容。
- `<pdu>` SMS 情况：*3GPP TS 24.011* SC 地址加 *3GPP TS 23.040* TPDU 的十六进制格式（每字节转两位 IRA 十六进制）。
- `<fo>` 整型，首字节（取决于命令或结果码：SMS-DELIVER、SMS-SUBMIT 默认 17、SMS-STATUS-REPORT、SMS-COMMAND 默认 2）。
- `<mr>` 整型，消息引用（TP-Message-Reference）。
- `<ra>` 字符串，接收方地址（TP-Recipient-Address），类型由 `<tora>` 给出。
- `<tora>` 整型，接收方地址类型（默认见 `<toda>`）。
- `<dt>` 字符串，释放时间（TP-Discharge-Time），格式 `"yy/MM/dd,hh:mm:ss zz"`（年取后两位，含时区），如 "94/05/06,22:10:00+08" 表示 1994/5/6 22:10:00 GMT+2 小时。
- `<st>` 整型，TP-Status；`<ct>` 整型，TP-Command-Type（默认 0）。
- `<sn>` 整型，CBM 序列号；`<mid>` 整型，CBM 消息标识符；`<page>` 整型，CBM 页参数 bits 4–7；`<pages>` 整型，CBM 页参数 bits 0–3。
- `<pid>` 整型，协议标识符（TP-Protocol-Identifier，默认 0）；`<dcs>` 整型，数据编码方案（SMS Data Coding Scheme 默认 0，或 CBM 编码方案）；`<vp>` 有效期（取决于 SMS-SUBMIT `<fo>`，整型或时间字符串）。
- `<prt>` 整型，优先级：`0` Normal；`1` Interactive；`2` Urgent；`3` Emergency。
- `<fmt>` 整型，格式：`0` GSM 7 bit；`1` ASCII；`6` Unicode。
- `<prv>` 整型，隐私：`0` Normal；`1` Restricted；`2` Confidential；`3` Secret。
- `<lang>` 整型，语言：`0` 未指定；`1` 英语；`2` 法语；`3` 西班牙语；`4` 日语；`5` 韩语；`6` 中文；`7` 希伯来语。
- `<type>` 整型：`0` Normal；`1` CPT；`2` Voice Mail；`3` SMS Report。
- `<cdata>` TP-Command-Data 字段，最大 157 字节。
- `<sca>`/`<tosca>` SC 地址及类型；`<err>` 错误码见 10.2。

**示例**：
```
AT+CMGF=1                  //设置短信格式为 Text 模式
OK
AT+CMGL="ALL"              //列出存储中所有消息
+CMGL: 1,"STO UNSENT","",,
<This is a test from Quectel>
+CMGL: 2,"STO UNSENT","",,
<This is a test from Quectel>
OK
```

### 6.7 AT+CMGR — 读取消息

从存储 `<mem1>` 返回位置 `<index>` 的短信。若消息状态为 "REC UNREAD"，状态变为 "REC READ"。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CMGR=?` | `OK` |
| 写 `AT+CMGR=<index>` | 见下（按消息类型） |

Text 模式响应：
- SMS-DELIVER：`+CMGR: <stat>,<oa>,[<alpha>],<scts>[,<tooa>,<fo>,<pid>,<dcs>,<sca>,<tosca>,<length>]<CR><LF><data>`
- SMS-SUBMIT：`+CMGR: <stat>,<da>,[<alpha>][,<toda>,<fo>,<pid>,<dcs>,[<vp>],<sca>,<tosca>,<length>]<CR><LF><data>`
- SMS-STATUS-REPORT：`+CMGR: <stat>,<fo>,<mr>,[<ra>],[<tora>],<scts>,<dt>,<st>`
- SMS-COMMAND：`+CMGR: <stat>,<fo>,<ct>[,<pid>,[<mn>],[<da>],[<toda>],<length><CR><LF><cdata>]`
- CBM 存储：`+CMGR: <stat>,<sn>,<mid>,<dcs>,<page>,<pages><CR><LF><data>`

PDU 模式：`+CMGR: <stat>,[<alpha>],<length><CR><LF><pdu>`，后跟 `OK`；若错误：`+CMS ERROR: <err>`。

- 最大响应时间：取决于消息内容长度；参考：3GPP TS 27.005。
- 参数同 6.6（另含 `<mn>` 整型，消息号 TP-Message-Number）。

**示例**：
```
+CMTI: "SM",3              //收到新消息并存到 "SM" 的 <index>=3
AT+CSDH=1
OK
AT+CMGR=3                  //读取消息
+CMGR: "REC UNREAD","+8615021012496",,"17/08/30,15:06:37+32",145,4,0,0,"+8613800210500",145,27
<This is a test from Quectel>
OK
```

### 6.8 AT+CMGS — 发送消息

从 TE 向网络发送短信（SMS-SUBMIT）。执行写命令后，等待提示符 `>`，然后开始写消息，之后按 Ctrl+Z 发送。按 Esc 可取消（确认 `OK`，但不发送）。发送成功后向 TE 返回消息引用 `<mr>`，可用于在后续 URC 投递状态报告中识别消息。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CMGS=?` | `OK` |
| 写（Text 模式 AT+CMGF=1）`AT+CMGS=<da>[,<toda>]`<br>写（PDU 模式 AT+CMGF=0）`AT+CMGS=<length>` | `>`，输入文本后按 Ctrl+Z 发送 / Esc 取消。<br>Text 模式成功：`+CMGS: <mr>` `OK`<br>PDU 模式成功：`+CMGS: <mr>` `OK`<br>若错误：`+CMS ERROR: <err>` |

- 最大响应时间：120 s，由网络决定；参考：3GPP TS 27.005。

**参数**：`<da>` 目的地址；`<toda>` 目的地址类型；`<length>` 消息长度（Text 模式为 AT+CMGR 中 `<data>`/`<cdata>` 字符数，PDU 模式为 TP 数据单元字节数）；`<mr>` 消息引用；`<err>` 错误码见 10.2。

**示例**：
```
AT+CMGF=1                       //设置短信格式为 Text 模式
OK
AT+CMGS="15021012496"
><This is a test from Quectel>  //输入文本后按 Ctrl+Z 发送
+CMGS: 247
OK
```

### 6.9 AT+CMMS — 发送更多消息

控制 SMS 中继协议链路的连续性。当启用（且网络支持）时，多条消息可更快发送，因为链路保持打开。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CMMS=?` | `+CMMS: (支持的 <n>s)`<br>`OK` |
| 读 `AT+CMMS?` | `+CMMS: <n>`<br>`OK` |
| 写 `AT+CMMS[=<n>]` | `OK` 或 `ERROR`<br>若错误：`+CMS ERROR: <err>` |

- 最大响应时间：120 s，由网络决定；特性：立即生效，配置不保存；参考：3GPP TS 27.005。

**参数 `<n>` 整型**：
- <u>`0` 功能禁用</u>。
- `1` 保持启用，直到最近一条发送命令（AT+CMGS、AT+CMSS 等）响应与下一条发送命令之间间隔超过 1–5 秒（具体由 ME 实现），然后 ME 关闭链路，TA 自动将 `<n>` 切回 0。
- `2` 功能启用。若间隔超过 1–5 秒，ME 关闭链路，但 TA 不自动将 `<n>` 切回 0。
- `<err>` 错误码见 10.2。

### 6.10 AT+CMGW — 写入消息到存储

存储短信（SMS-DELIVER 或 SMS-SUBMIT）到存储 `<mem2>`（见 AT+CPMS），返回存储位置 `<index>`。默认状态 "STO UNSENT"，但 `<stat>` 也允许其他状态。文本输入方式同 AT+CMGS。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CMGW=?` | `OK` |
| 写（Text 模式 AT+CMGF=1）`AT+CMGW=<oa/da>[,<tooa/toda>[,<stat>]]<CR>`<br>写（PDU 模式 AT+CMGF=0）`AT+CMGW=<length>[,<stat>]<CR>` | `>`，输入文本后按 Ctrl+Z 写入 / Esc 取消。成功：`+CMGW: <index>` `OK`<br>若错误：`+CMS ERROR: <err>` |

- 最大响应时间：300 ms；参考：3GPP TS 27.005。
- 参数：`<da>`/`<oa>`/`<tooa>`/`<toda>`/`<stat>`/`<length>`/`<pdu>`/`<index>` 同前；`<err>` 见 10.2。

**示例**：
```
AT+CMGF=1                       //Text 模式
OK
AT+CSCS="GSM"                   //字符集 "GSM"
OK
AT+CMGW="15021012496"
><This is a test from Quectel>  //Ctrl+Z 写入消息
+CMGW: 4
OK
AT+CMGF=0                       //PDU 模式
OK
AT+CMGW=18
>0051FF00000008000A0500030002016D4B8BD5
+CMGW: 5
OK
```

### 6.11 AT+CMSS — 从存储发送消息

从存储 `<mem2>`（见 AT+CPMS）发送位置 `<index>` 的消息到网络（SMS-SUBMIT）。若为 SMS-SUBMIT 给出新目的地址 `<da>`，则用它替代随消息存储的地址。发送成功返回引用 `<mr>`。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CMSS=?` | `OK` |
| 写 `AT+CMSS=<index>[,<da>[,<toda>]]` | Text 模式成功：`+CMSS: <mr>[,<scts>]` `OK`<br>PDU 模式成功：`+CMSS: <mr>[,<ackpdu>]` `OK`<br>若错误：`+CMS ERROR: <err>` |

- 最大响应时间：120 s，由网络决定；参考：3GPP TS 27.005。
- 参数：`<index>`/`<da>`/`<toda>`/`<mr>`/`<scts>` 同前；`<ackpdu>` *3GPP TS 23.040* RP-User-Data 元素（格式同 `<pdu>` 但不含 *3GPP TS 24.011* SC 地址字段，且不加双引号）；`<err>` 见 10.2。

**示例**：
```
AT+CMGF=1
OK
AT+CSCS="GSM"
OK
AT+CMGW="15021012496"
>Hello              //Ctrl+Z 写入
+CMGW: 4
OK
AT+CMSS=4           //发送存储中索引 4 的消息
+CMSS: 54
OK
```

### 6.12 AT+CNMI — 新消息指示到 TE

写命令选择当 TE 活动时（如 UART1_DTR 信号为低电平 ON）如何向 TE 指示网络新消息。若 TE 不活动（UART1_DTR 高电平 OFF），消息接收应按 *3GPP TS 23.038* 进行。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CNMI=?` | `+CNMI: (支持的 <mode>s),(支持的 <mt>s),(支持的 <bm>s),(支持的 <ds>s),(支持的 <bfr>s)`<br>`OK` |
| 读 `AT+CNMI?` | `+CNMI: <mode>,<mt>,<bm>,<ds>,<bfr>`<br>`OK` |
| 写 `AT+CNMI=[<mode>[,<mt>[,<bm>[,<ds>[,<bfr>]]]]]` | `OK` 或 `ERROR`<br>若错误：`+CMS ERROR: <err>` |

- 最大响应时间：300 ms；参考：3GPP TS 27.005。

**参数**：
- `<mode>` 整型：`0` 在 TA 中缓存 URC，缓冲区满时可丢弃最旧；`1` TA-TE 链路被占用（如数据模式）时丢弃并拒绝新收消息 URC，否则直接转发；<u>`2` 链路被占用时缓存 URC，占用解除后刷新到 TE，否则直接转发</u>。
- `<mt>` 整型，收到 SM 的存储规则（取决于编码方案、AT+CPMS 设置及此值）：`0` 不路由 SMS-DELIVER 指示到 TE；<u>`1` SMS-DELIVER 存到 ME/TA 时，存储位置指示路由到 TE：`+CMTI: <mem>,<index>`</u>；`2` SMS-DELIVER（class 2 除外）直接路由到 TE：`+CMT: [<alpha>],<length><CR><LF><pdu>`（PDU 模式）或 `+CMT: <oa>,[<alpha>],<scts>[,<tooa>,<fo>,<pid>,<dcs>,<sca>,<tosca>,<length>]<CR><LF><data>`（Text 模式）；class 2 消息按 `<mt>=1`；`3` class 3 SMS-DELIVER 直接路由到 TE（按 `<mt>=2` URC），其他编码方案按 `<mt>=1`。
- `<bm>` 整型，收到 CBM 存储规则：<u>`0` 不路由 CBM 指示到 TE</u>；`2` 新 CBM 直接路由到 TE：`+CBM: <length><CR><LF><pdu>`（PDU）或 `+CBM: <sn>,<mid>,<dcs>,<page>,<pages><CR><LF><data>`（Text）。
- `<ds>` 整型：<u>`0` 不路由 SMS-STATUS-REPORT 到 TE</u>；`1` 路由到 TE：`+CDS: <length><CR><LF><pdu>`（PDU）或 `+CDS: <fo>,<mr>,[<ra>],[<tora>],<scts>,<dt>,<st>`（Text）；`2` SMS-STATUS-REPORT 存到 ME/TA 时，存储位置指示路由到 TE：`+CDSI:<mem>,<index>`。
- `<bfr>` 整型：<u>`0` `<mode>=1/2` 时本命令定义的 URC 缓冲区刷新到 TE（刷新前先给 OK）</u>；`1` `<mode>=1/2` 时清空缓冲区。
- `<err>` 错误码见 10.2。

**URC（注）**：`+CMTI: <mem>,<index>` 收到新消息；`+CMT: [<alpha>],<length><CR><LF><pdu>` 短信直接输出；`+CBM: <length><CR><LF><pdu>` 小区广播消息直接输出。

**示例**：
```
AT+CMGF=1
OK
AT+CSCS="GSM"
OK
AT+CNMI=1,2,0,1,0           //设置 SMS-DELIVER 直接路由到 TE
OK
+CMT: "+8615021012496","17/08/30,17:07:21+32",145,4,0,0,"+8613800551500",145,28
This is a test from Quectel.   //短信进来时直接输出
```

### 6.13 AT+CSCB — 选择小区广播消息类型

选择 ME 接收哪些类型的 CBM。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CSCB=?` | `+CSCB: (支持的 <mode>s)`<br>`OK` |
| 读 `AT+CSCB?` | `+CSCB: <mode>,<mids>,<dcss>`<br>`OK` |
| 写 `AT+CSCB=<mode>[,mids>[,<dcss>]]` | `OK`<br>若错误：`+CMS ERROR: <err>` |

- 最大响应时间：300 ms；参考：3GPP TS 27.005。

**参数**：
- `<mode>` 整型：<u>`0` 接受 `<mids>` 和 `<dcss>` 指定的消息类型</u>；`1` 不接受 `<mids>` 和 `<dcss>` 指定的消息类型。
- `<mids>` 字符串，CBM 消息标识符的所有可能组合（默认空串），如 `"0,1,5,320–478,922"`。
- `<dcss>` 字符串，CBM 数据编码方案的所有可能组合（默认空串），如 `"0–3,5"`。
- `<err>` 错误码见 10.2。

### 6.14 AT+CSDH — 显示 Text 模式参数

控制 Text 模式结果码中是否显示详细头信息。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CSDH=?` | `+CSDH: (支持的 <show>s)`<br>`OK` |
| 读 `AT+CSDH?` | `+CSDH: <show>`<br>`OK` |
| 写 `AT+CSDH=[<show>]` | `OK` 或 `ERROR` |

- 最大响应时间：300 ms；特性：立即生效，配置不保存；参考：3GPP TS 27.005。

**参数 `<show>` 整型**：
- <u>`0` 不在 +CMT、+CMGL、+CMGR 结果码中显示 AT+CSCA 与 AT+CSMP 定义的头值（`<sca>`、`<tosca>`、`<fo>`、`<vp>`、`<pid>`、`<dcs>`）及 SMS-DELIVER/SMS-SUBMIT 的 `<length>`、`<toda>`、`<tooa>`</u>。
- `1` 在结果码中显示这些值。

**示例**：
```
AT+CSDH=0
OK
AT+CMGR=2
+CMGR: "STO UNSENT","",,
<This is a test from Quectel>
OK
AT+CSDH=1
OK
AT+CMGR=2
+CMGR: "STO UNSENT","",,128,17,0,0,143,"+8613800551500",145,18
<This is a test from Quectel>
OK
```

### 6.15 AT+CSMP — 设置 Text 模式参数（非 CDMA 网络）

选择短信发送到网络或存储时所需的额外参数值。可设置从短信被 SMSC 接收起的有效期（`<vp>` 范围 0 到 255），或定义有效期终止的绝对时间（`<vp>` 为字符串）。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CSMP=?` | `OK` |
| 读 `AT+CSMP?` | `+CSMP: <fo>,<vp>,<pid>,<dcs>`<br>`OK` |
| 写 `AT+CSMP=[<fo>[,<vp>[,<pid>[,<dcs>]]]]` | `OK` 或 `ERROR` |

- 最大响应时间：300 ms；特性：立即生效，配置不保存；参考：3GPP TS 27.005。

**参数**：
- `<fo>` 整型，首字节（取决于命令或结果码：SMS-DELIVER、SMS-SUBMIT 默认 17、SMS-STATUS-REPORT、SMS-COMMAND，整型格式。若已输入有效值一次，可省略该参数）。
- `<vp>` 有效期（取决于 SMS-SUBMIT `<fo>`：TP-Validity-Period，整型或时间字符串，见 AT+CMGR 的 `<dt>`）。
- `<pid>` 整型，协议标识符（TP-Protocol-Identifier，默认 0）。
- `<dcs>` 整型，数据编码方案（SMS Data Coding Scheme 默认 0，或 CBM 编码方案）。

---

## 7 分组域命令（Packet Domain）

### 7.1 AT+CGATT — PS 附着或分离

将 MT 附着到或从分组域服务分离。命令完成后 MT 保持 *V.250* 命令状态。若 MT 已处于所请求状态，命令被忽略并返回 `OK`。若无法达成所请求状态，返回 `ERROR` 或 `+CME ERROR`。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CGATT=?` | `+CGATT: (支持的 <state>s)`<br>`OK` |
| 读 `AT+CGATT?` | `+CGATT: <state>`<br>`OK` |
| 写 `AT+CGATT=<state>` | `OK` 或 `ERROR`<br>若错误：`+CME ERROR: <err>` |

- 最大响应时间：140 s，由网络决定；参考：3GPP TS 27.007。
- 参数：`<state>` 整型，PS 附着状态：`0` 分离；`1` 附着；其他值保留，将导致写命令返回 `ERROR`。`<err>` 见 10.2。

**示例**：
```
AT+CGATT=1      //附着到 PS 服务
OK
AT+CGATT=0      //从 PS 服务分离
OK
AT+CGATT?       //查询当前 PS 服务状态
+CGATT: 0
OK
```

### 7.2 AT+CGDCONT — 定义 PDP 上下文

为特定上下文 `<cid>` 指定 PDP 上下文参数。写命令特殊形式 `AT+CGDCONT=<cid>` 使 `<cid>` 的值变为未定义。不允许更改已激活上下文的定义。读命令返回每个已定义 PDP 上下文的当前设置。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CGDCONT=?` | `+CGDCONT: (支持的 <cid>s),<PDP_type>,<APN>,<PDP_addr>,(支持的 <d_comp>s),(支持的 <h_comp>s),(支持的 <IPv4AddrAlloc>s),(支持的 <request_type>s),(支持的 <P-CSCF_discovery>s),(支持的 <IM_CN_Signalling_Flag_Ind>s)`<br>`OK` |
| 读 `AT+CGDCONT?` | `+CGDCONT: <cid>,<PDP_type>,<APN>,<PDP_addr>,<d_comp>,<h_comp>,<IPv4AddrAlloc>,<request_type>,<P-CSCF_discovery>,<IM_CN_Signalling_Flag_Ind>`<br>`[...]`<br>`OK` |
| 写 `AT+CGDCONT=<cid>[,<PDP_type>[,<APN>[,<PDP_addr>[,<d_comp>[,<h_comp>[,<IPv4AddrAlloc>[,<request_type>[,<P-CSCF_discovery>[,<IM_CN_Signalling_Flag_Ind>]]]]]]]]]` | `OK` 或 `ERROR` |

- 最大响应时间：300 ms；特性：立即生效，自动保存；参考：3GPP TS 27.007。

**参数**：
- `<cid>` 整型，PDP 上下文标识符，本地用于 TE-MT 接口及其他 PDP 相关命令。允许范围（最小值=1）由测试命令返回，范围 1–15。
- `<PDP_type>` 字符串，包数据协议类型。EPS 服务仅支持 `"IP"`、`"IPv6"`、`"IPV4V6"`：`"IP"` IPv4（*IETF STD 5*）；`"PPP"` 点对点协议（*IETF STD 51*）；`"IPV6"` IPv6（*RFC 2460*）；`"IPV4V6"` 处理双 IP 栈 UE 能力的虚拟类型（*3GPP TS 24.301*）。
- `<APN>` 字符串，接入点名，用于选择 GGSN 或外部包数据网。若为空或省略，则请求签约值。
- `<PDP_addr>` 字符串，在 PDP 适用地址空间中标识 MT。若为空或省略，则地址可在 PDP 启动过程中由 TE 提供，或失败时请求动态地址。分配地址可用 AT+CGPADDR 读取。
- `<d_comp>` 整型，控制 PDP 数据压缩（仅 SNDCP 适用，*3GPP TS 44.065*）：<u>`0` Off</u>；`1` On（厂商首选压缩）；`2` V.42bis；`3` V.44（当前不支持）。
- `<h_comp>` 整型，控制 PDP 头压缩（*3GPP TS 44.065/25.323*）：<u>`0` Off</u>；`1` On；`2` RFC1144（仅 SNDCP 适用）；`3` RFC2507；`4` RFC3095（仅 PDCP 适用）。
- `<IPv4AddrAlloc>` 整型，控制 MT/TA 如何请求 IPv4 地址信息：<u>`0` 通过 NAS 信令分配</u>；`1` 通过 DHCP 分配。
- `<request_type>` 整型，PDP 上下文激活请求类型（见 *3GPP TS 24.301/24.008*；紧急承载服务须建立单独 PDP 上下文）。
- `<P-CSCF_discovery>` 整型，影响 MT/TA 如何请求 P-CSCF 地址（见 *3GPP TS 24.229 annex B/L*）：<u>`0` 不受 AT+CGDCONT 影响</u>；`1` 通过 NAS 信令获取；`2` 通过 DHCP 获取。
- `<IM_CN_Signalling_Flag_Ind>` 整型，向网络指示 PDP 上下文是否仅用于 IM CN 子系统相关信令：<u>`0` 指示不是仅用于 IM CN 子系统信令</u>；`1` 指示是仅用于 IM CN 子系统信令。

### 7.3 AT+CGACT — 激活或去激活 PDP 上下文

激活或去激活指定 PDP 上下文。命令完成后 MT 保持 V.250 命令状态。若某上下文已处于请求状态，该上下文状态保持不变。若执行激活形式时 MT 未 PS 附着，MT 先做 PS 附着再尝试激活。若未指定 `<cid>`，则激活/去激活所有已定义 PDP 上下文。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CGACT=?` | `+CGACT: (支持的 <state>s)`<br>`OK` |
| 读 `AT+CGACT?` | `+CGACT: <cid>,<state>`<br>`[...]`<br>`OK` |
| 写 `AT+CGACT=<state>,<cid>` | `OK` 或 `NO CARRIER`<br>若错误：`+CME ERROR: <err>` 或 `ERROR` |

- 最大响应时间：150 s，由网络决定；参考：3GPP TS 27.007。
- 参数：`<state>` 整型，PDP 上下文激活状态：`0` 去激活；`1` 激活；其他值保留，将导致写命令返回 `ERROR`。`<cid>` 整型，指定特定 PDP 上下文定义（见 AT+CGDCONT）。`<err>` 见 10.2。

**示例**：
```
AT+CGDCONT=2,"IP","UNINET"    //定义 PDP 上下文
OK
AT+CGACT=1,2                  //激活 PDP 上下文
OK
AT+CGACT?                     //查询当前 PDP 上下文状态
+CGACT: 1,1
+CGACT: 2,1
OK
AT+CGACT=0,2                  //去激活 PDP 上下文
OK
```

### 7.4 AT+CGPADDR — 显示 PDP 地址

返回指定上下文标识符的 PDP 地址列表。若未指定 `<cid>`，返回所有已定义上下文的地址。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CGPADDR=?` | `+CGPADDR: (已定义 <cid>s)`<br>`OK` |
| 写 `AT+CGPADDR[=<cid>[,<cid>[,…]]]` | `+CGPADDR: <cid>,<PDP_addr>`<br>`[...]`<br>`OK` 或 `ERROR` |

- 最大响应时间：300 ms；参考：3GPP TS 27.007。
- 参数：`<cid>` 整型，特定 PDP 上下文定义（见 AT+CGDCONT）。`<PDP_addr>` 字符串，在适用地址空间标识 MT，可为静态（AT+CGDCONT 定义时设置）或动态（上次激活时分配）。若无地址则省略。

**示例**：
```
AT+CGDCONT=1,"IP","UNINET"    //定义 PDP 上下文
OK
AT+CGACT=1,1                  //激活 PDP
OK
AT+CGPADDR=1                  //显示 PDP 地址
+CGPADDR: 1,"10.76.51.180"
OK
```

### 7.5 AT+CGCLASS — GPRS 移动台类别

设置 MT 按指定操作模式运行。详见 *3GPP TS 23.060*。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CGCLASS=?` | `+CGCLASS: (支持的 <class>s)`<br>`OK` |
| 读 `AT+CGCLASS?` | `+CGCLASS: <class>`<br>`OK` |
| 写 `AT+CGCLASS=<class>` | `OK`<br>若错误：`+CME ERROR: <err>` 或 `ERROR` |

- 最大响应时间：300 ms；特性：立即生效，自动保存；参考：3GPP TS 27.007。
- 参数：`<class>` 字符串，GPRS 移动类别（功能降序）：`A` Class-A 模式（A/Gb 模式）或 CS/PS 模式（最高）；`B` Class-B 模式或 CS/PS 模式；`CG` Class-C 仅 PS 模式（A/Gb 模式）或 PS 模式；`CC` Class-C 仅 CS 模式（A/Gb 模式）或 CS 模式（最低）。`<err>` 见 10.2。

### 7.6 AT+CGREG — GPRS 网络注册状态

查询网络注册状态，控制 URC 呈现：`<n>=1` 时 GERAN/UTRAN GPRS 注册状态变化上报 `+CGREG: <stat>`；`<n>=2` 时网络小区变化上报 `+CGREG: <stat>[,[<lac>],[<ci>],[<AcT>],[<rac>]]`。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CGREG=?` | `+CGREG: (支持的 <n>s)`<br>`OK` |
| 读 `AT+CGREG?` | `+CGREG: <n>,<stat>[,[<lac>],[<ci>],[<AcT>],[<rac>]]`<br>`OK` |
| 写 `AT+CGREG=<n>` | `OK` 或 `ERROR` |

- 最大响应时间：300 ms；特性：立即生效，配置不保存；参考：3GPP TS 27.007。

**参数**：
- `<n>` 整型：<u>`0` 禁用 URC</u>；`1` 使能 `+CGREG: <stat>`；`2` 使能带位置信息的 `+CGREG: <stat>[,[<lac>],[<ci>],[<AcT>],[<rac>]]`。
- `<stat>` 整型，GPRS 注册状态：`0` 未注册，未搜网（GMM 状态 GMM-NULL 或 GMM-DEREGISTERED-INITIATED，GPRS 服务禁用但用户请求时允许附着）；`1` 已注册，归属网络；`2` 未注册，正在尝试附着或搜网；`3` 注册被拒；`4` 未知（脱离 GERAN/UTRAN 覆盖）；`5` 已注册，漫游。
- `<lac>` 字符串，两字节位置区码（十六进制，如 "00C3" 等于十进制 195）。
- `<ci>` 字符串，四字节 GERAN/UTRAN 小区 ID（十六进制）。
- `<AcT>` 整型，所选接入技术：`0` GSM；`2` UTRAN；`3` GSM w/EGPRS；`4` UTRAN w/HSDPA；`5` UTRAN w/HSUPA；`6` UTRAN w/HSDPA+HSUPA；`7` E-UTRAN。
- `<rac>` 字符串，一字节路由区码（十六进制）。

### 7.7 AT+CGEREP — 分组域事件上报

使能/禁用在分组域 MT 或网络发生某些事件时，MT 向 TE 发送 URC `+CGEV: XXX`。`<mode>` 控制本命令指定的 URC 处理；`<bfr>` 控制 `<mode>=1/2` 时对缓存码的影响。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CGEREP=?` | `+CGEREP: (支持的 <mode>s),(支持的 <bfr>s)`<br>`OK` |
| 读 `AT+CGEREP?` | `+CGEREP: <mode>,<bfr>`<br>`OK` 或 `ERROR` |
| 写 `AT+CGEREP=<mode>[,<bfr>]` | `OK` 或 `ERROR` |
| 执行 `AT+CGEREP` | `OK` |

- 最大响应时间：300 ms；特性：立即生效，自动保存；参考：3GPP TS 27.007。

**参数**：
- `<mode>` 整型：<u>`0` 在 MT 中缓存 URC，缓冲区满丢弃最旧，不转发到 TE</u>；`1` MT-TE 链路被占用（如在线数据模式）时丢弃 URC，否则直接转发；`2` 链路被占用时缓存 URC，可用时刷新到 TE，否则直接转发。
- `<bfr>` 整型：<u>`0` `<mode>=1/2` 时清空本命令定义的 MT URC 缓冲区</u>；`1` `<mode>=1/2` 时刷新缓冲区到 TE（刷新前先给 OK）。

**URC 事件定义（注）**：
1. `+CGEV: REJECT <PDP_type>,<PDP_addr>`：MT 无法以 `+CRING` 上报 PDP 上下文激活的网络请求并自动拒绝（不适用于 EPS）。
2. `+CGEV: NW REACT <PDP_type>,<PDP_addr>,[<cid>]`：网络请求上下文重激活（不适用于 EPS）。
3. `+CGEV: NW DEACT <PDP_type>,<PDP_addr>,[<cid>]`：网络强制上下文去激活。
4. `+CGEV: ME DEACT <PDP_type>,<PDP_addr>,[<cid>]`：ME 强制上下文去激活。
5. `+CGEV: NW DETACH`：网络强制分组域分离（所有活动上下文被去激活，不单独上报）。
6. `+CGEV: ME DETACH`：ME 强制分组域分离。
7. `+CGEV: NW CLASS <class>`：网络强制改变 MS 类别（上报最高可用类别）。
8. `+CGEV: ME CLASS <class>`：ME 强制改变 MS 类别。
9. `+CGEV: PDN ACT <cid>`：激活上下文（LTE 中的 PDN 连接或 GSM/UMTS 中的主 PDP 上下文）。
10. `+CGEV: PDN DEACT <cid>`：去激活上下文。

### 7.8 AT+CGSMS — 选择主叫短信服务

指定 MT 发送 MO（主叫）短信使用的服务或服务偏好。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CGSMS=?` | `+CGSMS: (当前可用 <service>s)`<br>`OK` |
| 读 `AT+CGSMS?` | `+CGSMS: <service>`<br>`OK` |
| 写 `AT+CGSMS=<service>` | `OK`<br>若错误：`+CME ERROR: <err>` |

- 最大响应时间：300 ms；特性：立即生效，自动保存；参考：3GPP TS 27.007。
- 参数：`<service>` 整型：`0` 分组域；<u>`1` 电路交换</u>；`2` 分组域优先（GPRS 不可用则用电路交换）；`3` 电路交换优先（电路交换不可用则用分组域）。`<err>` 见 10.2。

### 7.9 AT+CEREG — EPS 网络注册状态

查询网络注册状态，控制 URC 呈现：`<n>=1` 时 E-UTRAN EPS 注册状态变化上报 `+CEREG: <stat>`；`<n>=2` 时网络小区变化上报 `+CEREG: <stat>[,[<tac>],[<ci>],[<AcT>]]`。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CEREG=?` | `+CEREG: (支持的 <n>s)`<br>`OK` |
| 读 `AT+CEREG?` | `+CEREG: <n>,<stat>[,<tac>,<ci>[,<AcT>]]`<br>`OK` |
| 写 `AT+CEREG=[<n>]` | `OK` 或 `ERROR` |

- 最大响应时间：300 ms；特性：立即生效，配置不保存；参考：3GPP TS 27.007。

**参数**：
- `<n>` 整型：<u>`0` 禁用 URC</u>；`1` 使能 `+CEREG: <stat>`；`2` 使能带位置信息的 `+CEREG: <stat>[,[<tac>],[<ci>],[<AcT>]]`。
- `<stat>` 整型，EPS 注册状态：`0` 未注册，未搜网；`1` 已注册，归属网络；`2` 未注册，正在尝试附着或搜网；`3` 注册被拒；`4` 未知（脱离 E-UTRAN 覆盖）；`5` 已注册，漫游。
- `<tac>` 字符串，两字节跟踪区码（十六进制）。
- `<ci>` 字符串，四字节 E-UTRAN 小区 ID（十六进制）。
- `<AcT>` 整型，所选接入技术：`0` GSM；`2` UTRAN；`3` GSM w/EGPRS；`4` UTRAN w/HSDPA；`5` UTRAN w/HSUPA；`6` UTRAN w/HSDPA+HSUPA；`7` E-UTRAN。

---

## 8 音频命令

### 8.1 AT+CLVL — 扬声器音量级别

选择 MT 内部扬声器的音量级别。

| 命令 | 响应 |
|---|---|
| 测试 `AT+CLVL=?` | `+CLVL: (支持的 <level>s)`<br>`OK` |
| 读 `AT+CLVL?` | `+CLVL: <level>`<br>`OK` 或 `ERROR` |
| 写 `AT+CLVL=<level>` | `OK` 或 `ERROR`<br>若错误：`+CME ERROR: <err>` |

- 最大响应时间：300 ms；特性：立即生效，配置不保存；参考：3GPP TS 27.007。
- 参数：`<level>` 整型，厂商特定范围的音量级别（最小值表示最低音量）。范围：0–5；默认：3。`<err>` 见 10.2。

### 8.2 AT+VTS — DTMF 与提示音生成

发送 ASCII 字符使 MSC 向远端发送 DTMF 音。仅可在语音呼叫中操作。

| 命令 | 响应 |
|---|---|
| 测试 `AT+VTS=?` | `+VTS: (支持的 <DTMF_string>s),(支持的 <duration>s)`<br>`OK` |
| 写 `AT+VTS=<DTMF_string>[,<duration>]` | `OK` 或 `ERROR`<br>若错误：`+CME ERROR: <err>` |

- 最大响应时间：取决于 `<DTMF_string>` 和 `<duration>` 长度；参考：3GPP TS 27.007。

**参数**：
- `<DTMF_string>` 字符串，字符集 `0...9`、`#`、`*`、`A`、`B`、`C`、`D`，须用引号 `"..."` 括起。一次发送多个音时，两音间隔 `<interval>` 可由 AT+VTD 指定。字符串最大长度 31 字节。
- `<duration>` 每个音的时长，单位 10ms（有容差）。范围 300–600，默认 300。若小于网络规定最小时间，实际时长为网络规定时间。若省略此参数，`<duration>` 可由 AT+VTD 指定。
- `<err>` 见 10.2。

**示例**：
```
ATD12345678900;     //拨号
OK
//呼叫已接通
AT+VTS="1"          //远端主叫可听到 DTMF 音
OK
AT+VTS="1234567890A"  //一次发送多个音
OK
```

### 8.3 AT+VTD — 提示音时长

设置 DTMF 音的时长，也可设置一次发送多个音时两音的时间间隔。

| 命令 | 响应 |
|---|---|
| 测试 `AT+VTD=?` | `+VTD: (支持的 <duration>s),(支持的 <interval>s)`<br>`OK` |
| 读 `AT+VTD?` | `+VTD: <duration>,<interval>`<br>`OK` |
| 写 `AT+VTD=<duration>[,<interval>]` | `OK` 或 `ERROR`<br>若错误：`+CME ERROR: <err>` |

- 最大响应时间：300 ms；特性：立即生效，配置不保存；参考：3GPP TS 27.007。

**参数**：
- `<duration>` 整型，音时长（1/10 秒，有容差）。范围 300–600，默认 300。若小于网络规定最小时间，实际时长为网络规定时间。
- `<interval>` 整型，AT+VTS 一次发送多个音时两音间隔。范围 300–600，默认 300，单位 100ms。
- `<err>` 见 10.2。

---

## 9 硬件相关命令

### 9.1 AT+QADC — 读取 ADC 值

读取 ADC 通道的电压值。

| 命令 | 响应 |
|---|---|
| 测试 `AT+QADC=?` | `+QADC: (支持的 <port>s)`<br>`OK` |
| 读 `AT+QADC=<port>` | `+QADC: <status>,<value>`<br>`OK` |

- 最大响应时间：300 ms。
- 参数：`<port>` 整型，ADC 通道号：`0` ADC 通道 0；`1` ADC 通道 1。`<status>` 整型，ADC 读取是否成功：`0` 失败；`1` 成功。`<value>` 整型，指定 ADC 通道电压，单位 mV。

---

## 10 附录：参考资料

### 10.1 术语缩写（表 2）

| 缩写 | 全称 |
|---|---|
| 3GPP | 3rd Generation Partnership Project |
| ADC | Analog-to-Digital Converter |
| APN | Access Point Name |
| ASCII | American Standard Code for Information Interchange |
| BAIC | Bar All Incoming Calls |
| BAOC | Bar All Outgoing Calls |
| BCD | Binary-Coded Decimal |
| BOIC | Bar Outgoing International Calls |
| BOIC-exHC | Bar Outgoing International Calls except to Home Country |
| CS | Circuit Switching |
| DCE | Data Communication Equipment |
| DHCP | Dynamic Host Configuration Protocol |
| DTE | Data Terminal Equipment |
| DTMF | Dual-Tone Multifrequency |
| EGPRS | Enhanced General Packet Radio Service |
| EPS | Evolved Packet System |
| E-UTRAN | Evolved Universal Terrestrial Radio Access Network |
| GGSN | Gateway GPRS Support Node |
| GMM | GPRS Mobility Management |
| GMT | Greenwich Mean Time |
| GPRS | General Packet Radio Service |
| GSM | Global System for Mobile Communications |
| HSDPA | High Speed Downlink Packet Access |
| HSUPA | High Speed Uplink Packet Access |
| IMEI | International Mobile Equipment Identity |
| IMSI | International Mobile Subscriber Identity |
| IP | Internet Protocol |
| IPv4 | Internet Protocol version 4 |
| IPv6 | Internet Protocol version 6 |
| IRA | International Reference Alphabet |
| ME | Mobile Equipment |
| MO | Mobile Originated |
| MS | Mobile Station |
| MSC | Mobile Switching Center |
| MT | Mobile Terminal |
| NAS | Non-Access Stratum |
| NITZ | Network Identity and Time Zone / Network Informed Time Zone |
| P-CSCF | Proxy-Call Session Control Function |
| PDP | Packet Data Protocol |
| PDU | Packet Data Unit |
| PIN | Personal Identification Number |
| PLMN | Public Land Mobile Network |
| PPP | Point-to-Point Protocol |
| PS | Packet Switching |
| PUK | PIN Unlock Key |
| RAT | Radio Access Technology |
| RF | Radio Frequency |
| RX | Receive |
| RxQual | Receive Quality |
| SM | Session Management |
| SMS | Short Message Service |
| SMSC | Short Message Service Center |
| SNDCP | SubNetwork Dependent Convergence Protocol |
| TA | Terminal Adapter |
| TE | Terminal Equipment |
| TFT | Traffic Flow Template |
| TPDU | Transport Protocol Data Unit |
| UART | Universal Asynchronous Receiver/Transmitter |
| USC-2 | Universal Character Set (UCS-2) Format |
| UE | User Equipment |
| UICC | Universal Integrated Circuit Card |
| UIM | User Identity Model |
| UMTS | Universal Mobile Telecommunications System |
| URC | Unsolicited Result Code |
| USB | Universal Serial Bus |
| (U)SIM | (Universal) Subscriber Identity Module |
| VLR | Visiting Location Register |

### 10.2 移动终端错误结果码 +CME ERROR（表 3：通用 +CME ERROR 码汇总）

最终结果码 `+CME ERROR: <err>` 表示与移动设备或网络相关的错误。其行为类似常规 `ERROR`：若某命令行中某命令返回 `+CME ERROR`，则同一命令行后续命令不执行（既不返回 `ERROR` 也不返回 `OK`）。`<err>` 格式可为数字或文字。

| 数字 `<err>` | 文字 `<err>` | 中文含义 |
|---|---|---|
| 0 | phone failure | 手机故障 |
| 1 | no connection to phone | 无法连接到手机 |
| 2 | phone-adaptor link reserved | 手机-适配器链路被占用 |
| 3 | operation not allowed | 不允许操作 |
| 4 | operation not supported | 不支持操作 |
| 5 | PH-SIM PIN required | 需要 PH-SIM PIN |
| 6 | PH-FSIM PIN required | 需要 PH-FSIM PIN |
| 7 | PH-FSIM PUK required | 需要 PH-FSIM PUK |
| 10 | SIM not inserted | SIM 未插入 |
| 11 | SIM PIN required | 需要 SIM PIN |
| 12 | SIM PUK required | 需要 SIM PUK |
| 13 | SIM failure | SIM 失败 |
| 14 | SIM busy | SIM 忙 |
| 15 | SIM wrong | SIM 错误 |
| 16 | incorrect password | 密码不正确 |
| 17 | SIM PIN2 required | 需要 SIM PIN2 |
| 18 | SIM PUK2 required | 需要 SIM PUK2 |
| 20 | memory full | 内存已满 |
| 21 | invalid index | 索引无效 |
| 22 | not found | 未找到 |
| 23 | memory failure | 内存故障 |
| 24 | text string too long | 文本字符串过长 |
| 25 | invalid characters in text string | 文本字符串含非法字符 |
| 26 | dial string too long | 拨号字符串过长 |
| 27 | invalid characters in dial string | 拨号字符串含非法字符 |
| 30 | no network service | 无网络服务 |
| 31 | network timeout | 网络超时 |
| 32 | network not allowed - emergency calls only | 网络不允许 - 仅紧急呼叫 |
| 40 | network personalization PIN required | 需要网络个性化 PIN |
| 41 | network personalization PUK required | 需要网络个性化 PUK |
| 42 | network subset personalization PIN required | 需要网络子集个性化 PIN |
| 43 | network subset personalization PUK required | 需要网络子集个性化 PUK |
| 44 | service provider personalization PIN required | 需要服务提供商个性化 PIN |
| 45 | service provider personalization PUK required | 需要服务提供商个性化 PUK |
| 46 | corporate personalization PIN required | 需要企业个性化 PIN |
| 47 | corporate personalization PUK required | 需要企业个性化 PUK |
| 901 | audio unknown error | 音频未知错误 |
| 902 | audio invalid parameters | 音频参数无效 |
| 903 | audio operation not supported | 音频操作不支持 |
| 904 | audio device busy | 音频设备忙 |

### 10.3 URC 汇总（表 4）

| 索引 | URC 显示 | 含义 | 触发条件 |
|---|---|---|---|
| 1 | `+CREG: <stat>` | MT 注册状态 | `AT+CREG=1` |
| 2 | `+CREG: <stat>[,[<lac>],[<ci>],[<AcT>]]` | 小区邻区变化后显示 MT 当前注册状态（含位置区码） | `AT+CREG=2` |
| 3 | `+CGREG: <stat>` | MT 网络注册状态 | `AT+CGREG=1` |
| 4 | `+CGREG: <stat>[,[<lac>],[<ci>],[<AcT>],[<rac>]]` | MT 网络注册及位置信息 | `AT+CGREG=2` |
| 5 | `+CEREG: <stat>` | MT 的 EPS 网络注册状态 | `AT+CEREG=1` |
| 6 | `+CEREG: <stat>[,[<tac>],[<ci>],[<AcT>]]` | MT 的 EPS 网络注册及位置信息 | `AT+CEREG=2` |
| 7 | `+CTZV: <tz>` | 时区上报 | `AT+CTZR=1` |
| 8 | `+CTZE: <tz>,<dst>,<time>` | 扩展时区上报 | `AT+CTZR=2` |
| 9 | `RING` | 来电 | N/A |
| 10 | `+CFUN: 1` | MT 所有功能可用 | N/A |
| 11 | `+CPIN: <state>` | (U)SIM 卡 PIN 状态 | N/A |
| 12 | `+CGEV: REJECT <PDP_type>,<PDP_addr>` | PDP 激活网络请求被自动拒绝 | `AT+CGEREP=2,1` |
| 13 | `+CGEV: NW REACT <PDP_type>,<PDP_addr>,[<cid>]` | 网络请求 PDP 重激活 | `AT+CGEREP=2,1` |
| 14 | `+CGEV: NW DEACT <PDP_type>,<PDP_addr>,[<cid>]` | 网络强制上下文去激活 | `AT+CGEREP=2,1` |
| 15 | `+CGEV: ME DEACT <PDP_type>,<PDP_addr>,[<cid>]` | ME 强制上下文去激活 | `AT+CGEREP=2,1` |
| 16 | `+CGEV: NW DETACH` | 网络强制分组域分离 | `AT+CGEREP=2,1` |
| 17 | `+CGEV: ME DETACH` | ME 强制分组域分离 | `AT+CGEREP=2,1` |
| 18 | `+CGEV: NW CLASS <class>` | 网络强制改变 MS 类别 | `AT+CGEREP=2,1` |
| 19 | `+CGEV: ME CLASS <class>` | ME 强制改变 MS 类别 | `AT+CGEREP=2,1` |
| 20 | `+CGEV: PDN ACT <cid>` | 激活上下文 | `AT+CGEREP=2,1` |
| 21 | `+CGEV: PDN DEACT <cid>` | 去激活上下文 | `AT+CGEREP=2,1` |

---

*Copyright © Quectel Wireless Solutions Co., Ltd. 2023. All rights reserved.*
