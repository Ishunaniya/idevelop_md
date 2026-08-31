# EC200A-CN(TA) QuecOpen ATC API 参考手册

> **来源文档**：Quectel_EC200A-CN(TA)_QuecOpen_ATC_API_参考手册_V1.0.0_Preliminary_20230220.pdf
> **适用模块**：LTE Standard 模块系列 — EC200A-CN(TA)
> **版本**：1.0.0　**日期**：2023-02-20　**状态**：临时文件（Preliminary）
>
> 本 md 为该 PDF 全文逐项整理，不省略技术内容。法律声明/免责声明/版权页仅摘其要点。

---

## 文档历史（修订记录）

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| -（创建） | 2021-12-08 | Stan LI | 文档创建 |
| 1.0.0 | 2023-02-20 | Sunshine HUANG | 临时版本 |

---

## 1 引言

移远通信 EC200A-CN(TA) 模块支持 **QuecOpen®** 方案；QuecOpen® 是开源的、基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。有关 QuecOpen® 的详细信息，请参考文档 [1]（《Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导》）。

**ATC** 全称 **AT Command**（AT 命令），用于和模块进行交互，可读取和设置模块的各项参数。本文档主要介绍在 QuecOpen® 方案下，EC200A-CN(TA) 模块 SDK 中提供的 **ATC API**。

---

## 2 ATC API

### 2.1 头文件

ATC API 的头文件为 **`ql_atc.h`**，位于 **`ql-sysroots/usr/include/ql-sdk`** 目录下。若无特别说明，本文档所涉及头文件均在该目录下。

### 2.2 函数概览

**表 1：函数概览**

| 函数 | 描述 |
|---|---|
| `ql_atc_init()` | 初始化 ATC 服务 |
| `ql_atc_send()` | 采用同步调用的方式发送 AT 命令 |
| `ql_atc_send_async()` | 采用异步调用的方式发送 AT 命令 |
| `ql_atc_deinit()` | 去初始化 ATC 服务 |

### 2.3 函数详解

#### 2.3.1 `ql_atc_init`

该函数用于初始化 ATC 服务。

**函数原型**
```c
int ql_atc_init(void)
```

**参数**：无

**返回值**
| 返回值 | 含义 |
|---|---|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

> **备注**：使用其他 ATC API 前，必须调用该函数初始化 ATC 服务。

---

#### 2.3.2 `ql_atc_send`

该函数采用**同步**调用的方式发送 AT 命令。

**函数原型**
```c
int ql_atc_send(char *req_buf, char *rsp_buf, int rsp_len)
```

**参数**
| 参数 | 方向 | 说明 |
|---|---|---|
| `req_buf` | [In] | 待发送的 AT 命令 |
| `rsp_buf` | [Out] | AT 命令的执行结果 |
| `rsp_len` | [In] | `rsp_buf` 的长度。单位：字节 |

**返回值**
| 返回值 | 含义 |
|---|---|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 2.3.3 `ql_atc_send_async`

该函数采用**异步**调用的方式发送 AT 命令。

**函数原型**
```c
int ql_atc_send_async(char *req_buf, int *async_index, void *async_cb)
```

**参数**
| 参数 | 方向 | 说明 |
|---|---|---|
| `req_buf` | [In] | AT 命令 |
| `async_index` | [Out] | 当前异步 AT 命令的索引值。可与第 2.3.3.1 章中的 `async_index` 进行比较，确定本次回调是哪次异步操作的结果 |
| `async_cb` | [In] | 异步回调函数，其类型为 `ql_atc_async_cb`，详情请参考第 2.3.3.1 章 |

**返回值**
| 返回值 | 含义 |
|---|---|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

##### 2.3.3.1 `ql_atc_async_cb`

该函数为采用异步调用的方式发送 AT 命令的**回调函数**。

**函数原型**
```c
typedef void (*ql_atc_async_cb)(int async_index, char *req_buf, char *rsp_buf)
```

**参数**
| 参数 | 方向 | 说明 |
|---|---|---|
| `async_index` | [In] | 异步发送 AT 命令的索引。该值可用于标识各个异步 AT 命令，防止多个异步 AT 命令同时执行时回调函数无法识别当前获取的数据是哪次异步发送 AT 命令返回的结果 |
| `req_buf` | [In] | 异步发送的 AT 命令 |
| `rsp_buf` | [In] | 异步发送 AT 命令的执行结果 |

**返回值**：无

---

#### 2.3.4 `ql_atc_deinit`

该函数用于去初始化 ATC 服务。若再次使用 ATC 相关功能，需要重新进行初始化操作。

**函数原型**
```c
int ql_atc_deinit(void)
```

**参数**：无

**返回值**
| 返回值 | 含义 |
|---|---|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

## 3 示例

本章所述代码示例均摘自 **`sample\test_sdk_api\m_atc.c`**，用户可自行查看接口函数的完整示例。程序启动后，必须调用 `ql_atc_init()` 初始化 ATC 服务。

```c
void atc_send_sample(void)
{
    int ret;
    char rsq_buf[] = "ati";
    char rsp_buf[128] = {0};

    ret = ql_atc_init();
    if(ret != QL_ERR_OK)
    {
        printf("init atc failed: %d\n", ret);
        return;
    }

    ret = ql_atc_send(rsq_buf, rsp_buf, sizeof(rsp_buf));
    if(ret == QL_ERR_OK)
    {
        printf("%s command get result: %s\n", rsq_buf, rsp_buf);
    }
    else
    {
        printf("%s command execute failed, ret=%d\n", rsq_buf, ret);
    }
}
```

---

## 4 附录 参考文档及术语缩写

**表 2：参考文档**

| 编号 | 文档名称 |
|---|---|
| [1] | Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导 |

**表 3：术语缩写**

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| ATC | AT Command | AT 命令 |
| API | Application Programming Interface | 应用程序编程接口 |
| IoT | Internet of Things | 物联网 |
| SDK | Software Development Kit | 软件开发工具包 |

---

*版权所有 © 上海移远通信技术股份有限公司 2023，保留一切权利。*
