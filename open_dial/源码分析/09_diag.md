# 09 — diag/diag.c + diag/diag.h

## 一、文件职责概述

故障诊断数据采集模块，自 `dial.c` 剥离（diag.c:4-6）。三件事：
1. **故障/恢复快照** `diag_snapshot`：一次性抓 `dmesg` + `logcat -d` 到 SD 卡。
2. **CP dump 捕获** `setup_cp_dump_capture`：把 `/media/sdcard` bind mount 到 `/sdcard`，使 EC200A(ASR1803 OpenCPU) CP 崩溃时内核 EEH 写入 `/sdcard/modem_dump/` 的 dump 不随重启丢失。
3. **解除挂载** `teardown_cp_dump_capture`。

不触碰 dial 运行期状态，纯旁路采集。

## 二、接口（diag.h）
| 函数 | 声明位置 | 调用方（实测） |
|---|---|---|
| `void diag_snapshot(const char *label)` | diag.h:10 | dial.c:634 `diag_snapshot("recovery")`、dial.c:707 `diag_snapshot("fault")`，**均经 `has_connected_once` 门控** |
| `void setup_cp_dump_capture(void)` | diag.h:11 | main.c:342（启动时一次） |
| `void teardown_cp_dump_capture(void)` | diag.h:12 | dial.c:438 |

## 三、逐函数分析

### `void diag_snapshot(const char *label)`（diag.c:22-58）
- 前置：`access("/media/sdcard", F_OK)!=0` 直接 return（无 SD 卡不采集，diag.c:24）。
- 建目录：先 `mkdir("/media/sdcard/dial_log")`（补父目录，diag.c:29），再建 `dial_log/dial_snap`；`mkdir` 失败且非 `EEXIST` → 打日志并 return（diag.c:31-34）。
- **写前清理**：`cleanup_dir_keep_newest(diag_dir, 40)`（misc 提供），保留最新 40 个（约 10 个故障周期，diag.c:37）。
- 时间戳 `%Y%m%d_%H%M%S`（localtime_r，diag.c:39-43）。
- 采集：`system("dmesg > .../dmesg_<label>_<ts>.log")`（内核 ring buffer 全量，diag.c:48-50）；`system("logcat -d -v time > .../logcat_<label>_<ts>.log")`（`-d` 导出后退出不阻塞，diag.c:53-55）。
- 收尾打 `[DIAG] Snapshot saved: ...`（diag.c:57）。
- 注释称单次 ~300KB（diag.c:20）。

### `void setup_cp_dump_capture(void)`（diag.c:67-128）
- `access("/media/sdcard")` 失败 → 打日志 return（diag.c:69-72）。
- **判定是否已挂载**：不是看 `/sdcard` 有无挂载（overlayfs 可能已挂 `/sdcard`），而是比较 `/sdcard` 与 `/media/sdcard` 的 `st_dev`，相同才算同一块卡（diag.c:77-82）。这是关键正确性设计。
- 未挂载则 `mount("/media/sdcard","/sdcard",NULL,MS_BIND,NULL)`（diag.c:89）——**用 `mount(2)` 系统调用而非 `system("mount --bind")`**，注释说明是为避免本进程 `SIGCHLD` 处理干扰 `system()` 内部 `waitpid()`（diag.c:87-88）。失败打日志 return；成功置 `g_cp_dump_mounted=1`。
- `mkdir("/sdcard/modem_dump")`（非 EEXIST 报错，diag.c:98-100）。
- `cleanup_dir_keep_newest("/sdcard/modem_dump", 10)` 保留最新 10 个 dump（diag.c:103）。
- 扫描 modem_dump 现存 dump，拼成一行日志（避免多文件刷屏，diag.c:106-123）。
- 额外再清一次 dial_snap 保留 40（diag.c:126-127）。

### `void teardown_cp_dump_capture(void)`（diag.c:130-138）
- 仅当 `g_cp_dump_mounted`（即 bind mount 由本进程建立）才 `umount("/sdcard")`，置回 0。若挂载是系统预先存在的（`already_mounted` 分支）则不解除——避免误卸系统挂载。

## 四、关键点 / 潜在问题
1. **`diag_snapshot` 用 `system()` 调 dmesg/logcat**：进程若装了自定义 `SIGCHLD` handler，`system()` 的 `waitpid` 可能受扰（setup 里特意避开了，但 snapshot 未避开）。**需实测确认** dmesg/logcat 是否稳定生成。
2. `g_cp_dump_mounted` 为静态单进程标志（diag.c:65）；进程被 L3 `exit` 重拉后此标志复位为 0，但届时 `/sdcard` 可能仍处于已挂载状态，会走 `already_mounted` 分支不重复挂载——逻辑自洽。
3. snapshot 严格受 `has_connected_once` 门控（dial.c:634/707），与 CLAUDE.md「从未连上不做徒劳动作」一致。
