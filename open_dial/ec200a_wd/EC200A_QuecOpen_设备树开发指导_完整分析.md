# EC200A-CN(TA) QuecOpen 设备树开发指导 — 完整分析

> **源文档**：Quectel_EC200A-CN(TA)_QuecOpen_设备树开发指导_V1.0.0_Preliminary_20220712.pdf
> **适用平台**：LTE Standard 模块系列 — EC200A-CN(TA) QuecOpen（基于 Linux 的嵌入式开发平台）
> **版本**：1.0.0　**日期**：2022-07-12　**状态**：临时文件（Preliminary）
> **原文页数**：15 页　**底层芯片**：ASR1803（板型 p401）
> **内容范围**：设备树概念、文件类型与关系、查找/修改/使用设备树文件、新增自定义字段及驱动解析

---

## 文档历史（修订记录）

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| - | 2022-03-04 | Aurel SUN | 文档创建 |
| 1.0.0 | 2022-07-12 | Aurel SUN / Eyelyn TANG | 临时版本 |

---

## 1 引言

EC200A-CN(TA) 模块支持 **QuecOpen®** 方案（开源、基于 Linux 的嵌入式开发平台）。本文档主要介绍在 QuecOpen® 方案下，**如何在 EC200A-CN(TA) 模块中查找、修改和使用设备树文件**，包括设备树文件内容的说明。详见参考文档 [1]。

---

## 2 设备树文件

### 2.1 设备树概念

- **设备树**是 Linux 开发中用于**描述硬件信息**的文件，包括：CPU 的数量和类别、内存基地址和大小、总线和桥、外设连接、中断控制器和中断使用情况、GPIO 控制器和 GPIO 使用情况、Clock 控制器和 Clock 使用情况。
- 设备树文件以 **`.dts`** 作为后缀，且可编译；编译产物是 **`.dtb`** 文件，该文件在 bootloader 中被读取，并传递给内核。
- **主要优势**：对于同一 SOC 的不同主板，**只需更换设备树文件**，即可实现不同主板的无差异支持，而**无需更换内核文件**。

### 2.2 设备树文件介绍

| 类型 | 全称 | 后缀 | 说明 |
|---|---|---|---|
| **DTS** | Device Tree Source | `*.dts` | ASCII 文本形式，描述硬件信息，一般放在内核 `/arch/arm/boot/dts` 目录下。**一个 DTS 文件对应一个 ARM 处理器** |
| **DTSI** | Device Tree Source Include | `*.dtsi` | 可被包含的设备树配置文件。一个 SOC 可能有多个电路板（每板一个 DTS），这些 DTS 有许多共同部分。为减少冗余，提取共同部分保存在 DTSI 中供不同 DTS 共用 |
| **DTC** | Device Tree Compiler | — | 编译工具，将 `.dts` 编译成 `.dtb` |
| **DTB** | Device Tree Blob | `*.dtb` | 二进制形式，由 DTC 编译 DTS 生成。Bootloader 引导内核时预先读取 DTB 到内存，由内核解析 |

### 2.3 设备树文件之间的关系

DTS 和 DTSI 文件为**源文件**，通过 **DTC** 工具编译为 **DTB** 文件（二进制），最后内核编译完打包时会以**追加的方式打包到 zImage** 中。

> **图 1：设备树文件之间的关系**
>
> ```
> .dts / .dtsi (source)          dtb (bin blob)            zImage
> File path: ql-ol-kernel/   ──DTC──▶  File path: ql-ol-kernel/  ──▶  ┌─ zImage ─┐
> arch/arm/boot/dts          (compiler) arch/arm/boot/dts             └─  .dtb   ─┘
> ```

---

## 3 设备树操作

> 在驱动开发阶段，需修改设备树。本章介绍如何在 EC200A-CN(TA) 模块中查找设备树相关文件以及如何新增自定义字段等。

### 3.1 DTS 文件

#### 3.1.1 查询当前使用的 DTS 文件

- EC200A-CN(TA) 中**只有一个 DTS 文件**。随着模块固件不断更新，DTS 文件可能也会相应更改。
- 由于内核中**未配置 `/sys/firmware/devicetree` 节点**，用户首先需通过 SDK 中 **Makefile** 中的内核配置来确定具体 DTS 文件。

**步骤 1**：打开 `Makefile` 文件，找到 DTB 文件为 **`asr1803-p401.dtb`**。Makefile 中编译相关片段（节选）：

```makefile
kernel:
    cd $(QL_KERNEL_DIR) ; \
    [ -f .config ] && cp ../config_quec.config .config; \
    make ARCH=$(QL_ARCH) CROSS_COMPILE=$(QL_CROSSTOOLS_PREFIX) ARCH=arm KBUILD_HAVE_NLS=no CONFIG_SHELL="/bin/bash" V='' CC="arm-openwrt-linux-musl-gnueabi-gcc" -j4
    cd $(QL_KERNEL_DIR) ; \
    make ARCH=$(QL_ARCH) CROSS_COMPILE=$(QL_CROSSTOOLS_PREFIX) HOSTCFLAGS="-O2 -Wall -Wmissing-prototypes -Wstrict-prototypes" KBUILD_HAVE_NLS=no CONFIG_SHELL="/bin/bash" V='' CC="arm-openwrt-linux-musl-gnueabi-gcc" zImage dtbs
    cp -fuR $(QL_KERNEL_DIR)/arch/arm/boot/zImage $(QL_TARGET_DIR)/zImage
    cat $(QL_KERNEL_DIR)/arch/arm/boot/dts/asr1803-p401.dtb >> $(QL_TARGET_DIR)/zImage
    chmod 644 $(QL_TARGET_DIR)/zImage
```

**步骤 2**：DTB 为编译后的文件，则可确定 DTS 文件为 **`ql-ol-kernel/arch/arm/boot/dts/asr1803-p401.dts`**。

#### 3.1.2 DTS 文件说明

DTS 文件包含一些与模块相关的属性。QuecOpen SDK 提供了内核文档（其中有详细介绍）。模块 DTS 文件名称为 **`asr1803-p401.dts`**，部分内容如下：

```dts
/*
 * Copyright (C) 2018 ASR Microelectronics Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "asr1803.dtsi"

/ {
    model = "ASR 1803(FALCON) Board EVB";
    compatible = "asr,1802s-evb", "asr,1802s";

    chosen {
        bootargs = "root=/dev/mtdblock5 rootfstype=squashfs init=/etc/preinit noinitrd console=ttyS0,115200 mem=128M";
    };

    memory {
        reg = <0x04000000 0x08000000>;
    };

    soc {
        axi@d4200000 {  /* AXI */
            usbphy: usbphy@d4207000 {
                status = "okay";
            };
            udc: udc@d4208000 {
                enable-vbuson-int = <0x1>;
                status = "okay";
            };
            mhci: ehci@d4208100 {
                status = "okay";
            };
            otg: otg@d4208100 {
                status = "okay";
                pinctrl-names = "default";
                pinctrl-0 = <&otg_vbus>;
                otg,vbus-gpio-vbus;
                gpio-num = <122>;
            ...
```

上图可以看到四个字段信息：**model、compatible、chosen、memory**：

| 节点/属性 | 含义 |
|---|---|
| `/` | 根节点 |
| `model` 属性 | 模块的 ID，类型为字符串，描述了模块或芯片型号 |
| `compatible` 属性 | 用来查找节点的方式之一，**一般一个驱动对应一个 compatible 属性** |
| `chosen` 属性 | 启动参数，作为 **cmdline** 信息 |
| `memory` 属性 | 内存信息 |

> ⚠️ **备注**：上文参数仅作查看属性的示例，**用户不可修改，否则可能会出现无法开机等问题**。

### 3.2 DTSI 文件

#### 3.2.1 查询当前使用的 DTSI 文件

随着模块固件更新，DTSI 文件也会相应更改。实际开发中 DTSI 文件较多，DTS 文件可以包含 DTSI 文件，且 DTSI 文件也可以包含另一个 DTSI 文件。

**步骤 1**：根据第 3.1.1 章找到当前使用的 DTS 文件后，在 `ql-ol-kernel/arch/arm/boot/dts` 路径下执行 `la | grep asr1803-p401`，可找到多个文件：

```sh
$ la | grep asr1803-p401
asr1803-p401.dtb
.asr1803-p401.dtb.cmd
.asr1803-p401.dtb.d.dtc.tmp
.asr1803-p401.dtb.d.pre.tmp
.asr1803-p401.dtb.dts.tmp
asr1803-p401.dts
```

**步骤 2**：可查看 `.asr1803-p401.dtb.d.pre.tmp` 找到**包含的 DTSI 文件**：

```sh
$ cat .asr1803-p401.dtb.d.pre.tmp
asr1803-p401.o: arch/arm/boot/dts/asr1803-p401.dts \
 arch/arm/boot/dts/asr1803.dtsi arch/arm/boot/dts/asr18xx-pinfunc.h \
 scripts/dtc/include-prefixes/dt-bindings/power/asr-pm.h \
 scripts/dtc/include-prefixes/dt-bindings/clock/asr,asr1803.h \
 scripts/dtc/include-prefixes/dt-bindings/clock/timer-mmp.h \
 scripts/dtc/include-prefixes/dt-bindings/mmc/asr_sdhci.h \
 scripts/dtc/include-prefixes/dt-bindings/phy/phy.h \
 include/generated/autoconf.h arch/arm/boot/dts/asr_pm802.dtsi \
 arch/arm/boot/dts/88pm805.dtsi arch/arm/boot/dts/asr_pm803.dtsi \
 arch/arm/boot/dts/asr1803_ab_flash_layout.dtsi \
 arch/arm/boot/dts/ql-asr1803-overlay.dtsi \
 arch/arm/boot/dts/ql-project-overlay.dtsi \
 arch/arm/boot/dts/ql-customer-overlay.dtsi
```

> **备注**：为方便用户开发，移远通信另外添加了文件 **`ql-asr1803-overlay.dtsi`**，使用方法参考第 3.3 章。

#### 3.2.2 DTSI 文件说明

DTSI 文件很多，涉及的属性也很多，此处不一一介绍。用户可在 **`ql-ol-kernel/Documentation/devicetree/bindings/`** 路径下，使用 `grep` 搜索相关的字段。

### 3.3 `ql-asr1803-overlay.dtsi` 文件使用

- `ql-asr1803-overlay.dtsi` 文件为移远通信为方便用户开发而另外添加的 DTSI 文件。
- **该文件优先级最高，在 DTS 文件最后加载。**
- **推荐用户在开发时，保持 `asr1803-p401.dts` 原始配置不变，通过在 `ql-asr1803-overlay.dtsi` 文件中进行对应修改来达到更改配置的目的。** 相关属性配置参考第 3.1.2 章。

**示例（修改 usim1 的 SIM 检测引脚）：**

a) 在 `ql-asr1803-overlay.dtsi` 中可以看到 `usim1` 的 `edge-detect-gpio` PIN 值为 **19**（原始节点定义，如在 `asr1803.dtsi` 等）：

```dts
usim1: usim {
    compatible = "asr,usim1";
    pinctrl-names = "default", "sleep";
    pinctrl-0 = <&usim1_pmx_func>;
    pinctrl-1 = <&usim1_pmx_func_sleep>;
    edge-detect-gpio = <19>; /* GPIO019: SIM detect pin */
    status = "okay";
};
sound {
    compatible = "ASRMICRO,asrmicro-snd-card";
    ssp-controllers = <&ssp_dai1>;
};
```

b) 在 `ql-asr1803-overlay.dtsi` 中对 `usim1` 的 `edge-detect-gpio` 修改 PIN 值为 **119**（通过引用 `&usim1` 覆盖原属性）：

```dts
&usim1 {
    edge-detect-gpio = <119>;
};
```

**关闭某功能的示例（将 `status` 由 `"ok"` 改为 `"disabled"` 关闭 LPM 功能）：**

```dts
quec,gpio_lpm{                               //larry.zhang20210827:wakeup driver.
    compatible = "quec,ql_lpm";
    pinctrl-names = "default","sleep";
    pinctrl-0 = <&wakeup_in_pin &sleep_sys_ind>;
    pinctrl-1 = <&wakeup_in_pin_sleep>;
    gpios = <&gpio 117 0>,
            <&gpio 120 0>;
    ql,gpio_names = "wakeup_in","sleep_sys_ind";
    ql,sleep-sys-ind-enable;
    ql,sleep-sys-ind-state = <0>;
    status = "ok";                           // change status = "ok"; to status = "disabled";
};
```

> 即：将 `status = "ok";` 改为 `status = "disabled";` 即可关闭 LPM 功能。

### 3.4 新增自定义字段

> 本节以**配置 GPIO** 为例，介绍如何新增自定义字段，并在驱动中解析。可参考 `ql-sysroots/usr/include/ql-sdk-cmpt/ql_gpio.h`。

**步骤 a**：在 `ql-ol-kernel/arch/arm/boot/dts/ql-asr1803-overlay.dtsi` 文件中，配置 `temp_gpio_default`：

```dts
temp_gpio_default {                 //自定义一个 temp_gpio_default 节点
    pinctrl-single,pins = <
        GPIO25 AF0 /* gpio25 */
    >;
    DS_MEDIUM;PULL_UP;EDGE_BOTH;LPM_NONE;
}
```

**部分 GPIO 属性如下：**

| 属性类别 | 可选值 |
|---|---|
| 功能选择 | `AF0`, `AF1` … `AF7` |
| 驱动能力 | `DS_SLOW0`, `DS_SLOW1`, `DS_MEDIUM`, `DS_FAST` |
| 上拉下拉 | `PULL_NONE`, `PULL_UP`, `PULL_DOWN`, `PULL_BOTH`, `PULL_FLOAT` |
| 边缘检测 | `EDGE_NONE`, `EDGE_RISE`, `EDGE_FALL`, `EDGE_BOTH` |
| 低功耗状态 | `LPM_NONE`, `LPM_DRIVE_LOW`, `LPM_DRIVE_HIGH`, `LPM_FLOAT` |

**步骤 b**：在文件 `ql-ol-kernel/arch/arm/boot/dts/qcom/ql-asr1803-overlay.dtsi` 中进行声明用户节点操作：

```dts
xxx{
    compatible = "xxx";                /* 匹配用户 driver */
    pinctrl-names = "temp_default";    /* 定义 pinctrl name, 驱动中使用 pinctrl_lookup_state() 接口解析 */
    pinctrl-0 = <&temp_gpio_default>;  /* 选中上面定义的 gpio 配置 */
    status = "ok";                     /* 使能此设备节点 */
}
```

**步骤 c**：设备树配置后，写驱动来解析并使能以上配置，相关内核 API 操作如下：

**1) 获取一个 pinctrl 句柄**（参数 `dev` 是包含这个引脚的 device 结构体）：

```c
/**
 * struct devm_pinctrl_get() - Resource managed pinctrl_get()
 * @dev: the device to obtain the handle for
 *
 * If there is a need to explicitly destroy the returned struct pinctrl,
 * devm_pinctrl_put() should be used, rather than plain pinctrl_put().
 */
struct pinctrl *devm_pinctrl_get(struct device *dev)
```

**2) 获取这个 pin 对应的引脚状态**：

```c
/**
 * pinctrl_lookup_state() - retrieves a state handle from a pinctrl handle
 * @p: the pinctrl handle to retrieve the state from
 * @name: the state name to retrieve
 */
struct pinctrl_state *pinctrl_lookup_state(struct pinctrl *p, const char *name)
```

**3) 设置此引脚为指定状态**（设置为 on 即表示使能以上配置）：

```c
/**
 * pinctrl_select_state() - select/activate/program a pinctrl state to HW
 * @p: the pinctrl handle for the device that requests configuration
 * @state: the state handle to select/activate/program
 */
int pinctrl_select_state(struct pinctrl *p, struct pinctrl_state *state)
```

> 解析流程：`devm_pinctrl_get()` 拿句柄 → `pinctrl_lookup_state()` 按 `pinctrl-names` 取状态 → `pinctrl_select_state()` 应用到硬件。

---

## 4 附录 参考文档与术语缩写

### 表 1：参考文档

| 序号 | 文档名称 |
|---|---|
| [1] | Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导 |

### 表 2：术语缩写

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| API | Application Programming Interface | 应用程序编程接口 |
| CPU | Central Processing Unit | 中央处理器 |
| DTB | Device Tree Blob | 设备树二进制文件 |
| DTC | Device Tree Compiler | 设备树编译器 |
| DTS | Device Tree Source | 设备树源文件 |
| DTSI | Device Tree Source Include | 包含设备树源文件 |
| GPIO | General-Purpose Input/Output | 通用型输入/输出 |
| ID | Identifier | 标识符 |
| IoT | Internet of Things | 物联网 |
| LPM | Low Power Mode | 低功耗模式 |
| SDK | Software Development Kit | 软件开发工具包 |
| SOC | System on Chip | 芯片系统 |

---

## 关键要点速查（实践提炼）

- 设备树链：`.dts/.dtsi`(源) ──DTC──▶ `.dtb`(二进制) ──追加打包──▶ `zImage`，bootloader 读 dtb 传给内核。
- 模块只有一个 DTS：**`asr1803-p401.dts`**（板型 p401）。`/sys/firmware/devicetree` 未启用，靠 Makefile 里的 `*.dtb` 名反推 DTS。
- 查 DTSI 包含关系：看 `.asr1803-p401.dtb.d.pre.tmp`。移远定制 overlay：`ql-asr1803-overlay.dtsi`（优先级最高，最后加载）。
- ⚠️ **核心约定**：**不要直接改 `asr1803-p401.dts`**（含 model/compatible/chosen/memory，改了可能无法开机），所有自定义改动都放到 `ql-asr1803-overlay.dtsi`，用 `&node {...}` 覆盖属性。
- 关功能：把目标节点 `status` 从 `"ok"` 改 `"disabled"`（如 LPM `quec,gpio_lpm`）。改 PIN：`&usim1 { edge-detect-gpio = <119>; }`。
- 新增 GPIO 字段三步：overlay 里定义 `temp_gpio_default`（pin+DS/PULL/EDGE/LPM）→ 声明设备节点（compatible/pinctrl-names/pinctrl-0/status）→ 驱动用 `devm_pinctrl_get` + `pinctrl_lookup_state` + `pinctrl_select_state` 解析使能。
- GPIO 属性四类：功能 `AF0~AF7`、驱动能力 `DS_*`、上下拉 `PULL_*`、边缘 `EDGE_*`、低功耗 `LPM_*`。
