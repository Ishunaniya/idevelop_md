# sound_light_alarm_sep 源码分析

> 核验基准：`/home/tronlong/lyp/code/skes/skes-apps/sound_light_alarm_sep`，Git 分支 `main_ui`，2026-09-28 核验时 HEAD 为 `b28d0b82054da75ee1ea0020464a660885fb1b3d`。本子项目相对前次基准 `6bc22effe3ac53e88d8d20bee73c3a1fb54b4c14` 没有源码差异。以下描述针对核验时工作树源码，不把注释中的计划或外部设备行为当作已经实现的功能。文中 `文件:行号` 指本项目源码；涉及上级库时单独注明。

## 文档目录

专题编号沿用原报告，以保留“§N”和“第 N 节”的交叉引用；阅读顺序按下面的主题排列。

1. [一、项目概述](#overview)
   - [专题 1：范围与结论](#detail-1)
   - [专题 3：配置、构建和部署边界](#detail-3)
2. [二、源码目录总览](#source-tree)
3. [三、核心架构设计](#architecture)
   - [专题 2：入口、初始化与线程](#detail-2)
4. [四、核心模块深度分析](#modules)
   - [专题 4：输入消息及状态更新](#detail-4)
   - [专题 5：装载机执行路径](#detail-5)
   - [专题 6：叉车执行路径](#detail-6)
5. [五、专题详解与设备排障](#details)
   - [专题 7：通信、错误处理和可验证边界](#detail-7)
   - [专题 8：源码中可直接确认的限制与风险](#detail-8)
   - [专题 9：核验依据与使用方式](#detail-9)
   - [专题 10：设备故障排查手册](#detail-10)

<a id="overview"></a>
## 一、项目概述

`sound_light_alarm_sep` 是常驻声光报警应用。从 SKES 消息获得人脸/安全带属性和 BSD/DMS 告警后，装载机路径直接控制本地串口和 GPIO，叉车路径把语音与 DO 指令交给本机服务。它不做人脸或告警检测。`crawler_crane` 可作为设备类型解析，但当前发送线程没有对应输出分支。外部串口服务或报警器是否执行须用下游证据确认；原第 1 节给出分析范围。

<a id="detail-1"></a>
### 1. 范围与结论

本程序是一个常驻进程，从本机 SKES 消息链路取得人脸/安全带属性和 DMS、BSD 告警，再按设备类型发送声光报警指令。装载机路径直接写 `/dev/ttyS1`；叉车路径向本机串口服务和 DO 服务发送 nanomsg 消息。没有图像识别或告警检测算法，告警状态由上游消息提供。`crawler_crane` 只完成设备类型解析，当前没有发送分支；程序仍会启动消息轮询并在主循环中等待。依据：`src/sound_light_alarm_sep.c:127-139,760-827,1453-1474`。

| 文件 | 本项目中的作用 |
| --- | --- |
| `src/main.c` | 日志、版本日志记录、信号处理、对象初始化和主循环。 |
| `src/sound_light_alarm_sep.c` | SKES 消息解析、状态保存、装载机及叉车控制、线程和资源接口。 |
| `src/sound_light_alarm_sep.h` | 版本、端口、DO 命令结构与公开接口；其中还有本文件未用到的 CAN/AEB 定义。 |
| `src/serial.c/.h` | POSIX 串口打开、配置、写入和关闭。 |
| `src/gpio.c/.h` | 通过 `/sys/class/gpio` 操作引脚。 |
| `CMakeLists.txt`、`sep.ini`、`sound_light_alarm_sep.ini.*` | 构建、安装和设备配置。 |
| `tests/test.cpp` | 手工循环发布 DMS/BSD 告警样例，不是自动断言测试。 |

<a id="detail-3"></a>
### 3. 配置、构建和部署边界

| 项目 | 源码中的实际行为 |
| --- | --- |
| `sep.ini` | `[sep] enabled=true`、`name=sound_light_alarm_sep`、`param=` 为空；随程序安装。`main` 不解析命令行参数。 |
| `SLA_TYPE` | CMake 安装时选配置：`forklift` 选叉车，其他值（含未设置）选装载机；安装后统一命名为 `sound_light_alarm_sep.ini`。运行分支看 INI 的 `[dev] type`，不是直接看 `SLA_TYPE`。 |
| 设备 INI | 装载机文件含 `type=wheel_loader`、`[log] level=4`；叉车文件含 `type=forklift`、`do_trigger_level=0`、`[log] level=4`。装载机没有 `do_trigger_level`，读取失败时保持零。 |
| UI JSON | 仅装载机发送逻辑在串口 fd 有效时每轮读取 `/tmp/ui_config.json` 的 `alarm.enabled` 和 `alarm.volume`；按 `int(volume/100*30)` 换算，无输入范围钳制。 |
| 构建 | 上级 CMake 纳入本目录；本目录以 C99 编译源码、C++11 编译手工测试，链接 `cjson`、`skes-linker`、`skes-utils`、`skes-log`、`nanomsg`、`curl`、`ssl`、`crypto`、`z`、`pthread`、`rt`。安装可执行文件、配置和若干动态库。 |

依据：`CMakeLists.txt:1-96`、`sep.ini`、两个设备 INI、`src/main.c:85-92`、`src/sound_light_alarm_sep.c:940-970`。安装后的绝对路径由顶层安装前缀决定；这里不能从子项目 CMake 单独推出设备上的最终绝对路径。

<a id="source-tree"></a>
## 二、源码目录总览

路径相对于 `skes-apps/sound_light_alarm_sep/`，主源码集中在 `src/`。

```text
sound_light_alarm_sep/
├── CMakeLists.txt、sep.ini                         构建、安装与 SEP 启动
├── sound_light_alarm_sep.ini.wheel_loader          装载机配置模板
├── sound_light_alarm_sep.ini.forklift              叉车配置模板
├── src/
│   ├── main.c                                     入口、日志和信号
│   ├── sound_light_alarm_sep.c/.h                 消息状态、设备分支及发送
│   ├── serial.c/.h                               本机串口封装
│   └── gpio.c/.h                                 GPIO 文件接口
└── tests/                                        循环发送告警的手工示例
```

`SLA_TYPE` 只决定安装哪份 INI 模板，运行分支依据安装后的 `[dev] type`。测试程序主动循环发消息，不是自动断言的无副作用测试。构建与文件职责见原第 1、3 节。

<a id="architecture"></a>
## 三、核心架构设计

```text
SKES /skes/filter、/skes/event ──> 消息线程 ──> 共享告警/授权状态
                                                    │
                                                    ▼
                                               设备发送线程
                                            ┌───────┴────────┐
                           wheel_loader：UI JSON + 串口/GPIO  forklift：人脸授权 + BSD/DMS
                                      │                               │
                              /dev/ttyS1、方向 GPIO          26006 语音、26008 DO
                                      │                               │
                                 本地报警器                   外部串口/DO 服务
```

`main` 还创建串口接收线程；它只在装载机类型下打开 `/dev/ttyS1`，收到数据只记录长度。装载机的声音控制还需 `/tmp/ui_config.json` 中的 `alarm.enabled/volume`；叉车须满足人脸与安全带授权条件，再按 BSD、DMS 优先级选语音。消息线程和发送线程共享字段，当前实现未见显式互斥保护。`crawler_crane` 当前无发送分支。线程与状态流见原第 2、4～6 节。

<a id="detail-2"></a>
### 2. 入口、初始化与线程

`main` 先开启文件日志，设置单文件上限 `5*1024*1024` 字节和文件数 4，读取并回写相对路径 `sound_light_alarm_sep.ini` 的 `[log] level`；读不到时初始级别为 `SKES_LEVEL_ERROR`。注册 `SIGUSR1` 后，调用日志宏记录由头文件宏组成的 `0.1.1.0` 版本；是否能看到该记录取决于日志级别。随后执行 `sound_light_alarm_new`、`sound_light_alarm_run`，每 200 ms 读取 `running` 标志；循环退出后调用 `sound_light_alarm_release`。运行成功后，当前 `main` 没有主动把 `running` 置为 false 的路径，因此通常会持续等待；`SIGUSR1` 仅切换进程内日志级别，不写回 INI，也不是退出信号。`argc/argv`、`last` 和 `timeout` 在当前入口不参与控制。依据：`src/main.c:23-129`、`src/sound_light_alarm_sep.h:17-20`。

`new` 用 `malloc` 加 `memset` 清零对象；调用 `skes_linker_init` 创建订阅端 `tcp://127.0.0.1:19226` 和发布端 `tcp://127.0.0.1:19225`，设置回调；串口描述符置为 `-1`。设备类型先设为装载机，再从 `[dev] type` 读取；`do_trigger_level` 从 `[dev] do_trigger_level` 读取。两次读取的返回值都没有用于中止初始化，所以缺失时保留零初始化的值。之后分别连接本机 RS485、RS232 和 DO 发布端口，再睡眠 1 秒，最终返回 `skes_linker_init` 的结果。依据：`src/sound_light_alarm_sep.c:797-827`。

`run` 依次创建消息轮询、发送、串口接收三个线程。任一步创建失败都立即返回错误，不会在该失败分支停止或等待此前已创建的线程。轮询线程设置 `skes_run` 为无限等待、可等待模式，持续消费消息；发送线程按设备类型运行；接收线程仅在装载机类型下打开 `/dev/ttyS1`，读取到的数据只记录长度，不解析。依据：`src/sound_light_alarm_sep.c:830-853,874-938,1453-1503`。

#### 启动与运行流程图（纯 Markdown 可显示）

```text
main
  |
  +--> init_log --> 读取/回写 [log] level --> 注册 SIGUSR1 --> 打印版本
  |
  +--> sound_light_alarm_new
  |      +--> 分配并清零状态
  |      +--> skes_linker_init(订阅 19226, 发布 19225)
  |      +--> 读取 [dev] type、do_trigger_level
  |      +--> 连接 RS485:26006、RS232:26007、DO:26008
  |      `--> 返回 linker 初始化结果
  |
  +--> sound_light_alarm_run
  |      +--> 消息轮询线程 --> SKES 回调 --> 更新共享状态
  |      +--> 发送线程 --> 装载机 / 叉车 / 无发送分支
  |      `--> 串口接收线程 --> 装载机打开并读取 / 其他类型立即退出
  |
  `--> 每 200 ms 检查 running --> 退出时 release
```

文字版流程：① 初始化日志与信号；② 创建共享状态和三个本机消息连接；③ 启动三个线程；④ 轮询线程更新 DMS/BSD/人脸状态，发送线程读取状态并选取设备路径，装载机接收线程维持串口；⑤ 主线程等待 `running` 变为假；⑥ 释放 linker 与对象。`main` 正常路径没有调用 `sound_light_alarm_stop`。线程间对状态字段没有显式互斥保护。依据：`src/main.c:90-129`、`src/sound_light_alarm_sep.c:797-853,1476-1529`。

<a id="modules"></a>
## 四、核心模块深度分析

| 模块 | 输入与处理 | 输出及当前边界 | 详解 |
| --- | --- | --- | --- |
| 生命周期与配置 | `main.c` 建实例和三线程；对象初始化读取 `[dev] type`、`do_trigger_level`。 | 配置读取失败不一定中止初始化；`main` 常规路径未调用完整的 `stop`。 | 原第 2、3、8 节 |
| SKES 消息解析 | `filter/properties` 更新人脸/安全带，`event/alarm` 更新 BSD/DMS。 | 只分派两类主题；叉车 DMS 细分位在 `normal` 时不全清，多个事件项还受循环索引复用影响。 | 原第 4、8 节 |
| 装载机发送 | 发送线程读取 UI JSON，结合 BSD/DMS 总状态生成寄存器写入帧。 | 本地写 `/dev/ttyS1`，GPIO 切换 RS485 方向；重发计数与上次音量是循环局部变量。 | 原第 5、8 节 |
| 叉车发送 | 人脸/安全带组合决定授权，授权后按 BSD、DMS 优先级选预设语音帧。 | 语音经 26006，DO 经 26008；一些此前已授权的组合不会立即撤销授权或写关闭电平。 | 原第 6、8 节 |
| 串口、GPIO 和服务封装 | `serial.c`、`gpio.c` 及 `com_output/do_output` 执行最终本机发送。 | 本地发送调用与硬件执行之间无完整应答闭环；设备接线、电平和外部服务需现场验证。 | 原第 5～7 节 |

<a id="detail-4"></a>
### 4. 输入消息及状态更新

`skes-linker` 根据话题把数据交给 `on_skes_msg_received`。该回调目前只分派 `skes_topic_type_filter` 与 `skes_topic_type_event`；心跳、版本、设备数据和位置分支被 `#if 0` 排除。`skes_msg_header_parse` 要求顶层有字符串 `type` 和存在的 `data` 字段；`data` 可以是数组或对象，具体形状由业务解析函数使用。当前两个处理函数不按 `action` 决策。话题名 `/skes/filter`、`/skes/event` 见上级 `skes-linker.c:403-413`；头解析约束见上级 `skes-msg.c:22-55`。本文件依据：`src/sound_light_alarm_sep.c:467-492,731-795`。

#### 消息解析流程图（纯 Markdown 可显示）

```text
19226 收到 SKES 消息
  |
  +--> /skes/filter
  |      +--> JSON/头解析失败 --> 不更新
  |      +--> type != properties --> 不更新
  |      `--> data.type == FaceRecognition
  |             +--> 标记“收到人脸消息”
  |             +--> FaceRecognitionResult --> 写入识别结果
  |             `--> seatbelt_fasten --> 写入安全带状态
  |
  +--> /skes/event
  |      +--> JSON/头解析失败或 type != alarm --> 不更新
  |      `--> 遍历 data[] 中的 source
  |             +--> bsd --> 装载机总体状态 / 叉车四方位状态
  |             `--> dms --> 装载机总体状态 / 叉车总体加细分状态
  |
  `--> 其他话题 --> 当前回调无业务处理
```

文字版流程：① 按话题分派；② 解析 JSON 与消息头；③ `filter/properties` 从 `data.type=FaceRecognition` 的属性数组取 `FaceRecognitionResult` 和 `seatbelt_fasten`；④ `event/alarm` 遍历 `data` 数组，按 `source`、`status` 和子字段更新状态；⑤ 发送线程在自己的循环中读取这些字段。解析函数内的 `action`、时间戳和消息 ID 没有参与控制决策。依据：`src/sound_light_alarm_sep.c:420-795`。

| 输入 | 装载机状态更新 | 叉车状态更新 |
| --- | --- | --- |
| `filter`：`properties`，`data.type=FaceRecognition` | 也会写入人脸/安全带字段，但装载机发送逻辑不读取它们。 | 设置 `is_face_recognition_msg_received`；属性 `FaceRecognitionResult` 写入整数结果，`seatbelt_fasten` 写入布尔值。未出现的属性保留旧值。 |
| `event`：`alarm`，`source=bsd` | `status=triggered` 设 `is_bsd_trigger=true`，`normal` 设 false。 | `normal` 清空前后左右四标志；`triggered` 要求 `data[]`，仅将 `camera.status=triggered` 且 `position` 为 `front/rear/left/right` 的方位置 true，同时用本次结果覆盖四个旧方位。`left_side/right_side` 在代码中被注释，不匹配。 |
| `event`：`alarm`，`source=dms` | `status=triggered/normal` 设置/清除 `is_dms_trigger`。 | `status=triggered/normal` 设置/清除总体标志；随后只要当前总体标志为 true，就遍历 `data.states[]` 字符串（即使本条消息未提供有效 `status`，但此前总体标志仍为 true）。“打电话”“抽烟”“吃东西”“正常驾驶”分别将对应标志置 true；没有在 normal 或新告警时清理这些细分标志。 |
| 其他消息/状态 | 没有对应状态更新。 | 没有对应状态更新。 |

源头：`src/sound_light_alarm_sep.c:420-465,494-666`。`filter` 解析器在标记“收到人脸消息”后才读取属性数组，因此即使 `data` 属性数组缺失，该标志也可能已为 true。对属性值的读取没有检查 cJSON 提取函数的返回值：类型不匹配时，当前代码仍会把局部默认值 0/false 写进状态。`on_msg_filter` 和 `on_msg_event` 在业务处理后仍返回 `-1`；上级 linker 的接收函数调用回调后不检查其返回值（上级 `skes-linker.c:501-527`），因此不能简单把该返回值解释成“消息没有生效”。

<a id="detail-5"></a>
### 5. 装载机执行路径

发送线程先将 GPIO 95/94 导出并设为输出高电平，然后进入装载机循环；串口接收线程初始化 GPIO 27/26 为 RS485 读方向，打开 `/dev/ttyS1`，尝试设为 9600、8N1，使用 `select` 最多等待 3 秒并读取；读失败时关闭后重试。发送线程只有在 `serialfd != -1` 时才读 UI JSON 和发送。串口回传仅记录长度。依据：`src/sound_light_alarm_sep.c:874-938,1167-1276,1453-1469`、`src/serial.h:4-13`。

#### 装载机流程图（纯 Markdown 可显示）

```text
发送线程循环
  |
  +--> serialfd == -1? --是--> sleep(1) --> 下一轮
  |
  `--> 读取 /tmp/ui_config.json
         +--> 文件/JSON/alarm 对象失败 --> 不发送 --> sleep(1)
         `--> 成功
                +--> enabled && (DMS 或 BSD)?
                |      +--> 否 --> NORMAL --> 发送关闭帧
                |      `--> 是 --> NORMAL->TRIGGER / TRIGGER->CONTINUE
                |                    +--> TRIGGER --> 发送开启帧
                |                    `--> CONTINUE --> 看本轮音量与本轮初始值 0
                |                           +--> 音量非零 --> 回到 TRIGGER，发送开启帧
                |                           `--> 音量为零 --> 本轮计数加 1，通常不发送
                `--> sleep(1) --> 下一轮（局部计数和上次音量重新置零）
```

文字版流程：① 若串口未就绪则等待；② 从 UI JSON 读取 `enabled` 与音量；③ 没有有效配置则本轮无输出；④ 未启用或没有 DMS/BSD 总体告警时设 `ALARM_NORMAL` 并发送关闭帧；⑤ 有告警时按 `NORMAL → TRIGGER → CONTINUE` 更新状态；⑥ 在 `TRIGGER` 发送开启帧；⑦ `CONTINUE` 根据代码中的局部变量决定是否发送；⑧ 睡眠 1 秒再循环。依据：`src/sound_light_alarm_sep.c:1167-1276`。

**必须按实际代码理解重发行为。** `continue_count`、`last_volume`、`curr_trigger` 都声明在 `while` 内，每一轮都会初始化为 0/false。`CONTINUE` 阶段只要换算后音量非零，就因 `volume != last_volume(0)` 返回 `TRIGGER` 并发送；换算后音量为 0 时，`continue_count` 每轮只能从 0 加到 1，达不到 15，也不会周期重发。因此源码注释所写“等待一段时间后重发”和“音量修改后立即重发”并不能按预期跨轮成立。`curr_trigger/last_trigger` 在当前逻辑中也不影响分支。依据：`src/sound_light_alarm_sep.c:1202-1273`。

装载机命令为 19 字节的寄存器写入帧：地址 `0x01`、功能码 `0x10`、起始寄存器 `0x0010`、5 个寄存器、10 字节数据；触发帧首个数据寄存器为 `0x0003`，关闭为 `0x0000`，音量写到数组下标 10。发送前对前 17 字节计算 CRC16，最后写低字节、高字节。`send_rs485_data` 先将 GPIO 27/26 置发送方向，再调用串口写入和 `tcdrain`，最后切回读取方向。依据：`src/sound_light_alarm_sep.c:175-194,972-990,1178-1265`。源码没有检查 `send_rs485_data` 的写入长度和 `tcdrain` 返回值。

<a id="detail-6"></a>
### 6. 叉车执行路径

叉车没有在本程序中打开 `/dev/ttyS1`：发送线程经 `com_output(COM_CH_RS485, ...)` 把预设的 8 字节帧发给本机 `26006` 端口；DO 命令发往 `26008`。程序也连接 `26007` 的 RS232 发布端，但当前叉车发送路径没有调用它。串口/DO 服务是否把命令送到硬件，取决于外部服务，不由本源码保证。依据：`src/sound_light_alarm_sep.c:992-1165,1278-1451`。

#### 叉车流程图（纯 Markdown 可显示）

```text
发送线程循环
  |
  +--> 尚未初始化提示?
  |      +--> 未收到人脸消息且启动不足“超过 10 秒” --> 等待 1 秒
  |      `--> 收到消息或超过 10 秒 --> 发第 10 曲提示 + DO false + sleep(5)
  |
  +--> 本轮是否允许执行决策? （未触发声音 / last_time=0 / 当前秒>last_time+3）
  |      +--> 否 --> sleep(1)
  |      `--> 是
  |             +--> 人脸=1 且安全带=true --> 首次授权发第 5 曲；DO true
  |             +--> 人脸=1 且安全带=false --> 未授权时发第 11 曲；DO false
  |             +--> 人脸=-1 且安全带=true --> 未授权时发第 6 曲；DO false
  |             +--> 人脸=-1 且安全带=false --> 未授权时发第 12 曲；已授权则清授权并重置提示
  |             `--> 人脸为其他值 --> 已授权则清授权并重置提示
  |             
  |             上述条件处理完后，如仍已授权：按 BSD 前/后/左/右，
  |             再按 DMS 抽烟/打电话/疲劳选一条语音帧
  `--> sleep(1) --> 下一轮
```

文字版流程：① 待人脸消息或 `curr_time > start_time + 10` 后，发第 10 曲初始化提示并写一次 DO 非触发值，睡眠 5 秒；② 用 `is_sound_triggered` 和 `last_time` 控制进入决策块的间隔；③ 人脸=1 且安全带扣好时首次授权、发第 5 曲，并写 DO 触发值；④ 其他明确失败组合在未授权时分别发第 11/6/12 曲并写 DO 非触发值；⑤ 授权存在时才按固定优先级选择一条 BSD/DMS 语音；⑥ 每轮睡眠 1 秒。源码注释称“命令发送间隔 5 秒”，实际条件是 `curr_time > last_time + 3`，且初始化还有一次 5 秒睡眠；不要据注释认定固定 5 秒间隔。依据：`src/sound_light_alarm_sep.c:1285-1450`。

| 状态/条件 | 当前直接操作 |
| --- | --- |
| 人脸 1、扣安全带、原先未授权 | 第 5 曲一次，标记授权；之后在允许决策的轮次调用 `do_output(true)`。 |
| 人脸 1、未扣安全带、原先未授权 | 第 11 曲，调用 `do_output(false)`；若此前已授权，此分支没有清除授权，也不发 DO false。 |
| 人脸 -1、扣安全带、原先未授权 | 第 6 曲，调用 `do_output(false)`。若此前已授权，该分支也不清授权、不写 DO false。 |
| 人脸 -1、未扣安全带、原先未授权 | 第 12 曲，调用 `do_output(false)`。 |
| 人脸 -1、未扣安全带、此前已授权 | 清 `is_vehicle_authorized` 与 `is_sound_triggered`，将局部静态 `init` 置 false；当轮没有直接调用 `do_output(false)`，下轮重新走初始化提示。 |
| 人脸其他值、此前已授权 | 同上清授权并重置提示；当轮没有直接写 DO false。 |
| 已授权且有多种告警 | 仅选优先级最高的一项：BSD 前→后→左→右→DMS 抽烟→打电话→疲劳。 |

叉车语音帧均为 `FF 06 00 0F 01 <曲目号> <预置两字节>`；曲目 1～4 为 BSD 前后左右，5/6 为人脸通过/失败，7/8/9 为抽烟/打电话/疲劳，10 为初始化，11/12 为另外两个人脸安全带组合。最后两字节是源码预置值，发送函数没有重新计算校验。`is_dms_tired_trigger` 只在发送条件中读取，没有看到赋值为 true 的当前消息解析路径；“吃东西”“正常驾驶”可置状态但没有对应发送分支。依据：`src/sound_light_alarm_sep.c:1278-1445`。

`do_output` 构造 `do_cmd_item_t`：进程 PID、命令标签 50、递增的 8 位命令序号、内容长度、通道 `DO_CTRL_CH_2=1`、模式 0，以及由 `do_trigger_level` 决定的电平。`do_trigger_level` 非零时 `trigger=true` 发 1、false 发 0；为零时相反。参数 `channel` 没参与填充，函数固定写第 2 路。依据：`src/sound_light_alarm_sep.h:37-71`、`src/sound_light_alarm_sep.c:1121-1165`。

<a id="details"></a>
## 五、专题详解与设备排障

本部分集中实现限制、测试边界和现场排障。专题编号沿用原文，文中的“§N”“第 N 节”仍指本文同编号的专题。

<a id="detail-7"></a>
### 7. 通信、错误处理和可验证边界

| 通道 | 本程序角色 | 说明 |
| --- | --- | --- |
| `127.0.0.1:19226` | SKES 订阅 | 接收 `/skes/filter` 和 `/skes/event`；19225 是本程序建立的发布连接，但当前有效回调无业务回复发送。 |
| `127.0.0.1:26006` | nanomsg PUB 客户端 | 叉车 RS485 原始语音帧；`com_output` 非阻塞发送，`EAGAIN` 最多尝试 5 次、相邻等待 200 ms。 |
| `127.0.0.1:26007` | nanomsg PUB 客户端 | 初始化了 RS232 通道，本程序的有效报警路径未调用。 |
| `127.0.0.1:26008` | nanomsg PUB 客户端 | 叉车 DO 二进制命令；`do_output` 对 `EAGAIN` 同样最多尝试 5 次。 |
| `/dev/ttyS1` | 装载机本地串口 | 接收线程负责打开和读，发送线程共用 `serialfd` 写；GPIO 27/26 切换 RS485 收发方向。 |

`com_init`、`do_init` 连接失败后只记录/返回，`sound_light_alarm_new` 不因这几条辅助连接失败而返回错误。后续发送会检查 fd 是否仍为 `-1`。`read_alarm_config` 在打开、分配、JSON 解析或缺少 `alarm` 对象时返回失败，但不验证 `fread` 字节数及 `enabled/volume` 字段读取是否成功；字段缺失时保留调用者初始化值。依据：`src/sound_light_alarm_sep.c:940-1165`。

<a id="detail-8"></a>
### 8. 源码中可直接确认的限制与风险

1. **装载机持续告警的重发状态没有跨轮保存。** `continue_count` 和 `last_volume` 的作用域在循环内；详见第 5 节。这是实际控制流，不是仅凭注释推断。
2. **叉车告警细分状态可能保留旧值。** DMS 的 `normal` 只清总体 `is_dms_trigger`，发送条件却直接读取 `is_dms_smoke_trigger` 等细分标志；BSD 的 `normal` 则清四方位。依据：`src/sound_light_alarm_sep.c:494-574,624-661,1408-1445`。
3. **叉车授权状态有未覆盖组合。** 人脸=1、安全带=false，或人脸=-1、安全带=true，且此前已授权时，都不会撤销授权；之后仍可进入告警语音选择。人脸=-1 且安全带=false、或人脸为其他值时，若此前已授权则只是重置本进程标志，DO 非触发值要等后续初始化路径才发送。依据：`src/sound_light_alarm_sep.c:1353-1446`。
4. **启动失败与退出资源不完整。** `new` 即使 `skes_linker_init` 失败，仍继续读取配置、连接辅助端口，随后把初始化错误返回 `main`；`main` 直接返回，没有调用 `release`。`run` 逐个创建线程，任一步失败即返回，既不停止已创建的线程，也不释放对象；`main` 同样直接返回。进程退出后操作系统会回收进程资源，但该路径没有程序内的有序清理。`run` 创建发送线程后又把接收线程 ID 写入同一 `com_pid`，`stop` 只能按保存的 ID 等待其中一个线程；`main` 不调用 `stop`，运行成功后也没有主动结束循环的条件。即使外部调用 `stop`，轮询线程设置了无限等待，`stop` 没有调用 linker 的 `skes_break` 唤醒它，`pthread_join(sre->pid)` 可能一直阻塞；普通消息到来后 `skes_run` 仍在内部循环，不会因为处理一条消息就返回。`release` 释放 linker 和对象，但本文件没有在其中关闭已打开的 `serialfd`、`com0_nn_fd`、`com1_nn_fd`、`do_nn_fd`。依据：`src/main.c:104-129`、`src/sound_light_alarm_sep.c:830-853,1476-1529`；上级 `skes-linker.c:530-628`。
5. **部分函数或参数未进入有效业务路径。** `calc_crc16` 未被调用；`on_msg_event_parse` 未被调用，且其内部叉车判断嵌在装载机分支下；公开头文件的 CAN/AEB 常量和结构在本程序有效逻辑中未使用；`do_output` 的 `channel` 参数未使用。`on_msg_face_recognition_filter_parse` 和 `on_msg_alarm_parse` 声明返回 `int`，但正常走到函数末尾没有返回语句；调用处也未使用它们的返回值。依据：`src/sound_light_alarm_sep.c:420-465,576-666,669-729,855-873,1121-1138`、`src/sound_light_alarm_sep.h:22-107`。
6. **串口和 GPIO 错误路径缺少完整校验。** `setup_serial` 的形参类型是 `char`，调用处传 `int serialfd`，若文件描述符超出 `char` 可表达范围会发生截断；调用处未检查配置结果。`gpio_init` 在 `fopen` 失败后仍调用 `fclose(fp)`；若确实打开失败，存在无效指针调用。`gpio_set_value` 写 3 字节，但格式化出的电平字符串只有 1 字节加终止符。依据：`src/serial.c:22-24,161-169`、`src/sound_light_alarm_sep.c:888-896`、`src/gpio.c:77-89,206-226`。
7. **同条事件中多个告警项的遍历存在干扰。** `on_msg_alarm_parse` 用变量 `i` 遍历外层 `data[]`；在叉车 DMS 的 `states[]` 循环中又用同一个 `i`。内层结束后外层自增，可能跳过后续告警项，或从较小索引重复处理；例如 DMS 项在外层索引 2、`states` 只有 1 项时，处理完后外层索引又回到 2，可反复处理同一项而无法结束该次回调。依据：`src/sound_light_alarm_sep.c:588-590,624-665`。
8. **发送结果与业务状态没有形成闭环。** 叉车分支不检查 `com_output` 的返回值；`do_output` 失败时只记录日志，业务侧仍按原路径更新授权/声音标志。故“已发送”在文档中表示调用了发送函数，不能推定设备收到或执行。依据：`src/sound_light_alarm_sep.c:1051-1093,1121-1165,1353-1445`。
9. **日志信号处理和测试覆盖存在边界。** `SIGUSR1` 处理函数调用日志相关函数，源码注释已标记重入问题；`SLATest` 每 5 秒循环发布一条总体 DMS/BSD `triggered/normal` 样例，无自动断言，也不提供叉车 BSD `camera` 数组或人脸属性消息，不能证明叉车分支、DO 和硬件输出正确。依据：`src/main.c:73-83`、`tests/test.cpp:13-92`、`tests/CMakeLists.txt:18-23`。

<a id="detail-9"></a>
### 9. 核验依据与使用方式

本分析逐段核对入口、消息解析、状态机、两种设备发送路径、串口/GPIO 封装、头文件、CMake/INI 和手工测试；用上级 `skes-linker` 的源码核对话题名、消息头约束和回调返回值处理。它是一份静态源码分析，真实报警器的语音编号、电平接线、串口服务行为和目标板输出仍需结合设备协议与现场验证。阅读或修改时，建议从 `src/main.c` → `sound_light_alarm_new/run` → `on_skes_msg_received` → `do_com_msg_sender` 的顺序进入，再按装载机/叉车分支查对应文件和行号。

<a id="detail-10"></a>
### 10. 设备故障排查手册

本节用于设备上怀疑声光报警异常时定位故障点。排查顺序从可观察事实出发：先确认运行的是哪个程序和配置，再确认上游消息是否满足本程序解析条件，最后检查输出通道与硬件。示例中的 `APP_DIR` 与 `PID` 应填入现场实际值；安装后的绝对路径不能仅凭本仓库源码推定。下列命令用于查看状态，不会主动触发报警。

#### 10.1 快速分流图（纯 Markdown 可显示）

```text
设备声光报警异常
  |
  +--> 进程不存在/频繁重启?
  |      `--> 查 sep.ini enabled、启动目录、INI、依赖、启动输出
  |
  +--> 进程存在但完全无响应?
  |      +--> 装载机 --> /tmp/ui_config.json、19226 消息、/dev/ttyS1、GPIO
  |      `--> 叉车   --> 人脸/安全带属性、BSD/DMS 事件、26006/26008
  |
  +--> 有日志显示发送，设备没动作?
  |      +--> 装载机 --> 串口描述符/串口参数/RS485 方向/设备侧
  |      `--> 叉车   --> nanomsg 对端、DO 命令服务、外部串口与继电器
  |
  `--> 错误动作/报警不消失/重复播报?
         +--> 核对输入状态和设备类型
         `--> 对照第 5/6/8 节的循环局部变量、旧告警位和授权分支
```

文字版流程：① 先核对症状出现时间、设备类型、应用进程和实际工作目录；② 再核对配置与上游消息；③ 区分“程序未生成指令”“程序调用发送函数但对端未执行”“硬件执行却不符合预期”；④ 按装载机或叉车对应路径逐级取证；⑤ 若现象吻合第 8 节已确认的控制流问题，保留原始消息与日志，再定位源码分支。不要仅凭界面显示或一条“发送成功”日志判断硬件已动作。

#### 10.2 第一步：确认进程、版本和启动目录

在设备上先执行只读检查：

```sh
ps -ef | grep '[s]ound_light_alarm_sep'
ps -ef | grep '[s]kes-sepd'
ps -ef | grep '[s]kes-relay'
PID=1234  # 改成上面查到的程序 PID
readlink -f /proc/"$PID"/exe
readlink -f /proc/"$PID"/cwd
ls -l /proc/"$PID"/task
```

- 进程不在：核对应用目录内 `sep.ini` 的 `[sep] enabled` 与 `name`，以及应用目录中是否有可执行文件。仓库的 `skes-sepd.sh` 扫描 `sep.ini`，启用时先 `cd` 到应用目录，再运行对应程序，每 5 秒复查一次；因此现场看到“进程又起来了”可能是守护脚本行为。源码依据：`skes-scripts/skes-sepd.sh:116-159`。
- 进程在：用 `/proc/$PID/exe` 核对实际二进制，用 `/proc/$PID/cwd` 核对相对路径基准。不要只用文档中的源码路径推断设备运行目录。若没有 `/proc/$PID` 读取权限，应以设备管理员身份查看。
- 线程异常少或频繁变化：本程序正常会尝试启动 3 个工作线程和主线程；叉车的“串口接收线程”创建后会立即返回，因此活动线程数不能机械地按 4 判定故障。若消息轮询线程卡在解析回调，结合第 8 节的 DMS 外层索引复用问题检查 CPU 使用情况及原始告警数组。源码依据：`src/sound_light_alarm_sep.c:874-886,1476-1503`。
- 源码版本宏 `0.1.1.0` 不能唯一识别 Git 提交。核对设备包、编译产物和部署记录，必要时比较二进制摘要；不能仅看分支名或日志版本号认定运行的是本文对应源码。

#### 10.3 第二步：核对配置与日志

```sh
APP_DIR='/替换为设备上的应用目录'
cat "$APP_DIR"/sep.ini
cat "$APP_DIR"/sound_light_alarm_sep.ini
ls -l "$APP_DIR"/sound_light_alarm_sep.log*
tail -n 100 "$APP_DIR"/sound_light_alarm_sep.log
```

- `dev.type` 应与实际设备一致：`wheel_loader` 走本地串口，`forklift` 走本机消息服务；`crawler_crane` 虽能解析但没有有效发送分支。缺失或非法类型时，初始化默认仍是 `wheel_loader`。启动时 `init_log` 会回写 `[log] level`；若原 INI 不存在，底层 `skes_set_ini_value` 可能新建一个只有日志段的文件，因此“INI 文件存在”不等于设备类型已正确配置。`SLA_TYPE` 只决定安装哪份 INI，运行时由 `dev.type` 决定逻辑。依据：`src/sound_light_alarm_sep.c:127-157,797-827,1453-1474`、`src/main.c:62-70`、`CMakeLists.txt:70-82`、上级 `skes-core/skes-utils/utils.c:981-1027`。
- `do_trigger_level` 只影响叉车 DO 有效电平，非零为触发高电平，零为触发低电平；装载机 INI 不含此项，读失败不会阻止程序运行。核对值时应同时核对继电器或输入模块的实际有效电平。依据：`src/sound_light_alarm_sep.c:160-173,1121-1143`。
- 日志文件名是相对路径 `sound_light_alarm_sep.log`，在守护脚本启动方式下通常位于应用目录。日志级别 `4` 为 ERROR，`0` 才包含 DEBUG；因此看不到 `bsd triggered`、`vehicle authorized`、`tx serial data` 等 DEBUG 记录并不等于没有处理消息。`SIGUSR1` 可在运行中循环切换级别，但只修改进程内级别，不调用 `save_log_level`，重启后仍以 INI 为准；当前信号处理函数使用日志函数，源码已标记重入问题；如需详细日志，应在受控排查时记录原 INI 值、调整 `[log] level` 后通过既有启动流程重启，取证后恢复。依据：`src/main.c:34-83`、`skes-3rd-party/skes-log/skes-log.h:223-230`。
- 优先找这些可直接对应源码的线索：`load 'sound_light_alarm_sep.ini' failed`、`invalid device type`、`invalid JSON`、`missing type or data`、`recv error`、`select error`、`nn_fd not init`、`do_nn_fd not init`、`nn_send ... failed`。`com_init` 的 `nn_connect` 失败部分用 `printf` 写标准输出，不保证出现在本程序文件日志中；需同时查看启动服务的标准输出记录。依据：`src/main.c:43-71`、`src/sound_light_alarm_sep.c:114-120,141-173,884-938,992-1165`。

#### 10.4 第三步：确认上游输入是否满足解析条件

先确认 `skes-relay` 在运行、本机端口是否建立，再收集故障时段的上游原始消息。只看 TCP 端口或进程存在，无法证明符合 JSON 格式的消息已到达本程序。`skes-relay.json` 的发布端为 `19226`、订阅端为 `19225`，与本程序的订阅/发布连接方向相对。可用设备自带的 `ss` 查看监听和连接；若没有则用 `netstat`。例如 `ss -lntp` 与 `ss -ntp`，重点核对 `19225/19226/26006/26008`。监听或连接存在只说明 TCP 层状态，这些命令不解码 nanomsg 消息，也不能证明业务数据已到达。依据：`skes-core/skes-relay/skes-relay.json:7-14`、`src/sound_light_alarm_sep.c:797-811`。

| 需要核对的原始内容 | 源码接受条件 | 不满足时的表现 |
| --- | --- | --- |
| 话题和顶层字段 | 话题 `/skes/filter` 或 `/skes/event`；顶层 `type` 是字符串且存在 `data`。`action` 当前不参与判断。 | 其他话题无业务处理；JSON/头不合法时记录 ERROR 并不更新状态。 |
| 人脸与安全带 | `/skes/filter`，`type=properties`，`data.type=FaceRecognition`，`data.data[]` 中有 `property_name=FaceRecognitionResult` 或 `seatbelt_fasten`；值分别应为数值和布尔值。 | 缺字段保留旧值；类型错误时局部默认值 0/false 仍会写入对应状态。仅匹配 `FaceRecognition` 即可将“已收到人脸消息”置 true。 |
| 装载机 BSD/DMS | `/skes/event`、`type=alarm`、`data[]` 中 `source=bsd/dms`，`status=triggered/normal`。 | 未设置对应总体告警位；未知状态不会自动恢复旧告警。 |
| 叉车 BSD | 同为 `alarm`，还要在每个 BSD 项的 `data[]` 中提供 `camera.position=front/rear/left/right` 和 `camera.status=triggered`；总体 `status=normal` 清四方位。 | 只有总体 `status=triggered` 而 `data` 字段缺失时，本次解析返回，旧方位保持不变；`data` 存在但为空或无有效 `camera` 项时，四方位会被本次空结果清零。 |
| 叉车 DMS | `source=dms`，`status=triggered`，`data.states[]` 是“打电话”“抽烟”“吃东西”“正常驾驶”等字符串。 | 只有总体触发而没有细分字符串时，没有对应语音选择；`normal` 不清细分标志，可能残留。 |

证据：`src/sound_light_alarm_sep.c:420-666,731-795`、上级 `skes-linker.c:403-413`、`skes-msg.c:22-55`。若只有界面报警、没有符合条件的 SKES 事件，应先查上游识别程序与 `skes-relay`；若原始消息已进入本程序而状态/选择不符，再定位本程序解析和分支。

#### 10.5 装载机：从 UI 配置追到本地串口

1. 用 `ls -l /tmp/ui_config.json` 和 `cat /tmp/ui_config.json` 核对临时文件是否存在、是否为完整 JSON，尤其是 `alarm.enabled` 和 `alarm.volume`。装载机串口已打开时，发送循环每轮重新读取此文件；没有 `alarm` 对象或解析失败时本轮没有新指令，缺少子字段则落到调用者初始化值 false/0。仓库中的 `skes-main/SettingsManager` 会生成该临时文件，持久化配置路径为 `/root/cameras/ui_config.json`；现场还应确认运行的是相应 UI。仅持久化文件存在，不能证明临时文件当前有效。依据：`src/sound_light_alarm_sep.c:940-970,1202-1215`、`skes-ui/skes-main/SettingsManager.cpp:227-245,295-348`。
2. 核对 `/dev/ttyS1`、串口描述符与 GPIO 文件。可只读查看：

   ```sh
   ls -l /dev/ttyS1
   ls -l /proc/"$PID"/fd
   cat /sys/class/gpio/gpio27/direction
   cat /sys/class/gpio/gpio26/direction
   cat /sys/class/gpio/gpio95/value
   cat /sys/class/gpio/gpio94/value
   ```

   `gpio27/26` 是 RS485 方向控制，空闲时应由代码切回读方向；95/94 在装载机发送线程启动时尝试设为高电平。若 GPIO 文件缺失或权限不符，检查导出/方向设置阶段的错误输出。源码的 `gpio_init` 错误路径也存在空指针 `fclose` 风险，不能只把启动失败归为硬件故障。依据：`src/sound_light_alarm_sep.c:46-57,175-194,874-938,1453-1469`、`src/gpio.c:77-89`。
3. 若无串口 fd，核对 `open uart ... failed`（标准输出）及设备节点权限；若日志有 `recv error`/`select error`，程序会关闭串口并重试。若 fd 存在仍无输出，核对 UART 9600、8N1、RS485 方向、报文 19 字节与报警器协议。源码对 `setup_serial` 和实际写入字节数未作完整校验；“tx serial data”是调用前的 DEBUG 日志，不证明设备收到帧。依据：`src/serial.c:9-20,22-169,171-190`、`src/sound_light_alarm_sep.c:888-938,1230-1265`。
4. 若出现持续重复播放或不能按注释周期重发，先对照第 5 节：`continue_count`、`last_volume` 每轮重置。换算后音量非零可每轮触发；换算后为零则计数不能累积到 15。UI 值的计算结果也没有在本程序中钳制到 0～30。依据：`src/sound_light_alarm_sep.c:940-960,1202-1273`。

#### 10.6 叉车：从授权状态追到消息服务与 DO

1. 先核对是否收到 `FaceRecognitionResult` 和 `seatbelt_fasten`，以及初始化提示是否发出。收到 `data.type=FaceRecognition` 或启动时间严格超过 10 秒时，程序才调用第 10 曲提示与 `do_output(false)`；之后还睡眠 5 秒。人脸 1 且扣安全带才会首次标记授权；只有授权后才选 BSD/DMS 方位语音。依据：`src/sound_light_alarm_sep.c:420-465,1324-1446`。
2. 核对本机 `26006` 与 `26008` 的对端服务是否在运行。`26006` 是 RS485 语音帧的 nanomsg 连接，`26008` 是 DO 命令连接；本程序对 `26007` 也建立连接，但有效报警路径不使用。`nn_fd not init`、`do_nn_fd not init` 表示本程序保存的发送 fd 没初始化成功；`nn_send ... failed` 表示发送调用失败。即使 `nn_send` 返回成功，也仅能说明本机消息调用成功，不等于语音模块或继电器实际动作。依据：`src/sound_light_alarm_sep.c:992-1165,1278-1445`。
3. 叉车 DO 指令固定发通道 `DO_CTRL_CH_2=1`、命令标签 50。若继电器状态与预期相反，核对 INI 的 `do_trigger_level` 与外部服务/接线约定；零表示触发时发送低电平，非零表示触发时发送高电平。若人脸状态变化后授权没有撤销，先查第 6/8 节的组合分支：人脸 1 未扣安全带或人脸 -1 已扣安全带、此前已授权时，当前代码不清授权。依据：`src/sound_light_alarm_sep.h:37-71`、`src/sound_light_alarm_sep.c:1121-1165,1353-1406`。
4. 若某一方向不播，核对上游 BSD 子数组中 `camera.position` 的确切字符串，代码只识别 `front/rear/left/right`，再核对优先级（前、后、左、右、抽烟、打电话、疲劳）；高优先级一直触发会挡住低优先级语音。若 DMS 告警已恢复仍重播，核对细分状态未清除的代码路径。若收到包含多个告警项的事件后处理卡住，核对 DMS 内层复用外层索引的情况，详见第 8 节。依据：`src/sound_light_alarm_sep.c:494-666,1408-1445`。
5. 本次核验的应用源码不含 `26006/26008` 对端服务的完整实现；本分析只能把界限划到本程序的 `nn_send`。要证明命令到达硬件，需结合对端服务日志、串口总线记录或设备实际响应，不能从本程序日志单独推出。

#### 10.7 常见现象、定位点和取证材料

| 现场现象 | 先看哪里 | 对照的源码原因/下一步 |
| --- | --- | --- |
| 程序未启动或反复启动 | `sep.ini`、实际 cwd、标准输出、INI 和动态库 | 守护脚本按 `enabled` 拉起；`new/run` 失败时 `main` 返回错误，失败路径未有序清理已建立的连接/线程。确认是启动链路、linker 初始化还是线程创建失败，结合标准输出和错误日志定位。 |
| 程序在、无任何报警 | `dev.type`、上游 `/skes/event`、装载机 UI 临时 JSON、叉车人脸属性 | 配置决定设备路径；消息不符合 `type/source/status` 条件不会更新业务状态。 |
| 界面有报警而装载机无声 | `/tmp/ui_config.json`、`/dev/ttyS1`、GPIO 27/26、报警器侧 | UI 显示不等于此进程读到有效临时 JSON 或成功写出串口帧。 |
| 叉车有初始化提示但后续不报 | 人脸 1/安全带、BSD 方位数组、DMS 字符串、优先级 | 发送分支要求授权；缺少细分状态或被更高优先级挡住时不会播期望语音。 |
| 日志显示发送但硬件不动作 | `26006/26008` 对端服务、串口/DO 实际输出 | 发送函数返回不代表硬件执行；本程序没有设备应答闭环。 |
| 报警不消失、错播或高 CPU | DMS 细分标志、授权组合、同一事件的数组顺序 | 第 8 节列出了旧标志、未撤销授权和循环变量复用。 |
| 只有 ERROR 没有状态日志 | `[log] level`、实际日志文件/标准输出 | 默认配置为 4，绝大多数分支状态记录为 DEBUG。 |

建议留存同一时段的：设备型号及 `dev.type`、进程路径与 cwd、二进制摘要/部署版本、`sep.ini` 和设备 INI、`/tmp/ui_config.json` 中的 `alarm` 字段、原始 SKES 话题及 JSON、应用与 relay 日志、对端串口/DO 服务日志、设备侧实际响应时间。对比时统一时间基准；`tests/test.cpp` 会循环主动发送告警样例，不能在真实运行设备上把它当作无副作用的“只读测试”。
