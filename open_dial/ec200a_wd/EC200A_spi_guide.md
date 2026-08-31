# EC200A-CN(TA) QuecOpen(SDK) SPI 开发指导

> **文档版本**：V1.0.1 Preliminary Confidential  
> **发布日期**：2024-04-09  
> **适用平台**：EC200A-CN(TA)（QuecOpen 模式）  
> **关键字**：SPI、spidev、quec_spi_chn、标准4线、扩展6线、主从模式

---

## 目录

- [1. 引言](#1-引言)
- [2. SPI 硬件接口](#2-spi-硬件接口)
- [3. SPI SDK API](#3-spi-sdk-api)
  - [3.1 头文件](#31-头文件)
  - [3.2 函数概览](#32-函数概览)
  - [3.3 函数详解](#33-函数详解)
    - [3.3.1 ql_spi_init](#331-ql_spi_init)
    - [3.3.2 ql_spi_write_read](#332-ql_spi_write_read)
    - [3.3.3 ql_spi_deinit](#333-ql_spi_deinit)
    - [3.3.4 SPI_MODE 枚举](#334-spi_mode-枚举)
    - [3.3.5 SPI 速率常量](#335-spi-速率常量)
- [4. DTS 配置](#4-dts-配置)
  - [4.1 SPI 主模式 DTS 配置](#41-spi-主模式-dts-配置)
  - [4.2 SPI 从模式 DTS 配置](#42-spi-从模式-dts-配置)
- [5. 内核驱动](#5-内核驱动)
  - [5.1 标准 4 线 SPI 设备驱动](#51-标准-4-线-spi-设备驱动)
  - [5.2 扩展 6 线 SPI 设备驱动](#52-扩展-6-线-spi-设备驱动)
  - [5.3 同时加载两路 SPI 设备驱动](#53-同时加载两路-spi-设备驱动)
- [6. 编译示例及功能验证](#6-编译示例及功能验证)
  - [6.1 编译步骤](#61-编译步骤)
    - [6.1.1 标准 4 线 SPI 编译步骤](#611-标准-4-线-spi-编译步骤)
    - [6.1.2 扩展 6 线 SPI 编译步骤](#612-扩展-6-线-spi-编译步骤)
    - [6.1.3 SPI 从模式](#613-spi-从模式)
  - [6.2 功能验证](#62-功能验证)
    - [6.2.1 4 线 SPI 功能验证](#621-4-线-spi-功能验证)
    - [6.2.2 6 线 SPI 功能验证](#622-6-线-spi-功能验证)
    - [6.2.3 SPI 从模式功能验证](#623-spi-从模式功能验证)
    - [6.2.4 复用引脚功能验证](#624-复用引脚功能验证)
- [7. 驱动调试](#7-驱动调试)
- [附录 参考文档及术语缩写](#附录-参考文档及术语缩写)

---

## 1. 引言

移远通信 EC200A-CN(TA) 模块支持 QuecOpen® 方案。QuecOpen® 是开源的基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。

本文档主要介绍 EC200A-CN(TA) 模块的 SPI 开发指导，包括 SPI 硬件接口、SDK API、DTS 配置、内核驱动加载、编译示例及功能验证等内容。

模块支持以下两种 SPI 工作方式：

1. **标准 4 线 SPI**：使用通用 `spidev.ko` 驱动，通过 `/dev/spidev1.0` 等设备节点操作。
2. **扩展 6 线 SPI**：使用 Quectel 专有的 `quec_spi_chn.ko` 驱动，增加了 `SPI_MRDY`（模块准备好）和 `SPI_SRDY`（从机准备好）两根握手信号线。

---

## 2. SPI 硬件接口

EC200A-CN(TA) 模块的 SPI 接口引脚说明如下：

**表 2：GPIO 描述**

| 引脚名称 | GPIO 编号 | 方向 | 说明 |
|---------|----------|------|------|
| SPI_CLK | — | 输出（主模式） | SPI 时钟信号（SCLK） |
| SPI_CS | — | 输出（主模式） | SPI 片选信号 |
| SPI_DIN / MOSI | — | 输出（主模式） | 主机发送数据线 |
| SPI_DOUT / MISO | — | 输入（主模式） | 主机接收数据线 |
| SPI_MRDY | GPIO 120 | 输出 | 模块准备好信号（仅扩展 6 线使用） |
| SPI_SRDY | GPIO 19 / GPIO 122 | 输入 | 从机准备好信号（仅扩展 6 线使用） |

> **说明**：SPI 接口对应的模块引脚编号请参考 EVB 原理图或模块 AT 指令手册。

---

## 3. SPI SDK API

### 3.1 头文件

SPI API 的头文件为 `ql_spi.h`，位于 `ql-sysroots/usr/include/ql-sdk` 目录下。

SPI API 的使用可参考 SDK 示例程序，示例程序 `main.c` 位于 `sample/spi/` 目录下（标准 4 线），扩展 6 线示例位于 `sample/spi_six_line/` 目录下。

---

### 3.2 函数概览

**表 1：函数概览**

| 函数 | 说明 |
|------|------|
| `ql_spi_init()` | 初始化 SPI 设备，返回文件描述符 |
| `ql_spi_write_read()` | 全双工 SPI 数据读写 |
| `ql_spi_deinit()` | 去初始化 SPI 设备，关闭文件描述符 |

---

### 3.3 函数详解

#### 3.3.1 ql_spi_init

该函数用于初始化 SPI 设备，并进行 SPI 数据传输的基本参数配置。

**函数原型**

```c
int ql_spi_init(const char *device, SPI_MODE spimode, uint8_t bits, uint32_t speed)
```

**参数**

| 参数 | 方向 | 类型 | 说明 |
|------|------|------|------|
| `device` | [In] | `const char *` | SPI 设备路径，如 `/dev/spidev1.0`、`/dev/spi1_0_0` |
| `spimode` | [In] | `SPI_MODE` | SPI 工作模式，详见 [3.3.4 SPI_MODE 枚举](#334-spi_mode-枚举) |
| `bits` | [In] | `uint8_t` | 每字位数，通常为 `8` |
| `speed` | [In] | `uint32_t` | 最大时钟频率（Hz），详见 [3.3.5 SPI 速率常量](#335-spi-速率常量) |

**返回值**

| 返回值 | 说明 |
|--------|------|
| ≥ 0（文件描述符 fd） | 函数执行成功 |
| < 0 | 函数执行失败 |

**示例**

```c
int fd;
fd = ql_spi_init("/dev/spidev1.0", SPIMODE0, 8, S_6_5M);
if (fd < 0) {
    printf("SPI init failed\n");
}
```

---

#### 3.3.2 ql_spi_write_read

该函数用于全双工 SPI 数据读写（同时发送和接收）。

**函数原型**

```c
int ql_spi_write_read(int fd, uint8_t *wbuf, uint8_t *rbuf, uint32_t len)
```

**参数**

| 参数 | 方向 | 类型 | 说明 |
|------|------|------|------|
| `fd` | [In] | `int` | `ql_spi_init()` 返回的文件描述符 |
| `wbuf` | [In] | `uint8_t *` | 发送数据缓冲区指针 |
| `rbuf` | [Out] | `uint8_t *` | 接收数据缓冲区指针 |
| `len` | [In] | `uint32_t` | 读写数据字节数 |

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败 |

**示例**

```c
uint8_t writebuf[1024];
uint8_t readbuf[1024];
for (int i = 0; i < 1024; i++)
    writebuf[i] = i % 256;
ql_spi_write_read(fd, writebuf, readbuf, 1024);
```

---

#### 3.3.3 ql_spi_deinit

该函数用于去初始化 SPI 设备，关闭文件描述符，释放相关资源。

**函数原型**

```c
int ql_spi_deinit(int fd)
```

**参数**

| 参数 | 方向 | 类型 | 说明 |
|------|------|------|------|
| `fd` | [In] | `int` | `ql_spi_init()` 返回的文件描述符 |

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| -1 | 函数执行失败 |

---

#### 3.3.4 SPI_MODE 枚举

SPI 工作模式枚举定义如下：

```c
typedef enum
{
    SPIMODE0 = SPI_MODE_0,
    SPIMODE1 = SPI_MODE_1,
    SPIMODE2 = SPI_MODE_2,
    SPIMODE3 = SPI_MODE_3,
} SPI_MODE;
```

SPI 共支持 4 种工作模式：`SPI_MODE_0`、`SPI_MODE_1`、`SPI_MODE_2`、`SPI_MODE_3`，其值为相位（CPHA 0x01）和极性（CPOL 0x02）的按位或。

- **时钟极性（CPOL）**：定义 SPI 总线空闲时，时钟信号 SCLK 的电平（1：SCLK 为高电平；0：SCLK 为低电平）。
- **时钟相位（CPHA）**：定义 SPI 数据采样是在 SCLK 的第几个边沿（0：第一个边沿开始；1：第二个边沿开始）。

| 模式 | CPOL | CPHA |
|------|------|------|
| SPI_MODE_0 | 0 | 0 |
| SPI_MODE_1 | 0 | 1 |
| SPI_MODE_2 | 1 | 0 |
| SPI_MODE_3 | 1 | 1 |

---

#### 3.3.5 SPI 速率常量

SPI 时钟速率常量（单位：Hz）：

| 常量 | 值（Hz） | 说明 |
|------|---------|------|
| `S_812K` | 812500 | 812.5 KHz |
| `S_1M` | 1000000 | 1 MHz |
| `S_1_625M` | 1625000 | 1.625 MHz |
| `S_3_25M` | 3250000 | 3.25 MHz |
| `S_6_5M` | 6500000 | 6.5 MHz（默认） |
| `S_13M` | 13000000 | 13 MHz |
| `S_26M` | 26000000 | 26 MHz |

> **注意**：实际支持的最大值由 SPI 控制器的配置决定，与理论最大值可能不同。内核模块加载时通过 `maxspeed` 参数指定，支持值为：812500、1000000、1625000、3250000、6500000、13000000 和 26000000。

---

## 4. DTS 配置

### 4.1 SPI 主模式 DTS 配置

SPI 主模式的 DTS 配置在文件 `asr1803-p401.dts`（或同类 DTS 文件）中。

**表 3：SPI 主模式 DTS 配置参数**

| 参数 | 说明 |
|------|------|
| `compatible` | 固定为 `"asr,asr-spi"` |
| `spi-max-frequency` | SPI 最大时钟频率，如 `<13000000>` |
| `asr,ssp-id` | SPI 控制器编号（如 `<1>`），对应 busnum 参数，**必须配置** |
| `asr,ssp-enhancement` | 使能 SSP 增强特性 |
| `asr,ssp-disable-frame` | 禁用帧模式 |
| `/* asr,ssp-slave-mode; */` | 注释掉表示主模式（去掉注释启用从模式） |
| `asr,slave-rxtimer-to-ms` | 从模式接收超时（ms），如 `<0>` |
| `asr,spi-master-rxto` | 主模式接收超时，如 `<8000>` |
| `asr,spi-slave-rxto` | 从模式接收超时，如 `<262144>` |
| `asr,spi-pio-interval` | PIO 模式间隔，如 `<5>` |
| `clocks` | 时钟源，如 `<&soc_clocks ASR1803_CLK_SSP0>` |
| `status` | `"okay"` 启用，`"disabled"` 禁用 |

**SPI 控制器编号（busnum）由 DTS 中 `asr,ssp-id` 决定：**

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
    asr,ssp-id = <1>;          /* busnum=1 */
    interrupts = <3>;
    asr,ssp-enhancement;
    asr,ssp-disable-frame;
    /* asr,ssp-slave-mode; */
    /* asr,slave-rxtimer-to-ms = <0>; */
    /* asr,spi-hold-frame-low; */
    /* asr,spi-master-rxto = <8000>; */
    /* asr,spi-slave-rxto = <262144>; */
    /* asr,spi-pio-interval = <5>; */
    clocks = <&soc_clocks ASR1803_CLK_SSP0>;
    status = "disabled";
};
```

> **备注**：`asr,ssp-id = <1>` 对应加载驱动时 `busnum=1`。此参数必须传入，否则 SPI 设备驱动会注册失败。

---

### 4.2 SPI 从模式 DTS 配置

若需将模块配置为 SPI 从模式，将 DTS 中 `/* asr,ssp-slave-mode; */` 的注释去掉即可启用从模式。

**表 4：SPI 从模式相关 DTS 参数**

| 参数 | 说明 | 示例值 |
|------|------|-------|
| `asr,ssp-slave-mode` | 启用从模式（去掉注释） | — |
| `asr,slave-rxtimer-to-ms` | 从模式接收超时时间（ms） | `<0>` |
| `asr,spi-slave-rxto` | 从模式接收超时计数 | `<262144>` |

**表 5：SPI 设备信息**

| 参数 | 描述 |
|------|------|
| `modalias` | SPI 驱动匹配名，主模式为 `"spidev"`，从模式为 `"spidev1"` |
| `.mode` | SPI 工作模式 |

---

## 5. 内核驱动

### 5.1 标准 4 线 SPI 设备驱动

标准 4 线 SPI 使用内核通用 `spidev.ko` 驱动（设备文件：`/dev/spidev1.0`）。

驱动文件 `spidev.c` 位于 `ql-ol-kernel/drivers/spi/` 路径下。编译后的内核模块存放于 rootfs 的 `/lib/modules/5.4.195/` 路径下。执行 `insmod` 命令即可加载驱动。

**加载示例：**

```bash
insmod /lib/modules/5.4.195/spidev.ko busnum=1 chipselect=0 maxspeed=6500000
```

**加载成功验证：**

```bash
ls /dev/spi*
# 输出: /dev/spidev1.0
```

```bash
lsmod
# 输出中可以看到 spidev 字段：
# spidev          16384  0
```

> **备注**：不使用通用 SPI 设备驱动时，用户需从外设设备供应商处获取设备驱动和配置手册，并按照配置手册自行进行相关配置。

---

### 5.2 扩展 6 线 SPI 设备驱动

驱动文件 `quec_spi_chn.c` 位于 `ql-ol-kernel/drivers/spi/` 路径下。编译后的内核模块存放于 rootfs 的 `/lib/modules/5.4.195/` 路径下。直接通过执行 `insmod` 命令传入参数比设备树传参方式更为灵活。

**表 6：内核模块加载时支持的参数**

| 参数 | 描述 | 默认值 |
|------|------|-------|
| `busnum` | SPI 控制器编号，由 DTS 中 `asr,ssp-id` 决定（如 `ssp0` 对应编号 1）。**此参数必须传入**，否则 SPI 设备驱动会注册失败。 | 无 |
| `chipselect` | 片选；取值支持 0、1、2 和 3。**此参数必须传入**，否则 SPI 设备驱动会注册失败。 | 无 |
| `spimode` | SPI 工作模式。默认值：`SPI_MODE_3`。SPI 共支持 4 种工作模式：`SPI_MODE_0`、`SPI_MODE_1`、`SPI_MODE_2`、`SPI_MODE_3`，其值为相位（CPHA 0x01）和极性（CPOL 0x02）的按位或。用户在执行 `insmod` 命令传入参数时可以进行修改。 | `SPI_MODE_3` |
| `maxspeed` | 最大时钟频率；可选参数。默认值：6500000；单位：Hz。该参数支持的值包括：812500、1000000、1625000、3250000、6500000、13000000 和 26000000；实际支持的最大值由 SPI 控制器的配置决定，与理论最大值可能不同。 | 6500000 |
| `bufsiz` | 缓存大小；可选参数。默认值：4096，单位：byte。用户可根据每次传输的数据量设置传输队列中每个 transfer 的大小。 | 4096 |
| `gpiomodemready` | SPI_MRDY 引脚。默认值：120。对应模块的引脚为 GPIO_120。 | 120 |
| `gpiomcuready` | SPI_SRDY 引脚。默认值：122。对应模块的引脚为 GPIO_122。 | 122 |

**加载示例：**

```bash
insmod /lib/modules/5.4.195/quec_spi_chn.ko busnum=1 chipselect=0 spimode=0 maxspeed=19200000 gpiomcuready=120 gpiomodemready=122
```

**加载成功验证（使用 `lsmod` 可看到 `quec_spi_chn` 字段）：**

```
pppox         16384  1  pppoe
pppox         16384  1  pppoe
quec_spi_chn  16384  0
rfcomm        32768  0
slhc          16384  1  ppp_generic
tunnel4       16384  2  xfrm4_tunnel,ipip
```

> **备注**：不使用通用 SPI 设备驱动时，用户需从外设设备供应商处获取设备驱动和配置手册，并按照配置手册自行进行相关配置。

---

### 5.3 同时加载两路 SPI 设备驱动

按照如下步骤同时加载两路标准 4 线 SPI 设备驱动。

**步骤一**：在 `ql-ol-kernel/drivers/spi/` 目录下复制 `spidev.c` 文件并重命名为 `spidev1.c`。修改 `spidev1.c` 文件如下：

```c
/* 修改设备主设备号，避免冲突 */
#define SPIDEV_MAJOR    154    /* assigned */
#define N_SPI_MINORS     32    /* ... up to 256 */

static DECLARE_BITMAP(minors, N_SPI_MINORS);

/* 修改 SPI 驱动名称 */
static struct spi_driver spidev_spi_driver = {
    .driver = {
        .name =  "spidev1",    /* 修改为 spidev1 */
        .of_match_table = of_match_ptr(spidev_dt_ids),
        .acpi_match_table = ACPI_PTR(spidev_acpi_ids),
    },
    .probe =  spidev_probe,
    .remove = spidev_remove,
    /* NOTE: suspend/resume methods are not necessary here */
};

/* 修改初始化函数中的设备类名 */
static int __init spidev_init(void)
{
    int status;
    BUILD_BUG_ON(N_SPI_MINORS > 256);
    status = register_chrdev(SPIDEV_MAJOR, "spi1", &spidev_fops);  /* 修改 */
    if (status < 0)
        return status;
    spidev_class = class_create(THIS_MODULE, "spidev1");  /* 修改 */
    if (IS_ERR(spidev_class)) {
        unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
        return PTR_ERR(spidev_class);
    }
    status = spi_register_driver(&spidev_spi_driver);
    if (status < 0) {
        class_destroy(spidev_class);
        unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
    }
    //jaydev add ,20220426: Users by parameter tests
    if (busnum != -1 && chipselect != -1) {
        struct spi_board_info chip = {
            .modalias = "spidev1",    /* 修改 */
            .mode     = spimode,
            ...
        };
    }
    ...
}
```

**步骤二**：修改 `ql-ol-kernel/drivers/spi/` 目录下的 `Makefile` 文件如下：

```makefile
# small core, mostly translating board-specific
# config declarations into driver model code
obj-$(CONFIG_SPI_MASTER)          += spi.o
obj-$(CONFIG_SPI_MEM)             += spi-mem.o
obj-$(CONFIG_SPI_SPIDEV)          += spidev.o
obj-$(CONFIG_SPI_SPIDEV)          += spidev1.o    # 新增
#2022/05/05 jayden add, compatible with spi 6-line driver
obj-$(CONFIG_SPI_SPIDEV_6LINE)    += quec_spi_chn.o
obj-$(CONFIG_SPI_LOOPBACK_TEST)   += spi-loopback-test.o
```

**步骤三**：编译 SDK。将生成的 `ql-ol-kernel/drivers/spi/spidev1.ko` 文件 push 到文件系统即可。

---

## 6. 编译示例及功能验证

模块 SDK 包中包含了 SPI 功能的编译示例 `main.c`，该示例路径为 `sample/spi/`。

### 6.1 编译步骤

#### 6.1.1 标准 4 线 SPI 编译步骤

**步骤一**：初始化 SPI 设备，并进行 SPI 数据传输。如下所示，示例以工作模式 `SPI_MODE_0`、8 位字长、6.5 MHz 最大时钟频率初始化 SPI 设备，并向设备写入 1024 个字节，同时读取 1024 字节；用户可根据实际应用需求编译相关信息。

```c
int main(int argc, char *argv[])
{
    int fd;
    int i;
    uint8_t writebuf[1024];
    uint8_t readbuf[1024];

    fd = ql_spi_init(device, SPIMODE0, 8, S_6_5M);
    for (i = 0; i < 1024; i++)
        writebuf[i] = i % 256;

    ql_spi_write_read(fd, writebuf, readbuf, 1024);

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

**步骤二**：进入 `sample/spi/` 目录，执行 `make` 命令生成可执行文件 `spi_test`。

```bash
cd ~/ql-ol-extsdk-ec200acntar03a01m2g_ocpu/sample/spi
make
# 编译成功后生成: main.c  main.o  Makefile  spi_test
```

---

#### 6.1.2 扩展 6 线 SPI 编译步骤

进入 `sample/spi_six_line/` 目录，执行 `make` 命令生成可执行文件 `example_six_line_spi`。

```bash
cd ~/ql-ol-extsdk-ec200acntar03a01m2g_ocpu/sample/spi_six_line
make
# 编译成功后生成: example_six_line_spi  main.c  main.o  Makefile
```

---

#### 6.1.3 SPI 从模式

若相连的 MCU 不支持从模式且需要使用 SPI 时，可将通过修改设备树文件 `asr1803-p401.dts` 将模块配置为从模式，详见 **第 4.1 章**。

配置两个模块分别为 SPI 主模式和 SPI 从模式并连接（在两个 LTE OPEN EVB 上）：

- EVB1 的 GPIO_23（J0201）连接 EVB2 的 GPIO_23（J0201）；
- EVB1 的 GPIO_22（J0201）连接 EVB2 的 GPIO_22（J0201）；
- EVB1 的 GPIO_21（J0201）连接 EVB2 的 GPIO_20（J0201）；
- EVB1 的 GPIO_20（J0201）连接 EVB2 的 GPIO_21（J0201）；
- 连接两块 EVB 的 GND 引脚。

硬件连接示意图如下所示（主机 DIN 连接从机 DOUT，主机 DOUT 连接从机 DIN，需交叉连接）：

```
主机（Master）         从机（Slave）
SPI_CLK     ───────→  SPI_CLK
SPI_CS      ───────→  SPI_CS
SPI_DIN     ───────→  SPI_DOUT
SPI_DOUT    ←───────  SPI_DIN
GND         ───────→  GND
```

**图 4：硬件连接示意图**

---

### 6.2 功能验证

#### 6.2.1 4 线 SPI 功能验证

参考 **第 6.1 章**编译并生成可执行文件 `spi_test` 后，可参考如下步骤完成功能验证。

**步骤一**：执行如下命令上传 `spi_test` 至模块。

```bash
adb push <spi_test在上位机路径> <模块内部路径，如/data>
```

**步骤二**：执行如下命令修改文件权限。

```bash
chmod 777 spi_test
```

**步骤三**：在 EVB 正面短接 J0201 的 GPIO_20 和 GPIO_21（实现环路测试）。

**步骤四**：执行如下命令动态加载 SPI 通用设备驱动。

```bash
insmod /lib/modules/5.4.195/spidev.ko busnum=1 chipselect=0 maxspeed=6500000
```

**步骤五**：执行 `ls /dev/spi*` 查看 `spidev1.0` 设备是否生成。若返回 `/dev/spidev1.0` 表示已生成 `spidev1.0` 设备。

```bash
/# ls /dev/spi*
/dev/spidev1.0
```

**步骤六**：执行 `spi_test`。若收到的数据与发出的数据一致，则 SPI 编译成功，功能验证通过。

执行结果示例（收到数据与发出数据一致，即 0x00～0xFF 循环）：

```
< open(/dev/spidev1.0, O_RDWR)=3 >
spi mode: 0x0
bits per word: 8
max speed    : 6500000 Hz (6500 KHz)
00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F
...
E0 E1 E2 E3 E4 E5 E6 E7 E8 E9 EA EB EC ED EE EF F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 FA FB FC FD FE FF
```

---

#### 6.2.2 6 线 SPI 功能验证

扩展 6 线 SPI 的源码为 `ql-ol-kernel/drivers/spi/quec_spi_chn.c`。参考 **第 6.1.2 章**编译完成并生成可执行文件 `example_six_line_spi` 后，可参考如下步骤完成功能验证。

**步骤一**：执行如下命令上传 `example_six_line_spi` 至模块。

```bash
adb push <example_six_line_spi在上位机路径> <模块内部路径，如/data/>
```

**步骤二**：执行如下命令修改文件权限。

```bash
chmod 777 example_six_line_spi
```

**步骤三**：确认 GPIO_19（对应模块的 GPIO3）与 GPIO_118（对应模块的 GPIO4）未被其他软件占用，在 LTE OPEN EVB 上：
- 短接 J0201 的 GPIO_20 和 GPIO_21；
- 短接 J0203 的 GPIO_4 和 GPIO_5。

**步骤四**：执行如下命令动态加载六线 SPI 设备驱动。

```bash
insmod /lib/modules/5.4.195/quec_spi_chn.ko busnum=1 chipselect=0 gpiomcuready=19 gpiomodemready=118
```

**步骤五**：执行 `ls /dev/spi*` 查看 `spi1_0_0` 设备是否生成。若返回 `/dev/spi1_0_0` 表示已生成 `spi1_0_0` 设备。

```bash
/# ls /dev/spi*
/dev/spi1_0_0  /dev/spi1_0_2  /dev/spi1_0_4  /dev/spi1_0_6
/dev/spi1_0_1  /dev/spi1_0_3  /dev/spi1_0_5  /dev/spi1_0_7
```

**步骤六**：执行 `example_six_line_spi`。若收到的数据与发出的数据一致，则 SPI 编译成功，功能验证通过。

执行结果示例（每次读取 16 字节，连续 3 次）：

```
read 16 bytes
0 1 2 3 4 5 6 7 8 9 a b c d e f
read 16 bytes
0 1 2 3 4 5 6 7 8 9 a b c d e f
read 16 bytes
0 1 2 3 4 5 6 7 8 9 a b c d e f
```

---

#### 6.2.3 SPI 从模式功能验证

参考 **第 6.1 章**编译并生成可执行文件 `spi_test` 后，可参考如下步骤在主机和从机分别执行完成功能验证。

**步骤一**：执行如下命令上传 `spi_test` 至模块。

```bash
adb push <spi_test在上位机路径> <模块内部路径，如/tmp/>
```

**步骤二**：执行如下命令修改文件权限。

```bash
chmod 777 spi_test
```

**步骤三**：执行如下命令动态加载 SPI 通用设备驱动。

```bash
insmod /lib/modules/5.4.195/spidev.ko busnum=1 chipselect=0 maxspeed=6500000
```

**步骤四**：在**从机**执行 `spi_test`。执行后，从机进入阻塞状态等待主机发起传输。如下图所示：

```
root@OpenWrt:/tmp# chmod 777 spi_test
root@OpenWrt:/tmp# insmod spidev.ko busnum=1 chipselect=0 spimode=0 maxspeed=19200008
root@OpenWrt:/tmp#
root@OpenWrt:/tmp# ./spi_test
< open(/dev/spidev1.0, O_RDWR)=3 >
spi mode: 0x0
bits per word: 8
max speed    : 6500000 Hz (6500 KHz)
（阻塞等待主机传输...）
```

**步骤五**：在**主机**执行 `spi_test`。执行后，主从机分别打印收发数据，若打印的收发数据一致，表示功能验证通过。

---

#### 6.2.4 复用引脚功能验证

**步骤一**：进入 `sample/spi/` 目录，修改 `main.c` 文件中如下操作节点为 `/dev/spidev2.0` 或 `/dev/spidev3.0`，执行 `make` 命令生成可执行文件 `spi_test`。

```c
#define device "/dev/spidev1.0"    /* 修改为 spidev2.0 或 spidev3.0 */
```

**步骤二**：执行如下命令上传 `spi_test` 至模块。

```bash
adb push <spi_test在上位机路径> <模块内部路径，如/tmp/>
```

**步骤三**：执行如下命令修改文件权限。

```bash
chmod 777 spi_test
```

**步骤四**：执行如下任一命令动态加载 SPI 通用设备驱动。

```bash
insmod /lib/modules/5.4.195/spidev.ko busnum=2 chipselect=0 maxspeed=6500000
insmod /lib/modules/5.4.195/spidev.ko busnum=3 chipselect=0 maxspeed=6500000
```

**步骤五**：执行 `spi_test`。若收到的数据与发出的数据一致，则 SPI 编译成功，功能验证通过。

---

## 7. 驱动调试

本章主要介绍 SPI 驱动的调试方法，帮助用户定位在模块运行期间出现的意外问题。

**步骤一**：确认 SPI 总线是否使能。若存在 `spi1` 文件夹，表示 SPI 总线已经使能：

```bash
root@OpenWrt:~#
root@OpenWrt:~# ls /sys/devices/platform/soc/d4000000.apb/asr-spi.0/spi_master
spi1
root@OpenWrt:~#
```

**步骤二**：执行如下命令，确认 SPI 引脚配置是否成功：

```bash
cat /sys/kernel/debug/pinctrl/d401e000.pinmux-pinctrl-single/pinmux-pins
```

若当前配置的 GPIO 已注册在 SPI 下，表示 SPI 引脚配置成功。示例输出：

```
pin 86  (PIN86 ): (MUX UNCLAIMED) (GPIO UNCLAIMED)
pin 87  (PIN87 ): (MUX UNCLAIMED) (GPIO UNCLAIMED)
pin 88  (PIN88 ): asr-spi.0 (GPIO UNCLAIMED) function ssp0_pmx_func group ssp0_pmx_func
pin 89  (PIN89 ): asr-spi.0 (GPIO UNCLAIMED) function ssp0_pmx_func group ssp0_pmx_func
pin 90  (PIN90 ): asr-spi.0 (GPIO UNCLAIMED) function ssp0_pmx_func group ssp0_pmx_func
pin 91  (PIN91 ): asr-spi.0 (GPIO UNCLAIMED) function ssp0_pmx_func group ssp0_pmx_func
```

**步骤三**：打开 SPI 调试日志。一般通过 kernel log 即可查看 SPI 相关错误，通过现有的错误即可分析相关问题。

**步骤四**：若传输过程中出现数据异常且硬件连接无异常，上述步骤均操作正确，需在 SPI 总线调试日志 `ql-ol-kernel/drivers/spi/spi-asr.c` 文件中打开 `/* #define CONFIG_ASR_SSP_DEBUG 1 */` 注释，复现问题，并提交内核日志至移远通信技术支持进行问题分析。

```c
#define TIMOUT_DFLT         8000
#define TIMOUT_DFLT_SLAVE   10000
#define SLAVE_RX_TIMER_MS   1000

static BLOCKING_NOTIFIER_HEAD(removed_notifier_list);

/* #define CONFIG_ASR_SSP_DEBUG 1 */   /* 去掉注释启用调试 */

static bool asr_spi_txfifo_full(const struct spi_driver_data *drv_data)
{
    return !(asr_spi_read(drv_data, STATUS) & STATUS_TNF);
}
```

---

## 附录 参考文档及术语缩写

**表 参考文档**

| 文档名称 |
|---------|
| [1] Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导 |

**表 术语缩写**

| 缩写 | 英文描述 | 中文描述 |
|------|---------|---------|
| API | Application Programming Interface | 应用编程接口 |
| CS | Chip Select | 片选 |
| CPHA | Clock Phase | 时钟相位 |
| CPOL | Clock Polarity | 时钟极性 |
| DTS | Device Tree Source | 设备树源文件 |
| GPIO | General Purpose Input/Output | 通用输入/输出 |
| MRDY | Modem Ready | 调制解调器准备好 |
| MISO | Master In Slave Out | 主机输入从机输出 |
| MOSI | Master Out Slave In | 主机输出从机输入 |
| SDK | Software Development Kit | 软件开发工具包 |
| SCLK | Serial Clock | 串行时钟 |
| SPI | Serial Peripheral Interface | 串行外设接口 |
| SRDY | Slave Ready | 从机准备好 |
