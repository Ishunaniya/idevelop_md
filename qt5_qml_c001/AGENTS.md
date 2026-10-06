# qt5_qml_c001 项目初始化与工作指引

核验日期：2026-09-30（Asia/Shanghai）。本文件根据当前源码、Git 信息及实际检查编写；详细分析见 [项目源码说明.md](项目源码说明.md)，命令结果与限制见 [核验记录.md](核验记录.md)。

## 目录与阅读顺序

当前目录保留三个 Markdown，以及 `evidence/diagrams/` 中三张流程图的 PNG/SVG。日常先读主文档即可。原 Word、核验附件和归档目录已按用户要求删除。

| 内容 | 用途 |
| --- | --- |
| [项目源码说明.md](项目源码说明.md) | 主文档：架构、模块、技术名词，以及 [设备故障排查 D01～D15](项目源码说明.md#device-troubleshooting) |
| 本文件 `AGENTS.md` | `/init` 对应的项目位置、Git、用途、构建和开发指引 |
| [核验记录.md](核验记录.md) | 已做检查、原始结果、差异及尚未实机验证的范围 |
| `evidence/diagrams/` | 六个流程图文件：三张图各有 PNG/SVG，分别用于直接显示和放大 |

2026-10-02 补充了阅读导航、清晰流程图和设备排查。故障内容直接加入现有主文档，没有另拆顶层手册。图的两种格式表示同一内容，现保留可直接阅读的图像，生成脚本和核验附件已删除。

## 项目位置与用途

| 项目 | 核验结果 |
| --- | --- |
| 源码位置 | `/home/tronlong/lyp/code/qt5_qml_c001` |
| 文档位置 | `/home/tronlong/lyp/perCode/idevelop_md/qt5_qml_c001` |
| 当前 Git 分支 | `feature/startup-update` |
| 本地上游跟踪分支 | `origin/feature/startup-update` |
| 核验提交 | `641af2c83ffe8ee9834f6de694c9d44a2de72f81` |
| 最新提交时间 | `2026-09-07T16:00:35+08:00`，来自 Git 提交记录 |
| 最新提交摘要 | `chore: 忽略交叉编译的测试二进制 (aarch64 build output)` |
| 工作区状态 | 核验时干净；与本地上游跟踪引用相比 ahead/behind 均为 0；没有 fetch，不能证明远端服务器没有新提交 |
| origin 地址 | `git@gitlab.irootech.com:overseas/hardware/applications/tpad/qt5_qml_c001.git` |
| 程序类型 | 面向 RK3576 嵌入式 Linux 的 Qt5/QML 车载界面程序；README 称其为“360 环视主程序” |
| 主要用途 | 摄像头画面显示、外部环视结果显示、检测框与告警状态展示、标定与配置管理、网络与存储状态展示、升级准备及板端启动脚本 |

依据：[README](/home/tronlong/lyp/code/qt5_qml_c001/README.md)、[程序入口](/home/tronlong/lyp/code/qt5_qml_c001/src/main.cpp:246)、[主界面](/home/tronlong/lyp/code/qt5_qml_c001/qml/MainPage.qml)及 [核验记录.md](核验记录.md) 中的历史检查结果。

当前模板包含 6 路摄像头。入口为 0～3 路绑定 QML 视频控件，为启用的 4 路及以上通道调用后台采集。检测框协议只定义 4 路，拍照 IPC 也只接受 0～3 路，不能把不同模块都概括为“支持 6 路”。

YOLO 推理程序和 `avm_app` 是外部进程：本仓库接收检测框、写出图像、控制环视启停并显示共享图像。不要将外部算法能力归为本仓库独立实现。分支名只是版本标识；不能从名字推断升级方案已完成设备验收。

### 相关技术与名词

| 技术或名词 | 含义及本项目用途 |
| --- | --- |
| Git 分支 / HEAD / 上游 | 分支是指向提交的名称，HEAD 标识当前检出版本；上游是用于比较的跟踪引用。本文件中的分支信息是核验时快照 |
| HMI | Human–Machine Interface，人机界面；这里指车载画面、告警、设置和操作页面 |
| Qt / QML / Qt Quick | Qt 提供应用框架；QML 描述界面与绑定；Qt Quick 负责这些界面的运行与渲染。C++ 向 QML 注入业务对象 |
| qmake / 交叉编译 / sysroot | qmake 从 `.pro` 生成 Makefile；交叉编译在 x86_64 主机生成 AArch64 程序；sysroot 保存目标平台头文件和库 |
| GStreamer / pipeline | 媒体处理框架及其元素链；用于采集、格式转换、预览、编码和测试工具中的 MP4 封装 |
| V4L2 / Wayland | Linux 视频设备接口及显示协议；本程序使用 `/dev/video*`，启动时强制 `wayland-egl` |
| POSIX / System V 共享内存 | 两套不同的进程间共享内存 API；分别用于具名图像/检测数据和整数 key 的 H.264 环形缓冲 |
| nanomsg / IPC | 消息通信库及进程间通信；本项目使用 REQ/REP 请求应答、PUB/SUB 发布订阅，并通过本机 TCP 或 IPC 地址连接 |
| YOLO / AVM | YOLO 是目标检测模型家族，此处相关推理由外部 `yolov5_object_detect*` 程序承担；AVM 指环视功能，此处拼接由外部 `avm_app` 承担 |
| OpenCV / 单应矩阵 | 图像处理库及平面坐标的 3×3 投影变换矩阵；用于图像转换和标定。矩阵不能单独证明实际距离精度 |
| H.264 / MP4 / PTS | 视频编码、媒体容器、呈现时间戳；编码帧先写共享内存，测试工具可封装为 MP4，时间戳用于重建播放时间线 |
| AEB / BSD / DMS | 自动紧急制动、盲区检测、驾驶员监测的功能名；本仓库包含相关 UI/配置/状态接口，实际制动或识别由整机相关服务验证 |
| i18n / TS / QM | 国际化、可编辑翻译源文件、编译后的翻译文件；QM 从运行程序目录的 `i18n/` 加载 |

更完整的术语、具体文件和边界见 [项目源码说明.md](项目源码说明.md)。

## 文档用途与加载说明

官方 OpenAI 文档将 `/init` 定义为生成 `AGENTS.md` 指引文件的交互命令，见 [Developer commands](https://learn.chatgpt.com/docs/developer-commands?surface=cli)。它不是 Bash 系统命令。

本次按用户指定，将指引和分析文档保存在源码仓库之外；没有在源码目录放置副本或链接，也没有启动嵌套 Codex 会话。因此应描述为完成 `/init` 对应的分析和文档生成，而不是声称终端执行了 `/init`。以后从源码目录工作时，请显式让助手读取本文件；不要假定外置文档已被自动加载。

## 目录与关键入口

| 路径（相对源码根目录） | 用途 |
| --- | --- |
| `src/main.cpp` | 应用启动、Qt/QML 注册、上下文属性、状态服务与视频绑定 |
| `src/VideoSurfaceItem.*` | 0～3 路主预览、图像显示处理、独立 H.264 编码、共享内存写出 |
| `src/camera_shm_multi.*`、`src/camera_manager.*` | 配置解析、通道设备映射、另一套采集管线、后台通道、拍照请求 |
| `src/*Bridge.*`、`src/NetStatusPoller.*` | 检测框、状态、环视控制、录像告警、网络状态等桥接 |
| `qml/main.qml`、`qml/MainPage.qml` | 顶层窗口及常驻主界面，警告、密码和设置通过覆盖层加载 |
| `qml/SettingsCenter.qml`、`qml/CalibrationPage.qml` | 设置中心及标定操作 |
| `config/` | 配置模板；不代表设备当前配置 |
| `assets/`、`qml.qrc` | 图标、图片、翻译与 Qt 资源清单；部分运行图标来自外部 `/root/pictures/` |
| `deploy/` | 部署、升级启动、USB 恢复及 Wi-Fi 初始化脚本 |
| `tests/` | 板端录像、共享内存和告警辅助程序；不是统一自动化测试套件 |
| `scripts/`、`tools/` | 调色、内存排查、目录整理和翻译辅助工具 |
| `docs/` | 既有设计及分析资料；其中性能和架构陈述需对照当前源码 |
| `src/legacy/` | 历史实现；未列入当前主工程 SOURCES |
| `third_party/nlohmann/` | 随仓库提供的 JSON 头文件库 |

## 构建与验证

主工程是 `qt5_qml_c001.pro`，不是 CMake 工程。它同时声明 `c++11` 和后续 `c++17`；本次生成的编译命令实际包含 `-std=gnu++1z`。Qt 的有效模块包括 core、gui、qml、quick、network、widgets。

参考构建流程（在源码外生成产物）：

```bash
source /home/tronlong/RK3576/rk3576_linux6.1_release/buildroot/output/rockchip_rk3576/host/environment-setup
mkdir -p /tmp/qt5_qml_c001_build
cd /tmp/qt5_qml_c001_build
qmake /home/tronlong/lyp/code/qt5_qml_c001/qt5_qml_c001.pro
make -j2
```

**当前主机实测：qmake 成功，make 失败。** 首个错误为 `gst/gst.h: No such file or directory`。SDK 本身有该头文件，但 `.pro` 的对应 include 路径仍使用不存在的 `/home/xp/...`。文档任务未修改这些路径，也没有验证最终链接和板端运行。不能写“编译通过”。原始构建日志已按用户要求删除，历史结果与首个错误保留在核验记录中。

依赖声明包括 GStreamer/GLib、TurboJPEG、nanomsg、OpenCV、pthread/dl、RKNN runtime 和 RGA。链接声明不等于应用实际执行了 RKNN 推理或 RGA 加速；当前主路径没有据此获得设备运行证明。

ASan 配置为 `qt5_qml_c001_asan.pro`，包含主 `.pro` 并追加 AddressSanitizer 参数；本次未构建或运行 ASan。已有二进制 `tests/test_h264_*` 为 AArch64，主机为 x86_64，没有执行它们。共享内存录像测试工程的 qmake 也已实测失败：`.pro:10` 的 `SYSROOT ?=` 报赋值语法错误；详见核验记录。

已执行的静态检查：8 个 JSON 模板解析成功；21 个 shell 脚本语法检查通过；6 个 TS 文件 XML 可解析；主工程声明的 32 个源文件、33 个头文件、6 个 TS 和 40 项 qrc 资源均存在。这些结果不等同于 QML 加载、翻译完整性或设备功能验收。

独立录像验证工程 `test_h264_record_verify.pro` 的 qmake 与 make 均成功，生成 AArch64 可执行文件；这只证明该独立工具本次可交叉编译，没有执行摄像头录像测试，也不改变主应用构建失败的结论。

## 运行与配置约定

- 入口强制 `QT_QUICK_BACKEND=opengl`、`QT_QPA_PLATFORM=wayland-egl`、`QT_OPENGL=es2`、`QSG_RENDER_LOOP=threaded`；外部设置被覆盖，不能宣称可直接用 `offscreen` 在本机冒烟运行。
- 通用配置查找函数先接受“已存在的绝对路径”，然后按应用目录、相对当前工作目录的 `config/`、当前工作目录依次查找；无候选时返回原名称。这只适用于调用该函数的代码。
- `AppSettings` 的 `SettingsManager` 默认读写 `/root/cameras/ui_config.json`，维护 `/tmp/ui_config.json`；AEB 每次启动强制开启且该状态不写入持久配置。
- `Settings` 的 `SettingsData` 管理通用设置，是另一对象；持久路径优先 `/root/cameras/settings.json`，再用应用目录，最后有 `/userdata/settings.json` 回退分支。
- 标定文件、矩阵、掩膜写入 `/userdata/`；升级包和标记位于 `/usrdata/upgrade/`。两个目录拼写不同，不可合并解释。
- `I18nBridge` 在 `<程序目录>/i18n/app_<locale>.qm` 加载翻译；首次语言回退为 `en_US`，是否成功取决于 QM 实际存在并能加载。
- `deploy/install_to_cameras.sh` 从源码根目录寻找可执行文件并复制配置，会按权限复制 `/usr/bin/install.sh`；它不会完整安装图片、QM、外部算法、升级 UI 和全部启动脚本。

## 修改与排查指引

这些是基于现有代码提出的工作约束，不代表仓库存在独立格式检查工具。

- 修改前重新检查 `git status`、当前分支和 HEAD；本文件中的版本信息只代表本次核验。
- 新增 C++ 实现同步维护 `.pro`，新增需打包的 QML/资源同步维护 `qml.qrc`；不要把 `src/legacy/` 的实现当成当前主入口。
- 保持 `v0`～`v3` 的 `objectName`、QML 模块名及上下文对象名一致；`main.cpp` 按这些名称查找和绑定。
- 修改跨进程结构时同时核对外部生产者/消费者、大小、对齐、像素格式、原子字段和时间戳；不要只改本仓库头文件。
- 修改设置时区分 `AppSettings`、`Settings`、`I18n` 的写入路径和字段；当前 `SettingsManager` 整体重建 JSON，而 I18n 在同名文件上保留旧字段后更新语言，需关注交互覆盖。
- 视频变更应分别验证预览、picshare、H.264、管线回退、拍照和退出；预览能显示不能证明录像链有效。
- 语言变更检查 `qsTr()`、TS、QM、运行部署位置和字体；运行自动翻译脚本会修改翻译文件并访问外部翻译服务。
- 验证结果必须区分源码静态发现、主机实际命令、板端测试及历史资料。没有测量不能写“低延迟”“无泄漏”“性能最优”。

## 需要特别核对的当前行为

1. 检测共享内存在构造时打开失败时，构造函数没有启动轮询定时器；读取函数虽有重试分支，也需先被调用。当前 QML 未发现主动 `refresh()` 调用，不能保证外部检测进程晚启动后自动恢复。
2. 主预览采集的 RGB 变体有编码支路；BGRA/MJPEG 回退变体没有对应编码支路，预览回退成功不等于 H.264 可用。
3. 后台 `start_camera_async()` 未向底层转发 `picshareWriteEnabled`，底层写出路径也没有对应开关判断；不要声称这一开关对所有通道一致生效。
4. 检测框由共享内存组装为 QVariant 数据，图像链路也有 memcpy/颜色转换；“端到端零拷贝”不符合当前代码。
5. `NetStatusPoller` 将外部 `network_online` 映射为界面的服务器在线文字；本对象未自行验证业务服务器会话。
6. 升级启动脚本引用外部升级 UI 和 S98/S99z 回滚门禁；本仓库没有完整提供这些组件，不能声明整机回滚已验证。

对应证据、其他差异和未验证内容见 [核验记录.md](核验记录.md)。
