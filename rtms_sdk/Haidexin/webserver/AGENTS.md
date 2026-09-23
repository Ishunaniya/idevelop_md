# webserver 项目说明

## 项目位置与用途

- 所属仓库：`rtms_sdk`；源码目录：`rtms_sdk/apps/webserver`，由 `rtms_sdk/apps/CMakeLists.txt` 纳入构建。它是基于 GoAhead 的设备网页服务，提供 Modbus、IEC、显示和摄像头配置的导入与导出，以及设备重启入口。
- 编写时仓库所在 Git 分支：`rk3568_ubuntu_20241218`（通过 `git branch --show-current` 核对）。分支属于整个 `rtms_sdk` 仓库，不是 `webserver` 独立分支；切换分支后应重新核对本文档。
- 本文档根据当前分支源码编写。设备上的配置路径、监听地址、依赖库和实际认证行为需以构建配置与目标设备为准。

### 相关技术与名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| SDK、CMake | `rtms_sdk` 是承载多个应用的源码仓库；CMake 读取 `CMakeLists.txt` 生成构建与安装规则。 |
| GoAhead | 嵌入式 Web 服务器库。`main.c` 通过它加载路由、监听 HTTP 端点并处理请求。 |
| 文档目录、工作目录 | 文档目录是 GoAhead 服务静态页面的根目录；工作目录是进程解析相对路径的位置。两者可能不同，项目代码同时用到它们。 |
| 路由、action | 路由文件把 URL 和 HTTP 方法交给文件处理器或 action 处理器；action 是由 `websDefineAction` 注册的 C 函数，例如导入配置和重启。 |
| 模板、前端脚本 | `www/template/` 中的 HTML 片段由加载 action 返回；`www/js/` 中的脚本发起请求、更新页面。 |
| Modbus、IEC、MQTT | 设备相关通信配置的名称。主页将 IEC 配置页面标为“MQTT/IEC上数配置”；本服务负责配置文件的网页导入与导出，不实现这些通信协议本身。 |
| Excel、JSON | Excel 工作簿是上传或下载的配置表；Python 脚本将上传的工作簿转换成应用使用的 JSON 配置文件。 |
| 交叉编译、动态库 | 交叉编译是在开发主机上生成目标设备程序；GoAhead 等动态库在运行时提供程序依赖。 |
| `ENABLE_LOGIN_AUTH`、`auth=form` | 前者是 CMake 的路由选择开关；后者是启用登录路由中的 GoAhead 表单认证设置。认证是否真正生效还取决于 GoAhead 的构建和用户配置。 |

## 主要文件

| 路径 | 职责 |
| --- | --- |
| `CMakeLists.txt`、`global_config.h.in` | 定义可执行文件、依赖、登录开关和安装内容。 |
| `main.c` | GoAhead 启动入口；解析命令行，打开文档目录和路由，监听端点并处理事件。 |
| `src/webserver_init.c` | 创建上传临时目录并注册各个 `/action/` 处理函数。 |
| `src/action_handle.c`、`src/action_handle.h` | 配置页面模板、文件上传/导入、文件导出和重启处理。 |
| `src/auth_pam.c` | GoAhead 的 `authPamVerify` 函数实现；当前代码不执行 PAM 密码校验，直接返回验证成功。 |
| `www/home.html`、`www/login.html`、`www/template/`、`www/js/` | 前端页面、各模块模板与浏览器端交互。 |
| `www/route_auth.txt`、`www/route_noauth.txt` | 分别定义启用和关闭登录时的路由。 |
| `www/scripts/` | Excel 转 JSON 的 Python 脚本，以及目前未由 action 注册使用的升级脚本和 openpyxl 安装包。 |
| `www/auth.txt` | GoAhead 用户和角色配置；启用认证时由 `main.c` 加载。 |

## 构建与部署

- CMake 最低版本为 3.11，项目版本为 1.10。构建依赖 nanomsg、cjson、cn-cbor、curl、sqlite、goahead；链接还使用 pthread、rt 和 stdc++。
- 交叉编译分支要求 `QL_MODULE_PLATFORM=RK3568_UBUNTU`，并使用 SDK 提供的头文件和库目录。
- `ENABLE_LOGIN_AUTH` 是 CMake 缓存变量，只接受 `0` 或 `1`，默认 `0`。安装时会把选中的路由文件复制为 `usr/local/www/route.txt`；两个路由版本也分别安装。
- 安装目标包括 `usr/local/www/webserver`、`www/` 页面及资源，以及项目依赖的动态库。构建还生成 `webserver_startup_level.json`，其中启动级别为 `1`。
- CMake 通过 `GLOB_RECURSE CONFIGURE_DEPENDS` 收集 `src/*.c` 和 `src/*.h`；新增这些文件后重新配置或构建即可纳入。`global_config.h.in` 会生成构建目录中的版本头文件。

## 运行与修改时关注

- `main.c` 默认读取 `route.txt` 和 `auth.txt`，接受文档目录及监听端点参数，也支持 `--home`、`--route`、`--auth`、`--background` 等选项。具体默认文档目录和监听端点来自 GoAhead 配置。启用 GoAhead 认证的构建会加载 `auth.txt`。
- `GOAHEAD_INIT` 在 CMake 中映射到 `webserver_init`。新增 action 时，同时检查 `src/webserver_init.c` 的注册、`src/action_handle.h/.c` 的实现、所选路由文件及前端请求地址和方法。
- 初始化会创建工作目录下的 `tmp/` 和 GoAhead 文档目录下的 `tmp/`。模板和转换脚本使用相对于工作目录或文档目录的路径；运行时要核对工作目录与文档目录的对应关系。
- 四类配置的导入均接收 Excel 文件，调用 `/usr/bin/python3` 执行各自的 `www/scripts/xlsx2json1.py` 至 `xlsx2json4.py`，再把 Excel 与生成的 JSON 移到设备目录。导出请求通过表单字段 `Filename` 选择文件，处理函数会限制为各接口对应的固定路径。
- 设备配置目标路径位于 `/usr/local/etc/`、`/root/final_config/`、`/root/car_fei_3d/data/`；修改导入/导出时，核对目标目录、权限、Python 依赖及前端表单。
- `request_reboot` 会执行系统重启命令；需要在目标设备或隔离环境中验证，避免在开发主机直接触发。
- `www/route.txt` 是源码中现有的路由文件；实际安装的同名文件由 `ENABLE_LOGIN_AUTH` 选择生成。启用登录时路由使用 `auth=form`；`src/auth_pam.c` 中的函数自身不验证密码，不能把它视为 PAM 认证已生效。修改登录行为时检查两个路由版本、`www/auth.txt` 和实际链接的 GoAhead 认证配置。
- 两个路由版本都声明了 `load_camera_app_web`，但 `src/webserver_init.c` 未注册这个 action，`src/action_handle.c` 也没有对应实现；摄像头表单实际放在显示配置模板中。`www/js/upgrade.js` 与升级模板引用的 `load_upgrade_web`、`start_upgrade` 也未注册，且主页没有载入升级脚本。维护这些页面时按实际入口核对，不要假定声明的功能可用。
