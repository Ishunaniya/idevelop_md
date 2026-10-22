# 现场修改对照说明

本文用于现场更换摄像头、模型、标定和算法时快速确认需要修改哪些文件。

## 1. task YAML 是什么

task YAML 是传给以下接口的任务配置文件：

```cpp
VisionTask::initialize("configs/load_near.yaml");
VisionTask::updateConfig("configs/load_near.yaml");
```

当前功能配置：

- `configs/load_near.yaml`：`0x0004`，载荷附近。
- `configs/load_under.yaml`：`0x0005`，载荷下方。
- `configs/boom_under.yaml`：`0x0006`，大臂下方。
- `configs/height_limit.yaml`：`0x0007`，高度限位。
- `configs/winch_ice.yaml`：`0x0008`，卷扬结冰。
- `configs/eval_*.yaml`：模型和板端测试配置，不是新的功能号。

上述VisionTask是通用入口。冰雪产品接入优先使用evs::winch_ice::Detector的两参detect；YAML由load_config读取后传initialize，运行时用DetectConfig更新。宿主负责工况触发，详见[冰雪说明](winch_ice.md)。

文件名和 YAML 中的 `task:` 目前只用于区分文件、日志和调试。真正决定功能的是：

```yaml
module_id: 0x0004
```

## 2. 只改配置，不需要重新编译

### 更换摄像头节点、分辨率或帧率

宿主使用配置采集时，修改运行目录下的 `camera_config.json`，以下为采集项示意：

```json
{
  "device": "/dev/video0",
  "channel": 0,
  "width": 1920,
  "height": 1080,
  "framerate": 25
}
```

这只改变采集参数。人员地面距离需核对对应相机标定及尺寸缩放策略；冰雪还需核对单相机ID、参考宽高及完整工作表面多边形。机位、视场、裁剪或变焦变化时应重新标定，不能只改采集JSON后认定面积区域仍有效。冰雪宽高比校验只发生于实际处理的新帧，无法发现所有同宽高比的视场变化。

### 更换模型或检测阈值

修改对应 task YAML：

```yaml
backend: rknn
Model_path: yolox.rknn
output_format: yolox
person_class_id: 0
confidence_threshold: 0.45
```

更换backend、模型或解码绑定参数须先release再initialize，或新建实例；对已初始化实例重复initialize会保留旧配置。阈值只在对应算法实际使用时生效，冰雪模型模式使用置信度/NMS筛选卷扬框，HSV阈值用于像素分类。原始头辅助层仍有模型常量，换自训模型不一定只改YAML即可。

### 更换某路摄像头的距离算法

修改对应 `id` 的 `mapper`：

```yaml
ground:
  cameras:
    - id: 0
      mapper: homography
```

已有类型：

- `homography`：单应矩阵。
- `crane_pose`：起重机工况算法入口，当前公式尚未实现，结果为 unknown。

### 更换标定矩阵

```yaml
params:
  matrix: /userdata/matrix_0_1280x720.txt
  calibration_width: 1280
  calibration_height: 720
  strict_matrix: true
```

正式现场建议使用 `strict_matrix: true`。文件缺失或内容错误时直接拒绝初始化。

### 修改告警区域

```yaml
zone:
  radius_m: 10
  consecutive_frames: 3
  half_angle_deg: 40
```

允许的热更新字段可由宿主显式调用updateConfig；程序不会自动监控文件变化。普通推理路径目前对输入尺寸、input_format、letterbox、video_channel存在返回0但忽略变更的遗留行为，这些字段应释放重建。冰雪对象清单、表面、参考尺寸、相机、模式和次数上限也需要释放重建；面积/HSV阈值更新只影响未结束对象。

## 3. 需要修改局部代码的情况

### 修改单应性算法

只改：

```text
src/ground/homography_mapper.cpp
```

### 实现起重机动态距离公式

只改：

```text
src/ground/crane_pose_mapper.cpp
```

### 新增鱼眼、双目等距离算法

1. 新增 `src/ground/xxx_mapper.cpp`。
2. 修改 `src/ground/ground_mapper_internal.h`，声明构造函数。
3. 修改 `src/ground/ground_mapper_factory.cpp`，注册 mapper 名字。
4. 修改 `CMakeLists.txt`，加入新源文件。
5. 算法参数写在 YAML 的 `ground.cameras[].params` 中。

不需要修改 `VisionTask`、plugin、`TaskConfig` 或 `config.cpp`。

### 修改 task 参考点

人员功能修改：

```text
src/tasks/person/person_plugin.cpp
PersonPlugin::ground_reference()
```

### 修改统一距离或角度算法

只改：

```text
src/ground/ground_metrics.cpp
```

### 修改人员告警规则

只改：

```text
src/tasks/person/person_plugin.cpp
```

### 新增模型输出格式

修改：

```text
include/evs/detection_decoder.h
src/core/detection_decoder.cpp
```

如果 RKNN 原始输出张量也变化，再修改：

```text
src/backend/rknn/rknn_backend.cpp
```

## 4. 当前需要注意

- `reference: task` 必须配 `distance_mode: radial`。
- `legacy_forward_y` 的角度范围是 `-180°..180°`。
- `radial` 的角度范围是 `0°..360°`。
- 使用地面映射的任务，其 `camera.video_channel` 每一路应在 `ground.cameras` 配置对应 `id`；当前冰雪图像模式要求ground.enabled=false，不使用地面矩阵。
- `EVS_GROUND_MODEL` 只用于调试，会覆盖所有摄像头 mapper，正式运行应取消设置。
- `MachineContext` 暂无绝对回转角。吊钩参考点暂按局部 `+Y` 方向计算。
- `crane_pose` 尚未实现真实公式。
- `Target` 已增加地面坐标字段，升级库后宿主必须使用新头文件重新编译。

## 5. 一般不要修改

现场适配原则上不要修改：

```text
src/vision_task.cpp
include/evs/vision_task.h
include/evs/task_config.h
src/core/config.cpp
```

这些文件负责公共流程和配置解析。摄像头差异放在 mapper，task 差异放在 plugin，模型差异放在 decoder/backend。

## 6. 修改后验证

主机编译测试：

```bash
cmake -S . -B /dev/shm/edgevision_host -DEVS_BACKEND=mock
cmake --build /dev/shm/edgevision_host -j4
ctest --test-dir /dev/shm/edgevision_host --output-on-failure
```

板端交叉编译完成后，先单独运行少量帧：

```bash
./evs_task_cam --task configs/eval_yolox.yaml \
  --config camera_config.json --channel 0 --frames 5
```

确认输出中 `status=0x00000000`，并核对 `ground_ok`、`distance_m`、`angle_deg` 和 `alarm`。
