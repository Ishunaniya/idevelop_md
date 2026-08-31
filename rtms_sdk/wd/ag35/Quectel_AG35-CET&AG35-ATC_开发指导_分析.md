# Quectel AG35-CET&AG35-EUT QuecOpen(SDK) ATC 开发指导 — 全量分析

> 本文是对原始 PDF《Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_ATC_开发指导_V1.0.pdf》的逐章全量精读还原。
> **ATC = AT Command。注意：本文档讲的不是"某条具体 AT 命令怎么用"，而是"在 QuecOpen 应用进程内部，用 SDK 的 C API（`ql_atc_*`）把 AT 命令发给模块"** —— 即一条**进程内直发 AT** 的通道。
> 与 open_dial 的 `at/` 访问层（当前走外部 `serial_atcmd` 命令）是**同一职责的两种实现路径**，本文档对本项目有直接的架构参考价值，故重点细读。

---

## 0. 文档元信息

| 项 | 内容 |
|---|---|
| 文档名 | AG35-CET&AG35-EUT QuecOpen(SDK) ATC 开发指导 |
| 适用模块系列 | LTE Standard：**AG35-CET、AG35-EUT** |
| 版本 | 1.0(**受控文件**，非临时版) |
| 日期 | 2024-09-24 |
| 总页数 | 13 页(正文 6~13) |
| 作者 | Keroro TAN(创建 Sunshine HUANG, 2023-10-24) |
| 厂商 | 上海移远通信技术股份有限公司(Quectel) |

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| -(创建) | 2023-10-24 | Sunshine HUANG | 文档创建 |
| 1.0 | 2024-09-24 | Keroro TAN | 1. 新增适用模块 AG35-EUT；2. **新增常见错误码和恢复措施(第 2.4 章)** |

---

## 1. 整书目录树

```
1  引言(ATC = AT Command，进程内发 AT)
2  ATC API
   2.1  头文件 ql_atc.h
   2.2  函数概览(表 1，4 个函数)
   2.3  函数详解
        2.3.1  ql_atc_init          初始化 ATC 服务
        2.3.2  ql_atc_send          同步发送 AT
        2.3.3  ql_atc_send_async    异步发送 AT
               2.3.3.1  ql_atc_async_cb(回调类型)
        2.3.4  ql_atc_deinit        去初始化 ATC 服务
   2.4  错误码及恢复措施(表 2，V1.0 新增)
3  示例(sample/test_sdk_api/m_atc.c)
4  附录 参考文档及术语缩写(表 3、表 4)
```

> **全文无任何流程图/时序图/截图**，是纯 API 参考文档，文本可 **100% 抽取还原**——本分析对该文档是完整覆盖，无遗漏、无图片缺口。

---

## 2. 逐章全量内容

### 第 1 章 引言

- AG35-CET / AG35-EUT 支持 **QuecOpen® 方案**(基于 Linux 的嵌入式开发平台，简化 IoV 软件开发，详见参考文档 [1]《快速开发指导》)。
- **ATC(AT Command)** 用于与模块交互，可**读取和设置模块的各项参数**。
- 本文档适用于 **SDK 构建环境**的 QuecOpen 方案，介绍 SDK 提供的 **ATC API** 及示例。

### 第 2 章 ATC API

#### 2.1 头文件

- ATC API 头文件：**`ql_atc.h`**
- 路径：**`ql-sysroots/usr/include/ql-sdk/`**(本文档涉及的所有头文件若无特别说明均在此目录)。

#### 2.2 函数概览(表 1)

| 函数 | 描述 |
|---|---|
| `ql_atc_init()` | 初始化 ATC 服务 |
| `ql_atc_send()` | 采用**同步**调用方式发送 AT 命令 |
| `ql_atc_send_async()` | 采用**异步**调用方式发送 AT 命令 |
| `ql_atc_deinit()` | 去初始化 ATC 服务 |

> 仅 4 个函数 + 1 个回调类型，是个极薄的封装层。整套生命周期：**init → (send | send_async) → deinit**。

#### 2.3 函数详解

##### 2.3.1 `ql_atc_init` — 初始化 ATC 服务

```c
int ql_atc_init(void)
```
- **参数**：无。
- **返回值**：

  | 返回值 | 含义 |
  |---|---|
  | `QL_ERR_OK` | 执行成功 |
  | `QL_ERR_SERVICE_NOT_READY` | 失败，服务未就绪 |
  | 其他值 | 失败，错误码详见 `ql_type.h` |

- **备注(强制)**：使用其他任何 ATC API 前，**必须先调用该函数**初始化 ATC 服务。

##### 2.3.2 `ql_atc_send` — 同步发送 AT 命令

```c
int ql_atc_send(char *req_buf, char *rsp_buf, int rsp_len)
```
- **参数**：
  - `req_buf`：[In] 待发送的 AT 命令。
  - `rsp_buf`：[Out] AT 命令的执行结果(**由调用方分配的输出缓冲区**)。
  - `rsp_len`：[In] `rsp_buf` 的长度，单位字节。
- **返回值**：

  | 返回值 | 含义 |
  |---|---|
  | `QL_ERR_OK` | 成功 |
  | `QL_ERR_NOT_INIT` | 失败，未初始化服务 |
  | `QL_ERR_SERVICE_NOT_READY` | 失败，服务未就绪 |
  | 其他值 | 失败，详见 `ql_type.h` |

> 同步语义：调用阻塞直到模块返回响应或失败。`rsp_buf`/`rsp_len` 由调用方负责，**缓冲不足会截断响应**。

##### 2.3.3 `ql_atc_send_async` — 异步发送 AT 命令

```c
int ql_atc_send_async(char *req_buf, int *async_index, void *async_cb)
```
- **参数**：
  - `req_buf`：[In] 待发送的 AT 命令。
  - `async_index`：[Out] 当前异步 AT 命令的索引值。**可与回调(2.3.3.1)里的 `async_index` 比较，确定本次回调对应哪一次异步发送。**
  - `async_cb`：[In] 异步回调函数，类型 `ql_atc_async_cb`(见 2.3.3.1)。
- **返回值**：同 `ql_atc_send`(OK / NOT_INIT / SERVICE_NOT_READY / 其他)。

###### 2.3.3.1 `ql_atc_async_cb` — 异步回调函数类型

```c
typedef void (*ql_atc_async_cb)(int async_index, char *req_buf, char *rsp_buf)
```
- **参数**：
  - `async_index`：[In] 异步发送 AT 命令的索引。**用于标识各异步命令，防止多个异步命令并发执行时回调无法识别数据来自哪一次发送(张冠李戴)。**
  - `req_buf`：[In] 异步发送的 AT 命令。
  - `rsp_buf`：[In] 异步发送 AT 命令的执行结果。
- **返回值**：无。

##### 2.3.4 `ql_atc_deinit` — 去初始化 ATC 服务

```c
int ql_atc_deinit(void)
```
- **参数**：无。
- **返回值**：`QL_ERR_OK` 成功 / 其他值失败(详见 `ql_type.h`)。
- **说明**：去初始化后**再次使用 ATC 功能需重新 `ql_atc_init()`**。

#### 2.4 错误码及恢复措施(表 2，V1.0 新增 —— 本文档最有工程价值的一节)

| 值 | 错误码 | 含义 | 恢复措施 |
|---|---|---|---|
| -1010 | `QL_ERR_INVALID_ARG` | 参数无效 | 确保 API 参数有效后重试 |
| -1067 | `QL_ERR_SERVICE_NOT_READY` | 服务未就绪 | **重复调用 `ql_atc_init()`**；若尝试一段时间(**建议 30 秒**)后仍失败，抓取调用全过程 log 提交移远 |
| -1099 | `QL_ERR_SERVICE_ABORT` | 服务端异常 | **`ql_atc_deinit()` → `ql_atc_init()` 重新初始化**后重试 |
| -1034 | `QL_ERR_NOT_INIT` | 未初始化服务 | 调用 `ql_atc_init()` 重新初始化后重试 |
| -1002 | `QL_ERR_INTERNAL` | 内部错误 | 重新 `ql_atc_init()` 后重试；仍报错则抓 log 提交移远 |
| -1003 | `QL_ERR_UNKNOWN` | 未知错误 | 同上：重新 init 后重试；仍报错抓 log 提交 |
| -1004 | `QL_ERR_GENERIC` | 通用错误 | 同上：重新 init 后重试；仍报错抓 log 提交 |
| -1030 | `QL_ERR_NO_MEM` | 内存不足 | 查模块内存占用是否过高，过高则释放后重试；仍报错则**排查内存泄漏**，确保内存充足后重试 |

> 三类恢复套路：① 参数类(-1010)→改参数；② 未就绪/未初始化(-1067/-1034)→重 init(-1067 最多轮询 ~30s)；③ 服务端异常(-1099)→deinit + init；④ 内部/未知/通用(-1002/-1003/-1004)→重 init，再不行上报；⑤ 内存(-1030)→释放/查泄漏。

### 第 3 章 示例(摘自 `sample/test_sdk_api/m_atc.c`)

> **程序启动后必须先 `ql_atc_init()`**。

```c
void atc_send_sample(void)
{
    int ret;
    char rsq_buf[] = "ati";          // 注：原文变量名拼作 rsq_buf；发送的命令是 ati(查模块信息)
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

> 注意：示例只演示了 `init` + 同步 `send`，**没有演示 `deinit`**(实际长驻进程应在退出/不再用 ATC 时 deinit 释放)。

### 第 4 章 附录

**表 3 参考文档**：[1] Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导。

**表 4 术语缩写**：ATC=AT Command、API=应用程序编程接口、IoV=车联网、SDK=软件开发工具包。

---

## 3. 关键警告与坑(手册"备注"汇总)

1. **必须先 init**：调任何其它 ATC API 前必须 `ql_atc_init()`，否则 `QL_ERR_NOT_INIT`(-1034)。
2. **同步 API 要给足缓冲**：`ql_atc_send` 的 `rsp_buf`/`rsp_len` 由调用方分配，缓冲不足会截断响应——发可能返回长文本的命令(如 `AT+COPS=?`)尤其注意预留足够大小。
3. **异步并发靠 `async_index` 配对**：多条异步命令并发时，回调里必须用 `async_index` 区分结果归属，否则张冠李戴。
4. **服务未就绪要轮询重试**：`QL_ERR_SERVICE_NOT_READY`(-1067) 建议轮询 `ql_atc_init()` 最多 ~30s，仍失败才上报。
5. **服务端异常须 deinit+init**：`QL_ERR_SERVICE_ABORT`(-1099) 不能只重试发送，要先 `deinit` 再 `init` 重建服务。
6. **deinit 后须重新 init** 才能再用。
7. **内存不足(-1030) 要查泄漏**：长驻进程反复发 AT 时尤其关注，提示与"守护进程内存稳定性"强相关。

---

## 4. 对 open_dial 项目的适用性批注(重点 —— 本份与本项目架构直接相关)

> open_dial 跑 **EC200A(OpenCPU/ASR1803)**，本文档面向 **AG35(LTE Standard)**。两者同源 ql-sdk、AT 访问层命名风格一致，但**型号不同，`ql_atc.h` 是否存在、API 名/可用性须以 EC200A 的 SDK 头文件为准核对**(用 `find ql-sysroots -name ql_atc.h`、看 EC200A SDK 是否提供该库)。

| 文档内容 | 与 open_dial 的关系 / 可借鉴点 |
|---|---|
| **`ql_atc_init/send/deinit` 进程内直发 AT** | **核心交集**：这正是 open_dial `at/` 层(`serial_atcmd` 发送+解析)的**另一条实现路径**。当前项目是 fork 外部 `serial_atcmd` 命令行工具发 AT；本文档是**进程内直接调 SDK C API**。后者优势：①不 fork 子进程(省内存/省 fork 开销，对长驻守护友好)、②不经 shell 解析(少一层出错点)、③同步 API 直接拿到 `rsp_buf` 便于解析。可作为 `at/` 层重构的备选方案评估。 |
| **`ql_atc_send` 同步阻塞** | open_dial 主状态机(`dial_loop`)是单线程顺序编排，发 AT 用同步 API 语义最直观、最贴合现有结构。但**注意阻塞时长**：若某条 AT 卡住会拖住主循环与 ping 检测。当前 `serial_atcmd` 走外部进程同样有超时问题，迁移时需保留超时/看门狗保护。 |
| **`ql_atc_send_async` + 回调(SDK 线程触发)** | ⚠️ **直接呼应 CLAUDE.md v1.28.2 的 use-after-free 教训**：异步回调在 **libql_sdk.so 的 epoll 线程**触发，与主循环并发。若 open_dial 改用异步 ATC，回调里写共享状态/日志**必须复用现有 `g_log_mtx` 递归锁模式**，否则重蹈"回调线程 fprintf 与主循环 fclose 并发→崩溃"的覆辙。**结论：能用同步就别用异步**，除非确有非阻塞需求且做好锁。 |
| **错误码恢复套路(2.4 节)** | `-1067 SERVICE_NOT_READY`(轮询30s重init) / `-1034 NOT_INIT`(重init) / `-1099 SERVICE_ABORT`(deinit+init) 这套恢复逻辑，**与 GNSS 文档的 -1099、EAVB 的 SERVICE_NOT_READY 完全一致**——是整个 ql-sdk 服务层的通用健壮性范式。若 open_dial 迁到 SDK API，可把这套"服务未就绪→重init / 服务异常→deinit+init"封进 `at/` 层统一重试，比目前判 `serial_atcmd` 返回码更结构化。 |
| **`QL_ERR_NO_MEM`(-1030) 查泄漏建议** | open_dial 是 7×24 长驻守护进程，反复发 AT；这条提示与项目的内存稳定性诉求高度契合，可作为长期监控项。 |
| **必须 init / deinit 配对** | 与 open_dial 现有的资源生命周期管理(log 句柄、SDK 域 init)一致；若引入 ATC API，初始化阶段 init、退出阶段(L3 exit 前)deinit，纳入现有 init/cleanup 流程。 |

**结论**：此文档对 open_dial **相关度高、属"架构可选优化"**。当前 `serial_atcmd` 路径已稳定工作，**不必为改而改**；本文档的真正价值是：① 提供 "SDK 进程内发 AT" 的备选实现(更省 fork、解析更直接)，② 给出统一的服务层错误码恢复策略。**若决定迁移，两条铁律**：(a) 先在 EC200A 确认 `ql_atc.h`/`ql-lib-atc` 存在；(b) 优先用同步 `ql_atc_send`，若用异步则回调写操作必须走现有递归锁，避免重演 v1.28.2 并发崩溃。
