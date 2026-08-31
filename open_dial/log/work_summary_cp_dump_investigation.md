# CP Dump 不完整问题排查工作记录

**时间：** 2026-05-27 ~ 2026-05-28
**固件版本：** `EC200ACNTAR02A04M2G_OCPU`
**问题描述：** 客户现场模组发生 CP 崩溃，SD 卡内保存的 CP dump 文件不完整，移远技术支持无法分析。

---

## 一、问题背景

客户设备基于移远 EC200A OpenCPU 模组，运行 OpenWrt 系统。我们的应用程序（`dial`，版本 v1.27.5）负责拨号管理，同时实现了将 `/sdcard` bind mount 到 SD 卡的机制，以便模组发生 CP dump 时能将 dump 文件持久化保存到 SD 卡，供移远分析。

客户现场触发了 `cp_DSP_COM_ERR_0x8164` 错误，移远收到 dump 后反馈文件被截断，内容太少无法分析。

---

## 二、现象对比分析

### 2.1 客户现场 dump（异常）

路径：`modem_dump/1_cp_DSP_COM_ERR_0x8164_unknown_2026-5-27-1-54-18/log/`

| 文件 | 大小 |
|---|---|
| `com_DDR_RW.bin` | 10,241 KB |
| `diag.sdl` | **1 KB**（异常）|
| `dmesg.txt` | 61 KB |

共 3 个文件。

### 2.2 正常 dump 应有内容（移远提供参考）

正常应包含约 20 个文件，包括：

| 文件 | 大小 |
|---|---|
| `cmdline.txt` | 326 B |
| `com_DDR_RW.bin` | ~9 MB |
| `com_DSP_DDR.bin` | 2,560 KB |
| `com_DTCM.bin` | 64 KB |
| `com_EE_Hbuf.bin` | 1 KB |
| `com_GBDump.bin` | 1,024 KB |
| `com_GplcSpy.bin` | 26 KB |
| `com_IPCDump.bin` | 4 KB |
| `com_ITCM.bin` | 64 KB |
| `com_L1aSpy.bin` | 6 KB |
| `com_MtpsRam.bin` | 64 KB |
| `com_RECORD.bin` | 58 KB |
| `com_WDT_END.bin` | 64 KB |
| `com_aplpSpy.bin` | 8 KB |
| `com_dtcSpy.bin` | 1 KB |
| `com_rti_tsk.bin` | 2 KB |
| `com_wdtKiCK.bin` | ~9 MB |
| `diag.sdl` | **~122 KB** |
| `dmesg.txt` | 114 B |
| `eeh_journal.txt` | 4 KB |
| `ubootlog.txt` | 4 KB |

### 2.3 核心差异

- `diag.sdl` 只有 1 KB，正常应为 122 KB，说明写到 1/122 时被打断
- 15 个 `com_*` 文件完全缺失，根本没有写入机会
- `com_DDR_RW.bin` 完整（说明 bind mount 工作正常，第一个文件已写完）

**初步判断：EEH 驱动在写 dump 过程中被 CP 复位打断。**

---

## 三、代码分析：bind mount 机制

### 3.1 实现原理

`dial.c` 中的 `setup_cp_dump_capture()` 函数（启动时调用）：

```c
// 将 /media/sdcard（SD 卡实际挂载点）bind mount 到 /sdcard
mount("/media/sdcard", "/sdcard", NULL, MS_BIND, NULL);

// 确保 modem_dump 目录存在
mkdir("/sdcard/modem_dump", 0755);

// 清理旧 dump，保留最新 10 个
cleanup_dir_keep_newest("/sdcard/modem_dump", 10);
```

原理：模组 EEH 驱动固定写入 `/sdcard/modem_dump/`，默认 `/sdcard` 为空目录（未挂载 SD 卡）。通过 bind mount，使 `/sdcard` 与 `/media/sdcard` 指向同一块 SD 卡分区，EEH 写入的 dump 文件即持久保存在 SD 卡上。

### 3.2 对 SD 卡的写操作限制

代码中对 SD 卡的所有写操作：

| 路径 | 操作 | 时机 |
|---|---|---|
| `/media/sdcard/dial_log/` | 写运行日志 | 持续 |
| `/media/sdcard/dial_snap/` | 写 dmesg+logcat 快照 | 故障时 |
| `/media/sdcard/call_fault.log` | 写故障记录（misc.c）| 故障时 |
| `/sdcard/modem_dump/` | 清理旧 dump（保留 10 个）| **仅启动时执行一次** |

**关键结论：清理操作只在进程启动时运行一次，不会在 dump 写入过程中干扰。**

### 3.3 SD 卡上的其他写入进程

通过 `ls -lh /media/sdcard` 发现，以下文件/目录并非我们的程序写入：

| 文件/目录 | 写入方 |
|---|---|
| `stub-reg.log`（活跃更新）| 其他进程（非我们）|
| `erk-root/`（11 个子目录）| Quectel ERK 系统服务 |
| `uds_ota/` | OTA 升级服务 |
| `log/` | 未知 |

但这些进程写各自独立目录，不影响 `modem_dump/`，且 SD 卡空间充足（53.4G 可用），不存在空间竞争。

---

## 四、本地复现测试

### 4.1 测试方法

清空 `modem_dump` 目录后，手动触发 CP dump：

```bash
rm -rf /sdcard/modem_dump/*
echo a > /dev/acipc
```

### 4.2 测试结果

触发后约 15 秒，dump 完整写入：

```
1_cp_Assert_2026-5-27-7-49-15.tar     10.1M  ← 所有 dump 文件的打包
1_cp_Assert_2026-5-27-7-49-15.tar.gz   2.3M  ← 压缩完成后 .tar 自动删除
```

内核日志时序：
```
[7427.7]  CP crash 触发
[7443.3]  do_acipc_notify_cp_action timeout: 0x544f4f42（"BOOT"，CP 复位完成）
```

CP 给了 EEH 约 **15 秒**时间写完全部 dump 文件。

### 4.3 结论

- bind mount 机制工作正常，dump 文件正确写入 SD 卡
- 手动触发的 Assert 类型给 EEH 足够时间（15 秒），dump 完整
- **客户现场 `DSP_COM_ERR_0x8164` 错误类型的复位比 Assert 更快，EEH 来不及写完**

---

## 五、mrvl_tel_diag.cfg 排查

### 5.1 移远的排查方向

移远要求修改 `mrvl_tel_diag.cfg` 中 `[SDSettings]` 的 `log_size` 参数，并修改 `default_media`，观察是否影响 dump。

### 5.2 配置文件路径确认

设备上找到两个位置：
- `/etc/mrvl_tel_diag.cfg` ← **实际生效路径**（overlayfs 合并视图）
- `/overlay/etc/root/mrvl_tel_diag.cfg` ← 对应 `/etc/root/mrvl_tel_diag.cfg`，不同路径，无服务读取

文档要求修改的 `/system/etc/mrvl_tel_diag.cfg` 在本设备不存在。

### 5.3 修改测试及结果

按移远要求，分别修改：
1. `log_size=10` → `log_size=20`：重启触发 dump，**无变化**
2. `default_media=1` → `default_media=2`：重启触发 dump，**无变化**

### 5.4 原因分析（查阅官方文档后确认）

查阅《Quectel AG35 QuecOpen SDK Log 抓取指导 V1.0.0》后确认：

**`mrvl_tel_diag.cfg` 与 CP dump 是两套完全独立的机制：**

| 机制 | 控制方 | 作用 |
|---|---|---|
| **CP dump** | 内核 EEH 驱动（自动）| CP 崩溃时抓取内存快照写入 `/sdcard/modem_dump/` |
| **Diag Log 录制** | `mrvl_tel_diag.cfg` + `mrvl_tel_diag` 服务 | 正常运行时持续录制实时 CP 侧诊断日志（`.sdl` 文件）|

`default_media` 和 `log_size` 控制的是实时 Diag Log 的录制行为，对 CP dump 没有任何影响。

---

## 六、serial_atcmd 与 AT 口调查

### 6.1 固件版本确认

```bash
serial_atcmd at+qgmr
# → EC200ACNTAR02A04M2G_OCPU
```

**设备是 EC200A，不是 AG35。** 参考文档为 AG35 版本，部分内容不直接适用。

### 6.2 AT+QCFG="cpdump" 查询

尝试多种方式均返回 ERROR：

```bash
serial_atcmd AT+QCFG="cpdump"       # ERROR
serial_atcmd 'AT+QCFG="cpdump"'     # ERROR
serial_atcmd at+qcfg=\"cpdump\"      # ERROR
```

### 6.3 serial_atcmd 通信机制调查

```bash
ls /dev/ttyUSB* /dev/ttyAT* 2>/dev/null  # 无输出，设备上没有任何 tty 端口
strace serial_atcmd at 2>&1 | grep open
```

strace 输出显示，`serial_atcmd` 依赖库：
- `libmtel.so` —— Marvell 电信库，负责 AP→CP 的内部 IPC 通道
- `liblog.so` —— 日志库
- `libprop2uci.so` / `libuci.so` —— OpenWrt UCI 配置
- `libubox.so` —— OpenWrt 基础库

**整个过程没有打开任何 `/dev/tty*` 设备。**

### 6.4 内部 IPC 与物理串口的区别

| | 内部 IPC（本设备）| 物理串口（ttyUSB）|
|---|---|---|
| **适用场景** | OpenCPU 应用运行在模组内部 | 外部主机通过 USB 连接模组 |
| **通信路径** | AP → `libmtel.so` → 内核 IPC → CP | 外部主机 → USB → 虚拟串口 → CP |
| **是否需要 /dev/tty** | 否 | 是 |
| **典型工具** | `serial_atcmd` | CATStudio、QLog、minicom |

EC200A 是 OpenCPU 方案，应用直接运行在模组 AP 侧，通过 `libmtel.so` 内部 IPC 与 CP 通信，无需物理串口。

### 6.5 结论

`AT+QCFG="cpdump"` 返回 ERROR 是 CP 真实回复，不是工具传参问题。当前固件 `EC200ACNTAR02A04M2G_OCPU` 不支持该命令，需移远确认支持该命令的最低固件版本。

---

## 七、责任划分与结论

### 7.1 我方责任（已验证正常）

| 项目 | 状态 | 依据 |
|---|---|---|
| `/sdcard` bind mount 实现 | ✅ 正常 | 手动触发 dump 完整写入 SD 卡 |
| SD 卡空间管理 | ✅ 正常 | 53.4G 可用，清理逻辑仅启动时运行 |
| 对 dump 写入无干扰 | ✅ 正常 | 代码分析，写操作各自独立目录 |

### 7.2 移远侧问题

| 问题 | 描述 |
|---|---|
| `DSP_COM_ERR_0x8164` dump 截断 | 该错误类型的 CP 复位时序过快，EEH 未能完成写盘 |
| `AT+QCFG="cpdump"` 不支持 | 当前固件不支持该命令，无法确认/修改 dump 配置 |

---

## 八、待移远回答的问题

1. **`DSP_COM_ERR_0x8164` 错误的复位流程中，是否有等待 EEH dump 写盘完成的保护机制？**
   - 手动触发 Assert（`echo a > /dev/acipc`）给 EEH 约 15 秒时间，dump 完整
   - 该错误类型复位明显更快，导致 EEH 仅写了第一个文件（`com_DDR_RW.bin`）就被中断

2. **`AT+QCFG="cpdump"` 在固件 `EC200ACNTAR02A04M2G_OCPU` 上返回 ERROR，支持该命令的最低固件版本是什么？**

3. **`mrvl_tel_diag.cfg` 修改对 dump 无影响已实验证实，移远要求修改该文件的目的是什么？预期应观察到什么变化？**

---

## 九、附：设备环境信息

```
固件版本：EC200ACNTAR02A04M2G_OCPU
应用版本：v1.27.5
SD 卡：/dev/mmcblk0p1，56.4G，已用 122.1M，可用 53.4G
挂载：/dev/mmcblk0p1 → /media/sdcard（原始）
      /dev/mmcblk0p1 → /sdcard（bind mount，由 dial 建立）
AT 命令通信：libmtel.so 内部 IPC（无物理串口）
```
