# 2026-10-07 推理与板端联调历史记录

以下保存原顶层README第8～11节的历史记录，便于查阅当时的模型、数据及排障过程。本次未重新执行其中的板端、精度或性能测试；这些数字不代表当前卷扬冰雪版本的验收结果。

阅读前请结合当前实现区分：

- 原记录中的`v4l2_detect_evs`、`v4l2_detect_evs_rga`是现场程序别名，当前CMake生成的目标名是`v4l2_detect`；相关模型、原始帧及`eval_yolo11*.yaml`等需现场另行准备，并非本仓库全部附带。
- 后端按输出张量形状识别原始头，上层`output_format`选择行解码器；原文“只由output_format决定”不应理解为仅改该字段就能改变模型结构。
- 下列RGA、sigmoid和性能结论是当时指定模型及运行库的记录，未在本轮目标板重现，不能推广为所有模型/运行库的通用行为。
- 当前RGA自动查库仍有预定义空变量导致跳过搜索的遗留问题；使用显式`RGA_LIB`并不等于该自动分支已修复。
- 当前入口、依赖、ABI3及可运行的mock/冰雪示例以[顶层README](../../README.md)、[冰雪说明](../winch_ice.md)和[交叉编译说明](../cross_compile.md)为准。

---

## 8. 已验证数据（2026-10-07）

| 场景 | 结果 |
|---|---|
| PC 单测（x86 / onnx 构建） | PASS=161 FAIL=0 |
| PC 真实模型 `yolov5s.onnx`（自检集，IoU 0.5 / score 0.25） | precision 1.000 / recall 1.000 / mAP 1.000 |
| RK3576 单测（rknn 构建） | PASS=137 FAIL=0 |
| RK3576 真实模型 `yolov5s-640-640_rk3576.rknn`（同一自检集） | precision 1.000 / recall 0.667 / mAP 0.667，单帧 ≈31 ms |
| PC↔NPU 逐框比对 | 6 框 IoU 0.77~1.00（均值 0.88），NPU 零误检 |
| 摄像头实时检测（1280×720 NV12） | 正确检出画面中的人；优化后 预处理 51.7 ms + NPU 29.4 ms ≈ 81 ms/帧（12.3 fps） |
| `yolo11.rknn`（YOLOv11 解耦头，9 输出） | bus 帧检出 4 人 + 1 巴士，与参考工程 `yolov5_object_detect` 同一份模型结果一致 |
| YOLOv11 实时检测（1280×720，`--rga`） | 预处理 **3.1 ms**（CPU 52.9 ms，**15~17×**）+ 推理 75.7 ms ≈ 78.8 ms/帧（**12.7 fps**） |
| RGA vs CPU 逐像素比对 | maxdiff=104 / meandiff=2.83（插值差异）；检测框差 ≈7 px，bus 帧目标数一致 |

> 自检集 = 同一模型在 Python 侧的解码结果当基准，证明的是**移植一致性**，不是检测精度。
> 验收数字请换成现场人工标注集后用同一条命令重跑。

---

## 9. YOLOv11 解耦头（`yolo11.rknn`）

板端 `yolo11.rknn` 与参考工程 `X:\apps\yolov5_object_detect` 是同一份模型
（md5 `8df71b48a8888b364689d238c083ca40`）。它和 yolov5s 的关键差别是**头部结构**：

| | yolov5s | yolo11 |
|---|---|---|
| 输出张量 | 3 个 `[1,255,S,S]` | **9 个**：每尺度 reg/cls/obj |
| 通道含义 | `3×(5+80)` 混合 | reg `[1,64,S,S]`（4 边 × 16 DFL bin）、cls `[1,80,S,S]`、obj `[1,1,S,S]` |
| 网格 | 80/40/20 | 80/40/20（stride 8/16/32） |
| 框解码 | anchor + exp() | **DFL-16**（16 个 bin 做 softmax 期望） |
| 分数 | `obj × cls` | `sigmoid(obj) × cls` |

解码实现在 `src/backend/yolo_decode.h`（`collect_v11_scales` / `decode_v11_scale`），
`decode_heads()` 会**按张量形状自动识别**布局，在 YOLOv5 anchor 路径之前分派，
所以同一个二进制可以同时跑 yolov5s 和 yolo11，只由 YAML 的 `output_format` 决定：

```yaml
output_format: yolo11      # yolov5s 用 yolov8（行矩阵语义）
person_class_id: 0         # 只检人；-1 = 全类
```

### 移植时踩到的两个坑

**① 解码层次放错了。** 后端 `rknn_backend` 在返回给上层之前**已经**调用过
`yolo::decode_heads()`，交给 decoder 的是 `[N, 4+80]` 的行矩阵，而不是 9 张原始
张量。在 `detection_decoder.cpp` 里按 `reg/cls/obj` 去切那段内存，只会得到
`targets=0`（连 `--score 0.01` 都是 0，因为每格算出来的分数都一样）。
YOLOv11 的解码必须放在 `yolo_decode.h` 里、和YOLOv5 的解码并列。

**② 该模型的 sigmoid 已被烧进图里。** 实测 `cls` 全部落在 `[0.0018, 0.88]`、
`obj` 落在 `[0.146, 0.69]`，即到达时**已经是概率**。若照参考工程再套一层
sigmoid，所有分数会被压向 `sigmoid(x)≈0.5`，表现为**全图 conf 恒等于 0.2685**、
框满天飞。`v11_fused_activation()` 默认按"已含激活"处理，需要原始 logits 时设
`EVS_YOLO11_SIGMOID=1`。（参考工程读的是 fp16 原始张量，所以那边必须套 sigmoid；
而 `rknn_outputs_get(want_float=1)` 给的是 SDK 反量化后的值，两者不能照抄。）

板端验收（`bus.nv12`，`--score 0.25`）：

```
全类    targets=5 -> class=0 x4 (0.88/0.84/0.82/0.62) + class=5 bus (0.75)
只检人  targets=4 -> 0.882 / 0.836 / 0.816 / 0.622
阈值扫描 0.15→5  0.25→5  0.5→5  0.7→4     （单调不增 = 解码正确）
```

> **快速自查**：若任意阈值下 `targets` 都不变，尤其是 conf 全是同一个数值，
> 基本就是上面第 ② 条（分数被压平），而不是坐标变换或 NPU 的问题。

---

## 10. RGA 硬件预处理（已调通）

`--rga` 把 letterbox + 缩放 + NV12→RGB 交给 RGA，预处理从 52.9 ms 降到 3.1 ms。

```bash
# 实时相机 + RGA（板端 192.168.11.1，root 空密码）
cd /root/evs
./v4l2_detect_evs_rga --task eval_yolo11.yaml --config camera_config.json \
  --channel 0 --frames 20 --score 0.25 --rga
#   [gst] capturing /dev/video0 1280x720 @25fps
#   frame 0: 1280x720 -> model 640x640, targets=1, pre[rga]=3.69ms infer=84.68ms
#   avg over 20 frames: pre=3.06ms infer=75.72ms total=78.79ms (12.7 fps)

# RGA 与 CPU 同帧逐像素比对
./v4l2_detect_evs_rga --task eval_yolo11.yaml --raw cap.nv12 \
  --raw-width 1280 --raw-height 720 --frames 1 --bench
#   [bench] rga=3.46ms cpu=53.33ms speedup=15.4x maxdiff=104 meandiff=2.833
```

### 三个必须知道的坑

**① CMake 默认会链到错的库。** `EVS_WITH_RGA=ON` 时若不显式指定，
`find_library` 会命中 `/home/xp/rknn_sdk/lib/librga.so`（**94 KB 精简版**）：
符号齐全，但 `imresize` 直接返回 `IM_STATUS_FAILED`（该 SDK 里 `0 = FAILED`、
`1 = SUCCESS`），日志表现为

```
[rga] falling back to CPU: imresize failed status=0
```

正确做法是指向 buildroot sysroot 里的真库（201 KB）与配套拆分头：

```bash
cmake -S . -B build_rga -DEVS_BACKEND=rknn -DEVS_WITH_RGA=ON \
  -DRGA_LIB=/home/xp/evs_build_aarch64/third_party/rga/lib/librga.so \
  -DRGA_API_INCLUDE_DIR=<SDK>/ubuntu/sysroots/armv8a-linux/usr/include/rga
```

> 头文件必须与库配套：混用 rknn_sdk 的新版单头会出现
> `dst handle=0x7f` + `Failed to call RockchipRga interface`。

**② `rga_buffer_t::wstride` 的单位是像素，不是字节。** 给 RGB888 传
`out_w*3`（1920）会让库按 1920 像素/行推进 → 三倍行溢出 → 图像撕裂
**并且破坏源帧**（640×640 输入下直接 `munmap_chunk(): invalid pointer` 崩溃）。
正确：传 `out_w`（640）。

**③ `dst_off` 是字节偏移，不能和 wstride 共用同一个变量。**
`dst_off = pady * stride`，若 stride 已是像素(640)，则
`140 × 640 = 89600` 字节 = **第 46 行**，整帧错位 94 行。症状非常有迷惑性：
`meandiff≈44`、颜色整体发暗发绿、检测框位置完全对不上——看起来像色域问题，
其实是几何错位（用 `EVS_RGA_NOPAD=1` 让偏移归零，内容立刻回到第 0 行即验证）。
现在代码里明确分成 `dst_stride_px`（给 librga）和 `dst_row_bytes`（自己寻址）。

> 顺带结论：librga **会忽略** `imcvtcolor` 的 CSC mode 参数（单独传
> `IM_YUV_TO_RGB_BT601_LIMIT` 或只设 `color_space_mode` 都不生效），
> 所以色域差异无法通过 API 调整；修好 ② ③ 之后纯 RGA 就与 CPU 基准一致。

### `EVS_RGA_CSC` 策略（1280×720 → 640×640 实测）

| 值 | 策略 | 预处理 | meandiff | 说明 |
|---|---|---|---|---|
| **0（默认）** | 纯 RGA：imresize + imcvtcolor | **3.5 ms** | 2.83 | 生产默认 |
| 6 | RGA 缩放 NV12，CPU 转 RGB | 11 ms | 1.56 | |
| 7 | CPU 全分辨率 CSC → RGA 缩放 RGB | 8.8 ms | 2.77 | |
| 8 | CPU 全分辨率 CSC + RGA 缩放 RGB | 41 ms | **1.31** | 像素最严格时用 |

### 排查手段

| 手段 | 用途 |
|---|---|
| `--bench` | 同帧跑 RGA + CPU，输出耗时 / maxdiff / meandiff / speedup |
| `EVS_DUMP_BENCH=/tmp/pp` | 导出两路预处理 PPM，PC 侧逐像素与位移分析 |
| `EVS_RGA_CSC=0..8` | 换预处理策略标定 |
| `EVS_RGA_NOPAD=1` | 验证 `dst_off` 是否被遵守 |
| `EVS_RKNN_DEBUG=1` | 打印每个 RKNN 输出的 dims / 类型 / 前 6 个值 |

### 已知限制

- `--rga-fd`（dma-buf 零拷贝）依赖 rkcif 的 DMABUF 输出能力，该板卡为 0，暂不可用；
  且 `EVS_RGA_CSC=8` 需要 CPU 侧帧，fd 路径不支持。
- 当前主项是推理（75.7 ms），相机 25 fps、实际 12.7 fps，仍会丢帧。

---

## 11. 板端验证速查

```bash
cd /root/evs
# 0 前置
ls -l v4l2_detect_evs v4l2_detect_evs_rga /root/cameras/yolo11.rknn bus.nv12

# 1 离线单帧  期望 targets=4（4 个人）
./v4l2_detect_evs --task eval_yolo11.yaml --raw bus.nv12 \
  --raw-width 640 --raw-height 640 --frames 1 --score 0.25

# 2 全类      期望 targets=5（4 人 + 1 巴士）
./v4l2_detect_evs --task eval_yolo11_all.yaml --raw bus.nv12 \
  --raw-width 640 --raw-height 640 --frames 1 --score 0.25

# 3 阈值单调性 期望 5 5 5 4（不变=分数被压平）
for s in 0.15 0.25 0.5 0.7; do
  printf 'score=%s -> ' $s
  ./v4l2_detect_evs --task eval_yolo11_all.yaml --raw bus.nv12 \
    --raw-width 640 --raw-height 640 --frames 1 --score $s 2>&1 | grep -o 'targets=[0-9]*' | head -1
done

# 4 实时相机  期望 1280x720 -> model 640x640
timeout 60 ./v4l2_detect_evs_rga --task eval_yolo11.yaml \
  --config camera_config.json --channel 0 --frames 20 --score 0.25 --rga

# 5 RGA/CPU 等价性  期望 maxdiff<120 meandiff<4
./v4l2_detect_evs_rga --task eval_yolo11.yaml --raw cap.nv12 \
  --raw-width 1280 --raw-height 720 --frames 1 --bench

# 6 yolov5s 回归（注意 output_format 必须是 yolov8，yolov5_raw 是非法名）
./v4l2_detect_evs --task eval_rknn_v8.yaml --raw bus.nv12 \
  --raw-width 640 --raw-height 640 --frames 1 --score 0.25
```

---
