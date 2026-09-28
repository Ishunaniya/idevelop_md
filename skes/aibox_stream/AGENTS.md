# aibox_stream 项目说明

## 项目位置与用途

- Git 仓库根目录：`/home/tronlong/lyp/code/skes`；本项目源码目录：`skes-apps/aibox_stream`。顶层 `CMakeLists.txt` 进入 `skes-apps/`，后者通过 `add_subdirectory(aibox_stream)` 纳入构建。
- 编写本文档时检出的 Git 分支：`main_ui`。这是当前工作树的分支名；切换分支后，应重新核对本文档中的功能和配置。本文档依据该分支当前工作树的源码整理。
- `aibox_stream` 是 SKES（SKy-Eyes System，集成摄像头、AI 和工况数据采集的车载系统）中的视频流服务。它按摄像头通道从本机共享内存读取已编码的 H.264 帧，处理 JT/T 1078 实时预览与录像回放，把视频写到 SD 卡，并发布存储卡、系统时间、容量和录像读写告警。外部供电状态也会被检测并控制录像，事件明细的具体限制见下文。
- `aibox_stream` 与 `jtt_808_1078` 是同一仓库中的不同应用：前者负责视频流和录像，后者处理相关终端通信。同仓库的 `skes-ui/skes-main/camera_shm_multi.cpp` 提供一个匹配的 H.264 共享内存生产实现；部署时必须有上游视频进程先写入共享内存，单独启动本程序不能产生摄像头画面。

### 相关技术和名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| Git 分支 `main_ui` | 当前检出的代码版本线；分支名描述代码归属，不是程序运行参数。 |
| CMake、交叉编译 | CMake 定义 `aibox_stream` 可执行文件和安装规则；交叉编译是在开发机上为目标 ARM Linux 设备构建程序。项目使用 C++11，并混合编译少量 C 文件。 |
| SKES、SEP | SKES 是上级系统；SEP 是 Skes-EndPoint，指处理摄像头、工况解析和上报等功能的应用。`sep.ini` 是随程序安装的启动配置，`param=f 8` 对应父进程模式启动 8 个通道。 |
| JT/T 1078、JT/T 808 | 1078 是车载视频通信协议，本程序封装并发送实时预览或录像回放数据；808 承担终端与平台的消息交互，本项目通过本机 UDP 与 `jtt_808_1078` 对接。接收处理区分 2013 与 2019 版消息偏移。 |
| H.264、I 帧、SPS/PPS、AUD | H.264 是输入的视频编码；I 帧是可独立解码的关键帧，SPS/PPS 是解码参数集，AUD 是访问单元分隔符。取流代码识别关键帧并去掉帧首 AUD，再交给预览与录像模块。 |
| IDR、G.711、BCD | IDR 是可作为新录像文件起点的 H.264 关键帧；G.711 是对讲相关的音频格式，当前独立音频落盘未启用；BCD 是 JT/T 时间字段使用的十进制编码，回放模块将其转换为时间范围。 |
| 共享内存、环形缓冲区 | 上游视频进程与本程序通过 System V 共享内存交换帧；当前取流入口在 `src/jtt1078/jtt1078_stream.cpp`，每通道按设备号计算 key，再按读索引、帧数量和互斥锁取帧。`src/sharedMemory/` 另有通用共享内存辅助代码，并非这条取流链路的入口。 |
| V4L2、GStreamer | V4L2 是 Linux 摄像头接口；同仓库 `skes-ui/skes-main/camera_shm_multi.cpp` 用 GStreamer 从 `/dev/videoN` 采集并把 H.264 数据写入共享内存，本程序消费其中的编码帧。 |
| UDP、TCP | UDP 用于接收本机控制消息并回复 `jtt_808_1078`；收到预览或回放指令后，相关模块再通过 TCP 向指定流媒体服务端发送 1078 数据。 |
| FFmpeg、MP4 | FFmpeg 的 `libavformat`、`libavcodec` 等用于把已有 H.264 帧封装为 MP4，并读取录像供回放；构建还链接 `libavutil`、`libswresample`。这里的录像视频不是由本程序从原始图像实时编码生成。 |
| SD 卡、TF 卡 | 可移除存储介质，代码统一通过 `/mnt/sdcard` 挂载点访问；正常录像和系统时间异常录像分别写入不同目录。 |
| nanomsg、发布订阅 | 本地进程间消息库；本程序以 `PUB` 套接字向 `127.0.0.1:19225` 发送 `/skes/event` 告警事件。 |
| cJSON、INI | cJSON 解析摄像头 JSON 配置并组织告警数据；INI 解析器读取设备标识和密钥配置。 |
| GPIO | 通用输入输出引脚；录像和告警模块读取外部供电状态，`TPAD_EXT_PWR_GPIO` 默认 119，构建时可改为设备实际编号（源码注释给 AIBOX 的编号为 122）。 |

## 主要文件与数据流

| 路径 | 职责 |
| --- | --- |
| `CMakeLists.txt`、`sep.ini` | 构建、依赖与安装规则；SEP 启动参数。 |
| `src/main.cpp`、`src/main.h` | 解析 `f <通道数>` / `c <通道号>` 参数，父进程拉起并监控各通道子进程，子进程读取配置并启动取流、预览、回放、UDP、告警任务。 |
| `src/jtt1078/`、`src/sharedMemory/` | 前者解析 1078 控制消息并直接从共享内存环形缓冲区读取 H.264 帧；后者虽被编译入程序，但当前取流路径没有调用其中的 `shm_init()`。 |
| `src/preview/`、`src/replay/`、`src/talkback/` | 1078 实时预览、正常时间录像回放、语音对讲相关实现。对讲的控制连接可由预览指令触发，但上下行处理线程在当前 `main.cpp` 中被 `#if 0` 关闭。 |
| `src/videoRecord/`、`src/videoRecordTimeErr/` | 正常录像、短视频、录像序号与空间清理；系统时间异常时的单独录像与配额管理。 |
| `src/udp/` | 按通道接收 UDP 控制消息，向 `jtt_808_1078` 发送回复；另含语音通道与 QT 交互的 UDP 实现。 |
| `src/alarm/` | 监测外部电源、SD 卡、时间、容量及录像读写状态，发布事件。 |
| `src/config/`、`src/log/` | INI 配置解析与日志封装。 |
| `docs/` | 告警、TF 卡录像优化及时间异常录像设计说明，修改对应逻辑时可一并核对。 |

数据主链路：上游视频进程 → 每通道共享内存 → `thread_1078_stream` → 实时预览发送与本地录像。回放从已存 MP4 文件读取；控制指令走 UDP，实时或回放流走到平台指定地址的 TCP 连接。

## 构建与部署

- 本项目由 `skes` 仓库顶层 CMake 统一构建；本目录要求 CMake 3.10 及以上、C++11，并查找 `ffmpeg-git`、`cjson-git`、`nanomsg-git`。链接 `pthread`、`swresample`、`avcodec`、`avformat`、`avutil`、`m`、`cjson`、`nanomsg`。具体交叉编译环境以仓库 `cmake/README.md` 和目标板工具链为准。
- 本目录 CMake 将 `CMAKE_INSTALL_PREFIX` 设为 `.`，安装规则把可执行文件与 `sep.ini` 放到 `apps/aibox_stream`、依赖动态库放到 `lib`；最终设备上的绝对路径还取决于安装调用及打包目录。`sep.ini` 当前为 `enabled=true`、`param=f 8`，对应父进程拉起 8 个通道。
- `TPAD_EXT_PWR_GPIO` 是 CMake 缓存参数，默认 119；目标设备若使用其他供电检测引脚，配置构建时需传入实际编号。

## 运行与修改时关注

- 子进程按通道索引读取 `/root/cameras/camera_config.json` 中的 `channel`、`device`、`width`、`height`。`device` 形如 `/dev/videoN` 时，共享内存 key 为 `0x8100 | (N & 0x0FFF)`，与上述生产实现的 H.264 区 key 对应；无法解析设备号时本程序退到 `0x8FFF`，而上述生产实现的回退 key 加 `0x100`，两者不匹配。配置缺失、设备路径异常或共享内存不存在时，通道无法正常取流。
- `/opt/conf.ini` 提供 `dev:id` 和 `dev:secret`。程序会在可执行文件同级的 `appVersion/aibox_stream.ini` 写入编译版本和时间；部署目录需允许写入。录像入口要求设备 ID 长度至少为 3，读取 ID 失败会阻止写录像。
- 正常录像目录为 `/mnt/sdcard/videoRecord/YYYYMMDD/通道号/`，按墙上时间的 5 分钟边界换片；异常时间录像目录为 `/mnt/sdcard/videoRecordTimeErr/通道号/`，按单调时间每 300 秒换片。正常录像和异常时间录像共用每通道递增序号，后者不以错误的系统日期命名。告警短视频位于正常录像日期目录下的 `shortVideo/`。
- `video_record.h` 将视频通道上限设为 8，以 SD 卡容量的 75% 为正常录像占用预算，并据配置参数推导提前清理旧目录的阈值；系统时间有效年份为 2026，SD 卡总使用率达到 95% 时停止录像。`video_record_timeerr.h` 定义异常时间录像的 10% 空间上限和 9% 清理阈值。两种录像均在真实 IDR 关键帧上开片。当前回放检索只扫描正常录像日期目录，不能直接检索 `videoRecordTimeErr` 文件。调整配额或文件名时应同步核对回放和清理逻辑。
- 每通道 UDP 监听端口是 `1210 + 通道号`（绑定所有本机接口），给 `jtt_808_1078` 的回复端口是 `127.0.0.1:1236`。当前 `USE_TALKBACK=1`，语音通道为索引 7；该通道还监听 UDP 2402，向本机 UDP 2401 发 QT 控制消息。`SAVE_AUDIO_FILE=0`，独立 G.711 音频文件写入逻辑未启用；对讲上下行线程也未启动。
- 告警模块以 nanomsg `PUB` 连接 `tcp://127.0.0.1:19225`，发布 `/skes/event`。它每秒检查一次，首次、状态变化或约 4 秒心跳时发送。外部供电缺失参与顶层 `status` 的 `malfunction` 判断，**但当前代码明确跳过外部供电的逐项告警**；逐项明细只含 SD 卡缺失、时间异常、容量不足和录像读写错误。录像本身仍因供电异常停止。
- 录像逻辑还会检测 SD 卡挂载与 GPIO 供电状态；确认存储 I/O 故障并进入 `sdcard_halted` 状态时会写 `/tmp/sdcard_halted_ch<通道号>.txt`。在普通开发机上运行需具备相应设备、共享内存和挂载环境。
- `src/main.cpp` 的父进程模式为 `aibox_stream f 8`，单通道模式为 `aibox_stream c <通道号>`；父进程定期检查子进程并重启退出的通道。修改通道数量时，应同时核对 `VIDEO_CHANNELS`、摄像头配置和 `sep.ini`。
- 当前目录没有独立的自动化测试目标；功能验证需要目标板的视频生产进程、存储卡、1078 对端以及告警订阅端配合。
