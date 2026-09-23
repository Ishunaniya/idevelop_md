# AG35-CET & AG35-EUT QuecOpen(SDK) 语音服务开发指导

> **文档元信息**
> - 原文档：`Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_语音服务_开发指导`
> - 适用模块：LTE Standard 模块系列（AG35-CET、AG35-EUT）
> - 版本：1.0.1
> - 日期：2025-03-24
> - 状态：临时文件（Preliminary / Confidential）
> - 作者：Sunshine HUANG / Joetar CHEN
> - 总页数：30 页
>
> ⚠️ **平台提示**：本文档面向 **AG35-CET/EUT**。本项目 `open_dial` 运行在 **EC200A/EG25** 上，API 命名风格相近但需对照实际平台 `ql_voice.h` 核对。

---

## 文档历史

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| - | 2023-10-24 | Sunshine HUANG | 文档创建 |
| 1.0.0 | 2023-10-24 | Sunshine HUANG | 临时版本 |
| 1.0.1 | 2025-03-24 | Joetar CHEN | 临时版本：<br>1. 新增使用模块 AG35-EUT。<br>2. 新增 API `ql_voice_set_service_error_cb()`（第 2 章）。 |

---

## 1 引言

移远通信 AG35-CET 和 AG35-EUT 模块支持 QuecOpen® 方案。QuecOpen® 是基于 Linux 的嵌入式开发平台，可简化应用的软件设计和开发过程。有关 QuecOpen® 的详细信息，请参考**文档 [1]**（《Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导》）。

本文档适用于 SDK 构建环境的 QuecOpen® 方案，主要介绍在 QuecOpen® 方案下，AG35-CET 和 AG35-EUT 模块的语音服务 API 及相关示例。

---

## 2 语音服务 API

### 2.1 头文件

语音服务 API 头文件为 `ql_voice.h`，位于 SDK 的 `ql-sysroots/usr/include/ql-sdk/` 目录下。若无特别说明，本文档所涉及头文件均在该目录下。

### 2.2 函数概览

#### 表 1：函数概览

| 函数 | 说明 |
|---|---|
| `ql_voice_init()` | 初始化语音服务 |
| `ql_voice_deinit()` | 注销语音服务 |
| `ql_voice_dial()` | 拨打电话 |
| `ql_voice_hangup_all()` | 挂断所有拨打的电话 |
| `ql_voice_answer()` | 接听电话 |
| `ql_voice_hangup()` | 挂断电话 |
| `ql_voice_hold()` | 开启呼叫保持 |
| `ql_voice_unhold()` | 关闭呼叫保持 |
| `ql_voice_get_records()` | 获取通话记录 |
| `ql_voice_forwarding()` | 设置呼叫转移 |
| `ql_voice_get_forwarding_status()` | 查询呼叫转移状态 |
| `ql_voice_waiting()` | 设置呼叫等待 |
| `ql_voice_get_waiting_status()` | 查询呼叫等待状态 |
| `ql_voice_autoanswer()` | 设置自动接听 |
| `ql_voice_send_dtmf_char()` | 发送 DTMF 字符 |
| `ql_voice_set_call_cb()` | 注册电话信息接收回调函数 |
| `ql_voice_set_service_error_cb()` | 设置语音服务错误事件接收回调函数 |

> **备注**
> 若无特别说明，本文档所述函数均不支持并发调用，并且不能在相关回调函数中调用以上函数。

### 2.3 函数详解

> 所有函数返回值约定：`QL_ERR_OK` 表示函数执行成功；其他值表示函数执行失败，错误码详见 `ql_type.h`。

#### 2.3.1 ql_voice_init

初始化语音服务。

```c
int ql_voice_init(void)
```

- **参数**：无
- **返回值**：`QL_ERR_OK` 成功；其他值失败（错误码详见 `ql_type.h`）。

> **备注**：使用其他语音服务 API 前，必须调用本函数初始化语音服务。

#### 2.3.2 ql_voice_deinit

注销语音服务。

```c
int ql_voice_deinit(void)
```

- **参数**：无
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

> **备注**：若不再使用语音服务，必须调用本函数注销语音服务、释放资源。

#### 2.3.3 ql_voice_dial

拨打电话。

```c
int ql_voice_dial(char *num, int len, uint32_t *id)
```

- **参数**：
  - `num`：[In] 对方电话号码。
  - `len`：[In] 电话号码长度。
  - `id`：[Out] 本次通话 ID。用于后续通话操作。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

#### 2.3.4 ql_voice_hangup_all

挂断所有拨打的电话。

```c
int ql_voice_hangup_all(void)
```

- **参数**：无
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

#### 2.3.5 ql_voice_answer

接听电话。

```c
int ql_voice_answer(uint32_t id)
```

- **参数**：`id`：[In] 通话 ID。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

#### 2.3.6 ql_voice_hangup

挂断电话。

```c
int ql_voice_hangup(uint32_t id)
```

- **参数**：`id`：[In] 通话 ID。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

#### 2.3.7 ql_voice_hold

开启呼叫保持。

```c
int ql_voice_hold(uint32_t id)
```

- **参数**：`id`：[In] 通话 ID。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

> **备注**：须确保已向运营商订购了呼叫保持业务，方可开启呼叫保持功能。

#### 2.3.8 ql_voice_unhold

关闭呼叫保持。

```c
int ql_voice_unhold(uint32_t id)
```

- **参数**：`id`：[In] 通话 ID。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

> **备注**：确保已向运营商订购了呼叫保持业务后，才能关闭呼叫保持功能。

#### 2.3.9 ql_voice_get_records

获取通话记录。

```c
int ql_voice_get_records(ql_voice_record_array_t *p_arr)
```

- **参数**：`p_arr`：[Out] 通话记录表；详见第 2.3.9.1 章。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

##### 2.3.9.1 ql_voice_record_array_t

通话记录表结构体定义如下：

```c
typedef struct
{
    int len;
    ql_voice_record_t records[QL_VOICE_MAX_RECORDS];
} ql_voice_record_array_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| int | `len` | 通话记录个数 |
| `ql_voice_record_t` | `records` | 通话记录详情。由参数 `len` 指定个数，`QL_VOICE_MAX_RECORDS` 表示可保存的最大记录，**最大值为 8**；详见第 2.3.9.2 章。 |

##### 2.3.9.2 ql_voice_record_t

通话记录结构体定义如下：

```c
typedef struct
{
    uint32_t                id;
    char                    number[QL_VOICE_MAX_PHONE_NUMBER + 1];
    QL_VOICE_STATE_E        state;
    QL_VOICE_TECH_E         tech;
    QL_VOICE_DIR_E          dir;
    QL_VOICE_END_REASON_E   end_reason;
} ql_voice_record_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| uint32_t | `id` | 通话 ID |
| char | `number` | 电话号码 |
| `QL_VOICE_STATE_E` | `state` | 当前通话状态；详见第 2.3.9.3 章。 |
| `QL_VOICE_TECH_E` | `tech` | 语音服务遵循的技术规范。`QL_VOICE_TECH_3GPP` 遵循 3GPP 规范；`QL_VOICE_TECH_3GPP2` 遵循 3GPP2 规范。 |
| `QL_VOICE_DIR_E` | `dir` | 通话方向。`QL_VOICE_DIR_MO` 主叫；`QL_VOICE_DIR_MT` 被叫。 |
| `QL_VOICE_END_REASON_E` | `end_reason` | 电话挂断原因。挂断原因比较多，详见 `ql_voice.h` 头文件。 |

##### 2.3.9.3 QL_VOICE_STATE_E

当前通话状态枚举定义如下：

```c
typedef enum
{
    QL_VOICE_STATE_INCOMING = 0x0000,
    QL_VOICE_STATE_DIALING  = 0x0001,
    QL_VOICE_STATE_ALERTING = 0x0002,
    QL_VOICE_STATE_ACTIVE   = 0x0003,
    QL_VOICE_STATE_HOLDING  = 0x0004,
    QL_VOICE_STATE_END      = 0x0005,
    QL_VOICE_STATE_WAITING  = 0x0006,
} QL_VOICE_STATE_E;
```

| 成员 | 描述 |
|---|---|
| `QL_VOICE_STATE_INCOMING` | 有电话等待接入 |
| `QL_VOICE_STATE_DIALING` | 拨号中 |
| `QL_VOICE_STATE_ALERTING` | 等待被叫接听 |
| `QL_VOICE_STATE_ACTIVE` | 电话已接听，通话建立。 |
| `QL_VOICE_STATE_HOLDING` | 呼叫保持中 |
| `QL_VOICE_STATE_END` | 通话结束 |
| `QL_VOICE_STATE_WAITING` | 呼叫等待中 |

#### 2.3.10 ql_voice_forwarding

设置呼叫转移。

```c
int ql_voice_forwarding(int reg, QL_VOICE_FW_COND_E cond, char *num, int len)
```

- **参数**：
  - `reg`：[In] 呼叫转移开启状态。`0` 关闭；其他值 开启。
  - `cond`：[In] 呼叫转移的条件；详见第 2.3.10.1 章。
  - `num`：[In] 呼叫转移的电话号码。
  - `len`：[In] 呼叫转移的电话号码长度。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

> **备注**：须确保已向运营商订购了呼叫转移业务，方可开启或关闭呼叫转移功能。

##### 2.3.10.1 QL_VOICE_FW_COND_E

呼叫转移条件枚举定义如下：

```c
typedef enum
{
    QL_VOICE__FW_COND_UNCONDITIONALLY = 0,
    QL_VOICE__FW_COND_MOBILEBUSY      = 1,
    QL_VOICE__FW_COND_NOREPLY         = 2,
    QL_VOICE__FW_COND_UNREACHABLE     = 3,
    QL_VOICE__FW_COND_ALLFORWARDING   = 4,
    QL_VOICE__FW_COND_ALLCONDITIONAL  = 5,
} QL_VOICE_FW_COND_E;
```

| 成员 | 描述 |
|---|---|
| `QL_VOICE__FW_COND_UNCONDITIONALLY` | 无条件转移 |
| `QL_VOICE__FW_COND_MOBILEBUSY` | 条件转移：被叫忙时转移。 |
| `QL_VOICE__FW_COND_NOREPLY` | 条件转移：被叫无回应时转移。 |
| `QL_VOICE__FW_COND_UNREACHABLE` | 条件转移：被叫不可达时转移（例如被叫关机）。 |
| `QL_VOICE__FW_COND_ALLFORWARDING` | 全部转移。即无条件转移和条件转移。 |
| `QL_VOICE__FW_COND_ALLCONDITIONAL` | 所有的条件转移 |

#### 2.3.11 ql_voice_get_forwarding_status

查询呼叫转移状态。

```c
int ql_voice_get_forwarding_status(QL_VOICE_FW_COND_E cond, ql_voice_fw_status_t *p_status)
```

- **参数**：
  - `cond`：[In] 呼叫转移的条件；详见第 2.3.10.1 章。不可将 `QL_VOICE__FW_COND_ALLFORWARDING` 和 `QL_VOICE__FW_COND_ALLCONDITIONAL` 作为查询条件。
  - `p_status`：[Out] 呼叫转移状态信息；详见第 2.3.11.1 章。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

##### 2.3.11.1 ql_voice_fw_status_t

呼叫转移状态信息结构体定义如下：

```c
typedef struct
{
    int enabled;
    int len;
    struct {
        QL_VOICE_FW_TYPE_E type;
        char number[QL_VOICE_MAX_PHONE_NUMBER + 1];
    } details[QL_VOICE_MAX_FW_DETAIL_LENGTH];
} ql_voice_fw_status_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| int | `enabled` | 呼叫转移开启状态。`0` 关闭；`1` 开启 |
| int | `len` | 呼叫转移个数 |
| `QL_VOICE_FW_TYPE_E` | `type` | 呼叫转移类型。`QL_VOICE_FW_TYPE_VOICE` 语音转移；`QL_VOICE_FW_TYPE_DATA` 数据转移；`QL_VOICE_FW_TYPE_VOICE_DATA` 语音数据转移。 |
| char | `number` | 呼叫转移号码 |
| anonymous struct | `details` | 呼叫转移详细信息。详见 `type` 和 `number` 参数定义。 |

#### 2.3.12 ql_voice_waiting

设置呼叫等待。

```c
int ql_voice_waiting(int enable)
```

- **参数**：`enable`：[In] 呼叫等待开启状态。`1` 开启；`0` 关闭。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

> **备注**：须确保已向运营商订购了呼叫等待业务，方可开启或关闭呼叫等待功能。

#### 2.3.13 ql_voice_get_waiting_status

查询是否已开启呼叫等待功能。

```c
int ql_voice_get_waiting_status(int *enabled)
```

- **参数**：`enabled`：[Out] 呼叫等待开启状态。`1` 开启；`0` 关闭。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

#### 2.3.14 ql_voice_autoanswer

设置自动接听。

```c
int ql_voice_autoanswer(int enable, uint32_t sec)
```

- **参数**：
  - `enable`：[In] 自动接听开启状态。`1` 开启；`0` 关闭。
  - `sec`：[In] 等待时间。单位：秒。等待设置的时间值后，自动接听电话。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

#### 2.3.15 ql_voice_send_dtmf_char

发送 DTMF 字符。

```c
int ql_voice_send_dtmf_char(uint32_t id, char c)
```

- **参数**：
  - `id`：[In] 通话 ID。
  - `c`：[In] DTMF 字符。合法字符：`0~9`、`A~D`、`*` 和 `#`。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

#### 2.3.16 ql_voice_set_call_cb

注册通话信息接收回调函数。

```c
int ql_voice_set_call_cb(ql_voice_call_cb_f cb)
```

- **参数**：`cb`：[In] 通话信息接收回调函数；详见第 2.3.16.1 章。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

##### 2.3.16.1 ql_voice_call_cb_f

该回调函数接收通话信息。

```c
typedef void (*ql_voice_call_cb_f)(ql_voice_record_array_t *p_arr)
```

- **参数**：`p_arr`：[Out] 通话记录表；详见第 2.3.9.1 章。

#### 2.3.17 ql_voice_set_service_error_cb

设置语音服务错误事件接收回调函数。仅当语音服务异常时，该回调函数才会被执行。

```c
int ql_voice_set_service_error_cb(ql_voice_service_error_cb_f cb)
```

- **参数**：`cb`：[In] 语音服务错误事件接收回调函数；详见第 2.3.17.1 章。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

##### 2.3.17.1 ql_voice_service_error_cb_f

该回调函数接收语音服务错误事件。

```c
typedef void (*ql_voice_service_error_cb_f)(int error);
```

- **参数**：`error`：[Out] 语音服务错误码；详见 `ql_type.h`。
- **返回值**：无

---

## 3 示例

本章所述代码示例均摘自 `sample/voice/main.c`，参考示例时请注意以下几点：

1. 程序启动后，必须调用 `ql_voice_init()` 初始化语音服务；
2. 程序退出前或不再使用语音服务时，必须调用 `ql_voice_deinit()` 释放资源；
3. 使用呼叫保持、呼叫等待、呼叫转移功能时，须确保已向运营商订购了相应业务。

### 3.1 拨打电话

```c
static void show_ql_voice_dial(void)
{
    int ret = 0;
    uint32_t id;
    char number[QL_VOICE_MAX_PHONE_NUMBER] = {0};

    printf("show ql_voice_dial: \n");
    printf("please enter the phone number: ");
    scanf("%s", number);
    getchar();
    ret = ql_voice_dial(number, strlen(number), &id);
    if (ret == QL_ERR_OK)
    {
        printf("ok, phone number is %s, call id is %u\n", number, id);
    }
    else
    {
        printf("failed, ret = %d\n", ret);
    }
}
```

### 3.2 挂断电话

```c
static void show_ql_voice_hangup(void)
{
    int ret = 0;
    uint32_t id;

    printf("show ql_voice_hangup: \n");
    printf("please enter call id: ");
    scanf("%u", &id);
    getchar();
    printf("call is %u\n", id);
    ret = ql_voice_hangup(id);
    if (ret == QL_ERR_OK)
    {
        printf("ok\n");
    }
    else
    {
        printf("failed, ret = %d\n", ret);
    }
}
```

### 3.3 接听电话

```c
static void show_ql_voice_answer(void)
{
    int ret = 0;
    uint32_t id;

    printf("show ql_voice_answer: \n");
    printf("please enter call id: ");
    scanf("%u", &id);
    getchar();
    printf("call is %u\n", id);
    ret = ql_voice_answer(id);
    if (ret == QL_ERR_OK)
    {
        printf("ok\n");
    }
    else
    {
        printf("failed, ret = %d\n", ret);
    }
}
```

### 3.4 挂断所有拨打的电话

```c
static void show_ql_voice_hangup_all(void)
{
    int ret = 0;

    printf("show ql_voice_hangup_all: ");

    ret = ql_voice_hangup_all();
    if (ret == QL_ERR_OK)
    {
        printf("ok\n");
    }
    else
    {
        printf("failed, ret = %d\n", ret);
    }
}
```

### 3.5 呼叫保持

```c
static void show_ql_voice_hold(void)
{
    int ret = 0;
    uint32_t id;

    printf("show ql_voice_hold: \n");
    printf("please enter call id: ");
    scanf("%u", &id);
    getchar();
    printf("call is %u\n", id);

    ret = ql_voice_hold(id);
    if (ret == QL_ERR_OK)
    {
        printf("ok\n");
    }
    else
    {
        printf("failed, ret = %d\n", ret);
    }
}
```

### 3.6 获取通话记录

```c
static void show_ql_voice_get_records(void)
{
    int i;
    int ret;
    ql_voice_record_array_t arr = {0};

    printf("show ql_voice_get_records: ");
    ret = ql_voice_get_records(&arr);
    if (ret == QL_ERR_OK)
    {
        printf("ok\n");
    }
    else
    {
        printf("failed, ret = %d\n", ret);
        return;
    }

    if (0 == arr.len)
    {
        printf("No records\n");
        return;
    }

    for (i = 0; i < arr.len; i++)
    {
        printf("  call id: %u\n", arr.records[i].id);
        printf("  number: %s\n", arr.records[i].number);
        if (arr.records[i].state <= QL_VOICE_STATE_WAITING
            && arr.records[i].state >= QL_VOICE_STATE_INCOMING)
        {
            printf("  state: %s\n", voice_state_str[arr.records[i].state]);
        }
        else
        {
            printf("  state: unknown %d\n", arr.records[i].state);
        }
        if (arr.records[i].tech <= QL_VOICE_TECH_3GPP2
            && arr.records[i].tech >= QL_VOICE_TECH_NONE)
        {
            printf("  tech: %s\n", voice_tech_str[arr.records[i].tech]);
        }
        else
        {
            printf("  tech: unknown %d\n", arr.records[i].tech);
        }

        printf("  call direction: %d\n", arr.records[i].dir);
        printf("  end reason: %d\n\n", arr.records[i].end_reason);
    }
}
```

### 3.7 呼叫转移

```c
static void show_ql_voice_forwarding(void)
{
    int ret;
    int reg = 0;
    QL_VOICE_FW_COND_E cond;
    char num[QL_VOICE_MAX_PHONE_NUMBER] = {0};

    printf("show ql_voice_forwarding: \n");
    printf("register forwarding? (0-no, 1-yes): ");
    scanf("%d", &reg);
    getchar();
    printf("under what condition?\n");
    printf(" 0: Unconditional\n");
    printf(" 1: Forward when the mobile device is busy\n");
    printf(" 2: Forward when there is no reply\n");
    printf(" 3: Forward when the call is unreachable\n");
    printf(" 4: All forwarding(0-3)\n");
    printf(" 5: All conditional forwarding(1-3)\n");
    scanf("%d", (int *)&cond);
    if (cond < 0 || cond > 5)
    {
        printf("bad choice\n");
        return;
    }
    getchar();
    if (0 == reg)
    {
        ret = ql_voice_forwarding(reg, cond, NULL, 0);
    }
    else
    {
        printf("enter the phone number: ");
        scanf("%s", num);
        getchar();

        ret = ql_voice_forwarding(reg, cond, num, strlen(num));
    }

    if (ret == QL_ERR_OK)
    {
        printf("ok\n");
    }
    else
    {
        printf("failed, ret = %d\n", ret);
    }
}
```

### 3.8 呼叫等待

```c
static void show_ql_voice_waiting(void)
{
    int enable;
    int ret = 0;

    printf("show ql_voice_waiting: \n");
    printf("enable(0-no, 1-yes): ");
    scanf("%d", &enable);
    getchar();

    ret = ql_voice_waiting(enable);
    if (ret == QL_ERR_OK)
    {
        printf("ok\n");
    }
    else
    {
        printf("failed, ret = %d\n", ret);
    }
}
```

### 3.9 自动接听电话

```c
static void show_ql_voice_autoanswer(void)
{
    int enable;
    uint32_t sec;
    int ret = 0;

    printf("show ql_voice_autoanswer: \n");
    printf("enable(0-no, 1-yes): ");
    scanf("%d", &enable);
    getchar();
    printf("seconds before auto answer: ");
    scanf("%u", &sec);
    getchar();

    ret = ql_voice_autoanswer(enable, sec);
    if (ret == QL_ERR_OK)
    {
        printf("ok\n");
    }
    else
    {
        printf("failed, ret = %d\n", ret);
    }
}
```

### 3.10 发送 DTMF 字符

```c
static void show_ql_voice_send_dtmf_char(void)
{
    int ret = 0;
    uint32_t id;
    char c;

    printf("show ql_voice_send_dtmf_char: \n");
    printf("please enter call id: ");
    scanf("%u", &id);
    getchar();
    printf("please enter dtmf char('0~9', 'A~d', '*', '#'): ");
    scanf("%c", &c);
    getchar();

    ret = ql_voice_send_dtmf_char(id, c);
    if (ret == QL_ERR_OK)
    {
        printf("ok\n");
    }
    else
    {
        printf("failed, ret = %d\n", ret);
    }
}
```

---

## 4 附录：参考文档及术语缩写

### 表 2：参考文档

| 编号 | 文档名称 |
|---|---|
| [1] | Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导 |

### 表 3：术语缩写

| 缩写 | 英文描述 | 中文描述 |
|---|---|---|
| ACK | Acknowledgment | 确认消息 |
| API | Application Programming Interface | 应用程序编程接口 |
| DTMF | Dual-tone Multifrequency | 双音多频 |
| ID | Identity | 身份标识号 |
| IoV | Internet of Vehicles | 车联网 |
| MSD | Minimum Set of Data | 最小数据集 |
| SDK | Software Development Kit | 软件开发工具包 |
| 3GPP | 3rd Generation Partnership Project | 第三代合作伙伴计划 |

---

*版权所有 © 上海移远通信技术股份有限公司 2025，保留一切权利。*
*Copyright © Quectel Wireless Solutions Co., Ltd. 2025.*
