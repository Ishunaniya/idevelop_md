# doubiche_car_fei_3d 开发与维护指南

## 1. 文档范围与路径

本文件根据 `/home/tronlong/lyp/code/doubiche_car_fei_3d` 的实际代码、工程文件、配置及现有 HMI 文档整理。初始化核查日期：2026-10-15。

- **源码目录**：`/home/tronlong/lyp/code/doubiche_car_fei_3d`。
- **本文档目录**：`/home/tronlong/lyp/perCode/idevelop_md/rtms_sdk/Haidexin/doubiche_car_fei_3d`。
- **设备上的既有部署目录**：代码多处使用 `/root/car_fei_3d`，它与上述两个宿主机目录不同。
- 本次初始化只生成本文件，没有复制源码、初始化新的 Git 仓库或修改源工程。
- 下文未加绝对路径的源码文件名，均相对于**源码目录**。执行构建、搜索和运行命令时先确认工作目录，不能直接在本文档目录运行 `make`。
- 初始化时源码目录没有 `.git`，执行 `git rev-parse --show-toplevel` 返回非 Git 仓库；本文档目录则位于上层 `idevelop_md` Git 仓库中。后续提交前分别确认所属仓库，不要把文档仓库状态当成源码版本状态。
- 本文件放在文档目录，并不会自动成为源码目录的祖先级指令。处理源码任务时应主动参照本文，并检查实际工作目录下是否另有适用的 `AGENTS.md`。
- 代码与本文发生差异时，以当前执行路径和实现为准，并同步维护本文。不要把历史注释、旧示例或生成文件当成最新接口说明。

## 2. 项目用途与技术基线

这是海德馨车辆的 Qt Quick/C++ 图形终端，窗口标题为“车载物联网智能终端”。功能包括车辆与底盘工况展示、作业统计、传感器和通信状态、告警查询、维保记录、终端信息、视频播放及云台控制。

- 主工程：`car_fei.pro`；可执行程序名：`car_fei`；构建系统为 **qmake + make**。
- C++ 后端使用 Qt QObject、属性、信号槽、串口和线程；前端使用 QML、JavaScript 和 Qt Quick LocalStorage。
- 保存的 `Makefile` 由 **Qt 6.5.3 / qmake 3.1** 生成，记录的 qmake 路径是 `/usr/local/Qt-6.5.3/bin/qmake`。
- 主页面和视频弹窗导入 `QtMultimedia 6.0`；许多基础组件仍使用 `QtQuick 2.14/2.15`、`QtQuick.Controls 2.14/2.15`。不能仅凭这些 2.x 导入号把项目认定为 Qt 5 工程。
- `car_fei.pro` 写有 `CONFIG += c++11`，但保存的 Qt 6 构建缓存来自 GCC 13.3，编译器默认标准为 C++17。选择工具链时核对实际 Qt 和编译标准，不要承诺纯 C++11 环境可构建。
- `CONFIG += static` 不代表最终程序完全静态链接。`file` 已确认保存的 `car_fei` 为动态链接的 **Linux ARM64 / AArch64 ELF**；根目录的海康及 OpenSSL 库样本也是 ARM64。
- 默认窗口大小为 **1280 × 800**，页面大量使用固定尺寸和绝对位置。UI 改动需在该分辨率下检查布局、触摸区域、键盘遮挡及弹窗。
- `CAR/CarWithSmoke.qml` 使用 Qt Quick 粒子效果；不要由目录名或项目名推断存在 Qt Quick 3D 模型或专门的 3D 引擎。

初始化环境未找到 `qmake`、`qmake6`、`qmllint`、`qmlformat` 或 `rcc` 命令，也未发现 `/usr/local/Qt-6.5.3`。本文中的完整构建和设备运行流程是供具备对应环境时执行的步骤，本次未完成编译或板端运行验证。

## 3. 文件与模块导航

| 路径 | 作用与阅读重点 |
| --- | --- |
| `main.cpp` | 应用启动、字体、GPS 和 ping 线程、QObject 上下文、QML 类型注册、工况初值、云台配置、缓存清理开关 |
| `main.qml` | ApplicationWindow、StackView 导航、全局作业统计、告警数据库信号接收、背景和导航按钮 |
| `VehicleData.h/.cpp` | 工况结构、Q_PROPERTY、JSON 解析、50001 状态订阅、网卡 IP 查询、历史 UDP 辅助代码 |
| `HmiWorkStatsEcho.h/.cpp` | 单调时钟、接收 QML 统计快照、三行文本导出、JSON 持久化与启动恢复 |
| `TerminalData.h/.cpp` | 终端名称、型号、程序版本、出厂日期和设备编号 |
| `RecordVehicle.h/.cpp` | 车辆档案，读取 `vehicleInfo` 下的名称、车牌、底盘号、启用日期、车型 |
| `SystemInfo.h/.cpp` | 每 10 秒通过外部命令读取 CPU、内存及磁盘信息 |
| `FileReader.h/.cpp` | QML 文件读取与写入；写入失败时还有 `/bin/sh` 回退路径 |
| `CppNanoMsg.h/.cpp` | 提供给 QML 的 nanomsg PUB/SUB 封装；与 VehicleData 自己的订阅实现分开 |
| `GPSReceiver.h/.cpp` | `/dev/ttyS9` 串口、GPGGA 解析、全局 GPS 数据及超时标志 |
| `VideoUrl.h/.cpp` | 读取视频源、方向名映射、主码流与预览子码流 URL 选择 |
| `PtzBridge.h/.cpp` | 暴露给 QML 的 ONVIF 云台接口、ready/lastError 和命令结果信号 |
| `OnvifPtzController.h/.cpp` | curl + OpenSSL 实现的 ONVIF SOAP、认证、Profile Token、连续移动和停止 |
| `CameraHkController.h/.cpp`、`CameraLoginWorker.h` | 既有海康 SDK 登录和云台控制实现，启动实例化路径当前被 `#if 0` 禁用 |
| `PingHelper.h` | 摄像头地址提取、ping 辅助类及常驻 PingThread；断网时会终止本进程 |
| `AlarmDatabaseHandler.h/.cpp` | 将数据库初始化/插入请求通过信号交给 QML，未直接用 C++ 操作 SQLite |
| `AppController.h` | QML 退出接口；`exitApp()` 首先调用 `std::exit(-1)` |
| `HMI/MainPage.qml` | 当前主页面：仪表、环境值、作业统计显示、一路视频预览、弹窗和 ONVIF 方向按钮 |
| `HMI/AlertDisplay.qml` | 告警列表、时间范围和类型筛选；数据库另有详情/删除接口，当前页面未接出对应操作 |
| `HMI/CarPage.qml` | 车辆档案和维保记录，包含延迟数据库初始化逻辑 |
| `HMI/DevicePage.qml` | 终端信息、部件状态、起升状态、GPS 展示、5555 辅助订阅和 SensorStatus 文件读取 |
| `HMI/SettingsPage.qml` | 字体、定位使能、阈值、通信参数及虚拟键盘相关设置 |
| `HMI/Dialog.qml` | 放大视频窗口、播放错误处理及历史海康触摸云台调用 |
| `HMI/Dashboard.qml`、`NavButton.qml` | 顶栏、网络信号、时间及底部导航组件 |
| `HMI/Meter.qml`、`SpeedMeter.qml`、`EnvDisplay.qml` | 仪表与环境显示组件 |
| `HMI/DatePicker.qml`、`ThresholdControl.qml`、`FontSettings.qml` | 日期选择、阈值和字体组件 |
| `HMI/*.js` | 告警、阈值、维保、网络 IP 等本地数据库访问逻辑 |
| `data/` | 工况启动样例、终端/车辆档案、摄像头配置、作业统计持久化样例 |
| `qml.qrc` | QML、图片和字体的资源清单；本次核查 66 项均存在 |
| `image/`、`images/`、`fonts/`、`CAR/assets/`、`sounds/` | 图像、字体、粒子和音频资源；`image` 与 `images` 是两个不同目录 |
| `HCNetSDK.h`、`HCNetSDKCom/`、根目录厂商 `.so` | 厂商 SDK 头文件与运行组件，按目标架构配套使用 |
| `precompiled.h` | 公共 Qt 头文件集合；当前主 `.pro` 未配置 `PRECOMPILED_HEADER`，不能仅凭文件名认定启用了预编译头 |

主 `.pro` 当前列出 14 个 C++ 源文件和 17 个项目头文件；这里的清单不包含 Qt 自动生成文件和厂商大头文件。完整入口核对应覆盖这 14 个源文件，而不是只看 main.cpp。

### 历史入口与文档

- `qml_gstreamer.pro`、`playercontroller.cpp/.h`、`only_video_main.qml` 是历史视频实验相关文件，不是当前主程序入口。旧 `.pro` 引用了多个 `/home/topeet/...` 路径以及当前目录缺失的 `VideoSurface.h`，不能当成可直接构建的备用工程。
- `HMI/MainPage2.qml`、`CAR/CarWithSmoke.qml_bk` 不是当前 StackView/资源清单的主入口。改页面前先追踪 `main.qml` 和 `qml.qrc`，避免只修改副本。
- 根目录 `AlarmDatabase.js` 与 `HMI/AlarmDatabase.js` 并存，当前 `main.qml` 导入后者；两者建表时间戳类型也不同。告警功能修改应优先定位实际 import。
- `HMI/` 下有文件名显示异常的 Markdown，内容包括页面数据元素清单和《HMI 优化与稳定性记录》。可通过 `rg --files HMI -g '*.md'` 定位，不要擅自批量改文件名或编码。
- 历史稳定性文档提到的 `hasAnyDialogOpen()`、播放回调修复等细节与当前源码不完全一致。当前可确认同名弹窗去重和放大前释放小窗；额外保护必须查看实现后再判断。

## 4. 启动流程与 C++/QML 接口

主要启动路径如下：

1. 设置 `QT_IM_MODULE=qtvirtualkeyboard`，创建 `QGuiApplication`，打开 `QML_XHR_ALLOW_FILE_READ=1`。
2. 尝试向 `/sys/class/backlight/backlight/brightness` 写入 `100`，启动摄像头连通性 PingThread。
3. 设置 OpenGL ES 3.2 格式并加载资源中的字体，构造 GPSReceiver。
4. 加载车辆档案、相对路径 `data/terminal_info.json` 和 `data/sources.json`，注册 QML 类型。main.cpp 的局部 TerminalData 只用于启动读取/打印；DevicePage 另建 TerminalData 实例并用绝对路径读取，两者不是共享实例。
5. 创建 QQmlApplicationEngine、PTZ 桥接、告警信号桥接与 VehicleData。若 `/tmp/realtime_status.json` 缺失，先尝试从部署目录中的备用 JSON 复制，再加载工况初值。
6. 创建系统信息、文件访问、作业统计及视频源上下文，设置离线存储根路径为 `./`。
7. 使用 `engine.load(QUrl::fromLocalFile("main.qml"))` 加载**磁盘上的 main.qml**，随后发出告警数据库初始化信号，进入事件循环。

| QML 名称 | C++ 对象/类型 | 约定 |
| --- | --- | --- |
| `vehicleData` | VehicleData 实例 | 主工况属性，通过 `dataUpdated()` 通知 |
| `RecordVehicleData` | RecordVehicle 实例 | 首字母大写是现有公开名称，不能随意改为小写 |
| `systemInfo` | SystemInfo 实例 | CPU/内存/磁盘展示 |
| `fileReader` | FileReader 实例 | 文件读写 |
| `hmiWorkStatsEcho` | HmiWorkStatsEcho 实例 | `setSnapshot()` 和 `steadyClockMs()` |
| `videoUrl` | VideoUrl 实例 | 视频源与预览 URL |
| `ptzBridge` | PtzBridge 实例 | `initialize()`、`startMove()`、`stopMove()` |
| `alarmDatabaseHandler` | AlarmDatabaseHandler 实例 | `initDatabaseSignal`、`insertAlarmSignal` |
| `Terminal 1.0 / TerminalData` | 注册类型 | 由 QML 实例化并加载终端档案 |
| `NanoMsg 1.0 / CppNanoMsg` | 注册类型 | 页面自己的 PUB/SUB 对象 |
| `AppController 1.0 / AppController` | 注册类型 | 应用退出 |

新增或重命名字段时，联动检查 C++ 成员、解析映射、getter、`Q_PROPERTY`、NOTIFY 信号、QML 绑定和样例 JSON。已有结构体属性不等于所有成员都自动成为可访问的 QML 属性，应参照已有标量属性的暴露方式。

虽然编译了 `qml.qrc`，当前 QML 主入口、部分配置和图片依赖磁盘及工作目录。发布时仅替换可执行文件，未必会更新实际显示的页面；只改磁盘 QML，也不等于更新了嵌入资源。

## 5. 实时数据链路与字段规则

### 当前主链路

```text
外部 vehicleDataEngine
  ├─ /tmp/realtime_status.json → 启动时 VehicleData::loadJson()
  └─ nanomsg tcp://127.0.0.1:50001
       → VehicleData 后台接收线程
       → QMetaObject::invokeMethod(..., Qt::QueuedConnection)
       → 主线程 parseJson() → dataUpdated() → QML
```

- 本目录没有 `vehicleDataEngine` 的实现，CAN/Modbus 采集与报文转换不能直接归因于这里的 UI。
- `/tmp/realtime_status.json` 是当前启动初始化来源；运行中手动修改文件不会自动触发主工况更新。`VehicleData.cpp` 顶部关于持续读文件的旧注释已不能完整描述实际行为。
- VehicleData 的 3 秒定时器只更新网卡 IP。UDP `127.0.0.1:9001` 相关函数仍存在，但当前构造和更新路径使用 50001 订阅，不要将 UDP 作为默认实时链路。
- 后台接收后必须保持主线程更新 QObject 属性；`parseJson()` 成功后已经发送一次 `dataUpdated()`，不要在订阅回调再次发送造成重复刷新。

### 其他输入通道

| 输入 | 当前用途 |
| --- | --- |
| `tcp://127.0.0.1:6666` | MainPage 的辅助订阅，解析“功率输出”“转速”“机油压力”“冷却液温度”等中文键；部分仪表仍会被此通道赋值 |
| `tcp://127.0.0.1:5555` | DevicePage 的辅助 GPS 订阅，当前主要解析和打印；经纬度展示保持 `vehicleData` 绑定 |
| `/tmp/SensorStatus` | DevicePage 每 3 秒读取的 JSON，含 noise/temp1/temp2/smoke 和部件状态；当前仅更新 sensorStatuses 缓存，未见控件消费，不能由文件更新推定显示已接通 |
| `/dev/ttyS9` | GPSReceiver，9600 波特率、8N1、无流控；识别 `$GPGGA`，有效性超时为 300 秒 |

不要把 50001、5555 和 6666 合并描述成同一数据接口。出现数据“跳回去”或仪表显示冲突时，检查所有对该控件赋值的路径。避免直接给已有绑定的 `Text.text` 赋值而破坏绑定。

GPSReceiver 将 GPGGA 中的数值及南北/东西符号写入 `globalGPSData`；当前解析没有完成度分到十进制度的转换。不要仅凭类名声称它输出标准十进制度坐标，也不要将其与页面显示的 vehicleData GPS 数据混为一条路径。

### JSON 解析约定

- `data/realtime_status.json` 是现有字段样例，包含 `chassisInfo`、`sensorStatus`、`sensorValues`、`vehicleStatus`、`actionStatus`、`vehicleAlarmStatus`、定位、网络及作业字段。
- `parseJson()` 递归展开对象，同时保存带完整路径和不带前缀的短键。不同分支的同名短键会发生覆盖；新增字段优先使用完整路径。
- 风速/风向优先取 `sensorValues.windSpeed/windDirection`，水平角优先取 `vehicleStatus.vehicleHorizontalX/Y`，部分动作告警优先取 `vehicleAlarmStatus.*`，再兼容短键。保留这些优先级。
- 已存在的键才更新，因此消息可以只包含部分字段。字符串数值兼容仅在部分辅助函数中实现，不是所有字段都可自动接受字符串。
- 保留字段大小写与来源语义，例如 `BatteryVoltage`、`ACFrequency`、`modbus_rpm`、`can_rpm`。底盘转速和机组转速不是可随意替换的同一字段。
- 机油压力先读 `modbus_oilPressure`，存在 `can_oilPressure` 时覆盖主仪表所用值；修改来源优先级要验证两个输入并存的场景。
- 起升状态 `vehicleAlarmStatus.insulatedBucketArmIsLifting` 的协议语义是 **0=起升、1=降落**，初始值为 1。不能按常规 `bool(0)==false` 推断动作是否有效。
- 网络界面收集 `eth0/eth1/eth2/usb0/usb1`；公开属性 `wlan0IP/wlan1IP` 实际分别映射 `usb0/usb1`，桥接接口可继承桥的 IP。不要未经核对按属性名称修改成 Linux wlan 网卡。
- `can_config.json`、`modbus_config.json` 的 JSON 根节点均为**数组**，不是包含 list 键的对象；本目录主 UI 未发现直接读取它们的路径。配置可能服务于外部采集程序，修改前定位消费者。

### VehicleData 全部公开属性与输入映射

当前 `VehicleData.h` 共声明 **96 个 Q_PROPERTY**，其中 4 个结构体聚合属性、92 个标量属性。下表覆盖全部名称；成组列出的名称按对应顺序映射。

输入列中，完整路径用于指出样例字段所在位置；除前文明确具有完整路径优先级的字段外，当前解析大多仍通过**短键**匹配，不能将本表解读为已经实现了完整路径隔离。

| QML 属性 | 输入/取得方式 |
| --- | --- |
| `chassisInfo`、`sensorStatus`、`sensorValues`、`vehicleStatus` | 同名内部结构体的聚合 getter；`chassisInfo` 标为 CONSTANT，其余用 dataUpdated；修改结构体成员时优先使用下列标量接口 |
| `gpsLocationStatus`、`gpsLongitude`、`gpsLatitude` | 顶层 `locationStatus`、`longitude`、`latitude` |
| `networkSignalStrength1`、`networkSignalStrength2` | 顶层 `networkCard1Signal`、`networkCard2Signal` |
| `networkDataUsage1`、`networkDataUsage2` | 顶层 `networkCard1DataUsage`、`networkCard2DataUsage`，按数值读取后转 QString |
| `chassisSpeed` | 短键 `speed`；当前样例位于 sensorValues，并非 chassisInfo |
| `chassisEngineRPM` | 短键 `modbus_rpm`，按数值转 QString |
| `chassisAtmosphericPressure` | chassisInfo 下 `atmosphericPressure`，按数值转 QString |
| `chassisOilPressure`、`chassisCoolantTemp` | chassisInfo 下 `can_oilPressure`、`can_coolantTemperature` |
| `chassisOilTemp`、`chassisFuelConsumption`、`chassisTotalMileage` | chassisInfo 下 `oilTemp`、`fuelConsumption`、`totalMileage` |
| `sensorNoise`、`sensorTemp1`、`sensorTemp2`、`sensorSmoke` | sensorStatus 下 `noise`、`temp1`、`temp2`、`smoke` |
| `sensorNoiseValue` | 短键 `noiseValue`；当前样例位于 vehicleStatus |
| `sensorLeftTemperatureValue`、`sensorRightTemperatureValue` | sensorValues 下 `leftTemperatureValue`、`rightTemperatureValue` |
| `sensorLeftHumidityValue`、`sensorRightHumidityValue` | sensorValues 下 `leftHumidityValue`、`rightHumidityValue` |
| `sensorWindSpeed`、`sensorWindDirection` | 优先完整路径 `sensorValues.windSpeed`、`sensorValues.windDirection`，兼容同名短键 |
| `vehiclePowerOutput`、`vehicleRPM` | vehicleStatus 下 `powerOutput`、`modbus_rpm` |
| `EngineRPM` | chassisInfo 下 `can_rpm`；属性名首字母大写，返回 int |
| `vehicleOilPressure` | `modbus_oilPressure`，有 `can_oilPressure` 时被后者覆盖 |
| `vehicleCoolantTemperature`、`vehicleFuelLevel` | vehicleStatus 下 `modbus_coolantTemperature`、`fuelLevel` |
| `vehicleTotalGeneratedPower`、`vehicleRemainingFuelGenerationTime` | vehicleStatus 下 `totalGeneratedPower`、`remainingFuelGenerationTime` |
| `vehicleHorizontalX`、`vehicleHorizontalY` | 优先完整路径 `vehicleStatus.vehicleHorizontalX`、`vehicleStatus.vehicleHorizontalY`，再兼容短键 |
| `vehicleWorkBucketLoad` | vehicleStatus 下 `workBucketLoad` |
| `workCount`、`currentWorkDuration`、`totalWorkDuration` | 顶层同名字段，表示车端值，与窗口层自行累计的统计要分清 |
| `eth0IP`、`eth1IP`、`eth2IP` | 本机 `ip addr show` 查询，不来自工况 JSON |
| `wlan0IP`、`wlan1IP` | 本机 usb0、usb1 的 IP，支持桥接继承 |
| `acFrequency` | sensorValues 下 `ACFrequency` |
| `lineVoltageAB`、`lineVoltageAC`、`lineVoltageBC` | sensorValues 下 `LineVoltageAB`、`LineVoltageAC`、`LineVoltageBC` |
| `phaseVoltageA`、`phaseVoltageB`、`phaseVoltageC` | sensorValues 下 `PhaseVoltageA`、`PhaseVoltageB`、`PhaseVoltageC` |
| `acCurrent`、`activePower` | sensorValues 下 `ACCurrent`、`ActivePower` |
| `batteryVoltage` | 短键 `BatteryVoltage`；当前样例位于 vehicleStatus |
| `cityCurrentIa`、`cityCurrentIb`、`cityCurrentIc` | sensorValues 下同名字段 |
| `cityInputP`、`cityInputQ` | sensorValues 下同名字段 |
| `cityBusUa`、`cityBusUb`、`cityBusUc` | sensorValues 下同名字段 |
| `cityBusUab`、`cityBusUbc`、`cityBusUca` | sensorValues 下同名字段 |
| `generatorUa`、`generatorUb`、`generatorUc` | sensorValues 下同名字段 |
| `generatorUab`、`generatorUbc`、`generatorUca` | sensorValues 下同名字段 |
| `generatorIa`、`generatorIb`、`generatorIc` | sensorValues 下同名字段 |
| `gcbStatus`、`mcbStatus` | sensorValues 下 `gcb`、`mcb` |
| `loadCabinetStatus`、`transformerStatus` | sensorValues 下同名字段 |
| `emergencyStopActive`、`exceedPermission` | actionStatus 下同名字段 |
| `leftFrontVerticalInPosition`、`leftRearVerticalInPosition`、`rightFrontVerticalInPosition`、`rightRearVerticalInPosition` | actionStatus 下同名字段，四个垂直到位状态 |
| `leftFrontHorizontalInPosition`、`leftRearHorizontalInPosition`、`rightFrontHorizontalInPosition`、`rightRearHorizontalInPosition` | actionStatus 下同名字段，四个水平到位状态 |
| `outriggerVirtualLegAlarm`、`workBucketOverloadAlarm`、`insulatedBucketArmIsLifting` | 优先 vehicleAlarmStatus 下同名完整路径，再兼容短键；界面虚腿报警另有派生计算，见后文 |

### 状态码与显示派生规则

| 状态/显示 | 当前代码语义 |
| --- | --- |
| `emergencyStopActive` | **0=急停按下/触发，1=释放/正常**；未收到数据时默认 1 |
| `insulatedBucketArmIsLifting` | **0=起升，1=未起升/降落**；默认 1；统计以该原始状态变迁计数 |
| 八个支腿到位属性 | 1=到位，其他值显示未到位；初始化为 0 |
| `exceedPermission` | 1 显示“是”并标红，否则“否” |
| `workBucketOverloadAlarm` | 1 显示有报警并标红，否则无报警 |
| DevicePage 虚腿报警 | `起升原始值===0` 且四个垂直到位中任一 `===0`；使用 `computedVirtualLegAlarm`，不直接显示 `vehicleData.outriggerVirtualLegAlarm` |
| MainPage 四个电气开关指示 | gcbStatus/mcbStatus/loadCabinetStatus/transformerStatus 为 0 时绿色，否则红色；不要直接套用其他设备的闭合/断开含义 |
| DevicePage 定位状态 | 当前用 `gpsLongitude > 1` 显示正常，否则异常，并非直接采用 gpsLocationStatus 或串口 isValid |
| DevicePage 网卡信号 | 1=差、4=良好、5=强，其余显示一般；顶栏则使用 0～5 对应的信号图 |

### 输入契约示例

以下是允许用于解析验证的**部分字段对象**，并非完整工况文件，也不代表建立网络连接后就会自动发布。要验证实际接收，发送方需使用 nanomsg PUB 协议及正确连接方向；`50001` 的订阅方由本程序连接外部发布服务，不能用普通 TCP 文本发送工具代替。

```json
{
  "sensorValues": {"windSpeed": 0.0, "windDirection": 90.0},
  "vehicleStatus": {"vehicleHorizontalX": 0.0, "vehicleHorizontalY": 1.5},
  "actionStatus": {"emergencyStopActive": 1, "leftFrontVerticalInPosition": 1},
  "vehicleAlarmStatus": {"insulatedBucketArmIsLifting": 1}
}
```

启动基线仍应使用完整 `data/realtime_status.json`；不要用此部分对象替换全部初值文件，因为多个结构体数值成员没有全面显式初始化，缺失字段不保证全为零。

## 6. 作业统计：必须保持的语义

统计状态位于 `main.qml` 的 ApplicationWindow，`objectName` 为 `vehicleHmiApplicationRoot`；MainPage 主要同步显示，C++ 负责导出和持久化。

- 根据起升状态变迁计算：从起升进入降落时，作业次数增加 **1**，并结算该轮起升时长。连续重复状态不能反复加次数。
- `hmiWorkCountEdges` 是历史命名；当前显示和导出的值就是次数，**不能再除以 2**。MainPage 中有关 `floor/2` 的旧注释不代表当前实现。
- 起升持续时长使用 `HmiWorkStatsEcho::steadyClockMs()` 的单调时钟，转换为小时。保留该路径，避免 NTP 或手动校时造成时长突变。
- 总时长在一轮结束时累计；起升期间本次时长可实时显示。不要未经需求变更把当前未结束的一轮反复加入累计值。
- 窗口层持续监听 `vehicleData.dataUpdated`，在切到告警、车辆或设置页时仍需统计。不要把统计重新放回受 `pageActive` 限制的 MainPage 回调。
- QML 向 C++ 调用 `setSnapshot(times, sessionHours, totalHours)`；首个快照立即落盘，此后定时周期为 3000ms。
- C++ 构造时先读取部署目录 `data/hmi_work_stats.json`，恢复 `/tmp/hmi_work_stats.txt`；文件缺失或解析失败时会建立零值文件。收到首个 QML 快照之前，定时刷新不会用成员初值覆盖恢复结果。
- QML 初始化优先使用 `/tmp` 三行文本，再考虑 LocalStorage，最后在提供了车辆对象时考虑车端值。当前 `Component.onCompleted` 主动调用 `hmiInitWorkStatsOnce(null)`，会提前完成初始化，不能声称运行中每帧都从车端累计值重新覆盖。

三行导出格式固定为：

```text
作业次数（整数，次）
本次作业时长（小时，小数四位）
累计总作业时长（小时，小数四位）
```

永久 JSON 字段为 `workCountTimes`、`sessionHours`、`totalHours`、`version`（当前 1）。LocalStorage 数据库 `HmiWorkStats` 使用 `kv(k,v)`，保存 `workCountEdges` 和 `cumulativeTotalWorkHours`。改格式时同时检查导出消费者、QML 恢复和旧数据兼容。

还需关注：当前起升过程中 MainPage 的每秒定时器更新显示；C++ 只写最近收到的 QML 快照。长时间没有新工况消息时，不能假定导出的时长必然每秒增长。

持久化使用截断写入，不是临时文件加原子替换，也未实现掉电事务。写入 `/tmp` 成功后才尝试写永久 JSON，两者不是一个原子操作；恢复时会处理负值和“本次大于总计”的情况。涉及可靠性要求时，应在保留原有格式与恢复顺序的前提下验证中断写入和文件损坏场景。

## 7. 数据库、配置与运行文件

`engine.setOfflineStoragePath("./")` 使 LocalStorage 数据库位于运行工作目录下的 `Databases/`。改变工作目录可能表现为“数据库丢失”，实际是访问了另一套目录。

| 数据库名 | 表与用途 |
| --- | --- |
| `AlarmDatabase` | `alarms(id,timestamp,type,content)`；当前 HMI 实现时间戳为整数秒 |
| `MaintenanceDatabase` | `maintenance(id,category,last_date,next_date,remarks)`；维保与试验记录 |
| `AlarmThresholdDatabase` | `thresholds(type,value)`；阈值设置 |
| `NetworkIPDatabase` | `network_ips(id,interface,ip)`；界面保存的网络 IP |
| `FontSettingsDB` | `FontSizeSettings(key,value)`；字体大小 |
| `SettingsDatabase` | `settings(key,value)`；定位使能等设置 |
| `HmiWorkStats` | `kv(k,v)`；作业次数和累计时长 |

- SQL 查询/写入数据优先使用参数绑定；修改表结构时考虑已部署数据库，保留用户历史记录。当前没有统一的数据库版本迁移机制。
- 告警插入按“整数秒时间戳 + 类型 + 内容”检查重复。这并非跨秒抑制重复告警的机制，不要扩大解读。
- JS 模块数据库句柄需先初始化。CarPage 已对初始化做延迟安排，避免切页同步执行大量查询。
- `VehicleData::checkForAlarms()` 定义了固定噪声、温度和烟雾阈值，但当前未发现执行调用；设置页保存的阈值也不能据此认定会自动影响 C++ 检测。
- 网络 IP 存入 LocalStorage 不等于执行了系统网络配置。定位/阈值等设置的实际效果也需追踪消费者，不要只凭控件存在判断已经接通后端。

各 JS 模块的可用操作如下，接口存在与页面接出功能需分别核对：

| 模块 | 当前操作 |
| --- | --- |
| `HMI/AlarmDatabase.js` | initDatabase、insertAlarm、queryAlarms、readAlarm、deleteAlarm、deleteAllAlarms、getAlarmCount、queryLatestAlarms |
| `HMI/maintenanceDatabase.js` | initDatabase、insertMaintenance、queryMaintenance、readMaintenance、updateMaintenance、deleteMaintenance、deleteAllMaintenance、queryLatestMaintenance |
| `HMI/AlarmThresholdManager.js` | initDatabase、upsertThreshold、queryThreshold、queryAllThresholds、deleteThreshold、deleteAllThresholds |
| `HMI/NetworkIPManager.js` | initDatabase、upsertIP、queryIP、initializeIPs、queryAllIPs、saveIP、updateIP |

`queryAlarms` 使用秒级时间戳的闭区间 BETWEEN；未指定排序。`queryLatestAlarms` 按时间倒序最多 10 条。`queryMaintenance` 未指定排序，同类别多条记录的展示不能保证按时间取最新；`queryLatestMaintenance(limit)` 则按 next_date 文本升序查询。

| 文件/位置 | 说明 |
| --- | --- |
| `data/sources.json` | 当前启动实际读取的视频源，初始化时只有一路；条目支持 `name/url/previewUrl` |
| `data/hdx_sources.json`、`org_sources.json`、`test_sources.json` | 其他视频源样例，初始化时各六路，不会自动替代 sources.json |
| `/root/car_fei_3d/data/vehicleRecord.json` | main.cpp 中车辆档案绝对路径，顶层为 `vehicleInfo` |
| `/root/car_fei_3d/data/terminal_info.json` | DevicePage 中终端档案绝对路径 |
| `/root/car_fei_3d/data/realtime_status.json` | 工况启动备用文件 |
| `/tmp/realtime_status.json` | 工况启动读取文件 |
| `/tmp/SensorStatus` | 部件/传感器状态 JSON，路径区分大小写 |
| `/tmp/hmi_work_stats.txt` | 三行作业统计接口 |
| `/root/car_fei_3d/data/hmi_work_stats.json` | 跨重启的作业统计永久文件 |
| `/dev/shm/car_fei_hmi_debug.log` | HmiWorkStatsEcho 跟踪日志，运行中持续追加 |
| `SDKLOG/`、`HCNetSDK_Log_Switch.xml` | 厂商 SDK 日志与日志配置 |

### 配置结构核对

| 配置 | 根节点、字段与边界 |
| --- | --- |
| `data/terminal_info.json` | 对象；name/model/programVersion/manufactureDate/deviceId 五个字符串；TerminalData 仅 loadJson 暴露为 Q_INVOKABLE，saveToJson 是普通 C++ 方法 |
| `data/vehicleRecord.json` | 对象中的 vehicleInfo；name/plateNumber/chassisNumber/startDate/vehicleType；映射到 vehicleName/vehiclePlateNumber/vehicleChassisNumber/vehicleStartDate/vehicleType |
| 视频源各 JSON | 对象中的 sources 数组；每项 name/url，可选 previewUrl；VideoUrl 按名称精确查找，当前未做全面的重复名、URL 和字段类型验证 |
| `can_config.json` | 根数组，当前 11 项；条目包含 byte_order/canid/data_type/length/name/offset/offset_value/scale_factor；具体单位、位序编码和 data_type 枚举需从外部消费者确认 |
| `modbus_config.json` | 根数组，当前 2 项；条目包含 address/data_length/data_type/function_code/name/offset/scale_factor；不能单凭配置推断完整串口或从站设置 |
| `sensor_val.json` | 根对象，noiseValue/leftTemperatureValue/rightTemperatureValue/leftHumidityValue/rightHumidityValue；当前活动 UI 路径未发现直接读此文件 |
| `data/hmi_work_stats.json` | 根对象，统计四字段，供部署位置的持久化读取；源码样例的位置不等于运行时绝对路径 |

例如 can_config 中存在 coolantTemperature/batteryVoltage 等名称，而实时 JSON 消费的是 can_coolantTemperature/BatteryVoltage 等字段；不能未经外部采集服务映射确认就直接重命名为一致。

## 8. 视频、云台与页面生命周期

- 当前 MainPage 的 `videoCount` 为 **1**，显示“车前”；VideoUrl 仍保留 1～6 到车前、车后、车左、车右、车上、车下的映射。扩展路数要同时检查配置、布局、解码资源及弹窗逻辑。
- 放大窗口通过 `getUrlByName()` 使用主 URL；小窗通过 `getPreviewUrlByName()` 优先使用显式 `previewUrl`，否则将 URL 中 `subtype=0` 改为 `subtype=1`。该回退只处理这一种字符串形式，不保证所有摄像头型号都能由主码流自动推导子码流。
- `MediaPlayer` 和 `VideoOutput` 存在动态创建路径。保持停止、销毁、清除引用的完整流程；放大前释放小窗，同名弹窗不重复创建，关闭时销毁窗口并移除映射。
- 主页面方向按钮使用 `ptzBridge`，按下移动，释放或取消时停止。ONVIF 控制与 RTSP 播放的地址和端口是不同配置。
- PTZ 初始化配置目前硬编码在 `main.cpp`，没有自动从 `data/sources.json` 同步。调整摄像头时检查两条配置路径；不要把摄像头明文凭据写进新增文档、日志或示例。
- OnvifPtzController 使用 HTTP SOAP 和 WS-Security 摘要，curl 连接超时 5 秒、请求超时 15 秒。桥接调用当前为同步方式，MainPage 完成时会初始化；初始化和网络异常可能阻塞 UI，排障时应考虑。
- ONVIF 初始化依次探测 Media 的 `/onvif/Media`、`/onvif/media_service`、`/onvif/device_service`，再通过 Stop 请求探测 `/onvif/PTZ`、`/onvif/ptz_service`、`/onvif/device_service`。15 秒是单请求上限，不是整个初始化上限；探测初始化本身会发送停止命令。
- ONVIF 支持 left/right/up/down/zoomin/zoomout，输入速度范围 0～1；当前现场适配为 up 负 tilt、down 正 tilt，tilt 乘 2 后限制到 [-1,1]。主页面垂直按钮传 0.8、水平按钮传 0.35。改方向或速度前保留现场适配语义，并分别验证。
- 海康 `cameraControl` 的上下文注册当前在 `#if 0` 中，但 `HMI/Dialog.qml` 仍存在其触摸调用。不要把 ONVIF 主界面按钮可用等同于弹窗的历史海康云台路径可用；改动时检查缺失对象和异常分支。
- StackView 保留底层 MainPage：从首页进入子页用 push，子页间替换栈顶，返回首页用 pop。避免返回时重建首页造成 PTZ、订阅和播放器重复初始化。
- 页面 `objectName` 参与背景和导航状态匹配。新增页面时同步 URL、objectName、导航按钮、背景、激活/停用钩子及资源清单。
- `pageActive` 用于主页面后台降载；保留隐藏页面的回调和定时器限制，同时保证窗口层统计不受影响。不要假设设置了 `pageActive=false` 就已停止所有视频流，应检查播放器生命周期。
- 默认不启用进程内 drop_caches。`CAR_FEI_DROP_CACHES_ENABLE=1` 或 `true` 才开启，间隔由正整数 `CAR_FEI_DROP_CACHES_INTERVAL_MS` 覆盖，默认 300000ms；`CAR_FEI_DROP_CACHES_DISABLE=1/true` 强制关闭。不要将周期清缓存当作常规性能修复。

### 五个页面的实际差异

| 页面 | 当前实现与修改时的核对点 |
| --- | --- |
| MainPage | 保留实例；后台限制部分回调；工况显示同步用 500ms 单次延迟；起升中显示每秒刷新。数据持续高频到达时反复 restart 的单次定时器可能延后触发，不能将其理解成固定每 500ms 必执行 |
| AlertDisplay | 起始时间初值为当前减 2400 小时（100 天），虽注释写 12 小时；部分编辑校验会回退到减 12 小时。按钮类型为全部/告警/变位/事件/操作，按时间范围查询并重填模型；当前没有接出详情或删除控件，也没有订阅插入信号自动刷新列表 |
| CarPage | 固定 24px 字号；进入后约 300ms 初始化最新告警、2000ms 初始化维保；维保类别为预防性试验/维护保养/交接试验/故障维修。录入仅检查上次日期非空，日期格式和先后关系没有完整校验 |
| DevicePage | 固定 20px 字号，不读取全局字号设置；GPS、动作、派生报警和 SensorStatus 分属不同来源；CPU/磁盘绑定 SystemInfo，内存文字当前固定 `0 M`，运行时长固定 `—`，OS 文本固定 Linux |
| SettingsPage | 字体/定位/阈值/IP 的本地设置；仍保留历史维保初始化与 UI 更新逻辑，不能按“维保已移走”的旧注释判断其完全没有数据库副作用 |

补充约定：

- FontSettings 保存中文档位“小/中/大”，加载对应 18/24/28px；SettingsPage 隐藏的字号滑块中“小”却赋值 14px。CarPage 和 DevicePage 固定字号，因此“全局改字号会改变所有页面”并非当前行为。
- SettingsPage 字号区域设置为 `visible: false`，其对象及初始化代码仍存在；不可见不表示 Component.onCompleted 不执行。
- 定位开关使用 `SettingsDatabase.settings` 的 `locationEnable`，字符串 true/false，默认 true；未发现它控制 GPSReceiver 串口开关或工况订阅的路径。
- 阈值以中文名称作主键，现有选项和界面默认：车速告警阈值 60/80/100/120（80），发动机水温告警阈值 90/95/100/105（95），过负荷告警阈值 80/90/100/110（90），电压低告警阈值 10/11/12/13（11）。这只是 UI 配置，不是已联动生效的控制协议。
- SettingsPage 的初始化代码包含“维保库为空则插入四条实验数据”的分支；新设备或测试库打开设置页时需检查是否执行到该分支。CarPage 当前没有自动调用其同名实验数据函数。
- SettingsPage 的 updateFontSize/applyFontSize/updateMaintenanceText 仍引用当前文件未定义的 maintenance_title_txt、preventiveTest、maintenance、handoverTest、faultRepair 等 id，可能导致运行时异常并打断后续初始化。处理设置页问题时先看具体 QML 错误，不要直接认定后续初始化均已成功。
- `HMI/Dialog.qml` 的根对象是 Item；CarPage/SettingsPage 还使用 Qt Quick Controls 的 Dialog API。涉及同名组件的加载失败时核对类型解析和导入，不能把自定义视频 Item 当作 Controls.Dialog。
- 顶栏每秒更新时间，代码使用 UTC 小时加 8 后写回本地 Date；告警时间转换和车辆页 toLocaleString 则依赖本地日期处理。时间显示或筛选异常时核对设备时区，不能保证现有写法适用于任意时区。
- DatePicker 年份输入当前含固定 2030；是否会影响业务取决于实际实例化路径。`ThresholdControl.qml` 也不等于设置页的实际阈值实现，修改前先定位引用。
- 主界面当前燃油数值文字使用四舍五入后加 `L`，有效燃油图形的高度固定为容器高度；旧百分比高度实现位于注释中。不要按历史注释擅自乘除 100 或假定图形高度反映液位。
- SystemInfo 的磁盘数值由 df 可用块数/总块数计算，属于剩余空间比例，虽属性名叫 diskUsage；CPU 当前用单次 /proc/stat 累计值计算，不是相邻采样周期的 CPU 利用率。

## 9. 构建与部署方式

### 依赖检查

需要匹配目标平台的 Qt Quick/QML/GUI/OpenGL/Network/Concurrent/SerialPort 开发环境，以及 QML 运行模块 Qt Quick Controls、Layouts、Window、LocalStorage、Multimedia、Particles、VirtualKeyboard/Settings。Multimedia 和 VirtualKeyboard 的运行插件也需要正确部署。

外部依赖包括：

- nanomsg 头文件和库（工程搜索 `/usr/local/include`、`/usr/local/lib`）。
- `nlohmann/json.hpp` 头文件。
- libcurl、OpenSSL 的头文件及 `curl/ssl/crypto` 库。
- 海康 `hcnetsdk/HCCore/hpr/PlayCtrl` 库及配套运行组件；当前即便 SDK 登录路径关闭，主 `.pro` 仍编译相应源码并链接这些库。
- 设备 OpenGL ES/EGL/DRM 与 Qt 平台插件；Linux 环境中的 `bash`、`sh`、`ip`、`ping`、`free`、`df`、`awk` 等命令。

不要将 ARM64 `.so` 直接交给 x86_64 主机链接。交叉编译需匹配 Qt、sysroot、编译器、厂商库和 ABI；当前仓库没有完整的交叉工具链脚本。

### 构建命令

以下命令针对**具有匹配 ARM64 Qt 6.5.3 和依赖的环境**。先核实工具存在及架构：

```bash
cd /home/tronlong/lyp/code/doubiche_car_fei_3d
/usr/local/Qt-6.5.3/bin/qmake -v
file car_fei libhcnetsdk.so libHCCore.so libPlayCtrl.so
```

优先使用独立构建目录，避免覆盖源码中混存的历史产物：

```bash
mkdir -p /tmp/doubiche-car-fei-build
cd /tmp/doubiche-car-fei-build
/usr/local/Qt-6.5.3/bin/qmake \
  /home/tronlong/lyp/code/doubiche_car_fei_3d/car_fei.pro \
  'LIBS+=-L/home/tronlong/lyp/code/doubiche_car_fei_3d'
make -j2
```

额外传入库搜索目录是因为原 `.pro` 使用 `-L./`，独立构建后 `./` 不再指向保存厂商库的源码根目录。该参数不能解决依赖缺失、架构不符或 ABI 冲突。设备内存不足时降低 make 并行度。

原目录构建也是现有工程支持的方式，但会更新混存产物：

```bash
cd /home/tronlong/lyp/code/doubiche_car_fei_3d
/usr/local/Qt-6.5.3/bin/qmake car_fei.pro
make -j2
```

更换机器或工具链后重新运行 qmake，不要沿用保存的绝对路径 Makefile。不要直接编辑 `Makefile`、`moc_*.cpp` 或 `qrc_qml.cpp` 来修复源码问题。

### 设备运行

仅在已准备好对应文件、依赖及显示环境的设备上执行：

```bash
cd /root/car_fei_3d
./car_fei -platform eglfs
```

- 保持工作目录正确，确保 `main.qml`、HMI、资源、`data/`、可写的数据库位置都可用。
- `LIBS` 的链接搜索路径不等于运行时加载路径；部署时核对系统动态链接器配置或现有启动脚本中的 `LD_LIBRARY_PATH`，避免混用根目录随附库与系统其他版本。
- 此源码快照没有附带服务单元或启动脚本。历史文档提及 `car_fei` systemd 服务，但实际服务名、WorkingDirectory、环境变量及自启动关系需在设备确认。
- 启动会访问背光 sysfs、串口、摄像头网络及持久化路径。设备出现摄像头由通变断时，PingThread 会调用 `kill -9` 终止本进程；这与单纯播放器报错不同。
- EGLFS/DRM 设备只保留一个显示实例；先检查现有服务和进程，再进行手动启动验证。

## 10. 修改约定与易错点

1. 优先完成局部修改，保持既有模块边界、公开名称、中文显示、字段单位和有效值语义。不要为单个问题引入 Qt 大版本迁移或重写构建系统。
2. 保持所改文件现有缩进、括号及换行风格；部分 QML 使用 CRLF，避免仅因换行转换造成整文件差异。中文保存为 UTF-8。
3. 新增 C++ 文件同步 `SOURCES/HEADERS`；新增嵌入资源同步 `qml.qrc`。`DISTFILES` 本身不代表资源已打包，也不保证部署时自动复制。
4. 当前 `.pro` 的 `DISTFILES` 含不存在的 `data/vehicleData.json`、`image/env_inner.png`、`images/mainPageBg.png` 等旧路径；不要据此推断实际文件名。应分别核对文件系统、qrc 和加载路径。
5. 新 QML 信号处理优先使用 `function onXxx(...)` 形式；访问动态对象、销毁回调及异步操作时检查对象仍有效。
6. 避免高频消息中重复日志、同步 shell、数据库查询、磁盘写入及昂贵对象创建。现有系统信息、网络查询、文件回退和 PTZ 都有同步操作，相关性能修改应量测阻塞来源。
7. `VehicleData` 的订阅使用 detached `std::thread`，CppNanoMsg 使用接收 QThread，均有生命周期收敛方面的历史问题。涉及退出、重建或重连时检查 socket 关闭、阻塞接收退出、线程回收及 QObject 生命周期。
8. 无效/缺失字段、合法零值、字符串数值和 NaN 要分别处理。不要用简单真值判断把合法 0 显示为缺失，也不要默认所有状态码都是 1=正常。
9. 用户数据库、设备档案、作业统计及摄像头配置视为运行数据。验证修改应使用副本；避免将设备状态、明文 URL 凭据或日志内容纳入常规代码差异。
10. 厂商头文件和二进制库按依赖资产维护，不手工修改 SDK 内部实现。更换库需说明来源、架构和兼容验证。
11. 源目录保存大量 `*.o`、`moc_*`、`qrc_*`、`.qmake.stash`、`Makefile`、`*.pro.user`、可执行文件和日志。不要以文件存在或时间戳证明当前源码已成功构建，也不要顺手批量删除它们。
12. `.gitignore` 忽略动态库、Databases、sounds、日志等，但已有文件仍可能是必需运行依赖。忽略规则不等于这些文件可从部署包移除；后续恢复 Git 管理时先核对跟踪策略。

### 已确认的实现限制

以下用于避免后续修改沿用错误假设，并不表示本次已修复源码：

- 四个电气开关量在 ACData 中存为 uint8_t，解析时却转为 `reinterpret_cast<int&>` 传给整数赋值函数。这不是安全的类型转换，存在写入相邻成员和未定义行为的风险。涉及这些字段时优先解决存储类型与赋值一致性，并做边界验证。
- 除 m_workInfo、动作/报警成员及部分显式赋值外，多个工况结构体没有全面数值初始化。启动文件加载失败或只推送部分字段时，不能假定其余数值有效。
- updateNetworkIPs 更新内部列表后没有单独发 dataUpdated；网卡 IP 的属性通知可能依赖后续工况消息，不能从“每 3 秒查询”推导出“无数据源时每 3 秒界面必刷新”。
- CppNanoMsg 为订阅创建 QThread，但未保存线程指针用于等待退出，析构只关 socket；多次 startSubscriber 也没有防重复的状态检查。PUB 为 bind、SUB 为 connect，接收订阅全部主题且按 UTF-8 文本发信号。
- VehicleData 后台线程在接收失败时等待 100ms 继续循环，没有统一停止控制、消息长度上限或显式失联状态；UI 会保留最后值，不应把保留值理解为通信正常。
- GPS 串口没有完整的跨 readyRead 行缓冲、NMEA 校验和/定位质量校验；300 秒超时只在串口成功打开后启动的定时器中检查。
- QML 入口加载后未检查 rootObjects 是否为空，也未建立加载失败自动退出策略；界面没出现时进程存在并不能证明页面加载成功。
- 历史 UDP 无参函数使用 1024 字节接收缓冲并在返回长度位置补零，满缓冲时存在越界；此路径并未因当前主链路停用而从源码移除。若重新启用，先处理边界、超时、JSON 异常和分片协议。

排障搜索优先排除巨大生成文件和 SDK 头文件，例如在源码根目录执行：

```bash
rg -n 'insulatedBucketArmIsLifting|dataUpdated|50001' \
  -g '*.cpp' -g '*.h' -g '*.qml' -g '*.js' \
  -g '!moc_*' -g '!qrc_*' -g '!HCNetSDK.h'
```

## 11. 验证与排障

设备现场排查参见 [分析报告 11.2–11.15](doubiche_car_fei_3d分析报告.md#112-现场排障入口)，包含按现象分流、检查步骤、结果判读、日志速查和恢复验收；其中板端命令是参考步骤，本次未连接设备执行。

### 验证级别

当前源码快照没有发现自动化测试目录、测试框架入口、CI 配置、README 或构建/部署 shell 脚本。不要声称存在 `make test`、CTest、npm 或其他未经发现的测试命令。

- **只修改本文**：核对源码文件、命令、字段、端口及路径，检查 Markdown 和差异即可，不需要运行车辆程序。
- **修改 C++**：在匹配环境完成 qmake/make；涉及协议、状态和累计逻辑时，用独立输入验证边界情况。
- **修改 QML/JS**：若具备 Qt 工具，使用同版本 qmllint 检查，并结合注册模块和上下文理解诊断；仍需设备运行验证实际页面、资源和媒体后端。
- **修改配置/资源**：检查 JSON/XML 可解析、相应文件存在、消费者读取路径正确；成功解析不等于业务配置有效。
- **涉及硬件、播放或 EGLFS**：记录板端人工验证范围和未覆盖项，不能以宿主机静态检查替代。

可在源码根目录执行以下不启动程序的完整性检查：

```bash
python3 - <<'PY'
import json
from pathlib import Path
import xml.etree.ElementTree as ET

root = Path.cwd()
assert (root / 'car_fei.pro').is_file(), '请在源码根目录执行'
configs = sorted((root / 'data').glob('*.json'))
configs += sorted(root.glob('*_config.json'))
if (root / 'sensor_val.json').is_file():
    configs.append(root / 'sensor_val.json')
for path in configs:
    value = json.loads(path.read_text(encoding='utf-8'))
    if path.name in ('can_config.json', 'modbus_config.json'):
        assert isinstance(value, list), f'{path}: 根节点应为数组'
entries = ET.parse(root / 'qml.qrc').findall('.//file')
missing = [entry.text for entry in entries if not (root / entry.text).is_file()]
assert not missing, f'缺失资源: {missing}'
print(f'JSON: {len(configs)} 个有效；QRC: {len(entries)} 项全部存在')
PY
```

### 按变更选择板端回归场景

| 变更范围 | 最少检查场景 |
| --- | --- |
| 工况解析 | 完整对象、部分字段、非法 JSON、数值/字符串、同名短键冲突、合法 0；50001 推送后 UI 更新 |
| 起升与统计 | 初始降落、初始起升、重复起升、起升→降落、重复降落、连续多轮、切页期间动作、重启恢复 |
| 时长 | 墙钟前后跳变、长时间没有新帧、界面与导出值一致、文本/JSON/LocalStorage 恢复优先级 |
| 导航与性能 | 五个页面反复切换、首页保留、字体显示、后台回调负载、首次打开车辆页和数据库查询 |
| 告警与维保 | 页面已有的查询/筛选/录入；若改数据库接口，再验证相应详情、更新和删除操作；使用可丢弃数据库副本 |
| 视频 | 小窗播放、放大、关闭、反复点击、源缺失、播放失败、资源释放和设备解码负载 |
| 云台 | 初始化失败/成功、四方向、按下/释放/取消停止、弹窗历史接口与主界面接口分别验证 |
| 部署 | 单实例、工作目录、图片字体、Qt 插件、动态库、串口权限、持久化可写、外部数据服务 |

### 常见症状定位

- **页面整体加载失败**：先看 QML 错误、当前工作目录、缺失 import、上下文属性及磁盘文件；当前入口没有启用 qrc main.qml。
- **只有初值、不再刷新**：检查 vehicleDataEngine 与 50001 推送，随后看解析错误、属性通知和 UI 绑定；不要只修改 `/tmp` 文件反复尝试。
- **工况跳变/来源冲突**：同时查看 6666 辅助赋值、50001 更新、JSON 完整路径优先级及同名短键。
- **统计次数翻倍或减半**：核对重复通知、状态变迁与旧 edges 命名，不要引入二次除 2。
- **统计恢复为零**：检查永久 JSON 和工作目录对应数据库；注意 C++ 先恢复临时文件的顺序，以及解析失败后的默认值处理。
- **摄像头断网后整个程序消失**：先看 PingThread 的主动 SIGKILL 路径及服务重启记录。
- **云台/首页启动卡顿**：检查同步 ONVIF 初始化的超时、配置独立性及主线程调用。
- **黑屏、页翻错误或长时间卡顿**：检查 EGLFS 多实例、视频管线数量、页面重建、同步阻塞和 drop_caches 环境开关。

在有相应命令和访问权限的设备上，可先做只读诊断：

```bash
pgrep -af car_fei
ss -lntp
ip addr show
tail -n 80 /dev/shm/car_fei_hmi_debug.log
```

若确认使用名为 `car_fei` 的 systemd 服务，再查看 `journalctl -u car_fei -n 100 --no-pager`；结合 `dmesg` 检索 DRM/KMS/page flip 等记录。日志涉及视频 URL 和设备信息时，对外分享前先去除凭据。

## 12. 任务收尾要求

- 明确修改的是源码还是文档，列出实际受影响模块及行为变化。
- 报告实际执行的静态检查、构建或板端验证结果；环境缺少依赖时说明未执行部分及具体原因。
- 涉及接口、端口、配置路径、数据库结构、作业统计格式或部署方式变化时，同步更新本文相关章节。
- 如果源码目录后续恢复了 Git 元数据，在该目录核对差异与未跟踪文件；文档改动则在文档所属仓库核对。提交前避免混入构建产物和运行数据。
- 本文初始化已核实：源码/文档路径、主要调用链、三个 nanomsg 端口、配置 JSON 可解析性、qrc 引用存在性及 ARM64 产物架构；未声称完成编译、GUI、摄像头、串口或板端业务验证。

### 覆盖边界

本指南覆盖主工程 14 个 C++ 源文件及其项目头文件、当前五个导航页、主窗口与视频弹窗、公共 QML/JS 组件、全部 96 个 VehicleData 属性、现有配置和运行路径。历史视频工程、备用页面、生成产物及厂商组件已标明用途，但没有将它们当作当前完整可用的主链路。

仍需实际环境提供证据的部分包括：设备 Qt/媒体插件与 SDK 的兼容组合、摄像头实际返回的 ONVIF/RTSP 行为、外部采集服务和发布协议的完整实现、systemd 启动配置、长期运行资源变化及掉电恢复。厂商二进制内部行为、未提供的外部服务和未执行的板端结果不属于本次静态初始化可证明的范围。
