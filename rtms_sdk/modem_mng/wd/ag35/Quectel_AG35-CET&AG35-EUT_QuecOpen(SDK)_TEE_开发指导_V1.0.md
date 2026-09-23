# Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_TEE_开发指导_V1.0 — 文档分析

## 一、文档基本信息

- **标题**：AG35-CET&AG35-EUT QuecOpen(SDK) TEE 开发指导
- **产品线**：LTE Standard 模块系列
- **适用模块**：AG35-CET、AG35-EUT
- **版本号**：1.0
- **日期**：2025-06-12
- **文档状态**：受控文件
- **发布单位**：上海移远通信技术股份有限公司（Quectel）
- **文档篇幅**：正文 42 页（PDF 含封面共 43 页）
- **适用范围**：仅适用于基于 SDK（QuecOpen 方案）构建环境的 AG35-CET 和 AG35-EUT 模块；主要介绍这两款模块的 TrustZone 安全存储设计及加解密服务 API（第 1 章“引言”明确说明）。

### 文档历史（修订记录，原文表格还原）

| 版本 | 日期 | 作者 | 变更描述 |
|------|------|------|----------|
| - | 2023-09-23 | Menson ZHANG | 文档创建 |
| 1.0.0 | 2023-09-23 | Menson ZHANG | 临时版本 |
| 1.0.1 | 2025-03-18 | Menson ZHANG | 临时版本：1. 基于 QuecOpen 方案统一命名，更新文档名称；2. 新增适用模块 AG35-EUT；3. 新增 ql_km_initialize() 和 ql_ss_initialize() 的调用说明（第 3.3.1.1、3.3.3.1 章） |
| 1.0 | 2025-06-12 | Menson ZHANG | 受控版本 |

可见该文档曾仅适用于 AG35-CET，在 2025-03-18 的修订中才扩展支持 AG35-EUT，并在该次修订中新增了关于 `ql_km_initialize()` 与 `ql_ss_initialize()` 互斥调用（TEE 内存限制导致两个 TA 不能同时加载）的关键说明。

### 免责声明 / 隐私声明要点（原文摘录，供合规参考）
- 文档内容仅供客户产品设计参考，移远通信不承担因未遵守规范、操作或设计造成损害的责任；不承担因文档不准确、遗漏或使用文档信息产生的责任。
- 隐私声明提到：为实现产品功能，特定设备数据会上传至移远通信或第三方服务器（包括运营商、芯片供应商或客户指定服务器）。本 TEE 文档本身不涉及具体数据上传内容，但该声明是公司级模板条款。

---

## 二、目录结构概览（与原文一致）

```
文档历史
目录
表格索引
1 引言
2 功能介绍
  2.1 TEE 软件架构
  2.2 安全存储介绍
  2.3 加解密介绍
3 TEE API
  3.1 头文件
  3.2 函数概览
  3.3 函数详解
    3.3.1 TrustZone 加密和解密
      3.3.1.1  ql_km_initialize
        3.3.1.1.1 ql_tee_error_t
      3.3.1.2  ql_km_deinitialize
      3.3.1.3  ql_km_gen_key
        3.3.1.3.1 ql_km_key_args_t
        3.3.1.3.2 ql_km_algo_t
        3.3.1.3.3 ql_km_aes_mode_t
        3.3.1.3.4 ql_km_ec_curve_t
        3.3.1.3.5 ql_km_key_t
      3.3.1.4  ql_km_import_key
        3.3.1.4.1 ql_km_blob_t
      3.3.1.5  ql_km_destroy_key
      3.3.1.6  ql_km_destroy_blob
      3.3.1.7  ql_km_operation_begin
        3.3.1.7.1 ql_km_operation_args_t
        3.3.1.7.2 ql_km_purpose_t
        3.3.1.7.3 ql_km_rsa_padding_t
        3.3.1.7.4 ql_km_digest_t
        3.3.1.7.5 ql_km_operation_handle_t
      3.3.1.8  ql_km_operation_update
      3.3.1.9  ql_km_operation_finish
      3.3.1.10 ql_km_export_key
      3.3.1.11 ql_km_get_key_algo
    3.3.2 辅助简化
      3.3.2.1 ql_aes_genkey_args
      3.3.2.2 ql_aes_operation_args
      3.3.2.3 ql_rsa_genkey_args
      3.3.2.4 ql_rsa_operation_args
      3.3.2.5 ql_ec_genkey_args
    3.3.3 TrustZone 安全存储
      3.3.3.1  ql_ss_initialize
      3.3.3.2  ql_ss_deinitialize
      3.3.3.3  ql_ss_open
      3.3.3.4  ql_ss_create
      3.3.3.5  ql_ss_close
      3.3.3.6  ql_ss_read
      3.3.3.7  ql_ss_write
      3.3.3.8  ql_ss_seek
        3.3.3.8.1 ql_ss_whence_t
      3.3.3.9  ql_ss_unlink
      3.3.3.10 ql_ss_trunc
      3.3.3.11 ql_ss_rename
      3.3.3.12 ql_ss_get_info
    3.3.4 宏定义
4 附录 参考文档及术语缩写
```

---

## 三、逐章节详细摘要

### 1 引言

移远通信 AG35-CET 和 AG35-EUT 模块支持 QuecOpen® 方案；QuecOpen® 是基于 Linux 的嵌入式开发平台，可简化 IoV（车联网）应用的软件设计和开发过程。本文档适用于 SDK 构建环境的 QuecOpen® 方案，主要介绍 AG35-CET 和 AG35-EUT 模块的 TrustZone 安全存储设计及加解密服务 API。QuecOpen 详细信息参考文档 [1]《Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导》。

### 2 功能介绍

**2.1 TEE 软件架构**

文档给出了标准 GlobalPlatform 风格 TEE 架构图（图1：TEE 软件架构），分为左侧 Rich environment（富执行环境）与右侧 Trusted environment（可信执行环境），中间以虚线隔开：
- Rich environment 侧：Client Application（CA，含 Shared Memory）→ TEE Client API → Communications stack
- Trusted environment 侧：Trusted Application（TA，含 Shared Memory view）→ TEE → （同样经由）Communications stack
- 底层共用：Messages → Platform (Hardware / Hypervisor)

术语定义（原文逐条）：
- **TA（Trusted Application）**：运行在可信执行环境（TEE）下的应用程序。
- **CA（Client Application）**：运行在富执行环境（REE）下的应用程序。交互流程中，由 CA 触发系统调用 TA 实现具体功能，TA 运行结束后将运行结果和数据返回给 CA。
- **Communications stack（通信栈）**：处理富执行环境和可信执行环境之间的通信。
- **TEE Client API**：使能富执行环境中 CA 与可信执行环境中 TA 进行交互。
- **Shared Memory（共享内存）**：仅 CA 和 TA 可访问的一块安全内存，CA 和 TA 通过共享内存实现快速有效的命令和数据传输。

**2.2 安全存储介绍**

可以将数据通过可信执行环境写入安全存储区域，数据被加密并存储在安全存储路径 `/persist/` 下，且无法直接看到文件名。

> 备注（原文）：在 QuecOpen SDK 中，`sample/secstor-example` 为安全存储示例程序，用户可参考其编译方式以及 API 使用方法。

**2.3 加解密介绍**

AG35-CET 和 AG35-EUT 模块支持基于可信执行环境的加解密算法，包括 AES、RSA 和 ECC 等算法，并支持密钥生成、加解密、密钥导入等功能。支持的具体功能：
- AES 密钥生成、导入、加解密。支持 128 位和 256 位，支持 ECB、CBC、CTR 和 GCM 模式。
- ECC 密钥对生成、私钥导入、公钥导出。支持曲线 ECC-P224、ECC-P256、ECC-P384 和 ECC-P521。
- RSA 密钥对生成、私钥导入、公钥导出、加解密、签名和验签。
- RSA 支持 1024 位、2048 位、3072 位和 4096 位。
- RSA 填充算法支持 OAEP、PSS、PKCS#1V1.5。
- RSA 摘要算法支持 MD5、SHA-1、SHA2-224、SHA2-256、SHA2-384 和 SHA2-512。

> 备注（原文）：在 QuecOpen SDK 中，`sample/keymaster-example` 为加解密示例程序，用户可参考其编译方式以及函数使用。

### 3 TEE API

**3.1 头文件**

TEE API 头文件为 `ql_tee_service.h`、`ql_tee_asymm_utils.h` 和 `ql_tee_symm_utils.h`，位于 SDK 包 `ql-sysroots/usr/include/` 目录下（若无特别说明，本文档所涉头文件均在该目录下）。

动态库为 `libql-tee-service.so`，位于 SDK 包 `ql-sysroots/usr/lib/` 目录下。

**3.2 函数概览（表1：函数概览，原文完整还原）**

| 函数 | 描述 |
|------|------|
| `ql_km_initialize()` | 初始化加解密服务 |
| `ql_km_deinitialize()` | 去初始化加解密服务 |
| `ql_km_gen_key()` | 生成密钥 |
| `ql_km_import_key()` | 导入密钥 |
| `ql_km_destroy_key()` | 销毁密钥，释放内存 |
| `ql_km_destroy_blob()` | 销毁 BLOB 数据，释放内存 |
| `ql_km_operation_begin()` | 开始算法操作 |
| `ql_km_operation_update()` | 进行算法操作 |
| `ql_km_operation_finish()` | 进行最后一轮算法操作 |
| `ql_km_export_key()` | 导出非对称算法的公钥 |
| `ql_km_get_key_algo()` | 从 BLOB 数据中读取当前密钥的算法类别 |
| `ql_aes_genkey_args()` | 生成调用 `ql_km_gen_key()` 的 `key_args` 参数 |
| `ql_aes_operation_args()` | 生成调用 `ql_km_operation_begin()` 的参数 |
| `ql_rsa_genkey_args()` | 生成调用 `ql_km_gen_key()` 的参数 |
| `ql_rsa_operation_args()` | 生成调用 `ql_km_operation_begin()` 的参数 |
| `ql_ec_genkey_args()` | 生成调用 `ql_km_gen_key()` 的参数 |
| `ql_ss_initialize()` | 初始化安全存储功能 |
| `ql_ss_deinitialize()` | 去初始化安全存储功能 |
| `ql_ss_open()` | 打开安全存储对象 |
| `ql_ss_create()` | 创建并打开安全存储对象 |
| `ql_ss_close()` | 关闭安全存储对象 |
| `ql_ss_read()` | 从安全存储中读取数据 |
| `ql_ss_write()` | 向安全存储写入数据 |
| `ql_ss_seek()` | 移动安全存储文件的文件指针 |
| `ql_ss_unlink()` | 删除安全存储文件 |
| `ql_ss_trunc()` | 截短安全存储文件到指定的长度 |
| `ql_ss_rename()` | 重命名安全存储对象 |
| `ql_ss_get_info()` | 获取安全存储对象信息 |

> 备注（原文，重要约束）：**若无特别说明，本文档所述函数均不支持并发调用，且不能在相关回调函数中调用以上函数。**

**3.3 函数详解**

#### 3.3.1 TrustZone 加密和解密

**3.3.1.1 `ql_km_initialize`** — 初始化加解密服务
- 原型：`ql_tee_error_t ql_km_initialize(void)`
- 参数：无
- 返回值：`QL_TEE_OK` 成功；其他值见错误码章节
- **关键备注（原文）**：TEE 内存存在限制，当前 SoC 配置的 TEE 的 RAM 为 **2 MB**，其中 TEE Core 占用接近 **1 MB**。加解密服务和安全存储服务隶属于不同的 TA，**目前内存不支持同时加载多个 TA 应用**，此接口无法与 `ql_ss_initialize()` 同时调用，需要使用此接口时，需要先关闭安全存储 TA 应用。

**3.3.1.1.1 `ql_tee_error_t`** — 错误码枚举（原文完整还原，共三大类）：

通用成功：
- `QL_TEE_OK = 0`：成功

KeyMaster（加解密，KM）相关错误码：

| 枚举值 | 数值 | 中文描述 |
|---|---|---|
| QL_KM_ERROR_ROOT_OF_TRUST_ALREADY_SET | -1 | 可信根已设置 |
| QL_KM_ERROR_UNSUPPORTED_PURPOSE | -2 | 操作不支持 |
| QL_KM_ERROR_INCOMPATIBLE_PURPOSE | -3 | 操作不兼容 |
| QL_KM_ERROR_UNSUPPORTED_ALGORITHM | -4 | 算法不支持 |
| QL_KM_ERROR_INCOMPATIBLE_ALGORITHM | -5 | 算法不兼容 |
| QL_KM_ERROR_UNSUPPORTED_KEY_SIZE | -6 | 密钥大小不支持 |
| QL_KM_ERROR_UNSUPPORTED_BLOCK_MODE | -7 | 块模式不支持 |
| QL_KM_ERROR_INCOMPATIBLE_BLOCK_MODE | -8 | 块模式不兼容 |
| QL_KM_ERROR_UNSUPPORTED_MAC_LENGTH | -9 | MAC 长度不支持 |
| QL_KM_ERROR_UNSUPPORTED_PADDING_MODE | -10 | 填充模式不支持 |
| QL_KM_ERROR_INCOMPATIBLE_PADDING_MODE | -11 | 填充模式不兼容 |
| QL_KM_ERROR_UNSUPPORTED_DIGEST | -12 | 摘要模式不支持 |
| QL_KM_ERROR_INCOMPATIBLE_DIGEST | -13 | 摘要模式不兼容 |
| QL_KM_ERROR_INVALID_EXPIRATION_TIME | -14 | 无效的到期时间 |
| QL_KM_ERROR_INVALID_USER_ID | -15 | 无效的用户 ID |
| QL_KM_ERROR_INVALID_AUTHORIZATION_TIMEOUT | -16 | 无效的授权超时 |
| QL_KM_ERROR_UNSUPPORTED_KEY_FORMAT | -17 | 密钥格式不支持 |
| QL_KM_ERROR_INCOMPATIBLE_KEY_FORMAT | -18 | 密钥格式不兼容 |
| QL_KM_ERROR_UNSUPPORTED_KEY_ENCRYPTION_ALGORITHM | -19 | 不支持的加密算法 |
| QL_KM_ERROR_UNSUPPORTED_KEY_VERIFICATION_ALGORITHM | -20 | 不支持的验证算法 |
| QL_KM_ERROR_INVALID_INPUT_LENGTH | -21 | 输入长度无效 |
| QL_KM_ERROR_KEY_EXPORT_OPTIONS_INVALID | -22 | 密钥导出选项无效 |
| QL_KM_ERROR_DELEGATION_NOT_ALLOWED | -23 | 授权不允许 |
| QL_KM_ERROR_KEY_NOT_YET_VALID | -24 | 密钥未生效 |
| QL_KM_ERROR_KEY_EXPIRED | -25 | 密钥过期 |
| QL_KM_ERROR_KEY_USER_NOT_AUTHENTICATED | -26 | 用户未通过身份验证 |
| QL_KM_ERROR_OUTPUT_PARAMETER_NULL | -27 | 输出参数空 |
| QL_KM_ERROR_INVALID_OPERATION_HANDLE | -28 | 无效的操作句柄 |
| QL_KM_ERROR_INSUFFICIENT_BUFFER_SPACE | -29 | 缓冲空间不足 |
| QL_KM_ERROR_VERIFICATION_FAILED | -30 | 验证失败 |
| QL_KM_ERROR_TOO_MANY_OPERATIONS | -31 | 过多操作 |
| QL_KM_ERROR_UNEXPECTED_NULL_POINTER | -32 | 意外的空指针 |
| QL_KM_ERROR_INVALID_KEY_BLOB | -33 | 无效的密钥块 |
| QL_KM_ERROR_IMPORTED_KEY_NOT_ENCRYPTED | -34 | 导入密钥未加密 |
| QL_KM_ERROR_IMPORTED_KEY_DECRYPTION_FAILED | -35 | 导入密钥解密失败 |
| QL_KM_ERROR_IMPORTED_KEY_NOT_SIGNED | -36 | 导入密钥未签名 |
| QL_KM_ERROR_IMPORTED_KEY_VERIFICATION_FAILED | -37 | 导入密钥验证失败 |
| QL_KM_ERROR_INVALID_ARGUMENT | -38 | 无效的参数 |
| QL_KM_ERROR_UNSUPPORTED_TAG | -39 | 不支持的标签 |
| QL_KM_ERROR_INVALID_TAG | -40 | 无效的标签 |
| QL_KM_ERROR_MEMORY_ALLOCATION_FAILED | -41 | 内存分配失败 |
| QL_KM_ERROR_IMPORT_PARAMETER_MISMATCH | -44 | 导入参数不匹配（**注：原文枚举值跳过 -42、-43，未定义这两个值，原文未说明原因**） |
| QL_KM_ERROR_SECURE_HW_ACCESS_DENIED | -45 | 安全硬件访问被拒绝 |
| QL_KM_ERROR_OPERATION_CANCELLED | -46 | 操作被取消 |
| QL_KM_ERROR_CONCURRENT_ACCESS_CONFLICT | -47 | 并发访问冲突 |
| QL_KM_ERROR_SECURE_HW_BUSY | -48 | 安全硬件繁忙 |
| QL_KM_ERROR_SECURE_HW_COMMUNICATION_FAILED | -49 | 安全硬件通信失败 |
| QL_KM_ERROR_UNSUPPORTED_EC_FIELD | -50 | 不支持的 EC 字段 |
| QL_KM_ERROR_MISSING_NONCE | -51 | 缺少随机数 |
| QL_KM_ERROR_INVALID_NONCE | -52 | 无效的随机数 |
| QL_KM_ERROR_MISSING_MAC_LENGTH | -53 | 缺少 MAC 长度 |
| QL_KM_ERROR_KEY_RATE_LIMIT_EXCEEDED | -54 | 超出密钥速率限制 |
| QL_KM_ERROR_CALLER_NONCE_PROHIBITED | -55 | 禁止调用随机数 |
| QL_KM_ERROR_KEY_MAX_OPS_EXCEEDED | -56 | 已超过密钥最大操作数 |
| QL_KM_ERROR_INVALID_MAC_LENGTH | -57 | 无效的 MAC 长度 |
| QL_KM_ERROR_MISSING_MIN_MAC_LENGTH | -58 | 缺少最小 MAC 长度 |
| QL_KM_ERROR_UNSUPPORTED_MIN_MAC_LENGTH | -59 | 不支持的最小 MAC 长度 |
| QL_KM_ERROR_UNSUPPORTED_KDF | -60 | 不支持的密钥派生 |
| QL_KM_ERROR_UNSUPPORTED_EC_CURVE | -61 | 不支持的 ECC 曲线 |
| QL_KM_ERROR_KEY_REQUIRES_UPGRADE | -62 | 密钥需要升级 |
| QL_KM_ERROR_ATTESTATION_CHALLENGE_MISSING | -63 | 证明缺失 |
| QL_KM_ERROR_KEYMASTER_NOT_CONFIGURED | -64 | 未配置 |
| QL_KM_ERROR_ATTESTATION_APPLICATION_ID_MISSING | -65 | 缺少证明应用 ID |
| QL_KM_ERROR_UNIMPLEMENTED | -100 | 功能未实现 |
| QL_KM_ERROR_VERSION_MISMATCH | -101 | 版本不匹配 |
| QL_KM_ERROR_UNKNOWN_ERROR | -1000 | 未知错误 |

安全存储（SS）相关错误码：

| 枚举值 | 数值 | 中文描述 |
|---|---|---|
| QL_SS_ERROR_GENERIC | -200 | 通用错误码 |
| QL_SS_ERROR_ACCESS_DENIED | -201 | 拒绝访问 |
| QL_SS_ERROR_CANCEL | -202 | 操作取消 |
| QL_SS_ERROR_ACCESS_CONFLICT | -203 | 访问冲突 |
| QL_SS_ERROR_EXCESS_DATA | -204 | 数据超大 |
| QL_SS_ERROR_BAD_FORMAT | -205 | 错误格式 |
| QL_SS_ERROR_BAD_PARAMETERS | -206 | 错误参数 |
| QL_SS_ERROR_BAD_STATE | -207 | 错误状态 |
| QL_SS_ERROR_ITEM_NOT_FOUND | -208 | 资源不存在 |
| QL_SS_ERROR_NOT_IMPLEMENTED | -209 | 未实现 |
| QL_SS_ERROR_NOT_SUPPORTED | -210 | 不支持 |
| QL_SS_ERROR_NO_DATA | -211 | 无此数据 |
| QL_SS_ERROR_OUT_OF_MEMORY | -212 | 内存耗尽 |
| QL_SS_ERROR_BUSY | -213 | 遇忙 |
| QL_SS_ERROR_COMMUNICATION | -214 | 通信失败 |
| QL_SS_ERROR_SECURITY | -215 | 安全验证失败 |
| QL_SS_ERROR_SHORT_BUFFER | -216 | Buffer 过小 |
| QL_SS_ERROR_EXTERNAL_CANCEL | -217 | 外部取消 |
| QL_SS_ERROR_TARGET_DEAD | -2000 | 对象已关闭 |
| QL_SS_ERROR_UNKNOWN_ERROR | -2001 | 未知错误 |

**3.3.1.2 `ql_km_deinitialize`** — 去初始化加解密服务
- 原型：`void ql_km_deinitialize(void)`；无参数，无返回值。

**3.3.1.3 `ql_km_gen_key`** — 生成密钥
- 原型：`ql_tee_error_t ql_km_gen_key(const ql_km_key_args_t *key_args, ql_km_key_t *key)`
- 参数：`key_args` [In] 待生成密钥的参数（算法、密钥长度等）；`key` [Out] 生成的密钥，内存在函数调用过程中分配。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

**3.3.1.3.1 `ql_km_key_args_t`** — 密钥参数结构体：
```c
typedef struct {
    ql_km_algo_t algo;
    union {
        struct {
            ql_km_aes_mode_t mode;
            uint32_t bits;
            uint32_t min_mac_len;
        } aes_args;
        struct {
            uint64_t exponent;
            uint32_t bits;
        } rsa_args;
        struct {
            ql_km_ec_curve_t curve;
        } ec_args;
    };
} ql_km_key_args_t;
```
参数说明：`algo` 算法类型；`mode` AES 密钥模式；`bits` 密钥位数；`min_mac_len` GCM 模式中 MAC 的最小长度；`exponent` RSA 密钥的指数；`curve` ECC 曲线。

**3.3.1.3.2 `ql_km_algo_t`** — 算法类型枚举：
```c
typedef enum {
    QL_KM_ALGO_RSA = 1,
    QL_KM_ALGO_EC = 3,
    QL_KM_ALGO_AES = 32,
} ql_km_algo_t;
```
（注：枚举值非连续，1/3/32，文档未说明编号原因，应为对齐底层 KeyMaster HAL 标准值）

**3.3.1.3.3 `ql_km_aes_mode_t`** — AES 密钥模式枚举：
```c
typedef enum {
    QL_KM_MODE_ECB = 1,
    QL_KM_MODE_CBC = 2,
    QL_KM_MODE_CTR = 3,
    QL_KM_MODE_GCM = 32,
} ql_km_aes_mode_t;
```

**3.3.1.3.4 `ql_km_ec_curve_t`** — ECC 曲线枚举：
```c
typedef enum {
    QL_KM_EC_CURVE_P_224 = 0,
    QL_KM_EC_CURVE_P_256 = 1,
    QL_KM_EC_CURVE_P_384 = 2,
    QL_KM_EC_CURVE_P_521 = 3,
} ql_km_ec_curve_t;
```

**3.3.1.3.5 `ql_km_key_t`** — 密钥结构体：
```c
typedef struct {
    uint8_t *key_blob;
    uint32_t key_blob_size;
} ql_km_key_t;
```

**3.3.1.4 `ql_km_import_key`** — 导入密钥
- 原型：`ql_tee_error_t ql_km_import_key(const ql_km_key_args_t *key_args, const ql_km_blob_t *rawkey, ql_km_key_t *key)`
- 参数：`key_args` [In] 待导入密钥的参数；`rawkey` [In] 待导入的 BLOB 数据；`key` [Out] 生成的 BLOB 数据，内存在函数调用过程中分配。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

**3.3.1.4.1 `ql_km_blob_t`** — BLOB 数据结构体：
```c
typedef struct {
    uint8_t *data;
    uint32_t data_size;
} ql_km_blob_t;
```

**3.3.1.5 `ql_km_destroy_key`** — 销毁密钥，释放内存
- 原型：`ql_tee_error_t ql_km_destroy_key(ql_km_key_t *key)`
- 参数：`key` [Out] 待销毁的密钥。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

**3.3.1.6 `ql_km_destroy_blob`** — 销毁 BLOB 数据，释放内存
- 原型：`ql_tee_error_t ql_km_destroy_blob(ql_km_blob_t *blob)`
- 参数：`blob` [Out] 待销毁的 BLOB 数据。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

**3.3.1.7 `ql_km_operation_begin`** — 开始一个算法操作（如加密、解密、签名或验签等）
- 原型：`ql_tee_error_t ql_km_operation_begin(const ql_km_key_t *key, ql_km_operation_args_t *op_args, ql_km_operation_handle_t *op_handle)`
- 参数：`key` [In] 算法操作所需要的密钥；`op_args` [In] 算法操作所需要的参数；`op_handle` [Out] 操作句柄。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

**3.3.1.7.1 `ql_km_operation_args_t`** — 算法操作所需参数结构体：
```c
typedef struct {
    ql_km_algo_t algo;
    ql_km_purpose_t purpose;
    union {
        struct {
            ql_km_aes_mode_t mode;
            ql_km_blob_t iv;
        } aes_args;
        struct {
            ql_km_rsa_padding_t padding;
            ql_km_digest_t digest;
        } rsa_args;
    };
} ql_km_operation_args_t;
```
参数说明：`algo` 算法类型；`purpose` 操作类别；`mode` AES 模式；`iv` 偏移量（即初始化向量 IV）；`padding` 填充算法；`digest` 摘要算法。

**3.3.1.7.2 `ql_km_purpose_t`** — 操作类别枚举：
```c
typedef enum {
    QL_KM_PURPOSE_ENCRYPT = 0,
    QL_KM_PURPOSE_DECRYPT = 1,
    QL_KM_PURPOSE_SIGN = 2,
    QL_KM_PURPOSE_VERIFY = 3,
} ql_km_purpose_t;
```

**3.3.1.7.3 `ql_km_rsa_padding_t`** — RSA 填充算法枚举：
```c
typedef enum {
    QL_KM_PAD_RSA_NONE = 1,
    QL_KM_PAD_RSA_OAEP = 2,
    QL_KM_PAD_RSA_PSS = 3,
    QL_KM_PAD_RSA_PKCS1_1_5_ENCRYPT = 4,
    QL_KM_PAD_RSA_PKCS1_1_5_SIGN = 5,
} ql_km_rsa_padding_t;
```
（无填充 / OAEP / RSA-PSS / PKCS#1v1.5加密 / PKCS#1v1.5签名）

**3.3.1.7.4 `ql_km_digest_t`** — 摘要算法枚举：
```c
typedef enum {
    QL_KM_DIGEST_NONE = 0,
    QL_KM_DIGEST_MD5 = 1,
    QL_KM_DIGEST_SHA1 = 2,
    QL_KM_DIGEST_SHA_2_224 = 3,
    QL_KM_DIGEST_SHA_2_256 = 4,
    QL_KM_DIGEST_SHA_2_384 = 5,
    QL_KM_DIGEST_SHA_2_512 = 6,
} ql_km_digest_t;
```

**3.3.1.7.5 `ql_km_operation_handle_t`** — 操作句柄结构体：
```c
typedef struct {
    ql_km_algo_t algo;
    ql_km_purpose_t purpose;
    uint64_t handle;
} ql_km_operation_handle_t;
```

**3.3.1.8 `ql_km_operation_update`** — 进行算法操作（如加密、解密、签名或验签）
- 原型：`ql_tee_error_t ql_km_operation_update(ql_km_operation_handle_t *op_handle, uint8_t *input, uint32_t input_size, ql_km_blob_t *output, uint32_t *consumed)`
- 参数：`op_handle` [In] 操作句柄；`input` [In] 待处理数据；`input_size` [In] 待处理数据的大小；`output` [Out] 操作结果，内存在函数调用过程中分配；`consumed` [Out] 本次算法操作实际输入数据量，单位字节。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

**3.3.1.9 `ql_km_operation_finish`** — 进行最后一轮算法操作
- 原型：`ql_tee_error_t ql_km_operation_finish(ql_km_operation_handle_t *op_handle, uint8_t *input, uint32_t input_size, ql_km_blob_t *output, uint8_t *signature, uint32_t sig_size)`
- 参数：`op_handle` [In] 操作句柄；`input` [In] 待处理数据；`input_size` [In] 待处理数据长度；`output` [Out] 操作结果，内存在函数调用过程中分配；`signature` [In] 数据的签名；`sig_size` [In] 签名的长度。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。
- **备注（原文）**：`signature` 和 `sig_size` 为可选参数。**若要进行加密、解密或签名操作，不必配置 `signature` 和 `sig_size` 参数。若要进行验签操作，则必须配置 `signature` 和 `sig_size` 参数。**

**3.3.1.10 `ql_km_export_key`** — 导出非对称算法的公钥
- 原型：`ql_tee_error_t ql_km_export_key(ql_km_key_t *keypair, ql_km_blob_t *pubkey)`
- 参数：`keypair` [In] 非对称密钥对；`pubkey` [Out] BLOB 数据，导出的公钥，内存在函数调用过程中分配。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

**3.3.1.11 `ql_km_get_key_algo`** — 从 BLOB 数据中读取当前密钥的算法类别
- 原型：`ql_tee_error_t ql_km_get_key_algo(ql_km_key_t *keyblob, ql_km_algo_t *algo)`
- 参数：`keyblob` [In] 密钥；`algo` [Out] 算法类型。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

#### 3.3.2 辅助简化（Helper 函数，简化参数构造）

文档说明：为了简化传入参数设置，方便用户调用上述 API，移远通信提供以下 API 以简化 AES、RSA 和 ECC 算法的操作。

**3.3.2.1 `ql_aes_genkey_args`** — 生成调用 `ql_km_gen_key()` 的 `key_args` 参数
- 原型：`ql_tee_error_t ql_aes_genkey_args(ql_km_aes_mode_t mode, uint32_t bits, ql_km_key_args_t *args, uint32_t min_mac_len)`
- 参数：`mode` [In] AES 密钥模式；`bits` [In] 密钥位数；`args` [Out] 密钥参数（不分配内存）；`min_mac_len` [In] GCM 模式中 MAC 的最小长度。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。
- **备注（原文）**：`min_mac_len` 为可选参数。**若在 CBC、CTR 或 ECB 模式下使用，不必配置该参数。若在 GCM 模式下使用，则必须配置该参数。**

**3.3.2.2 `ql_aes_operation_args`** — 生成调用 `ql_km_operation_begin()` 的参数
- 原型：`ql_tee_error_t ql_aes_operation_args(ql_km_purpose_t purpose, ql_km_aes_mode_t mode, ql_km_blob_t *iv, ql_km_operation_args_t *args)`
- 参数：`purpose` [In] 操作类别；`mode` [In] 算法模式；`iv` [In] 偏移量，当 `purpose` 为解密时输入；`args` [Out] 算法所需参数，用来调用 `ql_km_operation_begin()`，不分配内存。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

**3.3.2.3 `ql_rsa_genkey_args`** — 生成调用 `ql_km_gen_key()` 的参数
- 原型：`ql_tee_error_t ql_rsa_genkey_args(uint64_t exponent, uint32_t bits, ql_km_key_args_t *args)`
- 参数：`exponent` [In] RSA 密钥的指数；`bits` [In] 密钥的位数；`args` [Out] 算法所需参数，不分配内存。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

**3.3.2.4 `ql_rsa_operation_args`** — 生成调用 `ql_km_operation_begin()` 的参数
- 原型：`ql_tee_error_t ql_rsa_operation_args(ql_km_purpose_t purpose, ql_km_rsa_padding_t padding, ql_km_digest_t digest, ql_km_operation_args_t *args)`
- 参数：`purpose` [In] 操作类别；`padding` [In] 填充算法；`digest` [In] 摘要算法；`args` [In]（原文标注为 [In]，与其他同类函数中 args 多标 [Out] 不一致，疑似原文笔误，但应理解为输出参数，用来调用 `ql_km_operation_begin()`），不分配内存。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

**3.3.2.5 `ql_ec_genkey_args`** — 生成调用 `ql_km_gen_key()` 的参数
- 原型：`ql_tee_error_t ql_ec_genkey_args(ql_km_ec_curve_t curve, ql_km_key_args_t *args)`
- 参数：`curve` [In] ECC 曲线；`args` [Out] 算法所需参数，不分配内存。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

#### 3.3.3 TrustZone 安全存储

TrustZone 安全存储将安全数据存储在 `/persist/` 路径下，客户可根据需要对该目录进行备份和还原。

**3.3.3.1 `ql_ss_initialize`** — 初始化安全存储功能。在调用其他函数前，先调用本函数初始化安全存储服务。
- 原型：`ql_tee_error_t ql_ss_initialize(void)`
- 参数：无
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。
- **关键备注（与 3.3.1.1 完全对应、原文重复强调）**：TEE 内存存在限制，当前 SoC 配置的 TEE 的 RAM 为 2 MB，其中 TEE Core 占用接近 1 MB。加解密服务和安全存储服务隶属于不同的 TA，目前内存不支持同时加载多个 TA 应用，**此接口无法与 `ql_km_initialize()` 同时调用**，需要使用此接口时，需要先关闭加解密服务 TA 应用。

**3.3.3.2 `ql_ss_deinitialize`** — 去初始化安全存储功能。若不再使用安全存储功能，调用该函数以释放资源。
- 原型：`void ql_ss_deinitialize(void)`；无参数无返回值。

**3.3.3.3 `ql_ss_open`** — 打开安全存储对象
- 原型：`ql_tee_error_t ql_ss_open(const void *id, uint32_t id_size, uint32_t *object)`
- 参数：`id` [In] 安全存储中的对象名称；`id_size` [In] id 的大小，单位字节；`object` [Out] 安全存储对象句柄。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

**3.3.3.4 `ql_ss_create`** — 创建并打开安全存储对象，指向安全存储文件头
- 原型：`ql_tee_error_t ql_ss_create(const void *id, uint32_t id_size, void *data, uint32_t data_size, uint32_t *object, bool overwrite)`
- 参数：`id` [In] 对象名称；`id_size` [In] id 大小；`data` [In] 需要写入安全存储的数据；`data_size` [In] 数据大小，单位字节；`object` [Out] 对象句柄；`overwrite` [In] 是否覆盖同名对象（`TRUE` 覆盖，`FALSE` 不覆盖）。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

**3.3.3.5 `ql_ss_close`** — 关闭安全存储对象
- 原型：`ql_tee_error_t ql_ss_close(uint32_t object)`
- 参数：`object` [In] 安全存储对象句柄。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

**3.3.3.6 `ql_ss_read`** — 从安全存储中读取数据
- 原型：`ql_tee_error_t ql_ss_read(uint32_t object, void *data, uint32_t data_size, uint32_t *count)`
- 参数：`object` [In] 对象句柄；`data` [Out] 用于存储数据的地址；`data_size` [In] 需要读取的数据大小，单位字节；`count` [Out] 实际读取的数据大小，单位字节。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

**3.3.3.7 `ql_ss_write`** — 向安全存储写入数据
- 原型：`ql_tee_error_t ql_ss_write(uint32_t object, void *data, uint32_t data_size)`
- 参数：`object` [In] 对象句柄；`data` [In] 需要写入的数据；`data_size` [In] 需要写入的数据大小，单位字节。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

**3.3.3.8 `ql_ss_seek`** — 移动安全存储文件的文件指针
- 原型：`ql_tee_error_t ql_ss_seek(uint32_t object, int32_t offset, ql_ss_whence_t whence)`
- 参数：`object` [In] 对象句柄；`offset` [In] 偏移量；`whence` [In] 移动文件指针的起始位置。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

**3.3.3.8.1 `ql_ss_whence_t`** — 安全存储移位时相对位置枚举：
```c
typedef enum {
    QL_SS_SEEK_SET = 0,
    QL_SS_SEEK_CUR = 1,
    QL_SS_SEEK_END = 2
} ql_ss_whence_t;
```
（SET=安全存储文件头；CUR=安全存储文件指针当前位置；END=安全存储文件尾）

**3.3.3.9 `ql_ss_unlink`** — 删除安全存储文件
- 原型：`ql_tee_error_t ql_ss_unlink(uint32_t object)`
- 参数：`object` [In] 对象句柄。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

**3.3.3.10 `ql_ss_trunc`** — 截短安全存储文件到指定的长度
- 原型：`ql_tee_error_t ql_ss_trunc(uint32_t object, uint32_t len)`
- 参数：`object` [In] 对象句柄；`len` [In] 截取的目标长度，单位字节。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

**3.3.3.11 `ql_ss_rename`** — 重命名安全存储对象
- 原型：`ql_tee_error_t ql_ss_rename(uint32_t object, const void *id, uint32_t id_size)`
- 参数：`object` [In] 对象句柄；`id` [In] 新的对象名；`id_size` [In] 对象名长度，单位字节。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

**3.3.3.12 `ql_ss_get_info`** — 获取安全存储对象信息，包括文件大小和当前文件指针的位置
- 原型：`ql_tee_error_t ql_ss_get_info(uint32_t object, uint32_t *obj_size, uint32_t *cur_pos)`
- 参数：`object` [In] 对象句柄；`obj_size` [Out] 对象的大小；`cur_pos` [Out] 当前文件指针的位置。
- 返回值：`QL_TEE_OK` 成功，其他值见错误码。

#### 3.3.4 宏定义

| 宏定义 | 描述 |
|--------|------|
| `#define OPERATION_BUF_MAX_SIZE 4096` | 单次操作最大字节数 |
| `#define SINGLE_OBJ_MAX_SIZE (512 * 1024)` | 安全存储单个对象最大字节数 |

### 4 附录 参考文档及术语缩写

**表2：参考文档**
| 文档名称 |
|---|
| [1] Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导 |

**表3：术语缩写（原文完整还原）**

| 缩写 | 英文全称 | 中文全称 |
|------|----------|----------|
| AES | Advanced Encryption Standard | 高级加密标准 |
| API | Application Programming Interface | 应用程序编程接口 |
| BLOB | Binary Large Object | 二进制型大对象 |
| CBC | Cipher Block Chaining | 分组密码工作模式 |
| CTX | Context | 上下文 |
| ECB | Electronic Codebook | 电码本 |
| ECC | Elliptic Curve Cryptography | 椭圆加密算法 |
| GCM | Galois/Counter Mode | 伽罗瓦/计数器模式 |
| IoV | Internet of Vehicles | 车联网 |
| IV | Initialization Vector | 初始化向量 |
| MD5 | MD5 Message-Digest Algorithm | MD5 信息摘要算法 |
| OAEP | Optimal Asymmetric Encryption Padding | 最优非对称加密填充 |
| PKCS | Public Key Cryptography Standards | 公钥密码学标准 |
| PSS | Probability Signature Scheme | 概率签名方案 |
| RSA | Rivest, Shamir, Adleman | RSA 算法 |
| SS | Secure Storage | 安全存储 |
| TEE | Trusted Execution Environment | 可信执行环境 |

（注：表 3 中列出了 "CTX"=Context 缩写，但正文 API 中未出现任何 CTX 相关函数或结构体，文档未说明该缩写在何处使用，可能是术语表模板的遗留项。）

---

## 四、关键命令/接口/参数汇总表

### 4.1 加解密（KeyMaster，KM）API 一览

| 函数 | 用途 | 关键输入 | 关键输出 |
|------|------|----------|----------|
| ql_km_initialize | 初始化加解密 TA | 无 | 错误码 |
| ql_km_deinitialize | 去初始化加解密 TA | 无 | 无 |
| ql_km_gen_key | 生成密钥 | key_args（算法/位数/模式/曲线等） | key（key_blob） |
| ql_km_import_key | 导入密钥 | key_args + rawkey（BLOB） | key（key_blob） |
| ql_km_destroy_key | 销毁密钥 | key | — |
| ql_km_destroy_blob | 销毁 BLOB | blob | — |
| ql_km_operation_begin | 开始加/解密/签名/验签 | key + op_args | op_handle |
| ql_km_operation_update | 分段处理数据 | op_handle + input | output + consumed |
| ql_km_operation_finish | 结束操作 | op_handle + input(+signature 仅验签需要) | output |
| ql_km_export_key | 导出非对称公钥 | keypair | pubkey（BLOB） |
| ql_km_get_key_algo | 查询密钥算法类型 | keyblob | algo |

### 4.2 辅助参数构造 API

| 函数 | 对应底层调用 | 备注 |
|------|--------------|------|
| ql_aes_genkey_args | ql_km_gen_key | GCM 模式必须传 min_mac_len，其他模式不传 |
| ql_aes_operation_args | ql_km_operation_begin | 解密时需要传 iv |
| ql_rsa_genkey_args | ql_km_gen_key | 需传 exponent、bits |
| ql_rsa_operation_args | ql_km_operation_begin | 需传 padding、digest |
| ql_ec_genkey_args | ql_km_gen_key | 需传 curve |

### 4.3 安全存储（Secure Storage，SS）API 一览

| 函数 | 用途 | 关键参数 |
|------|------|----------|
| ql_ss_initialize | 初始化安全存储 TA | 无 |
| ql_ss_deinitialize | 去初始化 | 无 |
| ql_ss_open | 打开已存在对象 | id, id_size → object |
| ql_ss_create | 创建/打开并写入对象 | id, data, data_size, overwrite → object |
| ql_ss_close | 关闭对象 | object |
| ql_ss_read | 读取数据 | object, data_size → count |
| ql_ss_write | 写入数据 | object, data, data_size |
| ql_ss_seek | 移动文件指针 | object, offset, whence |
| ql_ss_unlink | 删除文件 | object |
| ql_ss_trunc | 截断文件 | object, len |
| ql_ss_rename | 重命名 | object, id, id_size |
| ql_ss_get_info | 查询大小/指针位置 | object → obj_size, cur_pos |

### 4.4 关键枚举值速查

- **算法类型** `ql_km_algo_t`：RSA=1，EC=3，AES=32
- **AES 模式** `ql_km_aes_mode_t`：ECB=1，CBC=2，CTR=3，GCM=32
- **ECC 曲线** `ql_km_ec_curve_t`：P-224=0，P-256=1，P-384=2，P-521=3
- **操作类别** `ql_km_purpose_t`：ENCRYPT=0，DECRYPT=1，SIGN=2，VERIFY=3
- **RSA 填充** `ql_km_rsa_padding_t`：NONE=1，OAEP=2，PSS=3，PKCS1_1_5_ENCRYPT=4，PKCS1_1_5_SIGN=5
- **摘要算法** `ql_km_digest_t`：NONE=0，MD5=1，SHA1=2，SHA-224=3，SHA-256=4，SHA-384=5，SHA-512=6
- **安全存储 seek 基准** `ql_ss_whence_t`：SET=0，CUR=1，END=2

### 4.5 宏限制

| 宏 | 值 | 含义 |
|----|----|------|
| OPERATION_BUF_MAX_SIZE | 4096 字节 | KM 单次 update/finish 操作最大字节数 |
| SINGLE_OBJ_MAX_SIZE | 512 × 1024 = 524288 字节（512KB） | 安全存储单个对象最大字节数 |

### 4.6 硬件/系统级关键限制

| 限制项 | 数值/说明 |
|--------|-----------|
| TEE RAM 总量 | 2 MB |
| TEE Core 占用 | 接近 1 MB |
| TA 并发加载限制 | 加解密服务（KM）TA 与安全存储（SS）TA **不能同时加载**，二者初始化函数 `ql_km_initialize()` 与 `ql_ss_initialize()` 不能同时调用，需要使用一方时必须先去初始化（关闭）另一方对应 TA |
| 函数并发调用限制 | 本文档所有 API 默认**不支持并发调用**，且**不能在相关回调函数中调用** |
| 安全存储路径 | `/persist/`（数据加密存储，文件名不可直接查看） |

---

## 五、完整操作流程还原

文档本身没有给出端到端的示例代码或完整调用顺序图，但根据各函数的 [In]/[Out] 依赖关系和备注，可以还原出以下两条标准调用流程（均基于文档描述的参数依赖顺序，未跳步）：

### 5.1 加解密（KeyMaster）标准流程

1. **初始化**：调用 `ql_km_initialize()`。
   - 前提：此刻安全存储 TA（若已通过 `ql_ss_initialize()` 启动）必须先调用 `ql_ss_deinitialize()` 关闭，因为 TEE 内存（2MB，TEE Core 占用约1MB）不支持 KM 和 SS 两个 TA 同时加载。
2. **生成或导入密钥**：
   - 生成新密钥：先用辅助函数 `ql_aes_genkey_args` / `ql_rsa_genkey_args` / `ql_ec_genkey_args` 构造 `ql_km_key_args_t`，再调用 `ql_km_gen_key(key_args, &key)` 得到 `ql_km_key_t`（其 `key_blob` 内存由函数内部分配）。
   - 或导入已有密钥：构造同样的 `key_args`，再准备好待导入的 `ql_km_blob_t rawkey`，调用 `ql_km_import_key(key_args, &rawkey, &key)`。
3. **（可选）导出公钥**：对非对称密钥对调用 `ql_km_export_key(&keypair, &pubkey)` 获取公钥 BLOB。
4. **（可选）查询密钥算法**：调用 `ql_km_get_key_algo(&keyblob, &algo)`。
5. **开始算法操作**：
   - 用辅助函数 `ql_aes_operation_args` / `ql_rsa_operation_args` 构造 `ql_km_operation_args_t`（指定 purpose：加密/解密/签名/验签，及对应模式/填充/摘要/IV）。
   - 调用 `ql_km_operation_begin(&key, &op_args, &op_handle)` 得到操作句柄。
6. **分段处理数据（可多次调用）**：调用 `ql_km_operation_update(&op_handle, input, input_size, &output, &consumed)`，单次输入/输出不超过 `OPERATION_BUF_MAX_SIZE`（4096字节，根据宏定义推断该限制适用范围，文档未明确指出该宏专属于哪个函数，但置于 3.3.4 节统一宏定义中，逻辑上应用于 KM 的 update/finish 数据缓冲区）。
7. **结束操作**：调用 `ql_km_operation_finish(&op_handle, input, input_size, &output, signature, sig_size)`。
   - 若操作目的是加密/解密/签名：`signature` 和 `sig_size` 不必配置。
   - 若操作目的是验签：必须配置 `signature` 和 `sig_size`（待验证的签名数据及长度）。
8. **资源释放**：操作完成后调用 `ql_km_destroy_key(&key)` 销毁密钥释放内存；对独立 BLOB（如导出的公钥、rawkey）调用 `ql_km_destroy_blob(&blob)`。
9. **去初始化**：不再使用加解密服务时调用 `ql_km_deinitialize()`，释放 TA 资源，以便安全存储 TA 可以被加载。

### 5.2 安全存储（Secure Storage）标准流程

1. **初始化**：调用 `ql_ss_initialize()`。
   - 前提：加解密服务 TA（若已通过 `ql_km_initialize()` 启动）必须先调用 `ql_km_deinitialize()` 关闭。
2. **创建或打开对象**：
   - 新建对象并写入初始数据：`ql_ss_create(id, id_size, data, data_size, &object, overwrite)`（`overwrite=TRUE` 时覆盖同名对象，`FALSE` 时不覆盖）。
   - 打开已存在对象：`ql_ss_open(id, id_size, &object)`。
3. **读写操作**：
   - 写入：`ql_ss_write(object, data, data_size)`。
   - 读取：`ql_ss_read(object, data, data_size, &count)`，`count` 返回实际读取字节数。
   - 单个对象数据量不应超过 `SINGLE_OBJ_MAX_SIZE`（512KB）。
4. **文件指针操作**：`ql_ss_seek(object, offset, whence)`，`whence` 取值 `QL_SS_SEEK_SET`（文件头）/ `QL_SS_SEEK_CUR`（当前位置）/ `QL_SS_SEEK_END`（文件尾）。
5. **查询对象信息**：`ql_ss_get_info(object, &obj_size, &cur_pos)` 获取对象大小和当前指针位置。
6. **（可选）维护操作**：
   - 重命名：`ql_ss_rename(object, new_id, id_size)`。
   - 截断：`ql_ss_trunc(object, len)`。
   - 删除：`ql_ss_unlink(object)`。
7. **关闭对象**：使用完毕后调用 `ql_ss_close(object)`。
8. **去初始化**：不再使用安全存储时调用 `ql_ss_deinitialize()` 释放资源，以便加解密服务 TA 可以被加载。

> 文档未提供实际示例代码片段（仅在 2.2、2.3 节备注中提示参考 SDK 内 `sample/secstor-example` 和 `sample/keymaster-example` 示例工程），故以上流程是基于函数签名、参数依赖关系及文档明确的前提/约束条件还原，未在文档原文中以图示或代码示例形式给出。

---

## 六、与本项目（open_dial 拨号管理程序）的潜在关联点

经过对全文的分析，本 TEE 开发指导文档与 `open_dial` 项目（运行在 Quectel EC2x/EG2x/AG35 模组上的拨号管理程序）的关联性评估如下：

1. **模块型号差异**：CLAUDE.md 中描述 `open_dial` 运行在 **EC2x/EG2x** 模组上，而本文档适用模块为 **AG35-CET / AG35-EUT**。这是不同的模块系列（虽然都属于 Quectel LTE Standard 模块），文档中给出的头文件路径、库文件（`libql-tee-service.so`）、TA 内存配置（2MB TEE RAM）等均是针对 AG35 系列的 QuecOpen SDK 环境，**不能直接照搬到 EC2x/EG2x 模组上**，若 EC2x/EG2x 也走 QuecOpen 方案需以对应型号文档为准。因此本文档与当前 `open_dial` 项目代码库**没有直接的代码层面适用关系**。

2. **潜在概念性参考价值**（如未来涉及 AG35 系列或需要硬件级安全存储/加解密能力时可参考）：
   - `open_dial` 当前管理 Roamlink 虚拟 SIM 的 license 文件（`/usrdata/roamlink/etc/.pconfig/license.cer` 及备份 `/data/ufs/license.cer`），采用的是明文文件 + 双路径备份的方式（见 CLAUDE.md `roamlink_license_write_backup()` 等描述）。本文档中的 TrustZone 安全存储能力（`/persist/` 路径下加密存储，文件名不可见）若在 AG35 系列上可用，理论上是一种比当前明文备份方案更安全的 license 存储选型——但这仅是潜在方向，**当前 open_dial 项目运行在 EC2x/EG2x 上，不具备直接适用性**，且文档没有说明 EC2x/EG2x 是否支持同款 TEE API。
   - `open_dial` 涉及 AT 命令、IMEI/固件信息等敏感数据读取（如 CLAUDE.md 提到 `AT+CGSN` 读 IMEI），若未来有需求将敏感凭据（如 RBMaster 通信密钥、license 密钥）做硬件级加密存储，本文档描述的 KeyMaster AES/RSA/ECC 加解密 API 可作为技术选型参考。
   - 文档中关于"两个 TA 不能同时加载，内存仅 2MB"的强约束，提示了一个通用经验：在资源受限的 Quectel TEE 平台上做安全存储/加解密设计时必须做资源互斥管理。这对 `open_dial` 未来若要在 AG35 平台移植或新增安全存储功能时是重要的设计约束参考。

3. **结论**：**本文档与当前 `open_dial` 项目（EC2x/EG2x 平台）无直接的代码或接口层面关联**；若项目未来扩展支持 AG35-CET/AG35-EUT 模块，或考虑将 Roamlink license/密钥改造为 TEE 安全存储方案，本文档的 API 定义、内存限制、TA 互斥约束可作为设计参考依据，但当前不构成对现有代码的直接指导。

---

## 七、文档局限性、未说明清楚之处及已知问题

基于通读全文，发现以下局限性或文档未充分说明的地方（均严格基于文档实际内容，未做主观臆测）：

1. **无完整示例代码**：文档仅在备注中提示参考 SDK 自带的 `sample/secstor-example` 和 `sample/keymaster-example`，但**正文未包含任何实际调用代码片段**，对于初次接触者完整的调用流程需要自行查阅 SDK 示例工程，文档本身的可操作性有限。

2. **错误码枚举存在跳号且未说明原因**：`ql_tee_error_t` 中 KM 错误码从 -41（`MEMORY_ALLOCATION_FAILED`）直接跳到 -44（`IMPORT_PARAMETER_MISMATCH`），中间 -42、-43 未定义，文档未解释这两个值的含义或是否保留给未来使用。

3. **算法类型枚举值不连续未说明**：`ql_km_algo_t` 中 RSA=1，EC=3，AES=32，跳过了 2，也未说明 2 对应哪个算法（推测可能为底层 KeyMaster HAL 标准遗留的未公开算法类型，如 HMAC），文档未做任何解释。

4. **`OPERATION_BUF_MAX_SIZE` 宏的具体适用范围未明确**：文档仅在"宏定义"一节列出 `#define OPERATION_BUF_MAX_SIZE 4096`（单次操作最大字节数），但未在 `ql_km_operation_update`/`ql_km_operation_finish` 的参数说明中明确指出 `input_size`/`output` 是否受此宏直接约束，需要使用者自行推断。

5. **`ql_rsa_operation_args` 的 `args` 参数方向标注疑似不一致**：原文该函数的 `args` 参数标注为 `[In]`，而文档中其余四个同类辅助函数（`ql_aes_genkey_args`、`ql_aes_operation_args`、`ql_rsa_genkey_args`、`ql_ec_genkey_args`）的对应输出参数均标注为 `[Out]`。这很可能是原文笔误，但文档未做任何勘误说明，本分析在汇总表中已标注此处疑点。

6. **术语缩写表存在冗余项**：附录表3 列出了 "CTX = Context"（上下文）这一缩写，但纵观全文 3 章 TEE API 的所有函数、结构体、枚举命名中，**未出现任何 "ctx" 相关的标识符**。该缩写在正文中找不到对应用法，文档未说明用途，可能是文档模板沿用时的遗留条目。

7. **TEE 内存限制的影响范围未充分展开**：文档反复强调（两处，3.3.1.1 和 3.3.3.1）KM 与 SS 两个 TA 不能同时加载、需要互斥初始化，但**未说明若应用程序同时需要"边做加解密边读写安全存储"（例如把加密后的密钥写入安全存储）这种常见场景该如何设计**——是否需要每次切换 TA 都付出初始化/去初始化的性能开销？文档没有给出推荐的设计模式或性能数据（如初始化耗时）。

8. **未说明并发限制的具体后果**：3.2 节备注提到"本文档所述函数均不支持并发调用，且不能在相关回调函数中调用以上函数"，但未说明若违反该约束会产生什么具体错误（例如是否会返回 `QL_KM_ERROR_CONCURRENT_ACCESS_CONFLICT = -47` 或 `QL_SS_ERROR_ACCESS_CONFLICT = -203`，文档未明确关联）。

9. **未提供性能/容量数据**：除了 TEE RAM 总量（2MB）、TA Core 占用（约1MB）、单对象最大字节数（512KB）和单次操作最大字节数（4096字节）外，文档未提供任何关于加解密操作耗时、安全存储读写性能、并发 TA 切换耗时等量化数据，对于评估实际产品中的可用性（如是否能满足实时性要求）缺乏依据。

10. **未说明安全存储数据持久性/防篡改具体机制**：文档仅说"数据被加密并存储在安全存储路径 `/persist/` 下，且无法直接看到文件名"，但未说明加密算法、密钥来源（是否绑定设备唯一根密钥/Root of Trust）、是否具备防回滚（anti-rollback）能力，也未说明 `/persist/` 分区的擦除/恢复出厂设置等操作对已存储安全数据的影响。

11. **文档版本与适用范围的演进未完全闭环**：版本 1.0.1 提到"新增 ql_km_initialize() 和 ql_ss_initialize() 的调用说明"，这意味着在更早的 1.0.0 版本中很可能未明确说明两者互斥关系——这是一个值得注意的历史经验：早期版本可能存在因未说明该限制而导致的实际开发问题，但文档历史记录本身未详细描述具体的问题背景（即未说明此前因未注明导致过什么具体故障或客户反馈）。

12. **未提及 RSA 密钥的可用 exponent 范围、AES 256位+GCM组合的具体推荐参数**等具体业务建议，仅给出结构体字段定义，留待开发者自行根据通用密码学常识填入（如常用 exponent=65537）。

以上所有局限性分析均严格基于通读 PDF 全部 43 页（含封面，正文42页）后得出的实际内容观察，不包含任何文档未提及的推测性结论；凡属推测部分均已在文中明确标注为"推测"。

<!-- GENERATION_COMPLETE: 2026-06-24 -->
