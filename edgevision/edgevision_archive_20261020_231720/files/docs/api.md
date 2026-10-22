# EdgeVision API

给宿主程序的开发者。接入步骤、实例怎么建、工况谁来填，见 [host_api.md](host_api.md)。本文描述当前源码里的公开接口：`include/evs/` 下的头文件，以及 `VisionTask` 实际会执行的行为。

现场更换摄像头、标定、距离算法或模型时，直接对照
[现场修改对照说明](site_customization.md)。

库使用C++17，提供公共核心和独立业务插件的静态/动态构建。libevs.a仅为公共静态核心，所用功能还需对应插件；当前动态ABI为3。通用VisionTask接收图像和工况，返回检测框和告警；卷扬冰雪的客户Detector接口只接收图像，由宿主决定是否调用。它不打开相机，不链接GStreamer、Qt、RGA；冰雪面积算法依赖OpenCV core/imgproc。

---

## 1. 先选一条入口

| 入口 | 头文件 | 适合 |
|---|---|---|
| `VisionTask` | `evs/vision_task.h` | 产品功能。读 YAML，跑预处理、推理、解码，再交给功能插件 |
| `IFunctionPlugin` | `evs/function_plugin.h` | 人员 / 高度限位 / 卷扬冰雪。`VisionTask` 调 `plugin->decide`；冰雪在模型定位后使用原图计数 |
| `IGroundMapper` | `evs/ground_mapper.h` | 单路摄像头像素脚点到统一地面 X/Y 的可替换算法 |
| 三件套 | `preprocess.h`、`backend_factory.h`、`detection_decoder.h` | 现场看框、离线评测、自研后处理。不跑 `person_decide` |

`tools/v4l2_detect` 走的是三件套，再把框写到 `/detection_boxes`。它不调用 `VisionTask`，所以界面上没有吊钩圆或大臂扇形告警。

默认推理模式下 `VisionTask::detect` 的顺序：

```
camera_id 过滤
  → Preprocessor（CPU letterbox，NHWC uint8）
  → IInferBackend::infer
  → 后端内部 yolo_decode（YOLOv5 / YOLOv11 / YOLOX 头收成检测行）
  → IDetectionDecoder
  → map_target_to_original
  → IGroundMapper（按 camera_id：像素脚点 → 地面 X/Y）
  → apply_ground_metrics（按 task 参考点：X/Y → distance/angle）
  → plugin->decide（0x0004/5/6 人员，0x0007 高度限位，0x0008 卷扬冰雪）
  → Result
```

---

冰雪始终加载卷扬模型，模型框还原到原图后由 `WinchIcePlugin` 计算各框比例。`inference_enabled:false` 和旧ROI模式已拒绝。详见 [winch_ice.md](winch_ice.md)。

## 2. 最小调用

```cpp
#include "evs/vision_task.h"

evs::VisionTask task;
if (task.initialize("configs/load_near.yaml") != 0u)
    return 1;

evs::MachineContext ctx{};
ctx.context_valid = true;
ctx.hoist_radius = 55.f;

evs::Frame frame{};
frame.camera_id = 0;                 // 必须落在 YAML 的 video_channel 里
frame.width = w;
frame.height = h;
frame.stride = stride;               // 0 表示紧凑排列
frame.format = evs::PixelFormat::NV12;
frame.data = nv12;                   // 宿主持有，detect() 返回前必须有效
frame.timestamp_us = now_us;

evs::Result res{};
if (task.detect(frame, ctx, res) == 0u) {
    for (const evs::Target& t : res.targets) {
        // t.x/t.y 是原图上的中心，t.w/t.h 是宽高，都是 0..1
        int x1 = int((t.x - t.w * 0.5f) * w);
        int y1 = int((t.y - t.h * 0.5f) * h);
    }
    if (res.alarm == evs::AlarmState::Alarm) { /* 声光 / 联锁 */ }
}
task.release();
```

`initialize` 失败会回滚，不会留下半初始化状态。已经成功后再调用 `initialize` 直接返回 0，不会重新加载模型。要换模型，先 `release()` 再对一个新实例 `initialize`。

`detect` 对同一个实例是同步的，不要并发调用。`Frame::data` 和 `dma_fd` 的有效期只到 `detect` 返回。库内预处理读的是 `data`，不用 `dma_fd`。

### 怎么接到现场

一个功能号对应一个 `VisionTask`。`initialize` 读一份 YAML，按其中的 `module_id` 创建一个插件，这个实例之后只做这一项功能。`updateConfig` 不能改 `module_id`。载荷附近、载荷下方、大臂下方、高度限位、卷扬结冰各建各的实例，连续帧计数也互不相通。

| 功能 | `module_id` | 实例 |
|---|---|---|
| 载荷附近 | `0x0004` | 一个 `VisionTask`，YAML 例如 `configs/load_near.yaml` |
| 载荷下方 | `0x0005` | 另一个实例 |
| 大臂下方 | `0x0006` | 另一个实例 |
| 高度限位 | `0x0007` | 另一个实例。检测仍是骨架 |
| 卷扬结冰 | `0x0008` | 一个相机内1～2个对象共用一个实例；完整表面 + HSV 粗估，需要 OpenCV |

库不打开相机，也不读中控。调用 `detect` 的宿主每帧做两件事：

1. 把本进程取到的 NV12 填进 `Frame`。`camera_id` 必须在这一份 YAML 的 `video_channel` 里。
2. 把工况填进 `MachineContext`。数值由中控或 PLC 等其它进程送来，宿主收到后抄进结构体。库不打开那条通道。

同一功能的多路相机共用这一个实例，每张图调用一次 `detect`。载荷附近是臂头、左长焦、右长焦各一次。一次 `detect` 只处理一帧，三路结果怎么合成一次告警由宿主做。

板上用来跑通实例的程序是 `tools/task_cam`，可执行文件名 `evs_task_cam`。它用 GStreamer 打开摄像头，再调用 `VisionTask`。程序、YAML、相机配置和模型都相对当前目录，不写死安装路径。`configs/eval_yolox.yaml` 已经是载荷附近：`module_id: 0x0004`、`backend: rknn`、`output_format: yolox`、`person_class_id: 0`、`video_channel: [0]`，`Model_path` 为 `yolox.rknn`。这个程序把 `context_valid` 和 `is_working` 写成 1，只为验证链路；产品里换成中控传来的字段。

在放有这些文件的目录里执行：

```bash
./evs_task_cam --task eval_yolox.yaml --config camera_config.json --channel 0 --frames 5
```

跑通的标志是 stderr 里有 `task ready: module=0x0004`，stdout 里每帧 `status=0x00000000`，末行 `done frames=5/5`。`alarm=LogOnly` 且 `distance_m=-1` 表示人检出了，地面距离没算出来，只记录。`tools/v4l2_detect` 不走 `VisionTask`，界面上的框不是这条告警。

---

## 3. 输入

### `Frame`（`evs/frame.h`）

| 字段 | 含义 |
|---|---|
| `camera_id` | 与 YAML `video_channel` 比对。不匹配返回 `InvalidCameraIndex`，本帧丢弃 |
| `width` `height` | 像素尺寸，必须大于 0 |
| `stride` | NV12 时是 Y 平面每行字节数；RGB/BGR 时是每行字节数。0 表示 `width` 或 `width*3` |
| `format` | `NV12`、`RGB`、`BGR` |
| `timestamp_us` | 宿主时钟，库只记下来，不解释 |
| `data` | 像素指针。空指针返回 `NullPointer` |
| `dma_fd` | 没有 dma-buf 时为 -1。当前 CPU 预处理不读它 |

### `MachineContext`

工况由调用 `detect` 的宿主每帧填写。数值来自中控或 PLC，宿主抄进这个结构体后再传入。库不读取那条通道。人员策略用这些字段判断「静止」还是「作业」：

| 字段 | 含义 |
|---|---|
| `context_valid` | 默认 true。false 表示本帧没有可信工况 |
| `focal_length` | 焦距。默认 0 |
| `hoist_moving` `telescope_moving` | 起落钩、伸缩是否在动 |
| `hoist_radius` | 幅度，米。`0x0006` 用它当扇形半径，并夹到至少 50 m |
| `boom_length` `boom_angle` | 臂长（米）和臂尾角度（度）。已传入，人员策略当前不用 |
| `is_working` | 0 静止，1 作业。为 1 时直接视为作业；为 0 时仍看起落钩和伸缩 |

「作业」的判定：`context_valid` 为真，并且 `is_working` 为 1，或者起落钩、伸缩任一在动。

---

## 4. 输出 `Result`（`evs/status.h`）

| 字段 | 含义 |
|---|---|
| `status` | 0 成功，否则 `0xMMMMEEEE` |
| `alarm` | `None` / `LogOnly` / `Alarm` |
| `targets` | 本帧目标。坐标已经映回原图 |
| `limit_state` | 高度限位预留。当前人员任务恒为 0 |
| `ice_ratio` | 仅单对象有效本帧结果为0..1，双对象/未知为-1；读取逐对象valid/status及测量时间戳 |
| `camera_blind` | 光学致盲预留。当前判断是占位，恒为 false |
| `camera_channel` | 32 位。每个字节一路摄像头，低字节是本帧 `camera_id`，其余字节为 `0xFF` |

### `Target` 坐标

`x`、`y` 是框中心，`w`、`h` 是宽高，相对**原图**，范围 0..1。像素换算：

```
x1 = (x - w/2) * 原图宽
y1 = (y - h/2) * 原图高
x2 = (x + w/2) * 原图宽
y2 = (y + h/2) * 原图高
```

`ground_x_m`、`ground_y_m` 是 mapper 输出的地面坐标（米），`ground_ok` 表示映射是否成功。`distance_m` 和 `angle_deg` 再由 task 参考系统一换算。未写 `ground:` 时兼容旧行为：0–3 号相机使用内置/`/userdata` 单应矩阵，`distance_m` 取地面 `Y`，角度范围保持 `-180°..180°`。`ground.distance_mode: radial` 计算到参考点的二维欧氏距离，角度范围为 `0°..360°`。`reference: task` 必须搭配 `radial`，未知枚举值会直接拒绝配置。映射失败时 `distance_m=-1`，目标仍保留但不计入告警连续帧。`EVS_GROUND_MODEL` 仅用于调试强制覆盖所有摄像头 mapper，启用时会打印警告。

`Target` 新增的地面字段位于结构体末尾。宿主若直接链接 C++ 静态库，升级后必须使用同一版本头文件重新编译，不能把旧版 `Target` 二进制布局与新版库混用。

### 地面映射扩展接口

```cpp
struct GroundPoint {
    bool ok;
    float x_m;
    float y_m;
};

class IGroundMapper {
public:
    virtual uint32_t configure(const CameraGroundConfig& cfg) = 0;
    virtual GroundPoint map(const Target& target,
                            const Frame& frame,
                            const MachineContext& ctx) const = 0;
};

std::unique_ptr<IGroundMapper>
create_ground_mapper(const CameraGroundConfig& cfg);
```

`map()` 只负责生成统一地面 X/Y，不直接判断告警。`VisionTask` 随后调用插件的 `ground_reference()` 获得 task 参考点，再统一填写 `distance_m`、`angle_deg`。没有 mapper、mapper 返回 `ok=false` 或 task 参考点无效时，距离为 `kDistanceUnknown`（-1）。

### 错误码

返回值是 `0xMMMMEEEE`。高 16 位是该 `VisionTask` 的 `module_id`，低 16 位是错误类型。任务还没建起来时高 16 位为 `0x0000`。例如载荷下方参数无效是 `0x00051001`，大臂下方同一类错误是 `0x00061001`。

低 16 位按公共表使用，定义在 `EvsStatus`。`0x0000`–`0x0FFF` 只保留成功。下表只列现在会返回的项，高 16 位写成 `0x0000`，实际返回时换成任务的 `module_id`。

| 低 16 位 | 名字 | 何时出现 |
|---|---|---|
| `0x0000` | `Success` | |
| `0x1001` | `InvalidParameter` | 宽高非法、地面相机号重复 |
| `0x1002` | `NullPointer` | `frame.data == nullptr` |
| `0x1003` | `OutOfRange` | 高度限位角度或结冰面积比例越界 |
| `0x1005` | `InvalidCameraIndex` | `camera_id` 不在 `video_channel` |
| `0x1007` | `NotInitialized` | 未 `initialize` 就 `detect` / `updateConfig` |
| `0x1009` | `NotSupported` | 后端没编进库、没有对应功能插件，或 `updateConfig` 试图改模型契约 |
| `0x100A` | `HardwareFault` | 预留给光学致盲 |
| `0x2002` | `ModelLoadFailed` | 模型文件读失败或加载失败 |
| `0x2003` | `ModelInvalidFormat` | `output_format` 不认识，或模型输入属性异常 |
| `0x3001` | `InferFailed` | NPU 推理调用失败 |
| `0x3003` | `InvalidImageSize` | `detectPrepared` 的张量尺寸和模型不一致 |
| `0x4001` | `ConfigInvalid` | 配置文件打不开，或严格标定矩阵加载失败 |
| `0x4002` | `ConfigParse` | YAML 字段非法，例如未知的 `reference` / `distance_mode` |
| `0x6001` | `StorageRead` | 模型文件读不到 |

各功能自己的错误用 `make_module_error(module, code)`，低 16 位落在 `0xC000`–`0xFFFF`。冰雪对象的不可观测/缺框使用0x0008C001/0x0008C002，保存在逐对象status；告警本身看alert_level或Result.alarm，不是错误码。

---

## 5. `VisionTask`

```cpp
uint32_t initialize(std::string_view config_path);
uint32_t detect(const Frame& frame, const MachineContext& ctx, Result& out);
uint32_t updateConfig(std::string_view config_path);
void release();
```

不可拷贝。析构会再调一次 `release`，重复调用是安全的。

### `updateConfig` 能改什么

立刻作用到下一帧。`updateConfig` 先在临时对象中创建全部 ground mapper，再调用 `plugin->configure`；全部成功后才一次性替换 mapper 和内部配置。mapper 创建或插件配置失败时，旧 mapper 与旧配置保持不变。

- `confidence_threshold`（解码用的置信度；旧键 `thresholds.score` 仍可读）
- `nms_threshold`（写入配置；当前解码器尚未用它做 NMS）
- `latency_budget_ms`
- `debug` 整块
- `zone`（半径、连续帧、扇区、遮挡扇区）
- `on_missing_context`、`work_motion_epsilon`
- `ground` 整块（mapper 类型、矩阵、安装参数、通用 `params`、参考点和距离模式）
- 高度限位：`start_angle_deg`、`stop_angle_deg`、`min_frames`、`still_timeout_s`、`basic_boom_only`
- 卷扬冰雪：`area_ratio`、`ice_saturation_max`、`ice_value_min`；下一次调用生效；模型、输入尺寸及相机需释放重建。冰雪不保持报警。

人员策略热更新后，下一帧的 `person_decide` 使用新阈值。连续帧计数不会因为 `configure` 清零，只有 `release` 会 `reset`。

不能改，返回 `NotSupported`，原配置保持不变：

- `backend`、`Model_path`
- `output_format`、`person_class_id`、`decoder_classes`
- `module_id`

输入尺寸、input_format、letterbox、video_channel也不在热更新列表里。普通推理路径当前会返回0但忽略这些变更，不能将成功返回当成这些字段已生效；应release后initialize或重新建一个VisionTask。冰雪模型参数与相机有额外拒绝检查，详见冰雪契约。本轮文档修正没有改变该既有源码行为。

`0x0007` 由 `HeightLimitPlugin` 写 `limit_state`：模型没有开关框时为 1，有框之后由 `judge_switch_mount()` 用 OpenCV 写成 2 或 3。几何规则还没写时这一步返回 0。`0x0008` 由 `WinchIcePlugin` 对模型检测框分别测量框内HSV候选覆盖率；兼容Result.ice_ratio仅单对象有效时有值，双对象用lastWinchSnowResult或Detector读取。极暗/均匀饱和白在对象status写0x0008C001，调用级可成功；报警只属于当前帧。详见 [卷扬冰雪接入](winch_ice.md)。

### 调试

```cpp
const evs::DebugStats& debugStats() const;
std::string debugSnapshot() const;          // JSON：配置、计数、dump 策略
void setDebugConfig(const evs::DebugConfig& cfg);
bool saveLastInput(const std::string& path) const;  // 仅 debug.enabled 时缓存了上一帧输入
```

`DebugStats` 里有帧数、错误数、告警数、超预算次数，以及预处理 / 推理 / 解码 / 总耗时的 last、min、max、avg、p95。

`saveLastInput` 在调试关闭时没有缓存，返回 false。

---

## 6. 任务 YAML

解析器是受限YAML子集，支持标量、一层嵌套map、行内数组，以及专用ground.cameras映射列表；支持块式model.path/input_width/input_height/warmup。示例见configs/load_near.yaml、configs/eval_rknn.yaml及configs/winch_ice.yaml，不是任意YAML结构都可使用。

| 键 | 作用 |
|---|---|
| `task` | 名字，仅标识 |
| `module_id` | `0x0004`载荷附近、`0x0005`载荷下方、`0x0006`大臂下方、`0x0007`高度、`0x0008`冰雪 |
| `camera.video_channel` | 接受的相机号，如 `[0]`。旧键 `camera_ids` 仍可读 |
| `backend` | `mock`、`x86`、`onnx`、`rknn`。必须是本次编译编进去的那种 |
| `Model_path` | 模型文件。相对进程当前目录，例如yolox.rknn。也支持model字符串或model.path；冰雪同文件别名冲突时拒绝 |
| `output_format` | `flat6`、`yolov8`、`yolo11`（别名 `yolo11_raw`、`yolo11_decoup`）、`yolox` |
| `person_class_id` | `yolov8` / `yolo11` 只保留该类。`-1` 保留全部 |
| `decoder_classes` | 行宽 = 4 + 这个值。COCO 为 80 |
| `input_width` `input_height` | 模型输入，通常 640×640 |
| `input_format` | `rgb` 或 `bgr`，模型输入的通道顺序 |
| `letterbox` | true：保持比例，灰边 114。false：拉伸 |
| `confidence_threshold` | 置信度下限。旧键 `thresholds.score`、`score_threshold` 仍可读 |
| `on_missing_context` | `alarm` 或 `log_only`。只对 `0x0004` 生效 |
| `work_motion_epsilon` | 保留的兼容字段，默认0.1；当前人员作业判断未使用该数值 |
| `ground.enabled` | 是否执行地面映射 |
| `ground.reference` | `camera_origin` 保持旧参考原点；`task` 使用插件提供的 task 参考点 |
| `ground.distance_mode` | `legacy_forward_y` 保持旧 Y 距离；`radial` 计算二维距离 |
| `ground.cameras[].id` `mapper` | 每路相机及其 `homography` / `crane_pose` mapper |
| `ground.cameras[].params` | mapper 专属键值。解析器保留原始字符串，由 mapper 的 `configure()` 转成所需类型 |
| `params.matrix` | homography 单应矩阵路径 |
| `params.calibration_width` `calibration_height` | homography 标定图像尺寸，默认 1280×720 |
| `params.strict_matrix` | true 时矩阵读取失败直接拒绝配置；false 时 0–3 号相机可回退内置矩阵 |
| `latency_budget_ms` | 单帧超过则打日志并计入 `over_budget` |
| `zone.type` | `circle` 或 `sector` |
| `zone.radius_m` | 圆半径。`0x0004` / `0x0005` 默认 10 |
| `zone.consecutive_frames` | 作业中连续多少帧有人升级为 `Alarm` |
| `zone.half_angle_deg` | `0x0006` 扇形半角，默认 40 |
| `zone.has_occlusion` `occ_axis_deg` `occ_half_deg` | 仅 `0x0005`。遮挡扇区内的人不算进入告警区 |
| `debug.*` | 见 `DebugConfig`。默认全关 |

完整的单路 homography 配置：

```yaml
ground:
  enabled: true
  reference: camera_origin       # camera_origin | task
  distance_mode: legacy_forward_y # legacy_forward_y | radial
  cameras:
    - id: 0
      mapper: homography
      params:
        matrix: /userdata/matrix_0_1280x720.txt
        calibration_width: 1280
        calibration_height: 720
        strict_matrix: true
      mount:
        x_m: 0
        y_m: 0
        yaw_deg: 0
```

配置约束：

- `reference: task` 必须配 `distance_mode: radial`，否则 `initialize` / `updateConfig` 返回 `ConfigParse`（再带上该任务的 `module_id`）。
- `reference`、`distance_mode` 写入未知字符串时直接拒绝配置，不静默回退。
- `strict_matrix: true` 时矩阵不存在、内容不足 9 个数字或无法读取都会拒绝配置。
- `strict_matrix: false` 且相机号为 0–3 时，可回退到库内置旧矩阵，同时打印警告。
- `camera.video_channel` 中存在但 `ground.cameras` 未配置的相机会打印警告，该路 `distance_m` 保持 -1。
- 修改 YAML 后不会自动监视文件；重建任务或由宿主显式调用 `updateConfig(path)` 才会生效。

### 扩展新的地面算法

新增算法不修改 `VisionTask`、功能插件或 `detect()`：

1. 新增 `src/ground/xxx_mapper.cpp`，实现 `IGroundMapper::configure()` 和 `map()`。
2. 在 `src/ground/ground_mapper_internal.h` 声明构造函数。
3. 在 `src/ground/ground_mapper_factory.cpp` 注册新的 `mapper` 字符串。
4. 在 `CMakeLists.txt` 把源文件加入 `evs`。
5. 算法专属参数写入 `ground.cameras[].params`，mapper 使用 `evs/ground_params.h` 的 `ground_param_string/int/float/bool/float_array` 读取并校验。

`params` 的值在公共解析器中以原始字符串保存，因此新增畸变系数、双目基线或雷达外参等参数不需要修改 `TaskConfig`、`config.cpp` 或 `VisionTask`。

### 三个人员模块

策略在 `person_decide`。区内与否看 `Target::distance_m` 和 `angle_deg`，这两个值在 `decide` 之前由摄像头 mapper 与 task 参考系两阶段写入。距离为 -1 时人不算在区内，只记录，不计入连续帧。

| 模块 | 区域 | 无工况 | 静止 | 作业中 |
|---|---|---|---|---|
| `0x0004` | 圆，半径 `zone.radius_m`（默认 10 m） | YAML 为 `alarm` 时，区内有人立即 `Alarm`，不计数 | 区内有人只 `LogOnly`，计数清零 | 连续帧达到阈值后 `Alarm` |
| `0x0005` | 同样的圆，再挖掉遮挡扇区 | 只 `LogOnly`，忽略 YAML 的立即报警 | 同左 | 同 `0x0004` |
| `0x0006` | 扇形。半径为 `max(50, hoist_radius)`，轴线 0°，半角默认 40° | 只 `LogOnly` | 同左 | 同 `0x0004` |

---

## 7. 解码格式

`create_decoder(format, person_class_id, num_classes, model_in_w, model_in_h)` 未知格式返回空指针。`VisionTask` 在 `initialize` 时据此失败。

后端先把网络头收成统一的检测行，解码器再滤阈值和类别。YOLOv5、YOLOv11 与 YOLOX 的头解码在 `src/backend/yolo_decode.h`，由 RKNN 后端调用，不经过宿主。

| `output_format` | 解码器看到的张量 | 说明 |
|---|---|---|
| `flat6` | 每目标 6 个 float：`class, conf, x, y, w, h` | 单类人员检测器的旧布局。不按类过滤 |
| `yolov8` | 每行 `x, y, w, h, s0..sC-1`，相对模型输入 0..1 | argmax 得类别和分数。`person_class_id >= 0` 时只留该类 |
| `yolo11` | 与 `yolov8` 相同的行 | DFL、objectness、NMS 已在后端完成。这个名字用来标明模型是 YOLOv11 解耦头，解码器不再吃 9 个原始张量 |
| `yolox` | 与 `yolov8` 相同的行 | 一份 `[锚点数, 85]`，顺序为 stride 8/16/32，每个尺度里 y 在外、x 在内。中心 `(偏移+网格)×stride`，宽高 `exp(偏移)×stride`，分数为 objectness×类别。NMS 不分类别，IoU 0.5。logits 时设 `EVS_YOLOX_SIGMOID=1` |

`VisionTask` 在解码之后调用 `map_target_to_original`，所以 `Result::targets` 已经是原图坐标。若自己调用 `create_decoder`，框仍在模型输入坐标系，需要自己做这一步。

---

## 8. 不用 `VisionTask` 时

```cpp
#include "evs/preprocess.h"
#include "evs/backend_factory.h"
#include "evs/detection_decoder.h"

auto backend = evs::create_backend("rknn");   // 未编入该后端时返回空
backend->load(model_path, /*init_timeout_s=*/60);
backend->configure_input(640, 640);

evs::Preprocessor pre;
evs::PreprocessConfig pc;
pc.input_width = 640;
pc.input_height = 640;
pc.letterbox = true;
pc.target_format = evs::PixelFormat::RGB;
pre.configure(pc);

std::vector<uint8_t> input;
evs::PreprocessMap map;
pre.run(frame, input, map);

std::vector<float> output;
backend->infer(input, output);

auto dec = evs::create_decoder("yolo11", /*person_class_id=*/0, 80, 640, 640);
std::vector<evs::Target> targets;
dec->decode(output, /*score=*/0.25f, targets);
for (auto& t : targets)
    evs::map_target_to_original(map, t, frame.width, frame.height);
```

`Preprocessor::run` 输出 packed NHWC uint8，大小 `input_width * input_height * 3`。letterbox 的填充值是 114。NV12 转 RGB 使用 BT.601 查表，缩放是定点双线性。

`create_backend` 的可选字符串取决于编译宏，见下一节。`mock` 始终可用。

同一份模型由进程内 `global_registry()` 按 `backend:model_path` 引用计数，只加载一次。`VisionTask` 会自己 `acquire` / `release`。直接拿 `create_backend` 时，生命周期由调用方管理。

---

## 9. 编译进哪个后端

```bash
cmake -S . -B build -DEVS_BACKEND=mock    # 默认。单测与无模型联调
cmake -S . -B build -DEVS_BACKEND=x86     # 主机参考检测器
cmake -S . -B build -DEVS_BACKEND=onnx -DEVS_WITH_ORT=ON \
      -DONNXRUNTIME_LIB_DIR=<ort>/lib -DONNXRUNTIME_INCLUDE_DIR=<ort>/include
cmake -S . -B build -DEVS_BACKEND=rknn    # RK3576 / RK3588，需要 RKNN SDK
```

`onnx` 与 `x86` 是同一个 `X86Backend`。`EVS_WITH_ORT=OFF` 时不跑真实 ONNX，退回参考检测器。`qnn` 没有实现，工厂返回空指针。

RGA 预处理不在 `libevs` 里。它在 `tools/v4l2_detect/rga_preprocess.h`，编译选项 `EVS_WITH_RGA`。失败时应退回 `Preprocessor`。

---

## 10. 日志

进程级单例 `evs::Logger`。日志写入边界包含格式化、缓存和宿主sink异常隔离，异常时可丢弃该条日志，不改变已提交业务的成功结果。配置/查询接口不作无异常承诺。现有文件输出保留用于调试，产品接入由宿主sink接收并负责落盘，客户SDK的实际适配需按其真实接口完成。

```cpp
evs::Logger::instance().set_level(evs::LogLevel::Info);
evs::Logger::instance().set_file("/tmp/evs.log");   // 失败返回 false
evs::Logger::instance().set_sink([](evs::LogLevel, const std::string& line) {
    // 接到 syslog / Qt。空 sink 表示打到 stderr
});
```

级别：`Off=0 Error=1 Warn=2 Info=3 Debug=4 Trace=5`。一条日志的级别小于等于当前级别才输出。

宏：`EVS_LOGE` `EVS_LOGW` `EVS_LOGI` `EVS_LOGD` `EVS_LOGT`。编译期 `-DEVS_LOG_LEVEL=3` 会裁掉更细的调用。Release 默认裁到 Info，Debug 默认保留到 Trace。

`set_sink` 若按引用捕获宿主对象，对象销毁前必须 `reset_sink()`，否则后续日志打到悬空引用。

环境变量在进程启动时读取：

| 变量 | 作用 |
|---|---|
| `EVS_LOG_LEVEL` | `error` / `warn` / `info` / `debug` / `trace`，或 0..5 |
| `EVS_LOG_FILE` | 追加一个文件 sink |
| `EVS_YOLO_HEAD_MODE` | YOLOv5 头激活：`0` 原始 logits，`1` 类别已激活，`2` 全部已激活。默认自动判别。也接受 `EVS_RKNN_HEAD_MODE` |
| `EVS_YOLO11_SIGMOID` | 设置后，YOLOv11 类别分支再做一次 sigmoid |
| `EVS_YOLO_DUMP` | 设置后，把每个解码框打到 stderr |
| `EVS_RKNN_INPUT_SCALE` | float32 RKNN 输入的缩放。`1` 表示 0..255，`0.00392157` 表示 0..1。量化模型忽略 |
| `EVS_RKNN_DEBUG` | 设置后，打印每个输出头的维度和前几个值 |
| `EVS_NPU_CORE_MASK` | NPU 核掩码，覆盖 YAML 的 `npu_core_mask` |

`library_version()` 和 `library_snapshot()` 给 IPC 或日志拉一份库级快照，包含版本、编进的后端、注册表和最近日志。

---

## 11. 离线评测

`evs/offline_eval.h` 的 `EvalReport` 用任务 YAML 里的同一条预处理和后端，对 `images/` + `annotations.json` 算 precision、recall、F1、mAP。标注框与 `Target` 一样：中心加宽高，0..1，相对原图。

命令行入口是 `tools/eval_offline`，不经过人员告警。参数和数据集格式见 `docs/x86_offline_eval.md`。

---

## 12. 现在不要依赖的行为

- 未配置 `ground:` 或使用 `reference: camera_origin` 时，兼容距离仍是单应矩阵的 `Wy`，不是吊钩投影。`crane_pose` mapper 公式尚未实现，选中后距离为 -1。`reference: task` 下吊钩目前仅按局部 +Y 放置，因为 `MachineContext` 还没有绝对回转角。
- `0x0007` 已分成模型检开关和 OpenCV 判断安装姿态。安装姿态的几何规则还没写。`0x0008` 使用RKNN卷扬框＋OpenCV整框面积粗估，专用模型及现场精度待验收。
- 光学致盲不触发。
- `Frame::dma_fd` 还没有接到库内预处理。
- `qnn` 后端不存在。

冰雪正式路径始终使用RKNN定位卷扬，并以
各框面积为分母计算覆盖率。`ground.enabled:false`，单相机，每次detect处理当前帧。
`inference_enabled:false`仅用于模块0x0008的固定区域或人工框测试，不自动回退。
公开结果去掉会话完成/次数字段并增加每对象框，详情见[winch_ice.md](winch_ice.md)。
