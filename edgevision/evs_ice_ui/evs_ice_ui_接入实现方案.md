# 卷扬冰雪实时 UI 接入实现方案

> 历史方案：本文保留实施前的设计和当时完成范围。后续实现见[实现与板端验证](evs_ice_ui_实现与板端验证.md)，版本1.1的验证见[核验与优化](evs_ice_ui_核验与优化.md)。

本文对应用户提出的“相机取流 → 调用卷扬冰雪 so → 实时 UI 显示”的完整 demo。基于本轮工作区源码与设备 192.168.2.127 的只读核验制定；尚未实施新增 UI、构建新库或启动相机取流测试。

现有程序逻辑见同目录《evs_ice_ui_逻辑分析.md》。原始 UI C++ 源码尚未找到，但已提取设备二进制中的 QML，并核对相机后端及 UI 的主要调用链。

## 1. 实现结论与首版范围

建议在当前 edgevision 工程新增一个有完整源码的 Qt Quick 宿主，动态链接当前配套的 libevs_ice.so.1、libevs_core.so.1，通过公开的 evs::winch_ice::Detector 调用算法。沿用旧 demo 的画面、报警、开始/停止、快照和退出操作。

首版暂按单路相机设计，默认通道 0；提供停止后切换通道的能力。用户若确认需要多路同时显示，应扩展为每路采集和每路 Detector、统一串行推理调度，不能仅把 YAML 改成多通道数组。单路相机中的两个卷扬对象属于一帧的 objects，与两路相机不同。

实时传递采用进程内存中的“原图＋本帧结果”，不再让 PPM 文件和 stdout 分别承担画面与结果传输。快照和诊断结果按需落盘。

这是技术方案，不是对旧二进制的直接修改。除非后续找到并确认对应源码，否则不能声称正在原 UI 源工程上增量修改。

## 2. 已核实的事实与实现影响

| 核验项 | 当前事实 | 实现影响 |
| --- | --- | --- |
| 目标硬件 | 设备树包含 hytop,rk3588 / rockchip,rk3588，架构 aarch64 | 按 RK3588 Ubuntu 构建，PC x86 产物不能上板运行 |
| 构建工具 | 设备有 g++、cmake、qmake | 可优先板端原生构建，使用设备实际库与头文件 |
| Qt | Qt5Core/Gui/Qml/Quick 开发元数据为 5.15.3，QtQuick.2 与 Window.2 模块存在 | 可实现 Qt Quick 宿主；实际窗口与渲染仍需运行验证 |
| 显示会话 | Xorg、/tmp/.X11-unix/X0、xcb 平台插件及用户 Xauthority 存在 | 首先验证现有 X11 会话，不按旧 UI 的 Wayland 假设部署 |
| GStreamer/OpenCV | core pkg-config 1.22.9，app/video 1.20.1，v4l2src 插件 1.20.3，OpenCV 4.5.4 | 开发信息和插件版本不完全一致，须通过实际编译及管线测试确认可用性 |
| RKNN | /lib/librknnrt.so 可见，有已部署 RKNN API 头文件 | 编译时显式指定实际 SDK 路径；存在运行库不等于新版本推理已验收 |
| 相机 | /dev/video0 为 rkcif 多平面采集设备；只读查询当前为 NV12、1920×1080、stride=1920、SizeImage=3110400 | 这是当前驱动参数，未执行 STREAMON；仍需真实取流校验 |
| 相机颜色 | V4L2 查询声明 Rec.709 | 转 RGB 时核对实际 caps 的色彩矩阵和量化范围，避免沿用固定转换系数引起 HSV 统计偏差 |
| 模型 | 工作区与板上模型 SHA256 均为 cff5bb8245653e2fcf6eec66ae7083aa6506fad1d2e4fd89bec933a6afe7d5e8 | 模型文件一致；不能因此认定最新源码已完成板端验证 |
| 设备旧配置 | l_evs/winch_ice.yaml 为 backend: mock，UI 固定 --channel 0 | 不沿用此旧 YAML 作为新版正式模型配置 |
| 当前接口 | Detector 接受紧凑 RGB/灰度，返回逐对象 box、ice_ratio、valid | UI 使用公开头文件和 objects，算法只在 so 中执行 |
| 通用结果 | 当前冰雪插件 publish() 清空 Result.targets；多对象时通用 ice_ratio 为未知 | 旧 evs_ice_cam 的 targets.front() 和旧 UI 日志协议无法直接承接新版结果 |
| 版本状态 | 当前源码在旧板测之后做过插件/API 内部架构整改 | 固定本次源码和头文件快照，重新构建配套库与宿主后验收 |

设备已有模型位于 `/home/hytop/l_evs/rk3588_model_check_20261021_134125/source/winch_model.rknn`。它可作为已核对哈希的模型来源；该目录内旧 build 产物不能直接当作当前工作区最新构建。

## 3. 程序结构与线程

```text
Qt 主线程
  控制器：开始/停止/切换通道/快照/状态
  IceView：显示同一数据包中的图像、框、比例、报警
        ↑ 读取最新完成包，按序号去重
  最新完成包（容量 1）
        ↑
算法工作线程：串行 initialize / detect / updateConfig / release
  取得最新 RGB 图像 → Detector::detect → 组合本帧图像与结果
        ↑
  最新待处理帧（容量 1，慢时覆盖旧的未处理帧）
        ↑
采集工作线程：GStreamer → GstVideoFrame 映射 → 紧凑 RGB
        ↑
  camera_config.json 所选 device
```

主线程不执行取帧、RKNN 推理或等待相机启动。算法实例的方法只在同一算法线程串行调用；同模型多实例共用后端时也要统一串行调度。

两处待交付数据均只保留最新一包。向 GUI 投递的是合并后的更新通知，避免把完整图像逐帧塞进无上限的 Qt queued signal 队列。慢时允许丢弃未处理/未显示帧，但不能无限积压造成画面越来越滞后。

每帧宿主数据包至少包含：会话代次、uint64 帧序号、逻辑通道、采集时间、原图尺寸、自持有的紧凑 RGB、API 状态和 WinchSnowResult。序号独立于 API 的 uint32 timestamp_ms，不依赖毫秒时间戳唯一性。

检测期间保持 RGB 缓冲有效。显示层取得有所有权保证的 QImage 或帧包，不能持有已 unmap 的 GstBuffer 或已释放 cv::Mat 的裸指针。停止、通道切换和重新开始后丢弃旧会话的数据包。

## 4. 采集配置与像素格式

继续使用 JSON 的 channel/device 映射，新增宿主严格校验：通道唯一、通道存在、enable=true、尺寸和帧率有效、所选通道与算法实例 camera_channel 一致。错误明确显示，不回退到第一路相机。

已核对的旧 select_entry() 会在找不到通道时回退，显式命中时不检查 enable。当前 winch_ice_demo 虽检查命令通道和算法通道一致，却没有核对 select_entry() 回退后实际返回项的 channel，因此还不能当作严格通道选择实现直接复用。

旧 GstCapture::open() 把 w、h、fps 显式标记为未使用。新宿主应记录“请求参数”和“实际协商参数”，先验证设备可用的原始格式，再协商输出 RGB。支持指定采集模式时应在正确位置应用 caps；不应仅在 videoconvert 后改尺寸就宣称传感器采集模式已改变。

优先验证 GStreamer 输出 RGB 的管线，由转换元件处理实际色彩元数据。若采用旧 NV12 路径，也须核对协商颜色矩阵、量化范围及转 RGB 的一致性。当前 /dev/video0 声明 Rec.709，不应无验证地沿用手写固定 BT.601 转换。

取样使用 GstVideoInfo/GstVideoFrame 读取格式、plane offset 和行 stride，逐行整理为 width×height×3 的紧凑 RGB，再传给 Detector。不能仅拼接 GstMemory 块并假定无行填充。库内缩放/letterbox 保持由现有模型链路完成，宿主不能先把原图强制改成 640×640 后又按原图坐标绘框。

首版按实际协商尺寸显示，并检测变化；采集超时、总线错误、格式错误分别报告。取帧采用有界等待，允许停止请求被及时检查；停止操作不阻塞 GUI 等待相机。

## 5. 算法库接入与配置

宿主只包含公开 evs/winch_ice.h，不复制 RKNN 解码或 HSV 计量实现。顺序为 load_config()、initialize()，逐帧 detect()，退出 release()。每次调用显式检查返回值和逐对象有效性。

正式配置沿用当前工程格式，例如：

```yaml
task: winch_ice
module_id: 0x0008
camera:
  video_channel: [0]
ground:
  enabled: false
Model_path: ../winch_model.rknn
input_width: 640
input_height: 640
warmup: false
confidence_threshold: 0.45
nms_threshold: 0.5
area_ratio: 0.5
```

相对 Model_path 按 YAML 所在目录解析；示例不是对部署目录的强制约定。切换相机须释放并按新的 camera_channel 初始化实例，阈值更新通过算法线程串行执行 updateConfig()。

首版按开始检测后逐帧检测展示，不增加未经确认的 PLC 工况、十次重试或跨帧报警保持。若后续需要工况门控或报警保持，由宿主实现并与“本帧检测结果”分别展示。

当前库框为原图左上角 x/y 加 width/height；与旧版归一化中心框不同。当前对象按帧内从左到右排序，不是稳定跟踪 ID。UI 可以显示“本帧对象1/2”，不能直接当作固定物理卷扬身份。

## 6. UI 行为与状态

保留实时画面、比例与报警、开始/停止、快照、退出，增加通道选择和清晰的检测状态。画面显示使用同一个完成包内的图像和结果，绘图按原图像素坐标随画面等比例缩放和居中偏移。

| 情况 | UI 应显示 |
| --- | --- |
| 初始化/相机启动中 | 启动中，不显示旧正常结果 |
| API 成功，有有效对象，未达到阈值 | 各对象比例、本帧未报警 |
| 任一有效对象达到阈值 | 采用库返回的 ALERT_WARN 显示本帧报警，并标出相应有效对象 |
| 没有对象 | 未检测到卷扬 / NO RESULT，不显示正常 0% |
| 对象 valid=false 或 ratio=-1 | 该对象结果无效 / N/A，不换算为 -100% 或 0% |
| 部分对象有效 | 各自显示，提示部分结果无效；有效对象仍可触发库的报警 |
| detect 返回错误 | 检测错误和错误码，清除旧检测框/旧比例，不继续显示旧绿色结果 |
| 相机超时或数据过期 | 相机无新帧/结果过期；旧图如保留必须明确标识时间和过期状态 |
| 停止/切换通道 | 停止或切换中，不接收旧会话的迟到结果 |

全局报警读取本帧 alert_level；不要把旧 AlarmState::Alarm=2 与新版 ALERT_WARN=1 的数值混用。置信度属于卷扬模型，不能作为冰雪比例。snow_detections 当前为空，不能拿它代替卷扬框。

阈值文字读取当前配置，去掉旧 UI 固定 on>=12%/off<=6% 文案。报警按现有库逐帧计算，不自行引入回差或锁存。

显示 FPS 统计实际新图像，检测 FPS 统计完成的唯一帧；推理耗时读取 inference_time_ms，同时单独测量 detect 总耗时。采集到显示耗时使用宿主时间测量，不冒充从传感器曝光开始的完整延迟。

快照一次性保存当前同帧的原图、叠加图与结果 JSON，带通道、帧号和时间；不默认逐帧写 PPM。首版不把内部 testing 掩码接口作为宿主的正式依赖，如后续要叠加掩码再明确支持范围。

## 7. 计划新增的源码与构建位置

以下是拟新增位置，目前尚不存在这些实现：

```text
tools/winch_ice_ui/
  CMakeLists.txt             Qt、GStreamer video/app 与配套 evs 动态库
  main.cpp                  QApplication/QML 与参数入口
  ice_controller.h/.cpp     生命周期、配置、状态与最新完成包
  camera_source.h/.cpp      严格通道选择、GStreamer 映射与 RGB 整理
  detector_worker.h/.cpp    串行 Detector 调用与同帧结果封装
  ice_view.h/.cpp           原图与逐对象叠加绘制
  Main.qml / resources.qrc  界面与资源
```

Qt UI 作为显式可选构建目标，不让纯算法库强制依赖 Qt。旧命令行 demo 可保留用于隔离验证，但新增 UI 不依赖旧 stdout 正则。是否共用采集模块以实现时的局部改动评估为准，不把通道行为变更无验证地扩散到人员/高度等旧工具。

优先板端原生构建当前配套库及新宿主。部署放入独立版本目录，包含 bin、lib、config、模型及启动脚本，明确库搜索路径，检查 ldd 实际加载的新核心/冰雪库。模型单独部署，沿用当前工程不自动安装模型的约定。首次验证不覆盖 /home/hytop/l_evs 原 demo 或其他功能库。

设备当前是 X11；启动脚本使用已核对的 DISPLAY/Xauthority/XDG_RUNTIME_DIR。不能仅设置 DISPLAY 就宣称窗口可显示；必须完成真实窗口和图像绘制验证。

## 8. 实施顺序与通过条件

1. **固定基线和库构建。** 保存当前源码、公共头文件与模型哈希，重新编译 AArch64 核心/冰雪库；验证依赖、客户 API 初始化和单张真实图片推理。设备旧板测仅作对照。
2. **独立跑通真实相机。** 严格选择通道 0，从实际 GStreamer caps 得到图像，验证颜色、stride、尺寸、序号和采集停止；与单图入口对照同一 RGB 帧的库结果。
3. **接入新版对象结果。** 让真实相机的每个处理帧得到 objects、有效性、API 错误码和报警；先以 CLI 记录少量证据，证明库确实被调用。
4. **完成 Qt UI。** 以内存帧包显示同帧框与比例，完成状态表中的表现，以及开始/停止、快照、通道切换；UI 操作不等待推理或采集。
5. **联合验收。** 验证无目标、单目标、双目标、对象无效、阈值更新、相机错误、重复启停、退出与恢复；确认不会加载旧库、累积文件或积压帧队列。

自动验证应针对有意义的契约：指定不存在/禁用/重复通道不能误开其他相机，带 padding 的 RGB 整理正确，画面包与结果序号一致，通道切换丢弃旧会话，错误和无结果不会显示成正常 0%。模拟帧用于这些宿主契约，不当作真实 NPU 和相机验收。

算法精度另用真实积雪/无雪、反光、暗部、遮挡等样本评估。现有库是模型定位加 HSV 面积粗估，UI 跑通不能证明精度达标。多于两个保留候选目前会返回 OutOfRange，而不是任意数量都可显示；交叠和质量判定也可能造成对象无效，UI 必须如实呈现。

长时间运行验证时记录采集/检测/显示速率、耗时分布、内存和文件数量，以及异常恢复；在实测前不承诺 25fps 或固定端到端延迟。RKNN 同步调用当前没有硬取消接口，正常停止应等待正在进行的 detect 返回，不强杀线程；如需应对驱动永久阻塞的进程隔离，应另做明确需求和设计。

## 9. 本次完成范围

已完成：源码/API 核对、设备环境和相机参数只读查询、模型哈希对照、旧新接口差异确认以及本方案。

尚未完成：新宿主代码、当前版本板端构建、STREAMON 取流、Qt 图像显示、最新库真实 NPU 联调、性能/精度与异常恢复验收。此处不能表述为功能已实现。

主要源码依据：include/evs/winch_ice.h；src/tasks/winch_ice/winch_ice_api.cpp；src/tasks/winch_ice/winch_ice_plugin.cpp；tools/winch_ice_demo/main.cpp；tools/v4l2_detect/capture.cpp；docs/winch_ice.md；docs/plugin_architecture.md。上述相对路径属于 /home/tronlong/lyp/code/edgevision。
