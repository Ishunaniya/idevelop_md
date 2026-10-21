# EdgeVision 开发与协作指南

这份指南用于接手项目、定位改动和执行验证。先看下面的状态与阅读入口，再按任务查阅细节。

**当前状态（2026-10-22，提交 `5427d18`）**：冰雪已实现“RKNN卷扬定位＋OpenCV框内HSV候选面积估计”，新增Detector API、CLI和单路Qt宿主。固定快照mock测试：无OpenCV共享4/4、有OpenCV共享6/6、有OpenCV纯静态4/4；核心仍PASS=235 FAIL=0。旧工具默认关闭，开启仍有CMake错误；x86受保护源仍阻碍编译。已有通道0历史板测，本次未重跑硬件。详细契约、限制和证据见[分析报告](edgevision分析报告.md)第5.10、6.7、8章。

| 你要做什么 | 从这里开始 |
| --- | --- |
| 第一次接手，了解项目 | [路径与基线](#1-路径用途与事实基线) → [处理流程](#2-项目目标与职责边界) → [代码地图](#3-代码地图与阅读入口) |
| 编译并确认基础链路正常 | [构建与验证](#4-构建链接与验证)，先执行 4.1 的 mock 命令 |
| 在宿主中接入算法库 | [公共 API](#5-公共-api-与数据契约)，5.4 提供完整的最小调用示例 |
| 修改人员告警、高度限位或冰雪检测 | [功能规则与完成程度](#6-功能规则与完成程度) → [改动位置与扩展流程](#12-实施改动时的约定) |
| 更换相机、标定或模型 | [地面标定](#7-地面坐标与标定)、[配置热更新](#8-配置解析与热更新)、[后端解码](#9-后端与解码约束) |
| 排查没框、距离异常、UI 不更新 | [采集与排障](#10-采集共享内存与排障) → [已知问题](#11-已核实的现存问题与文档偏差) |

阅读约定：**已实测**指本次执行得到结果；**源码确认**指检查到实现但未执行对应硬件链路；**历史记录**来自仓库文档，需在目标设备复核；**待实现**指当前仍为占位逻辑。

## 1. 路径、用途与事实基线

- 本文件位于 `/home/tronlong/lyp/perCode/idevelop_md/edgevision/AGENTS.md`，是针对外部源码仓库生成的开发上下文。
- 实际源码根目录：`/home/tronlong/lyp/code/edgevision`。当前文档目录没有复制源码；不要把这里误当成能直接执行 `cmake -S .` 的源码根目录。
- 本文中的 `include/`、`src/`、`configs/`、`docs/` 等相对路径，除特别注明外，均相对于实际源码根目录。执行 Git、构建、搜索和运行命令时显式指定源码路径或先切换目录。
- 初次检查日期：2026-10-15；本次更新日期：2026-10-22。分析提交为 `5427d180dc849d3a2961be200a52435c3471df99`，直接父提交及上一版基线为 `137a845454d537085d28ab7b021a81581f545bc5`。核验开始及结束时分支为 `feature/split-core-plugin-libs`、HEAD为目标提交，工作区干净。
- 本次通过 `git archive` 提取目标提交到 `/tmp/edgevision_audit_5427_20261021/source`；独立构建不修改外部源码。结果见[报告核验记录](edgevision分析报告.md#125-核验记录与复现归档)。后续接手时重新核对分支、HEAD和工作区状态。
- 这是当时源码的事实记录。以后修改前先检查 `git status --short` 和相关实现；源码变化后同步更新本文，不能把历史测试结果当作当前保证。
- 本文件不会因放在文档目录而自动成为外部源码目录的祖先级指令文件；在这里接手项目时，应主动读取本文并核对目标仓库自己的适用指令。

## 2. 项目目标与职责边界

EdgeVision 是面向起重机安全检测的 C++17 算法库，采用 CMake 构建。可运行于 PC，也支持 RK3576 等 aarch64 板端 RKNN 推理。

完整任务流程：

```text
宿主提供 Frame 与 MachineContext
  → Preprocessor：颜色转换、缩放、letterbox
  → IInferBackend：mock / x86 / onnx / rknn
  → IDetectionDecoder：输出行矩阵转 Target
  → 框坐标映回原图
  → 按 camera_id 选择 IGroundMapper，生成地面坐标
  → ground_metrics：转换为任务参考点下的距离与角度
  → IFunctionPlugin：区域规则、跨帧状态、告警
  → Result 与日志、耗时统计、按策略取证
```

- 算法库负责上述同步处理流程。摄像头采集、GStreamer 管线、Qt/QML、录像、PLC/MCU 通信、声光及主控联锁由宿主负责。
- RGA 加速放在宿主工具侧；宿主完成预处理后可调用 `VisionTask::detectPrepared()`。不要为某一摄像头在核心库里直接打开 `/dev/video*`。
- 默认 `mock` 路径不依赖第三方算法 SDK。RKNN、ONNXRuntime 是各后端的可选依赖；RGA、GStreamer 属于采集工具依赖。
- “零依赖”不能泛化为当前所有目标都可直接构建：显式开启旧工具时还有现存 CMake 问题，见第 11 节。
- 功能插件分别编入人员、高度限位、冰雪三组库；decoder、mapper 和工厂表留在核心库。登记仍通过静态构造函数完成，动态插件在被系统加载器实际加载时向核心登记；项目尚无自动发现、`dlopen` 加载或卸载插件的管理器。

## 3. 代码地图与阅读入口

### 3.1 构建入口与公共头文件

| 路径 | 作用与修改入口 |
| --- | --- |
| `CMakeLists.txt` | 后端、可选依赖、`EVS_SRC`、静态/动态库、整库链接、工具及测试目标 |
| `include/evs/vision_task.h` | 宿主主入口、生命周期、同步检测与调试接口 |
| `include/evs/frame.h` | `PixelFormat`、`Frame`、`MachineContext` |
| `include/evs/status.h` | 错误码、`AlarmState`、`Target`、`Result`、相机号打包 |
| `include/evs/task_config.h` | 原配置布局保持；专属选项独立保存 |
| `include/evs/winch_ice.h` | 冰雪Detector、紧凑RGB/灰度、逐对象结果 |
| `include/evs/function_task_hooks.h` | 可选配置校验、预热及拒帧扩展，不改旧插件虚表 |
| `include/evs/iinfer_backend.h` | 推理后端接口；公共接口不暴露 SDK 头文件 |
| `include/evs/detection_decoder.h` | 检测输出解析接口与工厂 |
| `include/evs/function_plugin.h` | 功能插件接口与工厂 |
| `include/evs/ground_*.h` | 地面映射配置、参数读取、mapper、距离与角度接口 |

### 3.2 核心实现

| 路径 | 作用与修改入口 |
| --- | --- |
| `src/vision_task.cpp` | 初始化装配、取帧校验、每帧流程、热更新及释放 |
| `src/evs_register.h` | `FunctionPluginRegistration`、`DecoderRegistration`、`GroundMapperRegistration` |
| `src/core/config.cpp` | 轻量 YAML 子集解析，包含显式处理的 `ground.cameras` 列表 |
| `src/core/model_registry.cpp` | 按后端与模型路径共享后端、引用计数和快照 |
| `src/core/backend_factory.cpp` | 按编译宏创建后端；当前仍是显式分支 |
| `src/core/preprocess.cpp` | CPU NV12/RGB/BGR 转换、定点双线性、letterbox 及坐标逆变换 |
| `src/core/detection_decoder.cpp` | flat6、YOLO 行矩阵解析及静态登记；`decode_person()` 已由人员插件移入核心 |
| `src/backend/yolo_decode.h` | 共用 YOLOv5、YOLOv11、YOLOX 原始头解析及 NMS |
| `src/backend/mock/` | 无 SDK 占位后端，所有构建均编入 |
| `src/backend/x86/` | 参考检测器/ONNXRuntime 后端；当前 `.cpp` 带加密头 |
| `src/backend/rknn/` | RKNN SDK 加载、输入输出处理与解码；另有局部 README |
| `src/tasks/person/` | 0x0004/0x0005/0x0006 共用人员策略引擎及待办 |
| `src/tasks/height_limit/` | 高度限位插件、安装姿态判断入口及待办 |
| `src/tasks/winch_ice/` | 卷扬框筛选/NMS、HSV面积、Detector及文件配置适配 |
| `src/ground/` | homography、crane_pose、mapper 工厂、参数与几何换算 |
| `src/core/log.cpp`、`src/core/debug.cpp` | 日志、环形缓冲、快照、耗时统计和取证落盘 |
| `src/core/offline_eval.cpp` | 离线检测评测引擎，不执行功能插件告警 |

### 3.3 工具、配置与测试资产

| 路径 | 作用与修改入口 |
| --- | --- |
| `tools/eval_offline/` | 离线评测 CLI、合成数据脚本与已入库 fixtures |
| `tools/v4l2_detect/` | 采集验证 CLI、GStreamer/共享内存/MP4、RGA、检测框共享内存、启动脚本 |
| `tools/task_cam/main.cpp` | 经 `VisionTask` 执行完整功能插件的旧板端示例 |
| `tools/ice_replay/`、`tools/winch_ice_demo/` | P6回放、无窗口相机CLI和可选宿主工况门控 |
| `tools/winch_ice_ui/` | 严格选路、同帧图像结果、单通道Qt Quick宿主 |
| `tools/linux_source/` | 宿主分工说明，当前只有 README，不是另一个可执行程序 |
| `configs/` | 功能及评测 YAML、多摄像头 JSON |
| `tests/` | 核心、冰雪、分配失败、旧插件兼容、CLI；UI目录另有支持层测试 |
| `eval_onnx_demo/` | bus.ppm 与模型自检标注；不等同于现场独立人工标注集 |
| `yolov5s.onnx` | 已入库的约 29 MB 模型资产，避免无关替换与格式化 |
| `cmake/aarch64-linux-gnu.toolchain.cmake` | 发行版 aarch64 GNU 工具链配置 |

### 3.4 配套文档

建议先读公共头文件和 `src/vision_task.cpp`，再沿具体任务进入 plugin、mapper 或 backend。文档用途：

- `docs/host_api.md`：宿主集成、数据所有权、工况与结果。
- `docs/api.md`：字段、配置与接口细节。
- `docs/site_customization.md`：现场改动落点；其中新增 mapper 的工厂修改步骤已落后于静态登记实现。
- `docs/cross_compile.md`：库与宿主分开交叉编译、动态库交付。
- `docs/x86_offline_eval.md`：数据集、指标和 ONNX 评测。
- `docs/rknn_backend.md`、`src/backend/rknn/README.md`：RKNN 及板端事项。
- `docs/capture_test.md`：采集工具验证。
- `docs/observability.md`：日志、快照、取证。
- 各 `src/tasks/*/待办.md`：当前功能缺口。判断是否已实现时，以实际 `decide()`、mapper 和测试为准。

## 4. 构建、链接与验证

### 4.1 当前已实测可用的基础验证

源码固定快照来自5427d18，C++17、GNU13.3.0；临时目录若被清理，按分析报告8.3先重新git archive提取。

```bash
cmake -S /tmp/edgevision_audit_5427_20261021/source \
  -B /tmp/edgevision_audit_5427_20261021/build_mock \
  -DEVS_BACKEND=mock -DEVS_WITH_GSTREAMER=OFF
cmake --build /tmp/edgevision_audit_5427_20261021/build_mock -j4
ctest --test-dir /tmp/edgevision_audit_5427_20261021/build_mock --output-on-failure -V
```

本次无OpenCV共享CTest4/4，core235、ice静动各95；有OpenCV共享6/6，core235、ice静动各225、allocation75；有OpenCV纯静态4/4，失败均0。OpenCV依赖仅解包到/tmp，完整命令见[报告核验记录](edgevision分析报告.md#125-核验记录与复现归档)。正式冰雪必须OpenCV ON和RKNN，mock测试不证明真实模型/相机有效。

测试从构建目录运行，避免相对路径临时文件写入源码。旧235是核心程序检查数，不能代替全部CTest结果；此次未重跑真实ORT/RKNN、交叉编译或设备UI。

### 4.2 CMake 选项与目标

| 变量 | 默认值/语义 |
| --- | --- |
| `EVS_BACKEND` | `mock`；实现的选择为 `mock`、`x86`、`onnx`、`rknn`；`qnn` 仅有预留文字，没有可用实现 |
| `EVS_BUILD_SHARED` | `ON`，同时生成核心与三组插件的动态库；`OFF` 时四份静态库仍构建 |
| `EVS_WITH_ORT` | `OFF`；用于 x86/onnx 的真实 ONNXRuntime 路径 |
| `ONNXRUNTIME_LIB_DIR`、`ONNXRUNTIME_INCLUDE_DIR` | ORT 库目录、头文件目录 |
| `RKNN_API_LIB`、`RKNN_API_LIB_DIR`、`RKNN_API_INCLUDE_DIR` | RKNN 目标运行库与头文件位置 |
| `EVS_WITH_RGA` | `OFF`；启用采集工具的硬件预处理 |
| `RGA_LIB`、`RGA_API_INCLUDE_DIR`、`DRM_LIB` | RGA/DRM 目标库、配套头文件；多个 include 目录用带引号的分号列表传入 |
| `EVS_WITH_GSTREAMER` | ON；新CLI通过pkg-config，旧工具通过板端sysroot；UI独立要求GStreamer |
| `GSTREAMER_SYSROOT` | 仅旧工具覆盖历史板端sysroot |
| `EVS_WITH_OPENCV` | OFF；实际查找/链接core、imgproc，正式冰雪须ON |
| `EVS_BUILD_TESTS`、`EVS_BUILD_TOOLS` | ON；常规测试、新冰雪CLI总开关 |
| `EVS_BUILD_LEGACY_TOOLS` | OFF；旧采集/评测工具显式启用 |
| `EVS_BUILD_ICE_UI` | OFF；独立开关，要求Linux、共享库、OpenCV、Qt5≥5.12及GStreamer≥1.16 |

- `evs_objs` 是核心 PIC 对象库；`evs` 产出 `libevs.a`；`evs_shared` 产出 `libevs_core.so.1` 和链接名 `libevs_core.so`。核心不再包含功能规则实现。
- `evs_add_plugin()` 为每组插件创建 `<name>_objs`、静态目标 `<name>` 和可选动态目标 `<name>_shared`。常规静态测试整库链接四份归档；新动态冰雪工具直接链接core+ice。
- 常规测试/CLI仅在顶层且总开关开启时创建；UI为独立子目录开关。旧工具另需LEGACY_TOOLS=ON。`eval_offline` 只在 x86/onnx/rknn 构建创建；`v4l2_detect` 还受非 Windows 条件限制；`evs_task_cam` 还需要 GStreamer 头文件检测成功。
- CMake 中的 `if(NOT WIN32)` 并非严格 Linux 检查；不要据此宣称采集工具支持 macOS。
- 静态登记依赖完整保留目标文件。仓库内 `evs_link_whole(target)` 整库链接 `evs`、`evs_person`、`evs_height_limit`、`evs_ice` 四份归档；外部消费者可仅整库链接核心和所需插件。

| 功能库名 | 功能号 | 静态产物 | Linux 动态产物（SONAME） |
| --- | --- | --- | --- |
| `evs_person` | `0x0004/0x0005/0x0006` | `libevs_person.a` | `libevs_person.so.1` |
| `evs_height_limit` | `0x0007` | `libevs_height_limit.a` | `libevs_height_limit.so.1` |
| `evs_ice` | `0x0008` | `libevs_ice.a` | `libevs_ice.so.1` |

Linux 外部静态链接的人员功能示例：

```text
-Wl,--whole-archive /path/to/libevs.a /path/to/libevs_person.a -Wl,--no-whole-archive
```

SDK 运行库等依赖放在适当链接位置；MSVC 对各归档使用 `/WHOLEARCHIVE`。只有 `libevs.a` 时即使整库链接也没有功能插件；普通静态链接还可能丢失 decoder/mapper 登记。

动态插件由完整对象列表生成，依赖 `libevs_core.so.1`，自身 RUNPATH 为 `$ORIGIN`，可与核心放在同一目录。已有GNUInstallDirs安装core+ice静动库、头、配置、说明及存在的新增工具；人员/高度库不在install集合，模型也不自动安装。GNU/Linux 宿主还要保证插件未被 `--as-needed` 裁掉，例如 `-Wl,--no-as-needed -levs_person -Wl,--as-needed -levs_core`，并配置宿主的运行库搜索路径。2026-10-19历史实测核心单独加载时flat6 可解码，但人员任务初始化返回 `0x00041009`；显式保留人员插件后成功。仅在命令中写 `-levs_person` 不保证它被加载。

旧 `libevs.so.1` 已改为 `libevs_core.so.1`；旧宿主需更新链接配置并重新链接，不能只把新库按新文件名复制过去。库拆分发生于父提交137a845；5427d18新增冰雪API和非虚任务重载，保持原IFunctionPlugin/TaskConfig布局。同工具链v1头兼容测试通过，不能推广任意C++ ABI。

### 4.3 x86/ONNX 与离线评测

设计上，`x86` 未接 ORT 时使用红/蓝色块参考检测器；`onnx` 复用同一实现并接受 `onnx` 和 `x86` 后端名。仅选择 `onnx` 不代表真实模型已启用：必须核对 ORT 检测日志、头文件、运行库与模型加载结果。

**普通x86配置已通过，编译仍被加密源码阻碍；启用旧工具仍复现CMake错误。以下含旧评测工具的流程需先修复/恢复，不是本次通过的命令。**

```bash
cmake -S /home/tronlong/lyp/code/edgevision \
  -B /tmp/edgevision_x86 -DEVS_BACKEND=x86 -DEVS_WITH_GSTREAMER=OFF \
  -DEVS_BUILD_LEGACY_TOOLS=ON
cmake --build /tmp/edgevision_x86 -j4 --target evs_tests eval_offline
ctest --test-dir /tmp/edgevision_x86 --output-on-failure
/tmp/edgevision_x86/eval_offline \
  --task /home/tronlong/lyp/code/edgevision/configs/eval_demo.yaml \
  --dataset /home/tronlong/lyp/code/edgevision/tools/eval_offline/fixtures \
  --iou 0.5 --score 0.45 --dump --out /tmp/edgevision_eval_report
```

真实模型配置在上述基础上改用 `-DEVS_BACKEND=onnx -DEVS_WITH_ORT=ON`，并指定 ORT 目录。`configs/eval_onnx.yaml` 当前保留 Windows 绝对模型路径，Linux 上需改用本机路径的配置副本；不要假设原样可运行。Windows DLL 需对运行程序可见。

评测数据和指标约定：

- 数据集为 `images/*.ppm`（P6）和 `annotations.json`；框使用原图归一化中心坐标 `x,y,w,h`，类别字段为 `class`。
- `gen_fixtures.py` 使用 Python 标准库，实际输出到**脚本所在目录的 `fixtures/`**，不是当前目录；已有 fixtures 可直接读取，不必重复生成覆盖。
- CLI 输出 `report.json`、`report.csv`、`images.csv`，`--dump` 可输出对比图。显式指定 `--out /tmp/...`，否则可能向已入库数据目录写报告。
- 按类别和 IoU 贪心匹配，输出 P/R/F1、VOC all-point AP、mAP 和 micro P/R。合成集故意包含 FP/FN，不应要求所有指标为 1。
- 模型自产标注只用于移植一致性自检；评价现场精度需独立人工标注。
- `OfflineEvaluator` 执行检测链路，不等同于 `VisionTask` 的人员区域、工况和连续帧告警测试。

### 4.4 RKNN 与交叉编译

- 使用仓库工具链文件，准备 `aarch64-linux-gnu-g++`、`rknn_api.h`、目标架构 `librknnrt.so`。更换工具链必须使用新构建目录，不能指望修改已有缓存里的 `CMAKE_TOOLCHAIN_FILE` 生效。
- 库构建使用 `EVS_BACKEND=rknn`、`RKNN_API_LIB`、`RKNN_API_INCLUDE_DIR`；纯库交付关闭 GStreamer/RGA，构建核心及所需插件目标，例如动态人员功能的 `evs_shared evs_person_shared`。
- 默认LEGACY_TOOLS=OFF，可直接配置纯库。显式启用旧工具时，`--target evs evs_shared`仍不能绕过配置阶段错误。父项目add_subdirectory不会自动建立常规测试。
- `/home/xp/...` 是历史编译机路径，不能当成本机已安装依赖。显式核对实际 SDK、sysroot 与架构。
- RKNN 运行库应与板端匹配；历史记录指出 SDK 旧 runtime 与板端版本不一致。交叉链接使用编译机上的目标库副本，板端实际使用自己的兼容 runtime。
- RGA 库和头文件必须配套。历史文档指出精简版 RGA 可能符号齐全却缩放失败；不要只凭“链接成功”认定硬件预处理有效。
- 用 `file` 检查产物为 ARM aarch64；动态宿主需能找到 `libevs_core.so.1` 及所需插件的 `.so.1`。插件 RUNPATH 为 `$ORIGIN`，核心未写入编译机 SDK RPATH，宿主仍负责自身直接依赖及 SDK 的运行时搜索路径。
- 公共结构布局或接口变化时必须同步头文件并重编宿主；只有内部实现变化且 ABI 兼容时，才适合只替换动态库。

## 5. 公共 API 与数据契约

### 5.1 生命周期、所有权与并发

- `VisionTask` 不可复制。通常每个功能创建一个实例，以对应 YAML 初始化。
- `initialize(path)` 成功后再次调用直接返回 0，不切换配置或重新加载；换模型需先 `release()` 后重新初始化，或使用新实例。
- 初始化通过模型租约回滚已取得的引用和半初始化状态；冰雪启用warmup时要求成功，旧插件仍只警告。`release()` 可重复调用，析构也会调用。
- `detect()` 为同步调用；同一实例一次只处理一帧。不要并发执行该实例的检测、更新和释放。
- `ModelRegistry` 以 `(backend_type, model_path)` 共享后端并有互斥保护；这不等于共享 SDK 上下文的 `infer()` 自动线程安全。跨实例共用后端的并发需另行核对/串行化。
- 路径字符串决定共享键，不会自动规范化为同一物理文件；同一后端的输入配置也可能被不同任务设置，修改共享模型机制时需考虑这些约束。

### 5.2 Frame、预处理与 Prepared 路径

- `Frame::data` 由宿主拥有，在同步检测返回前必须有效；尺寸填写真实原图宽高，不能填写模型输入尺寸。
- 支持 `NV12`、`RGB`、`BGR`。`stride` 单位是字节；0 表示紧凑排列。NV12 的 Y/UV 使用相同行跨距，RGB/BGR 紧凑行为 `width*3`。
- CPU 预处理实际支持 RGB/BGR 直接输入，不需要仅因旧宿主文档的描述而强制先转 NV12。
- `dma_fd` 不是当前 CPU 预处理的数据替代品；`acceptFrame()` 仍要求非空 `data`。NV12 宿主需保证色度格式、尺寸与缓冲大小有效，现有接口没有长度字段替调用者验证全部内存边界。
- CPU 输出为 packed uint8 NHWC RGB/BGR，共 `input_width*input_height*3` 字节；letterbox 灰边为 114，映射由 `PreprocessMap` 保存。
- `detectPrepared()` 按值取得张量，可用移动语义交付；张量大小和 `map.in_w/in_h` 必须匹配配置。原始 `Frame` 仍用于尺寸、相机号和可能的图像规则处理。
- Prepared 路径要保持颜色顺序、letterbox、scale/pad 与 CPU 路径一致；避免再次预处理或重复映回坐标。
- 输入尺寸未配置为正数时，现有任务流程保留空张量/单位映射的旧行为，不等于任意真实模型都能接受这种输入。

### 5.3 工况、目标与结果

- 当前 `MachineContext::context_valid` 默认 **true**，`focal_length` 默认 0；已经没有旧角速度字段。人员作业判定等价于 `context_valid && (is_working != 0 || hoist_moving || telescope_moving)`。
- `focal_length` 当前不参与人员作业判定；`work_motion_epsilon` 保留兼容字段但 `is_working()` 不使用它。不要根据旧 README 恢复角速度逻辑。
- `Target.x/y` 为框中心，`w/h` 为宽高，最终相对原图归一化。UI 左上角应由 `(x-w/2, y-h/2)` 换算。
- 地面坐标 `ground_x_m/ground_y_m` 使用米，可信度由 `ground_ok` 表示。未知距离为 **-1**，不是 0；`track_id` 当前未实现跟踪，默认 -1。
- `AlarmState`：`None=0`、`LogOnly=1`、`Alarm=2`。`limit_state`：0 未知、1 未安装、2 装错、3 装对。`ice_ratio` 为面积占比。
- `Result.camera_channel` 每字节一个相机号，低字节为本帧，其余用 `0xFF` 填充。0 是真实相机号；当前一次 detect 只填一路，多路合并由宿主完成。
- 错误码为 `0xMMMMEEEE`，高 16 位为功能号。`with_module()` 保留成功 0 和已带模块的错误；私有错误使用 `make_module_error()` 的 `0xC000..0xFFFF` 区间。
- functionPlugin返回可写借用指针，不是只读接口；正式Detector无人工框入口。
- 调用方以返回值判断成功；冰雪hooks会清拒帧旧结果，旧插件前置失败仍未必重置Result。
- 未知 decoder 在 `VisionTask::initialize()` 返回 `ModelInvalidFormat`，未知 plugin/backend 返回 `NotSupported`；不要照旧文档把所有不支持都混为同一码。

### 5.4 最小接入示例

下面的配置和程序用于验证“初始化 → 输入一帧 → 读取结果 → 释放”的调用方式。它使用 mock，不加载真实模型，预期返回成功、目标为空、无告警；不用于检验检测精度。

把任务配置保存为 `/tmp/edgevision_minimal.yaml`：

```yaml
task: minimal_demo
module_id: 0x0004
camera:
  video_channel: [0]
backend: mock
Model_path: demo.mock
output_format: flat6
input_width: 64
input_height: 64
input_format: rgb
letterbox: true
ground:
  enabled: false
```

这里显式关闭地面映射，避免基础接入示例依赖现场标定文件。真实人员区域告警需要配置可靠的地面映射。

```cpp
#include <cstdint>
#include <iostream>
#include <vector>

#include "evs/vision_task.h"

int main() {
    evs::VisionTask task;
    if (task.initialize("/tmp/edgevision_minimal.yaml") != 0u)
        return 1;

    // 像素缓冲必须在 detect() 返回前保持有效。
    std::vector<uint8_t> pixels(64 * 64 * 3, 128);
    evs::Frame frame{};
    frame.camera_id = 0;
    frame.width = 64;
    frame.height = 64;
    frame.stride = 64 * 3;
    frame.format = evs::PixelFormat::RGB;
    frame.data = pixels.data();

    evs::MachineContext ctx{};
    ctx.context_valid = true;
    ctx.is_working = 1;

    evs::Result result{};
    const uint32_t status = task.detect(frame, ctx, result);
    if (status != 0u)
        return 2;

    std::cout << "targets=" << result.targets.size()
              << " alarm=" << static_cast<int>(result.alarm) << '\n';
    task.release();
    return 0;
}
```

示例需使用 C++17、仓库 `include/` 头文件和已构建的库。若链接静态库，按 4.2 保留全部登记对象；切换真实后端时同步调整构建选项、任务配置和模型文件。

## 6. 功能规则与完成程度

### 6.1 五类功能的当前状态

| 功能号 | 配置 | 当前行为 |
| --- | --- | --- |
| `0x0004` | `load_near.yaml` | 圆形区域，默认 10 m；无工况行为由配置决定，该样例为 `alarm` |
| `0x0005` | `load_under.yaml` | 圆形区域排除载荷遮挡扇区；无工况只记录 |
| `0x0006` | `boom_under.yaml` | 半径 `max(50, hoist_radius)` 的扇形，轴线暂固定 0°，默认半角 40° |
| `0x0007` | `height_limit.yaml` | 没有框输出 `limit_state=1`；保留最高置信度框并调用安装姿态入口，当前有框仍返回 0 |
| `0x0008` | `winch_ice.yaml` | RKNN定位卷扬＋框内HSV候选占比；任一有效对象≥阈值报警；每帧重算，无工况门控/锁存 |

### 6.2 人员告警与连续帧规则

1. 距离未知的框保留并仅记录，不进入报警区域。
2. 工况无效时，只有已映射到区域内的目标且策略为 `alarm` 才立即报警；未知距离不会凭“检测到人”直接报警。
3. 工况有效但静止时，区内目标只记录并清除连续帧计数。
4. 作业时区内有人连续计数，达到 `consecutive_frames` 报警。只有未知距离目标的帧既不推进也不清零；明确无区内目标且无未知目标时清零。
5. 热更新插件配置不清除连续计数；`reset()`/释放清除。计数目前属于整个实例，不是每个相机一份；修改多路策略前先确认业务期望。

### 6.3 剩余边界与冰雪接入约束

- 高度安装几何、观测窗口、基本臂规则及crane_pose仍占位；致盲、通用跟踪、QNN仍无实现。缺绝对回转角，人员task参考吊钩位置/扇形轴线仍有临时假设。
- 冰雪真实路径为单类YOLOX模型＋OpenCV矩形框内HSV粗估，非真实表面分割。正式Detector一实例一路、紧凑RGB24/灰度、同模型实例也须串行；工况、重试、告警保持由宿主负责，exit_on_alarm仅兼容保存。
- 结果读WinchSnowResult.objects的valid/ice_ratio，编号仅帧内排序；snow_detections始终空。通用Result.targets被清空，ice_ratio仅单有效对象有值，多对象/无/无效均-1。
- NMS后超过两个候选整帧OutOfRange；小于3×3、交叠、极暗/均匀极白对象无效，detect仍可能返回0。HSV默认S≤80、V≥180，3×3开闭运算，分母包含框内背景；反光误计/暗雪漏计及模型误检仍待现场精度验收。
- 公共TaskConfig.area_ratio仍.3；客户DetectConfig及冰雪文件默认.5。专用updateConfig整体更新置信度、NMS、面积阈值，非法值保留旧配置。
- 冰雪细节见[报告5.10及9.6](edgevision分析报告.md#510-winchiceplugin卷扬定位与框内冰雪候选面积)；UI与CLI选路差异见报告6.7。

## 7. 地面坐标与标定

### 7.1 相机配置与矩阵加载

- mapper 按相机独立配置；`camera.video_channel` 各路应有对应 `ground.cameras[].id`。漏配会警告，检测可继续但该路距离未知；重复/非法相机配置由构建 mapper 阶段校验。
- 其它功能未写`ground:`时，兼容相机 0–3，读取 `/userdata/matrix_<id>_1280x720.txt`，缺失可回退各路内置矩阵。
- 显式提供矩阵路径会默认按严格模式处理；现场样例还明确写 `params.strict_matrix: true`。因此即使后端为 mock，缺标定文件也可能初始化失败，不代表 mock 需要读取模型。

### 7.2 单应投影、距离与角度

- homography 用原图归一化框的底边中心投影到标定图尺寸，矩阵世界单位为厘米，输出除以 100 转米；非有限值、退化投影或无效前向点返回 unknown。
- 静态 mount 可做 XY/yaw 变换；`on_boom`、`offset_m` 并未在 homography 实现动态臂架公式。
- `legacy_forward_y` 使用参考点前向分量，角度实际归一化为 `(-180°,180°]`；`radial` 使用二维欧氏距离，角度 `[0°,360°)`。地面角从 +Y 向 +X 顺时针计算，不要与其它图像几何辅助函数的轴定义混淆。
- `ground.reference: task` 必须搭配 `distance_mode: radial`，否则解析失败。`camera_origin` 保留历史参考方式。
- 新 mapper 参数放入 `ground.cameras[].params`，由对应 `configure()` 读取、校验；公共解析器保存原始字符串，已有 `ground_param_*` 辅助函数支持标量和数组。
- 校准坐标、地面坐标、任务参考坐标和 UI 像素坐标必须明确区分，禁止通过填 0 伪造未知距离。

## 8. 配置解析与热更新

### 8.1 YAML 的真实能力

- 这是自写轻量解析器，不是完整 YAML 库。支持简单标量、常用单层嵌套、内联整数数组，以及专门解析的 `ground.cameras` 映射列表。
- 常用键包括 `task`、`module_id`、`camera.video_channel`、`backend`、`Model_path`、输入参数、解码参数、阈值、`zone`、`ground`、`debug` 及两个专属插件参数。
- 兼容 `model`/`Model_path`、`camera_ids`/`video_channel`、`score_threshold`/`confidence_threshold`、`thresholds.score`；样例优先保持现有规范拼写。
- 解析器并不严格拒绝全部未知键/非法数值，部分转换会默认为 0 或回退默认值；“成功解析”不代表每个字段有效或已参与算法。
- `module_id` 决定功能；`task` 文本主要用于标识，改文件名或 `task:` 不会切换算法。
- load_config成功不等于冰雪参数范围/模型检查通过；仍须检查initialize返回值。其它功能的`VisionTask`模型及标定相对路径按进程当前目录；加载冰雪库后，其文件适配器将模型相对路径按YAML目录归一化。直接构造ModelConfig/TaskConfig仍不经过文件适配。离线评测器还会在原路径不存在时尝试数据集目录下的模型路径。
- `camera_config.json` 属于宿主采集配置，与任务 YAML 是两套配置；仓库样例是 JSON 数组，不要用说明文档中的单个对象片段覆盖完整数组。

### 8.2 热更新以实现为准

`updateConfig()` 重新加载一份完整配置，不是局部补丁。新文件未写的字段会回到默认值，可能导致契约比较失败或意外重置策略。

| 分类 | 当前实际处理 |
| --- | --- |
| 明确拒绝变更并返回 `NotSupported` | `backend`、`Model_path`、`output_format`、`decoder_classes`、`person_class_id`、`module_id` |
| 实际应用 | 人员阈值与 `zone`、无工况策略、`work_motion_epsilon`、完整 `ground`、延迟预算、高度/冰雪专属参数、`debug` |
| 输入宽高、颜色格式、letterbox、`video_channel` | 人员/高度仍未比较也未应用；冰雪hooks拒绝改变 |

- 人员/高度输入与相机热更新仍未拒绝也未应用，改这些字段应重新初始化；冰雪通过可选hooks拒绝，不能混写。
- ground mapper 先在临时容器构建，插件 `configure()` 成功后再提交；失败保留现有有效配置。新增插件的 `configure()` 也应先完整验证再修改状态。
- 旧COCO helper的NMS仍用内部常量；冰雪插件自有NMS实际使用并支持热更新nms_threshold。
- YAML `npu_core_mask` 尚未透传 RKNN，实际使用 `EVS_NPU_CORE_MASK`。RKNN `init_timeout_s` 也没有形成 SDK 调用超时机制；`min_fps` 不是自动调速保证。
- 文件改动不会被自动监控；必须由宿主显式热更新或重建任务。

## 9. 后端与解码约束

### 9.1 编译选择与输出格式

- mock 始终编入。其他后端由 CMake 编译选择，YAML 不能凭空启用未编入的后端；`EVS_BACKEND=x86` 也不自动接受 `backend: onnx`。
- `flat6` 每条 `[class_id, confidence, center_x, center_y, width, height]`；是旧单类输出约定，不代表自动实现了人员类别过滤。
- `yolov8`/`yolo11`/`yolox` 上层接收归一化的 `[N,4+C]` 行矩阵，取类别分数 argmax 后再按 `person_class_id` 筛选；`-1` 表示保留全部类别。
- decoder 还登记 `yolo11_raw`、`yolo11_decoup` 别名，但它们同样接收后端已处理的行矩阵。不要因名字带 raw 就在上层再次拆九头或执行 sigmoid。

### 9.2 更换模型时必须核对的契约

- RKNN的旧COCO原始头解析在`src/backend/yolo_decode.h`，新非80类显式解析在`src/backend/rknn/detection_rows.h`，随后交上层decoder。YOLOv5 anchor 头、YOLOv11 DFL 解耦头、YOLOX packed/per-scale 输出在这里分派。
- 旧COCO helper仍固定80类/640/anchors；新增rknn且classes!=80的flat6/v8/YOLOX显式单输出路径，校验模型输入/行宽/元素数与数值。正式冰雪640单类YOLOX raw为[1,8400,6]、概率已激活，输出归一化[N,5]，NMS在插件执行。换模型核对具体分支，不能泛化任意raw布局。
- 类别数/行宽不匹配可能安静地产生 0 框。模型结构、输出激活、输入缩放、颜色顺序、类别号与 letterbox 映射应一起核对。
- 旧COCO helper的NMS常量：YOLOv5/v11 路径 IoU 0.45；YOLOX 路径 IoU 0.5 且不分类别。改共用解码需同时评估 PC/板端一致性。
- RKNN 按模型输入属性处理张量并将输出取为 float；float 输入缩放与模型预处理是否已融合有关，不要无证据重复除以 255。
- ORT 实现文件在本次快照中无法作为明文阅读，不能据旧文档推断所有模型布局都已支持。

## 10. 采集、共享内存与排障

### 10.1 工具的用途

- `v4l2_detect` 用于预处理、推理和框输出验证，没有调用 `VisionTask` 功能插件；屏幕有框不能证明人员区域告警已运行。
- `evs_task_cam` 会调用完整任务，采集线程保留最新帧，允许覆盖来不及消费的旧帧；示例工况是固定有效/作业值，产品需替换为真实中控数据。
- `v4l2_detect` 支持裸 NV12、直接设备、JSON 选路、`/picshare<N>` 输入和 MP4 关键帧等方式；以实际 CLI 解析及 `capture.cpp` 为准。公共采集注释未覆盖全部 native pixel-format 路径。
- `--shm N` 是图像输入，`--shm-write N` 是检测框输出，不能混淆。
- 板端调试先用固定原始帧隔离采集差异，再比较实时管线、CPU/RGA 和推理耗时。

### 10.2 检测框共享内存契约

`tools/v4l2_detect/detection_shm.h/.cpp` 与外部 Qt `DetectionBoxShmBridge` 对齐：

- POSIX 名称 `/detection_boxes`，magic `0xDE7EC710`，版本 1。
- 4 路，每路最多 64 框；单框 24 字节，通道块 1620 字节，头部起始偏移 16，总大小 6496 字节；已有 `static_assert` 约束布局。
- 使用 seqlock：写前 seq 变奇数、提交后变偶数，保留原子操作及内存序。每路只应有一个写者。
- Target 中心归一化坐标在发布端转换为原图像素角点；`mappedY` 使用厘米，非正值表示 unknown，与核心库的米制区分。
- 重连保持已有 seq 连续性；退出/丢失时使通道无效，关闭时不主动 unlink 仍被读者使用的段。修改布局必须同步消费者。
- `S99evsdetect` 是示例启动/状态/看门狗入口；watchdog 需要外部周期调用，不是脚本内置常驻监控。其默认二进制名与 `eval_yolo11.yaml` 也不是当前 CMake/配置目录必然产出的文件。

### 10.3 常用诊断项

| 项目 | 用途及限制 |
| --- | --- |
| `EVS_LOG_LEVEL`、`EVS_LOG_FILE` | 运行期日志级别与追加文件输出；编译期还受 `EVS_LOG_LEVEL` 宏裁剪 |
| `EVS_YOLO_HEAD_MODE` | 0 原始 logits、1 objectness/class 已激活但框仍未解码、2 完整 Detect 后处理已融合、3 auto；需结合实际模型输出核对 |
| `EVS_RKNN_HEAD_MODE` | RKNN 端模式选择，优先于通用变量 |
| `EVS_YOLO11_SIGMOID=1` | YOLOv11 原始 logits 的诊断开关，已含概率时避免重复激活 |
| `EVS_YOLOX_SIGMOID=1` | YOLOX logits 处理开关 |
| `EVS_YOLO_DUMP`、`EVS_RKNN_DEBUG` | 检测/原始头诊断；部分代码只检查变量存在，设为 `0` 仍可能开启，应取消变量以关闭 |
| `EVS_RKNN_INPUT_SCALE` | float RKNN 输入缩放，如 1 或约 1/255；结合模型转换契约使用 |
| `EVS_NPU_CORE_MASK` | 实际 RKNN 核亲和；unset/0 使用驱动默认选择 |
| `EVS_GROUND_MODEL` | 调试时覆盖所有相机 mapper，带警告，正式配置优先写 YAML |
| `EVS_RGA_CSC` | 采集侧预处理策略，默认 0；用 CPU 对比诊断颜色差异 |
| `EVS_RGA_NOPAD` | 诊断强制零 padding，可能破坏真实几何语义，不作生产修复 |
| `EVS_DUMP_BENCH`、`--bench` | 导出 CPU/RGA PPM 并比较耗时和像素差异 |

### 10.4 RGA 与调试接口的使用边界

- RGA `wstride` 单位是像素，自行计算缓冲地址/letterbox 偏移使用字节；RGB 的两者相差 3 倍。保留 `dst_stride_px`、`dst_row_bytes` 的区分。
- `--rga-fd` 依赖相机导出 DMA buffer 的能力；历史板卡不支持该路径，不能等同于常规 `--rga` 已验证。
- 插件decide失败会在核心dbg.note之前返回，不计入frames/error；专用inference_time_ms此时可为0。
- 调试 API 包括 `debugStats()`、`debugSnapshot()`、`setDebugConfig()`、`saveLastInput()`、`library_snapshot()`。默认不缓存每帧输入张量、不落盘，但基础统计仍会更新，不能把文档的“零开销”当作绝对事实。
- 取证受 `enabled`、间隔/告警策略和 `max_dumps` 限制；性能修改避免无条件张量拷贝、逐框日志或无限落盘。
- 全局日志 sink 不得长期引用已析构局部对象；退出作用域前解除回调。
- `library_snapshot()` 的顶层 backend 标签当前优先命中始终定义的 mock 宏，不能仅靠该标签判断实际任务后端；核对任务快照和模型注册表。

## 11. 已核实的现存问题与文档偏差

当前基线为5427d18，完整证据与历史区分见[报告8、12章](edgevision分析报告.md#八编译构建体系)。本次只读源码并在/tmp验证，未修复源码。

1. 默认旧工具关闭，普通x86配置可过；开启LEGACY_TOOLS后CMakeLists.txt:416仍报keyword/plain签名混用，zlib和旧GStreamer仍有历史绝对路径。
2. x86_backend.cpp及gen_selfcheck_dataset.py仍带E-SafeNet保护头；需授权正常明文来源，普通x86编译未通过。build_check.bat仍过时。
3. README等旧工况、热更新、fixtures、mapper说明可能落后，按模块/路径核对；不能再沿用“冰雪骨架”“OpenCV只加宏”“没有install”的旧结论。
4. 冰雪模型已入库但install不部署；人员/高度库也不在安装集合。纯VisionTask动态宿主仍防as-needed裁掉插件，Detector直接引用ice符号。
5. 新Qt UI严格选通道，单路同帧显示；相机CLI复用旧selector，缺失channel会回退、禁用项可能仍使用，存在采错设备但标任务通道的路径。UI保证不能推广到CLI。
6. 冰雪当前是HSV粗估，snow_detections为空；真值精度、其它物理通道及长时稳定性未验收。历史通道0 7/7、1000帧等本次未重跑；源码包哈希边界见报告8.7。
7. 人员/高度输入热更新、评测异常图统计/mAP口径、旧共享内存首次创建等问题未由本提交修复。
8. 新旧插件兼容只验证特定v1头及同工具链；模型共享不等于infer线程安全，仍无自动dlopen插件管理器。

## 12. 实施改动时的约定

### 12.1 根据职责定位改动

| 需求 | 首选位置 |
| --- | --- |
| 相机节点、分辨率、帧率、采集格式 | 宿主 JSON、`tools/v4l2_detect/capture.*` 或实际产品宿主 |
| CPU 预处理/坐标回映射 | `src/core/preprocess.cpp` |
| RKNN/ORT 输入输出与模型加载 | 对应 backend；原始 YOLO 头解析在 `yolo_decode.h` |
| 新输出行格式 | 新 decoder 实现、静态登记、`EVS_SRC` |
| 相机标定或新地面算法 | `src/ground/` 对应 mapper 与 YAML params |
| 距离/角度的统一定义 | `src/ground/ground_metrics.cpp` |
| 任务参考点和人员规则 | `src/tasks/person/person_plugin.cpp` |
| 高度安装判断 | `src/tasks/height_limit/height_limit_mount.cpp` |
| 冰雪面积及退出策略 | `src/tasks/winch_ice/` |
| 新功能 | 新 `IFunctionPlugin`、模块号登记、`evs_add_plugin()` 或所属插件源列表、配置及必要测试 |
| 框输出到 Qt | `detection_shm.*`，同时核对外部消费者协议 |

### 12.2 扩展流程

1. 新功能实现 `IFunctionPlugin` 的 `configure()`、`decide()`、`reset()`；需要地面量时实现 `ground_reference()`，不要复制公共地面换算。
2. 新功能在自身 `.cpp` 用 `FunctionPluginRegistration` 登记唯一模块号；新 decoder 用 `DecoderRegistration`；新 mapper 用 `GroundMapperRegistration`。
3. 新 decoder/mapper 等核心编译单元加入 `CMakeLists.txt::EVS_SRC`；功能编译单元加入所属 `evs_add_plugin()` 源列表，新插件组新增该调用，并维护 `evs_link_whole()` 的归档列表。核对外部静态消费者的整库链接，以及动态消费者对插件的保留和部署。
4. backend 是例外：仍需修改后端工厂及 CMake 编译选择，并保持 mock 始终可用。
5. 新配置明确初始化/热更新语义；功能私有配置优先独立FunctionConfigRegistration和FunctionOptions，可选IFunctionTaskHooks，不扩张原TaskConfig/插件虚表；mapper仍用params。
6. 只在相应实现目录引入可选 SDK/OpenCV 代码，公共接口保留 C++17 类型及现有依赖隔离。

### 12.3 风格、验证与交付

- 延续现有四空格缩进、`namespace evs`、头文件 `#pragma once`、局部匿名 namespace、RAII/智能指针风格；命名与附近代码一致。仓库未见统一格式化配置，不做无关全库格式化。
- 错误通过现有 `uint32_t`/`EvsStatus` 契约传播；清楚区分未初始化、配置错误、模型错误和推理失败。
- 避免将现场差异堆到 `VisionTask`；它负责装配和流程，具体算法留在插件/mapper/decoder/backend。
- 修规则时增加能证明行为的回归：连续帧边界、静止/无工况、未知距离、遮挡、热更新失败回滚；改预处理验证色序、stride、letterbox 和框回映射；改解码验证形状、激活、类别与 NMS。
- 使用现有 `tests/test_core.cpp` 或必要的新测试目标。文档/低影响配置调整无需为实现细节新增机械测试；代码变动至少运行对应 mock 基线，涉及真实后端需补说明实际验证范围。
- 真实模型或板端功能以固定帧对照、离线报告和实际设备测试为证；不能用 mock 通过代替硬件验证，也不能声称历史性能数字是本次结果。
- 不向版本库提交构建树、SDK 库、测试临时文件、调试图、个人绝对路径或新的备份文件。已有 fixtures 与报告是跟踪文件，避免运行工具时无意覆盖。
- 尊重已有未提交变更，编辑前后看 `git diff`/`git status`；不自动提交、清理模型、修改远端或部署板端服务。
- 若任务实际需要修改外部源码，先核对当前写入权限；本初始化仅授权在当前路径生成指南，不代表本次已修改或部署源码。
- 提交说明沿用仓库简短中文 `feat:` 等惯例。交付时说明改动内容、行为影响、实际执行的检查及剩余限制。

## 13. 后续接手的最小检查顺序

1. 确认当前所在目录与实际源码目录，读取适用指令，检查 Git 状态及最近提交。
2. 按需求读取对应公共契约、实现、配置和局部待办，不只依赖 README。
3. 先选合适后端和独立构建目录；需要x86/onnx/rknn或冰雪时，先核对第11节及相应依赖/旧工具开关。
4. 完成范围明确的改动，更新必要样例和说明，并运行匹配的验证。
5. 检查最终差异及生成物，报告已验证事实；区分“代码完成”“仅 mock 验证”“需要模型/板端验证”。
