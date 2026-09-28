# jtt_808_1078 项目说明

> 依据 2026-09-27 的本地工作区源码、CMake 和配置核对。本文是该应用的源码导览；分支或运行配置变化后需重新核对。文中的设备路径与端口是代码中的当前值，不代表已经在目标板验证。

## 项目位置与用途

| 项目 | 说明 |
| --- | --- |
| Git 仓库根目录 | `/home/tronlong/lyp/code/skes`。 |
| 应用源码目录 | `/home/tronlong/lyp/code/skes/skes-apps/jtt_808_1078`；上级 `skes-apps/CMakeLists.txt` 通过 `add_subdirectory(jtt_808_1078)` 将它纳入整仓构建。 |
| 本文档位置 | `/home/tronlong/lyp/perCode/idevelop_md/skes/jtt_808_1078/AGENTS.md`；在源码仓库外，不参与构建。 |
| Git 分支与版本 | 核对时为 `main_ui`，跟踪 `origin/main_ui`，HEAD `6bc22ef`。分支属于整个 `skes` 仓库，不是此应用独有；切换分支后运行 `git branch --show-current` 并复核本文。 |
| 产物 | CMake 项目和可执行文件名为 `jtt_808_1078`；安装规则把程序与 `808_config.json` 放入安装前缀下的 `apps/jtt_808_1078/`，共享库放入 `lib/`。 |
| 用途 | 设备侧 JT/T 808 平台通信：按配置连接多个平台，注册、鉴权、接收平台命令，上报位置、心跳、告警与流量；把视频控制转给 `aibox_stream`，处理视频文件查询/上传请求及告警抓图附件。 |

`808` 指终端与平台消息协议；`1078` 指车载音视频通信相关协议。本应用主要处理信令和文件/附件业务；实时音视频帧的生产、传输与本地录像还涉及同仓库的 `skes-apps/aibox_stream` 等进程，不应把本应用视为完整的视频编码器。

### 相关技术和名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| Git 分支、HEAD、`origin` | 分支是代码版本线；HEAD 是当前检出的提交；`origin` 是远端仓库名。`main_ui` 是记录文档时的工作分支，不是设备运行参数。 |
| SKES、AI 盒子（AIBOX） | SKES 是所属系统；AIBOX 是代码对设备侧终端的称呼。此进程通过本机组件获取定位、设备信息及告警事件。 |
| JT/T 808（2013/2019） | 终端与服务平台的通信协议；`808_version` 为 `1` 时走 2013 分支，为 `2` 时走 2019 分支。两种分支的报文头偏移与注册/鉴权处理不同。 |
| JT/T 1078 | 车载音视频通信相关协议；视频预览、回放控制通过本机 UDP 转交 `aibox_stream`，录制文件查询和上传处理在本进程的 `video/` 等代码中。 |
| 平台、注册、鉴权 | 平台是配置中的服务端实例。连接成功、注册和鉴权是不同阶段；代码用 `AIBOX_CONNECT_SUCESS`、`AIBOX_AUTHEND_SUCESS` 等状态区别。 |
| GPS、MCU、IPC | GPS 是定位数据，MCU 是微控制器；IPC 是本机进程间通信。当前启动路径使用 nanomsg 的 GPS 订阅与 MCU 请求应答，并读取 `/opt/conf.ini` 中的设备标识。 |
| TCP、UDP、nanomsg、POSIX 消息队列 | TCP 用于平台控制连接及告警附件连接；UDP 用于与 `aibox_stream` 等本机进程交换消息；nanomsg 用于本机消息总线；POSIX 消息队列 `/mq_jt808` 用于接收平台配置更新。协议传输方式与 JT/T 报文格式是两个不同层次。 |
| BSD、DMS、ADAS | 分别是盲区监测、驾驶员监测、高级驾驶辅助；本应用订阅相关事件并参与报警、抓图和附件上报。 |
| BCD、SD 卡 | BCD 是以十进制数字编码时间/标识的方式，报文和告警代码会用到；SD 卡位于设备挂载点 `/mnt/sdcard`，存放录像与告警图片。 |
| JSON、INI、cJSON | `808_config.json` 是平台 JSON 配置，`device_param.ini` 和 `/opt/conf.ini` 是 INI；cJSON 是 JSON 解析库。 |
| CMake、交叉编译、RPATH、`$ORIGIN` | CMake 定义构建安装；交叉编译为目标 Linux 设备生成程序。RPATH 是动态库查找路径；`$ORIGIN` 表示程序或库所在目录，可查找安装目录附近的 `lib`。 |

## 当前实际启动与数据流

`src/main.cpp` 的 `main()` 按顺序执行：初始化 `skes-log`（文件 `/tmp/jtt_808_1078.log`，初始 ERROR 级别）→ 注册 `SIGUSR1` 调整日志级别 → 写应用编译版本 → 清空告警状态 → 检查 SD 卡 → 启动与视频进程交互的 UDP 线程 → `basic_service_init()` 启动 GPS/MCU 本机消息线程并读取设备 ID → 初始化上报参数 → 从设备 ID 生成可选的平台手机号并更新 JSON → 订阅 SKES 告警事件并启动抓图线程 → 按 JSON 创建平台连接、周期上报和附件线程 → 启动平台配置更新队列线程 → 启动逐通道告警附件线程。最后主线程每 5 秒检查一次 SD 卡挂载。

主要链路：

1. **平台收发**：`jt808_platform_resource_init()` 读取 `808_config.json`；对有 IP/域名的槽位建立平台线程。连接后注册或使用已有鉴权信息；接收数据经 `jtt_808_1078_recv.cpp` 组帧，再由 `jtt_recv_realize.cpp` 分派命令。断线进入重连路径。
2. **周期上报**：每个启用平台的 `thread_aibox_report()` 以 1 秒为循环步长处理位置、心跳、超速状态与流量；默认位置间隔 15 秒、心跳 5 秒，平台参数可改变这些值。常规位置、心跳和流量发送会检查鉴权状态；临时位置跟踪分支需单独核对其状态判断。
3. **视频控制**：平台预览、预览控制、回放控制等命令经 `sendto_aibox_stream_platform()` 发往本机 `aibox_stream`。平台请求录像列表/文件上传时，本应用也会遍历 SD 卡录像目录并处理相应应答或 FTP 上传。实时视频帧处理需查 `aibox_stream`。
4. **告警抓图和附件**：`skes_services.cpp` 订阅 `/skes/event`，解析 BSD/DMS JSON，写入抓图队列；`capture_photo.cpp` 经本机抓图服务请求图片并写入 SD 卡告警目录。平台发起附件请求后，逐通道线程从告警图片队列取文件，按协议重命名并尝试 TCP 上传到平台指定的附件服务端。旧的 `thread_attachment()` 中直接调用上传的分支已注释，不能只凭其函数名判断现行上传路径。
5. **平台配置更新**：`jt808_cfg_param_get_from_app_init()` 创建监听 `/mq_jt808` 的线程。收到配置结构体后修改可执行文件所在目录的 `808_config.json`，随后代码执行 `killall -9 jtt_808_1078`；该流程依赖外部进程管理器重新拉起，代码本身没有在此处热加载。

## 文件导航

| 位置（均相对应用源码目录） | 作用与阅读入口 |
| --- | --- |
| `CMakeLists.txt`、`src/main.cpp` | 构建/安装和进程启动顺序。 |
| `src/jtt_808_1078/jt808_platform.cpp` | 多平台资源、TCP 连接、注册鉴权、重连及线程启动。 |
| `src/jtt_808_1078/jtt_808_1078_recv.cpp`、`jtt_808_1078_send.cpp`、`jtt_808_1078_utils.cpp` | 收包组帧、发送组包和协议工具。 |
| `src/jtt_recv_realize/jtt_recv_realize.cpp` | 平台下行命令分派：参数、文本、位置、录像、附件、预览/回放等。 |
| `src/jt808_config/` | 平台 JSON 读取、可选手机号生成、消息队列配置更新。 |
| `src/report/`、`src/gps/`、`src/app_server/`、`src/basic_service/` | 周期上报、GPS 转换、GPS/MCU 本机消息与设备 ID 初始化。 |
| `src/udp/udp_server_stream.cpp`、`src/video/`、`src/upload/` | 与视频进程的 UDP 控制、录像文件查找和 FTP 上传。 |
| `src/skes_services/`、`src/queue/`、`src/alarm/`、`src/attachment/` | 告警事件、抓图队列、图片生成/清理及附件协议上传。 |
| `src/config/`、`src/device_param/`、`src/ipc/`、`src/eeprom/` | INI/设备参数、POSIX 消息队列及设备信息相关实现。 |
| `tests/test.cpp` | `AlarmUploadTest`，在本机 nanomsg 端口模拟 BSD/DMS 事件；无 `add_test()` 注册，需区分测试可执行文件与 CTest 用例。 |

`src/serial/`、`src/ini/`、`src/jt808_config/cJSON.cpp` 等目录/文件虽存在，但部分源文件未列入主目标的 `SRC`。`basic_service.cpp` 中旧 Devin IPC SDK 初始化与回调路径被 `#if 0` 关闭；判断现行功能以入口调用、条件编译和 CMake 源文件清单共同为准。

## 配置、路径和本机接口

| 项目 | 当前代码所示行为 |
| --- | --- |
| `808_config.json` | 与可执行文件同目录读取；`Cloud_platforms` 含 `808_version`（1=2013，2=2019）、`isSnConvertPhone`、`platforms`、`IP/DN`、`port_number`、`ID/phone_number`；还有车牌号/颜色、音频格式和地方标准。空 IP 跳过该平台。当前最多启动 3 个平台槽位（`MAX_PLATFORM_NUM=3`）；JSON 解析循环本身没有检查条目上限，改配置时应校验长度。 |
| `/opt/conf.ini` | `[dev]` 下的 `id` 和 `secret` 由 `GetEncConfig()` 读取；设备 ID 用于终端身份及可选手机号生成。代码会在该路径缺失时记录失败，检查注册异常时先核对这里。 |
| `/userdata/puma/aibox/bin/device_param.ini` | 设备参数读写路径，定义在 `src/device_param/device_param.h`；接收平台设置参数的代码会用到。 |
| 可执行文件所在目录 | 程序写 `appVersion/jtt_808_1078.ini`；平台鉴权码文件按 `<平台索引>_key` 命名；可读取 `report_interval_internet_flow`，缺失或非正数时流量上报间隔默认 600 秒；配置更新会改写 `808_config.json`。目录需允许相应运行时写入。 |
| `/mnt/sdcard` | 启动和运行期间检查挂载；录像根目录为 `/mnt/sdcard/videoRecord/`，抓图/告警图片根目录为 `/mnt/sdcard/alarmRecord/`。抓图还依赖系统时间、存储容量与外部电源检测。 |
| `/sys/class/net/eth0/statistics/{tx_bytes,rx_bytes}` | 流量上报读取的网卡字节计数文件；设备网卡名变化会影响数据。 |
| 日志和临时图片 | `skes-log` 文件为 `/tmp/jtt_808_1078.log`；抓图请求使用 `/tmp/jtt_808_1078_alarm.jpg` 作为临时文件名。 |

| 通信接口 | 本应用角色与来源 |
| --- | --- |
| 平台 `IP/DN:port_number` | 对 `808_config.json` 中启用的平台发起 JT/T 808 TCP 连接；目标地址/端口取部署配置。 |
| 本机 UDP `1236` → `1210`～`1219` | `udp_server_stream.cpp` 接收视频进程回复（1236），向 `aibox_stream` 的通道端口发送控制消息。 |
| nanomsg `tcp://127.0.0.1:16005`、`38000` | 分别订阅 GPS、向 MCU 信息服务发请求；见 `app_gps.cpp`、`mcu.cpp`。 |
| SKES 消息 `tcp://127.0.0.1:19226` | `skes_services.cpp` 订阅事件消息。`tests/test.cpp` 可在 19226 端口发布模拟 BSD/DMS 事件，但需避免与设备现有服务争用。 |
| nanomsg `ipc:///tmp/cam_snapshot.ipc` | 抓图线程向本机 snapshot 服务发送请求并等待应答。 |
| POSIX 消息队列 `/mq_jt808` | 接收平台配置更新请求；其消息体是 `XSENSE_PLATFORM_CFG_T`。 |
| 附件 TCP / FTP | 告警附件通过平台下发的服务器地址建立 TCP 连接；录像文件上传代码使用 FTP。两者的服务器地址来自平台命令，不是固定本机端口。 |

`src/udp/` 还包含 1238、1260、1262 等接收模块，但这些模块的 `*_init()` 在当前 `main()` 中未调用；不要把它们当作已监听端口。`src/serial/app_serial.cpp` 也未加入当前主目标的 `SRC`。

## 构建、验证和修改注意

- CMake 要求 3.10 及以上，工程启用 C/C++ 语言；当前主目标 `SRC` 列出的源文件均为 `.cpp`，按 C++11 编译；依赖由整仓提供，包括 cJSON、nanomsg、curl、OpenSSL、zlib、libuuid、libexpat、collections-c、`skes-log`、`skes-linker` 等，并链接 `skes-utils`、`pthread`、`rt`、`m`。本目录的交叉编译器设置是注释示例；工具链和依赖前缀需按仓库/设备构建环境确定。`CMakeLists.txt` 定义相对安装位置，设备上的绝对目录取决于安装前缀。
- `tests/CMakeLists.txt` 构建 `AlarmUploadTest`，其 `test.cpp` 是循环/单次发送告警事件的模拟程序，不是覆盖协议解析、平台网络或附件上传的自动化单元测试。仓库顶层调用 `enable_testing()`，但本目录没有注册该目标为 CTest 用例。
- 修改协议字段时同时检查 2013/2019 两种报文头长度、`MESSAGE_INDEX_*`、收包组帧与 `jtt_recv_realize.cpp` 的命令偏移；修改视频命令时联查 `aibox_stream` 的 UDP 协议。修改报警抓图时联查 SKES 事件结构、摄像头通道映射、snapshot 服务、SD 卡目录及平台附件流程。
- 抓图代码默认外部供电 GPIO 为 119、时间有效年份下限为 2026，并检查 SD 卡使用率和告警目录配额；这些条件会影响普通开发机和目标板的抓图结果。配置及运行路径中包含设备标识、鉴权信息和平台地址，定位问题时避免把真实凭据写入文档。
- 以上是静态源码核验，尚不能证明目标板的服务、存储、网络、平台鉴权、视频流及附件上传真实可用；涉及这些行为需在对应部署环境验证。
