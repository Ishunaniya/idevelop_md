# Quectel AG35-CET QuecOpen GPIO API 参考手册 — 全文分析

> **文档全名**：AG35-CET QuecOpen GPIO API 参考手册（LTE Standard 模块系列）
> **版本**：V1.0.0（Preliminary，临时文件 / Not Checked）
> **日期**：2023-08-17
> **作者**：Gabriel LI
> **总页数**：31 页
> **本分析覆盖**：全部 31 页（封面+前言+正文 1~4 章+附录）
> **适用平台**：移远 AG35-CET 模块（QuecOpen 方案，基于 Linux 的嵌入式开发平台，面向 IoV 车联网应用）

---

## 0. 文档定位与结构总览

本手册是 AG35-CET QuecOpen GPIO 编程的 **API 参考手册**。它从开发者视角，分别介绍两类 GPIO 配置接口：

1. **内核层 GPIO 配置 API**（第 2 章）—— 在内核驱动 / 设备树中使用，依赖 Linux 标准 GPIO 子系统与 pinctrl 子系统。
2. **用户层 GPIO 配置 API**（第 3 章）—— 移远 QuecOpen SDK 封装的 `ql_gpio_*` 系列接口，应用程序（用户态）直接调用，无需写内核驱动。

文档历史：版本 1.0.0，2023-08-17 创建，状态为"临时版本（Preliminary, Not Checked）"。意味着接口可能后续变更，需以实际 SDK 头文件为准。

文档包含 4 张表格索引：
- 表 1：内核层 GPIO 配置 API 函数概览（第 8 页）
- 表 2：用户层 GPIO 配置 API 函数概览（第 16 页）
- 表 3：参考文档（第 30 页）
- 表 4：术语缩写（第 30 页）

---

## 1. 引言（第 7 页）

要点：

- AG35-CET 模块支持 **QuecOpen®** 方案。QuecOpen 是基于 Linux 的嵌入式开发平台，用于简化 IoV（车联网）应用的软件设计与开发。详细信息见参考文档 [1]。
- AG35-CER QuecOpen 模块支持 GPIO 功能（原文此处写"AG35-CER"，应为同系列笔误，实际指 AG35-CET）。
- 本文从用户开发角度，分别介绍 **内核层** 和 **用户层** GPIO 配置相关的 API 接口函数及示例，帮助实现简易而快速的开发。

**解读**：AG35 提供两条 GPIO 编程路径——若你在写内核驱动 / 调设备树，用第 2 章的标准 Linux GPIO/IRQ API；若你只写用户态应用，用第 3 章的 `ql_gpio_*` SDK 封装。两者面向的层次不同，但底层操作的都是同一组 BB（Baseband 基带芯片）GPIO 引脚。

---

## 2. 内核层 GPIO 配置 API（第 8~15 页）

### 2.1 头文件（第 8 页）

| 用途 | 头文件 | 路径 |
|---|---|---|
| 内核层 GPIO API | `gpio.h` | `ql-ol-kernel/include/linux/` |
| 内核层中断 API | `interrupt.h` | `ql-ol-kernel/include/linux/` |

若无特别说明，本章涉及的头文件均位于 `ql-ol-kernel/include/linux/` 目录下。

**解读**：这就是标准 Linux 内核的 GPIO 子系统头文件，AG35 内核源码树根为 `ql-ol-kernel`。

### 2.2 函数概览（表 1，第 8 页）

| 函数 | 描述 |
|---|---|
| `gpio_request()` | 申请 GPIO |
| `gpio_direction_input()` | 设置 GPIO 的使用方向为输入 |
| `gpio_direction_output()` | 设置 GPIO 的使用方向为输出以及输出的电平状态 |
| `gpio_get_value()` | 获取 GPIO 引脚的电平状态 |
| `gpio_set_value()` | 设置 GPIO 引脚的电平状态 |
| `gpio_to_irq()` | 将 GPIO 引脚号转换为相应的 IRQ 值，返回中断号 |
| `gpio_free()` | 释放 GPIO |
| `request_irq()` | 配置中断 |
| `free_irq()` | 注销中断 |

### 2.3 函数详解（第 9~11 页）

#### 2.3.1 `gpio_request` — 申请 GPIO

```c
int gpio_request(unsigned gpio, const char *label)
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `gpio` | [In] | 平台 BB 芯片 GPIO 引脚号 |
| `label` | [In] | 自定义一个标签（用于标识该 GPIO 用途，便于调试 / `/sys` 查看） |

**返回值**：`0` = 执行成功；其他值 = 执行失败。

#### 2.3.2 `gpio_direction_input` — 设置方向为输入

```c
int gpio_direction_input(unsigned gpio)
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `gpio` | [In] | 平台 BB 芯片 GPIO 引脚号 |

**返回值**：`0` = 成功；其他值 = 失败。

#### 2.3.3 `gpio_direction_output` — 设置方向为输出并设初始电平

```c
int gpio_direction_output(unsigned gpio, int value)
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `gpio` | [In] | 平台 BB 芯片 GPIO 引脚号 |
| `value` | [In] | 设置的电平状态。`0`=低电平；`1`=高电平 |

**返回值**：`0` = 成功；其他值 = 失败。

#### 2.3.4 `gpio_get_value` — 读取引脚电平

```c
int gpio_get_value(unsigned gpio)
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `gpio` | [In] | 平台 BB 芯片 GPIO 引脚号 |

**返回值**：`0`=低电平；`1`=高电平；其他值=函数执行失败。

#### 2.3.5 `gpio_set_value` — 设置引脚电平

```c
void gpio_set_value(unsigned gpio, int value)
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `gpio` | [In] | 平台 BB 芯片 GPIO 引脚号 |
| `value` | [In] | 设置的电平状态。`0`=低电平；`1`=高电平 |

**返回值**：无（`void`）。

#### 2.3.6 `gpio_to_irq` — GPIO 号转中断号

```c
int gpio_to_irq(unsigned gpio)
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `gpio` | [In] | 平台 BB 芯片 GPIO 引脚号 |

**返回值**：IRQ 值（正数值）= 成功；其他值 = 失败。

**解读**：将 GPIO 引脚号映射为内核中断号，得到的 IRQ 号供 `request_irq()` 使用。

#### 2.3.7 `gpio_free` — 释放 GPIO

```c
void gpio_free(unsigned gpio)
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `gpio` | [In] | 平台 BB 芯片 GPIO 引脚号 |

**返回值**：无（`void`）。与 `gpio_request()` 成对使用。

### 2.4 中断 API 函数详解（第 12 页）

#### 2.4.1 `request_irq` — 配置中断

```c
request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
            const char *name, void *dev)
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `irq` | [In] | 中断号，由 `gpio_to_irq()` 产生 |
| `handler` | [In] | 中断处理函数 |
| `flags` | [In] | 中断触发方式：边沿触发或电平触发 |
| `name` | [In] | 中断处理程序定义的一个标签 |
| `dev` | [In] | 传入中断处理程序的参数，默认为 NULL |

**返回值**：`0` = 成功；其他值 = 失败。

#### 2.4.2 `free_irq` — 注销中断

```c
void free_irq(unsigned int irq, void *dev_id)
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `irq` | [In] | 中断号，由 `gpio_to_irq()` 产生 |
| `dev_id` | [In] | 传入中断处理程序的参数，默认为 NULL，与 `request_irq()` 中的 `dev` 参数取值相同 |

**返回值**：无（`void`）。

**解读**：`free_irq` 的 `dev_id` 必须与 `request_irq` 注册时的 `dev` 一致——这是共享中断场景下内核区分多个处理函数的依据，注销时需精确匹配。

### 2.5 示例代码（第 13~15 页）

文档给出两种常见的内核态 GPIO 配置方式：**pinctrl-names 方式** 与 **自定义字段方式**。

#### 2.5.1 pinctrl-names 方式（第 13~14 页）

**a. GPIO 引脚配置**（文件路径 `ql-ol-kernel/arch/arm/boot/dts/ql-asr1803-overlay.dtsi`）：

```dts
temp_gpio_default {        // 自定义一个 temp_gpio_default 节点
    pinctrl-single,pins = <
        GPIO25 AF0  /* gpio25 */
    >;
    DS_MEDIUM;PULL_UP;EDGE_BOTH;LPM_NONE;
}
```

说明各字段：
- `pinctrl-single,pins = <GPIO25 AF0>`：选定 GPIO25，复用功能为 AF0（Alternate Function 0，即作普通 GPIO 用）。
- `DS_MEDIUM`：Drive Strength 驱动强度=中。
- `PULL_UP`：内部上拉。
- `EDGE_BOTH`：双边沿（上升+下降沿）触发。
- `LPM_NONE`：低功耗模式（Low Power Mode）= 无特殊处理。

**b. pinctrl-names 引用代码**（文件路径 `ql-ol-kernel/arch/arm/boot/dts/qcom/ql-asr1803-overlay.dtsi`）：

```dts
xxx{
    compatible = "xxx";                  /* 匹配用户 driver */
    pinctrl-names = "temp_default";      /* 定义 pinctrl name，驱动中使用 pinctrl_lookup_state() 接口解析 */
    pinctrl-0 = <&temp_gpio_default>;    /* 选中上面定义的 gpio 配置 */
    status = "ok";                       /* 使能此设备节点 */
}
```

**c. 驱动中解析以上配置的内核 API**（三步）：

1) 获取一个 pinctrl 句柄：
```c
/**
 * struct devm_pinctrl_get() - Resource managed pinctrl_get()
 * @dev: the device to obtain the handle for
 * If there is a need to explicitly destroy the returned struct pinctrl,
 * devm_pinctrl_put() should be used, rather than plain pinctrl_put().
 */
struct pinctrl *devm_pinctrl_get(struct device *dev)
```

2) 获取该引脚对应的引脚状态：
```c
/**
 * pinctrl_lookup_state() - retrieves a state handle from a pinctrl handle
 * @p: the pinctrl handle to retrieve the state from
 * @name: the state name to retrieve
 */
struct pinctrl_state *pinctrl_lookup_state(struct pinctrl *p, const char *name)
```

3) 设置引脚为某个状态：
```c
/**
 * pinctrl_select_state() - select/activate/program a pinctrl state to HW
 * @p: the pinctrl handle for the device that requests configuration
 * @state: the state handle to select/activate/program
 */
int pinctrl_select_state(struct pinctrl *p, struct pinctrl_state *state)
```

**解读**：pinctrl-names 方式是标准 Linux 引脚复用配置流程——设备树里定义引脚组+复用功能+电气属性，驱动里用 `devm_pinctrl_get` → `pinctrl_lookup_state` → `pinctrl_select_state` 三步把该状态写入硬件。`devm_` 前缀版本是资源托管型，设备卸载时自动释放，无需手动 `pinctrl_put`。

#### 2.5.2 自定义字段方式（第 14~15 页）

##### 2.5.2.1 普通 GPIO 示例

**a. 设备信息 .dtsi 中设备节点配置**：
```dts
xxxx: xxxxx{
    compatible = "xxxxxxx";       /* 匹配用户驱动 */
    ........
    temp-gpio = <&gpio xxx 0>;    /* quec,temp-gpio 自定义字段，xxx 是对应的 GPIO 号 */
    status = "ok";
}
```

**b. 驱动代码中解析 .dtsi 参数**：
```c
ret = of_get_named_gpio_flags(np, "temp-gpio", 0, NULL);
if (ret > 0)
{
    gpio_request(ret, "temp-quec,i2c-en-gpio");
    gpio_direction_output(ret, 1);
    msleep(100);
}
```

**解读**：自定义字段方式用 `of_get_named_gpio_flags()` 从设备树解析出 GPIO 号，再走标准 `gpio_request` + `gpio_direction_output` 流程。这里示例把引脚拉高输出（如使能某个 I2C 电源），并延时 100ms。

##### 2.5.2.2 GPIO 中断示例（第 15 页）

**a. 设备节点配置**：
```dts
xxxx: xxxxx{
    compatible = "xxxxxxx";              /* 匹配用户 driver */
    ........
    interrupt-parent = <&gpio>;
    interrupts = <xxx IRQ_TYPE_EDGE_RISING>;   /* xxx 是对应的 GPIO 号，IRQ_TYPE_EDGE_RISING 为中断方式 */
    ql,temp-gpio = <&gpio xxx IRQ_TYPE_EDGE_RISING>;  /* quec,temp-gpio 自定义字段，xxx 是对应的 GPIO 号 */
    status = "ok";
}
```

**b. 驱动代码中解析并注册中断**：
```c
gpio_pin = of_get_named_gpio_flags(np, "ql,temp-gpio", 0, NULL);
ret = gpio_direction_input(gpio_pin);
if (ret < 0) {
    printk("request failed\n");
}
ql_irq = gpio_to_irq(gpio_pin);
ret = request_irq(ql_irq, ql_irq_handler, IRQF_TRIGGER_RISING, "ql,temp-gpio", NULL);
if (ret < 0)
    printk("request handle failed\n");
```

**c. 中断处理函数示例**：
```c
static irqreturn_t ql_irq_handler(int irq, void *handle){
    printk("xxxxxxxxxxxxxxx\n");
}
```

**备注（重要限制）**：中断处理函数中不可有大量事件或耗时操作，否则会导致系统不响应或其他异常情况。如果需要做耗时操作，建议用户自行添加 **中断下半部** 处理机制（一般选择 **工作队列** 的方式）。

**解读**：这是标准 Linux GPIO 中断注册流程——先把引脚设为输入，`gpio_to_irq` 取中断号，`request_irq` 注册处理函数并指定触发方式（这里 `IRQF_TRIGGER_RISING` 上升沿）。中断上半部（handler）必须短小快速，耗时工作下沉到 workqueue（下半部），避免阻塞中断上下文。

---

## 3. 用户层 GPIO 配置 API（第 16 页起）

### 3.1 头文件（第 16 页）

GPIO API 头文件为 `ql_gpio.h`，位于 `ql-sysroots/usr/include/ql-sdk/` 目录下。若无特别说明，本章涉及的头文件均在该目录下。

### 3.2 函数概览（表 2，第 16~17 页）

| 函数 | 描述 |
|---|---|
| `ql_gpio_init()` | 对指定的 GPIO 进行初始化配置，包括引脚方向、电平、内部上下拉 |
| `ql_gpio_uninit()` | 释放指定的 GPIO 引脚 |
| `ql_gpio_base_init()` | 初始化 GPIO（基础初始化） |
| `ql_gpio_set_level()` | 当 GPIO 的方向为输出时，配置输出的电平 |
| `ql_gpio_get_level()` | 获取当前 GPIO 的电平状态 |
| `ql_gpio_set_direction()` | 配置引脚方向 |
| `ql_gpio_get_direction()` | 获取引脚方向配置 |
| `ql_gpio_set_pull_selection()` | 配置指定引脚的内部上下拉状态 |
| `ql_gpio_get_pull_selection()` | 获取引脚在模块内部的上下拉状态 |
| `ql_eint_enable()` | 使能引脚中断，并注册用户回调函数 |
| `ql_eint_disable()` | 注销引脚中断功能 |
| `ql_set_gpio_function()` | 配置指定 GPIO 引脚的功能 |
| `ql_get_gpio_function()` | 获取指定 GPIO 引脚的功能 |
| `ql_check_pin_function_status()` | 检查引脚功能 |

### 3.3 函数详解

#### 3.3.1 `ql_gpio_init` — 初始化配置 GPIO（第 17 页）

对指定 GPIO 进行初始化配置，包括引脚方向、电平、内部上下拉。

```c
int ql_gpio_init(ENUM_PINNAME      pin_name,
                 ENUM_PIN_DIRECTION dir,
                 ENUM_PIN_LEVEL     level,
                 ENUM_PIN_PULLSEL   pull_sel
                 )
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `pin_name` | [In] | GPIO 引脚号。详见 3.3.1.1（`ENUM_PINNAME`） |
| `dir` | [In] | 引脚方向。详见 3.3.1.2（`ENUM_PIN_DIRECTION`） |
| `level` | [In] | 引脚电平状态。详见 3.3.1.3（`ENUM_PIN_LEVEL`） |
| `pull_sel` | [In] | 内部上下拉。详见 3.3.1.4（`ENUM_PIN_PULLSEL`） |

**返回值**：
| 返回码 | 含义 |
|---|---|
| `RES_OK` | 函数执行成功 |
| `RES_IO_NOT_SUPPORT` | 执行失败，输入的 GPIO 无效（详见 3.3.1.5） |
| `RES_IO_ERROR` | 执行失败，I/O 错误（详见 3.3.1.5） |

##### 3.3.1.1 `ENUM_PINNAME` — GPIO 引脚号枚举（第 17~18 页）

```c
typedef enum{
    PINNAME_BEGIN   = -1,
    PINNAME_GPIO_123 = 123,
    PINNAME_GPIO_117 = 117,
    PINNAME_GPIO_19  = 19,
    PINNAME_GPIO_118 = 118,
    PINNAME_GPIO_120 = 120,
    PINNAME_GPIO_23  = 23,
    PINNAME_GPIO_126 = 126,
    PINNAME_GPIO_119 = 119,
    PINNAME_GPIO_34  = 34,
    PINNAME_GPIO_36  = 36,
    PINNAME_GPIO_35  = 35,
    PINNAME_GPIO_33  = 33,
    PINNAME_GPIO_49  = 49,
    PINNAME_GPIO_50  = 50,
    PINNAME_GPIO_24  = 24,
    PINNAME_GPIO_21  = 21,
    PINNAME_GPIO_22  = 22,
    PINNAME_GPIO_48  = 48,
    PINNAME_GPIO_55  = 55,
    PINNAME_GPIO_56  = 56,
    PINNAME_GPIO_57  = 57,
    PINNAME_GPIO_59  = 59,
    PINNAME_GPIO_58  = 58,
    PINNAME_GPIO_43  = 43,
    PINNAME_GPIO_32  = 32,
    PINNAME_GPIO_31  = 31,
    PINNAME_GPIO_52  = 52,
    PINNAME_GPIO_51  = 51,
    PINNAME_END     = 127
}ENUM_PINNAME
```

**成员（节选）**：
| 成员 | 描述 |
|---|---|
| `PINNAME_GPIO_117` | GPIO 引脚编号 117 |
| `PINNAME_GPIO_120` | GPIO 引脚编号 120 |
| `PINNAME_GPIO_19` | GPIO 引脚编号 19 |
| `PINNAME_GPIO_118` | GPIO 引脚编号 118 |
| …… | …… |
| `PINNAME_GPIO_51` | GPIO 引脚编号 51 |
| `PINNAME_END` | GPIO 最大支持引脚编号 127 |

**解读**：枚举值即 GPIO 物理编号本身（如 `PINNAME_GPIO_25=25`）。`PINNAME_BEGIN=-1`、`PINNAME_END=127` 为边界哨兵。AG35 用户层可用的 GPIO 是这个枚举里列出的离散集合（如 19,21~24,31~36,43,48~52,55~59,117~120,123,126 等），并非 0~127 全部连续可用——具体哪些引脚可作 GPIO 取决于硬件设计与引脚复用情况。

##### 3.3.1.2 `ENUM_PIN_DIRECTION` — 引脚方向枚举（第 19 页）

```c
typedef enum{
    PINDIRECTION_IN  = 0,
    PINDIRECTION_OUT = 1
}ENUM_PIN_DIRECTION
```

| 成员 | 描述 |
|---|---|
| `PINDIRECTION_IN` | 输入 |
| `PINDIRECTION_OUT` | 输出 |

##### 3.3.1.3 `ENUM_PIN_LEVEL` — 引脚电平枚举（第 19 页）

```c
typedef enum{
    PINLEVEL_LOW  = 0,
    PINLEVEL_HIGH = 1
}ENUM_PIN_LEVEL
```

| 成员 | 描述 |
|---|---|
| `PINLEVEL_LOW` | 低电平 |
| `PINLEVEL_HIGH` | 高电平 |

##### 3.3.1.4 `ENUM_PIN_PULLSEL` — 引脚上下拉枚举（第 19 页）

```c
typedef enum{
    PINPULLSEL_DISABLE  = 0,
    PINPULLSEL_PULLDOWN = 1,
    PINPULLSEL_PULLUP   = 2
}ENUM_PIN_PULLSEL
```

| 成员 | 值 | 描述 |
|---|---|---|
| `PINPULLSEL_DISABLE` | 0 | 禁用内部上下拉（悬空 / 浮空高阻） |
| `PINPULLSEL_PULLDOWN` | 1 | 内部下拉 |
| `PINPULLSEL_PULLUP` | 2 | 内部上拉 |

##### 3.3.1.5 返回值枚举（统一错误码，第 20 页）

所有 `ql_*` 用户层接口共用下面这套返回码枚举：

```c
enum {
    RES_OK              = 0,
    RES_BAD_PARAMETER   = -1,
    RES_IO_NOT_SUPPORT  = -2,
    RES_IO_ERROR        = -3,
    RES_NOT_IMPLEMENTED = -4
}
```

| 成员 | 值 | 描述 |
|---|---|---|
| `RES_OK` | 0 | 函数执行成功 |
| `RES_BAD_PARAMETER` | -1 | 无效参数 |
| `RES_IO_NOT_SUPPORT` | -2 | 不支持该 GPIO |
| `RES_IO_ERROR` | -3 | I/O 错误 |
| `RES_NOT_IMPLEMENTED` | -4 | 设置不生效 |

**解读**：这套统一错误码是判断 `ql_gpio_*` / `ql_eint_*` / `ql_*_gpio_function` 调用结果的依据。`RES_IO_NOT_SUPPORT` 通常意味着传入的 `pin_name` 不在 `ENUM_PINNAME` 支持列表内，或该引脚已被其他功能复用、不能作 GPIO；`RES_NOT_IMPLEMENTED`（设置不生效）则提示该操作在当前引脚上无实际效果。注意：部分"取值返回型"接口（如 `ql_gpio_get_level`/`get_direction`/`get_pull_selection`）的成功返回值是数据本身（0/1/2），失败才返回负的错误码——见下文逐个说明。

#### 3.3.2 `ql_gpio_uninit` — 释放 GPIO（第 20~21 页）

```c
int ql_gpio_uninit(ENUM_PINNAME pin_name)
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `pin_name` | [In] | GPIO 引脚号（见 3.3.1.1） |

**返回值**：`RES_OK`=成功；`RES_IO_NOT_SUPPORT`=输入 GPIO 无效；`RES_IO_ERROR`=I/O 错误。与 `ql_gpio_init` 成对使用。

#### 3.3.3 `ql_gpio_base_init` — 基础初始化 GPIO（第 21 页）

```c
int ql_gpio_base_init(ENUM_PINNAME pin_name)
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `pin_name` | [In] | GPIO 引脚号（见 3.3.1.1） |

**返回值**：`RES_OK`/`RES_IO_NOT_SUPPORT`/`RES_IO_ERROR`（含义同上）。

**重要说明**：该函数用于初始化 GPIO。**根据实际需求，在该函数与 `ql_gpio_init()` 间二选一使用**。

**解读**：这是本手册一个关键设计点——
- `ql_gpio_init()`：**一步到位的快速配置**，一次性设定方向+电平+上下拉。
- `ql_gpio_base_init()`：**仅做基础初始化（占用/申请引脚）**，之后需再分别调用 `ql_gpio_set_direction()`、`ql_gpio_set_level()`、`ql_gpio_set_pull_selection()` 逐项配置（"单独配置"流程，见 3.4.2）。
两者不应同时用于同一引脚，按场景择一。

#### 3.3.4 `ql_gpio_set_level` — 设置输出电平（第 21~22 页）

```c
int ql_gpio_set_level(ENUM_PINNAME pin_name, ENUM_PIN_LEVEL level)
```

适用于 GPIO 方向为 **输出** 时，配置输出电平。

| 参数 | 方向 | 含义 |
|---|---|---|
| `pin_name` | [In] | GPIO 引脚号 |
| `level` | [In] | 引脚电平状态（`PINLEVEL_LOW`/`PINLEVEL_HIGH`，见 3.3.1.3） |

**返回值**：`RES_OK`/`RES_IO_NOT_SUPPORT`/`RES_IO_ERROR`。

#### 3.3.5 `ql_gpio_get_level` — 读取电平（第 22 页）

```c
int ql_gpio_get_level(ENUM_PINNAME pin_name)
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `pin_name` | [In] | GPIO 引脚号 |

**返回值**：
| 返回 | 含义 |
|---|---|
| `0` | 低电平 |
| `1` | 高电平 |
| `RES_IO_NOT_SUPPORT` | 执行失败，不支持该 GPIO |

#### 3.3.6 `ql_gpio_set_direction` — 设置方向（第 22 页）

```c
int ql_gpio_set_direction(ENUM_PINNAME pin_name, ENUM_PIN_DIRECTION dir)
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `pin_name` | [In] | GPIO 引脚号 |
| `dir` | [In] | 引脚方向（`PINDIRECTION_IN`/`PINDIRECTION_OUT`，见 3.3.1.2） |

**返回值**：`RES_OK`/`RES_IO_NOT_SUPPORT`/`RES_IO_ERROR`。

#### 3.3.7 `ql_gpio_get_direction` — 读取方向（第 23 页）

```c
int ql_gpio_get_direction(ENUM_PINNAME pin_name)
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `pin_name` | [In] | GPIO 引脚号 |

**返回值**：
| 返回 | 含义 |
|---|---|
| `0` | 输入 |
| `1` | 输出 |
| `RES_IO_NOT_SUPPORT` | 执行失败，不支持该 GPIO |

#### 3.3.8 `ql_gpio_set_pull_selection` — 设置内部上下拉（第 23 页）

```c
int ql_gpio_set_pull_selection(ENUM_PINNAME pin_name, ENUM_PIN_PULLSEL pull_sel)
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `pin_name` | [In] | GPIO 引脚号 |
| `pull_sel` | [In] | 内部上下拉（`PINPULLSEL_DISABLE`/`PULLDOWN`/`PULLUP`，见 3.3.1.4） |

**返回值**：`RES_OK`/`RES_IO_NOT_SUPPORT`/`RES_IO_ERROR`。

#### 3.3.9 `ql_gpio_get_pull_selection` — 读取内部上下拉（第 23~24 页）

```c
int ql_gpio_get_pull_selection(ENUM_PINNAME pin_name)
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `pin_name` | [In] | GPIO 引脚号 |

**返回值**：
| 返回 | 含义 |
|---|---|
| `0` | 悬空（禁用上下拉） |
| `1` | 下拉 |
| `2` | 上拉 |
| `RES_IO_NOT_SUPPORT` | 执行失败，不支持该 GPIO |

#### 3.3.10 `ql_eint_enable` — 使能引脚中断并注册回调（第 24 页）

```c
int ql_eint_enable(ENUM_PINNAME      eint_pin_name,
                   ENUM_EINT_TYPE    eint_type,
                   ql_eint_callback  eint_callback
                   )
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `eint_pin_name` | [In] | GPIO 引脚号（见 3.3.1.1） |
| `eint_type` | [In] | 边沿触发类型（见 3.3.10.1 `ENUM_EINT_TYPE`） |
| `eint_callback` | [In] | 用户回调函数，中断触发时调用该回调（类型 `ql_eint_callback`） |

**返回值**：`RES_OK`=成功；其他值=失败（见 3.3.1.5）。

**解读**：这是用户层最重要的中断接口——无需写内核驱动，应用直接注册一个 EINT（外部中断）回调。底层由 SDK 在内核侧完成 `gpio_to_irq`+`request_irq` 的等价工作，并把中断事件回调到用户态。

##### 3.3.10.1 `ENUM_EINT_TYPE` — 中断触发类型枚举（第 25 页）

```c
typedef enum {
    EINT_SENSE_NONE,
    EINT_SENSE_RISING,
    EINT_SENSE_FALLING,
    EINT_SENSE_BOTH
}ENUM_EINT_TYPE
```

| 成员 | 描述 |
|---|---|
| `EINT_SENSE_NONE` | 引脚无边沿触发中断 |
| `EINT_SENSE_RISING` | 引脚上升沿触发中断 |
| `EINT_SENSE_FALLING` | 引脚下降沿触发中断 |
| `EINT_SENSE_BOTH` | 引脚双边沿触发中断 |

（枚举未显式赋值，按 C 规则依次为 0/1/2/3。）

#### 3.3.11 `ql_eint_disable` — 注销引脚中断（第 25 页）

```c
int ql_eint_disable(ENUM_PINNAME eint_pin_name)
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `eint_pin_name` | [In] | GPIO 引脚号 |

**返回值**：`RES_OK`=成功；`RES_BAD_PARAMETER`=参数错误。

#### 3.3.12 `ql_set_gpio_function` — 配置引脚功能（第 25~26 页）

```c
int ql_set_gpio_function(ENUM_PINNAME pin_name, unsigned int func)
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `pin_name` | [In] | GPIO 引脚号 |
| `func` | [In] | GPIO 功能配置，**取值范围 0~7** |

**返回值**：`RES_OK`=成功；其他值=失败。

**解读**：`func` 即引脚复用功能编号（Alternate Function 0~7）。在 AG35 上，要把一个默认作其他外设用途的引脚切回普通 GPIO，需先用本接口把功能设为对应的 GPIO 复用值（通常配合 `ql_check_pin_function_status` 判断，见 3.4.1 示例：当引脚功能不是 GPIO 时调用 `ql_set_gpio_function(pin, 1)`）。

#### 3.3.13 `ql_get_gpio_function` — 读取引脚功能（第 26 页）

```c
int ql_get_gpio_function(ENUM_PINNAME pin_name)
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `pin_name` | [In] | GPIO 引脚号 |

**返回值**：
| 返回 | 含义 |
|---|---|
| `0` | 引脚功能是 GPIO |
| `1` | 引脚功能不是 GPIO |
| `RES_IO_NOT_SUPPORT` | 不支持该 GPIO |
| `RES_IO_ERR` | 执行失败 |

#### 3.3.14 `ql_check_pin_function_status` — 检查引脚功能（第 26~27 页）

```c
int ql_check_pin_function_status(ENUM_PINNAME pin_name)
```

| 参数 | 方向 | 含义 |
|---|---|---|
| `pin_name` | [In] | GPIO 引脚号 |

**返回值**：
| 返回 | 含义 |
|---|---|
| `0` | 引脚功能是 GPIO |
| `1` | 引脚功能不是 GPIO |
| `RES_IO_NOT_SUPPORT` | 不支持该 GPIO |
| `RES_IO_ERR` | 执行失败 |

**解读**：`ql_get_gpio_function` 与 `ql_check_pin_function_status` 返回值定义完全一致（0=是 GPIO，1=不是 GPIO）。配置 GPIO 前应先用本接口检查：若返回 1（当前不是 GPIO 功能），需先 `ql_set_gpio_function` 切到 GPIO 功能，否则后续 `ql_gpio_init`/配置无效。

### 3.4 GPIO 配置（实战流程，第 27~28 页）

本节以 **GPIO 118** 为例，演示两条配置路径：用 `ql_gpio_init()` 做 **快速配置**，与用 `ql_gpio_base_init()` 做 **单独配置**。

#### 3.4.1 GPIO 快速配置（第 27 页）

三步：

1. 选定引脚：
```c
static ENUM_PINNAME m_gpio_pin_gpio = PINNAME_GPIO_118;
```

2. 调用 `ql_check_pin_function_status()` 检查引脚功能，若不是 GPIO 功能则先用 `ql_set_gpio_function()` 切为 GPIO：
```c
/*
 * Note:
 * When using gpio configure, you need to determine whether the function of pin is gpio function.
 * If it is not gpio function, the following gpio configure are invalid.
 * You can use the ql_set_gpio_function to setting.
 */
iRet = ql_check_pin_function_status(m_gpio_pin_gpio);
if(iRet == 0){
    iRet = ql_set_gpio_function(m_gpio_pin_gpio, 1);
    printf("< Set gpio(%d) function iRet=%d status=%d >\n",
           m_gpio_pin_gpio, iRet, ql_check_pin_function_status(m_gpio_pin_gpio));
}
```

3. 调用 `ql_gpio_init()` 一步完成初始化（方向=输出、电平=高、上下拉=禁用）：
```c
/*
 * Before using gpio init the application layer, you first need to determine whether it is used in the driver.
 */
iRet = ql_gpio_init(m_gpio_pin_gpio, PINDIRECTION_OUT, PINLEVEL_HIGH, PINPULLSEL_DISABLE);
printf("< Init GPIO: pin=%d, dir=%d, level=%d, iRet=%d >\n",
       m_gpio_pin_gpio, PINDIRECTION_OUT, PINLEVEL_HIGH, iRet);
```

**重要提示（注释中给出）**：在应用层使用 GPIO 前，必须先确认该引脚是否已在内核驱动中被占用——若已被驱动使用，应用层再配置会冲突/无效。

#### 3.4.2 GPIO 单独配置（第 28 页）

用 `ql_gpio_base_init()` 先做基础初始化，再分项配置。三种典型场景：

1. **设为输入引脚**：
   - `ql_gpio_base_init()` 设引脚号为 GPIO 118；
   - `ql_gpio_set_direction()` 设方向为输入；
   - `ql_gpio_set_pull_selection()` 设上/下拉。

2. **设为输出引脚**：
   - `ql_gpio_base_init()` 设引脚号为 GPIO 118；
   - `ql_gpio_set_direction()` 设方向为输出；
   - `ql_gpio_set_level()` 设输出电平。

3. **设为普通中断触发**：
   - `ql_gpio_base_init()` 设引脚号为 GPIO 118；
   - `ql_eint_enable()` 设边沿触发类型和中断回调函数。
   ```c
   iRet = ql_eint_enable(m_gpio_eint_pin_gpio, EINT_SENSE_BOTH, eint_callback);
   printf("< ql_eint_enable: pin=%d iRet=%d >\n", m_gpio_eint_pin_gpio, iRet);
   ```
   （示例中触发类型为双边沿 `EINT_SENSE_BOTH`。注：原文截图中字样为 `Ql_EINT_Enable`/`CMPT_EINT_SENSE_BOTH`，应与 3.3.10 的 `ql_eint_enable`/`EINT_SENSE_BOTH` 为同一接口/枚举的不同书写。）

**解读**：快速配置（`ql_gpio_init`）与单独配置（`ql_gpio_base_init`+分项设置）是两套等价但粒度不同的流程。中断场景下，无论哪条路径都先 `base_init` 占用引脚，再 `ql_eint_enable` 注册回调。

### 3.5 编译及功能验证（第 28~29 页）

#### 3.5.1 示例介绍及编译（第 28 页）

示例功能：**每隔 1 秒拉高/拉低 GPIO**。

编译流程：进入 `sample/gpio/` 目录，执行 `make` 生成 `gpio_test` 可执行文件。截图中的编译命令显示：
- 交叉编译器：`/opt/ql_crosstools/ql-ag35-18030-gcc-8.4.0-v1-toolchain/bin/arm-openwrt-linux-gcc`
- 编译参数：`-march=armv7-a -marm -mfpu=neon -mfloat-abi=hard`
- 头文件：`-I .../ql-sysroots/usr/include/ql-sdk`、`-I .../ql-sysroots/usr/include`
- 链接库：`-L .../ql-sysroots/usr/lib -lql_sdk -lpthread -lql_sys_log -lql_lib_ipc -lql_lib_utils`
- 编译产物：`main.o` → `gpio_test`

**解读**：用户层 GPIO 程序需链接移远 SDK 库：`libql_sdk`（GPIO 等核心 API）、`libql_sys_log`（日志）、`libql_lib_ipc`（进程间通信）、`libql_lib_utils`（工具）以及 `pthread`。工具链是 ASR 平台（AG35 基于 ASR1803）的 OpenWrt arm-openwrt-linux-gcc 8.4.0。

#### 3.5.2 功能验证（第 28~29 页）

操作步骤：

a. 编译并上传 `gpio_test` 到模块：
```sh
adb push <gpio_test 在上位机路径> <模块内部路径，如 /mnt>
```

b. 修改执行权限：
```sh
chmod 777 gpio_test
```

c. 执行 `gpio_test`，可观察到指定引脚的高低电平变化（也可用万用表测量）。运行输出示例：
```
root@OpenWrt:/mnt# ./gpio_test
< OpenLinux: GPIO example >
< Set gpio(118) function iRet=0 status=1>
< Init GPIO: pin=118, dir=1, level=1, iRet=0 >
< Pull pin level to low >
< Pull pin level to high >
< Pull pin level to low >
[   70.934112] KERNEL-TEXT-CRC: 0x25a0
< Pull pin level to high >
< Pull pin level to low >
< Pull pin level to high >
< Pull pin level to low >
< Pull pin level to high >
< Pull pin level to low >
```

**解读**：输出印证前述流程——先 `Set gpio(118) function`（功能切换成功，status=1 表示当前不是默认 GPIO 而被显式设过），再 `Init GPIO`（dir=1 输出、level=1 高、iRet=0 成功），随后循环 `Pull pin level to low/high` 即每秒翻转电平。中间夹杂的 `KERNEL-TEXT-CRC` 是内核日志，与 GPIO 测试无关。

---

## 4. 附录：参考文档及术语缩写（第 30 页）

### 表 3：参考文档
| 编号 | 文档名称 |
|---|---|
| [1] | Quectel_AG35-CET_QuecOpen_快速开发指导 |

### 表 4：术语缩写
| 缩写 | 英文 | 中文 |
|---|---|---|
| API | Application Programming Interface | 应用程序接口 |
| BB | Baseband | 基带 |
| GPIO | General-Purpose Input/Output | 通用输入/输出 |
| IoV | Internet of Vehicles | 车联网 |
| IRQ | Interrupt Request | 中断请求 |
| SDK | Software Development Kit | 软件开发工具包 |

---

## 5. 综合解读与对本项目（modem_mng）的关联提示

1. **两层 API 的选择**：modem_mng 作为用户态守护进程，若要在 AG35 平台直接控制 GPIO（例如网络 LED、复位脚），应优先使用第 3 章的 **用户层 `ql_gpio_*` API**，无需改内核驱动。本仓 CLAUDE.md 记载 AG35 调 `libledcontrol` 触发 SIGILL 已用宏空桩屏蔽（见记忆 [[ag35-ledcontrol-sigill-workaround]]）——若后续要绕过该库直接控灯，本手册的 `ql_gpio_init`+`ql_gpio_set_level` 即是可行替代路径（需先确认 LED 引脚号及其是否被内核驱动占用）。

2. **使用前必查引脚功能**：用户层配置 GPIO 前务必 `ql_check_pin_function_status()`，返回非 0（不是 GPIO）则先 `ql_set_gpio_function(pin, 1)`，否则后续配置静默失效——这是手册反复强调的坑点。

3. **引脚是否被内核驱动占用**：手册明确提示，应用层操作前要确认该引脚未在内核 dts/驱动中被占用，否则冲突。控灯引脚很可能已被某个内核 LED 驱动持有，这正是直接走用户层 API 之前需要排查的。

4. **中断回调约束**：无论内核层（`request_irq` 上半部）还是用户层（`ql_eint_enable` 回调），都不应执行耗时操作；内核侧建议用工作队列做下半部。

5. **链接库清单**（移植编译参考）：`-lql_sdk -lpthread -lql_sys_log -lql_lib_ipc -lql_lib_utils`，工具链 `arm-openwrt-linux-gcc 8.4.0`（ASR1803），编译参数 `-march=armv7-a -marm -mfpu=neon -mfloat-abi=hard`——与本仓 AG35 交叉编译配置可对照。

<!-- GENERATION_COMPLETE -->
