# RKNN 后端（`src/backend/rknn/`）—— 真机对接桩

本目录是 EdgeVision 在 **RK3588 / RKNN SDK 2.x** 上的推理后端实现，仅在
`EVS_BACKEND=rknn` 时编译进 `evs` 库（见顶层 `CMakeLists.txt`）。

## 构建

```bash
# 在 RK3588 开发机（Ubuntu 20.04，已装 RKNN SDK 2.1）上：
cmake -DEVS_BACKEND=rknn \
      -DCMAKE_PREFIX_PATH=/usr/lib   # 含 librknn_api.so 的路径
      ..
make
ctest
```

CMake 会 `find_library(rknn_api)` 并 `target_link_libraries(evs PUBLIC rknn_api)`。
若 SDK 不在默认路径，用 `-DRKNN_API_LIB=/path/to/librknn_api.so` 指定。

## 数据流约定

```
宿主 Frame (NV12/RGB/BGR, 1920x1080)
   -> Preprocessor  (letterbox/stretch + 颜色转换, 纯 C++, 输出 NHWC uint8)
   -> RknnBackend::infer(uint8 输入张量)
   -> rknn_inputs_set / rknn_run / rknn_outputs_get(want_float=1)
   -> std::vector<float> 输出张量
   -> IDetectionDecoder 解析为 Target (0..1, 模型输入坐标)
   -> map_target_to_original() 回映到原图坐标
```

- **输入**：直接喂 `Preprocessor` 产出的 uint8 `NHWC` 张量（RGB 或 BGR，由
  YAML `input_format` 决定）。INT8 模型吃 uint8，量化参数由模型自带，库不做归一化。
- **输出**：`rknn_outputs_get` 带 `want_float=1`，SDK 直接反量化为 `float`，
  后端按输出顺序拼接成 `std::vector<float>` 交给解码器。

## 联调 TODO（真机落地必须逐项确认）

1. **张量布局 / 轴序**：YOLOv8 经 RKNN 导出的输出可能是 `[1, N, 4+C]` 或
   `[1, 4+C, N]`，且通道轴位置因 export 工具而异。`RknnBackend` 目前按内存
   顺序直接拼接；若真模型轴序不同，需在此处做 permute，或在 `IDetectionDecoder`
   里适配。先 `rknn_dump_tensor` / 打印 `rknn_tensor_attr` 确认。
2. **输入 layout**：假设 `NHWC`。若模型要求 `NCHW`，需在 `Preprocessor` 输出后
   转置，或让后端按 `in_attr.fmt` 转换。当前 `rknn_input.fmt` 直接透传模型声明。
3. **NPU 核心亲和**：`npu_core_mask` 暂从环境变量 `EVS_NPU_CORE_MASK` 读取
   （0/2/4/3/7 → 核 0/1/2/0+1/0+1+2）。生产版应把 YAML `npu_core_mask` 透传进
   `IInferBackend::load`（需扩展 `load` 入参或加 setter）。
4. **预热 / 超时**：`TaskConfig.warmup` 已在 `VisionTask::initialize` 用零张量跑一次
   哑推理；`init_timeout_s` 目前未传给 `rknn_init`（SDK 无阻塞式超时，靠外部 watchdog）。
5. **量化模型路径**：YAML `model:` 指向 `.rknn`（INT8）。x86 验证仍用 `mock` 后端。

## 与 mock 后端的关系

`mock` 后端在 x86 上零依赖编译，跳过真实推理（`infer` 返回空张量），用于把
"预处理 → 推理(空) → 解码 → 策略" 全链路跑通与单测。换 `EVS_BACKEND=rknn` 即可
切换到本真机后端，框架其余代码（VisionTask / 解码器 / 策略 / 几何）完全不变。
