# RK3588 卷扬冰雪 SDK

源码版本：`d6b63ba`。先按第 3 节运行配套样例，再按第 5 节接入自己的程序。

## 1. 适用范围与验证环境

本 SDK 通过 `evs::winch_ice::Detector` 提供卷扬冰雪单帧检测接口，使用配套的核心库、卷扬冰雪库和 RK3588 模型。输入为紧凑 RGB24 或 8 位灰度图像，返回候选框、卷扬对象、面积占比和报警结果。

**当前尚未接入冰雪模型。`result_code` 暂表示筛选后的卷扬检测状态；`snow_detections` 暂存未经置信度过滤和 NMS 的卷扬候选。冰雪候选面积由卷扬框内的 HSV 颜色统计估算。**

库同步处理调用方传入的一帧图像。相机取流、图像格式转换、工况判断、重试和报警保持由客户程序负责，详细约定见 [API 说明](docs/API.md)。

以下为本包实际编译和验证环境：

| 项目 | 已验证环境 |
| --- | --- |
| 芯片 / 架构 | RK3588 / ARM64（aarch64） |
| 操作系统 | Ubuntu 22.04.5 LTS |
| Linux 内核 | 5.10.209-rk3588 |
| 接口语言 | C++17 |
| 编译器 | g++ 11.4.0 |
| CMake | 3.22.1，用于编译完整 demo |
| glibc | 2.35 |
| OpenCV | 4.5.4，运行依赖 `libopencv_core.so.4.5d`、`libopencv_imgproc.so.4.5d` |
| RKNN Runtime | 板端配套 `librknnrt.so`，文件校验值见 `VERSION.json` |

运行时需要本包的两个 EVS 库、配套模型，以及设备上的 RKNN、OpenCV 和 C/C++ 运行库；系统运行库不随 SDK 打包。直接运行已编译 demo 不需要编译器或 CMake。自行编译示例需要 g++，编译完整 demo 还需要 CMake；示例不需要 RKNN 或 OpenCV 开发头文件。

本包已在 RK3588 上验证，其他芯片或系统环境需单独确认兼容性。版本、依赖文件和校验值统一记录在 `VERSION.json`。

## 2. 交付包与目录结构

客户交付文件为 SDK 的 `.tar.gz` 压缩包及对应 `.sha256` 校验文件。解压后的目录如下；`records/` 只列记录组，组内文件用途见 [运行记录](docs/RUN_RECORDS.md)。

```text
edgevision_winch_ice_sdk_rk3588_d6b63ba/
├── README.md
├── VERSION.json
├── SHA256SUMS
├── bin/
│   └── winch_ice_api_demo
├── configs/
│   └── winch_ice.yaml
├── demo/
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── minimal.cpp
│   ├── recording.cpp
│   ├── recording.h
│   └── sample.ppm
├── docs/
│   ├── API.md
│   └── RUN_RECORDS.md
├── include/
│   └── evs/
│       ├── winch_ice.h
│       ├── alog.h
│       └── alg_api.h
├── lib/
│   ├── libevs_core.so -> libevs_core.so.1
│   ├── libevs_core.so.1
│   ├── libevs_ice.so -> libevs_ice.so.1
│   └── libevs_ice.so.1
├── models/
│   └── winch_model.rknn
├── records/
│   ├── default_run/
│   ├── update_log_run/
│   ├── high_confidence_run/
│   └── minimal_run/
└── source/
    └── edgevision_source_d6b63ba.tar.gz
```


| 路径 | 用途 |
| --- | --- |
| `lib/` | 核心库和卷扬冰雪库；保留 `.so` 到 `.so.1` 的符号链接 |
| `include/evs/winch_ice.h` | 检测接口、配置和结果类型；仅接入检测功能时只需此头文件 |
| `include/evs/alog.h` | 可选日志接口、回调和宏 |
| `include/evs/alg_api.h` | 转引 `alog.h`；现有 demo 使用此入口，编译该 demo 时两个日志头均须保留 |
| `models/winch_model.rknn` | 配套卷扬模型 |
| `configs/winch_ice.yaml` | demo 的示例配置；直接构造 API 配置时可不用 |
| `bin/winch_ice_api_demo` | 已编译的板端示例 |
| `demo/` | 示例源码、CMake 文件和样例图 |
| `docs/` | API 参数、调用时序和运行记录说明 |
| `records/` | 三组完整demo记录及最小示例编译、运行记录；产品程序运行时不需要 |
| `source/edgevision_source_d6b63ba.tar.gz` | 该提交的完整仓库源码，已核对 `.gitignore`，不含工作区新增文件或编译产物 |
| `VERSION.json`、`SHA256SUMS` | 版本、依赖及文件校验值 |

源码包保留该提交内的模型、测试资源和原有文档；其中的配套模型与 `models/` 内容相同。源码未展开，不附带编译目录。源码包内文档保留提交原文，SDK 接入以本目录的说明为准。

## 3. 快速运行

在 SDK 根目录执行。输出目录必须尚未存在：

```sh
# 默认检测
./bin/winch_ice_api_demo configs/winch_ice.yaml demo/sample.ppm --out /tmp/winch_ice_default
# 更新面积阈值，并接入日志回调
./bin/winch_ice_api_demo configs/winch_ice.yaml demo/sample.ppm --area-ratio 0.1 --log-callback --out /tmp/winch_ice_alarm
# 提高置信度阈值，查看筛选结果与完整候选的区别
./bin/winch_ice_api_demo configs/winch_ice.yaml demo/sample.ppm --confidence-threshold 1 --out /tmp/winch_ice_high
```

demo 读取 P6/255 PPM 图片。`sample.ppm` 是 704×576 RGB 样例。直接调用 API 可传紧凑 RGB24 或 8 位灰度缓冲区，不需要 PPM 或 YAML。

每次运行保存输入图、每帧画框图和 JSON、调用记录、配置及日志。失败时保留已生成文件，程序返回非 0。`--repeat N` 可连续处理同一图片 1～10000 次；省略 `--out` 时在当前目录的 `records/` 下创建新目录。

画框图只绘制筛选后的卷扬框，完整候选保存在 JSON 中。绿色表示低于面积阈值，红色表示达到阈值，黄色或 N/A 表示无法估算面积。

默认样例运行成功时，程序退出码为 0，终端包含：

```text
frame=1 result_code=1 alert_level=0 winch_candidates=7146 objects=1
```

输出目录中应有 `frame_000001.png`、`frame_000001.json`、`result.json` 和 `run.log`。图片有卷扬框，JSON 保存完整候选。上述数量仅对应本包样例，不能作为其他图片是否运行成功的判断标准。

## 4. 编译 demo

```sh
sdk_dir="$PWD"
cmake -S demo -B /tmp/winch_ice_demo_build -DEVS_SDK_ROOT="$sdk_dir"
cmake --build /tmp/winch_ice_demo_build -j2
```

编译结果在 `/tmp/winch_ice_demo_build/winch_ice_api_demo`。`demo/main.cpp` 展示接口调用，`recording.cpp/h` 负责读图、绘框和留档，编译时均需保留。demo 只使用包内头文件和两个库，不需要解压算法源码，也不需要 RKNN 或 OpenCV 开发头文件。

## 5. 接入自己的程序

最小示例为 [`demo/minimal.cpp`](demo/minimal.cpp)，只包含 `evs/winch_ice.h` 和 C++ 标准库头文件。它直接构造配置，完成初始化、单帧检测和释放，不依赖 YAML、日志回调或留档代码。

下面在 RK3588 板上编译独立程序，沿用 `bin/`、`lib/` 布局。在 SDK 根目录执行：

```sh
sdk_dir="$PWD"
app_dir="$(mktemp -d /tmp/winch_ice_customer.XXXXXX)"
mkdir "$app_dir/bin"
cp demo/minimal.cpp "$app_dir/main.cpp"
cp -a lib "$app_dir/lib"
g++ -std=c++17 -O2 -I"$sdk_dir/include" "$app_dir/main.cpp" \
  -L"$app_dir/lib" -levs_ice -levs_core \
  -Wl,-rpath,'$ORIGIN/../lib' -o "$app_dir/bin/my_winch_app"
"$app_dir/bin/my_winch_app" "$sdk_dir/models/winch_model.rknn" "$sdk_dir/demo/sample.ppm"
```

成功时打印 `result_code=1 alert_level=0 winch_candidates=7146 objects=1`，随后打印卷扬框、置信度、有效标记和面积占比。该示例只读取包内固定尺寸 PPM 样例；接入相机时，用自己的紧凑 RGB24 或灰度缓冲区填写 `ImageData`，字段见 [API 说明](docs/API.md)。它不画框、不写 JSON；完整运行留档使用前面的 `winch_ice_api_demo`。

最小示例退出码：0=检测完成，1=接口调用失败，2=参数、样例读取等程序错误。客户不应解析终端文字作为业务接口，应直接读取返回结构体。

以上为板端原生编译命令。交叉编译需另配与目标板兼容的 AArch64 工具链、sysroot 和运行库；仅使用 ARM64 编译器不保证 C++ 或 glibc 兼容。

## 6. 部署检查

```sh
sha256sum -c SHA256SUMS
ldd bin/winch_ice_api_demo
```

`ldd` 不应出现 `not found`，两个 EVS 库应来自本包 `lib/`。`.so` 与 `.so.1` 是链接名与实际库的关系，不是重复算法库。RKNN、OpenCV 及 C/C++ 运行库由设备提供，验证环境见 `VERSION.json`。

保留目录层级和符号链接即可搬迁 SDK。YAML 中的模型相对路径按 YAML 所在目录解析；直接构造 `ModelConfig` 时，相对路径按进程工作目录解析。

头文件、核心库和功能库应按同一批次配套使用，不混用不同交付版本。本包公开接口含 C++ 标准库类型，不承诺任意工具链或跨版本 C++ ABI 兼容；更新库时使用配套头文件重新编译客户程序。版本、模型及文件校验值见 `VERSION.json` 和 `SHA256SUMS`。

[API 参数与调用时序](docs/API.md) · [运行记录](docs/RUN_RECORDS.md)
