# X86 离线评测（精度 / 召回）

## 为什么需要 X86

现场拍下照片后，直接在 X86 笔记本上跑同一套算法代码，独立评估
**准确率（precision）** 与 **召回率（recall）**，而不必上板。好处：

- 排查漏检 / 误检，量化算法改进效果；
- 用真实照片做回归，避免“板子上才出问题”；
- 算法同事不需要嵌入式环境即可迭代模型。

平台无关的关键是 `IInferBackend` 抽象：嵌入式（RKNN）与 X86 共用同一套
`Preprocessor → Infer → Decode` 管线，只是后端不同。

## X86 后端（`x86`）的两种模式

| 模式 | 开关 | 依赖 | 用途 |
|------|------|------|------|
| **参考检测器**（默认） | `-DEVS_BACKEND=x86` | 无（纯 C++） | 零依赖跑通整套离线评测 / CI；是真实模型的对照基线 |
| **真实模型** | `-DEVS_BACKEND=onnx -DEVS_WITH_ORT` + ORT 库 | ONNXRuntime | 加载你部署同款的导出 `.onnx`，给出**真实生产精度/召回** |

两种模式接口完全一致，`OfflineEvaluator` 与 `VisionTask` 对它们一视同仁。
参考检测器是一个真实的颜色 blob 算法（红色→类0、蓝色→类1 的连通域），
不是空壳；它让你在没有 ORT 时也能验证评测方法论本身。

> 接入真实模型：把训练导出的 ONNX 放到 X86 机，CMake 配置
> `-DEVS_BACKEND=onnx -DEVS_WITH_ORT -DONNXRUNTIME_LIB_DIR=/path/to/ort/lib`，
> 任务 YAML 里 `backend: onnx`、`model: 你的.onnx`，
> `output_format` 改成该模型实际输出格式（`flat6` / `yolov8` / 自定义解码器）。
> ONNX 的输入约定与边缘端一致：**NHWC uint8**（由 `Preprocessor` 产出）。

## 数据集格式

一个数据集目录：

```
dataset/
  images/            # 照片，原生支持 PPM（P6）。现场 JPEG/PNG 见下方转换
    img1.ppm ...
  annotations.json   # 人工标注（0..1 相对坐标，与 Target 约定一致）
```

`annotations.json`：

```json
{
  "class_names": ["person", "helmet"],
  "images": [
    {"file": "img1.ppm", "boxes": [{"class": 0, "x": 0.3, "y": 0.4, "w": 0.2, "h": 0.3}]},
    {"file": "img2.ppm", "boxes": []}
  ]
}
```

坐标：`x,y` 是中心，`w,h` 是宽高，全部 0..1（相对于原图）。

### 现场照片（JPEG/PNG）转 PPM

框架刻意不引入 OpenCV，照片走轻量转换即可：

```bash
# 用 Pillow（推荐）
python3 -c "from PIL import Image; Image.open('field.jpg').convert('RGB').save('field.ppm')"
# 或任意工具：convert field.jpg field.ppm
```

也可直接送 RGB 裸数据，但 PPM 最省事。转换脚本示例见
`tools/eval_offline/gen_fixtures.py`（纯标准库，可参考）。

## 运行评测工具

```bash
# 1) 生成示例数据集（可选，用于自测）
python3 tools/eval_offline/gen_fixtures.py

# 2) 构建（x86 参考模式）
cmake -S . -B build -DEVS_BACKEND=x86
cmake --build build --config Release

# 3) 运行
./build/eval_offline --task configs/eval_demo.yaml \
                     --dataset tools/eval_offline/fixtures \
                     --iou 0.5 --dump --out ./out
```

输出：

- `out/report.json`：完整报告；
- `out/report.csv`：每类 + 汇总（precision/recall/F1/AP）；
- `out/images.csv`：每张图的 tp/fp/fn；
- `out/dump/*_cmp.ppm`：错检/漏检图的对比（红=预测、绿=标注）。

## 指标说明

- **TP / FP / FN**：按类别、以 IoU ≥ `iou` 做贪心匹配（预测按置信度降序）。
- **precision = TP/(TP+FP)**，**recall = TP/(TP+FN)**，**F1** 调和平均。
- **AP@IoU**：VOC all-point（右侧最大精度插值），**mAP** 为各类 AP 均值。
- **micro P/R**：把所有类别的预测/标注汇总后计算，反映整体。

> 注意：参考检测器的数字是“参考算法”的数字，用来验证评测链路；
> 真实模型的精度/召回请用 `onnx` 模式 + 你的导出模型得到。

## 复用既有调试能力

`OfflineEvaluator` 内部走的就是设备上的同一管线；它还会在开启 `--dump`
时用既有的 `save_rgb_ppm` 落盘对比图，与 `DebugConfig` 的 dump 机制同源。
需要把单帧张量单独拿出来看时，仍可用 `VisionTask::saveLastInput()`。
