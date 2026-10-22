# 任务插件

公共执行链位于libevs_core，业务分别位于libevs_person、libevs_height_limit、libevs_ice。当前动态库ABI统一为3；无版本链接名保留，宿主与插件须使用匹配头文件重编。

## 当前功能范围

| 模块 | 入口及实现 | 尚未完成的范围 |
|---|---|---|
| 0x0004/0x0005/0x0006 人员 | VisionTask推理、地面映射、PersonPlugin策略 | 真实PLC及现场标定/模型效果按产品验收；绝对回转角尚未接入 |
| 0x0007 高度限位 | 模型检开关，OpenCV安装姿态接口及状态骨架 | 完整安装姿态几何规则 |
| 0x0008 卷扬冰雪 | 客户两参Detector、RKNN定位＋OpenCV整框面积粗估、单相机逐帧1～2对象 | 专用模型交付、灰度、snow_detections及板端现场验收 |

冰雪不是恒为0的占位实现。有效单对象可取得0..1比例；未知或多对象的通用Result.ice_ratio为-1，完整结果通过Detector或lastWinchSnowResult读取。宿主决定角度为0且工作时是否调用；冰雪不使用MachineContext作触发判断。

## 人员策略

三个模块共用PersonPlugin和person_decide；相同backend/Model_path可通过ModelRegistry共享模型资源。make_person_config按功能号生成策略：

| 模块 | 当前参考及区域 | 工况无效时 |
|---|---|---|
| load_near 0x0004 | 配置的吊钩参考/旧原点；默认10m圆 | 映射后区内有人且on_missing_context=alarm时立即Alarm，否则按策略记录 |
| load_under 0x0005 | 默认10m圆，可排除遮挡扇区 | LogOnly |
| boom_under 0x0006 | 大臂参考扇形，半径取hoist_radius且至少50m | LogOnly |

工况有效时，is_working非零、hoist_moving或telescope_moving任一成立视为作业；work_motion_epsilon当前未参与判定。静止时区内有人只记录；作业时区内有效地面目标按连续帧累积，达到阈值才Alarm。无法得到地面位置的目标不能当作区内有效目标。宿主不得把LogOnly当成声光报警。

地面位置由公共mapper与ground_metrics适配，不再是旧阶段固定0。无可信映射时distance_m为-1、ground_ok为false；绝对回转角缺口及crane_pose占位需按[通用API](../../docs/api.md)处理。

## 推理与解码边界

推理路径在配置后端、模型及decoder后调用功能插件；插件可以提供preflight。冰雪model路径推理后用原图计算面积，固定区域/人工框测试跳过推理链。

- flat6：每目标为[class_id, confidence, center_x, center_y, width, height]。
- yolov8/yolo11/yolox上层decoder：接收[N,4+C]的中心框及类别分数行矩阵；person_class_id控制保留类别，decoder_classes控制行宽。
- RKNN原始YOLO头先由backend/yolo_decode.h按张量形状解码，再交上层decoder。当前辅助层有80类、640输入及锚框常量，不能只改decoder_classes就认定任意自训原始头可用。

未知output_format在VisionTask初始化时返回模块化ModelInvalidFormat；热更新禁止更改绑定模型的backend、Model_path、module_id及decoder参数。换模型需release后initialize，重复initialize保留旧配置。普通推理部分其他结构字段存在成功返回但忽略的遗留行为，见[顶层README](../../README.md)。

## 冰雪开发与验证

面积算法和公开适配位于本目录。正式入口加载卷扬模型，每个框独立统计冰雪像素
除以该框面积，默认>=0.5则本帧报警。没有首次结果锁定、重试上限和报警保持。
人工框仅为测试入口，不是客户detect新增参数，也不是模型失败时的回退。
真实雪识别精度不能由合成测试证明。

详见[冰雪说明](../../docs/winch_ice.md)、[当前后续工作](winch_ice/待办.md)和[验收记录](../../docs/winch_ice_acceptance.md)。

## 新增功能插件

1. 在src/tasks新增IFunctionPlugin实现及构造函数，用FunctionPluginRegistration登记功能号。
2. 在CMakeLists.txt通过evs_add_plugin生成独立插件，不把业务实现直接放入公共EVS_SRC。
3. 动态宿主链接公共库与新插件，并确保登记对象被加载；静态宿主保留归档登记对象。
4. 涉及公共结构/虚表变化时评估ABI，保持头文件、核心、相关插件和宿主一致；不能仅替换一个旧版插件。
