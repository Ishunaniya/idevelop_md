# EC200A-CN(TA) QuecOpen ADC 用户指导 — 完整分析

> 本文是对官方 PDF《Quectel_EC200A-CN(TA)_QuecOpen_ADC_用户指导_V1.0.0_Preliminary_20220711.pdf》的逐页完整转录与整理，**不遗漏任何技术点**（含封面元信息、法律声明、文档历史、目录、全部表格、API 定义、终端截图命令与输出、所有"备注"提示）。

---

## 0. 文档元信息（封面）

| 项目 | 内容 |
|------|------|
| 标题 | **EC200A-CN(TA) QuecOpen ADC 用户指导** |
| 适用系列 | **LTE Standard 模块系列** |
| 版本 | **1.0.0** |
| 日期（封面） | 2021-11-25 |
| 状态 | **临时文件**（Preliminary / Not Checked，未定稿草稿） |
| PDF 作者(属性) | Paul.Du（创建工具 Microsoft Word 2019） |
| PDF 生成日期 | 2022-07-13 |
| 文档总页数 | 13 页 |

> 全文带 "Preliminary Document / Not Checked" 水印，表明这是**未经最终校验的预发布版本**。

---

## 1. 法律与声明（第 1~2 页，要点转录）

### 1.1 公司与联系方式
- 公司全称：**上海移远通信技术股份有限公司**（简称"移远通信" / Quectel Wireless Solutions Co., Ltd.）
- 地址：上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼，邮编 200233
- 电话：+86 21 5108 6236　　邮箱：info@quectel.com
- 销售支持：http://www.quectel.com/cn/support/sales.htm
- 技术支持：http://www.quectel.com/cn/support/technical.htm　邮箱：support@quectel.com

### 1.2 前言
移远提供该文档以支持客户产品设计；客户须按文档规范、参数设计产品。参考设计仅作示例，客户应使用独立分析与判断。文档及服务在"**可用（as-is）**"基础上提供，移远可在不事先通知情况下随时增加、修改或重述本文档。

### 1.3 使用和披露限制
- **许可协议**：未经移远特别授权，接收方须对所接收的硬软件、材料和文档内容保密，仅用于本项目实施与开展。
- **版权声明**：移远及第三方对受版权保护资料拥有专有权；购买产品不视为授予许可。违反保密义务、未经授权使用将被追究法律责任。
- **商标**：文档内容不授予使用移远或第三方任何商标的权利。
- **第三方权利**：文档可能涉及属于第三方的硬软件和文档（"第三方材料"），移远对第三方材料不做任何明示或暗示保证。

### 1.4 隐私声明
为实现产品功能，特定设备数据将上传至移远或第三方服务器（运营商、芯片供应商或客户指定服务器）。移远严格遵守相关法律法规处理数据。

### 1.5 免责声明（4 条）
1. 移远不承担因未能遵守操作或设计规范造成损害的责任。
2. 移远不承担因文档信息不准确、遗漏或使用文档信息产生的任何责任。
3. 开发中功能的完整性/准确性/及时性尽力保证，但不排除错误或遗漏，对开发中功能使用不做暗示或法定保证。
4. 对第三方网站及资源的信息、内容、产品、服务的可访问性/安全性/准确性/合法性等不承担法律责任。

> 版权所有 © 上海移远通信技术股份有限公司 2022，保留一切权利。

---

## 2. 文档历史（第 3 页）

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|------|------|------|---------|
| -（草稿） | 2021-11-25 | Larry ZHANG | 文档创建 |
| **1.0.0** | 2022-07-11 | Jayden CHEN | 临时版本 |

---

## 3. 目录结构（第 4 页）

```
文档历史 ............................. 3
目录 ................................ 4
表格索引 ............................ 5
1  引言 ............................. 6
2  ADC 功能介绍 .................... 7
3  ADC API 综述 ................... 8
   3.1 库文件和头文件 ............... 8
   3.2 参考示例 .................... 8
   3.3 ql_adc_show ................. 8
       3.3.1 ADC_CHANNEL_E ......... 9
4  ADC 编译示例及功能验证 .......... 10
   4.1 ADC 编译示例 ............... 10
   4.2 ADC 功能验证 ............... 10
5  附录 ........................... 12
```

### 表格索引（第 5 页）
- 表 1：用于测试电压的 ADC 接口信息 …… 7
- 表 2：参考文档 …… 12
- 表 3：术语缩写 …… 12

---

## 4. 第 1 章　引言（第 6 页）

- 移远 EC200A-CN(TA) 模块支持 **QuecOpen®** 方案。
- **QuecOpen® 是基于 Linux 的嵌入式开发平台**，可简化 IoT 应用的软件设计和开发过程。
- QuecOpen 的详细信息参考**文档 [1]**（《Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导》）。
- 本文档主要介绍 QuecOpen 方案下 EC200A-CN(TA) 模块的 **ADC 功能开发**，包含：
  1. ADC 接口引脚介绍
  2. ADC 相关 API 函数
  3. ADC 编译示例和功能验证

---

## 5. 第 2 章　ADC 功能介绍（第 7 页）

### 5.1 ADC 概念
**ADC（模数转换器 / Analog-to-Digital Converter）**：将连续变化的模拟信号转换为离散的数字信号的器件，即把真实世界的模拟信号（如温度、电压、声音等）转换成更易于储存、处理和发射的数字形式。

### 5.2 硬件接口
EC200A-CN(TA) QuecOpen 模块共提供 **两路 ADC 接口**，用于**测量电压值**。

#### 表 1：用于测试电压的 ADC 接口信息

| 引脚名称 | 引脚号 | I/O | 描述 | 备注 |
|---------|-------|-----|------|------|
| **ADC0** | **45** | AI | 通用模数转换接口 | 不用则悬空 |
| **ADC1** | **44** | AI | 通用模数转换接口 | 不用则悬空 |

> **备注**：有关 ADC 引脚及参考电路的详情，请参考**文档 [2]**（《Quectel_EC200A-CN(TA)_QuecOpen_硬件设计手册》）。

**要点**：
- `I/O = AI` 表示 **Analog Input（模拟输入）**。
- 两路通道彼此独立：ADC0 → 引脚 45，ADC1 → 引脚 44。
- 未使用的 ADC 引脚应保持**悬空**。

---

## 6. 第 3 章　ADC API 综述（第 8~9 页）

本章介绍与 ADC 功能相关的 API。

### 6.1 库文件和头文件（3.1）

| 类型 | 文件 | 路径 |
|------|------|------|
| 库文件 | `libql_sdk.so` | `ql-ol-rootfs/usr/lib` 目录下 |
| 接口头文件 | `ql_adc.h` | `ql-sysroots/usr/include/ql-sdk` 目录下 |

> ADC 应用程序的编写需要依赖库文件 `libql_sdk.so`。

### 6.2 参考示例（3.2）

- 模块 SDK 包中提供一套完整的 ADC 编程接口，**接口使用示例路径为 `sample/adc/`**，用户可参考所提供示例完成 ADC 应用程序的编写。

**截图 1 — lib 目录（含移远提供的 API 接口库）**
```bash
ls ./ql-ol-rootfs/usr/lib/libql_sdk.so
./ql-ol-rootfs/usr/lib/libql_sdk.so
```

**截图 2 — include 目录（含所有 API 头文件）**
```bash
ls ./ql-sysroots/usr/include/ql-sdk/ql_adc.h
./ql-sysroots/usr/include/ql-sdk/ql_adc.h
```
> 截图中的工作环境提示符为 `ol@ol:~/1803_sec/ql-ol-extsdk-ec200acntar02a01m2gv01$`，表明 SDK 工程目录为 `ql-ol-extsdk-ec200acntar02a01m2gv01`。

### 6.3 ql_adc_show（3.3）

该函数用于**读取指定 ADC 通道的电压值**。

#### 函数原型
```c
int ql_adc_show(ADC_CHANNEL_E qadc);
```

#### 参数

| 参数 | 方向 | 说明 |
|------|------|------|
| `adc`（qadc） | [In] 输入 | ADC 通道号，详见 3.3.1 章（`ADC_CHANNEL_E`） |

#### 返回值

| 返回值 | 含义 |
|--------|------|
| **ADC 电压值（单位：mV）** | 函数执行**成功** |
| **-1** | 函数执行**失败** |

#### 3.3.1　ADC_CHANNEL_E（ADC 通道号枚举定义）

```c
typedef enum ADC_CHANNEL_ENUM{
    QADC_NONE = 0,
    ADC0      = 1,
    ADC1      = 2,
    QADC_END
} ADC_CHANNEL_E;
```

**成员说明**

| 成员 | 值 | 描述 |
|------|----|------|
| `QADC_NONE` | 0 | 不使用 ADC |
| `ADC0` | 1 | ADC0 通道 |
| `ADC1` | 2 | ADC1 通道 |
| `QADC_END` | （3，枚举结束哨兵） | — |

---

## 7. 第 4 章　ADC 编译示例及功能验证（第 10~11 页）

### 7.1 ADC 编译示例（4.1）

进入 `sample/adc/` 目录，执行 **`make`** 命令生成 **`adc_test`** 可执行文件。

**截图 — 编译过程关键信息**
```bash
cd sample/adc
make
# 交叉编译工具链：
# /opt/ql_crosstools/ql-ec200a-1803z-gcc-8.4.0-v1-toolchain/bin/arm-openwrt-linux-gcc
# 编译选项节选：
#   -o main.o -c main.c -march=armv7-a -mfpu=neon -mfloat-abi=hard
#   -I.../ql-sysroots/usr/include -I.../ql-sysroots/usr/include/ql-sdk -Wall -Wundef
# 链接：
#   -o adc_test main.o -march=armv7-a -mfpu=neon -mfloat-abi=hard
#   -L.../ql-sysroots/usr/lib -lql_sdk -lpthread -lql_sys_log -lql_lib_ipc -lql_lib_utils
ls
# adc_test  main.c  main.o  Makefile
```

**编译要点提取**：
- 工具链：`arm-openwrt-linux-gcc`（gcc 8.4.0，armv7-a，neon，硬浮点 `-mfloat-abi=hard`）。
- 头文件路径：`ql-sysroots/usr/include` 与 `ql-sysroots/usr/include/ql-sdk`。
- 链接库：`-lql_sdk -lpthread -lql_sys_log -lql_lib_ipc -lql_lib_utils`。
- 产物：`adc_test`。

### 7.2 ADC 功能验证（4.2）

本节以 **ADC0** 为例，介绍获取 ADC0 电压值的方法。

#### 步骤一：硬件准备
- 准备电压值范围为 **0 ~ VBAT_BB V** 的电源。
- 将电源的**地线与模块的地线相连接**（共地）。
- 电源的**正极连接到 ADC0 引脚**。

> **备注**：电源输出的准确电压值可以通过**万用表**进行测量。

#### 步骤二：上传可执行文件
将编译生成的可执行文件（以 `adc_test` 为例）上传至模块：
```bash
adb push <adc_test 在上位机路径>  <模块内部路径，如 /tmp>
```

#### 步骤三：修改权限并执行
上传后，修改文件权限并执行：
```bash
root@OpenWrt:/tmp# chmod 777 adc_test
root@OpenWrt:/tmp# ./adc_test
< Quectel OpenLinux: ADC example >
ADC0 : 1871
ADC1 : 10
root@OpenWrt:/tmp#
```

**结果说明**：
- 返回值 **`ADC0 : 1871`** 即为测量的电压值，**单位为 mV**（即 1871 mV ≈ 1.871 V）。
- 同时打印 `ADC1 : 10`（ADC1 通道当前测量值 10 mV，因其引脚悬空/未接，为基底噪声值）。
- 程序启动横幅：`< Quectel OpenLinux: ADC example >`。
- 模块运行环境为 **OpenWrt**（提示符 `root@OpenWrt:/tmp#`）。

---

## 8. 第 5 章　附录（第 12 页）

### 表 2：参考文档

| 编号 | 文档名称 |
|------|---------|
| [1] | Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导 |
| [2] | Quectel_EC200A-CN(TA)_QuecOpen_硬件设计手册 |

### 表 3：术语缩写

| 缩写 | 英文解释 | 中文解释 |
|------|---------|---------|
| **ADC** | Analog-to-Digital Converter | 模拟数字转换器 |
| **API** | Application Programming Interface | 应用程序接口 |
| **IoT** | Internet of Things | 物联网 |
| **SDK** | Software Development Kit | 软件开发工具包 |

---

## 9. 关键技术点速查（整理总结）

| 维度 | 关键信息 |
|------|---------|
| 平台 | QuecOpen（基于 Linux，运行环境 OpenWrt） |
| ADC 通道数 | 2 路：ADC0、ADC1 |
| 引脚映射 | **ADC0 = 引脚 45**，**ADC1 = 引脚 44**（均为 AI 模拟输入） |
| 输入电压范围 | **0 ~ VBAT_BB V** |
| 唯一 API | `int ql_adc_show(ADC_CHANNEL_E qadc)` |
| 通道枚举 | `QADC_NONE=0` / `ADC0=1` / `ADC1=2` / `QADC_END` |
| 返回值 | 成功=电压值（**mV**）；失败=**-1** |
| 依赖库 | `libql_sdk.so`（链接 `-lql_sdk -lpthread -lql_sys_log -lql_lib_ipc -lql_lib_utils`） |
| 头文件 | `ql_adc.h`（`ql-sysroots/usr/include/ql-sdk`） |
| 示例路径 | `sample/adc/`，`make` 生成 `adc_test` |
| 工具链 | `arm-openwrt-linux-gcc` 8.4.0（armv7-a / neon / 硬浮点） |
| 部署流程 | `make` → `adb push` 到 `/tmp` → `chmod 777` → `./adc_test` |
| 实测样例 | `ADC0 : 1871`（= 1871 mV ≈ 1.871 V）；`ADC1 : 10`（悬空基底值） |
| 硬件接线 | 电源正极→ADC 引脚，电源地→模块地（共地），万用表校准 |

---

## 10. 最小可用代码示例（依据文档信息复原）

> 文档未给出 `main.c` 源码，以下为依据 API 定义与运行输出**复原的等效最小示例**，便于直接套用。

```c
#include <stdio.h>
#include "ql_adc.h"   /* 位于 ql-sysroots/usr/include/ql-sdk */

int main(void)
{
    int v0, v1;

    printf("< Quectel OpenLinux: ADC example >\n");

    v0 = ql_adc_show(ADC0);   /* ADC0 = 1 */
    if (v0 < 0)
        printf("ADC0 read failed\n");
    else
        printf("ADC0 : %d\n", v0);   /* 单位 mV，例如 1871 */

    v1 = ql_adc_show(ADC1);   /* ADC1 = 2 */
    if (v1 < 0)
        printf("ADC1 read failed\n");
    else
        printf("ADC1 : %d\n", v1);

    return 0;
}
```

**编译/部署（照搬文档流程）：**
```bash
# 1) 在 SDK 的 sample/adc/ 目录编译
cd sample/adc && make            # 产物 adc_test

# 2) 推送到模组
adb push adc_test /tmp/

# 3) 模组侧赋权运行
chmod 777 /tmp/adc_test
/tmp/adc_test
```

---

## 11. 与 open_dial 工程的关联备注

- 本文档与拨号守护进程 `open_dial` **无直接依赖关系**，属同一硬件平台（EC200A/QuecOpen）的外设开发资料，预计用于后续在模组上扩展**电压采集 / 电源电压监控**等功能。
- 调用风格与工程现状一致：ADC 走 **SDK API（`ql_adc_show`）**，与工程中 `nw/`、`sim/` 走 ql-sdk 一致，区别于连通性检测走 AT 命令的辅助路径。
- 若将来集成：注意 `ql_adc_show` 返回 **mV 整数**、失败为 **-1**，需做失败判断；输入电压务必限制在 **0~VBAT_BB V** 区间，避免超压损伤引脚。

---

*本 Markdown 基于官方 PDF（13 页）逐页整理，覆盖封面、声明、文档历史、目录、表格索引、正文 1~5 章、全部 3 张表格、API 定义、枚举、编译/运行截图命令与输出及所有"备注"提示，无内容遗漏。*
