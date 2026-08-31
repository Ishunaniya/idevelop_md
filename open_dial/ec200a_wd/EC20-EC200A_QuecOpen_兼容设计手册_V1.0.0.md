# EC20-CE & EC20-CN & EC200A-CN(TA) QuecOpen 兼容设计手册

> **源文件**：`Quectel_EC20-CE&EC20-CN&EC200A-CN(TA)_QuecOpen_兼容设计手册_V1.0.0_Preliminary_20210923.pdf`
> **产品系列**：LTE Standard 模块系列
> **版本**：1.0.0　**日期**：2021-09-23　**状态**：临时文件（Preliminary）
> **版权**：版权所有 © 上海移远通信技术股份有限公司 2021，保留一切权利。

> 本 Markdown 为该 PDF 的**逐章节、逐表、逐图完整转写**。本手册的核心是 **EC20-CE / EC20-CN / EC200A-CN(TA) 三款 QuecOpen 模块之间的兼容设计对比**（频段、性能、引脚、电压、时序、接口、封装）。
> **命名说明**：文档中 **"EC20-CE QuecOpen"** 与 **"EC20 R2.1 QuecOpen"** 为同一款模块的两种写法（原文混用，本文保留）。表号原文从 15 直接跳到 17（无"表 16"）。

---

## 文档信息与法律声明

**移远通信联系方式**
- 上海移远通信技术股份有限公司
- 地址：上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼，邮编：200233
- 电话：+86 21 5108 6236　邮箱：info@quectel.com
- 销售：http://www.quectel.com/cn/support/sales.htm
- 技术支持：http://www.quectel.com/cn/support/technical.htm 或 support@quectel.com

**前言 / 使用和披露限制 / 免责声明**：内容与移远通信标准模板一致——文档仅作产品设计支持，参考设计仅为示例；接收方须保密；移远不承担因未遵守规范、信息不准确或遗漏造成的损害责任；对开发中功能不做保证；对第三方资源不承担法律责任。

---

## 文档历史 / 修订记录

| 版本 | 日期 | 作者 | 变更描述 |
|---|---|---|---|
| - | 2021-09-23 | Double YANG | 文档创建 |
| 1.0.0 | 2021-09-23 | Double YANG | 临时版本 |

---

# 1 引言

移远通信的 LTE Standard 模块 **EC20-CE & EC20-CN & EC200A-CN(TA) QuecOpen** 部分功能相互兼容。本文档主要描述这些模块之间的兼容设计。

## 1.1 特殊符号

**表 1：特殊符号**

| 符号 | 定义 |
|---|---|
| `*` | 若无特别说明，模块功能、特性、接口、引脚名称、AT 命令或参数后所标记的星号（*）表示该功能、特性、接口、引脚、AT 命令或参数正在开发中，因此暂不支持。 |

---

# 2 综述

## 2.1 产品简介

用户可根据需求选择合适的产品作为终端应用。

**表 2：支持的网络制式和分集接收功能**

| 模块 | LTE-FDD | LTE-TDD | WCDMA | TD-SCDMA | EVDO | CDMA | GSM | 分集接收 |
|---|---|---|---|---|---|---|---|---|
| EC200A-CN(TA) QuecOpen | √ | √ | √ | - | - | - | √ | √ |
| EC20-CN QuecOpen | √ | √ | √ | - | - | - | √ | √ |
| EC20-CE QuecOpen | √ | √ | √ | √ | √ | √ | √ | √ |

### 2.1.1 模块基本信息

**表 3：模块基本信息对比**

| 模块 | 封装 | 尺寸（mm） | 描述 |
|---|---|---|---|
| EC200A-CN(TA) QuecOpen | 80 个 LCC 引脚 + 64 个 LGA 引脚 | 29.0 × 32.0 × 2.65 | 多频段 LTE 模块 |
| EC20-CE QuecOpen | 80 个 LCC 引脚 + 64 个 LGA 引脚 | 29.0 × 32.0 × 2.4 | 多频段 LTE 模块 |
| EC20-CN QuecOpen | 80 个 LCC 引脚 + 64 个 LGA 引脚 | 29.0 × 32.0 × 2.4 | 多频段 LTE 模块 |

> 外观：三款模块均为方形贴片 LCC+LGA 封装（详见原文 表 3 外观图）。

### 2.1.2 模块频段

**表 4：模块频段对比**

| 模块 | LTE | UMTS | EVDO/CDMA | GSM | GNSS¹ |
|---|---|---|---|---|---|
| EC200A-CN(TA) QuecOpen | FDD: B1/B3/B5/B8；TDD: B34/B38/B39/B40/B41 | WCDMA: B1/B5/B8 | - | 900/1800 | GPS、GLONASS、BeiDou、Galileo、QZSS |
| EC20-CN QuecOpen | FDD: B1/B3/B5/B8；TDD: B34/B38/B39/B40/B41 | WCDMA: B1/B8 | - | 900/1800 | - |
| EC20-CE QuecOpen | FDD: B1/B3/B5/B8；TDD: B34/B38/B39/B40/B41 | WCDMA: B1/B8；TD-SCDMA: B34/B39 | BC0 | 900/1800 | GPS、GLONASS、BeiDou(COMPASS)、Galileo、QZSS |

> ¹ EC200A-CN(TA) QuecOpen、EC20 R2.1 QuecOpen 的 GNSS 为选配功能。

## 2.2 功能概述

**表 5：主要性能参数对比**

| 功能 | EC200A-CN(TA) QuecOpen | EC20-CN QuecOpen | EC20-CE QuecOpen |
|---|---|---|---|
| 供电 | 供电电压 3.4~4.3 V；典型值 3.8 V | 供电电压 3.3~4.3 V；典型值 3.8 V | 供电电压 3.3~4.3 V；典型值 3.8 V |
| 峰值电流 | VBAT 最大电流 3.0 A | VBAT 最大电流 2.0 A | VBAT 最大电流 2.0 A |
| 休眠耗流 | @ AT+CFUN=0（USB 断开）* | 0.76 mA @ AT+CFUN=0（USB 断开） | < 2 mA @ AT+CFUN=0（USB 断开） |
| 温度范围 | 正常 -35 ~ +75 °C²；扩展 -40 ~ +85 °C³；存储 -40 ~ +90 °C | 同左 | 同左 |
| 串口 | 主串口：用于 AT 命令传送和数据传输，最大 921600 bps（默认 115200），支持 RTS/CTS 硬件流控。调试串口：用于 Linux 控制、日志输出，115200 bps | 主串口：用于外设通信串口，最大 921600 bps（默认 115200），支持 RTS/CTS。调试串口同左 | 主串口：用于外设通信串口，最大 921600 bps（默认 115200），支持 RTS/CTS。调试串口同左 |
| (U)SIM 接口 | 支持 (U)SIM 卡 1.8/3.0 V | 同左 | 同左 |
| PCM 接口 | 需外接 Codec；16 位线性编码；**支持短帧模式**；支持主/从模式 | 需外接 Codec；16 位线性编码；**支持长帧和短帧模式**；支持主/从模式，长帧下只可用作主模式 | 同 EC20-CN |
| USB 接口 | 兼容 USB 2.0（默认从模式），最大 480 Mbps | 同左 | 同左 |
| SD 卡接口 | 符合 SD 3.0 协议 | 同左 | 同左 |
| SGMII 接口 | - | 支持 10/100/1000 Mbps 以太网工作模式 | 支持 10/100/1000 Mbps 以太网工作模式 |
| RGMII 接口 | RGMII 功能* | - | - |
| WLAN/蓝牙接口 | WLAN 功能*；蓝牙不支持 | - | 支持低功耗 SDIO 3.0 WLAN；蓝牙不支持 |
| ADC 接口 | 两路 ADC；电压范围 0~VBAT_BB；分辨率 12 bits | 两路 ADC；电压范围 0.3 V~VBAT_BB；分辨率 15 bits | 同 EC20-CN |
| 网络指示 | NET_STATUS 引脚指示网络状态 | 同左 | 同左 |
| 分集接收天线接口 | 支持 LTE 分集接收 | 支持 LTE/WCDMA 分集接收 | 支持 LTE/WCDMA 分集接收 |
| 天线接口 | 主天线 ANT_MAIN、分集 ANT_DRX、GNSS ANT_GNSS | 主天线 ANT_MAIN、分集 ANT_DIV | 主天线 ANT_MAIN、分集 ANT_DIV、GNSS ANT_GNSS |
| 固件升级 | 可通过 USB 接口或 DFOTA 升级 | 同左 | 同左 |

> ² 在此温度范围工作时，模块相关性能满足 3GPP 标准要求。
> ³ 在此温度范围工作时，模块仍能保持正常工作状态（语音、短信、数据传输和紧急呼叫——EC20-CN QuecOpen 的紧急呼叫功能正在开发中，EC200A-CN(TA) 不支持紧急呼叫，EC20 R2.1 支持紧急呼叫），不会出现不可恢复故障；射频频谱、网络基本不受影响；仅个别指标（如输出功率）可能超出 3GPP 范围；温度恢复正常后各项指标仍符合 3GPP 标准。

## 2.3 引脚分配

**图 1：EC20-CE & EC20-CN & EC200A-CN(TA) QuecOpen 引脚分配图（俯视图）**

图中并列给出三款模块的引脚分配俯视图：**EC200A-CN(TA) QuecOpen**（上）、**EC20 R2.1 QuecOpen**（下左）、**EC20-CN QuecOpen**（下右）。三者外形与焊盘位置完全兼容，差异以颜色区分。引脚名称与编号逐一对照见下方 **表 7**。

颜色图例：
> 灰色 = 兼容引脚（封装兼容且功能相同）；RESERVED 引脚；SGMII 引脚；WLAN 引脚（红）；SPI 引脚（橙）；RGMII 引脚（紫）；音频功能引脚（棕）；USIM2_VDD。

**图 1 备注**：
1. EC200A-CN(TA) QuecOpen 的 USB_BOOT 引脚在模块开机成功前禁止上拉到高电平。上图 EC200A-CN(TA) QuecOpen 引脚分配图为内部不带 GNSS 模块时的引脚分配图；当使用内置 GNSS 模块时，47 引脚可作为 ANT_GNSS 使用，63、66、118 引脚不能使用。
2. EC20 R2.1 QuecOpen 的引脚 GPIO1、GPIO5、WLAN_EN、COEX_UART_RX、COEX_UART_TX、USB_BOOT、SPI_CLK、PCM_SYNC、PCM_CLK 在模块开机成功前禁止上拉到高电平。
3. EC20-CN QuecOpen 的引脚 GPIO1、GPIO5、USB_BOOT、SPI_CLK 以及引脚 WLAN_EN、COEX_RXD、COEX_TXD 在模块开机成功前禁止上拉到高电平。

---

# 3 引脚描述

本章描述三款模块的引脚定义及比较。

**表 6：I/O 参数定义**

| 类型 | 描述 | 类型 | 描述 |
|---|---|---|---|
| AI | 模拟输入 | DIO | 数字输入/输出 |
| AO | 模拟输出 | OD | 漏极开路 |
| AIO | 模拟输入/输出 | PI | 电源输入 |
| DI | 数字输入 | PO | 电源输出 |
| DO | 数字输出 | PIO | 电源输入/输出 |

**表 7：引脚对比**

> 说明：以下三列分别为 EC200A-CN(TA) / EC20-CE / EC20-CN，三款引脚号一致（1~144），仅引脚名/电压域可能不同。**红色字体（本文用 ⚠ 标注）= 封装兼容但功能或电压域不同；其余 = 封装兼容且功能相同**。

| 引脚号 | EC200A-CN(TA) 引脚名 (I/O, 电压域) | EC20-CE 引脚名 (I/O, 电压域) | EC20-CN 引脚名 (I/O, 电压域) |
|---|---|---|---|
| 1 | GPIO1 (DIO, 1.8 V) | GPIO1 (DIO, 1.8 V) | GPIO1 (DIO, 1.8 V) |
| 2 | GPIO2 (DIO, 1.8 V) | GPIO2 (DIO, 1.8 V) | GPIO2 (DIO, 1.8 V) |
| 3 | GPIO3 (DIO, 1.8 V) | GPIO3 (DIO, 1.8 V) | GPIO3 (DIO, 1.8 V) |
| 4 | GPIO4 (DIO, 1.8 V) | GPIO4 (DIO, 1.8 V) | GPIO4 (DIO, 1.8 V) |
| 5 | GPIO5 (DIO, 1.8 V) | GPIO5 (DIO, 1.8 V) | GPIO5 (DIO, 1.8 V) |
| 6 | NET_STATUS (DO, 1.8 V) | NET_STATUS (DO, 1.8 V) | NET_STATUS (DO, 1.8 V) |
| 7 | VDD_EXT (PO, 1.8 V) | VDD_EXT (PO, 1.8 V) | VDD_EXT (PO, 1.8 V) |
| 8 | GND (-, GND) | GND | GND |
| 9 | GND (-, GND) | GND | GND |
| 10 | USIM_GND (-, GND) | USIM_GND | USIM_GND |
| 11 | DBG_RXD (DI, 1.8 V) | DBG_RXD (DI, 1.8 V) | DBG_RXD (DI, 1.8 V) |
| 12 | DBG_TXD (DO, 1.8 V) | DBG_TXD (DO, 1.8 V) | DBG_TXD (DO, 1.8 V) |
| 13 | ⚠ USIM_DET (DI, 1.8 V) | USIM_PRESENCE (DI, 1.8 V) | USIM_PRESENCE (DI, 1.8 V) |
| 14 | USIM_VDD (PO, 1.8/3.0 V) | USIM_VDD (PO, 1.8/3.0 V) | USIM_VDD (PO, 1.8/3.0 V) |
| 15 | USIM_DATA (DIO, 1.8/3.0 V) | USIM_DATA (DIO, 1.8/3.0 V) | USIM_DATA (DIO, 1.8/3.0 V) |
| 16 | USIM_CLK (DO, 1.8/3.0 V) | USIM_CLK (DO, 1.8/3.0 V) | USIM_CLK (DO, 1.8/3.0 V) |
| 17 | USIM_RST (DO, 1.8/3.0 V) | USIM_RST (DO, 1.8/3.0 V) | USIM_RST (DO, 1.8/3.0 V) |
| 18 | RESERVED (-, -) | RESERVED | RESERVED |
| 19 | GND (-, GND) | GND | GND |
| 20 | RESET_N (DI, 1.8 V) | RESET_N (DI, 1.8 V) | RESET_N (DI, 1.8 V) |
| 21 | PWRKEY (DI, VBAT 电压域) | ⚠ PWRKEY (DI, 模块上电后该引脚输出电压为 0.8 V) | ⚠ PWRKEY (DI, 模块上电后该引脚输出电压为 0.8 V) |
| 22 | GND (-, GND) | GND | GND |
| 23 | ⚠ SD_DET (DI, 1.8 V) | SD_INS_DET (DI, 1.8 V) | SD_INS_DET (DI, 1.8 V) |
| 24 | ⚠ PCM_DIN (DI, 1.8 V) | PCM_IN (DI, 1.8 V) | PCM_DIN (DI, 1.8 V) |
| 25 | ⚠ PCM_DOUT (DO, 1.8 V) | PCM_OUT (DO, 1.8 V) | PCM_DOUT (DO, 1.8 V) |
| 26 | PCM_SYNC (DIO, 1.8 V) | PCM_SYNC (DIO, 1.8 V) | PCM_SYNC (DIO, 1.8 V) |
| 27 | PCM_CLK (DIO, 1.8 V) | PCM_CLK (DIO, 1.8 V) | PCM_CLK (DIO, 1.8 V) |
| 28 | ⚠ SD_SDIO_DATA3 (DIO, 1.8/3.0 V) | SDC2_DATA3 (DIO, 1.8/3.0 V) | SDC2_DATA3 (DIO, 1.8/3.0 V) |
| 29 | ⚠ SD_SDIO_DATA2 (DIO, 1.8/3.0 V) | SDC2_DATA2 (DIO, 1.8/3.0 V) | SDC2_DATA2 (DIO, 1.8/3.0 V) |
| 30 | ⚠ SD_SDIO_DATA1 (DIO, 1.8/3.0 V) | SDC2_DATA1 (DIO, 1.8/3.0 V) | SDC2_DATA1 (DIO, 1.8/3.0 V) |
| 31 | ⚠ SD_SDIO_DATA0 (DIO, 1.8/3.0 V) | SDC2_DATA0 (DIO, 1.8/3.0 V) | SDC2_DATA0 (DIO, 1.8/3.0 V) |
| 32 | ⚠ SD_SDIO_CLK (DO, 1.8/3.0 V) | SDC2_CLK (DO, 1.8/3.0 V) | SDC2_CLK (DO, 1.8/3.0 V) |
| 33 | ⚠ SD_SDIO_CMD (DIO, 1.8/3.0 V) | SDC2_CMD (DIO, 1.8/3.0 V) | SDC2_CMD (DIO, 1.8/3.0 V) |
| 34 | ⚠ SD_SDIO_VDD (PO, 1.8/2.8 V) | VDD_SDIO (PO, 1.8/2.85 V) | VDD_SDIO (PO, 1.8/2.85 V) |
| 35 | ⚠ ANT_DRX (AI, -) | ANT_DIV (AI, -) | ANT_DIV (-, -) |
| 36 | GND (-, GND) | GND | GND |
| 37 | ⚠ SPI_CS* (DO, 1.8 V) | SPI_CS_N (DI, 1.8 V) | SPI_CS (DI, 1.8 V) |
| 38 | ⚠ SPI_DOUT* (DO, 1.8 V) | SPI_MOSI (DO, 1.8 V) | SPI_MOSI (DO, 1.8 V) |
| 39 | ⚠ SPI_DIN* (DI, 1.8 V) | SPI_MISO (DI, 1.8 V) | SPI_MISO (DI, 1.8 V) |
| 40 | SPI_CLK* (DO, 1.8 V) | SPI_CLK (DO, 1.8 V) | SPI_CLK (DO, 1.8 V) |
| 41 | I2C_SCL (OD, 外部需 1.8 V 上拉) | I2C_SCL (OD, 外部需 1.8 V 上拉) | I2C_SCL (OD, 外部需 1.8 V 上拉) |
| 42 | I2C_SDA (OD, 外部需 1.8 V 上拉) | I2C_SDA (OD, 外部需 1.8 V 上拉) | I2C_SDA (OD, 外部需 1.8 V 上拉) |
| 43 | RESERVED | RESERVED | RESERVED |
| 44 | ⚠ ADC1 (AI, 0~VBAT_BB) | ADC1 (AI, 0.3~VBAT_BB) | ADC1 (AI, 0.3~VBAT_BB) |
| 45 | ⚠ ADC0 (AI, 0~VBAT_BB) | ADC0 (AI, 0.3~VBAT_BB) | ADC0 (AI, 0.3~VBAT_BB) |
| 46 | GND (-, GND) | GND | GND |
| 47 | ⚠ ANT_GNSS* (AI, -) | ANT_GNSS (AI, -) | RESERVED (-, -) |
| 48 | GND (-, GND) | GND | GND |
| 49 | ANT_MAIN (DIO, -) | ANT_MAIN (DIO, -) | ANT_MAIN (DIO, -) |
| 50~54 | GND (-, GND) | GND | GND |
| 55 | RESERVED (-, -) | RESERVED | RESERVED |
| 56 | GND (-, GND) | GND | GND |
| 57 | ⚠ VBAT_RF (PI, 3.4~4.3 V) | VBAT_RF (PI, 3.3~4.3 V) | VBAT_RF (PI, 3.3~4.3 V) |
| 58 | ⚠ VBAT_RF (PI, 3.4~4.3 V) | VBAT_RF (PI, 3.3~4.3 V) | VBAT_RF (PI, 3.3~4.3 V) |
| 59 | ⚠ VBAT_BB (PI, 3.4~4.3 V) | VBAT_BB (PI, 3.3~4.3 V) | VBAT_BB (PI, 3.3~4.3 V) |
| 60 | ⚠ VBAT_BB (PI, 3.4~4.3 V) | VBAT_BB (PI, 3.3~4.3 V) | VBAT_BB (PI, 3.3~4.3 V) |
| 61 | STATUS (OD, -) | STATUS (OD, -) | STATUS (OD, -) |
| 62 | GPIO6 (DO, 1.8 V) | GPIO6 (DO, 1.8 V) | GPIO6 (DO, 1.8 V) |
| 63 | ⚠ GPIO7 (DO, 1.8 V) | UART1_TXD (DO, 1.8 V) | UART1_TXD (DO, 1.8 V) |
| 64 | MAIN_CTS (DO, 1.8 V) | MAIN_CTS (DO, 1.8 V) | MAIN_CTS (DO, 1.8 V) |
| 65 | MAIN_RTS (DI, 1.8 V) | MAIN_RTS (DI, 1.8 V) | MAIN_RTS (DI, 1.8 V) |
| 66 | ⚠ MAIN_DTR (DI, 1.8 V) | UART1_RXD (DI, 1.8 V) | UART1_RXD (DI, 1.8 V) |
| 67 | MAIN_TXD (DO, 1.8 V) | MAIN_TXD (DO, 1.8 V) | MAIN_TXD (DO, 1.8 V) |
| 68 | MAIN_RXD (DI, 1.8 V) | MAIN_RXD (DI, 1.8 V) | MAIN_RXD (DI, 1.8 V) |
| 69 | USB_DP (AIO, -) | USB_DP (AIO, -) | USB_DP (AIO, -) |
| 70 | USB_DM (AIO, -) | USB_DM (AIO, -) | USB_DM (AIO, -) |
| 71 | USB_VBUS (AI, 3.0~5.25 V) | USB_VBUS (AI, 3.0~5.25 V) | USB_VBUS (AI, 3.0~5.25 V) |
| 72 | GND (-, GND) | GND | GND |
| 73 | ⚠ RGMII_RX_1* (DI, 1.8/3.3 V) | RESERVED (-, -) | RESERVED (-, -) |
| 74 | ⚠ RGMII_CTL_RX* (DI, 1.8/3.3 V) | RESERVED | RESERVED |
| 75 | ⚠ RGMII_CK_RX* (DI, 1.8/3.3 V) | RESERVED | RESERVED |
| 76 | ⚠ RGMII_RX_0* (DI, 1.8/3.3 V) | RESERVED | RESERVED |
| 77 | ⚠ RGMII_TX_0* (DO, 1.8/3.3 V) | RESERVED | RESERVED |
| 78 | ⚠ RGMII_TX_1* (DO, 1.8/3.3 V) | RESERVED | RESERVED |
| 79 | ⚠ RGMII_RX_2* (DI, 1.8 V) | RESERVED | RESERVED |
| 80 | ⚠ RGMII_TX_2* (DO, 1.8 V) | RESERVED | RESERVED |
| 81 | ⚠ RGMII_CTL_TX* (DO, 1.8/3.3 V) | RESERVED | RESERVED |
| 82 | ⚠ RGMII_RX_3* (DI, 1.8 V) | RESERVED | RESERVED |
| 83 | ⚠ RGMII_CK_TX* (DO, 1.8 V) | RESERVED | RESERVED |
| 84 | ⚠ RGMII_TX_3* (DO, 1.8 V) | RESERVED | RESERVED |
| 85~112 | GND (-, GND) | GND | GND |
| 113 | RESERVED (-, -) | RESERVED | RESERVED |
| 114 | RESERVED (-, -) | RESERVED | RESERVED |
| 115 | USB_BOOT (DI, 1.8 V) | USB_BOOT (DI, 1.8 V) | USB_BOOT (DI, 1.8 V) |
| 116 | RESERVED (-, -) | RESERVED | RESERVED |
| 117 | RESERVED | RESERVED | RESERVED |
| 118 | WLAN_SLP_CLK* (DO, 1.8 V) | WLAN_SLP_CLK (DO, 1.8 V) | WLAN_SLP_CLK (-, -) |
| 119 | ⚠ RESERVED (-, -) | EPHY_RST_N (DO, 1.8/2.85 V) | EPHY_RST_N (DO, 1.8/2.85 V) |
| 120 | ⚠ RGMII_INT* (DI, 1.8/3.3 V) | EPHY_INT_N (DI, 1.8 V) | EPHY_INT_N (DI, 1.8 V) |
| 121 | ⚠ RGMII_MD_IO* (DIO, 1.8/3.3 V) | SGMII_MDATA (DIO, 1.8/2.85 V) | SGMII_MDATA (DIO, 1.8/2.85 V) |
| 122 | ⚠ RGMII_MD_CLK* (DO, 1.8/3.3 V) | SGMII_MCLK (DO, 1.8/2.85 V) | SGMII_MCLK (DO, 1.8/2.85 V) |
| 123 | ⚠ SPK_N* (AO, -) | SGMII_TX_M (AO, -) | SGMII_TX_M (AO, -) |
| 124 | ⚠ SPK_P* (AO, -) | SGMII_TX_P (AO, -) | SGMII_TX_P (AO, -) |
| 125 | ⚠ MIC_P* (AI, -) | SGMII_RX_P (AI, -) | SGMII_RX_P (AI, -) |
| 126 | ⚠ MIC_N* (AI, -) | SGMII_RX_M (AI, -) | SGMII_RX_M (AI, -) |
| 127 | ⚠ WLAN_PWR_EN (DO, 1.8 V) | PM_ENABLE (DO, 1.8 V) | PM_ENABLE (DO, 1.8 V) |
| 128 | ⚠ RESERVED (-, -) | USIM2_VDD (PO, 1.8/2.85 V) | USIM2_VDD (PO, 1.8/2.85 V) |
| 129 | ⚠ WLAN_SDIO_DATA3 (DIO, 1.8 V) | SDC1_DATA3 (DIO, 1.8 V) | SDC1_DATA3 (-, -) |
| 130 | ⚠ WLAN_SDIO_DATA2 (DIO, 1.8 V) | SDC1_DATA2 (DIO, 1.8 V) | SDC1_DATA2 (-, -) |
| 131 | ⚠ WLAN_SDIO_DATA1 (DIO, 1.8 V) | SDC1_DATA1 (DIO, 1.8 V) | SDC1_DATA1 (-, -) |
| 132 | ⚠ WLAN_SDIO_DATA0 (DIO, 1.8 V) | SDC1_DATA0 (DIO, 1.8 V) | SDC1_DATA0 (-, -) |
| 133 | ⚠ WLAN_SDIO_CLK (DO, 1.8 V) | SDC1_CLK (DO, 1.8 V) | SDC1_CLK (-, -) |
| 134 | ⚠ WLAN_SDIO_CMD (DO, 1.8 V) | SDC1_CMD (DO, 1.8 V) | SDC1_CMD (-, -) |
| 135 | ⚠ WLAN_WAKE (DI, 1.8 V) | WAKE_WLAN (DI, 1.8 V) | WLAN_WAKE (-, -) |
| 136 | WLAN_EN (DO, 1.8 V) | WLAN_EN (DO, 1.8 V) | WLAN_EN (-, -) |
| 137 | ⚠ RESERVED (-, -) | COEX_UART_RX (DI, 1.8 V) | COEX_RXD (-, -) |
| 138 | ⚠ RESERVED (-, -) | COEX_UART_TX (DO, 1.8 V) | COEX_RXD (-, -) |
| 139 | ⚠ RESERVED (-, -) | BT_EN (DO, 1.8 V) | BT_EN (-, -) |
| 140 | ⚠ MICBIAS* (PO, 1.8 V) | RESERVED (-, -) | RESERVED (-, -) |
| 141~144 | RESERVED (-, -) | RESERVED | RESERVED |

> **表 7 备注**：
> 1. 红色字体（本文 ⚠）标示的引脚表示封装兼容但功能或电压域不同；黑色字体（无标）标示的引脚表示封装兼容且功能相同。
> 2. 所有预留和不用的引脚需悬空。
> 3. 模块的引脚描述详细信息可参考 EC20-CE / EC20-CN / EC200A-CN(TA) QuecOpen 对应的硬件设计手册文档。

---

# 4 硬件参考设计

本章描述三款模块主要功能的兼容设计。

## 4.1 供电电源

### 4.1.1 模块工作电压

**表 8：模块工作电压范围对比**

| 模块 | 电源引脚 | 最小值 | 典型值 | 最大值 | 单位 | 描述 |
|---|---|---|---|---|---|---|
| EC200A-CN(TA) QuecOpen | VBAT_BB & VBAT_RF | 3.4 | 3.8 | 4.3 | V | 实际输入电压必须在最小、最大值范围内 |
| EC20-CE QuecOpen | VBAT_BB & VBAT_RF | 3.3 | 3.8 | 4.3 | V | 同上 |
| EC20-CN QuecOpen | VBAT_BB & VBAT_RF | 3.3 | 3.8 | 4.3 | V | 同上 |

> 兼容设计时，请确保模块输入电压**不低于 3.4 V、最大不超过 4.3 V**。

**图 2：突发传输电源要求**
- 在 2G 网络突发传输（Burst Transmission）时 VBAT 出现电压跌落（跌落）和起伏（纹波）；3G、4G 网络下电压跌落比 2G 小。

### 4.1.2 供电电源设计

为减少电压跌落，需使用低 ESR（ESR = 0.7 Ω）的 100 µF 滤波电容；建议分别给 VBAT_BB 和 VBAT_RF 预留 3 个（100 nF、33 pF、10 pF）最佳 ESR 的 MLCC，靠近 VBAT 引脚放置。VBAT_BB、VBAT_RF 需星型走线：VBAT_BB 走线宽度 ≥ 1 mm，VBAT_RF ≥ 2 mm。建议在电源前端加 VRWM = 4.5 V、PPP = 2160 W 的 WS4.5DPVL TVS 管。

**图 3：模块供电电路**
- `VBAT` → 经 TVS `D1`（WS4.5DPVL）对地 → 分两路：VBAT_RF 支路电容 C1 `100 µF`/C2 `100 nF`/C3 `33 pF`/C4 `10 pF`；VBAT_BB 支路电容 C5 `100 µF`/C6 `100 nF`/C7 `33 pF`/C8 `10 pF`；靠近模块（Module）放置。

> 供电能力要求：EC20-CE & EC20-CN QuecOpen 必须选至少 **2 A** 电流能力电源；EC200A-CN(TA) QuecOpen 必须选至少 **3 A**。压差小建议用 LDO，压差大建议用开关电源转换器。

## 4.2 开关机

### 4.2.1 开关机电路

三款模块开关机方式相同，均通过拉低 PWRKEY 实现开关机，推荐使用开集驱动电路。

**图 4：开集驱动开关机参考电路**
- Turn on/off pulse 经三极管开集驱动 PWRKEY；上拉/基极电阻 `4.7K`、`47K`；PWRKEY 处并 `10 nF` 电容。

**图 5：PWRKEY 按键开关机参考电路**
- 按键 `S1` 直接控制 PWRKEY，按键附近放 `TVS`（靠近 S1）用于 ESD 保护。

**图 6：EC200A-CN(TA) QuecOpen 开机时序图**

| 时序 | 信号/动作 | 时间 |
|---|---|---|
| T1 | VBAT 稳定后拉低 PWRKEY（VIL 0.3 × VBAT_BB） | 500 ms |
| T2 | → VDD_EXT 输出 | 约 17 ms |
| T3 | USB_BOOT（强制下载窗口） | TBD* |
| T4 | → RESET_N | 约 17 ms |
| T5 | → STATUS (OD) | 10 s |
| T6 | → UART（Inactive→Active） | 10 s |
| T7 | → USB（Inactive→Active） | 10 s |

**图 7：EC20-CE & EC20-CN QuecOpen 开机时序图**

| 时序 | 信号/动作 | 时间 |
|---|---|---|
| T1 | VBAT 稳定后拉低 PWRKEY（VIL 0.5 V） | 500 ms |
| T2 | → VDD_EXT | 约 100 ms |
| T3 | USB_BOOT | 200 ms |
| T5 | → STATUS (OD) | 2.5 s |
| T6 | → UART（Inactive→Active） | 12 s |
| T7 | → USB（Inactive→Active） | 13 s |

**表 9：EC20-CE & EC20-CN & EC200A-CN(TA) QuecOpen 开机时序相关时间**

| 模块 | T1 | T2 | T3 | T4 | T5 | T6 | T7 |
|---|---|---|---|---|---|---|---|
| EC200A-CN(TA) QuecOpen | ≥ 500 ms | 17 ms（典型值） | TBD* | 17 ms（典型值） | ≥ 10 s | ≥ 10 s | ≥ 10 s |
| EC20-CN QuecOpen | ≥ 500 ms | 100 ms（典型值） | ≥ 200 ms | - | ≥ 2.5 s | ≥ 12 s | ≥ 13 s |
| EC20-CE QuecOpen | ≥ 500 ms | - | - | - | ≥ 2.5 s | ≥ 12 s | ≥ 13 s |

> 备注：
> 1. 拉低 PWRKEY 前为保证 VBAT 稳定，建议 VBAT 上电到 PWRKEY 拉低的间隔 ≥ 30 ms。
> 2. 如需上电自动开机且不需要关机功能，可把 PWRKEY 直接下拉到地：EC200A-CN(TA) 下拉电阻建议 4.7 kΩ，EC20-CE & EC20-CN 建议 10 kΩ。
> 3. EC200A-CN(TA) 模块 RESET_N 引脚电压在 PWRKEY 拉低后才有电压；EC20-CE & EC20-CN 模块该引脚电压在 VBAT 上电后就有。

### 4.2.2 AT 命令关机

三款模块都可通过 `AT+QPOWD` 关机，过程等同拉低 PWRKEY 关机。详见文档 [1]。

**图 8：EC20-CE & EC20-CN & EC200A-CN(TA) QuecOpen 关机时序图**
- 拉低 PWRKEY（T1），STATUS(OD) 变化（T2）；Module Status：Running → Power-down procedure → OFF。

**表 10：关机时序相关时间**

| 模块 | T1 | T2 |
|---|---|---|
| EC200A-CN(TA) QuecOpen | * | * |
| EC20-CN QuecOpen | ≥ 650 ms | ≥ 29.5 s |
| EC20-CE QuecOpen | ≥ 650 ms | ≥ 29.5 s |

> 备注：
> 1. 正常工作时不要立即切断电源，以避免损坏内部 Flash；建议先 PWRKEY 或 AT 命令关机后再断电。
> 2. AT 命令关机时确保关机后 PWRKEY 一直高电平，否则模块完成关机后会自动再次开机。

## 4.3 复位

三款模块复位方式相同，均通过拉低 RESET_N 复位，推荐使用开集驱动电路。

**图 9：RESET_N 复位开集参考电路**
- Reset pulse 经三极管开集驱动 RESET_N；上拉/基极电阻 `4.7K`、`47K`。

**图 10（电路）：RESET_N 复位按钮参考电路**
- 按键 `S2` 直接控制 RESET_N，附近放 `TVS`（靠近 S2）。

**图 10（时序）：EC20-CE & EC20-CN & EC200A-CN(TA) QuecOpen 复位时序图**
> 注：原文两图同标"图 10"（按钮电路图 + 复位时序图）。
- RESET_N 拉低 T1（VIL 0.5 V）→ Module Status：Running → Resetting → Baseband restart。

**表 11：复位时序相关时间**

| 模块 | T1 |
|---|---|
| EC200A-CN(TA) QuecOpen | TBD |
| EC20-CE QuecOpen | 150 ms ≤ T1 ≤ 460 ms |
| EC20-CN QuecOpen | 150 ms ≤ T1 ≤ 460 ms |

> 备注：
> 1. EC200A-CN(TA) 拉低 RESET_N 仅复位模块内部基带芯片，不复位电源管理芯片；EC20-CN & EC20-CE 拉低 RESET_N 可复位电源管理芯片。
> 2. EC200A-CN(TA) 模块 RESET_N 拉低期间基带芯片处于复位状态，释放后系统重启；RESET_N 拉低过程中严禁 USB_BOOT 引脚拉高。
> 3. 确保 PWRKEY 和 RESET_N 引脚没有大负载电容，最大不超过 10 nF。

## 4.4 (U)SIM 接口

三款模块都默认支持 1.8/3.0 V (U)SIM 卡，(U)SIM 接口可相互兼容；通过 USIM_DET 或 USIM_PRESENCE 引脚支持 (U)SIM 卡热插拔。

**图 11：8-Pin (U)SIM 接口参考电路**
- USIM_VDD 经 `51K`（接 VDD_EXT）+ `15K` 上拉，并 `100 nF` 去耦。
- USIM_RST / USIM_CLK / USIM_DET(/USIM_PRESENCE) / USIM_DATA 各串 `0R` 接 (U)SIM 卡座（VCC、RST、CLK、IO、GND、VPP）。
- USIM_DATA、USIM_CLK、USIM_RST 各并 `33 pF` 电容对地。

**图 12：6-pin (U)SIM 接口参考电路**
- 不使用 (U)SIM 卡检测功能时，USIM_DET 或 USIM_PRESENCE 引脚悬空。
- USIM_VDD 经 `15K` 上拉 + `100 nF` 去耦；USIM_RST/CLK/DATA 各串 `0R`；并 `33 pF` 电容对地接卡座。

## 4.5 USB 接口

三款模块 USB 接口符合 USB 2.0 规范，支持高速（480 Mbps）和全速（12 Mbps）模式。建议预留测试点。

**图 13：USB 接口参考设计**
- Module ↔ MCU：USB_DM/USB_DP 经共模电感 `L1`（靠近模块）连到 MCU；预留 ESD Array。
- USB_VBUS 经 `R3`(NM_0R)、`R4`(NM_0R) 接 VDD（默认不贴）；预留测试点（Test Points），桩线尽量短。

设计原则：USB 走线包地、走 90 Ω 差分阻抗；不在晶振/振荡器/磁性器件/RF 信号下走 USB 线，建议内层差分立体包地；USB ESD 器件寄生电容 ≤ 2 pF 且靠近 USB 接口放置。

## 4.6 串口

三款模块都提供两路串口（主串口、调试串口）：
- 主串口：支持 4800/9600/19200/38400/57600/115200/230400/460800/921600 bps、1 Mbps，默认 115200 bps，用于数据传输和普通外设通讯；支持 RTS/CTS 硬件流控。
- 调试串口：115200 bps；EC200A-CN(TA) 用于部分日志输出，EC20-CE & EC20-CN 用于 Linux 控制和日志输出。

串口电平 1.8 V。主机 3.3 V 时需增加电平转换器。

**图 14：电平转换芯片参考电路**（Translator）
- VDD_EXT→VCCA（去耦 `C1 0.1 µF`），VDD_MCU→VCCB（去耦 `C2 0.1 µF`）。
- A1↔B1：MAIN_TXD↔TXD_MCU；A2↔B2：MAIN_RXD↔RXD_MCU；A3↔B3：MAIN_CTS↔CTS_MCU；A4↔B4：MAIN_RTS↔RTS_MCU。
- OE 经 `R1 10k` 上拉至 VDD_EXT、`R2 120k`；GND、NC。

**图 15：三极管电平转换参考电路**
- MCU/ARM ↔ Module：TXD↔MAIN_RXD/RXD、RXD↔MAIN_TXD/TXD（经三极管，`4.7K`@VDD_EXT、`10K`、`1 nF` 滤波）。
- 流控：RTS↔MAIN_RTS/RTS、CTS↔MAIN_CTS/CTS（`4.7K`@VDD_EXT/VCC_MCU、`10K`）；GPIO↔MAIN_DTR/DTR、GPIO↔DCD/GPIO7；共地。

> 备注：
> 1. 三极管电平转换电路不适用于波特率超过 460 kbps 的应用。
> 2. 串口硬件流控 CTS、RTS 采用直连方式，并注意输入输出方向。

## 4.7 PCM 和 I2C 接口

三款模块各提供 1 个 PCM 接口和 1 个 I2C 接口。

**表 12：PCM 接口对比**

| 功能 | EC200A-CN(TA) QuecOpen | EC20 R2.1 & EC20-CN QuecOpen |
|---|---|---|
| PCM 接口 | 需外接 Codec；16 位线性编码；**支持短帧模式**；支持主/从模式 | 需外接 Codec；16 位线性编码；**支持长帧和短帧模式**；支持主/从模式，长帧下只用作主模式 |

**图 16：PCM 和 I2C 接口电路参考设计**（带外部 Codec）
- Module 侧 PCM_CLK→BCLK、PCM_SYNC→LRCK、PCM_DOUT/PCM_OUT→DAC、PCM_DIN/PCM_IN→ADC。
- I2C_SCL→SCL、I2C_SDA→SDA，各经 `4.7K` 上拉至 `1.8 V`。
- Codec 侧：MICBIAS、INP/INN（BIAS）、LOUTP/LOUTN。

> 备注：
> 1. 建议在 PCM 信号线上预留 RC（R = 22 Ω, C = 22 pF）电路，特别是 PCM_CLK 上。
> 2. 三款模块在 I2C 相关应用中只能作为主设备。

## 4.8 ADC 接口

**表 13：ADC 接口对比**

| 功能 | EC200A-CN(TA) QuecOpen | EC20 R2.1 & EC20-CN QuecOpen |
|---|---|---|
| ADC 接口 | 两路 ADC；电压范围 0~VBAT_BB；分辨率 12 bits | 两路 ADC；电压范围 0.3 V~VBAT_BB；分辨率 15 bits |

> 备注：
> 1. 每个 ADC 引脚输入电压不能超过其各自相应的电压范围。
> 2. 模块 VBAT 不供电时，ADC 接口不能直接接任何输入电压。

## 4.9 网络状态指示

三款模块都有 NET_STATUS 网络状态引脚，用于驱动网络状态指示灯。

**表 14：网络指示引脚的工作状态**

| 引脚名 | 引脚工作状态 | 所指示的网络状态 |
|---|---|---|
| NET_STATUS | 慢闪（200 ms 高 / 1800 ms 低） | 找网状态 |
| NET_STATUS | 慢闪（1800 ms 高 / 200 ms 低） | 待机状态 |
| NET_STATUS | 快闪（125 ms 高 / 125 ms 低） | 数据传输模式 |
| NET_STATUS | 高电平 | 通话中 |

**图 17：网络指示参考电路**
- 模块"网络状态指示"引脚经三极管驱动 LED；上拉 `2.2K`@VBAT、基极 `4.7K`、`47K`。

## 4.10 STATUS

三款模块的 STATUS 为开漏输出引脚，用于指示工作状态。可连接带上拉的 GPIO 或 LED 指示电路。模块正常开机时 STATUS 输出低电平，否则为高阻态。

**图 18：STATUS 参考电路**（两种，任选其一）
- 方案一：STATUS → `10K` 上拉至 `VDD_MCU`，连 `MCU_GPIO`。
- 方案二：STATUS → `2.2K` 上拉至 `VBAT`，串 LED。

> 备注：
> 1. 模块 VBAT 不供电时，STATUS 不能作为关机状态指示。
> 2. 模块休眠时 STATUS 仍输出低电平驱动 LED，会在 VBAT 上产生额外电流消耗；VBAT 可换为外部可控电源（休眠时断电）以降低休眠耗流。

## 4.11 USB_BOOT 接口

三款模块都支持 USB_BOOT 功能；开机前将 USB_BOOT 上拉至 VDD_EXT，开机时快速进入强制下载模式，可通过 USB 升级软件。

**图 19：USB_BOOT 接口参考电路设计**
- `VDD_EXT` 经 `4.7K` 上拉到测试点（Test points）→ `USB_BOOT`；测试点附近放 `TVS`（靠近测试点）。

**图 20：进入强制下载时序图**

| 时序 | 信号/动作 | 说明 |
|---|---|---|
| T1 | 拉低 PWRKEY（VIL 0.5 V） | 释放后 PWRKEY 高电平：EC200A-CN(TA) 为 VH = VBAT；EC20-CE/EC20-CN 为 VH = 0.8 V |
| T2 | → VDD_EXT 输出 | 在 VDD_EXT 上电前上拉 USB_BOOT 至 1.8 V 可使模块开机后进入强制下载模式 |
| T3 | → RESET_N | - |

> 备注：
> 1. 拉低 PWRKEY 前确保 VBAT 稳定，建议 VBAT 上电到拉低 PWRKEY 间隔 ≥ 30 ms。
> 2. MCU 控制进入强制下载模式时按时序图控制，VBAT 上电前不建议上拉 USB_BOOT 到 1.8 V；手动方式按图 19 短接测试点即可。
> 3. 正常启动时 USB_BOOT 悬空，无需上拉或下拉。

## 4.12 天线接口

为获取更佳射频性能，需预留 π 型匹配电路，电容默认不贴。

**图 21：EC20-CE & EC20-CN & EC200A-CN(TA) QuecOpen 射频参考电路**
- ANT_MAIN → 串 `R1 0R` → 主天线；π 型预留 `C1`(NM)/`C2`(NM) 对地。
- ANT_DIV/ANT_DRX → 串 `R2 0R` → 分集天线；π 型预留 `C3`(NM)/`C4`(NM) 对地。

> 备注：
> 1. 为提高接收灵敏度，需保证主天线和分集接收天线距离合适。
> 2. π 型匹配元件（R1&C1&C2，R2&C3&C4）应尽量靠近天线放置。

**图 22：EC20-CE & EC20-CN QuecOpen GNSS 有源天线参考电路**
- ANT_GNSS → 串 `0R` → GNSS 天线；π 型预留 `NM` 电容、`100 pF`、`47 nH` 电感、`10R`，经 `0.1 µF` 去耦至 `VDD`（有源天线供电）。

> 备注：
> 1. 客户可根据有源天线类型选用外部 LDO 供电。
> 2. 若选用无源天线，则无需设计 VDD 电路。

---

# 5 物理尺寸

本章介绍三款模块的推荐封装及钢网设计。所有尺寸单位为毫米；未标注公差的尺寸公差为 ±0.2 mm。模块焊盘对应钢网厚度推荐 **0.18~0.20 mm**。

## 5.1 推荐兼容封装

**图 23：EC20-CE & EC20-CN & EC200A-CN(TA) QuecOpen 底视图**
- 模块底视焊盘布局图：四周一圈 LCC 周边焊盘 + 中央 LGA 焊盘阵列（三款共用同一焊盘布局）。

**图 24：EC20-CE & EC20-CN & EC200A-CN(TA) QuecOpen 推荐兼容封装（俯视图，不带 RGMII 功能时）**
- 整体焊盘区 **32.0 ±0.15（宽）× 29.0 ±0.15（高）**；Pin 1 位于左上角（带三角标记）。
- 主要标注尺寸：顶部 24.70、3.85、3.45；周边焊盘 4.80；间距 1.10、2.0、3.00、1.30、1.50、3.40、2.50、1.00、0.80、3.20；纵向 15.80。
- 中部 **Keepout area**（禁布区，对应 73~84 焊盘），标注 0.50。

> 备注：
> 1. 三款模块的封装完全兼容。
> 2. EC20-CE & EC20-CN QuecOpen 实际尺寸 29.0 × 32.0 × 2.4 mm；EC200A-CN(TA) QuecOpen 实际尺寸 29.0 × 32.0 × 2.65 mm。
> 3. 使用不带 RGMII 功能时 73~84 焊盘（Keepout area）无需进行原理图和 PCB 设计。
> 4. 为保证模块能够正常安装，请保证 PCB 板上模块和其他元器件之间的距离至少为 3 mm。

## 5.2 安装示意图

**图 25：安装效果图**
- 三维效果图：EC20-CE / EC20-CN / EC200A-CN(TA) 三款模块对齐堆叠在同一 PCB 封装上方，直观展示它们共用同一焊盘、可互换贴装（引脚兼容）。

---

# 6 生产焊接

## 6.1 生产焊接

用印刷刮板在网板上印刷锡膏，使锡膏通过网板开口漏印到 PCB 上。模块焊盘对应钢网厚度推荐 **0.18~0.20 mm**。详见文档 [6]。

推荐回流焊温度 **235~246 °C**，最高不超过 246 °C。强烈推荐完成 PCB 第一面回流焊后再贴模块。

**图 26：推荐的回流焊温度曲线**（无铅 SMT 回流焊）
- 吸热区（Soak Zone）：最大升温斜率 1~3 °C/s，升至约 150~200 °C（A→B）。
- 回流区（Reflow Zone，B→C→D）：最大升温斜率 2~3 °C/s，越过 217 °C，峰值 235~246 °C；冷却降温斜率 -1.5 ~ -3 °C/s。
- 关键温度刻度：100、150、200、217、235、246 °C。

**表 15：推荐的炉温测试控制要求**

| 项目 | 推荐值 |
|---|---|
| **吸热区（Soak Zone）** | |
| 最大升温斜率 | 1~3 °C/s |
| 恒温时间（A 和 B 之间：150~200 °C 期间） | 70~120 s |
| **回流焊区（Reflow Zone）** | |
| 最大升温斜率 | 2~3 °C/s |
| 回流时间（D：超过 220 °C 的期间） | 40~70 s |
| 最高温度 | 238~246 °C |
| 冷却降温斜率 | -1~4 °C/s |
| **回流次数** | |
| 最大回流次数 | 1 次 |

> 注：表 15 中"回流时间"门限（>220 °C）、最高温度（238~246 °C）、冷却斜率（-1~4 °C/s）与正文"图 26"曲线标注（>217 °C、235~246 °C、-1.5~-3 °C/s）存在原文不一致，本文均按原文保留。

> 备注：如需对模块进行喷涂，请确保所用喷涂材料不会与模块屏蔽罩或 PCB 发生化学反应，同时确保喷涂材料不会流入模块内部。

---

# 7 附录　参考文档和术语缩写

**表 17：参考文档**

| 编号 | 文档名称 |
|---|---|
| [1] | Quectel_EC20-CE_AT_Commands_Manual |
| [2] | Quectel_Module_Secondary_SMT_Application_Note |
| [3] | Quectel_EC20-CE_QuecOpen_硬件设计手册 |
| [4] | Quectel_EC20-CN_QuenOpen_硬件设计手册 |
| [5] | Quectel_EC200A-CN(TA)_QuecOpen_硬件设计手册 |
| [6] | Quectel_模块 SMT 应用指导 |

**表 18：术语缩写**

| 术语 | 英文全称 | 中文全称 |
|---|---|---|
| ADC | Analog-to-Digital Converter | 模数转换器 |
| bps | Bits Per Second | 比特/秒 |
| CTS | Clear to Send | 清除发送 |
| FDD | Frequency Division Duplex | 频分双工 |
| GSM | Global System for Mobile Communications | 全球移动通讯系统 |
| HSPA | High-Speed Packet Access | 高速分组接入 |
| LTE | Long Term Evolution | 长期演进 |
| PCB | Printed Circuit Board | 印制电路板 |
| RF | Radio Frequency | 射频 |
| TDD | Time Division Duplexing | 时分双工 |
| UART | Universal Asynchronous Receiver & Transmitter | 通用异步收发传输器 |
| (U)SIM | (Universal) Subscriber Identity Module | （全球）用户识别卡 |
| WCDMA | Wideband Code Division Multiple Access | 宽带码分多址 |
| WLAN | Wireless Local Area Network | 无线局域网 |

---

*（全文完。本 Markdown 依据 PDF V1.0.0 Preliminary 2021-09-23 完整转写；表 7 中 ⚠ 标注对应原文红色字体——封装兼容但功能/电压域不同的引脚。）*
