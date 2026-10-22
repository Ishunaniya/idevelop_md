# 板端取帧测试

`tools/v4l2_detect` 的采集层只通过 `FrameSource::grab()` 交一帧紧凑 NV12。来源可以是实时相机、MP4 关键帧、共享内存，或已经采好的裸帧。推理侧不区分这四种。

本文是 RK3576 板子 `192.168.11.1`（用户 `root`）上的测试步骤。任务文件是 `/root/evs/eval_yolo11.yaml`（模型 `/root/cameras/yolo11.rknn`）。

2026-10-11 的交叉编译产物在编译机 `edgevision/build_rga/`，还没有推到板子。板上已有的 `/root/evs/v4l2_detect_mp4` 是更早的程序。第 0 节说明这次怎么拷、怎么验。第 1 节起的命令把程序名换成新文件即可。不要替换开机脚本用的 `v4l2_detect_evs_rga2`。

---

## 0. 2026-10-11 交叉编译

工具链是 `aarch64-linux-gnu-g++`，后端 RKNN，打开 RGA 和 GStreamer。两份程序都是 aarch64 ELF：

| 编译机路径 | 大小 | BuildID | 测什么 |
|---|---|---|---|
| `build_rga/v4l2_detect`（与 `v4l2_detect_mp4` 同一份） | 674K | `518d60c2a60dc7bf6716c10ab7c81f9341501a33` | 取帧、推理、解码 |
| `build_rga/evs_tests` | 737K | `74e61ec61fcf35fb8030b9eadfc92c8dd7db1037` | 功能插件和人员告警 |

拷到板子时用新文件名，不覆盖旧程序，也不覆盖守护进程：

```bash
scp build_rga/v4l2_detect root@192.168.11.1:/root/evs/v4l2_detect_plugin
scp build_rga/evs_tests    root@192.168.11.1:/root/evs/evs_tests
```

板上确认是这一次：

```bash
readelf -n /root/evs/v4l2_detect_plugin | grep -A1 'Build ID'
# 518d60c2a60dc7bf6716c10ab7c81f9341501a33
```

### 0.1 功能插件

`v4l2_detect` 只做到解码，不调用 `VisionTask`。实时相机和 MP4 看不出人员告警、高度限位、卷扬冰雪。这三项在 `evs_tests` 里。编译机上的 mock 构建已经是 `PASS=203 FAIL=0`；板端再跑同一套单测，确认 aarch64 上行为一致。

```bash
cd /tmp
/root/evs/evs_tests
```

预期末行 `FAIL=0`。x86/ONNX 那段在这块板上会跳过，所以 `PASS` 会少于 203。单测走 mock 后端，不加载 `/root/cameras/yolo11.rknn`，也不占相机。

人员策略现在会随 `updateConfig` 在下一帧生效，连续帧计数不会被这次更新清掉。高度限位 `0x0007` 的安装姿态仍是骨架；卷扬冰雪 `0x0008` 可使用 [固定 ROI 图像粗估](winch_ice.md)，需 OpenCV、单路相机和实际表面标定。

### 0.2 取帧推理

下面第 1、2 节把 `./v4l2_detect_mp4` 换成 `./v4l2_detect_plugin`。实时相机预期仍是 `[gst] capturing /dev/video0 1280x720 @25fps` 和 `pre[rga]`。竖屏 `/root/screen.mp4` 的 letterbox 偏移 360 不是 16 字节对齐时，会退回 CPU，检测继续。

先确认没有别的检测进程占着 NPU：

```bash
pgrep -a v4l2 || true
cd /root/evs
```

没有输出就可以按下面短跑。`--frames N` 跑完即退出。`--daemon` 会一直占着相机和 NPU，测完用 `SIGTERM` 停掉。

---

## 1. 实时相机

GStreamer 管线：`v4l2src → NV12 → appsink`。配置在 `/root/evs/camera_config.json`，通道 0 是 `/dev/video0`，1280×720@25。

```bash
./v4l2_detect_mp4 --task eval_yolo11.yaml --config camera_config.json \
  --channel 0 --frames 5 --rga
```

预期：

- `[gst] capturing /dev/video0 1280x720 @25fps`
- 每帧 `pre[rga]`，推理大约 75 ms

一直跑，并把框写给界面（`/dev/shm/detection_boxes` 通道 0）：

```bash
./v4l2_detect_mp4 --task eval_yolo11.yaml --config camera_config.json \
  --channel 0 --daemon --stats --shm-write 0 --rga
```

`--daemon` 时采集和推理分成两个线程，帧经过进程内三槽邮箱。GStreamer 起不来时会退回直接 V4L2，日志里有 `falling back to legacy V4L2`。

---

## 2. MP4 关键帧

只把同步帧送进解码器，非关键帧在解码前丢掉。文件播完跳回开头。

板上已有 `/root/screen.mp4`（H.264，1200×1920）：

```bash
./v4l2_detect_mp4 --task eval_yolo11.yaml --mp4 /root/screen.mp4 \
  --frames 5 --score 0.25
```

预期：

```text
[mp4] video video/x-h264, keyframe filter on parser
[mp4] /root/screen.mp4 1200x1920 keyframes only, loop
frame 0: 1200x1920 -> model 640x640, ...
```

2026-10-09 用这支程序实测 2 帧：模型加载成功，两帧都完成推理。这路分辨率的 letterbox 偏移不是 16 字节对齐，日志为

```text
[rga] falling back to CPU: letterbox offset 360 is not 16-byte aligned
```

检测仍走 CPU 预处理。1280×720 的相机画面不受这一条影响，继续走 RGA。

换别的 MP4 时只改 `--mp4` 路径。需要 `h264parse` 或 `h265parse`，以及 `decodebin`、`videoconvert`。

---

## 3. 共享内存

读界面进程写好的 `/picshareN`（BGR，尾部 32 字节是帧号和宽高），在采集层转成 NV12。`/dev/shm` 里现有 `picshare2`、`picshare3`。界面没在写时，读到的是旧帧或打开失败。

```bash
./v4l2_detect_mp4 --task eval_yolo11.yaml --shm 2 \
  --width 1280 --height 720 --frames 5 --rga
```

预期：`[shm] consuming /picshare2`。实际宽高以共享内存尾部元数据为准，`--width` / `--height` 只在元数据还没就绪时作打开参数。

这和 `/dev/shm/detection_boxes` 不是一块内存。`picshare` 是输入图像，`detection_boxes` 是推理之后写出的框。

---

## 4. 已经采好的 NV12

| 文件 | 尺寸 | 字节数 |
|---|---|---|
| `/root/evs/cap.nv12` | 1280×720 | 1382400 |
| `/root/evs/bus.nv12` | 640×640 | 614400 |

```bash
./v4l2_detect_mp4 --task eval_yolo11.yaml --raw cap.nv12 \
  --raw-width 1280 --raw-height 720 --frames 1 --rga
```

640×640 的裸帧把宽高都改成 640。预期：`[raw] cap.nv12 1280x720 NV12`。

同帧对比 RGA 和 CPU：

```bash
./v4l2_detect_mp4 --task eval_yolo11.yaml --raw cap.nv12 \
  --raw-width 1280 --raw-height 720 --frames 1 --bench
```

---

## 5. 怎么判断走的是哪条入口

| 日志 | 入口 |
|---|---|
| `[gst] capturing` | 实时管线 |
| `falling back to legacy V4L2` | 实时管线失败，退回 V4L2 |
| `[mp4] ... keyframe filter` | MP4 关键帧 |
| `[shm] consuming /picshare` | 共享内存 |
| `[raw]` | 裸 NV12 文件 |

四种入口最后都打印同一行结果：`frame N: WxH -> model 640x640, targets=K`。这一行只说明推理跑完，不代表走了 `VisionTask` 的人员告警。人员、高度限位、卷扬冰雪用第 0.1 节的 `evs_tests`。

卷扬冰雪现有独立 `winch_ice_demo`（实时摄像头）与 `ice_replay`（图片/人工框序列），不再依赖v4l2_detect展示面积或会话结果。取图、工况模拟与真实工况接入、Ubuntu板编译步骤见 [冰雪完整说明](winch_ice.md)。
