# pRTP 仓库源码深度分析报告

> Meari Technology — 私有实时传输协议栈
> *pRTP Private Real-Time Protocol Suite*
> 原稿分析日期：2026 年 3 月 | 源码核验与补充：2026 年 10 月 4 日
> 核验源码：`/home/tronlong/lyp/meari/pRTP`。
> 当前快照：43 个 `.c`、1 个 `.cc`、77 个 `.h`；源文件合计 22,266 行，含头文件合计 41,268 行。

---

本版在 DOCX 原稿的九章结构上补充源码证据，并直接修订与当前实现不符的描述。文档目录 `/home/tronlong/lyp/perCode/idevelop_md/meari/pRTP` 仅存放报告，实际源码位于上方路径。协议实现、产品使用说明、主机验证结果分别按证据说明；没有目标设备运行结果的事项，不据此推定已在产品中启用。原始 DOCX 保留作为原稿。

## 目录

- [1. 仓库概览与背景](#section-1)
- [2. 整体架构设计](#section-2)
- [3. 各子模块详细分析](#section-3)
  - [3.1  prtp（IPC私有核心协议服务端）](#section-3-1)
  - [3.2  prtp_client（NVR端客户端，已弃用）](#section-3-2)
  - [3.3  prtp2_client（NVR/VMS 客户端）](#section-3-3)
  - [3.4  prtp_wlan（WLAN设备管理协议）](#section-3-4)
  - [3.5  prtp_dis（设备发现协议）](#section-3-5)
  - [3.6  dsp_mcu（主控与单片机通讯协议）](#section-3-6)
  - [3.7  common（公共基础库）](#section-3-7)
- [4. 协议帧格式详解](#section-4)
- [5. 数据流与关键时序图](#section-5)
- [6. 安全机制分析](#section-6)
- [7. 漏洞与安全风险分析](#section-7)
- [8. 代码质量评估](#section-8)
- [9. 总结与建议](#section-9)
- [10. 核验方法、结果与原稿修订记录](#section-10)

---

<a id="section-1"></a>

## 一、仓库概览与背景

pRTP 是 Meari Technology 的安防设备私有通信组件集合，包含 IPC/LCD 控制与媒体协议、VMS/NVR 协议客户端、WLAN 接入管理、局域网发现和 DSP/MCU 通信。源码中的版权及创建时间反映不同模块的演进；原稿中的英文全称和公司隶属关系没有在本快照中找到定义或依据，因此不作为源码核验结论。[README.md:1](/home/tronlong/lyp/meari/pRTP/README.md:1)、[pps_protocol_nvr.c:4](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr.c:4)。

仓库主体使用 C，另有一份 C++ JSON 测试源文件。构建采用 GNU Make 与 shell 脚本；同时包含 Windows Socket、线程和串口分支。当前 `.config` 指向 Ingenic T41 的 MIPS 交叉工具链，不能仅概括为 ARM/x86。README 提供 x86/VSCode 调试说明，实际支持范围还取决于配置、依赖和目标是否被收集。[.config:1](/home/tronlong/lyp/meari/pRTP/.config:1)、[Makefile:77](/home/tronlong/lyp/meari/pRTP/Makefile:77)、[src_objs.mk:5](/home/tronlong/lyp/meari/pRTP/src_objs.mk:5)、[README.md:32](/home/tronlong/lyp/meari/pRTP/README.md:32)。

### 1.1 仓库基本信息

| **属性** | **内容** |
| --- | --- |
| 版权所有 | Meari Technology Co., Ltd（2016–2022） |
| 编程语言 | C（主体）/ C++（测试） |
| 代码规模 | 当前快照 `.c/.cc` 22,266 行；加上 `.h` 共 41,268 行，含注释和空行 |
| 构建方式 | GNU Make + shell 脚本 |
| 目标平台 | 以嵌入式 Linux 为主；当前配置为 T41/MIPS，另有 Windows 条件分支 |
| 传输层 | TCP（控制/媒体流）+ UDP Multicast（发现） |
| 加密算法 | AES 库提供 ECB/CBC/CTR；发现密码使用 ECB，旧 prtp 仅实现 ECB 接收解密路径 |
| 哈希算法 | MD5（`prtp2_client` 中使用） |
| JSON 库 | cJSON（内嵌） |

### 1.2 目录结构总览

```text
pRTP/
├── common/              # 公共基础库（网络、缓冲、AES、日志等）
│   ├── cjson/           # 内嵌 cJSON + s2j 结构体转换
│   ├── inc/             # 公共头文件
│   └── src/             # 公共源文件
├── prtp/
│   ├── prtp/            # IPC 侧 prtp 服务端（核心）
│   ├── prtp_client/     # NVR 侧旧客户端（已弃用）
│   ├── prtp2_client/    # VMS 接入 NVR 的 NVNV 协议客户端
│   ├── prtp_wlan/       # WLAN 接入管理协议
│   ├── prtp_dis/        # IPC↔NVR 设备发现协议（UDP 组播）
│   └── dsp_mcu/         # 主控 DSP ↔ 单片机串口通讯协议
├── version/             # 版本号与 git 信息生成脚本
├── Makefile / src_objs.mk
├── all.sh / copy.sh
```

### 1.3 构建目标与依赖边界

Linux 默认收集 11 个库：`pps_serial`、`pps_tcp_net`、`aes`、`cjson`、`pps_prtp_client`、`pps_prtp_log`、`pps_prtp_dis`、`pps_prtp2_client`、`pps_prtp_wlan`、`pps_prtp`、`dsp_mcu_prot`。Windows 分支不收集旧 IPC 服务端 `pps_prtp`。`prtp_client` 虽在 README 中标注弃用，仍参与默认构建。[src_objs.mk:5](/home/tronlong/lyp/meari/pRTP/src_objs.mk:5)、[src_objs.mk:104](/home/tronlong/lyp/meari/pRTP/src_objs.mk:104)。

构建流程为 `.config` → 生成版本/Git 头文件 → 编译 `targets`；`all.sh` 随后还执行 `make test` 与 `make install`。库输出包括静态归档，未设置 `CONFIG_NO_SHARED=y` 时还生成共享库。`copy.sh` 按所选产品复制到兄弟 pack/app 目录；脚本中的目标目录是硬编码相对路径，不能把 README 所列环境变量视为已在该脚本实现。[Makefile:123](/home/tronlong/lyp/meari/pRTP/Makefile:123)、[Makefile:251](/home/tronlong/lyp/meari/pRTP/Makefile:251)、[all.sh:9](/home/tronlong/lyp/meari/pRTP/all.sh:9)、[copy.sh:24](/home/tronlong/lyp/meari/pRTP/copy.sh:24)。

当前快照存在如下构建限制：

- `.config` 引用 `/opt/toolchains/ingenic/t41/.../mips-linux-gnu-`，原生主机编译需显式覆盖交叉前缀及 `-muclibc`。
- `configs/` 缺失，因此删掉 `.config` 后，`all.sh` 的交互选择无法从本快照恢复配置。
- `test/` 缺失，但 Makefile 收集三个 `test/test_*.c` 目标；声明测试目标不等于测试可运行。
- `all.sh` 和版本生成脚本当前没有执行位；Linux Makefile 直接执行 `version/gen_git_info.sh` 会失败。
- `prtp/prtp/osal` 提供外部 Socket/线程等接口声明。本快照未提供对应 `.c` 实现；旧服务端静态库能够归档，并不意味着产品可独立链接。
- 公共 include 列表误写 `prtp2_clinet/inc`；`pps_prtp2_client` 目标另加了正确路径，因此此次主机静态库编译通过，但误写仍应修正。

依据：[all.sh:21](/home/tronlong/lyp/meari/pRTP/all.sh:21)、[src_objs.mk:20](/home/tronlong/lyp/meari/pRTP/src_objs.mk:20)、[src_objs.mk:145](/home/tronlong/lyp/meari/pRTP/src_objs.mk:145)、[Makefile:265](/home/tronlong/lyp/meari/pRTP/Makefile:265)、[pps_osal_socket.h:188](/home/tronlong/lyp/meari/pRTP/prtp/prtp/osal/pps_osal_socket.h:188)。

主机编译及限制的实际验证见第十章。

---

<a id="section-2"></a>

## 二、整体架构设计

### 2.1 系统角色关系

整个协议栈围绕三类硬件角色构建：

- IPC（IP Camera，摄像头）：按产品接入旧 prtp 服务端和 WLAN 客户端；README 将旧 prtp 服务端用途限定为单品、低功耗产品
- NVR（网络录像机）：是 VMS 所用 `prtp2_client` 的对端；WLAN 服务端的产品侧管理实现需由配套工程提供；可通过 `prtp_dis` 与 IPC 交换发现信息
- LCD/屏幕（婴儿监视器等消费类设备）：通过旧版 prtp 协议连接 IPC，获取视频流和温湿度等传感器数据
- MCU（微控制器/单片机）：通过 `dsp_mcu` 串口协议与主控 DSP 通讯（全时微功耗场景）

角色用途以 [README.md:3](/home/tronlong/lyp/meari/pRTP/README.md:3) 为准。此仓库提供的是通信组件和回调接口，不能单凭库名认定某个产品已同时运行全部模块。

### 2.2 架构分层

| **层次** | **模块** | **职责** |
| --- | --- | --- |
| 应用回调层 | `pps_nkit_nvr_api` / `pps_prtp_wlan_api` / `pps_prtp_dis_api` | 向上层业务暴露命令回调接口 |
| 命令分发层 | `pps_nkit_nvr_init` / `pps_prtp_wlan_client` | 消息类型识别与 switch 分发 |
| 协议封装层 | `pps_protocol_nvr` / `pps_prtp2_protocol` / `pps_prtp_wlan_protocol` | 帧封包/解包、校验和、AES 加密 |
| 传输管道层 | `pps_pipe` / `pps_tcp_server` / `pps_client` | 环形缓冲、异步收发、连接管理 |
| OSAL 层 | `pps_osal_socket` / `pps_osal_thread` / `pps_osal_mutex` | 跨平台操作系统抽象 |
| 公共工具层 | cbuf / log / tools / aes / mmap | 基础数据结构与工具 |

### 2.3 核心设计模式

- 管道-过滤器（Pipe &amp; Filter）：`pps_pipe` 作为核心通信管道，封装环形缓冲区，上层协议通过 `pps_pipe_recv`/write 完成数据流转
- 回调注册（Callback Registration）：`pps_nkit_nvr_server_reg_callback()` 按消息类型注册处理函数，实现消息路由的松耦合
- 全局设备状态：`pps_nkit_nvr_dev_info` 全局唯一，注册/reset 使用 mutex；流开关和部分读取尚未完整使用同一同步规则
- 双线程收发（Split Send/Receive）：`client_func`（接收线程）和 `client_send_pthread`（发送线程）分离，避免收发阻塞互相影响

### 2.4 四类线协议与业务边界

| 协议族 | 实际对端/用途 | 起始标识与结构 | 业务实现边界 |
| --- | --- | --- | --- |
| 旧 prtp / `prtp_client` | IPC 与 LCD/旧客户端的控制、取流、报警、对讲 | `56 56 50 99`，10 字节头 + 载荷 + 4 字节累加和 | IPC 业务由 `pps_nkit_nvr_callback_register()` 注册的回调执行 |
| `prtp2_client` | VMS/客户端接入 NVR；登录、预览、回放及配置 | `NVNV`，`NV_MsgHead` + JSON/二进制数据 | 本仓库包含客户端与结构体转换；NVR 服务端鉴权策略需在对端核验 |
| `prtp_wlan` | IPC 到 NVR/级联节点的接入管理 | `56 56 50 99`，8 字节头 + 载荷 + 2 字节累加和 | 此处是 IPC 客户端；名单、权限、无线配置由回调及对端负责 |
| `dsp_mcu` | 主控与辅助 MCU 的电源、事件、网络及 OTA 消息 | 同起始码，16 位命令/长度/累加和 | 协议通过发送、接收回调接到 UART 等传输，不自行决定休眠或升级策略 |

相同起始码不能证明三个二进制协议的头部或命令空间兼容；`NVNV` 更是独立协议族。依据：[pps_protocol_nvr.c:166](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr.c:166)、[netmsgdef.h:31](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/inc/netmsgdef.h:31)、[pps_prtp_wlan_protocol.c:36](/home/tronlong/lyp/meari/pRTP/prtp/prtp_wlan/src/pps_prtp_wlan_protocol.c:36)、[dsp_mcu_prot.c:54](/home/tronlong/lyp/meari/pRTP/prtp/dsp_mcu/src/dsp_mcu_prot.c:54)。

---

<a id="section-3"></a>

## 三、各子模块详细分析

<a id="section-3-1"></a>

### 3.1 prtp — IPC 私有核心协议服务端

这是整个仓库的核心模块，运行于 IPC（摄像头）侧，对外提供 TCP Server 接受 NVR 或 LCD 屏连接，处理注册、心跳、命令、媒体流四大类消息。

#### 3.1.1 主要源文件

| **文件** | **职责** |
| --- | --- |
| `pps_nkit_nvr_init.c` | 主控初始化、消息分发（当前 82 个 case 标签）、线程管理 |
| `pps_nkit_nvr_server.c` | TCP 服务端封装，基于 `pps_tcp_server` 构建 |
| `pps_tcp_server.c` | 完整的 TCP Server 实现：accept/select/多客户端管理 |
| `pps_protocol_nvr.c` | 协议帧封包/解包、起始码查找、校验和计算、AES 解密 |
| `pps_protocol_nvr_callback.c` | 各命令的发送构建函数（`nvr_send_xxx_cmd`） |
| `pps_protocol_nvr_conversion.c` | 结构体 ↔ 字节流的序列化/反序列化转换 |
| `pps_client_mgr.c` | 客户端管理（目前主要管理单一连接的 `device_id`） |
| `pps_pipe.c` | 协议管道，封装环形缓冲区，连接 TCP 收发与协议解析 |
| `pps_aes.c` | AES-128 ECB/CBC 实现（tiny-AES-c） |

#### 3.1.2 连接生命周期

1. 上层调用 `pps_nkit_nvr_start(port)`，启动 `nvr_monitor`；监控线程初始化服务端并注册 REGISTER、CMD、PINGREQ、DISCONNECT、MEDIA 回调。
2. TCP Server 接受连接。封装层默认 `max_clients=1`；若已有连接，新连接在注册之前就触发关闭旧连接和清空全局设备状态。
3. 每连接创建接收线程、发送线程及 1 MiB 发送管道。协议解包 REGISTER 后，先用请求中的 12 字节 SN 绑定管道，再进入注册回调。
4. 注册回调保存该 SN 对应的 `device_id`，调用产品注册通知，随后固定回复成功。没有在此链路中校验 license/secret，也没有依据产品回调返回值拒绝注册。
5. PINGREQ 更新单调时钟时间并回复 PINGRESP；此快照没有消费 `get_ping_req_time` 的超时关闭判断。
6. CMD 被分发给具体业务回调；MEDIA 接收用于音频/对讲；上层可调用发送接口推送视频、音频、报警。
7. DISCONNECT 回调目前只打印信息，关闭/清空代码被注释。TCP EOF 或服务停止结束收发线程；监控线程另有服务退出后的重新初始化逻辑。

依据：[pps_nkit_nvr_init.c:469](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_nkit_nvr_init.c:469)、[pps_nkit_nvr_server.c:44](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_nkit_nvr_server.c:44)、[pps_tcp_server.c:518](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_tcp_server.c:518)、[pps_protocol_nvr.c:323](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr.c:323)、[pps_nkit_nvr_init.c:110](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_nkit_nvr_init.c:110)、[pps_nkit_nvr_init.c:429](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_nkit_nvr_init.c:429)。

#### 3.1.3 帧协议格式

| **字段** | **长度** | **说明** |
| --- | --- | --- |
| 起始码 | 4 bytes | 固定值 0x56 0x56 0x50 0x99（大端） |
| 消息类型+标志 | 1 byte | 高4位=消息类型(0-14)，低4位=flags（最低位=加密标志） |
| 预留字节 | 1 byte | 0x80 表示分片包，0x00 表示完整包 |
| 载荷长度 | 4 bytes | 大端，不含头部/校验和；当前发送路径填入实际原始载荷长度 |
| 载荷 | N bytes | 命令或媒体数据；当前 pack 原样写入，接收侧可按加密标志进行 ECB 解密 |
| 校验和 | 4 bytes | 所有字节累加和（大端） |

#### 3.1.4 消息类型枚举

| **消息类型值** | **名称** | **方向** | **用途** |
| --- | --- | --- | --- |
| 1 | REGISTER | NVR→IPC | 设备注册，携带 license/secret/SN/MAC |
| 2 | REGISTERACK | IPC→NVR | 注册确认，返回 `device_id` |
| 3 | WARNING | IPC→NVR | 报警上报（移动侦测/PIR/分贝等），可附 JPEG |
| 7 | CMD | 双向 | 控制命令（60+ 种子命令） |
| 8 | MEDIA | 双向 | 音视频媒体流 |
| 12 | PINGREQ | NVR→IPC | 心跳请求 |
| 13 | PINGRESP | IPC→NVR | 心跳回复 |
| 14 | DISCONNECT | 双向 | 断开连接，携带 status（0=正常/1=休眠） |

#### 3.1.5 业务回调与媒体分片

`pps_protocol_nvr_callback.c` 把线协议转换为产品回调参数；`pps_nkit_nvr_callback_register()` 保存上层回调，返回值再用于构建多数命令响应。枚举和 switch 分支的存在不保证功能实现完整：例如 SET_AUTH、GET_AUTH 处理函数只打印 `Not implemented`。回调登记入口见 [pps_protocol_nvr_callback.c:1829](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr_callback.c:1829)。[pps_protocol_nvr_callback.c:411](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr_callback.c:411)。

流控制有四种值：0 停主码流、1 开主码流、2 停子码流、3 开子码流。只要主/子任一流开启，就开启音频；二者均关闭才关闭音频。状态设置和业务回调发生在流控制命令处理内，实际帧由上层提交。[pps_nkit_nvr_api.h:377](/home/tronlong/lyp/meari/pRTP/prtp/prtp/inc/pps_nkit_nvr_api.h:377)、[pps_nkit_nvr_init.c:74](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_nkit_nvr_init.c:74)、[pps_protocol_nvr_callback.c:391](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr_callback.c:391)。

`nvr_send_video_media()` 按 200 KiB 分片，分片帧额外携带 16 位总片数与片编号。片序号从 0 开始；片数为 1 时不写分片头。整个发送过程持有客户端管理锁，但只要中途写入失败，已入队的前几片仍可能被发出，上层需处理不完整帧。[pps_client_mgr.c:32](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_client_mgr.c:32)、[pps_client_mgr.c:299](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_client_mgr.c:299)、[pps_protocol_nvr.c:520](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr.c:520)。

<a id="section-3-2"></a>

### 3.2 `prtp_client` — NVR 端客户端（已弃用）

README 明确标注该模块已弃用，但四个源文件仍进入 `pps_prtp_client` 默认库。它使用与旧 prtp 类似的 10 字节头、4 字节累加和，以及旧消息/命令枚举。`prtp2_client` 接入的是独立 NVNV 协议，不能据此认定它是可直接替换旧客户端的线协议兼容实现。[README.md:10](/home/tronlong/lyp/meari/pRTP/README.md:10)、[src_objs.mk:104](/home/tronlong/lyp/meari/pRTP/src_objs.mk:104)、[pps_prtp_protocol.c:43](/home/tronlong/lyp/meari/pRTP/prtp/prtp_client/src/pps_prtp_protocol.c:43)。

<a id="section-3-3"></a>

### 3.3 `prtp2_client` — NVR/VMS 新客户端

README 定位为 VMS 接入 NVR 的客户端。本模块实现 NVNV 命令/媒体协议，通过通用 `pps_client` 管理连接，并提供命令、预览与回放接口；本快照没有 NVNV 服务端实现。[README.md:12](/home/tronlong/lyp/meari/pRTP/README.md:12)、[pps_prtp2_client_common.c:194](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/src/pps_prtp2_client_common.c:194)。

#### 3.3.1 主要文件

| **文件** | **职责** |
| --- | --- |
| `pps_prtp2_client_common.c` | 连接创建、挑战鉴权、登录、响应同步、预览和媒体回调（701 行） |
| `pps_prtp2_protocol.c` | NVNV 头封包/解包、头部 Adler-32 校验、JSON 转换分发 |
| `pps_prtp2_client_cmd.c` | 各控制命令的封装发送接口 |
| `pps_prtp2_client_playback.c` | 历史录像回放控制接口 |
| `pps_prtp_discovery.c` | 设备发现（复用 `prtp_dis` 逻辑） |
| `nv_md5.c` | MD5 实现，用于 `MD5(user:noce:password)` 挑战应答 |

**端口与超时：** `PPS_PRTP_NVR_PORT=10000`，公开登录接口仍接受显式 port。`PPS_PRTP_NVR_HEART_TIMEOUT=6000` 仅有定义，未被实现使用；同步命令默认等待 6000 ms，不能将其解释为已启用的心跳断线时间。内部周期保活与 15 秒检查代码均被注释。[pps_prtp2_protocol.h:26](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/inc/pps_prtp2_protocol.h:26)、[pps_prtp2_client_common.h:52](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/inc/pps_prtp2_client_common.h:52)、[pps_prtp2_client_common.c:165](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/src/pps_prtp2_client_common.c:165)。

#### 3.3.2 登录、预览与回放的连接组织

登录实际调用链为 `pps_prtp2_client_login()` → TCP connect → `client_auth()` 获取 `noce` 并提交 MD5 应答 → `client_commond_login()` 提交 LOGIN → 保存设备能力及登录状态。鉴权、登录响应必须返回成功，否则断开。`noce` 是源码拼写。[pps_prtp2_client_common.c:267](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/src/pps_prtp2_client_common.c:267)、[pps_prtp2_client_common.c:464](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/src/pps_prtp2_client_common.c:464)、[netmsgdef.h:123](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/inc/netmsgdef.h:123)。

命令、预览、回放采用不同连接类型；主码流预览/回放配置 1024 KiB 接收缓冲，子码流配置 192 KiB，命令连接配置 128 KiB。消息响应缓冲另分配，不能把这些数值作为 NVR 总通道上限。协议中的 `MAX_CHAN_NUMS=32`、WLAN 列表 36 和旧服务端默认单连接分别属于不同语义。[pps_prtp2_client_common.c:205](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/src/pps_prtp2_client_common.c:205)、[netmsgdef.h:34](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/inc/netmsgdef.h:34)。

同步调用用发送锁串行化请求，通过事件等待响应；异步调用和媒体回调另走相应分发路径。公共宏按消息 ID 关联请求，存在超时旧响应与新请求关联的问题，宏自身注释已说明清理事件不能完全避免误关联。[pps_client_extend.h:84](/home/tronlong/lyp/meari/pRTP/common/inc/pps_client_extend.h:84)、[pps_prtp2_client_common.c:399](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/src/pps_prtp2_client_common.c:399)。

<a id="section-3-4"></a>

### 3.4 `prtp_wlan` — WLAN 设备接入管理协议

专用于 IPC 与 NVR 之间的 WLAN 组网管理协议，涵盖设备添加、级联管理、通道删除与复位等功能，适用于单品、套装、低功耗全系设备。

#### 3.4.1 命令集

| **命令值** | **名称** | **方向** | **说明** |
| --- | --- | --- | --- |
| 0 | APPLY | IPC→NVR | 申请连接指定 NVR 或 IPC |
| 1 | `GET_LIST` | IPC→NVR | 获取当前通道列表 |
| 2 | `LIST_UPDATE` | NVR→IPC | 推送通道列表更新 |
| 3 | `WLAN_STATUS` | NVR→IPC | 下发级联开关状态 |
| 4 | `IS_FIRST` | NVR→IPC | 告知是否首次连接及 hostname |
| 5 | `GET_INFO` | NVR→IPC | 获取设备信息（版本/型号） |
| 6 | REG | IPC→NVR | 设备注册（hostname/token/类型/端口） |
| 7 | WAKEUP | 双向 | 唤醒消息 |
| 8 | RESET | NVR→IPC | 设备复位 |
| 9 | `UDHCPC_IP` | IPC→NVR | DHCP IP 上报 |
| 0xffff | HEART | 双向 | 心跳保活 |

#### 3.4.2 IPC 最大支持数量

宏定义 `PPS_PRTP_WLAN_IPC_MAX_NUMS=36`，用于客户端解析 GET_LIST 时截断 `nums`。它说明本客户端当前接收名单的上限，无法单独证明 NVR 硬件或服务端的最大接入路数。[pps_prtp_wlan_cmd_def.h:15](/home/tronlong/lyp/meari/pRTP/prtp/prtp_wlan/inc/pps_prtp_wlan_cmd_def.h:15)、[pps_prtp_wlan_protocol.c:225](/home/tronlong/lyp/meari/pRTP/prtp/prtp_wlan/src/pps_prtp_wlan_protocol.c:225)。

#### 3.4.3 客户端状态、保活与服务端边界

该快照实现 IPC WLAN 客户端：创建 handle 后连接 TCP 8045，通过回调填入 hostname/token/产品与协议属性，发送 REG 并检查响应，成功后设置 `reged=1`。未注册时，除 REG 外的消息被拒绝分发。[pps_prtp_wlan_client_common.c:8](/home/tronlong/lyp/meari/pRTP/prtp/prtp_wlan/src/pps_prtp_wlan_client_common.c:8)、[pps_prtp_wlan_client_common.c:137](/home/tronlong/lyp/meari/pRTP/prtp/prtp_wlan/src/pps_prtp_wlan_client_common.c:137)、[pps_prtp_wlan_callback.c:33](/home/tronlong/lyp/meari/pRTP/prtp/prtp_wlan/src/pps_prtp_wlan_callback.c:33)。

工作线程正常状态每轮发送心跳，`sleep(2)`；每五轮请求一次名单。实际间隔还受同步请求耗时影响，不能保证严格两秒/十秒。收到心跳响应更新 `heart_clk`，超过 6 秒时循环回调返回错误触发断线；重连由 WLAN 上层工作线程处理。8046 与 90 秒 MCU 心跳只是头文件常量，当前普通 WLAN connect 路径使用 8045 和 6 秒。[pps_prtp_wlan_client.c:41](/home/tronlong/lyp/meari/pRTP/prtp/prtp_wlan/src/pps_prtp_wlan_client.c:41)、[pps_prtp_wlan_client_common.c:74](/home/tronlong/lyp/meari/pRTP/prtp/prtp_wlan/src/pps_prtp_wlan_client_common.c:74)、[pps_prtp_wlan_callback.c:272](/home/tronlong/lyp/meari/pRTP/prtp/prtp_wlan/src/pps_prtp_wlan_callback.c:272)、[pps_prtp_wlan_protocol.h:26](/home/tronlong/lyp/meari/pRTP/prtp/prtp_wlan/inc/pps_prtp_wlan_protocol.h:26)。

<a id="section-3-5"></a>

### 3.5 `prtp_dis` — 设备发现协议

基于 UDP 组播的设备发现协议，使用 IANA 标准组播地址 239.255.255.250（与 UPnP/SSDP 相同），端口 3704。所有消息使用 cJSON 格式序列化，便于扩展。

#### 3.5.1 消息类型

| **消息类型** | **用途** |
| --- | --- |
| discovery | 主动探测：NVR 广播寻找局域网内 IPC |
| hello | IPC 上线公告（携带 IP/SN/版本/绑定状态等） |
| bye | IPC 下线公告 |
| bind | 设备绑定请求（携带 token） |
| `set_ip` | 远程设置静态 IP/掩码/网关（密码 AES 加密传输） |
| `set_pass` | 远程修改设备密码（新旧密码均 AES 加密） |
| `cmn_resp` | 通用应答（携带 uuid + result） |

发现/上线消息主要 JSON 字段为 `model`、`tp`、`ip`、`dhcp`、`mask`、`gate`、`port`、`factory`、`sn`、`version`、`bind`、`protocl`、`comm_mode`、`cap_dhcp_ip`、`cap_pw`、`video_in`，可选 `custom_model`。`bind_status` 是 C 结构体成员，线上名称为 `bind`；`token` 只在 IPC 的 bind 消息中由该打包函数输出，不能概括为所有 hello 消息都会发送 token。[pps_prtp_dis_protocol.c:313](/home/tronlong/lyp/meari/pRTP/prtp/prtp_dis/src/pps_prtp_dis_protocol.c:313)。

#### 3.5.2 响应策略与兼容默认值

发现服务收到消息后进入注册的 `recv_cb`/`recv_cmn_cb`，业务决定是否响应及填入什么设备信息。对于 discovery，当前服务端向请求源 IP/端口单播回复，消息类型仍是 discovery；hello 还可作为主动公告发送。原稿的“统一回复 hello”只是流程示意，不能代替实际服务实现。[pps_prtp_dis_api.c:445](/home/tronlong/lyp/meari/pRTP/prtp/prtp_dis/src/pps_prtp_dis_api.c:445)。

解包兼容旧 `dev_type` 与新 `protocl` 字段；缺省 `dhcp=1`、`video_in=1`，解析出的 port 为 0 时改成 **11224**。因此“发现默认端口 10000”不适用于这个模块的缺省规则。网络参数、改密请求传递到业务回调，实际密码校验和配置生效需继续核验产品工程。[pps_prtp_dis_protocol.c:117](/home/tronlong/lyp/meari/pRTP/prtp/prtp_dis/src/pps_prtp_dis_protocol.c:117)、[pps_prtp_dis_api.c:473](/home/tronlong/lyp/meari/pRTP/prtp/prtp_dis/src/pps_prtp_dis_api.c:473)。

<a id="section-3-6"></a>

### 3.6 `dsp_mcu` — 主控与单片机通讯协议

用于主控 SOC（DSP/CPU）与辅助 MCU 之间通过串口（UART）进行通讯，主要应用于全时微功耗（Low Power Camera）场景，协议采用自定义二进制帧格式。

#### 3.6.1 命令分组

| **命令范围** | **功能组** |
| --- | --- |
| 1–42（基础） | 心跳、版本查询、网络电源、重启、关机、PIR 控制/读取/ADC 设置、AOV 阈值、事件上报、MCU/DSP 唤醒睡眠、唤醒源配置、电池设置/读取、LED 控制、能力查询 |
| 1001–1004（OTA） | MCU 固件 OTA 升级（请求/数据/应答流程） |
| 1201–1214（WiFi） | WiFi 能力/信息/主机名/连接/断开/事件/扫描 |
| 1401–1414（蓝牙） | 蓝牙能力、信息、使能、名称、广播数据、收发消息 |
| 1601–1616（网络/本地心跳） | 网络连接、断开、发送、接收、状态；本地心跳控制和状态 |
| 2000（调试） | 调试命令 |

#### 3.6.2 与 UART、低功耗和 OTA 的接口关系

`dsp_mcu_prot_packet()` 接受 `prot_req_t`，先序列化固定请求，随后附加 `preq->dat`，通过调用方提供的 `send_cb` 提交完整帧。解包接口 `dsp_mcu_prot_unpacket()` 解析输入并调用接收回调，返回已消费的字节数；串口配置与读写在 `pps_uart`/`serial-port` 层，MCU 固件行为及主控任务调度不在本协议文件实现。[dsp_mcu_prot.c:54](/home/tronlong/lyp/meari/pRTP/prtp/dsp_mcu/src/dsp_mcu_prot.c:54)、[dsp_mcu_prot.h:12](/home/tronlong/lyp/meari/pRTP/prtp/dsp_mcu/inc/dsp_mcu_prot.h:12)、[prot.h:8](/home/tronlong/lyp/meari/pRTP/common/inc/prot.h:8)。

低功耗命令包括 AOV 阈值、唤醒源位掩码、MCU/DSP 唤醒睡眠、电池和 PIR 参数；WiFi/BLE/网络心跳命令支持把相关配置交给 MCU。命令定义与转换不等同于目标硬件已实现对应能力，OTA 同样只在这里提供请求、数据及应答协议。完整命令范围依据：[dsp_mcu_prot_def.h:7](/home/tronlong/lyp/meari/pRTP/prtp/dsp_mcu/inc/dsp_mcu_prot_def.h:7)。

<a id="section-3-7"></a>

### 3.7 common — 公共基础库

#### 3.7.1 模块列表

| **模块** | **文件** | **功能** |
| --- | --- | --- |
| 循环缓冲区 | `cbuf.c/h` | 通用环形缓冲，支持获取内部指针（零拷贝读取），无 2^N 大小限制 |
| AES 加密 | `aes.c/h` | tiny-AES-c 实现，支持 AES-128 ECB/CBC/CTR，查表法 |
| 日志系统 | `log.c/h` | 分级日志（VERBOSE/DEBUG/INFO/WARN/ERROR），支持 `LOG_TAG` 标签 |
| PPS 客户端 | `pps_client.c/h` | 通用 TCP 客户端（connect/重连/发送/接收循环） |
| PPS 服务端 | `pps_server.c/h` | 通用 TCP 服务端（accept/多客户端） |
| 组播 Socket | `pps_mcast_socket.c/h` | UDP 组播收发（用于设备发现） |
| UART | `pps_uart.c/h` | UART 串口通讯封装（用于 DSP-MCU） |
| 串口 | `serial-port-posix.c` | POSIX 串口配置（波特率/校验/停止位） |
| 网络配置 | `sysnetconfig.c` | 系统网络配置读取（IP/掩码/网关） |
| 时间工具 | `time64.c/h` | 64位时间戳处理 |
| 日志扩展 | `pps_prtp_log.c/h` | 协议专用日志格式 |
| cJSON | `cJSON.c/h` | 轻量 JSON 解析/生成（MIT 协议） |
| s2j | `s2j.c/h` | 基于 cJSON 的结构体 ↔ JSON 互转工具 |

#### 3.7.2 公共缓冲与网络循环

`cbuf` 使用模运算管理 head/tail，并保留一个空位区分满/空；创建尺寸要求为 4 的倍数，未要求 2 的幂。普通、网络接收、mmap 三种模式对连续区间处理不同；`CBUF_MODE_NET_RECV` 另分配复制辅助空间，不能把所有“获取指针”路径一概描述成零拷贝。[cbuf.c:15](/home/tronlong/lyp/meari/pRTP/common/src/cbuf.c:15)、[cbuf.c:88](/home/tronlong/lyp/meari/pRTP/common/src/cbuf.c:88)。

通用 `pps_client` 的工作线程依次执行 loop 回调、发送、接收、数据回调。数据回调返回已消费长度以推进环形缓冲；收到数据而缓冲满且无法解析完整帧时，会丢弃缓冲的一半进行恢复。网络读超时本身继续循环，不代表应用心跳已经超时。[pps_client.c:49](/home/tronlong/lyp/meari/pRTP/common/src/pps_client.c:49)、[pps_client.c:91](/home/tronlong/lyp/meari/pRTP/common/src/pps_client.c:91)。

`pps_pipe` 是旧服务端的固定发送环形缓冲与协议回调容器；它和通用客户端 `cbuf` 处理接收的模式不同。发送空间不足会返回失败，应由上层决定重试、丢帧或断线。[pps_pipe.c:17](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_pipe.c:17)、[pps_pipe.c:112](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_pipe.c:112)。

---

<a id="section-4"></a>

## 四、协议帧格式详解

### 4.1 旧 prtp 二进制帧（TCP）

帧头总长度 = 4（起始码）+ 1（类型+标志）+ 1（分片标志）+ 4（长度）= 10 bytes（宏 `PPS_NVR_HEAD_SIZE`）

帧尾：4 bytes 校验和（所有字节累加，大端）

总帧长 = 10 + 载荷长度 + 4

#### 4.1.1 帧结构内存布局

```text
[56 56 50 99] [TypeFlags] [PartFlag] [Len3 Len2 Len1 Len0] [Payload...] [CSum3 CSum2 CSum1 CSum0]
```

其中 TypeFlags = (MsgType &lt;&lt; 4) \| EncryptFlag，EncryptFlag 最低位 1=加密。

实际头结构使用 4 字节 length；头文件开头仍有“2Byte 长度”的旧注释，应以结构定义和 pack 实现为准。[pps_protocol_nvr.h:74](/home/tronlong/lyp/meari/pRTP/prtp/prtp/inc/pps_protocol_nvr.h:74)、[pps_protocol_nvr.c:166](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr.c:166)。

### 4.2 媒体流子包格式

媒体数据（MEDIA 消息类型）内部根据 `frame_type` 字段区分：

| **`frame_type`** | **帧类型** | **头结构** | **关键字段** |
| --- | --- | --- | --- |
| 0 | H264/H265 I帧（完整） | `prtp_video_Iframe_head_t` | `track_id`, seq, pts, resolution, utc, fps, `code_id`（编码类型） |
| 1 | H264/H265 P帧（完整） | `prtp_video_Pframe_head_t` | `track_id`, seq, pts |
| 2 | 音频 I帧 | `prtp_audio_frame_head_t` | `track_id`, seq, pts, `channel_nums`, `sample_rate`, bitwidth, utc, `time_zone`, `code_id` |
| 3 | 音频 P帧 | `prtp_audio_frame_head_t` | 同上 |
| 带分片标志 | 分片视频 I/P帧 | `prtp_video_fragment_*_stream_t` | 额外增加 `fragment_total` 和 `fragment_id` 字段 |

上述 `prtp_*` 媒体头名称来自已弃用客户端的结构定义，均为 C 内存表示，不能将 `sizeof(结构体)` 直接当作线格式长度。旧 IPC 发送端以 `ipc_video_stream_t`/`ipc_audio_stream_t` 等组织数据，逐字段序列化；实际 track_id 主视频为 0、子视频为 2、音频为 1。视频帧 code_type 对应 H264=0/H265=1，音频发送支持 AAC/PCM。相关头、变长 size 与分片信息应按对应方向的实现解析。[pps_prtp_cmd_def.h:809](/home/tronlong/lyp/meari/pRTP/prtp/prtp_client/inc/pps_prtp_cmd_def.h:809)、[pps_protocol_nvr.c:527](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr.c:527)、[pps_protocol_nvr_callback.c:1686](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr_callback.c:1686)、[pps_protocol_nvr_callback.c:1793](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr_callback.c:1793)。

### 4.3 设备发现消息格式（UDP JSON）

发现协议使用纯文本 JSON 格式，通过 UDP 发送到组播地址 239.255.255.250:3704。

示例 discovery 消息（NVR 广播）：

```json
{"msgtype":"discovery"}
```

示例 hello 消息（IPC 响应）：

```json
{"msgtype":"hello","model":"IPC-x01","ip":"192.168.1.100","dhcp":1,
 "sn":"SN12345678","version":"1.2.3","bind":0,"prtp":"server",
 "protocl":2,"port":10000,"comm_mode":1,"cap_dhcp_ip":1,"cap_pw":1,"video_in":1}
```

### 4.4 NVNV 协议头与媒体数据

在本次 x86_64 主机布局验证中，`NV_MsgHead_L=48`：

| 偏移 | 字段 | 大小 | 当前实现 |
| --- | --- | --- | --- |
| 0 | magic | 4 | ASCII `NVNV` |
| 4 | version | 4 | `0x20210124` |
| 8 | id | 4 | 32 位消息 ID |
| 12 | async | 4 | 异步标志 |
| 16 | res[5] | 20 | 保留区 |
| 36 | proto_len | 4 | JSON 或媒体头长度 |
| 40 | data_len | 4 | 附加二进制数据长度 |
| 44 | checksum | 4 | 仅对前 44 字节计算 Adler-32 |
| 48 | data | 可变 | proto 区之后紧跟二进制 data 区 |

总长度为 `NV_MsgHead_L + proto_len + data_len`。发送代码直接提交结构体，字段未作网络字节序转换；这意味着对端需与当前整数布局和端序约定一致。表中偏移是当前结构及主机核验结果，不能单凭其推定所有跨平台 ABI 均兼容。[netmsgdef.h:38](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/inc/netmsgdef.h:38)、[netmsgdef.h:99](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/inc/netmsgdef.h:99)、[pps_prtp2_protocol.c:38](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/src/pps_prtp2_protocol.c:38)。

普通控制请求经 `*_stru2json()` 生成 JSON；`MSGID_PREVIEW_DATA` 保留二进制媒体头，`NV_DataHead` 提供通道、码流、帧类型、时间、帧号、编码类型及分辨率/采样率等字段。本次主机上 `NV_DataHead_L` 同为 48 字节，不能沿用旧 prtp 的视频/音频子头解析。[netmsgdef.h:51](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/inc/netmsgdef.h:51)、[pps_prtp2_protocol.c:46](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/src/pps_prtp2_protocol.c:46)。

### 4.5 WLAN 与 DSP/MCU 帧

```text
[56 56 50 99] [CmdHi CmdLo] [LenHi LenLo] [Payload...] [SumHi SumLo]
```

二者均使用 8 字节头、2 字节尾，命令、长度和累加和按大端手工序列化，总帧长为 `10 + payload_len`。它们的命令空间和载荷结构不同，也不具备旧 prtp 的 type/flags 和 32 位 length。DSP/MCU 的附加 data 计入载荷长度，WLAN 的序列化方向主要针对 IPC 请求/应答。[pps_prtp_wlan_protocol.c:36](/home/tronlong/lyp/meari/pRTP/prtp/prtp_wlan/src/pps_prtp_wlan_protocol.c:36)、[dsp_mcu_prot.c:54](/home/tronlong/lyp/meari/pRTP/prtp/dsp_mcu/src/dsp_mcu_prot.c:54)。

---

<a id="section-5"></a>

## 五、数据流与关键时序

### 5.1 设备发现与注册时序

以下描述 NVR 发现并连接 IPC 的完整流程：

1. 调用方在指定网卡发送 UDP 组播 discovery 到 `239.255.255.250:3704`。
2. 对端发现服务调用业务回调判断是否响应，再向请求源单播设备信息；本快照响应类型为 discovery，hello 用于公告等路径。
3. 调用方获取对端 IP、协议类型和端口；若 port 为 0，发现模块默认补成 11224。
4. 根据协议族建立 TCP 连接；此后步骤以旧 prtp 为例。
5. 客户端发送 REGISTER（license/secret/SN/MAC/版本等），服务端先绑定 SN 并保存全局设备状态。
6. 当前旧 prtp 注册回调回复成功，响应中的 Camera device_id 来自调用方给定值，此处调用传入的是空 sn 缓冲；不能理解为已验证凭证或已生成可靠身份。
7. 进行控制、媒体和心跳交换；若选用 NVNV，则执行独立的挑战鉴权与 LOGIN 流程。

依据：[pps_prtp_dis_api.c:460](/home/tronlong/lyp/meari/pRTP/prtp/prtp_dis/src/pps_prtp_dis_api.c:460)、[pps_prtp_dis_protocol.c:137](/home/tronlong/lyp/meari/pRTP/prtp/prtp_dis/src/pps_prtp_dis_protocol.c:137)、[pps_nkit_nvr_init.c:110](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_nkit_nvr_init.c:110)、[pps_protocol_nvr.c:323](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr.c:323)。

### 5.2 媒体流控制时序

1. NVR 发送 `CMD(STREAM_CTRL, stream_ctrl=1)` 开启视频
2. IPC 设置 `main_video_switch`=1，`audio_switch`=1
3. IPC 连续推送 MEDIA 帧（视频 I/P 帧 + 音频帧）
4. NVR 可发送 `FORCE_I_FRAME` 强制 IPC 发送关键帧
5. NVR 发送 `CMD(STREAM_CTRL, stream_ctrl=0)` 关闭视频

以上为主码流示意；存在子码流时，关闭主码流不会必然关闭音频。上层提交媒体还要通过 device_id、流开关及帧/编码类型检查；库负责组织并发送帧，实际采集、编码和录制由业务提供。[pps_protocol_nvr_callback.c:1793](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr_callback.c:1793)。

### 5.3 报警处理时序

1. IPC 触发移动侦测/PIR/分贝侦测等事件
2. IPC 主动发送 WARNING 帧，`alarm_type` 标识报警类型
3. 若 `dat_type`=1，WARNING 帧附带 JPEG 截图数据（变长）
4. NVR 通过注册的回调函数处理报警，执行录像/推送等操作

该流程说明产品如何接入协议；本仓库提供 `pps_nkit_nvr_send_event_msg()`/`pps_nkit_nvr_send_alarm_msg()` 及 WARNING 组包，侦测、录像和通知策略由产品执行。是否附加 JPEG 还取决于调用方实际提供的 dat/dat_len，序列化代码将其作为第二数据段发送。[pps_protocol_nvr_callback.c:1615](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr_callback.c:1615)、[pps_protocol_nvr_conversion.c:86](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr_conversion.c:86)。

### 5.4 TCP Server 内部数据流

`pps_tcp_server_loop()` → `accept()` → 创建 `pps_tcp_server_client_t` → 启动两个线程：

- `client_func`（接收线程）：`select()` 等待数据 → `pps_socket_recv()` → 存入 rbuf → `pps_pipe_recv()` 解析
- `client_send_pthread`（发送线程）：轮询 `pps_pipe_need_tx()` → `pps_pipe_point2end()` 获取待发数据 → `send()`

`pps_pipe` 连接协议层与 TCP 发送层，写入固定环形缓冲时使用写锁；共享客户端、pipe 生命周期与上层状态仍需按第 7.3 节检查。

### 5.5 三条调用路径对照

```text
旧 prtp 控制接收：
recv → client.rbuf → pps_pipe_recv → find_one_packet → unpack/校验
     → package2struct → g_recv_callbacks → nkit 命令处理 → 产品业务回调

旧 prtp 媒体发送：
上层提交帧 → 查 SN 对应 pipe → 200 KiB 视频分片 → build_media
     → pack 原样入固定发送环形缓冲 → client_send_pthread → TCP send

NVNV 登录与业务：
TCP connect → NOCE_REQ/RSP → MD5(user:noce:password) → AUTH_REQ/RSP
     → LOGIN_REQ/RSP → 命令/预览/回放 → JSON 或 NV_DataHead + 帧数据
```

旧服务端 rbuf 为每客户端 256 KiB，发送 pipe 为 1 MiB；发送线程每次只推进实际发送的字节数，遇到短发送或失败跳出内层，20 ms 后重试。两个大小分别属于不同方向，不能相加解释成自动扩容队列。[pps_tcp_server.h:27](/home/tronlong/lyp/meari/pRTP/prtp/prtp/inc/pps_tcp_server.h:27)、[pps_tcp_server.c:323](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_tcp_server.c:323)、[pps_tcp_server.c:377](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_tcp_server.c:377)。

---

<a id="section-6"></a>

## 六、安全机制分析

### 6.1 旧 prtp 的实际鉴权与加密状态

REGISTER 包包含 license、secret、SN、factory_id 等字段，但当前服务端只解析、记录和通知上层，随后固定返回注册成功；注册通知的返回值未用于拒绝连接。应将这些字段称为“凭证承载字段”，不能据此认定身份鉴权已经实现。[pps_protocol_nvr_cmd_def.h:173](/home/tronlong/lyp/meari/pRTP/prtp/prtp/inc/pps_protocol_nvr_cmd_def.h:173)、[pps_nkit_nvr_init.c:110](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_nkit_nvr_init.c:110)。

初始化 `req_encrypt=0`，固定和动态密钥均由硬编码字符串 `c6daf97424ea15f501e14995bb6e982f` 生成。代码将高半字节左移 **8** 位再存入 `char`，实际应关注截断后的值，而非把字符串直接当作正确解码的 AES 密钥。本次主机调用原始初始化函数得到：

```text
06 0a 09 04 04 0a 05 05 01 01 09 05 0b 0e 08 0f
```

接收 unpack 可按标志执行 ECB 解密；发送 pack 则只设置标志，随后将载荷原样写入发送缓冲，没有执行 AES 加密。主机验证中 `encrypt=1` 的包仍包含原始明文。因此单独将 `req_encrypt` 改成 1 会产生标志与实际内容不一致，不能作为启用加密的修复方法。[pps_protocol_nvr.c:127](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr.c:127)、[pps_protocol_nvr.c:146](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr.c:146)、[pps_protocol_nvr.c:264](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr.c:264)。

模块提供 `pps_nkit_nvr_dynamic_key()` → `pps_pipe_set_dynamic_key()` → `pps_protocol_nvr_set_dynamic_key()` 的外部设置入口，所以原稿“动态密钥完全没有更新机制”过于绝对；但本快照没有在该入口内实现安全协商、来源验证或密钥轮换协议。[pps_nkit_nvr_server.c:146](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_nkit_nvr_server.c:146)、[pps_protocol_nvr.c:424](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr.c:424)。

### 6.2 NVNV 挑战鉴权与载荷保护

`prtp2_client` 先向对端请求 `noce`，计算 `MD5(user:noce:password)` 后发送 AUTH，再发送 LOGIN；失败响应会中止并断开。它与旧 prtp 无凭证校验的注册路径不同。对端生成 noce 的随机性、有效期、重用规则及权限检查不在本仓库，无法只凭客户端断言已解决重放问题。[pps_prtp2_client_common.c:267](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/src/pps_prtp2_client_common.c:267)、[pps_prtp2_client_common.c:464](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/src/pps_prtp2_client_common.c:464)、[netmsgdef.h:123](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/inc/netmsgdef.h:123)。

NVNV 封包未调用 AES 或 TLS；头部 Adler-32 只覆盖头结构中 checksum 之前的字节，**不覆盖 JSON 与二进制载荷**。主机验证中修改 payload 后，原始 unpack 仍返回成功；这证明该校验不提供载荷篡改检测，更不能作为消息认证。[netmsgdef.h:103](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/inc/netmsgdef.h:103)、[pps_prtp2_protocol.c:86](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/src/pps_prtp2_protocol.c:86)。

### 6.3 发现协议密码保护的真实边界

`set_ip` 和 `set_pass` 的密码字段确实通过 AES-ECB 转成十六进制密文，但 key 仅由同一消息的 UUID 与 serial_no 派生：去掉 UUID 的连字符后转成字节，再与 SN 字节按位或组合，过程未使用秘密。能够读取该消息且知道算法的一方可重建相同密钥。原稿“防止 UDP 中密码泄露”的结论应修正为：改变了密码字段的线上表示，未形成对被动观察者可靠的密码保密。[pps_prtp_dis_api.c:135](/home/tronlong/lyp/meari/pRTP/prtp/prtp_dis/src/pps_prtp_dis_api.c:135)、[pps_prtp_dis_api.c:154](/home/tronlong/lyp/meari/pRTP/prtp/prtp_dis/src/pps_prtp_dis_api.c:154)、[aes.c:520](/home/tronlong/lyp/meari/pRTP/common/src/aes.c:520)。

UUID 生成使用时间、静态状态和 `rand()`，不是安全随机源。网络参数/密码能否修改，还取决于接收业务回调的验证；不能把密码解密成功当作已授权操作。[pps_prtp_dis_api.c:49](/home/tronlong/lyp/meari/pRTP/prtp/prtp_dis/src/pps_prtp_dis_api.c:49)、[pps_prtp_dis_api.c:473](/home/tronlong/lyp/meari/pRTP/prtp/prtp_dis/src/pps_prtp_dis_api.c:473)。

### 6.4 WLAN Token 与校验机制

WLAN REG 携带 128 字节 token，客户端通过产品回调取值，并检查对端返回的注册结果。本仓库没有相应 WLAN 服务端 token 校验实现，不能据此认定设备所有权验证已完成。[pps_prtp_wlan_cmd_def.h:84](/home/tronlong/lyp/meari/pRTP/prtp/prtp_wlan/inc/pps_prtp_wlan_cmd_def.h:84)、[pps_prtp_wlan_callback.c:33](/home/tronlong/lyp/meari/pRTP/prtp/prtp_wlan/src/pps_prtp_wlan_callback.c:33)。

| 协议 | 校验范围 | 完整性边界 |
| --- | --- | --- |
| 旧 prtp | 头与载荷的 32 位字节累加和 | 校验可重新计算，无密码学消息认证 |
| WLAN | 头与载荷的 16 位字节累加和 | 同上 |
| DSP/MCU | 头与载荷的 16 位字节累加和 | 同上；物理串口的暴露面还取决于硬件 |
| NVNV | 头部前 44 字节的 Adler-32 | 不校验载荷，也无密码学消息认证 |

依据：[pps_protocol_nvr.c:176](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr.c:176)、[pps_prtp_wlan_protocol.c:64](/home/tronlong/lyp/meari/pRTP/prtp/prtp_wlan/src/pps_prtp_wlan_protocol.c:64)、[dsp_mcu_prot.c:86](/home/tronlong/lyp/meari/pRTP/prtp/dsp_mcu/src/dsp_mcu_prot.c:86)、[netmsgdef.h:110](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/inc/netmsgdef.h:110)。

`MSG_NOSIGNAL` 用于避免 send 触发 SIGPIPE，`SO_REUSEADDR` 用于地址复用；两者是进程健壮性/连接管理措施，不能归入密码学安全机制。[pps_tcp_server.c:184](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_tcp_server.c:184)、[pps_tcp_server.c:291](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_tcp_server.c:291)。

---

<a id="section-7"></a>

## 七、漏洞与安全风险分析

下列结论分别来自源码检查和主机函数级复现。优先级反映修复顺序，不是设备固件的 CVSS 评级；是否对远程攻击者可达、产品是否启用该路径，以及实际崩溃或代码执行后果，还需在目标产品中核实。

### 7.1 优先修复：输入边界与确定性崩溃路径

#### 7.1.1 旧 prtp 帧头完整性检查不足

`find_one_packet()` 找到四字节起始码后只检查剩余字节是否小于 4，随后立即读取偏移 6 开始的四字节长度；正确读取长度至少需要完整的 10 字节头。仅有 magic 的分段输入会读取未接收区域；传入恰好四字节分配区时，本次 ASan 复现为 heap-buffer-overflow。网络实际 rbuf 是较大固定数组，不能把函数级越界复现直接等同为每个四字节网络包都会崩溃，但完整性检查错误已经确认。[pps_protocol_nvr.c:75](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr.c:75)、[pps_tcp_server.c:408](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_tcp_server.c:408)。

帧长通过整型移位和加法计算，没有先限制载荷上限并检查溢出。应先验证完整头，再以无符号类型解码长度、检查 `payload <= receive_capacity - frame_overhead`，最后判断包是否接收完整。

#### 7.1.2 REGISTER 与命令载荷缺少逐字段长度验证

`pps_protocol_nvr_package2struct()` 的 REGISTER 分支依次读版本、标志和固定字符串，没有先核实 `msglen` 足够。主机向原函数传入一字节 REGISTER 载荷，ASan 在读取版本字段时确认 heap-buffer-overflow。其他命令分支也应逐个建立线格式最小长度与变长字段约束；输出结构体够大不能保证输入有效。[pps_protocol_nvr_conversion.c:2056](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr_conversion.c:2056)。

REGISTER 中固定长 SN/license/secret 被按字节复制，后续又用于 `%s` 和 `strlen()`；未终止字符串仍需专门处理。对 SOUND_DATA，来自载荷的 `pkg_size` 被用于申请和复制，复制前也应检查它不超过剩余输入长度。[pps_nkit_nvr_init.c:115](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_nkit_nvr_init.c:115)、[pps_protocol_nvr_conversion.c:3450](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr_conversion.c:3450)。

#### 7.1.3 音频载荷可超过固定 2 KiB 缓冲

转换函数定义静态 `abuf[2*1024]`，收到音频后直接按消息中的 size 执行 `memcpy(abuf, msg_temp, size)`，没有与 2048 或剩余输入长度比较。本次使用完整 22 字节音频子头与 2049 字节数据复现了超出目标对象的写入，ASan 报错栈指向该 memcpy。它与“报警结构体内有指针”的泛泛推测不同，是已定位的固定缓冲容量问题。[pps_protocol_nvr_conversion.c:2058](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr_conversion.c:2058)、[pps_protocol_nvr_conversion.c:3529](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr_conversion.c:3529)。

#### 7.1.4 发现 JSON 类型检查缺失

发现解析取得 `msgtype` 后直接将 `node->valuestring` 传给 `strcmp()`，没有检查节点是否为字符串。合法 JSON `{"msgtype":1}` 经原始函数即可触发空指针访问，本次 ASan/UBSan 已复现。后续 IP/SN 等节点也需按字段类型检查，解析入口应使用明确输入长度，且在写出前验证输出容量。[pps_prtp_dis_protocol.c:15](/home/tronlong/lyp/meari/pRTP/prtp/prtp_dis/src/pps_prtp_dis_protocol.c:15)。

#### 7.1.5 WLAN/DSP 解析和输出容量仍需加固

WLAN GET_LIST 将 nums 限制到 36，但在复制每项 hostname/quality 前未验证剩余 `msglen`，也未按完整列表检查输出空间。公共 `n2s` 宏按字段读取/复制并推进指针，不维护剩余输入计数。DSP/MCU 亦使用这些宏；不能把对输出结构体 sizeof 的判断当作对输入长度的校验。[pps_prtp_wlan_protocol.c:225](/home/tronlong/lyp/meari/pRTP/prtp/prtp_wlan/src/pps_prtp_wlan_protocol.c:225)、[n2s.h:29](/home/tronlong/lyp/meari/pRTP/common/inc/n2s.h:29)。

DSP/MCU `dsp_mcu_prot_unpacket()` 在转换后额外写 `prsp->prsp[prsp->lrsp]=0`。当调用方缓冲恰好等于结构体大小时，这个额外终止字节可能越过提供的空间；应移除不属于二进制结构的终止写入，或为其明确预留并验证容量。这项为源码检查发现，未执行目标设备复现。[dsp_mcu_prot.c:1398](/home/tronlong/lyp/meari/pRTP/prtp/dsp_mcu/src/dsp_mcu_prot.c:1398)。

### 7.2 优先修复：身份、保密与消息认证

#### 7.2.1 旧注册路径缺乏有效凭证验证

服务端先绑定请求 SN，再保存全局状态、通知业务，并固定回复成功。非空 SN 是格式约束，不能证明连接方身份。需在绑定与发布设备状态之前验证凭证，并让验证失败决定拒绝连接；授权检查还应覆盖后续控制命令。[pps_protocol_nvr.c:323](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr.c:323)、[pps_nkit_nvr_init.c:139](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_nkit_nvr_init.c:139)。

#### 7.2.2 发送明文、标志错误与硬编码密钥

旧 pack 未实现加密，NVNV 也未提供载荷加密，发现密码密钥由公开字段派生；这些问题分别属于不同路径。默认密钥的移位错误、ECB 的确定性、固定密钥的共用和缺少安全协商都需要处理，但不能据此宣称“只需开启开关”或“所有协议都使用同一 AES”。详见第六章。[pps_protocol_nvr.c:130](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr.c:130)、[pps_protocol_nvr.c:190](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr.c:190)、[pps_prtp_dis_api.c:135](/home/tronlong/lyp/meari/pRTP/prtp/prtp_dis/src/pps_prtp_dis_api.c:135)。

#### 7.2.3 校验可伪造，NVNV 载荷未校验

累加和与 Adler-32 无秘密，任何能修改消息的一方都可重算。NVNV 更允许不修改头校验就更改正文，本次函数测试已确认。是否最终执行敏感操作仍取决于业务鉴权和产品回调，不能把协议校验薄弱直接写成所有命令均已实现利用。[pps_prtp2_protocol.c:86](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/src/pps_prtp2_protocol.c:86)。

#### 7.2.4 发现信息与配置请求的暴露

UDP 发现消息无发送方签名或消息认证，可包含 IP/SN/版本/绑定状态，bind 消息还携带 token。发现数据应视为不可信线索，连接后的身份核验必须独立完成；改 IP/改密码的业务回调需验证调用方权限、目标 SN 及请求有效期。[pps_prtp_dis_protocol.c:313](/home/tronlong/lyp/meari/pRTP/prtp/prtp_dis/src/pps_prtp_dis_protocol.c:313)、[pps_prtp_dis_api.c:473](/home/tronlong/lyp/meari/pRTP/prtp/prtp_dis/src/pps_prtp_dis_api.c:473)。

### 7.3 可用性、共享状态与锁管理

#### 7.3.1 新连接在鉴权前替换旧连接

旧封装默认只支持一个连接；TCP accept 后若当前数量达到上限，立即关闭所有旧连接并清空状态，然后创建新客户端线程。触发条件是**建立新 TCP 连接**，无需先完成 REGISTER。底层虽将 max_clients 限制在 1–4，但不改变本封装默认 1 的事实。[pps_nkit_nvr_server.c:44](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_nkit_nvr_server.c:44)、[pps_tcp_server.c:239](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_tcp_server.c:239)、[pps_tcp_server.c:512](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_tcp_server.c:512)。

#### 7.3.2 缓冲满导致写入失败，不能推断无限积压

监听 Socket 请求 `SO_SNDBUF=400*1024`；实际内核缓冲大小以及 accepted socket 的有效配置需在运行时验证。用户态 pipe 在创建时固定分配 1 MiB，空间不足返回 -1，发送线程短发送后只推进已发长度并稍后重试。因此原稿“pipe 持续膨胀直至内存耗尽”没有源码依据；实际风险是排队延迟、失败返回、分片中断和上层若忽略返回值造成丢帧。[pps_tcp_server.c:195](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_tcp_server.c:195)、[pps_tcp_server.c:323](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_tcp_server.c:323)、[pps_tcp_server.c:377](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_tcp_server.c:377)、[pps_protocol_nvr.c:185](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr.c:185)。

#### 7.3.3 两个明确的加锁后错误返回

`nvr_add_item()` 持有 `g_mutex` 后 calloc 失败直接返回，未解锁；WLAN `pps_prtp_wlan_send_recv_message()` 持有 `slocker` 后发现发送空间不足也直接返回。这两个错误路径可能让后续调用永久等待，分别需在内存分配失败、发送拥塞条件下测试。[pps_client_mgr.c:80](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_client_mgr.c:80)、[pps_prtp_wlan_client_common.c:197](/home/tronlong/lyp/meari/pRTP/prtp/prtp_wlan/src/pps_prtp_wlan_client_common.c:197)。

#### 7.3.4 全局指针和状态保护不完整

`pps_nkit_nvr_get_device_id()` 无锁读取并返回共享数组；reset 会清空该数组。媒体开关和心跳时间的读写也不是全部受同一锁保护。协议层还复用静态 out_msg/cmd_msg1/音频缓存，在并发进入、上层持有回调指针或以后扩大连接数量时需明确生命周期和同步规则。默认单连接降低部分并发场景，但不消除调用方与后台线程的共享访问。[pps_nkit_nvr_init.c:34](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_nkit_nvr_init.c:34)、[pps_nkit_nvr_init.c:66](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_nkit_nvr_init.c:66)、[pps_protocol_nvr.c:313](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr.c:313)、[pps_protocol_nvr.c:435](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr.c:435)。

#### 7.3.5 停止逻辑和心跳规则的证据边界

`pps_tcp_server_stop()` 的客户端遍历实际上持有 `tcpsvc->lock`；原稿“循环未加锁”错误。另一关闭路径使用 `g_remove_mutex`，数量检查与部分查找也不总持有一致锁，应统一链表和对象生命周期规则；仅凭注释掉的旧检查不能断言已经证实某种竞态。[pps_tcp_server.c:250](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_tcp_server.c:250)、[pps_tcp_server.c:91](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_tcp_server.c:91)。

旧 prtp 只记录心跳时间，没有本模块的超时关闭检查，DISCONNECT 的关闭代码也被注释；NVNV 的周期保活代码未启用。WLAN 则有工作线程周期发送及 6 秒检查。这些区别需要在上层生命周期管理中明确。[pps_nkit_nvr_init.c:429](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_nkit_nvr_init.c:429)、[pps_prtp2_client_common.c:165](/home/tronlong/lyp/meari/pRTP/prtp/prtp2_client/src/pps_prtp2_client_common.c:165)、[pps_prtp_wlan_client_common.c:74](/home/tronlong/lyp/meari/pRTP/prtp/prtp_wlan/src/pps_prtp_wlan_client_common.c:74)。

### 7.4 原稿中需要收窄的其他风险描述

报警结构 `protocol_camera_warning_req_t` 的 dat 指针由序列化函数作为第二数据段读取，线上输出 dat_len 与数据字节，没有把指针地址直接发送。应核验调用方传入的容量、有效期与返回值；“C 结构体含指针”本身不能证明反序列化存在野指针漏洞。[pps_protocol_nvr_conversion.c:86](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr_conversion.c:86)。

`send_failed_cnt` 在此源码中只存在 extern 声明，没有实际引用，仅有声明不会产生未定义符号链接错误。SO_REUSEADDR 重复调用、HERAT/clinet/prtp2_clinet 拼写、未实现命令及默认构建保留弃用模块都值得清理，但应与可复现的内存错误区分。[pps_tcp_server.c:49](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_tcp_server.c:49)、[pps_tcp_server.c:184](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_tcp_server.c:184)、[src_objs.mk:20](/home/tronlong/lyp/meari/pRTP/src_objs.mk:20)。

---

<a id="section-8"></a>

## 八、代码质量与规模评估

### 8.1 已确认的工程组织优点

- 协议、网络和产品回调分开，便于复用通信组件并让应用处理具体业务。
- 结构、枚举和函数有较多中文说明；WLAN/DSP 使用公共字段转换宏，NVNV 采用结构体与 JSON 转换。
- 固定缓冲的写入路径有部分空间检查，常见空参数使用 LOG_CHECK；仍需补齐输入剩余长度与所有错误出口。
- 源对象按库收集，Linux/Windows 的串口及网络实现有条件区分。
- 多处共享状态采用 mutex/locker；需要进一步统一锁范围和返回路径，不能概括为所有共享状态均线程安全。

依据：[src_objs.mk:5](/home/tronlong/lyp/meari/pRTP/src_objs.mk:5)、[n2s.h:29](/home/tronlong/lyp/meari/pRTP/common/inc/n2s.h:29)、[pps_protocol_nvr.c:182](/home/tronlong/lyp/meari/pRTP/prtp/prtp/src/pps_protocol_nvr.c:182)、[pps_client_extend.h:43](/home/tronlong/lyp/meari/pRTP/common/inc/pps_client_extend.h:43)。

### 8.2 当前维护重点

命令枚举、转换函数和回调分发在多个文件维护；当前 `pps_nkit_nvr_recv_cmd_cb()` 有 **82 个 case 标签**，最大源文件 `pps_protocol_nvr_conversion.c` 为 **3576 行**。新命令的输入边界、转换、回调和响应必须同时更新，单看枚举容易误判支持范围。部分命令是未实现的占位函数，头注释也有陈旧长度描述。

本快照含 JSON 测试代码，但 Makefile 指向的协议测试目录缺失。不能把“仓库没有核心测试”绝对化为开发团队从未写过测试；可确认的是当前快照无法按声明直接运行这些测试。旧客户端弃用状态、公共 include 拼写及版本脚本权限也需要统一。

### 8.3 当前源码精确统计

统计采用读取文件后按物理行计数（含注释、空行），只收集 `.c`、`.cc`、`.h`，未把二进制测试数据、报告或构建产物计入。该数字不同于有效代码行和产品最终编译行。

| 模块目录 | C/C++ 文件数 | 源文件行数 | 头文件数 | 含头文件总行数 |
| --- | --- | --- | --- | --- |
| `prtp/prtp` | 9 | 8,383 | 22 | 13,216 |
| `prtp/prtp2_client` | 6 | 2,421 | 8 | 7,986 |
| `prtp/prtp_client` | 4 | 1,610 | 7 | 3,310 |
| `prtp/prtp_wlan` | 4 | 919 | 7 | 1,347 |
| `prtp/prtp_dis` | 2 | 921 | 2 | 1,086 |
| `prtp/dsp_mcu` | 1 | 1,407 | 2 | 2,127 |
| `common`，不含 cjson | 14 | 4,965 | 26 | 10,064 |
| `common/cjson`，含测试 | 4 | 1,640 | 3 | 2,132 |
| **合计** | **44** | **22,266** | **77** | **41,268** |

44 份源文件由 43 个 `.c` 与 1 个 `.cc` 构成。`common/cjson/test/main.c` 为 128 行，`test.cc` 为 209 行；扣除这两份测试后，源文件物理行合计为 **21,929**。common 的统计包含 Windows/POSIX 两套串口源文件，不能按“默认 Linux 实际编译”理解。原稿约 15,765/15,700 行属于未定义口径的估计，已由本表取代。

---

<a id="section-9"></a>

## 九、总结与建议

### 9.1 核验后的总体判断

pRTP 仓库提供多类安防通信组件，涵盖旧 IPC/LCD 控制与媒体、NVNV 的 VMS/NVR 登录及预览回放、WLAN 接入、局域网发现和 MCU 消息。业务回调使协议组件具备复用价值，但不同协议族、产品工程和外部 OSAL 的边界必须明确；“有命令/接口”不能一概解释为完整业务或完整产品已经实现。

当前应优先处理解析边界、身份检查、载荷保密与消息认证，以及几个错误出口未解锁的问题。四个输入边界路径已在主机原函数中复现；其设备可达性和产品后果仍需按具体固件确认。原稿对 prtp2 镜像协议、发现密码保护、心跳超时、无限积压和停止无锁等判断已经修正。

### 9.2 建议修复顺序

1. **输入安全**：在所有入口检查完整头和合法长度；为每命令建立固定最小长度、变长字段上限、输入剩余长度及输出容量检查。优先修复旧帧头、REGISTER、音频 2 KiB 缓冲和发现 JSON 类型。
2. **错误路径**：修复客户端管理 calloc 失败和 WLAN 发送空间不足时的漏解锁；用受控失败与拥塞测试验证后续请求不会挂起。
3. **身份与连接**：验证通过后才绑定 SN、发布状态和执行命令；拒绝非法凭证，并避免未经验证的新连接自动替换合法连接。
4. **安全协议**：补齐真实的加密封包/解包和兼容协商；使用秘密来源的密钥及经审查的认证加密或安全传输方案。不能只切换 req_encrypt，也不能仅添加可重算校验和。
5. **重放与权限**：在已认证消息中纳入请求序号、会话标识和 freshness 规则；noce/UUID 字段存在、HMAC 或时间戳单独存在，都不能替代完整策略。
6. **生命周期**：统一状态锁、对象所有权、回调数据有效期、心跳、断线与停止规则；上层明确发送失败时重试或丢帧，并处理视频分片中断。
7. **构建与测试**：恢复配置/测试资料、脚本执行权限和真实版本头生成；修正 include 拼写并记录外部 OSAL 依赖，再做产品链接、协议互通及压力测试。

### 9.3 后续验证项目

主机边界测试应覆盖短头、截断字段、异常类型、超大长度、错误校验、环形缓冲满和分片中途失败；并发测试覆盖重复连接、关闭期间发送、注册/reset 与回调持有数据。设备端还需确认实际开放端口、业务权限、NVR 挑战有效期、WLAN token 校验、串口输入边界和目标 ABI。这里只给出验证要求，不将未执行项目写成通过。

---

<a id="section-10"></a>

## 十、核验方法、结果与原稿修订记录

### 10.1 核验范围和方式

核验时间为 2026 年 10 月 4 日。检查源码目录清单、README、配置与构建文件，并沿旧 prtp、NVNV、WLAN、发现、DSP/MCU 的封包、找包、解包、转换、回调、收发和生命周期路径核对。本文引用的绝对路径和行号来自本次快照；后续源码更新后需重新定位。

本轮完成静态阅读、文件物理行统计、临时副本的主机静态库编译，以及原始函数的定向验证。没有目标设备、真实业务服务端或交叉编译环境运行结果；未完成设备端漏洞利用或产品全链路集成测试。

### 10.2 构建验证

在 `/tmp` 复制源码以隔离输出，覆盖 `CONFIG_CROSS_COMPILE_PREFIX=`、`CONFIG_EXTRA_CFLAGS=`、`CONFIG_NO_SHARED=y`，运行 `make targets -j4`。日志组件依赖生成的 `version.h/git_info.h`，本次在临时 build 中提供仅含审计占位宏的头文件后，**11 个静态库均成功归档**。

该结果证明此设置下源对象可由主机 GCC 编译，不证明外部符号全部可链接、目标 MIPS 构建成功、共享库加载成功或固件互通。未经处理直接运行默认 `all` 时，版本脚本无执行权限；`test` 所需顶层 test 目录也不在快照。占位头文件不代表恢复了真实版本/Git 元数据。

### 10.3 原函数验证结果

测试编译直接使用源码中的函数，采用 ASan/UBSan。测试未改动源库；为在当前受监控进程环境运行关闭了 LeakSanitizer 的退出扫描，未执行泄漏检测。

| 检查 | 输入/条件 | 结果 |
| --- | --- | --- |
| 正常找包 | 14 字节旧 prtp 空载荷帧 | 返回完整帧长度 14 |
| 初始化密钥 | 调用原始 `pps_protocol_nvr_init()` | 得到第 6.1 节所列字节；req_encrypt 默认为 0 |
| 头结构布局 | 主机 sizeof | 旧头 10，NVNV 头 48，NVNV 媒体头 48 字节 |
| 发现缺省值 | 只含 discovery 字符串类型 | 正常解析，port=11224，video_in=1 |
| 旧发送加密标志 | 16 字节载荷、encrypt=1 | 包长度 30，flag 为 1，载荷仍为明文 |
| NVNV 正文修改 | 生成有效头后修改 payload | 原始 unpack 仍成功，头校验未覆盖正文 |
| 短帧头 | 恰好四字节 magic 的分配区 | ASan heap-buffer-overflow，定位 `find_one_packet()` |
| 短 REGISTER | 一字节载荷 | ASan heap-buffer-overflow，定位 REGISTER 版本读取 |
| 错误 JSON 类型 | `{"msgtype":1}` | UBSan 空指针参数及 ASan 空地址访问，定位 strcmp |
| 超大音频 | 22 字节子头 + 2049 字节数据 | ASan 确认 memcpy 超出固定 2048 字节目标对象 |

异常测试以触发诊断并退出为预期结果；它们不是修复后的回归通过。短头/短 REGISTER 的精确分配区验证确认了 API 的长度约束问题，在服务端的大 rbuf 中还可能表现为读取未接收数据。超大音频则超过静态目标缓冲容量。具体产品影响需结合调用链、编译选项及运行环境确认。

### 10.4 原稿主要修订对照

| 原稿描述 | 当前源码核验后的结论 |
| --- | --- |
| prtp2 是旧服务端镜像，使用相同二进制帧 | 独立 NVNV 头、JSON/媒体格式及挑战鉴权 |
| NVR 侧新客户端直接连接多 IPC | README 定位是 VMS 接入 NVR，不能混用服务端角色 |
| 服务端上限为 4，新连接注册后踢旧连接 | 底层 clamp 为 1–4，封装默认 1，accept 后注册前即可替换 |
| 旧注册已验证 license/secret | 当前只解析/通知，固定成功响应 |
| 将 req_encrypt 改为 1 即可启用加密 | pack 只写标志，无发送加密；密钥解码还有移位截断 |
| 动态密钥完全无更新入口 | 有外部 setter，安全协商仍未在此实现 |
| 发现密码 AES 可防止 UDP 泄露 | key 由公开 UUID/SN 派生，不能形成可靠保密 |
| 发现统一回复 hello，缺省端口 10000 | 当前服务回复 discovery；缺省 port=11224 |
| hello 总是有 token/bind_status | token 在 bind 输出；线上字段名为 bind |
| WLAN 支持 36 即 NVR 产品最多 36 路 | 36 是当前客户端名单解析上限 |
| 6000 ms 是 prtp2 已运行的心跳超时 | 宏未使用，同步等待常用 6000 ms；保活循环被注释 |
| 旧 DISCONNECT/超时会清空关闭 | DISCONNECT 关闭代码被注释，无记录时间的超时消费 |
| pipe 积压会持续扩容并耗尽内存 | 固定 1 MiB，满时失败；延迟、丢帧和分片中断更有依据 |
| stop 客户端循环没有加锁 | 该遍历持有 tcpsvc->lock；其他路径需统一同步规则 |
| send_failed_cnt extern 会链接失败 | 仅声明且未使用，不会单凭声明产生未定义符号 |
| 报警 C 结构内指针即线上野指针风险 | 已显式序列化 dat_len + 数据，需检查调用方容量和有效期 |
| MCU 仅基础/OTA/WiFi/蓝牙四类 | 还含 1601–1616 网络/本地心跳及 2000 调试 |
| 总量约 15,765 行 | 当前源文件 22,266 行，含头文件 41,268 行 |

---

*— 报告结束 —*

本版源码结论以可定位的实现及明确记录的验证为依据；原稿、源码检查、主机结果和待验证事项按上述范围解释。
