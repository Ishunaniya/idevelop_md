# EC200A-CN(TA) QuecOpen RGMII 应用指导 — 完整分析

> **源文档**：Quectel_EC200A-CN(TA)_QuecOpen_RGMII_应用指导_V1.0.0_Preliminary_20220721.pdf
> **适用平台**：LTE Standard 模块系列 — EC200A-CN(TA) QuecOpen（基于 Linux 的嵌入式开发平台）
> **版本**：1.0.0　**日期**：2022-07-21　**状态**：临时文件（Preliminary）
> **原文页数**：25 页　**底层芯片**：ASR1803　**示例 PHY**：裕泰 YT8521
> **内容范围**：RGMII 功能的硬件电路设计、软件配置、调试方法

---

## 文档历史（修订记录）

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| - | 2022-04-15 | Larry ZHANG | 文档创建 |
| 1.0.0 | 2022-07-21 | Larry ZHANG / Eyelyn TANG | 临时版本 |

---

## 1 引言

EC200A-CN(TA) 模块支持 **QuecOpen®** 方案（开源、基于 Linux 的嵌入式开发平台）。本文档介绍 QuecOpen® 方案下 EC200A-CN(TA) 模块的 **RGMII 功能**，包括**硬件电路设计、软件配置以及调试方法**，帮助开发人员简易快速地进行开发。详见参考文档 [1]。

> RGMII = Reduced Gigabit Media Independent Interface（吉比特介质独立接口）。模块通过内部 **EMAC（以太网接入控制器）** + 外部 **PHY** 芯片实现以太网口。

---

## 2 硬件管脚分配

完整的硬件管脚资源参考文档 [2]。PHY 部分管脚的分配情况如下：

### 表 1：PHY 部分管脚分配

| 引脚号 | 引脚描述 | 复用功能 1（默认） |
|---|---|---|
| 6 | PHY 复位 | `gpio_20` |
| 52 | PHY 电源使能 | `gpio_43` |

> **备注**：目前选用一个 GPIO 管脚作为 PHY 的电源使能管脚。当模块休眠时，为了降低功耗，在 PHY 休眠时**断开 PHY 的供电**，在 PHY 唤醒时**重新上电**。

---

## 3 设备树介绍

设备树主要对**管脚、时钟、电源**等进行配置。

### 3.1 主要配置文件 `arch/arm/boot/dts/asr1803-p401.dts`

```dts
eth0: asr-eth@0xd4281800 {
    status = "disabled";
    compatible = "asr,asr-eth";
    pinctrl-names = "default", "rgmii-pins";
    pinctrl-0 = <&emac_pmx_func0 &emac_pmx_func2 &emac_pmx_func3>;
    pinctrl-1 = <&emac_pmx_func0 &emac_pmx_func1 &emac_pmx_func2 &emac_pmx_func3>;
    reg = <0xd4281800 0x200>;
    interrupts = <10 11>;
    lpm-qos = <PM_QOS_CPUIDLE_BLOCK_AXI>;
    clocks = <&soc_clocks ASR1803_CLK_EMAC>;
    clock-names = "emac-clk";

    reset-gpio = <&gpio 23 0>;
    reset-active-low;

    reset-delays-us = <0 100000 100000>;

    clk-tuning-enable;
    /* clk-config(32bit)
     *
     * clk_sel(clk-config[23:16])
     * RGMII:
     *  tx | clk_sel: 0 - from external RX clock
     *     |          1 - from inverted external RX clock
     *  rx | clk_sel: 0 - from external RX clock
     *     |          1 - from inverted external RX clock
     *
     * RMII:
     *  tx | clk_sel: 0 - RMII clock
     *     |          1 - Inverted RMII clock
     *  rx | clk_sel: 0 - RMII clock
     *     |          1 - Inverted RMII clock
     */
    tx-clk-config = <0x0>;
    rx-clk-config = <0x0>;
    3v3-enable = <1>; /* IO voltage, 1 - 3.3v, 0 - 1.8v */

    phy-handle = <&phy3>;

    /* enable fix link for ethernet switch */
    /*
            fixed-link {
                    speed = <100>;
                    full-duplex;
                    phy-mode = "rmii";
    */
};
```

### 3.2 补充配置文件 `ql-ol-kernel/arch/arm/boot/dts/ql-asr1803-overlay.dtsi`

```dts
&eth0 {
//eyelyn.tang 20220425 add rgmii
    status = "ok";
//end
    /delete-property/ tx-clk-phase;
    /delete-property/ rx-clk-phase;
    tx-clk-config = <0>;
    rx-clk-config = <0>;
    3v3-enable = <0>;            /* IO voltage, 1 - 3.3v, 0 - 1.8v */
    phy-handle = <&phy0>;
    reset-gpio = <&gpio 20 0>;
    enable-suspend;
    suspend-not-keep-power;
    ldo-gpio = <&gpio 43 0>;     //The gpio43 is power enable pin.
    ldo-active-low;
    ldo-delays-us = <0 10000 10000>;

    mdio: mdio-bus {
        phy0: phy@0 {
            reg = <0x3>;
        };
        /delete-node/ phy@3;
        /delete-node/ phy@1;
    };
};
```

> 关键属性说明：
> - `status = "ok"`：使能 eth0（overlay 覆盖主 dts 的 `disabled`）。
> - `reset-gpio = <&gpio 20 0>`：PHY 复位脚（GPIO20，对应引脚 6）。
> - `ldo-gpio = <&gpio 43 0>` + `ldo-active-low`：PHY 电源使能脚（GPIO43，对应引脚 52）。
> - `enable-suspend` / `suspend-not-keep-power`：休眠时对 PHY 断电以降耗。
> - `3v3-enable = <0>`：IO 电压，**1=3.3V，0=1.8V，默认 1.8V**。
> - `phy@0 { reg = <0x3>; }`：MDIO 总线上 PHY 地址为 3。

---

## 4 驱动加载

EC200A-CN(TA) 模块通过 **EMAC 驱动**进行**内部 MAC 和外部 PHY 的初始化及控制**。

### 4.1 EMAC 设备

模块目前支持的 EMAC 驱动：
- **EMAC**：该驱动**内嵌在内核中，随 Linux 系统自动加载，且不可卸载**。
- 驱动文件：`ql-ol-kernel/drivers/net/ethernet/asr/emac_eth.c`

### 4.2 加载 EMAC 驱动

EMAC 驱动内嵌在内核中，随内核加载，**无需手动加载**，模块上电后将自动加载该驱动。

### 4.3 PHY 设备

- PHY 驱动文件：`ql-ol-kernel/drivers/net/phy/quectel_phy.c`
- 当外部**未接入 PHY** 时，会使用**通用 PHY** 启动 `eth0` 网卡。
- 当外部**有接入 PHY** 时，会**获取 PHY ID** 进行 PHY 驱动的适配。

### 4.4 加载 PHY 设备驱动

**（情况一）接入 YT8521，PHY 驱动加载成功**，获取到的 **PHY ID 为 `0x11a`**，日志示例：

```
[ 26.840699] emac_phy_connect: phy@0
[ 26.844267] ===> set eamc interface: rgmii
[ 26.872022] enter yt8521_config_init
[ 26.884893] emac_phy_connect:  eth0: attached to PHY (UID 0x11a) Link = 0 irq=-1
[ 26.907188] clk_phase_rgmii_set phase:0 direction:tx 0x30007 0x30007
[ 26.913654] clk_phase_rgmii_set phase:0 direction:rx 0x30007 0x30007
[ 27.059383] MDIO clock div: 0x60
[ 27.062921] emac_adjust_link link:1 speed:1000M duplex:Full
[ 27.086528] br-lan: port 1(eth0) entered blocking state
[ 27.098057] br-lan: port 1(eth0) entered disabled state
[ 27.116174] device eth0 entered promiscuous mode
[ 27.846189] br-lan: port 1(eth0) entered blocking state
[ 27.851496] br-lan: port 1(eth0) entered forwarding state
[ 27.874096] IPv6: ADDRCONF(NETDEV_CHANGE): br-lan: link becomes ready
```

**（情况二）没有接入 PHY**，无法获取到 PHY ID，EMAC 驱动会**指定 PHY ID 为 `0x12345678`**，然后适配**通用驱动**，日志示例：

```
[ 29.641687] emac_phy_connect: phy@0
[ 29.653673] ===> set eamc interface: rgmii
[ 29.658157] emac_phy_connect:  eth0: attached to PHY (UID 0x12345678) Link = 0 irq=-1
[ 29.675145] clk_phase_rgmii_set phase:0 direction:tx 0x30007 0x30007
[ 29.681611] clk_phase_rgmii_set phase:0 direction:rx 0x30007 0x30007
[ 29.733186] MDIO clock div: 0x60
[ 29.753774] emac_adjust_link link:1 speed:1000M duplex:Full
...
```

---

## 5 PHY 常见配置

### 5.1 寄存器

> 本章以 **YT8521 PHY** 为例，介绍控制寄存器、状态寄存器和复位寄存器。若使用其他型号 PHY，需对照 PHY 芯片官方手册，在对应寄存器上进行类似操作。

#### 5.1.1 控制寄存器（0x00）

PHY 可通过配置控制寄存器配置**速率**和**自适应模式**，配置寄存器为 **`0x00`**。

##### 图 1：YT8521 PHY 控制寄存器（0x00）

| Bit | Symbol | Access | Default | Description |
|---|---|---|---|---|
| 15 | Reset | RW SC | 0x0 | PHY 软件复位。写 1 立即复位，完成后该位自动清零。0=Normal operation，1=PHY reset |
| 14 | Loopback | RW SWC | 0x0 | 内部环回控制。1'b0=禁用环回，1'b1=使能环回 |
| 13 | Speed_Selection(LSB) | RW | 0x0 | speed_selection[1:0] 的 LSB。链路速率可由自协商或手动选择（手动选择须先清 bit12=0）。Bit6,Bit13 组合：`11`=Reserved，`10`=1000Mb/s，`01`=100Mb/s，`00`=10Mb/s |
| 12 | Autoneg_En | RW | 0x1 | 1=使能自协商；0=禁用自协商 |
| 11 | Power_down | RW SWC | 0x0 | 1=Power down；0=Normal operation。从 power down 切回 normal 时即使用户未置 bit15(RESET)/bit9(RESTART_AUTO_NEGOTIATION)，软件复位与自协商也会执行 |
| 10 | Isolate | RW SWC | 0x0 | 将 PHY 与 RGMII/SGMII/FIBER 隔离。1'b0=Normal mode，1'b1=Isolate mode |
| 9 | Re_Autoneg | RW SC SWS | 0x0 | 硬/软复位后自协商自动重启（不论 bit9）。1=Restart Auto-Negotiation，0=Normal operation |
| 8 | Duplex_Mode | RW | 0x1 | 双工模式可由自协商或手动选择（手动须 bit12=0）。1=Full Duplex，0=Half Duplex |
| 7 | Collision_Test | RW SWC | 0x0 | 置 1 时 TX_EN 有效则 COL 信号有效。1=Enable COL signal test，0=Disable |
| 6 | Speed_Selection(MSB) | RW | 0x1 | 见 bit13 |
| 5:0 | Reserved | RO | 0x0 | 保留，写 0，读忽略 |

**示例：配置关闭自适应模式 + 100 M 速率**，对 0x00 寄存器写入：

```sh
echo 0x00-0x2100 > /sys/kernel/debug/eth/phy_reg_dump
```

> 说明：`0x2100` = bit13(Speed LSB)=1 + bit8(Duplex)=1 → 即 100Mb/s + Full Duplex，bit12(Autoneg)=0 关闭自协商。

#### 5.1.2 状态寄存器（0x11）

PHY 是否为 Link 状态，可通过 **`0x11` 状态寄存器 BIT[10]** 确定：BIT[10]=1 → **Link up**；否则 **Link down**。

##### 图 2：YT8521 PHY 状态寄存器（0x11）

| Bit | Symbol | Access | Default | Description |
|---|---|---|---|---|
| 15:14 | Speed_mode | RO | 0x0 | 仅当 bit11=1 时有效（自协商完成或禁用时 bit11 置位）。`11`=Reserved，`10`=1000Mbps，`01`=100Mbps，`00`=10Mbps |
| 13 | Duplex | RO | 0x0 | 仅当 bit11=1 时有效。1=Full-duplex，0=Half-duplex |
| 12 | Page Received real-time | RO | 0x0 | 1=Page received，0=Page not received |
| 11 | Speed and Duplex Resolved | RO | 0x0 | 自协商禁用时该位置 1 表示强制速率模式。1=Resolved，0=Not resolved |
| 10 | Link status real-time | RO | 0x0 | **1=Link up，0=Link down** |
| 9:7 | Reserved | RO | 0x0 | Reserved |
| 6 | MDI Crossover Status | RO | 0x0 | 仅当 bit11=1 时有效 |

#### 5.1.3 复位寄存器（0x00 BIT[15]）

通过设置 PHY 寄存器相关标志位可对 PHY 进行**软件复位**。可通过 **`0x00` 寄存器 BIT[15]** 进行复位。

> **图 3：YT8521 PHY 软件复位寄存器** — 寄存器布局与控制寄存器（0x00）相同，bit15=Reset（写 1 立即复位，完成自动清零）。

> ⚠️ **备注**：当前 YT8521 使用 **UTP_TO_RGMII 模式**，可通过 `0xA001` 寄存器 BIT[2:0] 确认。如果使用软件 reset 操作，注意其复位的是 **UTP 内部的逻辑寄存器**（参考供应商提供的 YT8521 Datasheet 文档）。

### 5.2 设备属性文件

模块支持通过读写设备属性文件的方式进行 PHY 寄存器的读取和配置等操作。

#### 5.2.1 `phy_reg_dump` 属性文件

用于读写**外部 PHY 寄存器**。
- 若 PHY 可通过 **Clause 22** 协议识别到 PHY ID，则该节点**只能用 Clause 22** 协议访问；
- 若 PHY 只支持 **Clause 45** 协议识别 PHY ID，则该节点**只能用 Clause 45** 协议访问。

读写命令如下：

**1) Clause 22 读取寄存器：**

```sh
echo <寄存器地址> > /sys/kernel/debug/eth/phy_reg_dump
cat /sys/kernel/debug/eth/phy_reg_dump
```

示例（读取 0x00 寄存器）：

```sh
echo 0x00 > /sys/kernel/debug/eth/phy_reg_dump
cat /sys/kernel/debug/eth/phy_reg_dump
```

**2) Clause 22 配置寄存器：**

```sh
echo <寄存器地址>-<设置参数> > /sys/kernel/debug/eth/phy_reg_dump
```

示例（设置 0x00 寄存器数值为 0x0140）：

```sh
echo 0x00-0x0140 > /sys/kernel/debug/eth/phy_reg_dump
```

**3) Clause 45 读取寄存器：**

```sh
echo <寄存器地址>-<设备 ID> > /sys/kernel/debug/eth/phy_reg_dump
cat /sys/kernel/debug/eth/phy_reg_dump
```

示例（读取设备 ID=7、寄存器地址 0x3c）：

```sh
echo 0x3c-7 > /sys/kernel/debug/eth/phy_reg_dump
cat /sys/kernel/debug/eth/phy_reg_dump
```

**4) Clause 45 配置寄存器：**

```sh
echo <寄存器地址>-<设备 ID>-<设置参数> > /sys/kernel/debug/eth/phy_reg_dump
```

示例（设置设备 ID=7、寄存器地址 0x3c 的数值为 0x0004）：

```sh
echo 0x3c-7-0x0004 > /sys/kernel/debug/eth/phy_reg_dump
```

#### 5.2.2 `quectel_clause45_Interface` 属性文件

若 PHY ID 可通过 Clause 22 协议识别，但有些寄存器需要通过 **Clause 45** 协议访问，则可操作 `quectel_clause45_Interface` 属性文件。操作方式与使用 Clause 45 协议读写 `phy_reg_dump` 一致。
- 文件路径：`/sys/kernel/debug/eth/quectel_clause45_Interface`

#### 5.2.3 `general_phy_dump` 属性文件

可获取 PHY **0x0~0x1F** 的寄存器值，获取的寄存器值输出到**内核日志**中。调试阶段对该属性文件执行 `cat`：

```sh
cat /sys/kernel/debug/eth/general_phy_dump
```

### 5.3 IO 电压设置

#### 5.3.1 PHY IO 寄存器配置

YT8521 PHY 支持的 IO 电压为 **1.8 V、2.5 V、3.3 V** 电压域。由于**模块 EMAC IO 仅支持 1.8 V 和 3.3 V**，故 PHY 的 **2.5 V 电压不可配置**。如需将外部 PHY IO 电压设为 3.3 V，需对 **`0xA001` 寄存器 BIT[5:4]** 进行设置。

##### 图 4：YT8521 PHY VDDIO 电压寄存器（0xA001）

| Bit | Symbol | Access | Default | Description |
|---|---|---|---|---|
| 15 | Sw_rst_n_mode | RW SC | 0x1 | 整片软件复位，chip mode 改变时也会触发，低有效，自清 |
| 14:12 | Reserved | RO | 0x0 | Reserved |
| 11 | Iddq_mode | RW | 0x0 | Iddq test mode |
| 10:9 | Reserved | RO | 0x0 | Reserved |
| 8 | Rxc_dly_en | RW POS | 0x1 | rgmii clk 2ns delay control，依赖 strapping |
| 7 | Reserved | RO | 0x0 | Reserved |
| 6 | En_ldo | RW | 0x1 | rgmii ldo enable，默认 0，power on strapping 完成后置 1 |
| 5:4 | Cfg_ldo | RW POS | 0x0 | RGMII ldo 电压控制，依赖 strapping：`2'b11`=1.8v，`2'b10`=1.8v，`2'b01`=2.5v，`2'b00`=3.3v |
| 3 | Reserved | RO | 0x0 | Reserved |
| 2:0 | Mode_sel | RW POS | 0x0 | chip mode，依赖 strapping：`3'b000`=UTP_TO_RGMII；`3'b001`=FIBER_TO_RGMII；`3'b010`=UTP_FIBER_TO_RGMII；`3'b011`=UTP_TO_SGMII；`3'b100`=SGPHY_TO_RGMAC；`3'b101`=SGMAC_TO_RGPHY；`3'b110`=UTP_TO_FIBER_AUTO；`3'b111`=UTP_TO_FIBER_FORCE |

> 注意 `Cfg_ldo` 的取值含义：`00`=3.3v，`01`=2.5v，`10`/`11`=1.8v。

#### 5.3.2 模块设备树配置

外部 PHY 的 IO 硬件电压需要与 **EMAC 驱动的 IO 电压匹配**。EMAC 驱动关于 IO 电压的配置见 `&eth0` 节点中的 `3v3-enable` 属性（见 3.2 节配置）：

```dts
3v3-enable = <0>;            /* IO voltage, 1 - 3.3v, 0 - 1.8v */
```

- 可通过修改设备树文件 `3v3-enable` 属性改变 PHY IO 的电压值。
- 模块当前**仅支持 3.3 V 和 1.8 V** 电压域，**默认为 1.8 V**。
- 设备树路径：`ql-ol-kernel/arch/arm/boot/dts/ql-asr1803-overlay.dtsi`。

### 5.4 RX/TX 延时设置

YT8521 PHY 支持 RX/TX 延时设置，可设置 **`0xA003` 寄存器**的 BIT[13:10] 和 BIT[7:4] 或 BIT[3:0]。

##### 图 5：YT8521 PHY TX/RX 延时寄存器（0xA003）

| Bit | Symbol | Access | Default | Description |
|---|---|---|---|---|
| 15 | Rgmac_cfg_mode | RW | 0x0 | 当 chip mode 为 SGPHY_TO_RGMAC 时，控制 RGMII speed/duplex/link status 来源。1=来自 EXT 0xA004；0=来自 RGMII OOB。详见 EXT 0xA005 |
| 14 | Tx_clk_sel | RW | 0x0 | 0=用原始 RGMII TX_CLK 驱动 TX_CLK delay train；1=用反相 RGMII TX_CLK。用于 debug |
| 13:10 | Rx_delay_sel | RW | 0x0 | RGMII RX_CLK delay train 配置，约 **150ps/step** |
| 9 | En_rgmii_fd_crs | RW | 0x0 | 见 EXT 0xA003 bit[8] |
| 8 | En_rgmii_crs | RW | 0x0 | 0=不将 GMII/MII CRS 编码进 RGMII OOB；1=半双工或 EXT 0xA003 bit[9]=1 时将其编码进 RGMII OOB |
| 7:4 | Tx_delay_sel_fe | RW | 0xf | 速率 100Mbps 或 10Mbps 时的 RGMII TX_CLK delay train 配置，典型 **150ps/step** |
| 3:0 | Tx_delay_sel | RW | 0x1 | 速率 1000Mbps 时的 RGMII TX_CLK delay train 配置，典型 **150ps/step** |

> ⚠️ **备注**：
> 1. 移远调试过程中发现，由于通信速率 **1000 Mbps 对硬件要求较高**，不同硬件版本需调整 PHY 的 RX/TX 延时设置后，RGMII 功能方可正常工作。
> 2. 若确认 PHY 状态已经 Link up，但 RGMII 功能不可正常使用，可通过**修改 TX/RX 延时**尝试解决。

### 5.5 驱动耗流调试

- 以 YT8521 PHY 芯片为例，PHY 芯片进入低功耗模式后的耗流数据见原文图 6（**进入低功耗时耗流依然较高**）。
- 为进一步降低功耗，可在模块休眠时对 PHY 芯片**断电处理**，以降低整体耗流。
- 若调试过程中出现网络异常，可通过 `ifconfig` 查询 `eth0` 网卡设备。若 RGMII 通信正常，可查看到 `eth0` 网卡上 **RX/TX 均有数据包**。
- 若出现网络不通的情况，请参考第 6.3 章进行排查和调试。

---

## 6 RGMII 调试说明

### 6.1 EVB 接 PHY 环境

在 EC200A-CN(TA) 模块的 **TE-A** 上插上 **PHY TE-A**（见原文图 7：PHY 与模块 TE-A 环境）。

### 6.2 电脑端设置（以 Windows 为例）

1. 电脑端 **IPv4 地址需修改为自动获取**（图 8）。
2. 电脑端**网卡速率改为自动协商**（图 9）。
3. ⚠️ **关闭电脑端的所有防火墙设置**。若存在防火墙，则模块 ping 不通电脑端。

### 6.3 调试步骤及注意事项

初次调试 RGMII 功能，可按下述步骤确认：

**步骤 1** — 确认 EMAC 驱动是否加载成功：
- 通过 `ifconfig` 查询是否已存在 `eth0` 网卡设备来判断。
- 若存在 → 加载成功；若不存在 → 建议检查 **PHY 供电是否正常、PHY IO 电压是否匹配**。

**步骤 2** — 驱动加载成功后，查看寄存器确认 PHY 是否 **Link up**：
- 若为 Link down → 检查 **PHY 速率选择**。

**步骤 3** — 若 PHY 已 Link up，则确认电脑端配置正确。插拔网线，查看电脑端是否分配到 `192.168.225.1` 同网段的 IP。若未分配 IP，按如下排查：
- **(1)** 检查 `eth0` 网卡是否在 `bridge0`：用 `brctl show` 查看，如不存在用 `brctl addif bridge0 eth0` 添加。
- **(2)** 插拔网线，查看 `eth0` 网卡上 RX/TX 是否有数据收发：
  - 若**没有**数据收发 → 调整 PHY 的 RX/TX 延时设置并再次尝试（参考第 5.4 章）；若仍无效，建议检查 **RGMII 硬件电路信号质量**。
  - 若**有**数据收发 → 建议在模块和电脑端**抓取网络数据包**，分析 **DHCP 网络协议包**。

**步骤 4** — 电脑端正常分配 IP 后：
- 若**模块 ping 不通电脑，但电脑可以 ping 通模块** → 需要**关闭电脑端的网络防火墙**。
- 若 ping 过程中有数据**丢包** → 建议检查 **RGMII 的 RX/TX 信号质量**。

---

## 7 附录 参考文档及术语缩写

### 表 2：参考文档

| 序号 | 文档名称 |
|---|---|
| [1] | Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导 |
| [2] | Quectel_EC200A-CN(TA)_QuecOpen_硬件设计手册 |

### 表 3：术语缩写

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| DHCP | Dynamic Host Configuration Protocol | 动态主机配置协议 |
| EMAC | Ethernet Media Access Controller | 以太网接入控制器 |
| IoT | Internet of Things | 物联网 |
| IPv4 | Internet Protocol version 4 | 互联网通信协议第四版 |
| MAC | Medium Access Control | 媒体访问控制 |
| PHY | Physical | 端口物理层 |
| RGMII | Reduced Gigabit Media Independent Interface | 吉比特介质独立接口 |
| TX/RX | Transport/Receive | 发送/接收 |

---

## 关键要点速查（实践提炼）

- 模块 = 内部 EMAC（内核内嵌、不可卸载、自动加载）+ 外部 PHY（示例 YT8521，ID `0x11a`）。未接 PHY 时用通用驱动（ID `0x12345678`）。
- 关键 GPIO：**PHY 复位 = GPIO20（引脚6）**，**PHY 电源使能 = GPIO43（引脚52）**，休眠断电省功耗。
- 设备树两文件：`asr1803-p401.dts`（主，默认 `disabled`）+ `ql-asr1803-overlay.dtsi`（overlay，`status="ok"` 真正使能 + 复位/电源/IO 电压/MDIO 地址）。
- **IO 电压只支持 1.8V 和 3.3V**（2.5V 不可用），由 `3v3-enable`（1=3.3V/0=1.8V，默认1.8V）控制，**必须与外部 PHY 硬件电压匹配**。
- PHY 寄存器读写走 `/sys/kernel/debug/eth/phy_reg_dump`（Clause22/45）；Link 判定看 0x11 寄存器 BIT[10]；速率/双工/自协商看 0x00 寄存器。
- **1000Mbps 调试关键**：硬件要求高，常需调 `0xA003` 的 RX/TX 延时（150ps/step）。Link up 但不通 → 优先调延时。
- 调试四查：`ifconfig` 看 eth0 → 寄存器看 Link up → `brctl show`/DHCP 抓包 → 关 PC 防火墙。
