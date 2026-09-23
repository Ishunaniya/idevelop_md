# LTE Standard(A) 系列 DFOTA 升级指导 V1.5 — 完整分析

> **原始文档**：仓库根目录的 `Quectel_LTE_Standard(A)系列_DFOTA_升级指导_V1.5.pdf`  
> **文档版本 / 日期**：V1.5 / 2026-02-25  
> **文档状态**：受控文件  
> **覆盖范围**：DFOTA 与 MiniFOTA 的适用范围、升级流程、`AT+QFOTADL` 用法、URC、故障恢复和结果码  
> **整理日期**：2026-09-23

本文按原 PDF 的章节完整梳理信息，并把原图中的流程整理为文字流程图和表格。命令、模块支持情况和结果码按 V1.5 原文归纳；实际产品是否支持某种升级路径，还需结合具体模块型号、Flash 容量和固件包确认。

## 目录

1. [文档信息与版本历史](#1-文档信息与版本历史)
2. [适用范围与升级方式选择](#2-适用范围与升级方式选择)
3. [升级前准备与整体流程](#3-升级前准备与整体流程)
4. [DFOTA 与 MiniFOTA 流程详解](#4-dfota-与-minifota-流程详解)
5. [AT 命令语法与通用参数](#5-at-命令语法与通用参数)
6. [`AT+QFOTADL` 四种升级入口](#6-atqfotadl-四种升级入口)
7. [DFOTA 与 MiniFOTA 差异](#7-dfota-与-minifota-差异)
8. [MiniFOTA 异常处理与注意事项](#8-minifota-异常处理与注意事项)
9. [全部结果码](#9-全部结果码)
10. [升级操作检查表](#10-升级操作检查表)
11. [参考资料与术语](#11-参考资料与术语)

## 1. 文档信息与版本历史

### 1.1 文档基本信息

| 项目 | 内容 |
|---|---|
| 文档名称 | LTE Standard(A) 系列 DFOTA 升级指导 |
| 文档版本 | V1.5 |
| 发布日期 | 2026-02-25 |
| 文档状态 | 受控文件 |
| 适用平台 | LTE Standard(A) 系列模块 |
| PDF 页数 | 文件共 35 页，正文页码为 1–34 |
| 主要内容 | DFOTA/MiniFOTA 流程、`AT+QFOTADL`、差分包要求、URC 与结果码 |

### 1.2 文档修订历史

| 版本 | 日期 | 本版主要修改 |
|---|---|---|
| 1.0 | 2020-06-10 | Ramos Zhang、Galen Zhou 创建初始版本。 |
| 1.1 | 2020-10-27 | Ramos Zhang：更新 EC200S 系列、EC600S-CN 和 EG912Y 系列的 DFOTA 升级方式；增加 EC600S-CN；更新 AT+QFOTADL 的 update_URC_max 说明（仅 EC200T 系列适用）；新增第 3.2.1.3 章备注。 |
| 1.2 | 2021-04-19 | Evan Meng：增加 EC200N-CN 和 EC600N-CN；补充 AT 命令 URL 最大长度及不同 Flash 版本的 URC 差异；更新 update_URC_max 说明；增加 8 MB/16 MB 升级方式差异；更新 err 结果码。 |
| 1.3 | 2022-06-16 | Amos Yao：增加 EC200A、EC800N-CN、EG915N 系列；移除 EG912Y-CN 和 EC200T；补充 8 MB 版本差分包后缀校验机制。 |
| 1.4 | 2025-02-14 | Pony Ma、Yule Deng：更名为“升级指导”；新增 DFOTA 实施与用户责任、MiniFOTA 内容；新增 EC200M-CN、EC600M/K、EC800K/M、EG600AK-CN、EG800AK-CN、EG800K、EG800W-CN、EG810M、EG912N-EN、EG915K-EU、EG950A；移除 EOL 的 EC200S、EC600S-CN、EC800N-CN、EG912Y-EU；将 EC200N-CN 更新为 EC200N 系列。优化流程图和步骤说明、AT 示例、FTP/HTTP URL 参数说明及命令支持情况，并更新两种 FILE 命令的功能描述。 |
| 1.5 | 2026-02-25 | Haldon Huang：新增 EG800M-CN；将 EG800AK-CN、EG915K-EU、EC200N 系列范围分别调整为 EG800AK 系列、EG915K 系列、EC200N-CN；移除 EG800W-CN；增加 DFOTA 包不得修改的说明、部分 EG915N 不支持 FTP DFOTA 的说明，并更新 HTTPS 服务器配置支持情况。 |

### 1.3 责任、保密和免责声明摘要

- **厂商联系信息：**上海移远通信技术股份有限公司，上海市松江区泗泾镇外婆泾路 8 号，邮编 201601；电话 +86 21 5108 6236；邮箱 info@quectel.com。当地办事处信息见 https://www.quectel.com.cn/contact；技术支持与文档反馈见 https://www.quectel.com.cn/contact?tab=t 或 support@quectel.com。
- **设计责任：**文档作为产品设计参考，用户须结合自身分析、评估和判断。内容和服务按“可用”状态提供，移远通信可不预先通知而更新文档。
- **许可与版权：**接收者须保密并仅为项目实施使用相关硬软件、材料和文档；未经书面许可，不得披露、复制、转载、翻译、分发、修改或创作衍生作品。购买产品仅涉及正常的非独家、免版税产品使用许可，不代表取得专利、版权或商标权；违反保密或未经授权使用可能被追究责任。
- **商标与第三方材料：**文档不授予使用移远或第三方商标、商号的权利；第三方材料仍受相应限制。移远通信不对第三方材料及第三方网站的适销性、特定用途、平静受益权、系统集成、信息准确性、知识产权不侵权、可访问性、安全性或可用性作保证。
- **隐私：**实现产品功能所需的设备数据可能上传到移远通信、运营商、芯片供应商或用户指定的服务器；用户与第三方交互前应了解其隐私和数据安全政策。
- **免责：**移远通信不承担违反操作/设计规范、文档不准确或遗漏、开发中功能缺陷，以及第三方网站和资源导致的相关损害或责任；开发中功能不作明示、暗示或法定保证（另有协议的除外）。原文版权标注为 © 2026 Quectel Wireless Solutions Co., Ltd.，保留一切权利。
- DFOTA 的发起时机和执行控制权属于用户侧。移远通信提供相应差分固件包支持，但不会单方面向用户设备推送升级。
- 具体产品支持情况以模块和固件实际规格为准；本文是面向工程使用的结构化整理，不替代原厂文件或授权。

## 2. 适用范围与升级方式选择

### 2.1 适用模块

V1.5 列出的 LTE Standard(A) 产品如下。表中“系列”表示原文按系列列出的适用范围；部分型号支持特性可能因子型号、固件或硬件配置不同而变化。

| 模块系列 | 适用型号/系列 |
|---|---|
| LTE Standard(A) | EC200A 系列 |
| LTE Standard(A) | EC200M-CN |
| LTE Standard(A) | EC200N-CN |
| LTE Standard(A) | EC600K-CN |
| LTE Standard(A) | EC600M 系列 |
| LTE Standard(A) | EC600N-CN |
| LTE Standard(A) | EC800K-CN |
| LTE Standard(A) | EC800M-CN |
| LTE Standard(A) | EG600AK-CN |
| LTE Standard(A) | EG800AK 系列 |
| LTE Standard(A) | EG800K 系列 |
| LTE Standard(A) | EG800M-CN |
| LTE Standard(A) | EG810M 系列 |
| LTE Standard(A) | EG912N-EN |
| LTE Standard(A) | EG915K 系列 |
| LTE Standard(A) | EG915N 系列 |
| LTE Standard(A) | EG950A 系列 |

**型号限定说明：**

1. EG950A 系列的 DFOTA 功能为可选项，需向移远通信技术支持确认具体支持情况。
2. 部分 EG915N 系列型号不支持通过 FTP 进行 DFOTA，需按具体型号向技术支持确认。
3. PDF 修订记录注明已删除 EG800W-CN；不可仅因早期版本曾列出该型号，就推定 V1.5 仍覆盖它。
4. 不得修改差分升级包内容，也不得解压后重新压缩。修改可能破坏包校验并导致升级失败。

### 2.2 DFOTA 与 MiniFOTA 的选择

原文以模块系列和 Flash 容量区分升级方式：

| 模块/Flash 条件 | 原文给出的升级方式 |
|---|---|
| EC200A 系列，8 MB 或 16 MB | DFOTA |
| EC200A 以外的系列，16 MB | DFOTA |
| EC200A 以外的系列，2 MB、4 MB 或 8 MB | MiniFOTA |

固件名称末尾常用于识别 Flash 容量：`M02` 表示 2 MB，`M04` 表示 4 MB，`M08` 表示 8 MB，`M16` 表示 16 MB。例如，`EC200ACNHAR01A02M16` 表示 16 MB；`EC200NCNLAR03A01M08` 表示 8 MB。此处示例按原文给出，正式升级仍须核对完整的模块型号和固件版本。

**选择路径时还应同时核对：**

- MiniFOTA 包由 `.mini_1` 和 `.mini_2` 两个文件组成，二者文件主体名称相同。
- MiniFOTA 通过服务器 URL 升级；不支持使用用户主机本地文件的两个 `FILE` 入口。
- MiniFOTA URL 长度限制为 128 个字符。2 MB 模块不支持 HTTPS 配置。
- FTP 方式的命令说明另有“2 MB 或 4 MB 模块不支持该命令”的备注，而通用 MiniFOTA 章节又说明 MiniFOTA 可使用 FTP。原文此处存在适用范围不够明确的表述，2 MB/4 MB 型号使用 FTP 前应向模块技术支持确认。
- 8 MB 版本模块不支持下载进度 URC 上报。

## 3. 升级前准备与整体流程

### 3.1 角色和流程

```text
用户确认模块原版本和目标版本
          │
          ▼
通过 ATI 获取原固件信息，并向移远技术支持申请匹配的差分包
          │
          ▼
用户将差分包上传到自有 FTP/HTTP(S) 服务器，或保存在用户侧主机
          │
          ▼
主机向模块 AT 口发送 AT+QFOTADL
          │
          ├─ 服务器方式：模块联网下载差分包
          └─ 本地文件方式：主机经串口或 USB AT 口发送差分包
          │
          ▼
模块验证差分包，并按 DFOTA 或 MiniFOTA 流程写入固件
          │
          ▼
模块重启并进入升级后的正常模式
```

移远侧主要提供与源版本、目标版本匹配的差分升级包；服务器搭建、上传、升级时机和 AT 命令由用户侧负责。FTP/HTTP(S) 服务器须由用户自建和控制，原文不包含服务器搭建教程。

### 3.2 升级前置条件

1. 使用 `ATI` 获取模块当前固件版本，并明确目标版本；将二者信息提供给移远技术支持以申请对应差分包。
2. 确认模块型号、Flash 容量、升级方式和差分包文件名/扩展名相互匹配。
3. 使用服务器方式时，先将差分包上传到自有服务器并记下路径、端口及认证信息。FTP 示例只支持服务器根目录中的文件路径。
4. 使用本地主机传输时，确认 AT 口、串口/USB 接口、硬件流控和待发送文件字节数。
5. DFOTA 升级过程中模块必须持续供电。断电可能导致升级失败且无法还原。
6. 确认网络可用、PDP/APN 配置正确。DFOTA FTP/HTTP 示例使用 PDP 上下文；MiniFOTA 不要求通过 `AT+QCFG="fota/cid"` 指定 PDP 上下文。
7. 保留升级期间与模块的 AT 通信能力，等待最终 `FOTA,END` 结果或按相应 MiniFOTA 状态处理异常。仅收到 `OK` 不等于整个升级已完成。

### 3.3 升级包及路径要求

- 差分包由移远技术支持按原固件和目标固件版本提供。
- 不修改、解压或重新压缩差分包。
- DFOTA 使用单个差分包，扩展名不得为 `.mini`。
- MiniFOTA 使用名称相同的两个文件：第一阶段包扩展名 `.mini_1`，第二阶段包扩展名 `.mini_2`。`.mini_1` 下载升级后，模块会自动请求同名 `.mini_2`。
- 原文所说的“差分固件包名称”不包含文件扩展名；因此两个 MiniFOTA 包在扩展名前的主体名称必须相同。
- 原图 2 用 `system_patch.mini_1` 和 `system_patch.mini_2` 展示命名方式；截图中的文件大小只是制作示例，不代表模块所需的固定包大小。
- MiniFOTA 依赖扩展名完整性校验，不得省略、改写或混淆上述后缀。
- MiniFOTA 流程启动后，只有两个包都下载和升级完成，模块才会回到正常模式；文件名、扩展名和 URL 路径应提前确认。

## 4. DFOTA 与 MiniFOTA 流程详解

### 4.1 DFOTA 阶段

DFOTA 将下载的差分包放在模块 UFS 中。可通过 `AT+QFLDS` 查询 UFS 空间。包下载后先进行校验，再重启进入升级模式。升级流程开始前，上位机仍可主动终止流程并回到升级前版本；升级开始后，应等待升级完成再恢复正常业务。

| 阶段 | 模块动作 | 结果/注意事项 |
|---|---|---|
| 1. 启动 | 用户执行 `AT+QFOTADL` | 接受命令后模块开始下载或等待本地包。 |
| 2. 下载 | 模块从 FTP/HTTP(S) 取包，或通过本地接口收包 | 网络或传输错误时上报相应 `<FTP_err>`、`<HTTP_err>` 或 `<file_err>`。 |
| 3. 校验 | 下载完成后检查差分包 | 校验错误会以 `FOTA,END` 上报 `<err>`，并退出升级流程。 |
| 4. 进入升级 | 校验正确后模块自动重启并执行差分升级 | 升级期间应保持供电，避免中断。 |
| 5. 完成 | 固件升级成功后模块再次自动重启 | 进入正常模式；成功结果为 `<err>=0`。 |

`<upgrade_mode>` 决定下载后的升级启动方式：`0` 表示下载成功后先重启再升级；`1` 表示下载成功后立即开始升级。参数值的具体支持情况可由 `AT+QFOTADL=?` 查询。

### 4.2 MiniFOTA 阶段

MiniFOTA 将程序分为负责联网下载的 Mini 系统和其余的大系统。大系统空间可在 Mini 系统更新期间用于保存 Mini 系统差分数据或升级备份；MiniFOTA 需要先更新 Mini 系统，再由新 Mini 系统下载并整包更新大系统。

| 阶段 | 模块动作 | 恢复特点 |
|---|---|---|
| 1. 进入 Mini 模式 | 执行 `AT+QFOTADL` 后自动重启，以旧版本 Mini 系统运行 | 流程尚未通过初始镜像匹配检查时，部分错误可以返回大系统。 |
| 2. 下载 `.mini_1` | 联网下载 Mini 系统差分包；下载包头 2 KB 后依据其中信息对模块已烧录镜像执行 CRC32 匹配检查 | 不匹配时停止下载并退回大系统，需修正包或 URL 后重新触发。 |
| 3. 校验并升级 Mini 系统 | `.mini_1` 完整下载后做完整性校验，成功后重启并更新 Mini 系统部分 | 校验错误会自动重启并重新执行下载阶段。 |
| 4. 启动新 Mini 系统 | Mini 系统更新完成后再次自动重启，开始运行新 Mini 系统 | 原文说明整个 MiniFOTA 过程约经历四次自动重启。 |
| 5. 下载 `.mini_2` | 新 Mini 系统联网下载第二个包，并以整包方式边下载边写入 Flash，覆盖大系统部分 | 断电、网络或服务器中断时，模块会重试本阶段；大系统已擦写，不能安全退回旧大系统。 |
| 6. 完成 | 第二包升级结束后自动重启 | Mini 系统和大系统都更新后进入正常模式。 |

MiniFOTA 的下载 URL、文件名和服务器可用性是升级连续性的关键。升级已进入 Mini 下载模式后，模块可能只保留注网、下载和升级能力；在 `.mini_1` 之后出现网络中断或 `.mini_2` 缺失时，可能持续留在 Mini 模式重试，直到网络和包恢复。

## 5. AT 命令语法与通用参数

### 5.1 AT 语句约定

- 每条命令以 `AT` 或 `at` 开头，以回车符 `<CR>` 结束。命令响应通常以 `<CR><LF>` 分隔；原 PDF 表格为便于阅读省略了这些控制字符。
- `<参数名>` 是参数占位符，实际输入时不输入尖括号；`[可选项]` 表示可省略，实际命令也不输入方括号。
- 未特别说明时，省略的设置参数使用其之前已设定的值或默认值。
- 文档中的下划线表示该参数默认值。

| AT 命令类型 | 格式 | 用途 |
|---|---|---|
| 测试 | `AT+<cmd>=?` | 查询命令是否存在及参数类型、取值或范围。 |
| 查询 | `AT+<cmd>?` | 查询命令当前参数值。 |
| 设置 | `AT+<cmd>=<p1>[,<p2>...]` | 设置参数。 |
| 执行 | `AT+<cmd>` | 查询特定信息或执行操作。 |

AT 示例中的 URL、域名、IP、用户名和密码均为演示内容，不是可直接使用的服务器配置；各独立示例之间也不构成连续操作步骤。

### 5.2 `AT+QFOTADL` 测试命令和通用参数

测试命令：

    AT+QFOTADL=?

原文给出的响应格式为：

    +QFOTADL: <url>,(支持的<upgrade_mode>列表),(支持的<download_URC_max>列表),(支持的<update_URC_max>列表)
    OK

响应包含 `<url>` 格式、支持的 `<upgrade_mode>` 列表、`<download_URC_max>` 列表和 `<update_URC_max>` 列表。应先用该命令确认固件所报告的具体取值范围。

| 参数 | 含义 | 允许值/范围 |
|---|---|---|
| `<url>` | FTP、HTTP(S) URL，或本地文件入口 | 字符串，最大长度 255 字节；MiniFOTA URL 另受 128 字符限制。 |
| `<upgrade_mode>` | 下载成功后的升级启动方式 | `0`：先重启再升级；`1`：立即升级。 |
| `<download_URC_max>` | 下载进度 URC 最大上报条数 | `0`：关闭；`5–100`：设置最大条数。末条表示下载完成。 |
| `<update_URC_max>` | 升级进度 URC 最大上报条数 | `0`：关闭；`5–100`：设置最大条数。末条表示升级完成。 |

当设置上报条数为 50 时，原文示例将第 25 条解释为约 50% 进度，第 50 条表示完成。8 MB 版本不支持下载进度 URC。MiniFOTA 不支持通过本命令配置下载/升级进度 URC；DFOTA 的升级 URC 数量也可能使用默认值而不接受该参数，详见[第 7 章](#7-dfota-与-minifota-差异)。

MiniFOTA 服务器 URL 最大长度为 128 个字符；一般命令说明的 255 字节限制不应覆盖这个更具体的限制。MiniFOTA 对 FTP URL 长度和 2 MB/4 MB FTP 支持的说明存在需要按型号确认的边界，见[第 2 章](#2-适用范围与升级方式选择)及各命令备注。

### 5.3 URC 阶段名称

| URC | 含义 |
|---|---|
| `FOTA,FTPSTART` / `FOTA,HTTPSTART` / `FOTA,FILESTART` | 开始相应来源的差分包传输。 |
| `FOTA,DOWNLOADING,<percent>` | 下载进度。 |
| `FOTA,FTPEND,<FTP_err>` / `FOTA,HTTPEND,<HTTP_err>` / `FOTA,FILEEND,<file_err>` | 相应来源传输结束；结果码为 0 表示包下载成功。 |
| `FOTA,START` | 开始固件升级阶段。 |
| `FOTA,UPDATING,<percent>` | 升级进度。 |
| `FOTA,END,<err>` | 升级流程结束；`<err>=0` 表示成功。 |

是否实际输出进度 URC 取决于传输方式、DFOTA/MiniFOTA 类型、Flash 版本、参数值和固件能力。MiniFOTA 的 URC 与上述 DFOTA 下载/升级分段流程不同，见第 7 章。

**原 PDF 图 3–5 的 URC 示例摘录：**

    +QIND: "FOTA","FILESTART"
    +QIND: "FOTA","DOWNLOADING",1
    +QIND: "FOTA","DOWNLOADING",2
    ...
    +QIND: "FOTA","DOWNLOADING",50
    +QIND: "FOTA","FILEEND",0

    +QIND: "FOTA","UPDATING",8
    +QIND: "FOTA","UPDATING",16
    ...
    +QIND: "FOTA","UPDATING",50
    +QIND: "FOTA","UPDATING",58
    +QIND: "FOTA","UPDATING",66
    ...
    +QIND: "FOTA","UPDATING",91
    +QIND: "FOTA","END",0

    +QIND: "FOTA","UPDATING",2
    +QIND: "FOTA","UPDATING",4
    ...
    +QIND: "FOTA","UPDATING",49
    +QIND: "FOTA","UPDATING",60
    +QIND: "FOTA","UPDATING",64
    +QIND: "FOTA","UPDATING",68
    ...
    +QIND: "FOTA","UPDATING",96
    +QIND: "FOTA","END",0

以上数值分别摘自原图的 DFOTA 下载、DFOTA 升级和 MiniFOTA 升级示例，用省略号表示原图中未逐项列出的中间 URC；它们是示例上报值，不应视为所有固件固定使用的进度序列。

## 6. `AT+QFOTADL` 四种升级入口

### 6.1 FTP 服务器方式

**命令格式：**

    AT+QFOTADL=<FTP_URL>[,<upgrade_mode>[,<download_URC_max>[,<update_URC_max>]]]

FTP URL 格式为：

    FTP://<user_name>:<password>@<serverURL>:<port>/<file_path>

| 字段 | 含义和限制 |
|---|---|
| `<FTP_URL>` | 以 `FTP://` 开头的完整地址，最大 255 字节。 |
| `<user_name>` / `<password>` | FTP 登录凭据，各自最大 50 字节。 |
| `<serverURL>` | 用户自建 FTP 服务器的 IP 地址或域名，最大 50 字节。 |
| `<port>` | 端口 1–65535，默认 21。 |
| `<file_path>` | 服务器上的包文件名，最大 50 字节；原文说明目前仅支持根目录存储。 |
| 其他可选参数 | 升级方式及下载/升级 URC 上报数量，定义见第 5.2 节。 |

**DFOTA URC 顺序：**`FTPSTART` → 可选 `DOWNLOADING` → `FTPEND,<FTP_err>` → `START` → 可选 `UPDATING` → `END,<err>`。某一步出错时也可能返回 `ERROR`；应结合 URC 与结果码判断错误发生在传输还是升级阶段。

**原文限制：**

1. Flash 为 2 MB 或 4 MB 的模块型号不支持该命令。该备注与后文“MiniFOTA 支持 FTP URL”并列出现，不能据此推断所有 2 MB/4 MB MiniFOTA 固件都支持或都不支持 FTP；须按准确型号和固件向技术支持确认。
2. 部分 EG915N 系列型号不支持 FTP DFOTA。
3. MiniFOTA 不支持通过 `AT+QCFG="fota/cid"` 设置 PDP 上下文。
4. MiniFOTA 不支持配置下载和升级进度 URC。

**DFOTA 配置示例（参数均为示意）：**

    AT+QICSGP=2,1,"cmnet","","",1
    AT+QCFG="fota/cid",2
    AT+QIACT=2
    AT+QFOTADL="FTP://test:test@192.0.2.2:21/EC200ACNHAR01A02M16.bin",1,50

示例先配置 PDP 上下文 2、指定 FOTA 使用该上下文并激活它，再通过 FTP URL 触发升级。`192.0.2.2` 和凭据仅为文档示例，必须替换为真实有效的配置。MiniFOTA 按原文说明无需执行示例中的 `fota/cid` 指定与 PDP 激活操作。

### 6.2 HTTP(S) 服务器方式

**命令格式：**

    AT+QFOTADL=<HTTP_URL>[,<upgrade_mode>[,<download_URC_max>[,<update_URC_max>]]]

URL 以 `http://` 或 `https://` 开头，格式为：

    http[s]://<HTTP_server_URL>:<HTTP_port>/<HTTP_file_path>

| 字段 | 含义和限制 |
|---|---|
| `<HTTP_URL>` | HTTP(S) 完整地址，最大 255 字节。MiniFOTA URL 另有最大 128 字符限制。 |
| `<HTTP_server_URL>` | 用户自建 HTTP(S) 服务器的 IP 地址或域名。 |
| `<HTTP_port>` | 端口 1–65535，默认 80。 |
| `<HTTP_file_path>` | 服务器上的差分包文件路径。 |
| 其他可选参数 | 升级方式、下载进度 URC 数量和升级进度 URC 数量。 |

**DFOTA URC 顺序：**`HTTPSTART` → 可选 `DOWNLOADING` → `HTTPEND,<HTTP_err>` → `START` → 可选 `UPDATING` → `END,<err>`。

**原文限制：**

1. MiniFOTA 支持 HTTP 和 HTTPS 服务器配置；2 MB Flash 的模块暂不支持 HTTPS 配置。
2. MiniFOTA 不支持通过该命令设置下载和升级 URC 数量。
3. MiniFOTA URL 最大 128 个字符；一般 URL 参数最大 255 字节。

**DFOTA 配置示例（示意地址）：**

    AT+QICSGP=2,1,"cmnet","","",1
    AT+QCFG="fota/cid",2
    AT+QIACT=2
    AT+QFOTADL="http://www.example.com:100/EC200ACNHAR01A02M16.bin",1,50

示例中的服务器地址不提供真实升级文件。使用 DFOTA 时按设备配置 PDP 上下文；MiniFOTA 按原文说明无需执行 `fota/cid` 设置和 PDP 激活操作。V1.5 的修订记录专门更新了 HTTPS 支持说明，部署前应按具体型号核实 HTTPS 能力。

### 6.3 用户侧主机文件方式（具备硬件流控）

**命令格式：**

    AT+QFOTADL="FILE:<length>"[,<upgrade_mode>[,<download_URC_max>[,<update_URC_max>]]]

`<length>` 是差分包的字节数。模块通过主串口或 USB AT 口接收主机发送的包，再执行升级。

**主串口操作顺序：**

1. 打开模块主串口并在主机串口工具中启用硬件流控。
2. 向模块发送 `AT+IFC=2,2`，启用模块硬件流控。
3. 在主机上选择与模块目标版本匹配的差分包，确认其实际长度。
4. 发送 `AT+QFOTADL="FILE:<length>"`，按需要追加升级和 URC 参数。
5. 等待模块进入接收阶段，再发送二进制差分包。

**移远 USB AT 口操作顺序：**

1. 打开移远 USB AT 口并启用主机侧硬件流控。
2. 发送 `AT+QCFG="usbifc",2,2`，启用模块 USB 硬件流控。
3. 选择并确认差分包及文件长度。
4. 发送 `AT+QFOTADL="FILE:<length>"`，再发送包数据。
5. 若 URC 通过 USB AT 口接收，建议禁用 USB 挂起功能，避免丢失进度 URC。

若未启用硬件流控，原文称主机发送速度会受限；NOR Flash 模块的速度限制为 15 KB/s。同时原文要求每次发送固件包控制在 32 字节以内。使用该本地文件入口时应按平台接口规则控制传输节奏和长度。

成功启动后，典型 URC 为 `FILESTART` → 下载进度 → `FILEEND,0` → `START` → 升级进度 → `END,0`。例如：

    AT+QCFG="usbifc",2,2
    AT+QCFG="usbifc"
    +QCFG: "usbifc",2,2
    AT+QFOTADL="FILE:4884688",1,50

`4884688` 是原文示例包长，并非任何实际固件的通用长度。发送字节数必须与实际文件完全一致。MiniFOTA 不支持此本地文件升级入口。

### 6.4 用户侧主机文件方式（串口不支持硬件流控）

当用户侧主机只有不支持硬件流控的串口时，使用专门的协议下载模式：

    AT+QFOTADL="FILE:<length>",<n>

| 参数 | 含义 |
|---|---|
| `<length>` | 差分包长度。 |
| `<n>` | `2` 表示使能协议下载模式进行 DFOTA。 |

接受命令后原文示例响应为 `010003`；若有错误则返回 `ERROR`。具体串口协议传输细节未在本 PDF 中公开，原文要求联系移远技术支持。因此不能只依赖上述命令就实现自定义主机侧文件发送协议。MiniFOTA 不支持此入口。

## 7. DFOTA 与 MiniFOTA 差异

| 比较项 | DFOTA | MiniFOTA |
|---|---|---|
| 差分包组成 | 单个差分包；扩展名不为 `.mini`。 | 两个包：同名的 `.mini_1` 和 `.mini_2`。 |
| 更新对象和阶段 | 下载单个差分包，校验后升级固件。 | 先升级 Mini 系统，再由新 Mini 系统下载并整包升级大系统。 |
| 支持的下载来源 | FTP、HTTP(S)、本地主机文件（硬件流控或专用协议下载方式）。 | 仅支持服务器 URL；不支持本地主机 `FILE` 两种入口。 |
| PDP 上下文 | 示例中可通过 `AT+QCFG="fota/cid"` 指定并激活 PDP。 | 不支持该 `fota/cid` 配置；示例说明不需要此操作。 |
| 下载进度 URC | 可配置下载进度 URC；8 MB 版本例外，不上报下载进度。 | 不支持通过 `AT+QFOTADL` 配置 URC。 |
| 升级进度 URC | DFOTA 支持升级阶段 URC，但最大上报数量可能采用默认值，不能由命令设置。 | 上报升级进度，最大数量使用默认值，不能由命令设置。 |
| MiniFOTA 进度解释 | 不适用。 | 总升级进度前 60% 对应 `.mini_1`，后 40% 对应 `.mini_2`。 |
| 断电/网络中断行为 | DFOTA 过程中必须保持供电；升级开始后等待完成。 | 不同阶段恢复能力不同；进入第二阶段后通常留在 Mini 模式持续重试，直到网络和包恢复。 |
| URL 长度 | 通用参数最大 255 字节。 | 服务器 URL 最大 128 个字符。 |
| Flash 映射 | EC200A 8 MB/16 MB；其他系列 16 MB。 | EC200A 以外的 2 MB/4 MB/8 MB。 |

**URC 重点：**DFOTA 的下载与升级进度分属下载 `DOWNLOADING` 和升级 `UPDATING` 阶段。MiniFOTA 只上报差分包升级进度，前 60% 映射到 `.mini_1`，后 40% 映射到 `.mini_2`。DFOTA 方式升级时可设置下载进度上报数，但升级进度 URC 数量使用默认值；MiniFOTA 两种进度数量都不支持通过 `AT+QFOTADL` 配置。

## 8. MiniFOTA 异常处理与注意事项

MiniFOTA 执行 `AT+QFOTADL` 后会自动进入下载模式。其恢复策略取决于失败发生阶段：

| 场景 | 模块行为 | 用户处理 |
|---|---|---|
| URL 错误、路径含不支持的特殊字符、文件不存在或服务器无法连接，且模块尚未下载内容 | 等待超时后退回大系统。 | 修正 URL、文件名、服务器或网络，再由用户重新触发升级。 |
| `.mini_1` 已下载头部 2 KB，但 CRC32 检查发现包与模块烧录镜像不匹配 | 停止下载并退回大系统。 | 确认模块原版本和差分包匹配，再重新触发。 |
| `.mini_1` 已通过镜像匹配，后续下载过程中网络中断且已有数据写入 Flash | 模块不会退出 Mini 系统返回大系统，而会持续重试下载。 | 恢复网络和服务器，确保正确 `.mini_1` 可访问，等待续行。 |
| `.mini_1` 升级完成，新 Mini 系统下载 `.mini_2` 失败、服务器异常或第二包不存在 | 大系统已被擦写，模块不能安全退回大系统，会在 Mini 模式等待/重试。 | 修正服务器内容和包名，在 Mini 下载模式中再次执行 `AT+QFOTADL` 继续升级。 |
| MiniFOTA 包下载或升级过程中模块重启 | 原文提示完整流程会经历四次自动重启，USB 等接口可能断开。 | 预期这些重启和连接变化，不要将其单独误判为升级失败；待流程结束再确认版本。 |

异常处置要点：

1. 升级前检查两份包都已上传，主体名称一致，扩展名准确，URL 指向正确位置。
2. 如果差分包错误，删除服务器上的错误文件，上传正确包；模块仍在 Mini 下载模式时，按正确 URL 和包名重新执行 `AT+QFOTADL`。
3. `.mini_1` 的校验通过、开始写入后，流程可能无法回退大系统。确保网络和服务器长期可用，并维持稳定供电。
4. 网络中断后的行为依阶段而异：初始阶段可能超时退回大系统；进入已验证数据写入阶段后则持续重试。不能把“可重试”理解成任何阶段都能安全取消。

## 9. 全部结果码

结果码分别由 FTP、HTTP(S)、本地文件传输和固件升级阶段上报。下载进度 URC 本身在 8 MB 版本上不支持；这不改变各结果码表的含义。

### 9.1 FTP 结果码 `<FTP_err>`

| 值 | 描述 |
|---:|---|
| 0 | FTP 下载成功 |
| 601 | FTP 未知错误 |
| 602 | FTP 服务受阻 |
| 603 | FTP 服务忙 |
| 604 | DNS 解析失败 |
| 605 | 网络错误 |
| 606 | 控制连接关闭 |
| 607 | 数据连接关闭 |
| 608 | 对方关闭 Socket |
| 609 | 超时错误 |
| 610 | 无效参数 |
| 611 | 文件打开失败 |
| 612 | 文件路径错误 |
| 613 | 文件错误 |
| 614 | 服务不可用，正在关闭控制连接 |
| 615 | 打开数据连接失败 |
| 616 | 连接关闭，传输中止 |
| 617 | 未请求文件 |
| 618 | 请求操作中止：处理时发生本地错误 |
| 619 | 请求操作未执行：系统内存不足 |
| 620 | 语法错误，命令未识别 |
| 621 | 参数语法错误 |
| 622 | 命令未实施 |
| 623 | 命令坏顺序 |
| 624 | 命令参数未实施 |
| 625 | 登录 FTP 失败 |
| 626 | 存储文件需账号 |
| 627 | 请求操作未执行 |
| 628 | 请求操作中止：页面类型未知 |
| 629 | 请求文件操作中止 |

### 9.2 HTTP(S) 结果码 `<HTTP_err>`

| 值 | 描述 |
|---:|---|
| 0 | HTTP(S) 下载成功 |
| 701 | HTTP(S) 未知错误 |
| 702 | HTTP(S) 超时 |
| 703 | HTTP(S) 忙 |
| 704 | HTTP(S) UART 忙 |
| 705 | HTTP(S) 未获取/发送请求 |
| 706 | HTTP(S) 网络繁忙 |
| 707 | HTTP(S) 网络打开失败 |
| 708 | HTTP(S) 网络未配置 |
| 709 | HTTP(S) 网络被去激活 |
| 710 | HTTP(S) 网络错误 |
| 711 | HTTP(S) URL 错误 |
| 712 | HTTP(S) URL 空 |
| 713 | HTTP(S) IP 地址错误 |
| 714 | HTTP(S) DNS 错误 |
| 715 | HTTP(S) Socket 创建错误 |
| 716 | HTTP(S) Socket 连接错误 |
| 717 | HTTP(S) Socket 读取错误 |
| 718 | HTTP(S) Socket 写入错误 |
| 719 | HTTP(S) Socket 关闭 |
| 720 | HTTP(S) 数据编码错误 |
| 721 | HTTP(S) 数据解码错误 |
| 722 | HTTP(S) 读取超时 |
| 723 | HTTP(S) 响应失败 |
| 724 | 来电繁忙 |
| 725 | 语音通话繁忙 |
| 726 | 输入超时 |
| 727 | 等待数据超时 |
| 728 | 等待 HTTP(S) 响应超时 |
| 729 | 分配内存失败 |
| 730 | 无效参数 |

### 9.3 文件传输结果码 `<file_err>`

| 值 | 描述 |
|---:|---|
| 0 | 文件下载成功 |
| 500 | 文件下载错误 |

### 9.4 固件升级结果码 `<err>`

| 值 | 描述 |
|---:|---|
| 0 | DFOTA 升级成功 |
| 504 | DFOTA 升级失败 |
| 505 | DFOTA 包校验出错 |
| 506 | DFOTA 固件 MD5 检查错误 |
| 507 | DFOTA 包版本不匹配 |
| 552 | DFOTA 包项目名不匹配 |
| 553 | DFOTA 包基线名不匹配 |

## 10. 升级操作检查表

### 10.1 执行前

- [ ] `ATI` 读取到的模块固件版本与申请差分包时提供的源版本一致。
- [ ] 已确认模块准确型号、Flash 大小和采用 DFOTA/MiniFOTA 的规则。
- [ ] 差分包未经修改；MiniFOTA 的 `.mini_1`、`.mini_2` 文件均存在且主体名称相同。
- [ ] 服务器、凭据、路径、端口和网络连通性已确认；MiniFOTA URL 不超过 128 字符。
- [ ] 已确认型号的 HTTPS/FTP 限制，尤其 2 MB HTTPS、2/4 MB FTP 和部分 EG915N FTP 支持情况。
- [ ] 若使用 DFOTA 服务器方式，已按实际 PDP 配置；MiniFOTA 不配置 `fota/cid`。
- [ ] 若使用本地主机文件方式，已核对文件实际字节数、接口、硬件流控及 USB 挂起设置。
- [ ] 升级期间保持模块供电，并预期 MiniFOTA 会自动重启、可能暂时断开 USB/AT 通道。

### 10.2 执行中与完成后

1. 发送 `AT+QFOTADL=?` 读取当前固件支持的参数，再选用支持的命令入口。
2. 按 FTP、HTTP(S) 或本地文件入口发送命令。
3. 区分下载结束码（`FTPEND`、`HTTPEND`、`FILEEND`）与最终升级结束码（`FOTA,END`）。下载成功不等于升级成功。
4. MiniFOTA 中若出现模块重启或 USB 断开，应结合当前包阶段与 URC 判断；第二阶段异常时恢复网络/服务器并继续，不要假设可回到旧大系统。
5. 完成后核对模块重新上线并通过 `ATI` 确认目标固件版本。

### 10.3 常见故障定位顺序

| 现象 | 先检查 |
|---|---|
| `AT+QFOTADL` 返回 `ERROR` | 命令格式、固件支持列表、URL 长度、该型号/Flash 是否支持对应入口。 |
| `FTPEND` 非 0 | DNS、网络、FTP 服务状态、端口、账号密码、文件名及根目录路径。 |
| `HTTPEND` 非 0 | URL 格式、DNS/IP、服务器端口与响应、PDP/网络状态、HTTPS 型号限制。 |
| `FILEEND` 非 0 | 文件长度、串口/USB 流控、传输字节数、传输节奏和 USB 挂起。 |
| `FOTA,END` 非 0 | 包校验、源版本/目标版本匹配、MD5、项目名和基线名。 |
| MiniFOTA 长时间处于下载模式 | 当前处于 `.mini_1` 还是 `.mini_2` 阶段，文件是否同名、扩展名是否完整、服务器和网络是否恢复。 |

以上定位顺序是对 PDF 流程和结果码的整理；最终错误原因应以模块实际 URC、AT 日志和对应固件支持信息为准。

## 11. 参考资料与术语

### 11.1 原 PDF 引用的配套资料

1. 《Quectel LTE Standard(A) 系列 FILE 应用指导》——用于查询 `AT+QFLDS` 等文件系统命令。
2. 《Quectel LTE Standard(A) 系列 TCP(IP) 应用指导》——用于查询 `AT+QICSGP` 等 PDP/TCP/IP 配置命令。

以上资料在原 DFOTA PDF 的参考文档表中列出。本分析不替代相应 AT 命令手册。

### 11.2 术语表

| 缩写 | 英文全称 | 中文含义 |
|---|---|---|
| APN | Access Point Name | 接入点名称 |
| DFOTA | Delta Firmware Over-The-Air | 固件空中差分升级 |
| FTP | File Transfer Protocol | 文件传输协议 |
| GSM | Global System for Mobile Communications | 全球移动通信系统 |
| GPRS | General Packet Radio Service | 通用无线分组业务 |
| HTTP | Hyper Text Transfer Protocol | 超文本传输协议 |
| HTTPS | Hyper Text Transfer Protocol Secure | 超文本传输安全协议 |
| IP | Internet Protocol | 网际互连协议 |
| LTE | Long Term Evolution | 长期演进 |
| MiniFOTA | 原文未提供英文全称 | 使用 Mini 系统分阶段完成的差分升级方式 |
| PAP | Password Authentication Protocol | 口令验证协议 |
| PDP | Packet Data Protocol | 分组数据协议 |
| URL | Uniform Resource Locator | 统一资源定位符 |
| WCDMA | Wideband Code Division Multiple Access | 宽带码分多址 |
| URC | 原文术语表未列出；常用展开为 Unsolicited Result Code | 主动上报结果码（常用译法） |

原 PDF 的术语缩写表列出 APN、DFOTA、FTP、GSM、GPRS、HTTP、HTTPS、IP、LTE、PAP、PDP、URL 和 WCDMA；MiniFOTA 与 URC 在正文中使用，但没有列入该缩写表。

---

**原始 PDF**：`Quectel_LTE_Standard(A)系列_DFOTA_升级指导_V1.5.pdf`（2026-02-25，34 页正文）。本文按原 PDF 结构整理，用于检索与工程阅读；模块支持矩阵、操作风险和 AT 命令的最终依据仍是具体产品固件及移远通信正式资料。
