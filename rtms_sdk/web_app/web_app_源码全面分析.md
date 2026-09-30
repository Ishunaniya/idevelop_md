# web_app 源码分析（当前分支实装）

> 核对基线：`rtms_sdk` 仓库 `develop/rtms_sdk_v1.3_20240408`，提交 `bc60961e`；源码根目录 `/home/tronlong/lyp/code/rtms_sdk/apps/web_app`。本文只把可从该工作树直接确认的行为写作“当前实现”。设备文件、外部命令、网络与构建产物的实际状态没有在目标设备验证。
>
> 阅读范围：本项目 CMake、启动脚本、路由/认证文件、项目特有 C 代码和当前页面及脚本；随仓库编入的 GoAhead 核心只分析与请求主链有关的入口和职责，`src/paks/`、`src/doc/`、CSS、图片、第三方压缩 JS 不逐行复述。文中 `源码:行号` 均相对于上述源码根目录；四份 `docs/` 设计文档仅作背景，具体行为以代码为准。

本文流程图采用纯文本代码块，按编号和箭头从上往下读；`├─`、`└─` 表示互斥分支。普通 Markdown 查看器即可显示，不需要 Mermaid 扩展。

<!-- rtms-analysis-guide:start -->

## 文档目录

本报告对应 `rtms_sdk/apps/web_app/` 在 `bc60961e` 的源码；跨项目关系见[源码分析总览](../源码分析总览.md)。下文原章节编号保留，先用概述、目录、架构、模块和流程五节建立整体脉络。

- [项目概述](#project-overview)
- [源码目录总览](#source-overview)
- [核心架构设计](#core-architecture)
- [核心模块深度分析](#core-modules)
- [关键流程与数据流](#key-flows)
- [1. 结论速览](#analysis-1)
- [2. 构建边界与目录职责](#analysis-2)
- [3. 启动、请求分发与认证](#analysis-3)
- [4. 浏览器页面与端到端功能](#analysis-4)
- [5. 数据路径、配置与外部依赖](#analysis-5)
- [6. 源码可证实的限制和维护风险](#analysis-6)
- [7. 模块关系与修改索引](#analysis-7)
- [8. 设备异常时的排查手册](#analysis-8)
- [9. 核对方法与边界](#analysis-9)

**顺读方式：**先读下面五节，再从原第 1 节起顺序阅读详细分析；需要查特定模块时，可用模块表跳到对应章节。

<a id="project-overview"></a>

## 项目概述

`web_app` 基于 GoAhead 提供记录查询和日志导出页面，也注册配置文件等后端 action；页面入口、action 注册和部署权限需分开核对。

<a id="source-overview"></a>

## 源码目录总览

以下路径相对于该提交的 `apps/web_app/`；“职责”只描述源码中的构建或调用角色。

| 路径 | 职责 |
| --- | --- |
| `CMakeLists.txt`、`global_config.h.in` | go、web_app、gopass 构建目标 |
| `webserver/start_web.sh` | 工作目录、监听和进程监测 |
| `webserver/route.txt`、`webserver/auth.txt` | 实际部署的路由与用户数据 |
| `webserver/html/` | 页面、记录查询和日志导出脚本 |
| `src/src/goahead.c` | HTTP 入口、参数和 action 注册 |
| `src/src/action_func.c` | 记录、配置、下载和批量导出 |
| `src/src/log_export.c` | 日志查询及异步 ZIP 导出 |
| `src/src/` | GoAhead 核心、INI/JSON/TLS 等库源码 |
| `docs/` | 日志导出设计资料；行为仍以实装代码为准 |

<a id="core-architecture"></a>

## 核心架构设计

```text
浏览器 → webserver/html → GoAhead 路由 / 认证
                            ↓
            goahead.c 注册的 action
              ├→ action_func.c → 记录 / 配置 / ZIP
              └→ log_export.c → 内存或 SD 日志 / ZIP
```

页面可见入口、已注册 action、已编译 GoAhead 处理器和设备上实际启用功能分别核对。图中箭头表示源码中的数据或控制方向；外部服务和设备效果以正文标明的证据边界为准。

<a id="core-modules"></a>

## 核心模块深度分析

下表给出主链路模块的入口、关键判断和详细分析位置；具体函数、常量与失败路径以所链章节中的源码引用为准。

| 模块 | 源码入口 | 关键判断与输出 | 详细分析 |
| --- | --- | --- | --- |
| 构建与服务入口 | `CMakeLists.txt`、`src/src/goahead.c` | 共享库与程序目标分离，脚本传入运行参数 | [进入章节](#analysis-2) |
| 路由与认证 | `webserver/route.txt`、`src/src/goahead.c` | 路由、注册、页面入口和认证配置共同决定可达请求 | [进入章节](#analysis-3) |
| 记录与配置 | `src/src/action_func.c` | 查询布局、文件路径、导出任务和配置接口分支不同 | [进入章节](#analysis-4) |
| 日志导出 | `src/src/log_export.c` | 独立目录来源、后台任务及进度查询 | [进入章节](#analysis-4) |

<a id="key-flows"></a>

## 关键流程与数据流

~~~text
start_web.sh → GoAhead / 路由 / 认证 → 已注册 action
浏览器查询 → 记录或日志列表 → 页面显示 / 单文件下载
勾选批量导出 → 后台 ZIP 任务 → 进度轮询 → 触发下载
~~~

[组件与数据流](#analysis-1)、[启动图](#analysis-3)、[记录查询图](#analysis-4)分别展开。日志批量导出还有独立的任务和进度路径，具体条件见第 4 节。

<!-- rtms-analysis-guide:end -->

<a id="analysis-1"></a>

## 1. 结论速览

`web_app` 是设备上的 GoAhead HTTP 服务。CMake 构建 `libgo.so`、`web_app`、`gopass`；启动脚本在 `/usrdata/webserver` 运行 `web_app`，将 `html/` 作为页面根目录，监听脚本指定的 `http://192.168.1.1:80`。页面实际露出的两个业务入口是“按日期查询记录”和“日志导出”。普通记录列表、配置文件导入/删除等后端 action 仍已注册，但主页导航入口被注释。记录查询有 `dated`、`erk`、`flat-can` 三套布局；日志查询另用 `mem`、`sd` 两套来源目录。两套批量导出各建自己的后台 ZIP 任务并由页面轮询进度。依据：`CMakeLists.txt:95-199`，`webserver/start_web.sh:3-31`，`webserver/html/home.html:262-265`，`src/src/goahead.c:404-423`。

### 1.1 组件与数据流

```text
浏览器（index.html / home.html）
    |
    | HTTP 页面、/action/请求
    v
GoAhead（路由、认证、静态文件）
    |
    | 按 action 名查找注册函数
    +--> action_func.c（记录、配置、下载）
    |       +--> conf.c / iniparser --> /opt/conf.ini
    |       +--> /opt/ 配置文件
    |       +--> /media/sdcard/ 记录和 ZIP
    |       +--> 系统 zip 命令
    |
    +--> log_export.c（日志查询、导出）
            +--> /tmp/rtms_log/
            +--> /media/sdcard/rtms_log/
            +--> 系统 zip 命令
```

这里的箭头表示代码调用或文件访问；图中没有画出未在本项目实现的记录生成进程、日志生成进程。GoAhead 核心负责网络、路由、认证、上传和静态文件服务，项目业务层实现查询、下载、打包。依据：`src/src/http.c:229-286`，`src/src/action.c:22-98`，`src/src/goahead.c:328-423`，`src/src/action_func.c:19-40`，`src/src/log_export.c:69-73`。

<a id="analysis-2"></a>

## 2. 构建边界与目录职责

| 部分 | 当前源码作用与核对点 |
| --- | --- |
| `apps/CMakeLists.txt` | `WITH_WEB_APP` 默认 `ON`，开启时 `add_subdirectory(web_app)`；它是仓库级开关，不是运行时开关。 |
| `CMakeLists.txt` | 项目版本 `2.8`、C99；`go` 共享库由 GoAhead 通用源码加选中的 TLS 适配层构成；`web_app` 由 `goahead.c`、`action_func.c`、`log_export.c` 构成并链接 `go`；`gopass` 也链接 `go`。 |
| `src/src/` | GoAhead 网络、路由、认证、文件、上传等核心及项目扩展。`action_func.c` 和 `log_export.c` 属于 `web_app` 主程序，不在 `go` 库的源文件列表中；`conf.c`、`sany_list.c`、`cJSON.c`、`opt_iniparser` 列在 `go` 库中。 |
| `webserver/` | 运行脚本、实际安装的 `route.txt`、`auth.txt`、静态 `html/` 及可选证书。`src/src/route.txt` 和 `src/src/web/` 是另一套随 GoAhead 源码提供的内容；CMake 安装的是 `webserver/` 下的文件。 |
| `docs/` | `URD`、`SRS`、`HLD`、`DDD` 四份日志导出文档，用于理解需求和设计；本文只依据实装代码判断行为。 |

`libgo.so` 中的 GoAhead 通用模块可按调用职责定位：`http.c/socket.c` 管 HTTP 与套接字；`route.c/auth.c` 管路由与用户认证；`action.c` 管业务 action 名到函数的映射；`file.c/fs.c` 管静态文件与文件系统；`upload.c` 管 multipart 上传；`cgi.c` 管 CGI；`js.c/jst.c` 管 GoAhead 内置脚本/模板；`runtime.c/osdep.c` 管运行时与系统适配；`mbedtls/` 或 `goahead-openssl/` 按 CMake 选项提供 TLS 适配。它们被编译进库，不表示当前 `webserver/route.txt` 对每种处理器都配置了入口。依据：`CMakeLists.txt:56-120`，`src/src/http.c:229-286`，`src/src/action.c:22-98`，`webserver/route.txt:1-7`。

### 2.1 当前构建单元逐项清单

下表按 `CMakeLists.txt` 的**实际源文件列表**核对；“编入”表示参加编译，“当前入口”表示本项目的启动和路由会走到相应处理器。两者不是一回事。

| 编译目标 | 源文件 | 代码职责及当前入口 |
| --- | --- | --- |
| `libgo.so` | `action.c`、`route.c`、`auth.c` | action 分发表、路由匹配、认证/会话；当前路由和登录均使用。 |
| `libgo.so` | `http.c`、`socket.c` | HTTP 请求解析、事件循环、监听和套接字；主服务必经。 |
| `libgo.so` | `file.c`、`fs.c`、`upload.c` | 静态页面/文件系统和 multipart 上传；当前页面与上传 action 使用。 |
| `libgo.so` | `cgi.c`、`js.c`、`jst.c`、`options.c` | CGI、GoAhead 内置脚本与模板、OPTIONS/TRACE 处理器；编译并初始化了相关处理器，但当前 `webserver/route.txt` 没有为 CGI/JST 等配置明确入口。 |
| `libgo.so` | `alloc.c`、`crypt.c`、`runtime.c`、`osdep.c`、`time.c`、`rom.c` | GoAhead 内存、加密/口令、运行时、系统适配、时间与 ROM 资源支持；属于核心底层能力，不单独形成 `web_app` 业务 action。 |
| `libgo.so` | `sany_list.c`、`cjson/cJSON.c`、`conf.c`、`opt_iniparser/dictionary.c`、`opt_iniparser/iniparser.c` | 项目列表工具、JSON、INI 配置读取和解析；其中记录、配置、日志 JSON 及设备 ID 查询会调用相应能力。 |
| `libgo.so` | `mbedtls/mbedtls.c` + `goahead-mbedtls/goahead-mbedtls.c`，或 `goahead-openssl/goahead-openssl.c` | 根据 CMake TLS 开关二选一；默认是 mbedTLS，当前脚本仍只监听 HTTP。 |
| `web_app` | `goahead.c`、`action_func.c`、`log_export.c` | 服务入口和业务 action 的全部 C 实装。 |
| `gopass` | `utils/gopass.c` | 独立命令行密码工具；可输出密码散列，传 `--file` 时读写认证文件，不由服务进程自动调用。 |

`src/src/Untitled-1.c` 是带说明文字的下载功能草稿，`utils/webcomp.c` 是生成 ROM 页面源码的独立工具；二者都不在当前 CMake 目标源文件列表中。`src/test/`、`src/doc/`、`src/paks/` 也未由本目录当前 CMake 目标直接编译。`webserver/html/js/generatefilelistbydefault.js` 含示例列表数据，`FileList.html` 中的引用被注释；不能将这些样例作为运行时设备文件。依据：`CMakeLists.txt:56-176`，`src/src/Untitled-1.c:1-20`，`src/src/utils/webcomp.c:1-18`，`webserver/html/FileList.html:106-110`。

CMake 的 TLS 开关默认 mbedTLS 开、OpenSSL 关，二者同时开启会直接报错；选 OpenSSL 时 `find_package(openssl "1.1.1" REQUIRED)`。交叉编译按 `QL_MODULE_PLATFORM` 分支配置宏、头文件和平台库，识别 EC200A、EG25G、MCIMX6Y2CVM08AB、AG35/AG35GL、RK3506；不匹配时报错。`web_app`、`gopass` 的 RPATH 为 `$ORIGIN`，与同目录 `libgo.so` 配套。安装规则目标为 CMake 安装前缀下的 `usrdata/webserver`，设备启动脚本则使用绝对路径 `/usrdata/webserver`；安装前缀需由上层构建决定。依据：`CMakeLists.txt:10-48,82-199`，`apps/CMakeLists.txt:124-127`。

仓库本目录的 `README.md` 把安装目录写成 `userdata/webserver`，但当前 CMake 安装规则和启动脚本均为 `usrdata/webserver`；本文采用可执行规则中的拼写。`src/test/` 是随 GoAhead 源码存在的测试目录，当前本项目 `CMakeLists.txt` 没有把它加入构建或定义专用测试目标。依据：`README.md:67-69`，`CMakeLists.txt:195-199`，`webserver/start_web.sh:3`。

**版本区分**：CMake `project(web_app VERSION 2.8)` 是本项目版本；GoAhead 配置头 `src/projects/goahead-linux-default-me.h:280-281` 中 `ME_VERSION` 为 `5.2.0`，`--version` 打印的是 `ME_VERSION`（`src/src/goahead.c:308-312`），两者不能混写。

<a id="analysis-3"></a>

## 3. 启动、请求分发与认证

### 3.1 启动逻辑流程图

```text
执行 start_web.sh
    |
    v
cd /usrdata/webserver --> killall -9 web_app
    |
    v
每 30 秒检查进程 ----------------------+
    | 已运行或程序文件不存在            |
    +-----------------------------------+
    |
    | 未运行且程序文件存在
    v
后台启动 web_app（传认证文件、记录模式、大小、日志、网页目录、HTTP 端点）
    |                                   |
    | 子进程                             +--> 脚本继续每 30 秒检查
    v
解析参数 --> websOpen 初始化 GoAhead 并加载 route.txt
    |
    +-- 失败 --> 返回错误
    |
    +-- 成功 --> 加载 auth.txt --> 监听端点
                  --> 注册 14 个业务 action --> 初始化日志任务表
                  --> websServiceEvents 处理请求
                  --> 收到终止信号 --> websClose
```

脚本先强制结束同名进程，再进入轮询；`killall` 的范围是进程名。脚本设定 `record_mode=dated`、`--max-export-size 50`、`stdout:2`、HTTP 80 端口，未传 `--route`，所以主程序用当前工作目录下的 `route.txt`。主程序默认记录模式与上限也分别是 `dated`、50 MB；`--max-export-size` 只接受 20–100。`websOpen` 完成核心、文件、上传、认证、路由等初始化；`websListen` 后注册业务 action，事件循环处理请求。当前退出路径调用 `websClose()`，没有显式调用 `log_export_deinit()`。依据：`webserver/start_web.sh:3-31`，`src/src/goahead.c:39-40,225-425`，`src/src/http.c:229-286`。

### 3.2 路由与认证实际位置

`webserver/route.txt` 先声明 `/css`、`/index.html`，再声明登录 POST、登出 GET/POST、`/` 的 `auth=form` 继续路由、`/action` 处理器和 `/` 静态路由。GoAhead 的 `websOpenAuth()` 先在同一 action 表中注册内置 `login`、`logout` 两个处理函数，项目 `goahead.c` 又注册 14 个业务 action；下节接口表只统计这 14 个业务项。登录处理函数从表单读取 `username/password`，验证后建立会话并按路由重定向；登出会销毁会话。GoAhead 的 `actionHandler` 从路径中取 action 名，在 `actionTable` 查找已注册 C 函数；未注册时返回 404。认证用户和角色由 `webserver/auth.txt` 加载，GoAhead 配置头启用 `ME_GOAHEAD_AUTH=1`。当前启动脚本使用 HTTP，因此该链路本身未启用 TLS；CMake 编入 TLS 库不等于脚本监听 HTTPS。依据：`webserver/route.txt:1-7`，`src/src/http.c:229-286`，`src/src/auth.c:145-166,482-559`，`src/src/action.c:22-98`，`src/src/goahead.c:328-419`，`src/projects/goahead-linux-default-me.h:93-97`。

一个请求的核心调用链为 `websServiceEvents()` 轮询 socket → `websAccept()` 建请求对象 → `websPump()` 解析请求/请求体 → `websRouteRequest()` 按路由匹配并执行认证检查 → `websRunRequest()` 注入查询/表单变量并调用匹配的处理器。静态页面走 `file` 处理器，`/action/...` 走 `action` 处理器；其后再由 action 名查找内置登录函数或本项目业务函数。这个流程只描述当前 GoAhead 的主要请求路径，上传体解析和后台写回还有各自的内部状态分支。依据：`src/src/http.c:739-800,930-960,1541-1564`，`src/src/route.c:41-156`，`src/src/action.c:22-98`，`src/src/file.c:187-191`。

<a id="analysis-4"></a>

## 4. 浏览器页面与端到端功能

`index.html` 是表单登录页，成功按路由重定向到 `home.html`。`home.html` 加载 jQuery、`i18n.js`、`tablepage.js`、`exportFile.js`、`importFile.js`、`queryByDate.js`、`logExport.js`；主页实际导航只显示按日期查询与日志导出，默认初始化日期查询并尝试从 `sessionStorage` 恢复时间范围和列表。`exportFile.js`/`importFile.js` 的函数仍可由源码调用，但对应主页导航被注释。`webserver/html/` 中其他模板页是已安装的静态资产，不能仅凭文件存在推断它们是主页当前入口。依据：`webserver/html/index.html:49-75`，`webserver/html/home.html:262-265,355-380`。

界面中文/英文文本由 `i18n.js` 的词条与 `data-i18n` 属性更新；列表由前端拼接表格，`tablepage.js` 按每页 50 行操作 DOM 行的显示状态，不请求服务端分页。`queryByDate.js` 和 `logExport.js` 各自维护勾选大小、进度条和 `sessionStorage` 查询状态；`sessionStorage` 只保存在当前浏览器会话，不是设备端持久状态。依据：`webserver/html/js/i18n.js:1-135`，`webserver/html/js/tablepage.js:1-120`，`webserver/html/js/queryByDate.js:51-112,191-225`，`webserver/html/js/logExport.js:82-142,225-265`。

### 4.1 当前 action 与输入输出

除 GoAhead 在 `auth.c` 注册的登录/登出外，下列 14 个 action 由 `goahead.c` 注册。大部分业务响应以 HTTP 200 返回 JSON 或 `OK/ERROR` 字符串；调用方不能只看 HTTP 状态判断业务成功。

| action | 主要输入 | 当前处理与输出 | 实现位置 |
| --- | --- | --- | --- |
| `getFileList` | 无 | 按记录模式列出记录；成功返回 `dirPath,fileList`，配置/目录失败时可能没有 JSON 响应体。 | `goahead.c:84-120` |
| `getFileListByDate` | `start_time,end_time`（秒） | 按记录模式和时间范围查询；失败时构造空 `fileList`，追加 `dev_id`。 | `action_func.c:1699-1762` |
| `getCfgFileList` | 无 | 扫描 `/opt/` 的文件名包含 `.ini` 或 `.json` 的普通文件；返回 `dirPath,fileList`。 | `goahead.c:123-148`、`action_func.c:481-577` |
| `getFile` | `Filename` | 用请求指定的路径打开文件，返回附件响应；由记录/日志单文件和 ZIP 下载共用。 | `goahead.c:151-162`、`action_func.c:225-357` |
| `uploadFile` | multipart 文件 | 将 GoAhead 上传临时文件重命名至文档根目录的 `tmp/`，文件名含 `.ini`/`.json` 时再调用 `mv` 移至 `/opt/`；返回 `OK/ERROR`。 | `goahead.c:164-218` |
| `delFile` | 原始请求体路径 | 若 `stat` 为普通文件，调用 `rm -rf`；返回 `OK/ERROR`。 | `goahead.c:47-82` |
| `batchDownload` | `dirPath,files,totalSize,zipName` | 创建记录 ZIP 后台线程，先回 `taskId`，或回 `error`。 | `action_func.c:1911-2037` |
| `getBatchDownloadProgress` | `taskId` | 回 `progress,status`，成功时带 `zipFile`，失败可带 `error`。 | `action_func.c:2056-2112` |
| `getExportConfig` | 无 | 回 `maxExportSizeMB`。 | `action_func.c:2039-2054` |
| `cleanupBatchFile` | `filePath` | 路径前缀为 `/media/sdcard/` 时调用 `unlink`，回 `status` 或 `error`。 | `action_func.c:2114-2139` |
| `getLogList` | `dirType,start_time,end_time` | 扫描 `mem/sd` 对应目录的 `.log` 文件，回 `dirPath,dirType,fileList,truncated` 或错误。 | `log_export.c:190-342` |
| `startLogExport` | `dirType,files,zipName` | 校验所选文件名并启动日志 ZIP 线程，回 `taskId` 或 `error`。 | `log_export.c:513-674` |
| `getLogExportProgress` | `taskId` | 回 `progress,status`，成功时带 `zipFile`。 | `log_export.c:676-734` |
| `cleanupLogExportFile` | `filePath` | 仅接受两个日志根目录下、文件名字符合限制的路径并调用 `unlink`。 | `log_export.c:736-793` |

`fileList` 项常见字段为 `fileName`、`subDir`、`date`、`size`；记录项的 `size` 以 JSON 数字输出，日志项的 `size` 以字符串输出。部分模式还返回 `start_time/end_time`、`totalCount`、`truncated`；不要依赖所有模式字段完全一致。记录和日志 action 对错误有的返回空表、有的返回 `error`，前端应分别处理。依据：`src/src/action_func.c:494-577,1020-1224,1401-1468,1621-1687,1699-1762`，`src/src/log_export.c:190-342`。

### 4.2 记录查询的逻辑流程图

```text
[1] queryByDate.js 读取本地起止时间
    ↓
[2] 转为 Unix 秒并校验，POST /action/getFileListByDate
    ↓
[3] 后端按 g_record_mode 选择一条路径：
    ├─ dated    → 读 conf.ini 的设备 ID → 遍历日期子目录
    │             → 解析文件名本地时间 → 按范围筛选
    ├─ flat-can → 读 conf.ini 的设备 ID → 遍历平铺文件
    │             → 解析 14 位本地时间；失败则用 mtime
    └─ erk      → 扫描 erk-root/raw-data
                  → 解析文件名 UTC 起止时间 → 按区间相交筛选
    ↓
[4] 返回 dirPath、fileList 等 JSON
    ↓
[5] 前端保存 sessionStorage 并渲染列表
    ├─ 点文件   → getFile 下载
    └─ 勾选批量 → 转入 4.4 导出流程
```

`dated` 目录名支持 `YYYYMMDD` 和 `YYMMDD`（两位年按 20YY 解释），范围查询先选日期目录，再解析文件名中的时间并按设备本地时区 `mktime` 比较；上限 2000 条。普通 `getFileList` 路径则扫描最多 500 条。`erk` 仅接收 `OM_MODBUS_` 或 `OM_CAN_` 开头且含 `_UTC_` 起止时间的 ZIP 名称，按 UTC 时间区间与查询区间相交判断，展示时转设备本地时间；`flat-can` 在文件名中寻找 14 位时间戳，找不到则用文件修改时间，两者范围查询也最多展示 2000 条，普通列表最多 500 条。依据：`src/src/action_func.c:19-40,675-725,874-939,1020-1224,1230-1687`。

`getFileListByDateAction` 将请求秒值用 `atoi` 转换；起始值大于结束值或查询准备失败时回空数组。前端从 `datetime-local` 构造秒值并验证区间，在 `sessionStorage` 保存查询状态，列表项中 `dirPath/subDir/fileName` 组合成下载路径。三种模式对 `totalCount/truncated` 的计算方式并不完全相同，尤其 `dated` 到 2000 条即停止继续扫描，不能把 `totalCount` 当成全盘文件总数。依据：`src/src/action_func.c:1020-1224,1699-1762`，`webserver/html/js/queryByDate.js:51-225,394-416`。

`action_func.c` 另有按单日列文件的 `get_file_list_by_date()`，`conf.c` 有 `log_level_parse()`、`machine_no_parse()`，`log_export.c` 有 `log_export_deinit()`；这些函数在本目录当前业务入口中没有调用点。它们属于源码能力，不能直接列成正在运行的页面流程。依据：`src/src/action_func.c:941-1018`，`src/src/conf.c:88-138`，`src/src/log_export.c:813-826`，以及 `src/src/goahead.c:404-425` 的实际注册和主循环。

### 4.3 日志查询与单文件下载

`logExport.js` 让用户选 `mem` 或 `sd` 并输入本地时间，转为秒值提交 `getLogList`。后端将 `mem` 固定映射到 `/tmp/rtms_log/`、`sd` 固定映射到 `/media/sdcard/rtms_log/`，仅考虑后缀为 `.log` 的目录项。时间优先解析 `<前缀>_YYYY-MM-DD_HHMMSS.log`，失败用 `stat` 修改时间；先筛选后保存最多 2000 项，再按所用时间降序排序。响应展示日期却再次读取文件修改时间，因此“筛选/排序时间”和“显示时间”可能不同。目录打不开时当前实现记录日志并返回空列表，而非 HTTP 错误。单文件下载仍调用公共 `getFile`，将所选根目录和文件名拼为 `Filename`。依据：`src/src/log_export.c:69-73,146-342`，`webserver/html/js/logExport.js:160-265,373-402`。

### 4.4 两套批量导出的共同流程与差异

```text
[1] 页面勾选文件，按最大大小分批
    ↓
[2] 选择导出类型：
    ├─ 记录 → POST batchDownload（dirPath/files/totalSize/zipName）
    │        → 记录任务表（最多 16 项）→ 后台线程逐个调用 zip
    │        → 轮询 getBatchDownloadProgress
    └─ 日志 → POST startLogExport（dirType/files/zipName）
             → 日志任务表（最多 8 项）→ 后台线程逐个调用 zip
             → 轮询 getLogExportProgress
    ↓
[3] 检查轮询响应：
    ├─ status=0     → 等待并继续轮询当前任务
    ├─ status=-1    → 显示 error，结束当前批次
    └─ status=1 且有 zipFile → getFile 触发 ZIP 下载
                              → 处理下一批
                              → 全部批次结束后调用对应 cleanup action
```

记录页面在 `home.html` 初始化时调用 `getExportConfig` 读取最大批量大小，默认 50 MB；日志模块也定义了 `fetchExportConfig()`，但当前 `logExport.js` 中没有调用点，因此日志前端实际使用写定的 50 MB。勾选文件后，前端按列表中的大小拆批，超出单批上限的单文件被跳过。ZIP 任务创建后每 500 ms 轮询进度，完成时通过 `getFile` 触发浏览器下载，依次处理下一批，最后请求清理。记录模块从 `queryByDate.js:445-727` 实现，日志模块从 `logExport.js:394-619` 实现。页面触发下载与随后发清理请求之间没有等待浏览器明确确认保存成功的回执，因此“下载完成”的界面文案只表示代码已触发下载动作。

| 项目 | 记录批量导出 | 日志批量导出 |
| --- | --- | --- |
| 请求目录 | 客户端传 `dirPath`，后台线程在该目录执行 `zip`。 | 服务端仅接受 `mem/sd`，映射为两个固定日志根目录。 |
| 所选文件 | `files` JSON 数组或含 `files` 的对象，被拼成空格分隔字符串。 | 同样解析数组或对象，但逐个用 `log_export_path_valid()` 校验文件名，只允许字母、数字、点、下划线、连字符，且不允许 `..`。 |
| 体积限制 | 服务端比较客户端提交的 `totalSize` 与 `g_max_export_size_mb`，不重新 `stat` 文件求和。 | 前端按写定的 50 MB 分批；`startLogExportAction` 没有服务端文件大小总量校验。 |
| 任务标识与槽位 | `taskId` 为秒级时间戳字符串；静态数组最多 16 项；已完成任务超过 300 秒时，在新建任务过程中清理。 | `logexp_<秒级时间>_<自增序号>`；静态数组最多 8 项；完成或失败且线程结束后超过 1 小时才可复用。 |
| ZIP 输出 | `/media/sdcard/<zipName>.zip`，缺省用 `batch_done_<taskId>.zip`。 | 所选日志根目录下的 `<zipName>.zip`，缺省用 `logexp_<taskId>.zip`。 |
| 成功判定 | 逐文件执行 `zip`；任意一次命令返回 0 即把任务标为成功，故可能产生仅含部分所选文件的 ZIP。 | 逐文件执行 `zip`；同样以至少一次成功作为任务成功条件。 |
| 清理 | `cleanupBatchFile` 只检查 `/media/sdcard/` 字符串前缀即 `unlink`。 | `cleanupLogExportFile` 检查固定日志根目录、剩余文件名字符集及 `..` 后 `unlink`。 |

依据：`src/src/action_func.c:44-131,1764-2139`，`src/src/log_export.c:25-73,104-143,345-826`。两套任务状态约定均为 `0` 处理中、`1` 完成、`-1` 失败；状态与进度保存在进程内存，重启后不能继续查旧任务。日志模块启动时清空任务表，`log_export_deinit()` 虽已实现，当前主程序退出路径未调用。依据：`src/src/goahead.c:421-425`，`src/src/log_export.c:34-45,795-826`。

### 4.5 配置文件处理与文件下载

配置列表固定扫描 `/opt/`，仅在目录项类型为普通文件且文件名**包含** `.ini` 或 `.json` 时加入列表；过滤不是“严格以扩展名结尾”。上传路径是 GoAhead 临时上传文件 → `html/tmp/<客户端文件名>` → shell `mv` 到 `/opt/<客户端文件名>`。GoAhead 的 `upload.c` 会先规范化并拒绝包含路径分隔符等特定字符的客户端文件名；业务层对 `.ini`/`.json` 仍只采用子串检查，且 `uploadFileAction` 没有检查 `mv` 的退出状态就设置成功返回。当前仓库的 `webserver/` 与 `webserver/html/` 都没有 `tmp/` 目录，启动脚本和本目录 CMake 也未创建它们；设备上是否由其他部署步骤提供，需实际检查。`getFile` 使用 `Filename` 设置 `wp->filename/wp->path`，GoAhead 页面文件接口打开并以 `Content-Disposition: attachment` 回传；下载对象不限于配置文件。`delFile` 从 `wp->input.servp` 取原始请求体，`stat` 为普通文件后通过 shell 删除。依据：`src/src/upload.c:230-259,465-477`，`src/src/goahead.c:47-218`，`src/src/action_func.c:225-357,481-577`。

<a id="analysis-5"></a>

## 5. 数据路径、配置与外部依赖

| 路径或对象 | 读写方式与条件 | 证据 |
| --- | --- | --- |
| `/usrdata/webserver`、`html/` | 启动脚本的工作目录、网页目录；脚本内写定服务端点、记录模式和日志级别。 | `webserver/start_web.sh:3-31` |
| `route.txt`、`auth.txt` | 工作目录中的路由；脚本用 `-a` 明确传认证文件。 | `src/src/goahead.c:232-337` |
| `/opt/conf.ini` | `conf_parse` 读取 `dev:id` 与 `dev:secret`；`dated/flat-can` 查询在读取失败时无法构造记录根目录。 | `src/src/conf.h:69`、`src/src/conf.c:55-85` |
| `/opt/log_cfg.ini`、`/opt/machine_info.ini` | `conf.c` 另有日志等级和机器编号解析函数；当前 `goahead.c` 的主请求流程没有直接调用它们，不据此推断启动必需。 | `src/src/conf.h:70-72`、`src/src/conf.c:88-138` |
| `/opt/` | 配置列表与上传目标；实际内容来自设备环境。 | `src/src/goahead.c:140,192-202` |
| `/media/sdcard/can_data_<dev_id>/` | `dated` 使用日期子目录、`flat-can` 使用平铺文件；两者是互斥的运行参数分支。 | `src/src/action_func.c:19-40` |
| `/media/sdcard/erk-root/raw-data/` | `erk` 平铺记录目录；不读取 `/opt/conf.ini` 获取设备 ID。 | `src/src/action_func.c:26-40`、`src/src/goahead.c:95-120` |
| `/media/sdcard/` | 记录批量 ZIP 输出；目录必须可写且设备有 `zip` 命令。 | `src/src/action_func.c:1819-1909` |
| `/tmp/rtms_log/`、`/media/sdcard/rtms_log/` | 日志读取和对应 ZIP 输出目录；`mem/sd` 由后端固定映射。 | `src/src/log_export.c:69-73,428-432` |

GoAhead 配置头还定义上传开关和 `tmp` 上传目录，并把 POST/PUT/UPLOAD 上限设为 204800000 字节；这属于 GoAhead 请求层容量常量，不等于业务 ZIP 的 20–100 MB 上限。静态页面依赖项目内的 JS/CSS 资源；`index.html` 还引用 Google Fonts 地址，断网环境下字体加载结果取决于浏览器网络，业务页面本身是本地静态文件。依据：`src/projects/goahead-linux-default-me.h:153-174,238-243`，`webserver/html/index.html:8-18`。

<a id="analysis-6"></a>

## 6. 源码可证实的限制和维护风险

以下是按当前源码得出的观察。它们不是已在设备上复现的故障报告；实际可触达性还受 GoAhead 路由、认证、文件权限和部署方式影响。

| 位置 | 代码现状 | 对维护或验证的含义 |
| --- | --- | --- |
| `getFile` 与 `delFile` | 前者直接用请求 `Filename` 设置文件路径；后者使用原始请求体作为待删路径，并拼入 `rm -rf` 命令；两个业务函数都未将路径限定到记录或配置根目录。 | 检查服务端可访问的路径范围、登录权限和输入校验，不能用“主页没有删除按钮”代表接口不可调用。依据：`src/src/goahead.c:47-82,151-162`，`src/src/action_func.c:225-300`。 |
| `getFile` action 函数签名 | `getFileAction` 定义为三个参数，但当前 GoAhead 配置 `ME_GOAHEAD_LEGACY=0`，`actionHandler` 按一个 `Webs *` 参数调用注册函数；函数体只使用第一个参数。 | 这是当前源码的函数签名不一致；修改此入口时应按当前 GoAhead 回调类型核对，不能依靠未使用的附加参数。依据：`src/src/goahead.c:151-162`，`src/src/action.c:45-68`，`src/projects/goahead-linux-default-me.h:123-124`。 |
| `uploadFile` | GoAhead 上传层拒绝路径分隔符等特定文件名字符；业务层仅用 `strstr` 判断包含 `.ini`/`.json`。上传需要工作目录的 `tmp/` 和网页目录的 `tmp/`；源码安装内容未提供它们。`mv` 通过 `system()` 执行且退出状态不参与 `OK` 判定。 | 先检查设备上的两个临时目录及权限，再确认 `/opt/` 是否真正落盘；现有返回 `OK` 不保证移动成功。依据：`src/src/upload.c:230-259,465-477`，`src/src/goahead.c:164-218`，`CMakeLists.txt:195-199`。 |
| 记录批量导出 | `dirPath`、`files`、`zipName` 参与 shell 命令；服务端只拿客户端传的 `totalSize` 与上限比较，没有对文件逐个重算。`taskId` 仅由秒级时间组成。 | 路径和 ZIP 名称需要服务端约束；同一秒创建多个任务时存在相同 ID 的可能，任务表的行为需实测。依据：`src/src/action_func.c:1764-2037`。 |
| 日志批量导出 | 所选日志文件名经 `log_export_path_valid()` 校验，但请求的 `zipName` 被复制到输出路径并传入 shell 命令，没有同等级字符校验；任务启动不复核总大小。 | 不应把文件名白名单等同于整个导出请求都已校验；核对 ZIP 名称和导出体积边界。依据：`src/src/log_export.c:104-143,428-479,513-674`。 |
| 导出完成 | 两套后台线程均采用“至少一个 `zip` 命令成功”作为总体成功条件；前端随后发起下载及清理，没有等待浏览器确认文件已保存。 | 成功状态可能对应部分文件被打包；要验证 ZIP 条目和下载时序。依据：`src/src/action_func.c:1870-1908`，`src/src/log_export.c:467-505`，`webserver/html/js/queryByDate.js:685-727`，`webserver/html/js/logExport.js:540-585`。 |
| 记录 ZIP 清理 | `cleanupBatchFile` 只做 `/media/sdcard/` 前缀比较，然后 `unlink`；没有检查路径规范化结果或文件是否为当前任务产物。 | 此接口的可删除范围取决于进程权限及请求路径；路径校验需要按实际权限模型审查。依据：`src/src/action_func.c:2114-2139`。 |
| 文件列表与页面渲染 | 后端文件名和路径直接进入多处 `innerHTML`/拼接的 HTML 或内联 `onclick`；日志文件名和记录文件名来自设备目录。 | 若设备目录中出现特殊字符文件名，页面展示/点击行为可能异常；需要对输出编码作专项验证。依据：`webserver/html/js/queryByDate.js:191-225`，`webserver/html/js/logExport.js:225-265`。 |
| 查询数量和时间 | `dated` 范围查询达到 2000 条就停止，`erk/flat-can` 扫描全目录后最多输出 2000 条；日志列表先存最多 2000 个再排序；日志按文件名时间筛选，但列表显示修改时间。 | “最新 2000 条”和 `totalCount` 的语义并非各模式严格一致；不同记录布局和时区应分别验证。依据：`src/src/action_func.c:1020-1224,1401-1468,1621-1687`，`src/src/log_export.c:190-342`。 |
| 会话与配置 | 任务状态仅在进程内存；`start_web.sh` 启动前强制结束同名进程；`dated/flat-can` 每次查询调用 `conf_parse`，依赖设备上的 `/opt/conf.ini`。 | 进程重启后旧任务 ID 不可继续查询；缺配置时记录查询会走失败/空表路径。依据：`webserver/start_web.sh:17-31`，`src/src/goahead.c:95-120`，`src/src/action_func.c:1699-1762`。 |

这些风险的优先级应由部署环境决定。本文只记录可从实现直接确认的输入来源、判断条件和输出行为，不以设计文档承诺或界面隐藏状态替代服务端事实。

<a id="analysis-7"></a>

## 7. 模块关系与修改索引

| 要修改的行为 | 需要联查的源码 |
| --- | --- |
| 启动地址、脚本守护、工作目录 | `webserver/start_web.sh`；`src/src/goahead.c` 参数解析；`webserver/route.txt`。 |
| 登录和权限 | `webserver/route.txt`、`webserver/auth.txt`；GoAhead `src/src/auth.c`、`route.c`、`http.c`；页面 `index.html`。 |
| 记录模式或设备 ID | `src/src/goahead.c` 的 `--record-mode`；`src/src/action_func.c/.h` 的三套扫描/解析；`src/src/conf.c/.h`；`queryByDate.js` 的路径和展示。 |
| 记录批量导出 | `queryByDate.js` 分批、命名、轮询；`action_func.c` 文件列表解析、任务表、ZIP 线程、清理；`getFile` 下载实现。 |
| 日志导出 | `logExport.js`；`log_export.c/.h` 的目录映射、时间解析、任务和路径守卫；`goahead.c` 注册；`CMakeLists.txt` 构建目标。 |
| 配置文件导入/下载 | `importFile.js`、`exportFile.js`、`home.html` 导航；`goahead.c` 上传/删除/列表；`action_func.c` 下载与文件列表。 |
| 打包与部署 | 顶层 `apps/CMakeLists.txt`、本目录 `CMakeLists.txt`、`webserver/` 安装内容、目标工具链与 `QL_MODULE_PLATFORM`。 |

<a id="analysis-8"></a>

## 8. 设备异常时的排查手册

本节用于判断故障是否发生在 `web_app`、页面、设备配置、存储或外部命令上。命令示例均以**只读检查**为主，在目标设备 shell 执行；设备工具集可能是 BusyBox，某个命令不存在时用同类工具替代。先保留故障现场，再决定是否重启。`start_web.sh` 一开始执行 `killall -9 web_app`，直接重跑它会终止当前服务并清空内存中的导出任务，不能当作无影响的检查命令。依据：`webserver/start_web.sh:17-31`，`src/src/action_func.c:44-51`，`src/src/log_export.c:34-67`。

### 8.1 从现象定位到层次

```text
浏览器打不开页面
  -> 先查设备 IP/端口和 web_app 进程
  -> 再查二进制、libgo.so、route.txt、auth.txt、html/ 是否齐全

页面能打开，但登录或功能异常
  -> 浏览器开发者工具查看请求 URL、状态码、响应体
  -> 401/登录失败：查认证与会话
  -> 404：区分静态文件不存在、action 未注册或路由不匹配
  -> HTTP 200 但 error/空列表：查对应业务 action 与设备目录

文件列表正常，但导出/下载失败
  -> 记录 taskId、轮询响应和 zipFile
  -> 查 zip 命令、目标目录权限/剩余空间、ZIP 是否生成
  -> 查 getFile 返回与前端清理时序
```

先用**首次失败的请求**定位层次：静态页面失败多在监听/部署；登录失败查路由/认证；查询为空查记录模式、配置、目录与时间；打包失败查任务、`zip` 和存储；下载失败查 `getFile` 的实际路径。不要先删除文件、重启服务或清理 ZIP，否则可能丢失任务状态和证据。

### 8.2 首轮只读检查

在设备上记录检查时间、设备 IP 与运行参数。以下命令中的 `<PID>` 应替换为实际进程号；不需要输入密码或复制 `auth.txt` 内容。

```sh
date
uptime
ps w | grep '[w]eb_app'
ls -ld /usrdata/webserver /usrdata/webserver/html
ls -l /usrdata/webserver/web_app /usrdata/webserver/libgo.so
ls -l /usrdata/webserver/route.txt /usrdata/webserver/auth.txt
ls -ld /usrdata/webserver/tmp /usrdata/webserver/html/tmp
df -h /media/sdcard /tmp /opt
command -v zip
```

`ls`/`df` 对不存在的目录报错本身就是线索。用 `ps` 获取 PID 后，可按设备提供的命令查看启动参数和当前工作目录：

```sh
tr '\000' ' ' < /proc/<PID>/cmdline
readlink /proc/<PID>/cwd
readlink /proc/<PID>/fd/1
readlink /proc/<PID>/fd/2
```

启动参数能确认实际 `--record-mode`、`--max-export-size`、网页目录和监听地址；工作目录决定相对的 `route.txt` 与 GoAhead 上传暂存 `tmp/` 路径。`fd/1`、`fd/2` 用来定位标准输出和错误输出流。脚本当前传 `--log stdout:2`，源码不保证有固定的 `/var/log/web_app.log`；如果输出连到 pipe、终端或进程管理器，应从对应日志收集。启动日志中的 `Version`、`App Version`、`Directory`、`Documents` 可帮助确认实际运行程序。依据：`webserver/start_web.sh:3-31`，`src/src/goahead.c:232-337,434-448`。

网络只读检查可用设备具备的工具执行其一：`ip addr`、`ss -lnt` 或 `netstat -lnt`；从能访问设备的主机请求 `http://192.168.1.1/index.html`。例如主机安装了 `curl` 时：

```sh
curl -i --max-time 5 http://192.168.1.1/index.html
```

`curl` 只是示例工具，目标设备未必安装。连接失败先核对脚本内 IP 是否属于设备网卡、端口 80 是否监听，以及主机到设备的网络路径；页面 404 再看 `html/index.html` 与 `route.txt`。**本检查使用 GET 静态登录页，不触发删除、上传或导出。**依据：`webserver/start_web.sh:4-6,29`，`webserver/route.txt:1-7`。

### 8.3 按症状逐项排查

| 现象 | 先看什么 | 对应代码与判断 |
| --- | --- | --- |
| `web_app` 进程不存在或反复重启 | `ps`、程序/库/路由/认证文件是否存在，启动输出是否有 `Cannot initialize server`、`Cannot load`、监听失败；系统日志是否有 OOM/崩溃记录。 | `goahead.c` 解析参数后依次执行 `websOpen`、`websLoad(auth)`、`websListen`，任何一步失败都可能退出；脚本下次轮询才会再启动。依据：`goahead.c:225-365`、`start_web.sh:17-31`。 |
| 进程存在但浏览器无法连接 | 核对 `/proc/<PID>/cmdline` 中端点、`192.168.1.1` 是否配置在设备、端口是否监听；从设备本机和访问端分别测。 | 脚本写定 `http://192.168.1.1:80`；二进制支持其他端点，但以实际进程参数为准。依据：`start_web.sh:4-6,29`、`goahead.c:339-369`。 |
| 登录页正常，登录失败或循环跳回 | 浏览器 Network 中 `/action/login` 的状态/重定向、Cookie；核对实际 `route.txt` 与 `auth.txt` 是否加载、设备时间和会话。 | `auth.c` 的登录函数读表单 `username/password`，校验后创建会话；路由设置 200/401 重定向。不要在故障记录中粘贴密码或 `auth.txt` 的散列。依据：`auth.c:482-559`、`webserver/route.txt:3-5`。 |
| 页面能登录但 JS 功能不显示 | 浏览器 Console 和 Network 里 `queryByDate.js`、`logExport.js`、`i18n.js` 的加载状态；确认 `home.html` 与脚本来自同一次安装。 | 页面入口及脚本引用位于 `home.html:262-265,355-380`；静态资产缺失与后端 action 错误分开判断。 |
| 记录查询空白或提示无文件 | 先取 `getFileListByDate` 的原始响应：是 200 空体、`fileList:[]`、`error` 还是非 200；再查记录模式、`/opt/conf.ini` 可读性、对应目录和时间范围。 | `dated/flat-can` 要 `dev:id` 与 `dev:secret` 成功解析，`erk` 不读该文件。查询准备失败时会构造空列表，所以“无文件”不一定等于目录真实为空。依据：`conf.c:55-85`、`action_func.c:1699-1762`。 |
| 只有部分记录显示或时间不符 | 检查设备 `date` 与时区、目录名和文件名格式、是否命中 2000 条上限；比较文件名时间与 `stat` 修改时间。 | `dated` 使用本地时间解析日期子目录；`erk` 文件名按 UTC 解析；`flat-can` 优先文件名时间、失败用 mtime。三种模式截断方式不同。依据：`action_func.c:1020-1687`。 |
| 日志查询为空 | 核对前端选的是 `mem` 还是 `sd`，对应目录是否存在且可读，文件是否以 `.log` 结尾，查询时间是否覆盖文件名时间或 mtime。 | `getLogList` 目录打不开也返回空列表，日志可能含 `cannot open ... errno=...`；列表最多保留 2000 项。依据：`log_export.c:69-73,190-342`。 |
| 导出开始后报 `Task not found` 或 `Task limit reached` | 记录 `taskId` 与进程 PID 是否变过、上次进程是否重启、是否有多个并发任务；检查任务年龄。 | 任务表在进程内存：记录最多 16 项，日志最多 8 项；记录成功任务过 300 秒后在新任务创建时回收，失败任务当前不会通过这条成功回收路径释放；日志已完成/失败且线程结束超过 1 小时可复用。依据：`action_func.c:44-131`、`log_export.c:25-45,345-369`。 |
| ZIP 导出失败或进度停住 | 检查 `zip` 是否存在、输出目录可写/剩余空间、所选源文件仍存在；保留创建任务响应和轮询 JSON，并查 `Zip failed`、`zip file ... ret=...` 输出。 | 两套线程都逐文件执行系统 `zip`；返回 `status=-1` 或 `error` 是明确失败，`status=0` 是进行中。成功只要求至少一次 `zip` 命令成功，需用 ZIP 条目核对是否包含所有选中文件。依据：`action_func.c:1819-1909`、`log_export.c:401-505`。 |
| 进度 100%，但浏览器没拿到 ZIP | 在前端 Network 查看 `getFile?Filename=...` 状态，核对轮询返回的 `zipFile` 路径当时是否存在、是否已被清理；查看浏览器下载限制。 | 前端在触发下载后继续下一批，全部结束再调用 cleanup；没有浏览器保存成功确认。服务端下载按请求的 `Filename` 打开文件。依据：`queryByDate.js:685-727`、`logExport.js:540-585`、`action_func.c:225-357`。 |
| 配置上传失败或页面报 OK 但 `/opt/` 没文件 | 依次检查 `/usrdata/webserver/tmp`（GoAhead 暂存）、`/usrdata/webserver/html/tmp`（业务重命名）、`/opt/` 是否存在且可写；收集 `Cannot create upload temp file`、`Cannot rename uploaded file` 输出。 | 源码安装内容没创建两个 `tmp/`；业务层调用 `mv` 后不核对返回值，`OK` 不代表最终文件已到 `/opt/`。依据：`upload.c:245-259,465-477`、`goahead.c:164-218`、`CMakeLists.txt:195-199`。 |

如果 ZIP 文件尚在设备上，检查内容时只读取列表，不修改文件：`unzip -l '<实际 ZIP 路径>'`（若设备安装 `unzip`）。`ls -l '<实际 ZIP 路径>'` 可确认文件是否仍在；对于日志 ZIP，输出就在所选 `rtms_log/` 目录；记录 ZIP 在 `/media/sdcard/`。不要用界面“100%”替代 ZIP 条目核验。

### 8.4 根据浏览器响应定位接口

打开浏览器开发者工具的 Network，保留**首次失败**的请求，记录 URL、方法、状态码、耗时、请求参数名、响应体和登录后的跳转，但不要记录认证密码、Cookie 值或设备密钥。建议按下面顺序看：

1. `/index.html`：无法取得页面，先查进程、IP、端口、路由、静态文件。
2. `/action/login`：401 或重定向异常，先查 `auth.txt`、路由与会话；不把 401 当作 ZIP 故障。
3. `/action/getFileListByDate` 或 `/action/getLogList`：HTTP 200 也要检查是否为空数组或 `error`；空响应体会让前端 JSON 解析失败。
4. `batchDownload` 或 `startLogExport`：先看是否拿到 `taskId`；有 `error` 时根据具体文本回查参数、文件列表和任务槽位。
5. 两个进度 action：`status=0` 继续观察、`status=1` 检查 `zipFile`、`status=-1` 读取 `error`；`Task not found` 常见于服务进程重启或任务表已回收，但须用 PID/启动时间证实。
6. `/action/getFile?Filename=...`：导出任务成功而下载失败时，以该请求的状态和服务端文件是否仍存在判定问题在下载路径还是 ZIP 生成。

如果需要区分“浏览器缓存旧前端”和“当前后端”，先比较 `home.html`、`queryByDate.js`、`logExport.js` 在 Network 的实际响应，必要时用新浏览器会话重新打开页面。查询状态缓存在 `sessionStorage`，它可能展示上次列表；用一次新的查询请求核对当前设备状态。依据：`home.html:355-380`，`queryByDate.js:51-112,131-188`，`logExport.js:82-142,160-224`。

### 8.5 日志收集与复现记录

脚本当前把 GoAhead 日志指向 `stdout:2`，很多 `logmsg(9, ...)` 的详细 JSON/逐文件时间比对在级别 2 下不会出现；`printf`/`error` 还可能走不同标准流。先确认 `fd/1`、`fd/2` 指向哪里，再按设备的进程管理方式取日志。必要时在受控环境临时提高 `--log` 级别并做单次复现；参数改动和服务重启会改变现场，须先保存原始证据。系统级崩溃可查看目标设备可用的内核日志，例如 `dmesg`，但不能把缺少日志直接当作程序未出错。

一份可复现的问题记录至少包括：设备平台与系统版本、`web_app --version` 输出、进程完整启动参数、发生时间及设备时区、记录模式或日志目录类型、首次失败请求/响应、对应进程输出、相关目录的存在性和权限、剩余空间、`zip` 可用性、任务 ID/状态变化。分享时遮盖用户名、密码、Cookie、`auth.txt` 散列、`/opt/conf.ini` 中的 `dev:secret` 及可能包含敏感数据的文件内容。依据：`src/src/goahead.c:265-315,434-448`，`src/src/conf.c:55-85`，`src/src/action_func.c:1718-1755`，`src/src/log_export.c:190-342,401-505`。

<a id="analysis-9"></a>

## 9. 核对方法与边界

本文通过静态阅读当前工作树和对照前后端 action 名、CMake 实际编译列表、路由和启动脚本形成；未运行设备程序、未向服务发送请求、未确认目标设备上 `/opt`、SD 卡和内存日志目录的内容，也未据设计文档推断当前代码之外的功能。可复核的主线索集中在：`CMakeLists.txt`、`webserver/start_web.sh`、`webserver/route.txt`、`src/src/goahead.c`、`src/src/action_func.c`、`src/src/log_export.c`、`src/src/conf.c`、`webserver/html/home.html`、`webserver/html/js/queryByDate.js` 和 `webserver/html/js/logExport.js`。若切换 Git 分支或修改这些文件，应重新校对流程图和接口表。
