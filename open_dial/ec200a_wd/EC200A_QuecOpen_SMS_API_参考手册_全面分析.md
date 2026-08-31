# EC200A-CN(TA) QuecOpen SMS API 参考手册 — 全面分析

> **文档信息**
> - 原文：Quectel EC200A-CN(TA) QuecOpen SMS API 参考手册
> - 版本：V1.0.0（临时文件）
> - 日期：2022-10-14
> - 适用模块系列：LTE Standard
> - 发布方：上海移远通信技术股份有限公司（Quectel Wireless Solutions Co., Ltd.）

---

## 目录

1. [文档基本信息](#1-文档基本信息)
2. [引言](#2-引言)
3. [SMS API 详解](#3-sms-api-详解)
   - [3.1 头文件](#31-头文件)
   - [3.2 函数概览](#32-函数概览)
   - [3.3 全局注意事项](#33-全局注意事项)
   - [3.4 函数详解](#34-函数详解)
4. [数据结构与枚举类型汇总](#4-数据结构与枚举类型汇总)
5. [完整代码示例](#5-完整代码示例)
6. [附录：参考文档与术语缩写](#6-附录参考文档与术语缩写)
7. [综合使用指南与注意事项汇总](#7-综合使用指南与注意事项汇总)

---

## 1. 文档基本信息

### 1.1 文档历史

| 版本  | 日期       | 作者        | 变更描述     |
|-------|------------|-------------|--------------|
| -     | 2021-11-22 | Lyndsay XIE | 文档创建     |
| 1.0.0 | 2022-10-14 | Lyndsay XIE | 临时版本发布 |

### 1.2 联系方式

- **公司**：上海移远通信技术股份有限公司
- **地址**：上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼，邮编：200233
- **电话**：+86 21 5108 6236
- **邮箱**：info@quectel.com
- **技术支持**：support@quectel.com

### 1.3 法律声明要点

- 文档内容仅供客户产品设计使用，参考设计仅为示例
- 未经书面同意，不得对所提供文档进行复制、转载、抄袭、出版、展示、翻译、分发、合并、修改或创建衍生作品
- 移远通信可在未事先通知的情况下，随时增加、修改或重述本文档
- **免责声明**：移远通信不对因使用开发中功能而遭受的损害承担责任，不排除功能错误或遗漏的可能

---

## 2. 引言

移远通信 EC200A-CN(TA) 模块支持 **QuecOpen® 方案**。

- **QuecOpen®** 是基于 Linux 的嵌入式开发平台，可简化应用的软件设计和开发过程
- 本文档主要介绍在 QuecOpen® 方案下，EC200A-CN(TA) 模块的 **SMS API** 及相关示例
- 更多 QuecOpen® 详细信息，请参考文档 [1]（见第6章）

---

## 3. SMS API 详解

### 3.1 头文件

| 属性     | 值                                              |
|----------|-------------------------------------------------|
| 头文件名 | `ql_sms.h`                                      |
| 路径     | SDK 包的 `ql-sysroots\usr\include\ql-sdk` 目录下 |

> 若无特别说明，本文档所涉及头文件均在该目录下。

---

### 3.2 函数概览

**表 1：函数概览**

| 函数                              | 说明                       |
|-----------------------------------|----------------------------|
| `ql_sms_init()`                   | 初始化 SMS 服务            |
| `ql_sms_deinit()`                 | 注销 SMS 服务              |
| `ql_sms_set_service_center_addr()`| 设置短信中心号码           |
| `ql_sms_get_service_center_addr()`| 获取短信中心号码           |
| `ql_sms_send_msg()`               | 发送文本短信（同步）       |
| `ql_sms_send_msg_async()`         | 异步发送文本短信           |
| `ql_sms_set_msg_recv_cb()`        | 设置文本短信接收回调函数   |
| `ql_sms_send_pdu()`               | 发送 PDU 短信（同步）      |
| `ql_sms_send_pdu_async()`         | 异步发送 PDU 短信          |
| `ql_sms_set_pdu_recv_cb()`        | 设置 PDU 短信接收回调函数  |
| `ql_sms_set_service_error_cb()`   | 设置 SMS 服务异常回调函数  |

---

### 3.3 全局注意事项

> **重要备注**
>
> 若无特别说明，本文档所述函数均**不支持并发调用**，并且**不能在相关回调函数中调用**以上函数。

---

### 3.4 函数详解

#### 2.3.1 `ql_sms_init` — 初始化 SMS 服务

**描述**

初始化 SMS 服务。使用其他 SMS API 前，**必须**首先调用本函数。

**函数原型**

```c
int ql_sms_init(void)
```

**参数**

无

**返回值**

| 返回值      | 描述                               |
|-------------|------------------------------------|
| `QL_ERR_OK` | 函数执行成功                       |
| 其他值      | 函数执行失败，错误码详见 `ql_type.h` |

**备注**

> 使用其他 SMS API 前，必须调用本函数初始化 SMS 服务。

---

#### 2.3.2 `ql_sms_deinit` — 注销 SMS 服务

**描述**

注销 SMS 服务，释放相关资源。

**函数原型**

```c
int ql_sms_deinit(void)
```

**参数**

无

**返回值**

| 返回值      | 描述                               |
|-------------|------------------------------------|
| `QL_ERR_OK` | 函数执行成功                       |
| 其他值      | 函数执行失败，错误码详见 `ql_type.h` |

**备注**

> 若不再使用 SMS，**必须**调用本函数注销 SMS 服务以释放资源。

---

#### 2.3.3 `ql_sms_set_service_center_addr` — 设置短信中心号码

**描述**

设置短信中心号码（SMSC Address）。

**函数原型**

```c
int ql_sms_set_service_center_addr(char *addr, int len)
```

**参数**

| 参数   | 方向  | 类型     | 描述                         |
|--------|-------|----------|------------------------------|
| `addr` | [In]  | `char *` | 短信中心号码                 |
| `len`  | [In]  | `int`    | 短信中心号码长度，单位：字节 |

**返回值**

| 返回值      | 描述                               |
|-------------|------------------------------------|
| `QL_ERR_OK` | 函数执行成功                       |
| 其他值      | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 2.3.4 `ql_sms_get_service_center_addr` — 获取短信中心号码

**描述**

获取短信中心号码。

**函数原型**

```c
int ql_sms_get_service_center_addr(char *addr, int len)
```

**参数**

| 参数   | 方向   | 类型     | 描述                         |
|--------|--------|----------|------------------------------|
| `addr` | [Out]  | `char *` | 短信中心号码（输出缓冲区）   |
| `len`  | [In]   | `int`    | 短信中心号码长度，单位：字节 |

**返回值**

| 返回值      | 描述                               |
|-------------|------------------------------------|
| `QL_ERR_OK` | 函数执行成功                       |
| 其他值      | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 2.3.5 `ql_sms_send_msg` — 发送文本短信（同步）

**描述**

同步发送文本短信。调用后阻塞，直到发送完成或失败后返回。

**函数原型**

```c
int ql_sms_send_msg(ql_sms_msg_t *p_msg)
```

**参数**

| 参数    | 方向  | 类型             | 描述                               |
|---------|-------|------------------|------------------------------------|
| `p_msg` | [In]  | `ql_sms_msg_t *` | 文本短信结构体指针，详见 §2.3.5.1 |

**返回值**

| 返回值      | 描述                               |
|-------------|------------------------------------|
| `QL_ERR_OK` | 函数执行成功                       |
| 其他值      | 函数执行失败，错误码详见 `ql_type.h` |

**备注**

> 为方便用户使用，当短信格式为 GSM 7-bit（`QL_SMS_MSG_FORMAT_GSM_7BIT`）时，用户输入 **ISO8859-1 格式**的短信信息即可，无需转换成 GSM 7-bit 格式，**该函数将自动进行格式转换**。ISO8859-1 编码向下兼容 ASCII 编码。

---

##### 2.3.5.1 `ql_sms_msg_t` — 文本短信结构体

**结构体定义**

```c
typedef struct
{
    QL_SMS_MSG_FORMAT_E format;
    char addr[QL_SMS_MAX_ADDR_LENGTH + 1];
    int content_size;
    char content[QL_SMS_MAX_SEND_MSG_LENGTH];
} ql_sms_msg_t;
```

**成员说明**

| 类型                   | 成员           | 描述                                                 |
|------------------------|----------------|------------------------------------------------------|
| `QL_SMS_MSG_FORMAT_E`  | `format`       | 短信格式，详见 §2.3.5.2                              |
| `char`                 | `addr`         | 发送短信时为**接收方地址**；接收短信时为**发送方地址** |
| `int`                  | `content_size` | 短信长度，单位：字节                                 |
| `char`                 | `content`      | 短信内容                                             |

**相关宏常量**

| 宏常量                      | 用途                   |
|-----------------------------|------------------------|
| `QL_SMS_MAX_ADDR_LENGTH`    | 地址最大长度           |
| `QL_SMS_MAX_SEND_MSG_LENGTH`| 发送短信内容最大长度   |
| `QL_SMS_MAX_RECV_MSG_LENGTH`| 接收短信内容最大长度   |

---

##### 2.3.5.2 `QL_SMS_MSG_FORMAT_E` — 文本短信格式枚举

**枚举定义**

```c
typedef enum
{
    QL_SMS_MSG_FORMAT_GSM_7BIT      = 0,
    QL_SMS_MSG_FORMAT_BINARY_DATA   = 1,
    QL_SMS_MSG_FORMAT_UCS2          = 2,
} QL_SMS_MSG_FORMAT_E;
```

**成员说明**

| 枚举值                           | 数值 | 描述                     |
|----------------------------------|------|--------------------------|
| `QL_SMS_MSG_FORMAT_GSM_7BIT`     | 0    | GSM 7-bit 编码（ASCII/ISO8859-1 兼容）|
| `QL_SMS_MSG_FORMAT_BINARY_DATA`  | 1    | 二进制数据短信           |
| `QL_SMS_MSG_FORMAT_UCS2`         | 2    | UCS-2 编码短信（支持中文等 Unicode 字符）|

---

#### 2.3.6 `ql_sms_send_msg_async` — 异步发送文本短信

**描述**

异步发送文本短信。函数调用后立即返回，短信发送结果通过回调函数通知。

**函数原型**

```c
int ql_sms_send_msg_async(ql_sms_msg_t *p_msg, int *id, ql_sms_msg_async_cb_f cb)
```

**参数**

| 参数    | 方向   | 类型                      | 描述                                              |
|---------|--------|---------------------------|---------------------------------------------------|
| `p_msg` | [In]   | `ql_sms_msg_t *`          | 文本短信结构体指针，详见 §2.3.5.1                |
| `id`    | [Out]  | `int *`                   | 异步发送文本短信事件 ID，用于在回调中标识该事件  |
| `cb`    | [In]   | `ql_sms_msg_async_cb_f`   | 异步发送回调函数，用于接收发送结果，详见 §2.3.6.1|

**返回值**

| 返回值      | 描述                               |
|-------------|------------------------------------|
| `QL_ERR_OK` | 函数执行成功（表示已成功提交发送请求）|
| 其他值      | 函数执行失败，错误码详见 `ql_type.h` |

**备注**

> 与 `ql_sms_send_msg` 相同，当短信格式为 GSM 7-bit 时，用户输入 ISO8859-1 格式的短信信息即可，该函数将自动进行格式转换。ISO8859-1 编码向下兼容 ASCII 编码。

---

##### 2.3.6.1 `ql_sms_msg_async_cb_f` — 异步发送文本短信回调函数类型

**描述**

该回调函数在异步发送文本短信操作完成后被调用，用于接收发送结果。

**函数原型**

```c
typedef void (*ql_sms_msg_async_cb_f)(int id, int result);
```

**参数**

| 参数     | 方向  | 类型  | 描述                                     |
|----------|-------|-------|------------------------------------------|
| `id`     | [In]  | `int` | 异步发送文本短信事件 ID                  |
| `result` | [In]  | `int` | 发送结果：`0` = 成功，其他值 = 失败（详见 `ql_type.h`）|

---

#### 2.3.7 `ql_sms_set_msg_recv_cb` — 设置文本短信接收回调函数

**描述**

注册文本短信接收回调函数，当模块收到文本短信时，SDK 将调用此回调通知应用层。

**函数原型**

```c
int ql_sms_set_msg_recv_cb(ql_sms_msg_recv_cb_f cb)
```

**参数**

| 参数  | 方向  | 类型                      | 描述                                         |
|-------|-------|---------------------------|----------------------------------------------|
| `cb`  | [In]  | `ql_sms_msg_recv_cb_f`    | 文本短信接收回调函数，详见 §2.3.7.1          |

**返回值**

| 返回值      | 描述                               |
|-------------|------------------------------------|
| `QL_ERR_OK` | 函数执行成功                       |
| 其他值      | 函数执行失败，错误码详见 `ql_type.h` |

---

##### 2.3.7.1 `ql_sms_msg_recv_cb_f` — 文本短信接收回调函数类型

**描述**

该回调函数在收到文本短信时由 SDK 调用，将短信内容、时间戳及长短信头部信息传递给应用层。

**函数原型**

```c
typedef void (*ql_sms_msg_recv_cb_f)(ql_sms_msg_t *p_msg,
                                     ql_sms_timestamp_t *timestamp,
                                     ql_sms_user_data_head_t *head);
```

**参数**

| 参数        | 方向  | 类型                       | 描述                                                      |
|-------------|-------|----------------------------|-----------------------------------------------------------|
| `p_msg`     | [In]  | `ql_sms_msg_t *`           | 文本短信结构体，详见 §2.3.5.1                             |
| `timestamp` | [In]  | `ql_sms_timestamp_t *`     | 短信时间戳，详见 §2.3.7.2                                 |
| `head`      | [In]  | `ql_sms_user_data_head_t *`| 长短信头部信息，用于标识长短信中的每个子短信，详见 §2.3.7.3|

**备注**

> 为方便用户使用，当短信格式为 GSM 7-bit（`QL_SMS_MSG_FORMAT_GSM_7BIT`）时，SDK 会**自动**将短信转换成 ISO8859-1 格式，**用户无需自己转换**。ISO8859-1 编码向下兼容 ASCII 编码。

---

##### 2.3.7.2 `ql_sms_timestamp_t` — 文本短信时间戳结构体

**结构体定义**

```c
typedef struct
{
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t timezone;
} ql_sms_timestamp_t;
```

**成员说明**

| 类型      | 成员       | 描述                                                              |
|-----------|------------|-------------------------------------------------------------------|
| `uint8_t` | `year`     | 年，**自 2000 年起计算**。例如，`19` 表示 2019 年                |
| `uint8_t` | `month`    | 月                                                               |
| `uint8_t` | `day`      | 日                                                               |
| `uint8_t` | `hours`    | 小时，24 小时制，范围：0~23                                       |
| `uint8_t` | `minutes`  | 分钟                                                             |
| `uint8_t` | `seconds`  | 秒                                                               |
| `uint8_t` | `timezone` | 时区，单位：**1/4 小时**。例如，中国位于 +8 区，其 `timezone` 为 **+32** |

**时区说明**

- `timezone` 字段的值 = 实际时区偏移量（小时） × 4
- 例：东八区（UTC+8）→ `timezone` = 8 × 4 = **32**

---

##### 2.3.7.3 `ql_sms_user_data_head_t` — 长短信头部信息结构体

**描述**

用于标识长短信（超过单条短信长度限制，被拆分成多条发送的短信）中的每个子短信片段。

**结构体定义**

```c
typedef struct
{
    uint8_t valid;
    uint8_t total_seg;
    uint8_t cur_seg_num;
    uint8_t ref_num;
} ql_sms_user_data_head_t;
```

**成员说明**

| 类型      | 成员          | 描述                                                  |
|-----------|---------------|-------------------------------------------------------|
| `uint8_t` | `valid`       | 本结构体是否有效：`1` = 有效；`0` = 无效（即本条短信**不是**长短信）|
| `uint8_t` | `total_seg`   | 长短信的**总段数**                                    |
| `uint8_t` | `cur_seg_num` | 本条短信的**段数编号**（当前是第几段）                |
| `uint8_t` | `ref_num`     | 本条短信的**接收编号**，为本条长短信的**唯一标识号**  |

**长短信重组逻辑**

通过 `ref_num` 区分属于同一条长短信的所有片段；通过 `cur_seg_num` 和 `total_seg` 确定顺序和完整性。

---

#### 2.3.8 `ql_sms_send_pdu` — 发送 PDU 短信（同步）

**描述**

同步发送 PDU 格式短信。调用后阻塞，直到发送完成或失败后返回。

**函数原型**

```c
int ql_sms_send_pdu(ql_sms_pdu_t *p_pdu)
```

**参数**

| 参数    | 方向  | 类型             | 描述                               |
|---------|-------|------------------|------------------------------------|
| `p_pdu` | [In]  | `ql_sms_pdu_t *` | PDU 短信结构体指针，详见 §2.3.8.1 |

**返回值**

| 返回值      | 描述                               |
|-------------|------------------------------------|
| `QL_ERR_OK` | 函数执行成功                       |
| 其他值      | 函数执行失败，错误码详见 `ql_type.h` |

---

##### 2.3.8.1 `ql_sms_pdu_t` — PDU 短信结构体

**结构体定义**

```c
typedef struct
{
    QL_SMS_PDU_FORMAT_E format;
    int content_size;
    char content[QL_SMS_MAX_SEND_PDU_LENGTH];
} ql_sms_pdu_t;
```

**成员说明**

| 类型                   | 成员           | 描述                               |
|------------------------|----------------|------------------------------------|
| `QL_SMS_PDU_FORMAT_E`  | `format`       | PDU 短信格式，详见 §2.3.8.2       |
| `int`                  | `content_size` | PDU 内容长度，单位：字节           |
| `char`                 | `content`      | PDU 内容（原始字节）               |

**相关宏常量**

| 宏常量                      | 用途               |
|-----------------------------|--------------------|
| `QL_SMS_MAX_SEND_PDU_LENGTH`| PDU 发送内容最大长度|

---

##### 2.3.8.2 `QL_SMS_PDU_FORMAT_E` — PDU 短信格式枚举

**枚举定义**

```c
typedef enum
{
    QL_SMS_PDU_FORMAT_CDMA   = 0,
    QL_SMS_PDU_FORMAT_GW_PP  = 6,
} QL_SMS_PDU_FORMAT_E;
```

**成员说明**

| 枚举值                    | 数值 | 描述                             |
|---------------------------|------|----------------------------------|
| `QL_SMS_PDU_FORMAT_CDMA`  | 0    | CDMA 网络 PDU 格式               |
| `QL_SMS_PDU_FORMAT_GW_PP` | 6    | GSM & WCDMA — 点对点（Point-to-Point）PDU 格式 |

**备注**

> 1. 编码 PDU 短信时，**无需**添加短信中心号码（SMSC 号码已包含在 PDU 数据中）。
> 2. **当前模块不支持 CDMA 网络制式**，请勿使用 `QL_SMS_PDU_FORMAT_CDMA`。

---

#### 2.3.9 `ql_sms_send_pdu_async` — 异步发送 PDU 短信

**描述**

异步发送 PDU 短信。函数调用后立即返回，发送结果通过回调函数通知。

**函数原型**

```c
int ql_sms_send_pdu_async(ql_sms_pdu_t *p_pdu, int *id, ql_sms_pdu_async_cb_f cb)
```

**参数**

| 参数    | 方向   | 类型                    | 描述                                               |
|---------|--------|-------------------------|----------------------------------------------------|
| `p_pdu` | [In]   | `ql_sms_pdu_t *`        | PDU 短信结构体指针，详见 §2.3.8.1                 |
| `id`    | [Out]  | `int *`                 | 异步发送 PDU 短信事件 ID，用于在回调中标识该事件  |
| `cb`    | [In]   | `ql_sms_pdu_async_cb_f` | 异步发送 PDU 短信回调函数，详见 §2.3.9.1           |

**返回值**

| 返回值      | 描述                               |
|-------------|------------------------------------|
| `QL_ERR_OK` | 函数执行成功                       |
| 其他值      | 函数执行失败，错误码详见 `ql_type.h` |

---

##### 2.3.9.1 `ql_sms_pdu_async_cb_f` — 异步发送 PDU 短信回调函数类型

**描述**

该回调函数处理 PDU 短信异步发送结果。

**函数原型**

```c
typedef void (*ql_sms_pdu_async_cb_f)(int id, int result);
```

**参数**

| 参数     | 方向  | 类型  | 描述                                     |
|----------|-------|-------|------------------------------------------|
| `id`     | [In]  | `int` | 异步发送 PDU 短信事件 ID                 |
| `result` | [In]  | `int` | 发送结果：`0` = 成功，其他值 = 失败（详见 `ql_type.h`）|

---

#### 2.3.10 `ql_sms_set_pdu_recv_cb` — 设置 PDU 短信接收回调函数

**描述**

注册 PDU 短信接收回调函数，当模块收到 PDU 短信时，SDK 将调用此回调通知应用层。

**函数原型**

```c
int ql_sms_set_pdu_recv_cb(ql_sms_pdu_recv_cb_f cb)
```

**参数**

| 参数  | 方向  | 类型                   | 描述                                      |
|-------|-------|------------------------|-------------------------------------------|
| `cb`  | [In]  | `ql_sms_pdu_recv_cb_f` | PDU 短信接收回调函数，详见 §2.3.10.1      |

**返回值**

| 返回值      | 描述                               |
|-------------|------------------------------------|
| `QL_ERR_OK` | 函数执行成功                       |
| 其他值      | 函数执行失败，错误码详见 `ql_type.h` |

---

##### 2.3.10.1 `ql_sms_pdu_recv_cb_f` — PDU 短信接收回调函数类型

**描述**

该回调函数在收到 PDU 短信时由 SDK 调用，将 PDU 数据传递给应用层。

**函数原型**

```c
typedef void (*ql_sms_pdu_recv_cb_f)(ql_sms_pdu_t *p_pdu);
```

**参数**

| 参数    | 方向  | 类型             | 描述                              |
|---------|-------|------------------|-----------------------------------|
| `p_pdu` | [In]  | `ql_sms_pdu_t *` | PDU 短信结构体，详见 §2.3.8.1    |

---

#### 2.3.11 `ql_sms_set_service_error_cb` — 设置 SMS 服务异常回调函数

**描述**

注册 SMS 服务异常回调函数，当 SMS 服务发生异常时通知应用层。

**函数原型**

```c
int ql_sms_set_service_error_cb(ql_sms_service_error_cb_f cb)
```

**参数**

| 参数  | 方向  | 类型                         | 描述                                      |
|-------|-------|------------------------------|-------------------------------------------|
| `cb`  | [In]  | `ql_sms_service_error_cb_f`  | SMS 服务异常回调函数，详见 §2.3.11.1      |

**返回值**

| 返回值      | 描述                               |
|-------------|------------------------------------|
| `QL_ERR_OK` | 函数执行成功                       |
| 其他值      | 函数执行失败，错误码详见 `ql_type.h` |

**备注**

> 模块**当前只在 SMS 服务端异常退出时**才会调用注册的 SMS 服务异常回调函数。

---

##### 2.3.11.1 `ql_sms_service_error_cb_f` — SMS 服务异常回调函数类型

**描述**

该回调函数处理 SMS 服务异常情况。

**函数原型**

```c
typedef void (*ql_sms_service_error_cb_f)(int error);
```

**参数**

| 参数    | 方向  | 类型  | 描述                             |
|---------|-------|-------|----------------------------------|
| `error` | [In]  | `int` | 错误码，详见 `ql_type.h`         |

---

## 4. 数据结构与枚举类型汇总

### 4.1 数据结构总表

| 结构体名称                  | 用途                     | 所属章节   |
|-----------------------------|--------------------------|------------|
| `ql_sms_msg_t`              | 文本短信数据结构         | §2.3.5.1   |
| `ql_sms_timestamp_t`        | 文本短信时间戳           | §2.3.7.2   |
| `ql_sms_user_data_head_t`   | 长短信头部信息           | §2.3.7.3   |
| `ql_sms_pdu_t`              | PDU 短信数据结构         | §2.3.8.1   |

### 4.2 枚举类型总表

| 枚举名称                  | 用途             | 所属章节  |
|---------------------------|------------------|-----------|
| `QL_SMS_MSG_FORMAT_E`     | 文本短信格式     | §2.3.5.2  |
| `QL_SMS_PDU_FORMAT_E`     | PDU 短信格式     | §2.3.8.2  |

### 4.3 回调函数类型总表

| 回调函数类型                  | 用途                          | 所属章节   |
|-------------------------------|-------------------------------|------------|
| `ql_sms_msg_async_cb_f`       | 异步发送文本短信结果回调      | §2.3.6.1   |
| `ql_sms_msg_recv_cb_f`        | 文本短信接收回调              | §2.3.7.1   |
| `ql_sms_pdu_async_cb_f`       | 异步发送 PDU 短信结果回调     | §2.3.9.1   |
| `ql_sms_pdu_recv_cb_f`        | PDU 短信接收回调              | §2.3.10.1  |
| `ql_sms_service_error_cb_f`   | SMS 服务异常回调              | §2.3.11.1  |

### 4.4 完整结构体与枚举速查

```c
/* ===== 文本短信格式枚举 ===== */
typedef enum {
    QL_SMS_MSG_FORMAT_GSM_7BIT      = 0,   // GSM 7-bit（ISO8859-1 / ASCII 兼容）
    QL_SMS_MSG_FORMAT_BINARY_DATA   = 1,   // 二进制数据
    QL_SMS_MSG_FORMAT_UCS2          = 2,   // UCS-2（Unicode，支持中文）
} QL_SMS_MSG_FORMAT_E;

/* ===== PDU 短信格式枚举 ===== */
typedef enum {
    QL_SMS_PDU_FORMAT_CDMA   = 0,   // CDMA（当前模块不支持）
    QL_SMS_PDU_FORMAT_GW_PP  = 6,   // GSM & WCDMA 点对点
} QL_SMS_PDU_FORMAT_E;

/* ===== 文本短信结构体 ===== */
typedef struct {
    QL_SMS_MSG_FORMAT_E format;                   // 短信格式
    char addr[QL_SMS_MAX_ADDR_LENGTH + 1];        // 地址（发送时为接收方，接收时为发送方）
    int content_size;                              // 内容长度（字节）
    char content[QL_SMS_MAX_SEND_MSG_LENGTH];     // 短信内容
} ql_sms_msg_t;

/* ===== 时间戳结构体 ===== */
typedef struct {
    uint8_t year;       // 年（从2000年起计，如19=2019年）
    uint8_t month;      // 月
    uint8_t day;        // 日
    uint8_t hours;      // 时（0~23）
    uint8_t minutes;    // 分
    uint8_t seconds;    // 秒
    uint8_t timezone;   // 时区（单位：1/4小时，如东八区=+32）
} ql_sms_timestamp_t;

/* ===== 长短信头部结构体 ===== */
typedef struct {
    uint8_t valid;        // 1=有效（是长短信），0=无效（不是长短信）
    uint8_t total_seg;    // 长短信总段数
    uint8_t cur_seg_num;  // 本段编号
    uint8_t ref_num;      // 长短信唯一标识号
} ql_sms_user_data_head_t;

/* ===== PDU 短信结构体 ===== */
typedef struct {
    QL_SMS_PDU_FORMAT_E format;                  // PDU 格式
    int content_size;                             // PDU 内容长度（字节）
    char content[QL_SMS_MAX_SEND_PDU_LENGTH];    // PDU 原始内容
} ql_sms_pdu_t;

/* ===== 回调函数类型 ===== */
typedef void (*ql_sms_msg_async_cb_f)(int id, int result);

typedef void (*ql_sms_msg_recv_cb_f)(ql_sms_msg_t *p_msg,
                                     ql_sms_timestamp_t *timestamp,
                                     ql_sms_user_data_head_t *head);

typedef void (*ql_sms_pdu_async_cb_f)(int id, int result);

typedef void (*ql_sms_pdu_recv_cb_f)(ql_sms_pdu_t *p_pdu);

typedef void (*ql_sms_service_error_cb_f)(int error);
```

---

## 5. 完整代码示例

> **示例来源**：`sample/test_sdk_api/main.c`，用户可自行查看接口函数的完整示例。
>
> **运行前提**：
> 1. 运行示例前必须调用 `ql_sms_init()` 初始化 SMS
> 2. 程序退出前或不使用 SMS 时，必须调用 `ql_sms_deinit()` 释放资源

---

### 5.1 获取短信中心号码

```c
void item_ql_sms_get_service_center_addr(void)
{
    int ret = 0;
    char addr[QL_SMS_MAX_ADDR_LENGTH] = {0};

    printf("test ql_sms_get_service_center_addr: ");
    ret = ql_sms_get_service_center_addr(addr, sizeof(addr));
    if (ret == QL_ERR_OK)
    {
        printf("ok\nservice center addr: %s\n", addr);
    }
    else
    {
        printf("failed, ret = %d\n", ret);
    }
}
```

**说明**：分配足够大的缓冲区（至少 `QL_SMS_MAX_ADDR_LENGTH` 字节），调用后打印短信中心号码字符串。

---

### 5.2 发送文本短信（同步，支持英文/二进制/中文）

> **说明**：异步发送文本短信（`ql_sms_send_msg_async`）与以下示例类似，用户可自行参看。

```c
void item_ql_sms_send_msg(void)
{
    int ret = 0;
    char buf[QL_SMS_MAX_SEND_MSG_LENGTH] = {0};
    int len = 0;
    int v = 0;

    ql_sms_msg_t *p_msg = calloc(1, sizeof(*p_msg));
    if (NULL == p_msg)
    {
        printf("out of memory\n");
        return;
    }

    printf("test ql_sms_send_msg: \n");
    printf("please enter msg format(0-gsm7, 1-binary, 2-ucs2): ");
    scanf("%d", (int *)&p_msg->format);
    if (2 < p_msg->format || 0 > p_msg->format)
    {
        free(p_msg);
        printf("bad format\n");
        return;
    }

    printf("please enter phone number: ");
    scanf(" %s", p_msg->addr);
    getchar();

    printf("example binary content: 30 31 32 33 q\n");
    printf("please enter content(binary content(hex) end with `q'): ");

    switch (p_msg->format)
    {
        case QL_SMS_MSG_FORMAT_GSM_7BIT:
        case QL_SMS_MSG_FORMAT_UCS2:
            /* GSM7BIT / UCS2: 直接读取字符串输入 */
            fgets(buf, QL_SMS_MAX_SEND_MSG_LENGTH, stdin);
            len = strlen(buf);
            if ('\r' == buf[len-1] || '\n' == buf[len-1])
            {
                buf[len-1] = 0;
                len--;
            }
            break;

        case QL_SMS_MSG_FORMAT_BINARY_DATA:
            /* 二进制：逐个读取十六进制字节，以 'q' 结尾 */
            while (1 == scanf("%x", &v))
            {
                buf[len++] = v;
            }
            getchar();  // read 'q'
            getchar();  // read '\n'
            break;

        default:
            free(p_msg);
            return;
    }

    printf("raw content size: %d\n", len);

    if (QL_SMS_MSG_FORMAT_UCS2 == p_msg->format)
    {
        /* UCS2：需要将 UTF-8 字符串转换为 UCS-2 编码 */
        p_msg->content_size = str_utf8_to_ucs2((uint8_t *)buf, len,
                                                (uint8_t *)p_msg->content);
    }
    else
    {
        memcpy(p_msg->content, buf, len);
        p_msg->content_size = len;
    }

    printf("sending msg, %d bytes...", p_msg->content_size);
    ret = ql_sms_send_msg(p_msg);
    if (ret != QL_ERR_OK)
    {
        printf("failed, ret = %d\n", ret);
    }
    else
    {
        printf("ok\n");
    }

    free(p_msg);
}
```

**关键点说明**

| 格式              | 输入方式                             | SDK 处理                         |
|-------------------|--------------------------------------|----------------------------------|
| `GSM_7BIT` (0)    | 直接输入 ISO8859-1 / ASCII 字符串    | SDK 自动转换为 GSM 7-bit         |
| `BINARY_DATA` (1) | 逐字节输入十六进制（以 `q` 结尾）    | 直接使用原始字节                 |
| `UCS2` (2)        | 输入 UTF-8 字符串                    | 需调用 `str_utf8_to_ucs2()` 转换 |

---

### 5.3 接收文本短信

#### 5.3.1 接收回调函数实现

```c
static void sms_msg_recv_cb(
    ql_sms_msg_t *p_msg,
    ql_sms_timestamp_t *timestamp,
    ql_sms_user_data_head_t *head)
{
    if (NULL == p_msg)
    {
        return;
    }

    printf("\nnew message coming...\n");
    printf("who: %s\n", p_msg->addr);  // 发送方号码

    /* 打印时间戳 */
    if (NULL != timestamp)
    {
        printf("timestamp: %02hhu-%02hhu-%02hhu %02hhu:%02hhu:%02hhu+%hhu\n",
               timestamp->year,
               timestamp->month,
               timestamp->day,
               timestamp->hours,
               timestamp->minutes,
               timestamp->seconds,
               timestamp->timezone);
    }

    printf("content size: %d\n", p_msg->content_size);
    printf("content: ");

    switch (p_msg->format)
    {
        case QL_SMS_MSG_FORMAT_GSM_7BIT:
            /* GSM7BIT：SDK 已自动转换为 ISO8859-1，直接打印 */
            printf("%s\n", p_msg->content);
            break;

        case QL_SMS_MSG_FORMAT_BINARY_DATA:
            /* 二进制：逐字节以十六进制打印 */
        {
            int i = 0;
            for (i = 0; i < p_msg->content_size; i++)
            {
                printf("%02x ", p_msg->content[i]);
            }
            printf("\n");
        }
            break;

        case QL_SMS_MSG_FORMAT_UCS2:
            /* UCS2：需将 UCS-2 转换为 UTF-8 后打印 */
        {
            char *buf = calloc(1, QL_SMS_MAX_RECV_MSG_LENGTH * 2);
            if (NULL == buf)
            {
                return;
            }
            unicode_str_to_utf8_str((unsigned short *)p_msg->content,
                                    (uint8_t *)buf,
                                    QL_SMS_MAX_RECV_MSG_LENGTH * 2);
            printf("%s\n", buf);
            free(buf);
        }
            break;
    }

    /* 长短信信息处理 */
    if (NULL != head && 1 == head->valid)
    {
        printf("total segments: %d\n", head->total_seg);
        printf("current segment: %d\n", head->cur_seg_num);
        printf("reference number: %d\n", head->ref_num);
    }
}
```

#### 5.3.2 注册接收回调函数

```c
void item_ql_sms_set_msg_recv_cb(void)
{
    int ret = 0;

    printf("test ql_sms_set_msg_recv_cb: ");
    ret = ql_sms_set_msg_recv_cb(sms_msg_recv_cb);
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

#### 5.3.3 接收文本短信输出示例

```
new message coming...
who: +8613670949352
timestamp: 21-10-12 11:18:10+32
content size: 13
content: 30 31 32 33 q
```

**字段解读**

| 字段             | 示例值              | 含义                              |
|------------------|---------------------|-----------------------------------|
| `who`            | `+8613670949352`    | 发件人号码（国际格式）            |
| `timestamp`      | `21-10-12 11:18:10+32` | 2021年10月12日 11:18:10，时区东8区 |
| `content size`   | `13`                | 短信内容长度（字节）              |
| `content`        | `30 31 32 33 q`     | 二进制短信内容（十六进制显示）    |

---

### 5.4 发送 PDU 短信

> **说明**：异步发送 PDU 短信（`ql_sms_send_pdu_async`）与以下示例类似，用户可自行参看。

```c
void item_ql_sms_send_pdu(void)
{
    int ret = 0;
    int v = 0;
    ql_sms_pdu_t pdu = {0};

    printf("test ql_sms_send_pdu: \n");
    printf("please enter pdu format(0-CDMA, 6-GW-PP): ");
    scanf("%d", (int *)&pdu.format);

    if (QL_SMS_PDU_FORMAT_CDMA != pdu.format &&
        QL_SMS_PDU_FORMAT_GW_PP != pdu.format)
    {
        printf("bad format: %d\n", pdu.format);
        return;
    }

    getchar();
    /* GW-PP PDU 示例内容说明 */
    printf("example GW-PP pdu content: "
           "00 01 00 0b 81 71 26 21 53 46 f7 00 04 0A "
           "30 31 32 33 34 35 36 37 38 39 q\n");
    printf("please enter content(hex), end with `q': ");

    /* 逐字节读取 PDU 十六进制内容，以 'q' 结尾 */
    while (1 == scanf("%x", &v))
    {
        pdu.content[pdu.content_size++] = v;
    }
    getchar();  // read 'q'
    getchar();  // read '\n'

    printf("content size: %d\n", pdu.content_size);
    printf("sending pdu...");
    ret = ql_sms_send_pdu(&pdu);
    if (ret != QL_ERR_OK)
    {
        printf("failed, ret = %d\n", ret);
    }
    else
    {
        printf("ok\n");
    }
}
```

**GW-PP PDU 格式说明**（以示例 PDU 为例）

```
00 01 00 0b 81 71 26 21 53 46 f7 00 04 0A 30 31 32 33 34 35 36 37 38 39
```

| 字段     | 字节           | 说明                           |
|----------|----------------|--------------------------------|
| SMSC 长度| `00`           | 0 表示使用默认短信中心         |
| PDU 类型 | `01`           | SMS-SUBMIT                     |
| MR       | `00`           | 消息参考号                     |
| DA 长度  | `0b`           | 目标地址长度（11位数字）       |
| DA TON/NPI | `81`         | 类型/编号计划                  |
| DA 号码  | `71 26 21 53 46 f7` | 目标号码（BCD 编码）    |
| PID      | `00`           | 协议标识                       |
| DCS      | `04`           | 数据编码方案（8-bit 数据）     |
| UDL      | `0A`           | 用户数据长度（10 字节）        |
| UD       | `30 31 32 33 34 35 36 37 38 39` | 用户数据内容 |

---

### 5.5 接收 PDU 短信

#### 5.5.1 PDU 接收回调函数实现

```c
static void sms_pdu_recv_cb(ql_sms_pdu_t *p_pdu)
{
    int i = 0;

    printf("\nnew pdu coming...\n");
    if (NULL == p_pdu)
    {
        return;
    }

    printf("format: ");
    switch (p_pdu->format)
    {
        case QL_SMS_PDU_FORMAT_CDMA:
            printf("cdma\n");
            break;
        case QL_SMS_PDU_FORMAT_GW_PP:
            printf("gw-pp\n");
            break;
        default:
            printf("unknown\n");
            break;
    }

    printf("content size: %d\n", p_pdu->content_size);
    printf("content(hex): ");
    for (i = 0; i < p_pdu->content_size; i++)
    {
        printf("%02x ", p_pdu->content[i]);
    }
    printf("\n");
}
```

#### 5.5.2 注册 PDU 接收回调函数

```c
void item_ql_sms_set_pdu_recv_cb(void)
{
    int ret = 0;

    printf("test ql_sms_set_pdu_recv_cb: ");
    ret = ql_sms_set_pdu_recv_cb(sms_pdu_recv_cb);
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

#### 5.5.3 接收 PDU 短信输出示例

```
format: gw-pp
content size: 37
content(hex): 08 91 68 31 08 70 75 05 F0 04 0d 01 68 31 76 90 04 53 f2 00 08 12 01 41 51 34 95 23 08 00 71 00 71 00 71

new message coming from sim_id:0...
who: +8613670949352
timestamp: 21-10-14 15:45:59+32
content size: 8
content: qqqq
```

**输出字段解读**

| 字段            | 示例值                          | 说明                             |
|-----------------|---------------------------------|----------------------------------|
| `format`        | `gw-pp`                         | GSM/WCDMA 点对点格式             |
| `content size`  | `37`                            | PDU 原始数据总长度（字节）       |
| `content(hex)`  | `08 91 68...`                   | PDU 原始字节（十六进制）         |
| `who`           | `+8613670949352`                | 发件人号码                       |
| `timestamp`     | `21-10-14 15:45:59+32`          | 2021年10月14日 15:45:59，东8区  |
| `content size`  | `8`                             | 解码后短信内容长度               |
| `content`       | `qqqq`                          | 解码后短信内容                   |

---

## 6. 附录：参考文档与术语缩写

### 表 2：参考文档（原文 Table 2）

| 序号 | 文档名称                                              |
|------|-------------------------------------------------------|
| [1]  | Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导           |

> 本文档第 1 章"引言"中提及的 QuecOpen® 详细信息，请参考文档 [1]。

### 表 3：术语缩写（原文 Table 3）

| 缩写   | 英文全称                                            | 中文描述                                         |
|--------|-----------------------------------------------------|--------------------------------------------------|
| API    | Application Programming Interface                   | 应用程序编程接口                                 |
| ASCII  | American Standard Code for Information Interchange  | 美国信息交换标准代码                             |
| CDMA   | Code-Division Multiple Access                       | 码分多址（利用码序列相关性实现的多址通信）       |
| GSM    | Global System for Mobile Communications             | 全球移动通信系统                                 |
| IoT    | Internet of Things                                  | 物联网                                           |
| ISO    | International Standard Organization                 | 国际标准化组织                                   |
| PDU    | Protocol Data Unit                                  | 协议数据单元                                     |
| SDK    | Software Development Kit                            | 软件开发工具包                                   |
| SMS    | Short Message Service                               | 短信息服务                                       |
| UCS    | Universal Character Set                             | 通用字符集                                       |
| WCDMA  | Wideband Code Division Multiple Access              | 宽带码分多址                                     |

---

## 7. 综合使用指南与注意事项汇总

### 7.1 标准使用流程

#### 发送短信流程

```
程序启动
    │
    ▼
ql_sms_init()          ← 必须首先调用
    │
    ├─ 发送文本短信（同步）：ql_sms_send_msg()
    ├─ 发送文本短信（异步）：ql_sms_send_msg_async()
    ├─ 发送 PDU 短信（同步）：ql_sms_send_pdu()
    └─ 发送 PDU 短信（异步）：ql_sms_send_pdu_async()
    │
    ▼
ql_sms_deinit()        ← 退出前必须调用
```

#### 接收短信流程

```
程序启动
    │
    ▼
ql_sms_init()
    │
    ├─ ql_sms_set_msg_recv_cb()   ← 注册文本短信接收回调
    ├─ ql_sms_set_pdu_recv_cb()   ← 注册 PDU 短信接收回调
    └─ ql_sms_set_service_error_cb() ← 注册服务异常回调
    │
    ▼
[等待消息到达，SDK 自动调用已注册的回调函数]
    │
    ▼
ql_sms_deinit()
```

### 7.2 关键注意事项总结

| 类别           | 注意事项                                                                 |
|----------------|--------------------------------------------------------------------------|
| **初始化顺序** | 使用任何 SMS API 前，**必须先调用** `ql_sms_init()`                     |
| **资源释放**   | 不再使用 SMS 时，**必须调用** `ql_sms_deinit()` 释放资源                |
| **并发限制**   | 所有 SMS API 函数**不支持并发调用**                                      |
| **回调限制**   | **不能在回调函数内部**调用 SMS API 函数                                  |
| **GSM 7-bit**  | 发送时用户提供 ISO8859-1 格式即可，SDK 自动转换为 GSM 7-bit             |
| **GSM 7-bit**  | 接收时 SDK 自动转换为 ISO8859-1 格式，用户无需手动转换                  |
| **UCS-2**      | 发送时需用户自行调用 `str_utf8_to_ucs2()` 将 UTF-8 转换为 UCS-2        |
| **UCS-2**      | 接收时需用户自行调用 `unicode_str_to_utf8_str()` 将 UCS-2 转换为 UTF-8 |
| **PDU 格式**   | 编码 PDU 时，**无需**在内容中包含短信中心号码                           |
| **CDMA 限制**  | EC200A-CN(TA) **不支持 CDMA 网络制式**，勿使用 `QL_SMS_PDU_FORMAT_CDMA`|
| **服务异常**   | 服务异常回调仅在 **SMS 服务端异常退出**时触发                           |
| **时区表示**   | `timezone` 字段单位为 1/4 小时；中国东8区 = 32                          |
| **时间年份**   | `year` 字段从 2000 年起计算；例如 `19` 表示 2019 年                     |
| **长短信**     | 通过 `ql_sms_user_data_head_t` 的 `ref_num` 识别同一条长短信的各片段   |

### 7.3 短信格式选择建议

| 场景                          | 推荐格式                       | 说明                              |
|-------------------------------|--------------------------------|-----------------------------------|
| 发送英文/数字短信             | `QL_SMS_MSG_FORMAT_GSM_7BIT`   | 编码效率最高，一条短信最多 160 字符|
| 发送中文/多语言短信           | `QL_SMS_MSG_FORMAT_UCS2`       | 支持 Unicode，需用户做编码转换    |
| 发送二进制数据（如 WAP PUSH）| `QL_SMS_MSG_FORMAT_BINARY_DATA`| 原始字节传输                      |
| 需要完全控制 PDU 内容         | PDU 模式（`ql_sms_send_pdu`）  | 适合高级用户，需手动构造 PDU      |

### 7.4 错误处理

- 所有 API 返回非 `QL_ERR_OK` 时，错误码定义详见 `ql_type.h`
- 异步接口中，`result` 参数为 `0` 表示成功，其他值为错误码（见 `ql_type.h`）
- 建议在关键操作中检查返回值并记录错误码，以便快速定位问题

### 7.5 内存管理建议

- 使用 `calloc` 分配 `ql_sms_msg_t` 结构体，确保所有字段初始化为 0
- 分配内存后应检查是否为 `NULL`（内存不足时的保护）
- 发送完成后立即调用 `free()` 释放动态分配的内存，避免内存泄漏
- UCS-2 接收时临时缓冲区大小建议为 `QL_SMS_MAX_RECV_MSG_LENGTH * 2`（每个 UCS-2 字符占 2 字节，转换为 UTF-8 后可能更长）

---

*文档分析完成。原文共 28 页，版本 V1.0.0（临时文件），2022-10-14。*
*分析整理：基于 Quectel EC200A-CN(TA) QuecOpen SMS API 参考手册全文内容。*
