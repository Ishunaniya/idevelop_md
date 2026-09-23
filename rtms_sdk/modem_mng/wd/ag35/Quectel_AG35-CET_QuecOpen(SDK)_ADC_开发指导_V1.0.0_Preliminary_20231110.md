# Quectel AG35-CET QuecOpen(SDK) ADC 开发指导

## 文档基本信息
- **文档标题**：AG35-CET QuecOpen(SDK) ADC 开发指导（页眉为 "AG35-CET QuecOpen(SDK) ADC 用户指导"）
- **厂商**：上海移远通信技术股份有限公司（Quectel Wireless Solutions Co., Ltd.）
- **适用模块**：LTE Standard 模块系列，AG35-CET
- **方案**：QuecOpen（基于 Linux 的嵌入式开发平台，简化 IoV/车联网应用开发）
- **版本号**：1.0.0（临时版本 / Preliminary）
- **日期**：2023-11-10
- **状态**：临时文件（Preliminary Confidential）
- **总页数**：13 页（正文 12 页编号）
- **作者**：Aurora JIANG
- **运行系统**：OpenWrt（模块端提示符 root@OpenWrt:/tmp#）
- **联系方式**：电话 +86 21 5108 6236；邮箱 info@quectel.com；技术支持 support@quectel.com

### 修订记录
- `-`，2023-11-10，Aurora JIANG，文档创建
- `1.0.0`，2023-11-10，Aurora JIANG，临时版本

### 文档目录结构
1. 引言
2. ADC 接口引脚
3. ADC API 综述（3.1 库文件和头文件、3.2 参考示例、3.3 ql_adc_show、3.3.1 ADC_CHANNEL_E）
4. ADC 编译示例及功能验证（4.1 ADC 编译示例、4.2 ADC 功能验证）
5. 附录 参考文档及术语缩写

---

## 逐章节内容摘要

### 第1章 引言
- AG35-CET 模块支持 QuecOpen 方案。QuecOpen 是基于 Linux 的嵌入式开发平台，可简化 IoV（车联网）应用的软件设计与开发，详细信息参考文档 [1]《Quectel_AG35-CET_QuecOpen_快速开发指导》。
- 本文档适用于 SDK 构建环境的 QuecOpen 方案，主要介绍 AG35-CET 模块的 ADC 功能开发，包含 ADC 接口引脚介绍、ADC 相关 API 函数以及 ADC 编译示例和功能验证。

### 第2章 ADC 接口引脚
- ADC（模数转换器）是将连续变化的模拟信号转换为离散的数字信号的器件，即将真实世界的模拟信号（温度、电压、声音等）转换成更容易储存、处理和发射的数字形式。
- AG35-CET QuecOpen 模块共提供**三路 ADC 接口**，用于测量电压值。
- **表1 用于测试电压的 ADC 接口信息**：

| 引脚名称 | 引脚号 | I/O | 描述 | 备注 |
|---|---|---|---|---|
| ADC0 | 173 | AI | 通用模数转换接口 | 不用则悬空 |
| ADC1 | 175 | AI | 通用模数转换接口 | 不用则悬空 |
| ADC2 | 172 | AI | 通用模数转换接口 | 不用则悬空 |

- **备注**：有关 ADC 引脚及参考电路的详情，请参考文档 [2]《Quectel_AG35-CET_QuecOpen_硬件设计手册》。

### 第3章 ADC API 综述
- 本章介绍与 ADC 功能相关的 API。

#### 3.1 库文件和头文件
- ADC API 的头文件为 `ql_adc.h`，位于 SDK 包 `ql-sysroots/usr/include/ql-sdk/` 目录下。若无特别说明，本文档所述头文件均位于该目录下。
- ADC 应用程序的编写需要依赖库文件 `libql_sdk.so`，位于 `ql-sysroots/usr/lib/` 目录下。

#### 3.2 参考示例
- 模块 SDK 包中提供一套完整的 ADC 编程接口，接口使用示例路径为 `sample/adc/`，用户可参考该示例完成 ADC 应用程序编写。
- `/lib/` 目录包含移远通信提供的 API 接口库（截图命令 `ls ./ql-sysroots/usr/lib/libql_sdk.so` 确认存在）。
- `/include/` 目录包含所有 API 头文件（截图命令 `ls ./ql-sysroots/usr/include/ql-sdk/ql_adc.h` 确认存在）。

#### 3.3 ql_adc_show
- 该函数用于读取指定 ADC 通道的电压值。
- **函数原型**：`int ql_adc_show(ADC_CHANNEL_E qadc)`
- **参数**：`adc`：[In] ADC 通道号，详见 3.3.1 章（ADC_CHANNEL_E 枚举）。
- **返回值**：
  - 函数执行成功：返回 ADC 电压值（单位 mV）。
  - 函数执行失败：返回 -1。

##### 3.3.1 ADC_CHANNEL_E
- ADC 通道号枚举定义：
  ```c
  typedef enum ADC_CHANNEL_ENUM{
      QADC_NONE = 0,
      ADC0 = 1,
      ADC1 = 2,
      ADC2 = 3,
      QADC_END
  }ADC_CHANNEL_E;
  ```
- **成员说明**：
  - `QADC_NONE`：不使用 ADC
  - `ADC0`：ADC0 通道
  - `ADC1`：ADC1 通道
  - `ADC2`：ADC2 通道

### 第4章 ADC 编译示例及功能验证

#### 4.1 ADC 编译示例
- 进入 `sample/adc/` 目录，执行 `make` 命令生成 `adc_test` 可执行文件。
- 截图显示交叉编译过程：使用工具链 `/opt/ql_crosstools/ql-ag35-1806e-gcc-8.4.0-glibc-v1-toolchain/bin/arm-openwrt-linux-gcc`，编译参数含 `-march=armv7-a -mfpu=neon -mfloat-abi=hard`，头文件路径 `-I .../ql-sysroots/usr/include/ql-sdk`，链接库 `-lql_sdk -lql_sys_log -lql_lib_ipc -lql_lib_utils -lpthread -Wall -Wundef`。
- 编译完成后 `ls` 可见生成的 `adc_test`、main.c、main.o、Makefile。

#### 4.2 ADC 功能验证
- 本章节以 ADC0 为例，介绍获取 ADC0 电压值的方法。
- **步骤一**：硬件准备。准备电压值范围为 0~VBAT_BB V 的电源；并将电源的地线与模块的地线相接，电源的正极连接到 ADC0 引脚。
  - **备注**：电源输出的准确电压值可以通过万用表进行测量。
- **步骤二**：将 ADC 编译后生成的可执行文件（以 adc_test 为例）上传至模块：`adb push <adc_test 在上位机路径> <模块内部路径，如 /tmp>`。
- **步骤三**：上传可执行文件后，修改文件权限并执行：
  - `chmod 777 adc_test`
  - `./adc_test`
  - 输出示例：
    ```
    < Quectel OpenLinux: ADC example >
    ADC0 : 1835
    ADC1 : 645
    ADC2 : 655
    ```
  - 其中返回值 `ADC0 : 1835` 即为测量的电压值，单位为 mV。

### 第5章 附录 参考文档及术语缩写
- **表2 参考文档**：
  - [1] Quectel_AG35-CET_QuecOpen_快速开发指导
  - [2] Quectel_AG35-CET_QuecOpen_硬件设计手册
- **表3 术语缩写**：
  - ADC：Analog-to-Digital Converter，模拟数字转换器
  - API：Application Programming Interface，应用程序接口
  - IoV：Internet of Vehicles，车联网
  - SDK：Software Development Kit，软件开发工具包

---

## 关键 API / 命令清单

### API 函数
- `int ql_adc_show(ADC_CHANNEL_E qadc)`
  - 作用：读取指定 ADC 通道的电压值。
  - 参数：`qadc`，ADC 通道号（ADC_CHANNEL_E 枚举：ADC0=1、ADC1=2、ADC2=3，QADC_NONE=0 表示不使用）。
  - 返回值：成功返回电压值（单位 mV，整数）；失败返回 -1。
  - 头文件：`ql_adc.h`（`ql-sysroots/usr/include/ql-sdk/`）；依赖库：`libql_sdk.so`（`ql-sysroots/usr/lib/`）。

### 枚举类型
- `ADC_CHANNEL_E`：ADC 通道号枚举，成员 QADC_NONE(0)/ADC0(1)/ADC1(2)/ADC2(3)/QADC_END。

### Shell / 编译 / 调试命令
- `make`（在 `sample/adc/` 目录）：交叉编译生成 adc_test 可执行文件。
- `adb push <本地路径> <模块路径>`：将可执行文件上传到模块（如 /tmp）。
- `chmod 777 adc_test`：赋予可执行文件可读写执行权限。
- `./adc_test`：运行 ADC 测试程序，打印三路 ADC 电压值（mV）。
- `ls ./ql-sysroots/usr/lib/libql_sdk.so` / `ls ./ql-sysroots/usr/include/ql-sdk/ql_adc.h`：确认库文件和头文件存在。

### 硬件接口（表1）
- ADC0：引脚号 173，AI，通用模数转换接口，不用则悬空。
- ADC1：引脚号 175，AI，通用模数转换接口，不用则悬空。
- ADC2：引脚号 172，AI，通用模数转换接口，不用则悬空。
- 测量电压范围：0 ~ VBAT_BB V（输入电压不应超过模块基带供电电压）。

---

## 示例代码说明

1. **ADC 通道枚举（3.3.1）**：
   ```c
   typedef enum ADC_CHANNEL_ENUM{
       QADC_NONE = 0,   // 不使用 ADC
       ADC0 = 1,        // ADC0 通道
       ADC1 = 2,        // ADC1 通道
       ADC2 = 3,        // ADC2 通道
       QADC_END
   }ADC_CHANNEL_E;
   ```
   说明：调用 `ql_adc_show(ADC0)` 即读取 ADC0 通道电压。枚举从 1 开始编号，0 保留为"不使用"，QADC_END 作为边界哨兵（可用于参数合法性校验）。

2. **典型调用方式（依据 ql_adc_show 原型与示例输出推断的用法）**：
   ```c
   int v0 = ql_adc_show(ADC0);   // 返回 mV，例 1835
   int v1 = ql_adc_show(ADC1);   // 例 645
   int v2 = ql_adc_show(ADC2);   // 例 655
   if (v0 < 0) { /* 读取失败处理 */ }
   ```
   说明：函数直接返回毫伏整数值，无需额外转换。失败返回 -1，调用方应判断返回值是否 < 0。文档示例程序 adc_test 即依次打印三路通道值。

3. **交叉编译命令（4.1，截图还原）**：
   ```sh
   ql@ubuntu:~/sdk/.../sample/adc$ make
   /opt/ql_crosstools/ql-ag35-1806e-gcc-8.4.0-glibc-v1-toolchain/bin/arm-openwrt-linux-gcc \
       -o main.o -c main.c -march=armv7-a -mfpu=neon -mfloat-abi=hard \
       -I .../ql-sysroots/usr/include -I .../ql-sysroots/usr/include/ql-sdk -Wall -Wundef
   arm-openwrt-linux-gcc -o adc_test main.o -march=armv7-a -mfpu=neon -mfloat-abi=hard \
       -L .../ql-sysroots/usr/lib -lql_sdk -lql_sys_log -lql_lib_ipc -lql_lib_utils -lpthread
   ```
   说明：编译分两步——先把 main.c 编为 main.o（指定 ARMv7-A、NEON、硬浮点 ABI 及头文件目录），再链接生成 adc_test（链接 `libql_sdk` 及日志/IPC/utils/pthread 等依赖库）。注意要使用 AG35-1806e 专用 GCC 8.4.0 工具链，且 `-march/-mfpu/-mfloat-abi` 必须与模块架构一致，否则运行时会报非法指令。

4. **功能验证流程（4.2）**：
   ```sh
   # 上位机
   adb push adc_test /tmp
   # 模块端
   root@OpenWrt:/tmp# chmod 777 adc_test
   root@OpenWrt:/tmp# ./adc_test
   < Quectel OpenLinux: ADC example >
   ADC0 : 1835
   ADC1 : 645
   ADC2 : 655
   ```
   说明：硬件上需将外部电源（0~VBAT_BB V）正极接到对应 ADC 引脚、地线与模块共地。程序输出三路通道的电压（mV）。验证时可用万用表测量电源实际电压与 ADC0 读数（1835mV）比对，确认读数准确。注意事项：输入电压不可超过 VBAT_BB，否则可能损坏引脚。

<!-- GENERATION_COMPLETE: 2026-06-25_03:45 -->
