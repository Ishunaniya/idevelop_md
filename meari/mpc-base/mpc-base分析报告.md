# mpc-base 仓库深度分析报告

> 原稿分析时间：2026 年 2 月 | 源码核验与补充：2026 年 9 月 30 日
> 核验源码：`/home/tronlong/lyp/meari/mpc-base`。

---

## 一、仓库概述

`mpc-base` 是一套嵌入式 **C/C++ 中间件框架**，保留 NVR（Network Video Recorder，网络录像机）的命名和通道管理接口，同时包含本地摄像头采集、编码、电池/MCU、AOV，以及套装 NVR 和独立 App 服务路径。它提供硬件抽象、媒体适配、AI 结果处理、网络通信和低功耗任务协调，是上层应用与设备能力之间的接口层；媒体 SDK、检测模型、录像存储和产品 App 等能力还依赖配套工程。

下文的“套装模式”指相机向配套NVR提供服务，“独立App模式”指通过产品App组件提供服务，源码分别用 `KOA_KIT`、`KOA_APP` 区分。模块接口、编译条件和运行时设备能力共同决定一项功能是否参与当前产品。

### 核心定位

```
┌────────────────────────────────────────────────┐
│         上层应用 (UI / App / 客户端)            │
├─────────────────────┬──────────────────────────┤
│  netclient（外部接口）│  modclient（内部接口）   │
├─────────────────────┴──────────────────────────┤
│              mpc-base 核心框架层                │
│  base / ipcm / dsp / ai / aov / nets / param   │
├────────────────────────────────────────────────┤
│           Hardware / OS / SDK 层               │
│   HAL / MCU / LED / PTZ / WiFi / 4G / BLE     │
└────────────────────────────────────────────────┘
```

---

## 二、支持平台

| 平台标识   | 说明                         | 构建脚本       |
|------------|------------------------------|----------------|
| `t32zmc`   | Ingenic T32，MIPS 交叉编译 | build/t32zmc   |
| `t32zmc_4g`| T32，增加 `NV_NOMCU` 分支 | build/t32zmc_4g|
| `t41zm`    | Ingenic T41 平台              | build/t41zm    |
| `x86`      | x86 Linux（PC 调试用）        | build/x86      |

编译方式：从源码根目录执行 `source build/x86`，再执行 `make all`。其他平台替换为对应的 `build/` 脚本。编译中间产物位于 `.build/`，库按 `NV_CPU_ARCH` 分目录，并复制到产品 SDK 的 `NV_LIBDIR`；可执行文件另复制到产品目录。

`t32zmc_4g` 在 `t32zmc` 配置基础上增加 `NV_NOMCU`，选择无MCU实现；4G实际可用性还要看设备能力和驱动。前三份脚本依赖兄弟 `mpc-pack`，x86 依赖 `ipc-pack`。构建目标、宏和依赖缺口在第八章展开。[t32zmc_4g:1](/home/tronlong/lyp/meari/mpc-base/build/t32zmc_4g:1)、[x86:1](/home/tronlong/lyp/meari/mpc-base/build/x86:1)。

---

## 三、目录结构总览

```
mpc-base/
├── build/          # 各平台编译配置（交叉编译工具链、FLAGS）
├── bin/            # 构建输出路径（当前快照未包含）
├── lib/            # 构建输出路径（当前快照未包含）
├── inc/            # 依赖/导出头文件路径（当前快照未包含）
├── doc/            # 开发文档（Development_Log.md、架构图）
│
├── mod/            # 核心业务模块（主体）
│   ├── base/       # 系统入口、总控 API
│   ├── ipcm/       # IP Camera 管理（通道管理核心）
│   ├── dsp/        # 媒体码流处理（解码/编码/帧管理）
│   ├── ai/         # AI 智能侦测（人形、烟火等）
│   ├── aov/        # AOV 任务调度（低功耗管理）
│   ├── nets/       # 私有协议网络服务端
│   ├── rpcapi/     # RPC 跨进程通信（Server + Client）
│   ├── param/      # 系统参数持久化管理
│   ├── event/      # 事件分发系统
│   ├── sys/        # 系统服务（看门狗、升级、时间）
│   ├── rtsps/      # RTSP 流媒体服务器
│   ├── preview/    # 实时预览
│   ├── playback/   # 录像回放
│   ├── 4g/         # 4G 模块管理
│   ├── lte/        # LTE 模块管理
│   ├── wlan/       # WiFi 模块管理
│   ├── wlan_client/# WiFi 客户端管理
│   ├── wired/      # 有线网络管理
│   ├── net_access/ # 网络接入状态管理
│   ├── net_wireless/# 无线资源需求/状态/异常管理
│   ├── bluetooth/  # BLE 蓝牙管理
│   ├── mcu/        # 单片机通信管理
│   ├── nkit/       # 套装 UDP 命令及码流服务
│   ├── discovery/  # 设备发现（局域网）
│   ├── hal/        # 硬件抽象层（MTD/加密/串口）
│   ├── ptz/        # PTZ 云台控制
│   ├── led/        # LED 状态灯控制
│   ├── button/     # 按键事件处理
│   ├── speaker/    # 喇叭/音频输出
│   ├── sound/      # 设备提示音/警示播放
│   ├── ftp/        # FTP 定时抓图上传
│   ├── qrcode/     # 扫码与配网数据解析
│   └── upgrade/    # 固件升级适配
│
├── modclient/      # Unix Socket RPC 调试程序
├── netclient/      # 外部接口（Socket，需用户鉴权）
│
└── util/           # 内部工具库
    ├── auth/       # Base64 编码 + MD5 摘要
    ├── cjson/      # JSON 解析（cJSON + s2j）
    ├── libnvrtools/# 基础工具（线程/锁/调试/网络/文件）
    │   ├── comm/   # 公共工具集合
    │   ├── file/   # 文件读写
    │   └── net/    # iKCP 可靠 UDP 协议
    ├── cyclone/    # 异步网络库（epoll/kqueue/select）
    ├── mem_trace/  # 内存泄漏追踪（含 MIPS backtrace）
    ├── cmd_srv/    # POSIX 命令执行服务
    └── uallsyms/   # 用户态符号表工具
```

**目录与构建范围**：上图同时列出源码目录和构建输出路径。`bin/lib/inc` 当前无实际文件；`mod/lte`、`mod/rtsps` 有源码，但未被核心Makefile收集；`util/cyclone` 未加入顶层默认工具库目标。本快照不含录像存储底层实现，其接口依赖配套工程。这些区别会影响后文对“模块存在”和“当前路径使用”的判断。[Makefile:10](/home/tronlong/lyp/meari/mpc-base/Makefile:10)、[Makefile:97](/home/tronlong/lyp/meari/mpc-base/mod/Makefile:97)。

---

## 四、核心架构设计

### 4.1 整体分层架构

```
┌──────────────────────────────────────────────────────────┐
│                     应用层接口                            │
│  ┌─────────────────────┐  ┌────────────────────────────┐ │
│  │    netclient         │  │       modclient            │ │
│  │（外部 Socket 接口）  │  │   （内部 RPC 调试程序）    │ │
│  │  需鉴权 / UI / APP   │  │   Unix Socket 跨进程调用   │ │
│  └─────────────────────┘  └────────────────────────────┘ │
├──────────────────────────────────────────────────────────┤
│                    业务逻辑层                              │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ │
│  │  base  │ │  ipcm  │ │ param  │ │ event  │ │  sys   │ │
│  └────────┘ └────────┘ └────────┘ └────────┘ └────────┘ │
├──────────────────────────────────────────────────────────┤
│                    媒体处理层                              │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐            │
│  │  dsp   │ │  ai    │ │ rtsps  │ │preview │            │
│  │码流处理│ │智能识别│ │RTSP流  │ │实时预览│            │
│  └────────┘ └────────┘ └────────┘ └────────┘            │
├──────────────────────────────────────────────────────────┤
│                  电源与任务调度层（AOV）                   │
│  ┌─────────────────────────────────────────────────────┐ │
│  │  nv_task（任务调度） + nv_battery（状态/统计）       │ │
│  │  nv_heart_beat（心跳） + nv_timer（定时器）          │ │
│  └─────────────────────────────────────────────────────┘ │
├──────────────────────────────────────────────────────────┤
│                    网络通信层                              │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ │
│  │ nets │ │ 4g   │ │ wlan │ │ wired│ │  BLE │ │nkit  │ │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ │
├──────────────────────────────────────────────────────────┤
│                    硬件抽象层（HAL）                       │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐          │
│  │ hal  │ │ mcu  │ │ ptz  │ │ led  │ │button│          │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘          │
└──────────────────────────────────────────────────────────┘
```

**架构图说明**：图中为逻辑职责分组，模块并非严格单向分层。AI直接从本地VI采集原始图像；DSP接收编码帧。RTSP框表示仓库有相应源码，当前默认构建和启动路径未接入。底层系统休眠、媒体和网络协议 SDK 均存在外部依赖。

---

## 五、核心模块深度解析

本章说明各模块的职责、内部机制和实现边界。AOV重点解释资源调度与休眠，AI重点解释从图像到业务结果的处理过程；第七章再把这些模块串成完整数据流，第十一章给出对应的调用片段。

### 5.1 base 模块 —— 系统总入口

**文件**: `mod/base/`

base 是整个框架的启动入口和统一对外 API 聚合层。

**主要职责**:

- `nv_start_base()` —— 系统启动总控；`nv_stop_base()` 当前只打印日志，停止代码被注释
- 暴露设备信息接口（UUID、PID、AuthKey、固件版本）
- 提供通道级别的快捷控制接口（MIC、翻转、OSD、日夜模式、移动侦测等）
- 系统重启/关机/延迟重启封装

**启动顺序**：`nv_start_base()` 内部依次完成以下工作：

1. 初始化硬件信息、按键和看门狗，加载配置。
2. 启动任务监控器和AOV协调器。
3. 初始化媒体，启动AI、编码、CPU、灯、MCU和无线资源任务，按能力启动PTZ。
4. 初始化事件、AI事件和用户管理，建立DSP缓冲区。
5. 初始化RPC对象，再启动外部data server，最后启动RPC监听；源码要求保持这一顺序。
6. 启动预览、音频、ipcm、网络和套装服务，再按宏/模式启动WiFi、蓝牙、提示音、统计、FTP及外部App。

这一入口返回 `void`，部分失败分支直接 `exit(EXIT_FAILURE)`。调用者通过它启动基础服务后，无需重复启动内部模块。当前 `nv_stop_base()` 的停止代码被注释，信号退出也只清理部分服务，因此启停生命周期尚不对称。[nv_base.cc:272](/home/tronlong/lyp/meari/mpc-base/mod/base/src/nv_base.cc:272)、[nv_base.cc:459](/home/tronlong/lyp/meari/mpc-base/mod/base/src/nv_base.cc:459)。

**设备配对模式**（`PPS_PAIR_TYPE`）:

```
LOCAL_CONNECT_MODE → 本地连接
QRCODE_MODE        → 二维码配网
AP_MODE            → AP 热点配网
BLE_MODE           → 蓝牙配网
```

---

### 5.2 ipcm 模块 —— IP 摄像机管理核心

**文件**: `mod/ipcm/`

ipcm 负责 IPC 通道的连接配置、状态、控制和能力集管理。当前明确接入的是本地通道以及默认 factory；默认远程操作多为返回成功的桩，不能据此认定已完成多厂家接入。

**内部架构（Factory 工厂模式）**:

```
nvipcm.cc（通道管理主控）
    │
    ├── nvfactory.cc（抽象工厂接口）
    │       │
    │       └── nvfactorydef.cc（默认实现；远程操作多为桩）
    │
    └── nvlocal.cc（本地通道管理）
        nvipcmconfig.cc（通道配置持久化）
```

factory 实际按枚举索引分发函数指针表；当前明确填充 `NV_FACTORY_DEF` 和 `NV_FACTORY_LOCAL`，本地操作由 `nvlocal.cc` 提供。ONVIF相关声明/依赖路径不等于已完整接入。[nvfactory.cc:76](/home/tronlong/lyp/meari/mpc-base/mod/ipcm/src/nvfactory.cc:76)、[nvfactorydef.cc:17](/home/tronlong/lyp/meari/mpc-base/mod/ipcm/src/nvfactorydef.cc:17)。

**通道配置结构** (`S_NV_IP_CONFIG`):

```c
typedef struct _NV_IP_CONFIG_ {
    char ip[64];        // IP 地址或 URL
    int  port;          // 端口
    int  protocl;       // 源码真实拼写，类型 E_NV_CAM_TYPE
    int  factory;       // factory 索引，当前明确接入 DEF / LOCAL
    char name[64];      // 用户名
    char pass[64];      // 密码
    char dev[64];       // 设备名称
    char serial[64];    // 设备序列号
    char dev_pid[64];   // 设备真实 PID
    char ty_pid[64];    // TUYA 绑定 PID
    char ty_devId[64];  // TUYA 绑定 UUID
} S_NV_IP_CONFIG;
```

结构体字段来源：[nvipcm.h:270](/home/tronlong/lyp/meari/mpc-base/mod/ipcm/inc/nvipcm.h:270)。上述字符串长度在源码中用 `NV_IP_LEN/NV_NAME_LEN/NV_PASS_LEN/NV_COMM_LEN` 表达，当前这些宏均为64。

**IPC 能力集位掩码系统**（下表使用 `S_NV_IPC_INFO` 的实际字段名）:

| 字段               | 含义                         |
|--------------------|------------------------------|
| `video_encode_cap` | H264/H265 编码支持           |
| `audio_encode_cap` | AAC/PCM/G711U 音频格式       |
| `motion_detect_cap`| 移动侦测/区域侦测/PIR 支持   |
| `ptz_cap`          | 云台 P/T/Z/预置点/追踪       |
| `audio_intercom_cap`| 音频输出/输入/双向对讲      |
| `day_night_cap`    | 日夜切换/白光/红外           |
| `person_detect_cap`| 人形侦测/画框/日夜过滤       |
| `sound_light_cap`  | 声光警示/警笛/白灯           |
| `aov_fps_cap`      | 单帧间隔选项：1/2/3/5/10/15/20/30秒 |
| `work_mode_cap`    | 省电/性能/自定义低功耗模式   |

同名基础能力在旧协议结构 `protocol_ipc_capa_ack_s` 中不带 `_cap` 后缀；两种结构不能混写。能力掩码与字段见 [nvipcm.h:180](/home/tronlong/lyp/meari/mpc-base/mod/ipcm/inc/nvipcm.h:180)、[nvipcm.h:463](/home/tronlong/lyp/meari/mpc-base/mod/ipcm/inc/nvipcm.h:463)。

能力表描述接口可表达的功能，本地能力还由产品配置填充；具体型号可用的功能以填充结果为准。

**异步消息队列设计**（`E_IPCM_MSG_TYPE_t`）:

大量设置命令通过通道消息队列异步执行；查询和配置管理等并非全部走队列。枚举除了1～44外，还包含1001～1010、2000等值。入队成功仅表示请求被接受，不是设备动作已经成功：

```
API 调用层
    │  nv_ipcm_set_xxx()
    ↓
消息队列入队（E_IPCM_MSG_XXX）
    │  IPCM_MSG_MAX_QU = 16
    ↓
ipcm 工作线程处理
    │
    ↓
通过 factory 分发（当前明确为默认/本地实现）
```

队列与异步分发见 [nvipcm.cc:30](/home/tronlong/lyp/meari/mpc-base/mod/ipcm/src/nvipcm.cc:30)、[nvipcm.cc:109](/home/tronlong/lyp/meari/mpc-base/mod/ipcm/src/nvipcm.cc:109)。

---

### 5.3 AOV 模块 —— 低功耗任务调度系统

**文件**: `mod/aov/`

AOV（Always-On Video，全时视频）是电池摄像机的低功耗任务协调模块。它根据业务需求安排各资源工作，并决定何时允许系统休眠；真正进入低功耗状态由外部媒体接口、MCU（单片机）和驱动完成。

#### 5.3.1 工作方式与关键状态

基本工作方式是：**没有事件时，周期醒来处理图像/单帧视频后再次休眠；有事件、取流或其他业务需求时，让相关模块持续工作一段时间**。

“全时”在这里通过周期采集和事件期间连续出流实现。图像传感器、CPU和AI并非一直以完整帧率运行，最终录像如何组织还要看外部存储实现。

需要把三个问题分开：

| 问题 | 查询/参数 | 实际含义 |
|---|---|---|
| 设备配置希望怎样工作？ | `PARAM_AOV_ON/OFF/AUTO` | AOV、强制实时，或根据电量/充电选择 |
| 当前视频要单帧还是连续？ | `nv_task_get_aov_flag()` | true 走单帧；false 走连续视音频模式 |
| CPU是否仍需连续工作？ | `nv_task_get_continue_flag()` | true表示续时未到；false表示可以尝试收尾休眠，还须通过安全检查 |

还要单独看 `nv_battery_get_is_low_power()`：它实际检查 MCU 报告的 `DSP_PROT_MODE_NOR_SLEEP`，不是在这个函数里直接比较“电量 < 某个阈值”。[nv_battery.cc:11](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_battery.cc:11)、[nv_task.cc:271](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task.cc:271)。

**关键例子：**日志/消息处理任务触发 `TRIG_MORE_TIME` 时，CPU需要醒着，视频仍可保持AOV单帧；AI触发 `TRIG_REAL_TIME_ENC` 时，则同时要求CPU运行、连续视频和音频。前者只延长运行时间，后者还改变媒体资源需求。触发器的具体效果见5.3.3。

#### 5.3.2 任务状态机与线程机制

`nv_task_start()` 建立 256 项任务槽，ID 从 1 开始，实际可用 255 项；总协调线程执行 `nv_task_pro()`。每个注册任务通过 `nv_start_task_help()` 创建自己的线程和命令队列。

这里的helper是单个任务的执行线程：`loop`是业务回调，`delay_ms`是无新命令时的循环等待时间；协调线程向它发送START、RESUME等命令，并接收运行/空闲状态。多个helper各自执行，协调器负责汇总状态和需求。

`TASK_STATE_E` 实际只有 `TASK_ST_IDLE`（空闲）和 `TASK_ST_RUNNING`（运行），下图简写为IDLE、RUNNING。源码旧注释里的 `ready`、`running(once)`、`suspend` 是概念步骤，不是实际状态枚举。`suspent_pre` 也不是本实现对任务发出的命令。

**任务状态机**：

```
注册 → START → RUNNING（按delay周期执行loop）
                  │
                  ├── SUSPENT / FINISH → loop收尾 → IDLE
                  │                                 │
                  │                              RESUME
                  │                                 ↓
                  └──────────────────────────── RUNNING
STOP → IDLE → helper线程退出
```

系统suspend发生在全部任务报告IDLE、并通过AI/MCU/pending检查之后；IDLE是用户态任务状态，不是硬件本身已睡下。

实际命令及线程行为：

| 命令 | helper线程如何处理 |
|---|---|
| `START` | 注册创建时即入队；启用回调循环 |
| `RESUME` | 从暂停恢复循环，并将此命令交给回调 |
| `NONE` | 没有新命令且到达循环等待时间时，运行中的任务继续调用回调 |
| `EXECUTE` | 唤起正在运行的 helper 去执行回调；它本身不把已暂停线程改成运行状态 |
| `SUSPENT` | 如正在运行，先把命令交给回调，再报告 IDLE 并暂停 |
| `FINISH` | 如正在运行，先给回调一次收尾机会，再报告 IDLE 并暂停 |
| `STOP` | 报告 IDLE 并退出线程；此分支不调用业务 loop |

这意味着资源释放是否执行，取决于业务回调及注销路径的具体实现；不能假定 STOP 会自动调用所有模块的清理逻辑。[nvtaskhelp.h:11](/home/tronlong/lyp/meari/mpc-base/util/libnvrtools/comm/inc/nvtaskhelp.h:11)、[nvtaskhelp.cc:55](/home/tronlong/lyp/meari/mpc-base/util/libnvrtools/comm/src/nvtaskhelp.cc:55)、[nv_task.cc:997](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task.cc:997)。

注册也有前提：必须已经 `nv_task_start()`；`delay_ms > 0`，优先级在 1～100，loop不能为空。即将休眠时注册被禁用，会返回失败并标记 pending；协调线程下一轮给出额外运行时间，调用者仍需处理失败/重试。因此不能先注册再启动，也不能使用 delay=0。[nv_task.cc:909](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task.cc:909)、[nvtaskhelp.cc:129](/home/tronlong/lyp/meari/mpc-base/util/libnvrtools/comm/src/nvtaskhelp.cc:129)。

#### 5.3.3 业务任务、资源任务与触发器

`nv_task_register()` 的两个重要参数：

- `resume_type`：这个任务属于普通业务还是特定驱动/资源组。
- `running_trig`：普通业务符合条件时，向系统声明自己需要哪些资源或更多运行时间。

当前调度先扫描 `TASK_N_NONE` 的**业务任务**，查询其 `running_cb`（运行需求判断回调），汇总 `TRIG_*`；然后按汇总结果恢复**资源任务组**。强制连续模式会跳过普通业务条件，关机时则只允许 `TRIG_DISK` 业务继续收尾。驱动任务按资源组条件恢复。

`running_cb`用于协调器判断需求和恢复任务，并不是helper每次执行loop之前的开关。一个helper已经处于RUNNING时，仍按自己的循环和命令执行，直到收到暂停、结束或停止命令。

实际 AI 的两个任务都是 `TASK_N_NONE`：`nn.det` 的条件恒为 true、触发类型 `TRIG_NONE`；`nn.trig` 的条件是有效目标延续时间未过、触发类型 `TRIG_REAL_TIME_ENC`。[nv_task.cc:601](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task.cc:601)、[nvIntelligence.cc:750](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:750)。

| 业务触发类型 | 当前调度器的主要效果 |
|---|---|
| `TRIG_NONE` | 恢复该业务任务，不主动续 CPU 时间，也不主动开其他资源 |
| `TRIG_MORE_TIME`、`TRIG_MCU` | 请求更多 CPU 时间 |
| `TRIG_AOV_ENC` | 请求视频资源，保持单帧语义，请求更多 CPU 时间 |
| `TRIG_REAL_TIME_ENC` | 设置连续视频，请求主/子视频和音频资源，续 CPU 时间 |
| `TRIG_DISK` | 请求磁盘资源，续 CPU 时间 |
| `TRIG_SPEAKER`、`TRIG_ETH` | 请求喇叭/以太网资源，续 CPU 时间 |
| `TRIG_4G` | 兼容旧触发值，请求4G/WiFi/HaLow；代码仍有调用者 |
| `TRIG_NET` | 请求4G/WiFi/HaLow/WLAN资源，续 CPU 时间 |
| `TRIG_BLE` | 请求蓝牙资源，续 CPU 时间 |
| `TRIG_PTZ` | 设置该轮候选续时为1秒；尽管常量名带MS，传入的函数按秒处理 |
| `TRIG_SOUND`、`TRIG_LIGHT` | 在此 switch 中主要设置续时；实际声/光控制由相应任务/模块执行 |

多数触发将候选续时设为 3 秒，并不断在协调扫描时刷新；全局截止时间使用 `max(旧截止, 当前时间+候选续时)`。**同一轮各任务的候选值不是逐项取 max，而是在 switch 中赋值**，不能把代码描述为任意任务组合都严格取最大请求。[nv_task.cc:303](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task.cc:303)、[nv_task.cc:628](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task.cc:628)。

资源组的恢复条件也不同：

| 资源任务组 | 本轮的主要恢复条件 |
|---|---|
| `TASK_N_CPU / TASK_N_FILLLIGT / TASK_N_SOUND` | CPU连续运行期限尚未到；这里恢复的是组内已注册任务 |
| `TASK_N_MAIN_VENC` | 非关机，且强制连续、非长休眠、或有视频需求，三者满足一个即可 |
| `TASK_N_SUB_VENC` | 非关机，且强制连续或有视频需求；普通无事件AOV不默认恢复它 |
| `TASK_N_AENC` | 本轮收到实时编码需求；实际音频初始化/停止还发生在媒体模式切换中 |
| `TASK_N_DISK / TASK_N_SPEAKER / TASK_N_ETH` | 分别有磁盘、喇叭、有线网络需求 |
| `TASK_N_4G / TASK_N_WiFi / TASK_N_HaLow` | `TRIG_4G`或`TRIG_NET`；4G在无需求时还会被显式暂停 |
| `TASK_N_BLE / TASK_N_WLAN` | 分别有蓝牙需求、`TRIG_NET`需求 |

多数资源组在这一阶段只处理恢复，随后由统一休眠流程暂停；4G另有无需求时立即暂停的分支。表中的动作作用于组内已注册的任务，具体产品是否注册某组还要看启动条件。[nv_task.cc:709](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task.cc:709)。

**实现边界**：WiFi分支因SDIO/MMC资源关系还会请求磁盘组，但资源组恢复函数在循环中无条件把返回值设为成功，循环结束后返回该值，不能据此确认组内确有WiFi任务。当前据该返回值请求磁盘组的条件可能过宽。[nv_task.cc:409](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task.cc:409)、[nv_task.cc:785](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task.cc:785)。

#### 5.3.4 AOV循环执行流程

```text
恢复符合条件的业务任务
  → 汇总资源需求、视频模式与连续运行截止时间
  → 恢复所需资源组
  → 检查是否仍需CPU连续运行
      ├── 是：等待约10ms，回到下一轮需求扫描
      └── 否：请求任务暂停/结束 → 等待全部报告IDLE
                → 再检查AI、MCU和pending
                    ├── 有新需求：取消休眠，回到扫描
                    └── 无新需求：调用媒体系统suspend
                                  → 外部系统唤醒后恢复执行
                                  → 读取原因，回到下一轮
```

普通、非长休眠模式默认恢复主编码任务，编码回调按当前 AOV 标志切换媒体模式或取单帧。AI检测任务也被恢复，但线程执行是异步的，是否取得有效图像取决于媒体准备与调用结果。不能把它写成“固定串行顺序、每次必定成功编码一帧再成功推理”。[nv_task.cc:813](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task.cc:813)、[nv_task_venc.cc:43](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task_venc.cc:43)。

启动阶段另有**60秒初始连续运行时间**：`nv_task_start()` 设置 `TASK_RUN_POWER_ON`。这段时间用于启动后的连续工作；进入稳定运行后，休眠间隔按配置确定，默认档位见5.3.6。任务注册时先收到START，随后才由协调器根据需求恢复或收尾。[nv_task.h:16](/home/tronlong/lyp/meari/mpc-base/mod/aov/inc/nv_task.h:16)、[nv_task.cc:1013](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task.cc:1013)。

暂时没有目标，且没有录像写入、网络需求、时间片等续时任务时，协调器可以休眠。`nn.det` 的 `running_cb=true` 只说明它每轮需要运行，并不会像 `TRIG_MORE_TIME` 一样阻止系统休眠；这是理解 AI 为什么不会让设备永远醒着的关键。

#### 5.3.5 AI目标与连续编码联动

AI对满足分数和状态过滤的目标更新保持截止时间。配置的事件保持时长以秒计，转换为毫秒后与当前时间相加：

```c
g_AIer[i].m_duration_ms =
    nv_getupms_alarm() + nv_aov_param_get_event_duration() * 1000LL;
```

随后 `nn.trig` 的条件返回 true，调度器处理 `TRIG_REAL_TIME_ENC`：设置 aov=false、请求视频/音频、刷新 CPU 续时，并在单帧切连续时立即给主编码任务发送一次 EXECUTE。

`nv_media_mode_change(false)` 实际初始化音频并启动主/子视频编码；切回 true 时停止连续视频和音频、更新 AOV OSD（画面叠加信息），并按实现处理 I 帧。音频动作不应仅依据 `TASK_N_AENC` 枚举推断，真实媒体切换代码也执行这些操作。[nvIntelligence.cc:558](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:558)、[nv_task.cc:649](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task.cc:649)、[nvencode.cc:2371](/home/tronlong/lyp/meari/mpc-base/mod/dsp/src/nvencode.cc:2371)。

这里有两份不同的截止时间：**目标保持时间**决定 `nn.trig` 是否继续声明实时编码需求，**CPU连续运行期限**决定协调器是否进入收尾。目标连续出现会刷新前者，其他业务也能刷新后者；所以事件保持3秒结束后，CPU仍可能继续运行。具体检测过滤和事件确认在5.5展开。

#### 5.3.6 低电量长休眠策略

| 状态 | 视频和收尾行为 | 系统休眠参数 |
|---|---|---|
| 普通 AOV、非 MCU 长休眠模式 | 主视频默认有恢复机会；发送 `SUSPENT` | `nv_aov_param_get_aov_interval()` |
| MCU报告 `NOR_SLEEP` 的长休眠模式 | 没有外部视频需求时不默认恢复主视频；发送 `FINISH` 做收尾 | `g_aov_max_interval`，初始150秒，可修改 |
| 有实时需求或强制连续模式 | 根据资源需求持续运行 | 当轮不进入系统 suspend |
| 正在安全关机/重启 | 只允许特定磁盘相关业务继续排空，再结束任务 | 转入 MCU 关机/重启动作 |

编码任务的 `FINISH` 路径写入两个结束帧，通知下游结束一段数据；普通 `SUSPENT` 不做同样的结束标记。当前低功耗标志来自 MCU状态，上层设置阈值后会下发 MCU；不能把主机配置中的百分比直接当成本函数唯一判断依据。[nv_task.cc:327](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task.cc:327)、[nv_task.cc:438](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task.cc:438)、[nv_task_venc.cc:107](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task_venc.cc:107)、[nv_mcu.cc:721](/home/tronlong/lyp/meari/mpc-base/mod/mcu/src/nv_mcu.cc:721)。

默认节能配置是间隔5秒、事件保持3秒；性能档为间隔2秒、事件保持6秒，并调整CPU/补光等参数。电量阈值实现默认是 **20%**，头文件注释写的10%与此不同。默认参数又可以受固定配置/客户类型覆盖，因此实际设备值须读取配置。[nv_aov_param.cc:10](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_aov_param.cc:10)、[nv_aov_param.cc:100](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_aov_param.cc:100)、[nv_aov_param.cc:204](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_aov_param.cc:204)。

#### 5.3.7 ON/OFF/AUTO工作模式

- `PARAM_AOV_ON`：使用 AOV 调度策略；有事件仍可临时连续编码。
- `PARAM_AOV_OFF`：强制连续工作；`nv_task_get_aov_flag()` 直接返回 false。
- `PARAM_AOV_AUTO`：根据电量与充电状态选择并保留上次选择，形成滞回。

AUTO 的实际分支：

| 电量和充电 | 选择 |
|---|---|
| 电量 < 50% | 开启 AOV |
| 50% ≤ 电量 < 70%，未充电 | 开启 AOV |
| 50% ≤ 电量 < 70%，正在充电 | 保留原状态 |
| 电量 ≥ 70%，`charge_state == 1` | 关闭 AOV，连续工作 |
| 电量 ≥ 70%，其他充电状态值 | 保留原状态 |

这个50/70规则与20%长休眠阈值是两个不同机制。AUTO 初始保持值为 AOV ON。[nv_task.cc:215](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task.cc:215)。

#### 5.3.8 系统休眠检查与唤醒来源

暂停阶段先开启10秒任务监控，通知所有任务，然后最多等3秒报告 IDLE。等待失败会恢复为 BUSY并额外运行3秒；连续超时计数超过10后尝试 MCU重启。真正 suspend 接口对特定 INVALID_PARAM 错误连续计数超过30也会尝试重启。

成功收尾后，再次检查 AI保持时间、MCU需求和 pending；任何一个满足条件都会取消这次休眠。通过检查才调用：

```c
pps_media_system_suspend(PPS_MEDIA_WKUP_ALRAM, suspend_sec);
```

唤醒后读取媒体库原因：ALRAM 分支当作RTC；KEY 分支当作MCU/按键类，非T32还读取 MCU来源区分 PIR。KEY一般续1秒，PIR续3秒；T32 KEY路径还触发 MCU处理。具体事件模块可以请求更长时间，不能把这些续时常量当成总唤醒时长。

**没有证据说明 `nv_timer` 自己直接配置 RTC，或普通 `TRIG_NET` 调用能直接唤醒已经硬件休眠的 CPU。** 网络唤醒需要网络模块/MCU/硬件中断配合；当前源码可确认网络事件会延长 MCU处理并请求网络资源，但硬件路径在外部接口中。[nv_task.cc:438](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task.cc:438)、[nv_task.cc:831](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task.cc:831)、[nv_net_wireless.cc:145](/home/tronlong/lyp/meari/mpc-base/mod/net_wireless/src/nv_net_wireless.cc:145)。

#### 5.3.9 时间片、定时器、心跳与统计

| 子模块 | 真实行为 | 容易误解之处 |
|---|---|---|
| `nv_time_slice` | 注册 `TASK_N_NONE`、条件恒true的任务，声明所需 `TRIG_*`；释放时注销 | 不是自动到期的时间预算；忘记释放可以长期续系统运行 |
| `nv_timer` | 按 `time(NULL)` 与上次时间的绝对差判断到期，恢复任务执行回调 | 不是独立RTC硬件定时器；系统休眠期间不会执行用户态回调 |
| `nv_heart_beat` | 单例任务产生 `E_EVENT_MSG_HEART_BEAT`；响应更新时间；启动时设置最大休眠间隔 | 150秒是初值而非固定最大限制；本文件不含心跳超时重启逻辑 |
| `nv_battery` | 内存中记录运行/IOT时间、报警次数、小时信号/电量、开机时间 | 7天×24小时是内存统计结构；本文件没有落盘持久化 |
| `nv_task_cpu` | 记录运行区间，给套装主码流更新 wakeup 状态 | 不应只凭任务名称解释成 CPU核心热插拔 |
| `nv_task_monitor` | 定时检查已登记操作截止时间，超时调用 MCU重启 | 这是任务操作超时监控，与业务心跳不同 |

定时器和心跳任务注册时 helper已经收到 START，所以不能假设第一次回调一定要等完整周期。心跳的 timeout 是其运行窗口参数，不是直接“多少秒不回包就重启”。[nv_time_slice.cc:27](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_time_slice.cc:27)、[nv_timer.cc:16](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_timer.cc:16)、[nv_heart_beat.cc:47](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_heart_beat.cc:47)、[nv_battery.cc:8](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_battery.cc:8)、[nvbaserpc.h:94](/home/tronlong/lyp/meari/mpc-base/mod/rpcapi/rpcApiClient/inc/nvbaserpc.h:94)、[nv_task_cpu.cc:19](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task_cpu.cc:19)、[nv_task_monitor.cc:25](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task_monitor.cc:25)。

#### 5.3.10 典型AOV场景

以下假定设备已进入稳定运行阶段，配置为AOV ON、间隔5秒、事件保持3秒、非长休眠模式，且媒体/检测库正常。场景用于解释代码条件，不代表时间精度测试结果。

1. 无事件，按当前间隔休眠；RTC恢复后，AI与主编码任务获得运行机会。
2. 第一张有效目标图像通过AI分数和状态过滤，马上刷新3秒保持时间；这已经能请求连续编码。
3. `nvaievent` 首次看到该类型时只记录检测时间，不一定马上发报警。
4. 若随后1秒内同类有效结果再次出现，并满足上次报警间隔，才发统一AI事件；事件模块再决定录像/抓图推送。
5. 编码、抓图、消息、网络、存储等任务可以分别提出需求，所以CPU/资源工作时间不只由AI保持时间决定。
6. 目标消失且各业务不再续时，任务收尾后再次检查AI/MCU/pending，才真正睡下。

这解释了三个观察结果为什么可能同时成立：**设备已经连续出流，但还没有推送；有画框，但没有录像；AI目标消失后，设备仍暂时醒着。** 每种行为由不同条件控制。

---

### 5.4 dsp 模块 —— 媒体码流处理

**文件**: `mod/dsp/`

**帧类型定义** (`E_NV_FRAME_TYPE`):

- 视频：JPEG、H264-I、H264-P、H265-I、H265-P、YUV420SP
- 音频：PCM、G711A、G711U、AAC、OPUS

这些是接口可表达的帧类型，不代表每个平台都具备所有编解码能力。`nvencode.cc`封装外部媒体采集/编码接口，`nvdsp.cc`接收编码帧并管理缓冲；`nvdecode.cc`当前主要是音频解码/播放适配，不能仅按文件名认定有完整视频解码器。[nvdsp.h:19](/home/tronlong/lyp/meari/mpc-base/mod/dsp/inc/nvdsp.h:19)。

**核心接口**:

```c
nv_start_dsp(chan_max)            // 启动 DSP 模块（支持多通道）
nv_dsp_data_pro(header, buf, sz) // 处理一帧数据
nv_dsp_read_net_frame(...)        // 读取网络发送帧
nv_dsp_get_next_I_frame(...)      // 查找下一个 I 帧（用于流起始）
nv_dsp_require_send_net_frame(...)// 请求发送帧（抢占机制）
```

**码流类型定义**:

```
NV_STREAM_MAIN         = 1   // 主码流（混合 or AOV套装）
NV_STREAM_SUB          = 2   // 子码流
NV_STREAM_MAIN_REAL_TIME = 3 // 主码流实时流（套装）
NV_STREAM_IMG          = 4   // 图片流
NV_STREAM_MAIN_AOV     = 8   // 公共宏声明；未见独立DSP缓冲路由
NV_STREAM_EVENT        = 9   // 公共宏声明；不据名称推定具体路由
```

**网络缓冲区** (`nvnetbuf`): 为每个通道/码流维护循环缓冲区，支持多消费者（多客户端同时拉流）。

---

### 5.5 ai 模块 —— 智能识别

**文件**: `mod/ai/`

**主要职责**：适配外部检测库，配置五类目标和区域，处理分数/状态结果，并分别联动AOV、画框、云台和报警。检测算法与产品业务策略需要分开理解。

本节按“启动 → 采图 → 检测与过滤 → 联动 → 事件确认 → 录像/推送”的顺序展开。VI是本地视频采集接口，NV12是原始图像格式，ROI是检测区域，PTZ是云台控制。检测器处理NV12图像，H264/H265压缩帧则走DSP和取流链路。

#### 5.5.1 检测能力与算法边界

当前主路径是 `nvIntelligence.cc` 使用外部 `hl_pps_nn_common_detector_create()` 和 `status_process()` 的通用目标检测接口，之后做本地类型、区域、分数、状态、画框和业务联动。

它配置了人、宠物、车辆、包裹、烟火五类位掩码，并将标签映射到本地智能事件类型。**能够配置这五类不证明产品实际模型对每类都可用或达到某种精度。** 模型文件、训练数据、网络结构和推理实现不在这份源码快照里。无法由此确认是否使用NPU、具体网络、误报率、夜间精度或实际推理帧率。

| 文件 | 当前作用 |
|---|---|
| `ai/src/nvIntelligence.cc` | 当前业务检测主实现 |
| `ai/inc/nvIntelligence.h` | 接口声明与裁剪桩 |
| `ai/src/hl_pps_nn_commondet.cc` | 只有 `NV_PPSNN_COM` 条件块，块内没有实现 |
| `ai/src/hl_pps_nn_persondet.c/.h` | `NV_PPSNN` 包裹的旧人形适配代码；当前主路径未调用它 |

旧人形适配中确实有 NV12缩放→RGB转换→`pps_nn_persondet_process`，但不能移植成对当前通用检测路径的说明；当前路径直接将 NV12传给外部 status_process。Makefile中AI C文件追加又被后面的 `SRCSC = ...` 覆盖，故旧C实现也不在当前最终C源清单里。[nvIntelligence.cc:191](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:191)、[hl_pps_nn_commondet.cc:1](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/hl_pps_nn_commondet.cc:1)、[hl_pps_nn_persondet.c:166](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/hl_pps_nn_persondet.c:166)、[Makefile:164](/home/tronlong/lyp/meari/mpc-base/mod/Makefile:164)。

#### 5.5.2 启动条件、共享资源和模型目录

`nv_start_intelligence()` 先读设备能力：不支持AI或 `nv_get_ai_decode_num()==0` 时直接返回成功，不创建检测任务。返回成功在这里可能表示“不需要启动”，不能用返回值单独判断AI已运行。

实际初始化流程：

1. 从设备配置读取AI槽位数 `g_AIer_max`，以及子图像尺寸。
2. 宽高分别向上64对齐。
3. 调 `hl_pps_nn_init()`，设置模型根目录为 **`/app/app/`**，再用 `pps_nn_mat_create()` 创建共享图像，保存其data指针和按64对齐的 NV12 `W×H×3/2` 容量值；实际分配由外部SDK完成。
4. 准备检测器配置：最大通道设为槽位数，模型类型读 `nv_get_ai_model_size()`，并设置类别位掩码等参数。注释列出小/中/大/精简模型，具体模型文件名和网络结构在外部依赖中。
5. 按上述配置创建全局检测器 `g_nn_detector`。
6. 初始化槽位表：`chan=-1` 表示空闲，`bstart`表示该槽是否处理通道；按每个视频通道的AI总开关自动open。
7. 注册 `nn.trig`、`nn.det` 两个AOV业务任务。

检测器配置中的 `dynamic_filter_threshold`、`score_threshold`、`linear_filter_enable` 传入 -1；其精确默认含义需要外部SDK确认。`classifier_enable=0` 表示本次配置关闭分类器支路，不能理解为关闭目标类别检测。[nvIntelligence.cc:191](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:191)、[nvIntelligence.cc:698](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:698)。

整个AI循环共用一份图像buffer和一个检测器，以不同 `AIerIdx` 表示槽位。`nn.det` 遍历槽位串行检测，并在每槽处理时持有 `g_intelligence_mutex` 写锁；不是每路通道各有独立推理线程。

首次运行尝试将该线程绑到CPU0。循环延时读取 `nv_get_ai_interval_ms()`，总吞吐量还受采集、检测、后处理、通道数和锁等待影响；源码旧注释的100ms、10帧/秒不能当成当前实测指标。[nvIntelligence.cc:505](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:505)、[nvIntelligence.cc:745](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:745)。

#### 5.5.3 `chan` 与 `AIerIdx` 不是同一个编号

- `chan`：产品视频通道编号。
- `AIerIdx`：检测器内部槽位编号。
- `nv_intelligence_open(chan)`：寻找空闲槽位，建立 `chan → AIerIdx` 映射，更新类别配置并打开处理。
- `nv_intelligence_close(chan)`：清除媒体框、释放该槽位映射并清空保持时间；没有销毁整个检测器。
- `nv_intelligence_set_param(chan)`：找到对应槽位更新类别，并将ROI宽高缓存清零，促使下一帧重新配置区域。

最多可同时打开多少通道由 `nv_get_ai_decode_num()` 返回的槽位数决定，不能仅凭 `NV_CHAN_MAX=4` 得出“4路并行AI”。已open的通道再次open直接返回成功；没有空槽则失败。[nvIntelligence.cc:583](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:583)。

#### 5.5.4 图像输入链路

图像从本地采集接口进入检测器，调用关系如下：

```text
外部媒体库本地 VI 图像
  → nv_media_recv_vi_frame(chan, NV_STREAM_SUB, ...)
  → pps_media_vi_recv_frame_v2(chan, 2, ...)
  → 得到 aligned_size 的宽高及 NV12 buffer
  → 构造 pps_nn_mat(w, h*3/2, 1, buffer)
  → status_process(detector, img, PPSNN_FORMAT_NV12, AIerIdx, objs)
```

`NV_STREAM_SUB` 在这个取图函数中被映射为媒体采集子通道2，注释说明抓拍/智能单独使用360P通道。它是**原始采集图像接口，不是从网络子码流中读H264/H265再交给AI**。实际尺寸以返回的 aligned_size 为准。[nvencode.cc:1806](/home/tronlong/lyp/meari/mpc-base/mod/dsp/src/nvencode.cc:1806)、[nvIntelligence.cc:533](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:533)、[nvIntelligence.cc:233](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:233)。

媒体尚未初始化或隐私遮蔽已开启时，跳过该槽；取图失败则记录错误，本次不执行检测。正常推理路径没有展示RGB转换、网络结构或NMS实现，它们是否存在以及如何执行，取决于外部库。

#### 5.5.5 参数：总开关、类型、区域、分数和布防

| 参数 | 在哪一层生效 | 具体影响 |
|---|---|---|
| AI总开关 `nv_nn_get_person_enable()` | 槽位open/close | 决定该视频通道是否参与检测；名字保留旧“person”前缀 |
| human/pet/car/package/fire 类型开关 | 检测器 `AI_MASK` | 更新外部检测器类别掩码 |
| 四点ROI | `set_roi_v2()` | 将百分比点位换算为当前图像像素区域 |
| `nv_nn_get_person_scroe()` | 本地结果后过滤 | 是接受分数阈值；函数名中的 `scroe` 是源码拼写 |
| 画框开关 | `nv_intelligence_rect_draw()` | 决定媒体叠框，独立于检测总开关 |
| AI布防时间、推送/录像开关 | `nvevent.cc` | 过滤录像/通知，不直接控制每帧是否推理；布防表示允许相应报警业务 |
| 追踪类型与PTZ能力 | 追踪调用前 | 限制可触发云台追踪的类别与状态 |

ROI配置是一组四边形：`set_roi_v2()` 接收一个区域、四个点；点的百分比按 `x*w/100`、`y*h/100` 换算。尺寸不变时不重复下发；类型/区域更新接口把缓存清零后下一次取图重新配置。是否支持任意自交、多边形裁剪等不能由调用层推断。[nvIntelligence.cc:103](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:103)、[nvIntelligence.cc:144](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:144)。

网络控制里，总开关调用open/close后保存配置；类别和ROI保存配置后调用 `nv_intelligence_set_param()`；分数和画框在处理时动态读取。**只直接调用 `nv_nn_set_person_enable()` 保存配置，不等于已调用open/close。** 不同入口不能混为一谈。[nvnetop.cc:1013](/home/tronlong/lyp/meari/mpc-base/mod/nets/src/nvnetop.cc:1013)、[nvchanparam.cc:1748](/home/tronlong/lyp/meari/mpc-base/mod/param/src/nvchanparam.cc:1748)。

#### 5.5.6 目标结果过滤与坐标结构

检测库返回目标坐标、分数、label和status。当前代码处理顺序是：

1. 取 `score*100` 转为整数。
2. 若 `pd_score <= score_min`，丢弃该目标；因此是**严格大于阈值**才通过，且包含整数截断。
3. 通过分数的目标进入画框数组，最多20个，坐标用左上角+宽高。
4. 若 `status==0`，该目标只保留画框，**不进入事件候选数组**。
5. `status!=0` 的目标进入事件候选数组，最多20个，坐标转中心点+宽高，并映射业务label。

因此如果阈值60，分数0.60对应整数60仍被过滤；0.609也可能被截成60。不要把“灵敏度”简单解释成“数值越大越敏感”：在当前后过滤中阈值越大，留下的目标通常越少。UI如何映射用户灵敏度档位还需看调用端。

画框数量和事件候选数量是不同字段。源码用status区分动态/静态颜色和报警资格；**status是由外部检测库产生的，本目录没有足够实现解释其运动判断算法**。代码注释提到仅实时模式支持动检，但本层过滤本身没有再判断AOV标志，应以外部库行为为边界。[nvIntelligence.cc:275](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:275)。

读代码时应以赋值和消费者为准：当前画框结果在 `num_rect / bbox_rect`，事件候选在 `num / bbox`；结构体定义处的坐标注释与实际填充方向相反，不能照抄注释来解释这两组结果。

| 结果用途 | 数量上限 | 坐标表达 |
|---|---|---|
| 本地画框候选 | 20 | 左上角+宽高 |
| 本地事件候选 | 20 | 中心点+宽高 |
| 实际媒体叠框 | 4 | 左上角/宽高归一化到图像比例 |
| 送给AI事件模块的框 | 12 | 中心点+宽高+业务label |

上限来自 [nvIntelligence.cc:29](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:29)、[nvIntelligence.cc:53](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:53)、[nvaievent.h:13](/home/tronlong/lyp/meari/mpc-base/mod/event/inc/nvaievent.h:13)。

#### 5.5.7 联动后处理

**AOV保持支路。** 事件候选非空便刷新该槽位 `m_duration_ms`，`nn.trig`据此向协调器声明连续编码需求，具体切换见5.3.5。`nv_intelligence_check_running()` 检查目标保持时间是否有效；它不用于检查检测线程存活或CPU负载。截止时间异常超过“当前+60秒”时，代码将其重置为当前时间。[nvIntelligence.cc:558](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:558)、[nvIntelligence.cc:674](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:674)。

**画框支路。** 先放status非零的黄色框，再放status为零的白色框，最多4个；还要满足设备支持AI框以及用户画框开关。没有目标时清框通过数量变化等条件处理；输出调用 `pps_media_ext_display_rect()`，不能直接等同于本地UI已收到事件消息。[nvIntelligence.cc:398](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:398)。

**PTZ支路。** `nv_intelligence_snap()` 每次最多选择事件候选里的第一个框尝试追踪，还需设备支持PTZ、已校准且label匹配配置追踪类型。即使第一个框类型不匹配，也不会继续尝试后面的框。注释说框按得分排序，但本文件没有自行排序，因此这里只能确认“选外部结果序列中的第一个”。[nvIntelligence.cc:381](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:381)、[nvIntelligence.cc:460](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:460)。

**事件/补光支路。** 最多封装12个框送入 `nv_ai_event_trig_status()`，并调用 `nv_FillLight_alarm_trigger(-1)`；真正报警和小图还要经历下一节的过滤。候选非空时每次检测都可以送该入口，不能把“记录上一帧目标数量”误读为“只有数量变化时才通知”。[nvIntelligence.cc:460](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:460)。

#### 5.5.8 智能事件确认与触发间隔

`nvaievent.cc` 按**视频通道×事件类型**维护上次检测时间和上次触发时间。处理一帧时：

1. 每个类别只处理该帧中的第一个目标，同类其余框跳过。
2. 首次出现只记录时间；需要已有上次同类结果，并且相隔**小于1000毫秒**才通过确认。压测标志可跳过这道确认。
3. 再检查上次触发：首次触发允许；否则要求 `(event_ms - trig_lasttime)/1000 > 6`。
4. 满足条件才发送事件并按套装模式生成目标小图。

注释写“间隔2分钟”，但是这一层常量是6秒，而且比较用整数秒再 `>6`，所以再次触发通常至少需要7000毫秒，不能写成准确“6秒一报”。两分钟限制在更后面的App推送层，二者不可混淆。

代码统一发：

```c
nv_event_trigger(E_EVENT_MSG_ALARM_NN_PERSON_DECT,
                 chan, -1, box_type, -1, NULL);
```

所以此处人/车/宠物/包裹/烟火共用一个消息种类，`int_value1` 携带真实类别。后续抓图/推送才映射为PET/CAR/PACKAGE/FIRE消息；并不是本地检测直接发五种独立事件。[nvaievent.cc:274](/home/tronlong/lyp/meari/mpc-base/mod/event/src/nvaievent.cc:274)、[nvmsgpush.cc:81](/home/tronlong/lyp/meari/mpc-base/mod/event/src/nvmsgpush.cc:81)。

#### 5.5.9 录像与App推送策略

通过5.5.8的事件确认后，`nv_event_trigger()` 将统一AI消息入队。事件工作线程在 `nvevent.cc` 处理它，并分别检查录像和抓图推送策略：

| 支路 | 主要条件 | 实际动作 |
|---|---|---|
| 录像 | 布防有效、AI录像开关、AI布防时间段 | 调 `StartMotionRecord(chan, NV_STREAM_MAIN)` |
| App抓图推送 | 全局AlarmPush开关、距该通道上次App推送至少120秒、布防、AI推送开关、AI时间段 | 调 `nv_app_msg_snap_pic()` 请求抓图 |
| 图片回调 | 再检查布防和全局推送开关 | 向套装写JPEG（支持类型受限），并写App消息缓冲 |

实际本地AI事件调用的是 **StartMotionRecord**，不是 StartEventRecord。前者包装为 `FS_RECORD_MOTION` 并传入60参数；这也不能直接证明物理录像最终持续60秒，底层行为在外部存储实现中。[nvevent.cc:774](/home/tronlong/lyp/meari/mpc-base/mod/event/src/nvevent.cc:774)、[nvRpcManager.cc:88](/home/tronlong/lyp/meari/mpc-base/mod/rpcapi/rpcApiServer/src/nvRpcManager.cc:88)。

120秒推送门限按通道记录，移动/分贝/AI等抓图推送共用同一时间戳，可能互相影响；AI时间段检查包含跨午夜处理。[nvevent.cc:218](/home/tronlong/lyp/meari/mpc-base/mod/event/src/nvevent.cc:218)、[nvevent.cc:288](/home/tronlong/lyp/meari/mpc-base/mod/event/src/nvevent.cc:288)、[nvevent.cc:787](/home/tronlong/lyp/meari/mpc-base/mod/event/src/nvevent.cc:787)。

这条链路在本目录中到达抓图请求和 `NvNetBuf` 消息写入。后续云端连接、上传和送达由独立App/产品组件承担。

事件源代码中的 `nv_check_ai_det_onoff()` 虽可向UI/客户端发送状态，但当前事件循环里的相应调用被注释。因此不能把“已有函数”写成“本地AI事件必然更新UI图标”。媒体叠框、套装小图、一般UI通知是不同路径。[nvevent.cc:765](/home/tronlong/lyp/meari/mpc-base/mod/event/src/nvevent.cc:765)、[nvuimsg.cc:12](/home/tronlong/lyp/meari/mpc-base/mod/event/src/nvuimsg.cc:12)。

将上述条件与AOV联动放在一起，可以区分五种时间参数：

| 时间条件 | 作用层 | 含义 |
|---|---|---|
| AI事件保持默认3秒，性能档6秒 | AI → AOV | 最后一次有效候选之后，继续声明实时编码需求 |
| 常见触发续时3秒 | AOV协调器 | 刷新CPU连续运行期限，与AI保持期限分开计算 |
| 同类结果间隔小于1000毫秒 | AI事件确认 | 确认连续出现，不是报警冷却时间 |
| 重复触发要求整数秒差大于6 | AI事件模块 | 同通道同类别再次发统一事件，通常至少相隔7000毫秒 |
| App抓图推送间隔至少120秒 | 事件业务策略 | 同通道多个报警来源共用，不限制每帧推理或AOV保持 |

#### 5.5.10 图像输出与抓图路径

| 图像/数据 | 来源 | 接收者与条件 |
|---|---|---|
| 检测用原始图像 | VI采集的NV12图像，共享buffer | 供检测和目标小图裁剪使用 |
| 套装目标小图 | 按目标中心/框裁剪后缩放为 **80×98 NV12** | 事件确认通过且 `g_ai_kit=true` 时写入 `NV_STREAM_IMG` |
| 报警抓图 | 独立 `nv_snapshot_preview_by_chan()` 请求，本地抓图线程/媒体接口生成 | 经回调和业务检查后写App消息缓冲；kit模式下允许的类别还写JPEG |

套装NV12小图在 `nvaievent` 确认目标后直接生成；报警抓图则经事件工作线程另行请求，受布防和推送策略控制。两者是独立支路。当前抓图选中 `#if 1` 的本地图像传感器路径，读网络I帧的旧路径位于 `#else`。[nvaievent.cc:129](/home/tronlong/lyp/meari/mpc-base/mod/event/src/nvaievent.cc:129)、[nvaievent.cc:216](/home/tronlong/lyp/meari/mpc-base/mod/event/src/nvaievent.cc:216)、[nvlocal.cc:731](/home/tronlong/lyp/meari/mpc-base/mod/ipcm/src/nvlocal.cc:731)、[nvsnapshot.cc:35](/home/tronlong/lyp/meari/mpc-base/mod/dsp/src/nvsnapshot.cc:35)。

小图处理每帧每类只取一个目标，因此“最多12框”不代表会生成12张小图。火类可以走本地NV12小图类型，但 `nv_ai_event_write_jpg()` 的类型白名单未包含 FIRE；不能认定所有类别JPEG都能同样发到套装。[nvaievent.cc:239](/home/tronlong/lyp/meari/mpc-base/mod/event/src/nvaievent.cc:239)。

#### 5.5.11 生命周期接口与编译裁剪的实际状态

| 接口 | 当前未裁剪源码中的状态 |
|---|---|
| `nv_start_intelligence()` | 有定义，初始化并注册任务 |
| `nv_intelligence_open/close/set_param()` | 有定义，管理槽位与配置 |
| `nv_intelligence_check_running()` | 有定义，检查目标保持期限 |
| `nv_intelligence_stress_snap()` | 有定义，开关压测目标注入 |
| `nv_stop_intelligence_pre()`、`nv_stop_intelligence()`、`nv_resume_intelligence()`、`nv_suspent_intelligence()` | 头文件有声明，**在本源码快照中未找到相应非裁剪定义** |

当前AI随系统休眠的机制是任务接收 `SUSPENT/FINISH`，不是调用那个只有声明的 `nv_suspent_intelligence()`。现有代码没有展示完整的检测器销毁、图像释放、双任务注销与重启生命周期。

定义 `NV_NO_AI` 或 `NV_NO_MEDIA` 时，头文件提供返回0的inline桩，检测检查桩返回0，压测桩无动作；Makefile裁掉相应AI源文件。**裁剪桩返回成功不表示功能可用**；宏也没有证明对应平台一定“没有NPU”。[nvIntelligence.h:11](/home/tronlong/lyp/meari/mpc-base/mod/ai/inc/nvIntelligence.h:11)、[nvIntelligence.cc:505](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:505)。

#### 5.5.12 错误处理与实现边界

以下不是“已经上板复现的故障”，而是代码中可定位的事实与由此产生的有限风险：

| 代码事实 | 可以得出的有限判断 |
|---|---|
| 创建通用detector后直接返回成功，没有检查返回指针；open更新参数直接用该指针 | 外部库创建失败时，初始化链可能继续，后续存在空指针风险 |
| `status_process()` 的返回值未检查，`objs` 没有本地零初始化 | 若SDK失败时不保证填充输出，后处理可能使用无效结果；需SDK契约/故障测试确认 |
| ROI更新、参数更新和事件调用若干返回值被忽略 | 上层成功/保持时间并不证明所有下游操作成功 |
| 检测loop将图像buffer容量传给事件模块，而不是本次采集返回的 `yuv_len` | 输入长度一致性依赖媒体实际布局，应结合SDK验证 |
| 多类别小图处理对传入buffer调用原地 `nv_nv12_clip()` | 前一类别裁图可能改变后一类别使用的图像；需要多类别场景核验 |
| `nv_init_ai_event()` 分配失败返回 `NV_SUCCESS` | 事件模块可能实际没初始化，但启动失败判断没能识别 |
| AI在事件模块之前注册运行，任务ID仅局部保存，无完整stop实现 | 不能假定支持可靠重复初始化、停启和启动初期不丢事件 |
| 压测开关可在120秒周期注入固定目标，并设置stress标志 | 诊断目标与真实检测必须区分，压测还绕过连续确认 |

一个实现细节是循环用 `nv_media_get_cover_onoff(i)` 的槽位索引检查隐私，随后用 `g_AIer[i].chan` 采图。槽位和视频通道不一一同号时可能检查错对象；当前媒体cover状态又是全局变量。这些是现有通道抽象需要注意的边界，不应泛化成已验证的独立多路隐私控制。[nvIntelligence.cc:524](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:524)、[nvencode.cc:2098](/home/tronlong/lyp/meari/mpc-base/mod/dsp/src/nvencode.cc:2098)。

证据：[nvIntelligence.cc:226](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:226)、[nvIntelligence.cc:250](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:250)、[nvIntelligence.cc:610](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:610)、[nvIntelligence.cc:493](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:493)、[nvaievent.cc:168](/home/tronlong/lyp/meari/mpc-base/mod/event/src/nvaievent.cc:168)、[nvaievent.cc:345](/home/tronlong/lyp/meari/mpc-base/mod/event/src/nvaievent.cc:345)。

---

### 5.6 事件系统 —— event 模块

**文件**: `mod/event/`

事件系统使用有上限的消息队列，由工作线程集中switch分发，并提供特定通知回调。当前没有可据以认定“任意模块注册任意事件订阅”的通用发布-订阅实现。[nvevent.cc:570](/home/tronlong/lyp/meari/mpc-base/mod/event/src/nvevent.cc:570)。

**主要事件类型**（头文件实际typedef名为 `E_EVENT_MSG_TYPE`，下表为部分事件）:

| 事件类型                          | 说明                   |
|-----------------------------------|------------------------|
| `E_EVENT_MSG_RECORD`              | 录像状态变化           |
| `E_EVENT_MSG_WIFI`                | WiFi 状态变化          |
| `E_EVENT_MSG_IPC_CONNECT`         | IPC 连接状态变化       |
| `E_EVENT_MSG_ALARM_MOTION_DECT`   | 移动侦测报警，触发抓图链路 |
| `E_EVENT_MSG_ALARM_NN_PERSON_DECT`| 本地AI统一入口，参数携带类别 |
| `E_EVENT_MSG_ALARM_NN_PET_DECT`   | 宠物侦测报警           |
| `E_EVENT_MSG_ALARM_NN_CAR_DECT`   | 车辆侦测报警           |
| `E_EVENT_MSG_ALARM_NN_FIRE_DECT`  | 火灾侦测报警           |
| `E_EVENT_MSG_HDCHG`               | 硬盘变动               |
| `E_EVENT_MSG_NVR_UPGRADE_PROGRESS`| NVR 升级进度           |
| `E_EVENT_MSG_MQTT_ONLINE_OFFLINE` | MQTT 平台上下线        |
| `E_EVENT_MSG_FTP_SNAP`            | FTP 定时抓图触发       |

**消息推送** (`nvmsgpush`): 组织抓图请求及App消息，写入帧/消息缓冲区；云端连接和上传由外部App/产品组件处理，本文件不能证明TUYA等云端已经收到报警。

**UI 消息** (`nvuimsg`): 提供系统状态通知接口。当前事件循环里移动/AI/分贝状态更新调用被注释，本地AI状态没有通过这些调用更新UI。AI事件的类别映射与媒体叠框已在5.5说明，二者各自有独立路径。[nvevent.cc:765](/home/tronlong/lyp/meari/mpc-base/mod/event/src/nvevent.cc:765)、[nvmsgpush.cc:81](/home/tronlong/lyp/meari/mpc-base/mod/event/src/nvmsgpush.cc:81)。

---

### 5.7 rpcapi 模块 —— 跨进程 RPC 通信

**文件**: `mod/rpcapi/`

分为 Server 和 Client 两侧，实现进程间通信（IPC）。

**RPC Server 侧**（`rpcApiServer`）:

```c
nv_init_rpc_sever(chan_max, disk_max)  // 初始化，分配通道/磁盘资源
nv_start_rpc_sever()                   // 启动 RPC 服务
nv_stop_rpc_sever()                    // 停止
```

**RPC 核心接口**（以下为 `nvRpcManager.h` 中的部分封装）:

- 录像控制：StartRecord、StopRecord、StartMotionRecord、StartEventRecord
- 存储管理：GetDiskNum、FormatDisk、GetDiskInfoAll
- 录像搜索：searchRecord、getSearchResult
- 回放控制：playbackCreate、playbackPlay、playbackSeek
- 系统日志：nv_write_log

**RPC Client 侧**（`rpcApiClient`）:

- `nvbaserpc`：基础 RPC 通信底层
- `nvShareBuf`：命名文件 + `mmap(MAP_SHARED)` 的共享数据传输
- `nvnetbuf`：网络缓冲区管理
- `StorageRPC`：存储专用 RPC 接口封装

**通信路径与存储依赖**：RPC使用 `AF_UNIX + SOCK_STREAM`，服务端/客户端连接 `/tmp/nvrbase.rpc`。共享数据打开 `/tmp/ShareDataInfo`；`nvmemfd`也使用 `open/ftruncate/mmap`，未调用 `memfd_create()`。录像、磁盘和检索接口包装外部 `nvApiStorage`/data server，快照不含完整录像文件系统。RPC对象析构还包含 `assert(0)`，断言启用时不能按普通可释放对象理解。[nvRpcApiServer.cc:2220](/home/tronlong/lyp/meari/mpc-base/mod/rpcapi/rpcApiServer/src/nvRpcApiServer.cc:2220)、[nvShareBuf.cc:37](/home/tronlong/lyp/meari/mpc-base/mod/rpcapi/rpcApiClient/src/nvShareBuf.cc:37)、[nvRpcApiServer.cc:2288](/home/tronlong/lyp/meari/mpc-base/mod/rpcapi/rpcApiServer/src/nvRpcApiServer.cc:2288)。

---

### 5.8 nets 模块 —— 私有协议网络服务

**文件**: `mod/nets/`

通过外部 `pps_server_create/start` SDK 建立TCP服务，处理客户端（App/UI）的私有协议连接。底层SDK实现不在快照里，不能由仓库中存在cyclone就认定此路径使用它。[nvnetserver.cc:81](/home/tronlong/lyp/meari/mpc-base/mod/nets/src/nvnetserver.cc:81)。

```c
nv_start_netserver()   // 启动服务
nv_stop_netserver()    // 停止服务
```

内部通过 `nvnetnode`（连接节点管理）和 `nvnetop`（网络操作）处理多客户端连接。协议由私有 `NV_MsgHead`、JSON控制负载、媒体等负载组成；netclient先请求nonce，再用用户名/密码相关MD5响应鉴权，随后login。接入控制最多2个不同设备、32个内部节点槽，这与视频通道数量不是同一限制。[netclient.cc:524](/home/tronlong/lyp/meari/mpc-base/netclient/src/netclient.cc:524)、[nvnetaccess.cc:12](/home/tronlong/lyp/meari/mpc-base/mod/net_access/src/nvnetaccess.cc:12)。

---

### 5.9 param 模块 —— 配置参数管理

**文件**: `mod/param/`

管理设备配置，`mod/param` 主要包含以下10个子模块；AOV、APN、FTP、存储等还有各自的配置入口：

| 子模块           | 说明                               |
|------------------|------------------------------------|
| `nvparam`        | 参数初始化/默认值/工厂恢复         |
| `nvchanparam`    | 通道编码/日夜/隐私/AI等参数；连接配置另由ipcm管理 |
| `nvnetparam`     | 网络参数（IP/DNS/端口）            |
| `nvuser`         | 用户账户管理（最多 16 用户）       |
| `nvability`      | 设备能力集配置                     |
| `nvgbparam`      | 国标（GB28181）协议参数            |
| `nvptzparam`     | PTZ 预置点参数                     |
| `nvfixparam`     | 固定参数（型号、序列号等）         |
| `nvnormalset`    | 通用配置（语言/分辨率/时间格式等） |
| `nvsysreset`     | 系统恢复出厂设置                   |

**持久化机制**：`Baseparam`用cJSON读写，`/home/cfg/*.bin` 后缀不代表二进制结构或加密数据。读取失败会生成默认值；保存先比较变化再写JSON。`saveconfig()` 没有用底层写文件结果控制返回或保存副本更新，不能承诺写失败可检测、掉电原子性。GB28181参数存在也不证明信令服务已在此完整实现。[nvparamhelp.cc:27](/home/tronlong/lyp/meari/mpc-base/util/libnvrtools/comm/src/nvparamhelp.cc:27)、[nvparamhelp.cc:72](/home/tronlong/lyp/meari/mpc-base/util/libnvrtools/comm/src/nvparamhelp.cc:72)。

---

### 5.10 sys 模块 —— 系统服务

**文件**: `mod/sys/`

| 组件              | 功能                                          |
|-------------------|-----------------------------------------------|
| `nvsystem`        | 系统初始化、重启、关机、版本管理              |
| `nvsystime`       | 系统时间管理、NTP 同步、时区配置              |
| `nvwatchdog`      | 硬件看门狗配置/喂狗，另有模块与系统健康检查 |
| `nvcustomer`      | 客户定制化参数管理                            |
| `nvfactorymodel`  | 产测工厂模式                                  |

**运行条件**：硬件看门狗设置60秒、约15秒喂一次，调试固件路径停止硬件看门狗。AOV任务监控和业务心跳是另外两类机制。时间模块虽有NTP线程，但base启动中的 `nv_time_sync()` 已注释，不能将其写成默认已执行。[nvwatchdog.cc:1](/home/tronlong/lyp/meari/mpc-base/mod/sys/src/nvwatchdog.cc:1)、[nv_base.cc:313](/home/tronlong/lyp/meari/mpc-base/mod/base/src/nv_base.cc:313)。

---

### 5.11 网络接入层

#### 4G/LTE 模块（`mod/4g/` 和 `mod/lte/`）

| 模块               | 功能                           |
|--------------------|--------------------------------|
| `nv_4g` / `nv_lte_ctl` | 4G/LTE 总控：拨号连接管理  |
| `nv_4g_at` / `nv_lte_at` | AT 命令收发（TTY 串口）  |
| `nv_4g_apn` / `nv_lte_apn` | APN 配置管理            |
| `nv_4g_upgrade` / `nv_lte_upgrade` | 模块固件升级     |
| `nv_lte_heartbeat` | LTE 心跳保活                   |

代码用 **`SIM_NUM = 2`** 表达和管理两个SIM，不证明每个产品都有双卡座、两卡可同时联网或不需硬件切换。`mod/lte` 当前未进入核心源清单，base初始化调用也被注释；实际启动的是 `net_wireless → 4g/WiFi` 路径。[nv_base.cc:325](/home/tronlong/lyp/meari/mpc-base/mod/base/src/nv_base.cc:325)。

#### WiFi 模块（`mod/wlan/`）

- `nv_wifi`：WiFi 连接状态管理
- `nv_wifi_op`：WiFi 操作（扫描/连接/断开）
- `nvipchannel`：IPC WiFi 信道管理
- `pps_nkit_wlan`：NKit 配网协议实现
- `pps_wpa_cli`：wpa_supplicant 命令行接口
- `iwlib/iwpriv`：无线工具库

**无线资源协调**（`mod/net_wireless/`）：`net.trig` 是 `TASK_N_NONE + TRIG_NET` 业务任务，根据需求期限和使用状态请求网络资源；设备适配层负责4G/WiFi接口，但部分WiFi switch/reset/upgrade适配仍被注释，不能视作全部功能对等。[nv_net_wireless_dev.cc:24](/home/tronlong/lyp/meari/mpc-base/mod/net_wireless/src/nv_net_wireless_dev.cc:24)、[nv_net_wireless_dev.cc:155](/home/tronlong/lyp/meari/mpc-base/mod/net_wireless/src/nv_net_wireless_dev.cc:155)。

`wlan_client` 处理套装绑定/上线和外部 `pps_prtp_wlan_*` 连接，恢复时申请无线资源，暂停时断开并释放；`wired`用板级脚本、ifconfig/ioctl配置网卡并注册 `TASK_N_ETH`。

#### Bluetooth 模块（`mod/bluetooth/`）

- `nv_ble_msg`：BLE 消息协议处理
- `nv_bt_at`：蓝牙 AT 命令
- `nv_common_bt`：蓝牙通用控制

---

### 5.12 modclient 与 netclient —— 双接口设计

```
┌───────────────────────────────────────────────────────┐
│                    调用者视角                          │
│                                                       │
│  ┌──────────────────┐     ┌─────────────────────────┐ │
│  │    modclient      │     │       netclient         │ │
│  │                   │     │                         │ │
│  │  本机跨进程RPC    │     │  跨网络外部调用         │ │
│  │  链接RPC客户端库  │     │  TCP Socket 连接        │ │
│  │  本地Socket接口   │     │  用户名/密码鉴权        │ │
│  │  nv_rpc_*调用     │     │  私有消息头/JSON/媒体    │ │
│  │  Unix Socket      │     │  兼容导出声明           │ │
│  │  当前已有实现     │     │  客户端 SDK             │ │
│  └──────────────────┘     └─────────────────────────┘ │
│           │                         │                  │
│           └──────────┬──────────────┘                 │
│                      ↓                                 │
│               mpc-base 核心框架                        │
└───────────────────────────────────────────────────────┘
```

**modclient** 当前产物是RPC调试可执行程序，链接 `librpcclient`，不是同进程静态库接口；README中的Unix Socket规划描述已落后于实现。

**netclient** 提供C风格SDK接口，有Windows `__declspec(dllexport)` 等兼容声明，但本次未验证Windows构建；实现包括：

- 连接管理、用户鉴权
- 通道预览/回放
- 参数读写
- 事件接收（移动侦测、AI 报警等）
- 错误码体系（`E_SDK_RET_CODE`）

---

### 5.13 其他模块与协同关系

**套装服务**（`mod/nkit/`）：在 `KOA_KIT` 模式启动UDP命令和四种流的发送服务，与AOV资源请求配合；还有每小时触发实时编码的 `kitWakeup` 定时任务。它不只是设备配对工具。[nv_kit_server.cc:62](/home/tronlong/lyp/meari/mpc-base/mod/nkit/src/nv_kit_server.cc:62)。

**预览与回放**（`mod/preview/`、`mod/playback/`）：预览从DSP实时缓冲读帧；回放创建播放对象，通过外部录像服务控制play/seek/speed/pause/step，再读对应帧缓冲，检索结果通过共享数据管理。底层文件系统和格式覆盖依赖存储组件。[preview.cc:33](/home/tronlong/lyp/meari/mpc-base/mod/preview/src/preview.cc:33)、[playback.cc:25](/home/tronlong/lyp/meari/mpc-base/mod/playback/src/playback.cc:25)。

**MCU与无MCU分支**：有MCU路径负责UART/协议请求、设备信息、工作模式、电量/充电、唤醒与网络事件、关机重启等。AOV阈值会经MCU设置接口下发。无MCU路径保留WiFi/BLE相关通信和部分替代行为，不能概括为“所有MCU接口都为空”。例如其重启调用系统 `reboot`，而关机、部分阈值处理仍是TODO。[nv_mcu.cc:316](/home/tronlong/lyp/meari/mpc-base/mod/mcu/src/nv_mcu.cc:316)、[nv_nomcu.cc:64](/home/tronlong/lyp/meari/mpc-base/mod/mcu/src/nv_nomcu.cc:64)。

| 模块 | 当前确认的职责 | 需保留的边界 |
|---|---|---|
| `hal` | MTD、设备出厂/能力信息、串口和HAL入口 | 不等于完整板级驱动都在此仓库 |
| `ptz` | 云台动作、位置校准、追踪、预置点、工作队列 | 实际电机/控制器由设备接口支持 |
| `led`、`button` | 灯状态与按键事件 | 型号/硬件映射需产品配置 |
| `aov/nvFillLight`、`event/nvsoundlight` | 补光及声光业务策略 | AI补光请求不等于所有报警动作无条件执行 |
| `speaker` | 对讲数据、本地音频输出适配 | 编码/音频设备依赖媒体库 |
| `sound` | 开机、联网、配网、错误码、升级等提示音及警示播放 | 分贝侦测实现另在 `dsp/nv_db.c`；不是通用声音AI |
| `ftp` | 参数、周期抓图、向App消息链路产生FTP图片类型 | 外部 `run_ftpclient.sh` 才启动上传进程 |
| `upgrade` | 接收升级数据、OTA缓冲、检查、执行/进度、恢复入口 | 固件格式、签名与断电安全需审查相应外部依赖和设备流程 |

证据入口：[nv_ptz.c:1](/home/tronlong/lyp/meari/mpc-base/mod/ptz/src/nv_ptz.c:1)、[nvsoundlight.cc:1](/home/tronlong/lyp/meari/mpc-base/mod/event/src/nvsoundlight.cc:1)、[nvsound.cc:190](/home/tronlong/lyp/meari/mpc-base/mod/sound/src/nvsound.cc:190)、[nv_db.c:1](/home/tronlong/lyp/meari/mpc-base/mod/dsp/src/nv_db.c:1)、[nv_snap.cc:20](/home/tronlong/lyp/meari/mpc-base/mod/ftp/src/nv_snap.cc:20)、[nv_base.cc:389](/home/tronlong/lyp/meari/mpc-base/mod/base/src/nv_base.cc:389)、[nv_device_upgrade.cc:556](/home/tronlong/lyp/meari/mpc-base/mod/upgrade/src/nv_device_upgrade.cc:556)。

声光还需区分两份实现：AOV的 `nvFillLight.cc` 真实调用媒体LED控制并根据电量、日夜、报警期限等条件决定输出；`event/nvsoundlight.cc` 仅 `NV_DEV_BASE` 分支有播放/计时线程，而且物理播放/灯动作仍有未完成位置，其他分支为成功桩。不能将后者的返回成功当成实际响铃/亮灯成功。[nvFillLight.cc:305](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nvFillLight.cc:305)、[nvFillLight.cc:449](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nvFillLight.cc:449)、[nvsoundlight.cc:36](/home/tronlong/lyp/meari/mpc-base/mod/event/src/nvsoundlight.cc:36)、[nvsoundlight.cc:206](/home/tronlong/lyp/meari/mpc-base/mod/event/src/nvsoundlight.cc:206)。

---

## 六、工具库详解（util/）

### 6.1 libnvrtools —— 基础工具集

| 工具          | 说明                                    |
|---------------|-----------------------------------------|
| `nvthreadhelp`| 线程创建/销毁封装（pthread）            |
| `nvmutexhelp` | 互斥锁/读写锁/条件变量封装              |
| `nvmsgqueue`  | 线程安全消息队列                        |
| `ThreadQueue` | C++ 模板消息队列                        |
| `nvdebughelp` | 分级日志系统（`NV_LOG_LEVEL`）          |
| `nvsysloghelp`| syslog 封装                             |
| `nvnethelp`   | 域名解析及网络可达性检查 `nv_net_ok2()` |
| `nvparamhelp` | 参数持久化 R/W                          |
| `nvcrc32help` | CRC32 校验                              |
| `nvfile`      | 文件读写封装                            |
| `pps_thread`  | 轻量级线程封装                          |
| `pps_debug`   | 调试打印工具                            |
| `ikcp`        | iKCP 可靠 UDP 协议（低延迟传输）        |
| `pps_c911`    | 固定编码字符串及 `pps_check_string_valid()` 校验 |

`pps_c911`实现固定字符串的编码/校验，调用入口为 `pps_check_string_valid()`。[pps_c911.c:1](/home/tronlong/lyp/meari/mpc-base/util/libnvrtools/c911/src/pps_c911.c:1)。

### 6.2 cyclone —— 异步网络库

`cyclone` 包含跨平台异步TCP网络框架源码和三种I/O多路复用实现；当前未加入顶层默认构建，不代表产品网络路径已使用下面全部组件：

```
cyEvent（事件循环）
    ├── cye_looper_epoll.cpp    → Linux epoll
    ├── cye_looper_kqueue.cpp   → macOS/BSD kqueue
    └── cye_looper_select.cpp   → select实现（可用性依赖平台/构建）

cyNetwork（TCP 网络）
    ├── cyn_tcp_server          → 多线程 TCP 服务器
    │   ├── master_thread       → 接受连接（Accept）
    │   └── work_thread         → 处理读写（epoll）
    └── cyn_tcp_client          → TCP 客户端

cyCrypt（加密）
    ├── cyr_rijndael            → AES 加密
    ├── cyr_dhexchange          → Diffie-Hellman 密钥交换
    ├── cyr_adler32             → Adler32 校验
    └── cyr_xorshift128         → 随机数生成
```

### 6.3 mem_trace —— 内存调试

支持两种架构的调用栈回溯：

- `mips/backtrace.c`：MIPS 架构专用（解析 MIPS 指令集回溯栈帧）
- `other/backtrace.c`：其他架构（使用 `backtrace()` 标准接口）

功能：内存分配/释放跟踪，检测内存泄漏，输出分配时的调用堆栈。

### 6.4 cjson —— JSON 处理

- `cJSON`：轻量级 JSON 解析器（Dave Gamble 著）
- `s2j`：JSON ↔ 结构体自动映射宏库，简化序列化/反序列化

---

## 七、关键数据流分析

### 7.1 视频流处理流程

当前可确认的主链是本地摄像头编码；ipcm在本地通道启动时注册媒体输出回调：

```
外部媒体库：本地摄像头采集/编码
    │ nv_video_stream_cb / nv_video_stream_recv
    ↓
构造 FsFrame_t，按套装 wakeup 状态选择主流路由
    │ local_start_stream() 已注册输出回调
    ↓
dsp 模块（nv_dsp_data_pro）
    │ 写入通道/码流环形缓冲区（NvNetBuf）
    ↓
┌────────────────────────────────────┐
│ 私有协议预览 / 套装UDP发送          │
│ 外部录像存储服务消费编码帧          │
└────────────────────────────────────┘

AI另从VI取NV12，不经过上述压缩帧分发链。
```

套装主流在I帧边界根据wakeup状态切换流1/3。当前DSP明确创建流1、2、3、4的缓冲；8/9是公共宏声明，不能仅凭名字认定另有完整流缓冲。本地视频回调当前固定 `channel=0`。[nvlocal.cc:319](/home/tronlong/lyp/meari/mpc-base/mod/ipcm/src/nvlocal.cc:319)、[nvencode.cc:219](/home/tronlong/lyp/meari/mpc-base/mod/dsp/src/nvencode.cc:219)、[nvencode.cc:281](/home/tronlong/lyp/meari/mpc-base/mod/dsp/src/nvencode.cc:281)。

**缓冲区实现**：`NvNetBuf`通过文件映射，将同一数据区连续映射两次处理跨尾部读取，各消费者维护读索引。容量有限，存在覆盖/索引恢复/抢占，不能理解为永不丢帧。[nvnetbuf.cc:90](/home/tronlong/lyp/meari/mpc-base/mod/rpcapi/rpcApiClient/src/nvnetbuf.cc:90)。

### 7.2 AI 报警事件处理流程

```
本地VI原始图像（NV12，子采集通道）
    │ nv_media_recv_vi_frame()
    ↓
外部通用检测器 status_process()
    │ ROI / 类别配置，返回坐标、score、label、status
    ↓
本地过滤：整数(score×100)严格大于阈值
    │
    ├── 画框候选 → 最多4框媒体叠加（有独立开关）
    │
    └── status非零 → 事件候选
            │
            ├── 刷新保持期限 → nn.trig → TRIG_REAL_TIME_ENC
            │                    连续编码、请求音频、续CPU时间
            ├── 首个候选 → 按PTZ能力/校准/类别尝试追踪
            ├── 补光请求 → nvFillLight按电量/日夜/期限等条件处理
            └── nvaievent：每类去重、<1秒连续确认、触发间隔
                    ↓
                确认通过
                    │
                    ├── kit模式 → 裁剪NV12目标小图 → 图片流缓冲
                    │
                    └── 统一AI事件入队（int_value1携带类别）
                            ↓
                        nvevent业务策略
                            ├── 录像条件通过 → StartMotionRecord
                            └── 推送条件通过 → 请求独立抓图
                                                ↓
                                            图片回调与再次检查
                                                ├── App消息缓冲
                                                └── kit允许类别的JPEG缓冲
```

有效目标、确认事件和业务输出是三个层次：候选先更新AOV保持期限；事件确认通过后，分别生成套装小图和统一事件；统一事件再进入录像/抓图推送策略。套装NV12小图不经过App推送的120秒门限，报警JPEG则来自另一条抓图支路。各层具体条件见5.5。

### 7.3 低功耗唤醒流程（AOV）

```
外部系统恢复CPU运行 → 读取RTC / KEY / PIR等唤醒原因
    ↓
AOV协调器汇总业务条件和TRIG_*，恢复所需资源
    │
    ├── 主编码任务：按当前模式取单帧或连续出流
    ├── AI任务：采集/检测，目标有效时刷新保持期限
    └── 网络/存储等：按各自需求处理业务
    ↓
检查CPU连续运行期限
    ├── 未到：继续扫描与执行，允许各业务刷新期限
    └── 已到：SUSPENT / FINISH → 等待全部IDLE
                  ↓
              再检查AI / MCU / pending
                  ├── 有需求：返回协调循环
                  └── 无需求：pps_media_system_suspend(
                                  PPS_MEDIA_WKUP_ALRAM, suspend_sec)
                                  ↓
                              系统休眠，等待下次恢复
```

图中展示普通AOV恢复后的协作关系；长休眠模式下，无视频需求时不会默认恢复主编码。RTC定时由系统suspend的秒数参数表达，PIR/网络硬件唤醒依赖MCU、驱动和中断。CPU恢复后，用户态定时器和 `TRIG_NET` 才参与需求判断；AI与编码各在线程中执行。模式和错误重试见5.3。

### 7.4 配对/连接流程

配网负责接收、解析连接信息，套装绑定、设备发现和私有TCP则有各自的服务入口。下面分别展示配网分支与产品服务，实线只连接当前已确认的处理环节：

```
配网入口（按产品配置和编译能力选择）
    │
    ├── QR Code扫描
    │       ↓
    │   qrcode / zbar读取图像中的字符串
    │       ↓
    │   字符串转换、信息解析（兼容普通码及XOR / miniz编码）
    │       ↓
    │   保存SSID / 密码 → nv_wpa_connect发起WiFi连接
    │
    ├── BLE配网
    │       ↓
    │   bluetooth协议处理
    │       ↓
    │   MCU WiFi配置接口
    │
    └── AP热点配网（原稿列出的方式，完整入口尚未确认）
            已确认的相关能力：wlan中的hostapd热点启停
            配网服务、凭据提交与后续连接：需结合产品组件确认

产品模式与通信服务（base和当前配置决定启动路径）
    │
    ├── KOA_KIT → wlan_client绑定/上线 + nkit UDP服务
    ├── KOA_APP → 启动外部run_app.sh
    └── 其他通信模块
            ├── discovery：设备发现
            ├── nets / netclient：私有TCP鉴权、控制与取流
            └── ipcm：通道配置、控制与本地媒体回调注册
```

二维码回调已经能追到WiFi连接；BLE通过MCU接口提交凭据，但MCU内部执行不在本仓库中。AP分支没有画处理箭头：热点启停还用于套装自组网，不能据此认定存在“AP → HTTP配网”的完整链路，也不能据此认定产品不支持AP配网。下半图列的是服务职责，服务间没有表示“全部必经”的串行箭头；实际启动顺序见5.1。接口依据见 [nv_wifi_op.cc:31](/home/tronlong/lyp/meari/mpc-base/mod/wlan/src/nv_wifi_op.cc:31)、[nv_common_bt.cc:123](/home/tronlong/lyp/meari/mpc-base/mod/bluetooth/src/nv_common_bt.cc:123)、[pps_device_qrcode.c:60](/home/tronlong/lyp/meari/mpc-base/mod/qrcode/src/pps_device_qrcode.c:60)、[pps_nkit_wlan.c:395](/home/tronlong/lyp/meari/mpc-base/mod/wlan/src/pps_nkit_wlan.c:395)、[nv_base.cc:218](/home/tronlong/lyp/meari/mpc-base/mod/base/src/nv_base.cc:218)。

---

## 八、编译构建体系

### Makefile 编译目标

```makefile
make fw      → 仅编译工具库（util/）
make client  → 编译工具库 + netclient
make all     → 编译util + netclient + mod + rpcApiClient + modclient
make clean   → 清理所有构建产物
```

### 平台环境变量

```bash
source build/x86       # 选择 x86 开发环境
# T41平台改用 source build/t41zm；一次选择一种平台配置
# 设置后环境变量：
# NV_CPU_ARCH=x86 / INGENIC_T41 / INGENIC_T32
# NV_DEV_TYPE=base / mpc
```

### 编译宏控制

| 宏定义            | 作用                         |
|-------------------|------------------------------|
| `NV_NO_MEDIA`     | DSP仅保留nvdsp.cc，裁AI/其他媒体来源及补光实现 |
| `NV_NO_AI`        | 禁用 AI 模块                 |
| `IPC_SYS_BACKUP`  | 控制备份产物复制/恢复分支；顶层备用编译命令已注释 |
| `NV_NOMCU`        | 选择无MCU实现，过滤有MCU实现 |
| `IPC_WIFI_SUPPORT`| 加入二维码来源及部分WiFi启动/include路径 |
| `NV_PPSNN_COM`    | 平台脚本定义；同名适配源为空，主AI仍用外部SDK |

**构建依赖**：快照缺少外部 `hl_pps_nn.h`、`pps_media_api.h`、`nvFsFrame.h`、`nvApiStorage.h` 等头文件/实现和模型；`netclient`示例目标引用的 `test/netclientapp.cc`、工具导出规则中的 `audio/inc/nv_media_audio.h` 也缺失。当前不能把它当成独立可构建的完整固件。

**源清单核验**：核心Makefile先向 `SRCSC` 追加AI C源，后面又用 `SRCSC = ...` 覆盖，旧 `hl_pps_nn_persondet.c` 未进入最终清单。本次只以 `make -qp` 查询AI开启、NO_AI、NO_MEDIA、NOMCU四类最终源清单，未执行构建recipe。[Makefile:131](/home/tronlong/lyp/meari/mpc-base/mod/Makefile:131)、[Makefile:164](/home/tronlong/lyp/meari/mpc-base/mod/Makefile:164)。

---

## 九、接口设计规范

### 9.1 C/C++ 混合接口风格

许多模块采用C链接名导出（`extern "C"`），内部以C/C++实现，常见声明形式为：

```c
#ifdef __cplusplus
extern "C" {
#endif
// 接口声明
extern int nv_xxx_function(params...);
#ifdef __cplusplus
}
#endif
```

这种形式控制C++名称修饰，便于跨语言链接；但部分头文件使用C++类、模板、智能指针等，不能由 `extern "C"` 直接得出所有头文件都可由纯C包含、数据ABI完全兼容的结论。静态库能否由C调用还要看对应头文件、数据布局与链接依赖。

### 9.2 返回值规范

```c
#define NV_SUCCESS        (0)    // 成功
#define NV_FAILURE        (-1)   // 通用失败
#define NV_RETRY          (-2)   // 需重试（回放过快）
#define NV_NO_DEVICE      (-3)   // 无设备
#define NV_EXCEED_LIMITED (-4)   // 超出上限
#define NV_NO_SPACE       (-5)   // 空间不足
```

上述为通用返回值的部分定义。还有 `NV_MEARI_IPC_SUCCESS=1`、`NV_SDK_SUCCESS=1`，外部媒体/SDK各有结果契约；不能统一把非0解释为失败。[nvcommon.h:40](/home/tronlong/lyp/meari/mpc-base/util/libnvrtools/comm/inc/nvcommon.h:40)。

### 9.3 通道索引规范

- 最大通道数：`NV_CHAN_MAX = 4`（静态上限，实际通道数运行时决定）
- 通道索引：`[0, chan_max)`
- 码流类型：主码流（1）、子码流（2）

**实际多通道边界**：4是数组上限，不是四路本地采集/AI已验证的能力。当前编码监控 `MONITOR_TASK_CHAN_MAX=1`，运行时通道数大于1时，`nv_start_task_venc()` 到chan1会因无效监控ID触发 `abort()`；本地视频回调也固定channel0，AI还有共享检测器/图像和槽位索引问题。[nv_task_monitor.h:12](/home/tronlong/lyp/meari/mpc-base/mod/aov/inc/nv_task_monitor.h:12)、[nv_task_venc.cc:128](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task_venc.cc:128)。

---

## 十、安全与加密

| 组件                        | 算法/技术               |
|-----------------------------|-------------------------|
| `util/auth/nv_md5`          | nonce/用户名/密码相关MD5鉴权；不代表密码仅存哈希 |
| `util/auth/nv_base64`       | Base64编码/解码，不是加密 |
| `mod/hal/pps_xxtea`         | 设备出厂/扩展信息的XXTEA处理；非全部JSON配置 |
| `mod/hal/pps_device_encryption` | 设备级加密         |
| `cyclone/cyCrypt/rijndael`  | AES算法源码；未证明当前私有网络调用 |
| `cyclone/cyCrypt/dhexchange`| DH算法源码；未证明当前网络协商接入 |
| `mod/qrcode`                | zbar扫码、XOR与miniz配网数据解码 |

用户配置仍读取 `pass` 字段，MD5鉴权不能证明密码只以不可逆形式落盘。库内存在密码算法也不能证明所有连接使用TLS、所有配置加密或系统安全；本次未做专项安全审计。[nvuser.cc:61](/home/tronlong/lyp/meari/mpc-base/mod/param/src/nvuser.cc:61)、[pps_device_encryption.c:212](/home/tronlong/lyp/meari/mpc-base/mod/hal/src/pps_device_encryption.c:212)。

---

## 十一、典型使用场景

### 场景一：设备基础服务初始化

```c
#include "nv_base.h"

// 按产品SDK配置能力、驱动和运行环境后，调用基础服务总入口。
// 此函数内部已经启动参数、任务、媒体、AI、DSP、ipcm、网络和RPC。
nv_start_base();
```

这是调用片段，不是完整应用。不要在它后面重复调用 `nv_start_dsp()`、`nv_start_intelligence()`、`nv_start_ipcm()` 等启动接口，也不要直接将运行时通道数设为4：当前本地编码监控只有一个通道。RPC/data server还必须遵循5.1中的启动顺序。`nv_stop_base()`当前未实现完整释放，不能将此片段扩展成可重复启停的演示。

### 场景二：电池相机 AOV 工作模式

当前AI在 `nv_start_intelligence()` 中实际注册两个业务任务，片段如下（这些回调是模块内部函数，不是供外部重复注册的接口）：

```c
// AOV协调器已由base先启动。
// 目标保持时间有效时，请求连续编码和CPU续时。
nv_task_register((char*)"nn.trig",
    TASK_N_NONE, TRIG_REAL_TIME_ENC,
    nv_intelligence_trigging, nv_intelligence_trig,
    1000, 50, NULL);

// 每轮恢复后执行检测；TRIG_NONE不会单独续CPU时间。
nv_task_register((char*)"nn.det",
    TASK_N_NONE, TRIG_NONE,
    nv_intelligence_running, nv_intelligence_loop,
    (int)ai_interval, 50, NULL);
```

需要独立注册业务时，应先启动调度器，使用 `TASK_N_NONE`表达业务条件和所需 `TRIG_*`；资源任务才使用MAIN_VENC、DISK等组。delay必须大于0，优先级在1～100，注册失败需要处理，不能用delay=0、先注册后启动的示例。存储业务需要磁盘资源时应声明 `TRIG_DISK`，不是只续时间就等于加载磁盘。[nvIntelligence.cc:750](/home/tronlong/lyp/meari/mpc-base/mod/ai/src/nvIntelligence.cc:750)、[nv_task.cc:909](/home/tronlong/lyp/meari/mpc-base/mod/aov/src/nv_task.cc:909)。

---

## 十二、总结

### 技术亮点

1. **低功耗任务协调**：AOV将业务需求、资源任务、连续视频和系统休眠衔接，使CPU续时与媒体运行模式可以分别控制。

2. **工厂模式的协议抽象**：ipcm通过factory函数表分发，把通道管理与具体操作分开；当前明确接入默认/本地实现。

3. **双接口客户端设计**：modclient通过本地Unix Socket RPC调试，netclient通过私有网络接口鉴权、控制和取流，服务不同调用场景。

4. **编译时能力裁剪**：通过宏定义（`NV_NO_MEDIA`、`NV_NO_AI`）支持按需裁剪，适应不同硬件配置和产品形态。

5. **任务与队列驱动**：ipcm设置队列、event集中分发、各任务helper线程协同，让控制、事件处理与资源协调各有执行路径。

### 局限与规划方向

- `nv_stop_base()`未实现对称释放；AI部分非裁剪stop/suspend接口仅有声明，RPC析构含断言，需要逐模块看生命周期。
- 当前本地编码监控限1通道，不能将静态上限4解释成四路产品能力。
- 检测模型、媒体系统、存储和App/云端依赖不完整，当前目录不能单独证明端到端能力。
- `Development_Log.md`记录过存储任务A/B同步、pRTP2依赖以太网以及5～10秒获取IP等开发信息；这些是历史说明，不是本次实测。[Development_Log.md:35](/home/tronlong/lyp/meari/mpc-base/doc/Development_Log.md:35)、[Development_Log.md:42](/home/tronlong/lyp/meari/mpc-base/doc/Development_Log.md:42)。

---

*本报告基于指定源码快照进行静态核验，覆盖文件清单、Makefile/四份平台脚本及主要启动、调度、媒体、AI、报警、RPC、网络、参数调用链；第三方工具库未逐函数审计。未执行固件编译、上板、功耗或云端联调，模型精度、实际续航、云端送达和最终录像可靠性仍需外部依赖/运行证据。*
