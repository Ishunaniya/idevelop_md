# Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_UART_开发指导_V1.0.1_Preliminary_20250306 —— PDF 分析报告

## 一、文档基本信息

- **标题**：AG35-CET&AG35-EUT QuecOpen(SDK) UART 开发指导
- **适用产品/模块**：AG35-CET、AG35-EUT（LTE Standard 模块系列）
- **适用方案**：QuecOpen®（基于 Linux 的嵌入式开发平台）SDK 构建环境
- **版本号**：1.0.1
- **日期**：2025-03-06
- **文档状态**：临时文件（Preliminary），文档内注明"本文档为临时版本，其中涉及的模块引脚或接口定义、频段、功能、特性及设计（若有）发生变动的可能性较大，部分参数经过初步验证或仍未验证。变更或补充信息将在后续正式版本中体现"。
- **作者**：Searle FANG
- **版权方**：上海移远通信技术股份有限公司（Quectel Wireless Solutions Co., Ltd.），版权所有 © 2025
- **保密等级**：文档水印标注 "Preliminary Confidential"

### 修订记录（文档历史）

| 版本 | 日期 | 作者 | 变更表述 |
|------|------|------|----------|
| - | 2023-11-16 | Searle FANG | 文档创建 |
| 1.0.0 | 2023-11-16 | Searle FANG | 临时版本 |
| 1.0.1 | 2025-03-06 | Searle FANG | 临时版本：1. 新增适用模块 AG35-EUT；2. 更新 MAIN_CTS 和 MAIN_RTS 引脚号（第 2.2 章） |

可见本次（V1.0.1）的核心变更：新增对 AG35-EUT 模块的适用性，并修正了主 UART（MAIN_CTS/MAIN_RTS）的引脚号定义。

### 文档目的与适用范围（第 1 章 引言）

- AG35-CET 和 AG35-EUT 模块支持 QuecOpen® 方案；QuecOpen® 是基于 Linux 的嵌入式开发平台，简化 IoV（车联网）应用的软件设计和开发过程。
- 本文档适用于 **SDK 构建环境** 下的 QuecOpen® 方案，主要介绍 AG35-CET 和 AG35-EUT 模块的 UART 功能开发，内容包含：UART 接口引脚介绍、设备树配置、UART 编译示例和功能验证。

## 二、目录结构概览

```
文档历史
目录
表格索引
1 引言
2 UART 功能和引脚综述
  2.1 接口功能
  2.2 引脚定义
3 UART 设备树配置
  3.1 配置说明
  3.2 调试 UART 设备树配置
  3.3 主 UART 设备树配置
  3.4 蓝牙 UART 设备树配置
  3.5 GNSS UART 设备树配置
4 UART 编译示例及功能验证
  4.1 编译步骤
  4.2 功能验证
    4.2.1 未使能流控
    4.2.2 使能硬件流控
5 附录 参考文档及术语缩写
```

全文正文共 20 页（封面、版权声明、文档历史等前置页另计），结构上是一篇典型的 Quectel SDK 子功能开发指导（UART 专题），覆盖"硬件引脚 → 软件设备树配置 → 编译验证"完整链路。

## 三、逐章节详细摘要

### 第 1 章 引言

- 简述 QuecOpen® 平台定位（基于 Linux 的嵌入式开发平台，简化 IoV 应用开发）。
- 指出本文档主要内容范围：UART 接口引脚介绍、设备树配置、UART 编译示例和功能验证。
- 引用 *文档 [1]*（QuecOpen® 详细信息，对应附录中的《Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导》）。

### 第 2 章 UART 功能和引脚综述

#### 2.1 接口功能

QuecOpen® 方案下，AG35-CET 和 AG35-EUT 模块默认支持 **4 路 UART 接口**：
1. 调试 UART
2. 主 UART
3. 蓝牙 UART
4. GNSS UART

**表 1：UART 接口功能综述**

| UART 类型 | 支持波特率（bps） | 默认波特率（bps） | 功能描述 |
|-----------|--------------------|---------------------|----------|
| 调试 UART | 115200 | 115200 | 用于 Linux 控制台和日志输出 |
| 主 UART | 4800/9600/19200/38400/57600/115200/230400/460800/921600 | 115200 | 用于数据传输和 AT 命令通信；支持 RTS/CTS 硬件流控 |
| 蓝牙 UART | 同上（4800~921600 多档） | 115200 | 用于蓝牙数据传输；支持 RTS/CTS 硬件流控 |
| GNSS UART | 同上（4800~921600 多档） | 115200 | 使用外置 GNSS 时，用于 GNSS 数据传输 |

备注：有关串口引脚复用的更多信息，请参考第 2.2 章和*文档 [2]*（《Quectel_AG35-CET&AG35-EUT_QuecOpen_GPIO_Configuration》）。

注意：表格表头写"支持波特率（bps）"对调试 UART 一行只填了"115200"单值，未列出多档可选值，意味着调试 UART **固定为 115200**，不可调（与后文 3.2 节"不允许修改该功能配置"的描述一致）。主 UART/蓝牙 UART/GNSS UART 三者共享同一组"支持波特率"取值列表（4800~921600 共 9 档），默认均为 115200。

#### 2.2 引脚定义

**表 2：调试 UART（设备树节点：uart1）引脚定义**

| 引脚名 | 引脚号 | I/O | 描述 | 备注 |
|--------|--------|-----|------|------|
| DBG_RXD | 72 | DI | UART_RXD | 建议预留测试点；1.8V 电压域；不用则悬空 |
| DBG_TXD | 71 | DO | UART_TXD | 同上 |

**表 3：主 UART（设备树节点：uart2）引脚定义**

| 引脚名 | 引脚号 | I/O | 复用功能 1（默认） | 复用功能 2 | 备注 |
|--------|--------|-----|----------------------|-------------|------|
| MAIN_CTS | 56 | DO | UART_CTS | GPIO_32 | 连接至 MCU 的 CTS；1.8V 电压域；不用则悬空 |
| MAIN_RTS | 57 | DI | UART_RTS | GPIO_31 | 连接至 MCU 的 RTS；1.8V 电压域；不用则悬空 |
| MAIN_RXD | 58 | DI | UART_RXD | GPIO_51 | 1.8V 电压域；不用则悬空 |
| MAIN_TXD | 60 | DO | UART_TXD | GPIO_52 | 1.8V 电压域；不用则悬空 |

> 这两个引脚号（56/57）正是 V1.0.1 相对 V1.0.0 的修订点（修订记录第 2 条："更新 MAIN_CTS 和 MAIN_RTS 引脚号"），文档未给出修订前的旧值，仅说明已更新。

**表 4：蓝牙 UART（设备树节点：uart3）引脚定义**

| 引脚名 | 引脚号 | I/O | 复用功能 1（默认） | 复用功能 2 | 备注 |
|--------|--------|-----|----------------------|-------------|------|
| BT_TXD | 163 | DO | UART_TXD | GPIO_54 | 建议预留测试点；1.8V 电压域；不用则悬空 |
| BT_RXD | 165 | DI | UART_RXD | GPIO_53 | 同上 |
| BT_CTS | 164 | DO | UART_CTS | GPIO_47 | 建议预留测试点；1.8V 电压域；不用则悬空；连接至 Wi-Fi & 蓝牙模块的 CTS |
| BT_RTS | 166 | DI | UART_RTS | GPIO_46 | 建议预留测试点；1.8V 电压域；不用则悬空；连接至 Wi-Fi &蓝牙模块的 RTS |

**表 5：GNSS UART（设备树节点：uart4）引脚定义**

| 引脚名 | 引脚号 | I/O | 复用功能 1（默认） | 复用功能 2 | 备注 |
|--------|--------|-----|----------------------|-------------|------|
| GNSS_RXD | 161 | DI | UART_RXD | GPIO_44 | 1.8V 电压域；使用外置 GNSS 时该 GNSS UART 用于 GNSS 数据传输（模块默认内置 GNSS，使用内置 GNSS 时 GNSS_TXD/RXD 可悬空） |
| GNSS_TXD | 151 | DO | UART_TXD | GPIO_45 | 同上 |

备注（页 9）：通过复用，UART 接口也可作为 GPIO 功能使用；有关模块 GPIO 引脚配置的详细信息，请参考*文档 [2]*。

### 第 3 章 UART 设备树配置

#### 3.1 配置说明

**表 6：UART 与设备树文件中字段的对应关系**

| UART 类型 | 设备树文件中的对应字段 |
|-----------|--------------------------|
| 调试 UART | uart1: uart@d4017000 |
| 主 UART | uart2: uart@d4036000 |
| 蓝牙 UART | uart3: uart@d4018000 |
| GNSS UART | uart4: uart@d401f000 |

说明文字：模块支持有关 UART 兼容的驱动、引脚选择、寄存器地址、UART 中断号、时钟、系统休眠等功能配置，用户可以根据需要打开或关闭相应功能。

#### 3.2 调试 UART 设备树配置

- 调试 UART **禁用硬件流控**，在模块内部显示为设备节点 `/dev/ttyS0`。
- 调试 UART 默认用于 Linux 控制台和日志输出，**不支持复用为其他功能，因此不允许修改该功能配置**。
- 截图显示设备节点列表（`ls /dev/tty*`），`/dev/ttyS0` 高亮标出。
- 调试 UART 设备树文件的 SDK 路径为 `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-common.dtsi`。
- 设备节点 `/dev/ttyS0` 在设备树文件中默认存在，即调试串口功能默认打开（`status = "okay"`），示例片段：

```
/* Debug UART */
uart1: uart@d4017000 {
        pinctrl-names = "default","sleep";
        pinctrl-0 = <&uart1_pmx_func1 &uart1_pmx_func2>;
        pinctrl-1 = <&uart1_pmx_func1_sleep &uart1_pmx_func2>;
        status = "okay";
};
```

#### 3.3 主 UART 设备树配置

- 主 UART **默认支持硬件流控**，在模块内部显示为设备节点 `/dev/ttyS1`。
- 主 UART 设备树文件的 SDK 路径同为 `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-common.dtsi`。
- 设备节点 `/dev/ttyS1` 默认存在，主 UART 功能默认打开（`status = "okay"`），示例片段：

```
/* Main UART */
uart2: uart@d4036000 {
        pinctrl-names = "default";
        pinctrl-0 = <&gps_pmx_uart_rxd &gps_pmx_uart_txd>;
        status = "okay";
};
```

> 注意：此处 `pinctrl-0` 引用名字带有 `gps_pmx_uart_*` 前缀（看起来像 GPS 相关的 pinmux 名称），但实际节点注释为 "Main UART"，对应设备节点 `uart2`。文档未对这一命名上的"GPS/Main"不一致做任何解释，疑似设备树底层 pinctrl 节点命名沿用了历史/共用资源（文档未说明缘由）。

- 关闭方法：通常推荐将主 UART 作为默认串口使用，或修改为普通 GPIO 使用。如需将主 UART 用作普通 GPIO，需修改主 UART 的开关配置文件 `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-std.dts`，在该文件末尾处添加以下内容以关闭主 UART 功能（此时设备节点 `/dev/ttyS1` 将不存在）：

```
&uart2{
        status= "disabled";
};
```

- 配图显示该 `disabled` 片段需添加在 `ql-1806e-std.dts` 文件末尾（位于 `#if (CONFIG_USE_ES8311) ... #endif` 代码块之后）。
- 完成设备树配置后，可参考第 4.1 章所述步骤完成内核的编译，然后将编译生成的内核镜像文件烧录到模块，即可使配置生效。

#### 3.4 蓝牙 UART 设备树配置

- 蓝牙 UART **默认支持硬件流控**，在模块内部显示为设备节点 `/dev/ttyS2`。
- 设备树文件 SDK 路径同为 `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-common.dtsi`。
- 设备节点 `/dev/ttyS2` 默认存在，蓝牙 UART 功能默认打开（`status = "okay"`），示例片段：

```
uart3: uart@d4018000 {
        compatible = "asr,mmp-uart";
        reg = <0xd4018000 0x1000>;
        interrupts = <28>;
        uart-drcmr-rx = <23>;
        uart-drcmr-tx = <24>;
        dmas = <&pdma0 23 1
                &pdma0 24 1>;
        dma-names = "rx", "tx";
        clocks = <&soc_clocks ASR1803_CLK_UART1>;
        resets = <&soc_clocks ASR1803_CLK_UART1>;
        pinctrl-names = "default";
        pinctrl-0 = <&uart3_pmx_uart_rxd &uart3_pmx_uart_txd>;
        lpm-qos = <PM_QOS_CPUIDLE_BLOCK_AXI>;
        status = "okay";
};
```

- 该节点是文档中信息最完整的 UART 设备树示例，包含 `compatible`（驱动兼容字符串 `asr,mmp-uart`，表明底层平台为 ASR/紫光展锐系列）、寄存器基址 `reg`、中断号 `interrupts`、DMA 请求线 `uart-drcmr-rx/tx`、DMA 通道 `dmas`、时钟/复位源 `clocks`/`resets`（均指向 `ASR1803_CLK_UART1`，注意 uart3 复用的时钟名仍叫 UART1，文档未解释这种命名映射）、pinctrl 配置、低功耗 QoS 约束 `lpm-qos = <PM_QOS_CPUIDLE_BLOCK_AXI>`（休眠时阻塞 AXI 总线空闲态，以保障 UART 不丢数据）。
- 关闭方法：通常推荐将蓝牙 UART 作为默认串口使用，或修改为普通 GPIO 使用。如需将蓝牙 UART 用作普通 GPIO，修改蓝牙 UART 开关配置文件 `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-std.dts`，在该文件末尾处添加以下内容以关闭蓝牙 UART 功能（此时设备节点 `/dev/ttyS2` 将不存在）：

```
&uart3{
        status= "disabled";
};
```

- 完成设备树配置后，同样参考第 4.1 章完成内核编译并烧录使配置生效。

#### 3.5 GNSS UART 设备树配置

- GNSS UART **禁用硬件流控功能**，在模块内部显示为设备节点 `/dev/ttyS3`。
- 模块默认内置 GNSS，使用内置 GNSS 时，GNSS_TXD/RXD 可悬空（即此时该路 UART 物理上不接外部设备，仍可在软件层面存在节点）。
- 设备树文件 SDK 路径同为 `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-common.dtsi`。
- 设备节点 `/dev/ttyS3` 默认存在，GNSS UART 功能默认打开（`status = "okay"`），示例片段：

```
uart4: uart@d401f000 {
        compatible = "asr,mmp-uart";
        reg = <0xd401f000 0x1000>;
        interrupts = <19>;
        uart-drcmr-rx = <19>;
        uart-drcmr-tx = <20>;
        dmas = <&pdma0 19 1
                &pdma0 20 1>;
        dma-names = "rx", "tx";
        clocks = <&soc_clocks ASR1803_CLK_UART3>;
        resets = <&soc_clocks ASR1803_CLK_UART3>;
        lpm-qos = <PM_QOS_CPUIDLE_BLOCK_AXI>;
        pinctrl-names = "default";
        pinctrl-0 = <&uart4_pmx_uart_rxd &uart4_pmx_uart_txd>;
        status = "okay";
};
```

- 关闭方法：通常推荐将 GNSS UART 作为默认串口使用，或修改为普通 GPIO 使用。如需将 GNSS UART 用作普通 GPIO，修改 GNSS UART 开关配置文件 `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-std.dts`，在该文件末尾处添加以下内容以关闭 GNSS UART 功能（此时设备节点 `/dev/ttyS3` 将不存在）：

```
&uart4{
        status= "disabled";
};
```

- 完成设备树配置后，同样参考第 4.1 章完成内核编译并烧录使配置生效。

> 观察：四路 UART 的硬件资源映射为 `uart1`→调试、`uart2`→主、`uart3`→蓝牙、`uart4`→GNSS；其中 `uart3` 的 `clocks`/`resets` 写的是 `ASR1803_CLK_UART1`（而不是 UART3），`uart4` 写的是 `ASR1803_CLK_UART3`（而不是 UART4），即设备树里的"时钟编号"与"UART 节点编号"存在错位的命名（这是文档截图本身呈现的内容，并非分析者推测；文档未对此错位做任何说明）。

### 第 4 章 UART 编译示例及功能验证

#### 4.1 编译步骤

SDK 包中包含 UART 功能的编译示例，路径为 `sample/uart`。示例演示如何从 TX 发送数据以及如何读取 RX 接收的数据。本章以**主 UART**为例介绍如何进行串口开发及功能验证。

**步骤一**：若修改了配置文件，需在 SDK 根目录下运行 `make kernel`，编译内核并制作内核镜像文件。将编译生成的内核镜像文件烧录到模块，使配置生效。
- 截图显示编译过程日志（`AS`/`LD`/`OBJCOPY` 等典型 Linux 内核编译输出片段），编译产物路径示例：`arch/arm/boot/compressed/bswapsdi2.o`、`arch/arm/boot/zImage`，最终拷贝到 `/home/searle/ql-ol-extsdk/.../ql-ag35cet01a02m2g_ocpu/target/zImage`，并执行 `cat ... dtb >> ... zImage` 将设备树二进制（dtb）追加到 zImage 后面，再用 `ls target` 验证。

**步骤二**：运行示例程序，UART 发送的数据信息如下（源码片段）：

```c
int main(int argc, char* argv[])
{
    int iRet;
    int baudRate;
    char strTmp[] = "uart test, =+-_0)9(8*7&6^5%4$3#2@1!`~)\n";
    if (argc < 2)
    {
        printf("Usage: %s <baud rate> \n", argv[0]);
        return -1;
    }
    ...
```

- 该测试字符串特意包含数字 0-9 与常见标点/特殊字符组合（`=+-_0)9(8*7&6^5%4$3#2@1!\`~)`），用于全面验证 UART 收发数据是否有丢字符、误码等问题。
- 程序要求传入命令行参数 `<baud rate>`，否则打印 Usage 并退出（返回值 -1）。

**步骤三**：进入目录 `sample/uart`，执行 `make` 生成可执行文件 `uart_test`。
- 截图显示交叉编译命令调用了 `/opt/ql_crosstools/ql-ag35-1806e-gcc-8.4.0-glibc-v1-toolchain/bin/arm-openwrt-linux-gcc`，关键编译/链接参数：
  - 编译：`-march=armv7-a -mfpu=neon -mfloat-abi=hard -I. -I<sysroot>/usr/include -I<sysroot>/.../include/ql-sdk -Wall -Wundef`
  - 链接：同样架构参数 + `-L<sysroot>/usr/lib -lql_sdk -lpthread -lql_sys_log -lql_lib_ipc -lql_lib_utils`
  - 最终生成 `uart_test`，目录下包含 `main.c`、`main.o`、`Makefile`、`uart_test`。

#### 4.2 功能验证

编译并生成可执行文件 `uart_test` 后，可参考 4.2.1/4.2.2 章节步骤完成功能验证。

##### 4.2.1 未使能流控

- 配置为无流控的部分代码截图（位于示例源码中，注释为"Start: If need, to modify uart dcb config"）：

```c
/* Start: If need, to modify uart dcb config */
ST_UARTDCB dcb = {
    .flowctrl = FC_NONE,   //none flow control
    .databit  = DB_CS8,    //databit: 8
    .stopbit  = SB_1,      //stopbit: 1
    .parity   = PB_NONE,   //parity check: none
    .baudrate = baudRate   //baudrate: 115200
};
```

- **步骤一**：依次执行如下命令将可执行文件 `uart_test` 上传至模块，并修改执行权限：

```sh
adb push <uart_test 在上位机路径> <模块内部路径，如/tmp>
chmod 777 /tmp/uart_test
```

- **步骤二**：以回环测试为例，将主串口的 TX 和 RX 连接（即硬件层面短接 TXD/RXD 做 loopback）。
- **步骤三**：执行 `uart_test` 文件，验证 UART 功能是否成功（如下所示发送和接受一致，即表示成功）。
  - 截图示例输出：
    ```
    < OpenLinux: UART example >
    SET DCB ret: 0
    GET DCB ret: 0; baudrate: 115200, flowctrl: 0, databit: 8, stopbit: 1, paritybit: 0
    > write(fd=3)=40
    < read(uart)=40:uart test, =+-_0)9(8*7&6^5%4$3#2@1!`~)
    ...（循环往复，发送与接收内容一致）
    ```
  - 截图中以红色箭头标注"接收到的数据"，证明发送数据(`write`)与读取数据(`read`)内容完全一致即表示回环测试通过。

##### 4.2.2 使能硬件流控

- **步骤一**：修改流控模式为硬件流控（即 `FC_RTSCTS`）：

```c
/* Start: If need, to modify uart dcb config */
ST_UARTDCB dcb = {
    .flowctrl = FC_RTSCTS,  //none flow control（注：此注释文字与实际取值不符，原文档原样为 "none flow control"，应为文档/示例代码注释未同步更新）
    .databit  = DB_CS8,     //databit: 8
    .stopbit  = SB_1,       //stopbit: 1
    .parity   = PB_NONE,    //parity check: none
    .baudrate = baudRate    //baudrate: 115200
};
```

> 文档限制点：原文示例代码中 `.flowctrl = FC_RTSCTS` 后面紧跟的行内注释依然写的是 `//none flow control`，与实际启用的硬件流控（RTS/CTS）矛盾——这是文档截图本身代码注释未更新所致的不一致，并非分析者推测。

- **步骤二**：依次执行如下命令将可执行文件 `uart_test` 上传至模块，并修改执行权限（命令与 4.2.1 相同）：

```sh
adb push <uart_test 在上位机路径> <模块内部路径，如/tmp>
chmod 777 /tmp/uart_test
```

- **步骤三**：将主 UART 与上位机连接。
- **步骤四**：执行 `uart_test 115200`，并在上位机使用对应的波特率以及硬件流控方式打开对应 COM 端口。验证串口功能，使用移远通信 **LTE OPEN EVB** 和上位机通信以进行功能测试。
  - 文档明确强调：**波特率必须与模块适用的波特率一致**；上位机串口工具中 Flow Control 需选择 **HW Ctrl Flow（硬件流控）**。
  - 截图（图 1：使能硬件流控时模块与上位机的通信）展示了使用 "QCOM_V1.6" 串口调试工具的双向通信会话：
    - Session "1 New Session (2)" 持续显示 `read(uart)=15:hello quectel`（模块侧收到来自上位机的字符串 "hello quectel"，长度 15）。
    - Session "2 192.168.21.34" 显示串口配置区：COM Port: 7，Baudrate: 115200，StopBits: 1，Parity: None，Byte Size: 8，**Flow Control: HW Ctrl Flow**；中部日志窗口显示模块端循环发送的测试字符串（"uart test, =+_0)9(8\*7&6^5%4\$3#2@1!\`~"，与示例源码一致）。
    - 下方还显示 DCD/CTS/RI 状态行（`DCD:0 CTS:0 RI:0` → `DCD:0 CTS:1 RI:0`），用于体现硬件流控信号 CTS 状态的变化。
    - 操作区可见 "Input String" 输入框填写 "hello quectel"，配合 "Send Command" 按钮、DTR/RTS/View File/Show Time/HEX String/Show in HEX/Send With Enter 等勾选项，是典型的串口调试助手界面。
  - 图注：**图 1：使能硬件流控时模块与上位机的通信**

### 第 5 章 附录 参考文档及术语缩写

**表 7：参考文档**

| 序号 | 文档名称 |
|------|----------|
| [1] | Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导 |
| [2] | Quectel_AG35-CET&AG35-EUT_QuecOpen_GPIO_Configuration |

**表 8：术语缩写**

| 缩写 | 英文描述 | 中文描述 |
|------|----------|----------|
| API | Application Programming Interface | 应用程序编程接口 |
| CTS | Clear To Send | 清除发送 |
| EVB | Evaluation Board | 评估板 |
| GPIO | General-Purpose Input/Output | 通用型输入/输出 |
| I/O | Input/Output | 输入/输出 |
| IoV | Internet of Vehicles | 车联网 |
| MCU | Microcontroller Unit | 微型控制单元 |
| RTS | Request To Send | 请求发送 |
| SDK | Software Development Kit | 软件开发工具包 |
| UART | Inter-Integrated Circuit | 集成电路总线 |

> **文档明显错误**：表 8 中 "UART" 一行的英文展开写的是 **"Inter-Integrated Circuit"**（这其实是 I2C/I²C 的全称），中文译为"集成电路总线"，同样是 I2C 的中文俗称。UART 的正确英文全称应为 "Universal Asynchronous Receiver/Transmitter"（通用异步收发传输器）。这是文档原文本身存在的术语表错误，并非分析者推测或编造，已据实记录。

## 四、关键命令/接口/参数汇总表

### 4.1 四路 UART 总览

| UART 类型 | 设备树节点 | 默认设备节点(/dev/) | 默认硬件流控 | 默认波特率 | 用途 |
|-----------|------------|----------------------|----------------|------------|------|
| 调试 UART | uart1: uart@d4017000 | /dev/ttyS0 | 禁用，且不允许修改配置 | 115200（固定） | Linux 控制台和日志输出 |
| 主 UART | uart2: uart@d4036000 | /dev/ttyS1 | 默认启用 | 115200 | 数据传输和 AT 命令通信 |
| 蓝牙 UART | uart3: uart@d4018000 | /dev/ttyS2 | 默认启用 | 115200 | 蓝牙数据传输；CTS/RTS 连接至 Wi-Fi&蓝牙模块 |
| GNSS UART | uart4: uart@d401f000 | /dev/ttyS3 | 禁用 | 115200 | 外置 GNSS 数据传输（内置 GNSS 时可悬空） |

### 4.2 关键引脚编号汇总

| 引脚名 | 所属 UART | 引脚号 | I/O | 默认功能 | 复用功能2 |
|--------|-----------|--------|-----|----------|------------|
| DBG_RXD | 调试 UART | 72 | DI | UART_RXD | — |
| DBG_TXD | 调试 UART | 71 | DO | UART_TXD | — |
| MAIN_CTS | 主 UART | 56 | DO | UART_CTS | GPIO_32 |
| MAIN_RTS | 主 UART | 57 | DI | UART_RTS | GPIO_31 |
| MAIN_RXD | 主 UART | 58 | DI | UART_RXD | GPIO_51 |
| MAIN_TXD | 主 UART | 60 | DO | UART_TXD | GPIO_52 |
| BT_TXD | 蓝牙 UART | 163 | DO | UART_TXD | GPIO_54 |
| BT_RXD | 蓝牙 UART | 165 | DI | UART_RXD | GPIO_53 |
| BT_CTS | 蓝牙 UART | 164 | DO | UART_CTS | GPIO_47 |
| BT_RTS | 蓝牙 UART | 166 | DI | UART_RTS | GPIO_46 |
| GNSS_RXD | GNSS UART | 161 | DI | UART_RXD | GPIO_44 |
| GNSS_TXD | GNSS UART | 151 | DO | UART_TXD | GPIO_45 |

### 4.3 设备树配置文件路径

| 用途 | 路径 |
|------|------|
| 四路 UART 设备树（默认开关、寄存器、中断、时钟等） | `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-common.dtsi` |
| UART 开关配置（关闭某路 UART 转作 GPIO） | `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-std.dts` |

### 4.4 示例代码关键数据结构

```c
ST_UARTDCB dcb = {
    .flowctrl = FC_NONE | FC_RTSCTS,  // 流控：无流控 / 硬件流控(RTS/CTS)
    .databit  = DB_CS8,                // 数据位：8
    .stopbit  = SB_1,                  // 停止位：1
    .parity   = PB_NONE,                // 校验：无
    .baudrate = baudRate                // 波特率：由命令行参数传入
};
```

### 4.5 编译相关命令

| 命令 | 用途 |
|------|------|
| `make kernel`（SDK 根目录） | 修改设备树配置文件后，编译内核并制作内核镜像文件 |
| `make`（`sample/uart` 目录） | 编译 UART 示例程序，生成可执行文件 `uart_test` |
| `adb push <uart_test 路径> <模块路径，如/tmp>` | 将编译好的测试程序推送到模块 |
| `chmod 777 /tmp/uart_test` | 赋予可执行权限 |
| `./uart_test <baud rate>` | 运行测试程序，指定波特率（如 `115200`） |

## 五、完整操作流程还原

### 流程 A：在设备树层面关闭某路 UART（转作 GPIO）

1. 打开 `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-std.dts` 文件。
2. 在文件末尾追加对应 UART 节点的 `status = "disabled"` 覆盖片段（例如关闭主 UART：`&uart2{ status= "disabled"; };`；蓝牙 UART 对应 `&uart3`；GNSS UART 对应 `&uart4`）。
   - 调试 UART（uart1）**不支持**该操作，文档明确不允许修改。
3. 在 SDK 根目录执行 `make kernel`，编译内核并生成内核镜像（zImage，且会自动追加设备树二进制 dtb）。
4. 将编译生成的内核镜像文件烧录到模块。
5. 配置生效后，对应的 `/dev/ttyS*` 节点将不再存在；引脚可作为普通 GPIO 使用（具体 GPIO 配置参考*文档 [2]*）。

### 流程 B：UART 编译示例及未使能流控（回环）功能验证

1. 确认/修改设备树配置（如需），执行 `make kernel` 编译并烧录内核镜像使配置生效（同流程 A 步骤3-4，仅当改了配置文件才需要）。
2. 进入 `sample/uart` 目录，确认示例源码中 `ST_UARTDCB dcb` 的 `.flowctrl` 设为 `FC_NONE`（默认/未使能流控场景）。
3. 执行 `make`，交叉编译生成可执行文件 `uart_test`。
4. 通过 `adb push` 将 `uart_test` 上传至模块（如 `/tmp` 目录），并 `chmod 777` 赋予执行权限。
5. 硬件上将主 UART 的 TX 和 RX 短接，构成本地回环（loopback）。
6. 在模块上执行 `./uart_test 115200`（或对应波特率参数）。
7. 观察输出：程序内部循环 `write()` 测试字符串后立即 `read()`，若读取内容与写入内容一致（如截图所示 40 字节发送即收到 40 字节且内容相同），表示 UART 收发功能正常。

### 流程 C：使能硬件流控的上位机联调验证

1. 修改示例源码 `ST_UARTDCB dcb` 中 `.flowctrl` 字段为 `FC_RTSCTS`（启用硬件流控）。
2. 重新执行 `make` 编译生成 `uart_test`（隐含步骤，文档未重复列出但逻辑上必须）。
3. 通过 `adb push` + `chmod 777` 将新编译的 `uart_test` 上传到模块并赋权（与流程 B 相同命令）。
4. 用物理线缆将模块的主 UART（含 RTS/CTS 信号线）与上位机（如 PC）连接（不再是本地 TX/RX 短接回环，而是真正的两端通信）。
5. 在模块上执行 `./uart_test 115200`。
6. 在上位机串口调试工具（示例使用 "QCOM_V1.6"，并配合移远通信 LTE OPEN EVB 评估板）中：
   - 打开对应 COM 端口；
   - 设置波特率必须与模块一致（示例 115200）；
   - Flow Control 选择硬件流控（HW Ctrl Flow）；
   - 其余串口参数：8 数据位、1 停止位、无校验。
7. 上位机侧可在 "Input String" 中输入字符串（示例 "hello quectel"）并通过 "Send Command" 发送给模块；模块侧持续循环发送测试字符串到上位机。
8. 双向收发数据正常、且 CTS/RTS 信号状态符合预期（如 CTS 从 0 变为 1），即表示硬件流控下的 UART 功能验证通过。

## 六、与本项目（open_dial）的潜在关联点

本项目 `open_dial` 是运行在 Quectel **EC2x/EG2x** 模组（OpenNPC、ARMv7 架构）上的拨号管理程序，通过共享的 AT 串口 fd（`smd_fd`，路径如 `/dev/smd8`）与模组通信，使用 `pthread_mutex_t g_at_port_mutex` 保护跨线程访问。本 PDF 针对的是 **AG35-CET/AG35-EUT** 模块的 QuecOpen(SDK) 方案，与 EC2x/EG2x 不是同一系列模块，但两者均为 Quectel 模组、且都涉及 UART/AT 通信底层机制，存在以下可参考之处：

1. **AT 串口与设备节点的对应关系**：本文档展示了 AG35 系列在 QuecOpen Linux 系统下，AT/数据通信走的是"主 UART"对应的 `/dev/ttyS1`（默认硬件流控），而不是 `/dev/smd*` 这种 SMD（共享内存设备）接口。这提示：`open_dial` 当前使用的 `/dev/smd8` 是 EC2x/EG2x 平台（高通 Qualcomm 平台，AT通过SMD/QMI暴露）特有的访问方式；若未来该拨号程序需要移植到 AG35（ASR/紫光展锐平台，从设备树 `compatible = "asr,mmp-uart"` 可判断）这类模块，则 AT 命令通道很可能要切换为标准 UART tty 设备（如 `/dev/ttyS1`）而非 SMD 接口，驱动层和打开方式都需要相应改造，不能直接复用现有 `smd_fd` 逻辑。

2. **硬件流控（RTS/CTS）配置思路可参考**：本文档详细说明了"未使能流控"和"使能硬件流控"两种模式下设备树/示例代码的差异（`FC_NONE` vs `FC_RTSCTS`），以及对应的物理连线和上位机配置方法。`open_dial` 项目当前 `src/at/at.c` 直接走 `Ql_SendAT()` 进行 AT 命令收发，文档未提及该函数底层串口是否启用硬件流控；如果未来排查 AT 通信丢字符/超时问题，可以参考本文档"流控配置→驱动生效需重新编译内核镜像→重新烧录"的完整链路作为排查方向之一（即流控本身是内核态/设备树层面的配置，应用层无法仅通过 termios 在运行时随意切换，需要确认设备树侧的默认流控设置）。

3. **波特率与协议参数一致性**：文档强调"波特率必须与模块适用的波特率一致"，且默认 115200。`open_dial` 项目 CLAUDE.md 中提到的 AT 串口路径为 `/dev/smd8`，未注明具体波特率，但本文档提示——如果该项目未来涉及对 AT 串口波特率/数据位/停止位/校验位的配置审查，应同时检查内核设备树侧的默认配置（是否与应用层 `termios` 设置匹配），而不仅是应用层参数。

4. **`Ql_SendAT()` 已知缺陷的对照视角**：本项目 CLAUDE.md "已知代码缺陷"中记录了 `src/at/at.c` `Ql_SendAT()` 此前存在的两个 bug（命令尾部 `\r`/`\n` 判断错误导致发送全 NUL、应答拷贝越界）。本 PDF 未涉及 `Ql_SendAT` 或任何应用层 AT 发送函数实现细节（Quectel SDK 层 `sample/uart` 示例使用的是其自有的 `ST_UARTDCB`/底层 UART API，与本项目的 `Ql_SendAT` 是不同代码路径，文档未提供任何可直接复用的实现），因此本文档**不能直接帮助定位或修复** `open_dial` 中 `at.c` 的既有 bug，仅可作为理解 Quectel 平台 UART 一般行为（流控、波特率、设备节点）的背景资料。

5. **总体结论**：本 PDF 是 AG35 系列模块在 QuecOpen SDK 环境下的 UART 外设开发指导（偏硬件/设备树/Linux 驱动层），与 `open_dial`（运行于 EC2x/EG2x 模组、聚焦拨号状态机与 AT 命令应用层逻辑）的**直接代码层面关联较弱**；主要参考价值在于：(a) 理解 Quectel 不同模块系列 UART/AT 通道命名差异（`/dev/smd*` vs `/dev/ttyS*`）的背景知识；(b) 若未来项目扩展到 AG35 平台，需要重新评估 AT 串口打开方式、流控配置及设备树修改流程。**目前 `open_dial` 代码库中未发现任何引用本 PDF 所述设备树文件、UART 设备节点（`/dev/ttyS1` 等）或 `ST_UARTDCB` 结构体的代码**，无直接代码层面关联。

## 七、文档局限性、未说明清楚之处及已知问题

1. **文档自身明确声明的局限性**：文档首页"文档历史"部分写明本文档为**临时版本（Preliminary）**，"其中涉及的模块引脚或接口定义、频段、功能、特性及设计（若有）发生变动的可能性较大，部分参数经过初步验证或仍未验证。变更或补充信息将在后续正式版本中体现。临时版本仅供临时参考使用，若与正式版本存在差异，应以最新版本为准"。即文档自身承认引脚号等参数可能尚未最终验证，使用时需注意核实（例如本次 V1.0.1 修订就是更正了 MAIN_CTS/MAIN_RTS 引脚号，印证了这一风险点确实发生过）。

2. **术语表错误**：附录表 8 中将 "UART" 的英文全称错误地写为 "Inter-Integrated Circuit"（中文"集成电路总线"），这实际是 I2C 的定义，而非 UART（Universal Asynchronous Receiver/Transmitter）。文档未对此自行纠正，属原文本身的错误。

3. **设备树时钟命名与端口号不一致，文档未解释**：蓝牙 UART（`uart3`）节点的 `clocks`/`resets` 字段写的是 `ASR1803_CLK_UART1`；GNSS UART（`uart4`）节点写的是 `ASR1803_CLK_UART3`。两者编号都与各自所属的 UART 节点编号（uart3/uart4）不一致，文档全文未对这种命名错位做任何说明或提示，是否为底层平台历史遗留的命名习惯、还是文档截图笔误，文档没有交代。

4. **主 UART pinctrl 引用名含 "gps_pmx" 前缀，文档未解释**：3.3 节中主 UART（`uart2`，注释为 "Main UART"）设备树片段的 `pinctrl-0 = <&gps_pmx_uart_rxd &gps_pmx_uart_txd>;` 使用了带 "gps" 字样的 pinmux 节点名，与"主 UART"的功能定位不直接对应，文档未说明该命名来源或是否有特殊含义。

5. **示例代码注释与实际配置矛盾，文档未修正**：4.2.2 节"使能硬件流控"步骤中给出的示例代码截图，`.flowctrl = FC_RTSCTS,` 后面的行内注释仍写的是 `//none flow control`（应为硬件流控相关说明），与实际启用的流控模式矛盾。文档原样呈现该截图，未指出或纠正这一注释错误。

6. **未涉及的内容（文档未提及，不应臆测）**：
   - 文档未给出 V1.0.0 → V1.0.1 修订前 MAIN_CTS/MAIN_RTS 的旧引脚号具体数值。
   - 文档未说明四路 UART 各自的硬件 FIFO 深度、最大传输距离、信号电平容差等电气特性细节。
   - 文档未说明"调试 UART 不允许修改功能配置"的底层技术原因（如是否被 bootloader/console 硬编码依赖）。
   - 文档未提供 `ST_UARTDCB` 结构体的完整字段说明（仅展示了 `flowctrl`/`databit`/`stopbit`/`parity`/`baudrate` 五个字段在示例中的赋值，未给出该结构体的完整定义、取值范围或 API 头文件出处）。
   - 文档未提及该 UART 开发指导是否适用于其他 Quectel 模块系列（如本项目所用的 EC2x/EG2x），仅声明适用于 AG35-CET 和 AG35-EUT。
   - 文档未说明 GNSS UART 在"内置 GNSS"模式下，该 UART 节点（`/dev/ttyS3`）虽默认 `status = "okay"`，但其 RX/TX 引脚悬空时，系统内部是否仍有其他方式与内置 GNSS 通信（如是否走其他总线），未作说明。

7. **文档未提及已知 Bug 或勘误列表**：文档中没有专门的"已知问题"章节，唯一可识别的"问题"是修订记录中提到的引脚号修正（间接说明 V1.0.0 中存在错误引脚号，但文档未具体列出错误值）。

<!-- GENERATION_COMPLETE: 2026-06-24 -->
