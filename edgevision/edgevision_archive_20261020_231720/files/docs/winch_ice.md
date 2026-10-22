# 卷扬冰雪：模型定位、逐帧计算

## 行为与接口

一个 `Detector` 绑定一个摄像头。`initialize` 加载并检查卷扬模型；每次
`detect(image, result)` 都处理当前图片，返回其中一个或两个卷扬的结果。
重复时间戳也重新处理，没有首次结果锁定、次数上限、结果缓存或报警保持。
宿主决定工况是否满足、调用频率、重试和报警保持。一个实例各方法串行调用。

```text
原图 → 模型预处理 → RKNN卷扬检测 → 框还原到原图、裁剪、类别过滤及NMS
     → 每个框内HSV冰雪候选及3×3开闭运算 → 各框比例 → 本帧全局报警
```

每个卷扬独立计算：

```text
surface_pixels = 原图有效矩形框的 width × height（包含冰雪覆盖部分）
snow_pixels    = 处理后位于该框内的冰雪候选像素数
ice_ratio      = snow_pixels / surface_pixels
本帧全局报警   = 任一有效卷扬 ice_ratio >= area_ratio（默认0.5）
```

分母不再额外加冰雪面积。各框分别生成掩码和计数，不合并两个卷扬的面积。
框使用原图左上角加宽高的半开区间。模型框还原并裁剪到图像边界后，左右端点
分别四舍五入为像素坐标；显示框与面积分母使用相同端点。

`initialize / detect / updateConfig / release` 方法形式保持不变，C++17，
入口 `include/evs/winch_ice.h`。本次公开结构已调整：移除检查完成、次数、会话状态，
每对象增加 `box`（框、类别、置信度）。第一批精简又移除了 `InputMode`、固定区域对象清单、
多边形和参考尺寸字段；公共配置只保留模型、检测参数和相机通道。应用须使用当前头文件及配套库重新编译。
库名沿用当前 `libevs_core.so.3`、`libevs_ice.so.3`（及静态库）。

- `status`/方法返回值：调用错误。错误调用清空本次对象和报警，不携带上一帧结果。
- `processed`：本次是否完成图像处理；`objects[i].valid/status/reason`：逐对象有效性。
- `result_code`：至少一个有效对象时 `RESULT_MATCH`，否则 `RESULT_NO_RESULT`。
- `alert_level`：仅当前帧的报警。宿主要保持历史报警时自行保存。
- `objects[i]`：本帧框、冰雪像素数、框面积、比例和状态。无效比例为-1，不是0%。
- 模型模式按框中心从左到右编号0/1，同横坐标按纵坐标排序；编号仅在本帧有效，
  不代表跨帧跟踪或主副身份，也不推断是否漏检了第二个卷扬。
- 无框：调用成功、`processed=true`、空对象、`ObjectNotFound`、无有效比例。
- NMS后仍有交叠像素的框：相关对象无效，`OverlappingBoxes`，不猜测冰雪归属。
  多于两个候选：调用错误，`UnexpectedObjectCount`，不静默截取。
- 表面小于3×3、极暗或均匀过曝白等不可观测情形不能作为正常0%；对象状态无效。
- `snow_detections` 保持为空：卷扬框不是冰雪目标框；实际卷扬框在各对象的 `box`。
  `inference_time_ms` 是本帧推理耗时（整数毫秒），`processing_time_ms` 是总处理耗时。

## 模型约定

正式配置为 `configs/winch_ice.yaml`，后端 `rknn`，始终加载模型。
先前模型YAML中的 `ice_mode: model` 和 `inference_enabled: true` 仍可读取，
不再用于选择执行分支；旧固定区域、人工框和标定配置明确报错。
仓库根目录已有卷扬模型 `ice_model.rknn`，配置直接使用此路径。
`zch_generalize_int8.rknn` 是重锤模型，不用于卷扬。

从文件内嵌元数据核对：目标RK3588，转换工具版本2.1.0+708089d1，输入
`images [1,3,640,640]`，单输出 `predictions [1,8400,6]`。
SHA256：`cff5bb8245653e2fcf6eec66ae7083aa6506fad1d2e4fd89bec933a6afe7d5e8`。
图中包含YOLOX的reg/obj/cls分支，obj/cls已带Sigmoid，未见坐标grid/exp解码节点。
据此按单类原始YOLOX输出接入，类别0视为卷扬；输出语义和预处理最终须与算法方
参考结果及RK3588实测核对，不能仅凭张量形状证明识别正确。

当前 `output_format: yolox` 每行6项为 `tx,ty,tw,th,objectness,class_probability`。
先按stride=8/16/32、每层先行后列展开的网格解码：

```text
center_x/y = (tx/ty + grid_x/y) × stride
width/height = exp(tw/th) × stride
confidence = objectness × class_probability
```

后端将框归一化后交给现有行解码器，再撤销letterbox映射回原图。
obj/cls不重复Sigmoid，8400个候选不等于8400个卷扬；置信度筛选与NMS后才是检测框。
适配支持下表三个明确契约，不能把本模型的6项当作 `flat6`：

| output_format | 张量形状 | 每行含义 |
|---|---|---|
| yolox | `[1,N,5+C]` 或 `[N,5+C]`，N须匹配stride=8/16/32网格总数 | 原始tx/ty/tw/th、目标概率、C个类别概率；640×640单类即 `[1,8400,6]` |
| yolov8 | `[1,N,4+C]`、`[1,4+C,N]`，或对应二维形式 | 中心x/y、宽高是模型输入像素；后接C个类别概率，取最高分类 |
| flat6 | `[1,N,6]` 或 `[N,6]` | 类别、置信度、归一化中心x/y、宽高，坐标相对模型输入 |

输入为batch=1、3通道；应用预处理产生NHWC字节，RKNN按模型转换。
当前外部输入配置为RGB、0..255、不额外除255，使用居中letterbox；模型元数据mean=0、
std=1及rgb2bgr转换由RKNN图处理。外部颜色顺序和补边方式仍需与算法方参考图片
核对，不能仅凭rgb2bgr标志再次在宿主交换通道。
`input_format` 为rgb/bgr，`letterbox`控制等比补边。类别数由 `decoder_classes`
配置，卷扬类别由 `winch_class_id` 配置。置信度/NMS阈值均可配置。
初始化检查模型尺寸及输出形状，推理后检查张量长度、有限数及概率范围。
原始DFL/其他多头、其他坐标语义、YOLOv5输出不能仅靠改名字接入，
需按实际模型增加后处理适配；当前明确报错，不套用COCO 80类解码。
模型错误不会自动退回人工框、固定区域或mock。

## 编译与运行

在目标Ubuntu板上安装构建依赖：

```bash
sudo apt-get install build-essential cmake pkg-config python3 libopencv-dev \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  gstreamer1.0-tools gstreamer1.0-plugins-base gstreamer1.0-plugins-good
cmake -S . -B build_ice -DCMAKE_BUILD_TYPE=Release -DEVS_BACKEND=rknn \
  -DEVS_WITH_OPENCV=ON -DEVS_WITH_GSTREAMER=ON \
  -DRKNN_API_INCLUDE_DIR=/path/to/rknn/include \
  -DRKNN_API_LIB=/path/to/librknnrt.so
cmake --build build_ice -j2
ctest --test-dir build_ice --output-on-failure
cmake --install build_ice --prefix "$PWD/package_ice"
```

目标板为RK3588 Ubuntu。RKNN头文件、运行库、模型及目标芯片需匹配；PC产物不能直接放到ARM板运行。
安装命令不自动复制模型；在仓库根目录运行示例，或将配置里的 `Model_path` 改为
板上模型绝对路径。
交叉编译沿用 `docs/cross_compile.md` 的工具链方式。

图片demo读取P6 PPM；JPEG/PNG可先转成PPM。JSON打印各框比例/报警，
输出目录保存原图、框和冰雪掩码叠加图及逐对象掩码。所有命令在仓库根目录运行：

```bash
./package_ice/bin/ice_replay configs/winch_ice.yaml input.ppm /tmp/ice_image/run
./package_ice/bin/winch_ice_demo --task configs/winch_ice.yaml \
  --device /dev/video0 --width 1280 --height 720 --frames 100 \
  --simulate-context --angle 0 --working 1 --out /tmp/ice_camera/run
```

摄像头demo当前使用GStreamer紧凑NV12；每帧终端输出结果，可用图像查看器打开
生成的 `_overlay.ppm` 查看框和冰雪区域。`--simulate-context`只模拟宿主允许调用的
工况，不模拟检测结果。真实工况使用 `--context-json live.json`，示例字段为
`{"valid":1,"boom_angle":0,"is_working":1,"timestamp_ms":123}`。
示例宿主使用单调毫秒时钟，检查数据年龄不超过500ms；这些策略不在库内。
相机工具按 `--frames` 处理帧数，不因第一帧正常或报警就退出。
RK3588实测首次出帧可能超过3秒；demo首次等待8秒，后续每帧等待3秒，超时会报错退出。

## 最小调用

```cpp
#include <evs/winch_ice.h>
using namespace evs::winch_ice;
// 初始化一次，配置来自实际卷扬模型。
WinchSnowConfig cfg;
auto rc = load_config("configs/winch_ice.yaml", cfg);
Detector detector;
if (!rc) rc = detector.initialize(cfg);
// 初始化成功后，宿主每取得一帧且工况允许时调用：
// ImageData image; 设置data/width/height/format/channels/stride/size_bytes。
// WinchSnowResult result;
// rc = detector.detect(image, result);
// 检查rc，再读取objects各对象的valid、box、ice_ratio及alert_level。
```

RGB/BGR通道数为3；NV12为1、宽高和stride须偶数，UV紧随Y且使用同一stride。
stride=0表示紧凑布局；size_bytes非零则校验长度，为零由调用者保证长度。
灰度不支持。图像数据在同步调用期间有效，库不保留图像指针。
时间戳仅用于结果诊断，不控制是否处理当前帧。
`updateConfig`更新面积/HSV/置信度/NMS阈值，下一次调用生效；更换模型、
输入尺寸须release后重新initialize。

## 软件回归与板端验证

软件回归覆盖6项张量解码、三层网格与exp坐标、obj×class、letterbox回映、NMS、
双卷扬比例与20%→60%→20%的逐帧报警变化，并保留旧格式回归。
没有RK3588/NPU时，像素测试直接调用内部插件，正式 `Detector` 仍要求RKNN模型：

```bash
cmake -S . -B build_ice_test -DEVS_BACKEND=mock -DEVS_WITH_OPENCV=ON \
  -DEVS_WITH_GSTREAMER=OFF -DEVS_BUILD_TESTS=ON
cmake --build build_ice_test -j2
ctest --test-dir build_ice_test --output-on-failure
```

人工框和输出图验证直接并入现有 `tests/test_ice.cpp`，分配失败测试复用
`tests/ice_test_support.h`；没有独立像素回放程序或新增宿主接口。内部测试不安装，
不验证模型识别，也不作为宿主调用接口。`include/evs/testing/ice_replay.h` 仅保留
读取当前掩码的诊断功能，不能注入人工框。旧配置和ROI配方原样存放于
`tests/fixtures/ice_legacy/`，用于退役配置拒绝检查和历史追溯，不再运行其旧模式。
第一批只收缩配置、旧分支和多边形计算；相机UI、第三个候选、框交叠和纯白无效等
业务边界本次未调整。工作区核验范围见 `docs/winch_ice_acceptance.md`，该历史记录不随运行包安装。

HSV默认S≤80、V≥180，只是颜色规则粗估。白色背景、金属反光可能误计，阴影雪、
脏雪、透明冰可能漏计。整框含支架/背景时分母也包含它们，准确性依赖模型框定义。
合成图片、人工框和模拟RKNN仅验证软件链路，不能证明现场冰雪识别精度。
已在RK3588 Ubuntu板上执行真实模型图片测试、逐帧API测试和摄像头采集推理，
详细环境、结果及耗时保留于工作区 `docs/winch_ice_acceptance.md`；本次收紧后的代码尚未重新板测。
6张图片中5张检出卷扬、1张未检出；一张肉眼无雪图片仍得到26.90%的HSV候选比例。
还需对照算法方参考结果核验颜色/补边、框及置信度，并用独立标注数据验收冰雪面积
和报警精度。当前实机运行通过不能作为精度验收通过的依据。
