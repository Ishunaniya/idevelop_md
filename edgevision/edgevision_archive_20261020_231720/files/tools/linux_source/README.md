# Linux宿主取帧接入说明

本目录说明库外宿主的职责，不是当前已实现的完整采集程序。实际摄像头工具位于tools/task_cam、tools/v4l2_detect和tools/winch_ice_demo。

宿主负责打开相机、持有图像内存、读取真实PLC工况、录像、界面及声光/联锁。算法库不访问/dev/video节点。

| 功能 | 宿主交给库的内容 |
|---|---|
| 人员/高度 | 原图Frame及当前MachineContext，调用VisionTask |
| 卷扬冰雪 | 宿主先判断功能开启、工况有效/新鲜、零角度且工作，再调用两参Detector::detect(ImageData,WinchSnowResult) |

冰雪一个实例只绑定一个相机，同一画面可有1或2个安装卷扬；不能把旧“卷扬×2”说明理解为当前实例支持两路相机。表面标定、分辨率、颜色格式和stride需要匹配现场，详情见[冰雪接入说明](../../docs/winch_ice.md)。

图像缓冲须在同步detect返回前有效；原图timestamp与实际帧对应。库当前不消费Frame.dma_fd进行零拷贝，不能只提供fd而省略可读像素。

核心算法库不链接GStreamer、Qt、RGA或mpp；顶层CMake会为相应宿主工具查找GStreamer/RGA等依赖。这与“CMake完全不查找采集库”不同。OpenCV用于当前冰雪面积算法，是真实库依赖。

通用结果的LogOnly与Alarm须分别处理；冰雪逐帧返回，历史报警是否保持由宿主决定。完整示例与部署见[顶层README](../../README.md)和[通用宿主说明](../../docs/host_api.md)。
