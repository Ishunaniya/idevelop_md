# Quectel AG35-CET & AG35-EUT QuecOpen Reference Design —— 全文分析

> 文档来源:`Quectel_AG35-CET&AG35-EUT_QuecOpen_Reference_Design_V1.0.pdf`(共 21 页 / 正文编号 0–6)
> 文档系列:LTE Standard Module Series
> 版本:1.0　日期:2025-06-12　状态:Released(已发布)
> 本 md 为逐页完整转录与说明,保留全部章节、原理图说明、注释(NOTE)与电气参数。原理图为矢量图无法内嵌,以"文字详述 + 图注"还原,关键元件位号/参数尽量保留。

---

## 封面

- **产品**:AG35-CET & AG35-EUT
- **标题**:QuecOpen Reference Design(QuecOpen 参考设计)
- **系列**:LTE Standard Module Series
- **Version**:1.0
- **Date**:2025-06-12
- **Status**:Released

---

## 联系方式与法律声明(扉页)

**总部联系方式:**
- Quectel Wireless Solutions Co., Ltd.(上海移远通信)
- 地址:Building 5, Shanghai Business Park Phase III (Area B), No.1016 Tianlin Road, Minhang District, Shanghai 200233, China
- Tel:+86 21 5108 6236
- Email:info@quectel.com
- 销售:http://www.quectel.com/support/sales.htm
- 技术支持 / 文档纠错:http://www.quectel.com/support/technical.htm
- 技术支持邮箱:support@quectel.com

**Legal Notices(法律声明)**:信息作为服务提供,基于客户需求并尽力保证质量;客户须自行做独立分析评估,参考设计仅作示例说明用途(illustrative purposes only);使用任何硬件/软件/服务前须仔细阅读本声明;文档以"as available"提供,移远可自行随时修订而不另行通知。

**Use and Disclosure Restrictions(使用与披露限制)**

- **License Agreements(许可协议)**:除非另有书面许可,所提供文档与信息须保密,且仅限本协议明示用途使用。
- **Copyright(版权)**:本产品及第三方产品可能含受版权保护材料,未经书面同意不得复制、转载、分发、合并、发布、翻译或修改;移远及第三方保留版权专有权;不授予任何专利/版权/商标权许可;购买行为不视为授予普通非排他、免版税许可之外的任何许可;保留对违规、未授权或恶意使用追究法律责任的权利。
- **Trademarks(商标)**:除非另有说明,本文档不得被解释为授予使用移远或第三方任何商标、商号、名称、缩写或假冒产品的权利。
- **Third-Party Rights(第三方权利)**:本文档可能引用第三方拥有的硬件、软件与/或文档("第三方材料"),使用须遵守其相应限制与义务;移远对第三方材料不作任何明示或默示保证(包括适销性、特定用途适用性、安宁享有、系统集成、信息准确性及不侵权等),并不构成对第三方材料的开发、增强、修改、分发、营销、销售或维持生产的承诺。
- **Privacy Policy(隐私政策)**:为实现模块功能,部分设备数据会上传至移远或第三方(包括运营商、芯片供应商或客户指定服务器)服务器;移远严格遵守相关法律法规处理数据;与第三方交互前请知悉其隐私与数据安全政策。
- **Disclaimer(免责声明)**:
  a) 因依赖本信息造成的任何人身伤害或损害,移远不承担责任。
  b) 因任何不准确、遗漏或使用本信息造成的后果,移远不承担责任。
  c) 在研功能/特性可能存在错误、不准确与遗漏;除非有效协议另有约定,不作任何明示或默示保证,并在法律允许最大范围内排除一切相关损失责任(无论该损失是否可预见)。
  d) 移远对第三方网站及资源上的可访问性、安全、准确、可用性、合法性或完整性,以及广告、商业要约、产品、服务和资料不承担责任。
- 版权:Copyright © Quectel Wireless Solutions Co., Ltd. 2025. All rights reserved.

---

## About the Document(关于本文档)

### Revision History(修订历史)

| Version | Date | Author | Description |
|---|---|---|---|
| -（无） | 2023-09-12 | Bob WANG | 文档创建(Creation of the document) |
| 1.0.0 | 2023-09-12 | Bob WANG | 初稿(Preliminary) |
| 1.0 | 2025-06-12 | Hale LI / Vigoss YU | 首个正式发布(First official release),变更如下 ↓ |

**1.0 版首次正式发布的具体变更:**

1. 增加适用模块 **AG35-EUT**。
2. Pin 127 由 **ANT_DRX** 更新为 **ANT_DIV**。
3. 更新 VBAT 电流要求(Sheets 1、2 和 5)。
4. 删除为 PWRKEY 和 VBAT 引脚预留的测试点(Sheets 1 和 14)。
5. 更新模块接口设计中的注释(Sheet 3):
   - 更新关于使用 GNSS_TXD/RXD 的注释;
   - 新增关于增加 33 pF 电容以防止 RF 干扰的注释;
   - 新增关于模块开机前其引脚不得有灌电流(no current sink)的注释。
6. 更新推荐的 VBAT 工作电压(Sheet 5)。
7. 更新 TVS 阵列的寄生电容要求(Sheet 6)。
8. 更新 codec 的 I2C 接口的上拉电阻要求(Sheet 7)。
9. 更新天线接口设计(Sheet 10):
   - 更新 DC 隔直电容的容值并新增相关注释;
   - 更新 GNSS 天线参考电路。
10. 更新电平转换电路——晶体管方案的参考设计(Sheet 11)。
11. 更新关于 USB_VBUS 供电的注释,并新增关于其控制电路的注释(Sheet 11)。
12. 更新 eMMC 应用设计,并删除支持的 eMMC 型号列表(Sheet 12)。
13. 更新强制下载模式(forced download mode)的参考设计(Sheet 14)。

---

## Contents(目录)

- About the Document …… 3
- Contents …… 5
- **1 Reference Design …… 6**
  - 1.1 Introduction …… 6
  - 1.2 Schematics …… 6

---

# 1 Reference Design(参考设计)

## 1.1 Introduction(简介)

本文档提供 Quectel **AG35-CET** 和 **AG35-EUT** 模块在 **QuecOpen®** 方案下的参考设计,涵盖:
- 框图(block diagram)
- 电源(power supply)
- 模块接口(module interfaces)
- (U)SIM 接口
- 音频 codec
- 模拟音频接口(analog audio interface)
- 天线接口(antenna interfaces)
- UART 接口
- USB 接口
- eMMC 应用
- AG35-CET/AG35-EUT 与 **FC30R(Wi-Fi 模块)** 之间的设计

## 1.2 Schematics(原理图)

后续页面给出的原理图仅供参考(for your reference only)。

> **NOTE**:参考设计中涉及的 IC,须向供应商确认其适用性与价格(confirm the applicability and price from the supplier about the IC involved in the reference design)。

---

# 原理图集(Schematics, Sheet 1–14)

> 图框信息(每张图均相同):Quectel Wireless Solutions;Project = AG35-CET&AG35-EUT QuecOpen;Version = 1.0;Drawn by = Hale LI/Vigoss YU;Checked by = Bonni LIN/Abbe SUN;Size = A3。

## Sheet 1 — Block Diagram(系统框图)

整机框图,左侧为客户 **MCU**,中间为 **AG35-CET&AG35-EUT** 模块,右侧为外围器件。主要互连关系:

**MCU 侧:**
- VDD_MCU → VDD
- VBAT_EN → GPIO_01
- VBUS_CTRL → GPIO_02
- CODEC_POWER_EN → GPIO_03
- GPIO_04 / 05 / 06 / 07 经 **Transistor Circuit(晶体管电路)** 分别接模块的 PWRKEY / RESET_N / WAKEUP_IN(GPIO2) / SLEEP_SYS_IND(GPIO5)
- MCU 的 SPI ↔ 模块 SPI1/SPI2
- MCU 的 Ethernet PHY ↔ 模块 RGMII/RMII
- MCU 的 UART 经 **3.3/1.8 V Level-shifting Circuit(电平转换电路)** ↔ 模块 Main UART(见 NOTE 1)

**模块外围:**
- ANT_MAIN / ANT_DIV / ANT_GNSS 天线
- USB Connector ↔ USB
- PCM / I2C → **ALC5616** codec → Loudspeaker or Headset(经 PA_EN);或 MIC/SPK → Loudspeaker or Headset(模拟音频,经 PA_EN)(见 NOTE 2)
- SDIO1 + WLAN Control ↔ **Wi-Fi module (FC30R)** → RF_ANT
- (U)SIM1/(U)SIM2 ↔ (U)SIM 卡
- SDIO2 ↔ **eMMC**
- **Test Points(测试点)**:Debug UART、USB_BOOT、USB、VDD_EXT、RESET_N
- ADC0 / ADC1 / ADC2 → ADC
- STATUS → Status Indication(状态指示)
- NET_STATUS → Network Status Indication(网络状态指示)
- VBAT_BB / VBAT_RF ← VBAT(**3.8 V @ 3.0 A**)

**NOTE:**
1. 若 MCU 的电平不是 1.8 V,建议使用晶体管电路或电压电平转换器(voltage-level translator)。
2. 模拟音频接口与数字音频接口只能二选一,不能同时使用;**推荐使用数字音频接口**。

## Sheet 2 — Power System Block Diagram(电源系统框图)

电源分配链路:
- **12 V Input → DC-DC → DC 5 V Output**(5 V 总线)
- 5 V → **TPS62130A**(由 VBAT_EN 使能)→ **DC 3.8 V @ 3.0 A** → AG35-CET & AG35-EUT
- 5 V → **MOS ON/OFF**(由 VBUS_CTRL 控制)→ **USB_VBUS** → 模块
- 5 V → **TPS62130A**(由 VDD_EXT 使能):
  - 输出 **DC 3.3 V @ 0.8 A** → **MOS ON/OFF**(由 WLAN_PWR_EN 使能)→ **FC30R**
  - 输出 **DC 3.3 V @ 0.2 A** → eMMC(VDD_1V8)
- 5 V → **SGM2019-ADJYN5G/TR**(由 CODEC_POWER_EN 使能)→ **DC 3.3 V** → Codec ALC5616
- 5 V → **SGM2019-ADJYN5G/TR**(由 VDD_EXT 使能)→ **DC 1.8 V**

## Sheet 3 — Module Interface Design(模块接口设计)

模块各功能引脚分组接出(器件位号 U0301A/B/C/D),涵盖:

- **Antenna**:ANT_MAIN / ANT_GNSS / ANT_DIV
- **RGMII/RMII**:RPHY_VDD、RGMII/RMII 各信号(RGMII_RB0_REF_N、RGMII_RB0_ADC、RGMII_RB0_MDIO、RGMII_RB0_INT_N、RGMII_RB0_RXD、RGMII_RB0_TXD…)
- **Others**:HPS、STATUS、NET_STATUS、VDD_EXT、PWRKEY、RESET_N
- **SDIO1 & Control(WLAN)**:WLAN_SLP_CLK、WLAN_WAKE、WLAN_EN、USB_BOOT(见 NOTE 4)、RESERVED9、SDIO1_DATA0~3、SDIO1_CLK、SDIO1_CMD、WLAN_PWR_EN、RF_EN
- **GPIO**:GPIO1(PA_EN)、GPIO2(WAKEUP_IN)、GPIO3…GPIO5(SLEEP_SYS_IND)…GPIO9
- **SPI**:SPI1_CLK、SPI1_CS、SPI1_MISO、SPI1_MOSI、SPI2_CLK、SPI2_CS、SPI2_DIN、SPI2_DOUT
- **(U)SIM**:USIM1_GND、USIM1_DET、USIM1_VDD、USIM1_CLK、USIM1_RST、USIM1_DATA、USIM2_*
- **I2C**:I2C1_SCL、I2C1_SDA、I2C2_SCL、I2C2_SDA(经 R0302 串阻)接 UIC_SCL/UIC_SDA
- **PCM**:CODEC_PCM_DOUT、CODEC_PCM_CLK、CODEC_PCM_DIN、CODEC_PCM_SYNC
- **USB**:USB_VBUS、USB_DM、USB_DP(经 L0301 共模电感,见 NOTE 2)
- **SDIO2(eMMC)**:EMMC_SDIO_DATA0~3、EMMC_SDIO_CMD、EMMC_SDIO_RST_N、EMMC_SDIO_CLK
- **UART**:DBG_RXD、DBG_TXD、GNSS_RXD/GNSS_TXD(见 NOTE 6)、BT_RTS/CTS/RXD/TXD、MAIN_TXD/RXD/RTS/CTS
- **ADC**:ADC0 / ADC1 / ADC2(见 NOTE 3)、RESERVED10
- **Audio**:RESERVED 引脚、SPK_P/SPK_N、MIC_BIAS、MIC_P/MIC_N、HPH
- 大量 **GND** 引脚

**NOTE:**
1. 建议为每个 GPIO 在模块附近预留一个 10 nF 电容。
2. 建议在 USB 连接器与模块之间串联一个共模扼流圈(common-mode choke)以滤除 EMI,共模扼流圈应尽量靠近模块放置。
3. ADC 引脚的输入电压范围是 **0–VBAT_BB**,推荐使用 ADC0/ADC1 接口。
4. 不要将 USB_BOOT(pin 146)上拉至高电平再开机,否则模块无法正常开机。
5. 所有 GND 必须接地;未使用引脚与 RESERVED 引脚保持悬空。
6. 若模块内置 GNSS,保持 GNSS_TXD/RXD 悬空;若模块不内置 GNSS,接外置 GNSS 时 GNSS_TXD/RXD 可用于 GNSS 数据通信。
7. 若 PCM_CLK(pin 67)、SDIO2_CLK(pin 5)、I2C_SCL(pin 43 和 74)、SPI_CLK(pin 80 和 195)及 SDIO1_CLK(pin 19)未使用,建议在这些引脚附近预留 **33 pF** 电容以防止 RF 干扰。
8. 模块开机前,确保模块引脚上**无灌电流(no current sink)**。

## Sheet 4 — MCU Interface Design(MCU 接口设计)

以 **U0401(代表客户 MCU)** 为核心:
- VDD_MCU、GND、TXD/RXD/CTS/RTS(↔ TXD_MCU/RXD_MCU/CTS_MCU/RTS_MCU)
- GPIO_01~10 → VBAT_EN、VBUS_CTRL、CODEC_POWER_EN、ON/OFF_MCU、RESET_MCU、WAKEUP_IN_MCU、SLEEP_SYS_IND…

**三个控制子电路:**
- **Module On/Off Control(开关机控制)**:ON/OFF_MCU 经晶体管 Q0401 → PWRKEY,并联 C0403(NM_15 nF)。
- **Module Reset Control(复位控制)**:RESET_MCU 经晶体管 Q0402 → RESET_N,并联 C0401(NM_15 nF)。
- **Module Wake-up Control(唤醒控制)**:VDD_EXT 经 R0401(10K)上拉,WAKEUP_IN_MCU 经 R0402(0R)/R0403(NM)、Q0403 → WAKEUP_IN,C0402(NM_100 nF)。

**NOTE:**
1. U0401 代表客户 MCU。模块 GPIO 的电源域为 1.8 V;若 U0401 电源域也是 1.8 V,则不需要相关电平转换电路。
2. 建议选用默认低电平的 GPIO 引脚作为模块 PWRKEY 和 RESET_N 的控制引脚。

## Sheet 5 — Power Supply Design(电源供电设计)

含 6 个子电路:

- **Car Battery 12 V Input(车载 12 V 输入)**:VBAT_12V_IN 经磁珠/电感 L0501(100 nF 等)及多颗滤波电容(C0542 100 nF、C0526 100 nF、C0523 1 µF、C0567 100 nF、C0522 10 µF、C0524 1 µF、C0543 100 nF 等)到 5V_IN,含 D0502/D0503 保护。
- **12 V to 5.0 V DC-DC Design**:基于 **TPS54360BQDDARQ1**,Vout = 0.8 × (1 + R0509 / R0518) = **5 V**;含电感 L0503(7.2 µH)、输出电容 C0517/C0518/C0507(47 µF×3)、反馈分压 R0509(95K ±1%)/R0518(18K ±1%)等;输出 VCC_5V。
- **5.0 V to 3.8 V DC-DC Design**:基于 **TPS62133QRGTRQ1**,Vout = 0.8 × (1 + R0523 / R0504) = **3.8 V**;电感 L0504(2.2 µH);反馈 R0523(56K ±1%)/R0504(15K ±1%);输出 VBAT;输入由 VBAT_EN 使能。
- **3.3 V Power Supply for FC30R and eMMC**:基于 **TPS62130QRGTRQ1**,Vout = 0.8 × (1 + R0502 / R0507) = **3.3 V**;电感 L0502(2.2 µH);输出分两路 VDD_3V3_EMMC 与 VDD_3V3_FC30R;FC30R 路经晶体管 Q0504 由 WLAN_PWR_EN 控制。
- **Power Supply for Codec**:基于两颗 **SGM2019-ADJYN5G/TR**:
  - 一路 **VDD_3V3 = 1.207 × (1 + R0505 / R0501) = 3.3 V**(由 CODEC_POWER_EN 经 Q0503 控制)→ VDD_3V3_CODEC
  - 一路 **VDD_1V8 = 1.207 × (1 + R0515 / R0506) = 1.8 V** → VDD_1V8
  - **NOTE(codec 电源):**①CODEC_PWR_EN 为低时 VDD_3V3_CODEC 正常输出,为高时输出关闭。②须遵守上下电时序保证音频 codec 正常工作:上电序 VDD_1V8 先于 VDD_3V3_CODEC;下电序 VDD_3V3_CODEC 先于 VDD_1V8;时序应据实际 codec 应用选择。
- **VBAT Design(VBAT 设计)**:VBAT 分别接 VBAT_BB 与 VBAT_RF;含多颗去耦电容(C0815/C0805/C0804/C0808 等)与 TVS(WS6.5DPUL-AT)。
  - **NOTE:**①VBAT 电源须能为模块提供至少 **3 A** 的足够电流。②VBAT 应以**星型(star)配置**布线到 VBAT_BB 与 VBAT_RF 引脚。③VBAT 推荐工作电压 **3.5–4.3 V**。

## Sheet 6 — (U)SIM Interface Design((U)SIM 接口设计)

**(U)SIM1 Interface** 电路(连接器 J0901,(U)SIM Card Connector):
- USIM1_VDD(经 R0905 10K、R0908 100K,见 NOTE 5)
- USIM1_RST / USIM1_CLK / USIM1_DATA 经串阻 R0607/R0608/R0609(0R,见 NOTE 3)
- USIM1_DET(检测,见 NOTE 2)
- 去耦电容 C0905/C0906/C0905(33 pF,见 NOTE 6)、C0907(100 nF,见 NOTE 1)
- TVS 阵列 D0606/D0605/D0606/D0607/D0608(见 NOTE 4)
- 连接器引脚:VCC、RST、CLK、I/O、RESERVED1、RESERVED2、CD、GND;屏蔽 GND1~GND4

**NOTE:**
1. USIM1_VDD 的去耦电容不应大于 **1 µF**,且必须放置在 (U)SIM 卡座处。
2. 模块提供输入引脚 USIM1_DET 用于检测 (U)SIM 卡的有无。
3. R0607–R0609 用于抑制 EMI、增强 ESD 防护并便于调试,建议使用串联电阻并默认贴 0 Ω 电阻。
4. 建议在 (U)SIM 卡座附近做 ESD 防护;TVS 阵列寄生电容不超过 **15 pF**,且应尽量靠近 (U)SIM 卡座放置。
5. USIM1_DATA 必须经一个 **10 kΩ 上拉电阻(R0905)** 接到 USIM1_VDD;在长走线及敏感环境下可改善抗干扰能力,应靠近 (U)SIM 卡座放置。
6. 建议在 USIM1_DATA、USIM1_CLK、USIM1_RST 走线上加 **33 pF** 电容滤除 RF 干扰。注意容值不可过大,否则会使信号波形变缓、导致读卡失败。
7. (U)SIM2 接口与 (U)SIM1 接口具有相同的设计要求。

## Sheet 7 — Audio Codec Design (Part 1)(音频 codec 设计 第 1 部分)

核心器件 **ALC5616(U0701)**,数字音频(PCM + I2C)方案:
- 电源 VDD_1V8 / VDD_3V3_CODEC,去耦电容组(C0717/C0720/C0719/C0710/C0726…4.7 µF/100 nF/2.2 µF 等)
- I2S/PCM 接口:CODEC_PCM_DOUT → DACDAT1、CODEC_PCM_DIN → ADCDAT1、CODEC_PCM_CLK → BCLK1、CODEC_PCM_SYNC → LRCK1、MCLK
- I2C 接口:I2C_SDA / I2C_SCL(经 R0705/R0706,见 NOTE 3)
- 输出:HPO_R/HPO_L → SPK_R/SPK_L,LOUTR/LOUTL → SPK_R/SPK_N
- MIC 输入:MIC_P/MIC_N(经 C0728/C0712 2.2 µF、R0703 等)、MIC_BIAS
- GPIO19RQ1、SDB 等控制

**NOTE:**
1. ALC5616 上电时序:DBVDD/I2C 上拉电源 → AVDD/DACREF/CPVDD → MICVDD → 软件初始化。
2. ALC5616 下电时序:软件关闭 codec 功能 → MICVDD → DBVDD/I2C 上拉电源 → AVDD/DACREF/CPVDD。
3. 上拉电阻 R0705 和 R0710 建议预留但默认不贴(NM):codec 的 I2C 接口已内部集成上拉电阻,无需外部上拉。若因 I2C 走线过长导致信号质量问题,可加外部上拉电阻以提升性能。
4. 模块上电成功后会自动通过 I2C 初始化 codec,因此 codec 的所有电源须在 codec 初始化前先建立。
5. 让 codec 远离 RF、电源等干扰源,并尽量用地包围 codec 音频信号。

## Sheet 8 — Audio Codec Design (Part 2)(音频 codec 设计 第 2 部分)

两类应用电路:

- **Audio - Headset Application(耳机应用)**:连接器 J0801;MIC_N/MIC_P(经 R0805/R0804)、SPK_L/SPK_R 接耳机;含去耦/ESD 器件(C0808/C0813/C0808 2.2 µF 等、D0807/D0821/D0822 等)。MIC_P 经 R0801 等接 J0801;靠近耳机放置去耦电容。
  - **CTIA / OMTP 选择表:**

    | 位号 | CTIA | OMTP |
    |---|---|---|
    | R0801/R0804 | NM | M（贴） |
    | R0803/R0805 | M（贴） | NM |

- **Audio - Loudspeaker Application(扬声器应用)**:连接器 J0802/J0803;MIC_P/MIC_N、SPK_P/SPK_N 经串阻(R0806/R0808 等);经外部功放(PA_SR 控制);多组去耦电容与 TVS(D0801/D0807/D0808 等)。

**NOTE:**
1. 对扬声器等大功率负载,设计中应增加音频功放(audio power amplifier)。
2. 扬声器最大容性负载为 **330 pF**,麦克风最大容性负载为 **250 pF**。
3. 模拟 GND 应经 0 Ω 电阻(R0802)接到主 GND。
4. J0803 是外部功放的简化扬声器输出电路。为抑制 POP 声,建议通过 PA_EN 控制外部功放,使其在模块音频功能建立后再开启。
5. 耳机应用中,MIC 与 SPK 信号需走差分对(differential pairs)。
6. 所有 MIC 与 SPK 走线应在上下层用地与地平面包围,并远离时钟、DC-DC 等噪声源。

## Sheet 9 — Built-in Analog Audio Interface(内置模拟音频接口)

模块内置模拟音频的 **Audio - Loudspeaker Application** 电路(连接器 J0901/J0902):
- MIC_N/MIC_P(经 C0903/C0908 180 pF 等输入)、MIC_BIAS(经 R0902 等)、SPK_P/SPK_N
- 串阻 R0903/R0904/R0905…(0R)、多组去耦电容(C0901/C0902/C0907/C0911/C0901 等)、ESD/TVS(D0902/D0903/D0904/D0901 等)
- 经 PA_SR 控制外部功放

**NOTE:**(与 Sheet 8 扬声器应用一致)
1. 大功率负载(如扬声器)应增加音频功放。
2. 扬声器最大容性负载 330 pF,麦克风最大容性负载 250 pF。
3. 模拟 GND 应经 0 Ω 电阻(R0901)接主 GND。
4. J0901 为 MIC 输入电路,J0902 为外部功放的简化扬声器输出电路;为抑制 POP 声,建议经 PA_EN 控制外部功放,使其在模块音频功能建立后再开启。
5. 耳机应用中 MIC 与 SPK 信号需走差分对。
6. 所有 MIC 与 SPK 走线应上下层用地包围,并远离时钟、DC-DC 等噪声源。

## Sheet 10 — Antenna Interface Design(天线接口设计)

四类天线接口:

- **Main Antenna Interface(主天线)**:ANT_MAIN → R1001(0R)、隔直电容 C1012(100 pF)→ 连接器 J1003;预留 C1007(NM)、C1009(NM)、ESD D1001。
- **Rx-diversity Antenna Interface(接收分集天线)**:ANT_DIV → R1001(0R)、C1008(100 pF)→ J1002;预留 C1001(NM)、C1002(NM)、ESD D1001。
- **GNSS Antenna Interface(GNSS 天线)**:VDD_GNSS(见 NOTE 3)经 R1009(10R)、C1808(100 pF)、电感 L1001(120 nH)供电;ANT_GNSS → R1002(0R)、C1010(100 pF)→ J1005;预留 C1003/C1004(NM)、ESD D1002。
- **Wi-Fi Module (FC30R) Antenna Interface(Wi-Fi 天线)**:FC30R_ANT → R1004(0R)、C1013(100 pF)→ J1004;预留 C1006(NM)、C1011(NM)、ESD D1002。

**NOTE:**
1. 主天线与分集天线电路必须预留;建议所有天线电路使用双 L 型电路(dual L-type circuits)便于后续调试。RF 天线单端阻抗为 **50 Ω**。
2. RF 天线单端阻抗为 **50 Ω**。
3. VDD_GNSS 的电压取决于实际情况。
4. ESD 防护器件需靠近天线放置以有效防静电;天线接口上 ESD 器件的结电容(junction capacitance)建议为 **0.05 pF**。
5. C1006、C1010、C1012 和 C1013 用作隔直(DC-blocking),容值建议为 **100 pF**。

## Sheet 11 — UART & USB Interface Design(UART 与 USB 接口设计)

四个子电路:

- **Level-shifting Circuit - Transistor Solution(电平转换 - 晶体管方案)**:TXD_MCU 经 Q1106(VDD_EXT、R1102 4.7K、C1101 1nF)→ MAIN_RXD;MAIN_TXD 经 Q1105(VDD_EXT/VDD_MCU、R1103 4.7K、R1104 10K、C1104 1nF)→ RXD_MCU。
- **Level-shifting Circuit - IC Solution (Recommended)(电平转换 - IC 方案,推荐)**:基于 **TXS0104EPWR(U1101)**;VCCA = VDD_EXT,VCCB = VDD_MCU;A1/A2/A3/A4 ↔ B1/B2/B3/B4 对应 MAIN_TXD/MAIN_RXD/MAIN_CTS/MAIN_RTS ↔ TXD_MCU/RXD_MCU/CTS_MCU/RTS_MCU;OE 经 R1101(120K)。
- **USB_VBUS Control(USB_VBUS 控制)**:VCC_5V 经 D1104、R1108(NM_0R)、Q1102(R1106 50K、C1103 10 nF、R1109 100K)由 VBUS_CTRL 控制 → USB_VBUS。
- **USB 2.0 Connector(USB 2.0 连接器)**:Micro_USB(J1101);USB_VBUS、USB_DM、USB_DP、GND;ESD 器件 D1101/D1106/D1102、C1105 1 nF 等。

**NOTE(电平转换):**
1. 有两种电平转换方案:晶体管方案与 IC 方案,**推荐后者(IC 方案)**。
2. TXS0104EPWR 的 VCCA 电源不得超过 VCCB;详情参阅 TXS0104EPWR 数据手册。
3. 晶体管方案不适用于波特率超过 **460 kbps** 的应用;电容 C1101 和 C1104(1 nF)可改善信号质量。

**NOTE(USB):**
1. USB 接口支持 USB 2.0 高速(high-speed)和全速(full-speed)模式,要求 MCU 支持 USB host 模式。
2. USB_VBUS 由该 MOSFET 电路供电(当模块连接到 MCU 时),当连接 PC 时由 PC 供电。USB_VBUS 用于检测 USB 连接。
3. VBUS_CTRL 用于在模块连接到 MCU 时打开/关闭 USB_VBUS 电源。
4. USB 上 ESD 防护器件的寄生电容应小于 **2 pF**。
5. D1104 可在模块经 USB 连接 PC 时防止 USB_VBUS 反灌(backflow)。

## Sheet 12 — eMMC Application Design(eMMC 应用设计)

eMMC 器件(U1201A/U1201B,EMMC 1 of 2 / 2 of 2):
- **EMMC IO**:CLK、CMD、RST_A、DS、DAT0~DAT7,经串阻 R1201~R1208(0R 等)接 EMMC_SDIO_CLK / EMMC_SDIO_CMD / EMMC_SDIO_RST_N / EMMC_SDIO_DATA0~3 等;上拉电阻组(NM_4K 等)接 VDD_1V8
- **POWER**:VCCQ(C1208/C1204)接 VDD_3V3_EMMC、VDD_3V3_EMMC;VCC 各引脚(C1211/C1209)接 VDD_1V8;VSSQ
- **GND**:VSS1~VSS6、VSSQ1~VSSQ6 等
- U1201B 大量 **NC(No Connect)** 引脚

**NOTE:**
1. 不同 eMMC 型号对电容参数要求可能不同,可据相关规格设计。
2. eMMC 详细信息请参考对应器件规格书。

## Sheet 13 — Wi-Fi Module Design (FC30R)(Wi-Fi 模块设计)

**FC30R(U1301A/U1301B)** Wi-Fi 模块设计:
- 电源:**SGM2019-ADJYN5G/TR(U1302)**,VOUT = 1.207 × (1 + R1315 / R1319) = **1.8 V**(R1315 39K ±1%、R1319 75K ±1%),输出 VDD_1V8_FC30R;另有 VDD_3V3_FC30R
- 使能控制:WLAN_EN 经 R1311/R1302、Q1301 控制 VDD_3V3_FC30R;WLAN_WAKE 经 R1305/R1312(4.7K)、Q1302
- FC30R 引脚:RF_ANT、WAKE_HOST、WLAN_EN、VDIO_SDIO、SDIO_D0~D3、SDIO_CLK、SDIO_CMD,经串阻 R1306~R1310 接 WLAN_SDIO_DATA0~3 / WLAN_SDIO_CMD / WLAN_SDIO_CLK;上拉电阻组接 VDD_1V8_FC30R;RESERVED 引脚悬空

**NOTE:**
1. 所有未使用引脚与 RESERVED 引脚保持不连接(悬空)。
2. SDIO 信号远离噪声源。
3. SDIO 信号阻抗须控制在 **50 Ω ±10%**,并尽量整体接地、走线尽量短。
4. SDIO 信号与其他信号的间距须大于两倍线宽;总线负载电容须小于 **15 pF**。
5. 更多内容参阅 FC30R 的参考设计。

## Sheet 14 — Other Designs(其他设计)

三个子电路:

- **Indicator(指示灯)**:VBAT → LED D1401 → R1402(2.2K)→ STATUS。
  - **NOTE:**①STATUS 是开漏输出(open drain output)引脚。②若设备休眠时需要低电流消耗,可将 STATUS 指示灯的电源 VBAT 替换为外部可控电源,在模块进入低功耗模式时关闭以降低功耗。
- **Reserved Test Points(预留测试点)**:连接器 U1401,引出 DBG_RXD、DBG_TXD、USB_BOOT、USB_VBUS、USB_DP、USB_DM、VDD_EXT、RESET_N 等;含 ESD 器件 D1404/D1405。
  - **NOTE:**①为 USB 与调试 UART 接口预留测试点用于抓取日志。②调试 UART 接口支持 1.8 V 电源域;若应用电源域为 3.3 V,应使用电压电平转换器。③USB 数据线上 ESD 器件的寄生电容不应超过 **2 pF**。
- **Forced Download Interface(强制下载接口)**:连接器 J1401;VDD_EXT 经 R1602(1K)、R1601(5K)与 D1606/D1607 接 USB_BOOT。
  - **NOTE:**①建议预留 USB_BOOT 接口设计。②USB_BOOT 默认保持开路;开机前将 USB_BOOT 上拉到 VDD_EXT,模块即进入强制下载模式(forced download mode)。

---

## 附:文档结构与关键事实汇总

- 文档为参考设计,共 **14 张原理图(Sheet 1–14)**,图纸尺寸 A3。
- 适用模块:**AG35-CET** 与 **AG35-EUT** 两款(QuecOpen 方案)。
- 关键电气参数速查:
  - VBAT:**3.8 V @ 3.0 A**,推荐工作电压 **3.5–4.3 V**,电源能力 ≥ **3 A**,星型布线到 VBAT_BB/VBAT_RF。
  - 电源链路:12 V → 5 V(TPS54360BQDDARQ1)→ 3.8 V(TPS62133)/3.3 V(TPS62130)/1.8 & 3.3 V(SGM2019)。
  - RF 天线单端阻抗 **50 Ω**;隔直电容 **100 pF**;天线 ESD 结电容 **0.05 pF**;USB ESD 寄生电容 < **2 pF**;(U)SIM TVS 寄生电容 < **15 pF**。
  - SDIO 信号阻抗 **50 Ω ±10%**,总线负载电容 < 15 pF。
  - 模拟音频容性负载:扬声器 ≤ 330 pF、麦克风 ≤ 250 pF。
  - 数字音频与模拟音频二选一,**推荐数字音频**。
  - 晶体管电平转换波特率上限 **460 kbps**,推荐用 TXS0104EPWR IC 方案。
  - ADC 输入范围 **0–VBAT_BB**,推荐 ADC0/ADC1。
- 与本仓库(modem_mng/EC200A 海外选网等)无直接代码耦合;此为 AG35 硬件参考设计,供硬件选型/原理图核对使用。
