# Quectel AG35-CET & AG35-EUT QuecOpen(SDK) (U)SIM 开发指导 — 全文分析

> **文档全名**：AG35-CET&AG35-EUT QuecOpen(SDK) (U)SIM 开发指导
> **适用模块**：LTE Standard 模块系列（AG35-CET、AG35-EUT）
> **版本**：V1.0.1（Preliminary / 临时文件）
> **日期**：2024-10-21
> **源 PDF 总页数**：66 页（页脚标 65，含封面共 66 物理页）
> **本分析覆盖**：全部 66 页
> **分析对象**：QuecOpen(SDK) 方案下 AG35-CET / AG35-EUT 模块提供的 (U)SIM API（头文件 `ql_sim.h`）及配套示例

---

## 文档定位与背景

- **QuecOpen® 方案**：基于 Linux 的嵌入式开发平台，可简化 IoV（车联网）应用的软件设计与开发流程。本文档面向「SDK 构建环境」的 QuecOpen 方案。
- **(U)SIM 卡**：用户身份识别卡 / 智能卡，GSM 数字移动电话机必须装上才能使用。卡芯片上存储数字移动电话客户信息、加密密钥、电话簿等内容，用于 GSM 网络身份鉴别并对语音通话加密。本文「(U)SIM」泛指 **ICC** 与 **UICC** 卡。
- 文档详细描述 SDK 中提供的 **(U)SIM API** 及相关示例。

### 修订记录（文档历史）

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| - | 2023-07-07 | Chann YE | 文档创建 |
| 1.0.0 | 2023-07-07 | Chann YE | 临时版本 |
| 1.0.1 | 2024-10-21 | Lyndsay XIE | 临时版本：1) 基于 QuecOpen 方案统一命名，更新文档名称；2) 新增适用模块 AG35-EUT；3) 修改 `QL_SIM_SLOT_E` 枚举描述（2.3.3.1）；4) 新增 `ql_sim_switch_slot()` 用于切换 (U)SIM 卡（2.3.23）；5) 新增 `ql_sim_get_active_slots()` 用于获取处于激活状态的 (U)SIM 物理卡槽号（2.3.24） |

> **解读**：V1.0.1 的核心增量是「**多卡槽 / 卡切换**」能力（`ql_sim_switch_slot` + `ql_sim_get_active_slots` + `QL_SIM_PHY_SLOT_E`），这与本仓库关注的双卡/选网场景直接相关。

---

## 1 引言（P7）

- AG35-CET / AG35-EUT 支持 QuecOpen 方案。QuecOpen 详细信息见参考文档 [1]。
- 本文适用于 SDK 构建环境，描述 AG35-CET/EUT 的 SDK 提供的 (U)SIM API 与示例。

---

## 2 (U)SIM API

### 2.1 头文件（P8）

- (U)SIM API 头文件为 **`ql_sim.h`**，位于 SDK 的 **`ql-sysroots\usr\include\ql-sdk\`** 目录下。
- 若无特别说明，本文涉及头文件均在该目录下。

### 2.2 函数概览（表 1，P8-9）

| 函数 | 说明 |
|---|---|
| `ql_sim_init()` | 初始化 (U)SIM 卡服务 |
| `ql_sim_deinit()` | 注销 (U)SIM 卡服务 |
| `ql_sim_get_imsi()` | 获取 (U)SIM 卡的 IMSI |
| `ql_sim_get_iccid()` | 获取 (U)SIM 卡的 ICCID |
| `ql_sim_get_phone_num()` | 获取 (U)SIM 卡的电话号码 |
| `ql_sim_get_operators()` | 获取运营商列表 |
| `ql_sim_enable_pin()` | 使能 PIN 码 |
| `ql_sim_disable_pin()` | 禁用 PIN 码 |
| `ql_sim_verify_pin()` | 验证 PIN 码 |
| `ql_sim_change_pin()` | 更改 PIN 码 |
| `ql_sim_unblock_pin()` | 解锁 (U)SIM 卡并设置新 PIN 码 |
| `ql_sim_get_card_info()` | 获取 (U)SIM 卡信息 |
| `ql_sim_read_file()` | 从 (U)SIM 卡文件读取数据 |
| `ql_sim_write_file()` | 把数据写入 (U)SIM 卡文件 |
| `ql_sim_get_file_info()` | 获取 (U)SIM 卡文件信息 |
| `ql_sim_read_phone_book()` | 读取 (U)SIM 卡电话簿中保存的联系人信息 |
| `ql_sim_write_phone_book()` | 修改电话簿内保存的联系人信息 |
| `ql_sim_open_logical_channel()` | 打开逻辑通道 |
| `ql_sim_close_logical_channel()` | 关闭逻辑通道 |
| `ql_sim_send_apdu()` | 发送 APDU |
| `ql_sim_set_card_status_cb()` | 设置 (U)SIM 卡状态接收回调函数 |
| `ql_sim_set_service_error_cb()` | 设置 (U)SIM 卡服务异常接收回调函数 |
| `ql_sim_switch_slot()` | 切换 (U)SIM 卡 |
| `ql_sim_get_active_slots()` | 获取处于激活状态的 (U)SIM 卡物理卡槽号 |

> **重要备注（全局约束）**：若无特别说明，本文所述函数**均不支持并发调用**，并且**不能在相关回调函数中调用以上函数**。
> **解读**：API 非线程安全；回调里禁止回调 API（避免重入死锁）。集成进 modem_mng 时需用单独工作线程串行化访问。

---

### 2.3 函数详解

#### 2.3.1 ql_sim_init（P9）
初始化 (U)SIM 卡服务。
```c
int ql_sim_init(void)
```
- **参数**：无
- **返回值**：`QL_ERR_OK` 成功；其他值失败，错误码详见 `ql_type.h`。
- **备注**：使用其他任何 (U)SIM API **前必须先调用本函数**初始化服务。

#### 2.3.2 ql_sim_deinit（P10）
注销 (U)SIM 卡服务。
```c
int ql_sim_deinit(void)
```
- **参数**：无
- **返回值**：`QL_ERR_OK` 成功；其他值失败。
- **备注**：程序退出前或不再使用 (U)SIM 服务时**必须调用本函数注销服务、释放资源**。

#### 2.3.3 ql_sim_get_imsi（P10-11）
获取 (U)SIM 卡的 IMSI。
```c
int ql_sim_get_imsi(QL_SIM_SLOT_E slot, QL_SIM_APP_TYPE_E app_type, char *imsi, int imsi_len)
```
| 参数 | 方向 | 含义 |
|---|---|---|
| `slot` | [In] | (U)SIM 卡逻辑卡槽号（见 2.3.3.1） |
| `app_type` | [In] | (U)SIM 卡应用类型（见 2.3.3.2） |
| `imsi` | [Out] | IMSI 缓存区，存放获取的 IMSI |
| `imsi_len` | [In] | IMSI 缓存区长度 |
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

##### 2.3.3.1 QL_SIM_SLOT_E（逻辑卡槽号枚举，P11）
```c
typedef enum
{
    QL_SIM_SLOT_INVALID = 0x000,
    QL_SIM_SLOT_1       = 0xB01,
    QL_SIM_SLOT_2       = 0xB02,
} QL_SIM_SLOT_E
```
| 成员 | 描述 |
|---|---|
| `QL_SIM_SLOT_INVALID` | 无效卡槽 |
| `QL_SIM_SLOT_1` | 卡槽 1 |
| `QL_SIM_SLOT_2` | 卡槽 2（**暂不支持**） |

> **解读**：这是**逻辑**卡槽号，与 2.3.23.1 的**物理**卡槽 `QL_SIM_PHY_SLOT_E` 不同。当前固件逻辑层只暴露 SLOT_1，SLOT_2 暂不支持——双卡需通过 `ql_sim_switch_slot` 在物理卡槽间切换，逻辑侧始终用 SLOT_1。

##### 2.3.3.2 QL_SIM_APP_TYPE_E（应用类型枚举，P11-12）
```c
typedef enum
{
    QL_SIM_APP_TYPE_UNKNOWN = 0xB00,
    QL_SIM_APP_TYPE_3GPP    = 0xB01,
    QL_SIM_APP_TYPE_3GPP2   = 0xB02,
    QL_SIM_APP_TYPE_ISIM    = 0xB03,
} QL_SIM_APP_TYPE_E
```
| 成员 | 描述 |
|---|---|
| `QL_SIM_APP_TYPE_UNKNOWN` | 未知应用类型 |
| `QL_SIM_APP_TYPE_3GPP` | 3GPP 类型应用程序（SIM/USIM） |
| `QL_SIM_APP_TYPE_3GPP2` | 3GPP2 类型应用程序（RUIM/CSIM） |
| `QL_SIM_APP_TYPE_ISIM` | ISIM 应用程序 |

> **解读**：国内常规 4G 卡用 `QL_SIM_APP_TYPE_3GPP`（SIM/USIM）。

#### 2.3.4 ql_sim_get_iccid（P12）
获取 (U)SIM 卡的 ICCID。
```c
int ql_sim_get_iccid(QL_SIM_SLOT_E slot, char *iccid, int iccid_len)
```
| 参数 | 方向 | 含义 |
|---|---|---|
| `slot` | [In] | 逻辑卡槽号（见 2.3.3.1） |
| `iccid` | [Out] | ICCID 缓存区 |
| `iccid_len` | [In] | ICCID 缓存区长度 |
- **返回值**：`QL_ERR_OK` 成功；其他值失败。
> **解读**：ICCID 不需要 `app_type` 参数（ICCID 是卡片级标识，与应用无关），而 IMSI/电话号码需要 `app_type`。

#### 2.3.5 ql_sim_get_phone_num（P12-13）
获取 (U)SIM 卡的电话号码。
```c
int ql_sim_get_phone_num(QL_SIM_SLOT_E slot, QL_SIM_APP_TYPE_E app_type, char *phone_num, int phone_num_len)
```
| 参数 | 方向 | 含义 |
|---|---|---|
| `slot` | [In] | 逻辑卡槽号 |
| `app_type` | [In] | 应用类型 |
| `phone_num` | [Out] | 号码缓存区 |
| `phone_num_len` | [In] | 号码缓存区长度 |
- **返回值**：`QL_ERR_OK` 成功；其他值失败。
> **注**：原文参数表把最后一个参数名也写作 `phone_num`（含义为缓存区长度），实为 `phone_num_len`，系文档笔误。

#### 2.3.6 ql_sim_get_operators（P13-14）
获取运营商列表。
```c
int ql_sim_get_operators(QL_SIM_SLOT_E slot, ql_sim_operator_list_t *list)
```
| 参数 | 方向 | 含义 |
|---|---|---|
| `slot` | [In] | 逻辑卡槽号 |
| `list` | [Out] | 运营商列表（见 2.3.6.1） |
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

##### 2.3.6.1 ql_sim_operator_list_t（运营商列表结构体，P14）
```c
typedef struct
{
    int len;
    ql_sim_operator_t operators[QL_SIM_NUM_OPERATOR_MAX];
} ql_sim_operator_list_t
```
| 类型 | 参数 | 描述 |
|---|---|---|
| `int` | `len` | 运营商信息数组的数量（`operators` 数组有效长度） |
| `ql_sim_operator_t` | `operators` | 运营商信息数组，有效长度由 `len` 指定，最大为 `QL_SIM_NUM_OPERATOR_MAX`（**24**）（见 2.3.6.2） |

##### 2.3.6.2 ql_sim_operator_t（运营商信息结构体，P14）
```c
typedef struct
{
    char mcc[QL_SIM_MCC_LENGHT];
    uint8_t mnc_len;
    char mnc[QL_SIM_MNC_MAX];
} ql_sim_operator_t
```
| 类型 | 参数 | 描述 |
|---|---|---|
| `char` | `mcc` | 移动设备国家代码（MCC），3 位 |
| `uint8_t` | `mnc_len` | MNC 长度，2 位或 3 位 |
| `char` | `mnc` | 移动设备网络代码（MNC）。示例：`00` 中国移动 TD 系统；`01` 中国联通 GSM 系统；`02` 中国移动 GSM 系统；`03` 中国电信 CDMA 系统 |

> **解读**：MCC+MNC 即 PLMN，46000/46002（移动）、46001（联通）、46003（电信）等。该结构与海外选网（COPS 切换）场景的 PLMN 解析直接相关——本仓库的 `[OPER]` 选网特性可借此读取卡内运营商列表。注意结构体名 `LENGHT` 系原文拼写（length 误拼）。

#### 2.3.7 ql_sim_enable_pin（P15-16）
使能 PIN 码。
```c
int ql_sim_enable_pin(QL_SIM_SLOT_E slot, QL_SIM_APP_TYPE_E app_type, QL_SIM_PIN_E pin, const char *pin_value)
```
| 参数 | 方向 | 含义 |
|---|---|---|
| `slot` | [In] | 逻辑卡槽号 |
| `app_type` | [In] | 应用类型 |
| `pin` | [In] | PIN 码编号（见 2.3.7.1） |
| `pin_value` | [In] | PIN 码字符串，必须以 `\0` 结尾，最大长度 `QL_SIM_PIN_MAX(8)` |
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

##### 2.3.7.1 QL_SIM_PIN_E（PIN 码编号枚举，P15-16）
```c
typedef enum
{
    QL_SIM_PIN_1 = 0xB01,
    QL_SIM_PIN_2 = 0xB02,
} QL_SIM_PIN_E
```
| 成员 | 描述 |
|---|---|
| `QL_SIM_PIN_1` | PIN1 码。由电信运营商提供，用于 (U)SIM 卡保密的个人识别码（Personal Identification Number） |
| `QL_SIM_PIN_2` | PIN2 码。由供应商提供的 (U)SIM 卡另一密码，用于限定拨号等功能的个人识别码，主要用于消除呼叫费用数据、设定通话费计费币别和计费单位、费用限制、限定拨号 |

- **备注**：
  1. 启用 (U)SIM 卡 PIN 码后，访问手机或无线通信模块需输入 PIN 码。
  2. 输入 PIN 码有次数限制，连续 **3 次**输错后 PIN 码被锁定，导致 (U)SIM 卡无法使用，需输入 **PUK** 码才能解锁。原始 PIN/PUK 码可通过运营商获取。

#### 2.3.8 ql_sim_disable_pin（P16）
禁用 PIN 码。
```c
int ql_sim_disable_pin(QL_SIM_SLOT_E slot, QL_SIM_APP_TYPE_E app_type, QL_SIM_PIN_E pin, const char *pin_value)
```
- 参数同 `ql_sim_enable_pin`（slot / app_type / pin / pin_value，pin_value 最大 8 字节、`\0` 结尾）。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

#### 2.3.9 ql_sim_verify_pin（P17）
验证 PIN 码。当 (U)SIM 卡状态为「请求 PIN1/PIN2」时，输入 PIN1/PIN2 码进行验证。**调用该函数前需先使能 PIN 锁。**
```c
int ql_sim_verify_pin(QL_SIM_SLOT_E slot, QL_SIM_APP_TYPE_E app_type, QL_SIM_PIN_E pin, const char *pin_value)
```
- 参数同上。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

#### 2.3.10 ql_sim_change_pin（P17-18）
更改 PIN 码。
```c
int ql_sim_change_pin(QL_SIM_SLOT_E slot, QL_SIM_APP_TYPE_E app_type, QL_SIM_PIN_E pin,
                      const char *old_pin_value, const char *new_pin_value)
```
| 参数 | 方向 | 含义 |
|---|---|---|
| `slot` | [In] | 逻辑卡槽号 |
| `app_type` | [In] | 应用类型 |
| `pin` | [In] | PIN 码编号 |
| `old_pin_value` | [In] | 旧 PIN 码，`\0` 结尾，最大 `QL_SIM_PIN_MAX(8)` |
| `new_pin_value` | [In] | 新 PIN 码，`\0` 结尾，最大 `QL_SIM_PIN_MAX(8)` |
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

#### 2.3.11 ql_sim_unblock_pin（P18）
解锁 (U)SIM 卡并设置新 PIN 码。PIN 码输入错误次数超过 3 次后被锁定，此时需用 PUK 码解开锁定状态。
```c
int ql_sim_unblock_pin(QL_SIM_SLOT_E slot, QL_SIM_APP_TYPE_E app_type, QL_SIM_PIN_E pin,
                       const char *puk_value, const char *pin_value)
```
| 参数 | 方向 | 含义 |
|---|---|---|
| `slot` | [In] | 逻辑卡槽号 |
| `app_type` | [In] | 应用类型 |
| `pin` | [In] | PIN 码编号 |
| `puk_value` | [In] | PUK 码，`\0` 结尾，最大 `QL_SIM_PIN_MAX(8)` |
| `pin_value` | [In] | 新 PIN 码，`\0` 结尾，最大 `QL_SIM_PIN_MAX(8)` |
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

#### 2.3.12 ql_sim_get_card_info（P19）
获取 (U)SIM 卡信息。
```c
int ql_sim_get_card_info(QL_SIM_SLOT_E slot, ql_sim_card_info_t *p_info)
```
| 参数 | 方向 | 含义 |
|---|---|---|
| `slot` | [In] | 逻辑卡槽号 |
| `p_info` | [Out] | (U)SIM 卡信息（见 2.3.12.1） |
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

##### 2.3.12.1 ql_sim_card_info_t（卡信息结构体，P19-20）
```c
typedef struct
{
    QL_SIM_CARD_STATE_E state;
    QL_SIM_CARD_TYPE_E  type;
    ql_sim_app_info_t   app_3gpp;
    ql_sim_app_info_t   app_3gpp2;
    ql_sim_app_info_t   app_isim;
} ql_sim_card_info_t
```
| 类型 | 参数 | 描述 |
|---|---|---|
| `QL_SIM_CARD_STATE_E` | `state` | (U)SIM 卡状态（见 2.3.12.2） |
| `QL_SIM_CARD_TYPE_E` | `type` | (U)SIM 卡类型（见 2.3.12.3） |
| `ql_sim_app_info_t` | `app_3gpp` | 3GPP 应用信息（见 2.3.12.4） |
| `ql_sim_app_info_t` | `app_3gpp2` | 3GPP2 应用信息（见 2.3.12.4） |
| `ql_sim_app_info_t` | `app_isim` | ISIM 应用信息（结构同上，下页续） |

> **解读**：卡信息把卡级状态/类型与三种应用（3GPP/3GPP2/ISIM）信息聚合在一起。下一段将继续 `QL_SIM_CARD_STATE_E` 等枚举的展开。

##### 2.3.12.2 QL_SIM_CARD_STATE_E（卡状态枚举，P20）
```c
typedef enum {
    QL_SIM_CARD_STATE_UNKNOWN                     = 0xB01,
    QL_SIM_CARD_STATE_ABSENT                      = 0xB02,
    QL_SIM_CARD_STATE_PRESENT                     = 0xB03,
    QL_SIM_CARD_STATE_ERROR_UNKNOWN               = 0xB04,
    QL_SIM_CARD_STATE_ERROR_POWER_DOWN            = 0xB05,
    QL_SIM_CARD_STATE_ERROR_POLL_ERROR            = 0xB06,
    QL_SIM_CARD_STATE_ERROR_NO_ATR_RECEIVED       = 0xB07,
    QL_SIM_CARD_STATE_ERROR_VOLT_MISMATCH         = 0xB08,
    QL_SIM_CARD_STATE_ERROR_PARITY_ERROR          = 0xB09,
    QL_SIM_CARD_STATE_ERROR_SIM_TECHNICAL_PROBLEMS = 0xB0A,
} QL_SIM_CARD_STATE_E
```
| 成员 | 描述 |
|---|---|
| `QL_SIM_CARD_STATE_UNKNOWN` | 未知状态 |
| `QL_SIM_CARD_STATE_ABSENT` | 无卡状态 |
| `QL_SIM_CARD_STATE_PRESENT` | 检测到 (U)SIM 卡 |
| `QL_SIM_CARD_STATE_ERROR_UNKNOWN` | 未知错误 |
| `QL_SIM_CARD_STATE_ERROR_POWER_DOWN` | (U)SIM 卡未上电 |
| `QL_SIM_CARD_STATE_ERROR_POLL_ERROR` | 轮询错误 |
| `QL_SIM_CARD_STATE_ERROR_NO_ATR_RECEIVED` | 未收到复位应答（ATR） |
| `QL_SIM_CARD_STATE_ERROR_VOLT_MISMATCH` | 电压失配 |
| `QL_SIM_CARD_STATE_ERROR_PARITY_ERROR` | 奇偶校验错误 |
| `QL_SIM_CARD_STATE_ERROR_SIM_TECHNICAL_PROBLEMS` | (U)SIM 卡技术问题 |

> **解读**：正常工作态是 `PRESENT`；`ABSENT` 即未插卡。一系列 `ERROR_*` 用于细分卡硬件/接触故障——这对网络异常检测/恢复方案很有价值：例如 `ERROR_NO_ATR_RECEIVED`/`ERROR_VOLT_MISMATCH` 多为卡座接触不良或卡损坏，软件重拨无效，应区别于网络侧问题处理。

##### 2.3.12.3 QL_SIM_CARD_TYPE_E（卡类型枚举，P21）
```c
typedef enum
{
    QL_SIM_CARD_TYPE_UNKNOWN = 0xB00,
    QL_SIM_CARD_TYPE_ICC     = 0xB01,
    QL_SIM_CARD_TYPE_UICC    = 0xB02,
} QL_SIM_CARD_TYPE_E
```
| 成员 | 描述 |
|---|---|
| `QL_SIM_CARD_TYPE_UNKNOWN` | 未知类型 |
| `QL_SIM_CARD_TYPE_ICC` | ICC 卡 |
| `QL_SIM_CARD_TYPE_UICC` | UICC 卡 |

##### 2.3.12.4 ql_sim_app_info_t（应用信息结构体，P21）
```c
typedef struct {
    QL_SIM_APP_STATE_E app_state;
    uint8_t pin1_num_retries;
    uint8_t puk1_num_retries;
    uint8_t pin2_num_retries;
    uint8_t puk2_num_retries;
} ql_sim_app_info_t
```
| 类型 | 参数 | 描述 |
|---|---|---|
| `QL_SIM_APP_STATE_E` | `app_state` | 应用状态（见 2.3.12.5） |
| `uint8_t` | `pin1_num_retries` | 输入 PIN1 码剩余尝试次数 |
| `uint8_t` | `puk1_num_retries` | 输入 PUK1 码剩余尝试次数 |
| `uint8_t` | `pin2_num_retries` | 输入 PIN2 码剩余尝试次数 |
| `uint8_t` | `puk2_num_retries` | 输入 PUK2 码剩余尝试次数 |

> **解读**：剩余尝试次数是判断「卡是否处于 PIN/PUK 锁定边缘」的关键——`pin1_num_retries==0` 表示 PIN1 已锁，需 PUK1 解；`puk1_num_retries==0` 表示卡已永久锁死。自动化拨号程序在尝试 verify_pin 前应先读这些值，避免把卡彻底锁死。

##### 2.3.12.5 QL_SIM_APP_STATE_E（应用状态枚举，P22）
```c
typedef enum {
    QL_SIM_APP_STATE_UNKNOWN                   = 0xB00,
    QL_SIM_APP_STATE_DETECTED                  = 0xB01,
    QL_SIM_APP_STATE_PIN1_REQ                  = 0xB02,
    QL_SIM_APP_STATE_PUK1_REQ                  = 0xB03,
    QL_SIM_APP_STATE_INITALIZATING             = 0xB04,
    QL_SIM_APP_STATE_PERSO_CK_REQ              = 0xB05,
    QL_SIM_APP_STATE_PERSO_PUK_REQ             = 0xB06,
    QL_SIM_APP_STATE_PERSO_PERMANENTLY_BLOCKED = 0xB07,
    QL_SIM_APP_STATE_PIN1_PERM_BLOCKED         = 0xB08,
    QL_SIM_APP_STATE_ILLEGAL                   = 0xB09,
    QL_SIM_APP_STATE_READY                     = 0xB0A,
} QL_SIM_APP_STATE_E
```
| 成员 | 描述 |
|---|---|
| `QL_SIM_APP_STATE_UNKNOWN` | 未知状态 |
| `QL_SIM_APP_STATE_DETECTED` | 已检测到 |
| `QL_SIM_APP_STATE_PIN1_REQ` | 等待输入 PIN1 码 |
| `QL_SIM_APP_STATE_PUK1_REQ` | 等待输入 PUK1 码 |
| `QL_SIM_APP_STATE_INITALIZATING` | 初始化中（注：原文 `INITALIZATING` 拼写） |
| `QL_SIM_APP_STATE_PERSO_CK_REQ` | 等待输入 CK 密码（**暂不支持**） |
| `QL_SIM_APP_STATE_PERSO_PUK_REQ` | 等待输入解锁个性化设置的密码（**暂不支持**） |
| `QL_SIM_APP_STATE_PERSO_PERMANENTLY_BLOCKED` | 个性化设置被永久锁定（**暂不支持**） |
| `QL_SIM_APP_STATE_PIN1_PERM_BLOCKED` | PIN1 码被永久锁定 |
| `QL_SIM_APP_STATE_ILLEGAL` | 非法状态 |
| `QL_SIM_APP_STATE_READY` | 初始化完成 |

> **解读**：`READY` 才能正常入网；`PIN1_REQ`/`PUK1_REQ` 需软件输入码；`PERSO_*` 是 SIM 锁/运营商个性化锁（暂不支持）；`ILLEGAL` 多见于黑卡/被网络拒绝的卡。

#### 2.3.13 ql_sim_read_file（P23）
从 (U)SIM 卡文件读取数据。
```c
int ql_sim_read_file(QL_SIM_SLOT_E slot, QL_SIM_APP_TYPE_E app_type, ql_sim_file_t *p_file)
```
> 注：原文函数原型首字母写作 `Int`（笔误，应为 `int`）。

| 参数 | 方向 | 含义 |
|---|---|---|
| `slot` | [In] | 逻辑卡槽号 |
| `app_type` | [In] | 应用类型 |
| `p_file` | [In] | 文件信息（见 2.3.13.1） |
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

##### 2.3.13.1 ql_sim_file_t（文件信息结构体，P23-24）
```c
typedef struct {
    uint32_t path_len;
    char     path[QL_SIM_PATH_MAX];
    uint16_t offset;
    uint8_t  record_idx;
    uint32_t data_len;
    uint8_t  data[QL_SIM_DATA_MAX];
} ql_sim_file_t
```
| 类型 | 参数 | 描述 |
|---|---|---|
| `uint32_t` | `path_len` | 文件路径长度 |
| `char` | `path` | 文件路径。长度由 `path_len` 指定，最大 `QL_SIM_PATH_MAX(20)` |
| `uint16_t` | `offset` | 指定写入文件中数据的起始位置。**仅对透明文件有效** |
| `uint8_t` | `record_idx` | 记录索引：`0`=读/写透明文件；`>0`=读/写循环或线性固定文件，记录索引由该值提供 |
| `uint32_t` | `data_len` | 数据长度，最大 `QL_SIM_DATA_MAX` |
| `uint8_t` | `data` | 待写入或待读取的数据，长度由 `data_len` 指定 |

> **解读**：(U)SIM 文件系统按 ETSI/3GPP 定义分三类：透明文件（一段连续二进制，用 `offset`）、循环文件、线性固定文件（按 `record_idx` 寻址记录）。`path` 是文件标识路径（如 `3F00`-`7FFF`-`6F07` 等十六进制 EF/DF 标识）。

#### 2.3.14 ql_sim_write_file（P24）
把数据写入 (U)SIM 卡文件。
```c
int ql_sim_write_file(QL_SIM_SLOT_E slot, QL_SIM_APP_TYPE_E app_type, ql_sim_file_t *p_file)
```
- 参数：slot / app_type / `p_file`[In]（同 2.3.13.1，data/data_len 为待写入数据）。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

#### 2.3.15 ql_sim_get_file_info（P24-25）
获取 (U)SIM 卡文件信息。
```c
int ql_sim_get_file_info(QL_SIM_SLOT_E slot, QL_SIM_APP_TYPE_E app_type, ql_sim_file_info_t *p_info)
```
- 参数：slot / app_type / `p_info`[In/Out]（见 2.3.15.1）。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

##### 2.3.15.1 ql_sim_file_info_t（文件信息结构体，P25）
```c
typedef struct {
    /* == filled by caller == */
    uint32_t path_len;
    char     path[QL_SIM_PATH_MAX];
    /* == filled by callee == */
    QL_SIM_FILE_TYPE_E file_type;
    uint16_t file_size;
    uint16_t record_size;
    uint16_t record_count;
} ql_sim_file_info_t
```
| 类型 | 参数 | 描述 |
|---|---|---|
| `uint32_t` | `path_len` | 文件路径长度（**调用者填**） |
| `char` | `path` | 文件路径，最大 `QL_SIM_PATH_MAX`（**调用者填**） |
| `QL_SIM_FILE_TYPE_E` | `file_type` | 文件类型（被调用者填，见 2.3.15.2） |
| `uint16_t` | `file_size` | 透明文件的大小（被调用者填） |
| `uint16_t` | `record_size` | 循环/线性固定文件中每个记录的大小（被调用者填） |
| `uint16_t` | `record_count` | 循环/线性固定文件中记录个数（被调用者填） |

> **解读**：典型用法是「先 `get_file_info` 探查文件类型与大小/记录数，再据此分配缓冲、构造 `ql_sim_file_t` 调用 read/write_file」。结构体注释明确区分了入参字段（path）与出参字段（type/size/count）。

##### 2.3.15.2 QL_SIM_FILE_TYPE_E（文件类型枚举，P26）
```c
typedef enum {
    QL_SIM_FILE_TYPE_UNKNOWN      = 0xB00,
    QL_SIM_FILE_TYPE_TRANSPARENT  = 0xB01,
    QL_SIM_FILE_TYPE_CYCLIC       = 0xB02,
    QL_SIM_FILE_TYPE_LINEAR_FIXED = 0xB03,
} QL_SIM_FILE_TYPE_E
```
| 成员 | 描述 |
|---|---|
| `QL_SIM_FILE_TYPE_UNKNOWN` | 未知类型 |
| `QL_SIM_FILE_TYPE_TRANSPARENT` | 透明文件（用 `file_size`） |
| `QL_SIM_FILE_TYPE_CYCLIC` | 循环记录文件（用 `record_size`/`record_count`） |
| `QL_SIM_FILE_TYPE_LINEAR_FIXED` | 线性固定文件（用 `record_size`/`record_count`） |

#### 2.3.16 ql_sim_read_phone_book（P26-27）
读取 (U)SIM 卡电话簿中保存的联系人信息。
```c
int ql_sim_read_phone_book(QL_SIM_SLOT_E slot, QL_SIM_APP_TYPE_E app_type, const char *pb_path,
                           uint8_t record_idx, ql_sim_phone_book_record_t *p_record)
```
| 参数 | 方向 | 含义 |
|---|---|---|
| `slot` | [In] | 逻辑卡槽号 |
| `app_type` | [In] | 应用类型 |
| `pb_path` | [In] | 电话簿路径，默认值 `QL_SIM_PB_DEFAULT_PATH` |
| `record_idx` | [In] | 电话簿编号（记录索引） |
| `p_record` | [In/Out] | 电话簿联系人信息（见 2.3.16.1） |
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

##### 2.3.16.1 ql_sim_phone_book_record_t（电话簿联系人结构体，P27）
```c
typedef struct
{
    char name[QL_SIM_PHONE_BOOK_NAME_MAX];
    char number[QL_SIM_PHONE_BOOK_NUMBER_MAX];
} ql_sim_phone_book_record_t
```
| 类型 | 参数 | 描述 |
|---|---|---|
| `char` | `name` | 联系人姓名，最大长度 `QL_SIM_PHONE_BOOK_NAME_MAX(15)`，由调用者设置 |
| `char` | `number` | 联系人号码，最大长度 `QL_SIM_PHONE_BOOK_NUMBER_MAX(25)`，由调用者设置 |

#### 2.3.17 ql_sim_write_phone_book（P27-28）
修改电话簿内保存的联系人信息。
```c
int ql_sim_write_phone_book(QL_SIM_SLOT_E slot, QL_SIM_APP_TYPE_E app_type, const char *pb_path,
                            uint8_t record_idx, ql_sim_phone_book_record_t *p_record)
```
- 参数同 `ql_sim_read_phone_book`（pb_path 默认 `QL_SIM_PB_DEFAULT_PATH`，record_idx 为电话簿编号，p_record[In/Out] 为联系人信息）。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

#### 2.3.18 ql_sim_open_logical_channel（P28）
打开**指定 UICC 卡**的逻辑通道。
```c
int ql_sim_open_logical_channel(QL_SIM_SLOT_E slot, uint8_t *channel_id)
```
| 参数 | 方向 | 含义 |
|---|---|---|
| `slot` | [In] | 逻辑卡槽号 |
| `channel_id` | [In/Out] | 逻辑通道 ID |
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

#### 2.3.19 ql_sim_close_logical_channel（P28-29）
关闭指定 UICC 卡的逻辑通道。
```c
int ql_sim_close_logical_channel(QL_SIM_SLOT_E slot, uint8_t channel_id)
```
| 参数 | 方向 | 含义 |
|---|---|---|
| `slot` | [In] | 逻辑卡槽号 |
| `channel_id` | [In] | 逻辑通道 ID |
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

> **解读**：逻辑通道是 ISO 7816-4 概念，用于在不打断主通道会话的前提下与卡上不同应用（如 SE 安全单元、运营商小程序）通信。打开后拿到 `channel_id`，配合 `ql_sim_send_apdu` 在该通道收发 APDU，用完关闭。

#### 2.3.20 ql_sim_send_apdu（P29-30）
向 (U)SIM 卡发送 APDU 数据。**调用此函数前需先打开一个逻辑通道。**
```c
int ql_sim_send_apdu(QL_SIM_SLOT_E slot, uint8_t channel_id, ql_sim_apdu_t *p_apdu)
```
| 参数 | 方向 | 含义 |
|---|---|---|
| `slot` | [In] | 逻辑卡槽号 |
| `channel_id` | [In] | 逻辑通道 ID |
| `p_apdu` | [In/Out] | APDU 数据（见 2.3.20.1） |
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

##### 2.3.20.1 ql_sim_apdu_t（APDU 数据结构体，P29-30）
```c
typedef struct
{
    uint32_t req_apdu_len;
    uint8_t  req_apdu[QL_SIM_APDU_DATA_MAX];
    uint32_t resp_apdu_len;
    uint8_t  resp_apdu[QL_SIM_APDU_DATA_MAX];
} ql_sim_apdu_t
```
| 类型 | 参数 | 描述 |
|---|---|---|
| `uint32_t` | `req_apdu_len` | 请求的 APDU 数据长度，单位字节，由调用者设置 |
| `uint8_t` | `req_apdu` | 请求的 APDU 数据内容，最大 `QL_SIM_APDU_DATA_MAX(1024)`，由调用者设置 |
| `uint32_t` | `resp_apdu_len` | 响应的 APDU 数据长度 |
| `uint8_t` | `resp_apdu` | 响应的 APDU 数据，最大 `QL_SIM_APDU_DATA_MAX(1024)` |

> **注**：原文 `resp_apdu` 描述写「长度由 `req_apdu_len` 指定」，应为 `resp_apdu_len`（文档笔误）。

#### 2.3.21 ql_sim_set_card_status_cb（P30-31）
设置 (U)SIM 卡状态接收回调函数。
```c
int ql_sim_set_card_status_cb(ql_sim_card_status_cb_f cb)
```
- 参数：`cb`[In] 卡状态回调（见 2.3.21.1）。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

##### 2.3.21.1 ql_sim_card_status_cb_f（卡状态回调原型，P30-31）
该回调函数接收 (U)SIM 卡状态信息。
```c
typedef void (*ql_sim_card_status_cb_f)(QL_SIM_SLOT_E slot, ql_sim_card_info_t *p_info)
```
| 参数 | 方向 | 含义 |
|---|---|---|
| `slot` | [In] | 逻辑卡槽号 |
| `p_info` | [In] | (U)SIM 卡信息（见 2.3.12.1） |
- **返回值**：无。

> **解读**：注册后，卡状态变化（插拔、PIN 状态变化等）会异步回调，携带完整 `ql_sim_card_info_t`。注意全局约束——回调内禁止调用其他 (U)SIM API。

#### 2.3.22 ql_sim_set_service_error_cb（P31）
设置 (U)SIM 卡服务异常接收回调函数。
```c
int ql_sim_set_service_error_cb(ql_sim_service_error_cb_f cb)
```
- 参数：`cb`[In] 服务异常回调（见 2.3.22.1）。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

##### 2.3.22.1 ql_sim_service_error_cb_f（服务异常回调原型，P31）
该回调函数接收 (U)SIM 卡服务异常信息。
```c
typedef void (*ql_sim_service_error_cb_f)(int error)
```
- 参数：`error`[In] 服务错误码。**当前只在服务端异常退出时才触发该回调，错误码为 `QL_ERR_ABORTED`。**
- **返回值**：无。

> **解读**：这是 SIM 服务进程崩溃/退出的告警通道。收到 `QL_ERR_ABORTED` 意味着底层 SIM 服务已挂，应用应考虑重新 `ql_sim_init` 或上报故障。对网络异常恢复方案有意义：服务级异常区别于卡级/网络级异常。

#### 2.3.23 ql_sim_switch_slot（P32，V1.0.1 新增）
切换 (U)SIM 卡（把某逻辑卡槽映射到某物理卡槽）。
```c
int ql_sim_switch_slot(QL_SIM_SLOT_E log_slot, QL_SIM_PHY_SLOT_E phy_slot);
```
| 参数 | 方向 | 含义 |
|---|---|---|
| `log_slot` | [In] | 逻辑卡槽号（见 2.3.3.1） |
| `phy_slot` | [In] | 物理卡槽号（见 2.3.23.1） |
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

##### 2.3.23.1 QL_SIM_PHY_SLOT_E（物理卡槽号枚举，P32）
```c
typedef enum
{
    QL_SIM_PHY_SLOT_INVALID = 0x000, /**< Invalid slot.        */
    QL_SIM_PHY_SLOT_1       = 0xB01, /**< Identify card in slot 1. */
    QL_SIM_PHY_SLOT_2       = 0xB02, /**< Identify card in slot 2. */
} QL_SIM_PHY_SLOT_E;
```
| 成员 | 描述 |
|---|---|
| `QL_SIM_PHY_SLOT_INVALID` | 无效卡槽号 |
| `QL_SIM_PHY_SLOT_1` | 卡槽 1 |
| `QL_SIM_PHY_SLOT_2` | 卡槽 2 |

- **备注（P33）**：
  1. `QL_SIM_SLOT_E` 表示**逻辑卡槽**（软件性质的卡槽）；`QL_SIM_PHY_SLOT_E` 表示**物理卡槽**。
  2. **模块当前支持 2 个物理卡槽，但仅支持 1 个逻辑卡槽。**

> **解读（对本仓库双卡场景极其关键）**：AG35 硬件有 2 个物理卡槽，但任一时刻协议栈只激活 1 个逻辑卡槽（**双卡单待**，非双待）。`ql_sim_switch_slot(QL_SIM_SLOT_1, QL_SIM_PHY_SLOT_2)` 即把逻辑卡槽 1 重新绑定到物理卡槽 2 的卡。这正是「EC200A_AG35 双卡单待实现」方案需要的底层原语：切卡 = 调 `ql_sim_switch_slot` 重映射 + 重新走入网/拨号流程。切换会复位卡会话，需重新读卡/重新注册网络。

#### 2.3.24 ql_sim_get_active_slots（P33，V1.0.1 新增）
获取处于激活状态的 (U)SIM 卡物理卡槽号。
```c
int ql_sim_get_active_slots(ql_sim_active_slots_t *p_active_slots);
```
- 参数：`p_active_slots`[In]（原文标 [In]，实际为输出，见 2.3.24.1）。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

##### 2.3.24.1 ql_sim_active_slots_t（激活卡槽信息结构体，P33）
```c
typedef struct
{
    int active_slots_len;
    QL_SIM_PHY_SLOT_E active_slots[QL_SIM_MAX_NUM_CARDS];
} ql_sim_active_slots_t;
```
| 类型 | 参数 | 描述 |
|---|---|---|
| `int` | `active_slots_len` | 物理卡槽号个数（有效个数），最大不超过 `QL_SIM_MAX_NUM_CARDS` |
| `QL_SIM_PHY_SLOT_E` | `active_slots` | 处于激活状态的 SIM 卡物理卡槽号（见 2.3.23.1） |

> **解读**：用于查询当前哪个/哪些物理卡槽被激活。结合 `ql_sim_switch_slot`，可实现「查当前激活卡槽 → 决定是否切到另一槽 → 切换 → 验证激活槽已变」的闭环。双卡单待下 `active_slots_len` 通常为 1。

---

## 3 示例（P34 起）

> 本章代码示例均摘自 **`sample\test_sdk_api\m_sim.c`**，用户可自行查看接口函数的完整示例。
>
> **全局备注**：1) 程序启动后必须调用 `ql_sim_init()` 初始化 (U)SIM 卡服务；2) 程序退出前或不再使用时必须调用 `ql_sim_deinit()` 释放资源。

### 3.1 获取 (U)SIM 卡的 IMSI（P34-35）
核心流程（`item_ql_sim_get_imsi`）：
```c
static void item_ql_sim_get_imsi(void)
{
    int ret = 0;
    char imsi[QL_SIM_IMSI_LENGTH+1] = {0};
    int input = 0;
    QL_SIM_SLOT_E slot;
    QL_SIM_APP_TYPE_E app_type;

    printf("test ql_sim_get_imsi: \n");
    printf("please enter slot(1 or 2): ");
    scanf("%d", &input); getchar();
    if (1 == input)      { slot = QL_SIM_SLOT_1; }
    else if (2 == input) { slot = QL_SIM_SLOT_2; }
    else { printf("bad slot: %d\n", input); return; }

    printf("please enter app type(0-unknown, 1-3gpp, 2-3gpp2, 3-isim): ");
    scanf("%d", &input); getchar();
    switch (input) {
        case 0: app_type = QL_SIM_APP_TYPE_UNKNOWN; break;
        case 1: app_type = QL_SIM_APP_TYPE_3GPP;    break;
        case 2: app_type = QL_SIM_APP_TYPE_3GPP2;   break;
        case 3: app_type = QL_SIM_APP_TYPE_ISIM;    break;
        default: printf("bad app type: %d\n", input); return;
    }

    ret = ql_sim_get_imsi(slot, app_type, imsi, sizeof(imsi));
    if (ret == QL_ERR_OK) { printf("IMSI: %s\n", imsi); }
    else                  { printf("failed, ret = %d\n", ret); }
}
```
> **要点**：IMSI 缓冲按 `QL_SIM_IMSI_LENGTH+1` 分配（留 `\0`）。slot/app_type 通过交互输入选择，是「先选槽再选应用类型」的标准前置流程，后续多个示例复用同一套选择代码。

### 3.2 获取 (U)SIM 卡的 ICCID（P35-36）
`item_ql_sim_get_iccid`：缓冲 `char iccid[QL_SIM_ICCID_LENGTH+1]`；只需选 slot（**无 app_type**，与 API 一致）；调用 `ql_sim_get_iccid(slot, iccid, sizeof(iccid))`，成功打印 `ICCID: %s`。

### 3.3 获取 (U)SIM 卡的电话号码（P36-38）
`item_ql_sim_get_phone_num`：缓冲 `char num[QL_SIM_PHONE_NUMBER_MAX+1]`；选 slot + app_type（同 3.1 套路）；调用 `ql_sim_get_phone_num(slot, app_type, num, sizeof(num))`，成功打印 `Phone number: %s`。

### 3.4 获取运营商列表（P38-39）
`item_ql_sim_get_operators`：定义 `ql_sim_operator_list_t list = {0}`；选 slot；调用 `ql_sim_get_operators(slot, &list)`；成功后若 `list.len==0` 打印 `No operators found`，否则遍历：
```c
for (i = 0; i < list.len; i++) {
    printf(" #%02d: ", i + 1);
    print_ascii("MCC: ", "", list.operators[i].mcc, (int)sizeof(list.operators[i].mcc));
    print_ascii(", MNC: ", "\n", list.operators[i].mnc, list.operators[i].mnc_len);
}
```
> **要点**：MCC 用结构体字段固定大小打印，MNC 用 `mnc_len`（2 或 3 位）打印——印证 2.3.6.2 中 MNC 长度可变。`print_ascii` 是示例自带的辅助打印函数。该示例直接对应海外选网中「枚举卡上可用 PLMN」的能力。

### 3.5 PIN 码操作（P39 起）
#### 3.5.1 使能 PIN 码（P39-，本段见开头）
`item_ql_sim_enable_pin`：除 slot/app_type 外，还需选 PIN 编号并输入 PIN 值。缓冲 `char pin_value[QL_SIM_PIN_MAX*2]`，逐字符读取（`char c` + `len` 计数）。本页仅展示到 slot 选择，PIN 编号选择与调用在下一段（P40+）。

#### 3.5.1（续）使能 PIN 码（P40-41）
PIN 编号选择与 PIN 值读取后调用 `ql_sim_enable_pin`：
```c
printf("please enter pin(1 or 2): ");
scanf("%d", &input); getchar();
if (1 == input)      { pin = QL_SIM_PIN_1; }
else if (2 == input) { pin = QL_SIM_PIN_2; }
else { printf("bad pin: %d\n", input); return; }

printf("please enter pin value(at most %d digit): ", QL_SIM_PIN_MAX);
if (NULL == fgets(pin_value, sizeof(pin_value), stdin)) {
    printf("can not read pin value\n"); return;
}
len = strlen(pin_value);
if ('\n' == pin_value[len-1]) { pin_value[len-1] = 0; len--; }   /* 去掉换行 */
printf("pin value: %s\n", pin_value);

printf("proceed? [y/n]: ");
c = getchar();
if ('\n' != c) { getchar(); }
if ('Y' != c && 'y' != c) { printf("abort\n"); return; }

ret = ql_sim_enable_pin(slot, app_type, pin, pin_value);
if (ret == QL_ERR_OK) { printf("ok\n"); }
else                  { printf("failed, ret = %d\n", ret); }
```
> **要点**：用 `fgets` 读 PIN 并手动去掉尾部 `\n`（PIN 必须 `\0` 结尾，不能含换行）；执行前有「proceed? [y/n]」二次确认——PIN 操作有 3 次错误锁卡风险，确认机制是安全设计。

#### 3.5.2 禁用 PIN 码（P42）
代码与 3.5.1 类似，**区别仅为调用的函数替换为 `ql_sim_disable_pin()`**。

#### 3.5.3 验证 PIN 码（P42）
代码与 3.5.1 类似，**区别仅为调用的函数替换为 `ql_sim_verify_pin()`**。

#### 3.5.4 更改 PIN 码（P42-44）
`item_ql_sim_change_pin`：相比 enable，需读两个 PIN 值——`old_pin_value` 与 `new_pin_value`（各 `char[QL_SIM_PIN_MAX*2]`，均用 fgets + 去 `\n`）。同样选 slot/app_type/pin，二次确认后调用：
```c
ret = ql_sim_change_pin(slot, app_type, pin, old_pin_value, new_pin_value);
```
成功 `ok`，失败打印 `ret`。

#### 3.5.5 解锁 (U)SIM 卡并设置新 PIN 码（P45-47）
`item_ql_sim_unblock_pin`：读 `puk_value` 与 `new_pin_value`（各 `char[QL_SIM_PIN_MAX*2]`，fgets + 去 `\n`）。选 slot/app_type/pin，二次确认后调用：
```c
ret = ql_sim_unblock_pin(slot, app_type, pin, puk_value, new_pin_value);
```
成功 `ok`，失败打印 `ret`。
> **要点**：解锁场景就是 PIN 被锁（输错 ≥3 次）后用 PUK 重置 PIN。注意 PUK 也有错误次数限制（一般 10 次），输错 PUK 超限将永久锁死卡。

### 3.6 (U)SIM 卡文件操作（P47 起）
#### 3.6.1 从 (U)SIM 卡文件读取数据（P47-49）
`item_ql_sim_read_file`：定义 `ql_sim_file_t file = {0}`；选 slot/app_type；读文件路径与记录索引：
```c
printf("please enter file path(at most %d hex[0·9A-F], e.g 3F002FE2): ", QL_SIM_PATH_MAX);
if (NULL == fgets(file.path, QL_SIM_PATH_MAX, stdin)) { ... return; }
len = strlen(file.path);
if ('\n' == file.path[len-1]) { file.path[len-1] = 0; len--; }
file.path_len = (uint32_t)len;

printf("please enter record index(0 for transparent access): ");
scanf("%hhu", (uint8_t *)&file.record_idx); getchar();

ret = ql_sim_read_file(slot, app_type, &file);
if (ret == QL_ERR_OK) {
    printf("data length: %u\n", file.data_len);
    uint32_t i = 0;
    printf("data: ");
    for (i = 0; i < file.data_len; i++) { printf("%02x ", file.data[i]); }
    printf("\n");
} else { printf("failed, ret = %d\n", ret); }
```
> **要点**：文件路径是十六进制字符串（如 `3F002FE2`——MF `3F00` 下的 EF_ICCID `2FE2`）；`record_idx=0` 表示透明文件访问；读出的 `data` 按 `%02x` 十六进制逐字节打印。

#### 3.6.2 把数据写入 (U)SIM 卡文件（P50-52）
`item_ql_sim_write_file`：除路径/record_idx 外，还需输入待写数据（十六进制流，以 `q` 结束）和 offset：
```c
printf("please enter data(hex, end with `q'): ");
while (1 == scanf("%hhx", &v)) { file.data[file.data_len++] = v; }
getchar();   // read `q'
getchar();   // read '\n'

printf("please enter data offset: ");
scanf("%hu", &file.offset); getchar();

ret = ql_sim_write_file(slot, app_type, &file);
```
> **要点**：`data_len` 由输入字节数累加得出；`offset` 仅对透明文件生效（见 2.3.13.1）。注意写卡文件需卡本身允许写（多数 EF 有访问条件限制，写保护文件会失败）。

#### 3.6.3 获取 (U)SIM 卡文件信息（P52-53）
`item_ql_sim_get_file_info`：定义 `ql_sim_file_info_t info = {0}`；选 slot/app_type，读 path（同上去 `\n`、设 path_len）；调用 `ql_sim_get_file_info(slot, app_type, &info)`，成功打印：
```c
printf("========= FILE INFO =========\n");
printf("path: %s\n", info.path);
printf("type: %s\n", file_type_desc(info.file_type));
printf("file size: %hu\n", info.file_size);
printf("record size: %hu\n", info.record_size);
printf("record count: %hu\n", info.record_count);
```
**实测执行结果（以文件 `3F002FE2` 为例，P53-54）**：
```
please enter slot(1 or 2): 1
please enter app type(0-unknown, 1-3gpp, 2-3gpp2, 3-isim): 1
please enter file path(at most 20 hex[0·9A-F], e.g 3F002FE2): 3F002FE2
========= FILE INFO =========
path: 3F002FE2
type: transparent
file size: 10
record size: 0
record count: 0
```
> **解读**：`3F002FE2` 即 EF_ICCID，是透明文件（transparent），大小 10 字节（ICCID 压缩 BCD 编码 10 字节=20 位数字）；透明文件 record_size/record_count 均为 0。`file_type_desc()` 是示例自带的枚举→字符串转换辅助函数。

### 3.7 获取 (U)SIM 卡状态（P54-55）
`item_ql_sim_get_card_info`：定义 `ql_sim_card_info_t info = {0}`；只选 slot（无 app_type）；调用 `ql_sim_get_card_info(slot, &info)`，失败直接返回；成功后分别打印卡级 state/type 及三套应用（3gpp/3gpp2/isim）的 app_state 与 PIN1/PUK1/PIN2/PUK2 剩余次数：
```c
printf("========= CARD INFO =========\n");
printf("state: %s\n", card_state_desc(info.state));
printf("type: %s\n", card_type_desc(info.type));
printf("3gpp:\n");
printf(" app state: %s\n", card_app_state_desc(info.app_3gpp.app_state));
printf(" PIN 1 retries: %hhu\n", info.app_3gpp.pin1_num_retries);
printf(" PUK 1 retries: %hhu\n", info.app_3gpp.puk1_num_retries);
printf(" PIN 2 retries: %hhu\n", info.app_3gpp.pin2_num_retries);
printf(" PUK 2 retries: %hhu\n", info.app_3gpp.puk2_num_retries);
printf("3gpp2:\n"); ...同上 app_3gpp2...
printf("isim:\n");  ...同上 app_isim...
```
**实测执行结果（P55）**：
```
========= CARD INFO =========
state: present
type: UICC
3gpp:
 app state: ready
 PIN 1 retries: 3
 PUK 1 retries: 10
 PIN 2 retries: 1
 PUK 2 retries: 10
3gpp2:
 app state: N/A
 PIN 1 retries: 0
 ...全 0...
isim:
 app state: N/A
 PIN 1 retries: 0
 ...全 0...
```
> **解读（实战参考）**：一张正常 UICC 卡——`state=present`、`type=UICC`、3GPP 应用 `app_state=ready`；PIN1 剩 3 次（未使能锁时为满值）、PUK1 剩 10 次；该卡 PIN2 仅剩 1 次（接近锁定，需谨慎）。3GPP2/ISIM 应用未激活，显示 `N/A` + 全 0。`card_state_desc`/`card_type_desc`/`card_app_state_desc` 均为示例自带枚举→字符串辅助函数。**这是 modem_mng 判断卡健康度最直接的接口**：可据 `state`/`app_state`/retries 区分「无卡 / 卡故障 / 需 PIN / 已就绪」。

### 3.8 电话簿操作（P56 起）
#### 3.8.1 读取电话簿信息（P56-57）
`item_ql_sim_read_phone_book`：定义 `ql_sim_phone_book_record_t record = {0}`，`uint8_t record_idx`；选 slot/app_type，读 record_idx（`scanf("%hhu", &record_idx)`）；调用：
```c
ret = ql_sim_read_phone_book(slot, app_type, QL_SIM_PB_DEFAULT_PATH, record_idx, &record);
if (ret == QL_ERR_OK) {
    printf("Name: %s\n", record.name);
    printf("Number: %s\n", record.number);
} else { printf("failed, ret = %d\n", ret); }
```
> **要点**：使用默认电话簿路径 `QL_SIM_PB_DEFAULT_PATH`；按 `record_idx` 定位单条记录。

#### 3.8.2 修改电话簿信息（P57-59）
`item_ql_sim_write_phone_book`（原文起始行误写 `{4`）：选 slot/app_type，读 record_idx，再读 name 与 number：
```c
printf("please enter name(at most %d chars): ", QL_SIM_PHONE_BOOK_NAME_MAX - 1);
if (NULL == fgets(record.name, QL_SIM_PHONE_BOOK_NAME_MAX, stdin)) {
    printf("\nname will be set to 0\n");
} else {
    len = strlen(record.name);
    if ('\n' == record.name[len-1]) { record.name[len-1] = 0; }
}
printf("please enter number(at most %d digits): ", QL_SIM_PHONE_BOOK_NUMBER_MAX - 1);
if (NULL == fgets(record.number, QL_SIM_PHONE_BOOK_NUMBER_MAX, stdin)) {
    printf("\nnumber will be set to 0\n");
} else {
    len = strlen(record.number);
    if ('\n' == record.number[len-1]) { record.number[len-1] = 0; }
}
ret = ql_sim_write_phone_book(slot, app_type, QL_SIM_PB_DEFAULT_PATH, record_idx, &record);
```
成功 `ok`，失败打印 `ret`。
> **要点**：name/number 最大可输入长度为 `MAX-1`（留 `\0`）；若读取失败则该字段置 0（清空该条记录的对应字段）。写电话簿即可新增/覆盖/清空指定 record_idx 的联系人。

### 3.9 发送 APDU（P60 起）
#### 3.9.1 打开逻辑通道（P60）
`item_ql_sim_open_logical_channel`：`uint8_t channel_id = 0`；只选 slot；调用并回填 channel_id：
```c
ret = ql_sim_open_logical_channel(slot, &channel_id);
if (ret == QL_ERR_OK) { printf("channel id: %hhu\n", channel_id); }
else                  { printf("failed, ret = %d\n", ret); }
```
> **要点**：成功后拿到 `channel_id`，后续 send_apdu / close 都要带这个 ID。

#### 3.9.2 关闭逻辑通道（P61）
`item_ql_sim_close_logical_channel`：选 slot，读 channel_id（`scanf("%hhu", &channel_id)`）；调用：
```c
ret = ql_sim_close_logical_channel(slot, channel_id);
```
成功 `ok`，失败打印 `ret`。

#### 3.9.3 发送 APDU（P62-63）
`item_ql_sim_send_apdu`：动态分配 APDU 结构体（注意大小，含两个 1024 字节缓冲）：
```c
ql_sim_apdu_t *p_apdu = NULL;
uint8_t v = 0, channel_id = 0;

p_apdu = calloc(1, sizeof(*p_apdu));
if (NULL == p_apdu) { printf("run out of memory\n"); return; }

/* 选 slot（出错时 free(p_apdu) 后 return） */
...
printf("please enter channel id: ");
scanf("%hhu", &channel_id); getchar();

printf("please enter apdu data(hex, end with `q'): ");
while (1 == scanf("%hhx", &v)) { p_apdu->req_apdu[p_apdu->req_apdu_len++] = v; }
getchar();   // read `q'
getchar();   // read '\n'

ret = ql_sim_send_apdu(slot, channel_id, p_apdu);
if (ret == QL_ERR_OK) {
    uint32_t i = 0;
    printf("repsonse apdu: ");
    for (i = 0; i < p_apdu->resp_apdu_len; i++) { printf("%c", p_apdu->resp_apdu[i]); }
    printf("\n");
} else {
    printf("failed, ret = %d\n", ret);
}
free(p_apdu);
```
> **要点**：
> - 用 `calloc` 动态分配 `ql_sim_apdu_t`——结构体含两个 `QL_SIM_APDU_DATA_MAX(1024)` 字节数组，约 2KB+，不适合栈上分配。
> - 请求 APDU 以十六进制流输入、`q` 结束，逐字节填入 `req_apdu` 并累加 `req_apdu_len`。
> - 响应用 `%c` 逐字节打印（示例假定响应是可打印 ASCII；若为二进制状态字应改 `%02x`）。
> - 所有出错分支都 `free(p_apdu)` 后返回——无内存泄漏。
> - **前置条件**：channel_id 必须是先前 `ql_sim_open_logical_channel` 拿到的有效通道。完整 APDU 流程 = open_logical_channel → send_apdu（可多次）→ close_logical_channel。

---

## 4 附录 参考文档及术语缩写（P64-65）

### 表 2：参考文档
| 编号 | 文档名称 |
|---|---|
| [1] | Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导 |

### 表 3：术语缩写
| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| 3GPP | 3rd Generation Partnership Project | 第三代合作伙伴计划 |
| 3GPP2 | 3rd Generation Partnership Project 2 | 第三代合作伙伴计划 2 |
| APDU | Application Protocol Data Unit | 应用协议数据单元 |
| API | Application Programming Interface | 应用程序编程接口 |
| ASCII | American Standard Code for Information Interchange | 美国信息交换标准码 |
| CK | Control Key | 控制密码 |
| CSIM | CDMA2000 Subscriber Identity Module | CDMA2000 用户识别模块 |
| GID1 | Group Identifier Level 1 | 组识别符（级别 1） |
| GID2 | Group Identifier Level 2 | 组识别符（级别 2） |
| GSM | Global System for Mobile | 全球移动通信系统 |
| ICCID | Integrated Circuit Card ID | 集成电路卡识别码 |
| ID | Identifier | 标识符 |
| IMSI | International Mobile Subscriber Identity | 国际移动用户识别码 |
| IoT | Internet of Things | 物联网 |
| ISIM | IP Multimedia Service Identity Module | IP 多媒体服务身份模块 |
| MCC | Mobile Country Code | 移动设备国家代码 |
| MNC | Mobile Network Code | 移动设备网络代码 |
| PIN | Personal Identification Number | 个人识别密码 |
| PUK | PIN Unblocking Key | PIN 码解锁密码 |
| RIM | Remote Interference Management | 远程干扰管理 |
| RUIM | Removable User Identity Module | 可移动用户识别模块 |
| SDK | Software Development Kit | 软件开发工具包 |
| SIM | Subscriber Identification Module | 用户识别模块 |
| UICC | Universal Integrated Circuit Card | 通用集成电路卡 |
| (U)SIM | Universal Subscriber Identity Module | 全球用户识别模块 |

---

## 全文要点总结与对 modem_mng / 双卡场景的工程参考

1. **API 调用范式统一**：所有 API 第一参数是逻辑卡槽 `QL_SIM_SLOT_E`（实际只支持 `SLOT_1`），凡涉及 IMSI/号码/PIN/文件/电话簿的还需 `QL_SIM_APP_TYPE_E`（国内卡用 `3GPP`）。生命周期为 `ql_sim_init()` → 业务 API → `ql_sim_deinit()`。

2. **双卡单待的底层支撑（V1.0.1 新增，最关键）**：
   - 硬件 2 个物理卡槽（`QL_SIM_PHY_SLOT_1/2`），但协议栈同时只激活 1 个逻辑卡槽 ⇒ **双卡单待**。
   - `ql_sim_switch_slot(log_slot, phy_slot)` 把逻辑卡槽重映射到指定物理卡槽 = 切卡原语。
   - `ql_sim_get_active_slots()` 查当前激活的物理卡槽。
   - 切卡闭环：`get_active_slots` → 决策 → `switch_slot` → 重新走入网/拨号 → 验证。切换会复位卡会话，需重读卡 + 重注册网络。

3. **卡健康度判定**：`ql_sim_get_card_info` 返回 `state`（present/absent/各类 error）、每个应用的 `app_state`（ready/pin1_req/puk1_req/illegal…）及 PIN/PUK 剩余次数。可据此把故障分层——无卡 / 卡硬件故障（接触、电压、ATR）/ 需 PIN / 已就绪 / 黑卡，区别于网络侧异常，指导恢复策略（卡硬件故障软件重拨无效）。

4. **运营商/PLMN 能力**：`ql_sim_get_operators` 读卡内运营商列表（MCC+MNC，最多 24 个），可服务海外选网 `[OPER]` 特性的「枚举可用 PLMN」。

5. **服务级异常告警**：`ql_sim_set_service_error_cb` 在 SIM 服务进程崩溃时回调 `QL_ERR_ABORTED`，应用应重新 init 或上报。

6. **并发/重入约束**：API 不支持并发，且禁止在回调内调用 API ⇒ 集成时需单工作线程串行化、回调里只置标志/投递事件。

7. **APDU/逻辑通道**：完整链路 open_logical_channel → send_apdu → close_logical_channel；APDU 结构体含 2×1024 字节缓冲，应堆分配。

8. **文档质量注记（笔误，移植/对接时留意）**：`get_phone_num` 末参数名笔误 `phone_num`、`read_file` 原型 `Int`、`ql_sim_apdu_t` 中 `resp_apdu` 长度描述误引 `req_apdu_len`、结构体名 `QL_SIM_MCC_LENGHT`/枚举 `INITALIZATING` 拼写错误、`write_phone_book` 示例起始 `{4`。这些不影响接口语义，但对照真实头文件 `ql_sim.h` 为准。

<!-- GENERATION_COMPLETE -->
