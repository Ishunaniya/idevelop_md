# EC200A-CN(TA) QuecOpen 硬件设计手册

> **源文件**：`Quectel_EC200A-CN(TA)_QuecOpen_硬件设计手册_V1.0.0_Preliminary_20230411.pdf`
> **产品系列**：LTE Standard 模块系列
> **版本**：1.0.0　**日期**：2023-04-11　**状态**：临时文件（Preliminary）
> **版权**：版权所有 © 上海移远通信技术股份有限公司 2023，保留一切权利。

> 本 Markdown 为该 PDF 的**逐章节、逐表、逐图完整转写**（含所有正文、表格、参考电路网表、时序参数与机械尺寸）。图形类内容（框图/引脚图/参考电路/时序图/尺寸图）已用文字 + 表格转写并附说明。

---

## 文档信息与法律声明

**移远通信联系方式**
- 上海移远通信技术股份有限公司
- 地址：上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼，邮编：200233
- 电话：+86 21 5108 6236　邮箱：info@quectel.com
- 销售：http://www.quectel.com/cn/support/sales.htm
- 技术支持：http://www.quectel.com/cn/support/technical.htm 或 support@quectel.com

**前言**：移远通信提供该文档内容以支持客户的产品设计。客户须按照文档中提供的规范、参数来设计产品。移远通信提供的参考设计仅作为示例，客户应在设计目标产品时使用独立的分析、评估和判断。文档和服务在"可用"基础上提供，移远通信可在未事先通知的情况下随时增加、修改或重述本文档。

**使用和披露限制**：包含许可协议、版权声明、商标、第三方权利、隐私声明等条款（接收方须对内容保密，不得用于本项目以外目的；未经书面同意不得复制、转载、翻译、分发、修改受版权保护资料）。

**隐私声明**：为实现产品功能，特定设备数据将上传至移远通信或第三方服务器（运营商、芯片供应商或客户指定服务器）。

**免责声明**：
1. 移远通信不承担任何因未能遵守有关操作或设计规范而造成损害的责任。
2. 不承担因本文档中任何不准确、遗漏或使用本文档信息而产生的责任。
3. 对开发中功能不做任何暗示或法定保证。
4. 对第三方网站及资源的可访问性、安全性、准确性、可用性、合法性和完整性不承担法律责任。

---

## 安全须知

为确保个人安全并保护产品和工作环境免遭潜在损坏，请遵循如下安全须知。产品制造商需将下列安全须知传达给终端用户，并体现在终端产品的用户手册中。

- **道路行驶**：开车时请勿使用手持移动终端设备（即使有免提功能）。先停车，再打电话。
- **登机前**：请关闭移动终端设备。飞机上禁止开启无线功能，以防干扰飞机通讯系统。
- **医院/健康看护场所**：注意是否存在移动终端使用限制，射频干扰可能导致医疗设备运行失常。
- **紧急呼叫**：设备并不保障在任何情况下均能有效连接（欠费或 (U)SIM 卡无效时）。紧急情况下不能将带紧急呼叫功能的设备作为唯一联系方式。
- **射频干扰**：开机时会接收和发射射频信号，靠近电视、收音机、电脑等会产生干扰。
- **易燃易爆品**：确保设备远离易燃易爆品；靠近加油站、油库、化工厂或爆炸作业场所时请关机。

---

## 文档历史 / 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| - | 2021-11-03 | Double YANG / Harlan JIANG | 文档创建 |
| 1.0.0 | 2023-04-11 | Double YANG / Mason GUO / Luis BU | 临时版本 |

---

# 1 引言

QuecOpen® 是一种以移远通信模块作为主处理器的应用方案。其主要特点：

- 快捷开发嵌入式应用，缩短产品开发周期
- 简化电路设计，降低成本
- 减小终端产品的实际尺寸
- 降低产品功耗
- 远程空中无线升级
- 改善产品的市场性价比，提升产品竞争力

本文档定义了 EC200A-CN(TA) QuecOpen® 模块及其与客户应用连接的空中接口和硬件接口，帮助客户快速了解模块的硬件接口规范、电气特性、机械规范及其他相关信息。

## 1.1 特殊符号

**表 1：特殊符号**

| 符号 | 定义 |
|---|---|
| `[…]` | 在引脚名称后的、包含数字范围的中括号表示所有相同类型的引脚。例如 `SD_SDIO_DATA[0:3]` 表示所有四个 SD_SDIO_DATA 引脚 SD_SDIO_DATA0、SD_SDIO_DATA1、SD_SDIO_DATA2 和 SD_SDIO_DATA3。 |

---

# 2 产品综述

本产品是一款具有分集接收功能的 **LTE-FDD/LTE-TDD/WCDMA/GSM** 无线通信模块，支持 LTE-FDD、LTE-TDD、HSUPA、HSPA+、WCDMA、EDGE 和 GPRS 网络数据连接，可为特殊应用提供语音功能，同时支持 GNSS 功能。客户可根据地区或运营商选择专用类型。

**表 2：模块基本信息**

| 基本信息 | 内容 |
|---|---|
| 封装及引脚数量 | 贴片模块，LCC 引脚 80 个，LGA 引脚 64 个 |
| 尺寸规格 | (29.0 ±0.15) mm × (32.0 ±0.15) mm × (2.65 ±0.2) mm |
| 重量 | 约 4.6 g |
| 无线网络功能 | LTE / WCDMA / GSM |

## 2.1 频段及功能

**表 3：无线网络制式**

| 制式 | EC200A-CN(TA) QuecOpen |
|---|---|
| LTE-FDD | B1 / B3 / B5 / B8 |
| LTE-TDD | B34 / B38 / B39 / B40 / B41 |
| WCDMA | B1 / B5 / B8 |
| GSM | 900 / 1800 |
| GNSS ¹ | GPS、GLONASS、BeiDou、Galileo、QZSS |

> ¹ 内置 GNSS 为选配功能。

## 2.2 关键特性

**表 4：模块主要性能**

| 参数 | 说明 |
|---|---|
| 供电电压 | VBAT 供电电压范围：3.4~4.3 V；典型供电电压：3.8 V |
| 短消息（SMS） | 文本和 PDU 模式；点对点短信收发；短消息小区广播；短消息存储默认存储至模块 |
| (U)SIM 卡接口 | 支持 (U)SIM 卡：1.8 / 3.0 V |
| 音频特性 | 支持 1 路数字音频接口（PCM）；支持 1 路模拟音频接口（MIC/SPK）；GSM：HR/FR/EFR/AMR/AMR-WB；WCDMA：AMR/AMR-WB；LTE：AMR/AMR-WB；支持回音消除和噪声抑制 |
| 数字音频接口 | PCM：用于音频，需外接 Codec 芯片；16 位线性编码格式；短帧模式；主模式 |
| SPI 接口 | 提供与外设双工、同步、串行通信链路；1 路 SPI，仅支持主模式；一对一连接，无片选信号；1.8 V 电压域，最高时钟 26 MHz |
| RGMII 接口 | 半/全双工速率 10 Mbps、100 Mbps 以太网连接 |
| I2C 接口 | 1 路 I2C；符合 I2C 总线协议规范 100/400 KHz；不支持多主机模式 |
| WLAN 接口 | 支持用于 WLAN 功能的 SDIO 接口 |
| USB 接口 | 支持 USB 2.0，最大 480 Mbps；用于 AT 命令、数据传输、软件调试和升级；USB 虚拟串口驱动支持 Windows 7/8/8.1/10、Linux 2.6~5.18、Android 4.x–12.x |
| SD_SDIO 接口 | 符合 SD3.0 协议；支持 eMMC 4.5.1 |
| 串口 | **主串口**：用于 AT 命令和数据传输，默认 115200 bps，支持 RTS/CTS 硬件流控。**调试串口**：用于 Linux 控制台和日志输出，115200 bps |
| AT 命令 | 3GPP TS 27.007 和 3GPP TS 27.005 定义的命令，以及移远增强型 AT 命令 |
| 分集接收功能 | 支持 LTE 分集接收 |
| 天线接口 | 主天线（ANT_MAIN）、分集接收天线（ANT_DRX）、GNSS 天线（ANT_GNSS）；50 Ω 特性阻抗 |
| 发射功率 | EGSM900: Class 4 (33 dBm ±2 dB)；DCS1800: Class 1 (30 dBm ±2 dB)；EGSM900 8-PSK: Class E2 (27 dBm ±3 dB)；DCS1800 8-PSK: Class E2 (26 dBm ±3 dB)；WCDMA: Class 3 (24 dBm +1/-3 dB)；LTE-FDD: Class 3 (23 dBm ±2 dB)；LTE-TDD: Class 3 (23 dBm ±2 dB) |
| LTE 特性 | 支持 3GPP R9 non-CA Cat 4 FDD 和 TDD；支持 1.4/3/5/10/15/20 MHz 射频带宽；下行支持 MIMO；上行 QPSK、16-QAM；下行 QPSK、16-QAM、64-QAM；LTE-FDD 下行最大 150 Mbps / 上行 50 Mbps；LTE-TDD 下行最大 130 Mbps / 上行 30 Mbps |
| UMTS 特性 | 支持 3GPP R7、HSPA+、HSDPA、HSUPA 和 WCDMA；支持 QPSK、16-QAM、64-QAM；HSUPA 上行最大 5.76 Mbps；WCDMA 下行最大 384 Kbps / 上行 384 Kbps |
| GSM 特性 | **GPRS**：多时隙等级 12；CS-1/CS-2/CS-3/CS-4；下/上行最大 85.6 Kbps。**EDGE**：多时隙等级 12；GMSK 和 8-PSK 调制；下/上行编码 MCS 1-9；下/上行最大 236.8 Kbps |
| 温度范围 | 正常工作温度 ²：-30~+75 °C；扩展工作温度 ³：-40~+85 °C；存储温度：-40~+90 °C |
| 软件升级 | 可通过 USB2.0 接口或 FOTA 升级 |
| RoHS | 所有器件完全符合 EU RoHS 标准 |

> ² 在此温度范围工作时，模块相关性能满足 3GPP 标准要求。
> ³ 在此温度范围工作时，模块仍能保持正常工作状态（语音、短信、数据传输等），不会出现不可恢复故障；射频频谱、网络基本不受影响；仅个别指标（如输出功率）可能超出 3GPP 范围；温度恢复正常后各项指标仍符合 3GPP 标准。

## 2.3 功能框图

**图 1：功能框图**

模块功能框图阐述了如下主要功能：**电源管理、基带、存储器、射频部分、外围接口**。

框图结构转写：
- **射频前端**：三个天线端口 `ANT_MAIN`、`ANT_DRX`、`ANT_GNSS`。
  - ANT_MAIN 经 `Switch` → `Duplex/BPF`（配 `SAW`）连接 `PA`（发射 Tx）与主接收 `PRx`。
  - ANT_DRX 经 `Switch` → `SAW` → 分集接收 `DRx`。
  - PA、PRx、DRx 均接入 `Transceiver`（收发器）。
  - `26M DCXO`（26 MHz 温补晶振）为系统提供时钟参考。
  - VBAT_RF 为射频部分（PA、Switch、Transceiver 等）供电。
- **基带与存储**：`Transceiver` ↔ `Baseband`（基带）；`GNSS` 模块、`Nand flash`、`LPDDR2` 均与 Baseband 相连。
- **电源管理**：`PMU`（电源管理单元）由 `VBAT_BB` 供电，并通过 `Control` 与基带交互；`PWRKEY`、`ADCs` 接入 PMU；`VDD_EXT` 由 PMU 输出。
- **外围接口**（自基带引出）：`VDD_EXT`、`SPKs`、`MICs`、`PCM`、`RESET_N`、`USB`、`(U)SIM`、`I2C`、`RGMII`、`UARTs`、`STATUS`、`SDIOs`、`SPI`。

## 2.4 引脚分配图

**图 2：引脚分配俯视图**

模块为方形贴片封装，四周一圈为 LCC/LGA 引脚，内部（中央）为 LGA 焊盘阵列。引脚编号 1~144（含 GND、RESERVED）。完整引脚名称与编号见下方 **表 6**。引脚颜色图例分类：

> Power Pins（电源）、Signal Pins（信号）、USIM、Debug_UART、Main_UART、SD、PCM_DOUT、SPI、WLAN、USB、I2C、ADC、GPIO、ANT（天线）、RGMII、RESERVED（预留）。

**图 2 备注**：
1. 上图为选配了 GNSS 功能模块的引脚分配图；当使用非内置 GNSS 模块时，引脚 63、66 可作为串口使用，引脚 118 可作为普通 GPIO 口使用。
2. 如果不使用引脚 PCM_CLK、EMMC_SDIO_CLK、I2C_SCL、SPI_CLK、WLAN_SDIO_CLK，为防止对射频产生干扰，建议在靠近这些引脚处分别贴一个 33 pF 电容。其他不用的引脚和预留引脚悬空，所有 GND 引脚连接到地网络上。
3. 模块完全开机之前禁止上拉引脚 115 和 119，否则可能导致模块工作异常。
4. 为提高 ESD 防护效果，建议所有 GPIO 口预留 10 nF 并联电容，更多信息请参考文档 [7]。

## 2.5 引脚描述表

**表 5：I/O 参数定义**

| 类型 | 描述 | 类型 | 描述 |
|---|---|---|---|
| AI | 模拟输入 | DIO | 数字输入/输出 |
| AO | 模拟输出 | OD | 漏极开路 |
| AIO | 模拟输入/输出 | PI | 电源输入 |
| DI | 数字输入 | PO | 电源输出 |
| DO | 数字输出 | | |

**表 6：模块引脚描述**

### 电源

| 引脚名 | 引脚号 | I/O | 描述 | DC 特性 | 备注 |
|---|---|---|---|---|---|
| VBAT_BB | 59、60 | PI | 模块基带电源 | Vmax=4.3 V, Vmin=3.4 V, Vnom=3.8 V | 外部电源必须能够提供达 0.8 A 的电流 |
| VBAT_RF | 57、58 | PI | 模块射频电源 | Vmax=4.3 V, Vmin=3.4 V, Vnom=3.8 V | 外部电源必须能够提供达 2.0 A 的电流 |
| VDD_EXT | 7 | PO | 外部电路 1.8 V 供电 | Vnom=1.8 V, IOmax=50 mA | 可为外部 GPIO 提供上拉电源；建议预留测试点；不用则悬空 |
| GND | 8、9、10、19、22、36、46、48、50~54、56、72、85~112 | - | 地 | - | - |

### 开关机

| 引脚名 | 引脚号 | I/O | 描述 | DC 特性 | 备注 |
|---|---|---|---|---|---|
| PWRKEY | 21 | DI | 模块开/关机 | VILmax = 0.3 × VBAT_BB | 不用则悬空；低电平有效 |
| RESET_N | 20 | DI | 模块复位 | VILmax = 0.5 V | 1.8 V 电压域；建议预留测试点；开机后低电平有效 |

### 状态指示接口

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| STATUS | 61 | OD | 运行状态指示 | 需要外部上拉；不用则悬空 |

### USB 接口

| 引脚名 | 引脚号 | I/O | 描述 | DC 特性 | 备注 |
|---|---|---|---|---|---|
| USB_VBUS | 71 | AI | USB 检测 | Vmax=5.25 V, Vmin=4.0 V, Vnom=5.0 V | 典型值 5.0 V；须预留测试点 |
| USB_DP | 69 | AIO | USB 差分数据（+） | - | 要求 90 Ω 差分阻抗；符合 USB 2.0 规范；须预留测试点 |
| USB_DM | 70 | AIO | USB 差分数据（-） | - | 同上 |

### (U)SIM 接口

| 引脚名 | 引脚号 | I/O | 描述 | DC 特性 | 备注 |
|---|---|---|---|---|---|
| USIM_VDD | 14 | PO | (U)SIM 卡供电电源 | 1.8 V：Vmax=1.9 V, Vmin=1.7 V；3.0 V：Vmax=3.05 V, Vmin=2.7 V | 模块自动识别 1.8 V 或 3.0 V (U)SIM 卡 |
| USIM_DATA | 15 | DIO | (U)SIM 卡数据 | 1.8 V：VILmax=0.54, VIHmin=1.26, VOLmax=0.2, VOHmin=1.6 V；3.0 V：VILmax=0.8, VIHmin=2, VOLmax=0.4, VOHmin=2.4 V | - |
| USIM_CLK | 16 | DO | (U)SIM 卡时钟 | 1.8 V：VOLmax=0.2, VOHmin=1.6 V；3.0 V：VOLmax=0.4, VOHmin=2.4 V | - |
| USIM_RST | 17 | DO | (U)SIM 卡复位 | 同 USIM_CLK | - |

### 模拟音频接口

| 引脚名 | 引脚号 | I/O | 描述 |
|---|---|---|---|
| MICBIAS | 140 | PO | 麦克风偏置电压 |
| MIC_P | 125 | AI | 麦克风输入通道（+） |
| MIC_N | 126 | AI | 麦克风输入通道（-） |
| SPK_P | 124 | AO | 模拟音频差分输出通道（+） |
| SPK_N | 123 | AO | 模拟音频差分输出通道（-） |

### SDIO 接口（eMMC）

| 引脚名 | 引脚号 | I/O | 描述 | DC 特性 | 备注 |
|---|---|---|---|---|---|
| EMMC_SDIO_CLK | 32 | DO | eMMC 时钟 | 1.8 V：VOLmax=0.2, VOHmin=1.6 V | eMMC 应用中 SDIO 信号电平需固定在 1.8 V；不用则悬空 |
| EMMC_SDIO_CMD | 33 | DIO | eMMC 命令 | 1.8 V：VOLmax=0.2, VOHmin=1.6, VILmin=-0.3, VILmax=0.54, VIHmin=1.26, VIHmax=2.0 V | 同上 |
| EMMC_SDIO_DATA0 | 31 | DIO | eMMC 数据位 0 | 同 CMD | - |
| EMMC_SDIO_DATA1 | 30 | DIO | eMMC 数据位 1 | 同 CMD | - |
| EMMC_SDIO_DATA2 | 29 | DIO | eMMC 数据位 2 | 同 CMD | - |
| EMMC_SDIO_DATA3 | 28 | DIO | eMMC 数据位 3 | 同 CMD | - |
| SDIO_VDD | 34 | PO | SDIO 电源 | IOmax=200 mA | 输出 1.8/2.85 V 支持软件配置；不用则悬空 |

> 注：本表（引脚描述）SDIO 引脚名为 `EMMC_SDIO_*`；第 4.7.1 节 表 20 中同一组引脚记为 `SD_SDIO_*`（功能相同）。

### 主串口

| 引脚名 | 引脚号 | I/O | 描述 | DC 特性 | 备注 |
|---|---|---|---|---|---|
| MAIN_CTS | 64 | DO | DTE 清除发送（连接至 DTE 的 CTS） | VOLmax=0.2, VOHmin=1.6 V | 1.8 V 电压域；不用则悬空 |
| MAIN_RTS | 65 | DI | DTE 请求发送（连接至 DTE 的 RTS） | VILmin=-0.3, VILmax=0.54, VIHmin=1.26, VIHmax=2.0 V | 同上 |
| MAIN_RXD | 68 | DI | 主串口接收 | 同 RTS | 同上 |
| MAIN_TXD | 67 | DO | 主串口发送 | VOLmax=0.2, VOHmin=1.6 V | 同上 |

### 调试串口

| 引脚名 | 引脚号 | I/O | 描述 | DC 特性 | 备注 |
|---|---|---|---|---|---|
| DBG_RXD | 11 | DI | 调试串口接收 | VILmin=-0.3, VILmax=0.54, VIHmin=1.26, VIHmax=2.0 V | 1.8 V 电压域；须预留测试点 |
| DBG_TXD | 12 | DO | 调试串口发送 | VOLmax=0.2, VOHmin=1.6 V | 同上 |

### I2C 接口

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| I2C_SCL | 41 | OD | I2C 串行时钟 | 需要外部 1.8 V 上拉 |
| I2C_SDA | 42 | OD | I2C 串行数据 | 不用则悬空 |

### PCM 接口

| 引脚名 | 引脚号 | I/O | 描述 | DC 特性 | 备注 |
|---|---|---|---|---|---|
| PCM_SYNC | 26 | DIO | PCM 帧同步 | VILmin=-0.3, VILmax=0.54, VIHmin=1.26, VIHmax=2.0, VOLmax=0.2, VOHmin=1.6 V | 1.8 V 电压域；模块作主设备时为输出，作从设备时为输入 |
| PCM_CLK | 27 | DIO | PCM 时钟 | 同上 | 同上；不用则悬空 |
| PCM_IN | 24 | DI | PCM 数据输入 | VILmin=-0.3, VILmax=0.54, VIHmin=1.26, VIHmax=2.0 V | 1.8 V 电压域；不用则悬空 |
| PCM_OUT | 25 | DO | PCM 数据输出 | VOLmax=0.2, VOHmin=1.6 V | 同上 |

### WLAN 接口

| 引脚名 | 引脚号 | I/O | 描述 | DC 特性 | 备注 |
|---|---|---|---|---|---|
| WLAN_PWR_EN | 127 | DO | WLAN 电源使能控制 | VOLmax=0.2, VOHmin=1.6 V | - |
| WLAN_SDIO_DATA3 | 129 | DIO | WLAN SDIO 数据位 3 | VILmin=-0.3, VILmax=0.54, VIHmin=1.26, VIHmax=2.0, VOLmax=0.2, VOHmin=1.6 V | - |
| WLAN_SDIO_DATA2 | 130 | DIO | WLAN SDIO 数据位 2 | 同上 | - |
| WLAN_SDIO_DATA1 | 131 | DIO | WLAN SDIO 数据位 1 | 同上 | - |
| WLAN_SDIO_DATA0 | 132 | DIO | WLAN SDIO 数据位 0 | 同上 | - |
| WLAN_SDIO_CLK | 133 | DO | WLAN SDIO 时钟 | VOLmax=0.2, VOHmin=1.6 V | - |
| WLAN_SDIO_CMD | 134 | DO | WLAN SDIO 命令 | VOLmax=0.2, VOHmin=1.6 V | - |
| WLAN_WAKE | 135 | DI | WLAN 唤醒模块 | VILmin=-0.3, VILmax=0.54, VIHmin=1.26, VIHmax=2.0 V | - |
| WLAN_EN | 136 | DO | WLAN 使能控制 | VOLmax=0.2, VOHmin=1.6 V | - |

### 射频天线接口

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| ANT_DRX | 35 | AI | 分集天线接口 | 50 Ω 特性阻抗；不用则悬空 |
| ANT_MAIN | 49 | AIO | 主天线接口 | 同上 |
| ANT_GNSS | 47 | AI | GNSS 天线接口 | 同上 |

### SPI 接口

| 引脚名 | 引脚号 | I/O | 描述 | DC 特性 | 备注 |
|---|---|---|---|---|---|
| SPI_CLK | 40 | DO | SPI 时钟 | VOLmax=0.2, VOHmin=1.6 V | 1.8 V 电压域；不用则悬空 |
| SPI_CS | 37 | DO | SPI 片选 | VOLmax=0.2, VOHmin=1.6 V | 同上 |
| SPI_DIN | 39 | DI | SPI 数据输入 | VILmin=-0.3, VILmax=0.54, VIHmin=1.26, VIHmax=2.0 V | 同上 |
| SPI_DOUT | 38 | DO | SPI 数据输出 | VOLmax=0.2, VOHmin=1.6 V | 同上 |

### ADC 接口

| 引脚名 | 引脚号 | I/O | 描述 | DC 特性 | 备注 |
|---|---|---|---|---|---|
| ADC0 | 45 | AI | 通用 ADC 接口 | 电压范围 0 V~3.8 V | 不用则悬空 |
| ADC1 | 44 | AI | 通用 ADC 接口 | 电压范围 0 V~3.8 V | 不用则悬空 |

### RGMII 接口

| 引脚名 | 引脚号 | I/O | 描述 | DC 特性 | 备注 |
|---|---|---|---|---|---|
| RGMII_RX_1 | 73 | DI | RGMII 接收数据位 1 | VILmin=-0.3, VILmax=0.54, VIHmin=1.26, VIHmax=2.0, VOLmax=0.2, VOHmin=1.6 V | 单端阻抗要求 50 Ω；不用则悬空 |
| RGMII_CTL_RX | 74 | DI | RGMII 接收控制 | 同上 | 同上 |
| RGMII_CK_RX | 75 | DI | RGMII 接收时钟 | 同上 | 同上 |
| RGMII_RX_0 | 76 | DI | RGMII 接收数据位 0 | 同上 | 同上 |
| RGMII_TX_0 | 77 | DO | RGMII 发送数据位 0 | 同上 | 同上 |
| RGMII_TX_1 | 78 | DO | RGMII 发送数据位 1 | 同上 | 同上 |
| RGMII_RX_2 | 79 | DI | RGMII 接收数据位 2 | 同上 | 同上 |
| RGMII_TX_2 | 80 | DO | RGMII 发送数据位 2 | 同上 | 同上 |
| RGMII_CTL_TX | 81 | DO | RGMII 发送控制 | 同上 | 同上 |
| RGMII_RX_3 | 82 | DI | RGMII 接收数据位 3 | 同上 | 同上 |
| RGMII_CK_TX | 83 | DO | RGMII 发送时钟 | 同上 | 同上 |
| RGMII_TX_3 | 84 | DO | RGMII 发送数据位 3 | 同上 | 同上 |
| RGMII_MD_IO | 121 | DIO | RGMII 管理数据 | 同上 | 同上 |
| RGMII_MD_CLK | 122 | DO | RGMII 管理数据时钟 | 同上 | 同上 |
| RGMII_INT | 120 | DI | RGMII 中断 | 同上 | 同上 |
| RGMII_RST_N | 119 | DO | RGMII 复位 | 同上 | 同上 |
| RGMII_PWR_EN | 23 | DO | RGMII 供电使能 | 同上 | 同上 |

### 其他接口

| 引脚名 | 引脚号 | I/O | 描述 | DC 特性 | 备注 |
|---|---|---|---|---|---|
| USB_BOOT | 115 | DI | 强制模块进入紧急下载模式 | VILmin=-0.3, VILmax=0.54, VIHmin=1.26, VIHmax=2.0 V | 1.8 V 电压域；高电平有效；建议预留测试点 |

### GPIO 引脚

| 引脚名 | 引脚号 | I/O | 描述 | DC 特性 | 备注 |
|---|---|---|---|---|---|
| GPIO1 | 1 | DI / DIO | WAKEUP_IN（休眠唤醒功能）/ 通用输入输出 | VILmin=-0.3, VILmax=0.54, VIHmin=1.26, VIHmax=2.0, VOLmax=0.2, VOHmin=1.6 V | 默认用作 WAKEUP_IN 休眠唤醒功能 |
| GPIO2 | 2 | DO / DIO | SLEEP_SYS_IND（系统休眠指示）/ 通用输入输出 | 同上 | 默认用作 SLEEP_SYS_IND 系统休眠指示 |
| GPIO3 | 3 | DIO | 通用输入/输出 | 同上 | - |
| GPIO4 | 4 | DIO | 通用输入/输出 | 同上 | - |
| GPIO5 | 5 | DIO | 通用输入/输出 | 同上 | - |
| GPIO6 | 62 | DIO | 通用输入/输出 | 同上 | - |
| GPIO7 | 6 | DIO | 通用输入/输出 | 同上 | - |
| GPIO8 | 13 | DIO | 通用输入/输出 | 同上 | - |

### 预留引脚

| 引脚名 | 引脚号 | 备注 |
|---|---|---|
| RESERVED | 18、43、55、113、114、116、117、128、137~139、141~144 | 保持悬空 |

## 2.6 评估板套件

移远通信提供评估板（UMTS&LTE EVB）及相关配件，用于模块的测试和使用。更多详细信息请参考文档 [4]。

---

# 3 工作特性

## 3.1 工作模式

**表 7：工作模式**

| 模式 | 功能 |
|---|---|
| 全功能模式（Idle） | 软件正常运行。模块注册上网络，能够接收和发送数据。 |
| 全功能模式（Voice/Data） | 网络连接正常。此模式下模块功耗取决于网络设置和数据传输速率。 |
| 最少功能模式 | `AT+CFUN=0` 可将模块设置成最少功能模式。此模式下射频和 (U)SIM 卡不工作。 |
| 飞行模式 | `AT+CFUN=4` 可将模块设置成飞行模式。此模式下射频不工作。 |
| 睡眠模式 | 此模式下模块功耗降到非常低，但仍可接收寻呼、短信、电话和 TCP/UDP 数据。 |
| 关机模式 | PMU 停止给基带和射频供电，软件停止工作，串口不通；但 VBAT_RF、VBAT_BB 仍然通电。 |

## 3.2 休眠模式

在休眠模式下，模块可将功耗降低到非常低的水平。详见文档 [5]。

**图 3：休眠模式下模块耗流示意图**
- 横轴 Run Time，纵轴 Current（电流）。
- 在 DRX 周期内，模块在 ON/OFF 之间交替：ON 期间出现电流脉冲（接收寻呼），OFF 期间电流降到极低基线。脉冲序列：DRX OFF→ON→OFF→ON→…
- 备注：DRX 周期值由基站通过无线网络发送。

对于车载应用，推荐使用特定 GPIO 实现模块与外部 MCU 间的休眠唤醒功能。

**图 4：休眠唤醒功能实现示意图**

```
Module                          Customer MCU
  WAKEUP_IN    ◄──────────────  GPIO
  SLEEP_SYS_IND ─────────────►  GPIO（INTn）
  GND          ──────────────   GND
```
- WAKEUP_IN 由 MCU 的 GPIO 驱动（输入到模块）；SLEEP_SYS_IND 输出到 MCU 的中断 GPIO；两者共地。

**表 8：休眠唤醒相关 GPIO 配置**

| 引脚名 | 引脚号 | 默认功能 | 描述 |
|---|---|---|---|
| GPIO1 | 1 | WAKEUP_IN | 低电平：外部 MCU 通知模块进入休眠状态；高电平：外部 MCU 通知模块退出休眠状态 |
| GPIO2 | 2 | SLEEP_SYS_IND | 低电平：模块处于休眠状态；高电平：模块处于唤醒状态 |

> 备注：若主机接口不是 1.8 V 电压域，请注意模块和外部 MCU 之间连接信号的电平匹配问题。详情参考文档 [5]。

## 3.3 飞行模式

进入飞行模式时，射频功能不可使用，且所有与射频相关的 AT 命令均不可访问。可通过 `AT+CFUN=<fun>` 或 API 命令进入：

- `AT+CFUN=0`：最少功能模式（关闭射频和 (U)SIM 卡）
- `AT+CFUN=1`：全功能模式（默认）
- `AT+CFUN=4`：飞行模式（关闭射频功能）

API 命令信息参考文档 [6]。

## 3.4 电源设计

### 3.4.1 电源接口

模块有 4 个 VBAT 电源引脚，分为两个电压域：
- 2 个 **VBAT_RF** 引脚用于给模块射频供电。
- 2 个 **VBAT_BB** 引脚用于给模块基带供电。

**表 9：电源接口引脚定义**

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| VBAT_BB | 59、60 | PI | 模块基带电源 | 外部电源必须能够提供达 0.8 A 的电流 |
| VBAT_RF | 57、58 | PI | 模块射频电源 | 外部电源必须能够提供达 2.0 A 的电流 |
| VDD_EXT | 7 | PO | 外部电路 1.8 V 供电 | 可为外部 GPIO 提供上拉电源；建议预留测试点；不用则悬空 |

### 3.4.2 供电参考

电源设计对模块性能至关重要。模块必须选择至少能提供 **3 A** 电流能力的电源。若输入电压与模块供电电压差不大，建议选 LDO；若压差较大，建议用开关电源转换器。

**图 5：供电输入参考电路**（+5 V 供电，采用 TI DC-DC `TPS62130A`，典型输出 3.8 V，负载电流峰值 3.0 A）

转写（TPS62130A 外围）：
- 输入 `DC_IN` → PVIN（pin 11/12）、AVIN（pin 10）；输入端去耦 `10 µF` + `0.1 µF`。
- `VBAT_EN` 经 `1M` 电阻连 EN（pin 13）。
- 开关 SW（pin 1/2/3）经 `2.2 µH` 电感输出 `VBAT`；反馈分压 `750K` / `100K`；输出电容 `22 µF` + `100 nF`。
- FB（pin 5）、AGND（pin 6）、FSW（pin 7，经 `200K` 设频率）、PG（pin 4）、DET（pin 8）。
- SS/TR 经 `3.3 nF` 软启动电容；PGND（pin 15/16）、VOS（pin 14）、GND（pin 9）接地。

> 备注：具体参考电路请参考文档 [7]。

### 3.4.3 电压稳定性要求

模块供电范围 3.4~4.3 V，需确保输入电压不低于 3.4 V。

**图 6：突发传输电源要求**
- 突发发射（Burst Transmission）时 VBAT 出现电压跌落（Drop）和纹波（Ripple）；需控制跌落与纹波在规格内。

为减少电压跌落，需使用低 ESR（ESR = 0.7 Ω）的 100 µF 滤波电容；建议分别给 VBAT_BB 和 VBAT_RF 预留 3 个（100 nF、33 pF、10 pF）具有最佳 ESR 的片式多层陶瓷电容（MLCC），靠近 VBAT 引脚放置。外部供电连接模块时，VBAT_BB 与 VBAT_RF 需采用星型走线：VBAT_BB 走线宽度 ≥ 1 mm，VBAT_RF 走线宽度 ≥ 2 mm。原则上 VBAT 走线越长，线宽越宽。

为保证电源稳定，建议在电源前端加大功率 TVS 管（如 WS4.5DPVL-AT，VRWM = 4.5 V，Vc = 12 V @ Ipp = 180 A）。

**图 7：模块供电电路**

转写：
- `VBAT` → 磁珠 `FB1`（BLM21PG600SH1D）→ `VBAT_RF`；→ 磁珠 `FB2`（BLM21PG600SH1D）→ `VBAT_BB`。
- TVS `D1` 对地。
- VBAT_RF 支路电容：C1 `100 µF` + C2 `100 nF` + C3 `33 pF` + C4 `10 pF`。
- VBAT_BB 支路电容：C5 `100 µF` + C6 `100 nF` + C7 `33 pF` + C8 `10 pF`。
- 电容靠近模块（Module）放置。

## 3.5 开机

### 3.5.1 PWRKEY 开机

**表 10：PWRKEY 接口引脚定义**

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| PWRKEY | 21 | DI | 模块开/关机 | 低电平有效 |

当模块处于关机模式时，可通过拉低 PWRKEY 至少 **500 ms** 使模块开机。推荐使用开集或开漏驱动电路控制 PWRKEY 引脚。

**图 8：开集驱动开机参考电路**
- Turn-on pulse（≥500 ms 低脉冲）经三极管开集驱动 PWRKEY；上拉/基极电阻 `4.7K`、`47K`；PWRKEY 处并 `100 nF` 电容。

**图 9：按键开机参考电路**
- 按键 `S1` 串 `R1 1K` 接 PWRKEY（Turn-on pulse）；按键附近放置 `TVS`（靠近 S1）用于 ESD 保护。

**图 10：开机时序图**

| 时序 | 信号/动作 | 时间 |
|---|---|---|
| T1 | VBAT 稳定后拉低 PWRKEY（VIL ≤ 0.3 × VBAT） | ≥ 500 ms |
| T2 | PWRKEY 拉低 → VDD_EXT 输出 | 约 17 ms |
| T4 | → RESET_N 动作 | 约 17 ms |
| T5 | → STATUS (OD) 拉低（指示运行） | ≥ 15 s |
| T6 | → UART 由 Inactive 转 Active | ≥ 10 s |
| T7 | → USB 由 Inactive 转 Active | ≥ 30 s |

> 备注：
> 1. 在拉低 PWRKEY 之前，需保证 VBAT 电压稳定，同时确保 PWRKEY ≤ 0.3 × VBAT。
> 2. 如果客户需要上电自动开机且不需要关机功能，可把 PWRKEY 直接下拉到地，下拉电阻建议 4.7 kΩ。

## 3.6 关机

### 3.6.1 PWRKEY 关机

模块在开机状态下，拉低 PWRKEY 引脚至少 **650 ms** 后释放，模块将执行关机流程。

**图 11：关机时序图**
- PWRKEY 拉低 650 ms（VIH = VBAT 释放后）→ Module Status：Running → Power-down procedure → OFF。

### 3.6.2 AT 命令关机

`AT+QPOWD` 命令可执行模块关机，过程等同拉低 PWRKEY 关机。详见文档 [2] 中 AT+QPOWD 命令。

> 备注：
> 1. 模块正常工作时不要立即切断电源，以避免损坏内部 Flash。强烈建议先通过 PWRKEY 或 AT 命令关机后再断电。
> 2. 使用 AT 命令关机时，请确保关机命令执行后 PWRKEY 一直处于高电平；否则模块完成关机后会自动再次开机。

## 3.7 复位

RESET_N 引脚可使模块复位。拉低 RESET_N 至少 **1 ms** 后释放可使模块复位。RESET_N 信号对干扰敏感，走线应尽量短并包地处理。

**表 11：复位接口引脚定义**

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| RESET_N | 20 | DI | 模块复位 | 1.8 V 电压域；建议预留测试点；开机后低电平有效 |

参考电路与 PWRKEY 控制电路类似，可用开集/开漏驱动电路或按钮控制。

**图 12：开集驱动复位参考电路**
- MCU `GPIO` 经三极管 `Q1`（基极 `4.7K`、`47K`）开集驱动 `RESET_N`（Reset pulse ≥1 ms）。

**图 13：RESET_N 按键复位参考电路**
- 按键 `S2` 直接控制 RESET_N，按键附近放 `TVS`（靠近 S2）。

**图 14：RESET_N 复位时序图**
- RESET_N 拉低 ≥ 1 ms（VIL ≤ 0.5 V）→ Module Status：Running → Baseband resetting → Baseband restart。

> 备注：
> 1. 确保 PWRKEY 和 RESET_N 引脚没有大负载电容，最大不超过 100 nF。
> 2. RESET_N 仅复位模块内部基带芯片，不复位电源管理芯片。
> 3. 复位功能建议仅在 AT+QPOWD 和 PWRKEY 关机失败后使用。

---

# 4 应用接口

## 4.1 USB 接口

USB 接口符合 USB 2.0 规范，支持高速（480 Mbps）、全速（12 Mbps）模式，仅支持 USB 从模式，用于 AT 命令、数据传输、软件调试、固件升级。

**表 12：USB 接口引脚定义**

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| USB_VBUS | 71 | AI | USB 检测 | 典型值 5.0 V；须预留测试点 |
| USB_DP | 69 | AIO | USB 差分数据（+） | 要求 90 Ω 差分阻抗；须预留测试点 |
| USB_DM | 70 | AIO | USB 差分数据（-） | 符合 USB 2.0 规范 |

> 更多 USB 2.0 规范信息见 http://www.usb.org/home 。

**图 15：USB 接口参考设计**
- Module 与 MCU 之间：USB_DM/USB_DP 经共模电感 `L1`（靠近模块）连到 MCU；预留 ESD Array。
- USB_VBUS 经 `R1`(NM_0R)、`R2`(NM_0R) 接 VDD（默认不贴）；预留测试点（Test Points），桩线（stubs）尽量短。

设计原则：
- USB 走线周围需包地，走 90 Ω 阻抗差分线。
- 不要在晶振、振荡器、磁性器件和 RF 信号下面走 USB 线；建议走内层差分且上下左右立体包地。
- USB 数据线 ESD 器件寄生电容应 ≤ 2 pF，并尽量靠近 USB 接口放置。

## 4.2 USB_BOOT 接口

如果在模块开机前将 USB_BOOT 上拉至 VDD_EXT，开机时模块将快速进入强制下载模式，可通过 USB 升级软件，节省时间。

**表 13：USB_BOOT 接口引脚定义**

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| USB_BOOT | 115 | DI | 强制模块进入紧急下载模式 | 1.8 V 电压域；高电平有效；建议预留测试点 |

**图 16：USB_BOOT 参考设计电路**
- `VDD_EXT` 经 `R1 4.7K` →（按键 `S1`）→ `R2 4.7K` → `USB_BOOT`；S1 靠近放置。

**图 17：进入强制下载时序**

| 时序 | 信号/动作 | 时间 |
|---|---|---|
| T1 | VBAT 上电稳定后拉低 PWRKEY（VIL 0.5 V） | ≥ 500 ms |
| T2 | → VDD_EXT 输出 | 约 17 ms |
| T3 | USB_BOOT 上拉有效（位于 VDD_EXT 输出窗口内） | - |
| T4 | → RESET_N 动作 | 约 17 ms |

> 备注：
> 1. 拉低 PWRKEY 前需保证 VBAT 稳定，建议 VBAT 上电到拉低 PWRKEY 的间隔 ≥ 30 ms。
> 2. 用 MCU 控制进入强制下载模式时按时序图控制，VBAT 上电前不建议上拉 USB_BOOT 到 1.8 V；手动方式按所示短接测试点即可。

## 4.3 (U)SIM 接口

模块提供 1 个 (U)SIM 接口，符合 ETSI 和 IMT-2000 规范，支持 1.8 V 和 3.0 V (U)SIM 卡并默认适配 eSIM。

**表 14：(U)SIM 接口引脚定义**

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| USIM_VDD | 14 | PO | (U)SIM 卡供电电源 | 模块自动识别 1.8 V 或 3.0 V (U)SIM |
| USIM_DATA | 15 | DIO | (U)SIM 卡数据 | 同上 |
| USIM_CLK | 16 | DO | (U)SIM 卡时钟 | 同上 |
| USIM_RST | 17 | DO | (U)SIM 卡复位 | 同上 |

**图 18：(U)SIM 接口参考电路图**（含 eSIM）
- USIM_VDD 经 `20K` 上拉，并联 `10 pF` + `100 nF` 去耦。
- USIM_RST / USIM_CLK / USIM_DATA 各串 `0R` 电阻（便于调试）后接 (U)SIM 卡座 / eSIM（VCC、RST、CLK、IO、GND、VPP）。
- USIM_DATA、USIM_CLK、USIM_RST 各并 `10 pF` 电容对地（滤除射频干扰）。

设计原则：
- (U)SIM 靠近模块摆放，信号线布线长度不超过 200 mm。
- (U)SIM 信号线远离 RF 线和 VCC 电源线。
- (U)SIM 卡座地与模块 USIM_GND 走线短而粗；USIM_VDD 与 USIM_GND 布线宽度 ≥ 0.5 mm。
- USIM_CLK 与 USIM_DATA 不能太靠近，两走线间增加地屏蔽。
- 建议在 (U)SIM 引脚增加 TVS 阵列（寄生电容 ≤ 15 pF），并在 USIM_DATA、USIM_CLK、USIM_RST 上并 33 pF 电容滤除射频干扰；外围器件尽量靠近卡座。
- USIM_DATA 上的上拉电阻有利于增加抗干扰能力（走线过长或有近距离干扰源时，建议靠近卡座增加上拉电阻）。

## 4.4 PCM 和 I2C 接口

模块提供 1 个 PCM 接口和 1 个 I2C 接口。PCM 支持短帧模式（模块做主设备）：

短帧模式下数据在 PCM_CLK 下降沿采样、上升沿发送；PCM_SYNC 下降沿代表高有效位；当 PCM_SYNC 达 8 kHz 时，PCM_CLK 支持 2048 kHz。模块支持 16 位线性编码格式。

**图 19：短帧模式时序图**（PCM_SYNC = 8 kHz、PCM_CLK = 2048 kHz）
- 一帧周期 125 µs（= 1/8 kHz），PCM_CLK 在帧内含 256 个时钟（编号 1…255、256）。
- PCM_SYNC 帧同步脉冲标记帧起始（高有效位 MSB）。
- PCM_OUT、PCM_IN 在 MSB 对齐处开始传输 16 位数据。

**表 15：PCM 接口引脚定义**

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| PCM_SYNC | 26 | DIO | PCM 帧同步 | 1.8 V 电压域；不用则悬空 |
| PCM_CLK | 27 | DIO | PCM 时钟 | 同上 |
| PCM_IN | 24 | DI | PCM 数据输入 | 1.8 V 电压域；不用则悬空 |
| PCM_OUT | 25 | DO | PCM 数据输出 | 同上 |

**表 16：I2C 接口引脚定义**

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| I2C_SCL | 41 | OD | I2C 串行时钟 | 需要外部 1.8 V 上拉 |
| I2C_SDA | 42 | OD | I2C 串行数据 | 不用则悬空 |

**图 20：PCM 和 I2C 接口电路参考设计**（带外部 Codec 芯片）
- Module 侧 PCM_CLK→BCLK、PCM_SYNC→LRCK、PCM_DOUT→DAC、PCM_DIN→ADC。
- I2C_SCL→SCL、I2C_SDA→SDA，SCL/SDA 各经 `4.7K` 上拉至 `1.8 V`。
- Codec 侧：MICBIAS、INP/INN（麦克风差分输入，BIAS）、LOUTP/LOUTN（喇叭差分输出）。

## 4.5 模拟音频接口

模块提供 1 路模拟音频输入通道和 1 路模拟音频输出通道。

**表 17：模拟音频接口引脚定义**

| 引脚名 | 引脚号 | I/O | 描述 |
|---|---|---|---|
| MICBIAS | 140 | PO | 麦克风偏置电压 |
| MIC_P | 125 | AI | 麦克风输入通道（+） |
| MIC_N | 126 | AI | 麦克风输入通道（-） |
| SPK_P | 124 | AO | 模拟音频差分输出通道（+） |
| SPK_N | 123 | AO | 模拟音频差分输出通道（-） |

## 4.6 串口

模块有两个串口：主串口和调试串口。
- **主串口**：支持 4800/9600/19200/38400/57600/115200/230400/460800/921600 bps 和 1 Mbps，默认 115200 bps，用于数据传输；支持 RTS/CTS 硬件流控。
- **调试串口**：支持 115200 bps，用于部分日志输出。

**表 18：主串口引脚定义**

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| MAIN_CTS | 64 | DO | DTE 清除发送（连接至 DTE 的 CTS） | 1.8 V 电压域；不用则悬空 |
| MAIN_RTS | 65 | DI | DTE 请求发送（连接至 DTE 的 RTS） | 同上 |
| MAIN_RXD | 68 | DO* | 主串口接收 | 同上 |
| MAIN_TXD | 67 | DO | 主串口发送 | 同上 |

> *注：表 18 原文将 MAIN_RXD 的 I/O 列标为 DO；按功能与表 6（DI）应为接收输入，此处保留原文记载。

**表 19：调试串口引脚定义**

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| DBG_RXD | 11 | DI | 调试串口接收 | 1.8 V 电压域；须预留测试点 |
| DBG_TXD | 12 | DO | 调试串口发送 | 同上 |

串口电平为 1.8 V。若客户主机系统电平为 3.3 V，需增加电平转换器。推荐使用 TI `TXS0104E`。

**图 21：电平转换芯片参考电路**（TXS0104E）
- VDD_EXT→VCCA（去耦 `C1 0.1 µF`），VDD_MCU→VCCB（去耦 `C2 0.1 µF`）。
- A1↔B1：MAIN_TXD↔RXD_MCU；A2↔B2：MAIN_RXD↔TXD_MCU；A3↔B3：MAIN_CTS↔CTS_MCU；A4↔B4：MAIN_RTS↔RTS_MCU。
- OE 经 `R1 10k` 上拉至 VDD_EXT、`R2 120k`；GND 接地；NC 悬空。

**图 22：三极管电平转换参考电路**
- Module 侧 MAIN_RXD/MAIN_TXD 经三极管电平转换接 MCU_TXD/MCU_RXD；上拉 `4.7K`@VDD_EXT/VDD_MCU、`10K`、滤波 `1 nF`。
- 硬件流控 MAIN_RTS/MAIN_CTS 经另一组三极管转换接 MCU_RTS/MCU_CTS；共地。

> 备注：
> 1. 三极管电平转换电路不适用于波特率超过 460 kbps 的应用。
> 2. 串口硬件流控 CTS、RTS 采用直连方式：模块 CTS 连 MCU 的 CTS，模块 RTS 连 MCU 的 RTS。

## 4.7 SDIO 接口

模块提供 2 路支持 SD 3.0 协议的 SDIO 接口。

### 4.7.1 SD_SDIO 接口

SD_SDIO 接口支持 1.8 V eMMC。

**表 20：SD_SDIO 接口引脚定义**

| 引脚名称 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| VDD_SDIO | 34 | PO | SD 卡 SDIO 总线上拉电源 | 使用 eMMC 时不做 SDIO 上拉电源；不用则悬空 |
| SD_SDIO_DATA3 | 28 | DO | SDIO 总线 DATA3 | eMMC 应用中电压域为 1.8 V；不用则悬空 |
| SD_SDIO_DATA2 | 29 | DIO | SDIO 总线 DATA2 | 同上 |
| SD_SDIO_DATA1 | 30 | DIO | SDIO 总线 DATA1 | 同上 |
| SD_SDIO_DATA0 | 31 | DIO | SDIO 总线 DATA0 | 同上 |
| SD_SDIO_CMD | 33 | DIO | SDIO 总线命令 | 同上 |
| SD_SDIO_CLK | 32 | DIO | SD 卡 SDIO 总线时钟 | 同上 |

#### 4.7.1.1 eMMC 应用参考设计

**图 23：eMMC 应用参考设计**

转写：
- Module 侧 6 根信号经 `0 Ω` 串联电阻 R1~R6 连到 eMMC：
  - EMMC_SDIO_DATA3→DAT3、DATA2→DAT2、DATA1→DAT1、DATA0→DAT0、CLK→CLK、CMD→CMD（各串 R1~R6 = 0R）。
- 上拉电阻 R7~R10（默认不贴 `NM_47K`）上拉到模块 `VDD_1V8`。
- eMMC 侧：VCCQ 由 VDD_1V8 供电（含 `R11 10K`、`R12 47K`，去耦 `C8 100nF` + `C9 22 µF`）；VDD1 去耦 `C12 100nF` + `C13 1uF`；RST 由 VDD_3V3 域；C10 `100nF` + C11 `1uF`。
- 预留电容 C1~C7（默认不贴 NM）位于信号线对地。

eMMC 电路设计原则：
- 为避免总线抖动，预留上拉电阻 R7~R10（10 kΩ~100 kΩ，推荐 47 kΩ），上拉到模块 VDD_SDIO 引脚（默认不贴）。
- 预留串联电阻 R1~R6（推荐 0 Ω）；预留电容 C1~C6（默认不贴）；电阻电容靠近模块侧放置。
- EMMC_SDIO 信号走线：立体包地，阻抗 50 Ω ±10 %；远离射频、模拟、时钟、DCDC 等噪声信号；CLK 与 DATA[0:3]/CMD 等长处理（相差 < 1 mm），总长 < 70 mm（模块内部走线 40 mm，外部走线需 < 30 mm）。
- SDIO 信号与其他信号间距 > 2 倍线宽，总线负载 < 40 pF。

### 4.7.2 WLAN_SDIO 接口

模块提供 1 路低功耗 SDIO 3.0 WLAN 和 1 路控制接口。

**表 21：WLAN 接口引脚定义**

| 引脚名 | 引脚号 | I/O | 描述 |
|---|---|---|---|
| WLAN_PWR_EN | 127 | DO | WLAN 电源使能控制 |
| WLAN_SDIO_DATA3 | 129 | DIO | WLAN SDIO 数据位 3 |
| WLAN_SDIO_DATA2 | 130 | DIO | WLAN SDIO 数据位 2 |
| WLAN_SDIO_DATA1 | 131 | DIO | WLAN SDIO 数据位 1 |
| WLAN_SDIO_DATA0 | 132 | DIO | WLAN SDIO 数据位 0 |
| WLAN_SDIO_CLK | 133 | DO | WLAN SDIO 时钟 |
| WLAN_SDIO_CMD | 134 | DO | WLAN SDIO 命令 |
| WLAN_WAKE | 135 | DI | 通过外部 Wi-Fi 模块唤醒主机（模块） |
| WLAN_EN | 136 | DO | WLAN 使能控制 |

SDIO 设计原则：
- WLAN_SDIO 信号线立体包地，阻抗 50 Ω ±10 %；远离射频、模拟、时钟、DC-DC 等噪声信号。
- CLK 与 DATA[0:3]/CMD 等长（相差 < 1 mm），总长 < 70 mm（模块内部走线 30 mm，外部走线需 < 40 mm）。
- WLAN_SDIO_CLK 信号线上靠近模块放置 15~24 Ω 终端匹配电阻，CLK 引脚到电阻走线 < 5 mm。
- WLAN_SDIO 信号与其他信号间距 > 2 倍线宽，总线负载 < 15 pF。

**图 24：WLAN 接口连接参考设计图**（以 FC30R Wi-Fi 为例）
- POWER：WLAN_PWR_EN → DCDC/LDO → VDD_3V3、VDD1V8_VIO。
- 数据：WLAN_SDIO_DATA3/2/1/0 → SDIO_D3/D2/D1/D0；WLAN_SDIO_CLK 经 `15~24R` → SDIO_CLK；WLAN_SDIO_CMD → SDIO_CMD（器件靠近模块放置）。
- 控制：WLAN_EN → WLAN_EN；WAKE_WLAN → WAKE_ON_WIRELESS。

## 4.8 ADC 接口

模块支持两路通用模数转换接口。为提高测量准确度，建议 ADC 布线包地处理。

**表 22：ADC 接口引脚定义**

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| ADC0 | 45 | AI | 通用 ADC 接口 | 不用则悬空 |
| ADC1 | 44 | AI | 通用 ADC 接口 | 不用则悬空 |

使用 `AT+QADC=<port>` 或 API 读取 ADC 电压值：`AT+QADC=0` 读 ADC0；`AT+QADC=1` 读 ADC1。AT 命令参考文档（手册引用源），API 参考文档 [6]。ADC 分辨率最高 12 位。

**表 23：ADC 特性**

| 名称 | 最小值 | 典型值 | 最大值 | 单位 |
|---|---|---|---|---|
| ADC0 电压范围 | 0 | - | 3.8 | V |
| ADC1 电压范围 | 0 | - | 3.8 | V |
| ADC 分辨率 | - | 12 | - | bits |

> 备注：
> 1. 每个 ADC 引脚输入电压不能超过其各自相应的电压范围。
> 2. 模块在 VBAT 不供电时，ADC 接口不能直连输入电压。
> 3. ADC 内部是分压电阻，外部 ADC 接口连接上拉电阻不能超过 30 kΩ。

## 4.9 SPI 接口

模块支持一路 SPI，支持主模式，最大时钟频率 26 MHz。

**表 24：SPI 接口引脚定义**

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| SPI_CLK | 40 | DO | SPI 时钟 | 1.8 V 电压域；不用则悬空 |
| SPI_CS | 37 | DO | SPI 片选 | 同上 |
| SPI_DIN | 39 | DI | SPI 数据输入 | 同上 |
| SPI_DOUT | 38 | DO | SPI 数据输出 | 同上 |

**图 25：SPI 接口时序**
- 时钟周期 `T`；SPI_CS 拉低有效期间，SPI_CLK 输出（编号 1、2、3、4…）；SPI_DOUT 在时钟边沿输出数据，SPI_DIN 在时钟边沿采样输入数据。

SPI 电压域为 1.8 V。若主机为 3.3 V，需增加电平转换器。

**图 26：SPI 电平转换参考电路（电平转换芯片）**
- VDD_EXT→VCCA（去耦 `0.1 µF`），VDD_MCU→VCCB（去耦 `0.1 µF`）。
- A1↔B1：SPI_CS↔SPI_CS_N_MCU；A2↔B2：SPI_CLK↔SPI_CLK_MCU；A3↔B3：SPI_DOUT↔SPI_DIN_MCU；A4↔B4：SPI_DIN↔SPI_DOUT_MCU；OE、GND、NC。

> 备注：模块 SPI 接口电平 1.8 V，与 3.3 V 系统连接时需增加支持 26 MHz 通信的电平转换电路。

## 4.10 指示信号

**表 25：指示接口引脚定义**

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| STATUS | 61 | OD | 运行状态指示 | 需要外部上拉；不用则悬空 |

STATUS 是开漏输出引脚，用于指示工作状态，可连接带上拉的 GPIO 或 LED 指示电路。

**图 27：STATUS 参考电路**（两种，任选其一）
- 方案一：STATUS → `10K` 上拉至 `VDD_MCU`，并连到 `MCU_GPIO`。
- 方案二：STATUS → `2.2K` 上拉至 `VBAT`，串接 LED（点亮指示）。

---

# 5 射频特性

## 5.1 蜂窝网络

### 5.1.1 天线接口和工作频段

模块设计有 1 个主天线接口、1 个分集天线接口、1 个 GNSS 天线接口。天线端口阻抗 50 Ω。

**表 26：蜂窝网络天线接口引脚定义**

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| ANT_DRX | 35 | AI | 分集天线接口 | 50 Ω 特性阻抗 |
| ANT_MAIN | 49 | AIO | 主天线接口 | 50 Ω 特性阻抗 |

> 备注：仅支持无源天线。

**表 27：模块工作频段**

| 工作频段 | 发送（MHz） | 接收（MHz） |
|---|---|---|
| EGSM900 | 880~915 | 925~960 |
| DCS1800 | 1710~1785 | 1805~1880 |
| WCDMA B1 | 1920~1980 | 2110~2170 |
| WCDMA B5 | 824~849 | 869~894 |
| WCDMA B8 | 880~915 | 925~960 |
| LTE-FDD B1 | 1920~1980 | 2110~2170 |
| LTE-FDD B3 | 1710~1785 | 1805~1880 |
| LTE-FDD B5 | 824~849 | 869~894 |
| LTE-FDD B8 | 880~915 | 925~960 |
| LTE-TDD B34 | 2010~2025 | 2010~2025 |
| LTE-TDD B38 | 2570~2620 | 2570~2620 |
| LTE-TDD B39 | 1880~1920 | 1880~1920 |
| LTE-TDD B40 | 2300~2400 | 2300~2400 |
| LTE-TDD B41 | 2535~2655 | 2535~2655 |

### 5.1.2 发射功率

**表 28：模块射频发射功率**

| 频段 | 发射功率最大值 | 发射功率最小值 |
|---|---|---|
| EGSM900 MHz | 33 dBm ±2 dB | 5 dBm ±5 dB |
| DCS1800 MHz | 30 dBm ±2 dB | 0 dBm ±5 dB |
| GSM900 (8-PSK) | 27 dBm ±3 dB | 5 dBm ±5 dB |
| DCS1800 (8-PSK) | 26 dBm ±3 dB | 0 dBm ±5 dB |
| WCDMA B1/B5/B8 | 24 dBm +1/-3 dB | < -49 dBm |
| LTE-FDD B1/B3/B5/B8 | 23 dBm ±2 dB | < -39 dBm |
| LTE-TDD B34/B38/B39/B40/B41 | 23 dBm ±2 dB | < -39 dBm |

> 备注：在 GPRS 网络 4 时隙发送模式下，最大输出功率减小 2.5 dB。该设计符合 3GPP TS51.010-1 第 13.16 章节所述 GSM 规范。

### 5.1.3 接收灵敏度

**表 29：射频接收灵敏度**（单位 dBm）

| 频段 | SIMO 最小值 | SIMO 典型值 | SIMO 最大值 | 3GPP SIMO 最小值 |
|---|---|---|---|---|
| EGSM900 | -109 | -108 | -107 | -109 |
| DCS1800 | -108 | -107.5 | -107 | -108 |
| WCDMA B1 | -109.5 | -109 | -108.5 | -109.5 |
| WCDMA B5 | -110 | -109 | -108 | -110 |
| WCDMA B8 | -111 | -110.5 | -110 | -111 |
| LTE-FDD B1 | -101.5 | -101 | -105.5 | -101.5 |
| LTE-FDD B3 | -101 | -100.5 | -100 | -101 |
| LTE-FDD B5 | -103 | -102 | -101 | -103 |
| LTE-FDD B8 | -103 | -102.5 | -102 | -103 |
| LTE-TDD B34 | -102.5 | -102 | -102 | -102.5 |
| LTE-TDD B38 | -100 | -99.5 | -99 | -100 |
| LTE-TDD B39 | -101 | -100.5 | -100 | -101 |
| LTE-TDD B40 | -100.5 | -100 | -99.5 | -100.5 |
| LTE-TDD B41 | -100 | -99.5 | -99 | -100 |

### 5.1.4 参考设计

模块提供 2 路射频天线接口。为获取更佳射频性能，需预留 π 型匹配电路；匹配元件（C1/R1/C2）尽量靠近天线放置；电容默认不贴；天线接口 ESD 防护器件结电容建议 ≤ 0.05 pF。

**图 28：射频参考设计**
- ANT_MAIN → 串 `R1 0R` → Main antenna；π 型预留电容 `C1`(NM) / `C2`(NM) 对地；预留 `ESD`。
- ANT_DRX → 串 `R2 0R` → Diversity antenna；π 型预留电容 `C3`(NM) / `C4`(NM) 对地；预留 `ESD`。

## 5.2 射频信号线布线指导

用户 PCB 上所有射频信号线特性阻抗应控制在 50 Ω。阻抗由材料介电常数、走线宽度（W）、对地间隙（S）、参考地平面高度（H）决定。PCB 特性阻抗控制通常采用**微带线**与**共面波导**两种方式。

**图 29：两层 PCB 板微带线结构**
- 叠层（自上而下）：TOP（信号走线，宽度 W）— PREPREG（介质，厚度 H）— BOTTOM（完整参考地）。
- 微带线：信号走线在表层，参考地在底层；H 为介质厚度。

**图 30：两层 PCB 板共面波导结构**
- 叠层：TOP（信号走线宽度 W，两侧各留间隙 S 与同层地共面）— PREPREG（厚度 H）— BOTTOM（参考地）。
- 共面波导：信号两侧 S 间隙 + 底层参考地，W 为线宽。

**图 31：四层 PCB 板共面波导结构（参考地为第三层）**
- 叠层：TOP（S W S）— PREPREG — Layer2 — Layer3（参考地）— BOTTOM。
- H 为 TOP 到 Layer3（参考地）的距离；信号两侧间隙 S、外侧地 2W。

**图 32：四层 PCB 板共面波导结构（参考地为第四层）**
- 叠层：TOP（S W S）— PREPREG — Layer2 — Layer3 — BOTTOM（参考地）。
- H 为 TOP 到 BOTTOM（参考地）的距离；信号两侧间隙 S、外侧地 2W。

射频电路设计原则：
- 使用阻抗模拟计算工具对射频信号线进行精确 50 Ω 阻抗控制。
- 与射频引脚相邻的 GND 引脚不做热焊盘，要与地充分接触。
- 射频引脚到 RF 连接器距离尽量短；避免直角走线，建议走线夹角 135°。
- 连接器件封装建立时注意信号脚离地保持一定距离。
- 射频信号线参考地平面应完整；信号线和参考地周边增加地孔提升性能；地孔与信号线距离 ≥ 2 倍线宽（2×W）。
- 射频信号线必须远离干扰源，避免与相邻层任何信号线交叉或平行。
- 更多射频 layout 说明参考文档 [3]。

## 5.3 GNSS 天线接口

**表 30：GNSS 天线引脚定义**

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| ANT_GNSS | 47 | AI | GNSS 天线接口 | 50 Ω 阻抗特性；不用则悬空 |

**表 31：GNSS 频率**

| 类型 | 频率 | 单位 |
|---|---|---|
| GPS | 1575.42 ±1.023 | MHz |
| GLONASS | 1597.5~1605.8 | MHz |
| Galileo | 1575.42 ±2.046 | MHz |
| BeiDou | 1561.098 ±2.046 | MHz |
| QZSS | 1575.42 | MHz |

**表 32：GNSS 性能**

| 参数 | 条件 | 典型值 | 单位 |
|---|---|---|---|
| 灵敏度 - 捕获 | 自主模式 | -148 | dBm |
| 灵敏度 - 重捕 | 自主模式 | -160 | dBm |
| 灵敏度 - 追踪 | 自主模式 | -163 | dBm |
| 首次定位时间 - 冷启动 @ 净空环境 | - | 35.2 | s |
| 首次定位时间 - 温启动 @ 净空环境 | 自主模式 | 24.6 | s |
| 首次定位时间 - 热启动 @ 净空环境 | 自主模式 | 1.5 | s |
| 定位精度 - CEP-50 | 自主模式 @ 净空环境 | 2 | m |

> 备注：在极限环境下，测试结果会有所恶化。

**图 33：GNSS 天线参考电路**
- ANT_GNSS → 串 `0R` → GNSS 天线；π 型预留 `NM` 电容、`100 pF`、`47nH` 电感、`10R`，并经 `0.1 µF` 去耦至 `VDD`（有源天线供电）；预留 `ESD`。

> 备注：
> 1. 客户可根据有源天线类型选用外部 LDO 供电。
> 2. 若选用无源天线，则无需设计 VDD 电路。
> 3. 天线接口 ESD 防护器件结电容建议 ≤ 0.05 pF。

## 5.4 天线设计要求

**表 33：天线设计要求**

| 天线类型 | 要求 |
|---|---|
| GNSS | 频率范围 1559~1609 MHz；极化 RHCP 或线性；VSWR < 2（典型值）；无源天线增益 > 0 dBi；有源天线噪声系数 < 1.5 dB；有源天线增益 > 0 dBi；有源天线内嵌 LNA 增益 17 dB~45 dB |
| GSM/CDMA/UMTS/TD-SCDMA/LTE | VSWR ≤ 2；效率 > 30 %；增益 1 dBi；最大输入功率 50 W；输入阻抗 50 Ω；极化垂直方向；线缆插入损耗：LB(<1 GHz) < 1 dB、MB(1~2.3 GHz) < 1.5 dB、HB(>2.3 GHz) < 2 dB |

## 5.5 射频连接器推荐

如使用射频连接器，推荐使用 Rosenberger 的四合一 **HFM 连接器**（具体选型按自身天线选择）。

**图 34：HFM 连接器**
- Rosenberger 四合一 HFM 连接器外形图。详情见 https://www.rosenbergerap.com 。

如使用射频连接器，亦推荐使用 Hirose 的 **U.FL-R-SMT 天线座**。

**图 35：天线座尺寸（单位：毫米，U.FL-R-SMT）**
- 俯视/侧视主要尺寸：中心孔 ø2；高度方向 1.25 / 0.35；座体 2.6 / 2.8 / 0.6；外形 [3.1]、1.8；焊盘开口 2.0 Max（N×4）。
- 推荐焊盘图（右图）：4±0.05 × 2.2±0.05 外框；SIG 焊盘 1.9±0.05 宽、1.05±0.05；两侧 GND，间距 1±0.05；中部信号区标注"No conductive traces in this area"（此区域无导电走线）。

可选择 U.FL-LP 系列插头与 U.FL-R-SMT 配合使用。

**图 36：与天线座匹配的插头规格**

| Part No. | U.FL-LP-040 | U.FL-LP-066 | U.FL-LP(V)-040 | U.FL-LP-062 | U.FL-LP-088 |
|---|---|---|---|---|---|
| Mated Height | 2.5 mm Max.<br>(2.4 mm Nom.) | 2.5 mm Max.<br>(2.4 mm Nom.) | 2.0 mm Max.<br>(1.9 mm Nom.) | 2.4 mm Max.<br>(2.3 mm Nom.) | 2.4 mm Max.<br>(2.3 mm Nom.) |
| Applicable cable | Dia. 0.81 mm 同轴线 | Dia. 1.13 mm 和 Dia. 1.32 mm 同轴线 | Dia. 0.81 mm 同轴线 | Dia. 1.1 mm 同轴线 | Dia. 1.37 mm 同轴线 |
| Weight (mg) | 53.7 | 59.1 | 34.8 | 45.5 | 71.7 |
| RoHS | YES | YES | YES | YES | YES |

**图 37：射频连接器安装图（单位：毫米）**
- 各 U.FL-LP 插头（Plug）与 U.FL-R-SMT-1 天线座（Receptacle）的配合安装，对应电缆直径：
  - U.FL-LP-040 + U.FL-R-SMT-1：Cable Dia. 0.81
  - U.FL-LP(V)-040 + U.FL-R-SMT-1：Cable Dia. 0.81
  - U.FL-LP-066 + U.FL-R-SMT-1：Cable Dia. 1.32 / Dia. 1.13
  - U.FL-LP-062 + U.FL-R-SMT-1：Cable Dia. 1.00
  - U.FL-LP-088 + U.FL-R-SMT-1：Cable Dia. 1.37

---

# 6 电气性能和可靠性

## 6.1 绝对最大额定值

**表 34：绝对最大额定值**

| 参数 | 最小值 | 最大值 | 单位 |
|---|---|---|---|
| VBAT_RF / VBAT_BB | -0.3 | 4.7 | V |
| USB_VBUS | -0.3 | 5.5 | V |
| VBAT_BB 最大电流 | 0 | 0.8 | A |
| VBAT_RF 最大电流 | 0 | 2.0 | A |
| 数字接口电压 | -0.3 | 2.0 | V |
| ADC0 电压 | 0 | 3.8 | V |
| ADC1 电压 | 0 | 3.8 | V |

## 6.2 电源额定值

**表 35：模块电源额定值**

| 参数 | 描述 | 条件 | 最小值 | 典型值 | 最大值 | 单位 |
|---|---|---|---|---|---|---|
| VBAT | VBAT_BB 和 VBAT_RF | 实际输入电压必须在该范围之内 | 3.4 | 3.8 | 4.3 | V |
| VBAT | 突发发射时的电压跌落 | 最大发射功率等级时 | 0 | 0 | 400 | mV |
| VBAT_RIPPLE | 电源纹波 | 实际输入电压纹波必须在要求范围内 | - | - | 50 | mV |
| IVBAT | 峰值电流 | 最大发射功率等级时 | 0 | 1.8 | 2.0 | A |
| USB_VBUS | USB 检测 | - | 4.0 | 5.0 | 5.25 | V |

## 6.3 功耗

**表 36：模块功耗（EC200A-CN(TA) QuecOpen）**

| 描述 | 条件 | 典型值 | 单位 |
|---|---|---|---|
| 关机模式 | 模块关机 | 170 | uA |
| 睡眠模式 | AT+CFUN=0 (USB disconnected) | 2 | mA |
| 睡眠模式 | EGSM900 @ DRX=2 (USB disconnected) | 4.68 | mA |
| 睡眠模式 | EGSM900 @ DRX=5 (USB disconnected) | 4.35 | mA |
| 睡眠模式 | EGSM900 @ DRX=5 (USB suspend) | 4.5 | mA |
| 睡眠模式 | EGSM900 @ DRX=9 (USB disconnected) | 4.25 | mA |
| 睡眠模式 | DCS1800 @ DRX=2 (USB disconnected) | 4.63 | mA |
| 睡眠模式 | DCS1800 @ DRX=5 (USB disconnected) | 4.33 | mA |
| 睡眠模式 | DCS1800 @ DRX=5 (USB suspend) | 4.44 | mA |
| 睡眠模式 | DCS1800 @ DRX=9 (USB disconnected) | 4.25 | mA |
| 睡眠模式 | WCDMA @ PF=64 (USB disconnected) | 5.28 | mA |
| 睡眠模式 | WCDMA @ PF=64 (USB suspend) | 5.35 | mA |
| 睡眠模式 | WCDMA @ PF=128 (USB disconnected) | 4.67 | mA |
| 睡眠模式 | WCDMA @ PF=256 (USB disconnected) | 4.35 | mA |
| 睡眠模式 | WCDMA @ PF=512 (USB disconnected) | 4.2 | mA |
| 睡眠模式 | LTE-FDD @ PF=32 (USB disconnected) | 5.02 | mA |
| 睡眠模式 | LTE-FDD @ PF=64 (USB disconnected) | 4.48 | mA |
| 睡眠模式 | LTE-FDD @ PF=64 (USB suspend) | 4.26 | mA |
| 睡眠模式 | LTE-FDD @ PF=128 (USB disconnected) | 1.19 | mA |
| 睡眠模式 | LTE-FDD @ PF=256 (USB disconnected) | 4.13 | mA |
| 睡眠模式 | LTE-TDD @ PF=32 (USB disconnected) | 5.05 | mA |
| 睡眠模式 | LTE-TDD @ PF=64 (USB disconnected) | 4.5 | mA |
| 睡眠模式 | LTE-TDD @ PF=64 (USB suspend) | 4.6 | mA |
| 睡眠模式 | LTE-TDD @ PF=128 (USB disconnected) | 4.26 | mA |
| 睡眠模式 | LTE-TDD @ PF=256 (USB disconnected) | 4.14 | mA |
| 空闲模式 | EGSM900 @ DRX=5 (USB disconnected) | 15 | mA |
| 空闲模式 | EGSM900 @ DRX=5 (USB connected) | 41 | mA |
| 空闲模式 | WCDMA @ PF=64 (USB disconnected) | 17 | mA |
| 空闲模式 | WCDMA @ PF=64 (USB connected) | 42 | mA |
| 空闲模式 | LTE-FDD @ PF=64 (USB disconnected) | 16 | mA |
| 空闲模式 | LTE-FDD @ PF=64 (USB connected) | 41 | mA |
| 空闲模式 | LTE-TDD @ PF=64 (USB disconnected) | 16 | mA |
| 空闲模式 | LTE-TDD @ PF=64 (USB connected) | 41 | mA |
| GPRS 数据传送 | EGSM900 4DL/1UL @ 33 dBm | 217 | mA |
| GPRS 数据传送 | EGSM900 3DL/2UL @ 33 dBm | 423 | mA |
| GPRS 数据传送 | EGSM900 2DL/3UL @ 31.8 dBm | 515 | mA |
| GPRS 数据传送 | EGSM900 1DL/4UL @ 29.8 dBm | 550 | mA |
| GPRS 数据传送 | DCS1800 4DL/1UL @ 29.8 dBm | 170 | mA |
| GPRS 数据传送 | DCS1800 3DL/2UL @ 29.58 dBm | 300 | mA |
| GPRS 数据传送 | DCS1800 2DL/3UL @ 29 dBm | 354 | mA |
| GPRS 数据传送 | DCS1800 1DL/4UL @ 27.5 dBm | 330 | mA |
| EDGE 数据传送 | EGSM900 4DL/1UL @ 28 dBm | 145 | mA |
| EDGE 数据传送 | EGSM900 3DL/2UL @ 26.2 dBm | 243.3 | mA |
| EDGE 数据传送 | EGSM900 2DL/3UL @ 24.5 dBm | 305 | mA |
| EDGE 数据传送 | EGSM900 1DL/4UL @ 23 dBm | 285 | mA |
| EDGE 数据传送 | DCS1800 4DL/1UL @ 27.5 dBm | 118 | mA |
| EDGE 数据传送 | DCS1800 3DL/2UL @ 26.1 dBm | 188.6 | mA |
| EDGE 数据传送 | DCS1800 2DL/3UL @ 24.7 dBm | 239 | mA |
| EDGE 数据传送 | DCS1800 1DL/4UL @ 23.2 dBm | 282 | mA |
| WCDMA 数据传送 | WCDMA B1 HSDPA @ 22.7 dBm | 493 | mA |
| WCDMA 数据传送 | WCDMA B5 HSDPA @ 22.8 dBm | 480 | mA |
| WCDMA 数据传送 | WCDMA B8 HSDPA @ 22.7 dBm | 465 | mA |
| WCDMA 数据传送 | WCDMA B1 HSUPA @ 22.7 dBm | 504 | mA |
| WCDMA 数据传送 | WCDMA B5 HSUPA @ 22.8 dBm | 492 | mA |
| WCDMA 数据传送 | WCDMA B8 HSUPA @ 22.8 dBm | 477 | mA |
| LTE 数据传送 | LTE-FDD B1 @ 23 dBm | 627 | mA |
| LTE 数据传送 | LTE-FDD B3 @ 23 dBm | 592 | mA |
| LTE 数据传送 | LTE-FDD B5 @ 23 dBm | 568 | mA |
| LTE 数据传送 | LTE-FDD B8 @ 22.6 dBm | 550 | mA |
| LTE 数据传送 | LTE-TDD B34 @ 23.5 dBm | 291 | mA |
| LTE 数据传送 | LTE-TDD B38 @ 22.7 dBm | 339 | mA |
| LTE 数据传送 | LTE-TDD B39 @ 22.7 dBm | 245 | mA |
| LTE 数据传送 | LTE-TDD B40 @ 22.8 dBm | 330 | mA |
| LTE 数据传送 | LTE-TDD B41 @ 23 dBm | 348 | mA |
| GSM voice call 模式 | EGSM900 PCL=5 @ 33 dBm | 225.8 | mA |
| GSM voice call 模式 | EGSM900 PCL=12 @ 20 dBm | 83.7 | mA |
| GSM voice call 模式 | EGSM900 PCL=19 @ 6 dBm | 53.6 | mA |
| GSM voice call 模式 | DCS1800 PCL=0 @ 31 dBm | 165 | mA |
| GSM voice call 模式 | DCS1800 PCL=7 @ 17 dBm | 64 | mA |
| GSM voice call 模式 | DCS1800 PCL=15 @ 1.5 dBm | 51 | mA |
| WCDMA voice call 模式 | WCDMA B1 @ 22.9 dBm | 484 | mA |
| WCDMA voice call 模式 | WCDMA B5 @ 23 dBm | 474 | mA |
| WCDMA voice call 模式 | WCDMA B8 @ 23 dBm | 440 | mA |

## 6.4 数字逻辑电平特性

**表 37：1.8 V I/O 要求**

| 参数 | 描述 | 最小值 | 最大值 | 单位 |
|---|---|---|---|---|
| VIH | 输入高电平 | 1.26 | 2.0 | V |
| VIL | 输入低电平 | -0.3 | 0.54 | V |
| VOH | 输出高电平 | 1.6 | - | V |
| VOL | 输出低电平 | - | 0.2 | V |

**表 38：(U)SIM 卡 1.8 V I/O 要求**

| 参数 | 描述 | 最小值 | 最大值 | 单位 |
|---|---|---|---|---|
| USIM_VDD | 供电 | 1.7 | 1.9 | V |
| VIH | 输入高电平 | 1.26 | - | V |
| VIL | 输入低电平 | -0.3 | 0.54 | V |
| VOH | 输出高电平 | 1.6 | - | V |
| VOL | 输出低电平 | - | 0.2 | V |

**表 39：(U)SIM 卡 3.0 V I/O 要求**

| 参数 | 描述 | 最小值 | 最大值 | 单位 |
|---|---|---|---|---|
| USIM_VDD | 供电 | 2.7 | 3.05 | V |
| VIH | 输入高电平 | 2 | 2.3 | V |
| VIL | 输入低电平 | -0.3 | 0.8 | V |
| VOH | 输出高电平 | 2.4 | - | V |
| VOL | 输出低电平 | - | 0.4 | V |

## 6.5 静电防护

人体静电、微电子间带电摩擦等产生的静电会通过各种途径放电给模块，可能造成损坏，应重视静电防护（研发、生产、组装、测试佩戴防静电手套；在电路接口处和易受 ESD 影响点位增加防静电器件）。

**表 40：ESD 性能参数（温度：25–30 ºC，湿度：45 ±5 %）**

| 测试点 | 接触放电 | 空气放电 | 单位 |
|---|---|---|---|
| VBAT、GND | ±6 | ±10 | kV |
| 主分集天线接口 | ±4 | ±8 | kV |
| 其他接口 | ±0.2 | ±1 | kV |

## 6.6 工作和存储温度

**表 41：工作和存储温度**

| 参数 | 最小值 | 典型值 | 最大值 | 单位 |
|---|---|---|---|---|
| 正常工作温度范围 ⁴ | -30 | +25 | +75 | °C |
| 扩展工作温度范围 ⁵ | -40 | - | +85 | °C |
| 存储温度范围 | -40 | - | +90 | °C |

> ⁴ 在此温度范围工作时，模块相关性能满足 3GPP 标准要求。
> ⁵ 在此温度范围工作时，模块仍能保持正常工作状态（语音、短信、数据传输、紧急呼叫等），不会出现不可恢复故障；射频频谱、网络基本不受影响；仅个别指标（如输出功率）可能超出 3GPP 范围；温度恢复正常后各项指标仍符合 3GPP 标准。

---

# 7 结构与规格

本章描述模块的机械尺寸，所有尺寸单位为毫米；所有未标注公差的尺寸，公差为 ±0.2 mm。

## 7.1 机械尺寸

**图 38：俯视及侧视尺寸图（单位：毫米）**
- **俯视图（Top）**：外形 32 ±0.15（宽）× 29 ±0.15（高）；Pin 1 角带倒角 `C1`（标 "Pin 1"）；四周一圈引脚。
- **侧视图（Side）**：模块厚度 2.65 ±0.2；引脚/焊盘高度 0.8。

**图 39：模块底视尺寸图（单位：毫米）**
- 底视图给出 LGA/LCC 焊盘阵列的详细定位尺寸；整体 32.0 ±0.15 × 29.0 ±0.15。
- 可见的焊盘排布参考尺寸（部分）：边距 1.90、引脚区起始 1.30 / 3.85、行/列间距约 0.87、2.0、2.15、3.4、3.2 等（详细逐焊盘坐标见原 PDF 图 39 矢量图）。
- 备注：移远通信模块的平整度符合《JEITA ED-7306》标准要求。

## 7.2 推荐封装

**图 40：推荐封装（俯视图）**
- 整体焊盘区 32（宽）× 29（高）；四周引脚焊盘 + 中央 LGA 焊盘阵列。
- 关键尺寸：周边焊盘节距 1.3 mm；横向 19×1.3 = 24.7；纵向 12×1.3 = 15.6、6×1.3 = 5.6；Pin 1 角 `C0.8` 倒角；其余标注 2.5、1.9、0.8、3.5、4.8、9.6、8.2、3.2、3.4、1、16、3.85、14.5、1.8 等。
- 未标注公差（Unlabeled tolerance）：±0.2 mm。

> 备注：为保证模块能够正常安装，请保证 PCB 板上模块和其他元器件之间的距离至少为 3 mm。

## 7.3 俯视图和底视图

**图 41：模块俯视图和底视图**
- 模块设计效果图（俯视 + 底视）。

> 备注：如上为模块设计效果图。实际产品外观和标签信息，请参照移远通信的模块实物。

---

# 8 存储、生产和包装

## 8.1 存储条件

模块以真空密封袋形式出货，湿度敏感等级为 **MSL 3**，存储需遵循：

1. 推荐存储条件：温度 23 ±5 °C，相对湿度 35~60 %。
2. 推荐存储条件下，可在真空密封袋中存放 12 个月。
3. 在温度 23 ±5 °C、相对湿度 < 60 % 的车间条件下，拆封后车间寿命为 168 小时 ⁶。此条件下可直接回流生产或其他高温操作；否则需存储于相对湿度 < 10 % 环境（如防潮柜）。
4. 若处于以下条件，需对模块进行预烘烤以防止吸湿受潮后高温焊接出现 PCB 起泡、裂痕、分层：
   - 存储温湿度不符合推荐存储条件；
   - 拆封后未能按第 3 条完成生产或存放；
   - 真空包装漏气、物料散装；
   - 模块返修前。
5. 模块烘烤处理：
   - 在 120 ±5 °C 条件下高温烘烤 8 小时；
   - 二次烘烤的模块须在烘烤后 24 小时内完成焊接，否则仍需在干燥箱内保存。

> ⁶ 仅在相对湿度较低的车间环境符合 IPC/JEDEC J-STD-033 规范时适用；不确定车间温湿度或相对湿度 > 60 % 时，请在拆封后 24 小时内完成贴片回流，请勿提前大量拆包。

> 备注：
> 1. 为预防受潮导致的起泡、分层等焊接不良，应严格管控，不建议拆开真空包装后长时间暴露在空气中。
> 2. 烘烤前将模块从包装取出，裸模块放置在耐高温器具上以免损伤塑料托盘/卷盘；二次烘烤须在烘烤后 24 小时内焊接，否则需在干燥箱保存。短时间烘烤参考 IPC/JEDEC J-STD-033 规范。
> 5. 拆包、放置模块时注意 ESD 防护（如佩戴防静电手套）。

## 8.2 生产焊接

用印刷刮板在网板上印刷锡膏，使锡膏通过网板开口漏印到 PCB 上。模块焊盘对应钢网厚度推荐 **0.18~0.20 mm**。详情参考文档 [1]。

推荐回流焊峰值温度 **235~246 °C**，最高不超过 246 °C。为避免模块反复受热损坏，强烈推荐客户完成 PCB 第一面回流焊后再贴模块。

**图 42：推荐的回流焊温度曲线**（无铅 SMT 回流焊）
- 升温段（Soak Zone 前）：升温斜率 0~3 °C/s，升至约 150 °C。
- 恒温段（Soak Zone，A→B，150~200 °C）。
- 回流段（Reflow Zone，B→C→D）：越过 217 °C，峰值 235~246 °C；升温斜率 0~3 °C/s，降温斜率 -3~0 °C/s。
- 关键温度刻度：100、150、200、217、235、246 °C。

**表 42：推荐的炉温测试控制要求**

| 项目 | 推荐值 |
|---|---|
| **吸热区（Soak Zone）** | |
| 升温斜率 | 0~3 °C/s |
| 恒温时间（A 和 B 之间：150~200 °C 期间） | 70~120 s |
| **回流焊区（Reflow Zone）** | |
| 升温斜率 | 0~3 °C/s |
| 回流时间（D：超过 217 °C 的期间） | 40~70 s |
| 最高温度 | 235~246 °C |
| 冷却降温斜率 | -3~0 °C/s |
| **回流次数** | |
| 最大回流次数 | 1 |

> 备注：
> 1. 以上工艺参数均针对焊点实测温度，PCB 上焊点最热点和最冷点均需满足规范。
> 2. 生产焊接或其他可能直接接触模块的过程中，不得使用任何有机溶剂（酒精、异丙醇、丙酮、三氯乙烯等）擦拭模块屏蔽罩，否则可能造成屏蔽罩生锈。
> 3. 移远洋白铜镭雕屏蔽罩可满足：12 小时中性盐雾测试后镭雕信息清晰可辨识、二维码可扫描（可能有白色锈蚀）。
> 4. 如需对模块喷涂，请确保喷涂材料不会与屏蔽罩或 PCB 发生化学反应，且不流入模块内部。
> 5. 请勿对模块进行超声波清洗，否则可能造成内部晶体损坏。
> 6. 因 SMT 流程复杂性，如遇不确定情况或文档 [1] 未提及的流程（选择性波峰焊、超声波焊接），请于 SMT 流程开始前与移远技术支持确认。

## 8.3 包装规格

本章仅体现包装关键参数和流程，所有图示仅供参考，具体包材外观、结构以实际交货为准。本模块采用载带包装。

### 8.3.1 载带

**图 43：载带尺寸图**
- 尺寸标注项：W（载带宽）、P（口袋节距）、T（载带厚）、A0、B0、K0、K1、F、E；定位孔 ø1.55、口袋 ø1.5；首口袋距 2.00、口袋节距 4.00。

**表 43：载带尺寸表（单位：毫米）**

| W | P | T | A0 | B0 | K0 | K1 | F | E |
|---|---|---|---|---|---|---|---|---|
| 44 | 44 | 0.35 | 32.5 | 29.5 | 3 | 3.8 | 20.2 | 1.75 |

### 8.3.2 胶盘

**图 44：胶盘尺寸图**
- 标注 øD1（外径）、øD2（内孔径）、W（盘宽）。

**表 44：胶盘尺寸表（单位：毫米）**

| øD1 | øD2 | W |
|---|---|---|
| 330 | 100 | 44.5 |

### 8.3.3 模块贴片方向

**图 45：模块贴片方向**
- 卷盘组成（自外向内）：Protective tape（保护带）、Cover tape（上盖带）、Module（模块）、Carrier tape（载带）、Plastic reel（塑料卷盘）。
- 卷盘送入 SMT 设备，"Direction of SMT" 标明送料方向；图示 Module Pin 1 与 Circular hole（定位圆孔）相对位置，明确模块在载带中的贴装方向。

### 8.3.4 包装流程

**图 46：包装流程**
1. 将模块放入载带中，使用上带热封；再将热封后的载带缠绕到胶盘中，用保护带缠绕防护。**1 个胶盘可装载 250 片模块。**
2. 将包装完成的胶盘和 1 张湿敏卡、1 包干燥剂放入真空袋中，抽真空。
3. 将抽真空后的胶盘放入披萨盒内。
4. 将 4 个披萨盒放入 1 个卡通箱内，封箱。**1 个卡通箱可包装 1000 片模块。**

---

# 9 附录　参考文档及术语缩写

**表 45：参考文档**

| 编号 | 文档名称 |
|---|---|
| [1] | Quectel_模块 SMT 应用指导 |
| [2] | Quectel_EC200A-CN(TA)_QuecOpen_AT_Commands_Manual |
| [3] | Quectel_射频 LAYOUT_应用指导 |
| [4] | Quectel_UMTS&LTE_EVB_User_Guide |
| [5] | Quectel_EC200A-CN(TA)_QuecOpen_低功耗模式用户指导 |
| [6] | Quectel_EC200A-CN(TA)_QuecOpen_设备管理 API_参考手册 |
| [7] | Quectel_EC200A-CN(TA)_QuecOpen_参考设计手册 |
| [8] | Quectel_EC200A-CN(TA)_QuecOpen_ADC_用户指导 |

**表 46：术语缩写**

| 术语 | 英文全称 | 中文全称 |
|---|---|---|
| AMR | Adaptive Multi-Rate | 自适应多速率 |
| API | Application Programming Interface | 应用程序接口 |
| BeiDou | BeiDou Navigation Satellite System | 北斗卫星导航系统 |
| bps | Bytes per second | 比特每秒 |
| CDMA | Code Division Multiple Access | 宽带码分多址 |
| CS | Coding Scheme | 编码方案 |
| CTS | Clear To Send | 清除发送 |
| DRX | Discontinuous Reception | 非连续接收 |
| DTE | Data Terminal Equipment | 数据终端设备 |
| EFR | Enhanced Full Rate | 增强型全速率 |
| EGSM | Enhanced GSM | 增强型 GSM |
| ESD | Electrostatic Discharge | 静电放电 |
| EVB | Evaluation Board | 评估板 |
| FDD | Frequency Division Duplexing | 频分双工 |
| FR | Full Rate | 全速率 |
| GMSK | Gaussian Filtered Minimum Shift Keying | 高斯滤波最小相移键控 |
| GLONASS | Global Navigation Satellite System (Russia) | 格洛纳斯导航卫星系统 |
| GND | Ground | 地 |
| GNSS | Global Navigation Satellite System | 全球导航卫星系统 |
| GPS | Global Positioning System | 全球定位系统 |
| GSM | Global System for Mobile Communications | 全球移动通信系统 |
| HR | Half Rate | 半速率 |
| HSDPA | High Speed Downlink Packet Access | 高速下行分组接入 |
| IEEE | Institute of Electrical and Electronics Engineers | 电机与电子工程师学会 |
| LED | Light Emitting Diode | 发光二极管 |
| LGA | Land Grid Array | 栅格阵列封装 |
| LTE | Long Term Evolution | 长期演进 |
| MCS | Modulation and Coding Scheme | 调制和编码方案 |
| PCB | Printed Circuit Board | 印制电路板 |
| PCM | Pulse Code Modulation | 脉冲编码调制 |
| PDU | Protocol Data Unit | 协议数据单元 |
| PF | Paging Frame | 寻呼帧 |
| PSK | Phase Shift Keying | 相移键控 |
| QAM | Quadrature Amplitude Modulation | 正交振幅调制 |
| QPSK | Quadrature Phase Shift Keying | 正交相移键控 |
| QZSS | Quasi-Zenith Satellite System | 准天顶卫星系统 |
| RF | Radio Frequency | 射频 |
| RoHS | Restriction of Hazardous Substances | 限制有害物质 |
| RTS | Request To Send | 请求发送 |
| SDIO | Secure Digital Input and Output Card | 安全的数字输入和输出卡 |
| RGMII | Reduced Gigabit Media Independent Interface | 精简吉比特介质独立接口 |
| SMS | Short Message Service | 短消息服务 |
| SPI | Serial Peripheral Interface | 串行外设接口 |
| TCP | Transmission Control Protocol | 传输控制协议 |
| TDD | Time Division Duplexing | 时分双工 |
| TD-SCDMA | Time Division-Synchronous Code Division Multiple Access | 时分-同步码分多址 |
| UART | Universal Asynchronous Receiver/Transmitter | 通用异步收发传输器 |
| UDP | User Datagram Protocol | 用户数据报协议 |
| UMTS | Universal Mobile Telecommunications System | 通用移动通信系统 |
| URC | Unsolicited Result Code | 非请求结果码 |
| USB | Universal Serial Bus | 通用串行总线 |
| (U)SIM | (Universal) Subscriber Identity Module | （全球）用户识别卡 |
| Vmax | Maximum Voltage Value | 最大电压值 |
| Vnom | Nominal Voltage Value | 标称电压值 |
| Vmin | Minimum Voltage Value | 最小电压值 |
| VIHmax | Maximum Input High Level Voltage Value | 最大输入高电压值 |
| VIHmin | Minimum Input High Level Voltage Value | 最小输入高电压值 |
| VILmax | Maximum Input Low Level Voltage Value | 最大输入低电压值 |
| VILmin | Minimum Input Low Level Voltage Value | 最小输入低电压值 |
| VOHmin | Minimum Output High Level Voltage Value | 最小输出高电压值 |
| VOLmax | Maximum Output Low Level Voltage Value | 最大输出低电压值 |
| VSWR | Voltage Standing Wave Ratio | 电压驻波比 |
| WCDMA | Wideband Code Division Multiple Access | 宽带码分多址 |
| WLAN | Wireless Local Area Network | 无线局域网 |

---

*（全文完。本 Markdown 依据 PDF V1.0.0 Preliminary 2023-04-11 完整转写；机械尺寸图 图39/图40 的逐焊盘精确坐标以原始矢量图为准。）*
