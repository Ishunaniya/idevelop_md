# evs_ice_ui 逻辑核对

> 历史记录：本文分析设备上的旧版二进制。新宿主的实现见[实现与板端验证](evs_ice_ui_实现与板端验证.md)，后续版本1.1的验证见[核验与优化](evs_ice_ui_核验与优化.md)。

本次已通过 SSH 登录 192.168.2.127，以只读方式检查 `/home/hytop/l_evs/evs_ice_ui`、`evs_ice_cam`、`winch_ice.yaml`、`camera_config.json` 及库依赖。未启动或停止设备上的 UI、相机或检测程序，未修改设备文件及当前工程代码。

指定路径是 AArch64 可执行文件。搜索设备 `/home/hytop` 的相关 C++/头文件/QML 工程文件，尚未找到对应原始源码。以下为从 ELF 函数符号、反汇编、内嵌 QML 和配置交叉确认的静态调用逻辑，不代表已经完成相机实测。

## 实际流程

```text
evs_ice_ui（Qt Quick/QML）
  Component.onCompleted → IceController::start()
  QProcess 启动 evs_ice_cam
    读取 camera_config.json 的 channel 0
    /dev/video0 → GStreamer → 紧凑 NV12
      ├→ evs::Frame → VisionTask::detect → libevs_core.so.1 / libevs_ice.so.1
      │    └→ stdout: frame N WxH ice_ratio=... alarm=... box=...
      │          └→ IceController::onStdout() → 更新比例、报警、尺寸、单个框
      └→ NV12 转 RGB → 编号 P6 PPM /tmp/evs_ice_shot_*.ppm
             └→ 80ms 定时 pollFrame() → 读取完整帧 → overlay()
                   → 保存当前 QImage → frameSeqChanged()
                   → IceView（QQuickPaintedItem）刷新
                   → 等比例缩放并居中绘制
```

UI 本身没有链接 evs 算法库，也不直接打开摄像头。算法库已经由相机子进程动态链接。画面通过本机文件传递，检测结果通过子进程标准输出传递；这条 UI 路径没有使用共享内存传图。

## 启动与停止

内嵌启动命令为：

```sh
exec ./evs_ice_cam --task winch_ice.yaml --config camera_config.json --channel 0 --frames 999999 --save /tmp/evs_ice_shot.ppm
```

可执行文件与配置均为相对路径。UI 的 start() 先清理旧相机子进程与旧临时帧，再启动 QProcess，监听 stdout、stderr 和退出事件，启动刷新与清理定时器。stop() 停止定时器，终止子进程，必要时 kill，并执行进程及临时帧清理。上述是程序自身的逻辑，本次未执行这些动作。

## 采集与算法

当前相机配置中 channel 0 为 `/dev/video0`，请求 1920×1080、25fps。实际帧尺寸应以 GStreamer 协商结果为准。

GStreamer 字符串包含 `v4l2src ... ! videoconvert ! video/x-raw,format=I420 ! videoconvert ! video/x-raw,format=NV12 ! appsink`。drop 模式使用 `drop=true sync=false max-buffers=1`。

evs_ice_cam 使用旧版 `evs::VisionTask::initialize/detect/release`。其 main() 调用顺序为 grab_for()、构造 NV12 Frame、detect()、打印结果并 fflush()、转换 RGB 并写 PPM。RGB 转换在检测之后，用于预览文件；不能把当前工程新版 Detector 所需的 RGB 输入要求反推为旧 demo 的输入方式。

结果日志只有一个全局 ice_ratio/alarm。存在目标时只取 `targets.front()` 输出一个框，空目标输出 `box=none`。

## 相机通道配置与切换

相机通道由三处共同确定：`camera_config.json` 定义逻辑通道对应的物理节点，启动参数 `--channel N` 选择一路，任务 YAML 的 `camera.video_channel` 声明算法允许处理的逻辑通道。

### 设备当前的通道映射

设备文件为 `/home/hytop/l_evs/camera_config.json`，当前映射如下：

| 逻辑通道 channel | 相机设备 device | 配置宽高 | 配置帧率 |
| --- | --- | --- | --- |
| 0 | /dev/video0 | 1920×1080 | 25fps |
| 1 | /dev/video3 | 1920×1080 | 25fps |
| 2 | /dev/video13 | 1920×1080 | 25fps |
| 3 | /dev/video14 | 1920×1080 | 25fps |

`channel` 是业务使用的相机编号，不等同于 `/dev/videoN` 的 N，也不是必须依赖 JSON 数组位置的索引。例如 `--channel 1` 对应的是 `/dev/video3`。

JSON 顶层必须是数组。单路条目示例：

```json
[
    {
        "device": "/dev/video0",
        "channel": 0,
        "enable": true,
        "width": 1920,
        "height": 1080,
        "framerate": 25
    }
]
```

这里的宽高和帧率是配置值，不能仅凭 JSON 宣称相机实际输出已被强制设置；实际尺寸与格式须以 GStreamer 协商结果核对。

### 通道如何传到算法

当前 UI 内嵌命令固定传入 `--config camera_config.json --channel 0`，后端的对应链路为：

```text
--channel 0
  → 在 JSON 中按 channel 字段查找通道 0
  → 读取该条目的 device、width、height、framerate
  → 打开 /dev/video0
  → 构造 Frame，camera_id=0
  → VisionTask::detect()
  → 任务 YAML 的 video_channel 应包含 0
```

当前设备 `/home/hytop/l_evs/winch_ice.yaml` 中的配置为：

```yaml
camera:
  video_channel: [0]
```

JSON 控制取哪一个设备的图，YAML 控制算法处理哪个逻辑相机编号，二者职责不同。只修改 YAML 不会让采集端自动改开另一台相机。

### 换用通道 1 的方法

若要把这套 demo 改为通道 1，应完成以下配套修改：

1. 确认 JSON 中存在 `channel: 1`，其 `device` 为要使用的节点，当前配置为 `/dev/video3`。
2. 任务 YAML 改为 `camera.video_channel: [1]`。
3. 将 UI 控制器启动相机后端时的 `--channel 0` 改为 `--channel 1`，重新编译部署 UI。当前程序未提供通道选择控件，不能只改 YAML 或假定给 UI 传一个同名参数就能生效。

相机后端的独立命令示例如下；本次仅记录，没有执行：

```sh
cd /home/hytop/l_evs
./evs_ice_cam --task winch_ice.yaml \
  --config camera_config.json --channel 1 \
  --frames 999999 --save /tmp/evs_ice_shot.ppm
```

该命令只展示后端参数；独立运行后端不等于已经启动 UI。

### 当前实现的选择规则和限制

已对照设备 `cap::select_entry()` 的调用信息及本地 `tools/v4l2_detect/capture.cpp`：

- 指定非负通道号时，先按 `channel` 字段查找，匹配成功即返回该项；此分支不检查 `enable`。
- 未匹配到指定通道或未指定有效通道提示时，会回退到第一个 `enable=true` 的条目；仍未找到则返回数组第一项，数组为空才返回空。
- 因此旧版选择函数本身没有严格保证“指定通道不存在就报错”，也不能仅靠 `enable=false` 保证显式指定的通道不会被打开。后续接入应明确是否改为严格检查，避免图像来源与相机编号不一致。

当前 `evs_ice_ui` 只有一个相机子进程和一个画面区域，固定选择通道 0。配置文件里存在四路、四路均 `enable=true`，不代表它会同时采集、检测或显示四路。实现多路需要扩展采集调度、算法实例和结果绑定、UI 显示及帧传递机制；旧版统一的临时文件名和进程清理方式也需要调整。

JSON 中的 `pos_x`、`pos_y`、`display_width`、`display_height`、`previewEnabled` 不在这套采集配置解析器读取的字段中，不决定该冰雪 UI 的布局。当前 IceView 使用 QML 的窗口布局，并保持图像宽高比居中绘制。

以上描述对应设备旧 demo 的 VisionTask 接口。接入当前新版 `Detector` 时，还要把每个实例的 `camera_channel` 与宿主所选相机对应；不能把旧版的多通道数组当作一个新版实例同时处理多路的配置。

## UI 接收与显示

onStdout() 用正则解析：

```text
frame\s+\d+\s+(\d+)x(\d+)\s+ice_ratio=([\d.]+)\s+alarm=(\w+)\s+box=(none|[\d.]+,[\d.]+,[\d.]+,[\d.]+)
```

帧号被匹配但没有作为捕获字段保存。比例、alarm、尺寸和框写入控制器属性，通知 QML 刷新。

pollFrame() 每 80ms 调用 shotFilesByNewest()，最多尝试最新三个候选文件，选择能成功解析的 P6 PPM。parsePPM() 校验 P6、尺寸、最大值 255、RGB 数据完整长度；数据不足会失败，并尝试其他候选。

overlay() 用 QPainter 在图像上画框和文字。随后在互斥保护下更新当前 QImage，发出 frameSeqChanged()。IceView::paint() 取得当前图像，保持宽高比并居中显示。显示 FPS 根据 UI 刷新时间计算，不能直接视为 NPU 推理 FPS。

另一个 1000ms 定时器运行 cleanTmp()，保留最新四个编号 PPM，删除更旧文件。

内嵌 QML 显示覆盖率、FPS、分辨率；按 running 和 alarm 显示 STOPPED、OK、SNOW。按钮为开始/停止、快照、退出，并有错误提示区域。

## 现有配置及后续接入差异

设备 `/home/hytop/l_evs/winch_ice.yaml` 当前包含：

```yaml
backend: mock
Model_path: models/winch_ice.rknn
output_format: yolov8
area_ratio: 0.3
exit_on_alarm: false
```

因此，已确认这套旧 demo 链接并调用冰雪 so，但不能据此认定它使用了当前的 winch_model.rknn 和真实 RKNN 卷扬定位。现有 UI 还包含 `on>=12% off<=6%` 的固定文案，不能当作当前配置或实际报警阈值。

当前工作区提供的新接口是 `<evs/winch_ice.h>` 的 `evs::winch_ice::Detector`，输入紧凑 RGB/灰度，输出逐对象 `objects[].box/ice_ratio/valid` 和本帧全局报警。旧 demo 的 VisionTask、单框、全局比例日志不能直接当作新版接口；即便 so 尾号相同，后续也应使用配套头文件重新编译宿主。

后续实现应保留已经核清的用户流程，并适配新版库的输入和逐对象结果。原 demo 的图像文件与 stdout 独立更新，未按同一帧号配对；新实现需明确图像和结果的对应关系。当前仅完成第一步逻辑核对，未实施接入或更换设备库。

## 覆盖范围与尚未完成的核对

本文已覆盖启动与停止、相机采集、算法调用入口、图像与结果传递、UI 刷新与叠加、临时文件清理、配置及新版接口差异，可用于理解现有 demo 和确定后续接入位置。

本文尚不能作为完整源码审查或实机验收结论。以下内容仍需补充：

- 原始 C++ 源码与构建工程，及其与设备二进制的对应关系。
- 旧版冰雪库内部的具体计算过程、ROI 策略及报警状态机；当前主要核对了宿主调用入口。
- stderr、进程异常退出、采集超时等错误分支的逐项核对与运行验证。
- 快照的完整写入行为、相机断开后的恢复行为及退出时的资源释放实测。
- 实际相机协商格式、画面和结果同步情况、运行帧率、延迟及长时间运行表现。
- 使用当前新版算法库和真实 RKNN 模型后的完整链路验证。

## 本机核对材料

下列核对材料仍保存在本机临时目录 `/tmp/edgevision_ice_ui_review/`，未随本文移动；临时目录可能被系统清理。

- `evs_ice_ui`：设备程序副本；SHA256 `0d07bb3086fb3e74813d297978d063d222bd9ae33e0feb62e2a3f4a6d0f0ca96`
- `evs_ice_cam`：设备程序副本；SHA256 `5a9f458003e12947ed684bfba318b2dba5901014a160e61abffc81d0aa7f9b5a`
- `embedded_ui.qml`：从 UI 可执行文件直接提取的 QML。
- `ui.asm`：设备 objdump 输出的 UI 调用信息。
- `camera_main.asm`：设备 objdump 输出的相机后端 main() 调用信息。
