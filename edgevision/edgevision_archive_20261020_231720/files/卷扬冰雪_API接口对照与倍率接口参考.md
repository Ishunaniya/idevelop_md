> 历史接口对照：下文包含旧的一次检查会话设计。当前接口已改为每次detect处理当前帧，整框计算比例；以[当前冰雪说明](docs/winch_ice.md)及公开头文件为准。

# 卷扬冰雪 API 接口对照与倍率接口参考

整理日期：2026-10-20。

本文完整整理本会话上一轮分析的第二至第六点：四个公开 API、detect 入参、detect 出参、配置入参，以及客户倍率识别接口对当前源码的参考建议。初版整理仅新增文档。本次修订同步记录已授权的日志异常隔离与模型配置解析修复；真实模型、灰度及板端验收等缺口仍须分别完成。配置结构统一的分析见第七节，本次没有直接重构 TaskConfig 的公开布局。

核验依据：

- 客户需求：[中控详细设计说明书0921.docx](/home/tronlong/lyp/perCode/idevelop_md/demand/中控详细设计说明书0921.docx)，卷扬冰雪章节。
- 当前公共头文件：[include/evs/winch_ice.h](include/evs/winch_ice.h)。
- 当前接口实现：[src/tasks/winch_ice/winch_ice_api.cpp](src/tasks/winch_ice/winch_ice_api.cpp)。
- 当前会话和报警实现：[src/tasks/winch_ice/winch_ice_plugin.cpp](src/tasks/winch_ice/winch_ice_plugin.cpp)。
- 用户粘贴的倍率识别公共头文件 `hmpd.hpp`。只取得了头文件，未核验倍率库实现及运行结果。
- 上轮完整核验及复现证据：[API 对照核验报告](build_ice_delivery/review_20261020_api_contract/API对照核验.md)、[实际复现输出和调用栈](build_ice_delivery/review_20261020_api_contract/contract_probe.log)。

DOCX 定位使用 `word/document.xml` 中 `w:p` 的零起点编号，不是 Word 页码。四个方法原型位于 P3737、P3809～3810、P3913、P3925；输入输出字段位于 P3769～3909；生命周期及展示逻辑位于 P3935～3951；参考 YAML 位于 P3963～3996。

阅读时必须区分三个结论：**函数原型是否对齐、字段及运行行为是否完整实现、二进制是否兼容**。四个方法原型对齐，不等于后两项也全部成立。

## 二、四个公开 API 是否符合客户文档

### 2.1 当前完整原型

当前类型和检测器位于 `evs::winch_ice` 命名空间，`ErrorCode` 定义为 `uint32_t`。

```cpp
ErrorCode initialize(const WinchSnowConfig& config);
ErrorCode detect(const ImageData& image, WinchSnowResult& result);
ErrorCode updateConfig(const DetectConfig& config);
void release();
```

| API | 入参 | 出参 | 返回值 | 与客户冰雪 DOCX 的对应 |
|---|---|---|---|---|
| `initialize` | `const WinchSnowConfig&` | 无 | `ErrorCode`，0 成功、非 0 异常 | 原型一致；当前无模型版本不实现文档描述的模型加载、推理资源分配及预热 |
| `detect` | `const ImageData&` | `WinchSnowResult&` | `ErrorCode`，0 调用成功、非 0 异常 | 原型一致；没有工况参数；图像能力、结果内容仍存在下文列出的差异 |
| `updateConfig` | `const DetectConfig&` | 无 | `ErrorCode` | 原型一致；常规非法阈值拒绝；已隔离日志格式化、缓存和宿主回调异常，并增加更新故障注入回归 |
| `release` | 无 | 无 | `void` | 原型一致；释放内部资源，不销毁外部持有的 Detector 对象 |

`release` 的接口原型明确为 `void`，不能为了套用通用错误码描述擅自改成返回 `ErrorCode`。客户后面的通用日志描述把 release 也列入 ErrorCode 记录，与其原型有文字不一致；这里以明确的方法原型为准。

### 2.2 工况职责已经确定

`boom_angle`、`is_working`、`WorkingCondition` 不在冰雪 `detect` 入参中，也不在 `ImageData` 中。公开配置里也没有 `context_max_age_ms`。

当前约定：宿主根据功能开关、真实工况有效性、工况时效、大臂角度为 0、设备正在工作等业务条件，决定是否调用 `detect(image, result)`。库不读取 PLC，也不再根据工况判断是否检测。

通用 `VisionTask` 仍带 `MachineContext`，因为其他功能需要；冰雪插件忽略这份工况。不能因为通用接口中仍有它，就认为冰雪专用公开 API 仍要求提供工况。

demo 中的宿主判断见 [tools/ice_replay/host_gate.h](tools/ice_replay/host_gate.h)。其中的 500ms 是示范宿主策略，不是冰雪库配置或客户已经签收的时延指标。`api_called`、`host_reason` 是 demo 的宿主诊断字段，不属于 `WinchSnowResult`。

### 2.3 生命周期及重复初始化

- 构造后处于未初始化状态。
- 初始化成功后可以调用 `detect`、`updateConfig`。
- 当前重复调用 `initialize` 会直接返回 0，保留原配置和原检查会话，不重新加载或重新开始。
- `release` 可以重复调用；释放后可以再次 `initialize`。
- 当前析构会通过内部对象清理资源。
- 一个实例的方法应串行调用，不支持对同一实例并发修改配置和检测。

倍率参考注释写的是“重复初始化先 release 再重新初始化”，与当前冰雪的幂等行为不同。冰雪 DOCX 要求重复调用不泄漏，没有明确规定重复调用必须重新加载，不能只依据倍率接口就把当前行为判为错误。

更改安装对象、相机、表面、参考尺寸、输入模式或尝试上限，当前应 `release` 后重新 `initialize`；热阈值调整使用 `updateConfig`。已经结束的对象不会因更新阈值被重新测量。

### 2.4 当前能确认与不能确认的范围

可以确认四个核心方法的方法名、参数方向、类型名和返回形式已对齐冰雪 DOCX；不能确认已经满足全部模型与影子系统能力。

DOCX 没有提供可逐字对比的完整供应商公共头文件和二进制布局，不能承诺与一个未知旧头文件二进制兼容。当前工程统一使用 ABI 3，头文件、核心库、相关插件及宿主必须匹配重编，不能混用旧 ABI 1/2。

## 三、detect 入参逐项对照

### 3.1 客户规定的五个图像字段

| 字段 | 客户 DOCX 要求 | 当前类型及行为 | 结论 |
|---|---|---|---|
| `data` | `const uint8_t*`，只读图像数据指针 | 相同；由宿主持有，库同步调用期间借用，不保留输入指针 | 一致 |
| `width` | `uint32_t`，大于 0 | 同类型，当前要求至少 2，并检查内部整数转换上限 | 类型一致，接受范围更窄 |
| `height` | `uint32_t`，大于 0 | 同类型，当前要求至少 2，并检查内部整数转换上限 | 类型一致，接受范围更窄 |
| `channels` | `uint32_t`；3=RGB24，1=灰度 | 同类型；RGB/BGR 使用 3，显式 NV12 使用 1；灰度尚不支持 | 未完整符合 |
| `timestamp_ms` | `uint32_t`，毫秒 | 相同；当前另有会话内图像时间戳向前的去重约定 | 类型、单位一致，额外行为需明确 |

当前整数校验还要求 `width <= INT_MAX/3`、`height <= INT_MAX/2`，以及显式 stride 不超过 `INT_MAX`；这些是实现的边界检查，不表示实际支持接近此上限的图像或已经验收相应内存规模。

### 3.2 当前新增的三个图像字段

| 扩展字段 | 类型 | 默认值 | 用途 |
|---|---|---|---|
| `format` | `ImageFormat` | `ImageFormat::RGB` | 明确 RGB、BGR、NV12、Gray 的解释方式；Gray 当前明确不支持 |
| `stride` | `uint32_t` | 0 | 每行字节数；0 表示紧凑布局 |
| `size_bytes` | `size_t` | 0 | 非零时验证缓冲区最小长度；0 表示调用者保证实际分配足够 |

扩展字段具有工程用途，但改变了结构布局。字段追加不能自动保证与其他旧头文件的二进制兼容。

RGB/BGR 要求 `channels=3`，有效行距至少为 `width*3`。NV12 必须显式指定格式，要求 `channels=1`，宽、高和行距为偶数，UV 平面紧随 Y 平面，两平面使用同一行距。宿主必须保证数据在同步调用期间 CPU 可读且有效。

### 3.3 灰度输入的实际缺口

上轮契约探针已经验证：

| 调用方式 | 当前返回值 | 说明 |
|---|---|---|
| 只按客户图像字段填写，`channels=1`，不改默认 `format=RGB` | `0x00083002` | 按 RGB 解释时通道数不符 |
| 明确填写 `format=Gray`、`channels=1` | `0x00081009` | 当前显式拒绝灰度输入 |

所以不能把“NV12 也使用 channels=1”解释成“已经支持客户所说的灰度图”。也不能仅复制灰度成三个相同通道，就宣称已获得与彩色 HSV 等效的冰雪判别能力。需要制定并验证灰度方案，或者与集成方明确实际输入能力。

### 3.4 时间戳的额外约定

首次传入时间戳 0 是合法的。对于尚未完成的检查，后续新图应使用前进的时间戳；重复或倒序会被去重，不增加观察尝试。短时跨 `uint32_t` 回绕使用无符号差值判断。

如果宿主一直不填写时间戳、每帧都保持 0，首次未知观察之后可能一直被当成重复帧。客户 DOCX 只规定毫秒字段，没有明确规定严格前进，所以必须将该供应商约定写入公共接口说明。

库不再拿图像时间戳与当前墙钟、工况时间比较。首次离线图像可以使用任意时间原点，之后保持顺序。相机采集是否积压、真实曝光时间与工况是否同步，仍由宿主处理。

## 四、detect 出参逐项对照

### 4.1 客户规定的五个结果字段

| 字段 | 客户要求 | 当前实现 | 对照结论 |
|---|---|---|---|
| `result_code` | `ResultCode`，对应 `int32_t`；0 无识别结果、1 有效结果 | `enum class ResultCode : int32_t`，取值 0/1 | 数值和底层宽度一致；C++ 调用形式不同于倍率的整数别名 |
| `alert_level` | `AlertLevel`，对应 `int32_t`；0 无报警、1 报警 | `enum class AlertLevel : int32_t`，取值 0/1 | 一致 |
| `snow_detections` | `std::vector<Detection>`；模型检出的、未按置信度阈值过滤的所有冰雪目标，供影子系统使用 | 同类型，但当前无模型版本始终为空 | 字段存在，所要求的模型及影子输出能力尚未完成 |
| `inference_time_ms` | `uint32_t`，当前帧模型推理毫秒 | 同类型；当前无模型版本为 0 | 类型和单位一致，不代表已经执行模型推理 |
| `camera_channel` | `uint32_t`，当前输入图像摄像头通道 | 同类型，由配置绑定普通相机 ID | 一致；同一摄像头内两个卷扬仍使用一个相机 ID |

冰雪对外报警值为 1。不能把通用内部 `AlarmState::Alarm=2`，或者倍率参考的 `ALERT_DANGER=2`，直接当成冰雪对外报警值。

`inference_time_ms` 不能被面积处理耗时冒充；当前面积处理耗时由扩展的 `processing_time_ms` 表示。HSV 候选掩码、人工卷扬框都不是客户定义的模型冰雪检测目标，不能伪造类别和置信度填进 `snow_detections`。

### 4.2 当前新增的顶层结果字段

| 扩展字段 | 类型 | 用途 |
|---|---|---|
| `objects` | `std::vector<WinchObjectResult>` | 每个预期卷扬的身份、比例、状态及有效性 |
| `check_state` | `CheckState` | 当前检查状态 |
| `reason` | `Reason` | 图像去重等调用处理原因 |
| `check_complete` | `bool` | 所有预期对象是否都已结束 |
| `processed` | `bool` | 本次是否实际处理新图 |
| `processing_time_ms` | `double` | 本次面积处理耗时，毫秒 |
| `status` | `uint32_t` | 本次调用错误，与 detect 返回值同步 |

这些扩展用于双对象和一次检查的集成，需要随公共头文件交接，不能把它们写成 DOCX 已逐字段规定的内容。

### 4.3 每个卷扬的完整结果字段

| `WinchObjectResult` 字段 | 类型 | 含义 |
|---|---|---|
| `winch_id` | `uint32_t` | 对应安装配置中的对象身份 |
| `state` | `ObjectState` | Pending、Normal、Alarm、Failed |
| `reason` | `Reason` | 缺框、不可观测等原因 |
| `status` | `uint32_t` | 对象级错误，与顶层调用错误分开 |
| `attempts` | `uint32_t` | 已消费的对象观察尝试数 |
| `measurement_timestamp_ms` | `uint32_t` | 最近一次对象观察尝试对应的图像时间 |
| `ice_ratio` | `float` | 有效候选覆盖率；无效时为 -1 |
| `surface_pixels` | `uint64_t` | 完整可见工作表面像素数 |
| `snow_pixels` | `uint64_t` | 当前规则选出的冰雪候选像素数 |
| `valid` | `bool` | 比例是否为有效测量 |

`valid=false`、比例 -1 不能按无雪 0% 处理。当前 `snow_pixels` 是 HSV 候选数量，不是经过独立像素真值验收的真实冰雪面积。

`CheckState` 包括 Ready、Checking、Completed、CompletedWithFailures。`Reason` 包括 None、DuplicateFrame、ObjectNotFound、Unobservable、InvalidSurface、UnsupportedInput；枚举中存在某个值不代表所有入口都会实际设置该值，应按具体返回路径解读。

### 4.4 Detection 的全部字段及坐标歧义

| 字段 | 客户类型 | 当前类型和定义 |
|---|---|---|
| `class_id` | `uint32_t` | 相同，模型类别 ID |
| `confidence` | `float`，0～1 | 相同，模型类别置信度 |
| `x` | `uint32_t` | 相同，原图左上角 x |
| `y` | `uint32_t` | 相同，原图左上角 y |
| `width` | `uint32_t` | 当前定义为像素宽度 |
| `height` | `uint32_t` | 当前定义为像素高度 |

客户原文的 `width`/`height` 字段名及范围像“宽、高”，但说明写“右下点 x/y 坐标”，存在歧义。当前按宽高定义，不能宣称与客户原文字面完全一致。真实模型未接入、结果列表为空，也就尚未完成真实模型框坐标输出的验收。

交接时需要将宽高或右下坐标的含义统一，并约定图像坐标还原、边界及矩形区间规则；当前不要通过修改名字来掩盖该歧义。

### 4.5 返回码、有效结果、报警与全部完成

这四种信息应分别读取：

1. `detect` 返回值及顶层 `status`：本次调用是否成功；0 不等于“无冰雪”，非 0 不代表可以清除历史报警。
2. `result_code`：是否已有可用于业务判断的有效结果。
3. `alert_level`：当前会话是否已有卷扬成立报警；任一对象报警则全局报警并保持。
4. `check_complete`、对象列表：是否所有预期卷扬都已有最终状态。

| 当前对象情况 | 当前结果码与报警 | 全部完成 |
|---|---|---|
| 两对象都有效正常 | MATCH、NONE | 是 |
| A 报警，B 仍缺框 | MATCH、WARN | 否 |
| A 正常，B 仍缺框 | NO_RESULT、NONE | 否 |
| A 报警，B 尝试耗尽失败 | MATCH、WARN | 是，但为 CompletedWithFailures |
| A 正常，B 尝试耗尽失败 | NO_RESULT、NONE | 是，但不能宣称全部正常 |

一个卷扬报警、另一个未知时，若宿主看到 MATCH 就停止，第二个对象可能没有最终结果。若要求主副卷扬都完成，应等 `check_complete`。客户“返回结果后不再调用”与本次多对象扩展必须明确对应停止条件。

每个卷扬第一次取得有效比例后会保持最终结果；后续新增雪不会在同一次会话重算该对象。需要复检应创建新会话，而不是只热改阈值。缺框或不可观测对象默认最多尝试 10 次，一个新图最多给每个未结束对象增加一次。

在持续内存不足、对象列表无法复制时，当前错误返回可以只保留标量报警与完成信息，`objects` 可能为空。调用方必须根据非零 status 处理详情缺失，不能用空列表推断没有卷扬或清除已有报警。

## 五、配置入参是否一致

### 5.1 客户规定的字段

| 所属结构 | 字段 | 客户类型或要求 | 当前情况 |
|---|---|---|---|
| `WinchSnowConfig` | `model_config` | 自定义 `ModelConfig` | 保留 |
| `WinchSnowConfig` | `detect_config` | 自定义 `DetectConfig` | 保留 |
| `ModelConfig` | `Model_path` | `std::string`，有效模型路径 | 名称和类型保留；当前无模型版本不执行加载 |
| `ModelConfig` | `input_width` | `int32_t`，初始 0 | 名称、类型和初始值保留 |
| `ModelConfig` | `input_height` | `int32_t`，初始 0 | 名称、类型和初始值保留 |
| `ModelConfig` | `warmup` | `bool` | 保留，当前默认 false；客户未规定必须默认 true |
| `DetectConfig` | `confidence_threshold` | `float`，0～1 | 保留并校验；当前默认 0.45 |
| `DetectConfig` | `nms_threshold` | `float`，0～1 | 保留并校验；当前默认 0.5 |

### 5.2 当前完整扩展配置

| 所属结构 | 扩展字段 | 类型 | 默认值或用途 |
|---|---|---|---|
| `WinchSnowConfig` | `camera_channel` | `uint32_t` | 默认 0；绑定单摄像头，当前有效 ID 为 0～254 |
| `WinchSnowConfig` | `reference_width` | `uint32_t` | 默认 0；必须提供有效标定参考宽度 |
| `WinchSnowConfig` | `reference_height` | `uint32_t` | 默认 0；必须提供有效标定参考高度 |
| `WinchSnowConfig` | `max_attempts` | `uint32_t` | 默认 10；未知观察尝试上限 |
| `WinchSnowConfig` | `mode` | `InputMode` | 默认 FixedSurface；另有 ManualBoxes 和暂不支持的 Model |
| `WinchSnowConfig` | `objects` | `std::vector<WinchObjectConfig>` | 当前要求 1 或 2 个实际安装对象 |
| `DetectConfig` | `area_ratio` | `float` | 默认 0.5；比例大于等于阈值成立报警 |
| `DetectConfig` | `saturation_max` | `int32_t` | 默认 80；HSV 饱和度上限，范围 0～255 |
| `DetectConfig` | `value_min` | `int32_t` | 默认 180；HSV 亮度下限，范围 0～255 |
| `WinchObjectConfig` | `winch_id` | `uint32_t` | 对象稳定身份，不按每帧框顺序区分 |
| `WinchObjectConfig` | `name` | `std::string` | 对象名称 |
| `WinchObjectConfig` | `surface_polygon` | `std::vector<float>` | 完整工作表面多边形 |

固定表面模式的多边形相对原图归一化，人工框模式则是框内归一化表面模板。不能直接混用两种坐标。工作表面包括被雪覆盖的部分，应排除背景、法兰和支架；矩形框只负责定位，不能直接把框面积作为冰雪面积。

客户在配置结构中允许供应商补充“其他字段”，所以不能把这些扩展一概判为超范围；应明确默认值、标定要求、热更新范围和交接版本。

### 5.3 字段存在与功能实现不是一回事

当前非模型版本使用 HSV 候选规则及面积比值：S 不超过 `saturation_max`，V 不低于 `value_min`，随后结合表面和形态学统计比例。

`confidence_threshold`、`nms_threshold` 虽然保留并校验，但当前不参与该像素分类，也没有执行真实模型的置信度过滤和 NMS。`Model_path`、模型尺寸及 `warmup` 保留，不代表已经加载或预热模型；Model 模式会明确拒绝，不通过 mock 输出伪装为真实模型结果。

客户参考配置中的 `canary.canary_enabled` 是产品灰度测试/发布开关，与 `channels=1` 的灰度图像是两回事，不能把是否支持这个业务开关当作是否支持灰度图的证据。宿主仍负责产品功能开关、UI、报文和影子系统集成。

`updateConfig` 可更新检测阈值，只影响尚未完成的对象。当前常规非法值会拒绝更新。上一轮复现的日志异常导致“返回失败而阈值已改变”已通过日志写入边界异常隔离修复；未宣称仅凭此测试即可证明所有未来配置扩展都具备完整事务语义。

YAML 文件字段能否正确转换成上述结构，也是独立的核验项。本次已支持块式 `model` 段中的 `path/input_width/input_height/warmup`，兼容旧式 `Model_path` 与 `model: 路径`。不把空容器头当路径，不读取其他段的 `path`。冰雪同文件别名值一致时接受、冲突时拒绝；缺省字段保留调用者值，失败不发布半成品配置。其他模块保留既有平铺配置的宽松校验与最后值优先；新增块式模型配置也可读取。仍只支持文档说明的 YAML 子集，不宣称支持行内映射等完整 YAML 语法。

## 六、倍率头文件对当前源码的参考价值

### 6.1 当前已经具备的共同设计

- 独立功能命名空间，冰雪为 `evs::winch_ice`。
- 主要用户入口为一个公共头文件 `evs/winch_ice.h`。
- PIMPL 隐藏实现细节。
- 禁止拷贝，配置和结果各自使用结构体。
- initialize、detect、updateConfig、release 四个核心方法。
- `uint32_t` 错误码，以及模块号加低位错误原因的编码体系。
- 默认初始化的结果字段，避免未赋值数据。

当前 Detector 虽未像倍率头那样逐项显式删除移动构造和移动赋值，但因为已有用户声明的析构及删除的拷贝操作，没有自动生成可用移动操作，不能误判当前类可移动。

### 6.2 建议借鉴并补齐的内容

| 倍率头文件中的做法 | 对冰雪的具体建议 | 性质 |
|---|---|---|
| 每个 API 有明确注释 | 用 Doxygen 写清输入输出、所有权、格式、有效期、串行调用、返回值和失败副作用 | 接口可读性与可交接性改进 |
| 生命周期说明清楚 | 明确当前重复初始化保留原会话，重建配置应先释放；不能让宿主误认为重新 initialize 就重新检查 | 当前行为需要明确说明 |
| 公共命名错误常量 | 提供 `MODULE_ID=0x0008`、`SUCCESS`、实际使用的常用 `ERROR_*` 和对象级错误常量 | 便利性建议，当前不是已经实现的全部功能 |
| 错误描述工具 | 可补充 `getErrorMessage()`、对象错误说明、结果与报警字符串转换 | 板端诊断便利性建议 |
| 版本查询 | 可补充 `getVersion()`；区分库版本、ABI 版本和配置版本 | 交付追溯建议 |
| 状态查询 | 可补充 `isInitialized()` 等查询 | 便利接口，不是冰雪 DOCX 强制缺项 |
| 模型配置与检测配置分开 | 保留当前两层配置，说明哪些字段可热改、哪些需要重建 | 当前已经有基础结构 |
| 公共头包含完整契约 | 冰雪调用者应能方便获得错误值及含义，不必到内部实现猜数值 | 建议完善公共接口说明 |

当前通用错误定义位于 [include/evs/status.h](include/evs/status.h)。若增加冰雪公共错误常量，应区分顶层调用错误和对象级错误，不能简单复制倍率模块 `0x0002` 的数值。冰雪模块为 `0x0008`；也不能把声明了网络、存储等错误名称误写成当前冰雪检测已经具有网络或存储功能。

### 6.3 不应直接照搬的内容

| 倍率参考内容 | 冰雪应保留的约定或处理方式 |
|---|---|
| detect 接收角度、长度、操作手反馈及当前倍率 | 冰雪保持两参 detect；是否调用由宿主判断，不增加倍率专属参数 |
| 图像宽、高、通道为 `int32_t` | 冰雪按自身 DOCX 保持 `uint32_t` |
| 时间戳为 `int64_t` | 冰雪按自身 DOCX 保持 `uint32_t` 毫秒 |
| 推理耗时为 `float` | 冰雪按自身 DOCX 保持 `uint32_t inference_time_ms` |
| 模型路径名 `model_path` | 冰雪文档字段为 `Model_path`，保留客户大小写 |
| 存在 `RESULT_LOW_CONFIDENCE`、`ALERT_DANGER` | 冰雪当前按文档保持结果 0/1、报警 0/1，不随意增加业务值 |
| 默认预热为 true | 冰雪文档没有规定必须默认 true，不为外观统一而改变当前默认值 |
| 内部控制起臂倍率识别会话 | 不把倍率角度、臂长规则搬回冰雪库；各功能职责以各自要求为准 |
| 重复初始化先释放再重建 | 当前冰雪重复初始化幂等，应明确差异，不直接照搬行为 |

### 6.4 枚举写法与兼容建议

倍率使用整数别名和命名空间常量，当前冰雪使用固定底层类型的 `enum class`。例如当前正确写法为：

以下代码只演示枚举比较；真实调用应先读取 detect 返回码并处理调用错误，再解释本次有效结果。

```cpp
using namespace evs::winch_ice;

if (result.result_code == ResultCode::RESULT_MATCH &&
    result.alert_level == AlertLevel::ALERT_WARN) {
    // 已有有效报警；全部对象是否结束还需看 check_complete。
}
```

数值为 1 不意味着当前枚举可以不经转换直接与任意整数混用。`enum class` 有类型检查价值，不必为了模仿倍率而立即改为整数别名。

如需提供类似客户倍率示例的 `RESULT_MATCH`、`ALERT_WARN` 写法，可以考虑增加类型化常量别名。以下仅为建议示例，尚未加入源码：

```cpp
inline constexpr ResultCode RESULT_MATCH = ResultCode::RESULT_MATCH;
inline constexpr AlertLevel ALERT_WARN = AlertLevel::ALERT_WARN;
```

这类便利写法不要求直接改变现有枚举底层类型或结构布局；实际变更仍应检查名字冲突及宿主源代码兼容性。

### 6.5 风格和参考材料的边界

继续使用仓库 `.clang-format`，保留约定的 4 空格缩进、函数定义间空行、无行尾空格及文件末尾换行。公开名称需要优先满足客户原型和既有调用方式，不为追求表面统一批量改名。

`#pragma once` 与 include guard、字段单独一行与同行声明、注释是否使用长横线，本身不是功能正确性问题。建议借鉴倍率头的注释组织和契约完整性，而不是大面积重排共享源码。

倍率公共头文件也不能替代实现和实测证据。例如其 `SCORE_SIZE=12` 与“无倍率加 1～10 倍率”的索引说明需要澄清额外槽位含义，不能直接认定参考件没有歧义。该项只指出头文件说明需要明确，不据此推断倍率模型实际计算错误。

### 6.6 本文对应的验证边界

上一轮实际执行过：OpenCV/GStreamer ON 的增量构建、现有 CTest 6/6、15 个专用文件及 7 个共享修改行的格式检查、22 个 C++ 文件空白检查、6 个 Python 文件的 Black 检查、12 个编译条目的语法和类型检查，以及 `git diff --check`。这些结果属于上一轮运行记录，本次仅整理文档，没有重新执行算法测试。

上一轮新增契约探针复现了配置更新日志异常和嵌套模型路径未读取问题。本次已分别落实源码修复，并补充配置加载、旧写法兼容、失败保留、宿主回调异常及配置更新分配故障回归；本轮实际执行结果统一记录于 docs/winch_ice_acceptance.md。旧核验日志保留当时的故障现象用于追溯。

真实模型、灰度支持、模型冰雪框与影子输出尚未完成；真实 Ubuntu 板、摄像头、PLC、现场准确率、端到端时延和长期运行也不能因本文整理完成而视为已验收。


## 七、TaskConfig 与 model_config 能否统一，以及历史遗留

### 7.1 它们的层级不同

`TaskConfig` 是 [include/evs/task_config.h](include/evs/task_config.h) 中的完整任务配置类型，涵盖任务/模块、模型后端、模型参数、解码、相机、人员策略、地面映射、调试以及高度/冰雪各自的参数。

`model_config` 是 `WinchSnowConfig` 里的成员名，其类型是 `evs::winch_ice::ModelConfig`，当前只有 `Model_path/input_width/input_height/warmup` 四个模型参数。

所以可以统一模型参数的定义与映射，但不能直接把整个任务配置当成模型子配置。合理目标是完整任务配置包含一个公共模型子结构，对外冰雪配置也复用它；TaskConfig 自身仍然存在。

### 7.2 当前为何有两份表达

现有 Git 基础版本已经使用平铺 TaskConfig，VisionTask、模型资源加载、解码、预处理、插件、离线评估和摄像头工具围绕它调用。本轮增加的冰雪类型化公共接口按客户文档提供 WinchSnowConfig 和嵌套 ModelConfig，再通过 winch_ice_api.cpp 映射到通用任务执行链。

因此当前路径包括：

```text
YAML → 配置解析 → TaskConfig → 通用任务执行链

客户 WinchSnowConfig.model_config → 明确适配 → TaskConfig → 通用任务执行链
```

两层适配本身不是错误；实际问题是两种 YAML 写法未正确归一到同一内部字段，以及映射重复需要维护。本次先修复前者，保留已有 C++ 使用方式。

### 7.3 难点与迁移影响

| 历史或约束 | 统一时必须处理的内容 |
|---|---|
| 已有 `cfg.Model_path/cfg.input_width` 等平铺访问 | 需迁移 VisionTask、解析器、离线评估、直接使用 TaskConfig 的工具和测试，不能只改冰雪适配器 |
| TaskConfig 位于公开 include 目录，并通过公开初始化及插件接口使用 | 它不只是私有临时变量；调整成员结构可能改变布局、偏移和聚合初始化方式，需要宿主/核心/相关插件一致重编及 ABI 核验 |
| 同时支持旧标量 model、新 model 配置段及 Model_path | 统一 C++ 结构不等于自动兼容 YAML，仍需别名映射、冲突和缺省规则 |
| 对外 ModelConfig 只有四个字段 | 通用执行链还有 backend、input_format、letterbox、output_format、decoder_classes、init_timeout_s、npu_core_mask 等，应划分公共模型信息与引擎运行选项，不把所有 TaskConfig 字段塞入客户模型结构 |
| 已有配置加载的成功覆盖、失败保留语义 | 重构时仍应保留调用者未被文件指定的值，并在解析失败时保持旧配置 |
| 初始化参数和热更新参数职责不同 | 模型文件及解码等变更需要重建；面积/置信度等策略有各自更新限制，不能因为合并结构就让所有模型参数可热改 |
| 当前 TaskConfig 直接依赖冰雪公共头里的专用类型 | 若抽取公共 ModelConfig，应放入独立通用头，避免让所有模型配置依赖冰雪业务命名空间 |
| 部分旧 x86 后端源码含非文本内容，尚未恢复完整构建验收 | 大范围重构前还需补齐受影响旧工具和后端的完整构建证据，不能只用当前 mock 路径替代 |

技术难点主要是迁移与兼容性，不涉及必须等待冰雪模型，也不是不能重构。

### 7.4 建议的分步统一路线

1. 本次先把旧式和客户块式 YAML 的模型字段归一到现有内部字段，修复漏读和冲突处理，保持两个公开结构布局及冰雪四个方法原型不变。
2. 后续把稳定的公共模型信息抽到独立通用头，明确四个公共字段、默认值和引擎专有参数的归属。
3. 让 TaskConfig 包含该公共模型子结构；对外 WinchSnowConfig 保持客户要求的 model_config 名字。需要兼容既有类型名时评估别名或适配，不在缺少 ABI 验证时假定二进制无变化。
4. 逐一迁移字段访问及映射，保持唯一可写来源，完成所有受影响模块、工具、插件和宿主的编译与回归，再按实际兼容性发布版本。

不建议同时保留一套可写的平铺字段和一套可写的嵌套字段，长期依赖互相复制；这会产生到底哪一份生效的新问题。也不建议简单用引用成员维持旧字段写法，引用成员会引入额外布局、拷贝绑定和生命周期约束。

本次没有把这项跨模块结构迁移混入两个问题的修复，也没有把客户日志 SDK 的实际接入写成已经完成。当前日志接口继续保留既有宿主 sink/调试能力，新增异常隔离；客户日志的确切头文件、函数签名及生命周期约定仍需按其真实参考接入。
