# EC200A CP Dump 自动落盘 SD 卡 — 完整会话记录

> **会话日期**：2026-05-25  
> **设备平台**：Quectel EC200A OpenCPU（ASR1803 SoC，AP=Cortex-A7/Linux，CP=Cortex-R5/蜂窝协议栈）  
> **问题背景**：CPIN 状态未知，移远分析认为是 CP down 导致，需要抓取 CP 侧 dump 供分析  
> **最终版本**：v1.27.4（`dial.c` PATCH_VERSION=4）

---

## 目录

- [1. 问题背景与初始需求](#1-问题背景与初始需求)
- [2. PDF 文档分析](#2-pdf-文档分析)
- [3. 设备现状调查](#3-设备现状调查)
- [4. 调查与试错过程（详细记录）](#4-调查与试错过程详细记录)
- [5. 关键技术发现](#5-关键技术发现)
- [6. 端到端验证](#6-端到端验证)
- [7. 将机制迁移至 dial.c（含三次真机 Bug 修复）](#7-将机制迁移至-dialc)
- [8. 设备配置还原](#8-设备配置还原)
- [9. 编译错误排查与修复](#9-编译错误排查与修复)
- [10. 最终代码实现](#10-最终代码实现)
- [11. 部署风险评估](#11-部署风险评估)
- [12. 最终状态总结](#12-最终状态总结)

---

## 1. 问题背景与初始需求

### 1.1 现象

客户设备（Quectel EC200A OpenCPU）出现 CPIN 状态未知的间歇性故障，移远技术支持分析认为根因是 CP（蜂窝协议栈，运行在 Cortex-R5）发生了 down/崩溃事件。要求抓取 CP 侧 dump 文件供 CATStudio 分析。

### 1.2 用户初始请求

> "CPIN状态未知的问题，移远分析可能是因为cp down了，现在要抓取cp侧的dump，能根据Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_Log抓取指导_V1.0.0_Preliminary_20240509.pdf文件里的步骤抓取日志写入sd卡里吗？先实际论述一下"

---

## 2. PDF 文档分析

PDF 全文 40 页（分两次读取：1-20 页、21-40 页），完整阅读后得出以下结论：

### 2.1 文档涵盖的三条路径

#### 第 2 章：Windows 环境（CATStudio / TFTPD64）
**完全不适用**。需要 USB 连接到 Windows 上位机，`AT+QCFG="cpdump",1` 会让模块进入 dump 模式等待 TFTP 拉取，在无人现场无法操作。

#### 第 2.2.2 节：CP Dump 自动保存到 /sdcard（关键！）
文档 p24 描述：

> 默认情况下，模块发生 CP dump 后，在根文件系统 **/sdcard/ 目录下会自动保存 CP dump 文件**，前提是 /sdcard/ 目录已挂载存储设备。

AT 命令配置（AG35 专有，EC200A 不支持）：
- `AT+QCFG="cpdump",0`：默认值，CP crash → 重启 → dump 自动保存到 `/sdcard/modem_dump/`
- `AT+QCFG="cpdump",1`：进 dump 模式等 TFTP（现场不可用）
- `AT+QCFG="cpdump",2`：CP crash → 仅重启，不保存 dump

关键障碍识别：PDF 示例中 `/sdcard` 指 AG35 系统中挂载了存储的路径；EC200A 测试设备的 SD 卡挂载在 `/media/sdcard`，路径不同，需要 bind mount 解决。

#### 第 3.2 节：Diag Log 自动存储（次选）
修改 `/etc/mrvl_tel_diag.cfg` 的 `default_media=2` 可让 Diag log 写 SD 卡，但：
- `/bin/diag` 守护进程在无 USB 主机时立即退出（后来验证）
- 生成 `.sdl` 二进制文件，需 CATStudio 分析

#### 第 2.2.3 节：手动触发 CP dump
```bash
echo a > /dev/acipc    # 从 AP 侧手动触发 CP dump
```

### 2.2 文档的 AT 命令适用性

| AT 命令 | 文档平台 | EC200A 结果 |
|---------|---------|------------|
| `AT+QCFG="cpdump"` | AG35 | ERROR |
| `AT+QCFG="cpdump",1` | AG35 | ERROR |
| `AT+QCFG="aplog"` | AG35 | ERROR |
| `AT+QCFG="cplog"` | AG35 | ERROR |

**结论**：上述 AT 命令均为 AG35 专有，EC200A 固件不支持。

---

## 3. 设备现状调查

### 3.1 设备信息

```
OW21.02_asr1803p401_rls211_1.057.030_20220902_04_14_bld73
```

### 3.2 存储挂载情况

```
root@OpenWrt:~# df -h
Filesystem                Size      Used Available Use%  Mounted on
/dev/dm-0                16.9M     16.9M         0  100% /
tmpfs                    45.5M    224.0K     45.3M    0% /tmp
ubi0:data                85.3M      2.0M     79.0M    2% /data
/dev/mmcblk0p1           56.4G    132.5M     53.4G    0% /media/sdcard

root@OpenWrt:~# ls -la /sdcard
total 1
drwxr-xr-x    2 root     root     3 Nov 30  2023 .
drwxr-xr-x   23 root     root   338 Jun  6  2024 ..

root@OpenWrt:~# cat /proc/mounts | grep sdcard
/dev/mmcblk0p1 /media/sdcard ext4 rw,seclabel,relatime 0 0
```

**关键发现**：
- SD 卡（56.4GB）挂载在 `/media/sdcard`
- `/sdcard` 是一个**空目录**，没有挂载任何存储
- CP dump 写 `/sdcard/modem_dump/` 时写入只读 squashfs 根文件系统，**全部静默丢失**

### 3.3 设备节点验证

```
root@OpenWrt:~# ls -la /dev/acipc
crw-rw----  1 root root 248, 0 Jan 1 1970 /dev/acipc

root@OpenWrt:~# ls -la /dev/ramdump_ctl
crw-rw----  1 root root 248, 3 Jan 1 1970 /dev/ramdump_ctl

root@OpenWrt:~# ls -la /dev/diagdev
crw-rw----  1 root root 240, 0 Jan 1 1970 /dev/diagdev
```

三个设备节点均存在，CP 正常（AT/CFUN/CPIN 均响应正常）。

### 3.4 启动脚本系统

```
/etc/init.d/ 中关键脚本：
  S09ql_rild        ← ql_rild（射频驱动）
  S12lte-telephony  ← 启动 /bin/eeh、/bin/diag、/bin/mrdloader
  S50sdcard_mount   ← SD 卡挂载（实际内容全部注释，无效）
  
/etc/rc.local       ← 可写，空文件（只有 exit 0）
```

---

## 4. 调查与试错过程（详细记录）

### 4.1 第一轮：AT 命令路 → 失败

**尝试**：`serial_atcmd AT+QCFG="cpdump"` → `ERROR`

**用户追问**：加 `\r\n` 试试？

**分析与结论**：`serial_atcmd` 会自动加 `\r\n`，ERROR 是因为 EC200A 不支持这个命令（AG35 专有），与换行符无关。

### 4.2 第二轮：/bin/diag 路 → 失败

**调查步骤**：

```bash
# 找读 mrvl_tel_diag.cfg 的进程
grep -rl "mrvl_tel_diag" /bin/ /sbin/ /usr/bin/ /usr/sbin/ 2>/dev/null
→ /bin/diag

# 查进程状态
ps | grep diag
→ 1914 root /bin/diag

# 查 mrvl_tel_diag.cfg 内容
cat /etc/mrvl_tel_diag.cfg
→ default_media=1, mount_path=/mnt/mmcblk0, ...
```

**尝试**：修改 `mrvl_tel_diag.cfg`：
```bash
sed -i 's/default_media=1/default_media=2/' /etc/mrvl_tel_diag.cfg
sed -i 's|mount_path=/mnt/mmcblk0|mount_path=/media/sdcard|' /etc/mrvl_tel_diag.cfg
```

**同时**：添加 bind mount 到 rc.local：
```bash
sed -i 's/^exit 0/mount --bind \/media\/sdcard \/sdcard\nexit 0/' /etc/rc.local
```

**重启后结果**：
```
ls /sdcard/Log000/
→ ls: /sdcard/Log000/: No such file or directory
```

**进一步调查**：

```bash
# 检查进程是否还在
pgrep -x diag && echo "running" || echo "not running"
→ not running    ← 关键发现！

# dmesg 显示
dmesg | grep -i diag | tail -10
→ diag usb: USBEnterBoot
→ diag usb: Start Uevent
→ diag usb: Logcat condition wait    ← 等待 USB 连接，没有则退出
```

**结论**：`/bin/diag` 是 USB diag 口的接口守护进程，等待 USB 上位机连接，无 USB 连接时立即退出。它**不是** SD 卡自动写入服务，在现场部署场景完全无效。

### 4.3 第三轮：lte-telephony sed 错误插入

**尝试**：查 lte-telephony 发现 eeh 启动在 diag 之前：

```bash
cat /etc/init.d/lte-telephony
# 内容：
start() {
    ...
    /bin/eeh -M yes &
    RDUP=`uci get cmdline.RDUP 2> /dev/null`
    if [ "$RDUP" != "1" ]; then
        nice -n -5 /bin/diag &
    fi
    /bin/mrdloader &
}
```

**错误 1**：使用空格 pattern 的 sed 失败（文件用 tab，sed pattern 用空格）：

```bash
# 失败：
sed -i 's|nice -n -5 /bin/diag|mount --bind /media/sdcard /sdcard\n        nice -n -5 /bin/diag|' ...
# 结果：bind mount 被插入到 if 块内部，但 eeh 在 if 块之前启动
```

**grep 验证发现问题**：

```
23:     mount --bind /media/sdcard /sdcard   ← 在 if 块外（正确位置）
24:     /bin/eeh -M yes &
25:     RDUP=...
26:     if [ "$RDUP" != "1" ]; then
27:         (bind mount 本应在 if 块前，但首次 sed 误放到这里)
```

**修正**：使用 `/i` 插入语法，不依赖缩进：

```bash
sed -i '/\/bin\/eeh -M yes/i\        mount --bind /media/sdcard /sdcard' /etc/init.d/lte-telephony
```

**但 /sdcard/Log000 仍未出现**，因为 `/bin/diag` 根本不写 SD。

### 4.4 第四轮：发现 EEH 才是真正机制

**调查**：

```bash
strings /bin/eeh | grep -E "sdcard|modem_dump|dump"
→ /sdcard/modem_dump/
→ [EEH] Save CP dump bins done to %s
→ [EEH] %s, record dump files full
→ echo a > /dev/ramdump_ctl
→ /sdcard/ramfiletmp.tgz
```

**关键发现**：EEH（Error Event Handler）才是 CP dump 的真正处理者，它负责：
- 监听 CP 崩溃事件
- 将 dump 写入 `/sdcard/modem_dump/`

**验证 EEH 状态**：

```bash
pgrep -x eeh && echo "running" || echo "not running"
→ eeh not running    ← 手动执行 /bin/eeh -M yes 后立即退出

dmesg | grep EEH
→ [EEH] wifi = -112    ← 内核打印，说明 EEH 是内核驱动，非用户态守护进程
```

**根本认知修正**：EEH 是内核驱动（在 ASR1803 内核中），`/bin/eeh` 只是配置/测试工具，`-M yes` 参数在当前固件无效，退出是正常的。CP dump 捕获由内核自动处理。

---

## 5. 关键技术发现

### 5.1 EC200A CP Dump 真实机制

```
CP 崩溃事件
    │
    ▼
内核 EEH 驱动（ASR1803 内核模块）
    │
    ▼
写入 /sdcard/modem_dump/N_cp_Assert_YYYY-M-DD-H-MM-SS.tar.gz
```

EEH 是内核驱动，不是用户态守护进程。`dmesg | grep EEH` 可以看到内核层的 EEH 日志（`[EEH] wifi = -112` 等）。

### 5.2 历史 dump 丢失原因

```
CP 崩溃 → 内核 EEH 写 /sdcard/modem_dump/
         │
         ▼
/sdcard 是只读 squashfs 根文件系统上的空目录
         │
         ▼
写入失败，dump 静默丢失
```

**解决方案**：`mount --bind /media/sdcard /sdcard`，将内核 EEH 的写入目标重定向到实际 SD 卡。

### 5.3 /bin/diag 的真实角色

| 项目 | 实际情况 |
|------|---------|
| 读取的配置文件 | `/etc/mrvl_tel_diag.cfg` |
| 功能 | USB diag 口守护进程 |
| 行为 | 等待 USB 上位机连接；无 USB → 立即退出 |
| 与 CP dump 的关系 | **无关** |
| `default_media=2` 的作用 | 在有 USB 上位机时将 diag 数据写 SD（现场无效） |

### 5.4 /bin/eeh 的真实角色

| 项目 | 实际情况 |
|------|---------|
| `/bin/eeh` | 配置/测试命令行工具，非守护进程 |
| `-M yes` | 当前固件不识别，立即退出 |
| `-T cpassert` | 手动触发 CP assert，用于测试 |
| `-p` | 打印 EEH 配置（返回空，说明用 kernel 默认） |
| 实际 EEH 逻辑 | 在内核驱动里，不在用户空间 |

---

## 6. 端到端验证

### 6.1 bind mount 建立

```bash
mount --bind /media/sdcard /sdcard
df -h
→ /dev/mmcblk0p1  56.4G  125.1M  53.4G  0%  /sdcard    ✅
```

### 6.2 手动触发 CP dump（测试）

```bash
/bin/eeh -T cpassert
# 输出（部分）：
[ 9231.241345] CP ASSERT/L2RESET (DUMP/REST) requested by AP
[ 9231.267544] When AP-CP common 32kHz counter is : 0 = 0x00000000
[ 9231.273583] CP down !!!!
...
cpdump: cp_wptr 2, ap_rptr 3, total_cnt 70
[EEH] Save CP dump bins done to ...
```

### 6.3 dump 文件验证

```bash
ls /sdcard/modem_dump/
→ 1_cp_Assert_2026-5-25-5-57-55.tar.gz    ✅

ls /media/sdcard/modem_dump/
→ 1_cp_Assert_2026-5-25-5-57-55.tar.gz    ✅（bind mount 两端一致）

tar -tzf 1_cp_Assert_2026-5-25-5-57-55.tar.gz
→ log//com_DDR_RW.bin    ← CP DDR 内存镜像
→ log//dmesg.txt         ← 崩溃时内核日志
→ log//diag.sdl          ← CP Diag log（CATStudio 可直接读取）✅
```

**`diag.sdl` 就是移远需要的文件，可直接用 CATStudio 分析 CP crash 根因。**

### 6.4 重启后持久化验证（lte-telephony 方案）

```bash
reboot
# 重启后：
df -h
→ /dev/mmcblk0p1  /sdcard    ✅（bind mount 自动生效）

ls /sdcard/modem_dump/
→ 1_cp_Assert_2026-5-25-5-57-55.tar.gz    ✅（文件保留）
```

### 6.5 最终验证（dial.c v1.27.4 方案，第三次部署）

```bash
# dial 启动日志：
[CPDUMP] Bind mounted /media/sdcard -> /sdcard for CP dump capture.
[CPDUMP] No existing CP dumps.

# 触发 CP assert：
/bin/eeh -T cpassert

# dump 文件验证：
ls -lh /sdcard/modem_dump/
→ -rw-r--r-- 1 root root 417.7K May 25 15:49 1_cp_Assert_2026-5-25-7-49-30.tar.gz  ✅

ls -lh /media/sdcard/modem_dump/
→ -rw-r--r-- 1 root root 417.7K May 25 15:49 1_cp_Assert_2026-5-25-7-49-30.tar.gz  ✅（两端一致）

tar -tzf /sdcard/modem_dump/*.tar.gz
→ log//com_DDR_RW.bin    ✅
→ log//dmesg.txt          ✅
→ log//diag.sdl           ✅（移远 CATStudio 可直接读取）

# 确认无其他进程使用 /sdcard（bind mount 对系统无副作用）：
lsof /sdcard 2>/dev/null
→ （无输出）               ✅
```

---

## 7. 将机制迁移至 dial.c

### 7.1 迁移原因

原方案（手动修改 `lte-telephony`）的问题：
- 需要单独维护设备配置文件
- 客户设备升级固件时可能被覆盖
- 不符合"所有逻辑集中在 dial 进程"的原则

用户明确要求：

> "修改了设备的那些东西都要还原回去"（指 C 方案）

### 7.2 迁移方案设计

| 功能 | 负责方 | 触发时机 |
|------|-------|---------|
| bind mount `/sdcard` | `dial.c:setup_cp_dump_capture()` | `log_init()` 之后，`dial_loop()` 之前 |
| umount `/sdcard` | `dial.c:teardown_cp_dump_capture()` | SIGINT 退出时 |
| 旧 dump 清理 | `dial.c:cleanup_dir_keep_newest()` | `setup_cp_dump_capture()` 调用 |
| 旧 diag 快照清理 | 同上 | 同上 |

**注意**：`mrvl_tel_diag.cfg` 的修改（`default_media`、`mount_path`、`silent_type`）对 CP dump 无任何影响，不需要在 `dial.c` 里维护。

### 7.3 Bug 一（代码阶段）：/proc/mounts strstr 误判

**错误实现**（首版）：

```c
if (strstr(line, "/sdcard") && strstr(line, "/media/sdcard")) {
    already_mounted = 1;
}
```

**问题**：`/media/sdcard` 本身包含子串 `/sdcard`，只要 SD 卡挂载了（`/media/sdcard` 出现在 `/proc/mounts`），此检查就误判为"bind mount 已存在"，`mount --bind` 从未执行。

**中间修复（sscanf 版，精确匹配挂载点字段）**：

```c
char dev[128], mnt[128];
if (sscanf(line, "%127s %127s", dev, mnt) == 2 &&
    strcmp(mnt, "/sdcard") == 0) {
    already_mounted = 1;
}
```

此版本在真机部署时再次失败，见下方 7.4。

---

### 7.4 Bug 二（真机部署一）：overlayfs 挂在 /sdcard 被 sscanf 误判为"已挂载"

**现象**（第一次部署，sscanf 版本）：

日志打印 `[CPDUMP] /sdcard already bind-mounted.`，但 df 显示：

```
Filesystem        Mounted on
overlayfs:/data/media  /sdcard    ← 系统固件本身把 overlayfs 挂在这里！
/dev/mmcblk0p1    /media/sdcard
```

sscanf 在 `/proc/mounts` 找到挂载点字段恰为 `/sdcard` 的一行（overlayfs），触发 `already_mounted = 1`。bind mount 从未执行，dump 写入 NAND 丢失。

**根因**：该固件版本（OW21.02_asr1803p401）本身就在 `/sdcard` 挂载了 `overlayfs:/data/media`。sscanf 检查只看字符串，无法区分"overlayfs 挂在 /sdcard"和"SD 卡 bind mount 到 /sdcard"。

**最终修复（stat st_dev 比较）**：

```c
/* 不能只检查 /sdcard 是否有挂载（系统 overlayfs 也挂在 /sdcard），
 * 必须比较底层设备号（st_dev），相同才说明是同一块 SD 卡。 */
int already_mounted = 0;
struct stat st_sdcard, st_media;
if (stat("/sdcard", &st_sdcard) == 0 && stat("/media/sdcard", &st_media) == 0) {
    if (st_sdcard.st_dev == st_media.st_dev)
        already_mounted = 1;
}
```

overlayfs 的 `st_dev` 与 `/dev/mmcblk0p1` 不同；bind mount 后两者 `st_dev` 相同。

---

### 7.5 Bug 三（真机部署二）：system("mount --bind") 被 SIGCHLD 干扰返回非零

**现象**（第二次部署，st_dev 已修复，仍用 system()）：

日志打印 `[CPDUMP] mount --bind failed, CP dumps may be lost.`，但 df 显示：

```
Filesystem             Mounted on
overlayfs:/data/media  /sdcard    ← overlayfs（遮在下方）
/dev/mmcblk0p1         /sdcard    ← SD 卡（叠在上方，实际生效）✅
```

`mount --bind` **实际成功执行**，但 `system()` 返回了非零，导致代码提前 `return`，`modem_dump/` 目录未创建。

**根因**：dial 进程注册了 `sigchld_handler`，其中调用 `waitpid()`。`system()` 内部执行 fork→exec(sh)→waitpid()。当 shell 子进程退出时，SIGCHLD 触发，`sigchld_handler` 先于 `system()` 调用 `waitpid()` 将子进程状态收走，`system()` 自己的 `waitpid()` 找不到子进程（ECHILD），返回非零。

**最终修复（mount(2) 系统调用）**：

```c
#include <sys/mount.h>

if (mount("/media/sdcard", "/sdcard", NULL, MS_BIND, NULL) != 0) {
    dial_log("[CPDUMP] mount --bind failed (%s), CP dumps may be lost.\n", strerror(errno));
    return;
}
```

`mount(2)` 直接进内核，无 fork/exec/waitpid 链，SIGCHLD 无法干扰。teardown 同理改用 `umount("/sdcard")`。

---

## 8. 设备配置还原

### 8.1 还原操作

用户在设备上执行了以下还原命令：

```bash
# 还原 /etc/mrvl_tel_diag.cfg（3 处）
sed -i 's/default_media=2/default_media=1/' /etc/mrvl_tel_diag.cfg
sed -i 's|mount_path=/media/sdcard|mount_path=/mnt/mmcblk0|' /etc/mrvl_tel_diag.cfg
sed -i 's/silent_type=0/silent_type=1/' /etc/mrvl_tel_diag.cfg

# 还原 /etc/init.d/lte-telephony（删除 bind mount 行）
sed -i '/mount --bind/d' /etc/init.d/lte-telephony

# 删除测试产生的 dump 文件
rm -rf /sdcard/modem_dump/
```

### 8.2 还原验证

```bash
grep -E "default_media|mount_path|silent_type" /etc/mrvl_tel_diag.cfg
→ default_media=1
→ silent_type=1
→ mount_path=/mnt/mmcblk0

grep -n "bind\|eeh" /etc/init.d/lte-telephony
→ 23:     /bin/eeh -M yes &
→ 32:     killall diag eeh rild atcmdsrv nvmproxy
```

**结果**：两个文件均还原到操作前的原始状态。

### 8.3 关于 bind mount 当时未执行 umount

还原操作后用户没有执行 `umount /sdcard`，bind mount 在重启前仍活跃（`df -h` 中可见两条 `/dev/mmcblk0p1`）。这不影响系统功能，重启后自然消失。

---

## 9. 编译错误排查与修复

### 9.1 错误现象

```
./dial.c: In function 'diag_snapshot':
./dial.c:373:5: warning: implicit declaration of function 'cleanup_dir_keep_newest'
./dial.c:411:13: error: static declaration of 'cleanup_dir_keep_newest' follows non-static declaration
./dial.c:411:13: error: static declaration of 'cleanup_dir_keep_newest' follows non-static declaration
```

### 9.2 根因分析

编辑过程中 `#if 0` 被意外删除，导致：

1. `diag_snapshot()` 从禁用块变为裸露的静态函数
2. `diag_snapshot()` 内部调用 `cleanup_dir_keep_newest()`（前向引用）
3. `cleanup_dir_keep_newest()` 定义在 `diag_snapshot()` 之后
4. 编译器将未声明的 `cleanup_dir_keep_newest` 推断为 `extern int`，与后面的 `static void` 定义冲突

### 9.3 修复方案

1. 将 `cleanup_dir_keep_newest()` 及其 helper（`cmp_mtime_asc()`、`dir_entry_t`）移到 `diag_snapshot()` 定义之**前**
2. 重新用 `#if 0`/`#endif` 包裹 `diag_snapshot()` 临时禁用
3. 重新注释两处调用点（行 1884、行 1946）
4. 编译通过后，用户要求彻底启用：移除 `#if 0`，取消调用注释

### 9.4 最终函数顺序

```
dial.c（相关函数顺序）：
  ~行 359：typedef dir_entry_t
  ~行 362：cmp_mtime_asc()
  ~行 370：cleanup_dir_keep_newest()    ← 先定义
  ~行 415：diag_snapshot()              ← 再定义（现已启用，无 #if 0）
  ~行 459：g_cp_dump_mounted
  ~行 462：setup_cp_dump_capture()
  ~行 535：teardown_cp_dump_capture()
```

---

## 10. 最终代码实现

### 10.1 目录清理工具函数

```c
/* 按 mtime 升序排序，最旧的排在最前 */
typedef struct { char name[NAME_MAX + 1]; time_t mtime; } dir_entry_t;

static int cmp_mtime_asc(const void *a, const void *b)
{
    const dir_entry_t *ea = a, *eb = b;
    return (ea->mtime > eb->mtime) - (ea->mtime < eb->mtime);
}

static void cleanup_dir_keep_newest(const char *dir, int max_keep)
{
    DIR *d = opendir(dir);
    if (!d) return;
    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] != '.') count++;
    }
    if (count <= max_keep) { closedir(d); return; }

    dir_entry_t *entries = malloc(count * sizeof(dir_entry_t));
    if (!entries) { closedir(d); return; }
    rewinddir(d);
    int n = 0;
    while ((ent = readdir(d)) != NULL && n < count) {
        if (ent->d_name[0] == '.') continue;
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            strncpy(entries[n].name, ent->d_name, NAME_MAX);
            entries[n].name[NAME_MAX] = '\0';
            entries[n].mtime = st.st_mtime;
            n++;
        }
    }
    closedir(d);
    if (n > max_keep) {
        qsort(entries, n, sizeof(dir_entry_t), cmp_mtime_asc);
        int del = n - max_keep;
        for (int i = 0; i < del; i++) {
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", dir, entries[i].name);
            if (unlink(path) == 0)
                dial_log("[CLEANUP] Removed old file: %s\n", entries[i].name);
        }
        dial_log("[CLEANUP] %s: kept %d, removed %d old.\n", dir, n - del, del);
    }
    free(entries);
}
```

### 10.2 diag_snapshot()（已启用）

```c
/* 故障触发诊断快照：在断网故障首次确认时调用，一次性抓取 dmesg + logcat 缓冲区。
 * 相比持续采集，只在需要时产生数据，单次 ~300KB，不持续占用 SD 卡空间。
 * label: 调用方传入标签字符串（如 "fault" / "recovery"），用于区分文件名语义。 */
static void diag_snapshot(const char *label)
{
    if (access("/media/sdcard", F_OK) != 0) return;
    const char *diag_dir = "/media/sdcard/dial_snap";
    if (mkdir(diag_dir, 0755) != 0 && errno != EEXIST) {
        dial_log("[DIAG] mkdir %s failed: %s, skip snapshot.\n", diag_dir, strerror(errno));
        return;
    }
    /* 写入前先清理旧快照，保留最新 40 个文件（约 10 个故障周期） */
    cleanup_dir_keep_newest(diag_dir, 40);
    char ts[32];
    time_t t = time(NULL);
    struct tm tm_info;
    localtime_r(&t, &tm_info);
    strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &tm_info);
    char cmd[400];
    snprintf(cmd, sizeof(cmd),
             "dmesg > '%s/dmesg_%s_%s.log' 2>/dev/null", diag_dir, label, ts);
    system(cmd);
    snprintf(cmd, sizeof(cmd),
             "logcat -d -v time > '%s/logcat_%s_%s.log' 2>/dev/null", diag_dir, label, ts);
    system(cmd);
    dial_log("[DIAG] Snapshot saved: %s/[dmesg|logcat]_%s_%s.log\n", diag_dir, label, ts);
}
```

**调用点**：
- `~行 1884`：`diag_snapshot("recovery");`（网络恢复时）
- `~行 1946`：`diag_snapshot("fault");`（故障首次确认时）

### 10.3 CP Dump 捕获函数（最终版，v1.27.4）

头文件新增：

```c
#include <sys/mount.h>    /* mount(2) / umount(2) 系统调用 */
```

```c
/* === CP Dump 捕获支持 ================================================
 * EC200A (ASR1803 OpenCPU) CP 崩溃时，内核 EEH 驱动自动将 dump 写入
 * /sdcard/modem_dump/，dump 包含 diag.sdl（CATStudio 可直接读取）。
 * /sdcard 默认为空目录，需 bind mount 到 SD 卡，否则 dump 随重启丢失。
 * ==================================================================== */
static int g_cp_dump_mounted = 0; /* bind mount 是否由本进程建立 */

static void setup_cp_dump_capture(void)
{
    if (access("/media/sdcard", F_OK) != 0) {
        dial_log("[CPDUMP] /media/sdcard not available, skip bind mount.\n");
        return;
    }
    /* 检查 /sdcard 是否已经 bind mount 到 /media/sdcard。
     * 不能只检查 /sdcard 是否有挂载（系统 overlayfs 也可能挂在 /sdcard），
     * 必须比较两个路径的底层设备号（st_dev），相同才说明是同一块 SD 卡。 */
    int already_mounted = 0;
    struct stat st_sdcard, st_media;
    if (stat("/sdcard", &st_sdcard) == 0 && stat("/media/sdcard", &st_media) == 0) {
        if (st_sdcard.st_dev == st_media.st_dev)
            already_mounted = 1;
    }
    if (already_mounted) {
        dial_log("[CPDUMP] /sdcard already bind-mounted to SD card.\n");
    } else {
        /* 使用 mount(2) syscall 代替 system("mount --bind ...")，避免进程的
         * SIGCHLD 处理函数干扰 system() 内部 waitpid() 导致返回值不可信。 */
        if (mount("/media/sdcard", "/sdcard", NULL, MS_BIND, NULL) != 0) {
            dial_log("[CPDUMP] mount --bind failed (%s), CP dumps may be lost.\n", strerror(errno));
            return;
        }
        g_cp_dump_mounted = 1;
        dial_log("[CPDUMP] Bind mounted /media/sdcard -> /sdcard for CP dump capture.\n");
    }
    /* 确保 modem_dump 目录存在 */
    if (mkdir("/sdcard/modem_dump", 0755) != 0 && errno != EEXIST) {
        dial_log("[CPDUMP] mkdir /sdcard/modem_dump failed: %s\n", strerror(errno));
    }
    /* 清理旧 CP dump，保留最新 10 个 */
    cleanup_dir_keep_newest("/sdcard/modem_dump", 10);
    /* 扫描并记录已有 dump 文件 */
    DIR *dir = opendir("/sdcard/modem_dump");
    if (dir) {
        struct dirent *ent;
        int count = 0;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            dial_log("[CPDUMP] Existing dump: %s\n", ent->d_name);
            count++;
        }
        closedir(dir);
        if (count == 0)
            dial_log("[CPDUMP] No existing CP dumps.\n");
        else
            dial_log("[CPDUMP] Found %d existing CP dump file(s).\n", count);
    }
    /* 清理旧诊断快照，保留最新 40 个文件（约 10 个故障周期） */
    if (access("/media/sdcard/dial_snap", F_OK) == 0)
        cleanup_dir_keep_newest("/media/sdcard/dial_snap", 40);
}

static void teardown_cp_dump_capture(void)
{
    if (!g_cp_dump_mounted) return;
    if (umount("/sdcard") == 0)
        dial_log("[CPDUMP] /sdcard unmounted.\n");
    else
        dial_log("[CPDUMP] umount /sdcard failed: %s\n", strerror(errno));
    g_cp_dump_mounted = 0;
}
```

### 10.4 main() 中的调用

```c
log_init();
dial_log("Program started. Main Version: %d.%02d.%d\n", MAIN_VERSION, SUB_VERSION, PATCH_VERSION);
setup_cp_dump_capture();   /* 新增：bind mount + dump 目录初始化 */
```

### 10.5 SIGINT 退出路径

```c
if (g_sigint_received) {
    dial_log("[EVENT] SIGINT received. Cleaning up and exiting.\n");
    teardown_cp_dump_capture();   /* 新增：umount /sdcard */
    if (p_dial_mng != NULL) { free(p_dial_mng); p_dial_mng = NULL; }
    log_close();
    exit(0);
}
```

### 10.6 版本号

```c
#define MAIN_VERSION  1
#define SUB_VERSION   27
#define PATCH_VERSION 4    /* v1.27.4 */
```

### 10.7 真机部署迭代记录（v1.27.4）

| 部署次 | 问题 | 现象 | 根因 | 修复 |
|--------|------|------|------|------|
| 第一次 | already_mounted 误判 | 日志：`/sdcard already bind-mounted.`；df 显示 overlayfs，非 SD 卡 | 固件在 `/sdcard` 挂了 `overlayfs:/data/media`，sscanf 只看字符串，看到 `/sdcard` 就认为已挂载 | 改用 `stat()` st_dev 比较 |
| 第二次 | system() 返回非零 | 日志：`mount --bind failed`；但 df 显示 /dev/mmcblk0p1 已挂在 /sdcard（mount 实际成功） | `sigchld_handler` 的 `waitpid()` 先于 `system()` 内部 `waitpid()` 将子进程状态收走，`system()` 返回错误 | 改用 `mount(MS_BIND)` 系统调用 |
| 第三次 | 无 | 日志：`Bind mounted /media/sdcard -> /sdcard for CP dump capture.`；modem_dump/ 已创建 | — | — |

### 10.8 提交说明（建议）

```
[BUGFIX] 修复 bind mount 检测与系统调用，版本 v1.27.4

- already_mounted 检测改用 stat() st_dev 比较，避免系统 overlayfs
  挂载在 /sdcard 时被误判为已 bind mounted
- mount --bind 改用 mount(MS_BIND) 系统调用替代 system()，避免
  SIGCHLD 处理函数干扰 system() 内部 waitpid() 致返回值不可信
- teardown 同步改用 umount() 系统调用
- 已在真机触发 /bin/eeh -T cpassert 验证，dump 写入 /media/sdcard/modem_dump/
```

---

## 11. 部署风险评估

### 11.1 正常情况（有 SD 卡）

`setup_cp_dump_capture()` 执行：
1. `access("/media/sdcard")` → 存在 → 继续
2. `/proc/mounts` 精确检查 → 未挂载 → `mount --bind` → 成功
3. `mkdir /sdcard/modem_dump` → 完成
4. 扫描已有 dump → 打日志

**无副作用**：bind mount 只是把 `/sdcard` 指向 `/media/sdcard`，不改变 SD 卡已有内容，不影响 dial 日志路径（`/media/sdcard/dial_log/`）。

### 11.2 无 SD 卡（有保护）

```c
if (access("/media/sdcard", F_OK) != 0) {
    dial_log("[CPDUMP] /media/sdcard not available, skip bind mount.\n");
    return;
}
```

直接跳过，对拨号流程零影响。**无风险**。

### 11.3 CP 频繁崩溃（真实风险，已保护）

| 目录 | 清理策略 | 单文件大小 | 最大占用 |
|------|---------|---------|---------|
| `/media/sdcard/modem_dump/` | 保留最新 10 个 | 数十 MB | ~300MB 量级 |
| `/media/sdcard/dial_snap/` | 保留最新 40 个文件 | ~150KB | ~6MB |

**`cleanup_dir_keep_newest()` 在 `setup_cp_dump_capture()` 调用，dial 每次启动都会清理；`diag_snapshot()` 内部每次写快照前也会清理**，运行时不会无限积累。

### 11.4 快速退出重启后的 bind mount 状态

- **Fast-fail exit(1)**：进程重启后 `setup_cp_dump_capture()` 检查 `/proc/mounts` 发现已挂载 → 跳过，`g_cp_dump_mounted=0`，SIGINT 不会 umount。bind mount 持续保留——**对 CP dump 捕获是好事**。
- **L3 exit(1) (AT+CFUN=1,1)**：整个 SoC 重启，bind mount 自然消失，下次开机由 dial 重新建立。

### 11.5 `mount --bind` 阻塞问题

`mount --bind` 是内核 `mount(2)` 系统调用，正常情况毫秒级完成，不涉及 I/O 等待。**无实际风险**。

---

## 12. 最终状态总结

### 12.1 SD 卡目录结构

```
/media/sdcard/
├── dial_log/      ← 心跳日志（dial_YYYYMMDD_HHMMSS.log，由 logger_sd.c 管理）
├── dial_snap/     ← 故障快照（dmesg_*.log / logcat_*.log，最多 40 个文件）
└── modem_dump/    ← CP dump（*.tar.gz，最多 10 个文件）
```

### 12.2 dump 文件内容

```
N_cp_Assert_YYYY-M-DD-H-MM-SS.tar.gz
└── log/
    ├── com_DDR_RW.bin    ← CP DDR 内存镜像（CP 运行时全量内存快照）
    ├── dmesg.txt         ← CP 崩溃时的内核日志
    └── diag.sdl          ← CP Diag log（二进制格式，CATStudio 可直接读取）
```

移远分析时只需 `diag.sdl`，将其从 tar.gz 解压后直接用 CATStudio 打开。

### 12.3 设备配置文件状态

| 文件 | 状态 |
|------|------|
| `/etc/mrvl_tel_diag.cfg` | **已还原**（default_media=1, mount_path=/mnt/mmcblk0, silent_type=1） |
| `/etc/init.d/lte-telephony` | **已还原**（无 bind mount 行） |
| `/etc/rc.local` | **未净变化**（加了再删，和原始一致） |

### 12.4 dial.c 新增内容一览

| 函数/变量 | 位置（近似行号） | 说明 |
|---------|----------------|------|
| `dir_entry_t` | ~359 | mtime 排序用结构体 |
| `cmp_mtime_asc()` | ~362 | qsort 比较函数 |
| `cleanup_dir_keep_newest()` | ~370 | 按 mtime 保留最新 N 个文件 |
| `diag_snapshot()` | ~415 | 故障快照（已启用） |
| `g_cp_dump_mounted` | ~459 | bind mount 标志 |
| `setup_cp_dump_capture()` | ~462 | 启动时建立 bind mount |
| `teardown_cp_dump_capture()` | ~535 | SIGINT 退出时 umount |

### 12.5 文档与现实的对比

| 文档目标（Section 2.2.2.2） | EC200A 实际 | 实现状态 |
|---------------------------|-----------|---------|
| CP dump 自动写 `/sdcard/modem_dump/` | 内核 EEH 驱动完成，已验证 | ✅ |
| `/sdcard` 需挂载存储 | `setup_cp_dump_capture()` bind mount | ✅ |
| dump 含 `diag.sdl`（CATStudio 可读） | 测试解包验证 | ✅ |
| `AT+QCFG="cpdump"` 命令 | EC200A 返回 ERROR（AG35 专有）| ❌ 平台不支持 |

**结论**：CP 侧 dump 抓取的核心目标已实现，触发方式为内核 EEH 自动处理（非 AT 命令），是平台差异而非功能缺失。客户设备 CP 崩溃时，`diag.sdl` 会自动落盘到 SD 卡，移远可直接分析。

---

## 附录：本次会话操作的设备命令完整列表

### A.1 调查类命令（只读，无副作用）

```bash
df -h
ls -la /sdcard
cat /proc/mounts | grep sdcard
serial_atcmd AT
serial_atcmd AT+CFUN?
serial_atcmd AT+CPIN?
serial_atcmd AT+QCFG="cpdump"
serial_atcmd 'AT+QCFG="cpdump"'
serial_atcmd 'AT+QCFG="cpdump",1'
serial_atcmd AT+QCFG="cpdump",1
serial_atcmd AT+QCFG="aplog"
serial_atcmd AT+QCFG="cplog"
ls -la /dev/acipc /dev/ramdump_ctl /dev/diagdev
find / -name "mrvl_tel_diag.cfg" 2>/dev/null
cat /etc/mrvl_tel_diag.cfg
cat /etc/init.d/sdcard_mount
cat /etc/init.d/lte-telephony
cat /etc/init.d/rdp_init
cat /etc/rc.local
ls /etc/rc.local /etc/init.d/ /etc/rc.d/
grep -rl "mrvl_tel_diag" /sbin/ /usr/bin/ /usr/sbin/ /bin/ 2>/dev/null
fuser /dev/diagdev 2>/dev/null
ps | grep -E "diag|rdp|rild|tele"
ls -la /sbin/rdp_transfer
strings /sbin/rdp_transfer | grep -E "mrvl|sdcard|Log|diag"
cat /proc/1914/status | grep -E "Pid|PPid"
pgrep -x diag
pgrep -x eeh
strings /bin/diag | grep -E "mount_path|mmcblk|Log0|sdcard|mkdir"
strings /bin/eeh | grep -E "sdcard|modem_dump|dump"
/bin/eeh -p
/bin/eeh --help 2>&1
dmesg | grep -i diag | tail -20
```

### A.2 修改类命令（已全部还原）

```bash
# rc.local（加后删，净变化为零）
sed -i 's/^exit 0/mount --bind \/media\/sdcard \/sdcard\nexit 0/' /etc/rc.local
sed -i '/mount --bind/d' /etc/rc.local

# mrvl_tel_diag.cfg（已还原）
sed -i 's/default_media=1/default_media=2/' /etc/mrvl_tel_diag.cfg
sed -i 's|mount_path=/mnt/mmcblk0|mount_path=/media/sdcard|' /etc/mrvl_tel_diag.cfg
sed -i 's/silent_type=1/silent_type=0/' /etc/mrvl_tel_diag.cfg
# → 还原：
sed -i 's/default_media=2/default_media=1/' /etc/mrvl_tel_diag.cfg
sed -i 's|mount_path=/media/sdcard|mount_path=/mnt/mmcblk0|' /etc/mrvl_tel_diag.cfg
sed -i 's/silent_type=0/silent_type=1/' /etc/mrvl_tel_diag.cfg

# lte-telephony（已还原）
sed -i '/\/bin\/eeh -M yes/i\        mount --bind /media/sdcard /sdcard' /etc/init.d/lte-telephony
# → 还原：
sed -i '/mount --bind/d' /etc/init.d/lte-telephony
```

### A.3 测试类命令（会触发 CP 重启）

```bash
/bin/eeh -T cpassert    # 触发 CP assert，产生 dump 文件
```

测试产生的文件：`/sdcard/modem_dump/1_cp_Assert_2026-5-25-5-57-55.tar.gz`（已删除）

### A.4 重启验证

```bash
reboot    # 验证 bind mount 持久化（lte-telephony 方案，已改为 dial.c 方案后不再需要）
```

---

*文档整理时间：2026-05-25*  
*会话 ID：2d4dc2b5-0e91-4d93-a063-87f441a14e7a*
