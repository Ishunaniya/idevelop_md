# API说明

头文件：include/evs/winch_ice.h；命名空间：evs::winch_ice；语言：C++17。ErrorCode为uint32_t，0表示接口成功，非0表示失败。业务结果另看result_code、valid和alert_level。

当前模型检测卷扬。result_code和snow_detections暂按卷扬输出；alert_level依据HSV候选面积占比计算，尚未接入冰雪模型。

## 调用时序

```text
准备 WinchSnowConfig
  → initialize(config)
      失败：记录错误，修正配置后重试
      成功：进入检测
  → detect(image, result)
      返回非 0：本帧失败，不使用该帧业务结果
      返回 0：读取候选，再判断卷扬结果、对象 valid 和报警
  → 按需 updateConfig(完整 DetectConfig)
      成功：后续检测使用新阈值
      失败：继续使用原配置
  → 继续下一帧，或 release()
```

取图、工况判断、上电后只执行一次的策略、重试和报警保持由调用方处理。初始化前或释放后不能检测；更换模型、输入规格或相机时须先释放，再初始化。图像缓冲区至少保持到 `detect` 返回。

`demo/main.cpp` 提供可运行流程：读取配置和图片 → 初始化 → 按需更新阈值 → 检测 → 释放。失败时保存已执行步骤和错误码。带 `--log-callback` 时，在检测流程前注册日志回调，释放实例后撤销回调，见第 8 节。

最小接入程序见 [`demo/minimal.cpp`](../demo/minimal.cpp)，独立编译命令见 [README](../README.md)。相机取流、图像格式转换由调用方完成；绘框、写图片和 JSON 是 demo 的演示功能，不是检测接口必需步骤。

## 1. 四个核心方法

### initialize：初始化

```cpp
ErrorCode Detector::initialize(const WinchSnowConfig& config);
```

| 项目 | 说明 |
| --- | --- |
| 输入config | 完整功能配置，字段见第2节。模型路径和输入尺寸须与配套模型一致。库保存所需配置，调用返回后可释放外部配置对象。 |
| 输出参数 | 无。 |
| 返回值 | 0表示初始化成功；非0表示失败，库释放本次已取得的资源，可修正后重试。 |

初始化完成模型加载、张量检查、参数配置和可选预热。warmup=true时预热失败也使初始化失败。已初始化后重复调用直接返回0，保留原配置；新参数不会重新加载或校验。换模型、相机或输入规格，应先release再initialize。

常见错误：配置非法0x00084001、阈值越界0x00081003、能力不支持0x00081009、模型加载失败0x00082002、张量不匹配0x00082003、预热失败0x00083001、内存不足0x00081004。

### detect：处理一帧

```cpp
ErrorCode Detector::detect(const ImageData& image, WinchSnowResult& result);
```

| 项目 | 说明 |
| --- | --- |
| 输入image | 当前原图，字段见第3节；紧凑RGB24或8位灰度，缓冲区保持到调用返回。 |
| 输出result | 本帧结果，字段见第4节；调用方可复用同一个结果对象。 |
| 返回值 | 0表示本帧调用正常，仍须判断结果有效性；非0时旧候选、对象和报警被清空，本帧业务结果不可用。 |

须先成功初始化。每次同步处理一帧，包括重复时间戳；错误后可以继续下一帧。没有自动报警锁存或停止检测。

返回码为0时，可读取snow_detections供影子系统使用，不要求result_code=1。业务结果再看result_code及objects[i].valid，只有valid=true才读取ice_ratio。低分候选、筛选后的卷扬框、面积估算有效的对象是不同阶段。没有卷扬框、无法估算面积、有效占比为0%是三种不同情况。

常见错误：未初始化0x00081007、空指针0x00081002、通道数错误0x00083002、尺寸错误0x00083003、推理失败0x00083001、张量或输出错误0x00082003、后处理失败0x00083005、NMS后业务对象超过两个0x00081003、内存不足0x00081004。

### updateConfig：更新阈值

```cpp
ErrorCode Detector::updateConfig(const DetectConfig& config);
```

| 项目 | 说明 |
| --- | --- |
| 输入config | confidence_threshold、nms_threshold、area_ratio三个阈值整体更新。 |
| 输出参数 | 无。 |
| 返回值 | 0表示更新成功，后续detect使用新阈值；非0表示更新失败，原配置保留。 |

须先成功初始化。三个字段均须是[0,1]内有限数，NaN/Inf拒绝。不修改模型、输入尺寸和相机。建议复制当前DetectConfig后修改目标字段，成功后再保存宿主当前配置：

```cpp
auto next = cfg.detect_config;
next.area_ratio = 0.8f;
if (detector.updateConfig(next) == 0) cfg.detect_config = next;
```

常见错误：未初始化0x00081007、非法阈值0x00081003、内存不足0x00081004。

### release：释放资源

```cpp
void Detector::release();
```

无输入、无输出参数、无返回值。释放内部任务和模型引用，不销毁外部Detector对象，也不释放调用方的图像和结果容器。允许重复调用；释放后可重新initialize，detect/updateConfig在重新初始化前返回未初始化。

构造后为未初始化状态；析构自动清理资源。Detector不可拷贝或移动。不要在检测未返回时并发release/析构。已经返回的结果由调用方持有，release不会自动清除宿主保存的旧结果。

## 2. 配置字段

### WinchSnowConfig

| 字段 | 类型 | 默认值 | 含义/范围 |
| --- | --- | --- | --- |
| model_config | ModelConfig | 见下表 | 模型配置。 |
| detect_config | DetectConfig | 见下表 | 检测阈值。 |
| camera_channel | uint32_t | 0 | 绑定相机的普通ID，0～254；不是多通道打包值。 |

### ModelConfig

| 字段 | 类型 | 默认值 | 含义/范围 |
| --- | --- | --- | --- |
| Model_path | std::string | 空 | 有效模型文件路径，保留大写M。直接构造配置时，相对路径按进程工作目录解释。 |
| input_width | int32_t | 0 | 模型输入宽，像素；要求1～8192且与模型一致，本包填640。 |
| input_height | int32_t | 0 | 模型输入高，像素；同上，本包填640。 |
| warmup | bool | false | 是否初始化预热，true时预热失败也会使初始化失败。 |

### DetectConfig

三个字段都是 `float`，必须是 [0,1] 内的有限数，不接受 NaN 或 Inf。

| 字段 | 默认值 | 判断规则 | 调整的影响 |
| --- | --- | --- | --- |
| confidence_threshold | 0.45 | 置信度大于0且达到此值的卷扬框，才进入后续筛选；阈值为0时仍排除零分框。 | 调高会排除更多低分框，也可能漏检；调低会采用更多低分框，也可能引入错误框。 |
| nms_threshold | 0.5 | 用 NMS 去掉重复框：两个框的 IoU 超过此值时，优先保留高分框。 | 调高更容易保留重叠框；调低去重更严格，也可能排除相邻目标。 |
| area_ratio | 0.5 | 任一 valid=true 的对象 ice_ratio 达到此值即报警；0.5 表示50%。 | 调高更难触发报警，调低更容易触发报警。 |

IoU 是两个框的交集面积除以并集面积。三个配置项都不用于过滤 `snow_detections`，其中仍保留合法的低分、零分候选。

`confidence` 是模型给每个框的评分，由 `snow_detections[i].confidence` 和 `objects[i].box.confidence` 返回，随图片变化，不是输入阈值，也不等同于识别准确率。`ice_ratio` 是 HSV 颜色统计得到的面积占比，与置信度不同。HSV 表示色相 H、饱和度 S、亮度 V。

`detect` 的返回结构不回传配置，demo 另行保存当次配置用于留档。实际样例数值见[运行记录](RUN_RECORDS.md)。

边界值：confidence_threshold=0仍排除零分业务框；=1时仅评分为1的框可通过。area_ratio=0时，只要有面积估算有效的对象，即使占比为0也满足报警条件。三个值均不是越高或越低就一定越好。

输入尺寸是模型尺寸，不是相机/原图尺寸。本包单类YOLOX格式为640×640、原始输出[1,8400,6]；后端和输出格式在库内固定。HSV的S≤80、V≥180及3×3开闭运算不由这三个阈值改变。

### 哪些参数可以更新

| 参数 | 修改方式 |
| --- | --- |
| confidence_threshold、nms_threshold、area_ratio | 用 updateConfig 整体传入三个字段；成功后生效，失败保留原配置 |
| Model_path、input_width、input_height、warmup、camera_channel | 在 WinchSnowConfig 中设置；修改后先 release，再 initialize。已初始化时重复 initialize 不会应用新值 |
| 模型解码格式、HSV 的 S/V 门槛和形态学参数 | 当前公开接口不提供修改；不能通过 updateConfig 改变 |

## 3. 图像字段 ImageData

| 字段 | 类型 | 默认值 | 含义/范围 |
| --- | --- | --- | --- |
| data | const uint8_t* | nullptr | 原图首字节，非空、CPU可读。实际缓冲区至少width×height×channels字节。 |
| width | uint32_t | 0 | 原图像素宽，>0且≤INT_MAX/3；width×height≤INT_MAX。 |
| height | uint32_t | 0 | 原图像素高，>0且≤INT_MAX/2；还受总像素和设备内存限制。 |
| channels | uint32_t | 3 | 3=RGB24，R/G/B顺序；1=8位灰度。 |
| timestamp_ms | uint32_t | 0 | 毫秒标记，允许0、重复和自然回绕；库不据此去重或判断工况时效。 |

无stride/size_bytes字段。RGB每行width×3字节，灰度每行width字节，带行填充的图像先整理。BGR/NV12先转换成RGB。输入内存由宿主持有，至少保持到detect返回；库不保存图像指针。灰度复制成三通道后处理，其精度需单独验证。

## 4. 输出字段

### WinchSnowResult

| 字段 | 类型 | 默认值 | 含义 |
| --- | --- | --- | --- |
| result_code | ResultCode | RESULT_NO_RESULT | 临时表示卷扬检测状态：1至少一个通过筛选且置信度大于0的卷扬框，0没有；与HSV面积估算是否有效无关，不是API返回码。 |
| alert_level | AlertLevel | ALERT_NONE | 1表示有有效对象达到面积阈值，0表示本帧没有有效报警触发。接口失败时不据此判断安全状态。 |
| snow_detections | std::vector<Detection> | 空 | 临时返回卷扬模型合法候选；原图像素坐标、模型输出顺序，保留合法零分框，不做置信度过滤/NMS/Top-K，不限两个。像素宽高为0的框不入列。不是冰雪模型结果。 |
| inference_time_ms | uint32_t | 0 | 推理调用计时，四舍五入到毫秒，含后端调用/张量处理；不是纯NPU芯片时间或完整detect耗时。 |
| camera_channel | uint32_t | 0xFFFFFFFF | 本次绑定的普通相机ID；错误返回时不依赖该字段的特定值。 |
| objects | std::vector<WinchObjectResult> | 空 | 最多两个NMS后卷扬对象，按框中心从左到右排列，序号只对本帧有效。 |

### WinchObjectResult

| 字段 | 类型 | 默认值 | 含义 |
| --- | --- | --- | --- |
| box | Detection | 见下表 | 原图卷扬定位框。 |
| ice_ratio | float | -1 | valid=true时[0,1]，候选像素数/原图矩形框面积；无效时为-1。 |
| valid | bool | false | 当前对象的面积估算是否有效；false为未知，不能按0%处理。 |

无效对象不阻止其他有效对象触发报警。没有业务框时result_code=0，但snow_detections仍可能非空；已有卷扬框而全部无法估算面积时result_code=1、valid=false。结果没有稳定对象ID和对象级错误码。

### Detection

| 字段 | 类型 | 默认值 | 含义 |
| --- | --- | --- | --- |
| class_id | uint32_t | 0 | 本包单类卷扬ID为0。 |
| confidence | float | 0 | [0,1]，卷扬定位的目标概率×类别概率，不是冰雪占比。 |
| x | uint32_t | 0 | 原图框左上角x，像素。 |
| y | uint32_t | 0 | 原图框左上角y，像素。 |
| width | uint32_t | 0 | 框宽，像素；右边界=x+width。 |
| height | uint32_t | 0 | 框高，像素；下边界=y+height。 |

面积占比的分母为裁剪后原图框的width×height，不是卷筒表面分割面积。

### 枚举

| 枚举 | 底层类型 | 值 | 含义 |
| --- | --- | --- | --- |
| ResultCode::RESULT_NO_RESULT | int32_t | 0 | 没有通过业务筛选的卷扬。 |
| ResultCode::RESULT_MATCH | int32_t | 1 | 至少一个通过业务筛选的卷扬。 |
| AlertLevel::ALERT_NONE | int32_t | 0 | 本帧无有效报警。 |
| AlertLevel::ALERT_WARN | int32_t | 1 | 本帧有效对象达到阈值。 |

### 如何判断本帧结果

先检查 `detect` 的返回码，再读业务字段。下面的 `valid` 和 `ice_ratio` 指 `objects` 中各个对象的字段。

| 情况 | 返回码 | result_code | 对象和面积占比 | alert_level |
| --- | --- | --- | --- | --- |
| 接口失败 | 非0 | 不用于业务判断 | 本帧业务输出不可用，不使用旧结果 | 不用于判断安全状态 |
| 没有筛选后的卷扬框 | 0 | 0 | objects为空；snow_detections仍可能非空 | 0 |
| 检出卷扬，但所有对象都无法估算面积 | 0 | 1 | 所有valid=false、ice_ratio=-1；占比未知 | 0，不表示无冰雪 |
| 至少一个对象面积估算有效，且没有对象达到面积阈值 | 0 | 1 | 有效对象valid=true、ice_ratio低于area_ratio | 0 |
| 任一有效对象达到面积阈值 | 0 | 1 | 该对象valid=true、ice_ratio≥area_ratio | 1 |

一个对象无效时，另一个有效对象仍可触发报警。有效占比为0%与无法估算面积不同；`area_ratio=0` 时，有效占比0%也会触发报警。`result_code` 和 `snow_detections` 当前仍暂按卷扬输出，不能据此认定已检测到冰雪目标。

## 5. 可选辅助函数 load_config

```cpp
ErrorCode load_config(const std::string& path, WinchSnowConfig& out);
```

| 项目 | 说明 |
| --- | --- |
| 输入path | 可读YAML路径；相对路径按进程工作目录。支持本包平铺Model_path等键。 |
| 输出out | 成功后替换为解析配置；失败保留原值。模型相对路径按YAML所在目录转换为绝对路径。 |
| 返回值 | 0解析成功，非0失败；不加载模型，不证明模型存在或格式匹配，数值范围最终以initialize检查为准。 |

这是可选工具，不属于四个核心方法。客户直接构造WinchSnowConfig即可不使用YAML。winch_ice.yaml是示例配置，不是API必需文件。

本包配置将模型写为../models/winch_model.rknn。解析器不是通用YAML库，不支持嵌套model.path形式。常见错误：文件不可读/内容非法0x00084001、语法/键值/冲突0x00084002、内存不足0x00081004。

## 6. 错误码和调用约束

非0错误码以0xMMMMEEEE表示，本功能MMMM=0008。ErrorCode为uint32_t，下表列出本包使用的错误码。

| 错误码 | 含义 |
| --- | --- |
| 0x00081001 | 参数非法。 |
| 0x00081002 | 空指针。 |
| 0x00081003 | 参数越界或业务对象超过两个。 |
| 0x00081004 | 内存分配失败。 |
| 0x00081005 | 相机号非法/不匹配。 |
| 0x00081007 | 未初始化。 |
| 0x00081009 | 后端或能力不支持。 |
| 0x00082002 | 模型读取/加载失败。 |
| 0x00082003 | 模型张量规格或数值不符合格式要求。 |
| 0x00083001 | 输入设置、推理或获取输出失败。 |
| 0x00083002 | 图像数据/通道数非法。 |
| 0x00083003 | 图像尺寸非法。 |
| 0x00083004 | 预处理失败。 |
| 0x00083005 | 后处理失败。 |
| 0x00084001 | 配置不可读或非法。 |
| 0x00084002 | 配置解析失败。 |

部分底层错误会归并为模型加载或推理失败，需结合日志排查。库内不可观测状态0x0008C001通常转为对象valid=false、ice_ratio=-1，不一定作为整帧返回码。

同实例的方法与析构须串行，共享模型的不同实例也统一串行调度。结果容器归调用方，跨线程使用时自行同步。调用方自身C++分配和对象构造仍可能抛异常，应在宿主边界处理。库不保证固定的完成时间，也未提供取消正在进行的检测的接口。

### 常见问题

| 现象 | 检查与处理 |
| --- | --- |
| 启动提示找不到 libevs_*.so.1 | 保留 bin/、lib/层级及.so符号链接；用ldd确认加载本包库，两个EVS库须配套 |
| 找不到 librknnrt.so 或 libopencv_*.so.4.5d | 检查目标板运行库。本包依赖OpenCV core/imgproc及RKNN运行库，由设备提供；不要只因客户程序未使用OpenCV就省略运行库 |
| GLIBC_*、GLIBCXX_* 或 CXXABI_* 版本不满足 | 使用与目标板运行环境兼容的工具链或配套二进制；不能靠修改模型文件解决 |
| 0x00082002，模型加载失败 | 检查模型路径、读权限、文件校验值及RK3588目标匹配情况；结合RKNN日志排查 |
| 0x00084001 / 0x00084002，配置读取或解析失败 | 检查YAML路径、读权限及键值；load_config按YAML所在目录解析模型相对路径，直接构造配置则按工作目录解析 |
| 0x00081007，未初始化 | 先确认initialize返回0；release后须重新初始化 |
| 0x00081003，参数越界或对象数超限 | 初始化/更新时检查三个阈值是[0,1]有限数；detect时检查日志，筛选后超过两个对象也会返回此码，候选数组不会静默截成两个 |
| 图像数据、尺寸错误，或框和颜色异常 | 按ImageData约定检查缓冲区长度、RGB顺序和紧凑行布局；BGR/NV12及带行填充图像先转换、整理 |
| demo提示输出目录已存在 | 为--out指定一个新目录；不要覆盖已有运行记录 |
| demo读图失败 | 主demo只读P6/255 PPM；minimal.cpp只演示包内704×576样例。API本身接收内存缓冲区，不接收图片文件路径 |

## 7. 使用边界

- HSV按卷扬矩形框内的颜色估算冰雪候选面积，不是卷筒真实表面的分割结果。反光、背景、暗雪、透明冰和定位框偏差都会影响占比。
- 筛选后最多两个卷扬对象，超过两个时报错并清空本帧输出。此限制不用于截断snow_detections。
- 对象交叠、小于3×3的框、极暗或近均匀过曝图像等情况，可能返回valid=false、ice_ratio=-1，不能按0%处理。
- 系统运行库由目标设备提供，环境及依赖版本见VERSION.json。同为AArch64不代表任意发行版都能直接运行，部署前检查ldd。

## 8. 日志接口

头文件：include/evs/alog.h；include/evs/alg_api.h仅转引此头。接口为C链接符号，实现在libevs_core.so.1。日志设置为进程全局，影响使用同一核心库的所有Detector。

### 使用顺序

不接管日志时，可直接使用Detector。库首次构造内部Logger时安装默认桥，EVS日志输出到默认控制台；EVS_LOG_LEVEL和EVS_LOG_FILE影响默认Logger。

需要接入宿主日志时，在初始化Detector前注册回调，释放全部实例并停止库调用后再撤销：

```cpp
#include <evs/alg_api.h>
#include <cstdio>

static void receive_log(int level, const char* tag, const char* file,
                        int line, const char* msg) {
    std::fprintf(stderr, "[%d][%s][%s:%d] %s\n", level,
                 tag ? tag : "", file ? file : "", line, msg ? msg : "");
}

// 在现有程序中：
// alg_init(receive_log);
// initialize → detect / updateConfig → release全部实例
// alg_log_set_cb(nullptr);
```

完整可运行流程在demo/main.cpp中，运行命令：

```sh
./bin/winch_ice_api_demo configs/winch_ice.yaml demo/sample.ppm --log-callback --out /tmp/winch_log_run_01
```

注册宿主回调后，消息由宿主处理；默认Logger的文件输出、级别过滤和环形记录不再自动作用于这些消息。RKNN/OpenCV直接输出的内容不保证经过此回调；demo另外捕获标准输出/错误到run.log。

### 函数输入和输出

| 函数 | 输入 | 输出/返回值 |
| --- | --- | --- |
| int alg_init(alg_log_cb log_cb) | 宿主回调地址；nullptr清空回调。 | 保存回调，当前实现固定返回0；只设置日志，不初始化模型或Detector。 |
| void alg_log_set_cb(alg_log_cb cb) | 新回调或nullptr。 | 直接替换全局回调，无输出参数和返回值。 |
| void alg_log_set_default_cb(alg_log_cb cb) | 默认回调或nullptr。 | 仅当前没有回调时赋值；已有回调保持不变；无返回值。一般供库内部使用。 |
| int alg_log_has_cb(void) | 无。 | 1=当前有回调（包括默认桥），0=没有。不能据此区分默认桥与宿主回调。 |
| void alg_log_emit(int level, const char* tag, const char* file, int line, const char* fmt, ...) | 级别、模块名、源文件、行号、printf格式及匹配的变参。 | 同步调用当前回调，无返回值；没有回调时直接返回。fmt须为有效非空格式串，变参类型必须匹配。 |

`alg_log_set_cb(nullptr)`只清空回调，不恢复默认桥。默认桥在内部Logger构造时安装一次；不要把撤销回调当成“恢复默认输出”。所有设置函数必须在停止库日志产生后串行调用，不能与detect、初始化或其他发日志的线程并发修改；本版全局回调指针没有并发保护。

### 回调参数

```cpp
typedef void (*alg_log_cb)(int level, const char* tag,
                           const char* file, int line, const char* msg);
```

| 参数 | 含义和使用约定 |
| --- | --- |
| level | int，0=DEBUG、1=INFO、2=NOTICE、3=WARN、4=ERROR、5=FATAL；对应ALG_LOG_*常量。FATAL只是日志级别，接口本身不结束进程。 |
| tag | const char*，模块名；ALOG宏默认“algo”，可在包含头前定义ALG_MODULE。宿主读取时按可能为空处理。 |
| file | const char*，日志来源文件路径，可能包含编译目录；宿主可取文件名。按可能为空处理。 |
| line | int，来源行号。 |
| msg | const char*，已格式化消息；内部缓冲区1024字节，最多保留1023字节正文，超长截断。库不自动追加换行，格式串自身可包含换行。 |

回调无返回值，在发日志的线程内同步执行。三个字符串指针只在本次回调期间使用，异步入队须复制内容。回调保持简短，不抛异常，不在回调中调用ALOG/EVS日志或再次进入Detector，避免递归和重入。

### ALOG宏

ALOG_DEBUG、ALOG_INFO、ALOG_NOTICE、ALOG_WARN、ALOG_ERROR、ALOG_FATAL均接收printf格式串和变参，自动填入级别、ALG_MODULE、__FILE__和__LINE__，无返回值。例如：

```cpp
#define ALG_MODULE "customer"
#include <evs/alog.h>
// 注册回调后：ALOG_INFO("frame=%u", frame_id);
```

EVS原Trace/Debug映射到DEBUG，Info映射到INFO，Warn映射到WARN，Error映射到ERROR。未注册回调时不格式化消息，但C/C++函数参数表达式仍会求值，不应在参数中放有副作用的操作。

[运行记录](RUN_RECORDS.md) · [完整示例](../demo/main.cpp)
