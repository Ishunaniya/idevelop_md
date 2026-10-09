# nvr-wlan 仓库深度分析报告

> 分析与核验日期：2026 年 10 月 8 日；独立复核与补全：2026 年 10 月 9 日。  
> 核验源码：`/home/tronlong/lyp/meari/nvr-wlan`；报告输出：`/home/tronlong/lyp/perCode/idevelop_md/meari/nvr-wlan`。  
> 结构参照：[mpc-base分析报告.md](/home/tronlong/lyp/perCode/idevelop_md/meari/mpc-base/mpc-base分析报告.md)。本报告沿用其十二章组织方式，内容按 nvr-wlan 的实际职责重新分析。

### 阅读导引

这套程序负责给 NVR 建立摄像机连接所需的网络环境，维护摄像机连接与父子关系，并把 NVR 的唤醒、删除和解绑要求转成控制消息。阅读时可沿着“启动进程 → 配置网络 → 摄像机接入 → 更新拓扑 → 收发控制消息 → 断线清理”的顺序理解；具体执行路径见第七章。

第一次通读可按第一至十二章顺序阅读。希望先掌握全貌，可先读第一、第四、第七、第十一章，再查第五、六章的实现细节。核对问题与证据时，重点看第八章的实际测试、第十章的问题汇总和第十二章的覆盖清单；各节末尾的源码链接用于复查对应实现。

| 章节 | 主要回答的问题 |
|---|---|
| [一、仓库概述](#chapter-01) | 程序负责什么，与 NVR 主服务如何分工？ |
| [二、支持平台](#chapter-02) | 有哪些构建配置，运行时如何选择网络类型？ |
| [三、目录结构总览](#chapter-03) | 文件如何组织，哪些属于业务代码与第三方工具？ |
| [四、核心架构设计](#chapter-04) | 进程、线程、共享状态和外部组件如何协作？ |
| [五、核心模块深度解析](#chapter-05) | 启动、网络、拓扑、TCP 和私有协议具体怎样实现？ |
| [六、工具库详解](#chapter-06) | 地址、路由、DNS、无线扫描和第三方工具有哪些能力及边界？ |
| [七、关键数据流分析](#chapter-07) | 一次启动、接入、唤醒或删除会经过哪些模块？ |
| [八、编译构建体系](#chapter-08) | 如何构建，哪些检查已执行，当前阻碍是什么？ |
| [九、接口设计规范](#chapter-09) | 返回值、索引、字符串、字节序和线程访问有哪些约定？ |
| [十、安全与加密](#chapter-10) | 接入与授权如何处理，已确认的问题影响哪些路径？ |
| [十一、典型使用场景](#chapter-11) | 不同网络类型和设备状态下，程序实际如何工作？ |
| [十二、总结](#chapter-12) | 已覆盖哪些文件，哪些结论成立，哪些验证尚未完成？ |

### 常用术语

| 术语 | 本报告中的含义 |
|---|---|
| NVR | 网络录像机；本文重点分析其网络管理进程及与主服务的协作 |
| IPC | 网络摄像机；文中的“IPC 接入”指摄像机接入，进程间通信另作说明 |
| CPU / MCU | 低功耗摄像机的两个控制端身份，分别使用本地 TCP 8045 / 8046 服务 |
| AP | 无线接入点模式，供摄像机连接 NVR 的无线网络 |
| PoE | 本文对应内部有线网口初始化分支，具体供电控制需另查外部实现 |
| RPC / SDK | 向 NVR 主服务调用功能的接口／产品开发依赖，部分实现不在本目录 |
| hostname | 本地列表和控制协议使用的设备标识，不能直接视为已验证的 DNS 名称 |
| 拓扑 / 父节点 | 摄像机之间的连接关系，以及本地记录的上级设备关系 |
| hostapd / wpa_cli / udhcpd | 分别用于 AP 管理、无线控制命令和 DHCP 地址分配的外部程序 |
| WEXT / ioctl / ABI | 无线扩展接口／向内核或驱动提交请求的方式／双方必须一致的数据布局与调用约定 |
| 隔离测试 / fixture | 使用原始源码和外部依赖替身测试局部行为，不代表整机运行结果 |
| ASan | 检测内存越界等问题的测试工具；报告中的错误输出是缺陷复现证据 |

### 核验方法与范围

**证据约定**：下文“已确认”指直接核对到当前源码或本次隔离测试结果；“静态问题”指可从具体语句与调用条件推导的问题，尚未在整机复现；“待验证”指需要缺失 SDK、驱动、产品配置或运行证据。源码存在、编入目标、入口调用、设备运行成功是四种不同证据，不相互替代。

**核验边界**：已清点整个目录，核对全部 16 个顶层 C/C++ 源文件、顶层 TCP 头文件和 23 个 inc 头文件，追踪启动、AP/PoE/7628 初始化、信道选择、拓扑、TCP、私有协议、RPC 和消息消费链；检查第三方工具构建配置、与主进程交互的入口及 ATBM 定制实现。第三方源树包含近百万行 C/C++/头文件，未对其每一个密码算法、驱动后端和协议状态机做逐函数安全审计。已对关键疑点编译原始源码进行隔离复现，未运行设备初始化程序、修改系统网络、连接真实 NVR 或进行上板测试。不能将本报告理解为完整固件通过验证或全库不存在其他缺陷。

**原稿完成度复核**：10 月 8 日原稿共 598 行，实际止于第五章 5.9 节，第六至十二章缺失，且存在指向缺失第八章的引用。因此原稿未完成。10 月 9 日复核补齐十二章，增加网络工具库、第三方工具关联、构建、接口、安全、数据流、场景和证据覆盖表，并纠正部分“静态确认／隔离复现”的混用。当前交付状态为**本地源码分析文档已补齐；全库逐函数安全审计、产品 SDK 构建和整机验证未完成**，具体边界见 12.3。

**独立复核依据**：复核时全部 3,788 个非 .git 文件的内容 SHA-256 与前次清单一致，未发现新增、删除或内容改变；文件权限不在该哈希比对范围。重新执行原隔离脚本，主要输出与旧日志一致；另新增带断言的工具函数测试和严格编译探测。源目录未修改。本报告保留测试方法与结果描述，测试脚本、原始日志、快照清单和旧稿备份不随文提供；复查方式见 12.4。

---

<a id="chapter-01"></a>

## 一、仓库概述

`nvr-wlan` 是一套面向嵌入式 Linux NVR 的**独立网络管理与 IPC 控制进程**。顶层默认构建产物为 `build/nvr_wlan`。它等待 NVR 主服务就绪，从外部 RPC/共享数据获取设备与通道信息，初始化对应网络环境，再提供两个 TCP 控制端口，协调普通 IPC 和低功耗 IPC 的 CPU/MCU 连接。

从可追踪的执行路径看，核心职责包括：

- 按 NVR 的 network 类型选择标准 WiFi AP、PoE 内部网口或 MTK7628 私有命令路径。
- 从 NVR 读取 SSID、密码、信道及增强模式，配置 hostapd、地址和 DHCP。
- 维护以 hostname 标识的 IPC 列表、质量值、父节点与通道编号，向主服务提交拓扑索引。
- 解析 IPC 注册、申请连接、设备列表、心跳和信息应答；向 IPC 发送唤醒、解绑复位、信息查询和网络开关。
- 读取外部共享消息缓冲中的唤醒/删除/解绑事件，将其转换为 TCP 控制请求。
- 带有通用 IPv4/IPv6、路由/DNS、Socket、Wireless Extensions 工具和独立 hostapd/wpa_supplicant/iwpriv 源树。

**本仓库没有顶层视频采集、编码、录像存储或 AI 检测实现。** `nvr_wlan_rpc.cc` 中的媒体缓冲大小 getter 只是读取外部设备信息字段；`NVR_WLAN_PRTP/PRTP2` 在这里主要用于映射传给 NVR 的 factory，不构成 pRTP 媒体传输实现。低功耗状态通过 RPC 上报，CPU 是否真的休眠/唤醒由 IPC、MCU、驱动及主服务完成。[main.cc:13](/home/tronlong/lyp/meari/nvr-wlan/main.cc:13)、[nvr_wlan_rpc.cc:23](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_rpc.cc:23)、[nvr_wlan_rpc.cc:248](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_rpc.cc:248)。

### 核心定位

~~~text
NVR 主进程 / 产品 SDK（本目录之外）
  ├── RPC：设备、WiFi、通道、在线信息、事件、看门狗
  ├── CShareDataBuf：通道数量与连接状态
  └── NvNetBuf "app-msg-push"：业务消息
                    ↕
             nvr_wlan 独立进程
  ├── 网络环境初始化：WiFi AP / PoE / 7628
  ├── hostname 列表与父子拓扑
  ├── TCP 8045：普通 IPC / 低功耗 CPU 控制连接
  └── TCP 8046：低功耗 MCU 控制连接
           ↕                      ↕
  原生 Socket / ioctl      外部系统命令服务
                           /var/systemCmd.socket
                                  ↓
           hostapd / wpa_cli / udhcpd / iwpriv /
           ifconfig / route / ifplugd / 产品脚本
                                  ↓
                Linux 网卡与产品 WiFi 驱动
~~~

图中的两条底层路径都存在：TCP 与扫描 ioctl 在本进程执行；`pps_system_cmd` 通过 Unix Socket 交给外部服务执行文本命令。不能将所有网络动作解释为本进程直接调用 `system()`。

### 源码快照

| 项目 | 本次读取结果 |
|---|---|
| Git 分支 | master |
| HEAD | `980e9345627f174e22f06a2da87c71eaa9918404` |
| 最新提交 | 2024-12-09 17:05:31 +08:00；提交说明为解决 jingle 自动信道设置问题 |
| 工作树 | `git diff --summary` 显示 3,751 项 mode change；`git diff --numstat` 没有非零文本增删项，但对二进制有差异标记 |
| 快照核对范围 | 本次核对全部 3,788 个非 .git 文件的路径、字节数和 SHA-256；独立清单不随文提供 |

HEAD 标识源码历史基点；工作树并非 clean，且附带旧二进制，不能仅以 HEAD 推定全部文件内容与已发布固件一致。

---

<a id="chapter-02"></a>

## 二、支持平台

### 2.1 构建脚本中列出的产品配置

以下是 `all.sh` 的实际菜单与传入参数。表述为“构建配置”，不意味着已在这些设备上验证成功。

| 菜单项 | CROSS_COMPILE | ARCH_PATH |
|---|---|---|
| nc7 | `arm-buildroot-linux-uclibcgnueabihf-` | `arm-buildroot-linux-uclibcgnueabihf-linux` |
| nt7 | `mipsel-MTK7628_MR-linux-uclibc-` | `mipsel-buildroot-linux` |
| nd7 / nd5 | `mips-linux-uclibc-` | `mips-a1-linux-uclibc-linux` |
| x86_64 | 空，使用主机 gcc/c++/g++ | `x86_64` |
| t23n | `/opt/toolchains/ingenic/t23/mips-gcc540-glibc222-64bit-r3.3.0.smaller/bin/mips-linux-gnu-` | `mips-t23-linux-uclibc-linux-jingle` |

来源：[all.sh:5](/home/tronlong/lyp/meari/nvr-wlan/all.sh:5)。不能仅由 nc7/nd7 产品名进一步确定完整 SoC 型号、无线芯片、通道数量或硬件能力。

### 2.2 构建平台与运行网络类型的区别

运行时 `nv_get_nvr_type()` 调用外部 `nv_get_support_network` 获取类型；`nv_wlan_init()` 分别处理：

| support_network 值 | 初始化入口 | 本地可确认的行为 |
|---|---|---|
| 0 | `pps_wifi_init` | 标准 hostapd AP 路径 |
| 1 | `pps_poe_init` | eth1 内部网络、路由、DHCP 与静态租约 |
| 2 | `pps_7628_init` | ra0 的 SSID/WPAPSK/WPA2PSK/AES 私有命令 |
| 其他 | 无对应分支 | 仍可能继续处理 eth0 和 hostname 列表 |

`all.sh` 的菜单未设置上述运行类型；芯片/模块路径检测又会影响 AP 的信道算法。产品型号、构建架构、network 类型和检测到的驱动文件是不同维度。[nvr_wlan_rpc.cc:293](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_rpc.cc:293)、[nvr_wlan.c:550](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan.c:550)。

### 2.3 当前可构建性的实际状态

指定源码目录的兄弟 `/home/tronlong/lyp/meari/nvr-pack` 不存在。直接执行只读 `make -n all`，在 Makefile 第 28 行即停止：

~~~text
Makefile:28: *** nvr-pack path is null.  Stop.
~~~

因此本次没有得到真实产品的编译、链接或部署结果。主机隔离测试能够验证一些独立函数；它们不能代替 nc7、nt7、nd7/nd5、t23n 的产品 SDK 构建。x86 配置还有 `-muclibc` 与无效 EXTRA_CFLAGS 等问题，详见第八章。

---

<a id="chapter-03"></a>

## 三、目录结构总览

~~~text
nvr-wlan/
├── Makefile / all.sh              # 主进程构建及产品菜单
├── main.cc                       # main：等待 NVR → 共享数据 → 网络 → TCP
├── nvr_wlan.c                    # hostname 列表、父子拓扑、状态检测
├── nvr_wlan_rpc.cc               # 外部 NVR RPC/共享数据适配
├── nvr_wlan_pushmsg.cc           # app-msg-push 缓冲读取
├── nvr_wlan_tcp_server.c/.h      # 双监听服务、每连接线程、唤醒/复位标志
├── nvr_wlan_protocol.c           # 帧同步、校验、命令分发与打包
├── nvr_wlan_common.c             # 单调时钟秒数
├── pps_netmng.c                  # AP/PoE/7628 初始化及信道选择分流
├── pps_device_ap.c               # 旧 AP 事件监听代码已注释
├── pps_cmd_cli.c                 # 外部 Unix Socket 系统命令客户端
├── wifi6_best_ch.c               # ATBM606x 扫描消息适配
├── atbm_ioctrl.c                 # SIOCDEVPRIVATE ioctl
├── iwpriv.c / iwlib.c            # 内嵌 WEXT 私有扫描及工具库
├── netconfig.c                  # 网卡、地址、路由、DNS、统计与校验
├── net_base_operate.c            # Socket/读写/地址转换工具
├── inc/                         # 23 个头文件
├── tool/
│   ├── iwpriv_code_v0.7.3/       # 独立 ATBM 示例工具，含旧 .o/.d/ELF
│   ├── wpa_supplicant-2.9/       # 独立 supplicant/CLI/P2P 等源树
│   └── hostapd/
│       ├── hostapd-2.9_ATBM_0.1/
│       │   └── hostapd-2.9_ATBM_0.1/  # 实际 hostapd/src 所在层
│       ├── libnl-3.4.0/          # netlink 库源码与已配置构建树
│       └── libnl/                # 安装树：头文件/库/pkgconfig 等
├── README.md                    # 仅“介绍 / nvr wlan”
├── .clang-format / .gitignore
└── build/                       # Makefile 输出路径；本次原目录未生成
~~~

### 3.1 规模统计与统计口径

统计当前磁盘文件，排除 .git，包含工具目录中已存在的对象、依赖文件和二进制。代码文件只统计 `.c/.cc/.cpp/.h`；行数包含注释、空行和重复副本。

| 范围 | 文件数 | C/C++/头文件数 | 代码文件行数 | .o/.d/.a/.so 文件数 |
|---|---:|---:|---:|---:|
| 根目录 | 22 | 17 | 11,616 | 0 |
| inc | 23 | 23 | 4,222 | 0 |
| tool/iwpriv_code_v0.7.3 | 20 | 8 | 8,021 | 8 |
| tool/wpa_supplicant-2.9 | 1,163 | 711 | 480,117 | 312 |
| tool/hostapd，包含 libnl 两类树 | 2,560 | 1,107 | 478,046 | 654 |
| 合计 | **3,788** | **1,866** | **982,022** | **974** |

`.so.200` 等带版本后缀文件不计入最后一列；这不是“所有 ELF 数量”。业务根目录及 inc 合计 40 个文件、15,838 行，第三方代码规模不能算作本仓库自行实现的业务量。

### 3.2 实际默认编译范围

顶层 `obj-y` 只展开根目录 `*.c/*.cc`：13 个 C 文件和 3 个 C++ 文件。不会递归构建 `tool`，也不把 hostapd/wpa_supplicant/libnl 静态合入 nvr_wlan。`pps_device_ap.c` 虽被编入，其旧控制线程实现已注释；“编入源文件”也不意味着对应旧功能在运行。[Makefile:49](/home/tronlong/lyp/meari/nvr-wlan/Makefile:49)、[pps_device_ap.c:68](/home/tronlong/lyp/meari/nvr-wlan/pps_device_ap.c:68)。

---

<a id="chapter-04"></a>

## 四、核心架构设计

### 4.1 总体协作关系

这是按职责划分的协作图；业务文件之间并非严格单向分层。

~~~text
main.cc
  ├── nvr_wlan_rpc.cc ── StorageRPC / nvbaserpc / CShareDataBuf
  │       ├── 看门狗线程
  │       └── 消息线程 ── nvr_wlan_pushmsg.cc ── 外部 NvNetBuf
  │                         └── 唤醒/删除/解绑 → TCP 客户端标志
  ├── nvr_wlan.c
  │       ├── pps_netmng.c → 标准 WiFi / PoE / 7628
  │       │       ├── pps_cmd_cli.c → 外部命令服务
  │       │       ├── iwpriv.c + iwlib.c → ATBM603x 私有扫描
  │       │       └── wifi6_best_ch.c + atbm_ioctrl.c → ATBM606x
  │       └── hostname / chan / quality / parent 数组
  └── nvr_wlan_tcp_server.c（8045 + 8046）
          ├── nvr_wlan_protocol.c → 解析命令、生成应答
          │       ├── 更新拓扑
          │       └── RPC 添加/上报 IPC 信息
          └── 每连接线程 → 心跳、发送缓冲、断线状态上报
~~~

### 4.2 状态与执行单元

| 状态/对象 | 来源与持有位置 | 使用方式 |
|---|---|---|
| `g_nvr_info` | 等待 NVR 后通过 RPC 获取一次 | 缓冲大小、设备信息 getter |
| `g_max_chan_nums` | `CShareDataBuf::GetChanNum()` 初始化缓存 | 限定通道遍历 |
| `g_share_data_` | `SDATA_READONLY` 模式构造 | `GetChanStatus(chan)` 轮询 |
| `wlan_device_list` | 固定数组，长度为外部 `NV_CHAN_MAX` | hostname、chan、quality 和父指针 |
| `g_server/g_mcu_server` | 两个堆分配的服务对象 | 各自有 mutex 和客户端链表 |
| `nvr_tcp_client_t` | 每次 accept 后分配 | 两个 10 KiB 缓冲、状态标志及 MCU 指针 |
| `g_wlan_msg_push_buf` | 外部 `NvNetBuf` 对象 | 消费 app-msg-push 中的事件帧 |
| `lan_ip` | 标准 AP 或 PoE 初始化时写入 | 注册应答返回内部 IPv4 地址 |

常驻执行单元可确认有：看门狗线程、消息消费线程、两个监听线程，以及每连接一个 detached 客户端线程。网络初始化主要在 main 启动流程中同步执行；没有另一个本地 AP 状态管理线程接替它。线程调度和共享缓冲内部同步规则依赖外部库，不能只由 `unique_ptr` 或 mutex 字段认定所有并发访问安全。

### 4.3 生命周期与失败传播

`main` 只对 `nv_wlan_init_share()==NV_FAILURE` 做失败退出处理；未检查 `nv_wlan_init` 和 `nvr_tcp_server_init` 返回值。`nvr_tcp_server_init` 的监听绑定在新线程中执行，bind/listen 失败不会回传 main；线程创建报错后函数仍返回 0。主循环只 `sleep(10000)`，没有 graceful stop、信号清理或重连总控。[main.cc:17](/home/tronlong/lyp/meari/nvr-wlan/main.cc:17)、[nvr_wlan_tcp_server.c:352](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_tcp_server.c:352)、[nvr_wlan_tcp_server.c:514](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_tcp_server.c:514)。

因此“main 持续运行”“喂狗线程工作”“初始化日志打印 ok”均不能证明 AP、DHCP 或两个监听服务已经成功工作。

---

<a id="chapter-05"></a>

## 五、核心模块深度解析

本章按执行关系展开：先说明启动与 NVR 主服务接口，再说明网络初始化和信道选择，随后分析设备拓扑、TCP 连接和协议，最后串起低功耗控制、外部消息与系统命令。每节先描述正常行为，再列出问题及其触发条件；第七章将这些模块连接成完整流程。

### 5.1 main 与 RPC 模块——启动入口及 NVR 适配

**文件**：`main.cc`、`nvr_wlan_rpc.cc`、`inc/nvr_wlan_rpc.h`。

#### 5.1.1 启动顺序

`main` 设置 NV_WARN 日志等级后按以下顺序执行：

1. `nv_wlan_wait_nvr_start()`：循环调用 `StorageRPC::is_nvr_start()`，未就绪每次休眠 2 秒；没有总等待上限。
2. NVR 就绪后再休眠 3 秒，启动看门狗任务，每 `WATCH_DOG_FEED_INTERVAL` 调用 `nv_feed_watch_dog(WATCH_DOG_WLAN)`。
3. 通过 `StorageRPC::get_nvr_device_info` 填充 `g_nvr_info`，启动 detached 消息线程。
4. `nv_wlan_init_share()`：构造 `CShareDataBuf(SDATA_READONLY)`，缓存通道数。
5. `nv_wlan_init()`：初始化本地拓扑 mutex，选择网络类型，处理 eth0 并拉取 hostname。
6. `nvr_tcp_server_init()`：创建 8045 和 8046 服务线程。
7. main 常驻休眠。

设备信息 RPC 返回结果与消息线程创建结果未完整检查；`new (nothrow)` 判空只确认对象分配结果，不能据此证明外部共享数据映射已有效。[nvr_wlan_rpc.cc:76](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_rpc.cc:76)、[nvr_wlan_rpc.cc:99](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_rpc.cc:99)。

**启动窗口问题（静态）**：消息线程在共享数据及 TCP 服务初始化前已启动。唤醒入口会检查 `g_mcu_server`，但复位入口 `nvr_wlan_dev_reset` 没有检查 `g_server`。如果在该窗口收到能解析到 hostname 的删除/解绑事件，存在访问未就绪服务对象的路径；是否实际出现取决于外部消息到达时序，未做整机复现。[nvr_wlan_rpc.cc:130](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_rpc.cc:130)、[nvr_wlan_rpc.cc:342](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_rpc.cc:342)、[nvr_wlan_tcp_server.c:489](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_tcp_server.c:489)。

#### 5.1.2 RPC 能力与结果契约

| 本地入口 | 外部调用或数据 | 本地行为/边界 |
|---|---|---|
| `nv_wlan_get_chan_ip` | `nv_get_ipc_info` | 成功时按 MODC_COMM_LEN 写出 IP，接口无独立输出容量参数 |
| `nv_wlan_get_chan_param` | `nv_get_chan_param` | 只提取 hostname |
| `nv_wlan_get_chan_status` | `GetChanStatus` | 仅值 1 返回 1，其余变为 0 |
| `nv_wlan_get_chan_quality` | `nv_get_chan_frame_loss` | 初值 1，忽略 RPC 返回；不是无线 RSSI 测量 |
| `nv_wlan_get_wifi_info/config` | `nv_get_wifi_info` | 输出配置，未检查该 RPC 结果 |
| `nv_wlan_set_chan_superidx` | `nv_set_chan_superidx` | 原样转发结果 |
| `nv_wlan_add_ipc_2_ipcham` | `nv_add_ipc_2_ipcham` | 构造 ipchan_param_t 后请求添加/更新 |
| `nv_wlan_pull_ipc_info` | `nv_get_online_ipc` | 尽管外部函数名有 get，实参携带在线/离线信息 |
| `nv_wlan_update_lp_ipc_status` | `nv_rpc_send_batterycam_msg` | 枚举强制转换，调用后本地总返回 0 |
| `nv_wlan_get_login_status` | `nv_rpc_get_login_status` | 返回本地变量；当前主链未使用其鉴权 |
| `nv_wlan_set_event_trigger` | `nv_rpc_set_event_trigger` | 上报扫描开始/结束等外部事件 |
| 各 stream/speak/snapshot/playback size getter | `g_nvr_info` 字段 | 没有建立或搬运相应媒体流 |

**添加参数映射**：普通 IPC 的 `para.protocl=0`，其他类型为 1；`type=0`；协议类型等于 `NVR_WLAN_PRTP2` 时 `factory=3`，否则 1。携带 hostname、IP、端口、dev_pid 和 is_qrcode，实际通道分配/持久化/鉴权由外部主服务实现。本目录不能证明 `factory=3` 的完整驱动或 pRTP2 接入成功。[nvr_wlan_rpc.cc:222](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_rpc.cc:222)、[nvr_wlan_rpc.cc:248](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_rpc.cc:248)。

**在线信息约束**：`nv_wlan_pull_ipc_info` 要求 hostname 与 version 非空，否则返回 -1，TCP 首次握手及异常断线未拿到 version 时可能无法通过该入口上报；调用方没有统一重试机制。`strncpy(...,len-1)` 等 getter 不自行显式终止输出，当前若调用者使用清零缓冲通常有终止符，API 本身不能保证任意调用者安全。

#### 5.1.3 “加密状态”接口的实际含义

`nv_get_nvr_encryption_status` 只是比较外部设备 `serial_no` 是否为一个指定占位字符串。没有在该函数中执行加密算法；`pps_netmng_start` 中与它相关的测试热点连接代码已注释。不能据其名称得出“所有通信或配置已加密”的结论。[nvr_wlan_rpc.cc:279](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_rpc.cc:279)、[pps_netmng.c:399](/home/tronlong/lyp/meari/nvr-wlan/pps_netmng.c:399)。

### 5.2 pps_netmng——三种网络初始化路径

**文件**：`pps_netmng.c`，并使用 `netconfig.c` 和外部系统命令服务。

#### 5.2.1 标准 WiFi AP

`pps_wifi_init → pps_netmng_start → pps_netmng_hostapd_start` 的执行步骤可确认如下：

1. 从 RPC 获取 SSID、密码、wlan_channel、wlan_better 与型号名称。
2. 创建 DHCP 租约文件；发送移动 `/dev/random` 并链接 `/dev/urandom` 的命令。
3. 开启 p2p0/wlan0，按模块路径发送 ATBM 功率/频率命令。
4. 用 sed 修改 `/etc/hostapd.conf` 的固定第 7、14 行（SSID/密码）；第 9 行（信道）由后续分支生成。
5. 查询 eth0 IPv4，关闭已有 hostapd，等待 PID 文件消失。
6. 启动 hostapd，给 p2p0 配置内部地址，启动 udhcpd。
7. 等待 hostapd PID，读取 `hostapd_cli -ip2p0 status` 写出的文件；状态以 ENABLED 或 ACS 开头就接受，否则反复重启。

源码中的“set_freq 14,2380”是传给产品驱动的私有命令。其实际频率、带宽、功率及驱动接受结果需要产品驱动证据；它不能按常规信道名直接等同于标准 14 信道的实际射频参数。[pps_netmng.c:216](/home/tronlong/lyp/meari/nvr-wlan/pps_netmng.c:216)。

**内部地址策略**：标准 AP 用字符串前缀 `strncmp(eth0_ip,"172.168.0",9)` 检测冲突，匹配时选 `162.168.0.1/24` 并替换 DHCP 文件，否则用 `172.168.0.1/24`。这不是通用子网冲突检测，也没有完整候选子网搜索；地址在源码中写死。替换过的 DHCP 文件在 else 分支没有恢复为 172.* 的对称操作，实际重复启动结果取决于已有文件。[pps_netmng.c:319](/home/tronlong/lyp/meari/nvr-wlan/pps_netmng.c:319)。

**启动可靠性问题**：

- 等待 PID 出现/消失及 AP 重启循环均无总超时；正常 main 流程中，卡在此处会阻止后续 TCP 服务启动。
- 把 ACS 状态视为可继续，不等于自动信道过程已经完成。
- 很多命令和配置 RPC 返回值被忽略；`pps_wifi_init` 即使内层返回失败也继续打印 ok 并返回 0。
- hostapd.conf/udhcpd.conf 模板及 udhcpc.script/ifplugd.action 不在本目录；不能保证固定行号对应当前固件模板。
- 同一文件中两处“去除双引号”的 C 字符串拼接实际得到 `sed -i 's///g' /etc/hostapd.conf`，没有包含待移除的双引号。该表达式已通过编译探测确认；本次未执行 sed 修改真实配置。
- `get_ap_status` 读取首行后直接返回，没有 fclose；隔离测试调用 4 次增加 4 个打开的文件描述符。它也没有用 len 限制 sscanf 的 `%s`，没有完整检查 fgets/sscanf 成功。

证据：[pps_netmng.c:41](/home/tronlong/lyp/meari/nvr-wlan/pps_netmng.c:41)、[pps_netmng.c:272](/home/tronlong/lyp/meari/nvr-wlan/pps_netmng.c:272)、[pps_netmng.c:323](/home/tronlong/lyp/meari/nvr-wlan/pps_netmng.c:323)、[pps_netmng.c:412](/home/tronlong/lyp/meari/nvr-wlan/pps_netmng.c:412)。

#### 5.2.2 PoE 内部网络

`pps_poe_init`：

1. 读取 eth0 默认网关；成功时先删除。
2. 创建 DHCP 租约文件并开启 eth1。
3. 使用 `is_same_subnet("eth0","172.168.0.1")` 按实际 eth0 地址/掩码判断；同网段选 162.168.0.1，否则 172.168.0.1。
4. 给 eth1 设置 /24 地址，添加经内部地址的默认路由，metric 199；若此前取得 eth0 网关，再以 metric 199 恢复 eth0 默认路由。
5. 把 udhcpd.conf 中的 p2p0 替换为 eth1。
6. 从 `nv_rpc_get_dhcp_ip_mac_pair` 逐通道取 IP/MAC，非空就向 DHCP 文件追加 static_lease。
7. 启动 udhcpd。

这条链只证明网口、路由和 DHCP 配置意图；没有检测 PoE 供电、交换机端口状态或完整 ONVIF 服务。static_lease 循环没有本地清理旧条目的操作，重复初始化可能累计注释/租约行；两条同 metric 默认路由的实际选择应以设备路由表与网络测试确认。[pps_netmng.c:422](/home/tronlong/lyp/meari/nvr-wlan/pps_netmng.c:422)、[netconfig.c:983](/home/tronlong/lyp/meari/nvr-wlan/netconfig.c:983)。

#### 5.2.3 MTK7628 路径与 eth0

`pps_7628_init` 读取 SSID/密码，重启 ra0 后发送 `AuthMode=WPA2PSK`、`EncrypType=AES`、`SSID=...`、`WPAPSK=...`，SSID 命令执行两次。它不在本地设置 DHCP、内部 IP 或 `lan_ip`；这些可能由产品环境配置，当前目录不能确认。返回 0 也不校验驱动最终设置结果。[pps_netmng.c:484](/home/tronlong/lyp/meari/nvr-wlan/pps_netmng.c:484)。

所有类型在 `nv_wlan_init` 中，只要 `/sys/class/net/eth0` 存在，就发送 `ifconfig eth0 up` 和 `ifplugd -nI -i eth0 -d 1 -r /etc/ifplugd.action &`。该产品脚本没有随本目录提供，有线 DHCP/热插拔行为不能仅由调用命令确认。[nvr_wlan.c:565](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan.c:565)。

### 5.3 信道选择——分支、算法与驱动 ABI

#### 5.3.1 选择路径

| 条件 | 源码选择 |
|---|---|
| 型号前 10 字符不匹配 `IView 204W` 且 wlan_better=1 | 固定配置 channel=14 |
| 非上述特殊型号，非增强，wlan_channel≠0 | 使用指定 wlan_channel；该分支无本地范围校验 |
| 非特殊型号，自动信道，检测到 `/sys/module/8192fu/` | 写 channel=0，交给外部 hostapd ACS |
| 非特殊型号，自动信道，检测到 customer ATBM606x USB/SDIO .ko | `wifi6_best_ch()` |
| 非特殊型号，自动信道，检测到 customer atbm603x .ko | `altm_cmd_best_scan()` |
| 自动信道但以上模块路径都不匹配 | chan 保持 -1，范围检查返回 NV_FAILURE |
| 型号前 10 字符匹配 `IView 204W` | 无论上述配置如何，执行 wpa_cli 扫描统计，选 1/6/11 |

驱动扫描路径前后发扫描开始/结束事件；Realtek ACS 和 IView 统计路径不经过同样的事件对。模块检测有时检测已加载的 `/sys/module`，有时检测 `/customer/*.ko` 文件存在，两者都不能单独证明驱动接口已准备好。[pps_netmng.c:283](/home/tronlong/lyp/meari/nvr-wlan/pps_netmng.c:283)。

#### 5.3.2 IView 的 AP 数量统计

`get_wifi_channel_quality` 通过 wpa_cli scan，等 10 秒后读取 scan_result 文件，按频率统计 14 个信道的 AP 数量。它不使用 RSSI、链路负载或信道忙比例加权：

- 信道 1 的评分 = 信道 1～5 的 AP 数之和。
- 信道 6 的评分 = 信道 2～9 的 AP 数之和。
- 信道 11 的评分 = 信道 7～13 的 AP 数之和。
- 从 1 开始，只在评分**严格更小**时改为 6 或 11；相等保留先选者。
- 统计到的 14 信道不参与上述三组评分；空结果会选择 1。
- 最后选择 11 时将 min 写为 range2，但之后没有再比较，这个错误赋值本身不会改变当前最终选出的信道。

该策略可称 AP 数量启发式选择，不宜称“最优射频信道算法”或“已验证避免干扰”。没有在本地启动 wpa_supplicant 的有效主链，扫描命令需要产品环境提前提供可用服务。[pps_netmng.c:105](/home/tronlong/lyp/meari/nvr-wlan/pps_netmng.c:105)、[pps_netmng.c:119](/home/tronlong/lyp/meari/nvr-wlan/pps_netmng.c:119)。

#### 5.3.3 ATBM603x：内嵌 iwpriv

`altm_cmd_best_scan` 在栈上建立 `BestChannelSelect_t`，设置 start=1、end=13、SpecialFlag=0，把结构地址作为 argv 参数传给 `iwpriv_proc1`。后者通过 `iw_get_priv_info` 查驱动的私有命令描述，执行对应 ioctl，再从结构中取 suggest_ch 返回。

实际扫描计算在驱动内；本地主要负责消息组织。该栈结构的 scan_result 未整体初始化。`iwpriv.c` 在 IW_PRIV_TYPE_CHAR 分支先对参数做 strlen，再对 `best_ch_scan` 改用结构体长度；由于该参数实际是二进制结构，前面的 strlen 存在不合适的字符串访问条件。根目录版本执行成功后统一按“最佳信道结果”解释缓冲，不能当成完整通用 iwpriv 工具复用。[pps_netmng.c:511](/home/tronlong/lyp/meari/nvr-wlan/pps_netmng.c:511)、[iwpriv.c:355](/home/tronlong/lyp/meari/nvr-wlan/iwpriv.c:355)、[iwpriv.c:464](/home/tronlong/lyp/meari/nvr-wlan/iwpriv.c:464)。

#### 5.3.4 ATBM606x：wifi6_best_ch 与 SIOCDEVPRIVATE

`atbm_ioctrl_init` 创建 PF_INET/SOCK_DGRAM 控制 socket；`ioctrl_wext_send_ack` 用 `ifreq.ifr_data` 携带 `altm_wext_msg`，调用 `SIOCDEVPRIVATE`。尽管对象名叫 `nl80211_global`，这条实现是 ioctl，未使用 libnl netlink。[atbm_ioctrl.c:25](/home/tronlong/lyp/meari/nvr-wlan/atbm_ioctrl.c:25)、[atbm_ioctrl.c:51](/home/tronlong/lyp/meari/nvr-wlan/atbm_ioctrl.c:51)。

`wifi6_best_ch` 的隔离复现与静态核对：

1. 构造 `scan_cmd={"wlan0","best_ch_scan","1","13"}`；处理函数却使用 argv[0]/argv[1] 转换数值，实际发给替身驱动的 start/end 均为 **0/0**。SDIO 的首项改成 p2p0，转换仍为 0。
2. 驱动返回 support_chan_num=18 时，函数能打印 special 结构的 suggest_ch，但最终返回普通 14 信道结构的 suggest_ch。隔离注入“special 建议 11”时，**打印 11、返回 0**。
3. **静态确认**：ioctl 的返回与 msg.value 状态没有完整传递到最后的选择结果；调用失败也继续复制 externData。上层只检查返回信道在 1～14。
4. **静态确认**：`atbm_ioctrl_dinit` 只关闭 fd>0；未重置 fd，fd=0 情况未释放。
5. **接口边界**：消息结构依赖双方对 int 宽度、字段偏移、填充与返回布局的共同约定，没有接口版本协商；未获得驱动定义，不能据此断言实际 ABI 已不匹配。

来源：[wifi6_best_ch.c:108](/home/tronlong/lyp/meari/nvr-wlan/wifi6_best_ch.c:108)、[wifi6_best_ch.c:163](/home/tronlong/lyp/meari/nvr-wlan/wifi6_best_ch.c:163)、[atbm_ioctrl.c:19](/home/tronlong/lyp/meari/nvr-wlan/atbm_ioctrl.c:19)。

文件名中的 WiFi6 不能证明完整 802.11ax 功能或性能；随附 hostapd .config 的 `CONFIG_IEEE80211AX=y` 本身处于注释状态，实际产品可能使用其他二进制/驱动组合，需另外核验。

### 5.4 nvr_wlan——hostname 列表与父子拓扑

**文件**：`nvr_wlan.c`、`inc/nvr_wlan.h`。

#### 5.4.1 数据结构与规模

~~~c
typedef struct wlan_topological {
    char hostname[64];
    int chan;
    int quality;
    struct wlan_topological *p_node;
} wlan_topological_t;
~~~

这是按当前头文件的字段展开，hostname 容量来自本地 `HOSTNAME_MAX_SIZE=64`。数组大小 `NV_WLAN_SIZE=NV_CHAN_MAX`，NV_CHAN_MAX 来自缺失的 `nv_base.h`；不能借用参考 mpc-base 报告的 4 得出本项目也为四通道。

`WIFI_MAX_LOAD_CNT=WIFI_MAX_CNT`、`WIFI_MAX_LOAD_PATH_LEN=WIFI_MAX_PATH_LEN+1` 也由外部 SDK 决定。路径长度包含当前 IPC；直接连接 NVR/无父节点以 NULL 表示。load_cnt 统计直接孩子，不是整个子树设备总数。[inc/nvr_wlan.h:21](/home/tronlong/lyp/meari/nvr-wlan/inc/nvr_wlan.h:21)、[nvr_wlan.c:38](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan.c:38)。

#### 5.4.2 列表更新与质量

`nv_wlan_update_all_hostname` 持有全局 mutex，经 RPC 逐通道读取 hostname 与 frame_loss 质量；保留仍存在的 hostname，删除消失节点，加入新 hostname，最后将非空槽位向前压紧。

`nv_wlan_get_all_hostname` 按每项 `hostname[64]+quality[1]` 写出，只写到首个空槽；接口没有输出容量参数，当前协议调用者按数组上限分配。`nv_wlan_clear_quality` 把 quality 设置为 **3**，并非清零；当前顶层调用链没有调用它。质量从 RPC frame_loss 获得，也不是 `netconfig` 中的无线信号质量。[nvr_wlan.c:220](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan.c:220)、[nvr_wlan.c:320](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan.c:320)、[nvr_wlan.c:372](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan.c:372)。

#### 5.4.3 父节点与 superidx

`nv_wlan_set_parent_hostname` 设置当前节点父指针，然后清除原来以该节点为父的直接孩子关系。它不重建完整子树，不是无损拓扑重排。主服务 superidx 映射为：

| p_hostname 条件 | 提交的 superidx |
|---|---:|
| NULL | -1 |
| 字符串包含 NVR | 0 |
| 查找到另一 IPC | 父节点 chan+1 |

函数忽略 `nv_wlan_set_chan_superidx` 的返回，最终本地返回 0。NULL 同时用于“无已知父节点”和“直接 NVR”的内部指针表示，真正区分还需 superidx/连接状态。[nvr_wlan.c:183](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan.c:183)、[nvr_wlan.c:240](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan.c:240)。

#### 5.4.4 已核验的拓扑缺陷

- **压紧数组不修复父指针（隔离复现）**：初始 A、B、C 占 0/1/2 槽，B→A、C→B；删除 A 后把 B 搬到 0、C 搬到 1，但 C 的 p_node 仍指向旧 1 槽，结果 **C→C 自环**。如果随后检查其路径长度，循环没有终止条件，也持有全局 mutex，可能阻塞拓扑相关调用。
- **同 hostname 迁移通道不刷新 chan（隔离复现）**：匹配已有节点时只更新 quality。测试把 C 从外部 channel2 移到 channel5，缓存仍为 2，后续通道级控制可能投向旧通道。
- **无完整环检测（静态）**：设置父节点会清除该节点的直接孩子关系，这一点会打断部分潜在循环；但路径遍历没有 visited 集合或次数上限，不能处理数组压紧已经造成的环。这里未单独证明只调用父节点设置接口就能形成长环。
- **拓扑打印只沿第一个孩子继续（静态）**：每层找到一个子节点就 break；同一父节点有多个孩子时日志不能完整展示分支，不能据日志图推断真实完整树。
- **IP 去重为前缀比较（隔离复现）**：`strncmp(ip_addr,ip,strlen(ip))` 会把已有 `172.168.0.10` 与输入 `172.168.0.1` 当作匹配，影响设备存在性及解绑判断。

来源：[nvr_wlan.c:92](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan.c:92)、[nvr_wlan.c:344](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan.c:344)、[nvr_wlan.c:396](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan.c:396)、[nvr_wlan.c:424](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan.c:424)。测试采用 8 槽数组替身，只验证上述与具体产品上限无关的机制。

#### 5.4.5 更新触发的边界

`nv_wlan_list_is_update` 先比较共享通道连接状态与 static last_status，只有状态变化才调用刷新，并且只有刷新返回 1（成员增删）才返回 true。单纯 hostname 更改但连接状态不变、quality 变化或通道迁移，不能靠此路径保证即时广播。初始化全 0 时也不触发变化，但 main 已进行一次主动刷新。

`nv_wlan_ad_hoc_is_update` 比较外部 network 开关与 static last_status，两者都由两个监听线程调用，static 数组/标志无共同锁。属于可见的并发访问问题，未做 ThreadSanitizer 或整机时序验证。[nvr_wlan.c:464](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan.c:464)、[nvr_wlan.c:495](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan.c:495)。

`nv_wlan_get_udhcpc_is_restart` 维护一次性 hostname 记录表，但协议中对应重启调用已经注释；不能描述为已启用的 DHCP 自动重启补救机制。

### 5.5 双 TCP 服务——连接、线程与发送队列

**文件**：`nvr_wlan_tcp_server.c/.h`。

#### 5.5.1 监听与连接模型

| 参数 | 真实代码 |
|---|---|
| 普通/CPU 控制端口 | TCP 8045 |
| MCU 控制端口 | TCP 8046 |
| 监听地址 | IPv4 `INADDR_ANY` |
| listen backlog | 8；不是总客户端上限 |
| 服务循环 | select，空闲等待 200 ms |
| 客户端循环 | 非阻塞 socket + select，等待 100 ms |
| 单连接收/发缓冲 | 各 10×1024 字节 |
| 超时判定 | 8045 为 6 秒；其他端口为 300+10=310 秒 |
| 时钟 | `CLOCK_MONOTONIC`，仅取秒 |
| 线程生命周期 | 客户端/服务线程自行 detach，无统一 stop/join |

MCU 注释“每90秒发送心跳”并不改变实际 310 秒超时。所有校验通过并进入命令解析的帧都会刷新 heart_time，包括未知 cmd；不是只有 HERAT 命令才续期。[nvr_wlan_tcp_server.h:26](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_tcp_server.h:26)、[nvr_wlan_tcp_server.c:191](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_tcp_server.c:191)、[nvr_wlan_common.c:5](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_common.c:5)、[nvr_wlan_protocol.c:167](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_protocol.c:167)。

每次 accept 会分配约 20 KiB 缓冲加状态结构和独立线程，没有应用级连接计数上限。fd 直接传入 FD_SET，没有验证其小于 FD_SETSIZE；高 fd 情况存在越界条件，但未施加连接压力进行复现。

#### 5.5.2 空闲时的请求调度

select 返回 0 且 reg=1 时：

- MCU 类型只检查 is_update_wakeup。
- 其他类型按以下 else-if 优先级生成一次请求：reset → 首次连接查询（每 2 秒）→ info 查询（每 2 秒）→ 列表更新 → network 开关更新。
- 标志在打包之前清除；发送缓冲空间不足时不会自动恢复标志。
- 接收数据/发送就绪频繁时不经过 select==0 的分支，异步标志的执行时间不保证。
- reset 设置 send_reset_dev=1 后不等待明确确认；重启/唤醒应答也没有带序列号的完成状态机。

返回 0 的 `nvr_wlan_set_wake_up` 或 `nvr_wlan_dev_reset` 仅表示找到了客户端并置位，不等于远端动作成功。[nvr_wlan_tcp_server.c:234](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_tcp_server.c:234)、[nvr_wlan_tcp_server.c:458](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_tcp_server.c:458)。

#### 5.5.3 发送与错误处理问题（静态）

`send` 后只执行 `send_len-=ret`，没有把未发送数据移到缓冲头。例：队列为 ABCDEF、首次仅发 AB，send_len 变 4，下一次仍从原缓冲起点发送 ABCD，正确的 CDEF 被破坏。属于部分发送必然触发的逻辑问题；本次没有通过真实 TCP 压力诱发短写。

对非阻塞 socket，send<0、recv<=0 和 select<0 均直接退出，没有按 EAGAIN/EWOULDBLOCK/EINTR 分别重试。efds 虽清零并传入 select，却没有 FD_SET，所以该异常检测分支没有覆盖预期的异常 fd。bind/listen 失败分支未关闭已创建服务 socket；客户端 pthread_create 使用 ret<0 判断，而本文件其他线程创建检查 ret!=0，错误处理条件不一致。[nvr_wlan_tcp_server.c:289](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_tcp_server.c:289)、[nvr_wlan_tcp_server.c:383](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_tcp_server.c:383)、[nvr_wlan_tcp_server.c:443](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_tcp_server.c:443)。

### 5.6 私有 WLAN 协议——真实线格式和命令语义

**文件**：`nvr_wlan_protocol.c`、`inc/nvr_wlan_protocol.h`。

#### 5.6.1 帧格式

~~~text
offset  size  含义
0       4     56 56 50 99
4       2     cmd，大端
6       2     payload 长度，大端
8       N     payload
8+N     2     累加和，大端

总长 = 10 + N
累加和 = (head + cmd + len + payload 的各字节总和) mod 65536
~~~

这是普通字节累加校验，不是 CRC、MAC、签名或加密。源码结构头为两个 uint16_t 加 4 字节数组；本次主机 sizeof=8 与硬编码线字段吻合。解析 cmd 用手工交换字节，依赖小端宿主解释；不能据此保证任意大端平台兼容。[nvr_wlan_protocol.c:14](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_protocol.c:14)、[nvr_wlan_protocol.c:85](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_protocol.c:85)、[nvr_wlan_protocol.c:377](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_protocol.c:377)。

#### 5.6.2 全部命令与有效处理

| 值 | 命令 | 当前本地处理 |
|---:|---|---|
| 0 | APPLY_CONNECT | 至少 128 字节：hostname[64]+parent[64]；同意/拒绝返回 payload[1]，0=同意 |
| 1 | GET_WLAN_LIST | 刷新列表，返回 count[1]+每项 hostname[64]/quality[1] |
| 2 | PULL_WLAN_LIST_UPDATE | NVR 发空负载更新通知；收到应答只 break |
| 3 | PULL_WLAN_ON_OFF | NVR 发 status[1]；收到应答只 break |
| 4 | PULL_IS_FIRST_CONNECT | NVR 发空查询；收到至少65字节时读取 is_qrcode[1]+hostname[64]，调用添加 IPC 的 RPC |
| 5 | PULL_GET_INFO | NVR 发空查询；收到至少128字节时读取 version[64]+model[64]，上报在线信息 |
| 6 | REG | 至少257字节；按 token 比较判定接受，记录连接信息，返回145字节应答 |
| 7 | WAKEUP | NVR 发空负载；应答仅记录日志 |
| 8 | RESET | NVR 发 type[1]，当前定义解绑复位 type=0；switch 没有独立 RESET 应答 case |
| 9 | RESTART | 有打包接口；应答只 break，当前 DHCP 重启触发代码已注释 |
| 65535 | HERAT | 心跳请求返回 payload[1]=0；保留源码拼写 |

收到 break 或未知命令后通常返回 -1；外层仍消费该包，并不因此主动关闭连接。不能把 -1 一概解释成鉴权失败或网络故障。[inc/nvr_wlan_protocol.h:23](/home/tronlong/lyp/meari/nvr-wlan/inc/nvr_wlan_protocol.h:23)、[nvr_wlan_protocol.c:171](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_protocol.c:171)。

头文件“列表每项64字节”的注释漏了 quality，实际响应每项是65字节；REG 的注释也未完整描述后续 protocol/port/comm_mode 字段。协议对接应依据实际读写偏移，不能只使用这些注释。

#### 5.6.3 REG 字段布局

以下偏移从 payload 起点算：

| 偏移 | 长度 | 字段与本地行为 |
|---:|---:|---|
| 0 | 64 | hostname；直接 memmove 到 client.hostname，未强制 NUL 终止 |
| 64 | 128 | token 区域；比较只使用 NV_COMM_LEN，后文64/128为线字段，不是该外部宏值的推断 |
| 192 | 1 | dev_type：实际枚举 0普通、1 MCU、2 CPU |
| 193 | 16 | tp_pid；复制16字节到初始清零的64字节字段 |
| 209 | 1 | protocol_type：0 PRTP、1 PRTP2 |
| 210 | 2 | stream_port，大端；为0时改11224 |
| 212 | 1 | comm_mode |
| 213～256 | 44 | 本地未解析的保留区域 |

解析最低长度257。REG 应答 payload=145：status[1]、lan_ip[16]，其余128字节保持0。对于 type=MCU，接收注册后先用 `nv_wlan_add_ipc_2_ipcham(hostname,0,0,ip,0,NULL,0)` 请求更新 IP，然后绑定对应 CPU，并按 CPU 是否存在上报 WAKEUP 或 SLEEP。这次特殊调用的参数不要误写成“以 MCU 类型直接添加新媒体通道”。[nvr_wlan_protocol.c:257](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_protocol.c:257)、[nvr_wlan_protocol.c:307](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_protocol.c:307)。

头文件 REG 注释写 reg_type=1:cpu/2:mcu，而实际枚举为1:MCU/2:CPU；本报告以被执行的枚举和分支为准。

#### 5.6.4 APPLY_CONNECT 与首次连接的实际语义

APPLY 先尝试清空旧父节点；parent 字符串包含 `_` 就被视为 NVR，设置 superidx=0、reg=1 并返回同意，不做 token 校验，也未检查设置函数全部返回值。否则检查候选父节点的路径长度/直接孩子数量，再建立父子关系。

PULL_IS_FIRST_CONNECT 应答中的第一个字节在代码里叫 is_qrcode。收到后先关闭 get_first_connect 标志，再请求添加/更新 IPC、刷新列表；CPU 类型尝试绑定 MCU 并上报 WAKEUP。非 MCU 且非扫码连接时，还检查本地是否已存在，不存在则安排解绑复位。它不是“判断首次连接成功之后才允许所有命令”的统一认证流程。[nvr_wlan_protocol.c:172](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_protocol.c:172)、[nvr_wlan_protocol.c:219](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_protocol.c:219)。

#### 5.6.5 接收缓存与边界问题

正常路径下，完整帧被逐个解析；不足一帧时保留 recv_len 等下次数据，打包函数对输出容量做检查。本次确认：5字节分片保留5、补齐心跳后应答11且剩余0；两个连续心跳应答22；错误校验和不生成应答。

同时确认下列缺陷：

- **噪声前缀长度没有从 dat_len 扣除**：查到 start 后只减 packet_len，不减 start-s 的跳过量；搬移起点也据错误的 dat_len 计算。1个噪声字节+完整心跳后竟保留1字节；10 KiB 输入中4字节噪声+完整帧的隔离测试，ASan 检出 find_start_code 的越界读。
- **无 NUL 的固定字段按字符串读取**：APPLY、首次连接、GET_INFO 等只检查总 payload 下限，却直接 strlen、strstr、`%s` 格式化；REG 的 hostname 则固定复制64字节。隔离测试用全为A且无终止符的128字节 APPLY payload，ASan 在 snprintf 处检出越界读。真实 TCP 缓冲可能把读取落在未接收/旧数据区域，不能依赖初始化清零解决持续接收的边界问题。
- **超大帧/无头垃圾没有可恢复的丢弃策略**：长度字段可声称超过10 KiB缓冲容量；find_one_packet 持续等完整帧，recv缓冲满后下一次 recv 的请求长度为0并进入断线。无头数据也一直积累。这个路径没有明确报告协议长度错误。
- **注册不作为解析前置条件**：主解析 switch 前没有 reg 检查，未注册的 FIRST_CONNECT 应答仍能进入添加 RPC；本次已捕获一次该外部调用。实际添加是否最终被主服务拒绝仍需主服务实现。
- **命令 ACK 无请求关联**：无 sequence ID、没有检查是否正在等待某种应答，能够接受重复或主动提交的应答类帧。
- **校验通过即续心跳**：异常语义/未知命令也能续期，未注册会话在有有效帧时可持续存活。

来源：[nvr_wlan_protocol.c:51](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_protocol.c:51)、[nvr_wlan_protocol.c:118](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_protocol.c:118)、[nvr_wlan_protocol.c:158](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_protocol.c:158)。隔离检测证明本地函数的越界条件，不等于已证明远程代码执行或具体产品可利用。

### 5.7 低功耗 IPC——CPU/MCU 关联与状态上报

`nvr_client_child_bind` 根据服务对象的 lpc_mcu 标志，在**另一端口的链表**中反向查找相同 hostname；匹配 dev_type 后把 MCU 对象指针保存在 CPU 的 `lp_ipc_mcu_client` 中。

| 条件 | 本地上报的状态 |
|---|---|
| CPU 的首次连接应答处理完成 | LP_IPC_WAKEUP=2 |
| MCU 注册，找到 CPU | LP_IPC_WAKEUP=2 |
| MCU 注册，没有 CPU | LP_IPC_SLEEP=3 |
| CPU 断线但 lp_ipc_mcu_client 非空 | LP_IPC_SLEEP=3 |
| MCU 断线且没有对应 CPU | LP_IPC_OFFLINE=1 |

状态枚举强制转换后通过外部 `nv_rpc_send_batterycam_msg` 发送；缺失外部 NV_BATTERYCAM_STATUS 定义，不能完全核验双方数值 ABI 是否一致。[inc/nvr_wlan.h:37](/home/tronlong/lyp/meari/nvr-wlan/inc/nvr_wlan.h:37)、[nvr_wlan_tcp_server.c:64](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_tcp_server.c:64)、[nvr_wlan_tcp_server.c:318](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_tcp_server.c:318)。

**已复现的解绑链表错误**：`nvr_client_child_unbind` 在 MCU 离线时选择并锁住 CPU 的 g_server，但实际遍历 `client->server->client`（MCU 自己的链表）。客户端已经从该链表移除时，无法清除 CPU 的 MCU 指针。隔离结果为 bind_ret=0、unbind_ret=-1、CPU_pointer_still_MCU=1。离线对象随后 free，CPU 中保留悬空关联，可导致以旧非空指针错误判断 MCU 仍在线；当前观察到的断线判断只检查是否非空，未证实这处必然解引用导致 UAF。[nvr_wlan_tcp_server.c:122](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_tcp_server.c:122)。

此外，关联指针的读取/写入不由同一 mutex 全程保护，`nvr_client_get_parent_client` 在解锁后返回链表对象裸指针，没有引用计数或存活保障。不过当前调用者只判断返回值是否为 NULL，不能直接据其返回裸指针断言当前发生了 UAF 解引用。新旧同 hostname/IP 的替换连接通过 `nvr_client_query_new_client` 减少旧连接离线操作，但不能替代跨端口对象生命周期管理。

这是一套“控制连接存在性 → 业务状态”的推导，不是对 CPU 实际休眠状态、MCU 电量或无线唤醒成功的硬件测量。

### 5.8 pushmsg——外部事件消费与控制联动

`nv_wlan_msg_push_create` 以外部设备信息中的容量/单项大小构造 `NvNetBuf("app-msg-push",rb_size,100*1000,true,it_size)`。其内部 mmap、读索引、超时参数单位及跨进程同步实现不在本目录；不能直接移植参考报告中另一个项目的 NvNetBuf 行为作为本项目已核验事实。[nvr_wlan_pushmsg.cc:20](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_pushmsg.cc:20)。

消息线程使用 `ReadIdx=UINT32_MAX`，读取 NetFrameIndex 的 value/channel/dataLen；只有 addr非空且len>0才处理以下事件：

| msgtype | 当前动作 |
|---|---|
| E_EVENT_CMD_BATTERY_CAM_WAKEUP | 通过 chan 找 hostname，置对应 MCU is_update_wakeup |
| E_EVENT_MSG_BATTERY_CAM_UNBIND | 置普通/CPU 客户端 is_reset_dev，再刷新 hostname 列表 |
| E_EVENT_MSG_IPC_DEL | 同上 |
| 其他 | 无业务处理 |

没有消费到数据时休眠10ms。有 payload 但本分支不解析其内容；zero-length 控制事件被忽略。`nv_wlan_msg_push_create` 忽略内部创建结果并返回 NV_SUCCESS，线程也不检查；失败后可能只不断轮询 NULL，外部无法由当前返回值区分。`nv_wlan_msg_push_destroy` 有函数定义，但当前常驻主链没有调用，头文件也没有导出它。[nvr_wlan_pushmsg.cc:42](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_pushmsg.cc:42)、[nvr_wlan_rpc.cc:342](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_rpc.cc:342)。

### 5.9 系统命令与旧 AP 代码的实现边界

#### 5.9.1 pps_cmd_cli

本文件自行 `#define PPS_SYSTEM_CMD`，默认走真实 Unix Socket 客户端，而不是只打印的 #else 桩：

~~~text
pps_system_cmd(command)
    → AF_UNIX / SOCK_STREAM
    → bind 本地 "/var/" + pthread_self 指针格式
    → chmod 本地 socket 0700
    → connect /var/systemCmd.socket
    → 发 native int len + command 字节（长度不含末尾 NUL）
    → 单次 recv 4 字节返回值
    → close
~~~

发送缓冲为1024字节，拒绝 header.len≥1024。writen 使用 `net_base_operate.c` 的完整发送循环；接收返回 int 却只有一次 recv，没有 readn 或完整4字节累积，短读会失败；没有设置该 recv 的响应超时。Unix 客户端 bind 路径在成功/失败退出没有 unlink 清理，下一次相同线程调用开头会先 unlink。[pps_cmd_cli.c:30](/home/tronlong/lyp/meari/nvr-wlan/pps_cmd_cli.c:30)、[pps_cmd_cli.c:106](/home/tronlong/lyp/meari/nvr-wlan/pps_cmd_cli.c:106)。

服务器端不在本仓库，不能确认它使用何种 shell、权限身份、并行/超时管理或返回值编码。但本地明确发送含重定向、后台符号、管道、sed 和反引号替换的文本，依赖外部服务理解这些语法。命令中的 SSID、密码、IP/MAC 没有在这里完整转义；输入含引号或 shell/ sed 元字符时至少会破坏预期命令，是否能扩大为命令注入取决于外部执行方式和主服务输入约束。

#### 5.9.2 pps_device_ap 与残留头文件

`pps_device_ap.c` 仍定义全局回调指针、枚举和 context 结构，但 AP 事件线程、wpa_ctrl attach/request、初始化/退出实现主体全部注释。头文件仍声明 `pps_wpa_cli_ap_mode_reinit`，`pps_netmng.c` 的调用也是注释；当前顶层链路不使用该旧模块监听 AP 站点事件。

`inc/pps_wpa_cli.h` 中大量连接/重连/状态接口没有随顶层提供实现；`inc/pps_device_netmng.h`、`hwl_config.h`、`hwl_net.h` 为注释保留文件，不能算作有效网络管理适配层。`pps_wpa_cli.h` 把 C++条件宏拼为 `__cpluplus`，正常 C++ 编译器不会据此进入 extern "C" 区块。[pps_device_ap.c:138](/home/tronlong/lyp/meari/nvr-wlan/pps_device_ap.c:138)、[inc/pps_wpa_cli.h:19](/home/tronlong/lyp/meari/nvr-wlan/inc/pps_wpa_cli.h:19)。

---

<a id="chapter-06"></a>

## 六、工具库详解

本仓库没有参考项目的 `util/` 目录；对应内容由顶层 `netconfig.c`、`net_base_operate.c`、`iwlib.c` 和 `tool/` 提供。以下区分主流程实际调用、编入但未在主流程调用，以及独立第三方程序，避免把库的全部接口当作产品已启用功能。

### 6.1 netconfig——网卡、地址、路由、DNS 与输入校验

文件共 3,367 行，能力覆盖表如下。接口“存在”只指当前文件有实现。

| 功能组 | 代表入口 | 本地实现方式 | 主进程关联 |
|---|---|---|---|
| 物理链路及接口选择 | `pps_set_net_param`、`pps_get_net_connect_status`、`interface_is_used`、`pps_get_current_interface_used` | ethtool ioctl；从路由表判断接口是否有 UP 路由 | 主启动不调用这些接口；不能据此认定自动切换上网接口 |
| IPv4 参数读取 | `pps_get_gateway/ipaddr/mac/netmask/broadcast_addr` | `/proc/net/route` 与 SIOCGIF* ioctl | AP 读取 eth0 IP；PoE 读取 eth0 网关及子网 |
| IPv4 参数配置 | `pps_set_ipaddr/mac/netmask/broadcast_addr`、`if_up/down`、`set_mtu` | SIOCSIF* ioctl，MAC 设置先 down 后 up | AP/PoE/7628 主流程主要发送 ifconfig/route 文本，未统一通过这些 setter |
| 路由管理 | `pps_set_active_gateway/active_route`、`pps_set_gateway`、`pps_del_gateway`、`pps_del_all_gateway_by_iface` | rtentry + SIOCADDRT/SIOCDELRT | PoE 主流程另用外部 `route` 命令；这些 API 的问题不能全部算作 PoE 必现故障 |
| IPv6 参数 | `pps_get_ipv6addr_num/ipv6_gateway_num/valid_ipv6`、`pps_set_v6gateway`、`pps_del_v6_gateway` | `/proc/net/if_inet6`、`/proc/net/ipv6_route`、IPv6 路由 ioctl | 没有追踪到它们建立 8045/8046 IPv6 监听 |
| DNS | `pps_get_dns/set_dns`、`dns_res_init`、`check_dns_exist` | `/etc/resolv.conf`；wlan0/eth0 临时 DNS 文件；`res_init` | 作为工具保留，当前 AP/PoE 初始化没有调用这些接口 |
| 流量与无线参数 | `pps_get_interface_receive_transmit_state/total_flow`、`pps_get_waln_signal_level`、`get_waln_essid` | `/proc/net/dev`，WEXT 与 `/proc/net/wireless` | 主拓扑的 quality 来源是 frame_loss RPC，与这些无线质量接口不同 |
| 地址与名称验证 | `check_str_ipv4/ipv6`、`is_ipaddr_lawful`、`is_domain_valid`、`is_hostname_valid`、掩码及多播判断 | 自定义规则与 libc inet_pton | WLAN 协议解析没有统一调用这些校验器 |
| 辅助连接及地址转换 | `pps_connect_with_timeout`、`pps_get_peer_addr`、`pps_ipaddr_convert` | 非阻塞 connect/select；getpeername；映射 IPv4/IPv6 | 双 TCP 服务另有自己的 accept/recv/send 实现 |

**主链可确认的正确机制**：`is_same_subnet` 读取接口 IP 与掩码并比较 `(ip & mask)==(gateway & mask)`，返回 1同网段、0不同、-1错误；PoE 调用者只在值为1时更换内部网段。另一个工具 `is_in_same_subnet` 比较两个地址的各自网络号，补充替身测试中 `10.0.0.2/24` 与 `10.0.0.88/24` 返回1。这里没有把 AP 的字符串前缀检测误当成此工具的行为。[netconfig.c:983](/home/tronlong/lyp/meari/nvr-wlan/netconfig.c:983)、[netconfig.c:1783](/home/tronlong/lyp/meari/nvr-wlan/netconfig.c:1783)。

**已确认的问题与适用范围**：

| 问题 | 代码与触发条件 | 证据等级及范围 |
|---|---|---|
| DNS 输出容量参数没有使用 | `pps_get_dns` 用无宽度 `%s` 写入 name[50] 与调用者缓冲；即使传 len=4 仍读取完整 DNS 文本 | 补充测试输入 `nameserver 1.2.3.4`、输出 malloc(4)，ASan 检出越界写；当前主流程未调用 |
| 空 DNS 文件仍判定存在 DNS | `check_dns_exist` 忽略 getter 返回值，只判断主 DNS 是否不以 `0.0.0.0` 或 `::` 开头；空字符串也满足 | 隔离输入空文件返回1；不是域名解析成功验证 |
| IPv4 校验接受异常文本 | `is_ipaddr_lawful` 未检查 sscanf 转换项数，缺字段沿用初始0 | 隔离输入 `...` 返回TRUE；不能把该函数看成严格 IPv4 语法校验 |
| IPv6 /128 被拒绝 | `pps_convert_ipv6_prefix_netmask` 在 prefix/8>=16 时返回ERROR | 隔离 prefix=128 返回-1；这是工具实现边界，未证明影响当前IPv4主链 |
| 路由设备指针写法错误 | `del_gateway_by_iface` 执行 `strcpy((char *)&rt.rt_dev, iface)`，把名称写入指针成员自身而非给指针赋值 | 替身 ioctl 截获 rt_dev 成员内实际为 `eth1` 字节；真实内核如何返回需设备验证，不在本次修改真实路由 |
| 空指针检查晚于使用 | `pps_get_peer_addr` 先 `bzero(ip,...)`，再检查 ip==NULL | 静态确认 NULL 参数路径有非法写；未动态触发 |
| 接口名称缺少边界检查 | 多处 `strcpy(ifr.ifr_name,p_interface_name)`，没有检查名称小于IFNAMSIZ | 静态越界条件；主链当前传固定短名 eth0/eth1，不等于主链必然越界 |
| MAC 初始化及打印不严谨 | `pps_get_mac` 与 `get_mac_str` 用 sizeof(指针)清零 ifreq，随后用 char 字节配 `%02x` | 静态确认；有符号 char 与高位MAC字节的组合可能输出扩展值，未对产品char符号配置做结论 |
| 流量文本处理缺少边界 | 只读取 `/proc/net/dev` 前1023字节；换行查找后错误地再次检查 pDev_s；memcpy 至stat_info[256]无容量检查 | 文件截断或较长统计行下存在NULL差值/越界条件；接口匹配使用strstr也可能匹配名称子串，未动态复现 |
| DNS 配置失败清理不完整 | `pps_set_dns` 在 strcmp前不检查iface；未检查多个fopen/malloc；fread 200字节后以 `%s` 输出，不保证NUL；早退未完整关闭文件/释放两个缓冲 | 静态确认；没有写入本机/etc文件 |
| IPv6工具参数契约不清 | `pps_set_v6gateway` 的 gateway 虽为char*，实际memcpy 16字节到二进制地址；IPv6无冒号转换固定读取32字符 | 必须传满足容量/格式的输入；不能按名字传普通IPv6字符串并期待自动转换 |

位置分别见[netconfig.c:768](/home/tronlong/lyp/meari/nvr-wlan/netconfig.c:768)、[netconfig.c:1945](/home/tronlong/lyp/meari/nvr-wlan/netconfig.c:1945)、[netconfig.c:1990](/home/tronlong/lyp/meari/nvr-wlan/netconfig.c:1990)、[netconfig.c:2064](/home/tronlong/lyp/meari/nvr-wlan/netconfig.c:2064)、[netconfig.c:2726](/home/tronlong/lyp/meari/nvr-wlan/netconfig.c:2726)、[netconfig.c:3008](/home/tronlong/lyp/meari/nvr-wlan/netconfig.c:3008)、[netconfig.c:3092](/home/tronlong/lyp/meari/nvr-wlan/netconfig.c:3092)、[netconfig.c:3181](/home/tronlong/lyp/meari/nvr-wlan/netconfig.c:3181)。

额外返回值边界：`pps_get_default_iface` 文件可读时，即使未找到默认路由也返回0而不写输出；`pps_get_current_interface_used` 找不到任何候选时仍保留0，与“wlan0有路由”相同；`pps_get_net_connect_status` 忽略ethtool ioctl失败。这些接口存在，不能据此认定返回值能准确区分所有失败状态。[netconfig.c:388](/home/tronlong/lyp/meari/nvr-wlan/netconfig.c:388)、[netconfig.c:415](/home/tronlong/lyp/meari/nvr-wlan/netconfig.c:415)、[netconfig.c:3307](/home/tronlong/lyp/meari/nvr-wlan/netconfig.c:3307)。

### 6.2 net_base_operate——Socket 与自定义地址转换

| 接口组 | 实现及使用约束 |
|---|---|
| `inet_pton_with_af/inet_ntop_with_af` | 自己解析／格式化IPv4与IPv6，返回约定与libc不同；本地OK=0、ERROR=-1 |
| `SOCKET_ADDR_T`、`make_addr_bystring` | sockaddr_in／sockaddr_in6共用union；IPv6路径依赖自定义转换器 |
| `socket_create/bind/listen/accept_with_timeout/reuse` | 包装系统Socket API；accept超时单位ms，TIME_OUT_INFINITE=0xffffffff |
| `ip_connect_withtimeout/socket_connect_withtimeout` | 先非阻塞connect，再select与SO_ERROR；前者解析域名并尝试候选，后者使用传入地址 |
| `socket_sendto/recvfrom` | 根据协议族选择地址大小；不负责WLAN帧校验与会话认证 |
| `set_socket_linger/keeplive/mcast_ttl` | Socket选项工具；不能替代本服务的6／310秒应用心跳 |
| `readn/writen`、`send_data_insmall_packet` | 累计读写；`pps_system_cmd` 实际使用writen，双TCP发送队列未使用它 |
| `socketaddr_tostr/is_stringip_ipv6` | IPv6输出使用静态共享缓冲；IPv6识别函数仅统计冒号，不是严格语法校验 |

**原隔离测试重新确认**：IPv4输出 `1.2.3.4` 的可见长度7，传入容量7时，`inet_ntop4_xx` 使用 `strlen(tmp)>size` 而不是 `>=`，随后strcpy还写入NUL，ASan检出越界写。IPv4输入输出空间只有3字节时，内部返回-1，外层把非零当成功，最终返回OK=0且输出不变。[net_base_operate.c:103](/home/tronlong/lyp/meari/nvr-wlan/net_base_operate.c:103)、[net_base_operate.c:245](/home/tronlong/lyp/meari/nvr-wlan/net_base_operate.c:245)、[net_base_operate.c:418](/home/tronlong/lyp/meari/nvr-wlan/net_base_operate.c:418)。

**补充确认**：非法IPv6文本 `abcd`，libc inet_pton返回0表示失败，而自定义 `inet_pton_with_af` 返回0表示本地成功。内部IPv6解析未强制验证最终字节数已填满；这会传播到使用该解析器的地址构造接口，不能据“兼容IPv6”声明认为所有输入都被严格验证。[net_base_operate.c:309](/home/tronlong/lyp/meari/nvr-wlan/net_base_operate.c:309)。

`readn/writen` 每次select等待10秒，正向短读／短写通过移动指针继续处理，这一机制与双TCP队列短写错误不同。但该10秒不是总调用期限：每次有进展就重新计时；select的EINTR没有重试，writen的send=0也没有前进。`pps_cmd_cli.c` 的外部声明writen第三参数为size_t，定义为int；当前传入小于1024，未复现错误，但跨文件声明应保持一致。[net_base_operate.c:1091](/home/tronlong/lyp/meari/nvr-wlan/net_base_operate.c:1091)、[net_base_operate.c:1151](/home/tronlong/lyp/meari/nvr-wlan/net_base_operate.c:1151)。

连接工具还存在契约差异：`ip_connect_withtimeout` 的有限超时路径成功后不恢复阻塞模式；`socket_connect_withtimeout` 则强制改回阻塞模式，未保留调用者原属性。域名解析不纳入select期限，每个候选地址有自己的等待期限；因此不能把参数解释为覆盖DNS及全部候选的端到端上限。SO_ERROR读取结果未完整检查，输入fd也缺少FD_SETSIZE校验。当前8045/8046服务没有通过这些主动连接函数建立IPC连接。[net_base_operate.c:787](/home/tronlong/lyp/meari/nvr-wlan/net_base_operate.c:787)、[net_base_operate.c:893](/home/tronlong/lyp/meari/nvr-wlan/net_base_operate.c:893)。

### 6.3 iwlib、WEXT 与链表头文件

`iwlib.c` 共3,214行，`inc/iwlib.h` 声明 WT_VERSION=29、WE_VERSION=21、WE_MAX_VERSION=22。它提供：控制Socket建立与接口枚举、WEXT版本及range读取、动态扩容的私有ioctl表、基本无线配置、频率／功率／质量格式化、WEP密钥和MAC输入、WEXT事件拆解与扫描链表生成。它不是nl80211/libnl库。[inc/iwlib.h:94](/home/tronlong/lyp/meari/nvr-wlan/inc/iwlib.h:94)。

实际ATBM603x链为 `iwpriv_proc1 → iw_sockets_open → iw_get_priv_info → set_private_cmd → ioctl → suggest_ch`。`iw_get_priv_info` 从16项起扩容，在errno=E2BIG时按驱动返回长度或翻倍重试，失败释放并把输出指针置NULL；当前调用者处理了获取失败。频率转换与dBm/mW转换的默认分支使用pow/log10/floor/ceil，主机对象单独链接不加-lm失败，加-lm成功，不能把独立iwpriv工具里的WE_NOLIBM配置自动套到顶层目标。[iwlib.c:594](/home/tronlong/lyp/meari/nvr-wlan/iwlib.c:594)、[iwlib.c:944](/home/tronlong/lyp/meari/nvr-wlan/iwlib.c:944)。

扫描API以 `iw_process_scan` 驱动重试：首次请求后返回250ms，再遇EAGAIN返回100ms，retry>150失败；结果为malloc分配的wireless_scan链，调用者负责释放。`iw_scan` 与处理结果时会把context.result置NULL，没有替调用者回收旧链。当前主链的IView路径调用的是外部wpa_cli，ATBM603x调用私有best_ch_scan，不走这套普通扫描API。[iwlib.c:3032](/home/tronlong/lyp/meari/nvr-wlan/iwlib.c:3032)、[iwlib.c:3187](/home/tronlong/lyp/meari/nvr-wlan/iwlib.c:3187)。

库接口仍不能无条件复用：`iw_print_stats` 按snprintf的“应输出长度”移动指针／减容量，短缓冲下后续写可能越界；`iw_in_inet` 将解析后的标准名称strcpy回调用者输入，没有独立容量；`iw_get_stats` 的/proc分支使用strtok并缺少完整字段判空；`iw_process_scanning_token` 对部分非AP事件直接使用wscan，依赖驱动事件顺序。以上为静态条件，未宣称在当前ATBM私有扫描主链发生。[iwlib.c:1262](/home/tronlong/lyp/meari/nvr-wlan/iwlib.c:1262)、[iwlib.c:1347](/home/tronlong/lyp/meari/nvr-wlan/iwlib.c:1347)、[iwlib.c:2190](/home/tronlong/lyp/meari/nvr-wlan/iwlib.c:2190)、[iwlib.c:2944](/home/tronlong/lyp/meari/nvr-wlan/iwlib.c:2944)。

`inc/list.h` 为内嵌双向链表，使用GNU typeof／语句表达式／container_of，不自行加锁或管理对象生存期；TCP服务通过自己的mutex保护插入删除。`inc/wireless.h` 是WEXT用户空间常量与结构定义；它包含ABI而不包含无线驱动实现。`inc/iwscan.h` 的iwscan／iwscan_v2等声明，在顶层没有对应实现，不能视为产品已启用扫描模块。

### 6.4 独立 iwpriv 示例工具

`tool/iwpriv_code_v0.7.3` 与根目录内嵌版本属于不同构建目标：Makefile生成iwpriv_demo，编入main、iwpriv、iwlib、getResults；CC默认mips-linux-uclibc-gcc，启用WE_NOLIBM和WE_ESSENTIAL。main有命令分发表，覆盖best_ch_scan、私有消息、吞吐率及common等入口；getResults按命令解释输出，而根目录iwpriv成功后统一提取最佳信道。[tool/iwpriv_code_v0.7.3/Makefile:9](/home/tronlong/lyp/meari/nvr-wlan/tool/iwpriv_code_v0.7.3/Makefile:9)、[main.c:4](/home/tronlong/lyp/meari/nvr-wlan/tool/iwpriv_code_v0.7.3/main.c:4)。

独立工具也有可见边界：main打印Operate failed后仍返回0；best_scan把二进制结构当 `%s` 打印，并将argv[3]改为结构地址、按4参数调用内部处理器。argc=3时argv[3]是原NULL终止槽，不能仅凭此写入认定数组越界。它不由顶层Makefile生成，也没有证据说明设备PATH中的iwpriv就是此iwpriv_demo。目录自带旧对象和ELF只证明存在旧产物，不能证明其与当前源码或固件一致。[main.c:121](/home/tronlong/lyp/meari/nvr-wlan/tool/iwpriv_code_v0.7.3/main.c:121)、[main.c:417](/home/tronlong/lyp/meari/nvr-wlan/tool/iwpriv_code_v0.7.3/main.c:417)。

### 6.5 hostapd、wpa_supplicant 与 libnl

| 源树 | 本地确认的职责与配置 | 与nvr_wlan关系 |
|---|---|---|
| hostapd 2.9_ATBM_0.1 | hostapd/main.c与config_file.c；src/ap的AP状态、WPA认证、beacon、hw_features及ACS；src/drivers的后端；src/crypto/tls/eap等库 | 本进程发送hostapd启动命令、hostapd_cli status命令；没有直接调用其内部函数 |
| wpa_supplicant 2.9 | wpa_supplicant/main.c、wpa_supplicant.c、events.c；配置与ctrl_iface；src/rsn_supp/eap_peer/p2p/drivers及crypto | IView扫描用wpa_cli scan/scan_result；有效主链没有启动supplicant的本地代码，需外部环境 |
| libnl源码与安装树 | 3.4.0；通用Netlink消息／Socket／cache，genl、route与netfilter；安装头文件与库位于tool/hostapd/libnl | 供独立hostapd/supplicant nl80211后端使用；顶层LDFLAGS没有-lnl* |

随附hostapd.config启用DRIVER_HOSTAP、DRIVER_NL80211、LIBNL32、IEEE80211N、IEEE80211W、ACS和内部TLS；IEEE80211AX处于注释状态。supplicant.config启用WEXT、NL80211、WIRED、WPS、CTRL_IFACE、内部TLS、AP、P2P、N/AC等，ACS处于注释状态。这说明特定源树的构建选择，不证明产品实际运行了全部功能；无线加密最终还依赖没有随本目录提供的hostapd.conf等运行配置。

**ATBM具名逻辑已核对**：nl80211事件路径的 `atbm_channel_change_event` 检查reason_code>=9999及特定源MAC，将mgmt.seq_ctrl／duration解释为信道／副信道；channel为1～13时调用 `hostapd_change_channel`。后者清理各BSS的站点及部分状态、修改conf并reload BSS。这是第三方工具源内的产品定制路径，与nvr_wlan自己的启动信道选择是两个执行位置；还需要驱动发送匹配事件才能成立，未做空口／驱动复现。[driver_nl80211_event.c:850](/home/tronlong/lyp/meari/nvr-wlan/tool/hostapd/hostapd-2.9_ATBM_0.1/hostapd-2.9_ATBM_0.1/src/drivers/driver_nl80211_event.c:850)、[hostapd.c:287](/home/tronlong/lyp/meari/nvr-wlan/tool/hostapd/hostapd-2.9_ATBM_0.1/hostapd-2.9_ATBM_0.1/src/ap/hostapd.c:287)。

另在hostapd_select_hw_mode的channel14分支，日志写“Disable OFDM/HT/VHT/HE”，但将hw_mode改B、关闭N/AC的三条语句被注释，只保留关闭AX。不能根据日志判定实际已禁用全部OFDM/HT/VHT。没有可信的未改动上游基线，本报告不把所有源码差异都归因于ATBM补丁。[hw_features.c:883](/home/tronlong/lyp/meari/nvr-wlan/tool/hostapd/hostapd-2.9_ATBM_0.1/hostapd-2.9_ATBM_0.1/src/ap/hw_features.c:883)。

第三方详细协议状态机、全部驱动后端、密码算法及上游补丁／CVE对应关系尚未逐函数审计。本次没有联网补充版本漏洞结论，也没有根据“2.9”或“3.4.0”直接给出已存在具体CVE、已修复或安全的结论。

---

<a id="chapter-07"></a>

## 七、关键数据流分析

### 7.1 启动、配置及失败传播

~~~text
main
 → 等StorageRPC::is_nvr_start（每2秒，无总超时）
 → 额外等待3秒 → 启动看门狗 → 获取设备信息 → 启动消息线程
 → CShareDataBuf只读对象 → 缓存通道数
 → nv_wlan_init
     → RPC读network类型 → AP／PoE／7628同步初始化
         → RPC配置 + ioctl查询／私有扫描
         → /var/systemCmd.socket → 外部命令执行者 → 网卡/配置/守护程序
     → 若eth0存在则启动ifplugd → 刷新hostname
 → 分配双TCP服务 → 启动8045/8046监听线程
 → main常驻休眠
~~~

该链有两种不同故障：内层扫描失败被pps_wifi_init吞掉后仍能进入TCP初始化；PID／AP状态循环卡住则main不能进入TCP初始化。消息线程／看门狗可能已在运行，这时“进程活着”不能代表启动完成。`nv_wlan_init` 多次调用nv_get_nvr_type，而不是获取一次快照；配置在这些RPC之间改变时，不保证按同一类型值选择，未动态构造此时序。

### 7.2 普通IPC接入及拓扑申请

~~~text
accept8045 → 清零client、默认类型0、设置首次查询/信息查询标志
 → 分片接收 → 找帧头/长度 → 字节累加校验
 → REG：读取WiFi密码，或接受POE/OTHER前缀 → reg=1
     → 缓存hostname、类型、协议、媒体端口、comm_mode → REG应答
 → select空闲时查询FIRST_CONNECT
     → IPC应答is_qrcode+hostname → RPC添加/更新通道
     → 读取外部hostname列表 → 校验存在性 → 可能安排RESET
 → select空闲时查询GET_INFO → version/model → RPC上报在线

APPLY_CONNECT可另行到达：
 清理旧父子关系 → parent含"_"视为NVR，或校验IPC父节点容量
 → 修改父指针 → 提交superidx → reg=1 → 返回同意/拒绝
~~~

上图描述可执行的正常意图，不表示强制先后协议：APPLY能直接置reg；FIRST_CONNECT、GET_LIST等在主switch前没有注册门槛。FIRST_CONNECT提交给RPC的hostname来自payload，而其他后续动作可使用已有client.hostname，两者没有一致性验证。不能把REG成功、RPC请求发出、通道最终添加、媒体在线视作同一个成功事件。[nvr_wlan_protocol.c:219](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_protocol.c:219)。

### 7.3 列表与network开关广播

~~~text
两个监听线程select超时
 → 读共享通道状态／RPC network开关
 → static last_status检测变化
 → hostname成员变化才通知list更新
 → 锁普通/CPU服务链表，为每个客户端置更新标志
 → 各客户端select空闲、reg=1、非MCU时生成通知帧
 → 发送队列 → IPC回ACK（本地只break，无完成跟踪）
~~~

列表更新发送的是空负载通知，IPC再发GET_LIST才能取count+hostname/quality的完整列表；不是直接广播完整拓扑。静态变化记录由两个监听线程共享且缺统一锁；这些通知只置g_server链表标志，不置g_mcu_server链表。高accept压力和持续客户端I/O也会延后超时分支调度。[nvr_wlan_tcp_server.c:403](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_tcp_server.c:403)、[nvr_wlan_protocol.c:340](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_protocol.c:340)。

### 7.4 电池IPC唤醒、休眠与离线

~~~text
外部app-msg-push：WAKEUP事件 + channel + 非空payload
 → 消息线程 → channel映射hostname
 → 锁8046链表找MCU → is_update_wakeup=1
 → MCU客户端reg=1且select空闲 → 打包WAKEUP → TCP发送
 → MCU/IPC硬件动作（仓库外）
 → CPU重连8045并完成FIRST_CONNECT
 → 按hostname关联MCU → RPC上报WAKEUP

CPU断线且缓存MCU指针非空 → RPC上报SLEEP
MCU断线且找不到CPU → RPC上报OFFLINE
~~~

“事件被消费”“标志被设置”“帧发出”“收到ACK”“CPU实际唤醒”分别需要不同证据。当前唤醒ACK只日志，不能证明动作完成；MCU指针解绑错误还会破坏从连接存在性推导的睡眠状态，详见5.7。

### 7.5 删除、解绑与断线清理

外部DEL／UNBIND事件先按channel获取缓存hostname，给g_server匹配连接置reset标志，再刷新hostname列表；客户端稍后在空闲分支发RESET type=0。删除事件到达时若缓存已无hostname或没有连接，返回-1且消息线程未重试。客户端循环另在send_reset_dev=0时检查设备存在性；不存在可重新安排RESET，但查询错误返回-1不等于不存在0。

断线时先close及移除client，再检查同hostname+IP是否有新连接；没有新连接则清理本地父关系、解绑CPU/MCU、提交离线信息及低功耗状态，然后free。`nv_wlan_set_superidx(hostname,NULL)` 在断线流程中已注释，不能写成断线必然同步清除主服务的superidx。上述业务失败不会统一阻止free或启动补偿。[nvr_wlan_tcp_server.c:318](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_tcp_server.c:318)。

### 7.6 媒体数据流的仓库边界

本地传输的有效payload是设备标识、token、列表、状态、信息、控制命令。`stream_port`从REG读取并转交主服务，设备信息中的媒体缓冲大小只提供getter。本目录没有将TCP8045/8046数据送到视频解码／编码／录像／AI模块的链路，也没有本地媒体会话创建实现。参考报告的视频／AI流章节在本项目应以此边界说明代替，不能照搬出不存在的媒体架构。

---

<a id="chapter-08"></a>

## 八、编译构建体系

### 8.1 默认目标、源文件与依赖

顶层Makefile默认 `all: lib lib_copy target target_copy`，输出build/nvr_wlan；默认lib-y为空，不生成自己的静态库。所有根目录C/C++源展开为16个对象，C使用gcc，C++使用c++及-std=c++11；存在C++对象时链接g++并附-lstdc++。公共选项是 `-Wall -Wno-unused-function -Werror -O0 -g`，依赖文件为-MMD -MP生成的.d，最终没有启用strip。[Makefile:5](/home/tronlong/lyp/meari/nvr-wlan/Makefile:5)、[Makefile:104](/home/tronlong/lyp/meari/nvr-wlan/Makefile:104)。

| 依赖 | 本地依据 | 本次状态 |
|---|---|---|
| nvr-pack兄弟树 | 由PWD把 `/nvr-wlan` 替成 `/nvr-pack` 后wildcard | 指定源码目录的兄弟树不存在；直接make -n all报错 |
| 产品头文件 | nvr-pack/include/nvrbase/util、mod、rpcclient | 当前无法核对真实结构体／宏／IPC库ABI |
| RPC与基础工具库 | 默认-lrpcclient -lnvrtools；两个arch路径提供-L | 未取得对应产品架构库并完成链接 |
| pthread、C++运行库 | -lpthread；条件-lstdc++ | 隔离测试使用主机库，不能代表目标运行库 |
| 数学符号 | iwlib默认分支调用pow/log10/floor/ceil | 顶层未显式-lm或定义WE_NOLIBM；主机单对象对照链接证实需要数学符号提供者 |
| 系统程序及运行配置 | hostapd、hostapd_cli、wpa_cli、udhcpd、ifconfig、route、ifplugd、iwpriv、sed等 | 随附部分工具源码；PATH部署与配置模板不在此构建目标中闭环 |

缺nvr-pack会在解析Makefile时停止，连clean也受此路径检查影响。`NVR_PACK_PATH`依据环境PWD而不是Makefile实际所在目录；用make -C、迁移目录或重命名时需确认其展开值。本次Make变量探测显式传入临时不存在的SDK路径，仅查询对象与标志，不冒充真实SDK构建。[Makefile:26](/home/tronlong/lyp/meari/nvr-wlan/Makefile:26)。

### 8.2 平台菜单与变量不一致

产品／编译器表见2.1。`all.sh` 菜单把app_type保留在脚本变量中，make调用没有显式传 `app_type=t23`，t23n名称也与Makefile条件字符串t23不同；Makefile据 `ifneq (${app_type},t23)` 为编译与链接都加-muclibc。正常菜单调用没有完成该豁免条件传递；若外部环境另传app_type，结果需按实际环境确认。[all.sh:33](/home/tronlong/lyp/meari/nvr-wlan/all.sh:33)、[Makefile:93](/home/tronlong/lyp/meari/nvr-wlan/Makefile:93)。

x86菜单传EXTRA_CFLAGS=-DX86_64，但顶层Makefile没有读取EXTRA_CFLAGS，因此这个宏不会由该菜单自动进入实际CFLAGS。所有平台共用build目录，对象名称不包含ARCH_PATH或编译器；切换架构／选项时，旧对象可能因源码时间戳未变而继续使用，需要隔离构建目录或明确清理策略。

**本机实测的-muclibc行为**：GCC能够编译／链接一个空main；默认产物解释器是 `/lib64/ld-linux-x86-64.so.2`，加-muclibc后是 `/lib/ld64-uClibc.so.0`。这支持“选项改变运行时加载器要求”，不支持“一定被本机GCC拒绝”。真实目标工具链对这个选项的处理，以及产品rootfs是否包含对应加载器／运行库仍待核验，不能把主机探测推成所有交叉工具链的结论。

### 8.3 打包、并行与版本信息

target_copy默认复制到nvr-pack/arch/ARCH_PATH/pack/app；mips-a1额外复制到base产品目录。顶层没有构造固件镜像、刷机、启动服务或执行运行测试的规则，复制成功只证明某个文件被复制。

`all` 的target与target_copy是并列先决条件，target_copy没有依赖target；make -j时可能在新目标链接完成前拷贝。lib_copy与lib也没有完整顺序依赖。默认单进程菜单没有传-j，但从Makefile不能保证外部MAKEFLAGS或其他调用不会启用并行。[Makefile:104](/home/tronlong/lyp/meari/nvr-wlan/Makefile:104)、[Makefile:125](/home/tronlong/lyp/meari/nvr-wlan/Makefile:125)、[Makefile:148](/home/tronlong/lyp/meari/nvr-wlan/Makefile:148)。

版本宏包含GIT_BRANCH、TIME、COMMIT、AUTHOR、DIRTY。DIRTY依赖git status中精确的中文清洁状态文本；换locale或Git输出格式可能把干净目录标为P，不是可靠的内容diff检测。宏只记录构建时字符串，不能证明旧ELF和源码清单一一对应。[Makefile:10](/home/tronlong/lyp/meari/nvr-wlan/Makefile:10)。

### 8.4 独立第三方构建

hostapd和supplicant分别有自己的Makefile／.config，CC均写有mips-linux-uclibc-gcc；hostapd使用LIBNL变量追加头文件与库路径，supplicant.config则包含 `/home/wangruxin/code/hostapd/libnl` 的绝对路径。libnl的旧config.log记录mips-linux-uclibc host和该安装prefix。它们是已有构建环境记录，不能假定在当前目录直接可重建；顶层也没有递归调用这些Makefile。

hostapd默认独立目标为hostapd和hostapd_cli；supplicant包含wpa_supplicant、wpa_cli、wpa_passphrase等构建规则。不应只因源码含AP/P2P/TLS就认定所有相应程序或功能均被固件部署。[hostapd/Makefile:1276](/home/tronlong/lyp/meari/nvr-wlan/tool/hostapd/hostapd-2.9_ATBM_0.1/hostapd-2.9_ATBM_0.1/hostapd/Makefile:1276)、[supplicant/.config:49](/home/tronlong/lyp/meari/nvr-wlan/tool/wpa_supplicant-2.9/wpa_supplicant/.config:49)、[libnl/config.log:816](/home/tronlong/lyp/meari/nvr-wlan/tool/hostapd/libnl-3.4.0/config.log:816)。

### 8.5 已执行检查与不能替代的结果

| 检查 | 本次结果 | 含义 |
|---|---|---|
| bash -n all.sh | 成功 | 只验证shell语法 |
| 原目录make -n all | exit=2，缺nvr-pack | 产品构建入口目前不具备完整依赖 |
| make -qp +显式临时SDK路径+app_type=t23 | 查询出16对象、目标及标志；有非零状态 | Make数据库查询，不是编译或“构建通过” |
| 6组原隔离fixture | 编译／执行完成，原有输出重新确认 | 测试依赖SDK/RPC/驱动替身、8通道fixture及-w，不代表真实宏／ABI／-Werror构建 |
| 原3组ASan触发案例 | 都检出预期heap-buffer-overflow | 测试进程非零退出是预期问题复现；不代表修复后通过 |
| 2组补充工具fixture | 断言通过；新增DNS越界ASan案例检出预期错误 | 核对工具输入／返回和替身ioctl参数，未修改本机网络或/etc |
| 主机严格编译独立源 | iwlib、atbm_ioctrl、pps_cmd_cli成功；wifi6_best_ch失败 | 使用顶层的-Wall/-Werror/O0/g，wifi6的if(argv[1])触发-Werror=address；只针对当前主机GCC |
| iwlib对象链接对照 | 未-lm失败；加-lm成功 | 已验证数学符号需求，真实SDK可能另提供符号，未据此宣称产品一定链接失败 |
| ELF解释器探测 | 见8.2 | 只验证空main在本机工具链的选项效果 |

以上为本次已执行检查的结果摘要，原始命令日志及测试脚本不随文提供。全目标nc7／nt7／nd7／nd5／x86／t23n的编译、链接、部署和上板检查均未完成，不能在报告中填成“支持平台测试通过”。

---

<a id="chapter-09"></a>

## 九、接口设计规范

### 9.1 C/C++ 混合与头文件完整性

业务头文件大多使用 `#ifdef __cplusplus extern "C"` 对外提供C符号；main另显式声明nvr_tcp_server_init，RPC／pushmsg实现为C++。list.h依赖GNU扩展；nvr_tcp_server.h使用HOSTNAME_MAX_SIZE却未自行包含定义它的nvr_wlan.h，当前调用者先包含该头才满足条件，单独包含不保证可编译。

已确认的声明边界包括：pps_wpa_cli.h拼错__cplusplus；pps_netmng.h仍声明nv_get_ipc_net_attr、nv_set_login_status、nv_get_login_status等而顶层无对应实现；iwscan.h只有声明；netconfig.h声明pps_get_mac_num，当前netconfig.c没有其定义。实际业务入口是nv_wlan_get_login_status，不能把相似名字当同一API。wpa_ctrl.h虽声明控制接口，顶层没有编入wpa_ctrl.c且旧AP调用都注释。它们可能来自旧移植或外部实现，但当前目录不能证明所有声明都可链接。[inc/pps_netmng.h:31](/home/tronlong/lyp/meari/nvr-wlan/inc/pps_netmng.h:31)、[inc/netconfig.h:60](/home/tronlong/lyp/meari/nvr-wlan/inc/netconfig.h:60)。

### 9.2 返回值不是一个统一规范

| 接口 | 当前约定 | 调用者应区分的含义 |
|---|---|---|
| 多数配置／RPC函数 | 0成功、负值失败，或转发外部值 | 但部分函数忽略内层返回而恒返回0 |
| nv_wlan_update_all_hostname | 1成员变化、0无成员变化、-1错误 | quality改变不一定返回1；通道迁移还存在缓存缺陷 |
| nv_wlan_check_dev_is_exist | 1存在、0不存在、-1参数／查询错误 | -1不代表“已确认不存在”；当前不同调用使用不同条件 |
| nv_wlan_check_hostname_is_connect | bool | 检查路径及直接孩子容量，函数名不能当硬件连接检测 |
| nv_get_nvr_model_name | 正常返回NV_TRUE，错误-1 | 与0成功风格不同 |
| nvr_wlan_protocol_recv | 正常返回新增应答字节数；参数错误-1 | 单帧处理-1被外层消费，不自动关闭TCP |
| 系列协议打包 | 正长度、-1失败 | 队列置位成功不是打包／发送成功 |
| pps_system_cmd | 外部服务返回int或本地-1 | status编码、进程退出原因及系统实际效果依赖服务端 |
| 自定义inet_pton_with_af | OK=0、ERROR=-1 | 与libc inet_pton的1成功／0无效不同，且有已复现错误映射 |

调用者的错误传播应逐函数核对；本报告没有以“统一NV_SUCCESS/NV_FAILURE”概括整个目录。

### 9.3 通道、列表索引与拓扑索引

通道chan在本地遍历使用0起始；数组槽位是压紧后的列表位置，不一定等于chan。主服务superidx约定从本地赋值看为-1清除、0 NVR、父IPC的chan+1。LP_IPC状态按1离线／2唤醒／3睡眠，dev_type则0普通／1 MCU／2 CPU；它们是不同枚举。

`NV_WLAN_SIZE=NV_CHAN_MAX`、`WIFI_MAX_LOAD_CNT=WIFI_MAX_CNT`、路径上限 `WIFI_MAX_PATH_LEN+1`，其中外部宏定义缺失。不能将测试fixture的8槽、WIFI_MAX_CNT=4、WIFI_MAX_PATH_LEN=4写成产品能力。消息线程直接使用NetFrameIndex.channel，没有启用注释中的Tuya通道转换；外部生产者channel的约定要与此相匹配。[inc/nvr_wlan.h:21](/home/tronlong/lyp/meari/nvr-wlan/inc/nvr_wlan.h:21)、[nvr_wlan_rpc.cc:357](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_rpc.cc:357)。

列表count在线格式占1字节，源码用char参与长度计算，quality也截到1字节，协议没有传独立chan／parent字段。产品上限需要同时满足字段范围和10KiB发送容量；未知NV_CHAN_MAX时不能断言所有配置都适配，也不能武断认定当前产品超过限制。

### 9.4 字节序、对齐与字符串容量

协议cmd／len／stream_port／校验和在线格式均按大端字节构造，但解析cmd采用直接结构体读取再无条件交换，依赖小端宿主；噪声前缀还可使结构体指针非对齐。ioctl消息及系统命令int头使用本机ABI，不是这套网络字节序。外部端必须核对结构大小与宏值，不能因为本机sizeof结果正确就推定所有架构均正确。

hostname、version、model的64字节线字段必须有受限读取与终止策略；仅校验整个payload长度不约束字段strlen。多数getter没有输出容量参数或不明确检查len，例如nv_wlan_get_chan_ip按MODC_COMM_LEN输出、nv_wlan_get_all_hostname默认调用者准备整个列表容量。当前调用点与公共API契约应分开说明，不能假定所有调用者总传清零缓冲。

### 9.5 并发、所有权与完成状态

拓扑数组由全局mutex保护，服务链表由各自mutex保护；客户端对象由accept创建、其线程退出后free，所有线程无统一stop/join。服务线程及消息线程置标志时持链表锁，但客户端读取、清除标志及写身份字段时没有获取同一锁，属于本地可见的非同步访问；mutex只保护了部分链表操作。未执行ThreadSanitizer，不能给出实际竞态出现概率。

拓扑更新、set_superidx及低功耗上报在持拓扑锁时调用外部RPC，阻塞时间会延伸到等待该锁的其他业务。`wlan_device_list_mutex` 已用静态初始化，又在nv_wlan_init重新pthread_mutex_init；消息线程更早启动，初始化／使用时序未闭环。这里是同步设计边界，不声称已经复现系统死锁。

异步控制入口的成功只表示“匹配并置位”；没有统一request ID、ACK状态、超时重发、失败回滚及关闭协议。调用方应按现有能力解释结果，不能把返回0定义成远端已经执行成功。

---

<a id="chapter-10"></a>

## 十、安全与加密

### 10.1 暴露面与信任边界

8045与8046绑定IPv4 INADDR_ANY，未在本地限定只接受内部AP／PoE网卡，也没有在accept处验证来源IP、MAC或接口。实际可达性还取决于设备地址、路由、网络隔离及防火墙，当前目录没有提供这些最终规则；不能仅因绑定ANY就写成“互联网可直接攻击”。

本地控制协议没有TLS封装、消息MAC、序列号或重放窗口，字节累加和只能检测一部分传输错误。无线链路的WPA/WPA2配置与TCP控制会话鉴权是两个边界：7628有WPA2PSK/AES设置命令，标准AP依赖外部模板；不能据此推断私有TCP协议具备端到端加密。[nvr_wlan_tcp_server.c:378](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_tcp_server.c:378)、[nvr_wlan_protocol.c:102](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_protocol.c:102)。

### 10.2 注册及会话授权的实际条件

REG判定明确接受三个条件之一：WiFi密码比较匹配，token以 `POE` 开头，token以 `OTHER` 开头。后两项不是精确等值比较，也未按当前network类型限制；原fixture用 `OTHERsuffix`、WiFi密码为secret，仍得到reg=1、应答status=0，已重新复现。这证明本地固定前缀接受路径，不能解释为“只有真实PoE产品才能使用POE token”。[nvr_wlan_protocol.c:264](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_protocol.c:264)。

此外，APPLY parent含下划线可直接置reg；未注册FIRST_CONNECT仍能调用添加RPC。解析器没有强制dev_type与监听端口匹配，也没有把应答hostname与client.hostname校验一致。dev_type／protocol_type／comm_mode由对端提供，没有完整枚举范围验证。

这些证据足以说明本进程没有形成完整会话授权闭环。外部RPC是否有二次授权、主服务最终是否接受通道添加、固件是否隔离控制端口，都缺少实现／配置证据，因此不能直接声称已经取得主服务权限、远程代码执行或整机接管。

### 10.3 输入、资源与本地命令风险

| 风险 | 已核验事实 | 实际影响边界 |
|---|---|---|
| 协议越界读取 | 帧头噪声后剩余长度计算错误；无NUL固定字段按字符串读取；原ASan均复现 | 本地函数有越界条件；真实缓冲、架构和配置影响结果，未证明可利用为代码执行 |
| 控制帧语义缺少关联 | 未注册也处理业务命令；任何通过校验的帧可续心跳；ACK无请求关联 | 可持续占用会话／触发本地动作的条件成立；最终业务效果需外部实现 |
| 连接资源没有应用总上限 | 每连接线程及约20KiB缓冲，backlog8只是等待队列；无FD_SETSIZE检查 | 连接增长存在内存、线程及fd压力条件；未做实机容量／拒绝服务压力测试 |
| 命令参数转义不完整 | SSID／密码等被拼进sed、iwpriv文本；外部执行者不在仓库 | 至少可能破坏命令和配置；shell注入的最终可达性依赖外部解释方式与输入约束 |
| 凭据日志 | pps_system_cmd无条件printf完整命令及结果；7628以NV_WARN打印SSID／密码；REG token、设备authkey亦有日志调用 | 命令日志中的密码无需打开TRACE即可进入stdout；落盘／日志读取权限仍待设备核验 |
| 临时文件与配置修改 | 固定/tmp扫描／状态路径、固定hostapd.conf行号、DHCP全局替换与追加 | 依赖系统目录权限及模板，未核验跨进程竞争、符号链接或权限攻击可达性 |
| 随机设备修改 | AP／7628发送移动/dev/random并链接/dev/urandom命令，忽略部分失败 | 已确认系统级修改意图；不能脱离内核、启动阶段和实际执行结果直接断言密码随机性已被削弱 |
| 工具库输入安全 | DNS容量忽略、接口名无界复制等见第六章 | 编入目标但多条未在本地主链调用；应与可远程触发的协议问题分开评估 |

凭据位置见[pps_cmd_cli.c:119](/home/tronlong/lyp/meari/nvr-wlan/pps_cmd_cli.c:119)、[pps_netmng.c:496](/home/tronlong/lyp/meari/nvr-wlan/pps_netmng.c:496)、[nvr_wlan_rpc.cc:122](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_rpc.cc:122)、[nvr_wlan_protocol.c:276](/home/tronlong/lyp/meari/nvr-wlan/nvr_wlan_protocol.c:276)。main初始日志级别为WARN，部分INFO／TRACE调用可能被过滤，但pps_system_cmd的printf不受该等级控制；不能简单写成“默认全部密钥日志都会打印”或“WARN已消除泄露”。

### 10.4 按本地证据安排修正顺序

以下为建议，不表示已经修改源码或产品已经确认对应故障。

| 顺序 | 应处理的机制 | 为什么优先 | 验收依据 |
|---|---|---|---|
| 第一批 | 帧缓存剩余长度、受限固定字段、cmd解析及容量验证 | 外部输入直接进入解析，已有ASan复现 | 原噪声／无NUL案例不再越界，保持正常分片／连续帧行为 |
| 第一批 | REG固定前缀及APPLY绕过注册、应答类命令授权 | 本地会话门槛可被协议分支绕过，已有RPC调用捕获 | 明确产品认证契约后验证各network与设备类型的正负案例 |
| 第一批 | 数组压紧父指针、MCU解绑链表、TCP短写偏移 | 已有拓扑／关联复现，短写条件可直接推导错误 | 删除／迁移后拓扑一致，关联退出清除，短写仍发送完整正确帧 |
| 第二批 | 启动失败传播、PID总超时、服务bind就绪及消息启动次序 | 现有“返回0／进程活着”不能代表服务工作 | 故障注入下启动明确失败，消息处理只在依赖初始化完成后开始 |
| 第二批 | 线程创建返回判断、标志同步、连接总量与fd边界 | 资源错误和并发路径没有闭环 | pthread错误／高fd／并发唤醒删除时有可观测且受控的结果 |
| 第二批 | 工具转换器／DNS／路由参数错误、链接配置和平台变量 | 本地工具已有失败证据；构建配置与菜单不一致 | 原工具fixture恢复正确契约，各目标干净构建并记录ABI与产物 |
| 第三批 | 配置模板、凭据日志、命令参数、部署与第三方维护 | 需要外部系统服务／固件配置配合，不宜仅改单文件推定完成 | 追溯配置、限制日志、确认执行方式及实际部署二进制来源 |

没有做统计测量，以上不包含虚构的故障率、延迟、吞吐率、内存峰值、攻击评分或修复收益数字。

---

<a id="chapter-11"></a>

## 十一、典型使用场景

### 11.1 标准WiFi NVR开机并连接普通IPC

前提是主服务RPC／共享数据可用、network=0、存在p2p0/wlan0、系统命令服务与配置模板已部署，且所选驱动的扫描／hostapd接口可用。程序等待主服务，按SSID／密码／信道初始化AP，配置内部IPv4和DHCP，再启动TCP服务。普通IPC接入8045，完成REG、首次连接与设备信息应答后，由RPC请求添加和在线上报。

本地成功指标需要分开看：AP PID文件存在只是进程状态；ENABLED／ACS只是该状态检查接受；reg=1只是本地登记；外部通道在线才是共享状态来源。端口监听成功、DHCP租约、IPC媒体在线需要额外设备证据。AP初始化失败返回值被吞掉和AP循环卡住是不同故障场景。

### 11.2 PoE NVR内部网口与静态租约

network=1时，读取eth0默认网关、配置eth1内部/24地址、添加／恢复默认路由，替换DHCP接口并从RPC追加静态租约；有线IPC仍可使用私有TCP登记和设备控制能力。这里没有PoE供电控制或完整ONVIF发现／媒体实现，不能把static_lease注释中的onvif当ONVIF栈。

需要在产品环境核验eth0/eth1角色、内部地址是否与外网重叠、默认路由metric实际结果、重复启动是否残留租约／地址修改。若is_same_subnet返回-1，调用者走默认172.168.0.1分支，不能据此认为“已经确认没有冲突”。

### 11.3 7628产品私有配置

network=2时，程序重启ra0并通过私有命令设置WPA2PSK/AES与SSID／WPAPSK；默认不在此函数设置内部地址和DHCP。lan_ip为静态清零对象，只有AP／PoE路径赋值；在本地执行一次7628初始化的正常启动路径中，REG应答内部IP字段仍是空字符串。若设备上能返回有效内部IP，需找到额外配置／不同二进制的证据，不能靠“产品通常如此”补写实现。

### 11.4 IView与ATBM自动信道

IView 204W型号前10字节精确匹配时，程序用外部wpa_cli扫描并按AP数量选择1／6／11；扫描结果为空可选择1，扫描文件打不开返回-1且该特殊分支没有相同的1～14检查。其他型号按wlan_better、固定信道、8192fu ACS或ATBM私有扫描选择。ATBM606x的0/0请求及18信道返回错误已在替身测试复现，不能据函数打印“suggest channel 11”就认定上层用了11。

### 11.5 电池IPC CPU睡眠、MCU保持连接与唤醒

MCU使用8046、CPU使用8045是服务设计意图，但协议不严格限制对端类型与端口。MCU注册时按hostname找CPU并上报WAKEUP或SLEEP；外部应用请求唤醒后给MCU会话置位，空闲调度发WAKEUP。CPU重新连接并首次应答后可上报WAKEUP；CPU断线、MCU指针非空时上报SLEEP。

MCU310秒超时、CPU6秒超时由控制连接心跳管理，不是设备实际休眠计时器。关联解绑缺陷可使CPU仍持旧MCU指针，故状态上报必须与真实MCU连接及硬件动作交叉核验，不能将低功耗枚举上报当功耗或睡眠成功测量。

### 11.6 删除通道、设备重连及故障恢复

外部主服务发DEL／UNBIND事件时，本进程安排RESET并刷新缓存。重连可通过同hostname+IP的新连接检测减少旧连接退出带来的离线操作；不同IP重连不满足该精确条件。删除导致数组压紧时还可能破坏父指针；通道迁移hostname不变时缓存chan不刷新，这会影响后续channel到hostname映射。

当前没有统一恢复监督：监听线程退出不会被main重新创建，AP初始化后的PID状态也没有本地常驻监控，消息置位失败没有持续重试。上层固件可能有监控机制，但本目录没有其实现，不应以看门狗喂狗线程代替功能健康检查。

---

<a id="chapter-12"></a>

## 十二、总结

### 12.1 可以由当前源码得出的结论

本仓库的自有业务职责是NVR网络环境初始化、hostname拓扑缓存、私有TCP接入与电池IPC控制联动；真实通道数据、持久化、消息共享缓冲及多数系统命令执行由目录外部组件提供。AP／PoE／7628分支、ATBM603x／606x适配、8045／8046双服务及低功耗状态桥接都能在源码中追踪。

同时，本地有已复现的协议内存边界、拓扑数组压紧、通道缓存、MCU解绑、扫描返回、地址转换及文件描述符问题，另有静态可推导的会话授权、短写、初始化、并发和工具接口缺陷。不能因项目规模小、使用mutex、采用字节校验或附带hostapd源码，就推定其可靠性与安全性已闭环。

### 12.2 本次覆盖清单

| 顶层编译源文件 | 本次核对内容 | 报告位置 |
|---|---|---|
| main.cc | 入口、初始化顺序、失败检查、常驻方式 | 4.3、5.1、7.1 |
| nvr_wlan_rpc.cc | RPC映射、getter返回契约、看门狗、消息线程、设备信息日志 | 5.1、5.8、9、10 |
| nvr_wlan_pushmsg.cc | NvNetBuf构造、消息头映射、生命周期与失败返回 | 5.8、7.4／7.5 |
| nvr_wlan.c | hostname增删、通道／质量、父关系、superidx、状态检测、网络分流 | 5.4、7、9.3／9.5 |
| nvr_wlan_tcp_server.c | 服务与客户端完整生命周期、超时、发送、异步标志、CPU/MCU关联 | 5.5、5.7、7、10 |
| nvr_wlan_protocol.c | 帧查找／校验／缓存、全部命令case、打包与字段布局 | 5.6、7.2、9.4、10.2 |
| nvr_wlan_common.c | CLOCK_MONOTONIC取秒与错误结果未检查 | 5.5 |
| pps_netmng.c | 三类初始化、信道分支、系统配置、AP等待／重试、lan_ip | 5.2、5.3、11 |
| pps_cmd_cli.c | Unix Socket命令帧、短读、权限路径、声明、日志 | 5.9、6.2、9、10 |
| pps_device_ap.c | 有效定义与整段注释的旧实现、残留声明 | 5.9 |
| wifi6_best_ch.c | 消息布局、参数下标、14／18信道解包与失败传播 | 5.3.4、8.5 |
| atbm_ioctrl.c | Socket与ioctl、接口名、返回状态、fd释放条件 | 5.3.4 |
| iwpriv.c | 私有命令表查询、参数编码、二进制参数、最佳信道解包 | 5.3.3、6.3 |
| iwlib.c | 控制接口／版本／range、私有表、转换、格式化、事件／扫描链表 | 6.3、8.5 |
| net_base_operate.c | 全部地址／Socket／连接／完整读写工具及输入契约 | 6.2、9 |
| netconfig.c | 全部网卡／路由／DNS／IPv6／名称验证／统计工具及可见失败路径 | 6.1、9 |

头文件共24个（根目录1+inc23），逐项职责与实际实现边界如下：

| 头文件 | 核对要点 |
|---|---|
| nvr_wlan_tcp_server.h | 双端口、10KiB缓冲、client/server字段及依赖HOSTNAME_MAX_SIZE |
| inc/nvr_wlan.h | 外部规模宏、64字节hostname、父指针、LP状态与接口 |
| inc/nvr_wlan_common.h | 单调时间接口 |
| inc/nvr_wlan_protocol.h | 全部命令、结构头、dev_type／protocol枚举与注释不一致 |
| inc/nvr_wlan_rpc.h | 外部头依赖、RPC适配与媒体缓冲getter声明 |
| inc/nvr_wlan_pushmsg.h | 公开create／get接口，没有公开destroy |
| inc/pps_netmng.h | 网络初始化及残留无本地实现声明 |
| inc/pps_cmd_cli.h | C/C++系统命令入口 |
| inc/pps_device_ap.h | 残留AP重启声明 |
| inc/pps_wpa_cli.h | 旧控制接口、ipc_net_attr、C++宏拼写错误 |
| inc/pps_device_netmng.h | 整体注释，不提供有效管理实现 |
| inc/hwl_config.h | 整体注释，不构成实际平台配置 |
| inc/hwl_net.h | 整体注释，不构成网络适配实现 |
| inc/pps_type.h | 定宽及平台相关类型、GNU属性、字长条件 |
| inc/netconfig.h | Socket地址与网卡API，pps_get_mac_num缺本地定义 |
| inc/net_base_operate.h | IPv4/IPv6联合地址、超时常量、读写API及地址宏 |
| inc/private_data.h | ATBM603x私有结构、scan_result及版本 |
| inc/atbm_ioctrl.h | ATBM606x int/int/externData[256] ioctl结构与API |
| inc/wifi6_best_ch.h | wifi6_best_ch公开入口 |
| inc/iwscan.h | 扫描结构与无顶层实现的接口声明 |
| inc/iwlib.h | WT29／WEXT版本、无线数据类型与内联ioctl接口 |
| inc/wireless.h | WEXT用户空间ABI常量与结构 |
| inc/list.h | GNU链表宏、插入删除／遍历与锁责任 |
| inc/wpa_ctrl.h | supplicant/hostapd控制声明及事件；旧AP调用注释 |

Makefile、all.sh、README、.gitignore与格式配置亦已核对；第三方3组工具的角色、构建配置、入口及与业务相关的定制代码见6.4／6.5和8.4。目录规模及统计口径见3.1，本节列出业务与头文件的覆盖范围；**哈希清点不等于逐函数审计**。

### 12.3 完成状态与尚未验证项

| 交付／验证项 | 当前状态 | 不能据此推出的结论 |
|---|---|---|
| 对照参考结构的十二章MD | 已补齐 | 章节齐全不等于固件验证通过 |
| 原稿主要事实、宏／端口／帧布局及调用链复核 | 已复核并纠正证据表述 | 不能证明全库没有其他缺陷 |
| 顶层业务、网络工具与全部头文件静态分析 | 已覆盖，列出具体问题与使用条件 | 不等于所有函数所有输入和线程交错均已动态测试 |
| 原隔离测试及新增工具测试 | 已执行，方法与结果见正文；原始日志及脚本不随文提供 | SDK／RPC／驱动替身不是产品ABI与整机行为 |
| 第三方角色、配置与ATBM相关定制路径 | 已核对 | 近百万行第三方代码仍未逐函数审计；上游基线／补丁／CVE映射未完成 |
| 产品SDK、外部命令服务、驱动、配置模板 | 缺关键内容 | 不确认最终RPC权限、消息同步、命令注入可达性或无线参数 |
| 各平台完整构建与固件部署 | 未完成；nvr-pack依赖缺失 | 不确认所有平台当前可构建／可运行 |
| 上板、空口、媒体在线、低功耗动作及负载／竞态 | 未执行 | 不确认真实唤醒成功、产品稳定性、性能或已知问题触发频率 |

因此，若“完成”指已有原稿是否已经全面交付，答案是**原稿未完成**；本次已补齐本地可核验的源码分析文档及证据。如果“完成”包括全第三方逐函数审计、SDK／固件／硬件全链验证，则当前仍不能认定完成。后续核验需要具体版本的nvr-pack头库、系统命令服务实现、产品网络与hostapd/udhcpd模板、驱动源／ABI、部署二进制和设备运行结果；不能以猜测补足这些事实。

### 12.4 核验结果的复查方式

当前交付仅包含本报告，旧稿备份、测试脚本、原始日志和快照清单不随文提供。静态结论可沿各节源码链接检查具体语句及调用条件；测试结果可查8.5与对应模块中的输入、替身条件和观察值。需要独立复现动态结果时，须重新构建测试程序，正文摘要不能替代原始执行记录。

本次隔离测试在临时目录构建原始源码，不运行网络初始化。fixture中的宏值和返回数据是替身；ASan错误退出属于问题复现，不能按“所有测试都通过”隐藏缺陷。源码中的问题本次仅分析与记录，未修改源目录。


