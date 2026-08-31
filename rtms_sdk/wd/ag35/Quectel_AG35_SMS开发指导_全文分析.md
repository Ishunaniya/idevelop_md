# Quectel AG35-CET & AG35-EUT QuecOpen(SDK) SMS 开发指导 —— 全文详尽分析

> **源文档全名**：Quectel AG35-CET&AG35-EUT QuecOpen(SDK) SMS 开发指导
> **版本**：V1.0
> **日期**：2024-08-20
> **状态**：受控文件（Confidential）
> **适用模块**：AG35-CET、AG35-EUT（LTE Standard 模块系列）
> **总页数**：30 页（PDF 页脚标注 1/29 ~ 29/29，封面页不计入页脚编号）
> **本分析覆盖页范围**：全文 1–30 页（封面 + 正文 1~29）
> **分析定位**：本文档面向使用 QuecOpen(SDK) 方案、基于 Linux 嵌入式开发平台的客户，介绍 AG35 模块的 SMS（短信）C 语言 API 及完整示例代码。

---

## 0. 文档元信息与背景

### 0.1 文档历史（修订记录）

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| -（草稿） | 2023-07-08 | Chann YE | 文档创建 |
| 1.0 | 2024-08-20 | Lyndsay XIE | 受控版本：1. 新增适用模块 AG35-EUT；2. 基于 QuecOpen 方案统一命名，更新文档名称。 |

**解读**：本文档最初（2023-07）只针对 AG35-CET 创建，V1.0 正式受控时把适用范围扩展到 AG35-EUT，并把命名统一到 QuecOpen 体系。两个型号共用同一套 SMS API，差异不在 SMS 层面。

### 0.2 平台背景（第 1 章 引言）

- AG35-CET 与 AG35-EUT 模块支持 **QuecOpen®** 方案。
- QuecOpen® 是**基于 Linux 的嵌入式开发平台**，用于简化 IoV（车联网）应用的软件设计和开发过程。
- 本文档适用于 **SDK 构建环境**的 QuecOpen® 方案，主要介绍这两款模块的 **SMS API 及相关示例**。
- QuecOpen 详细信息参见参考文档 [1]（见附录）。

### 0.3 文档结构总览（目录）

文档正文分 4 章：
1. **引言**（第 6 页）
2. **SMS API**（第 7–20 页）—— 头文件、函数概览、函数详解（11 个 API + 多个回调类型 + 数据结构 + 枚举）、错误码及恢复措施
3. **示例**（第 21–28 页）—— 5 个完整示例：获取短信中心号码、发送文本短信、接收文本短信、发送 PDU 短信、接收 PDU 短信
4. **附录**（第 29 页）—— 参考文档及术语缩写

表格索引共 4 张：表 1 函数概览（p7）、表 2 错误码及恢复措施（p19）、表 3 参考文档（p29）、表 4 术语缩写（p29）。

---

## 1. SMS API 总论（第 2 章）

### 1.1 头文件（2.1）

- SMS API 头文件为 **`ql_sms.h`**。
- 位置：SDK 的 **`ql-sysroots/usr/include/ql-sdk/`** 目录下。
- 若无特别说明，本文档涉及的头文件均在该目录下（例如错误码定义所在的 `ql_type.h` 也在此处）。

### 1.2 函数概览（2.2，表 1）

全部 11 个 SMS API 函数：

| 函数 | 说明 |
|---|---|
| `ql_sms_init()` | 初始化 SMS 服务 |
| `ql_sms_deinit()` | 注销 SMS 服务 |
| `ql_sms_set_service_center_addr()` | 设置短信中心号码 |
| `ql_sms_get_service_center_addr()` | 获取短信中心号码 |
| `ql_sms_send_msg()` | 发送文本短信（同步） |
| `ql_sms_send_msg_async()` | 异步发送文本短信 |
| `ql_sms_set_msg_recv_cb()` | 设置文本短信接收回调函数 |
| `ql_sms_send_pdu()` | 发送 PDU 短信（同步） |
| `ql_sms_send_pdu_async()` | 异步发送 PDU 短信 |
| `ql_sms_set_pdu_recv_cb()` | 设置 PDU 短信接收回调函数 |
| `ql_sms_set_service_error_cb()` | 设置 SMS 服务异常回调函数 |

**关键备注（并发限制）**：
> 若无特别说明，本文档所述函数均**不支持并发调用**，并且**不能在相关回调函数中调用以上函数**。

**解读**：这是一条贯穿全套 API 的硬约束。SMS API 非线程安全、非可重入——不能多线程同时调用，也不能在接收回调 / 异步发送结果回调 / 服务异常回调里反过来调用任意 SMS API。实际开发中常见误用是“收到短信后在 recv 回调里直接调用 `ql_sms_send_msg` 回复”，按此约束这是被禁止的，必须把发送动作投递到独立线程/队列里执行。

---

## 2. 函数详解（2.3）

### 2.1 `ql_sms_init`（2.3.1）—— 初始化 SMS 服务

- **函数原型**：
  ```c
  int ql_sms_init(void)
  ```
- **参数**：无
- **返回值**：
  - `QL_ERR_OK`：函数执行成功
  - 其他值：函数执行失败；错误码详见 `ql_type.h`
- **备注**：使用其他 SMS API 前，**必须**调用本函数初始化 SMS 服务。

**解读**：所有 SMS 操作的前置入口。务必检查返回值；初始化失败时不应继续调用其他 API。

### 2.2 `ql_sms_deinit`（2.3.2）—— 注销 SMS 服务

- **函数原型**：
  ```c
  int ql_sms_deinit(void)
  ```
- **参数**：无
- **返回值**：`QL_ERR_OK` 成功；其他值失败（错误码见 `ql_type.h`）
- **备注**：若不再使用 SMS，**必须**调用本函数注销 SMS 服务以**释放资源**。

**解读**：与 `ql_sms_init` 成对出现的资源管理函数。配合错误码表，`QL_ERR_UNKNOWN` 的恢复手段之一就是 deinit 后重新 init。

### 2.3 `ql_sms_set_service_center_addr`（2.3.3）—— 设置短信中心号码

- **函数原型**：
  ```c
  int ql_sms_set_service_center_addr(char *addr, int len)
  ```
- **参数**：
  | 方向 | 参数 | 含义 |
  |---|---|---|
  | [In] | `addr` | 短信中心号码 |
  | [In] | `len` | 短信中心号码长度，单位：字节 |
- **返回值**：`QL_ERR_OK` 成功；其他值失败（错误码见 `ql_type.h`）

**解读**：短信中心号码（SMSC）是运营商提供的短信网关号码。一般 SIM 卡内已写入，多数场景无需手动设置；当 SIM 内 SMSC 缺失或发送失败时可手动设置。

### 2.4 `ql_sms_get_service_center_addr`（2.3.4）—— 获取短信中心号码

- **函数原型**：
  ```c
  int ql_sms_get_service_center_addr(char *addr, int len)
  ```
- **参数**：
  | 方向 | 参数 | 含义 |
  |---|---|---|
  | [Out] | `addr` | 短信中心号码（输出缓冲区） |
  | [In] | `len` | 短信中心号码长度，单位：字节（即调用方提供的缓冲区长度） |
- **返回值**：`QL_ERR_OK` 成功；其他值失败（错误码见 `ql_type.h`）

**解读**：`addr` 为输出参数，调用方需预先分配缓冲区并通过 `len` 告知容量。示例 3.1 即演示该函数用法。

### 2.5 `ql_sms_send_msg`（2.3.5）—— 同步发送文本短信

- **函数原型**：
  ```c
  int ql_sms_send_msg(ql_sms_msg_t *p_msg)
  ```
- **参数**：
  - `p_msg` [In]：文本短信，详见 2.3.5.1 `ql_sms_msg_t`
- **返回值**：`QL_ERR_OK` 成功；其他值失败（错误码见 `ql_type.h`）
- **重要备注（编码自动转换）**：
  > 为方便用户使用，当短信息格式为 **GSM 7-bit**（`QL_SMS_MSG_FORMAT_GSM_7BIT`）时，用户输入 **ISO8859-1** 格式的短信息即可，无需转换成 GSM 7-bit 格式，该函数将自动进行格式转换。ISO8859-1 编码向下兼容 ASCII 编码。

**解读**：这是发送端的一个重要便利特性——用户用熟悉的 ISO8859-1/ASCII 文本即可，SDK 内部完成到 GSM 7-bit 的打包。注意这是**同步阻塞**接口，函数返回即代表发送结果。

#### 2.5.1 `ql_sms_msg_t`（2.3.5.1）—— 文本短信结构体

```c
typedef struct
{
    QL_SMS_MSG_FORMAT_E format;
    char addr[QL_SMS_MAX_ADDR_LENGTH + 1];
    int content_size;
    char content[QL_SMS_MAX_SEND_MSG_LENGTH];
} ql_sms_msg_t
```

参数表：

| 类型 | 参数 | 描述 |
|---|---|---|
| `QL_SMS_MSG_FORMAT_E` | `format` | 文本短信格式，详见 2.3.5.2 章 |
| `char` | `addr` | 发送短信时为**接收方地址**；接收短信时为**发送方地址** |
| `int` | `content_size` | 文本短信长度。范围：**0~1440**；单位：字节 |
| `char` | `content` | 文本短信内容 |

**解读**：
- `addr` 字段大小为 `QL_SMS_MAX_ADDR_LENGTH + 1`（+1 为字符串结束符预留），发收双向复用同一结构。
- `content_size` 上限 **1440 字节**，远超单条短信的 140 字节上限，说明 SDK 支持**长短信（多段拼接）自动分段发送**。
- `content` 缓冲区大小为 `QL_SMS_MAX_SEND_MSG_LENGTH`。

#### 2.5.2 `QL_SMS_MSG_FORMAT_E`（2.3.5.2）—— 文本短信格式枚举

```c
typedef enum
{
    QL_SMS_MSG_FORMAT_GSM_7BIT      = 0,
    QL_SMS_MSG_FORMAT_BINARY_DATA   = 1,
    QL_SMS_MSG_FORMAT_UCS2          = 2,
} QL_SMS_MSG_FORMAT_E
```

成员说明：

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_SMS_MSG_FORMAT_GSM_7BIT` | 0 | GSM 7-bit（标准 ASCII/拉丁文本，每字符 7bit，单段 160 字符） |
| `QL_SMS_MSG_FORMAT_BINARY_DATA` | 1 | 二进制短信（8-bit 数据，单段 140 字节） |
| `QL_SMS_MSG_FORMAT_UCS2` | 2 | UCS-2 编码短信（用于中文等非拉丁字符，单段 70 字符） |

**解读**：发送中文须使用 `UCS2`；纯英文/数字用 `GSM_7BIT` 最省空间；传输任意字节流用 `BINARY_DATA`。三种格式决定单段容量与是否触发编码转换。

### 2.6 `ql_sms_send_msg_async`（2.3.6）—— 异步发送文本短信

- **函数原型**：
  ```c
  int ql_sms_send_msg_async(ql_sms_msg_t *p_msg, int *id, ql_sms_msg_async_cb_f cb)
  ```
- **参数**：
  | 方向 | 参数 | 含义 |
  |---|---|---|
  | [In] | `p_msg` | 文本短信，详见 2.3.5.1 章 |
  | [Out] | `id` | 异步发送文本短信事件 ID，用于在回调函数中标识本次短信事件 |
  | [In] | `cb` | 异步发送文本短信回调函数，用于接收发送结果，详见 2.3.6.1 章 |
- **返回值**：`QL_ERR_OK` 成功；其他值失败（错误码见 `ql_type.h`）
- **备注**：与同步版相同的编码自动转换说明——格式为 GSM 7-bit 时输入 ISO8859-1 即可，函数自动转换；ISO8859-1 向下兼容 ASCII。

**解读**：异步版本立即返回并通过 `id` 关联后续回调，发送的最终结果由 `cb` 异步告知。`id` 是输出参数，调用方可保存它以在回调里区分多次发送。

#### 2.6.1 `ql_sms_msg_async_cb_f`（2.3.6.1）—— 文本短信异步发送结果回调

- **函数原型**：
  ```c
  typedef void (*ql_sms_msg_async_cb_f)(int id, int result)
  ```
- **参数**：
  | 方向 | 参数 | 含义 |
  |---|---|---|
  | [In] | `id` | 异步发送文本短信事件 ID（与 `ql_sms_send_msg_async` 输出的 `id` 对应） |
  | [In] | `result` | 异步发送结果：`0` = 成功；其他值 = 失败（错误码见 `ql_type.h`） |

**解读**：回调里通过 `id` 匹配具体哪一次发送、通过 `result` 判定成败。注意前述并发约束——不能在此回调里调用 SMS API。

### 2.7 `ql_sms_set_msg_recv_cb`（2.3.7）—— 设置文本短信接收回调

- **函数原型**：
  ```c
  int ql_sms_set_msg_recv_cb(ql_sms_msg_recv_cb_f cb)
  ```
- **参数**：`cb` [In]：文本短信接收回调函数，详见 2.3.7.1 章
- **返回值**：`QL_ERR_OK` 成功；其他值失败（错误码见 `ql_type.h`）

#### 2.7.1 `ql_sms_msg_recv_cb_f`（2.3.7.1）—— 文本短信接收回调类型

- **函数原型**：
  ```c
  typedef void (*ql_sms_msg_recv_cb_f)(ql_sms_msg_t *p_msg,
                                       ql_sms_timestamp_t *timestamp,
                                       ql_sms_user_data_head_t *head)
  ```
- **参数**：
  | 方向 | 参数 | 含义 |
  |---|---|---|
  | [In] | `p_msg` | 文本短信，详见 2.3.5.1 章（此处 `addr` 为发送方地址） |
  | [In] | `timestamp` | 文本短信时间戳，详见 2.3.7.2 章 |
  | [In] | `head` | 长短信头部信息，用于标识长短信中的每个子短信，详见 2.3.7.3 章 |
- **重要备注（接收端编码自动转换）**：
  > 为方便用户使用，当短信息格式为 GSM 7-bit（`QL_SMS_MSG_FORMAT_GSM_7BIT`）时，**SDK 会自动将短信息转换成 ISO8859-1 格式**，用户无需自己转换。ISO8859-1 编码向下兼容 ASCII 编码。

**解读**：接收方向同样做了自动解码——收到 GSM 7-bit 短信时 SDK 直接给出 ISO8859-1 文本。`head` 用于长短信重组：同一条长短信被拆成多段，每段携带相同 `ref_num`、不同 `cur_seg_num`，应用需自行按 `total_seg`/`cur_seg_num` 拼接。

#### 2.7.2 `ql_sms_timestamp_t`（2.3.7.2）—— 短信时间戳结构体

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
} ql_sms_timestamp_t
```

参数表：

| 类型 | 参数 | 描述 |
|---|---|---|
| `uint8_t` | `year` | 年。**自 2000 年起计算**，例如 19 表示 2019 年 |
| `uint8_t` | `month` | 月 |
| `uint8_t` | `day` | 日 |
| `uint8_t` | `hours` | 小时。24 小时制，范围 0~23 |
| `uint8_t` | `minutes` | 分钟 |
| `uint8_t` | `seconds` | 秒 |
| `uint8_t` | `timezone` | 时区。**单位 1/4 小时**。例如中国位于 +8 区，其 `timezone` 为 **+32**（8×4=32） |

**解读**：该时间戳来自短信中心（SMSC）打的 TP-SCTS。两个易错点：① `year` 是相对 2000 年的偏移；② `timezone` 以 15 分钟（1/4 小时）为单位，换算实际时区须除以 4。这是 SMS PDU 规范中 SCTS 的标准表示。

#### 2.7.3 `ql_sms_user_data_head_t`（2.3.7.3）—— 长短信头部结构体

```c
typedef struct
{
    uint8_t valid;
    uint8_t total_seg;
    uint8_t cur_seg_num;
    uint8_t ref_num;
} ql_sms_user_data_head_t
```

参数表：

| 类型 | 参数 | 描述 |
|---|---|---|
| `uint8_t` | `valid` | 本结构体是否有效。`1` = 有效；`0` = 无效，即本条短信不是长短信 |
| `uint8_t` | `total_seg` | 长短信的总段数 |
| `uint8_t` | `cur_seg_num` | 本条短信的段数编号（当前是第几段） |
| `uint8_t` | `ref_num` | 本条短信的接收编号，为本条长短信的**唯一标识号**（同一长短信各段相同） |

**解读**：长短信重组的关键。逻辑：先看 `valid`，若为 0 则普通单段短信直接处理；若为 1，则用 `ref_num` 把属于同一长短信的各段归组，按 `cur_seg_num` 排序、收满 `total_seg` 段后拼接成完整内容。应用层需自行做缓存与重组（含超时丢弃缺段的处理）。

### 2.8 `ql_sms_send_pdu`（2.3.8）—— 同步发送 PDU 短信

- **函数原型**：
  ```c
  int ql_sms_send_pdu(ql_sms_pdu_t *p_pdu)
  ```
- **参数**：`p_pdu` [In]：PDU 短信，详见 2.3.8.1 章
- **返回值**：`QL_ERR_OK` 成功；其他值失败（错误码见 `ql_type.h`）

**解读**：PDU 方式由应用自行构造完整的 PDU 字节流，灵活但需要掌握 3GPP TS 23.040 PDU 格式。

#### 2.8.1 `ql_sms_pdu_t`（2.3.8.1）—— PDU 短信结构体

```c
typedef struct
{
    QL_SMS_PDU_FORMAT_E format;
    int content_size;
    char content[QL_SMS_MAX_SEND_PDU_LENGTH];
} ql_sms_pdu_t
```

参数表：

| 类型 | 参数 | 描述 |
|---|---|---|
| `QL_SMS_PDU_FORMAT_E` | `format` | PDU 短信格式，详见 2.3.8.2 章 |
| `int` | `content_size` | PDU 短信长度。范围 **0~255**；单位：字节 |
| `char` | `content` | PDU 短信内容（PDU 字节流） |

**解读**：与文本短信结构相比，PDU 结构无 `addr` 字段（目标地址已编码进 PDU 内部）。`content_size` 上限 255 字节，对应单条 PDU。

#### 2.8.2 `QL_SMS_PDU_FORMAT_E`（2.3.8.2）—— PDU 短信格式枚举

```c
typedef enum
{
    QL_SMS_PDU_FORMAT_CDMA   = 0,
    QL_SMS_PDU_FORMAT_GW_PP  = 6,
} QL_SMS_PDU_FORMAT_E
```

成员说明：

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_SMS_PDU_FORMAT_CDMA` | 0 | CDMA 制式下的 PDU 短信格式（**不支持**） |
| `QL_SMS_PDU_FORMAT_GW_PP` | 6 | GSM 和 WCDMA 制式下的**点对点短信消息协议**的 PDU 短信格式 |

**重要备注**：
> 编码 PDU 短信时，**无需添加短信中心号码**。

**解读**：AG35 实际只支持 `QL_SMS_PDU_FORMAT_GW_PP`（值 6），CDMA 格式仅列出但不可用。GW_PP 即 3GPP 的点对点（Point-to-Point）SMS。备注的“无需添加短信中心号码”很关键——这与标准 AT+CMGS 的 PDU 不同：标准 AT PDU 模式首字节是 SMSC 长度（可填 00 表示用默认），而这里 SDK 已替你处理 SMSC，PDU 内容从 TPDU 部分（首字节为 PDU-type/FO）开始构造。

### 2.9 `ql_sms_send_pdu_async`（2.3.9）—— 异步发送 PDU 短信

- **函数原型**：
  ```c
  int ql_sms_send_pdu_async(ql_sms_pdu_t *p_pdu, int *id, ql_sms_pdu_async_cb_f cb)
  ```
- **参数**：
  | 方向 | 参数 | 含义 |
  |---|---|---|
  | [In] | `p_pdu` | PDU 短信，详见 2.3.8.1 章 |
  | [Out] | `id` | 异步发送 PDU 短信事件 ID，用于在回调中标识本次 PDU 事件 |
  | [In] | `cb` | 异步发送 PDU 短信回调函数，详见 2.3.9.1 章 |
- **返回值**：`QL_ERR_OK` 成功；其他值失败（错误码见 `ql_type.h`）

#### 2.9.1 `ql_sms_pdu_async_cb_f`（2.3.9.1）—— PDU 异步发送结果回调

- **函数原型**：
  ```c
  typedef void (*ql_sms_pdu_async_cb_f)(int id, int result)
  ```
- **参数**：
  | 方向 | 参数 | 含义 |
  |---|---|---|
  | [In] | `id` | 异步发送 PDU 短信事件 ID |
  | [In] | `result` | 异步发送 PDU 短信结果：`0` = 成功；其他值 = 失败（错误码见 `ql_type.h`） |

**解读**：与文本短信异步回调结构完全一致，仅语义为 PDU。

### 2.10 `ql_sms_set_pdu_recv_cb`（2.3.10）—— 设置 PDU 短信接收回调

- **函数原型**：
  ```c
  int ql_sms_set_pdu_recv_cb(ql_sms_pdu_recv_cb_f cb)
  ```
- **参数**：`cb` [In]：PDU 短信接收回调函数，详见 2.3.10.1 章
- **返回值**：`QL_ERR_OK` 成功；其他值失败（错误码见 `ql_type.h`）

#### 2.10.1 `ql_sms_pdu_recv_cb_f`（2.3.10.1）—— PDU 短信接收回调类型

- **函数原型**：
  ```c
  typedef void (*ql_sms_pdu_recv_cb_f)(ql_sms_pdu_t *p_pdu)
  ```
- **参数**：`p_pdu` [In]：PDU 短信，详见 2.3.8.1 章

**解读**：PDU 接收回调只给出原始 PDU 结构（不像文本接收回调那样附带 timestamp 和长短信头），因为时间戳、长短信头等信息都已编码在 PDU 字节流内部，需要应用自行解析 PDU 提取。

### 2.11 `ql_sms_set_service_error_cb`（2.3.11）—— 设置 SMS 服务异常回调

- **函数原型**：
  ```c
  int ql_sms_set_service_error_cb(ql_sms_service_error_cb_f cb)
  ```
- **参数**：`cb` [In]：SMS 服务异常回调函数，详见 2.3.11.1 章
- **返回值**：`QL_ERR_OK` 成功；其他值失败（错误码见 `ql_type.h`）
- **重要备注**：
  > 模块当前只在 **SMS 服务端异常退出时**才会调用注册的 SMS 服务异常回调函数。

**解读**：这是健壮性兜底——SMS 服务进程异常退出时触发，应用可在此回调里执行 `ql_sms_deinit()`+`ql_sms_init()` 重新拉起服务（但要注意并发约束：不能直接在回调里调用 SMS API，应投递到其他线程处理）。

#### 2.11.1 `ql_sms_service_error_cb_f`（2.3.11.1）—— SMS 服务异常回调类型

- **函数原型**：
  ```c
  typedef void (*ql_sms_service_error_cb_f)(int error)
  ```
- **参数**：`error` [In]：错误码，详见 `ql_type.h`

---

## 3. 错误码及恢复措施（2.4，表 2 —— 部分，续见后页）

常见 SMS API 错误码及恢复措施（本段覆盖到 p19 显示的前 4 个）：

| 返回值 | 错误码 | 含义 | 恢复措施 |
|---|---|---|---|
| -1003 | `QL_ERR_UNKNOWN` | 未知错误 | 调用 `ql_sms_deinit()` 后再调用 `ql_sms_init()` 重新初始化 SMS 服务后重试。若仍报错，则需抓取调用 API 全过程的 log 并提交至移远通信技术支持 |
| -1006 | `QL_ERR_UNSUPPORTED` | 不支持 | 当前操作不支持，请检查参数等是否正确，然后重试。若仍报错，则需抓取调用 API 全过程的 log 并提交至移远通信技术支持 |
| -1010 | `QL_ERR_INVALID_ARG` | API 参数无效 | 保证 API 参数有效性后重试 |
| -1030 | `QL_ERR_NO_MEM` | 内存不足 | 检查系统内存使用率，确认占用内存异常的进程是否有内存泄漏，或主动释放一些内存后重试 |
| -1034 | `QL_ERR_NOT_INIT` | 未做初始化 | 先调用 `ql_sms_init()` 初始化 SMS 服务后重试 |
| -1067 | `QL_ERR_SERVICE_NOT_READY` | 服务未就绪 | 重复调用 `ql_sms_init()`，尝试一段时间（建议 30 秒），若仍然失败，则需抓取调用 API 全过程的 log 并提交至移远通信技术支持 |
| -1078 | `QL_ERR_SMSC_ADDR` | 短信服务中心地址错误 | 设置正确的短信服务中心地址后重试。若仍报错，则需抓取调用 API 全过程的 log 并提交至移远通信技术支持 |
| -1099 | `QL_ERR_SERVICE_ABORT` | 服务异常退出 | 调用 `ql_sms_deinit()` 后再调用 `ql_sms_init()`，待初始化 SMS 服务成功后重试 |

**错误码恢复策略小结（解读）**：
- **参数类**（-1010 `INVALID_ARG`、-1078 `SMSC_ADDR`）：应用自查参数/SMSC，可立即纠正。
- **状态类**（-1034 `NOT_INIT`、-1067 `SERVICE_NOT_READY`、-1099 `SERVICE_ABORT`、-1003 `UNKNOWN`）：核心修复手段是 `deinit→init` 重新拉起 SMS 服务，`SERVICE_NOT_READY` 还需带 ~30s 重试窗口（服务端启动需要时间）。
- **资源类**（-1030 `NO_MEM`）：排查内存泄漏后重试。
- **能力类**（-1006 `UNSUPPORTED`）：当前操作/参数不被支持。
- 所有“仍报错”的兜底动作都是统一的：抓取全过程 log 提交移远技术支持。这些错误码的数值在 `ql_type.h` 中定义，是 QuecOpen 全 SDK 通用的错误码体系（非 SMS 专属），故 SMS 文档只列出 SMS 场景常见的子集。

---

## 4. 示例（第 3 章，p21–p28）

> 本章所有代码示例均摘自 SDK 中的 **`sample/sms/main.c`**，用户可自行查看接口函数的完整示例。
>
> **运行前提（两条硬性注意）**：
> 1. 运行示例文件前**必须**调用 `ql_sms_init()` 初始化 SMS 服务；
> 2. 程序退出前或不使用 SMS 时，**必须**调用 `ql_sms_deinit()` 释放资源。

### 4.1 获取短信中心号码（3.1）

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

**解读**：标准的“分配缓冲区 → 传入 `sizeof` → 判 `QL_ERR_OK`”模式。`addr` 缓冲区按 `QL_SMS_MAX_ADDR_LENGTH` 分配，返回的 SMSC 是 ASCII 字符串可直接 `%s` 打印。

### 4.2 发送文本短信（3.2）

该示例演示发送**英文（GSM 7-bit）、二进制（binary）和中文（UCS2）** 三种短信。文档说明：异步发送（`ql_sms_send_msg_async`）与此示例类似，用户可自行参考。

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
        printf("out of memery\n");
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
            fgets(buf, QL_SMS_MAX_SEND_MSG_LENGTH, stdin);
            len = strlen(buf);
            if ('\r' == buf[len-1] || '\n' == buf[len-1])
            {
                buf[len-1] = 0;
                len--;
            }
            break;
        case QL_SMS_MSG_FORMAT_BINARY_DATA:
            while (1 == scanf("%x", &v))
            {
                buf[len++] = v;
            }
            getchar();   // read `q'
            getchar();   // read '\n'
            break;
        default:
            free(p_msg);
            return;
    }
    printf("raw content size: %d\n", len);
    if (QL_SMS_MSG_FORMAT_UCS2 == p_msg->format)
    {
        p_msg->content_size = str_utf8_to_ucs2((uint8_t *)buf, len, (uint8_t *)p_msg->content);
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

**逐段解读**：
- **结构体动态分配**：`ql_sms_msg_t` 体积较大（含 `content[QL_SMS_MAX_SEND_MSG_LENGTH]`），示例用 `calloc` 堆分配并在所有出口 `free`，避免大对象压栈。
- **格式校验**：`format` 必须落在 0~2（与 `QL_SMS_MSG_FORMAT_E` 枚举一致），否则 `bad format` 返回。
- **三种格式的内容输入**：
  - GSM_7BIT 与 UCS2 共用 `fgets` 读一行文本，并去掉行尾 `\r`/`\n`。
  - BINARY_DATA 用 `scanf("%x", ...)` 循环读十六进制字节（例：`30 31 32 33` 即 "0123" 的 ASCII），以 `q` 结束（示例提示串 `example binary content: 30 31 32 33 q`）。
- **编码转换是应用职责（关键）**：
  - **UCS2**：示例调用 **`str_utf8_to_ucs2()`** 把用户输入的 UTF-8 文本转成 UCS-2，写入 `content` 并返回字节数。**注意**：这意味着发中文时应用要自己做 UTF-8→UCS-2 转换；SDK 的自动转换只覆盖 GSM 7-bit ↔ ISO8859-1，**不覆盖 UCS-2**。
  - **GSM_7BIT / BINARY**：直接 `memcpy`，`content_size = len`（GSM 7-bit 的 ISO8859-1→7bit 打包由 `ql_sms_send_msg` 内部完成，应用无需处理）。
- `str_utf8_to_ucs2` 是 SDK 提供的辅助函数（非 SMS API 表中函数，属工具函数）。

### 4.3 接收文本短信（3.3）

#### 4.3.1 接收回调函数实现

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
    printf("who: %s\n", p_msg->addr);
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
        printf("%s\n", p_msg->content);
        break;
    case QL_SMS_MSG_FORMAT_BINARY_DATA:
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
    {
        char *buf = calloc(1, QL_SMS_MAX_RECV_MSG_LENGTH * 2);
        if (NULL == buf)
        {
            return;
        }
        unicode_str_to_utf8_str((unsigned short *)p_msg->content, (uint8_t *)buf,
                                QL_SMS_MAX_RECV_MSG_LENGTH * 2);
        printf("%s\n", buf);
        free(buf);
    }
        break;
    }
    if (NULL != head && 1 == head->valid)
    {
        printf("totoal segments: %d\n", head->total_seg);
        printf("current segment: %d\n", head->cur_seg_num);
        printf("reference number: %d\n", head->ref_num);
    }
}
```

**逐段解读**：
- **入参判空**：`p_msg`、`timestamp`、`head` 都可能为 NULL，回调里逐一判空（`head` 还需 `head->valid==1` 才是长短信）。
- **时间戳打印**：用 `%02hhu`（uint8_t 两位）格式化，对应示例输出 `21-10-12 11:18:10+32`——即 2021-10-12 11:18:10，时区 +32（1/4 小时单位）= 东八区 UTC+8。
- **三种格式的内容解析**：
  - GSM_7BIT：`content` 已被 SDK 解为 ISO8859-1 文本，直接 `%s` 打印。
  - BINARY_DATA：按 `content_size` 逐字节 `%02x` 十六进制打印。
  - UCS2：调用 **`unicode_str_to_utf8_str()`**（与发送侧 `str_utf8_to_ucs2` 互逆）把 UCS-2 转回 UTF-8 再打印；临时缓冲按 `QL_SMS_MAX_RECV_MSG_LENGTH * 2` 分配（UTF-8 可能比 UCS-2 占更多字节，故 ×2）。
- **长短信信息**：`head->valid==1` 时打印总段数 / 当前段号 / 引用号（原文有 typo `totoal`）。

#### 4.3.2 注册接收回调

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

#### 4.3.3 接收文本短信结果示例（p25 截图）

```
new message coming...
who: +8613670949352
timestamp: 21-10-12 11:18:10+32
content size: 13
content: 30 31 32 33 q
```

**解读**：这是收到一条 **binary 格式**短信的输出（content 以十六进制逐字节展示）。`30 31 32 33` 对应 ASCII "0123"，后跟空格与 `q`，`content size: 13` 为字节数。时间戳 `+32` 再次印证时区单位为 1/4 小时。

### 4.4 发送 PDU 短信（3.4）

异步发送 PDU（`ql_sms_send_pdu_async`）与此示例类似，可自行参考。

```c
void item_ql_sms_send_pdu(void)
{
    int ret = 0;
    int v = 0;
    ql_sms_pdu_t pdu = {0};

    printf("test ql_sms_send_pdu: \n");
    printf("please enter pdu format(0-CDMA, 6-GW-PP): ");
    scanf("%d", (int *)&pdu.format);
    if (QL_SMS_PDU_FORMAT_CDMA != pdu.format && QL_SMS_PDU_FORMAT_GW_PP != pdu.format)
    {
        printf("bad format: %d\n", pdu.format);
        return;
    }

    getchar();
    printf("example GW-PP pdu content: 00 01 00 0b 81 71 26 21 53 46 f7 00 04 0A 30 31 32 33 34 35 36 37 38 39 q\n");
    printf("please enter content(hex), end with `q': ");
    while (1 == scanf("%x", &v))
    {
        pdu.content[pdu.content_size++] = v;
    }
    getchar();   // read `q'
    getchar();   // read '\n'

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

**逐段解读 + PDU 字段拆解**：
- 注意此处 `pdu` 是**栈上结构体**（`ql_sms_pdu_t pdu = {0}`），因为 PDU 结构 `content[QL_SMS_MAX_SEND_PDU_LENGTH]` 仅 255 字节级别，比文本短信结构小，可直接放栈。
- 格式只接受 0(CDMA, 实际不支持) 或 6(GW_PP)；实用值是 6。
- PDU 内容由 `scanf("%x")` 逐字节读入，以 `q` 结束。
- **示例 PDU 解析**（`00 01 00 0b 81 71 26 21 53 46 f7 00 04 0A 30 31 32 33 34 35 36 37 38 39`），按 3GPP TS 23.040 TPDU（SMS-SUBMIT）：
  - `00` = SMSC 长度（这里因为 SDK 自管 SMSC，首字节其实是 TPDU 起始；按文档“编码 PDU 无需添加 SMSC”，此 `00` 可理解为 PDU-type 字段的占位/FO）
  - `01` = TP-MR（Message Reference）
  - `00` = TP-DA 地址长度
  - `0b` = 11（被叫号码数字位数）
  - `81` = 号码类型（TON/NPI，81=未知格式/ISDN）
  - `71 26 21 53 46 f7` = 半字节交换（swap）后的被叫号码 → `17 62 12 35 64 7f` → 即号码 `17621235647`（f 为填充）
  - `00` = TP-PID（协议标识）
  - `04` = TP-DCS（数据编码方案，04=8bit 二进制）
  - `0A` = TP-UDL（用户数据长度 = 10 字节）
  - `30 31 32 33 34 35 36 37 38 39` = 用户数据 = ASCII "0123456789"

  （以上为对示例 PDU 的结构性解读；实际字段顺序遵循 SMS-SUBMIT PDU 规范，应用须自行按规范构造。）

### 4.5 接收 PDU 短信（3.5）

#### 4.5.1 PDU 接收回调函数实现

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

**解读**：PDU 接收回调只拿到 `p_pdu`（无单独的 timestamp/head 参数），把整条 PDU 以十六进制 dump 出来。时间戳、发送方号码、长短信头等都需要应用自行从这段 PDU 字节流里按规范解析。

#### 4.5.2 注册 PDU 接收回调

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

#### 4.5.3 接收 PDU 短信结果示例（p28 截图）

```
format: gw-pp
content size: 37
content(hex): 08 91 68 31 08 70 75 05 f0 04 0d 68 31 76 90 04 53 f2 00 08 12 01 41 51 54 05 23 08 00 71 00 71 00 71 00 71

new message coming from sim_id:0...
who: +8613670949352
timestamp: 21-10-14 15:45:59+32
content size: 8
content: qqqq
```

**解读**：
- 上半段是 PDU 接收回调输出：`format: gw-pp`，整条 PDU 37 字节十六进制 dump。可见 PDU 内含 SMSC 信息（`08 91 ...`，08=SMSC 长度、91=国际号码标志）、DCS=08（UCS-2）、用户数据 `00 71 00 71 00 71 00 71`（UCS-2 的 "qqqq"）。
- 下半段 `new message coming from sim_id:0...` 是**文本短信回调**同时被触发的输出（同一条短信既走 PDU 回调也走文本回调），`content: qqqq` 与 PDU 中 `0071 0071 0071 0071` 一致。`sim_id:0` 提示有多 SIM 概念（这里是 0 号卡）。

---

## 5. 附录（第 4 章，p29）

### 5.1 参考文档（表 3）

| 编号 | 文档名称 |
|---|---|
| [1] | Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导 |

### 5.2 术语缩写（表 4）

| 缩写 | 英文描述 | 中文描述 |
|---|---|---|
| API | Application Programming Interface | 应用程序编程接口 |
| ASCII | American Standard Code for Information Interchange | 美国信息交换标准代码 |
| CDMA | Code-Division Multiple Access | 码分多址 |
| GSM | Global System for Mobile Communications | 全球移动通信系统 |
| IoV | Internet of Vehicles | 车联网 |
| ISO | International Standard Organization | 国际标准化组织 |
| PDU | Protocol Data Unit | 协议数据单元 |
| SDK | Software Development Kit | 软件开发工具包 |
| SMS | Short Message Service | 短信息服务 |
| UCS | Universal Character Set | 通用字符集 |
| WCDMA | Wideband Code Division Multiple Access | 宽带码分多址 |

---

## 6. 总体技术要点与开发实践总结

1. **生命周期**：`ql_sms_init()`（必须最先）→ 业务 → `ql_sms_deinit()`（必须释放）。所有 API 失败都返回非 `QL_ERR_OK`，错误码在 `ql_type.h`。

2. **两种短信通道**：
   - **文本短信**（msg）：结构体带 `addr` 字段，SDK 处理 GSM 7-bit↔ISO8859-1 自动转换；UCS-2 的 UTF-8 转换需应用自己做（`str_utf8_to_ucs2`/`unicode_str_to_utf8_str`）；接收回调附带 timestamp 和长短信 head，适合一般业务。
   - **PDU 短信**（pdu）：应用自构造/自解析 PDU 字节流，无需填 SMSC，灵活但门槛高；接收回调只给原始 PDU。

3. **同步 vs 异步**：每种通道都有同步（阻塞返回结果）和异步（立即返回，结果经 `cb(id, result)` 回调）两套发送 API；异步用 `id` 关联请求与结果。

4. **三个回调维度**：发送结果回调、接收回调（文本/PDU 各一）、服务异常回调（服务端崩溃时触发，可在外部线程做 deinit+init 自愈）。

5. **核心约束（务必牢记）**：
   - **非并发、非可重入**：不能多线程并发调用，不能在任意回调里调用 SMS API。
   - **content_size 上限**：文本短信 0~1440 字节（支持长短信自动分段）；PDU 0~255 字节。
   - **长短信重组**：依赖 `ql_sms_user_data_head_t` 的 `valid/total_seg/cur_seg_num/ref_num`，应用层自行缓存拼接。
   - **时间戳两个坑**：`year` 自 2000 年偏移；`timezone` 单位为 1/4 小时（东八区 = 32）。
   - **PDU 格式**：仅 `QL_SMS_PDU_FORMAT_GW_PP`(=6) 可用，CDMA(=0) 不支持。

6. **编码转换对照表**：

   | 方向 | 格式 | 谁负责转换 |
   |---|---|---|
   | 发送 | GSM 7-bit | SDK（ISO8859-1→7bit 自动） |
   | 发送 | UCS-2 | 应用（`str_utf8_to_ucs2`） |
   | 发送 | Binary | 无（原样字节） |
   | 接收 | GSM 7-bit | SDK（7bit→ISO8859-1 自动） |
   | 接收 | UCS-2 | 应用（`unicode_str_to_utf8_str`） |
   | 接收 | Binary | 无（原样字节） |

7. **示例工程位置**：`sample/sms/main.c`；头文件 `ql_sms.h` 位于 `ql-sysroots/usr/include/ql-sdk/`。

---

<!-- GENERATION_COMPLETE -->
