# EC200A-CN(TA) QuecOpen I2C 开发指导 — 完整分析

> **源文档**：Quectel_EC200A-CN(TA)_QuecOpen_I2C_开发指导_V1.0.0_Preliminary_20220629.pdf
> **适用平台**：LTE Standard 模块系列 — EC200A-CN(TA) QuecOpen（基于 Linux 的嵌入式开发平台）
> **版本**：1.0.0　**日期**：2022-06-29　**状态**：临时文件（Preliminary）
> **原文页数**：15 页　**底层芯片**：ASR1803

---

## 文档历史（修订记录）

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| - | 2021-11-25 | Larry ZHANG | 文档创建 |
| 1.0.0 | 2022-06-29 | Larry ZHANG / Jayden CHEN | 临时版本 |

---

## 1 引言

- EC200A-CN(TA) 模块支持 **QuecOpen®** 方案（基于 Linux 的嵌入式开发平台，简化 IoT 应用软件设计开发）。详见参考文档 [1]。
- 本文档介绍 QuecOpen® 方案下 EC200A-CN(TA) 系列模块的 **I2C 功能开发**，包含：**I2C 接口引脚定义、设备树配置、功能验证、驱动调试方法**。

---

## 2 I2C 引脚分配

模块默认支持的 I2C 引脚信息如下（设备树节点：**`twsi0`**）：

### 表 1：I2C（设备树节点：twsi0）引脚定义

| 引脚名 | 引脚号 | I/O | 复用功能 1（默认） | 复用功能 2 | 备注 |
|---|---|---|---|---|---|
| I2C_SCL | 41 | DO | I2C_SCL | GPIO_49 | 需要外部 1.8 V 上拉；不用则悬空。 |
| I2C_SDA | 42 | IO | I2C_SDA | GPIO_50 | 需要外部 1.8 V 上拉；不用则悬空。 |

**备注：**
1. EC200A-CN(TA) QuecOpen 模块**默认支持一路 I2C 接口**，且模块在与 I2C 接口有关的应用中**只能作为主设备（Master）**。GPIO 引脚配置详情参考文档 [2]。
2. **时钟支持**：标准（**100 kHz**）和 fast（**400 kHz**）；**默认为 fast 时钟**。

---

## 3 I2C 设备树配置

> 以下章节以 I2C 从设备 **TLV320AIC3104 codec** 为例，介绍 I2C 开发的相关信息。

### 3.1 I2C 总线驱动设备树配置

**（1）I2C 总线驱动配置**位于文件 `ql-ol-kernel/arch/arm/boot/dts/asr1803.dtsi`，默认配置如下：

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

> **备注**：此部分配置用户无需关注，相关配置在默认情况下都已配置完成。

**（2）I2C 总线开关配置**见文件 `ql-ol-kernel/arch/arm/boot/dts/ql-asr1803-overlay.dtsi`。I2C 总线**默认使能**（`status = "okay"`）。
> 通过将 `status = "okay"` 改为 `status = "disabled"`，即可**关闭 I2C 总线**。

```dts
//i2c
&twsi0 {
    status = "okay";
    /delete-node/ touch1;
    /delete-node/ alc5616@1b;
    tlv320aic3x_codec:tlv320aic3x_codec@18{
        compatible = "quectel,tlv320aic3104";
        reg = <0x18>;
        ai3x-micbias-vg = <2>;
    };

    rt5616:rt5616_codec@1b{
        compatible = "quectel,rt5616";
        reg = <0x1b>;
    };

    nau88c10:nau8810@1a {
        compatible = "quectel,nau88c10";
        reg = <0x1a>;
        interrupt-parent = <&gpio>;
        interrupts = <124 0x1>;
        marvell,88pm805-irq-write-clear;
    };
};
```

### 3.2 I2C 总线引脚设备树配置

I2C 总线引脚配置信息见文件 `ql-ol-kernel/arch/arm/boot/dts/qcom/asr1803-p401.dts`，默认配置如下：

```dts
twsi0_pmx_func: twsi0_pmx_func {
    pinctrl-single,pins = <
        GPIO49 AF1
        GPIO50 AF1
    >;
    MFP_LPM_FLOAT;
};
twsi0_pmx_gpio: twsi0_pmx_gpio {
    pinctrl-single,pins = <
        GPIO49 AF0
        GPIO50 AF0
    >;
    MFP_LPM_FLOAT;
};
```

> **备注**：如上默认配置已经在 I2C 引脚设备树配置文件中设置完成，用户无需重新设置。如果用户对引脚电平有特殊要求，则可根据具体需求对 I2C 引脚配置进行相应修改。

> 说明：`AF1` = 复用功能 1（I2C 功能），`AF0` = 复用功能 0（GPIO 功能）。

### 3.3 I2C 从设备设备树配置

I2C 从设备 **TLV320AIC3104 codec** 的配置信息如下，详见 SDK 文件 `ql-ol-kernel/arch/arm/boot/dts/qcom/ql-asr1803-overlay.dtsi`：

```dts
//i2c
&twsi0 {
    status = "okay";
    /delete-node/ touch1;
    /delete-node/ alc5616@1b;
    tlv320aic3x_codec:tlv320aic3x_codec@18{   // ← 本例新增的从设备
        compatible = "quectel,tlv320aic3104";
        reg = <0x18>;
        ai3x-micbias-vg = <2>;
    };

    rt5616:rt5616_codec@1b{
        compatible = "quectel,rt5616";
        reg = <0x1b>;
    };

    nau88c10:nau8810@1a {
        compatible = "quectel,nau88c10";
        reg = <0x1a>;
        interrupt-parent = <&gpio>;
        interrupts = <124 0x1>;
        marvell,88pm805-irq-write-clear;
    };
};
```

> ⚠️ **如果用户需要新增 I2C 从设备**，须从设备供应商处获取**驱动程序**和**配置手册**，并按照配置手册新增 I2C 从设备。从设备地址通过节点的 `reg = <0xXX>;` 指定（如 TLV320AIC3104 为 `0x18`）。

---

## 4 I2C 编译示例及功能验证

模块 SDK 包中包含 I2C 功能的编译示例，路径为 **`sample/i2c`**。该示例演示如何**将数据写入位于指定地址的从设备的寄存器地址，然后读取数据**。

### 4.1 编译步骤

**步骤 1**：示例核心宏定义（将数据写入指定地址从设备的寄存器地址，然后读取）：

```c
#define I2C_DEV          "/dev/i2c-0"
#define I2C_SLAVE_ADDR   0x18    //codec 3104
#define WHO_AM_I         0x09
#define WHO_AM_I_VALUE   0x12
```

**步骤 2**：进入目录 `sample/i2c`，执行 `make` 命令生成可执行程序 **`i2c_test`**。

编译使用的工具链为 `arm-openwrt-linux-gcc`（`ql-ol-extsdk`），链接库包括 `-lql_sdk -lpthread -lql_sys_log -lql_lib_ipc -lql_lib_utils` 等。编译完成后目录下生成 `i2c_test`。

### 4.2 功能测试

**步骤 1**：参考第 4.1 章编译并生成可执行程序 `i2c_test`。

**步骤 2**：执行以下命令将 `i2c_test` 上传至模块：

```sh
adb push <i2c_test 在上位机路径> <模块内部路径，如/tmp>
```

**步骤 3**：将模块和 codec 连接到 **OPEN_EVB** 上的模块以建立硬件通道。

**步骤 4**：执行 `i2c_test`，验证 I2C 功能是否编译成功（**如下所示两处 value 一致，即表示成功**）：

```
root@Openwrt:/tmp# ./i2c_test
< ql_i2c_init=3 >
< write i2c value=0x12, iRet=0 >
< read i2c iRet=0, value=0x012 >
root@Openwrt:/tmp#
```

> ✅ 写入 `value=0x12` 与读回 `value=0x012` 一致，`iRet=0`，表示 I2C 读写功能正常。

---

## 5 I2C 驱动调试方法

本章介绍常见的 I2C 软件和硬件问题，以及从软件角度进行 I2C 的调试。

**（1）确认 I2C 总线是否使能**：若存在 `i2c-0` 文件，表示 I2C 总线已使能。

**（2）确认 I2C 从设备相关配置是否成功**：
- 若存在 `3-0018`，则表示当前 TLV320AIC3104 codec **已经配置且 codec 开关已经打开**。
- 在 `3-0018` 下有 `driver` 文件，表示当前 **codec 驱动已经匹配成功**。

> 说明：`3-0018` 中 `3` 为 I2C 总线号，`0018` 为从设备地址 `0x18`。

**（3）确认 I2C 引脚配置是否成功**，执行：

```sh
cat /sys/kernel/debug/pinctrl/d401e000.pinmux-pinctrl-single/pinmux-pins
```

> 若当前配置的 GPIO（GPIO49/GPIO50）已注册在 I2C 下，表示 I2C 引脚配置成功。

**（4）打开 I2C 调试日志**：
- 一般通过 kernel log 即可查看 I2C 相关错误，通过现有错误可以查看相关问题。
- 例如，可以通过 `dmesg` 过滤 I2C log：

```sh
dmesg | grep -i i2c
```

- 若需查看完整的 kernel log，可以直接输入 `dmesg` 输出 log。

---

## 6 附录 参考文档及术语缩写

### 表 2：参考文档

| 序号 | 文档名称 |
|---|---|
| [1] | Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导 |
| [2] | Quectel_EC200A-CN(TA)_QuecOpen_GPIO 配置 |

### 表 3：术语缩写

| 缩写 | 英文描述 | 中文描述 |
|---|---|---|
| API | Application Programming Interface | 应用程序编程接口 |
| CPU | Central Processing Unit | 中央处理器 |
| EVB | Evaluation Board | 评估板 |
| GPIO | General-Purpose Input/Output | 通用型输入/输出 |
| I2C | Inter-Integrated Circuit | 集成电路总线 |
| IoT | Internet of Things | 物联网 |
| I/O | Input/Output | 输入/输出 |
| SDK | Software Development Kit | 软件开发工具包 |

---

## 关键要点速查（实践提炼）

- 一路 I2C（节点 `twsi0`），模块**只能作主设备**；引脚 **SCL=Pin41(GPIO49)**、**SDA=Pin42(GPIO50)**，**需外部 1.8 V 上拉**。
- 默认时钟 **fast 400 kHz**（也支持标准 100 kHz）；设备节点 `i2c@d4011000`，`ilcr/iwcr` 调速率。
- 设备树三处：总线驱动 `asr1803.dtsi`（无需动）、总线开关 `ql-asr1803-overlay.dtsi`（`status` 开关）、引脚 `asr1803-p401.dts`（`AF1`=I2C / `AF0`=GPIO）。
- 新增从设备：在 `&twsi0` 节点下加子节点，`compatible` + `reg=<地址>`，并从供应商取驱动与配置手册。
- 用户态设备节点 `/dev/i2c-0`；示例 `sample/i2c` → `i2c_test`，写读 value 一致即成功。
- 调试四板斧：查 `i2c-0`（总线）→ 查 `3-0018`/其 `driver`（从设备与驱动匹配）→ 查 `pinmux-pins`（引脚）→ `dmesg | grep i2c`（内核日志）。
