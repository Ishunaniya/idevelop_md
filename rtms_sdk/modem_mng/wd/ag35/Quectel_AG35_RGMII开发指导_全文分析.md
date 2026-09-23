# Quectel AG35-CET & AG35-EUT QuecOpen(SDK) RGMII 开发指导 — 全文详尽分析

> **文档全名**：AG35-CET&AG35-EUT QuecOpen(SDK) RGMII 开发指导
> **适用模块系列**：LTE Standard 模块系列（AG35-CET、AG35-EUT）
> **版本**：V1.0.1（临时文件 / Preliminary Confidential）
> **日期**：2025-03-18
> **作者**：Gabriel LI
> **总页数**：30 页（PDF 内部页码标注为 "X / 29"，即正文 29 页编号 + 封面）
> **本分析覆盖页范围**：全文 1–30 页（PDF 内部页 1–29 + 封面）

---

## 文档定位与背景

本文档是移远通信（Quectel，上海移远通信技术股份有限公司）针对 **AG35-CET** 和 **AG35-EUT** 两款 LTE Standard 模块在 **QuecOpen(SDK)** 方案下使用 **RGMII / RMII 以太网接口** 的开发指导。QuecOpen 是基于 **Linux** 的嵌入式开发平台，用于简化 IoV（车联网/物联网）应用的软件设计与开发。

**关键定位（与本仓库 modem_mng 的关系）**：本仓库正在做 AG35 平台支持（见 CLAUDE.md 与最近提交 `4cbca24e [BUILD] 新增 AG35 平台构建支持`）。本 RGMII 文档讲的是 **AG35 模块对外接以太网 PHY 芯片的 MAC 侧（RGMII/RMII）配置与调试**，属于硬件/内核驱动/设备树层面，与拨号 daemon 的应用逻辑无直接耦合，但若 AG35 产品形态需要走有线网口（eth0），本文是设备树与 PHY 调试的权威参考。

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| -（创建） | 2023-11-09 | Gabriel LI | 文档创建 |
| 1.0.0 | 2023-11-09 | Gabriel LI | 临时版本 |
| 1.0.1 | 2025-03-18 | Gabriel LI | 临时版本：①新增适用模块 **AG35-EUT**；②修改设备树文件配置、修改切换为 RMII 模式的方法、修改在 RMII 模式下切换电压阈的方法（均集中在**第 3 章**） |

> **解读**：V1.0.1 相对 V1.0.0 的核心增量全部落在第 3 章（设备树），且新覆盖 AG35-EUT。文档自我声明为"临时文件"，引脚/接口/频段等可能变动，需以最新正式版为准。

---

## 文档结构总览（目录）

正文共 7 章：

1. **引言**（p7）
2. **引脚描述**（p8）
3. **设备树介绍**（p9–12）
4. **加载驱动**（p13–15）：4.1 EMAC 驱动、4.2 PHY 驱动（4.2.1 加载 PHY 驱动）
5. **PHY 设备**（p16–25）：5.1 寄存器（5.1.1 控制 / 5.1.2 状态 / 5.1.3 复位）、5.2 设备属性文件（5.2.1 phy_reg_dump / 5.2.2 quectel_clause45_Interface / 5.2.3 general_phy_dump）、5.3 IO 电压设置（5.3.1 PHY IO 寄存器配置 / 5.3.2 设备树配置）、5.4 TX/RX 延时设置、5.5 驱动耗流调试
6. **RGMII 调试说明**（p26–28）：6.1 EVB 接 PHY 环境、6.2 电脑端设置、6.3 调试步骤及注意事项
7. **附录 参考文档及术语缩写**（p29）

**表格索引**：表1 RGMII 引脚定义（p8）、表2 参考文档（p29）、表3 术语缩写（p29）。
**图表索引**：图1 YT8521 PHY 控制寄存器、图2 状态寄存器、图3 软件复位寄存器、图4 VDDIO 电压寄存器、图5 TX/RX 延时寄存器、图6 低功耗模式耗流数据、图7 PHY 与模块 TE-A 环境、图8 Windows 改 IP 为自动获取、图9 Windows 改速率为自协商。

---

## 第 1 章 引言（p7）

- AG35-CET / AG35-EUT 支持 QuecOpen 方案；QuecOpen 是基于 Linux 的嵌入式开发平台，简化 IoV 应用软件设计开发。QuecOpen 详情见参考文档 [1]。
- 本文档适用于 **SDK 构建环境**的 QuecOpen 方案，介绍 AG35-CET/EUT 模块的 **RGMII 及 RMII 功能**，覆盖：硬件电路设计、软件配置、调试方法。

---

## 第 2 章 引脚描述（p8）

模块内部提供一组**以太网 MAC 的 RGMII/RMII 接口**，可与外部 PHY 连接。详细引脚信息见参考文档 [2]。PHY 部分关键引脚分配：

### 表 1：RGMII 引脚定义

| 引脚号 | 引脚名 | 引脚描述 | 复用功能 1（默认） |
|---|---|---|---|
| 6 | **RMII_RST_N** | 复位外部 PHY | GPIO_20 |
| 4 | **EPHY_VDD** | PHY 供电电源 | -（无复用） |

**关键说明与限制**：
- PHY 电源使能引脚可使用**普通 GPIO**（即非固定，可灵活选用任意 GPIO 作 LDO/电源使能），但 **RMII_RST_N 引脚不可更改**（固定为 6 脚 / GPIO_20，作为外部 PHY 复位脚）。
- **EPHY_VDD（4 脚）是输入引脚**，用于控制模块内部网卡供电，其电压必须与外接 PHY 的 IO 电压域匹配：
  - 外接 PHY 为 **1.8 V 电压域** → EPHY_VDD 需外部输入 **1.8 V**，**支持 RGMII 模式**。
  - 外接 PHY 为 **3.3 V 电压域** → EPHY_VDD 需外部输入 **3.3 V**，**仅支持 RMII 模式**（3.3V 域不支持 RGMII）。

> **解读（硬件红线）**：电压域选择是 RGMII vs RMII 的物理前提。1.8V→可 RGMII（千兆），3.3V→只能 RMII（百兆）。这与第 3 章设备树里 `3v3-enable` 配置必须一致，否则会烧模块（见 p12 备注）。

---

## 第 3 章 设备树介绍（p9–12）

设备树主要对**引脚、时钟、电源**进行配置。

### 3.1 主配置文件 `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-common.dtsi`

核心节点 `eth0: asr-eth@0xd4281800`，完整配置如下：

```dts
eth0: asr-eth@0xd4281800 {
    status = "okay";
    compatible = "asr,asr-eth";
    pinctrl-names = "default", "rgmii-pins";
    pinctrl-0 = <&emac_pmx_func0 &emac_pmx_func2 &emac_pmx_func3>;
    pinctrl-1 = <&emac_pmx_func0 &emac_pmx_func1 &emac_pmx_func2 &emac_pmx_func3>;
    reg = <0xd4281800 0x200>;
    interrupts = <10 11>;
    lpm-qos = <PM_QOS_CPUIDLE_BLOCK_AXI>;
    clocks = <&soc_clocks ASR1803_CLK_EMAC; &soc_clocks ASR1803_CLK_EMAC_PTP>;
    clock-names = "emac-clk", "ptp-clk";
    ptp-support;
    ptp-clk-rate = <100000000>;
    clk-tuning-enable;

    /* clk-config(32bit)
     *
     * clk_sel(clk-config[23:16])
     * RGMII:
     *   tx | clk_sel: 0 - from external RX clock
     *                 1 - from inverted external RX clock
     *   rx | clk_sel: 0 - from external RX clock
     *                 1 - from inverted external RX clock
     *
     * RMII:
     *   tx | clk_sel: 0 - RMII clock
     *                 1 - Inverted RMII clock
     *   rx | clk_sel: 0 - RMII clock
     *                 1 - Inverted RMII clock
     */
    tx-clk-config = <0x0>;
    rx-clk-config = <0x0>;

    /* enable 1000M phy */
    3v3-enable = <0>;            /* IO voltage, 1 - 3.3v, 0 - 1.8v */
    phy-handle = <&phy0>;
    reset-gpio = <&gpio 20 0>;
    reset-active-low;
    reset-delays-us = <0 100000 100000>;
    enable-suspend;
    suspend-not-keep-power;
    ldo-gpio = <&gpio 117 0>;    // The gpio117 is power enable pin.
    ldo-active-low;
    ldo-delays-us = <0 10000 10000>;
    flow-control-threshold = <60 90>;

    /* enable fix link for ethernet switch */
    /*
    fixed-link {
        speed = <100>;
        full-duplex;
        phy-mode = "rmii";
    };
    */

    mdio: mdio-bus {
        #address-cells = <0x1>;
        #size-cells = <0x0>;
        /* YT8521 10M/100M/1000M 1.8V RGMII PHY */
        phy0: phy@0 {
            compatible = "ethernet-phy-ieee802.3-c22";
            device_type = "ethernet-phy";
            reg = <0x0>;                  /* set phy address */
            phy-mode = "rgmii";
            tx_rx_delay = <0xb 0x0>;      /* 150ps per step */
        };
    };
};
```

**逐字段解读**：

| 字段 | 含义 / 取值 |
|---|---|
| `status = "okay"` | 启用该 EMAC 节点 |
| `compatible = "asr,asr-eth"` | 绑定 ASR 平台 EMAC 驱动（AG35 基于 ASR1803 芯片，故下文 `ASR1803_CLK_EMAC`） |
| `pinctrl-0` / `pinctrl-1` | 两组引脚复用：default 用 func0/2/3；rgmii-pins 额外加 func1（RGMII 比 RMII 多一组数据/时钟脚） |
| `reg = <0xd4281800 0x200>` | EMAC 寄存器基址 + 长度 0x200 |
| `interrupts = <10 11>` | EMAC 使用 10、11 号中断 |
| `clocks / clock-names` | EMAC 主时钟 + PTP 时钟（`emac-clk`、`ptp-clk`） |
| `ptp-support; ptp-clk-rate = <100000000>` | 支持 PTP（精密时间协议），PTP 时钟 100 MHz |
| `clk-tuning-enable` | 启用时钟微调 |
| `tx-clk-config / rx-clk-config = <0x0>` | TX/RX 时钟来源选择，bit[23:16] 为 clk_sel。RGMII 下 0=取外部 RX 时钟、1=取反相外部 RX 时钟；RMII 下 0=RMII 时钟、1=反相 RMII 时钟。默认 0x0（正常相位） |
| **`3v3-enable = <0>`** | **IO 电压域：1=3.3V，0=1.8V**。当前默认 0（1.8V，支持千兆 RGMII） |
| `phy-handle = <&phy0>` | 指向下方 mdio 总线上的 phy0 节点 |
| `reset-gpio = <&gpio 20 0>` | PHY 复位脚 = GPIO20（对应表1 的 RMII_RST_N，不可改） |
| `reset-active-low` | 复位低有效 |
| `reset-delays-us = <0 100000 100000>` | 复位时序（前/持续/后延时，单位 us）：拉低前 0，持续 100ms，释放后等 100ms |
| `enable-suspend; suspend-not-keep-power` | 支持挂起；挂起时不保持 PHY 供电（省电） |
| `ldo-gpio = <&gpio 117 0>` | **PHY 电源使能脚 = GPIO117**（注释：power enable pin）。即第 2 章所说"PHY 电源使能可用普通 GPIO" |
| `ldo-active-low` | 电源使能低有效 |
| `ldo-delays-us = <0 10000 10000>` | LDO 上电时序，持续/后延时各 10ms |
| `flow-control-threshold = <60 90>` | 流控阈值（低 60 / 高 90，水线百分比，用于以太网流控暂停帧） |
| `fixed-link {...}`（注释掉） | 若对接以太网交换机（无 PHY 自协商）可启用 fixed-link：固定 100M 全双工 rmii。默认注释，使用真实 PHY 自协商 |
| `mdio-bus / phy0: phy@0` | MDIO 管理总线及挂载的 PHY |
| `compatible = "ethernet-phy-ieee802.3-c22"` | PHY 走 IEEE 802.3 Clause 22 管理接口 |
| `reg = <0x0>` | **PHY 地址 = 0**（MDIO 地址，需与硬件 strapping 一致） |
| `phy-mode = "rgmii"` | **接口模式（rgmii 或 rmii）** |
| `tx_rx_delay = <0xb 0x0>` | TX/RX 延时配置，**每 step 150ps**。TX=0xb(11)→11×150ps≈1.65ns，RX=0x0→0 |

> 默认 PHY 为 **YT8521**（裕太微 10M/100M/1000M 1.8V RGMII PHY）。

### 3.2 补充配置文件 `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-std.dts`

针对另一颗 PHY（Motorcomm **YT8010A** 100Base-T1 PHY，RMII）的条件编译开关：

```c
/* Motorcomm YT8010A 100Base-T1 PHY RMII */
#define CONFIG_USE_YT8010A   0

//gabriel.li 20230523 add, If using YT8010A
#if (CONFIG_USE_YT8010A)
&eth0 {
    3v3-enable = <1>;
    mdio: mdio-bus {
        phy0: phy@0 {
            phy-mode = "rmii";   //Setting interface mode to "rgmii" or "rmii"
        };
    };
};
#endif
```

> **解读**：若使用 YT8010A（车规 100Base-T1 单对线 PHY，3.3V 域），把 `CONFIG_USE_YT8010A` 置 1，会覆盖 eth0 为 `3v3-enable=<1>`（3.3V）+ `phy-mode="rmii"`。这与第 2 章"3.3V 域仅支持 RMII"一致。

### 3.3 模式切换方法（V1.0.1 新增重点）

**① RGMII ↔ RMII 切换**：默认 RGMII。切换到 RMII，只需把 mdio-bus → phy0 节点中的
```dts
phy-mode = "rgmii";  →  phy-mode = "rmii";
```
（文档配图用红框标出该行，p11 图）。

**② RMII 模式下电压阈切换（1.8V ↔ 3.3V）**：RMII 模式支持 1.8V 与 3.3V 两种电压阈，默认 1.8V。切换到 3.3V：
```dts
3v3-enable = <0>;  →  3v3-enable = <1>;   /* IO voltage, 1 - 3.3v, 0 - 1.8v */
```
（p11 第二个红框图标注 3v3-enable 行）。

### 3.4 ⚠️ 关键安全备注（p12）

> **仅 RMII 模式支持 3.3 V 电压阈。** 如设置 `3v3-enable = <0>`（电压阈为 1.8 V），但在 **EPHY_VDD 引脚输入 3.3 V 供电**，**将对模块造成不可逆损伤**。

> **解读（最高优先级红线）**：软件设备树 `3v3-enable` 与硬件 EPHY_VDD 实际供电电压**必须严格一致**。1.8V 设备树 + 3.3V 实际供电 = 烧模块。这是整篇文档里唯一标注"不可逆损伤"的硬约束。

---

## 第 4 章 加载驱动（p13–15）

### 4.1 EMAC 驱动（p13）

- 模块通过 **EMAC 驱动**完成内部 MAC 与外部 PHY 的初始化及控制。
- 驱动文件：`ql-ol-kernel/drivers/net/ethernet/asr/emac_eth.c`。
- 该驱动**内嵌在内核中**，模块上电后随 Linux 一起加载，**不可卸载**（built-in，非 module）。

### 4.2 PHY 驱动（p13–15）

- 通用 PHY 驱动文件：`ql-ol-kernel/drivers/net/phy/quectel_phy.c`。
- 行为：
  - **外部未接入 PHY** → 使用**通用 PHY** 启动 eth0 网卡。
  - **外部接入 PHY** → 读取 **PHY ID** 进行 PHY 驱动适配。
- 已适配的 PHY 设备在 `quectel_phy.c` 中均有寄存器配置体现，调试同型号 PHY 可参考。文档给出 **JL3113** 型号 PHY 的时钟延时配置示例（`quectel_phy.c` 第 595–632 行）：

```c
595
596    if (phydev->interface == PHY_INTERFACE_MODE_SGMII) {
597        err = jl3xxx_set_phy_inter(phydev, PHY_SGMII);
598        if (err < 0)
599            return err;
600    } else {
601        err = jl3xxx_set_phy_inter(phydev, PHY_RGMII);
602        if (err < 0)
603            return err;
604    }
605
606    //1000M  TX or RX delay enabled and TX CLOCK DELAY LEVEL
607    //jl3xxx_set_rgmii_dly(phydev, true, true);
608
609    //TX CLOCK DELAY LEVEL 1ns
610    //jl3xxx_clear_mmd_bits(phydev, JL3XXX_IEEE_CTL_ADDR,
611    //          JL3XXX_RGMII_CFG, JL3XXX_RGMII_TX_DLY_LEVEL_0);
612    //jl3xxx_clear_mmd_bits(phydev, JL3XXX_IEEE_CTL_ADDR,
613    //          JL3XXX_RGMII_CFG, JL3XXX_RGMII_TX_DLY_LEVEL_1);
614    //TX CLOCK DELAY LEVEL 1.2ns
615    //jl3xxx_set_mmd_bits(phydev, JL3XXX_IEEE_CTL_ADDR,
616    //          JL3XXX_RGMII_CFG, JL3XXX_RGMII_TX_DLY_LEVEL_0);
617    //jl3xxx_clear_mmd_bits(phydev, JL3XXX_IEEE_CTL_ADDR,
618    //          JL3XXX_RGMII_CFG, JL3XXX_RGMII_TX_DLY_LEVEL_1);
619    //TX CLOCK DELAY LEVEL 1.5ns
620    //jl3xxx_clear_mmd_bits(...JL3XXX_RGMII_TX_DLY_LEVEL_0);
621    //jl3xxx_set_mmd_bits(...JL3XXX_RGMII_TX_DLY_LEVEL_1);
622    //TX CLOCK DELAY LEVEL 1.8ns
623    //jl3xxx_set_mmd_bits(...JL3XXX_RGMII_TX_DLY_LEVEL_0);
624    //jl3xxx_set_mmd_bits(...JL3XXX_RGMII_TX_DLY_LEVEL_1);
630    // soft reset
631    jl3xxx_set_mmd_bits(phydev, 1, 0, BIT(15));
632    msleep(20);
```

**解读**：
- `jl3xxx_set_phy_inter()` 根据 `phydev->interface` 选择 SGMII 或 RGMII。
- 注释段展示了通过两个 bit（`TX_DLY_LEVEL_0/1` 的 set/clear 组合）配置 **4 档 TX 时钟延时**：1ns(0,0)、1.2ns(1,0)、1.5ns(0,1)、1.8ns(1,1)。即用 MMD 寄存器位写不同组合得到不同延时档。
- 末尾 `set_mmd_bits(phydev, 1, 0, BIT(15))` 写 MMD device1 寄存器0 的 bit15 做软复位，随后 `msleep(20)` 等待生效。

> **备注**：调试 PHY 设备时，除参考驱动文件中的配置外，**还需与实际硬件强关联**。调试中如有问题，请联系移远技术支持。

### 4.2.1 加载 PHY 驱动 — 启动日志判读（p14–15）

**情形 A：外接 YT8521，驱动加载成功**（PHY ID = **0x11a**）。关键日志：
```
emac_phy_connect: phy@0
---> set eamc interface: rgmii
enter yt8521_config_init
emac_phy_connect: eth0: attached to PHY (UID 0x11a) Link = 0 irq=-1
clk_phase_rgmii_set phase:0 direction:tx 0x30007 0x30007
clk_phase_rgmii_set phase:0 direction:rx 0x30007 0x30007
MDIO clock div: 0x60
emac_adjust_link link:1 speed:1000M duplex:Full
br-lan: port 1(eth0) entered blocking state
device eth0 entered promiscuous mode
br-lan: port 1(eth0) entered forwarding state
IPV6: ADDRCONF(NETDEV_CHANGE): br-lan: link becomes ready
```
> 判读：`UID 0x11a` 确认识别到 YT8521；`enter yt8521_config_init` 说明命中专用初始化；`speed:1000M duplex:Full` 表示千兆全双工链路；eth0 加入 `br-lan` 网桥并进入 forwarding。

**情形 B：未接入 PHY，无法获取 PHY ID**，EMAC 驱动默认给 PHY ID 赋 **0x12345678** 后套用通用驱动。日志：
```
emac_phy_connect: phy@0
---> set eamc interface: rgmii
emac_phy_connect: eth0: attached to PHY (UID 0x12345678) Link = 0 irq=-1
clk_phase_rgmii_set phase:0 direction:tx 0x30007 0x30007
MDIO clock div: 0x60
emac_adjust_link link:1 speed:1000M duplex:Full
device bridge0 entered promiscuous mode
bridge0: port 1(ecm0) entered blocking state
device ecm0 entered promiscuous mode
bridge0: port 2(eth0) entered forwarding state
ccidatastub_ioctl,cmd=0x4004f602
CCIDATASTUB_DATAHANDLE: cid =0, type =1
```
> 判读：**UID 0x12345678 是"未识别到真实 PHY"的哨兵值** —— 调试时若看到此 UID，说明 MDIO 没读到 PHY（硬件未接/地址错/供电/复位问题），此时走通用驱动，链路状态不可信。这是现场快速定位"PHY 没起来"的关键特征。

---

## 第 5 章 PHY 设备（p16–25，本段覆盖至 p19）

本章以 **YT8521 PHY** 为例介绍控制/状态/复位寄存器；用其他型号 PHY 时需对照该 PHY 芯片官方手册在对应寄存器做类似操作。

### 5.1 寄存器

#### 5.1.1 控制寄存器（**0x00** 寄存器，图1）

通过配置 0x00 寄存器配置速率和自适应模式。各 bit：

| Bit | Symbol | Access | Default | 说明 |
|---|---|---|---|---|
| 15 | Reset | RW SC | 0x0 | PHY 软件复位。写 1 立即复位，完成后自动清 0。0=正常，1=复位 |
| 14 | Loopback | RW SWC | 0x0 | 内部环回控制。0=禁用环回，1=启用环回 |
| 13 | Speed_Selection(LSB) | RW | 0x0 | speed_selection[1:0] 低位。仅当 bit12(Autoneg)=0 时有效。**bit6,bit13 组合**：11=Reserved，10=1000Mb/s，01=100Mb/s，00=10Mb/s |
| 12 | Autoneg_En | RW | 0x1 | 1=启用自协商，0=禁用 |
| 11 | Power_down | RW SWC | 0x0 | 1=掉电（power down） |
| 10 | Isolate | RW SWC | 0x0 | 将 PHY 与 RGMII/SGMII/FIBER 隔离。0=正常，1=隔离 |
| 9 | Re_Autoneg | RW SC SWS | 0x0 | 1=重启自协商过程，0=正常。硬/软复位后自动重启自协商（不论本位是否置 1） |
| 8 | Duplex_Mode | RW | 0x1 | 双工模式（Autoneg 关闭时手动有效）。1=全双工，0=半双工 |
| 7 | Collision_Test | RW SWC | 0x0 | 1=启用 COL 信号测试（TX_EN 有效时 COL 也有效），0=禁用 |
| 6 | Speed_Selection(MSB) | RW | 0x1 | 见 bit13 组合表 |
| 5:0 | Reserved | RO | 0x0 | 保留，写 0、读忽略 |

> **Power_down 补充**（图1 续）：当端口从 power down 切回正常时，**即使用户未置 bit15(RESET)/bit9(RESTART_AUTO_NEGOTIATION)，也会自动执行软复位和自协商**。
>
> **Access 缩写**：RW=读写，RO=只读，SC=Self-Clear（自清），SWC=Software Clear，SWS=Software Set。

**配置示例**（关闭自适应 + 100M 速率，需写 0x00 寄存器）：
```sh
echo 0x00-0x2100 > /sys/kernel/debug/eth/phy_reg_dump
```
> 解读：值 0x2100 = bit13=1(Speed LSB)、bit8=1(Full Duplex)... 实际 0x2100 = 0010_0001_0000_0000：bit13=1、bit8=1。bit12=0(关自协商)、bit6=0 → bit6,bit13=0,1=100Mb/s，全双工。写法是 `寄存器地址-值` 重定向到 `phy_reg_dump` 属性文件（见 5.2.1）。

#### 5.1.2 状态寄存器（**0x11** 寄存器，图2）

判断 PHY 是否 Link：读 **0x11 寄存器 Bit[10]**，=1 为 Link up，否则 Link down。各 bit：

| Bit | Symbol | Access | Default | 说明 |
|---|---|---|---|---|
| 15:14 | Speed_mode | RO | 0x0 | 仅当 bit11=1 有效。11=Reserved，10=1000Mbps，01=100Mbps，00=10Mbps |
| 13 | Duplex | RO | 0x0 | 仅当 bit11=1 有效。1=全双工，0=半双工 |
| 12 | Page Received real-time | RO | 0x0 | 1=收到 page，0=未收到 |
| 11 | Speed and Duplex Resolved | RO | 0x0 | 自协商禁用时该位为 1 表示强制速率模式。1=已解析，0=未解析 |
| 10 | **Link status real-time** | RO | 0x0 | **1=Link up，0=Link down** |
| 9:7 | Reserved | RO | 0x0 | 保留 |
| 6 | MDI Crossover Status | RO | 0x0 | 仅当 bit11=1 有效（MDI/MDIX 交叉状态） |

> **解读**：**0x11.bit[10] 是判断链路是否 up 的权威位**（实时链路状态）；bit[15:14]+bit[13] 给出协商后的速率与双工，但仅在 bit[11]=1（已解析）时可信。

#### 5.1.3 复位寄存器（**0x00** 寄存器 Bit[15]，图3）

通过设置 PHY 0x00 寄存器 **Bit[15]** 做软件复位（图3 寄存器位定义与 5.1.1 控制寄存器 bit15..bit11 一致：Reset/Loopback/Speed_Selection(LSB)/Autoneg_En/Power_down）。

> **备注（重要 — 芯片模式 Mode_sel，图3 续）**：当前 YT8521 PHY 使用 **UTP To RGMII** 通信模式，可通过 **0xA001 寄存器 Bit[2:0]** 确认 PHY 是否处于该模式：

| Bit | Symbol | Access | Default | 说明 |
|---|---|---|---|---|
| 2:0 | **Mode_sel** | RW POS | 0x0 | 芯片模式（取决于 strapping）：`000`=UTP_TO_RGMII；`001`=FIBER_TO_RGMII；`010`=UTP_FIBER_TO_RGMII；`011`=UTP_TO_SGMII；`100`=SGPHY_TO_RGMAC；`101`=SGMAC_TO_RGPHY；`110`=UTP_TO_FIBER_AUTO；`111`=UTP_TO_FIBER_FORCE |

> 对 **0xA001** 寄存器 Bit[2:0] 的配置**在软件复位后生效**，详情参考 YT8521 PHY 芯片官方手册。
> （Access "RW POS" 指写后需软复位 / power-on strapping 相关。）

> **解读**：AG35 RGMII 默认对接的是"双绞线(UTP) 转 RGMII"模式（Mode_sel=000）。若 PHY strapping 错误导致进入别的模式，链路异常，应先读 0xA001.bit[2:0] 核对，改后必须软复位（写 0x00.bit15）才生效。

---

### 5.2 设备属性文件（p20–21）

模块支持**通过读写设备属性文件（debugfs）**对 PHY 寄存器进行读取和配置。

#### 5.2.1 `phy_reg_dump` 属性文件（p20–21）

路径：`/sys/kernel/debug/eth/phy_reg_dump`。可用于读写外部 PHY 寄存器，支持 **Clause 22** 和 **Clause 45** 两种协议。

**■ Clause 22 PHY 寄存器**

读取寄存器（先 echo 地址、再 cat 取值）：
```sh
echo <寄存器地址> > /sys/kernel/debug/eth/phy_reg_dump
cat   /sys/kernel/debug/eth/phy_reg_dump
```
示例：读 0x00 寄存器：
```sh
echo 0x00 > /sys/kernel/debug/eth/phy_reg_dump
cat /sys/kernel/debug/eth/phy_reg_dump
```
配置寄存器（格式 `地址-值`）：
```sh
echo <寄存器地址>-<设置参数> > /sys/kernel/debug/eth/phy_reg_dump
```
示例：把 0x00 寄存器写为 0x0140：
```sh
echo 0x00-0x0140 > /sys/kernel/debug/eth/phy_reg_dump
```

**■ Clause 45 PHY 寄存器**（多一个"设备 ID"维度）

读取（格式 `寄存器地址-设备ID`）：
```sh
echo <寄存器地址>-<设备ID> > /sys/kernel/debug/eth/phy_reg_dump
cat   /sys/kernel/debug/eth/phy_reg_dump
```
示例：读 设备 ID=7、寄存器地址=0x3c：
```sh
echo 0x3c-7 > /sys/kernel/debug/eth/phy_reg_dump
cat /sys/kernel/debug/eth/phy_reg_dump
```
配置（格式 `寄存器地址-设备ID-设置参数`）：
```sh
echo <寄存器地址>-<设备ID>-<设置参数> > /sys/kernel/debug/eth/phy_reg_dump
```
示例：设备 ID=7、寄存器 0x3c、写值 0x0004：
```sh
echo 0x3c-7-0x0004 > /sys/kernel/debug/eth/phy_reg_dump
```

> **解读（命令格式对照）**：Clause 22 用 `地址[-值]`（两段式）；Clause 45 用 `地址-设备ID[-值]`（三段式，多了 MMD 设备 ID）。读操作就是不带最后的"值"段，写操作带上"值"段。

#### 5.2.2 `quectel_clause45_Interface` 属性文件（p21）

路径：`/sys/kernel/debug/eth/quectel_clause45_Interface`。
**适用场景**：PHY ID 可通过 **Clause 22** 协议获取，但**部分寄存器需通过 Clause 45 协议访问**时，操作该属性文件。操作方式与用 Clause 45 协议读写 `phy_reg_dump` 一致（即三段式 `地址-设备ID[-值]`）。

> **解读**：这是为"混合 Clause22/45"PHY 准备的专用入口 —— 当 PHY 的身份识别走 C22、但某些扩展寄存器只能走 C45 时使用，避免和主 `phy_reg_dump` 的协议判定冲突。

#### 5.2.3 `general_phy_dump` 属性文件（p21）

路径：`/sys/kernel/debug/eth/general_phy_dump`。
用于一次性获取 PHY **0x0 – 0x1F**（即标准 Clause 22 的 32 个寄存器）的全部值，结果输出到**内核日志**（dmesg）。调试时执行：
```sh
cat /sys/kernel/debug/eth/general_phy_dump
```

> **解读**：`phy_reg_dump` 一次读一个寄存器，`general_phy_dump` 一次性 dump 全部标准寄存器到 dmesg，适合快速全景排查（如核对 0x00/0x11 等关键寄存器）。

### 5.3 IO 电压设置（p21–22）

#### 5.3.1 PHY IO 寄存器配置（0xA001 寄存器，图4）

YT8521 PHY 支持 IO 电压 **1.8 V / 2.5 V / 3.3 V** 三种电压域。但**模块 EMAC 仅支持 1.8 V 和 3.3 V**，故 PHY 的 **2.5 V 电压域不可配置**。

如需将外部 PHY IO 电压设为 **3.3 V**，需配置 **0xA001 寄存器 Bit[5:4]**。0xA001 寄存器完整描述：

| Bit | Symbol | Access | Default | 说明 |
|---|---|---|---|---|
| 15 | Sw_rst_n_mode | RW SC | 0x1 | 整片软件复位，芯片模式改变时也会触发；低有效；自清 |
| 14:12 | Reserved | RO | 0x0 | 保留 |
| 11 | Iddq_mode | RW | 0x0 | Iddq 测试模式 |
| 10:9 | Reserved | RO | 0x0 | 保留 |
| 8 | Rxc_dly_en | RW POS | 0x1 | RGMII 时钟 2ns 延时控制，取决于 strapping |
| 7 | Reserved | RO | 0x0 | 保留 |
| 6 | En_ldo | RW | 0x1 | RGMII LDO 使能，默认 0，上电 strapping 完成后被置 1 |
| **5:4** | **Cfg_ldo** | RW POS | 0x0 | **RGMII LDO 电压控制（取决于 strapping）**：`2'b11`=1.8v；`2'b10`=1.8v；`2'b01`=2.5v；`2'b00`=3.3v |
| 3 | Reserved | RO | 0x0 | 保留 |
| 2:0 | Mode_sel | RW POS | 0x0 | 芯片模式（同 5.1.3 图3 表）：000=UTP_TO_RGMII；001=FIBER_TO_RGMII；010=UTP_FIBER_TO_RGMII；011=UTP_TO_SGMII；100=SGPHY_TO_RGMAC；101=SGMAC_TO_RGPHY；110=UTP_TO_FIBER_AUTO；111=UTP_TO_FIBER_FORCE |

> **Cfg_ldo 取值要点（易错）**：`00`=3.3V、`01`=2.5V、`10`/`11`=1.8V。注意它**不是单调递增**编码，且与 strapping 引脚相关（POS = power-on strapping）。
>
> **备注**：如果外部 PHY IO 电压**默认为 1.8 V，该寄存器不用配置**。

#### 5.3.2 设备树配置（p22）

外部 PHY 的 IO 硬件电压需与 EMAC 驱动的 IO 电压匹配。EMAC 驱动关于 IO 电压的配置（位于 `ql-1806e-std.dts`，YT8010A 条件块内）：

```dts
//gabriel.li 20230523 add, If using YT8010A
#if (CONFIG_USE_YT8010A)
&eth0 {
    3v3-enable = <1>;
    mdio: mdio-bus {
        phy0: phy@0 {
            phy-mode = "rmii";   //Setting interface mode to "rgmii" or "rmii"
        };
    };
};
&pmm8021do4 {
    regulator-min-microvolt = <3300000>;  // MDIO_BUS MDIO/MDC IO voltage, 3300000 - 3.3v, 1800000 - 1.8v
    regulator-boot-on;
    regulator-always-on;
};
#endif
```

**关键点**：
- 改 PHY IO 电压需**同时修改两处**设备树字段：
  1. `3v3-enable`（eth0 数据线 IO 电压：`<1>`=3.3V，`<0>`=1.8V）
  2. `regulator-min-microvolt`（`&pmm8021do4` 节点，MDIO_BUS 的 MDIO/MDC IO 电压：`3300000`=3.3V，`1800000`=1.8V）
- 模块当前仅支持 **3.3 V 和 1.8 V** 电压域，默认 1.8 V，**仅 RMII 模式下支持 3.3 V 电压阈**。
- 设备树路径：`ql-ol-kernel/arch/arm/boot/dts/ql-1806e-std.dts`。

> **解读**：IO 电压一致性需"三方对齐"——硬件 EPHY_VDD 供电、`3v3-enable`（数据线）、`regulator-min-microvolt`（MDIO 管理线）必须同为 1.8V 或同为 3.3V，否则 MDIO 读不到 PHY 或损伤模块。

### 5.4 TX/RX 延时设置（0xA003 寄存器，图5，p23）

**问题背景**：通信速率 **1000 Mbps 对硬件要求较高**，不同硬件版本需调整 PHY 的 **RX/TX 延时**，RGMII 才能正常工作。YT8521 通过 **0xA003 寄存器**的 **Bit[13:10]、Bit[7:4]、Bit[3:0]** 打开/关闭 TX/RX 延时。寄存器描述：

| Bit | Symbol | Access | Default | 说明 |
|---|---|---|---|---|
| 15 | Rgmac_cfg_mode | RW | 0x0 | 当芯片模式为 SGPHY_TO_RGMAC 时，控制 RGMII 速率/双工/链路状态来源。1=来自 EXT 0xA004；0=来自 RGMII OOB（详见 EXT 0xA005） |
| 14 | Tx_clk_sel | RW | 0x0 | 0=用原始 RGMII TX_CLK 驱动 TX_CLK 延时链；1=用反相 RGMII TX_CLK 驱动。**用于 debug** |
| 13:10 | **Rx_delay_sel** | RW | 0x0 | **RGMII RX_CLK 延时链配置，约 150ps/step** |
| 9 | En_rgmii_fd_crs | RW | 0x0 | 见 EXT 0xA003 bit[8] |
| 8 | En_rgmii_crs | RW | 0x0 | 0=不把 GMII/MII CRS 编码进 RGMII OOB；1=半双工模式或 EXT 0xA003 bit[9]=1 时把 CRS 编码进 RGMII OOB |
| 7:4 | **Tx_delay_sel_fe** | RW | 0xf | **速率为 100M/10M 时**的 RGMII TX_CLK 延时链配置，典型 150ps/step |
| 3:0 | **Tx_delay_sel** | RW | 0x1 | **速率为 1000M 时**的 RGMII TX_CLK 延时链配置，典型 150ps/step |

> **延时配置三段式要点**：
> - **RX 延时**：bit[13:10]（`Rx_delay_sel`），不分速率。
> - **TX 延时**：分速率两套——百兆/十兆用 bit[7:4]（`Tx_delay_sel_fe`，默认 0xf）；千兆用 bit[3:0]（`Tx_delay_sel`，默认 0x1）。
> - 每 step ≈ 150ps（与设备树 `tx_rx_delay = <0xb 0x0>; /* 150ps per step */` 一致）。

> **备注（重要限制）**：
> 1. 若 PHY 已 Link up 但 **RGMII 出现丢包**，可通过修改 TX/RX 延时尝试解决。
> 2. **模块 EMAC 不支持配置 TX/RX 时钟延迟**，**仅当适配的千兆 PHY 支持调整 TX/RX 时钟延迟时**，相关配置才生效。（即延时必须由 PHY 侧承担，MAC 侧无能为力。）

### 5.5 驱动耗流调试（图6，p24–25）

以 YT8521 PHY 为例，模块进入低功耗模式后 PHY 芯片各状态的耗流数据：

#### 图6：低功耗模式耗流数据

| Condition（状态） | DVDD_RGMII (mA) | DVDD33 + VDD33_LX (mA) | AVDD33 (mA) | Power Consumption (mW) |
|---|---|---|---|---|
| Reset | 3 | 1.4 | 6 | 34.32 |
| Power Down | 0.3 | 1.7 | 5 | 23.1 |
| Link Down | 0.3 | 18.2 | 24.6 | 142.23 |
| Link Up @1000Mbps | 5.6 | 131.8 | 72.2 | 691.68 |
| Traffic @1000Mbps | 13 | 144.8 | 71.5 | 756.69 |

> **测试条件（Note）**：TT IC，DVDD_RGMII / DVDD33 / VDD33_LX / AVDD33 = 3.3V，VDDL = 1.2V（VDD33_LX(pin33) 接电感 SWPA3012S2R2NT），室温。

**耗流梯度解读**：Power Down(23.1mW) < Reset(34.32mW) < Link Down(142mW) ≪ Link Up 千兆(691.68mW) < 千兆有流量(756.69mW)。即**千兆链路本身就很耗电（~0.7W），有流量再加 ~65mW**；空闲未连(Link Down)已达 142mW。

> **备注与降耗建议**：
> - PHY 芯片耗流数据**以实际测试数据为准**。
> - 如需进一步降低功耗，可在**模块休眠时对 PHY 芯片断电处理**，降低整体耗流（呼应设备树 `enable-suspend; suspend-not-keep-power;` 与 `ldo-gpio`）。

**调试中查网络异常**：用 `ifconfig` 查询 eth0 网卡。RGMII 通信正常时，eth0 网卡上 TX/RX 均应有数据包。p25 示例 `ifconfig` 输出（红框标注 eth0）：
```
eth0  Link encap:Ethernet  HWaddr 00:30:87:0A:36:54
      inet6 addr: fe80::230:87ff:fe0a:3654/64 Scope:Link
      UP BROADCAST MULTICAST  MTU:1500  Metric:1
      RX packets:0 errors:0 dropped:0 overruns:0 frame:0
      TX packets:5 errors:0 dropped:0 overruns:0 carrier:0
      collisions:0 txqueuelen:1000
      RX bytes:0 (0.0 B)  TX bytes:566 (566.0 B)
      Interrupt:42 Base address:0x1800
```
（输出还含 bridge0=192.168.225.1、ecm0、lo 等接口。）若出现网络不通，参考第 6.3 章排查。

---

## 第 6 章 RGMII 调试说明（p26–28）

初次调试 RGMII/RMII 功能的硬件环境、电脑端设置与排查步骤。

### 6.1 EVB 接 PHY 环境（图7，p26）

在 AG35-CET 模块的 **TE-A**（评估底板）上插上 **PHY TE-A**（PHY 转接板，带 RJ45 网口），构成调试环境（图7 实物照片：AG35-CET 模块 + PHY 子板 + RJ45）。

### 6.2 电脑端设置（图8、图9，p27–28）

以 Windows 系统为例，三项必做设置：

1. **IPv4 地址改为"自动获取"**（DHCP）——图8：网络连接 → 以太网属性 → Internet 协议版本 4(TCP/IPv4) → 自动获得 IP/DNS。
2. **网卡速率改为"自动协商"（Auto Negotiation）**——图9：以太网属性 → 高级 → 属性列表选 "Speed & Duplex" → 值设为 "Auto Negotiation"。
3. **关闭电脑端所有防火墙**——若存在防火墙，则模块 **ping 不通电脑端**。

> **解读**：模块侧带 DHCP server（网段 192.168.225.x），故电脑端要 DHCP 自动获取；速率设自协商以匹配 PHY 自协商；防火墙会拦截 ICMP 导致 ping 不通——这是排查"ping 不通但链路正常"的常见外因。

### 6.3 调试步骤及注意事项（p28）

初次调试 RGMII/RMII 的标准排查流程（5 步）：

1. **确认 EMAC 驱动加载成功**：`ifconfig` 查是否存在 **eth0** 网卡。存在=加载成功；不存在=检查 **PHY 供电是否正常、PHY IO 电压是否匹配**。
2. **确认 PHY 是否 Link up**：驱动加载成功后，读寄存器（0x11.bit[10]）确认。若 Link down，**检查 PHY 速率选择**。
3. **若 PHY 已 Link up，确认电脑端配置正确**：插拔网线，查电脑端是否分配到 **192.168.225.x 同网段 IP**。若未分配 IP，按下法排查：
   1. 检查 eth0 是否在 bridge0：`brctl show` 查看；不存在则 `brctl addif bridge0 eth0` 添加。
   2. 插拔网线，查 eth0 上 RX/TX 是否有数据收发。
      - **eth0 无数据收发** → 调整 PHY 的 **TX/RX 延时**再试（参考 5.4 章）；若仍无效，检查 **RGMII 硬件电路信号质量**。
      - **eth0 有数据收发** → 在模块和电脑端抓网络数据包，分析 **DHCP 网络协议包**。
4. **电脑端正常分配 IP，但模块 ping 不通电脑、而电脑能 ping 通模块** → 需**关闭电脑端网络防火墙**。
5. **若 ping 过程中有数据丢包**：
   1. **百兆 PHY 丢包** → 检查 RX/TX **硬件电路信号质量**。
   2. **千兆 PHY 丢包** → 先把 PHY 配置为**百兆模式**测试是否仍丢包；若百兆下传输正常，先调整 **TX/RX 时钟延迟**测试，再检查 RGMII 的 TX/RX 硬件信号质量。

> **解读（排查决策树）**：故障定位顺序——驱动(eth0存在?) → 链路(Link up?) → 二层(bridge/数据收发?) → 三层(DHCP/IP?) → 连通性(ping/防火墙?) → 质量(丢包→延时/信号)。"先降到百兆排除速率因素"是定位千兆丢包的核心手法（千兆对延时和信号质量敏感）。

---

## 第 7 章 附录 参考文档及术语缩写（p29）

### 表 2：参考文档

| 编号 | 文档名称 |
|---|---|
| [1] | Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导 |
| [2] | Quectel_AG35-CET&AG35-EUT_QuecOpen_硬件设计手册 |

### 表 3：术语缩写

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| DHCP | Dynamic Host Configuration Protocol | 动态主机配置协议 |
| EMAC | Ethernet Media Access Controller | 以太网接入控制器 |
| I/O | Input/Output | 输入/输出 |
| IoV | Internet of Vehicles | 车联网 |
| IPv4 | Internet Protocol version 4 | 互联网通信协议第四版 |
| MAC | Medium Access Control | 媒体访问控制 |
| MDIO | Management Data Input/Output | 管理数据输入/输出 |
| NIC | Network Interface Controller | 网络接口控制器 |
| PHY | Physical | 端口物理层 |
| RGMII | Reduced Gigabit Media Independent Interface | 吉比特介质独立接口 |
| TX/RX | Transport/Receive | 发送/接收 |

---

## 全文要点速查与工程提示（分析者总结）

1. **电压红线（最高优先级）**：设备树 `3v3-enable` / `regulator-min-microvolt` 必须与硬件 EPHY_VDD 实际供电严格一致。1.8V 设备树 + 3.3V 供电 = **模块不可逆损伤**（p12）。
2. **模式与电压的绑定**：1.8V 域→支持 RGMII（千兆）；3.3V 域→仅 RMII（百兆）。YT8521(1.8V RGMII) 是默认 PHY；YT8010A(3.3V 100Base-T1 RMII) 由 `CONFIG_USE_YT8010A` 切换。
3. **关键文件路径**：设备树主 `ql-1806e-common.dtsi` + 补充 `ql-1806e-std.dts`；EMAC 驱动 `drivers/net/ethernet/asr/emac_eth.c`（built-in 不可卸载）；通用 PHY 驱动 `drivers/net/phy/quectel_phy.c`。
4. **debugfs 调试入口**：`/sys/kernel/debug/eth/` 下 `phy_reg_dump`（C22 两段式/C45 三段式读写）、`quectel_clause45_Interface`（C22 识别+C45 寄存器）、`general_phy_dump`（dump 0x0–0x1F 到 dmesg）。
5. **关键 PHY 寄存器（YT8521）**：0x00 控制（速率/自协商/复位 bit15）、0x11 状态（链路 bit10、速率 bit15:14）、0xA001 模式/IO 电压（Mode_sel bit2:0、Cfg_ldo bit5:4）、0xA003 TX/RX 延时（RX bit13:10、TX 百兆 bit7:4 / 千兆 bit3:0，150ps/step）。
6. **故障特征值**：启动日志 `PHY (UID 0x12345678)` = **未识别到真实 PHY**（走通用驱动，链路不可信）；正常 YT8521 为 `UID 0x11a`。
7. **千兆丢包排查心法**：降到百兆测试以排除速率因素 → 调 TX/RX 延时 → 查 RGMII 硬件信号质量。
8. **功耗**：千兆链路 ~0.7W；休眠时可对 PHY 断电（`suspend-not-keep-power` + LDO GPIO 控制）降耗。
9. **ping 不通但链路正常**：优先怀疑电脑端防火墙 / IP 非 192.168.225.x 网段 / 速率非自协商。

*（本分析已覆盖 PDF 全部 30 页：封面 + 正文 p1–29。）*

<!-- GENERATION_COMPLETE -->
