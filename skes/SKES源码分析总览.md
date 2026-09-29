# SKES 六个应用源码分析总览

> 范围：本目录中除 `skes-dataengine/` 之外的六篇源码分析。对应源码位于 `/home/tronlong/lyp/code/skes/skes-apps/`，分析基线以各篇报告顶部记录为准。本文件只整理六个应用的关系与阅读入口；设备是否同时部署、启用和连通，应以现场配置及证据确认。

## 文档目录

1. [一、范围与阅读顺序](#overview)
2. [二、文档与源码目录总览](#source-tree)
3. [三、核心架构设计](#architecture)
4. [四、核心模块深度分析](#modules)
5. [五、构建和部署边界](#build)
6. [六、核验方法与限制](#verification)

<a id="overview"></a>
## 一、范围与阅读顺序

这六篇文档分析的是六个独立可执行应用，不是 SKES 仓库全部模块。建议先看本总览确定程序和接口边界，再进入对应报告的“源码目录总览→核心架构设计→核心模块深度分析”，最后按具体问题阅读原有的专题与设备排障章节。`skes-dataengine/` 不在本文件的比较或汇总范围。

| 要了解的问题 | 对应报告 |
| --- | --- |
| 平台 808 接入、注册、心跳、录像查询与告警附件 | [jtt_808_1078 源码分析](jtt_808_1078/jtt_808_1078_源码分析.md) |
| H.264 取流、1078 预览/回放、SD 卡录像与存储告警 | [aibox_stream 源码分析](aibox_stream/aibox_stream_源码分析.md) |
| 共享内存图像、人脸识别、投票及安全带状态 | [sep-faces 源码分析](sep-faces/sep-faces_源码分析.md) |
| 四个 CAN ID 到五个 SKES 属性、属性查询应答 | [filter_sep 源码分析](filter-sep/filter_sep_源码分析.md) |
| AEB/故障/配重到车辆 CAN 输出 | [skes-aeb2vcu 源码分析](skes-aeb2vcu/skes-aeb2vcu_源码分析.md) |
| 人脸授权、BSD/DMS 到声光与 DO 输出 | [sound_light_alarm_sep 源码分析](sound_light_alarm/sound_light_alarm_sep_源码分析.md) |

<a id="source-tree"></a>
## 二、文档与源码目录总览

```text
本目录 skes/                         源码仓库 skes/skes-apps/
├── SKES源码分析总览.md               ├── jtt_808_1078/        平台通信与业务协调
├── jtt_808_1078/                    ├── aibox_stream/        视频流与录像
├── aibox_stream/                    ├── sep-faces/           人脸识别
├── sep-faces/                       ├── filter_sep/          CAN 属性转换
├── filter-sep/                      ├── skes-aeb2vcu/       车辆控制消息桥接
├── skes-aeb2vcu/                   └── sound_light_alarm_sep/ 声光报警
└── sound_light_alarm/
```

文档目录名 `filter-sep`、`sound_light_alarm` 与源码目录/可执行文件名不完全相同，定位源码时使用右栏名称。六篇报告各自列出更细的源码树、入口文件及未启用路径；本表仅作导航。各应用还依赖 `skes-core/`、`skes-3rd-party/`、`skes-ui/` 或外部服务的部分接口，不能把这些依赖都归为六个应用自身的实现。

<a id="architecture"></a>
## 三、核心架构设计

下图按接口分为四条可从六篇文档核对的业务链。图中的箭头是源码中的输入输出或兼容接口，不表示这些应用在同一设备上必定同时启动。

```text
上游 H.264 生产者 ──> aibox_stream <──本机 UDP──> jtt_808_1078 <──TCP──> 808 平台
                           └──> SD 卡 MP4                ├──> 录像检索/FTP
                           └──> 1078/TCP 视频            └──> 告警抓图/附件

上游 BGR 共享内存 ──> sep-faces ── /skes/filter FaceRecognition ──> SKES relay
BSD/DMS 上游事件 ── /skes/event ──────────────────────────────> SKES relay
                                                               └──> sound_light_alarm_sep
                                                                    ├──> 本地 RS485/GPIO
                                                                    └──> 串口/DO 服务

CAN 发布端 ──> filter_sep ── /skes/filter filter_table ──> SKES relay / 消费者
AEB 属性、故障/BSD/配重事件 ──> skes-aeb2vcu ── can_frame_t ──> CAN0 TX 服务 ──> VCU
```

`filter_sep` 发布的 `filter_table` 与 `skes-aeb2vcu` 接受的 `AEB` 属性不是同一种消息，不能只因为都使用 `/skes/filter` 就把二者画成直接调用关系。`sep-faces` 的 `FaceRecognition` 属性符合 `sound_light_alarm_sep` 的输入解析条件；实际到达仍取决于 relay、部署和运行状态。`jtt_808_1078` 与 `aibox_stream` 的 UDP 控制、回复端口分别由两篇报告核对；平台应答与画面送达也是不同层次的结果。

上述跨应用连线的直接源码依据如下。路径均相对于源码仓库根目录；表中只确认生产端与消费端的接口条件，不能据此认定现场已经连通。

| 连线 | 生产端证据 | 消费端证据 |
| --- | --- | --- |
| `jtt_808_1078` ↔ `aibox_stream` | `skes-apps/jtt_808_1078/src/udp/udp_server_stream.cpp:24-25,166-197`：向本机 1210～1219 发送控制，接收端口为 1236。 | `skes-apps/aibox_stream/src/udp/udp_server.cpp:21-22,41-74,109-128`：接收本机控制并把回复发往 1236。 |
| `sep-faces` → `sound_light_alarm_sep` | `skes-apps/sep-faces/src/sep-face.cc:143-180,1247-1266`：周期发布 `FaceRecognitionResult` 和 `seatbelt_fasten`，符合条件时附 `face_id`。 | `skes-apps/sound_light_alarm_sep/src/sound_light_alarm_sep.c:420-464`：消费 `FaceRecognition` 中的识别结果与安全带布尔值；当前解析函数不读取 `face_id`。 |
| `filter_sep` 与 `skes-aeb2vcu` 的属性区别 | `skes-apps/filter_sep/src/filter_pub/filter_pubsub.cpp:312-329,390-399`：发布 `data.type=filter_table`。 | `skes-apps/skes-aeb2vcu/skes-aeb2vcu.c:477-499`：过滤条件为 `data.type=AEB`。 |
| `skes-aeb2vcu` → CAN0 TX 服务 | `skes-apps/skes-aeb2vcu/skes-aeb2vcu.c:1374-1402,1499-1502`：连接并发送整个 `can_frame_t`。 | 本次只核对发送端和 `skes-apps/skes-aeb2vcu/skes-aeb2vcu.h:26,34-42` 中的 26002 端口/结构定义；CAN 服务和物理 VCU 的实际接收须现场验证。 |

<a id="modules"></a>
## 四、核心模块深度分析

### 4.1 平台通信与视频链

[jtt_808_1078](jtt_808_1078/jtt_808_1078_源码分析.md) 以 `808_config.json` 建平台槽和 TCP 会话，处理 2013/2019 报文、周期上报与下行命令。视频命令交由 [aibox_stream](aibox_stream/aibox_stream_源码分析.md) 的通道 UDP 入口；后者从共享内存取已编码 H.264，发预览或写 MP4，回放再从已录 MP4 取包。双方文档都区分本机控制确认、平台 TCP 数据和真实画面效果；录像目录、通道索引与异常时间目录须按各自实现核对。

### 4.2 图像识别与声光链

[sep-faces](sep-faces/sep-faces_源码分析.md) 从 `/picshare<channel>` 获取 BGR 帧，用照片内存图库与多帧投票形成 `FaceRecognitionResult`、按条件附带的 `face_id` 和安全带属性。[sound_light_alarm_sep](sound_light_alarm/sound_light_alarm_sep_源码分析.md) 解析其中的识别结果、安全带属性以及 BSD/DMS 事件；装载机还检查 UI 配置并直接写串口，叉车根据人脸与安全带状态发送初始化/失败提示或完成授权，授权后才按 BSD/DMS 状态选告警语音。两端分别有模型、DI、消息、外部硬件等边界，单条发布或发送日志不能证明声光设备已动作。

### 4.3 CAN 属性与车辆控制链

[filter_sep](filter-sep/filter_sep_源码分析.md) 将四个目标 CAN ID 的字段转为五个 `filter_table` 属性，同时处理查询与应答；有效数据门槛、位提取和 JSON 契约都在其报告中。[skes-aeb2vcu](skes-aeb2vcu/skes-aeb2vcu_源码分析.md) 则根据 AEB 属性、故障/BSD/配重事件及 DI 方向生成设备类型对应的 `can_frame_t`，发给 CAN0 TX 服务。二者是不同的输入输出链，不能把 `filter_table` 直接等同为 `skes-aeb2vcu` 的 `AEB` 输入。

<a id="build"></a>
## 五、构建和部署边界

源码仓库的 `skes-apps/CMakeLists.txt` 纳入 `jtt_808_1078`、`aibox_stream`、`skes-aeb2vcu`、`filter_sep` 和 `sound_light_alarm_sep`；`sep-faces` 只在目标处理器不是 x86/x86_64 时纳入。当前仓库模板的 `sep.ini` 中，除 `sep-faces` 的 `enabled=false` 外，其余五个应用均为 `enabled=true`；**构建纳入、模板启用和设备实际运行是三件事**。`aibox_stream` 模板参数为 `f 8`，其余五个模板 `param` 为空。部署绝对路径、外部服务是否存在和设备二进制版本均须在目标环境核对。

<a id="verification"></a>
## 六、核验方法与限制

各篇报告依据核验时的源码、配置、构建清单及可达入口整理，并保留文件行号、未启用代码和需要目标板验证的限制。2026-09-29 检查时源码仓库 `main_ui` 的 HEAD 为 `d79a399`；相对于多篇报告记录的 `b28d0b8`，六个应用目录、所查的 SKES 工具与消息库、摄像头共享内存生产文件及上级 `skes-apps/CMakeLists.txt` 没有已提交差异。当前工作树的上级 CMake 另有启用 `skes-dataengine` 的未提交改动，本总览不分析该应用。

本总览仅归纳这些已有静态核验结论；没有运行六个应用、注入车辆控制指令、验证真实平台协议或观察物理报警器。跨进程问题应保存同一时间段的上游输入、本进程处理证据和下游接收证据，再按各篇“设备排障”章节定位首个失败节点。
