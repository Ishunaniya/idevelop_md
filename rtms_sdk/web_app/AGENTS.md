# web_app 项目说明

## 项目位置与用途

- Git 仓库根目录：`/home/tronlong/lyp/code/rtms_sdk`；本项目源码目录：`apps/web_app/`；文档存放目录：`/home/tronlong/lyp/perCode/idevelop_md/rtms_sdk/web_app/`。`apps/CMakeLists.txt` 通过默认开启的 `WITH_WEB_APP` 开关将其纳入 SDK 构建。
- 编写本文档时，工作树所在 Git 分支为 `develop/rtms_sdk_v1.3_20240408`（已用 `git branch --show-current` 核对）。它是整个 `rtms_sdk` 仓库的分支，不是 `web_app` 的独立分支。切换分支后，应重新对照源码核对本文档。
- 本项目是基于 GoAhead 的设备 Web 服务：提供登录页面、记录文件按日期查询及下载、批量打包导出、内存或 SD 卡日志导出，以及配置文件的上传与下载。`web_app` 是服务进程；`libgo.so` 是 GoAhead 共享库；`gopass` 是密码工具。
- 本文依据上述分支的源码整理。设备 IP、存储目录、认证是否生效、记录格式及外部命令可用性，需要结合目标设备上的配置和运行环境确认。

### 相关技术与名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| SDK、CMake、`WITH_WEB_APP` | `rtms_sdk` 是承载应用的仓库；CMake 读取各级 `CMakeLists.txt` 生成构建和安装规则；`WITH_WEB_APP` 决定顶层是否编译本目录，默认 `ON`。 |
| GoAhead、`libgo.so` | GoAhead 是嵌入式 HTTP 服务器；本目录的 `src/src/` 包含服务器源码，构建时生成共享库 `libgo.so`，供 `web_app` 和 `gopass` 链接。 |
| HTTP 路由、action | `webserver/route.txt` 将 URL 映射到静态文件或 action 处理器；`goahead.c` 用 `websDefineAction` 把 `/action/...` 名称注册到 C 函数。新增接口时需同时检查路由、注册和页面请求。 |
| 文档目录、工作目录 | 文档目录是 GoAhead 提供网页文件的根目录；工作目录决定 `route.txt` 等相对路径如何解析。启动脚本先进入 `/usrdata/webserver`，再以其 `html/` 子目录作为文档目录。 |
| `auth.txt`、`gopass` | `auth.txt` 定义 GoAhead 的用户、角色和密码数据；`gopass` 用于生成 GoAhead 密码数据。路由中的 `auth=form` 与编译期认证开关、用户配置共同决定实际登录行为。 |
| mbedTLS、OpenSSL、TLS | TLS 是 HTTPS 使用的加密传输层。CMake 默认启用 mbedTLS，可改用 OpenSSL；两个选项不可同时开启。启动脚本当前传入的是 `http://` 监听地址。 |
| cJSON、INI | cJSON 用于处理接口中的 JSON 数据；iniparser 用于读取 `/opt/conf.ini` 等 INI 配置，部分记录目录依赖其中的设备 ID。 |
| 记录布局 `dated`、`erk`、`flat-can` | `--record-mode` 选择记录文件扫描方式：按日期子目录、ERK 原始数据平铺目录、CAN 记录平铺目录；默认 `dated`。它是读取已有文件的布局适配，不负责产生记录文件。 |
| `dirType=mem/sd` | 日志导出接口的目录类型：`mem` 对应 `/tmp/rtms_log/`，`sd` 对应 `/media/sdcard/rtms_log/`。与记录文件的 `--record-mode` 是两个独立概念。 |
| 异步导出任务、ZIP | 批量记录导出与日志导出通过后台线程调用设备上的 `zip` 命令；页面使用任务 ID 查询进度，再下载生成的 ZIP。 |
| 交叉编译、`QL_MODULE_PLATFORM` | 交叉编译是在开发主机上为目标设备生成程序；CMake 按 `QL_MODULE_PLATFORM` 选择目标平台宏、SDK 头文件及链接库。 |

## 主要文件

| 路径 | 职责 |
| --- | --- |
| `CMakeLists.txt`、`global_config.h.in` | 定义项目版本、TLS 选项、平台依赖、三个构建目标与安装内容。 |
| `src/src/goahead.c` | 命令行解析、GoAhead 初始化与监听、action 注册及事件循环。 |
| `src/src/action_func.c/.h` | 配置文件、记录文件列表与按日期查询、下载和批量打包导出。 |
| `src/src/log_export.c/.h` | 内存和 SD 卡日志列表、路径校验、ZIP 导出任务及进度查询。 |
| `src/src/conf.c/.h`、`src/src/opt_iniparser/` | 读取设备 INI 配置。 |
| `src/src/`、`src/src/cjson/`、`src/src/mbedtls/` | GoAhead 服务实现及随项目构建的 JSON、TLS 相关源码。 |
| `src/src/utils/gopass.c` | 密码工具入口。 |
| `webserver/route.txt`、`webserver/auth.txt` | HTTP 路由和 GoAhead 用户、角色配置。 |
| `webserver/start_web.sh` | 设备启动与进程监测脚本；设定监听地址、日志级别、记录布局等启动参数。 |
| `webserver/html/` | 登录和功能页面；`home.html` 是主要操作页，`js/queryByDate.js` 与 `js/logExport.js` 分别实现记录查询和日志导出界面。 |
| `docs/URD_v1.0_20260923.md`、`docs/SRS_v1.0_20260923.md`、`docs/HLD_v1.0_20260923.md`、`docs/DDD_v1.0_20260923.md` | 日志导出功能的用户需求、软件需求、概要设计和详细设计；属于源码仓库内的设计资料。 |

## 已注册的 Web 接口

下表列的是 `goahead.c` 当前实际注册的 action。请求路径均以 `/action/` 为前缀；是否能从当前页面直接点到，取决于页面导航和脚本。

| action 名称 | 用途 |
| --- | --- |
| `getFileList`、`getFileListByDate` | 查询记录文件列表；后者按时间范围筛选。 |
| `getCfgFileList` | 查询 `/opt/` 的配置文件列表。 |
| `getFile` | 按请求中的 `Filename` 下载文件；记录、配置和导出 ZIP 共用。 |
| `uploadFile`、`delFile` | 上传配置文件到 `/opt/`、删除指定文件。 |
| `batchDownload`、`getBatchDownloadProgress`、`cleanupBatchFile` | 创建记录文件 ZIP 任务、查询进度、清理导出文件。 |
| `getExportConfig` | 返回记录导出的大小等配置。 |
| `getLogList` | 按来源目录与时间范围查询日志。 |
| `startLogExport`、`getLogExportProgress`、`cleanupLogExportFile` | 创建日志 ZIP 任务、查询进度、清理导出文件。 |

`/action/login` 和 `/action/logout` 由 GoAhead 的路由及认证机制处理，不在上述 `websDefineAction` 注册列表中。

## 构建与部署

- CMake 最低版本为 3.11，项目版本为 2.8，C 标准设为 C99。顶层 `WITH_WEB_APP` 默认开启；本目录的 `WEB_APP_WITH_MBEDTLS` 默认 `ON`，`WEB_APP_WITH_OPENSSL` 默认 `OFF`，不可同时开启。选择 OpenSSL 时 CMake 要求找到 1.1.1 版本。
- 构建目标为共享库 `go`（输出 `libgo.so`）、主程序 `web_app` 和工具 `gopass`。主程序额外编译 `goahead.c`、`action_func.c`、`log_export.c`；动态链接路径设为 `$ORIGIN`，即从程序所在目录寻找同目录的库。
- 交叉编译分支识别 `EC200A`、`EG25G`、`MCIMX6Y2CVM08AB`、`AG35`、`AG35GL`、`RK3506` 等 `QL_MODULE_PLATFORM` 值；其中部分平台还需 SDK 的 sysroot 路径及平台库。构建应使用对应设备的 SDK 工具链和环境变量。
- `make install` 按 CMake 的安装前缀，将程序、`libgo.so`、脚本、路由、用户文件和 `html/` 安装到 `usrdata/webserver`；证书和私钥文件为可选安装项。目标设备的运行位置由启动脚本写为 `/usrdata/webserver`。

## 运行与修改时关注

### 设备目录与配置来源

| 路径 | 当前用途 |
| --- | --- |
| `/usrdata/webserver/` | 启动脚本设定的工作目录；程序、共享库、路由、认证文件和 `html/` 的部署位置。 |
| `/opt/conf.ini` | 提供设备 ID 等信息，`dated` 和 `flat-can` 记录查询依赖它。 |
| `/opt/` | 配置文件列表、上传目标目录；`conf.h` 还定义 `/opt/log_cfg.ini` 和 `/opt/machine_info.ini` 供相应配置解析函数使用。 |
| `/media/sdcard/can_data_<dev_id>/` | `dated` 或 `flat-can` 记录根目录；前者继续按日期子目录查找。 |
| `/media/sdcard/erk-root/raw-data/` | `erk` 模式的记录根目录。 |
| `/media/sdcard/` | 记录批量导出的 ZIP 输出位置。 |
| `/tmp/rtms_log/`、`/media/sdcard/rtms_log/` | 日志来源目录；日志导出的 ZIP 也写入所选来源目录。 |

### 启动、权限与功能入口

- `webserver/start_web.sh` 将工作目录切换到 `/usrdata/webserver`，脚本写定的监听地址为 `192.168.1.1:80`，日志输出为 `stdout:2`；启动时传入 `--max-export-size 50` 和 `--record-mode dated`。脚本每 30 秒检查进程，不存在时重启。修改运行参数时同时检查脚本和 `goahead.c` 的参数解析。
- `route.txt` 对 `/` 使用 `auth=form`，并提供 `/action/login`、`/action/logout` 和通用 `/action` 路由。认证数据来自 `auth.txt`；实际认证表现需与构建生成的 GoAhead 配置和目标设备一起核对。`auth.txt` 包含预置账户数据，处理文档和日志时避免复制其密码散列。
- 记录文件根目录由 `--record-mode` 决定：`dated` 使用 `/media/sdcard/can_data_<dev_id>/<日期目录>/`，`erk` 使用 `/media/sdcard/erk-root/raw-data/`，`flat-can` 使用 `/media/sdcard/can_data_<dev_id>/`。`dated` 与 `flat-can` 需从 `/opt/conf.ini` 读取设备 ID；切换记录布局时联查 `action_func.c`、`goahead.c` 和前端列表逻辑。
- 配置文件列表读取 `/opt/`；上传接口将网页上传的 `.ini` 或 `.json` 文件移到 `/opt/`。下载接口使用请求中的 `Filename`，删除接口也操作请求指定的文件；改动相关逻辑时要同时核对路径约束、路由权限与页面传参，避免将前端限制视为服务端校验。
- 记录批量导出使用 `batchDownload`、`getBatchDownloadProgress`、`cleanupBatchFile` 等 action；上限通过 `--max-export-size` 设置，允许值为 20 至 100 MB，默认 50 MB。生成的 ZIP 位于 `/media/sdcard/`，打包依赖设备上的 `zip` 命令。
- 日志导出由独立的 `log_export.c` 实现，支持查询 `/tmp/rtms_log/` 和 `/media/sdcard/rtms_log/`，再通过 `startLogExport`、`getLogExportProgress` 和 `cleanupLogExportFile` 完成打包、轮询与清理。目录类型和文件名在该模块中校验；修改路径时需同步检查前后端的 `dirType` 映射和下载逻辑。
- `webserver/html/home.html` 的当前导航露出“按日期查询”和“日志导出”；普通记录导出与配置导入入口在页面中被注释，虽然相关 JS 和后端 action 仍在源码中。维护功能时区分“已实现接口”和“当前页面可见入口”。
- 当前 `CMakeLists.txt` 未定义本项目专用自动化测试目标。修改导出或上传行为后，优先在具备相应目录、配置、认证和 `zip` 命令的目标环境验证；不要在开发机直接运行会管理设备进程的 `start_web.sh`。
