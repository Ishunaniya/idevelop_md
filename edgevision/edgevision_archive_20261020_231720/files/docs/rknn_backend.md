# RKNN 后端（RK3576 NPU）交叉编译与真机验证

本文记录 EdgeVision 的 RKNN/NPU 路径怎么编、怎么在板子上验证，以及 2026-10-07
在 **TL3576-EVM（RK3576，librknnrt 2.3.0）** 上实测到的数字和踩过的坑。

## 1. 架构位置

```
Frame(NV12/RGB) → Preprocessor(NHWC uint8, letterbox)
   → RknnBackend::infer()   ← src/backend/rknn/rknn_backend.cpp
   → rknn_inputs_set / rknn_run / rknn_outputs_get(want_float=1)
   → yolo::decode_heads()   ← src/backend/yolo_decode.h（与 x86/ONNX 同一份代码）
   → yolov8 行 → YoloV8Decoder → Target
```

**为什么要共用 `yolo_decode.h`**：PC 用 ONNXRuntime、板子用 RKNN，两边的后处理必须
产生同样的框，否则离线评测和现场实测对不上，且无法判断是谁错了。共用一份代码后，
任何差异都只可能来自模型本身（量化/融合），这是本轮实测的主要收益。

## 2. 交叉编译（x86_64 编译机 → aarch64 板子）

**两套工具链都实测可用**（138 上都能构建，产物在板子上跑出相同结果）：

| 工具链 | 路径 / 安装 | 编译器 | 备注 |
|---|---|---|---|
| **Rockchip SDK 预置**（推荐，与板子同源） | `/home/xp/rk3576/rk3576_linux6.1_release/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu` | `aarch64-none-linux-gnu-g++ 10.3.1`，自带 sysroot | **不要**再传 `-DCMAKE_SYSROOT=$SDK/ubuntu/sysroots/armv8a-linux`：那个 sysroot 缺 `/lib/ld-linux-aarch64.so.1`，CMake 试编译会失败；用它自带的 libc 即可 |
| Ubuntu 交叉工具链 | `sudo apt-get install -y g++-aarch64-linux-gnu` | `aarch64-linux-gnu-g++ 11.4` | V4L2 相关代码用它编（见 §7） |

```bash
# 用 SDK 工具链
cat > /home/xp/tc_sdk.cmake <<'EOF'
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(SDK /home/xp/rk3576/rk3576_linux6.1_release)
set(TC ${SDK}/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu)
set(CMAKE_C_COMPILER   ${TC}/bin/aarch64-none-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER ${TC}/bin/aarch64-none-linux-gnu-g++)
set(CMAKE_FIND_ROOT_PATH ${TC}/aarch64-none-linux-gnu)
EOF

cmake -S . -B build_sdk -DCMAKE_TOOLCHAIN_FILE=/home/xp/tc_sdk.cmake \
      -DEVS_BACKEND=rknn \
      -DRKNN_API_LIB=/home/xp/rknn_sdk/lib/librknnrt.so \
      -DRKNN_API_INCLUDE_DIR=/home/xp/rknn_sdk/include
cmake --build build_sdk -j4
```

> SDK 里也有 `ubuntu/sysroots/armv8a-linux/usr/lib/librknnrt.so`，但版本是 **2.0.0b0**，
> 板子上跑的是 **2.3.0**。链接请用**从板子上拉下来的那份**，避免 runtime/头文件版本错配。

编译机准备（用 apt 工具链时才需要，一次性）：

```bash
sudo dpkg --configure -a          # 上次 apt 被中断过
sudo apt-get -o Acquire::ForceIPv4=true update
sudo apt-get install -y g++-aarch64-linux-gnu
```

拿 SDK 资产（**板子上只有 `librknnrt.so`，没有头文件**）：

| 资产 | 怎么拿 |
|---|---|
| `rknn_api.h` | 从 `rockchip-linux/rknn-toolkit2` 的 `rknpu2/runtime/Linux/librknn_api/include/` 取（**版本要与板端 runtime 对齐**，本板是 2.3.0） |
| `librknnrt.so` | 从板子 `/usr/lib/librknnrt.so` 直接拉（7.2 MB），交叉链接只能用这份，链接器看不到目标机上的库 |

配置与编译：

```bash
cmake -S . -B build_rknn \
      -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.toolchain.cmake \
      -DEVS_BACKEND=rknn \
      -DRKNN_API_LIB=/home/xp/rknn_sdk/lib/librknnrt.so \
      -DRKNN_API_INCLUDE_DIR=/home/xp/rknn_sdk/include
cmake --build build_rknn -j4
```

产出的 `build_rknn/eval_offline`、`build_rknn/evs_tests` 是 aarch64 ELF，
直接 scp 到板子即可（板端 `/usr/lib` 已有 `librknnrt.so`，无需再带运行库）。

## 3. 板端验证

```bash
# 数据集与 PC 端完全同一份（eval_onnx_demo/）
./eval_offline --task configs/eval_rknn.yaml --dataset ./eval_onnx_demo \
               --iou 0.5 --score 0.25 --out ./out
```

`configs/eval_rknn.yaml` 只需把 `model` 指到板上的 `.rknn`；解码方式、阈值、
输入尺寸与 `configs/eval_onnx.yaml` 一致，保证两边可比。

## 4. 实测结果（2026-10-07，TL3576-EVM / RK3576）

模型：`/root/cameras/yolov5s-640-640_rk3576.rknn`（int8，3 输出头 `[1,255,80/40,20]`）
图片：`bus.ppm` 640×640（`yolov5` 经典街景，含人）

| 指标 | PC（ONNXRuntime，fp32 头） | 板端（RKNN，int8 头） |
|---|---|---|
| 单测 | `evs_tests` **161/0** | `evs_tests` **137/0**（少的是 x86/ONNX 专属段） |
| micro precision | **1.000** | **1.000**（0 误检） |
| micro recall | **1.000** | 0.667 |
| mAP@0.5 | **1.000** | 0.667 |
| 单帧 NPU 耗时 | — | **≈31 ms**（`rknn_run`+取输出+解码，稳定 5 次一致） |

逐框对比（`EVS_YOLO_DUMP=1` 导出两侧检测框后按 IoU 配对）：

| PC 框 conf | 最近的 NPU 框 conf | IoU |
|---|---|---|
| 0.890 | 0.886 | **0.894** |
| 0.889 | 0.890 | **0.938** |
| 0.847 | 0.834 | **0.844** |
| 0.394 | 0.393 | **0.770** |
| 0.083 | 0.080 | **0.846** |
| 0.083 | 0.083 | **1.000** |

- 匹配上的 6 个框平均 IoU **0.88**；
- **NPU 没有任何 PC 侧不存在的框**（零误检）；
- 差的那 2 个是 bus.jpg 里只露出上半身的行人，PC 侧输出高瘦框（h≈0.95），
  int8 模型把它们并进了相邻的大框 → 表现为召回少 2、但精度不掉。

## 5. 三个必须知道的坑

### 5.1 RKNN 模型可能**已经融合了 Detect 后处理**

本板 `yolov5s-640-640_rk3576.rknn` 的输出**不是原始 logits**：抽查三个头的首 6 个
值分别是 `0.5751 0.4929 0.4029 0.4577 0.4968 0.5164`，而同一张图 ONNX 原始 logits 是
`0.2785 -0.1032 -0.4313 -0.2102 -0.0195 0.0614` —— 前者正好是后者的 sigmoid。
如果按原始 logits 再 sigmoid 一次，`obj≈sigmoid(0.5)=0.62`，**25200 个格子全部成为候选**，
NMS 后仍然几百个误检（实测 P=0.011）。

因此 `yolo_decode.h` 里有 `HeadMode`：

| 模式 | 含义 |
|---|---|
| `kRaw`(0) | 原始 logits：obj/cls 走 sigmoid，尺寸走 `exp()`（ONNX/PyTorch 标准导出） |
| `kClsAct`(1) | 只融合了 obj/cls 的 sigmoid |
| `kAllAct`(2) | 整个 Detect 融合：中心 `(2σ-0.5+grid)*stride`，尺寸 `(2σ)²*anchor` |
| `kAuto`(3) | **默认**：按取值分布自动判断 |

自动判据（实测可靠）：原始 logits 头只有约 **2%** 的值落在 `[0,1]`（均值 -5.6），
融合后的头 **~100%** 落在 `[0,1]`。所以 `>=0.9` 即判为 `kAllAct`。
需要强制时：`EVS_YOLO_HEAD_MODE=0|1|2`（PC 与板端通用）。

### 5.2 `exp()` 会炸出退化框，必须加合理性过滤

原始 logits 尺寸项是 `exp(tw)*anchor`，`tw` 稍大就是整幅图高的框。这类框能活过 NMS，
把 precision 拉垮。`yolo_decode.h` 里统一加了门槛（两条路径都生效）：

```cpp
if (!(bw > 0.f && bh > 0.f) || bw > kMaxBoxSize || bh > kMaxBoxSize) continue;  // 1.2（归一化）
```

加上之后 PC 侧从 8 个"GT"（其中 4 个是 exp 溢出产生的 h>1 垃圾框）收敛到 **6 个干净框**，
`tools/eval_offline/gen_selfcheck_dataset.py` 同步加了同样的过滤，基准才自洽。

### 5.3 头文件/API 细节

- `rknn_init()` 收的是**模型内存**，不是路径；必须先读文件再传指针+长度。
- `rknn_outputs_release(ctx, n_outputs, outputs)` —— 2.x 是三参数，不是 `(ctx, extend)`。
- 取名用 `Session::GetInputNameAllocated` 风格 → `rknn_query(RKNN_QUERY_INPUT_ATTR/OUTPUT_ATTR)`
  填 `index` 后查询，返回的是统一的 `rknn_tensor_attr`（`dims[]` 是逻辑 NCHW 顺序）。
- 输入属性本板是 `type=int8 fmt=NHWC`：此时**直接把 uint8 缓冲交给 runtime**
  （`pass_through=0`，由驱动做布局/量化转换）。`EVS_RKNN_INPUT_SCALE` 只对
  **float32 输入**的模型有意义（0..255 或 0..1），对量化模型无效。
- 板端 root 密码是**空的**（`paramiko` 用空密码才能连），建议 `passwd root` 设强密码。

## 6. 环境开关速查

| 变量 | 作用 |
|---|---|
| `EVS_YOLO_HEAD_MODE=0\|1\|2` | 强制解码模式（默认自动） |
| `EVS_YOLO_DUMP=1` | 打印解码后的每个框（cls/conf/cx/cy/w/h），用于 PC↔NPU 逐框比对 |
| `EVS_RKNN_INPUT_SCALE` | 仅 float32 模型输入：0.00392157 = 0..1，1.0 = 0..255 |
| `EVS_RKNN_DEBUG=1` | 打印每个输出头的 dims 与前 6 个值 |
| `EVS_NPU_CORE_MASK` | NPU 亲和（0 自动 / 2 core1 / 4 core2 / 3 core0+1 / 7 三核） |

## 7. 接真实摄像头（/dev/video0）跑现场检测

`tools/v4l2_detect` 是**取景验证工具**：V4L2 取帧（或读 v4l2-ctl 抓下来的裸帧）
→ 与生产完全相同的管线（letterbox → NPU → 解码 → 映射回原图坐标）→ 打印检测框、
各阶段耗时，并可存一张画框的 PPM 快照。

```bash
# 板端
v4l2-ctl -d /dev/video0 --set-fmt-video=width=1280,height=720,pixelformat=NV12
v4l2-ctl -d /dev/video0 --stream-mmap=3 --stream-count=1 --stream-to=/root/evs/cap.nv12

./v4l2_detect --task configs/eval_rknn.yaml \
               --raw /root/evs/cap.nv12 --raw-width 1280 --raw-height 720 \
               --frames 1 --score 0.25 --save /root/evs/shot.ppm
```

2026-10-07 实测（TL3576-EVM，`/dev/video0` = `stream_cif_mipi_id0`，NV12 1280×720）：

```
[raw] /root/evs/cap.nv12 1280x720 NV12 (1382400 bytes)
frame 4: 1280x720 -> model 640x640, targets=1, pre=50.98ms infer=27.64ms total=78.62ms
  class=0 conf=0.773  box(px)=[540 510 377 315] norm=[0.4219 0.7084 0.2948 0.4370]
  npu run = 28599 us
avg over 5 frames: pre=51.69ms infer=29.40ms total=81.09ms (12.3 fps)
```

画面里是一名戴眼镜的人，模型正确框出（快照 `reports_rknn/shot_camera_1280x720.png`）。

### 性能：预处理优化（已落地，2.1×）

`Preprocessor` 原本是逐像素浮点双线性（92 万像素 × 3 通道），单帧 **109 ms**，
比 NPU 推理（30 ms）慢 3 倍，是实时瓶颈。三处优化后降到 **51.7 ms**：

| 优化 | 说明 | 效果 |
|---|---|---|
| 定点双线性 | 6-bit 定点权重（`>>6`）+ 每行 x 映射缓存，替代浮点乘加 | 109 → 70 ms |
| letterbox 行裁剪 | 640×640 里上下 280 行是纯 padding，直接 `memset(114)` 不参与插值 | 与上一项叠加 |
| NV12→RGB 查表 | BT.601 的 9 张 256 项 LUT，每像素 3 查表 + 2 加法，取代 8 次乘法 | 70 → **51.7 ms** |

整帧 **144 ms → 81 ms（6.9 → 12.3 fps）**；检测结果与优化前一致（框 `[540 510 377 315]`，
conf 0.773，定点舍入带来 ≤1 px / 0.05 conf 的微差），板端单测 `PASS=137 FAIL=0`、
离线评测 P=1.000 / R=0.667 均未变化。

### RGA 硬件缩放：接口已就绪，本板驱动暂不接受

宿主侧模块 `tools/v4l2_detect/rga_preprocess.{h,cpp}` 已完成（CMake 选项 `EVS_WITH_RGA`，
工具参数 `--rga` / `--bench`），letterbox 语义与库内 CPU 版完全一致，失败自动回退 CPU。
**但在这块板子上拿不到加速**，原因都是驱动/环境约束：

1. **RGA2 只接受 dma-buf**。用虚拟地址提交任务时驱动直接报
   `librga only supports the use of handles only or no handles`，
   `imresize` / `imcvtcolor` 全部 `Invalid argument`。这与设计文档一致
   （RGA 的输入输出必须是 dma-buf，不能是 malloc 堆内存）。
2. 板端**没有 ION、没有 GBM**；用 DRM dumb buffer（`CREATE_DUMB` + `PRIME_HANDLE_TO_FD`）
   能拿到 dma-buf fd，但用该 fd 提交 job 仍被拒（`RGA_BLIT fail: Invalid argument`）。
3. `imfill`（RGA_COLORFILL）对 RGB888 被拒 → 灰边 padding 只能 CPU `memset`（约 0.1 ms）。

**下一步**：产品宿主（`camera_shm_multi`）的帧本身就是 dma-buf，接上后 `--rga` 可直接启用；
若要自行分配，需要对齐 Rockchip 官方 allocator（`external/linux-rga/samples/allocator_demo`）。

**编译注意**：RGA 头必须与库版本匹配 —— 板端 `librga.so.2.1.0` 对应 SDK 的
`librga-dev_2.1.0-1_arm64.deb`（单头 `im2d.h` + `rga.h` 里的 `RK_FORMAT_*`）；
SDK `external/linux-rga` 的头是新版 extern "C" API，与 2.1.0 库**不兼容**（链接报
undefined reference）。librga 还需链接 `libdrm`（`drmIoctl`/`drmPrimeHandleToFD`），
交叉编译时用 aarch64 目标 sysroot 的 `libdrm.so`，别用板端那份（glibc 版本更高）。

### 已知限制

- 编译机 videodev2 头与运行内核的 `struct v4l2_format` 布局对不上，ioctl 号编解码不一致 →
  `VIDIOC_S_FMT_MPLANE` / `VIDIOC_G_FMT_MPLANE` 返回 `EINVAL`；板子上厂商自带的
  `v4l2-ctl` 抓帧正常，所以用 `--raw` 模式（`--device` 留给头文件与目标板一致的场景）。
  另外 `VIDIOC_S_FMT_MPLANE` 的方向是 **`_IOW` 而不是 `_IOWR`**，写错会让 ioctl 号变一个值；
  `v4l2_buffer.offset` 在老内核头里位于具名 union `m`（`b.m.offset`）。