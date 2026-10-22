# Ubuntu板部署、交叉编译与ABI说明

板端是Ubuntu，可以原生编译，也可以在PC上使用匹配目标Ubuntu的工具链和sysroot交叉编译。开发机修改源码不会自动更新板上程序；PC x86_64产物不能直接放到ARM板运行。

## 1. 冰雪测试路径：Ubuntu原生编译

在目标Ubuntu板的仓库根目录执行。依赖安装、完整命令及摄像头/PLC验收见[冰雪说明](winch_ice.md)。已有OpenCV/GStreamer开发依赖时：

```bash
cmake -S . -B build_ice_ubuntu -DCMAKE_BUILD_TYPE=Release \
  -DEVS_BACKEND=mock -DEVS_WITH_OPENCV=ON -DEVS_WITH_GSTREAMER=ON
cmake --build build_ice_ubuntu -j2
ctest --test-dir build_ice_ubuntu --output-on-failure
cmake --install build_ice_ubuntu --prefix "$PWD/package_ice"
file package_ice/lib/libevs_core.so.3 package_ice/lib/libevs_ice.so.3
ldd package_ice/bin/ice_replay
ldd package_ice/bin/winch_ice_demo
```

这里mock构建只用于固定区域/人工框测试。正式卷扬模型模式需EVS_BACKEND=rknn和匹配RKNN SDK，完整命令见[冰雪说明](winch_ice.md)。摄像头demo另需GStreamer及设备插件；缺少开发依赖时CMake可能省略该工具，应核对构建目标和安装产物。若修改了CMAKE_INSTALL_LIBDIR，请相应调整上面的lib路径。

OpenCV关闭时，冰雪初始化明确返回NotSupported。当前安装规则收集核心库和冰雪库、匹配头文件、冰雪工具、配置及资料；其他插件不在该安装集合内，不能把冰雪包当成所有功能插件的完整产品包。

## 2. 模型后端：交叉编译

工具链、RKNN头文件/运行库以及OpenCV、GStreamer等依赖必须匹配目标Ubuntu的架构和运行时。不能仅凭同为aarch64，就认定Buildroot与Ubuntu的库兼容。

仓库提供cmake/aarch64-linux-gnu.toolchain.cmake，使用发行版的aarch64-linux-gnu工具链；实际SDK路径和查找根需要按现场环境配置。以下是**RKNN基础库、不启用OpenCV或摄像头工具**的示例，替换其中SDK路径后执行：

```bash
cmake -S . -B build_board \
  -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.toolchain.cmake \
  -DEVS_BACKEND=rknn \
  -DRKNN_API_LIB=/path/to/ubuntu-target-sdk/lib/librknnrt.so \
  -DRKNN_API_INCLUDE_DIR=/path/to/rknn/include \
  -DEVS_WITH_OPENCV=OFF -DEVS_WITH_GSTREAMER=OFF -DEVS_WITH_RGA=OFF \
  -DEVS_BUILD_TESTS=OFF -DEVS_BUILD_TOOLS=OFF
cmake --build build_board -j4 --target \
  evs evs_shared evs_person_shared evs_height_limit_shared evs_ice_shared
```

这条命令会生成冰雪插件文件，但OpenCV关闭时不具备冰雪面积检测能力。若需在同一RKNN构建中启用冰雪，应打开EVS_WITH_OPENCV，并提供目标OpenCV core/imgproc头文件与库；不能链接开发机OpenCV。目标SDK/sysroot尚未核对时，先使用第1节的Ubuntu原生构建验证冰雪。

RKNN运行库应取自目标板匹配版本，头文件与运行库也需配套。历史联调记录中的固定路径、文件体积或版本号不表示当前目标板状态。仅替换工具链路径不会自动补齐sysroot的全部依赖。

首次配置后，修改源码可直接cmake --build。更换工具链、架构或sysroot时使用新的构建目录，避免沿用旧缓存和目标文件。其他功能的后端与模型验证见[rknn_backend.md](rknn_backend.md)。

## 3. 为什么库文件从.so.1变为.so.3

变化的是**ABI版本后缀**，不是功能模块号、算法准确率版本或目标CPU型号。当前CMake中核心库和三个插件的SOVERSION均为3。

| 名称 | 含义 | 本次情况 |
|---|---|---|
| libevs_core.so、libevs_ice.so等 | 无版本链接名，编译宿主时-levs_core、-levs_ice使用它 | 基础名称保留 |
| libevs_core.so.3、libevs_ice.so.3等 | 当前动态库文件名及ELF SONAME，宿主运行时记录并查找它 | 与旧ABI1/2区分 |
| libevs_person.so.3、libevs_height_limit.so.3 | 同一公共接口版本下重编的其他插件 | 与核心库使用匹配版本 |
| libevs.a、libevs_ice.a等 | 静态归档 | 名称不加此版本后缀，但宿主同样需要匹配头文件重编 |

本次升级的依据是实际公开布局和调用接口改变：

- TaskConfig增加冰雪配置、对象清单和运行模式等成员，其对象尺寸和字段偏移与旧版不同。
- IFunctionPlugin增加虚函数，旧插件按旧虚表布局编译，不能假定与新核心库兼容。
- 冰雪公开图像/结果/配置结构及detect接口在开发过程中调整，宿主需要使用当前两参接口和匹配结构。

旧二进制可能仍按旧字段偏移、对象尺寸或虚表位置访问。只把新实现命名为旧.so.1，或建立旧版本名到新库的软链接，不能让这些差异自动消失。因此当前使用新的SONAME，并要求相关宿主和插件成套重新构建。

数字3是本工程当前选择的ABI编号，不是编译器强制必须取3；关键是与不兼容的旧版本区分。后续仅修改内部实现、未破坏ABI时，无需每次递增该编号。这次文档原有“SONAME为2”的残留已统一为3，没有再次更改CMake版本。

拆分名称也需区分历史：本仓库HEAD基准已经生成libevs_core.so和各插件库，本次不是把libevs.so重新命名成libevs_core.so。若其他旧交付说明写的是单库libevs.so.1，应按它对应的源码/宿主版本核对，不能直接套到当前分库构建。

## 4. 宿主链接与部署

动态冰雪宿主的链接示例，假设头文件、库和程序按常见include/lib/bin布局安装：

```bash
c++ -std=c++17 your_app.cpp -Ipackage_ice/include -Lpackage_ice/lib \
  -levs_ice -levs_core '-Wl,-rpath,$ORIGIN/../lib' -o package_ice/bin/your_app
readelf -d package_ice/lib/libevs_core.so.3
readelf -d package_ice/lib/libevs_ice.so.3
ldd package_ice/bin/your_app
```

直接引用Detector符号可保留冰雪动态库。仅调用VisionTask、依赖插件静态注册的宿主，应保留其原有--no-as-needed/加载策略；静态消费者则需保留归档登记对象并链接所用第三方库。

部署时保留所需.so.3实际文件及无版本链接名；安装树或打包工具应保留符号链接。核心库、插件和宿主必须来自匹配版本，不能只换一个插件后继续使用旧核心或旧宿主。程序旁放库还需要对应RUNPATH或加载路径，单纯文件放在同目录不保证动态加载器会找到它。

核验至少包括：

- file显示目标架构；目标ARM板应为aarch64，主机验证包则可能为x86-64。
- readelf中的SONAME/NEEDED与预期版本一致；冰雪插件的核心依赖为libevs_core.so.3。
- ldd没有not found，且实际加载的是当前交付目录中的核心与插件，没有误载旧库。
- OpenCV、C++运行库及按构建选择的RKNN依赖在目标板可用；摄像头demo另外具备GStreamer及设备插件。

旧user_evs_demo属于外部宿主工程，不是本仓库本轮已编译的目标。其默认RGA选项、链接路径和PLC接入需要在该工程中同步核验，不能沿用历史路径就声称已验收。

## 5. GStreamer、RGA与验证边界

摄像头工具支持显式GSTREAMER_SYSROOT：指定后仅使用该目录内的GStreamer/glib头文件及库，不回退到开发机pkg-config；未指定时使用pkg-config。交叉编译必须确保它指向目标依赖。v4l2_detect所需zlib也从显式sysroot查找。

RGA是宿主预处理依赖，冰雪像素算法本身不使用它。当前存在RGA自动库查找被预定义空变量跳过的遗留问题，需要显式RGA_LIB及匹配的RGA_API_INCLUDE_DIR；本轮文档更新没有修复该源码分支。libdrm支持DRM_LIB显式覆盖或自动查找。

主机上的CMake桩库检查只验证依赖选择和生成的链接命令，不能证明真实ARM库链接、摄像头采集、NPU/RGA执行或端到端时延已经通过。实际目标板结果应另行记录。
