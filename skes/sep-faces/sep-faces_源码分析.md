# sep-faces 源码分析

> 核对对象：`/home/tronlong/lyp/code/skes/skes-apps/sep-faces`，Git 分支 `main_ui`，2026-09-28 本轮核对时 `skes-apps/sep-faces` 目录树哈希为 `6af7866a35fe8b010e56f806613af0fa4331ef40`。目录树哈希用于固定本文对应的源码内容，不受仓库其他目录提交影响；上级 CMake、日志库和消息初始化函数另按本轮工作树核对。本文描述能从本目录源码、配置和上级 CMake 直接确认的行为；目标板上的模型精度、外部服务实现与真实时序未作推断。
>
> 本文流程图使用纯文本代码块，普通 Markdown 查看器无需 Mermaid 也可显示；每张图后均有文字版流程。

## 文档目录

专题编号沿用原报告，以保留“§N”和“第 N 节”的交叉引用；阅读顺序按下面的主题排列。

1. [一、项目概述](#overview)
   - [专题 1：项目边界与文件索引](#detail-1)
   - [专题 2：构建、安装与启动条件](#detail-2)
   - [专题 4：配置读取与当前实际值](#detail-4)
2. [二、源码目录总览](#source-tree)
3. [三、核心架构设计](#architecture)
   - [专题 3：启动、并行线程和退出流程](#detail-3)
4. [四、核心模块深度分析](#modules)
   - [专题 5：图像输入、图库和推理](#detail-5)
   - [专题 6：持续识别、投票与状态流转](#detail-6)
   - [专题 7：消息输入、遗留代码与持久化边界](#detail-7)
5. [五、专题详解与设备排障](#details)
   - [专题 8：可直接核验的限制与排查顺序](#detail-8)
   - [专题 9：设备现场排查手册](#detail-9)

<a id="overview"></a>
## 一、项目概述

`sep-faces` 是独立的人脸识别应用：读取上游写入的 `/picshare<通道号>` BGR 图像，用 RKNN 或 mock 后端检测、提取特征，与启动时从照片建立的内存图库比对，经多帧投票发布 `FaceRecognition` 属性；DI 查询线程并行维护安全带与冻结状态。它不采集摄像头，也不驱动安全带传感器。当前 `sep.ini` 为 `enabled=false`，是否运行须核对设备实际部署配置。详细边界见原第 1、2 节。

<a id="detail-1"></a>
### 1. 项目边界与文件索引

`sep-faces` 是独立可执行程序。输入是上游写入的 POSIX 共享内存图像、启动时读取的本地照片、DI 服务返回的数字输入状态，以及 SKES 消息；输出是 SKES filter 属性消息、心跳响应和日志。当前主路径没有摄像头驱动、拍照采集或在线录入功能。

| 文件 | 当前职责 |
| --- | --- |
| `src/main.cc` | 日志初始化、版本输出、信号处理、构建/启动/停止入口。 |
| `src/sep-face.h`、`src/sep-face.cc` | 配置结构和解析、图片图库、四个工作线程、消息处理、投票、结果发布、冻结状态。 |
| `src/picshare_reader.h`、`.cc` | 从 `/picshare<channel>` 读取 BGR 帧。 |
| `src/face_engine.h`、`.cc` | 选择推理后端、对比特征、计算 Top1/Top2 与阈值判定。 |
| `src/face_backend.h`、`src/face_backend_rknn.cc` | 后端接口；RKNN 模型加载、检测、关键点/对齐、特征提取。 |
| `src/face_backend_mock.cc` | 不运行真实模型的简化后端。 |
| `src/face_db.h`、`.cc` | 二进制人脸库及备份恢复；当前主识别循环不从它取图库。 |
| `src/di_get.h`、`.cc` | nanomsg REQ/REP 查询 DI 电平。 |
| `config.json`、`sep.ini`、`sep-faces.ini` | 运行参数、SEP 启用状态、日志等级。 |
| `models/`、`docs/` | 两个 RKNN 模型；需求、设计与优化说明。设计文档的规划内容不等于当前执行路径。 |

<a id="detail-2"></a>
### 2. 构建、安装与启动条件

- 顶层 `CMakeLists.txt` 进入 `skes-apps/`；后者仅在目标处理器非 `x86/x86_64` 时 `add_subdirectory(sep-faces)`。本目录目标名是 `sep-faces`，使用 C++11，编译列表明确列出 8 个 `.cc` 文件。声明的最低 CMake 版本是 2.8.12，但调用了 `target_link_options`；构建机须使用支持该命令的版本。
- `find_package` 涉及 e2fsprogs、OpenSSL、curl、zlib、cJSON、collections-c、nanomsg、skes-log、skes-linker；链接还包含 `skes-utils`、OpenCV 静态库、`rknnrt`、`rga`、pthread 等。RKNPU2、RGA、OpenCV 路径在本目录 `CMakeLists.txt` 中写死为当前 RK3576 SDK 路径。源码直接使用 RKNN API、OpenCV；未见直接调用 RGA API。
- 安装规则放置程序、`config.json`、`sep.ini`、`sep-faces.ini`、两个 `.rknn` 文件及 cJSON 动态库。它**不安装 `faces/` 照片目录**；源码目录当前也没有 `faces/`。`sep.ini` 当前 `enabled=false`，因此仅安装不代表 SEP 会启用该程序。
- 程序从当前工作目录读取相对路径 `config.json`、`sep-faces.ini`、`faces`、`face_db.bin` 和模型路径。运行目录与安装文件布局必须对应；模型文件存在只说明可尝试 RKNN 初始化，不证明目标设备上的模型推理可用。

<a id="detail-4"></a>
### 4. 配置读取与当前实际值

`parse_config()` 先填源码内置默认值，再用 `config.json` 的 `face` 对象中类型符合预期的字段覆盖。下面列的是**当前仓库 JSON 值**，不是缺字段时的内置默认值。`config.json` 顶层 `version`、`debug`、`log` 在本目录 `parse_config()` 中没有读取。

| 配置组 | 当前值与实际作用 |
| --- | --- |
| 输入与图库 | `picshare_channel=4` → `/picshare4`；`face_images_dir="faces"`；`face_images_max=10`；`db_path="face_db.bin"` 只用于旧式库。照片目录不在本次源码/安装产物中。 |
| 主循环与发布 | `detect_fps=10` → 取不到新帧时，读帧函数每次重试等待 `1000/max(1, detect_fps)` ms；有新帧时不按此值限速；`recognize_vote_frames=7`、`recognize_vote_need=4`；`publish_interval_ms=1000`，解析时下限 50 ms。实际频率受取帧、推理和线程调度影响。 |
| 识别阈值 | `hit_threshold=0.45`，`margin_threshold=0.06`，`min_face_px=40`，`min_blur_var=60`。 |
| 检测/特征模型 | `backend_type="rknn"`；`face_detect_model_path="models/RetinaFace.rknn"`；`face_embed_model_path="models/mobilefacenet_arcface.rknn"`；`detect_input_width=320`、`detect_input_height=320`、`embed_input_width=112`、`embed_input_height=112`、`embed_output_dim=512`。 |
| 检测后处理 | `detect_input_color="bgr"`；`detect_conf_threshold=0.5`；`detect_nms_threshold=0.45`（NMS IoU 阈值）；`detect_max_boxes=64`；`detect_dump_input_path=""` 关闭检测和特征输入图像 dump，`detect_dump_interval_ms=0`。 |
| 方向与安全带 | `camera_rotation=270`（顺时针 270°）；`freeze_on_match=1`、`freeze_on_seatbelt_fasten=1`；`seatbelt_fasten_di_level=0`。解析器只接受 0/90/180/270 的归一化旋转角。 |
| 失配图片 | `mismatch_images_dir=""`，当前不落盘；`mismatch_images_max=100`，仅在启用目录后用于清理。 |
| 遗留一次性操作 | `recognize_fps=2`、`boost_fps=15`、`boost_duration_ms=1500`、`enroll_sample_count=7`、`enroll_max_frames=12`、`recognize_max_frames=12`；当前主循环不调用旧一次性操作链。其中 `recognize_fps`、`boost_duration_ms` 在本目录除解析/日志外没有实际控制调用。 |
| 遗留应答 | `enable_filter_reply=1`、`response_timeout_ms=5000`；配置开启不代表接收路径已调用应答函数。 |

`src/main.cc` 从 `sep-faces.ini` 读取 `[log] level`，设置相对路径 `sep-faces.log`、每文件 5 MiB、最多 4 个文件，并尝试把等级写回 INI；这与 `config.json` 顶层 `log.file`/`log.size`/`log.count` 是两套数据。SIGUSR1 只改变当前进程日志等级，未见在该信号分支写回 INI。

<a id="source-tree"></a>
## 二、源码目录总览

以下是 `skes-apps/sep-faces/` 的主源码与部署文件；`faces/` 不在当前源码目录中，安装规则也没有安装它。

```text
sep-faces/
├── CMakeLists.txt、sep.ini          构建、安装与 SEP 启用状态
├── config.json、sep-faces.ini      识别参数与日志等级
├── models/                         RetinaFace、MobileFaceNet RKNN 模型
├── docs/                           需求、设计与优化记录
└── src/
    ├── main.cc                     日志、信号与进程入口
    ├── sep-face.cc/.h              配置、图库、四线程和状态流转
    ├── picshare_reader.cc/.h       POSIX 共享内存图像读取
    ├── face_engine.cc/.h           后端选择与相似度匹配
    ├── face_backend_rknn.cc        RKNN 检测、对齐和特征提取
    ├── face_backend_mock.cc        简化调试后端
    ├── face_db.cc/.h               旧式二进制库及备份
    └── di_get.cc/.h                本机 DI 服务查询
```

文件职责逐项见原第 1 节；`docs/` 中的方案不自动等于当前入口已实现的能力。

<a id="architecture"></a>
## 三、核心架构设计

```text
上游 BGR 共享内存 /picshare4 ──> 读帧 ──> 旋转/检测/对齐/特征
                                                  └──> 图片内存图库匹配
                                                        └──> 7 帧/4 票判定
                                                              └──> /skes/filter
本地照片 faces/ ── 启动时装载 ────────────────────────> 图片内存图库
DI 服务 127.0.0.1:38000 ──> 安全带状态/冻结解除 ────────> 发布状态
SKES 心跳与旧控制消息 ──> 消息线程；旧在线录入命令当前只解析并忽略
```

`msg_thread`、`loop_thread`、`pub_thread`、`di_thread` 并行执行。识别发布按周期读上次状态，无新帧不保证状态自动清零；只有两个冻结开关同时开启时才会在匹配后冻结，而置冻结前不检查当前安全带值。模型初始化失败可能回退 mock，mock 输出不能用来验收真实人脸识别。启动、投票、发布和冻结的原始条件见原第 3、5、6、7 节。

<a id="detail-3"></a>
### 3. 启动、并行线程和退出流程

#### 3.1 普通 Markdown 可见的流程图

```text
main()
  |
  +--> init_log(): sep-faces.ini 的 [log] level；sep-faces.log
  +--> 注册 SIGINT/SIGTERM/SIGUSR1；输出版本
  +--> sep_face_new()
  |      |
  |      +--> 读取 config.json，设置 FaceConfig
  |      +--> 选择 RKNN 或 mock；初始化 FaceEngine
  |      +--> 从 face_images_dir 建立 image_db
  |      +--> 读取/恢复旧式 face_db.bin（当前主识别不使用）
  |      +--> 连接 SKES 发布端 19225 / 订阅端 19226
  |
  +--> sep_face_run(): running = true
  |      +--> msg_thread  : 接收心跳、filter 输入
  |      +--> loop_thread : 取图、检测、匹配、多帧投票
  |      +--> pub_thread  : 周期发布识别和安全带状态
  |      +--> di_thread   : 查询 DI 通道 0，更新安全带/冻结状态
  |
  +--> 主线程每 200 ms 检查退出标志
         SIGUSR1 -> 循环调整日志等级
         SIGINT/SIGTERM -> sep_face_stop() -> join 四线程
                         -> sep_face_release() -> 释放 linker/对象
```

文字版：`main()` 先初始化日志和信号，再调用 `sep_face_new()` 读取配置、准备模型与图库、连接消息端点。`sep_face_run()` 同时启动消息、识别、发布、DI 四个线程。主线程只处理信号与退出；收到 SIGUSR1 改变内存中的日志等级，收到 SIGINT/SIGTERM 后停止线程并释放资源。`src/main.cc` 未使用命令行参数。停止时会等待四个线程退出；DI 线程可能正等待最长 5 秒的接收超时，随后还有每轮 1 秒休眠，因此关闭不一定即时完成。

#### 3.2 初始化顺序与失败分支

1. `parse_config()` 要求工作目录下 `config.json` 可读取且 JSON 有效，否则 `sep_face_new()` 返回失败。
2. `backend_type` 精确等于 `rknn` 时选择 RKNN，其他字符串归入 mock。若任一模型文件不存在，先改选 mock；若 RKNN 初始化失败，`FaceEngine` 也尝试回退 mock。调用者没有检查 `engine.init()` 的返回值，实际后端应结合启动日志检查。
3. `load_image_db()` 读取照片、检测并提取特征，建立只在内存中的 `image_db`；即使目录不存在或图库为空，函数仍把 `image_db_ready` 置为真并返回。
4. 之后加载旧式 `face_db.bin`。主文件不存在时从空库开始（此分支不会尝试 `.bak`）；主文件存在但无效时才尝试 `.bak`；加载失败仅记日志并设置 `db_ready=false`，不直接阻止当前图片图库识别。
5. `skes_linker_init()` 接收本机发布端 `tcp://127.0.0.1:19225` 和订阅端 `tcp://127.0.0.1:19226` 地址并初始化句柄。若该调用失败，初始化失败；本目录源码不定义端口服务端的具体实现，也不能仅凭地址参数确定外部服务的连接状态。

<a id="modules"></a>
## 四、核心模块深度分析

| 模块 | 入口及主要数据 | 当前机制与边界 | 详解 |
| --- | --- | --- | --- |
| 配置与生命周期 | `main.cc`、`sep-face.cc` 读取工作目录的 JSON/INI 并创建四线程。 | JSON 的 `face` 字段覆盖源码默认值；模型、图库和日志文件使用相对路径。 | 原第 2～4 节 |
| 图像输入 | `picshare_reader.cc` 从 POSIX 共享内存读取 BGR 帧。 | 通过帧序号避开重复帧；不会自行生成画面，也不能仅凭序号检查保证任何上游时序都无撕裂帧。 | 原第 5.1 节 |
| 检测与特征 | `face_backend_rknn.cc` 使用检测模型、关键点对齐和特征模型。 | 每帧最终只处理最高分框；模型预处理与真实模型的匹配仍需目标板核验。mock 仅供链路调试。 | 原第 5.3 节 |
| 图片图库与匹配 | `load_image_db()` 从前若干候选照片建内存库，`FaceEngine` 比较 Top1/Top2。 | 图库仅在启动时装载；旧 `face_db.bin` 不向当前主识别循环提供身份。 | 原第 5.2、6.1、7 节 |
| 投票与发布 | 识别线程维护滑动窗口，发布线程组成 `FaceRecognition` 属性。 | 当前配置为 7 帧需 4 票；错误码 237 被正数判定误纳入候选，码与名字也非同一锁内快照。 | 原第 6.1、6.2 节 |
| DI 与消息 | `di_get.cc` 查询通道 0；消息线程回应心跳并解析旧控制指令。 | DI 失败保留旧状态，冻结没有自动超时；旧录入/删除指令当前不执行。 | 原第 6.3、7 节 |

<a id="detail-5"></a>
### 5. 图像输入、图库和推理

#### 5.1 `/picshare` 读取

`fetch_bgr_from_picshare_face()` 以 `shm_open(O_RDONLY)`、`fstat`、`mmap(PROT_READ)` 打开 `/picshare<channel>`。共享内存尾部的 `PicMetaV2` 包含 `frame_seq`、行列数、OpenCV 类型和 `data_bytes`；代码检查容量关系及 `type==CV_8UC3`，跳过重复序号，复制图像后再次比较序号，成功时返回独立的 `cv::Mat`。当前主循环传入 `nth=1`、`retries=2`、失败分支等待参数 `1000/detect_fps` ms；读到新帧时立即返回，失败时主循环另等 10 ms 后重试。读端不会自己生成图像。

读端仅在首次映射或映射指针为空时重新打开共享内存；元数据异常或序号长期不变时只在现有映射上重试。它检查前后帧序号，但没有额外跨进程锁或完整的双缓冲协议验证；不能仅凭此代码证明所有写入时序下都不会出现撕裂帧。源代码未在本函数中显式校验 `rows*cols*3 <= data_bytes`，该边界依赖上游元数据可信。

#### 5.2 启动图片图库

`list_image_files()` 只筛选 JPG/JPEG/PNG 文件名并按路径排序；`load_image_db()` 至多查看前 `face_images_max` 个候选文件，逐一 `imread`、检测、提取特征。成功条目顺序分配整数 ID `1..N`，`face_id` 字符串取文件名去扩展名；失败文件跳过。**限制数作用于候选文件**，前 10 张若有失败，代码不会再读取第 11 张补足。图库只在启动时装载，运行中增删照片不会自动更新。

加载后代码打印每个向量的范数、自相似度和图库两两余弦分数，至少两张照片时记录 `face_db_selfcheck summary`。`weak`/`COLLAPSED` 是源码内诊断阈值：任一图库身份对的余弦分数最高值 `cross_max >= 0.55` 为 `weak`；`cross_max >= 0.7` 或所有身份对均值 `cross_mean >= 0.5` 为 `COLLAPSED`（优先于 `weak`）。不足两张照片时不产生两两比较的 summary；这些判定不是模型精度测试结论。

#### 5.3 RKNN 与 mock 后端

RKNN 后端分别加载检测和特征模型，查询模型张量属性。检测输入先保持宽高比缩放并填充至配置尺寸；根据检测模型输入颜色设置决定是否 BGR→RGB。代码接受单个 `[N,5]` 式检测输出，或 boxes/scores/可选 landmarks 的 RetinaFace 输出；后者需与生成的 prior 数量一致。检测结果经置信度筛选、NMS 和数量限制后，**只把最高分框**映射回原图；即使保留多个框，当前接口每帧仍只处理一个人脸。

特征提取要求框宽、高至少为 `min_face_px`。有匹配的 5 点关键点时做仿射对齐，失败则扩大人脸框后裁剪缩放；计算灰度拉普拉斯方差，低于 `min_blur_var` 时跳过。RGB 三通道特征模型路径强制 BGR→RGB；量化输入传 `uint8`，非量化输入按 `(像素/127.5)-1` 生成 float，必要时重排 HWC/NCHW。输出长度须等于 `embed_output_dim`，随后做 L2 归一化。真实模型是否与这些预处理完全匹配，需要模型转换设置和设备实测确认。

mock 后端把画面中心的正方形当作人脸，使用灰度直方图式特征并归一化。它可验证调用链，但没有真实检测与身份识别语义。启动日志中的 `backend=` 才能区分实际选中的后端；存在 `.rknn` 文件并不能排除初始化失败后的 mock 回退。

<a id="detail-6"></a>
### 6. 持续识别、投票与状态流转

#### 6.1 普通 Markdown 可见的主循环流程图

```text
loop_thread / face_loop()
  |
  +-- 若 frozen 且两个 freeze 开关都为真 --> 等 50 ms --> 重试
  |
  +-- 从 /picshare4 取新帧？ --否--> 等 10 ms --> 重试（发布状态不变）
  |                         是
  +-- 旋转画面；检测最佳人脸框？ --否--> code=0，清 face_id/票窗 --> 重试
  |                             是
  +-- 图片图库为空或未就绪？ --是--> code=-2，清 face_id/票窗 --> 重试
  |                            否
  +-- 特征提取成功？ --否--> code=0，清 face_id/票窗 --> 重试
  |                     是
  +-- FaceEngine::match：Top1 分数和 Top1-Top2 差距均过阈值？
  |       是 -> 当前帧为该 ID 投 1 票；否 -> 当前帧记 0 票
  |       注：维数错误码 237 也满足 code>0，会误入计票分支
  +-- 更新至多 7 帧滑动窗口；同一 ID 达到 4 票？
          |-- 窗口未满 --------> 保持上一发布状态
          |-- 达标 -----------> code=1、face_id=照片名；清票窗；可能冻结
          +-- 窗口满但未达标 -> code=-1、清 face_id；可选保存失配图
```

文字版：识别线程先检查冻结，再取一张新帧并按配置旋转。检测失败发布 0；检测到人脸而图片图库为空发布 -2；提特征失败发布 0。匹配阶段先按图库全部身份计算余弦分数，`top1 >= hit_threshold` 且 `top1-top2 >= margin_threshold` 才给该身份计票。最近 7 帧窗口中的同一身份达到 4 票时发布 1 和照片名；窗口满但无人达标时发布 -1；窗口尚未满时沿用上一发布状态。无新帧时也沿用上一状态。

`FaceEngine::match()` 对图库每个身份的多个向量取该身份最高分，再求总体 Top1/Top2。图片图库当前每个成功照片只有一个向量。若图库只有一个身份，`top2_score` 保持 `-1`，分差按 `top1 - (-1)` 计算；在当前 `hit_threshold=0.45`、`margin_threshold=0.06` 下，只要 Top1 达到命中阈值，分差条件也会满足。维数不匹配会返回内部码 237；主循环仅用 `code>0` 判断是否给某个 ID 计票，因此 **237 也会被当作候选 ID 计票**。若该错误持续达到投票门槛，代码可能发布 `FaceRecognitionResult=1`，而 `image_names[236]` 通常不存在，导致 `face_id` 为空。这是调用链中的实际边界问题，不应把 237 理解为主循环已正确处理的异常。计票优先比较票数，相同票数按平均分选胜者。命中后清空票窗；未命中且窗口已满时继续滑动，并在配置了目录时保存窗口图像。

#### 6.2 周期发布值与 JSON 结构

| `FaceRecognitionResult` | 当前主路径赋值位置和含义 |
| --- | --- |
| `0` | 初始状态；没有检测到脸、提特征失败，或 DI 解冻时重置。不能简单理解为“画面中肯定没人”。 |
| `1` | 某个正数匹配码的多帧票数达到要求；通常对应图片身份。由于错误码 237 也可能被计票，不能只凭 `1` 就断言身份有效；非空 `face_id` 才会加入该属性。 |
| `-1` | 检测与特征阶段通过，但投票窗口已满且无身份达标。 |
| `-2` | 检测到人脸，但 `image_db` 未就绪或为空。 |

`publish_loop()` 按 `publish_interval_ms` 读取共享状态，调用 `build_face_result_payload()` 发送 `skes_topic_type_filter`。外层 JSON 有 `version=""`、`id=""`、由 `skes_get_clock_time()` 取得的系统实时时钟毫秒时间戳 `ts`、`action="post"`、`type="properties"`；`data.type="FaceRecognition"`，属性数组含 `FaceRecognitionResult`（int32）和 `seatbelt_fasten`（boolean），`code=1` 且照片名非空时加入 `face_id`（string）。周期发布和图像分析是不同线程，因此一个周期消息可能重复上次结果。

`published_code` 是原子变量，`published_face_id` 由互斥锁保护；发布线程分别读取二者，并非一次锁内读取的组合快照。源码因此没有保证每条并发消息的码与名字必定来自同一识别帧。`seatbelt_fasten` 在 `sep_face_new()` 中没有显式初始化，发布线程可能先于 DI 首次成功查询执行；首次 DI 读数前不宜把该值当成已确认状态。

#### 6.3 DI、安全带与冻结

`di_thread` 每轮通过 nanomsg `NN_REQ` 向 `tcp://127.0.0.1:38000` 请求状态，解析 `status.io[]` 中 `dev="di"`、`chn=0` 的 `params.level`。当前配置 `seatbelt_fasten_di_level=0`，因此程序把 level 等于 0 解释为“已系”；非 0 解释为“未系”，若此前处于冻结态则将识别码重置为 0、清除冻结。每轮末尾睡 1 秒；接收超时配置为 5 秒。DI 获取失败时只记错误，不更新已有状态。

识别命中时，只要 `freeze_on_match` 和 `freeze_on_seatbelt_fasten` **同时为真**，代码就置 `frozen=true`，并没有在置位前检查当前 `seatbelt_fasten` 值。随后识别线程在冻结态不取帧；直到 DI 读到非有效电平才解冻。当前配置两个开关都为 1。安全带服务不可用时，源码没有额外的冻结超时释放路径。`di_init()` 在连接或设置超时失败后会 `nn_close(di_nn_fd)`，但没有把 `di_nn_fd` 重置为 `-1`；后续调用会因非负 fd 而跳过重新建连。这是静态可见的重连缺口。`di_get()` 中的 `printf("di_level = %d", di_level)` 把指针按整数打印，不能把这行输出当作实际电平；应以 `di_loop()` 在成功查询后记录的 `get di level ...` 日志或 DI 服务端数据为准。

<a id="detail-7"></a>
### 7. 消息输入、遗留代码与持久化边界

`msg_thread` 通过 `skes_run()` 接收消息，设置运行超时 200 ms。心跳 topic 解析后把 action 改为 response，设置固定 UUID 和名称 `sep-face`，计数器加一并回发。filter topic 只接受外层 `type="properties"` 且 `data.type="CentralControlScreen"` 的消息；它解析 `Start_Face_Recognition`、`Record_Face`、`Remove_Face`、`Update_Face`、`Check_Face`、`Face_ID`，记录日志后明确忽略，不修改当前识别状态。

源码仍有 `set_pending_request()`、`do_one_shot()`、`collect_embeddings()`、`recognize_with_vote_streaming()`、`maybe_publish_filter_result()` 和 `FaceDb` 增删改接口，但当前接收回调没有调用 `set_pending_request()` 或 `do_one_shot()`。所以 `enable_filter_reply=1` 不能证明旧操作或带原请求 ID 的识别应答已生效。`face_db.bin` 使用 `FDB1` 标识，读取版本 1/2，写入版本 2，身份范围 1..230；写文件先落 `.tmp`、尝试备份原文件到 `.bak`、再 `rename`；备份复制的返回值被忽略，不能据此保证每次保存都有有效 `.bak`。当前活跃识别仍只比对启动图片图库，二进制库是否加载成功不改变该事实。

当失配图片目录非空且投票窗口满而未匹配时，`save_mismatch_images()` 将窗口图像逐张写成 JPEG，再按路径排序删除超出 `mismatch_images_max` 的图片。清理复用 `list_image_files()`，因此会扫描该目录中所有 JPG/JPEG/PNG 文件，**不限于本程序生成的 `mismatch_*.jpg`**；目录中不宜混放其他图片。当前 `mismatch_images_dir=""`，所以不会执行写盘。该保存调用位于持有 `published_mu` 的作用域中，启用保存后磁盘操作期间可能延迟发布线程读取 `face_id`；这是源码可见的锁范围，并未测量具体延迟。

<a id="details"></a>
## 五、专题详解与设备排障

本部分集中实现限制、测试边界和现场排障。专题编号沿用原文，文中的“§N”“第 N 节”仍指本文同编号的专题。

<a id="detail-8"></a>
### 8. 可直接核验的限制与排查顺序

1. **启动即失败**：先核对当前工作目录的 `config.json` 是否存在且 JSON 有效，再看模型/后端和 `skes_linker_init` 日志。`sep.ini` 当前禁用也会影响由 SEP 启动的部署方式。
2. **总是 `-2`**：先确认 `faces/` 是否实际部署、文件扩展名是否为 JPG/JPEG/PNG、前 10 个候选照片是否成功读图/检测/提特征；看 `face_image_db add` 与 `loaded entries` 日志。模型降级为 mock 时也要核对图库和识别可信度。
3. **总是 `0`**：看 `picshare shm_open_failed`/`no_new_frame`、检测日志、最小脸尺寸和模糊度门槛；无新帧时状态未必自动变 0，应结合图像序号日志判断。
4. **有脸但常为 `-1`**：核对实时画面方向、图库照片朝向、BGR/RGB 与模型预处理、`top1`/`top2` 分数及 7 帧/4 票日志。`face_db_selfcheck` 只能辅助定位特征异常，不能单独证明识别效果。
5. **命中后不再识别**：检查 `frozen` 和 DI 通道 0 的电平/服务连通性。当前实现要求 DI 返回非有效电平才解除冻结；没有自动超时。
6. **旧控制命令无响应**：按第 7 节确认当前回调只解析并忽略命令，旧录入/删除/更新代码没有调用入口。不能通过修改 `enable_filter_reply` 让它自动接通。

核验定位：入口与日志见 `src/main.cc:52`；配置解析见 `src/sep-face.cc:225`；图库加载见 `src/sep-face.cc:570`；消息回调见 `src/sep-face.cc:782`；主循环见 `src/sep-face.cc:1299`；发布与 DI 线程见 `src/sep-face.cc:1247`；共享内存读取见 `src/picshare_reader.cc:27`；RKNN 后端见 `src/face_backend_rknn.cc:25`；匹配判定见 `src/face_engine.cc:85`；二进制库见 `src/face_db.cc:129`。行号以本次核对的工作树为准。

<a id="detail-9"></a>
### 9. 设备现场排查手册

本节假定设备上运行的是本目录对应版本的 `sep-faces`。先确认设备实际二进制、配置和模型版本；若固件与本文核对的工作树不同，应以设备版本重新对照源码。下面命令用于 Linux 设备，示例中的进程号和应用目录变量需替换为现场值；BusyBox/精简系统没有的命令可用等价工具；`/dev/shm/picshare4` 只适用于相应共享内存挂载/命名空间，`ss` 的连接状态也只是本机一层证据。先收集现状，再决定是否修改配置或重启，以免丢失故障发生时的日志、进程状态和共享内存信息。

#### 9.1 先定位进程、工作目录和文件

```sh
ps -ef | grep '[s]ep-faces'
SEP_FACES_PID=1234                  # 改为上一行查到的 PID
SEP_FACES_DIR=/path/to/apps/sep-faces # 改为设备实际应用目录
readlink -f "/proc/$SEP_FACES_PID/exe"
readlink -f "/proc/$SEP_FACES_PID/cwd"
tr '\000' ' ' < "/proc/$SEP_FACES_PID/cmdline"
ls -l "$SEP_FACES_DIR/sep-faces" "$SEP_FACES_DIR/config.json" "$SEP_FACES_DIR/sep.ini" "$SEP_FACES_DIR/sep-faces.ini"
ls -l "$SEP_FACES_DIR/models/RetinaFace.rknn" "$SEP_FACES_DIR/models/mobilefacenet_arcface.rknn"
ls -ld "$SEP_FACES_DIR/faces"
cat "$SEP_FACES_DIR/config.json" "$SEP_FACES_DIR/sep-faces.ini"
ls -l /dev/shm/picshare4
ss -tn
cat "/proc/$SEP_FACES_PID/status"
df -h "$SEP_FACES_DIR"
```

- **进程不存在**：先确认由谁启动、是否已启用。本仓库 `sep.ini` 为 `enabled=false`；设备上的实际配置可能不同。检查启动器/SEP 的日志及可执行文件是否存在。若日志有 `sep-face init failed`，按第 3.2 节顺序排查配置解析和 linker 初始化。若异常退出，再查看设备系统日志、退出码或 core dump；本项目源码不能替代外部启动器日志。
- **进程存在但工作目录不对**：程序读取 `config.json`、INI、模型、图库和日志使用相对路径；应以 `/proc/<PID>/cwd` 为准。文件“在可执行文件旁边”并不足以证明程序会读取到它。`config.json` 不可读或无效会直接使初始化失败；模型路径找不到则可能回退 mock；`faces` 找不到会造成图库为空。
- **文件齐但行为仍异常**：记录实际配置内容、文件大小/时间戳和启动日志中的 `backend=`、`channel=`、`detect_model=`、`embed_model=`，确认运行中的不是另一套部署目录或旧进程。不要只看仓库中的默认 `config.json`。

#### 9.2 日志等级与关键证据

当前仓库 `sep-faces.ini` 的 `[log] level=4`。在本项目链接的 `skes-log` 中，INFO=1、WARNING=3、ERROR=4，日志宏按“当前等级数值 <= 消息等级数值”过滤；因此等级 4 下看不到本文提到的 `face_publish`、`face_live_vote`、`face_image_db loaded` 等 INFO 日志，也看不到 WARNING。先查看设备当前 INI 和真实日志等级，**缺少 INFO 不能直接判定线程没有运行**。

`src/main.cc` 设置日志文件名为当前工作目录下的 `sep-faces.log`、每文件最大 5 MiB、最多 4 个文件。设备上的日志还可能被系统服务接管，须结合启动方式核对。需要详细定位时，可在保留现场信息后把设备 INI 的等级改为 `1` 并按设备运维流程重启，以采集 INFO 及更严重级别日志；恢复原等级时也需按同样流程。SIGUSR1 的实现是将等级加一并在超过最大值时回绕，不是“一次切到详细日志”的快捷键，且不会在该信号分支写回 INI。详细日志可能较多，抓到证据后再恢复原设置。

可优先在日志中按下表的**实际源码字符串**定位；无相应日志时先核对等级、文件路径和进程是否运行到该阶段。

| 日志关键词 | 对应代码位置 | 排查含义 |
| --- | --- | --- |
| `sep-face init failed`、`face_config_loaded`、`sep-face config backend=` | `main.cc`、`sep-face.cc` | 初始化失败、配置解析值和最终后端。`face_config_loaded` 为 INFO；若配置读取失败不会打印它。 |
| `rknn backend requested but model missing`、`rknn ... init failed`、`rknn backend ready` | `sep-face.cc`、`face_backend_rknn.cc` | 模型缺失/初始化失败/成功；先看是否已回退 mock。 |
| `face_image_db empty`、`face_image_db skip ...`、`face_image_db loaded entries=` | `load_image_db()` | 照片目录为空、某张照片失败、最终图库数量。目录为空或不存在时会提前返回，可能没有 `loaded entries=`。 |
| `picshare shm_open_failed`、`picshare no_new_frame`、`picshare new_frame after gap` | `picshare_reader.cc` | 打不开共享内存、帧序号未推进、停帧后恢复。相关警告有限流，约每 2 秒最多记录一次。 |
| `rknn_detect_pipeline`、`detect_postprocess`、`embed_preprocess skip` | RKNN 后端 | 检测置信度/NMS、映射框、最小脸/模糊度/对齐等失败原因。 |
| `face_match decision=`、`face_live_vote` | `face_engine.cc`、`face_loop()` | 单帧相似度判定与多帧票数。先区分“每帧不通过阈值”和“单帧通过但票数不够”。 |
| `face_publish code=`、`failed to build face publish payload` | `publish_loop()` | 本地发布路径所读到的状态或构造消息失败；**不能单凭该日志证明下游收到**。 |
| `failed to get di level`、`seatbelt unfasten!` | `di_loop()` | DI 查询失败或读到非有效电平后解除冻结。 |
| `face_cmd ignored due to new startup-recognize workflow` | 消息回调 | 旧控制指令被收到并解析，但当前实现明确忽略。 |

#### 9.3 按现象逐层定位

##### A. 进程未启动、启动即退或模型变成 mock

1. 记录 `/proc/<PID>/cwd`（若进程仍在）、启动方式、设备上的 `sep.ini` 和启动日志。区分“SEP 未启用”与“进程启动后失败”。
2. 若 `sep-face init failed`：核对工作目录的 `config.json` 是否存在、可读且是合法 JSON；`parse_config()` 出错会直接失败。再看 `skes_linker_init` 调用是否返回错误；它接收发布地址 `127.0.0.1:19225`、订阅地址 `127.0.0.1:19226`。这两个地址涉及外部消息服务，需结合对应服务进程和端口状态排查。
3. 若日志显示 `backend=mock`：先核对配置中 `backend_type` 是否精确为 `rknn`，再核对两份模型文件及实际工作目录；文件存在时继续查看 RKNN 初始化/张量查询错误及设备 NPU 运行时。mock 后端只是简化流程，不能用其结果验收真实人脸识别。
4. 若进程崩溃或被杀：记录系统日志中对应时间的信号、OOM/内存信息、core dump 及二进制版本。源码没有独立的崩溃恢复机制；进程是否重启由外部管理器决定。

##### B. 程序运行，但无有效画面或一直输出 `0`

1. 确认设备实际 `picshare_channel`，当前仓库配置为 4；Linux 常见配置下可在 `/dev/shm/picshare4` 查看共享内存对象，但以设备挂载命名空间和上游实现为准。`picshare shm_open_failed` 指向对象不存在/不可访问；`no_new_frame` 指向当前映射上的序号没有新进展，不能仅凭此认定摄像头硬件坏了。
2. 同时检查上游写帧进程与共享内存对象的存在、大小、权限、帧序号增长。该读端只检查尾部元数据容量关系和 `CV_8UC3` 类型，不能把任何共享内存文件都当作可用画面。上游重新创建共享内存而本进程持有旧映射时，也可能持续看不到新帧；先保留日志和对象信息，再按设备流程评估重启读端是否能恢复映射。
3. 有新帧后看 `rknn_detect_pipeline raw/conf_kept/nms_kept`、`detect_postprocess`。无框可能是模型/输入颜色/画面朝向/阈值问题；本程序只取最高分框。若已经检测到框却仍为 `0`，看 `extract_embedding skip reason=too_small`、`embed_preprocess skip reason=too_blurry`、`align_failed`、维数错误和推理错误。
4. `0` 也可能是刚启动或安全带状态导致的重置；无新帧时旧发布状态不会自动清零。结合时间戳、读帧日志和 DI 日志判断，不能只凭一个结果码下结论。

##### C. 一直输出 `-2`、图库为空或 `face_id` 不出现

1. `-2` 的前提是当前后端报告检测到人脸，而图片图库为空/未就绪。若已回退 mock，mock 对非空画面使用中心框，`-2` 本身不能证明真实画面有人脸。检查实际工作目录的 `faces`，以及设备 `config.json` 中的 `face_images_dir` 和 `face_images_max`。本仓库不安装照片，必须由部署过程提供。只放 `face_db.bin` 不会填充当前识别使用的 `image_db`。
2. 核对前 `face_images_max` 个按文件名排序的 JPG/JPEG/PNG 候选照片。`skip read_failed` 是读图失败，`skip no_face` 是检测失败，`skip extract_failed` 是特征提取失败。程序不会为了补足成功数而继续查看限制之后的照片；增加、替换照片后需重启才能重建图库。
3. 若 `loaded entries>0` 却没有 `face_id`：先检查是否真的出现有效 `code=1`，再检查照片文件名去扩展名是否为空，以及发布码/名字不同步窗口和第 6 节的错误码 237 计票边界。正常 `-1`/`0`/`-2` 消息不会附带 `face_id`。

##### D. 有人脸却常输出 `-1`、误匹配或偶发不稳定

1. 分开看三关：检测框是否稳定、`embed_preprocess` 是否成功、`face_match decision=` 的 `top1/top2/margin` 是否达当前阈值 0.45/0.06。阈值未达标是单帧不投身份票；单帧达标但 `face_live_vote` 中赢家票数不足 4/7，则是多帧稳定性问题。
2. 对照当前 `camera_rotation=270` 的实时画面方向和图库照片方向。实时帧在检测前旋转，启动加载的照片未经过同一旋转；还需核对检测 `detect_input_color=bgr`、特征输入 BGR→RGB、模型转换时的均值/标准差与量化设置。这里是排查点，不能仅靠源码证明模型参数正确。
3. 看 `face_db_selfcheck` 的范数与两两余弦；`weak`/`COLLAPSED` 提示图库特征区分度异常，但阈值是本程序的启发式告警，并不是准确率结论。若准备调整阈值、照片或模型，记录调整前后的相同样本、分数和结果，避免只用一次识别成败判断改善。
4. 当前每帧只处理最佳检测框，多人同框时不能假定每个人都被识别；对现场现象先核对日志中的映射框和画面位置。

##### E. 识别成功后卡住、`seatbelt_fasten` 不对或解冻慢

1. 核对 `face_publish ... frozen=`、当前两个冻结开关以及 `seatbelt_fasten_di_level`。当前配置是 DI 通道 0 的电平 0 视为已系；两个冻结开关都为 1 时，识别命中即置冻结，**并不先检查安全带是否已系**。
2. 查看 DI 服务 `127.0.0.1:38000`、`failed to get di level`、成功查询后的 `get di level ...` 和服务端返回的 `status.io[]`。`di_get.cc` 自身打印的 `di_level = ...` 把指针误作数值，不可用于判断真实电平。只有成功解析到通道 0 且电平不等于当前有效电平，程序才解除冻结并把识别码置 0。DI 失败时维持旧状态；这里没有自动超时解冻。
3. 若 DI 初始连接或设置超时失败，`di_get.cc` 关闭 socket 后没有重置静态 fd，后续循环可能跳过重新连接。先保存故障日志与 DI 服务状态，再按运维流程评估恢复动作；不要把单次端口检查正常等同于程序当前 DI 连接正常。
4. 周期发布线程与 DI 线程并行；首次成功 DI 读数前，源码未显式初始化 `seatbelt_fasten`。排查启动初期的单条安全带状态时，应等待并确认 DI 实际读数。

##### F. 本地有 `face_publish`，平台却收不到；旧控制指令无结果

1. `face_publish` 表示本地调用了 `skes_msg_reply()`，其返回值在 `publish_loop()` 中未检查。继续核对 19225/19226 对应的消息服务、订阅者和外层 JSON `data.type="FaceRecognition"`/属性名；需要下游日志或抓包等独立证据证明消息真正到达。
2. 确认进程实际连接的是代码中的环回地址，并检查外部消息服务是否在同一网络命名空间。消息传输是异步的；初始化调用成功、端口可见或本地发布日志，均不足以单独证明端到端成功。
3. 旧 `CentralControlScreen` 的录入、删除、更新、检查指令被解析后只记日志并忽略；`enable_filter_reply=1` 不会让旧 `do_one_shot()` 自动执行。若需求是在线录入，应按接口需求补全调用链，而不是反复调整图库或超时值。

##### G. 运行慢、日志少、磁盘或内存异常

1. 同步记录进程 CPU/内存、系统剩余内存、磁盘空间、日志增长量及 `rknn_detect_pipeline ... infer_ms/post_ms`。`detect_fps=10` 只影响无新帧时的重试等待，有新帧时没有该参数施加的 10 FPS 上限；`publish_interval_ms=1000` 是发布线程睡眠间隔，不是识别耗时上限。
2. 若启用了 `mismatch_images_dir`，每次投票窗口满而未匹配都可能保存窗口里的多张图；目录清理会扫描所有 JPG/JPEG/PNG，不只清理本程序生成的图片。磁盘慢或满时，`save_mismatch_images()` 又处于 `published_mu` 锁内，可能拖慢 `face_id` 读取与发布。当前仓库配置该目录为空，因此此写盘路径默认关闭。
3. 大量 INFO 日志也可能影响观察和存储；先确认设备实际日志级别。`config.json` 顶层 `log` 字段并不控制本程序日志，真正读取的是 `sep-faces.ini`。

#### 9.4 交接时最少保留哪些证据

- 故障开始/结束时间及设备时间是否可信；固件或安装包版本、`sep-faces` 二进制路径、实际工作目录、实际 `config.json`、INI 及模型文件大小/时间戳。
- 故障前后日志片段：启动后端、图库加载、共享内存状态、推理/阈值/投票、周期发布、DI 查询；同时记录设备上的实际日志等级。
- 一条真实的上游帧序号变化证据、一条本程序发布证据、必要时一条下游接收证据。只有某一层日志时，应把结论限定在该层。
- 复现条件：单人/多人、照片图库数量、摄像头方向、DI 电平、是否冻结、网络服务是否在同一命名空间；记录每次配置改动与重启时间。

以上排查步骤依据静态源码与仓库内日志定义整理，未在目标板运行模型、共享内存写端或外部服务。涉及外部协议兼容、识别精度、实时性和硬件电平含义的结论，应以对应服务代码、模型转换参数和目标板实测为准。
