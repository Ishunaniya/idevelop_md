# EC200A-CN(TA) QuecOpen 参考设计手册

> **原文档**：`Quectel_EC200A-CN(TA)_QuecOpen_参考设计手册_V1.0.0_Preliminary_20210819.pdf`
> **适用**：LTE Standard 模块系列 ｜ **版本**：1.0.0 ｜ **日期**：2021-08-19 ｜ **状态**：临时文件（Preliminary）
> **作者/审核**：绘制 Double YANG ／ 审核 Eden LIU ｜ 图纸尺寸 A2
> **说明**：本文档主体是一套 15 页的设计原理图（A2 图纸）。本 md 完整转录文档的全部文字信息——章节、各页图名、电压公式、引脚/信号定义、元件型号、以及**每一页的"备注"逐条原文**，图形线路无法以文字呈现的部分已用结构化文字补足，内容不删减。

---

## 目录

- [文档信息与法律声明](#文档信息与法律声明)
- [文档历史](#文档历史)
- [原目录](#原目录)
- [1. 参考设计](#1-参考设计)
  - [1.1 引言](#11-引言)
  - [1.2 原理图](#12-原理图)
- [原理图各页详解（1/15 ~ 15/15）](#原理图各页详解)
  - [页 1 — 参考设计框图](#页-1--参考设计框图)
  - [页 2 — 电源框图](#页-2--电源框图)
  - [页 3 — 模块接口（引脚定义）](#页-3--模块接口引脚定义)
  - [页 4 — 主控制器接口](#页-4--主控制器接口)
  - [页 5 — 电源设计](#页-5--电源设计)
  - [页 6 — (U)SIM 接口设计](#页-6--usim-接口设计)
  - [页 7 — 音频 Codec 设计（ALC5616）](#页-7--音频-codec-设计alc5616)
  - [页 8 — 音频 Codec 设计（NAU88C10YG）](#页-8--音频-codec-设计nau88c10yg)
  - [页 9 — 音频 Codec 设计（模拟音频接口）](#页-9--音频-codec-设计模拟音频接口)
  - [页 10 — 模拟音频接口设计](#页-10--模拟音频接口设计)
  - [页 11 — 天线设计](#页-11--天线设计)
  - [页 12 — 串口设计](#页-12--串口设计)
  - [页 13 — eMMC Design](#页-13--emmc-design)
  - [页 14 — FC20 系列设计（Wi-Fi/BT）](#页-14--fc20-系列设计wi-fibt)
  - [页 15 — 其他设计（指示灯/测试点/强制下载）](#页-15--其他设计指示灯测试点强制下载)
- [附录 A：电源轨与稳压方案汇总](#附录-a电源轨与稳压方案汇总)
- [附录 B：全文"备注"索引](#附录-b全文备注索引)

---

## 文档信息与法律声明

**联系方式**

- 上海移远通信技术股份有限公司
- 地址：上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼，邮编 200233
- 电话：+86 21 51086236 ｜ 邮箱：info@quectel.com
- 当地办事处：http://www.quectel.com/cn/support/sales.htm
- 技术支持/文档反馈：http://www.quectel.com/cn/support/technical.htm ｜ support@quectel.com

**前言**：移远提供该文档内容用以支持其客户的产品设计。客户须按照文档中提供的规范、参数来设计其产品。因未能遵守有关操作或设计规范而造成的损害，移远不承担任何责任。在未声明前，移远有权对该文档进行更新。

**免责声明**：移远尽力确保开发中功能的完整性、准确性、及时性或效用，但不排除上述功能错误或遗漏的可能。除非其他有效协议另有规定，否则移远对开发中功能的使用不做任何暗示或明示的保证。在适用法律允许的最大范围内，移远不对任何因使用开发中功能而遭受的损失或损害承担责任，无论此类损失或损害是否可以预见。

**保密义务**：除非移远特别授权，否则我司所提供文档和信息的接收方须对接收的文档和信息保密，不得将其用于除本项目的实施与开展以外的任何其他目的。未经移远书面同意，不得获取、使用或向第三方泄露我司所提供的文档和信息。对于任何违反保密义务、未经授权使用或以其他非法形式恶意使用所述文档和信息的违法侵权行为，移远有权追究法律责任。

**版权申明**：本文档版权属于上海移远通信技术股份有限公司，任何人未经我司允许而复制转载该文档将承担法律责任。版权所有 ©上海移远通信技术股份有限公司 2021，保留一切权利。Copyright © Quectel Wireless Solutions Co., Ltd. 2021.

---

## 文档历史

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| - | 2021-08-19 | Double YANG | 文档创建 |
| 1.0.0 | 2021-08-19 | Double YANG | 临时版本 |

---

## 原目录

- 文档历史 ……… 1
- 目录 ……… 3
- **1 参考设计** ……… 4
  - 1.1 引言 ……… 4
  - 1.2 原理图 ……… 4

---

## 1. 参考设计

### 1.1 引言

本文档为 EC200A-CN(TA) QuecOpen® 模块的参考设计，主要包含**电源、串口、(U)SIM、音频**等接口设计。

### 1.2 原理图

如下为 EC200A-CN(TA) QuecOpen 模块的设计原理图。**本设计仅作参考。** 原理图共 15 页（A2 图纸），逐页详解见下文。

---

## 原理图各页详解

> 每页标题对应图纸标题栏的"文档类型：参考设计"。以下逐页给出：图名、核心内容/信号、关键元件与公式、以及该页"备注"原文。

### 页 1 — 参考设计框图

整机参考设计框图，展示 EC200A-CN(TA) QuecOpen 模块与外围的连接关系。

- **MCU 侧电源/控制**：VDD_MCU、VDD；GPIO_01→VBAT_EN、GPIO_02→VBUS_CTRL、GPIO_07→CODEC_POWER_EN、GPIO_09→SD_PWR_EN；GPIO_03/04/05/06/08 经晶体管电路对接模块的 PWRKEY、RESET_N、MAIN_DTR、W_DISABLE#(GPIO4)、AP_READY(GPIO2) 等控制线。
- **天线**：分集天线 ANT_DRX、主天线 ANT_MAIN、GNSS 天线 ANT_GNSS（**内置 GNSS 为可选**）。
- **音频**：PCM + I2C 接 Codec **ALC5616 或 NAU88C10YG**（二选一），经 MIC/SPK 接话柄或耳机。
- **Wi-Fi/SD**：WLAN_SDIO 接 **FC20 系列**（FC20 系列天线）；SD_SDIO 接 **eMMC**。
- **(U)SIM**：(U)SIM 卡。
- **其它接口**：USB_BOOT、Debug UART、ADC0/ADC1（12-bit ADC，0～VBAT_BB）、STATUS、NET_MODE(GPIO5)、NET_STATUS（状态指示）、USB、Main UART（经 3.3/1.8 V 电平转换）。
- **供电**：VBAT_BB / VBAT_RF / VBAT，主供电 **3.8 V / 3.0 A**。
- **图内备注**：
  1. 使用三极管电平转换电路，或使用电平转换器。
  2. 内置 GNSS 功能为可选功能。
  3. 框选部分功能 2 选 1。

### 页 2 — 电源框图

电源链路框图（示例输入 DC 12 V）：

| 级 | 器件 | 输出 | 使能 | 去向 |
|---|---|---|---|---|
| 一级降压 | DC-DC | DC 5 V | — | 总线 5 V |
| 模块主电源 | TPS62130A | **DC 3.8 V @ 3.0 A**（VBAT_EN 使能） | VBAT_EN | EC200A-CN(TA) QuecOpen |
| USB VBUS | MOS ON/OFF（VBUS_CTRL 使能） | USB_VBUS | VBUS_CTRL | 模块 USB 检测 |
| Wi-Fi 电源 | MOS ON/OFF（WLAN_PWR_EN 使能） | DC 3.3 V @ 0.8 A | WLAN_PWR_EN | FC20 系列 |
| eMMC 电源 | TPS62130A（VDD_EXT 使能） | DC 3.3 V @ 0.2 A | VDD_EXT | eMMC |
| Codec 3.3 V | SGM2019-ADJYN5G/TR（CODEC_POWER_EN 使能） | DC 3.3 V | CODEC_POWER_EN | Codec ALC5616 或 NAU88C10YG |
| Codec 1.8 V | SGM2019-ADJYN5G/TR（VDD_EXT 使能） | DC 1.8 V | VDD_EXT | Codec |

### 页 3 — 模块接口（引脚定义）

本页为模块 LCC 封装引脚原理图符号（U0101 拆分为 A/B/C/D 段）。下表为从图中可辨识的**信号—引脚号**对应（GND 引脚众多，逐一接地）：

| 引脚 | 信号 | 引脚 | 信号 |
|---|---|---|---|
| 1 | GPIO1 / WAKEUP_IN | 2 | GPIO2 / AP_READY |
| 3 | GPIO3 | 4 | GPIO4 / W_DISABLE# |
| 5 | GPIO5 / NET_MODE | 6 | NET_STATUS |
| 7 | VDD_EXT | 8、9 | GND |
| 10 | USIM_GND | 11 | DBG_RXD |
| 12 | DBG_TXD | 13 | USIM_DET |
| 14 | USIM_VDD | 15 | USIM_DATA |
| 16 | USIM_CLK | 17 | USIM_RST |
| 18 | RESERVED | 37 | SPI_CS |
| 38 | SPI_DOUT | 39 | SPI_DIN |
| 40 | SPI_CLK | 41 | I2C_SCL |
| 42 | I2C_SDA | 43 | RESERVED |
| 44 | ADC1（ADC1_INPUT，经 R0105/R0106） | 45 | ADC0（ADC0_INPUT，经 R0104 0R） |
| 46 | GND | 47 | ANT_GNSS（R0103 0R；无 GNSS 时为 RESERVED） |
| 48 | GND | 49 | ANT_MAIN |
| 50~52、53、54 | GND | 73 | RGMII_RX_1 |
| 74 | RGMII_CTL_RX | 75 | RGMII_CK_RX |
| 76 | RGMII_RX_0 | 77 | RGMII_TX_0 |
| 78 | RGMII_TX_1 | 79 | RGMII_RX_2 |
| 80 | RGMII_TX_2 | 81 | RGMII_CTL_TX |
| 82 | RGMII_RX_3 | 83 | RGMII_CK_TX |
| 84 | RGMII_TX_3 | 85~112 | GND（多数） |
| 113~116 | （USB/控制相关）USB_BOOT、RESET_N、PWRKEY 等 | 117 | RESERVED |
| 118 | WLAN_SLP_CLK | 119 | RESERVED |
| 120 | RGMII_INT | 121 | RGMII_MD_IO |
| 122 | RGMII_MD_CLK | 123 | SPK_N |
| 124 | WLAN_WAKE | 125 | SPK_P / WLAN_EN |
| 126 | MIC_P | 127 | MIC_N |
| 128 | MICBIAS / MODULE_MICBIAS | 129 | WLAN_SDIO_DATA3 |
| 130 | WLAN_SDIO_DATA2 | 131 | WLAN_SDIO_DATA1 |
| 132 | WLAN_SDIO_DATA0 | 133 | WLAN_SDIO_CLK |
| 134 | WLAN_SDIO_CMD | 135、136 | WLAN_WAKE |
| 137 | WLAN_EN | 138、139、140 | RESERVED |
| 141~144 | RESERVED | — | — |

> 其他经本页引出的信号：MAIN_CTS/MAIN_RTS/MAIN_TXD/MAIN_RXD（主串口）、MAIN_DTR、USB_DP/USB_DM（经共模电感 L0101 + R0101/R0102 NM_0R，USB_DP_TEST/USB_DM_TEST 测试点）、USB_VBUS（DLM0NSN900HY2D）、PCM_DOUT/PCM_SYNC/PCM_CLK/PCM_DIN、SD_SDIO_DATA0~3 / SD_SDIO_CMD / SD_SDIO_CLK / SD_SDIO_VDD、CODEC_PCM_DIN/DOUT/SYNC/CLK、EMMC_RST、ANT_DRX、STATUS、SD_DET、VBAT_BB / VBAT_RF。

**本页备注（原文）：**

1. 所有 GND 引脚需做接地处理，其他不用的引脚和 RESERVED 引脚保持悬空。
2. 建议 MCU 与模块间 USB 通信时串联共模电感 L0101 以滤除 EMI 干扰，电感尽量靠近模块侧放置。同时预留 USB 升级测试点，且分支走线尽量短，2 个电阻均靠近模块的 USB 接口，以保证单条 USB 通路的信号完整性。
3. 建议 ADC 引脚预留电阻分压电路。
4. 强烈建议天线电路预留 π 型电路，便于后期调试。射频天线的单端阻抗为 50 Ω。
5. 在模块开机成功前，禁止 USB_BOOT 引脚上拉到高电平。
6. 当使用内置 GNSS 模块时，47 号引脚可以使用，63、66 号引脚不能使用；当使用内置不带 GNSS 模块时，47 号引脚不能使用，63、66 号引脚可以使用。

### 页 4 — 主控制器接口

展示客户 MCU（U0201）与模块之间的电平转换与控制电路。

- **MCU 串口/USB 引出**：TXD→RXD_MCU、RXD→TXD_MCU、CTS→CTS_MCU、RTS→RTS_MCU、RI、DCD、DTR；USB_VBUS、USB_D+→USB_DP、USB_D-→USB_DM、USB_ID。
- **MCU GPIO 分配**：
  - GPIO_01 → VBAT_EN
  - GPIO_02 → VBUS_CTRL
  - GPIO_03 → ON/OFF_MCU
  - GPIO_04 → RESET_MCU
  - GPIO_05 → W_DISABLE_MCU
  - GPIO_06 → SLEEP_STATUS_MCU
  - GPIO_07 → CODEC_POWER_EN
  - GPIO_08 → WAKEUP_IN_MCU
  - GPIO_09 ~ GPIO_13（备用）
- **模块开关机控制**：PWRKEY 经 Q0206（DTC043ZEBTL）+ C0203 100 nF，由 ON/OFF_MCU 驱动。
- **模块复位控制**：RESET_N 经 Q0207（DTC043ZEBTL），由 RESET_MCU 驱动。
- **模块唤醒控制**：WAKEUP_IN 经 Q0201（DTC043ZEBTL）+ C0205 100 nF，由 WAKEUP_IN_MCU 驱动。
- **模块飞行模式控制**：W_DISABLE# 经 Q0202（DTC043ZEBTL）+ C0201 100 nF，由 W_DISABLE_MCU 驱动。
- **睡眠状态**：AP_READY 经 Q0204（2SC4617TLQ）+ R0201 10K → SLEEP_STATUS_MCU。
- **USB VBUS 通断**：来自主板 5 V（DC_5V）经 Q0203（SI2333CDS-T1，P-MOS）+ Q0205（DTC043ZEBTL，由 VBUS_CTRL 控制）+ R0202/R0204 4.7K + C0202 1 nF → USB_VBUS。

**本页备注（原文）：**

1. U0201 指代客户的 MCU。EC200A-CN(TA) QuecOpen 模块的 GPIO 接口是 1.8 V 电压域，如果 U0201 的 GPIO 为 1.8 V 电压域，则相关的电平转换电路可以忽略。
2. EC200A-CN(TA) QuecOpen 模块的 USB 接口支持 USB 2.0 高速和全速模式，因此主控制器必须支持 USB 主模式或者 OTG 功能。模块和主控制器的 USB_VBUS 作为输入源，需要由外部提供。模块的 USB_VBUS 用于 USB 检测。VBUS_CTRL 用于控制 USB_VBUS 电源的通断。
3. 建议客户 MCU 端选用默认低电平的 GPIO 口作为 EC200A-CN(TA) QuecOpen 模块 PWRKEY 和 RESET_N 的控制引脚。

### 页 5 — 电源设计

涵盖模块主电源、VBAT、PCM Codec 供电、FC20/eMMC 供电等多组稳压电路。

**模块电源设计（DC 12 V → DC 5 V → VBAT 3.8 V）**

- 一级：DC12V 经 F0301 保险 + SS5P4 + L0301 100μH + TPS54560（U0301，BOOT/SW/VIN/EN/RT/CLK/COMP/FB/GND 引脚）→ **DC_5V**，反馈分压公式：
  - `DC_5V = (R0322 / R0323 + 1) × 0.8 = 5 V`
- 二级：DC_5V 经 **TPS62130A（U0302）** + L0302 7.2μH / L0303 2.2μH → **VBAT**，公式：
  - `VBAT = (R0317 / R0316 + 1) × 0.8 = 3.8 V`
  - VBAT_EN 使能；输出滤波 C0324/C0327/C0328/C0333/C0334（47μF×3、22μF、100nF）等。
- 保护：SM6S20A（TVS）、SS5P4/B560C 肖特基、WS4.5DPVL。

**VBAT 设计**

- VBAT 分别就近 **VBAT_BB 引脚**与 **VBAT_RF 引脚**布线（星型）。
- 去耦：C0325/C0301/C0302（100 μF、100 nF、33 pF、10 pF）就近 VBAT_BB；C0303/C0304/C0305/C0306 就近 VBAT_RF。
- **本块备注**：
  1. VBAT 供电电流需满足 3.0 A 的额定输出能力。
  2. VBAT 走线应该采用星型结构连接到引脚 VBAT_BB 和 VBAT_RF。
  3. VBAT 工作电压范围：3.4～4.5 V。

**PCM Codec 供电方案（VDD_3V3 / VDD_1V8）**

- VDD_3V3：DC_5V 经 **SGM2019-ADJYN5G/TR（U0304）**，CODEC_POWER_EN 经 Q0303（DTC043ZEBTL）控制，公式：
  - `VDD_3V3 = (R0304 / R0308 + 1) × 1.207 = 3.3 V`
- VDD_1V8：DC_5V 经 **SGM2019-ADJYN5G/TR（U0305）**，公式：
  - `VDD_1V8 = (R0310 / R0302 + 1) × 1.207 = 1.8 V`
- **本块备注**：
  1. CODEC_PWR_EN 为低电平时可确保 VDD_3V3 的正常输出；为高电平时，将关闭 VDD_3V3 输出。
  2. 如下的上/下电时序用于确保 codec 工作正常：
     - **上电顺序**：先上电 VDD_1V8，然后 VDD_3V3。
     - **下电顺序**：先下电 VDD_3V3，然后 VDD_1V8。

**FC20 系列、eMMC 卡供电方案（VDD3V3）**

- VDD_EXT/DC_5V 经 **TPS62130A（U0303）** + L0304 2.2μH → VDD3V3，公式：
  - `VDD3V3 = (R0306 / R0307 + 1) × 0.8 = 3.3 V`
- WLAN_PWR_EN 经 Q0302（DTC043ZE）控制；输出 VDD3V3_FC20。
- **本块备注**：使用 Wi-Fi 模块供电需要加入 MOSFET 开关缓慢启动电路，如 FC20 系列。

### 页 6 — (U)SIM 接口设计

- (U)SIM 卡座 J0401（6 VCC / 5 RST / 4 CLK / I/O 3 / GND 1 / VPP 2 / 8 DET）。
- 电源：USIM_VDD 经 R0406 15K 上拉、R0404 51K、C0404 100 nF。
- 信号串阻：USIM_RST(R0401 0R)、USIM_CLK(R0402 0R)、USIM_DATA(R0403 0R)；滤波 C0401~C0403 33 pF。
- ESD：U0401 **ESDA6V8AV6**。

**本页备注（原文）：**

1. (U)SIM 卡座需增加 ESD 防护器件 U0401，器件的寄生电容需不超过 15 pF。
2. (U)SIM 卡座的 GND 建议连接到模块的 USIM_GND 引脚，避免 (U)SIM 卡座的地被干扰。如果客户 PCB 的 GND 很完整，USIM_GND 也可以直接接到 PCB 的 GND。
3. 上拉电阻 R0406 有助于提高 (U)SIM 卡的抗干扰性能，建议靠近 (U)SIM 卡座放置。
4. R0401～R0403 用于调试；电容 C0401～C0403 可用于滤除 EGSM900 干扰。
5. 电容 C0404 的容值须小于 1 μF，并靠近 (U)SIM 卡座放置。
6. 布局走线可参考文档《Quectel_EC200A-CN(TA)_QuecOpen_硬件设计手册》。

### 页 7 — 音频 Codec 设计（ALC5616）

- Codec U0501 **ALC5616**，供电 CPVDD(15)/DCVDD(30)/DBVDD(29)/MICVDD(31)/AVDD(6) 来自 VDD_3V3、VDD_1V8。
- PCM 接口（经 0R 串阻 R0509/R0511/R0513/R0515）：CODEC_PCM_DOUT→DACDAT1(22)、CODEC_PCM_DIN→ADCDAT1(21)、CODEC_PCM_CLK→BCLK1(24)、CODEC_PCM_SYNC→LRCK1(23)、MCLK(25)。
- I2C：SDA(27)/SCL(26)，经 R0517/R0518 4.7K 上拉到 VDD_1V8。
- 输出：HPO_R(17)/HPO_L(20)→SPK_R/SPK_L；LOUTL/P(9)、LOUTR/N(10)→SPK_P/SPK_N。
- 输入：IN1P/DMC_DAT(2)、IN2P(3)、IN2N/JD2(4)、MICBIAS1(32)→MIC+/MIC-。

**本页备注（原文）：**

1. ALC5616 上电顺序：DBVDD / I2C 上拉电源 / AVDD / DACREF / CPVDD 上电 → MICVDD 上电 → 软件初始化配置。
2. ALC5616 下电顺序：通过软件关闭所有 Codec 功能 → MICVDD 下电 → DBVDD / I2C 上拉电源 / AVDD / DACREF / CPVDD 下电。
3. 模块在 PWRKEY 被拉低开机之后会自动通过 I2C 初始化 Codec，所以在此之前 Codec 所有电源均需要上电。
4. 模拟地与数字地之间需要用封装为 R-0805 的 0 Ω 电阻连接，具体可参考"音频 Codec 设计（模拟音频接口）"页面。
5. 最大输出功率：耳机驱动，负载为 32 Ω 时，输出功率为 30 mW。

### 页 8 — 音频 Codec 设计（NAU88C10YG）

- Codec U0601 **NAU88C10YG**（与 ALC5616 二选一）。
- **供电引脚表（原文）：**

| 引脚号 | 引脚名 | 电压范围 | 描述 |
|---|---|---|---|
| 2 | VDDA | 2.5～3.6 V | 模拟 VDD |
| 4 | VDDD | 1.71～3.6 V | 数字 VDD |
| 17 | VDDSPK | 2.5～5.5 V | SPK 供给电压 |

- PCM 接口（经 22R 串阻 R0604~R0607）：CODEC_PCM_CLK / CODEC_PCM_SYNC / CODEC_PCM_DOUT / CODEC_PCM_DIN 对应 SCLK(11)/FS(8)/DACIN(7)/ADCOUT(6)/MCLK(10)/BCLK(9)。
- I2C：SDIO(12)/SCLK(11) 经 R0608/R0609 4.7K 上拉；R0621/R0601 10K。
- 输出：SPKOUT-(16)/SPKOUT+(14)→SPK_N/SPK_P（经 R0618/R0619 0R）；MOUT(13)→SPK_R/SPK_L。
- 输入：MIC+(20)/MIC-(19)→MIC_P/MIC_N（R0611/R0612 0R）；MICBIAS(1)、VREF(18)、VDDSPK(17)、VSSSPK(15)。

**本页备注（原文）：**

1. Codec 音频信号尽可能包地处理，同时，在摆件时，Codec 部分应当远离射频以及电源等干扰源。
2. VDDA 引脚的电压要始终不低于 VDDD 引脚的电压。
3. 模拟地与数字地之间需要用封装为 R-0805 的 0 Ω 电阻连接，具体可参考"音频 Codec 设计（模拟音频接口）"页面。

> （本页标题栏沿用"参考设计"；备注编号原文跳至 3，未见独立第 4/5 条。）

### 页 9 — 音频 Codec 设计（模拟音频接口）

Codec 的模拟侧外围（话柄应用 / 耳机应用），含 ESD 与 RC 滤波。

- **话柄应用**：MIC_P/MIC_N、SPK_P/SPK_N 经 RC（10 pF/33 pF）+ ESD（D0701~D0704，B0701~B0704 0R）→ 连接器 J0701。
- **耳机应用**：经 J0702；CTIA/OMTP 切换由 R0701/R0702/R0704/R0705 的 M/NM 选择决定：
  - CTIA：R0702/R0705 = NM / M；
  - OMTP：R0701/R0704 = M / NM。
  - ESD：ESD9X5.0ST5G、PESD5V0S1BL、D0705~D0707；MIC_P 经 R0703 0R（R-0805）。

**本页备注（原文）：**

1. 音频 Codec 的模拟输出只能驱动话柄和耳机，对于扬声器等其他大功率负载应用，设计上需考虑增加音频功放。
2. 话柄应用中，MIC 和 SPK 信号均需要差分走线。
3. 耳机应用中，MIC 信号需要差分走线。
4. 所有 MIC 和 SPK 信号均需要上下左右立体包地，远离干扰源。
5. 音频 Codec 设计中，ALC5616 和 NAU88C10YG 只能二选一。

### 页 10 — 模拟音频接口设计

模块**自带模拟音频**（与 Codec 方案二选一）的麦克风偏置与接口电路。

- **麦克风偏置电路**：MODULE_MICBIAS 经 C0823 100 nF；R0801/R0804 510R、R0802/R0803 1.5K、C0822 2.2 μF；MODULE_MIC_P/MODULE_MIC_N 经 C0814/C0815 100 nF → MICP/MICN。
- **麦克风应用 / 话柄应用**：MICP/MICN、MODULE_SPK_P/MODULE_SPK_N 经 10 pF/33 pF RC + ESD（D0801~D0807）+ B0801~B0804 0R → 接口 J0801（MIC）、J0802。

**本页备注（原文）：**

1. MIC 和 SPK 信号均需要差分走线。
2. 所有 MIC 和 SPK 信号均需要上下左右立体包地，远离干扰源。
3. 在音频设计中，模块模拟音频和 Codec 可选其一，不需要都设计在电路里。
4. 模块的模拟输出只能驱动听筒，对于扬声器等其他大功率负载应用，设计上需考虑增加音频功放。

### 页 11 — 天线设计

四路天线，均建议预留 π 型匹配电路。

| 天线 | 接口 | 匹配/元件 | 信号 |
|---|---|---|---|
| 主天线电路 | J0901 | R0901 0R + C0901/C0902 NM（π） | ANT_MAIN |
| 分集接收天线电路 | J0902 | R0902 0R + C0903/C0904 NM（π） | ANT_DRX |
| GNSS 天线电路（有源） | J0904 | VDD_GNSS + C0905 0.1μF + R0903 10R + L0901 47nH + C0908 100pF + R0905 0R + C0909/C0910 NM（π） | ANT_GNSS |
| FC20 系列天线设计 | J0903 | R0904 0R + C0906/C0907 NM（π） | FC20_ANT |

**本页备注（原文）：**

1. 强烈建议主天线、分集接收天线、GNSS 天线及 FC20 系列天线电路预留 π 型电路，便于后期调试。
2. 分集接收功能默认打开，如果不使用分集天线，需使用 AT 命令关闭分集接收功能，关于 AT 命令的详细信息，可参考《Quectel_EC200A-CN(TA)_QuecOpen_硬件设计手册》。
3. 射频天线的单端阻抗为 50 Ω。
4. VDD_GNSS 电压视实际情况而定。

### 页 12 — 串口设计

主串口（Main UART）电平转换，提供两种方案。

- **三极管转换方案**：RXD_MCU/TXD_MCU 经 Q1001/Q1002（2SC4617TLQ）+ R1003/R1004/R1005/R1006（10K/4.7K）+ C1001/C1002 1 nF → MAIN_RXD/MAIN_TXD。
- **串口转换芯片方案（推荐）**：U1001 **TXS0104E**（VCCA=VDD_EXT，VCCB=VDD_MCU），A1~A4↔B1~B4 对应 MAIN_TXD/MAIN_RXD/MAIN_CTS/MAIN_RTS ↔ TXD_MCU/RXD_MCU/CTS_MCU/RTS_MCU；OE 经 R1001 10K / R1002 120K。

**本页备注（原文）：**

1. 本设计中串口的电平转换电路提供了三极管转换方案和串口转换芯片方案，推荐使用串口转换芯片方案。
2. TXS0104E 的 VCCA 必须小于等于 VCCB，更多设计细节可参考 TXS0104E 芯片数据手册。
3. 三极管方案适用于波特率低于 460 kbps 的应用场合，1 nF 电容有助于改善信号质量。

### 页 13 — eMMC Design

eMMC 存储（U1101，封装 153-ball）设计。

- 数据/命令/时钟经 0R 串阻：SD_SDIO_DATA0~3（R1101/R1102/R1105/R1106 0R）→ DAT0~DAT3；SD_SDIO_CLK（R1110 0R）→ CLK；SD_SDIO_CMD（R1111 0R）→ CMD；EMMC_RST → RSTN；DAT4~DAT7、DS、VCCQ/VCC/VDDI/VSS 等。
- 供电 VDD_1V8（VCCQ）、VDD3V3（VCC）；去耦 C1108~C1113。

**eMMC 去耦电容（按型号，原文表）：**

| 型号 | C1108 | C1109 | C1110 | C1111 | C1112 | C1113 |
|---|---|---|---|---|---|---|
| eMMC08G-S100 | 100 nF | 2.2 μF | 100 nF | 1 μF | 100 nF | 1 μF |
| SDINBDG4-8G-I | 100 nF | 4.7 μF | 100 nF | 4.7 μF | 100 nF | 1 μF |
| MTFC4GMDEA-4M IT | 100 nF | 2.2 μF | 100 nF | 2.2 μF | 100 nF | 1 μF |

**本页备注（原文）：**

1. EC200A-CN(TA) QuecOpen 支持的型号有：eMMC08G-S100、SDINBDG4-8G-I、MTFC4GMDEA-4M IT。
2. 不同类型电容值要求：见上表（C1108~C1113）。
3. eMMC 详细信息请参考相应的器件规格书。
4. SD_SDIO 默认支持 eMMC，也可以通过软件配置成 SD 卡，两者不能同时支持，想要了解更多的细节，请联系移远通信技术支持。

### 页 14 — FC20 系列设计（Wi-Fi/BT）

FC20 系列 Wi-Fi/BT 模组（U1201，分 A/B 段）设计。

- **接口**：DBG_TXD(4)、LTE_UART_TXD(5)、LTE_UART_RXD(6)、BT_UART_RTS(7)、BT_UART_CTS(8)、WLAN_EN(9)、BT_EN(10)、VIO(11)、PCM_IN(13)、PCM_SYNC(14)、PCM_CLK(15)、PCM_OUT(16)、BT_UART_TXD(17)、BT_UART_RXD(18)、32KHZ_IN(19)、VDD_3V3(21)、SDIO_D3(22)/D2(23)/D1(24)/D0(25)、SDIO_CLK(26)、SDIO_CMD(27)、WAKE_ON_WIRELESS(32)、RF_ANT(30)→FC20_ANT。
- **SDIO 串阻 0R**：R1206~R1210/R1217 → WLAN_SDIO_DATA0~3 / CLK / CMD（接模块）。
- 控制：WLAN_WAKE（R1203 0R）、WLAN_EN（U1201-A）、VDD_EXT 上拉 R1204 10K；VDD3V3_FC20。R1212 必须要贴。
- 32.768 kHz：C1201~C1204；预留 R1201 NM_0R 调试。

**本页备注（原文）：**

1. 将所有不用的引脚和 RESERVED 引脚悬空。
2. WLAN_SDIO 信号走线应远离噪声信号（如 CLK、DC-DC），同时，其他敏感电路也要远离 SDIO 接口信号。
3. WLAN_SDIO 信号线阻抗需要控制在 50 Ω ± 10 %，需要立体包地，布线长度小于 50 mm。
4. WLAN_SDIO 信号走线需做等长处理，WLAN_SDIO_CLK、WLAN_SDIO_DATA 和 WLAN_SDIO_CMD 之间的走线长度之间相差需小于 1 mm，而且总的长度应小于 50 mm。因为模块内部走线为 30 mm，所以外部走线长度应该小于 20 mm。
5. WLAN_SDIO 信号线距离其他信号线必须大于 2 倍线宽，总线负载电容小于 15 pF。
6. FC20 系列的 VIO 需要在 VDD_3V3 之前上电。

### 页 15 — 其他设计（指示灯/测试点/强制下载）

**指示灯电路：**

| 指示 | 元件 | 电源 |
|---|---|---|
| STATUS | D1303 + R1301 2.2K（开漏直驱） | VBAT |
| NET_MODE | D1301 + R1302 2.2K + Q1301（DTC043ZEBTL） | DC_5V |
| NET_STATUS | D1302 + R1303 2.2K + Q1302（DTC043ZEBTL） | DC_5V |

- **指示灯备注（原文）：**
  1. 模块的 STATUS 引脚为开漏输出结构。
  2. 关于 NET_MODE 和 NET_STATUS 的指示详情，可参考文档《Quectel_EC200A-CN(TA)_QuecOpen_硬件设计手册》。
  3. 客户端对整机睡眠有低功耗要求时，可将 STATUS、NET_MODE 和 NET_STATUS 指示灯电源 VBAT 和 DC_5V 更换为外部可控电源，并在模块睡眠时关断，以减小睡眠功耗。

**预留测试点（连接器 J1302）**：VBAT、PWRKEY、USB_VBUS、USB_DM_TEST、USB_DP_TEST、DBG_RXD、DBG_TXD、DBG_TXD_FC20，配 ESD（D1305~D1310 ESD9L5.0ST5G / ESD9X3.3ST5G、SD12C.TCT）。

- **测试点备注（原文）：**
  1. 模块 USB 接口和调试串口都预留测试点用于软件抓取日志。
  2. USB 接口还可以预留测试点用于模块软件升级。
  3. USB 信号线上的 ESD 寄生电容不超过 2 pF。
  4. 调试串口的电平为 1.8 V，与 3.3 V 系统连接时需要电平转换芯片。

**强制下载（连接器 J1301）**：USB_BOOT 经 R1305 + R1304 4.7K + D1312（SZESD9X3.3ST5G）/ D1304（ESD9X3.3ST5G），由 VDD_EXT 上拉。

- **强制下载备注（原文）：**
  1. 强烈建议客户预留 USB_BOOT 接口设计。
  2. USB_BOOT 默认悬空，在 VDD_EXT 上电前上拉；USB_BOOT 可以使模块开机后快速进入紧急下载模式。

---

## 附录 A：电源轨与稳压方案汇总

| 电源轨 | 稳压器件 | 使能信号 | 输出/能力 | 反馈公式 |
|---|---|---|---|---|
| DC_5V | TPS54560（U0301） | — | 5 V | `DC_5V = (R0322/R0323 + 1) × 0.8` |
| VBAT | TPS62130A（U0302） | VBAT_EN | 3.8 V @ 3.0 A（范围 3.4~4.5 V） | `VBAT = (R0317/R0316 + 1) × 0.8` |
| VDD_3V3（Codec） | SGM2019-ADJYN5G/TR（U0304） | CODEC_POWER_EN（低有效） | 3.3 V | `VDD_3V3 = (R0304/R0308 + 1) × 1.207` |
| VDD_1V8（Codec/系统） | SGM2019-ADJYN5G/TR（U0305） | — | 1.8 V | `VDD_1V8 = (R0310/R0302 + 1) × 1.207` |
| VDD3V3（FC20/eMMC） | TPS62130A（U0303） | VDD_EXT | 3.3 V | `VDD3V3 = (R0306/R0307 + 1) × 0.8` |
| USB_VBUS | MOS（Q0203/Q0205） | VBUS_CTRL | 由主板 5 V 通断 | — |
| Wi-Fi 3.3 V | MOS 缓启动（FC20 系列） | WLAN_PWR_EN | 3.3 V @ 0.8 A | — |

**Codec 电源时序**：上电 VDD_1V8 → VDD_3V3；下电 VDD_3V3 → VDD_1V8。

---

## 附录 B：全文"备注"索引

| 页 | 主题 | 备注条数 |
|---|---|---|
| 1 | 参考设计框图 | 3 |
| 3 | 模块接口 | 6 |
| 4 | 主控制器接口 | 3 |
| 5 | 电源设计（VBAT / Codec 供电 / FC20-eMMC 供电） | 3 + 2 + 1 |
| 6 | (U)SIM 接口设计 | 6 |
| 7 | 音频 Codec（ALC5616） | 5 |
| 8 | 音频 Codec（NAU88C10YG） | 3 |
| 9 | 音频 Codec（模拟音频接口） | 5 |
| 10 | 模拟音频接口设计 | 4 |
| 11 | 天线设计 | 4 |
| 12 | 串口设计 | 3 |
| 13 | eMMC Design | 4 |
| 14 | FC20 系列设计 | 6 |
| 15 | 其他设计（指示灯/测试点/强制下载） | 3 + 4 + 2 |

**反复出现的关键设计约束（跨页汇总）：**

- 不用的引脚与 RESERVED 引脚一律**悬空**；GND 全部接地。
- 射频/天线单端阻抗 **50 Ω**，全部天线预留 **π 型**匹配。
- USB ESD 寄生电容 < 2 pF；(U)SIM ESD 寄生电容 < 15 pF。
- 模块 GPIO/调试串口为 **1.8 V 电压域**，与 3.3 V 系统互联需电平转换。
- WLAN_SDIO：50 Ω±10 %、立体包地、等长（互差 <1 mm）、外部走线 <20 mm、负载电容 <15 pF。
- 音频模拟地/数字地用 **R-0805 0 Ω** 电阻单点连接；MIC/SPK 差分 + 立体包地。
- GNSS 与否决定 47/63/66 号引脚用法（见页 3 备注 6）。
- 开机成功前禁止 USB_BOOT 上拉到高电平（页 3 备注 5）。
- 详细布局走线、AT 命令、NET 指示灯含义见《Quectel_EC200A-CN(TA)_QuecOpen_硬件设计手册》。

---

*本 md 由原 PDF（15 页 A2 原理图 + 说明）全文转录整理。原理图中的连线拓扑、器件位号与封装均按图面文字如实记录；少量纯图形信息（走线、网络标号位置）无法以文字 1:1 复刻，已以信号/元件清单与备注原文等价呈现。如需精确逐脚封装引脚定义，请配合《硬件设计手册》。*
