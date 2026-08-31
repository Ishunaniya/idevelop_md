# Quectel AG35-CET & AG35-EUT QuecOpen(SDK) SPI 开发指导 — 全文分析

> **文档全名**：AG35-CET&AG35-EUT QuecOpen(SDK) SPI 开发指导
> **适用模块**：AG35-CET / AG35-EUT（LTE Standard 模块系列）
> **版本**：1.0
> **日期**：2024-11-29
> **状态**：受控文件
> **总页数**：35 页（PDF 页脚标注 1/34 ~ 34/34，即正文 34 页 + 封面 1 页）
> **本分析覆盖范围**：全部 35 页（封面 + 正文 1~34）
> **平台背景**：内核为 Linux OpenWrt 5.4.195，SoC 为 ASR1806（asr,asr-spi 控制器）

---

## 0. 文档元信息与修订历史

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| - | 2024-03-08 | Aurora JIANG | 文档创建 |
| 1.0 | 2024-11-29 | Gabriel LI | 受控版本；更新加载 SPI 设备驱动的部分参数及默认值（第 5 章、第 6.2 章） |

**解读**：V1.0 相对初稿的关键变化集中在「以 `insmod` 方式加载 SPI 设备驱动时的传参」以及「功能验证示例」两块（第 5 章 SPI 设备驱动、第 6.2 章功能验证）。说明早期版本的传参/默认值有调整，移植或复用旧脚本时应以本版的参数表为准。

### 文档结构（目录概览）

- **第 1 章 引言** — 文档定位、适用范围
- **第 2 章 SPI 说明** — SPI 框架三层结构、4 线/6 线 SPI 区别
- **第 3 章 引脚定义及使用** — 引脚定义表、标准 4 线定义、扩展 6 线使用（含请求发起时序）
- **第 4 章 设备树配置** — SPI1（ssp1）、SPI2（ssp0）、复用 SPI（ssp2）三类设备树配置
- **第 5 章 SPI 设备驱动** — 标准 4 线（spidev.c）、扩展 6 线（quec_spi_chn.c）驱动及 insmod 传参
- **第 6 章 编译示例及功能验证** — 编译步骤 + 主/从模式 + 6 线功能验证
- **第 7 章 驱动调试**
- **第 8 章 附录** — 参考文档及术语缩写

---

## 1. 引言（第 7 页）

- AG35-CET 和 AG35-EUT 模块支持 **QuecOpen®** 方案。QuecOpen® 是基于 Linux 的嵌入式开发平台，用于简化 IoV（车联网）应用的软件设计和开发过程。QuecOpen 详情见参考文档 [1]。
- 本文档适用于 **SDK 构建环境**的 QuecOpen 方案，主要介绍 AG35-CET / AG35-EUT 模块的 **SPI 功能开发**，覆盖：SPI 接口引脚定义和使用、设备树配置、SPI 设备驱动、编译示例及功能验证、驱动调试。

---

## 2. SPI 说明（第 8~9 页）

### 2.1 总体能力

- AG35-CET / AG35-EUT 模块**默认提供 2 路 SPI 接口**（SPI1、SPI2）。
- 支持**主从模式**（master / slave）。
- 支持的**最大时钟频率为 26 MHz**。

### 2.2 SPI 框架的三层组成（用户仅需修改 SPI 设备驱动）

模块的 SPI 框架由三部分组成（见图 1：SPI 框架）：

1. **SPI 核心（SPI core）**
   - 提供 SPI 总线驱动和设备驱动的**注册/注销**方法、SPI 通信方法、与具体控制器无关的代码，以及检测设备、检测设备地址的上层代码。

2. **SPI 总线驱动（SPI 控制器）**
   - 是 SPI 控制器的软件实现，由 CPU 控制，使 SPI 控制器与从设备完成数据通信。

3. **SPI 设备驱动**
   - 即客户的 SPI 从设备驱动，是对 SPI 硬件体系结构中**设备端**的实现；设备挂接在受 CPU 控制的 SPI 控制器上，通过 SPI 控制器与 CPU 交换数据。

### 2.3 图 1：SPI 框架（层次映射，重要）

文档用一张框架图把「概念层」映射到「实际文件/设备树节点」：

| 概念层 | 对应实体 | 说明 |
|---|---|---|
| CPU | — | 主控 |
| OS | **Linux OpenWrt 5.4.195** | 经 **Platform bus** 连接 |
| SPI 适配器（SPI adapter） | 设备树节点 **ssp0**；compatible driver：`ql-ol-kernel/drivers/spi/spi-asr.c` | SPI 控制器的设备树配置及驱动 |
| SPI 设备驱动 | 1. `ql-ol-kernel/drivers/spi/spidev.c`<br>2. `ql-ol-kernel/drivers/spi/quec_spi_chn.c` | 经 **SPI bus** 连接；SDK 提供的两种 SPI 设备驱动 |

**关键文件路径（务必记住）**：
- 控制器驱动：`ql-ol-kernel/drivers/spi/spi-asr.c`
- 标准 4 线设备驱动：`ql-ol-kernel/drivers/spi/spidev.c`
- 扩展 6 线设备驱动：`ql-ol-kernel/drivers/spi/quec_spi_chn.c`

### 2.4 4 线 SPI 与 6 线 SPI 的区别（核心概念）

SPI 从设备驱动分为「标准 4 线」和「扩展 6 线」两种：

- **4 线 SPI**：通常用来连接 **SPI Flash、LCD** 等，**由模块发起请求**（模块为主）。对应驱动 `spidev.c`。
- **6 线 SPI**：通常用来与 **MCU 通信**，**模块、MCU 均可发起请求**，相比串口通信更高速。对应驱动 `quec_spi_chn.c`。在标准 4 线（CLK/CS/DIN/DOUT）基础上增加了两根握手信号线 **SPI_SRDY** 与 **SPI_MRDY**。

---

## 3. 引脚定义及使用（第 10~13 页）

### 3.1 引脚定义（第 10~11 页）

QuecOpen 方案下，模块默认提供 2 路 SPI：SPI1 和 SPI2。

#### 表 1：SPI1（软件设备树节点：**ssp1**）引脚定义

| 引脚名 | 引脚号 | I/O | 复用功能 0 |
|---|---|---|---|
| SPI1_CLK | 80 | DO | GPIO_21 |
| SPI1_CS | 79 | DO | GPIO_22 |
| SPI1_DIN | 78 | DIO | GPIO_24 |
| SPI1_DOUT | 77 | DIO | GPIO_23 |

#### 表 2：SPI2（软件设备树节点：**ssp0**）引脚定义

| 引脚名 | 引脚号 | I/O | 复用功能 0 |
|---|---|---|---|
| SPI2_CLK | 195 | DO | GPIO_33 |
| SPI2_CS | 192 | DO | GPIO_34 |
| SPI2_DIN | 193 | DIO | GPIO_35 |
| SPI2_DOUT | 194 | DIO | GPIO_36 |

> **关键易错点**：软件设备树节点名与 SPI 编号是**交叉的**——
> - **SPI1 → 设备树节点 `ssp1`**
> - **SPI2 → 设备树节点 `ssp0`**（注意不是 ssp2！）
> - **复用 SPI → 设备树节点 `ssp2`**
> 这个映射在后文设备树配置、insmod 传 `busnum` 时反复用到，极易混淆。

#### 表 3：复用 SPI（软件设备树节点：**ssp2**）引脚定义

模块支持把如下 GPIO 复用为 SPI（复用功能 **1**，注意是功能 1 而非功能 0）：

| 引脚名 | 引脚号 | I/O | 复用功能 1 |
|---|---|---|---|
| GPIO5 | 147 | DO | SSP2_CLK |
| GPIO6 | 150 | DO | SSP2_FRM |
| GPIO7 | 159 | DO | SSP2_TXD |
| GPIO8 | 143 | DI | SSP2_RXD |

**备注（针对复用 SPI）**：
1. **非默认的复用功能需配置后才可生效**。有关 SPI 复用功能的详情，请参考文档 [2]。
2. **SSP2 接口可以定义成 SPI；当用作 SPI 时，仅支持主模式（master only）**。

> **解读**：复用 SPI（ssp2）受限——只能当主机用，不能做从机；而原生 SPI1/SPI2 可主可从。设备树里 ssp2 默认是 `disabled`，需手动打开（见 4.3）。

### 3.2 引脚使用

#### 3.2.1 标准 4 线 SPI 定义（第 11 页）——以 SPI1 为例

#### 表 4：SPI1（软件设备树节点：ssp1）引脚定义（含描述）

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| SPI1_CLK | 80 | DO | SPI 时钟 | 1.8 V 电压域，不用则悬空 |
| SPI1_CS | 79 | DO | SPI 片选 | 同上 |
| SPI1_DIN | 78 | DIO | SPI 数据输入 | 同上 |
| SPI1_DOUT | 77 | DIO | SPI 数据输出 | 同上 |

> **电气注意**：SPI 引脚为 **1.8 V 电压域**；不使用时**悬空**即可。

#### 3.2.2 扩展 6 线 SPI 使用（第 12~13 页）

##### 3.2.2.1 引脚使用 —— 以 SPI1 为例

#### 表 5：扩展 6 线 SPI 引脚功能描述

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| SPI1_CLK | 80 | DO | SPI 时钟 | 1.8 V 电压域，不用则悬空 |
| SPI1_CS | 79 | DO | SPI 片选 | 同上 |
| SPI1_DIN | 78 | DIO | SPI 数据输入 | 同上 |
| SPI1_DOUT | 77 | DIO | SPI 数据输出 | 同上 |
| **SPI_SRDY** | 可由用户选择（**默认选择引脚 62，即 GPIO3**） | DI | SPI 从机 ready 信号，空闲为低；当 SPI 从机准备好接收或发送数据时，该引脚被拉高 | — |
| **SPI_MRDY** | 可由用户选择（**默认选择引脚 144，即 GPIO4**） | DO | 模块输出信号，空闲为低；当模块要输出数据时，驱动自动拉高该引脚 | — |

> **解读**：6 线 = 标准 4 线 + SRDY（从机就绪，输入到模块）+ MRDY（主机就绪，模块输出）。SRDY/MRDY 的引脚号不固定，可由用户选择，默认 SRDY=GPIO3(62)、MRDY=GPIO4(144)。两信号**空闲均为低电平**，握手时拉高。

##### 3.2.2.2 请求发起流程（握手时序，核心）

6 线 SPI 双方都能发起请求，用 MRDY/SRDY 两条握手线协调。文档给出两张时序图（图 2、图 3）和对应的步骤描述。

###### 图 2：模块发起请求

**模块发送数据流程（模块作为发起方）**：
1. 驱动**自动拉高 SPI_MRDY** 通知 SPI 从机。
2. 判断 **SPI_SRDY 是否为高电平**，若不为高电平则**等待 SPI_SRDY 的上升沿中断**。
3. 收到 **SPI_SRDY 上升沿中断**，**开始 SPI 传输**。
4. 传输完毕，若要继续发送数据，则**保持 SPI_MRDY 为高电平**，并继续第 2 步；否则**拉低 SPI_MRDY**。

**SPI 从机接收数据流程（与上面配对）**：
1. 收到 **SPI_MRDY 上升沿中断**，表示模块需要发送数据。
2. 准备好 SPI 传输，并**拉高 SPI_SRDY** 通知模块开始 SPI 传输。
3. 等待 SPI 传输结束，然后**拉低 SPI_SRDY**。
4. 若 SPI_MRDY 为高电平，继续第 2 步。

###### 图 3：SPI 从机发起请求

**SPI 从机发送数据流程（从机作为发起方）**：
1. 准备好 SPI 传输，并**拉高 SPI_SRDY**。
2. 等待 SPI 传输结束，然后**拉低 SPI_SRDY**。
3. 若要继续发送数据，则继续第 1 步。

**模块接收数据流程（与上面配对）**：
1. 收到 **SPI_SRDY 上升沿中断**，表示 SPI slave 需要发送数据。
2. **拉高 SPI_MRDY**，并开始 SPI 传输。
3. 等待传输结束，然后**拉低 SPI_MRDY**。

> **解读（握手语义）**：无论谁发起，传输都以「发起方拉高自己的 RDY → 对端拉高对端 RDY 表示就绪 → 在双方 RDY 均高期间完成一次 SPI 帧 → 发起方/对端按需拉低」为基本节拍。数据交换（Data Exchange）以 **Header → Data → Header** 的形式分段（图中标注），即每次传输先发头再发数据再发尾，便于对端确定长度。这套握手由 `quec_spi_chn.c` 驱动实现，应用层一般无需自己拉 GPIO。

---

## 4. 设备树配置（第 14~18 页）

> 三个 SPI 的设备树节点与文件位置：
> - **总线节点定义**（reg/dma/clock/ssp-id 等）：`ql-ol-kernel/arch/arm/boot/dts/asr1806.dtsi`
> - **总线开关 + 引脚 pinctrl**：`ql-ol-kernel/arch/arm/boot/dts/ql-1806e-common.dtsi`
> 文档强调：这些默认配置**SDK 已设置完成，用户一般无需重新设置**，仅在「把 SPI 脚改作普通 GPIO」或「把 GPIO 复用为 SPI」或「对引脚电平有特殊要求」时才需改。

### 4.1 SPI1 设备树配置（节点 ssp1，第 14~15 页）

#### 4.1.1 SPI1 总线设备树配置（`asr1806.dtsi`，默认配置）

```dts
ssp1: spi@d401b800 {
    compatible = "asr,asr-spi";
    #address-cells = <1>;
    #size-cells = <0>;
    reg = <0xd401b800 0x30>;

    dmas = <&pdma0 54 0
            &pdma0 55 0>;
    dma-names = "rx", "tx";

    asr,ssp-lpm-qos = <PM_QOS_CPUIDLE_BLOCK_AXI>;
    asr,ssp-clock-rate = <13000000>;
    asr,ssp-id = <2>;
    interrupts = <2>;
    asr,ssp-enhancement;
    asr,ssp-disable-dma;
    /* asr,ssp-hold-frame-low; */
    /* asr,spi-master-rxto = <3000>; */
    /* asr,spi-slave-rxto = <262144>; */
    /* asr,spi-pio-interval = <5>; */
    clocks = <&soc_clocks ASR1803_CLK_SSP1>;
    status = "disabled";
};
```

**字段解读（SPI1/ssp1）**：
- `reg = <0xd401b800 0x30>` — 控制器寄存器基址 0xd401b800，长度 0x30。
- `dmas = <&pdma0 54 0 &pdma0 55 0>` + `dma-names = "rx","tx"` — RX 用 PDMA 通道 54，TX 用 55。
- `asr,ssp-clock-rate = <13000000>` — 总线节点默认 13 MHz（注意与第 26 MHz 最大值的区别，这是默认值）。
- `asr,ssp-id = <2>` — SSP 控制器 ID 为 **2**（对应 insmod 传 `busnum`，见第 5 章）。
- `interrupts = <2>` — 中断号 2。
- `asr,ssp-enhancement` — 使能增强模式。
- `asr,ssp-disable-dma` — **禁用 DMA**（用 PIO 方式）。
- `clocks = <&soc_clocks ASR1803_CLK_SSP1>` — 时钟源。
- `status = "disabled"` — 总线节点本身在 `asr1806.dtsi` 里默认 disabled（实际使能在 common.dtsi 里覆盖）。
- 被注释掉的可选项（默认不启用）：`asr,ssp-hold-frame-low`（帧间保持低）、`asr,spi-master-rxto=<3000>`（主机接收超时）、`asr,spi-slave-rxto=<262144>`（从机接收超时）、`asr,spi-pio-interval=<5>`（PIO 间隔）。

> **备注**：如上默认配置已经在 SPI 总线配置文件中设置完成，用户无需重新设置。

#### 4.1.1（续）SPI1 总线开关配置（`ql-1806e-common.dtsi`，默认使能）

```dts
ssp1: spi@d401b800 {
    status = "okay";
    compatible = "asr,asr-spi";
    #address-cells = <1>;
    #size-cells = <0>;
    reg = <0xd401b800 0x30>;
    /* dmas = <&pdma0 54 0
            &pdma0 55 0>;
       dma-names = "rx", "tx"; */
    asr,ssp-lpm-qos = <PM_QOS_CPUIDLE_BLOCK_AXI>;
    asr,ssp-clock-rate = <26000000>;
    asr,ssp-id = <2>;
    interrupts = <2>;
    asr,ssp-disable-dma;
    /* asr,ssp-hold-frame-low */
    clocks = <&soc_clocks ASR1803_CLK_SSP1>;
    pinctrl-names = "default";
    pinctrl-0 = <&ssp1_pmx_func>;
    asr,spi-inc-mode;
};
```

**与默认配置（asr1806.dtsi）的差异（重要）**：
- `status = "okay"` — common.dtsi 把 SPI1 **实际使能**了（覆盖 asr1806.dtsi 的 disabled）。
- `asr,ssp-clock-rate = <26000000>` — 实际时钟提到 **26 MHz**（即最大频率）。
- DMA 被注释掉、保留 `asr,ssp-disable-dma`（PIO）。
- 新增 `pinctrl-names = "default"` + `pinctrl-0 = <&ssp1_pmx_func>` — 绑定引脚复用配置。
- 新增 `asr,spi-inc-mode`。

> **如何把 SPI1 引脚改作普通 GPIO**：将 `status = "okay"` 改为 `status = "disabled"`，即可关闭 SPI1 总线。

#### 4.1.2 SPI1 引脚设备树配置（`ql-1806e-common.dtsi`）

```dts
ssp1_pmx_func: ssp1_pmx_func {
    pinctrl-single,pins = <
        GPIO23 AF7 /* TXD */
        GPIO24 AF7 /* RXD */
        GPIO22 AF7 /* FRM */
        GPIO21 AF7 /* SCLK */
    >;
    DS_MEDIUM;PULL_NONE;EDGE_NONE;SL_NORMAL;
};
```

**引脚复用映射（SPI1，复用功能 AF7）**：
- GPIO23 → TXD（即 SPI1_DOUT，引脚 77）
- GPIO24 → RXD（即 SPI1_DIN，引脚 78）
- GPIO22 → FRM（即 SPI1_CS，引脚 79）
- GPIO21 → SCLK（即 SPI1_CLK，引脚 80）
- 电气属性：`DS_MEDIUM`（驱动能力中等）、`PULL_NONE`（无上下拉）、`EDGE_NONE`（无边沿）、`SL_NORMAL`（普通压摆率）。

> **备注**：默认配置已设置完成，用户无需重新设置；若对引脚电平有特殊要求，可据需修改 SPI 引脚配置。

### 4.2 SPI2 设备树配置（节点 ssp0，第 16~17 页）

#### 4.2.1 SPI2 总线设备树配置（`asr1806.dtsi`，默认配置）

```dts
ssp0: spi@d401b000 {
    compatible = "asr,asr-spi";
    #address-cells = <1>;
    #size-cells = <0>;
    reg = <0xd401b000 0x30>;

    dmas = <&pdma0 52 0
            &pdma0 53 0>;
    dma-names = "rx", "tx";

    asr,ssp-lpm-qos = <PM_QOS_CPUIDLE_BLOCK_AXI>;
    asr,ssp-clock-rate = <13000000>;
    asr,ssp-id = <1>;
    interrupts = <3>;
    asr,ssp-enhancement;
    asr,ssp-disable-dma;
    /* asr,ssp-hold-frame-low; */
    /* asr,spi-master-rxto = <3000>; */
    /* asr,spi-slave-rxto = <262144>; */
    /* asr,spi-pio-interval = <5>; */
    clocks = <&soc_clocks ASR1803_CLK_SSP0>;
    status = "disabled";
};
```

**字段解读（SPI2/ssp0）——与 SPI1 的差异**：
- `reg = <0xd401b000 0x30>` — 基址 0xd401b000（SPI1 是 0xd401b800）。
- `dmas = <&pdma0 52 0 &pdma0 53 0>` — RX=52、TX=53。
- `asr,ssp-id = <1>` — SSP 控制器 ID 为 **1**（SPI1 是 2）。
- `interrupts = <3>` — 中断号 3（SPI1 是 2）。
- `clocks = ... ASR1803_CLK_SSP0` — 用 SSP0 时钟。
- 其余（lpm-qos、enhancement、disable-dma、注释可选项）与 SPI1 相同。

#### 4.2.1（续）SPI2 总线开关配置（`ql-1806e-common.dtsi`，默认使能）

```dts
ssp0: spi@d401b000 {
    status = "okay";
    /* dmas = <&pdma0 52 0
            &pdma0 53 0>;
       dma-names = "rx", "tx"; */
    pinctrl-names = "default";
    pinctrl-0 = <&ssp0_pmx_func>;
    asr,spi-inc-mode;
};
```

> **把 SPI2 引脚改作普通 GPIO**：将 `status = "okay"` 改为 `status = "disabled"` 以关闭 SPI2 总线。

#### 4.2.2 SPI2 引脚设备树配置（`ql-1806e-common.dtsi`）

```dts
ssp0_pmx_func: ssp0_pmx_func {
    pinctrl-single,pins = <
        GPIO36 AF1 /* TXD */
        GPIO35 AF1 /* RXD */
        GPIO34 AF1 /* FRM */
        GPIO33 AF1 /* SCLK */
    >;
    DS_MEDIUM;PULL_NONE;EDGE_NONE;SL_NORMAL;
};
```

**引脚复用映射（SPI2，复用功能 AF1，注意不是 AF7）**：
- GPIO36 → TXD（SPI2_DOUT，引脚 194）
- GPIO35 → RXD（SPI2_DIN，引脚 193）
- GPIO34 → FRM（SPI2_CS，引脚 192）
- GPIO33 → SCLK（SPI2_CLK，引脚 195）
- 电气属性同 SPI1：`DS_MEDIUM;PULL_NONE;EDGE_NONE;SL_NORMAL`。

> **解读**：SPI1 用 AF7 复用，SPI2 用 AF1 复用——同样是 SPI 功能，不同 SSP 控制器对应的复用功能号不同，配引脚时不能照抄。

### 4.3 复用 SPI 设备树配置（节点 ssp2，第 17~18 页）

#### 复用 SPI 总线配置（`asr1806.dtsi`，默认 disabled）

```dts
ssp2: spi@d401c000 {
    compatible = "asr,asr-spi";
    #address-cells = <1>;
    #size-cells = <0>;
    reg = <0xd401c000 0x30>;

    dmas = <&pdma0 60 0
            &pdma0 61 0>;
    dma-names = "rx", "tx";

    asr,ssp-lpm-qos = <PM_QOS_CPUIDLE_BLOCK_AXI>;
    asr,ssp-clock-rate = <13000000>;
    asr,ssp-id = <3>;
    interrupts = <1>;
    asr,ssp-enhancement;
    /* asr,ssp-disable-dma; */
    /* asr,ssp-hold-frame-low; */
    /* asr,spi-master-rxto = <3000>; */
    /* asr,spi-slave-rxto = <262144>; */
    /* asr,spi-pio-interval = <5>; */
    clocks = <&soc_clocks ASR1803_CLK_SSP2>;
    status = "disabled";
};
```

**字段解读（复用 SPI/ssp2）**：
- `reg = <0xd401c000 0x30>` — 基址 0xd401c000。
- `dmas = <&pdma0 60 0 &pdma0 61 0>` — RX=60、TX=61。
- `asr,ssp-id = <3>` — 控制器 ID **3**。
- `interrupts = <1>` — 中断号 1。
- 注意：与 SPI1/SPI2 不同，这里 `asr,ssp-disable-dma` 是**被注释掉的**（默认不禁 DMA）。
- `clocks = ... ASR1803_CLK_SSP2`，`status = "disabled"`（默认关闭）。

#### 复用 SPI 开关配置（`ql-1806e-common.dtsi`，默认关闭）

```dts
ssp2: spi@d401c000 {
    status = "disabled";
    asr,ssp-id = <3>;
    /* dmas = <&pdma0 60 0
            &pdma0 61 0>;
       dma-names = "rx", "tx"; */
    pinctrl-names = "default";
    pinctrl-0 = <&ssp2_pmx_func>;
    asr,spi-inc-mode;
};
```

> **把普通 GPIO 复用为 SPI 外设功能**：将 `status = "disabled"` 改为 `status = "okay"` 以打开 SPI 总线。
> （这与 SPI1/SPI2 相反——SPI1/SPI2 默认 okay 需要时关掉；复用 SPI 默认 disabled 需要时打开。）

> **备注**：复用 SPI 的引脚 pinctrl（`ssp2_pmx_func`）默认配置已在 SPI 引脚设备树配置文件中设置完成，用户无需重新设置。

---

## 5. SPI 设备驱动（第 19~22 页，部分续下一段）

模块提供两种 SPI 设备驱动：**标准 4 线**与**扩展 6 线**。SPI 设备驱动以**内核模块（.ko）方式编译**，编译后的设备驱动文件存放于 **`/lib/modules/5.4.195/`** 目录下。本章以 **SPI1** 为例。

### 5.1 标准 4 线 SPI 设备驱动（spidev.c，第 19 页起）

- 驱动文件 **`spidev.c`** 位于 `ql-ol-kernel/drivers/spi/` 目录下。
- 编译后的内核模块存放于 rootfs 的 **`/lib/modules/5.4.195/`** 目录下。
- 执行 **`insmod`** 命令即可加载驱动。
- **直接通过 insmod 命令传入参数，比设备树传参方式更为灵活。**

#### 表 6：内核模块加载时支持的参数（标准 4 线 SPI 设备驱动）

| 参数 | 描述 |
|---|---|
| **busnum** | SPI 控制器编号，由设备树节点的 `asr,ssp-id` 值决定（文档用红框圈出设备树中 `asr,ssp-id = <1>;` 一行说明）。**此参数必须传入，否则 SPI 设备驱动会注册失败。** |

> **busnum 取值要点（结合第 3 章映射）**：
> - 想用 **SPI1（ssp1）** → `asr,ssp-id = <2>` → `busnum=2`
> - 想用 **SPI2（ssp0）** → `asr,ssp-id = <1>` → `busnum=1`
> - 想用 **复用 SPI（ssp2）** → `asr,ssp-id = <3>` → `busnum=3`
> 文档此处示例图框出的是 ssp0（SPI2），其 `asr,ssp-id = <1>`。该图还展示了从机相关的注释项：`asr,ssp-slave-mode`、`asr,slave-rxtiner-to-ms = <0>`、`asr,spi-master-rxto = <8000>`、`asr,spi-slave-rxto = <262144>`、`asr,spi-pio-interval = <5>` 等（默认均注释）。

#### 表 6（续）：标准 4 线驱动 insmod 全部参数（第 19~20 页）

| 参数 | 描述 | 默认值/取值 | 必填 |
|---|---|---|---|
| **busnum** | SPI 控制器编号，由设备树 `asr,ssp-id` 决定 | SPI1=2 / SPI2=1 / 复用=3 | **必填**，否则注册失败 |
| **chipselect** | 片选 | 取值支持 **0、1、2、3** | **必填**，否则注册失败 |
| **spimode** | SPI 工作模式 | 默认 **SPI_MODE_3**；可选 SPI_MODE_0/1/2/3 | 可选 |
| **maxspeed** | 最大时钟频率（Hz） | 默认 **13000000**；支持值见下 | 可选 |
| **bufsiz** | 缓存大小（字节） | 默认 **4096** | 可选 |

**spimode 取值含义**：4 种工作模式由「相位 CPHA(0x01)」与「极性 CPOL(0x02)」按位或组成，用户在 insmod 时可改。
- **CPOL（时钟极性）**：定义 SPI 总线空闲时 SCLK 电平。`1`=SCLK 高电平空闲；`0`=SCLK 低电平空闲。
- **CPHA（时钟相位）**：定义数据采样在 SCLK 的第几个边沿。`0`=第一个边沿开始采样；`1`=第二个边沿开始采样。
- 故 SPI_MODE_0=(CPOL0,CPHA0)、MODE_1=(0,1)、MODE_2=(1,0)、MODE_3=(1,1)。

**maxspeed 支持的离散值**（Hz）：`812500`、`1000000`、`1625000`、`3250000`、`6500000`、`13000000`、`26000000`。
> 注意：**实际支持的最大值由 SPI 控制器配置决定，可能与理论最大值不同**。即虽然枚举里有 26 MHz，但能否跑到取决于控制器实际配置。

**加载示例命令**：
```sh
insmod /lib/modules/5.4.195/spidev.ko busnum=1 chipselect=0 spimode=0 maxspeed=6500000
```
加载成功后用 **`lsmod`** 可看到 `spidev` 模块条目（size 16384，used 0）。

> **解读**：注意此处文档正文写默认 spimode 为 SPI_MODE_3，但示例命令传 `spimode=0`。busnum=1 对应 SPI2(ssp0)。这两个「必填」参数（busnum、chipselect）缺一个驱动 probe 就失败。

### 5.2 扩展 6 线 SPI 设备驱动（quec_spi_chn.c，第 20~22 页）

- 驱动文件 **`quec_spi_chn.c`** 位于 `ql-ol-kernel/drivers/spi/` 目录下。
- 编译后的内核模块同样存放于 rootfs 的 **`/lib/modules/5.4.195/`**。
- 同样用 **`insmod`** 加载，传参比设备树更灵活。

#### 表 7：内核模块加载时支持的参数（扩展 6 线 SPI 设备驱动）

| 参数 | 描述 | 默认值/取值 | 必填 |
|---|---|---|---|
| **busnum** | SPI 控制器编号，由设备树 `asr,ssp-id` 决定（文档红框圈 ssp0 的 `asr,ssp-id=<1>`） | 同 4 线 | **必填**，否则注册失败 |
| **chipselect** | 片选 | 取值 **0、1、2、3** | **必填**，否则注册失败 |
| **spi_mode** | SPI 工作模式（注意 6 线驱动参数名是 `spi_mode`，4 线是 `spimode`） | 默认 **SPI_MODE_0**（注意：与 4 线默认 MODE_3 不同！）；可选 MODE_0/1/2/3 | 可选 |
| **speed_hz** | 最大时钟频率 Hz（6 线驱动参数名是 `speed_hz`，4 线是 `maxspeed`） | 默认 **13000000**；支持值同表 6（812500~26000000 那一组） | 可选 |
| **frame_size** | 缓存大小（字节）（6 线驱动参数名是 `frame_size`，4 线是 `bufsiz`） | 默认 **512**（注意：与 4 线默认 4096 不同！） | 可选 |
| **gpiomodemready** | **SPI_MRDY 引脚**（模块就绪，模块输出） | 默认 **120**，对应模块引脚 GPIO_120 | 可选 |
| **gpiomcuready** | **SPI_SRDY 引脚**（从机/MCU 就绪，输入模块） | 默认 **122**，对应模块引脚 GPIO_122 | 可选 |

> **CPOL/CPHA 说明同 4 线**：spi_mode 由相位 CPHA(0x01) 与极性 CPOL(0x02) 按位或组成。

> **4 线 vs 6 线参数名对照表（极易混淆，移植脚本必查）**：
> | 含义 | 4 线（spidev） | 6 线（quec_spi_chn） |
> |---|---|---|
> | 工作模式 | `spimode`（默认 MODE_3） | `spi_mode`（默认 MODE_0） |
> | 最大频率 | `maxspeed`（默认 13M） | `speed_hz`（默认 13M） |
> | 缓存 | `bufsiz`（默认 4096） | `frame_size`（默认 512） |
> | MRDY 引脚 | 无 | `gpiomodemready`（默认 120） |
> | SRDY 引脚 | 无 | `gpiomcuready`（默认 122） |
>
> 注意握手引脚的「默认值」此处是 120/122，而第 3 章表 5 里 SRDY/MRDY 默认是 GPIO3(62)/GPIO4(144)——两处默认值不一致，是文档表 5（硬件引脚说明）与表 7（驱动参数默认）口径不同，实际以 insmod 实参为准。

**加载示例命令**：
```sh
insmod /lib/modules/5.4.195/quec_spi_chn.ko busnum=1 chipselect=0 spi_mode=0 speed_hz=6500000 gpiomcuready=120 gpiomodemready=122
```
加载成功后用 **`lsmod`** 可看到 `quec_spi_chn` 模块条目。

> **备注（重要限制）**：不使用 SDK 提供的 SPI 设备驱动时，用户需从外设设备供应商处获取设备驱动和配置手册，并据此自行配置。

---

## 6. 编译示例及功能验证（第 23~31 页）

模块 SDK 包中含 SPI 功能的编译示例 `main.c`，路径 `sample/spi/`。示例演示「初始化 SPI 设备 → 读取数据」。

### 6.1 编译步骤

#### 6.1.1 标准 4 线 SPI 编译步骤（第 23 页）

**步骤一**：初始化 SPI 设备并进行数据传输。`sample/spi/main.c` 示例以工作模式 **SPI_MODE_0、8 位字长、6.5 MHz 最大时钟频率**初始化 SPI 设备，向设备写入 1024 字节，同时读取 1024 字节。核心示例代码：
```c
int main(int argc, char *argv[])
{
    int fd;
    int i;
    uint8_t writebuf[1024];
    uint8_t readbuf[1024];

    fd = ql_spi_init(device, SPIMODE0, 8, S_6_5M);   /* 模式0,8位字长,6.5MHz */

    for (i = 0; i < 1024; i++)
        writebuf[i] = i % 256;

    ql_spi_write_read(fd, writebuf, readbuf, 1024);   /* 全双工读写 1024 字节 */

    for (i = 0; i < 1024; i++) {
        if (!(i % 32))
            puts("");
        printf("%.2X ", readbuf[i]);
    }
    puts("");

    ql_spi_deinit(fd);
    return 0;
}
```
**涉及的 SDK API（应用层）**：
- `int ql_spi_init(const char *device, int mode, int bits, int speed)` — 打开并初始化 SPI 设备，返回 fd。参数：device 设备节点（如 `/dev/spidev1.0`）、mode 工作模式（SPIMODE0~3）、bits 字长（8）、speed 速率宏（`S_6_5M`=6.5MHz）。
- `int ql_spi_write_read(int fd, uint8_t *txbuf, uint8_t *rxbuf, int len)` — SPI 全双工读写 len 字节。
- `void ql_spi_deinit(int fd)` — 关闭释放 SPI 设备。

**步骤二**：进入 `sample/spi/` 目录，执行 **`make`** 生成可执行文件 **`spi_test`**。
- 编译工具链：`/opt/ql_crosstools/ql-ag35-1806e-gcc-8.4.0-glibc-v1-toolchain/bin/arm-openwrt-linux-gcc`
- 编译选项：`-march=armv7-a -marm -mfpu=neon -mfloat-abi=hard -Wall -Wundef`
- 链接库：`-lql_sdk -lpthread -lql_sys_log -lql_lib_ipc -lql_lib_utils`

#### 6.1.2 扩展 6 线 SPI 编译步骤（第 24 页）

进入 `sample/spi_six_line/` 目录，执行 **`make`** 生成可执行文件 **`example_six_line_spi`**。编译工具链/选项/链接库与 4 线相同（同样 `-lql_sdk -lpthread -lql_sys_log -lql_lib_ipc -lql_lib_utils`）。

### 6.2 功能验证

#### 6.2.1 SPI1 主模式功能验证（第 24~25 页）

**步骤一**：进入 `sample/spi/`，修改 `main.c`，把设备宏改为 SPI1 对应节点，执行 `make` 生成 `spi_test`：
```c
#include <linux/types.h>
#include <linux/spi/spidev.h>

#define device   "/dev/spidev2.0"      /* SPI1 对应 spidev2.0 */

typedef enum {
    SPIMODE0 = SPI_MODE_0,
    SPIMODE1 = SPI_MODE_1,
    SPIMODE2 = SPI_MODE_2,
    SPIMODE3 = SPI_MODE_3,
} SPI_MODE;
```
> **关键映射**：SPI1（busnum=2，ssp1）的设备节点是 **`/dev/spidev2.0`**；SPI2（busnum=1，ssp0）的设备节点是 **`/dev/spidev1.0`**。即 `spidev<busnum>.<chipselect>`。

**步骤二**：`adb push <spi_test 上位机路径> <模块内部路径，如 /tmp>` 上传到模块。
**步骤三**：`chmod 777 spi_test` 修改权限。
**步骤四**：在 **LTE OPEN EVB** 上，**短接 GPIO_20 和 GPIO_21**（自环回测试：MOSI 接 MISO 验证回读）。
**步骤五**：动态加载驱动：
```sh
insmod /lib/modules/5.4.195/spidev.ko busnum=2 chipselect=0 spimode=0 maxspeed=6500000
```
**步骤六**：`ls /dev/spi*` 查看 `spidev2.0` 是否生成。返回 `/dev/spidev2.0` 表示已生成。
**步骤七**：执行 `./spi_test`。若收到的数据与发出的数据一致（打印出 00 01 02 … FF 循环的递增字节序列），则 SPI 编译成功，功能验证通过。终端打印含 `spi mode:0x0`、`bits per word: 8`、`max speed: 6500000 Hz (6500 KHz)`。

#### 6.2.2 SPI2 主模式功能验证（第 25~26 页）

参考 6.1.1 编译生成 `spi_test` 后：
**步骤一**：`adb push` 上传 spi_test 到模块（如 /tmp/）。
**步骤二**：`chmod 777 spi_test`。
**步骤三**：在 LTE OPEN EVB 上**短接 SPI_MOSI_AG35（J1005）和 SPI_MISO_AG35（J1005）**（自环回）。
**步骤四**：动态加载：
```sh
insmod /lib/modules/5.4.195/spidev.ko busnum=1 chipselect=0 spimode=0 maxspeed=6500000
```
**步骤五**：`ls /dev/spi*` 查看 `spidev1.0` 是否生成。返回 `/dev/spidev1.0` 表示已生成。
**步骤六**：执行 `./spi_test`，收发一致即通过（同样打印 00~FF 递增序列、`spi mode:0x0`、`max speed 6500000 Hz`）。

> **SPI1 vs SPI2 验证差异**：SPI1 用 busnum=2→spidev2.0、短接 GPIO_20/GPIO_21；SPI2 用 busnum=1→spidev1.0、短接 J1005 的 MOSI/MISO。

#### 6.2.3 SPI 从模式功能验证（第 27~29 页）

本节配置 **SPI1 为主模式、SPI2 为从模式**，演示主从对接。

**步骤一**：修改 `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-std.dts`，用宏定义配主从（**0=主模式，1=从模式**）。现将 SPI2 设为从模式：
```c
#define CONFIG_USE_ES8311    0

/* Config spi1 or spi2 slave mode */
#define CONFIG_SPI1_SLAVE    0     /* SPI1 主模式 */
#define CONFIG_SPI2_SLAVE    1     /* SPI2 从模式 */

#define CONFIG_USE_FCS950U   0
```

**步骤二**：复制 `ql-ol-kernel/drivers/spi/spidev.c` 并重命名为 **`spidev1.c`**，修改 `spidev1.c`：
- `#define SPIDEV_MAJOR 154`（assigned 主设备号 154），`#define N_SPI_MINORS 32`（最多到 256）。
- of_device_id 匹配表：`{ .compatible = "asr,slic" }`（从设备走 `asr,slic` 匹配）。
- `spi_driver` 的 `.driver.name = "spidev1"`，`.probe = spidev_probe`，`.remove = spidev_remove`。
- `spidev_init()` 中：`register_chrdev(SPIDEV_MAJOR, "spi1", &spidev_fops)`、`class_create(THIS_MODULE, "spidev1")`、`spi_register_driver(&spidev_spi_driver)`；并新增 jayde 的「按参数测试」分支：`if (busnum != -1 && chipselect != -1)` 时构造 `spi_board_info chip = { .modalias = "spidev1", .mode = spimode, ... }`。
> 即第二份驱动 `spidev1.c` 用独立的名字（spidev1/spi1）和独立 compatible（asr,slic），以便主、从两路 spidev 驱动共存不冲突。

**步骤三**：修改 `ql-ol-kernel/drivers/spi/` 下的 `Makefile`，新增 spidev1.o 与六线驱动：
```makefile
obj-$(CONFIG_SPI_MASTER)        += spi.o
obj-$(CONFIG_SPI_MEM)           += spi-mem.o
obj-$(CONFIG_SPI_SPIDEV)        += spidev.o
obj-$(CONFIG_SPI_SPIDEV)        += spidev1.o
# 2022/05/05 jayden add, compatible with spi 6-line driver
obj-$(CONFIG_SPI_SPIDEV_6LINE)  += quec_spi_chn.o
obj-$(CONFIG_SPI_LOOPBACK_TEST) += spi-loopback-test.o
```

**步骤四**：升级镜像，push 驱动 `spidev1.ko` 到模块：
1. 在根目录执行 **`make`** 编译，用 `target/` 下生成的 **`zImage`** 和 **`root.squashfs`** 替换固件包 `update/` 目录下的同名文件，在上位机用 **SWDownloader.exe** 升级（详见文档 [3]）。编译命令含 `ARCH=arm CROSS_COMPILE=/opt/ql_crosstools/ql-ag35-1806e-gcc-8.4.0-glibc-v1-toolchain/bin/arm-openwrt-linux- ... -j4 zImage dtbs`。
2. `adb push <spidev1.ko 上位机路径> /dev/` 把 `spidev1.ko` push 到模块 `/dev/`。

**步骤五**：执行文件：
1. 进入 `sample/spi/`，`make` 生成 `spi_test`，重命名为 **`spi_test1`**（接收端，对应从模式准备接收）。
2. 进入 `sample/spi/`，把 `main.c` 的 `#define device "/dev/spidev2.0"`（SPI1）后 `make`，重命名为 **`spi_test2`**（发送端）。

**步骤六**：`adb push spi_test1、spi_test2` 至模块（如 /tmp/）。
**步骤七**：`chmod 777 spi_test1` + `chmod 777 spi_test2`。
**步骤八**：在 LTE OPEN EVB 上完成主从对接短接：
- 短接 **GPIO_23（J0201）** 和 **SPI_CLK_AG35（J1005）**
- 短接 **GPIO_22（J0201）** 和 **SPI_CS_N_AG35（J1005）**
- 短接 **GPIO_21（J0201）** 和 **SPI_MOSI_AG35（J1005）**
- 短接 **GPIO_20（J0201）** 和 **SPI_MISO_AG35（J1005）**

**步骤九**：动态加载主、从两路驱动：
```sh
insmod /lib/modules/5.4.195/spidev.ko busnum=1 chipselect=0 spimode=0 maxspeed=6500000
insmod /dev/spidev1.ko busnum=2 chipselect=0 spimode=0 maxspeed=6500000
```
**步骤十**：`ls /dev/spidev1.0`、`ls /dev/spidev2.0` 确认两节点都生成。
**步骤十一**：**先在模块上执行 `spi_test1` 准备接收数据**，然后在上位机执行 `spi_test2` 发送数据。若收发一致则通过（双终端打印同样的 00~FF 递增序列）。

> **主从验证要点**：从机要先起监听（spi_test1），主机后发（spi_test2）；从机驱动 spidev1.ko 走 busnum=2（SPI1 硬件改成从），主机 spidev.ko 走 busnum=1（SPI2 作主）。注意这里与单主模式 busnum 与 SPI 编号的对应被设备树主从宏改写，需结合 ql-1806e-std.dts 的 CONFIG_SPIx_SLAVE 一起看。

#### 6.2.4 6 线 SPI 功能验证（第 30~31 页）

扩展 6 线源码为 `ql-ol-kernel/drivers/spi/quec_spi_chn.c`。参考 6.1.2 编译生成 `example_six_line_spi` 后：

**步骤一**：`adb push <example_six_line_spi 上位机路径> <模块内部路径，如 /data/>`。
**步骤二**：`chmod 777 example_six_line_spi`。
**步骤三**：在 LTE OPEN EVB 上：
- 短接 **SPI_MOSI_AG35（J1005）和 SPI_MISO_AG35（J1005）**（数据线自环回）
- 短接 **GPIO3（J0203）和 GPIO4（J0203）**（即 SRDY/MRDY 握手线自环回）
**步骤四**：动态加载六线驱动：
```sh
insmod /lib/modules/5.4.195/quec_spi_chn.ko busnum=1 chipselect=0 spi_mode=0 speed_hz=6500000 gpiomcuready=120 gpiomodemready=122
```
**步骤五**：`ls /dev/spi*` 查看设备。六线驱动生成的节点形如 **`/dev/spi1_0_0` ~ `/dev/spi1_0_7`**（共 8 个，命名规则 `spi<busnum>_<chipselect>_<n>`）：
```
/dev/spi1_0_0  /dev/spi1_0_2  /dev/spi1_0_4  /dev/spi1_0_6
/dev/spi1_0_1  /dev/spi1_0_3  /dev/spi1_0_5  /dev/spi1_0_7
```
**步骤六**：执行 `./example_six_line_spi`。收发一致即通过，终端循环打印 `read 16 bytes` + `0 1 2 3 4 5 6 7 8 9 a b c d e f`。
> 文档还示范用 `cat /sys/kernel/debug/gpio` 查看握手 GPIO 状态：可见 `gpio-120 (SPI_SRDY) in lo`、`gpio-122 (SPI_MRDY) out lo` 等，确认 SRDY/MRDY 已被驱动接管。

> **解读（六线设备节点）**：六线驱动一次注册 8 个字符设备（minor 0~7），区别于 spidev 的单一 `spidevB.C`；应用 `example_six_line_spi` 打开其中节点收发，并依赖 SRDY/MRDY 握手。注意步骤四命令里 `speed_hz` 与 `gpiomcuready/gpiomodemready` 的实参；这里把 gpiomcuready 设 120、gpiomodemready 设 122（与表 7 默认 120/122 对应，但 cu/modem 的赋值需对照实际接线）。

---

## 7. 驱动调试（第 32 页）

本章介绍 SPI 驱动调试方法（以 SPI1 为例），用于定位运行期意外问题。三个排查步骤：

**步骤一：确认 SPI 总线是否使能**。若存在 `spi1/` 文件夹则表示总线已使能：
```sh
ls /sys/devices/platform/soc/d4009000.apb/asr-spi.0/spi_master
# 输出 spi1 表示 SPI 总线已使能
```

**步骤二：确认 SPI 引脚配置是否成功**：
```sh
cat /sys/kernel/debug/pinctrl/d401e000.pinmux-pinctrl-single/pinmux-pins
```
若当前配置的 GPIO 已注册在 SPI 下，表示引脚配置成功。示例输出可见：
```
pin 88 (PIN88): asr-spi.0 (GPIO UNCLAIMED) function ssp0_pmx_func group ssp0_pmx_func
pin 89 (PIN89): asr-spi.0 (GPIO UNCLAIMED) function ssp0_pmx_func group ssp0_pmx_func
pin 90 (PIN90): asr-spi.0 (GPIO UNCLAIMED) function ssp0_pmx_func group ssp0_pmx_func
pin 91 (PIN91): asr-spi.0 (GPIO UNCLAIMED) function ssp0_pmx_func group ssp0_pmx_func
```
即这些 pin 的 function 显示为 `ssp0_pmx_func`、owner 为 `asr-spi.0`，说明已被 SPI 控制器认领。

**步骤三：打开 SPI 调试日志**。一般通过 **kernel log** 即可查看 SPI 相关错误，根据现有报错分析定位问题。

> **解读（调试三板斧）**：① 看 `spi_master` 目录确认控制器注册；② 看 pinctrl 的 `pinmux-pins` 确认引脚复用生效（function=sspX_pmx_func 且 owner=asr-spi.X）；③ 看 kernel log 抓驱动报错。这三步对应「控制器层 → 引脚层 → 运行期」的逐层排查。

---

## 8. 附录 参考文档及术语缩写（第 33~34 页）

### 表 8：参考文档

| 编号 | 文档名称 |
|---|---|
| [1] | Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导 |
| [2] | Quectel_AG35-CET&AG35-EUT_QuecOpen_GPIO_Configuration |
| [3] | Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_固件下载指导 |

> 对应正文引用：[1]→QuecOpen 总体（引言）；[2]→SPI 复用功能/GPIO 配置（第 3 章复用 SPI）；[3]→固件升级 SWDownloader（第 6.2.3 从模式升级镜像）。

### 表 9：术语缩写

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| CPHA | Clock Phase | 时钟相位 |
| CPOL | Clock Polarity | 时钟极性 |
| CPU | Central Processing Unit | 中央处理器 |
| CS | Chip Select | 片选 |
| DI | Digital Input | 数字输入 |
| DO | Digital Output | 数字输出 |
| EVB | Evaluation Board | 评估板 |
| GPIO | General Purpose Input/Output | 通用输入/输出 |
| I/O | Input/Output | 输入/输出 |
| IoV | Internet of Vehicles | 车联网 |
| LTE | Long-Term Evolution | 长期演进 |
| SCLK | Serial Clock | 串行时钟 |
| SDK | Software Development Kit | 软件开发工具包 |
| SPI | Serial Peripheral Interface | 串行外设接口 |

---

## 附：全文要点速查（移植/调试备忘）

1. **SPI↔设备树节点↔busnum 三角映射**（全文最易错）：
   - SPI1 = 节点 `ssp1` = `asr,ssp-id=2` = busnum **2** = `/dev/spidev2.0` = pinctrl AF7 = reg 0xd401b800
   - SPI2 = 节点 `ssp0` = `asr,ssp-id=1` = busnum **1** = `/dev/spidev1.0` = pinctrl AF1 = reg 0xd401b000
   - 复用 SPI = 节点 `ssp2` = `asr,ssp-id=3` = busnum **3** = reg 0xd401c000（仅主模式，默认 disabled）
2. **两种驱动**：4 线 `spidev.ko`（Flash/LCD，模块主发）；6 线 `quec_spi_chn.ko`（与 MCU 通信，双向握手 SRDY/MRDY）。
3. **insmod 必填**：busnum + chipselect（缺则注册失败）；4 线默认 spimode=MODE_3/maxspeed=13M/bufsiz=4096，6 线默认 spi_mode=MODE_0/speed_hz=13M/frame_size=512。
4. **最大时钟 26 MHz**，但实际受控制器配置限制；离散速率档 812.5K~26M。
5. **电气**：1.8 V 电压域，未用悬空。
6. **设备树开关**：SPI1/SPI2 默认 `okay`（改 disabled 还原 GPIO）；复用 SPI 默认 `disabled`（改 okay 启用）。
7. **调试**：`ls .../spi_master` → `cat .../pinmux-pins` → kernel log。
8. **驱动文件三件套**：控制器 `spi-asr.c`、4 线 `spidev.c`、6 线 `quec_spi_chn.c`，均在 `ql-ol-kernel/drivers/spi/`，ko 装在 `/lib/modules/5.4.195/`。

<!-- GENERATION_COMPLETE -->
