# AG35-CET QuecOpen(SDK) I2C 开发指导

> **文档版本**：V1.0.0  
> **日期**：2023-11-14  
> **状态**：临时文件  
> **适用平台**：AG35-CET（ASR1806e，LTE Standard 模块系列）  
> ⚠️ **平台说明**：本文档面向 AG35-CET/EUT，本项目实际运行于 EC200A/EG25，硬件接口细节（引脚号、DTS 路径）请以实际平台手册为准，功能原理和调试方法具有参考价值。

---

## 目录

1. [引言](#1-引言)
2. [I2C 引脚分配](#2-i2c-引脚分配)
3. [I2C 设备树配置](#3-i2c-设备树配置)
4. [I2C 编译示例及功能验证](#4-i2c-编译示例及功能验证)
5. [I2C 驱动调试方法](#5-i2c-驱动调试方法)
6. [附录 参考文档及术语缩写](#6-附录-参考文档及术语缩写)

---

## 1 引言

移远通信 AG35-CET 模块支持 QuecOpen® 方案。QuecOpen® 是基于 Linux 的嵌入式开发平台，可简化 IoV 应用的软件设计和开发过程。

本文档适用于 SDK 构建环境的 QuecOpen® 方案，主要介绍 AG35-CET 模块的 I2C 功能开发，包含 I2C 接口引脚定义、设备树配置、功能验证和驱动调试的方法等。

---

## 2 I2C 引脚分配

模块默认支持两路 I2C 接口，引脚信息如下表所示：

### 表 1：I2C1（设备树节点：twsi0）引脚定义

| 引脚名 | 引脚号 | I/O | 复用功能 1（默认） | 复用功能 2 | 备注 |
|--------|--------|-----|-------------------|-----------|------|
| I2C1_SCL | 43 | OD | I2C_SCL | GPIO_49 | 需要外部 1.8V 上拉；不用则悬空 |
| I2C1_SDA | 42 | OD | I2C_SDA | GPIO_50 | 需要外部 1.8V 上拉；不用则悬空 |

### 表 2：I2C2（设备树节点：twsi3）引脚定义

| 引脚名 | 引脚号 | I/O | 复用功能 1（默认） | 复用功能 2 | 备注 |
|--------|--------|-----|-------------------|-----------|------|
| I2C2_SCL | 74 | OD | I2C_SCL | GPIO_41 | 需要外部 1.8V 上拉；不用则悬空 |
| I2C2_SDA | 73 | OD | I2C_SDA | GPIO_42 | 需要外部 1.8V 上拉；不用则悬空 |

**备注：**
1. 模块默认支持两路 I2C 接口，且模块在与 I2C 接口有关的应用中只能作为主设备。有关模块 GPIO 引脚配置的详细信息，请参考 Quectel_AG35-CET_QuecOpen_GPIO 配置文档。
2. I2C 接口支持标准模式（100 kHz）和快速模式（400 kHz）；默认为快速模式。

---

## 3 I2C 设备树配置

本章介绍 I2C 设备树配置信息。I2C 接口在软件设备树文件（`.dtsi` 文件）中对应的字段如下表所示：

### 表 3：I2C 接口与设备树文件中字段的对应关系

| I2C 接口 | 设备树文件中的对应字段 |
|---------|----------------------|
| I2C1 | twsi0: i2c@d4011000 |
| I2C2 | twsi3: i2c@d4010c00 |

### 3.1 I2C1 设备树配置

#### 3.1.1 I2C1 总线驱动设备树配置

I2C1 总线驱动配置位于 SDK 包中文件 `ql-ol-kernel/arch/arm/boot/dts/asr1806.dtsi`，其默认配置如下所示：

```dts
twsi0: i2c@d4011000 {
        compatible = "mrvl,mmp-twsi";
        #address-cells = <1>;
        #size-cells = <0>;
        reg = <0xd4011000 0x60>;
        interrupts = <7>;
        lpm-qos = <PM_QOS_CPUIDLE_BLOCK_DDR>;
        mrvl,i2c-fast-mode;
        /*
         *ilcr: fast mode b17~9=0x23, 390k
         *      standard mode b8~0=0x9f, 97k
         *iwcr: b5~0=b01010 recommended value from spec
         */
        marvell,i2c-ilcr = <0x82c469f>;
        marvell,i2c-iwcr = <0x142a>;
        pinctrl-names = "default","gpio";
        pinctrl-0 = <&twsi0_pmx_func>;
        pinctrl-1 = <&twsi0_pmx_gpio>;
        i2c-gpio = <&gpio 49 0 &gpio 50 0>;
        clocks = <&soc_clocks ASR1803_CLK_TWSI0>;
        clock-names = "twsi0_clk";
        status = "disabled";
};
```

**备注：** 此部分配置用户无需关注，相关配置在默认情况下都已配置完成。

I2C1 总线开关配置见 SDK 包中文件 `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-std.dts`；I2C1 总线默认使能，通过添加 `status = "disabled"` 即可关闭 I2C1 总线：

```dts
&twsi0{
        status = "disabled";
        #if (CONFIG_USE_TLV320AIC3104)
        tlv320aic3104@18{
                status= "okay";
        };
        #endif

        #if (CONFIG_USE_RT5616)
        rt5616@1b{
                status= "okay";
        };
        #endif

        #if (CONFIG_USE_NAU8810)
        nau8810@1a {
                status= "okay";
        };
        #endif

        #if (CONFIG_USE_ES8311)
        //when CE pin is equal to low level, it is 0x18, high level equal 0x19
        es8311@19 {
                reg = <0x19>;
                status= "okay";
        };
        #endif
}
```

#### 3.1.2 I2C1 引脚设备树配置

I2C1 引脚配置见 SDK 包中文件 `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-common.dtsi`，其默认配置如下所示：

```dts
twsi0_pmx_func: twsi0_pmx_func {
        pinctrl-single,pins = <
                GPIO049 AF1
                GPIO050 AF1
        >;
        MFP_LPM_FLOAT;
};

twsi0_pmx_gpio: twsi0_pmx_gpio {
        pinctrl-single,pins = <
                GPIO049 AF0
                GPIO050 AF0
        >;
        MFP_LPM_FLOAT;
};
```

- **AF1**：引脚复用为 I2C 功能（I2C_SCL / I2C_SDA）
- **AF0**：引脚复用为 GPIO 功能（GPIO_49 / GPIO_50）

**备注：** 如上默认配置已经在 I2C1 引脚设备树配置文件 `ql-1806e-common.dtsi` 中设置完成，用户无需重新设置。如果用户对引脚电平有特殊要求，则可根据具体需求对 I2C1 引脚配置进行相应修改。

#### 3.1.3 I2C1 从设备设备树配置

I2C1 从设备的配置信息位于 SDK 包中文件 `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-common.dtsi`。TLV320AIC3104 codec 配置如下：

```dts
/* I2C for codec */
twsi0: i2c@d4011000 {
        status= "ok";
        tlv320aic3104@18{
                compatible = "quectel,tlv320aic3104";
                reg = <0x18>;
                ai3x-micbias-vg = <2>;
                status= "disabled";
        };

        rt5616@1b{
                compatible = "quectel,rt5616";
                reg = <0x1b>;
                status= "disabled";
        };

        nau8810@1a {
                compatible = "quectel,nau8810";
                reg = <0x1a>;
                interrupt-parent = <&gpio>;
                interrupts = <124 0x1>;
                marvell,88pm895-irq-write-clear;
                status= "disabled";
        };

        es8311@19 {
                compatible = "quectel,es8311";
                //when CE pin is equal to low level, it is 0x18, high level equal 0x19
                reg = <0x19>;
                status= "disabled";
        };
};
```

已集成的从设备列表：

| 从设备 | I2C 地址 | compatible 字符串 | 用途 |
|--------|---------|------------------|------|
| TLV320AIC3104 | 0x18 | quectel,tlv320aic3104 | 音频 codec |
| RT5616 | 0x1b | quectel,rt5616 | 音频 codec |
| NAU8810 | 0x1a | quectel,nau8810 | 音频 codec |
| ES8311 | 0x18/0x19 | quectel,es8311 | 音频 codec（CE脚低=0x18，高=0x19）|

如果用户需要新增 I2C1 从设备，须从设备供应商处获取驱动程序和配置手册并按照配置手册新增 I2C1 从设备。

**I2C1 从设备开关配置：**

I2C1 从设备的开关配置见文件 `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-std.dts`；I2C1 从设备 TLV320AIC3104 codec 默认使能，通过将 `CONFIG_USE_TLV320AIC3104 1` 改为 `CONFIG_USE_TLV320AIC3104 0`，即可关闭 I2C1 从设备 TLV320AIC3104 codec：

```c
/* Config codecs */
#define CONFIG_USE_TLV320AIC3104    1   // 改为 0 可关闭
#define CONFIG_USE_NAU8810          0
#define CONFIG_USE_RT5616           0
#define CONFIG_USE_ES8311           0
```

### 3.2 I2C2 设备树配置

#### 3.2.1 I2C2 总线驱动设备树配置

I2C2 总线驱动配置位于 SDK 包中文件 `ql-ol-kernel/arch/arm/boot/dts/asr1806.dtsi`，其默认配置如下所示：

```dts
twsi3: i2c@d4010c00 {
        compatible = "mrvl,mmp-twsi";
        #address-cells = <1>;
        #size-cells = <0>;
        reg = <0xd4010c00 0x50>;
        interrupts = <67>;
        lpm-qos = <PM_QOS_CPUIDLE_BLOCK_DDR>;
        mrvl,i2c-fast-mode;
        marvell,i2c-ilcr = <0x82c469f>;
        marvell,i2c-iwcr = <0x142a>;
        /* implement this part in board file */
        pinctrl-names = "default","gpio";
        pinctrl-0 = <&twsi3_pmx_func>;
        pinctrl-1 = <&twsi3_pmx_gpio>;
        i2c-gpio = <&gpio 41 0 &gpio 42 0>;
        clocks = <&soc_clocks ASR1803_CLK_TWSI3>;
        clock-names = "twsi3_clk";
        status = "disabled";
};
```

**备注：** 此部分配置用户无需关注，相关配置在默认情况下都已配置完成。

I2C2 总线开关配置见文件 `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-std.dts`；I2C2 总线默认使能，通过在文件最后添加如下内容即可关闭 I2C2 总线：

```dts
&twsi3 {
        status= "disabled";
};
```

#### 3.2.2 I2C2 引脚设备树配置

I2C2 引脚配置信息见 SDK 包中文件 `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-common.dtsi`，其默认配置如下所示：

```dts
twsi3_pmx_func: twsi3_pmx_func {
        pinctrl-single,pins = <
                GPIO41 AF3
                GPIO42 AF3
        >;
        MFP_LPM_FLOAT;
};

twsi3_pmx_gpio: twsi3_pmx_gpio {
        pinctrl-single,pins = <
                GPIO41 AF0
                GPIO42 AF0
        >;
        MFP_LPM_FLOAT;
};
```

- **AF3**：引脚复用为 I2C 功能（I2C_SCL / I2C_SDA）
- **AF0**：引脚复用为 GPIO 功能（GPIO_41 / GPIO_42）

**备注：** 如上默认配置已经在 I2C2 引脚设备树配置文件 `ql-1806e-common.dtsi` 中设置完成，用户无需重新设置。如果用户对引脚电平有特殊要求，则可根据具体需求对 I2C2 引脚配置进行相应修改。

---

## 4 I2C 编译示例及功能验证

模块 SDK 包中包含了 I2C 功能的编译示例，该示例路径为 `sample/i2c`。所述示例演示如何将数据写入位于指定地址的从设备的寄存器地址，然后读取数据。

### 4.1 编译步骤

1. 示例代码定义了以下常量（以 TLV320AIC3104 codec 为目标从设备）：

   ```c
   #define I2C_DEV         "/dev/i2c-0"
   #define I2C_SLAVE_ADDR  0x18    // codec 3104
   #define WHO_AM_I        0x09
   #define WHO_AM_I_VALUE  0x12
   ```

   功能：将值 `0x12` 写入从设备地址 `0x18` 的寄存器地址 `0x09`，然后读回并校验。

2. 进入目录 `sample/i2c`，执行 `make` 命令生成可执行程序 `i2c_test`：

   ```bash
   cd sample/i2c
   make
   ```

   编译命令（自动执行，使用交叉编译工具链）：

   ```bash
   /opt/ql_crosstools/ql-ag35-1806e-gcc-8.4.0-glibc-v1-toolchain/bin/arm-openwrt-linux-gcc \
     -o main.o -c main.c -march=armv7-a -marm -mfpu=neon -mfloat-abi=hard \
     -I. -I/home/searle/ql-ol-extsdk-ag35cetcar01a02m2g_ocpu/ql-sysroots/usr/include/ql-sdk \
     -Wall -Wundef \
     /opt/ql_crosstools/ql-ag35-1806e-gcc-8.4.0-glibc-v1-toolchain/bin/arm-openwrt-linux-gcc \
     -o i2c_test main.o -march=armv7-a -marm -mfpu=neon -mfloat-abi=hard \
     -L/home/searle/ql-ol-extsdk-ag35cetcar01a02m2g_ocpu/ql-sysroots/usr/lib \
     -lql_sdk -lpthread -lql_sys_log -lql_lib_ipc -lql_lib_utils
   ```

   编译完成后生成 `i2c_test`、`main.c`、`main.o`、`Makefile`。

### 4.2 功能测试

1. 参考第 4.1 章编译并生成可执行程序 `i2c_test`。

2. 执行以下命令将 `i2c_test` 上传至模块：

   ```bash
   adb push <i2c_test在上位机路径> <模块内部路径，如/tmp>
   ```

3. 将模块和 codec 连接到 OPEN_EVB 上的模块以建立硬件通道。

4. 执行 `i2c_test`，验证 I2C 功能是否编译成功（如下所示两处 value 一致，即表示成功）：

   ```bash
   root@OpenWrt:/tmp# ./i2c_test
   < ql_i2c_init=3 >
   < write i2c value=0x12, iRet=0 >
   < read i2c iRet=0, value=0x012 >
   ```

   - 写入值：`0x12`（iRet=0 表示写入成功）
   - 读取值：`0x012`（与写入值一致，表示 I2C 读写功能正常）

---

## 5 I2C 驱动调试方法

本章介绍常见的 I2C 软件和硬件问题，以及从软件角度进行 I2C 的调试。

**步骤 1：确认 I2C1 总线是否使能**

若存在 `i2c-0` 文件，表示 I2C1 总线已使能：

```bash
ls /sys/devices/platform/soc/d4000000.apb/pxa2xx-i2c.0/
```

预期输出（I2C1 已使能）：

```
driver        of_node    supplier:mmp-gpio
driver_override  power     uevent
i2c-0         subsystem
modalias      supplier:d401e000.pinmux
```

**步骤 2：确认 I2C2 总线是否使能**

若存在 `i2c-3` 文件，表示 I2C2 总线已使能：

```bash
ls /sys/devices/platform/soc/d4000000.apb/pxa2xx-i2c.3/
```

预期输出（I2C2 已使能）：

```
driver        of_node    supplier:mmp-gpio
driver_override  power     uevent
i2c-3         subsystem
modalias      supplier:d401e000.pinmux
```

**步骤 3：确认 I2C1 从设备相关配置是否成功**

若存在 `0-0018`，则表示当前 TLV320AIC3104 codec 已经配置且 codec 开关已经打开：

```bash
ls /sys/devices/platform/soc/d4000000.apb/pxa2xx-i2c.0/i2c-0/
```

预期输出（codec 已配置）：

```
0-0018     i2c-dev   power
0-001a     name      subsystem
0-001b     new_device  uevent
delete_device  of_node   waiting_for_supplier
```

在 `0-0018` 下有 `driver` 文件，表示当前 codec 驱动已经匹配成功：

```bash
ls /sys/devices/platform/soc/d4000000.apb/pxa2xx-i2c.0/i2c-0/0-0018/
```

预期输出：

```
driver    modalias    name    of_node    power    subsystem    uevent
```

**步骤 4：执行以下命令，确认 I2C 引脚配置是否成功：**

```bash
cat /sys/kernel/debug/pinctrl/d401e000.pinmux-pinctrl-single/pinmux-pins
```

- **I2C1 引脚配置成功**（GPIO 已注册在 I2C1 下）：

  ```
  pin 104 (PIN104): pxa2xx-i2c.0 (GPIO UNCLAIMED) function twsi0_pmx_func group twsi0_pmx_func
  pin 105 (PIN105): pxa2xx-i2c.0 (GPIO UNCLAIMED) function twsi0_pmx_func group twsi0_pmx_func
  ```

- **I2C2 引脚配置成功**（GPIO 已注册在 I2C2 下）：

  ```
  pin 96 (PIN96): pxa2xx-i2c.3 (GPIO UNCLAIMED) function twsi3_pmx_func group twsi3_pmx_func
  pin 97 (PIN97): pxa2xx-i2c.3 (GPIO UNCLAIMED) function twsi3_pmx_func group twsi3_pmx_func
  ```

**步骤 5：打开 I2C 调试日志**

一般通过 kernel log 即可查看 I2C 相关错误，通过现有的错误可以查看相关问题。例如，可以通过 `dmesg` 过滤 I2C log：

```bash
dmesg | grep i2c
```

示例输出：

```
[  0.121999] I2C: i2c-0: PXA I2C adapter
[  0.122162] pxa2xx-i2c pxa2xx-i2c.2: could not get pinctrl
[  0.122213] I2C: i2c-2: PXA I2C adapter
[  0.152224] I2C: i2c-2: PXA I2C adapter
[  0.152273] I2C: i2c-3: PXA I2C adapter
[  1.234757] i2c /dev entries driver
[  1.326379] I2C: i2c-0: Enter asc3x_i2c-probe
[  1.327477] i2c: pxa-i2c: slave_0x18 error: exhausted retries
[  1.333668] i2c: msg num: 0 msg_idx: -2000 msg_ptr: 0
[  1.339109] i2c: IDMR: 00000003 IBDR: 00000030 ICR: 00000000 ISR: 00000000
[  1.346112] I2C: log
[  1.346254] Failed: i2c-0: slave_18, R 1 byte by 2 msg R: 00: 00
```

若需查看完整的 Kernel log，可以直接输入 `dmesg` 输出 log。

**常见 I2C 问题排查要点：**

| 症状 | 可能原因 | 排查方式 |
|------|---------|---------|
| `/dev/` 下无 `i2c-0` | I2C1 总线未使能 | 检查 DTS `status = "okay"` |
| `/dev/` 下无 `i2c-3` | I2C2 总线未使能 | 检查 DTS `&twsi3` 节点 |
| `0-0018` 不存在 | codec 开关未打开 | 检查 `CONFIG_USE_TLV320AIC3104` 宏 |
| `0-0018` 下无 `driver` | codec 驱动未匹配 | 检查驱动是否编译进内核 |
| exhausted retries | 从设备无响应 | 检查硬件连接、上拉电阻、I2C 地址 |
| GPIO UNCLAIMED | 引脚未被 I2C 控制器认领 | 正常状态，表示引脚功能已配置给 I2C |

---

## 6 附录 参考文档及术语缩写

### 表 4：参考文档

| 编号 | 文档名称 |
|------|---------|
| [1] | Quectel_AG35-CET_QuecOpen_快速开发指导 |
| [2] | Quectel_AG35-CET_QuecOpen_GPIO 配置 |

### 表 5：术语缩写

| 缩写 | 英文描述 | 中文描述 |
|------|---------|---------|
| API | Application Programming Interface | 应用程序编程接口 |
| CPU | Central Processing Unit | 中央处理器 |
| EVB | Evaluation Board | 评估板 |
| GPIO | General-Purpose Input/Output | 通用型输入/输出 |
| I2C | Inter-Integrated Circuit | 集成电路总线 |
| IoV | Internet of Vehicles | 车联网 |
| I/O | Input/Output | 输入/输出 |
| SDK | Software Development Kit | 软件开发工具包 |
