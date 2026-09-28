# skes-aeb2vcu 项目说明

## 项目位置与用途

- Git 仓库根目录：`/home/tronlong/lyp/code/skes`；项目源码目录：`skes-apps/skes-aeb2vcu`。仓库顶层 `CMakeLists.txt` 进入 `skes-apps/`，后者通过 `add_subdirectory(skes-aeb2vcu)` 纳入构建。本文档存放在独立的知识库目录，不参与程序构建。
- 编写本文档时检出的 Git 分支：`main_ui`，跟踪 `origin/main_ui`。分支名表示当前工作树的代码版本线，不是设备型号或程序配置项。本文档依据该分支当前工作树的源码整理；切换分支后应核对功能、配置和端口。
- `skes-aeb2vcu` 是 SKES 系统中把感知事件转成车辆控制与提示数据的应用。它从本机 SKES 消息总线接收过滤结果及事件，按设备类型生成 CAN 帧；DI 查询线程读取数字输入，用于判断叉车行驶方向。`aeb2vcu` 可理解为“自动紧急制动（AEB）到车辆控制单元（VCU）”的连接模块；实际代码同时处理盲区告警和履带吊配重识别。串口发送接口已实现，但当前主流程没有调用它。
- 设备类型由安装时选择的 `skes-aeb2vcu.ini` 决定：装载机 `wheel_loader`、履带吊 `crawler_crane`、叉车 `forklift`。不同类型共用进程和通信框架，但 CAN 数据内容、周期及配置参数不同。

### 相关技术和名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| Git 分支 `main_ui` | 当前检出的仓库分支；文档描述的是该分支当前工作树，不代表所有分支都有相同实现。 |
| SKES、SEP | SKES 是所属系统；`sep.ini` 是随程序安装的应用启动配置，包含 `enabled=true`、`name=skes-aeb2vcu`。 |
| AEB、VCU | AEB 是自动紧急制动；VCU 是车辆控制单元。本程序把 AEB 告警、使能状态和故障状态转换成面向车辆的控制数据。实际车辆行为还取决于下游 CAN 服务及 VCU。 |
| BSD | Blind Spot Detection，盲区检测。程序处理前、后、左、右方向的 BSD 告警，并在装载机输出中轮流编码声光提示。 |
| CAN、CAN ID、DLC | CAN 是车载总线；CAN ID 标识帧类型，DLC 表示数据字节数。程序把业务状态封装为 8 字节 CAN 数据，再交给本机 CAN 发送服务。 |
| DI、DO | DI 是数字输入；程序读取通道 1，其结果用于叉车行驶方向判断。DO 是数字输出；头文件定义了 AEB 和 SEP 面板控制通道及命令结构，但当前主流程没有发出 DO 命令。 |
| RS-485、RS-232 | 两种串行通信接口；程序初始化了两路本机 nanomsg 发送连接，`com_output` 目前没有调用点。 |
| nanomsg、PUB/SUB、REQ/REP | nanomsg 是进程间通信库。SKES 消息总线和 CAN/串口发送使用发布订阅模式；DI 查询使用请求应答模式。`127.0.0.1` 地址表示同一设备上的本机服务。 |
| cJSON、JSON、INI | cJSON 解析总线事件及 UI 配置中的 JSON；INI 文件保存设备类型、制动参数、识别阈值和日志等级。 |
| CMake、交叉编译 | CMake 定义可执行文件、依赖和安装规则；交叉编译是在开发机上为目标 Linux 设备生成程序。项目主逻辑为 C99，测试目标使用 C++11。 |
| 校验位、生命信号 | 装载机 AEB CAN 数据中的字段：生命信号随发送递增，校验位由数据及 CAN ID 计算；用于接收端识别连续性及校验内容。 |

## 主要文件与数据流

| 路径 | 职责 |
| --- | --- |
| `CMakeLists.txt` | 查找依赖、构建 `skes-aeb2vcu`、按 `SKES_AEB2VCU_TYPE` 选择设备配置并安装程序与动态库。 |
| `main.c` | 初始化日志、处理 `SIGUSR1` 切换日志等级、创建并运行应用实例。 |
| `skes-aeb2vcu.c`、`skes-aeb2vcu.h` | 消息解析、设备状态、故障码、CAN 帧生成、串口连接及工作线程；头文件定义帧结构、设备类型、CAN ID 和端口。串口发送函数目前未被主流程调用。 |
| `di_get.c`、`di_get.h` | 通过本机请求应答接口读取 DI 通道状态。 |
| `skes-aeb2vcu.ini.*`、`sep.ini` | 三种设备配置模板及应用启动配置。安装后设备配置统一命名为 `skes-aeb2vcu.ini`。 |
| `tests/` | Google Test 测试目标和集成测试示例；测试需要依赖项和相应运行环境。 |

主要链路：感知/告警服务 → SKES 本机消息总线 → `on_skes_msg_received` 接收 `filter`、`event` → 按设备类型更新状态 → `do_can_msg_sender` 编码 CAN 帧 → 本机 CAN 发送服务 → 车辆侧。叉车另使用 DI 查询结果判断方向；装载机另读取 UI 使能配置。

### 三种设备的实际处理

| 设备类型 | 输入与输出 |
| --- | --- |
| 装载机 `wheel_loader` | `filter` 中 `type=properties`、`data.type=AEB` 的属性值更新 AEB 控制字段；`event` 中的 `fault` 更新故障码，`alarm` 中的 BSD 更新声光提示方向。读取 UI 开关后，约每 100 ms 发送一次 8 字节 AEB CAN 帧；故障列表按序轮询，每帧带生命信号及校验位。当前定义 `USE_NEWC_VEHICLE`，协议 CAN ID 为 `0x168B9664`，写入本地帧结构的 `can_id` 时还会置位扩展帧标志 `0x80000000`。 |
| 履带吊 `crawler_crane` | 消息类型为 `event`、其中 `source=counterweight-recognition` 时，读取摄像头通道 4、5 的 `texts`；仅采用分数达到 `score_threshold` 且数值大于 0 的识别结果。两路配重相加乘 10，约每 500 ms 发送到 CAN ID `0x600`。 |
| 叉车 `forklift` | `event` 中的 AEB `alarm` 解析前/后触发与等级；DI 通道 1 与 `forward_di_level` 比较判断前进方向。按前后方向及两级制动配置计算制动值，状态降级时可短暂保持上次值（代码中的 1 秒窗口）。电机版本 1 使用 CAN ID `0x440`、约 100 ms 周期；版本 2 使用 `0x120`、约 35 ms 周期。 |

## 构建与部署

- 从仓库顶层 CMake 构建；本目录不是独立完整的构建工程。本目录设置 C99、`debug` 构建类型，查找 `e2fsprogs-git`、`openssl-git`、`curl-git`、`zlib-git`、`cjson-git`、`collections-c-git`、`nanomsg-git`、`skes-log`、`skes-linker-git`，并链接 `skes-utils`、`pthread` 等库。测试子目录还查找 Google Test。工具链与第三方包的准备方式以仓库构建配置为准。
- CMake 参数 `SKES_AEB2VCU_TYPE` 可选 `wheel_loader`、`crawler_crane`、`forklift`；未匹配时安装装载机模板。安装目标为 `apps/skes-aeb2vcu/` 下的可执行文件、`sep.ini` 和重命名后的 `skes-aeb2vcu.ini`，共享库安装到 `lib/`；实际绝对路径取决于顶层安装前缀。
- `USE_NEWC_VEHICLE` 在本目录 CMake 中定义，会影响装载机 AEB 帧的 CAN ID、字段布局和校验相关逻辑。更改车辆协议时应同时核对该宏、`aeb_fault_t` 的 8 字节布局和接收端协议。
- 当前头文件中的应用版本常量为 `1.1.3.0`（宏名沿用 `SKES_RELAY_VERSION_*`），`main.c` 启动时将它写入日志。它与仓库顶层的安装包版本是不同的版本字段。

## 配置、通信与运行关注点

- `skes-aeb2vcu.ini` 的 `[dev] type` 决定运行分支。缺失或无效类型时，实例初值仍是 `wheel_loader`，但加载函数会记录错误。履带吊模板的 `score_threshold=0.85`；叉车模板的 `brake_motor_version=2`，前后一级制动值均为 60、二级均为 40，`forward_di_level=0`。`[log] level` 控制日志级别，三个模板均为 4；`main.c` 启动时读取并写回，日志文件为 `skes-aeb2vcu.log`。
- SKES 消息总线地址由代码设置为 `tcp://127.0.0.1:19225`（发布端连接地址）和 `tcp://127.0.0.1:19226`（订阅端连接地址）。当前消息回调处理 `filter` 与 `event` 两个主题；心跳、版本、设备数据、定位的分支被条件编译关闭。`filter` 仅处理消息类型 `properties` 中的 AEB 属性；`event` 主题处理消息类型 `fault`、`alarm`（AEB/BSD）和 `event`（配重识别）。当前活跃路径主要接收消息，不能把已创建发布句柄理解为正在定期发布状态。
- CAN 发送线程当前连接 `tcp://127.0.0.1:26002`（CAN0 TX）。装载机生成 AEB 控制/故障和 BSD 声光提示帧；履带吊把两个摄像头通道识别的配重相加后写入 CAN ID `0x600`；叉车按前后告警、行驶方向和制动等级生成制动帧。头文件也定义了其他 CAN 收发端口，但当前线程没有连接它们。
- 两路串口发送连接使用 `127.0.0.1:26006`（RS-485）和 `127.0.0.1:26007`（RS-232），但当前主流程未调用 `com_output` 发数据。DI 查询连接 `127.0.0.1:38000`，所有设备类型都会启动 DI 线程并循环读取通道 1；结果仅被叉车制动分支用于方向判断。DI 请求接收超时配置为 5 秒，循环间隔为 1 秒。端口上的服务需由设备其他组件提供。
- 装载机约每 5 秒读取 `/tmp/ui_config.json` 中的 `aebEnabled` 和 `alarm.enabled` 开关；禁用 AEB 时清零相应控制字段，禁用告警时清零声音提示。修改使能行为应同时核对 UI 配置、事件解析及发送线程。
- `skes_aeb2vcu_run` 启动消息轮询、CAN 发送和 DI 查询三个线程。`main.c` 轮询运行标志；`SIGUSR1` 会循环切换日志等级。程序使用相对路径查找 `skes-aeb2vcu.ini` 并写日志，运行时工作目录需要能找到配置且允许写日志。验证车辆输出还需要本机消息、CAN 服务和目标设备环境。
- 当前 `on_msg_filter` 和 `on_msg_event` 即使完成解析也在函数末尾返回 `-1`；排查消息回调错误或重试行为时，应结合 `skes-linker` 的返回值约定核对。这是源码现状，本文档不将其解释为消息未被处理。
- `tests/CMakeLists.txt` 构建 `LoadDeviceTypeTest` 和 `Aeb2VcuTest`，但 CTest 只注册了前者。更重要的是，`LoadDeviceTypeTest` 虽然如此命名，实际模拟的是 `[log] level` 读取函数，既未调用真正的 `load_device_type`，也未覆盖 CAN 协议或车辆行为；`Aeb2VcuTest` 是需连接本机事件端口的手工消息发送示例。协议改动应结合目标环境验证。
