# Quectel AG35-CET&AG35-EUT QuecOpen(SDK) 设备树开发指导

## 文档基本信息
- **文档标题**：AG35-CET&AG35-EUT QuecOpen(SDK) 设备树开发指导
- **厂商**：上海移远通信技术股份有限公司（Quectel Wireless Solutions Co., Ltd.）
- **适用模块**：LTE Standard 模块系列，AG35-CET 与 AG35-EUT
- **方案**：QuecOpen（基于 Linux 的嵌入式开发平台，简化 IoV/车联网应用软件设计与开发）
- **版本号**：1.0（受控版本）
- **日期**：2025-04-22
- **状态**：受控文件
- **总页数**：17 页（正文 16 页编号）
- **联系方式**：电话 +86 21 5108 6236；邮箱 info@quectel.com；技术支持 support@quectel.com
- **底层芯片**：ASR Microelectronics ASR1806（DTS model 标识为 "AG35CET (ASR 1806) Module"，compatible = "asr,1803-evb","asr,1803"）

### 修订记录
- `-`，2022-11-23，Jensen ZHANG，文档创建
- `1.0.0`，2023-11-23，Jensen ZHANG，临时版本
- `1.0.1`，2024-12-06，Gabriel LI，临时版本：① 基于 QuecOpen 方案统一命名，更新文档名称；② 新增适用模块 AG35-EUT
- `1.0`，2025-04-22，Gabriel LI，受控版本

### 文档目录结构
1. 引言
2. 设备树基本概念（2.1 概述、2.2 设备树文件、2.3 设备树文件之间的关系）
3. 设备树操作（3.1 DTS 文件、3.2 DTSI 文件、3.3 DTB 文件、3.4 新增自定义字段）
4. 附录 参考文档与术语缩写

---

## 逐章节内容摘要

### 第1章 引言
- AG35-CET 和 AG35-EUT 模块支持 QuecOpen 方案。QuecOpen 是基于 Linux 的嵌入式开发平台，可简化 IoV（车联网）应用的软件设计和开发过程，详细信息参考文档 [1]《Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导》。
- 本文档适用于 SDK 构建环境的 QuecOpen 方案，主要介绍：设备树相关基本概念、设备树文件之间的关系、如何查看与驱动相关的设备信息文件配置、如何查找相关字段解释，从而实现简易快速的开发。

### 第2章 设备树基本概念

#### 2.1 概述
- 设备树是 Linux 开发中用于描述硬件信息的文件，描述内容包括：CPU 的数量和类别、内存基地址和大小、总线和桥、外设连接、中断控制器和中断使用情况、GPIO 控制器和 GPIO 使用情况、Clock 控制器和 Clock 使用情况。
- 设备树的主要优势：对于同一 SoC 的不同主板，只需更换设备树文件即可实现不同主板的无差异支持，而无需更换内核文件。

#### 2.2 设备树文件
- **DTS（Device Tree Source）**：ASCII 文本形式的文件，后缀 `.dts`，用于描述硬件信息，一般存放在内核 `/arch/arm/boot/dts/` 目录下。通常一个 DTS 文件对应一个 ARM 处理器。详见 3.1 章。
- **DTSI（Device Tree Source Include）**：文本形式的设备树源包含文件，后缀 `.dtsi`。一个 SoC 可能有多个不同的电路板，每个电路板拥有一个 DTS 文件，这些 DTS 文件存在许多共同部分；为减少冗余代码，设备树将共同部分提取保存在 DTSI 文件中供不同 DTS 文件共用。详见 3.2 章。
- **DTB（Device Tree Blob）**：二进制形式的文件，后缀 `.dtb`。通过 DTC 编译 DTS 文件生成。Bootloader 在引导内核时会预先读取 DTB 文件到内存，进而由内核解析。
- **DTC（Device Tree Compiler）**：编译工具，用于将 `.dts` 文件编译成 `.dtb` 文件。

#### 2.3 设备树文件之间的关系
- DTS 和 DTSI 文件为源文件，通过 DTC 工具编译为 DTB 文件（二进制），最后内核编译完成后以追加的方式打包到 zImage 中。
- 文件路径：DTS/DTSI 源文件与 DTB 文件均位于 `ql-ol-kernel/arch/arm/boot/dts/`。
- 关系链（图1）：`DTS and DTSI files (Source)` → DTC compiles → `DTB file (Binary Blob)` → 追加打包进 `zImage`（zImage 内含 .dtb）。

### 第3章 设备树操作
- 在驱动开发阶段需进行设备树的修改。本章介绍如何在模块中查找设备树相关文件以及如何新增自定义字段。

#### 3.1 DTS 文件

##### 3.1.1 查询当前使用的 DTS 文件
- 模块中只有一个 DTS 文件。随着模块固件更新，DTS 文件可能也会相应更改。
- 由于内核中未配置 `/sys/firmware/devicetree` 节点，用户需通过 SDK 中 Makefile 的内核配置来确定 DTS 文件。
- **步骤1**：打开 Makefile 文件，找到 DTB 文件为 `ql-1806e-std.dtb`（Makefile 中的 `pre_build`/`DTC_FLAGS` 编译规则将 `arch/arm/boot/dts/ql-1806e-std.dtb` 追加到 zImage）。
- **步骤2**：DTB 为编译后的文件，则可确定 DTS 文件为 `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-std.dts`。

##### 3.1.2 DTS 文件说明
- DTS 文件中包含一些与模块相关的属性。
- QuecOpen SDK 在 `ql-ol-kernel/Documentation/devicetree/` 目录下提供了内核文档，详细介绍 DTS 文件相关知识。
- 模块 DTS 文件名称为 `ql-1806e-std.dts`，部分内容（截图）：
  - 头部 `// SPDX-License-Identifier: GPL-2.0`，版权 ASR Microelectronics Co., Ltd. 2023。
  - `/dts-v1/;`
  - `#include "asr1806.dtsi"`、`#include "ql-1806e-common.dtsi"`、`#include "asr_pm802.dtsi"`、`#include "ql-1806e-flash-layout.dtsi"`
  - `#define CONFIG_USE_YT8010A 1`（Motorcomm YT8010A 100Base-T1 PHY RMII）
  - `#define CONFIG_USE_EMMC 1`、`#define CONFIG_USE_EMMC_SD_ADAPTIVE 0`（1 支持 emmc 默认，0 支持 SD card）
  - codecs 配置：`CONFIG_USE_TLV320AIC3104 1`、`CONFIG_USE_NAU8810 0`、`CONFIG_USE_RT5616 0`、`CONFIG_USE_ES8311 0`、`CONFIG_USE_FCS950U 0`
  - 设备树主入口 `/ { model = "AG35CET (ASR 1806) Module"; compatible = "asr,1803-evb","asr,1803"; };`
- `ql-1806e-std.dts` 引用了 `ql-1806e-common.dtsi`，其部分内容含四个属性（图）：
  - `chosen { bootargs = "root=/dev/mtdblock5 rootfstype=squashfs init=/etc/preinit noinitrd console=ttyS0,115200 mem=128M"; };`
  - `aliases { serial2 = &uart3; serial3 = &uart4; };`
  - `firmware { optee { compatible = "linaro,optee-tz"; method = "smc"; }; };`
  - `memory { reg = <0x00000000 0x10000000>; };`
- **四个属性说明**（"/" 表示根节点）：
  - `model`：模块 ID，字符串类型，描述模块或芯片型号。
  - `compatible`：用来查找节点的方式之一，通常一个驱动对应一个 compatible 属性。
  - `chosen`：启动参数，作为命令行信息。
  - `memory`：内存信息。
- **备注（重要）**：以上参数的介绍仅用于说明如何查看属性含义，文件内容不可修改，否则可能会出现无法开机等问题。

#### 3.2 DTSI 文件

##### 3.2.1 查询当前使用的 DTSI 文件
- 随着模块固件更新，DTSI 文件也会相应更改。实际开发中，DTSI 文件可以被多个 DTS 文件包含，并且 DTSI 文件还可以包含其他 DTSI 文件。
- 根据 3.1.1 章找到当前使用的 DTS 文件后，在 `ql-ol-kernel/arch/arm/boot/dts/` 路径下执行 `la | grep ql-1806e-std`，可找到多个文件：
  - `ql-1806e-std.dtb`、`.ql-1806e-std.dtb.cmd`、`.ql-1806e-std.dtb.d.dtc.tmp`、`.ql-1806e-std.dtb.d.pre.tmp`、`.ql-1806e-std.dtb.dts.tmp`、`ql-1806e-std.dts`
- 可通过查看文件获取 DTSI 文件。以 `ql-1806e-std.dtb.d.pre.tmp` 为例（`cat` 输出依赖列表）：
  - 依赖含 `arch/arm/boot/dts/ql-1806e-std.dts`、`arch/arm/boot/dts/asr1806.dtsi`、`arch/arm/boot/dts/asr18xx-pinfunc.h`、`scripts/dtc/include-prefixes/dt-bindings/power/asr-pm.h`、`scripts/dtc/include-prefixes/dt-bindings/clock/asr,asr1803.h`、`scripts/dtc/include-prefixes/dt-bindings/clock/timer-mmp.h`、`scripts/dtc/include-prefixes/dt-bindings/mmc/asr_sdhci.h`、`scripts/dtc/include-prefixes/dt-bindings/phy/phy.h`、`include/generated/autoconf.h`、`arch/arm/boot/dts/ql-1806e-common.dtsi`、`arch/arm/boot/dts/asr_pm802.dtsi`、`arch/arm/boot/dts/ql-1806e-flash-layout.dtsi`

##### 3.2.2 DTSI 文件说明
- DTSI 文件较多，涉及的属性也较多。用户可在 `ql-ol-kernel/Documentation/devicetree/bindings/` 路径下使用 `grep` 搜索相关字段（查询字段含义）。

#### 3.3 DTB 文件
- 在设备树编译流程中，`ql-1806e-std.dtb` 优先级最高，在 DTS 文件层级中最后加载。
- 建议开发时保持 `asr1806.dtsi`、`ql-1806e-common.dtsi`、`asr_pm802.dtsi` 和 `ql-1806e-flash-layout.dtsi` 文件原始配置不变，通过修改 `ql-1806e-std.dtb` 文件来更改配置。相关属性配置参考 3.1.2 章。
- **示例一：修改节点对应边缘检测 GPIO 引脚**：
  - 步骤1：在 `ql-1806e-common.dtsi` 中看到 usim1 节点的 `edge-detect-gpio` 的 PIN 值为 119（原始配置，见下方示例代码）。
  - 步骤2：在 `ql-1806e-std.dtb` 中修改 usim1 节点下 `edge-detect-gpio` 的 PIN 值为 19。
- **示例二：关闭模块某项功能**：
  - 在 `ql-1806e-std.dtb` 文件中，对应功能节点下设置 `status = "disabled"` 即可关闭对应功能。
  - 示例：`quec,gpio_lpm` 节点（wakeup driver），将原 `status = "ok";` 改为 `status = "disabled";` 即关闭该功能。

#### 3.4 新增自定义字段
- 以配置 GPIO 为例，介绍如何自定义一个字段并在驱动中解析。
- **步骤1**：参考 `ql-ol-kernel/arch/arm/boot/dts/asr18xx-pinfunc.h` 配置 GPIO 引脚（该头文件用 `#define GPIOxx 0x0DC...` 列出各 GPIO 寄存器偏移地址，如 GPIO00=0x0DC、GPIO01=0x0E0、…、GPIO25=0x140、GPIO33=0x168 等，步进 0x4）。
- **步骤2**：修改 `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-std.dtb` 文件，配置自定义节点 `temp_gpio_default`（见示例代码）。
  - **部分 GPIO 属性说明**：
    - 功能选择：GPIO 可复用的功能 AF0~AF7
    - 驱动能力：DS_SLOW0；DS_SLOW1；DS_MEDIUM；DS_FAST
    - 上下拉选择：PULL_NONE；PULL_UP；PULL_DOWN；PULL_BOTH；PULL_FLOAT
    - 边缘检测：EDGE_NONE；EDGE_RISE；EDGE_FALL；EDGE_BOTH
    - 低功耗状态：LPM_NONE；LPM_DRIVE_LOW；LPM_DRIVE_HIGH；LPM_FLOAT
- **步骤3**：修改 `ql-1806e-std.dtb` 文件，定义引用该 pinctrl 的设备节点 `xxx`（见示例代码）。
- **步骤4**：设备树配置完成后，在对应的驱动中解析并使能以上配置，使用三个内核 pinctrl API（见关键 API 清单）。

### 第4章 附录 参考文档与术语缩写
- **表1 参考文档**：[1] Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导
- **表2 术语缩写**：
  - API：Application Programming Interface，应用程序编程接口
  - CPU：Central Processing Unit，中央处理器
  - DTB：Device Tree Blob，设备树二进制文件
  - DTC：Device Tree Compiler，设备树编译器
  - DTS：Device Tree Source，设备树源文件
  - DTSI：Device Tree Source Include，包含设备树源文件
  - GPIO：General-Purpose Input/Output，通用型输入/输出
  - ID：Identifier，标识符
  - IoV：Internet of Vehicles，车联网
  - LPM：Low Power Mode，低功耗模式
  - SDK：Software Development Kit，软件开发工具包
  - SoC：System on Chip，芯片系统

---

## 关键 API / 命令清单

### Shell 命令
- `la | grep ql-1806e-std`：在 `ql-ol-kernel/arch/arm/boot/dts/` 目录列出与当前 DTS 相关的所有文件（.dts/.dtb 及中间编译产物）。作用：定位当前使用的 DTS/DTSI 文件。
- `cat .ql-1806e-std.dtb.d.pre.tmp`：查看 DTB 的预处理依赖列表，从中得知该 DTS 引用了哪些 .dtsi/.h 文件。
- `grep <字段>`（在 `ql-ol-kernel/Documentation/devicetree/bindings/` 下执行）：搜索 DTSI 属性字段的含义说明。

### 内核 pinctrl API（3.4 步骤4，用于驱动中解析并使能自定义 GPIO 配置）
1. `struct pinctrl *devm_pinctrl_get(struct device *dev)`
   - 作用：获取一个 pinctrl 句柄（Resource managed pinctrl_get）。
   - 参数：`dev` 为包含此引脚的 device 结构体。
   - 返回值：返回 `struct pinctrl *` 句柄。如需显式销毁应使用 `devm_pinctrl_put()` 而非普通 `pinctrl_put()`。
2. `struct pinctrl_state *pinctrl_lookup_state(struct pinctrl *p, const char *name)`
   - 作用：从 pinctrl 句柄中检索一个状态句柄。
   - 参数：`p` 为 pinctrl 句柄；`name` 为要检索的状态名（对应 DTS 中 `pinctrl-names`，如 "temp_default"）。
   - 返回值：返回 `struct pinctrl_state *` 状态句柄。
3. `int pinctrl_select_state(struct pinctrl *p, struct pinctrl_state *state)`
   - 作用：将一个 pinctrl 状态选择/激活/编程到硬件（select/activate/program a pinctrl state to HW）。设置为 on 即表示使能上述配置。
   - 参数：`p` 为请求配置的设备的 pinctrl 句柄；`state` 为要选择/激活/编程的状态句柄。
   - 返回值：`int`（成功/失败）。

### 设备树属性 / 配置项
- `model`：模块/芯片型号（字符串）。本模块为 "AG35CET (ASR 1806) Module"。
- `compatible`：节点匹配属性，一个驱动对应一个 compatible。本模块根节点为 "asr,1803-evb","asr,1803"。
- `chosen` / `bootargs`：内核启动命令行（root、rootfstype、init、console、mem 等）。
- `memory` / `reg`：内存基地址与大小（本模块 `<0x00000000 0x10000000>`，即 256MB 区间，bootargs 中 mem=128M）。
- `aliases`：串口别名映射（serial2=&uart3、serial3=&uart4）。
- `status = "okay"/"ok"/"disabled"`：使能/关闭设备节点（关闭功能的关键属性）。
- `edge-detect-gpio = <PIN>`：节点边缘检测 GPIO 引脚号（usim1 节点 SIM detect pin，原值 119）。
- `pinctrl-names` / `pinctrl-0` / `pinctrl-1`：pinctrl 状态名及对应配置引用（如 "default"、"sleep"）。
- `pinctrl-single,pins = <...>`：在自定义节点中声明引脚及其功能/驱动能力/上下拉/边缘/低功耗组合。
- GPIO 属性枚举值：功能 AF0~AF7；驱动能力 DS_SLOW0/DS_SLOW1/DS_MEDIUM/DS_FAST；上下拉 PULL_NONE/PULL_UP/PULL_DOWN/PULL_BOTH/PULL_FLOAT；边缘 EDGE_NONE/EDGE_RISE/EDGE_FALL/EDGE_BOTH；低功耗 LPM_NONE/LPM_DRIVE_LOW/LPM_DRIVE_HIGH/LPM_FLOAT。

---

## 示例代码说明

1. **DTS 头部与主入口（ql-1806e-std.dts，3.1.2）**：
   ```dts
   /dts-v1/;
   #include "asr1806.dtsi"
   #include "ql-1806e-common.dtsi"
   #include "asr_pm802.dtsi"
   #include "ql-1806e-flash-layout.dtsi"
   #define CONFIG_USE_YT8010A 1        /* Motorcomm YT8010A 100Base-T1 PHY RMII */
   #define CONFIG_USE_EMMC 1            /* 1=emmc(默认), 0=SD card */
   #define CONFIG_USE_EMMC_SD_ADAPTIVE 0
   #define CONFIG_USE_TLV320AIC3104 1   /* 选用的音频 codec */
   / {
       model = "AG35CET (ASR 1806) Module";
       compatible = "asr,1803-evb","asr,1803";
   };
   ```
   说明：通过 `#include` 引入 SoC 公共 dtsi、板级公共 dtsi、PMIC dtsi、Flash 布局 dtsi；`#define` 宏在编译期决定是否使能某 PHY/EMMC/codec（1 启用、0 禁用）。这是"同一 SoC 不同主板靠 DTS 区分"的体现。关键注意：此文件不可随意修改，否则可能无法开机。

2. **ql-1806e-common.dtsi 的四个属性**：
   ```dts
   chosen { bootargs = "root=/dev/mtdblock5 rootfstype=squashfs init=/etc/preinit noinitrd console=ttyS0,115200 mem=128M"; };
   aliases { serial2 = &uart3; serial3 = &uart4; };
   firmware { optee { compatible = "linaro,optee-tz"; method = "smc"; }; };
   memory { reg = <0x00000000 0x10000000>; };
   ```
   说明：`bootargs` 指定根分区为 mtdblock5、squashfs 文件系统、控制台 ttyS0 115200、内存 128M；`firmware/optee` 声明 OP-TEE 安全固件用 SMC 调用；`memory` 声明内存区间。仅用于理解，不可修改。

3. **修改 usim1 边缘检测 GPIO（3.3 示例一）**：
   - 原始（ql-1806e-common.dtsi）：
     ```dts
     usim1: usim {
         compatible = "asr,usim1";
         pinctrl-names = "default", "sleep";
         pinctrl-0 = <&usim1_pmx_func>;
         pinctrl-1 = <&usim1_pmx_func_sleep>;
         edge-detect-gpio = <119>;   /* GPIO119: SIM detect pin */
         status = "okay";
     };
     ```
   - 在 ql-1806e-std.dtb 中覆盖：
     ```dts
     &usim1 {
         edge-detect-gpio = <19>;
     };
     ```
   说明：用 `&usim1` 引用已有节点，仅覆盖 `edge-detect-gpio` 一个属性，把 SIM 卡热插拔检测引脚从 GPIO119 改为 GPIO19。这体现"保持 dtsi 不变、只在最高优先级 dtb 中覆盖"的推荐做法。

4. **关闭功能节点（3.3 示例二）**：
   ```dts
   quec,gpio_lpm {                              // gabriel20230517: wakeup driver
       compatible = "quec,ql_lpm";
       pinctrl-names = "default","sleep";
       pinctrl-0 = <&wakeup_in_pin &sleep_sys_ind>;
       pinctrl-1 = <&wakeup_in_pin_sleep>;
       gpios = <&gpio 118 0>, <&gpio 38 0>;
       ql,gpio-names = "wakeup_in","sleep_sys_ind";
       ql,sleep-sys-ind-enable;
       ql,sleep-sys-ind-state = <0>;
       status = "ok";                           // 改为 status = "disabled"; 即关闭
   };
   ```
   说明：这是 wakeup（唤醒）驱动节点，包含唤醒输入引脚与睡眠系统指示引脚配置。将 `status = "ok";` 改为 `status = "disabled";` 即可禁用该功能。`status` 是通用的节点使能开关。

5. **新增自定义 GPIO pinctrl 节点（3.4 步骤2）**：
   ```dts
   temp_gpio_default {                  // 自定义一个 temp_gpio_default 节点
       pinctrl-single,pins = <
           GPIO25 AF0 /* gpio25 */
       >;
       DS_MEDIUM;PULL_UP;EDGE_BOTH;LPM_NONE;
   };
   ```
   说明：定义引脚 GPIO25 复用为 AF0（普通 GPIO 功能），驱动能力中等（DS_MEDIUM）、内部上拉（PULL_UP）、双边沿检测（EDGE_BOTH）、低功耗时无特殊状态（LPM_NONE）。GPIO25 的寄存器偏移来自 asr18xx-pinfunc.h（0x140）。

6. **引用自定义 pinctrl 的设备节点（3.4 步骤3）**：
   ```dts
   xxx {
       compatible = "xxx";                  /* 匹配用户 driver */
       pinctrl-names = "temp_default";      /* 定义 pinctrl name，驱动中用 pinctrl_lookup_state() 接口解析 */
       pinctrl-0 = <&temp_gpio_default>;    /* 选中上面定义的 gpio 配置 */
       status = "ok";                       /* 使能此设备节点 */
   };
   ```
   说明：`compatible="xxx"` 用于和用户驱动匹配；`pinctrl-names="temp_default"` 是状态名，驱动里用 `pinctrl_lookup_state(p,"temp_default")` 查找；`pinctrl-0` 绑定步骤2 的引脚配置。驱动 probe 时依次调用 `devm_pinctrl_get → pinctrl_lookup_state → pinctrl_select_state` 即可把该 GPIO 配置写入硬件生效。

<!-- GENERATION_COMPLETE: 2026-06-25_03:45 -->
