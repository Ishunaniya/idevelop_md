# 172.20.10.9：evs 目录与 Bash 历史核验

附件说明：原始核验证据共 32 个文件，已收纳到 [证据附件.zip](证据附件.zip)。下文中的证据路径均指压缩包内路径；需要逐项查看时，将压缩包解压到本目录即可。历史命令逐行解释仍可直接打开。

核验日期：2026-10-19。使用用户指定的 `hytop` 账户，通过 SSH 读取设备。
初始目录快照时间为设备时间 **09:11:31 +0800**，最终内容复核为 **09:19:21 +0800**。
没有启动 UI、摄像头、RKNN 推理、测试程序或重放历史命令；没有修改设备应用、配置、模型或历史文件。

后续验证目录（2026-10-19 更新）：用户已将 `/home/hytop/evs` 的内容复制到
**`/home/hytop/l_evs`**，今后使用新目录验证程序。复制关系来自用户说明，本轮未重新核对新目录的文件与哈希。
本文的设备清单、二进制结论与附件仍对应原目录的检查快照；历史路径保留原值。
后续在设备上执行验证命令前，先进入新工作目录：

```bash
cd /home/hytop/l_evs
```

结论：`/home/hytop/evs` 是一套混合了多代部署程序、分库插件、模型试验和样图的调试目录。
核验时用于对照的是本机 `/home/tronlong/lyp/code/edgevision` 的单库基线
`b69bfdb2e76efe93585983d2c714d270be96724c`，不能把设备产物认定为该提交的构建结果。
后续本机已切换到 `feature/split-core-plugin-libs`，基线为
`137a845454d537085d28ab7b021a81581f545bc5`，并恢复了冰雪功能的未提交修改。
新分支也采用核心库与独立功能库；库名相同仍不能证明与本次设备快照的实现或 ABI 一致。
板端冰雪 UI 链路走的是 **mock 后端 + OpenCV SnowTracker 图像规则**；独立的 `ice_rk3588.rknn`
试验记录则是 **六分类输出**。两者都不是已经核验完成的“模型矩形框输入面积后处理”链路。

先看这四点即可理解本次发现：

- **设备目录放了什么**：多代程序、模型、配置和样图混放，不能整体视为同一版本的发布包。
- **冰雪 UI 如何判断**：静态证据指向图像颜色、梯度和掩码规则，输出候选区域比例；没有证据证明它使用模型矩形框计算覆盖率。
- **为什么配置 30% 不等于实际 30% 报警**：当前设备插件采用平滑后的 12% 进入、6% 解除阈值；0.3 同时出现在平滑系数中，不能混淆。
- **现有模型能否直接提供矩形框**：已有冰雪试验日志给出 6 个分类分数，不能直接提供矩形框或面积占比。

阅读顺序：了解文件用途看第 2 节；理解冰雪逻辑看第 3～4 节；理解历史操作先看第 6 节的范围表，
再按行号查 [历史命令逐行解释](history_annotated.md)。原始输出只在需要复查时解压附件。
本文的“静态核验”指读取文件、依赖、符号和反汇编；“历史记录”指已有命令或日志，均不等于本次重新运行程序验证。

## 1. 设备与检查范围

| 项目 | 实际读取结果 |
|---|---|
| 设备名 | smartdisplayer |
| 设备树 model | HYTOP RK3588 Board |
| compatible | hytop,rk3588 / rockchip,rk3588 |
| 系统 | Ubuntu 22.04.5 LTS，aarch64 |
| 内核 | 5.10.209-rk3588 |
| 当前账户 | hytop，uid=1000，包含 video、sudo 等组；本次没有使用 sudo |
| OpenCV pkg-config 版本 | 4.5.4，插件依赖 SONAME 为 .so.4.5d |
| 当前 librknnrt 内嵌版本字符串 | 2.3.0 (c949ad889d@2024-11-07T11:35:33)；未加载模型运行验证 |
| 根文件系统 | 29G，总体已用 19G、可用 8.4G，约 70% |
| 目录内容 | 150 个普通文件、4 个子目录、1 个符号链接；普通文件合计 149,500,935 字节 |
| 当前 EVS 进程 | 两次快照未发现 evs_ice / evs_task / evs_detect / evs_high / v4l2_detect / ice_infer 等应用进程 |

完整清单及 SHA-256 见 `files.tsv`。前后比较，150 个普通文件的内容哈希全部相同。
这是本次检查窗口内的内容一致性证据，不是模型正确性或程序运行结果的保证。

## 2. 目录里的文件分别做什么

| 类别 | 文件与用途 |
|---|---|
| 旧一体库 | `libevs.so`，`libevs.so.1 -> libevs.so`；包含旧版 WinchIcePlugin 和其他功能符号 |
| 新核心库 | `libevs_core.so.1`；包含 VisionTask、RKNN 后端和插件注册/创建接口 |
| 冰雪插件 | `libevs_ice.so.1`；包含 WinchIcePlugin、SnowTracker、snow_detector.cpp 的编译痕迹；依赖核心库和 OpenCV |
| 高度插件 | `libevs_height_limit.so.1`；包含 HeightLimitPlugin 和 judge_switch_mount，依赖核心库 |
| 冰雪程序 | `evs_ice_cam` 负责采集/调用功能，`evs_ice_ui` 是 Qt Quick 界面 |
| 人员检测程序 | `evs_task_cam`、`evs_detect_ui`、`user_evs_demo`；不应与冰雪 UI 混用为同一条验证链路 |
| 高度示例 | `evs_high_demo`，链接核心库和高度插件，命令字符串显示可按 `height_limit.yaml` 批量处理图片目录 |
| 检测工具历史版本 | `v4l2_detect`、`_sdk` 相关评测程序、`v4l2_detect_rga/new/wstride/yolov5raw/merged/evs/evs_rga/evs_rga2/mp4` 及 .bak/.old 对照版本；文件名不能单独证明哪个是最终版本 |
| 测试与评测 | `evs_tests`、`eval_offline`、`eval_offline_sdk`、annotations JSON、`eval_onnx_demo/images/bus.ppm` |
| 模型 | 6 个 RKNN 文件，具体见下表 |
| 配置 | 9 份 YAML、`camera_config.json`，包含旧板型/旧 root 路径记录 |
| 图像/视频 | 78 个 JPG、5 个 PNG、3 个 PPM、2 个 NV12、1 个 MP4，含重复图和历史渲染输出，不是统一标注验证集 |
| 冰雪模型探测源码 | `ice_infer_demo.cpp`、`ice_infer_demo.py`、`probe_ice.py`；目录中可见的 cpp 只有该演示文件，并非完整核心/插件源码 |
| 辅助脚本 | `ppm2nv12.py`、`evs_clean.sh`、`health_check.sh` |
| 历史输出 | `run0.txt`、`run1.txt`、`r_raw.txt`、`r_u8.txt`、`r_im.txt`、`probe_out.txt`、`brun3.log` |
| 操作说明 | `README_switch.md`，主要针对旧 RK3576、旧 /root 路径，不能直接当作本机位的当前操作手册 |

普通 ELF 文件共 29 个，其中不少旧工具是 `-rw-rw-r--`，当前没有执行位。
例如 `evs_tests`、`eval_offline` 和多个旧 v4l2 工具不能直接用 `./文件名` 启动。
这不表示它们损坏，也不影响普通共享库读取；本次没有为了运行它们修改权限。

模型核对：

| 模型 | 字节数 | 可确认的用途/限制 |
|---|---:|---|
| `yolox.rknn` | 10,565,689 | eval_yolox.yaml 当前引用的相对模型文件 |
| `yolov5s-640-640.rknn` | 8,357,380 | eval_rknn_board.yaml 指向该设备上的绝对路径；配置注释称 RK3588 版本，未重新查询模型 |
| `yolov5s-640-640_rk3576.rknn` | 8,229,506 | 文件名/旧配置指向 RK3576 路径；mtime 为 1970 年，不能用作部署时间证据 |
| `ice_rk3588.rknn` | 2,017,789 | 现存源码和日志显示 224×224 输入、6 个分类分数；未重新加载推理 |
| `high_rk3588.rknn` | 4,910,095 | height_limit.yaml 引用 |
| `hight_rk3588.rknn` | 4,910,095 | 与 high_rk3588.rknn 的 SHA-256 完全一致，只是多一个 t 的重复副本 |

其他已确认的相同内容：

- `ice_test1.jpg` 与 `test_pics/img_v3_0214i_bcfb0319-9f3a-4947-ba6b-890a0bd1176m.jpg` 相同。
- `v4l2_detect_evs_rga2` 与 `v4l2_detect_mp4.old` 相同。
- `test_pic/AC0220CD0042_1_5_250123_160822_0002_051824_ff.jpg` 与 `...250217_125058_0002_043953_ff.jpg` 相同。

这些只是内容去重事实，本次未清理或删除任何副本。

## 3. 板端冰雪插件和 UI 的实际关系

通过 ELF DT_NEEDED、RUNPATH、导出符号、UTF-16 字符串和有限函数反汇编，可确认以下静态连接：

```text
evs_ice_ui（Qt Quick，创建 QProcess）
  → evs_ice_cam --task winch_ice.yaml --config camera_config.json --channel 0
  → libevs_core.so.1：VisionTask
  → libevs_ice.so.1：WinchIcePlugin::decide
  → SnowTracker::update(Frame)
  → snow_coverage：OpenCV 图像颜色/梯度/掩码规则
  → 比例、报警状态及候选包围框 → UI
```

`evs_ice_cam` 的 ELF 直接依赖核心库和冰雪库，二者使用 `$ORIGIN` RUNPATH。
冰雪库有模块静态初始化和对 `evs::register_function_plugin` 的引用，核心库导出注册/创建接口。
这是**独立共享库中的场景插件，加载后向工厂登记**。本次没有发现支持热插拔的独立插件发现接口，
不能仅因有 .so 就推导出 dlopen 动态发现、任意版本混用或运行中换插件。

这里描述的是本次读取的设备二进制。此前“插件编进 libevs”的说明只适用于本机旧单库基线；
后续 `feature/split-core-plugin-libs` 已改为核心库和独立功能库。
仍需拿到板端对应源码、头文件及构建配置，才能准确对齐 ABI（宿主与库之间的二进制接口）和版本，
不能仅凭新分支也拆了库就认定可以直接替换设备文件。

UI 中的实际启动命令字符串为：

```text
exec ./evs_ice_cam --task winch_ice.yaml --config camera_config.json --channel 0 --frames 999999 --save /tmp/evs_ice_shot.ppm
```

界面还内嵌停止旧 `evs_ice_cam`、清理旧 PPM 的命令，并解析 `ice_ratio=... alarm=... box=...` 输出。
因此启动该 UI 本身会启动采集子进程、结束旧同名采集进程并写入临时图像；本次只读出了字符串，没有执行。

`winch_ice.yaml` 当前关键配置：

```yaml
module_id: 0x0008
backend: mock
Model_path: models/winch_ice.rknn
area_ratio: 0.3
exit_on_alarm: false
```

`/home/hytop/evs/models/winch_ice.rknn` 实际不存在，但 mock 后端不是正常的真实模型加载路径。
当前 UI 命令没有指向 `ice_rk3588.rknn`；不能把那个模型文件存在视为 UI 正在使用它的证明。

### 3.1 面积与报警的关键不一致

对当前 `libevs_ice.so.1` 的静态核验发现：

- `WinchIcePlugin::configure` 保存了 area_ratio 和 exit_on_alarm。
- `decide` 调用 `SnowTracker::update(Frame)`，没有消费传入的模型 `targets` 来计算面积。
- `SnowTracker` 调用颜色转换、S/V 通道比较、Sobel、blur、掩码处理、形态学、countNonZero 和 boundingRect。
- 输出 `box` 来自候选掩码的 boundingRect；这不是“模型输入了一个矩形框”的证据。
- `decide` 使用 tracker 返回的布尔状态报警，没有读取保存的 area_ratio。

通过 `snow_config()::cfg` 常量和 update 的比较分支，可以确定本二进制的策略：

```text
上次保存的平滑值 < 0：平滑比例 = 当前图像规则比例
否则：平滑比例 = 0.3 × 当前比例 + 0.7 × 上次平滑比例
平滑比例 >= 0.12：进入有雪状态
平滑比例 <= 0.06：解除有雪状态
中间区间：保留前一状态
```

所以，**YAML 中的 area_ratio=0.3 不是该板端插件当前实际采用的报警比例阈值**。
这里的 0.3 是常量中的平滑系数，进入/解除状态的阈值是 0.12/0.06。
`exit_on_alarm:false` 则使进入状态时可以持续给出报警结果。

证据地址：configure 为 `0x7814`，decide 为 `0x787c`；update 为 `0xa08c`。
常量 cfg 位于 `.rodata:0xb120`，update 使用的 +84/+88/+92 偏移分别是 0.12/0.06/0.3。
详细静态输出保存在 `elf.json` 和 `detail.json`。这些结论没有冒充实际图像回放或板端精度验证。

另一个边界：frame_bgr 失败时 update 走原始比例为 0 的路径，decide 最终仍返回 0。
这与本机新实现“异常为 -1 且返回错误”的语义不同，不能把两个版本的异常行为混为一谈。

## 4. ice_rk3588.rknn 是另一条六分类试验链路

目录中的 `snapshot/ice_infer_demo.cpp`、`snapshot/ice_infer_demo.py`
和 `snapshot/probe_ice.py` 都单独加载 ice_rk3588.rknn。现存日志输出形状为 `[6]`，
展示的是 6 个分类分数/概率，没有矩形框坐标，也没有分割掩码。

C++ 演示给出的默认标签为 dry_road、fully_snowy、icy、snow_blowing、snow_melting、wet_road。
这些是 demo 源码写入的默认名字；没有训练数据类别顺序文件佐证，不能据此确认模型类别顺序正确。
某一类置信度 0.998 也不代表“冰雪覆盖率 99.8%”，分类置信度与表面面积占比不同。

现存试验记录的限制：

- probe_out.txt 有 `current input size(602112) > need input size(150528)`，不能当作全部输入协议已经验证正确。
- r_raw/r_u8/r_im 日志显示不同归一化尝试，且使用 bus_box.png 等示例；日志带 PNG CRC 警告。
- run0/run1 的早期记录对近似和为 1 的输出又做了 softmax，不能把其结果作为校准后的可靠概率；当前 Python 源码已不再做这一步，旧日志与当前源码不能混为同一次运行。
- C++ demo 先按 NHWC 顺序读取 dims，之后才按 in_attr.fmt 分支；若运行时输入属性为 NCHW，这个尺寸读取方式不正确。实际运行时属性本轮没有查询，不能把框架布局日志直接当作该属性值。

因此，后续若需求是“模型矩形框输入”，仍需确认真正的检测模型及框类别/格式；现有六分类试验文件不能直接提供所需矩形框。

## 5. 配置、路径和旧说明的差异

后续使用 `/home/hytop/l_evs` 时，还需核对新目录副本中的绝对路径：复制目录、改变工作目录不会自动改写 YAML 或脚本里的路径。
例如本次快照的 `eval_rknn_board.yaml` 仍显式引用 `/home/hytop/evs/yolov5s-640-640.rknn`，
部分旧脚本使用 `/root/evs`；若继续沿用，可能仍访问旧目录。调整应针对新验证目录的副本，附件中的历史证据保持原样。

- camera_config.json 当前启用 4 路，通道 0/1/2/3 分别是 `/dev/video0`、`/dev/video3`、`/dev/video13`、`/dev/video14`，配置均为 **1920×1080、25 fps**。
- README_switch.md 写的是旧 RK3576、旧 IP、`/root/evs`、1280×720，并声称某些性能数字；这些只能作为历史说明，不是本次 RK3588 的运行结果。
- eval_yolox.yaml 的相对 `yolox.rknn`、height_limit.yaml 的 `high_rk3588.rknn` 在本目录存在且 hytop 可读。
- eval_rknn_board.yaml 使用 `model:` 别名及 `/home/hytop/evs/yolov5s-640-640.rknn`，该文件存在；不能遗漏 model 别名后误报模型缺失。
- 其他旧 YAML 有 `/root/cameras/yolo11.rknn`、`/root/cameras/yolov5s-640-640_rk3576.rknn`。hytop 查询得到 Permission denied，**不是确认文件不存在**。
- health_check.sh 会切换 `/root/evs`、设置摄像头格式、采集文件并启动推理；它不是只读健康查询，不能照着本次检查范围直接执行。
- evs_clean.sh 会循环创建软链接并删除较早的 PPM；本次未启动。
- 不同 libevs、core、插件与工具文件有不同 mtime 和 BuildID，且无本次可核对的完整板端源码版本号，不能把它们认定为同一提交的完整发布包。

## 6. history 记录的操作是什么

读取的是 **hytop 的 `/home/hytop/.bash_history`**，不是 root 的历史，也不是其他正在运行 shell 的内存历史。
可见文件有 **1694 行**，mtime 为 **2026-10-16 11:52:42 +0800**。
当前仍有其他 Bash/图形终端会话，尚未落盘的命令不在这份记录里；非交互 SSH/SFTP 操作也不一定被 Bash history 记录。
本次没有执行 `history -a/-w` 强制写回，也没有读取其他账户的私有历史。

`.bashrc` 配置 HISTCONTROL=ignoreboth、HISTSIZE=1000、HISTFILESIZE=2000，并启用追加历史。
以空格开头的命令、部分重复命令可能不保存。文件没有 `#Unix时间戳`，因此下面只能按文件行号解释，
不能把行号当作准确时间线；history 不记录退出码，也不能据命令推断其执行成功。
第 **961 行含 706 个 NUL 空字节**及残留命令文本，不能确认损坏原因，也不能据此宣称存在恶意操作。

| 文件行号范围 | 主要意图 |
|---|---|
| 1～239 | ROS2/Qt 显示工程编译、启动 ros2_qt_display；触摸设备/goodix/evtest 查询 |
| 240～329 | 编译和进程处理；交换空间设置；备份/追加 fstab；重启；构建脚本改名 |
| 330～603 | 硬件自检、内存/磁盘查询、CPU 调频与 stress 压测、旧目录清理、ROS2 包调整 |
| 604～789 | crane_safety 库编译、手工 g++ 链接、GDB 调试，逐步补 RKNN/RGA/OpenCV 依赖 |
| 790～960 | swapfile 删除尝试、告警系统/数据库调试、库调试和关机尝试 |
| 961～1133 | 异常历史行；RKNN 运行库/头文件定位、系统库安装/替换尝试、rknn_server 启动脚本处理、CMake 依赖排障 |
| 1134～1399 | hook_config_rec 的 model_service、实现、前后处理及类型修改，反复清理/配置/编译/复制测试库 |
| 1400～1607 | hook_overwind 工程和测试目录整理、编译、复制库、执行 test_hook_overwind，多轮修改调试 |
| 1608～1668 | evs 目录清理、摄像头格式/占用排障、YOLOX 摄像头测试、Qt 人员 UI 和桌面录屏 |
| 1669～1694 | 查看冰雪模型文件、打包 evs、查询视频协商、赋予执行权限并启动冰雪 UI |

常见构建命令的含义：

- `cmake ..` 生成构建文件，`make` 编译；反复出现的 `rm -r *` 意图清理当时目录。没有当时的输出/初始目录，不能可靠断言每次删的是哪一目录。
- `g++ ... -lcrane_safety -lrknnrt -lrga ...` 是手工链接库的排障过程，`LD_LIBRARY_PATH` 用于运行期查找共享库。
- `vim model_service.cpp / hook_config_detector_impl.cpp / postprocess.cpp ...` 表示打开源码编辑/查看，无法从历史重建具体改动。
- 历史包含 `cmmake`、`touh` 等试探或拼写错误；不能把所有记录视为成功执行的操作。

最近 EVS 记录逐项说明：

| 行号 | 操作意图 |
|---:|---|
| 1623、1625 | 删除 evs 旧匹配文件/输出目录；没有输出证实删除结果 |
| 1628 | media-ctl 查看媒体拓扑 |
| 1631～1633 | 尝试修改 root 密码和切换用户；不显示密码，也不能确认成功 |
| 1637、1640、1642～1645、1648 | 用 eval_yolox.yaml、camera_config.json、channel 0 跑 5/30 帧摄像头功能测试 |
| 1638～1639 | 查看 /dev/video0 支持格式和占用进程 |
| 1646～1647 | 调试 evs_task_cam；下一条改用正确的 gdb --args 形式传目标参数 |
| 1649～1655、1662～1668 | 给 Qt UI 执行权限，设置 DISPLAY=:0 并运行；exec 会替换当前 shell |
| 1656～1657 | 尝试用 FFmpeg 录制 X11 屏幕，分别尝试 libx264 和 h264_rkmpp 编码 |
| 1661 | 尝试 GStreamer ximagesrc 桌面录屏，写入 /home/hytop/uitest/screen.mp4 |
| 1672 | 仅列出 ice_rk3588.rknn，不是执行模型 |
| 1676 | 尝试把 evs 打包为 evs_onboard.tar.gz；归档路径在上级当时目录，不能由当前 evs 清单确认打包成功 |
| 1679～1680 | 查询摄像头宽高及 GStreamer 协商结果；后一条会短暂打开相机，但不代表已经跑冰雪模型 |
| 1682～1687、1694 | 启动 evs_ice_ui，并给 UI/evs_ice_cam 添加执行权限 |

逐行解释覆盖全部 1694 行，见 [history_annotated.md](history_annotated.md)；脱敏原始行列表见 `history_redacted.txt`。

历史中能看到冰雪 UI 的启动，却没有足够命令和输出证明其使用六分类模型完成了覆盖率算法；
这与现有 mock YAML、SnowTracker 二进制调用链相吻合。

## 7. 本次结论的边界与证据文件

已完成：设备身份、目录全清单和内容哈希、全部小文本、可见 Bash 历史、重点 ELF 依赖与符号、
冰雪插件关键调用与常量、UI 内嵌启动命令，以及检查结束时的内容一致性复核。

没有把历史命令当作成功执行记录；没有宣称实际 UI/NPU/相机本次运行通过；没有把分类置信度、
候选包围框或图像规则比例当作已验证的模型矩形框覆盖率。

证据已归档到本机 `/home/tronlong/lyp/perCode/idevelop_md/edgevision/device_audit_172.20.10.9_20261019/`，没有上传或更改设备：

- `files.tsv`：完整文件清单、权限、大小、mtime、SHA-256。
- `inventory.json`：初始目录、脱敏文本、账户可见历史与系统快照。
- `elf.json`：重点二进制的架构、依赖、符号、字符串、关键函数反汇编。
- `detail.json`：UI UTF-16 启动命令、SnowTracker 常量与面积函数、模型路径访问结果。
- `verify.json`：最终哈希与进程复核。
- `snapshot/`：读取到的小文本文件副本，便于逐行查看；不是完整源码仓库。
- [history_annotated.md](history_annotated.md)：所有历史行的命令意图解释。

后续要对接模型框方案，应先确认期望对齐的是板端这套 core+插件 ABI，还是本机源码分支，
再提供实际检测模型的类别和输出协议。当前这两个实现及六分类演示不能直接互相替代。
