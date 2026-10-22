# 宿主接入说明

给编写宿主程序的开发者。宿主负责取图、接收工况、调用库、把结果送到界面或联锁。公共库 `libevs_core.so` 负责预处理、推理、解码和地面距离。人员、高度限位、卷扬冰雪分别在 `libevs_person.so`、`libevs_height_limit.so`、`libevs_ice.so`，宿主只链接用到的功能库。

字段级说明、YAML 全表和解码格式见 [api.md](api.md)。本文只写宿主要接的那一层。

现场修改文件位置和修改范围见 [现场修改对照说明](site_customization.md)。

冰雪原图模式可跳过模型推理，具体条件和输入约定见 [winch_ice.md](winch_ice.md)。

卷扬冰雪优先使用 `evs/winch_ice.h` 的类型化 `Detector`，参见 [冰雪完整接入说明](winch_ice.md)。以下通用接口说明主要用于其他功能。

头文件在 `include/`。宿主包含 `evs/vision_task.h` 即可，它会带上 `Frame`、`MachineContext`、`Result`。语言是 C++17。板上链接 `libevs_core.so`、用到的插件库；RKNN推理构建另需 `librknnrt.so`，无模型冰雪构建不需要它。取流用的 GStreamer、Qt、RGA 由宿主自己链接，不要链进算法库。

程序、任务 YAML、`camera_config.json` 和模型都相对进程的当前目录。先进入放这些文件的目录再启动，不要在命令里写死安装路径。`Model_path: yolox.rknn` 就是当前目录下的 `yolox.rknn`。以 `/` 开头的路径仍按绝对路径打开。

---

## 1. 宿主每帧做的事

```
其它进程的工况 ──抄进 MachineContext
相机 NV12      ──填进 Frame
        │
        ▼
VisionTask::detect(frame, ctx, result)
        │
        ▼
读 Result：框、alarm、distance_m、limit_state、ice_ratio
        │
        ▼
界面 / 声光 / 联锁
```

库不打开摄像头，也不读取中控或 PLC 的通道。人员/高度功能的工况由别的进程送来之后，宿主填进 `MachineContext`；卷扬冰雪使用专用两参Detector接口，工况仅由宿主判断，不传入冰雪库。

图像用紧凑 NV12（`stride == width`，或 `stride == 0`）。冰雪图像模式可直接使用 RGB/BGR 或带行填充的 NV12；其他模式仍按既有预处理约定传入。`Frame::data` 在 `detect` 返回前保持有效。`dma_fd` 当前预处理不读，没有 dma-buf 时填 `-1`。

宿主也可以自己做成模型输入再调用 `detectPrepared`。板上用 RGA 做 letterbox 时走这条，库里的 CPU 预处理就不再跑。`preprocess_ms` 记进统计里的 pre。`Frame` 仍带原图宽高和 `camera_id`，框和距离按原图坐标算。`tensor` 是 `inputWidth() * inputHeight() * 3` 的紧密 RGB。

---

## 2. 一个功能一个实例

每个 `module_id` 单独建一个 `VisionTask`，用对应 YAML 调用一次 `initialize`。连续帧计数留在各自实例里。`updateConfig` 不能改 `module_id`、模型路径和 `output_format`。

| 功能 | `module_id` | 配置示例 | 当前能用到哪一步 |
|---|---|---|---|
| 载荷附近 | `0x0004` | `configs/eval_yolox.yaml`、`configs/load_near.yaml` | 人员检测和告警可用。默认兼容相机地面 `Wy`；可配置 task 参考点径向距离 |
| 载荷下方 | `0x0005` | `configs/load_under.yaml` | 规则已写，地面 mapper 与参考点由 YAML 配置 |
| 大臂下方 | `0x0006` | `configs/boom_under.yaml` | 规则已写。半径用 `hoist_radius` |
| 高度限位 | `0x0007` | `configs/height_limit.yaml` | 模型没检出开关时 `limit_state=1`。检出后由 OpenCV 判断 2 或 3，规则还没写时为 0 |
| 卷扬结冰 | `0x0008` | `configs/winch_ice.yaml` | 固定 ROI + HSV 粗估，图像模式无需模型，需 OpenCV；[完整接入说明](winch_ice.md) |

同一份模型路径由库内注册表共用，人员三个功能可以指向同一个 `yolox.rknn`，各建各的实例。

人员功能的多路相机可共用一个实例；冰雪一个实例绑定一个摄像头，同画面可检测一个或两个卷扬，每次调用独立返回本帧结果。每张图调用一次 `detect`，`Frame.camera_id` 必须落在该 YAML 的 `camera.video_channel` 里。一次调用只处理一帧。载荷附近若要臂头、左长焦、右长焦一起出结论，由宿主对三次 `Result` 做汇总。

同一个实例上的 `detect` 是同步的，不要并发调用。

---

## 3. 调用顺序

```cpp
#include "evs/vision_task.h"

evs::VisionTask task;
if (task.initialize("configs/eval_yolox.yaml") != 0u)
    return 1;   // 失败已回滚，可换路径再试

evs::MachineContext ctx{};
// 每帧用中控最新值覆盖下面这些字段
ctx.context_valid = true;
ctx.focal_length = focal_length;
ctx.is_working = working_from_plc;
ctx.hoist_moving = hoist_moving;
ctx.telescope_moving = telescope_moving;
ctx.boom_angle = boom_angle_deg;
ctx.boom_length = boom_length_m;
ctx.hoist_radius = hoist_radius_m;
ctx.timestamp_us = now_us;

evs::Frame frame{};
frame.camera_id = 0;                  // 必须在 YAML 的 video_channel 中
frame.width = w;
frame.height = h;
frame.stride = 0;                     // 紧凑排列
frame.format = evs::PixelFormat::NV12;
frame.data = nv12;                    // detect 返回前有效
frame.timestamp_us = now_us;

evs::Result res{};
if (task.detect(frame, ctx, res) == 0u) {
    if (res.alarm == evs::AlarmState::Alarm) {
        // 声光或联锁
    }
}
task.release();   // 析构也会再调一次，重复调用是安全的
```

`initialize` 成功后再调用，直接返回0，不会重新加载模型。换模型时先release再initialize，或新建实例；不能只改YAML后重复initialize。

`updateConfig(path)` 下一帧起生效，用来改置信度、区域半径、连续帧数、无工况策略、调试开关以及完整 `ground` 配置。新的 mapper 会先在临时容器里全部创建，成功后一次性替换；任一 mapper 或插件配置失败时旧配置保留。改 `module_id`、`backend`、`Model_path`、`output_format`、`person_class_id` 会返回 `NotSupported`。连续帧计数不会因这次更新清零，只有 `release` 会清。

普通推理路径的输入宽高、input_format、letterbox、video_channel目前存在返回0但忽略变更的遗留行为，应释放重建，不能按返回0判断这些字段已更新。nms_threshold可保存，但当前原始头NMS使用辅助层常量。冰雪安装参数与阈值的更新范围见专用说明，不能直接套普通人员规则。

---

## 4. 工况怎么填

`MachineContext` 的数来自中控或 PLC。宿主收到后抄进结构体，再传给当次 `detect`。库按这一帧的值判断作业还是静止。

| 字段 | 单位 | 人员功能怎么用 |
|---|---|---|
| `context_valid` | — | 默认 true。false：本帧没有可信工况 |
| `focal_length` | — | 焦距。默认 0。人员告警当前不用 |
| `is_working` | 0 静止，1 作业 | 为 1 时视为作业 |
| `hoist_moving` `telescope_moving` | — | 任一为真，且工况有效，也算作业 |
| `hoist_radius` | 米 | `0x0006` 的扇形半径，库内夹到至少 50 m |
| `boom_angle` | 度 | 已传入。人员告警当前不用。高度限位公式写上之后用 |
| `boom_length` | 米 | 同上。结构体里还没有单独的「是否基本臂」字段 |
| `timestamp_us` | 微秒 | 人员流程按原行为；冰雪忽略MachineContext，图像只按自身时间戳去重 |

作业判定：`context_valid` 为真，并且 `is_working == 1`，或者起升、伸缩表明在动。

测试程序 `tools/task_cam` 把 `context_valid` 和 `is_working` 写成 1，只为在板上跑通实例。产品宿主换成中控传来的值。

---

## 5. 怎么读结果

通用接口通常在 `detect` 返回0时读取本帧结果。冰雪使用逐帧结果，分别读取调用错误、对象有效性和本帧报警；错误不会携带旧帧结果，详见[冰雪说明](winch_ice.md)。

| `Result` 字段 | 含义 |
|---|---|
| `status` | 0 成功 |
| `alarm` | `None=0` 无事，`LogOnly=1` 只记录，`Alarm=2` 报警 |
| `targets` | 本帧目标，坐标已映回原图 |
| `limit_state` | 高度限位：0 安装姿态还不能判断，1 模型未检出开关，2 错误安装，3 正确安装。2 和 3 只由 OpenCV 给出 |
| `ice_ratio` | 冰雪兼容字段：仅单对象有效结果为0..1，双对象为-1；使用lastWinchSnowResult或Detector逐对象结果 |
| `camera_blind` | 光学致盲。当前恒为 false |
| `camera_channel` | 32 位小端。字节 0 是本帧 `camera_id`，其余字节为 `0xFF`（通道 0 是真实相机，空位不能用 0） |

`Target` 的 `x`、`y` 是框中心，`w`、`h` 是宽高，相对原图，范围 0..1。画到原图像素上：

```
x1 = (x - w/2) * 原图宽
y1 = (y - h/2) * 原图高
像素宽 = w * 原图宽
像素高 = h * 原图高
```

`class_id` 在 YOLOX 人员配置下为 0。`confidence` 是置信度。`track_id` 当前恒为 -1。`ground_ok` 表示目标是否成功映射到地面；成功时 `ground_x_m`、`ground_y_m` 是统一地面坐标（米）。

`Target` 的地面字段改变了 C++ 结构体布局。升级 `libevs.a` 时宿主必须同步使用本版本头文件重新编译。

`distance_m` 单位是米，`angle_deg` 是相对于 task 参考系、从地面 +Y 向 +X 顺时针的角度。`legacy_forward_y` 模式保持旧的 `-180°..180°` 范围；`radial` 模式使用 `0°..360°`。未配置 `ground:` 时兼容旧行为：0–3 号相机读取 `/userdata/matrix_<id>_1280x720.txt`（缺失时使用内置矩阵），距离取 `Wy/100`。配置 `ground.distance_mode: radial` 后才计算目标地面点到 task 参考点的二维距离。映射失败时 `distance_m` 为 -1。

每路摄像头的算法专属参数写在 `ground.cameras[].params` 下。公共解析器只保存键值，具体 mapper 在 `configure()` 中读取并校验；新增鱼眼、双目等参数不需要修改 `TaskConfig` 或 `VisionTask`。

宿主必须保证 `camera.video_channel` 中每一路在 `ground.cameras` 都有对应 `id`；缺少时初始化会打印警告，该路检测仍运行，但目标距离保持 -1。显式标定文件建议设置 `params.strict_matrix: true`，缺文件时让初始化失败，避免实车静默使用内置矩阵。

人员告警拿 `distance_m` 和区域半径比较。-1 表示人还在结果里，但只记录，不计入连续帧。`ground.reference: task` 必须搭配 `distance_mode: radial`。0x0004/0x0005 使用插件提供的吊钩参考点；工况无效时回退到 homography 原点，以保留 `on_missing_context: alarm` 语义。当前 `MachineContext` 没有绝对回转角，因此有效工况下暂按局部 +Y 方向放置吊钩，正式二维车体坐标需宿主补充回转角。环境变量 `EVS_GROUND_MODEL` 仅用于调试强制覆盖所有相机 mapper，启用时会打印警告；`crane_pose` 公式尚未实现。

### 人员告警

| 模块 | 区域 | 无工况 | 静止 | 作业中 |
|---|---|---|---|---|
| `0x0004` | 圆，默认半径 10 m | YAML `on_missing_context: alarm` 时，区内有人立即 `Alarm` | 只 `LogOnly`，连续计数清零 | 连续帧达到 `zone.consecutive_frames`（默认 3）后 `Alarm` |
| `0x0005` | 同样的圆，再去掉遮挡扇区 | 只 `LogOnly` | 同左 | 同 `0x0004` |
| `0x0006` | 扇形。半径 `max(50, hoist_radius)`，轴线 0°，半角默认 40° | 只 `LogOnly` | 同左 | 同 `0x0004` |

`configs/load_near.yaml` 写了无工况立即报警。`configs/eval_yolox.yaml` 没写这项，默认只记录。

---

## 6. 宿主会见到的错误

高 16 位是正在运行的功能：载荷附近 `0x0004`，载荷下方 `0x0005`，大臂下方 `0x0006`，高度限位 `0x0007`，卷扬结冰 `0x0008`。还没有任务时高 16 位是 `0x0000`。低 16 位见下表。载荷下方相机号非法是 `0x00051005`，大臂下方是 `0x00061005`。

| 低 16 位 | 名字 | 宿主侧原因 |
|---|---|---|
| `0x0000` | 成功 | |
| `0x1001` | `InvalidParameter` | 宽高非法、地面相机号重复 |
| `0x1002` | `NullPointer` | `frame.data` 为空 |
| `0x1005` | `InvalidCameraIndex` | `camera_id` 不在 `video_channel` |
| `0x1007` | `NotInitialized` | 尚未成功 `initialize` |
| `0x1009` | `NotSupported` | 后端未编进本库、没有该功能插件，或热更新改了模型、后端、`module_id` |
| `0x2002` | `ModelLoadFailed` | 模型文件读失败或加载失败 |
| `0x2003` | `ModelInvalidFormat` | `output_format` 不认识 |
| `0x4001` | `ConfigInvalid` | YAML 打不开，或严格标定矩阵加载失败 |
| `0x4002` | `ConfigParse` | YAML 字段非法 |

---

## 7. 板上的参考程序

`tools/task_cam` 编出来是 `evs_task_cam`。它打开摄像头，创建一个载荷附近实例，跑若干帧后退出。工况在程序里写成固定值。

在放有 `evs_task_cam`、`eval_yolox.yaml`、`camera_config.json` 和 `yolox.rknn` 的目录里执行：

```bash
./evs_task_cam --task eval_yolox.yaml --config camera_config.json --channel 0 --frames 5
```

跑通时 stderr 有 `task ready: module=0x0004`，stdout 每帧 `status=0x00000000`，末行 `done frames=5/5`。

`tools/v4l2_detect` 只做到解码并把框画进预览，不调用 `VisionTask`。界面上的框没有人员区域告警。产品宿主按本文第 3 节调用。

### 卷扬冰雪接入补充

公开接口采用 `evs::winch_ice::Detector::detect(const ImageData&, WinchSnowResult&)`，
不接收工况；宿主决定是否调用。模型定位一个或两个卷扬，每框用OpenCV计算冰雪像素数
除以整个框面积；任一有效对象达到默认50%阈值，本帧全局报警。
每次调用重新处理，包括相同时间戳；不保留首次结果、不限重试次数、不保持历史报警。

对象结果含原图框、面积、比例和valid/status；无框返回空对象，无效比例不能当0%。
模型输出契约、测试入口、Ubuntu板端编译命令见[winch_ice.md](winch_ice.md)。
库名不变，公开结果结构已调整，宿主与库须使用匹配头文件重新编译。
