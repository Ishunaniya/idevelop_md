# sep-faces 项目说明

## 项目位置与用途

- Git 仓库根目录：`/home/tronlong/lyp/code/skes`；本项目源码目录：`skes-apps/sep-faces`；本文档存放目录：`/home/tronlong/lyp/perCode/idevelop_md/skes/sep-faces`。顶层构建进入 `skes-apps/`，其 `CMakeLists.txt` 仅在目标处理器不是 x86/x86_64 时纳入 `sep-faces`。
- 编写本文档时检出的 Git 分支：`main_ui`，即此工作树当前的代码版本线。切换分支或更新源码后，请重新核对本文档。以下描述以当前工作树实际代码和配置为准，特别是设计文档中尚未落地或已改变的行为。
- `sep-faces` 是 SKES 系统中的独立人脸识别进程。它从摄像头共享内存读取 BGR 图像，使用人脸检测模型定位人脸、提取特征，与启动时由照片建立的内存图库比对，通过 SKES 消息通道周期发布识别结果和安全带状态。
- 该进程依赖上游程序持续写入 `/picshare<通道号>` 共享内存，并依赖本机消息服务和 DI 状态服务。`sep-faces` 自身不采集摄像头，也不负责安全带传感器驱动。

### 相关技术和名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| Git 分支 `main_ui` | 当前检出的开发分支名，用于标识本文档对应的代码状态；不是运行参数。 |
| SKES、SEP | SKES 是上级系统；SEP 是应用管理/启动配置机制。本项目的 `sep.ini` 声明 `name=sep-faces`，目前 `enabled=false`，部署后不会因该配置自动启用。 |
| CMake、交叉编译、RPATH | CMake 定义 C++11 可执行文件、依赖和安装规则；交叉编译是在开发机上生成 ARM 设备程序。RPATH 决定部署后运行时动态库的搜索位置。此项目 CMake 中含 RK3576 SDK 的绝对路径，迁移开发机时需调整。 |
| BGR、OpenCV | BGR 是摄像头共享内存和 OpenCV 图像矩阵采用的颜色通道顺序。OpenCV 负责读图、旋转、裁剪、缩放、模糊度计算和人脸对齐；模型输入颜色可由配置控制。 |
| POSIX 共享内存、`/picshare4` | 使用 `shm_open`/`mmap` 读取上游生成的图像共享内存。`picshare_channel=4` 时读取 `/picshare4`；读端检查帧序号和元数据，避免重复读取或读取写入中的帧。 |
| RKNN、RKNPU2、RGA | RKNN 是 Rockchip NPU 模型格式和推理接口；RKNPU2 提供运行库，RGA 是 Rockchip 图像处理加速库。当前配置选择 `rknn`，构建脚本链接 `librknnrt` 与 `librga`；此目录的推理源码使用 RKNN API 和 OpenCV，未见直接调用 RGA API。 |
| RetinaFace、MobileFaceNet/ArcFace | 前者检测人脸及关键点；后者从对齐的人脸生成特征向量（embedding），供身份比对。默认模型文件分别是 `models/RetinaFace.rknn` 和 `models/mobilefacenet_arcface.rknn`。 |
| embedding、余弦相似度 | embedding 是人脸的数值特征；`FaceEngine` 比较当前特征与图库特征的余弦分数，并同时检查最高分阈值与前两名分差阈值。 |
| 关键点、对齐、NMS | 关键点是眼、鼻、嘴等位置；对齐将人脸摆正后提特征；NMS（非极大值抑制）合并重叠的人脸检测框，检测置信度、NMS 阈值及检测框上限在 `config.json` 中。 |
| 多帧投票、冻结 | 多帧投票要求同一身份在最近若干帧中累计达到票数才认定匹配。配置允许匹配后冻结识别；当前代码同时要求 `freeze_on_match` 和 `freeze_on_seatbelt_fasten` 为真才进入冻结态，DI 状态转为未系安全带后解除。 |
| DI、安全带状态 | DI（Digital Input，数字输入）从本机 `tcp://127.0.0.1:38000` 的 nanomsg REQ/REP 服务获取；当前固定读取通道 0，并用 `seatbelt_fasten_di_level` 判断系带状态。 |
| skes-linker、nanomsg、cJSON | `skes-linker` 封装本机 SKES 消息收发；nanomsg 用于 DI 请求/应答；cJSON 解析配置和封装属性消息。 |
| `/skes/filter`、心跳 | SKES 的属性消息与心跳消息由 `skes-linker` 按 topic 类型收发。识别结果通过 filter 类型发布；收到心跳后返回响应。 |
| mock 后端 | 无真实模型推理的替代实现，主要用于接口/流程调试。RKNN 模型缺失或初始化失败时可能降级到它；其检测、特征生成是简化算法，不能等同真实识别能力。 |

## 主要文件与数据流

| 路径 | 职责 |
| --- | --- |
| `CMakeLists.txt` | 定义依赖、RKNN/OpenCV 路径、链接项、可执行文件和安装内容。 |
| `src/main.cc` | 初始化日志、版本输出、信号处理，并启动/停止 `SepFaceEntry`。 |
| `src/sep-face.cc`、`src/sep-face.h` | 解析配置，建立图片图库，处理消息、识别循环、多帧投票、周期发布与安全带状态线程。 |
| `src/picshare_reader.cc` | 从 `/picshare<channel>` 共享内存读取 BGR 帧。 |
| `src/face_backend_rknn.cc`、`src/face_backend_mock.cc` | 真实 NPU 推理后端和简化调试后端；前者处理检测、关键点/对齐和特征提取。 |
| `src/face_engine.cc` | 选择后端，以相似度和阈值判断图库匹配结果。 |
| `src/face_db.cc` | 旧式二进制人脸库的加载、备份恢复、增删改查；当前主识别循环使用启动时由图片建立的 `image_db`。 |
| `src/di_get.cc` | 向本机 DI 服务查询数字输入通道状态。 |
| `config.json`、`sep.ini`、`sep-faces.ini` | 运行参数、SEP 启动配置和日志等级。 |
| `models/`、`docs/` | RKNN 模型；需求、设计和优化记录。`docs/设计文档.md` 含方案及历史描述，现状以源码和配置为准。 |

主数据流：上游摄像头进程 → `/picshare4` → 可选旋转 → RetinaFace 检测/对齐 → MobileFaceNet 特征 → 图片内存图库匹配 → 多帧投票 → SKES filter 消息发布。DI 线程并行查询安全带状态，消息线程处理心跳与兼容性输入。

## 构建与部署

- 本项目通过 SKES 顶层 CMake 构建。`skes-apps/CMakeLists.txt` 在非 x86/x86_64 目标上添加此目录；本目录声明 CMake 最低 2.8.12，但同时使用 `target_link_options`（需要更新版本的 CMake）；顶层声明最低 3.10。本目录设定 C++11，并查找 OpenSSL、curl、zlib、cJSON、nanomsg、skes-log、skes-linker 等依赖。还链接 OpenCV、RKNN、RGA、pthread 等库。
- `CMakeLists.txt` 将 RKNPU2 和 OpenCV 路径硬编码在本机 RK3576 SDK 下。换工具链或 SDK 路径前需检查 `RKNPU_PATH`、`OPENCV_DIR`、`LIB_ARCH` 以及相关库是否存在；具体顶层交叉编译方法以仓库构建文档为准。
- 安装规则把可执行文件、`config.json`、两个 INI 文件放到 `apps/sep-faces`，两个 RKNN 模型放到 `apps/sep-faces/models`，并安装 cJSON 动态库。最终绝对路径由顶层安装前缀决定。
- `sep.ini` 当前为 `enabled=false`、`param=`，需要由部署配置决定是否启用。程序读取相对路径 `config.json`、`sep-faces.ini` 和模型路径，启动时应核对工作目录与实际文件位置。

## 当前配置与接口核对

- 配置优先级：`src/sep-face.cc` 先设源码内置值，再读取工作目录下 `config.json` 的 `face` 节覆盖。因此“源码默认值”和“仓库当前配置”并不总相同，例如源码默认 `face_images_dir` 为空、`seatbelt_fasten_di_level=1`，而当前 JSON 分别为 `faces` 和 `0`。修改或删除 JSON 字段时应按这一规则核对实际值。
- 实时检测按 `detect_fps` 调节取帧间隔。当前持续识别路径使用 `recognize_vote_frames` 与 `recognize_vote_need`；`recognize_fps`、`boost_fps`、`boost_duration_ms`、`enroll_sample_count`、`enroll_max_frames`、`recognize_max_frames` 主要服务于遗留一次性操作代码，当前收到的旧操作指令不会触发该代码。
- 周期发布的 JSON 外层包含 `version`、`id`、`ts`、`action=post`、`type=properties`；`data.type=FaceRecognition`，其属性数组包含 `FaceRecognitionResult` 和 `seatbelt_fasten`，仅结果为 `1` 且取得照片名时增加 `face_id`。周期发布的 `id` 为空字符串；`publish_interval_ms=1000` 表示目标周期 1 秒，并非识别计算必定每秒完成一次。
- `seatbelt_fasten_di_level=0` 是当前配置的有效电平，DI 查询固定使用通道 0。DI 查询失败时线程仅记错误日志，保持已有状态；启动后的首次成功查询前不应把发布的安全带值当成已核实的传感器读数。
- 模型输入尺寸当前为检测 320×320、特征 112×112，特征维数配置为 512；检测置信度 0.5、NMS 0.45、最大框数 64；匹配阈值 `hit_threshold=0.45`、`margin_threshold=0.06`、最小人脸尺寸 40 像素、模糊度阈值 60。模型推理正确性还取决于模型自身的输入格式、颜色和量化预处理，不能只看 JSON 值。
- 加载图库后有 `face_db_selfcheck` 日志，记录各身份特征及两两相似度；`verdict=weak/COLLAPSED` 提示特征可能区分度不足，可优先核对 BGR/RGB 顺序、对齐、模型量化和照片质量。

## 运行与修改时关注

- 默认 `picshare_channel=4`；没有上游 `/picshare4` 图像帧时无法识别。默认 `backend_type=rknn`，若模型不存在或初始化失败会尝试 `mock`，应检查启动日志中的实际后端名。
- 当前 `config.json` 把 `face_images_dir` 设为相对路径 `faces`，上限 `face_images_max=10`；源码目录中没有 `faces/`，CMake 安装规则也没有安装照片。部署时需另行创建并提供可读取、可检测、可提取特征的 JPG/JPEG/PNG 照片。程序按文件名排序，启动时一次性建图库，`face_id` 取文件名去扩展名；增删照片后需重启。目录不存在或照片均处理失败时，内存图库为空。只改 `face_db.bin` 不会给当前主识别循环增加身份。
- `FaceRecognitionResult` 当前代码使用：`0`=未检测到有效人脸/特征，`1`=图库匹配且多帧投票通过，`-1`=有人脸但投票窗口未匹配，`-2`=检测到人脸但图片图库未就绪或为空。成功时可附带 `face_id`（照片文件名去扩展名）；消息也附带 `seatbelt_fasten`。外部协议对 `-2` 的接受情况需与消费端核对。
- 当前 `config.json` 使用 `recognize_vote_frames=7`、`recognize_vote_need=4`、`detect_fps=10`、`publish_interval_ms=1000`、`seatbelt_fasten_di_level=0`。调阈值和投票参数时一起查看 `hit_threshold`、`margin_threshold`、`min_face_px`、`min_blur_var`，并用实际现场样本验证误识别与漏识别。
- `camera_rotation=270` 表示将实时输入帧顺时针旋转 270 度（等价于逆时针 90 度）；启动建图库时读取的照片没有经过该旋转。更换摄像头安装方向时核对实时画面与图库照片的朝向。`detect_input_color=bgr` 等模型参数需与 RKNN 模型转换时采用的预处理一致。
- `freeze_on_match=1` 且 `freeze_on_seatbelt_fasten=1` 时，匹配后识别线程冻结；DI 通道 0 返回非 `seatbelt_fasten_di_level` 时解除冻结并将结果重置为 0。DI 服务不可用会影响安全带状态和解除冻结流程，联调时应验证。
- 默认发布端连接 `tcp://127.0.0.1:19225`，订阅端连接 `tcp://127.0.0.1:19226`；DI 查询连接 `tcp://127.0.0.1:38000`。`config.json` 虽设 `enable_filter_reply=1`，但当前接收路径只解析并记录旧 `CentralControlScreen` 人脸操作指令；设置待回复请求和执行一次性操作的函数均没有调用点。因此不能据此认为录入、删除、更新或带请求 ID 的应答已接通。
- `mismatch_images_dir` 当前为空，未匹配图像保存功能不会写入文件；启用后在投票窗口满而未匹配时会写入窗口中的图像，并按 `mismatch_images_max=100` 清理旧文件。`src/main.cc` 实际设置日志文件为相对路径 `sep-faces.log`、5 MiB/文件和最多 4 个文件，并从 `sep-faces.ini` 读取日志等级；`config.json` 顶层的 `log` 字段未在本项目解析。
- 本目录没有独立自动化测试目标。验证真实识别需目标板、RKNN 模型、摄像头共享内存写端、消息服务、DI 服务及样本照片；只用 mock 后端不能证明模型识别效果。
