# EdgeVision 边缘视觉框架

起重机安全检测算法库，使用C++17。公共执行链与业务插件分别构建，取图、PLC数据、界面、录像和声光控制由宿主负责。

当前卷扬冰雪版本提供**模型定位卷扬、OpenCV面积粗估和逐帧接入demo**，根目录已有面向RK3588的`ice_model.rknn`，本批精简尚未重新实机复测。不同功能的输入接口和依赖不同：

| 用途 | 入口 | 依赖与当前能力 |
|---|---|---|
| 基础构建及规则回归 | `evs::VisionTask`、mock后端 | C++17与CMake；mock不读取真实模型，推理输出为空 |
| 卷扬冰雪 | `evs::winch_ice::Detector` | OpenCV core/imgproc；RKNN卷扬框＋HSV整框面积粗估；人工框用于测试 |
| 人员/高度模型链路 | `evs::VisionTask` | 根据构建选择RKNN或x86/ORT；高度安装姿态仍有未完成规则 |
| 图片回放 | `ice_replay` | OpenCV；人工框不等于模型检测结果 |
| 冰雪摄像头demo | `winch_ice_demo` | OpenCV、GStreamer及目标相机驱动/运行插件 |

## 1. 接口与执行流程

人员/高度的默认推理路径：

```text
Frame + MachineContext
  → 预处理 → 推理后端 → 输出行解码 → 原图坐标还原
  → 地面映射（按配置）→ 功能规则 → Result
```

卷扬冰雪的客户接口：

```text
宿主判断功能开启、工况有效且新鲜、boom_angle == 0、is_working == 1
  → Detector::detect(ImageData, WinchSnowResult)
  → RKNN卷扬检测框 + 框内HSV冰雪候选 → 逐框面积占比 → 本帧结果及全局报警
```

冰雪`detect`只有图像入参和结果出参，不接收工况。一个实例绑定一个摄像头，
每次调用处理当前帧内一个或两个卷扬；任一有效对象超阈值则本帧全局报警。
没有首次结果锁定、重试上限、重复帧缓存或跨帧报警保持，宿主负责这些业务策略。
覆盖率为每个框内冰雪像素数除以该框面积，框面积已包含冰雪部分。

正式配置为`configs/winch_ice.yaml`、RKNN模型模式，加载根目录`ice_model.rknn`，
按单类YOLOX的`[1,8400,6]`原始输出接入；重锤模型不能识别卷扬。
固定区域与人工框只用于测试，加载模型失败不会自动回退。详细接入见[冰雪说明](docs/winch_ice.md)。

## 2. 目录与链接方式

| 路径 | 内容 |
|---|---|
| `include/evs/winch_ice.h` | 冰雪客户接口、配置和结果 |
| `include/evs/vision_task.h`、`frame.h`、`task_config.h` | 通用执行链及任务配置 |
| `include/evs/testing/ice_replay.h` | 人工框注入及掩码诊断，仅作验证 |
| `src/core/`、`src/vision_task.cpp` | 配置、模型资源、推理执行链、日志及调试 |
| `src/tasks/person/`、`height_limit/`、`winch_ice/` | 业务插件；冰雪含面积算法和接口适配 |
| `src/backend/`、`src/ground/` | 模型后端与地面映射 |
| `tools/ice_replay/`、`tools/winch_ice_demo/` | 冰雪回放、样例生成及摄像头demo |
| `tools/eval_offline/`、`tools/v4l2_detect/`、`tools/task_cam/` | 模型离线评测、取景验证及通用任务摄像头宿主 |
| `configs/winch_ice*.yaml` | 单对象、双对象和人工框示例，现场须重新标定 |
| `docs/` | API、部署、冰雪实现、测试记录及历史资料 |

| 库 | 内容 |
|---|---|
| `libevs_core.so.3` / `libevs.a` | 公共执行链、配置、后端、decoder、地面映射及注册表 |
| `libevs_person.so.3` / `libevs_person.a` | 人员功能0x0004/0x0005/0x0006 |
| `libevs_height_limit.so.3` / `libevs_height_limit.a` | 高度限位0x0007 |
| `libevs_ice.so.3` / `libevs_ice.a` | 卷扬冰雪0x0008 |

无版本链接名仍是`libevs_core.so`、`libevs_ice.so`等。动态消费者链接公共库与所用插件；仅依赖静态注册的宿主需要保留插件的加载/登记对象。静态消费者应整库链接所用插件归档及`libevs.a`，并链接对应第三方依赖；单独`libevs.a`不包含全部业务插件。

当前ABI为3，公共结构及插件虚表已有变化，宿主、头文件、核心库及相关插件须成套重编。为什么版本后缀变化、如何检查实际加载路径，见[交叉编译说明](docs/cross_compile.md)。`cmake --install`当前安装核心与冰雪库、头文件、冰雪工具及资料；其他插件需按实际产品交付范围另外收集。

## 3. 快速开始

### 3.1 PC基础回归：mock、无OpenCV

在仓库根目录执行，需要C++17编译器与CMake；Python可供附加检查使用：

```bash
cmake -S . -B build_mock -DEVS_BACKEND=mock \
  -DEVS_WITH_OPENCV=OFF -DEVS_WITH_GSTREAMER=OFF -DEVS_BUILD_TOOLS=OFF
cmake --build build_mock -j2
ctest --test-dir build_mock --output-on-failure
```

这验证基础执行链和现有规则，不证明真实模型推理或冰雪像素算法可用。以实际CTest结果和日志验收，不固定要求历史某个`PASS`计数。

当前`src/backend/x86/x86_backend.cpp`含非文本内容，`EVS_BACKEND=x86/onnx`完整构建存在已复现阻塞。本轮未修复该后端；不把mock构建通过称作x86/ORT真实模型通过。需要该路径时，先恢复正确源文件，再按[离线评测说明](docs/x86_offline_eval.md)验证。

示例数据生成脚本实际写入`tools/eval_offline/fixtures`。后端恢复后，离线命令应使用该目录：

```bash
python3 tools/eval_offline/gen_fixtures.py
./build_x86/eval_offline --task configs/eval_demo.yaml \
  --dataset tools/eval_offline/fixtures --iou 0.5 --score 0.45 --dump --out ./out
```

上面的`build_x86`须已用x86后端成功构建；当前阻塞未解除前，该命令不作为可通过的快速开始。

### 3.2 Ubuntu板端或PC：冰雪像素算法与demo

Ubuntu板可以直接原生编译，不要求先交叉编译。以下依赖安装命令仅适用于具备相应软件源和权限的Ubuntu环境：

```bash
sudo apt-get update
sudo apt-get install build-essential cmake pkg-config python3 libopencv-dev \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  gstreamer1.0-tools gstreamer1.0-plugins-base gstreamer1.0-plugins-good
cmake -S . -B build_ice -DCMAKE_BUILD_TYPE=Release \
  -DEVS_BACKEND=mock -DEVS_WITH_OPENCV=ON -DEVS_WITH_GSTREAMER=ON
cmake --build build_ice -j2
ctest --test-dir build_ice --output-on-failure
cmake --install build_ice --prefix "$PWD/package_ice"
```

这里的mock构建仅供内部插件像素回归，正式Detector和demo始终要求卷扬模型，须用RKNN构建，见[冰雪编译说明](docs/winch_ice.md#编译与运行)。摄像头demo需要GStreamer开发依赖；缺失时CMake可能省略摄像头工具，回放与库仍可构建。跨架构部署前核对`file`和`ldd`，PC x86_64库不能直接放到ARM板运行。

合成像素和人工框测试已移到 `tests/`，由CTest调用内部插件，不再通过正式库切换模式，
也不随安装包交付。真实模型图片/相机demo的编译、调用和限制见[冰雪说明](docs/winch_ice.md)。

## 4. 冰雪客户API示例

以下程序示范两参API的缓冲区填写与结果读取，运行目录为仓库根目录，须使用RKNN构建。
输入为合成像素，模型可能检不出卷扬；实际宿主应填入相机图像，不接真实PLC：

```cpp
#include "evs/winch_ice.h"
#include <iostream>
#include <vector>

int main() {
    using namespace evs::winch_ice;
    WinchSnowConfig config;
    if (load_config("configs/winch_ice.yaml", config) != 0)
        return 1;
    Detector detector;
    if (detector.initialize(config) != 0)
        return 2;

    std::vector<uint8_t> pixels(200 * 100 * 3, 60);
    for (int y = 0; y < 100; ++y)
        for (int x = 0; x < 60; ++x)
            for (int c = 0; c < 3; ++c)
                pixels[(y * 200 + x) * 3 + c] = 230;
    ImageData image;
    image.data = pixels.data();
    image.width = 200;
    image.height = 100;
    image.channels = 3;
    image.timestamp_ms = 0; // 诊断信息，重复时间戳也处理当前图像
    image.size_bytes = pixels.size();

    WinchSnowResult result;
    const auto status = detector.detect(image, result);
    // 库只返回本帧；宿主自行决定是否保持历史报警。
    if (result.alert_level == AlertLevel::ALERT_WARN)
        std::cout << "alarm\n";
    std::cout << "status=" << status << " objects=" << result.objects.size() << '\n';
    return status ? 3 : 0;
}
```

保存为`ice_example.cpp`后，在常见`lib`安装布局下动态链接：

```bash
c++ -std=c++17 ice_example.cpp -Ipackage_ice/include -Lpackage_ice/lib \
  -levs_ice -levs_core '-Wl,-rpath,$ORIGIN/../lib' -o package_ice/bin/ice_example
./package_ice/bin/ice_example
```

若自定义了`CMAKE_INSTALL_LIBDIR`，请相应调整链接路径和RUNPATH。生产宿主先根据功能开关、真实工况有效性与时效、零角度及工作状态决定是否调用；`detect`不自行检查这些工况。调用期间图像内存必须有效。

RGB/BGR使用3通道；NV12须显式指定格式、1通道、偶数宽高和合法行距，UV紧随Y且使用同一行距。灰度尚不支持。`size_bytes=0`表示调用者自行保证缓冲区足够；非零时库校验长度。

结果分别读取调用status、逐对象valid/status及本帧alert_level。无框返回空对象，
不可观测对象比例无效，不使用上一帧结果。对象框和本帧编号均在结果中返回；
没有跨帧主副卷扬身份跟踪。完整说明见[冰雪说明](docs/winch_ice.md)。

## 5. 人员/高度的通用宿主接入

通用入口为`VisionTask::detect(const Frame&, const MachineContext&, Result&)`。`Frame`使用原图像素；工况由宿主真实采集并按帧更新。下面是可编译的接入函数示例，任务初始化、采集和实际绘制/声光由宿主实现：

```cpp
#include "evs/vision_task.h"
#include <iostream>

void process_person_frame(evs::VisionTask& task, const evs::Frame& frame,
                          const evs::MachineContext& context) {
    evs::Result result;
    if (task.detect(frame, context, result) != 0)
        return; // 此函数面向普通人员结果，冰雪使用专用结果结构
    for (const auto& target : result.targets) {
        const int left = int((target.x - target.w / 2) * frame.width);
        const int top = int((target.y - target.h / 2) * frame.height);
        const int right = int((target.x + target.w / 2) * frame.width);
        const int bottom = int((target.y + target.h / 2) * frame.height);
        std::cout << left << ',' << top << ',' << right << ',' << bottom << '\n';
        // 绘制前按宿主画布裁剪边界。
    }
    if (result.alarm == evs::AlarmState::Alarm)
        std::cout << "alarm\n"; // 宿主在这里接声光/联锁
    else if (result.alarm == evs::AlarmState::LogOnly)
        std::cout << "log only\n";
}
```

`Target.x/y`为归一化中心坐标，`w/h`为宽高。人员任务可配置多路相机，但地面距离需要对应相机标定；改变分辨率、机位或视场时，应核对mapper的标定尺寸与缩放策略。冰雪一个实例仅允许单相机，按模型框统计；更换相机后需重新初始化并核对模型定位效果。

`tools/task_cam`走通用任务规则；`tools/v4l2_detect`主要用于取景、预处理、推理和画框，不调用VisionTask人员告警。库不打开相机，也不链接Qt/GStreamer/RGA；顶层CMake会为宿主工具查找相应依赖。

完整通用API、像素布局及工况约定见[host_api.md](docs/host_api.md)和[api.md](docs/api.md)。

## 6. 配置、换模型和热更新

YAML是仓库受限子集，不是通用YAML引擎。以下为主要分组，完整冰雪字段及合法值见[冰雪说明](docs/winch_ice.md)：

| 分组 | 主要字段 |
|---|---|
| 任务及相机 | `task`、`module_id`、`camera.video_channel`、`camera.image_width/image_height`；兼容`video_channel/camera_ids` |
| 模型 | `backend`、平铺`Model_path`或字符串`model`；支持块式`model.path/input_width/input_height/warmup`，也兼容平铺尺寸/预热 |
| 推理输入/解码 | `input_format`、`letterbox`、`output_format`、`person_class_id`、`decoder_classes` |
| 阈值/区域 | `confidence_threshold`、`score_threshold`、`thresholds.score`、`nms_threshold`、`zone.*` |
| 地面映射 | `ground.enabled/reference/distance_mode/cameras`、每路mapper/标定及`params` |
| 工况/性能 | `on_missing_context`、`work_motion_epsilon`、`init_timeout_s`、`latency_budget_ms`、`min_fps`、`npu_core_mask` |
| 高度 | `start_angle_deg`、`stop_angle_deg`、`min_frames`、`still_timeout_s`、`basic_boom_only` |
| 冰雪 | `ice_saturation_max`、`ice_value_min`、`area_ratio`（模型定位始终启用） |
| 其他 | `debug.*`、`canary.canary_enabled`；字段可加载不表示库实现全部宿主灰度发布业务 |

客户配置仍有`model_config`与`detect_config`，适配层映射到通用TaskConfig当前的平铺模型字段。本次未进行两者公开布局统一；详见[配置结构分析](卷扬冰雪_API接口对照与倍率接口参考.md)。

### 模型输出边界

`output_format=yolov8/yolo11/yolox`选择上层行解码器。当前这些decoder接收`[N,4+C]`中心框及类别分数行矩阵。冰雪RKNN路径使用专用适配：`yolox`按stride=8/16/32解码`[N,5+C]`，置信度取obj×class；`yolov8`接收已解码框；`flat6`为`[class,confidence,center_x,center_y,width,height]`。其他任务的RKNN原始头仍由`src/backend/yolo_decode.h`处理，不能将任意原始头直接交给行decoder。

`decoder_classes`可改变上层行宽，但当前原始头辅助层仍有`kClasses=80`、`kInputSize=640`及锚框常量。自训2类原始头、其他尺寸/布局不能只改YAML就认定支持，需适配后端并验证。`nms_threshold`目前可加载和保存，但原始头NMS使用辅助层常量；冰雪模型模式单独使用配置的类别、置信度和NMS阈值筛选卷扬框，HSV负责像素分类。

### 配置生命周期

已初始化实例重复`initialize`返回成功并保留原配置。更换模型或安装参数应先`release`再`initialize`，或新建实例；修改磁盘YAML不会自动生效。

| 入口 | 当前行为 |
|---|---|
| 冰雪`updateConfig(DetectConfig)` | 更新面积/HSV/置信度/NMS阈值，非法参数拒绝；下一次detect生效 |
| 冰雪模型、输入布局与相机通道 | 释放后重新初始化 |
| 通用`updateConfig`的backend/Model_path/module_id/decoder绑定参数变化 | 返回NotSupported，需释放重建 |
| 普通推理路径的输入宽高、input_format、letterbox、video_channel变化 | 当前存在返回0但忽略这些变更的遗留行为；需释放重建，不能按返回0认定它们已生效 |
| 通用区域、工况策略、ground、调试等允许字段 | 按通用API热更新；参数/mapper校验失败保留旧策略 |

## 7. 日志、部署及已知限制

日志可通过`Logger::set_sink`交给宿主，产品落盘由宿主负责；仓库原有文件/缓存调试能力仍保留。写记录的异常已隔离，设置/查询仍可能分配；实际客户日志SDK尚未接入，不承诺零动态分配。接口见[日志说明](docs/api.md#10-日志)。

常用诊断变量按路径使用：

| 变量 | 用途 |
|---|---|
| `EVS_LOG_LEVEL`、`EVS_LOG_FILE` | 记录级别及可选调试文件 |
| `EVS_YOLO_HEAD_MODE` | 原始头激活解释：0原始、1类别已激活、2全部已激活；默认自动判别 |
| `EVS_YOLO11_SIGMOID`、`EVS_YOLOX_SIGMOID` | 对应模型头分数解释，需结合实际导出契约 |
| `EVS_RKNN_DEBUG`、`EVS_RKNN_INPUT_SCALE`、`EVS_NPU_CORE_MASK` | RKNN输出、float输入量纲及NPU亲和 |
| `EVS_RGA_CSC`、`EVS_RGA_NOPAD`、`EVS_DUMP_BENCH` | 宿主RGA诊断，不属于冰雪像素算法入口 |

当前仍未处理或未验收：

- x86/onnx后端非文本源码阻塞，普通推理的部分热更新字段被忽略。
- CMake的RGA自动查库被预定义空变量跳过；当前需显式`RGA_LIB`和匹配头文件。此说明不代表已修复自动分支或验收硬件。
- 卷扬模型已到位，RK3588板端验收、灰度和影子候选输出未完成；本版只提供本帧编号；HSV对高光/阴影雪等有已知局限。
- 目标Ubuntu板、相机真实stride/offset、PLC时钟、准确率、端到端时延及长期运行需现场记录。回放和主机测试不替代这些验收。

[交叉编译与ABI说明](docs/cross_compile.md)区分Ubuntu原生编译和目标sysroot交叉编译。旧工具别名、固定路径及2026-10-07性能/精度记录已保存在[历史联调资料](docs/history/README_20261007_inference_notes.md)，不作为当前版本的测试通过证明。

## 8. 后续工作与验证记录

优先在RK3588上核验卷扬模型的预处理与参考输出，完成板端独立像素真值验证；灰度及影子输出另行安排；按客户实际接口接日志SDK。公共构建和配置遗留问题需分别修源码并增加对应回归。

冰雪功能、上板步骤和待验收范围见[完整说明](docs/winch_ice.md)。各轮实际运行的检查见[验收记录](docs/winch_ice_acceptance.md)，其中历史轮次与后续文档修订分开记录，不把未执行项目写为通过。
