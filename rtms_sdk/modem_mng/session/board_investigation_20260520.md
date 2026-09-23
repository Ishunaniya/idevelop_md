# 开发板启动链路调查记录

**日期：** 2026-05-20  
**设备：** EG25 平台开发板  
**目的：** 搞清楚 `modem_mng` 是如何被启动和管理的，以及与同事描述的差异

---

## 一、起因与问题背景

同事说："暂时是把 modem_mng 重命名成 dial 处理掉的"，意思是将 cmake 编译出的 `modem_mng` 改名为 `dial`，放到 `/usr/dial/` 下替换旧版本。

但板子上的实际情况与此描述不符，因此展开调查。

---

## 二、板子上二进制文件的实际状态

```
路径                    大小      时间            状态
/usr/dial/dial          218.8K   2024-07-22      未运行（旧版本遗留）
/usr/bin/modem_mng      282.3K   2025-04-15      正在运行（PID 826）
```

**关键命令输出：**
```sh
# ps aux | grep modem_mng
  826 root       0:00 /usr/bin/modem_mng

# find / -name "modem_mng" 2>/dev/null
/usr/bin/modem_mng

# ls -lh /usr/bin/modem_mng
-rwxrwxrwx    1 1005     1005      282.3K Apr 15  2025 /usr/bin/modem_mng

# ls -lh /usr/dial/dial
-rwxr-xr-x    1 mosquitt 1001      218.8K Jul 22  2024 /usr/dial/dial
```

**结论：** 这块板子没有按"重命名成 dial"的方式部署。`/usr/dial/dial` 是 2024 年的旧版本，没有任何进程在使用它；`/usr/bin/modem_mng` 是 2025 年 4 月部署的新版本，是当前实际运行的程序。

---

## 三、文件 Owner 说明（mosquitt 是什么）

`ls -lh` 中大量文件显示 owner 为 `mosquitt 1001`，这是原厂 Quectel 固件在 Yocto 构建系统中打包时留下的账号。`mosquitto` 是一个开源 MQTT broker，`mosquitt` 是其在构建机上对应的服务账号（显示时截断了末尾字母）。

`/opt/start_daemon.sh` 中明确有：
```sh
/usr/bin/mosquitto -c /etc/mosquitto/mosquitto.conf > /dev/null 2>&1 &
```

| Owner | UID | 含义 |
|---|---|---|
| `mosquitt` | 1001 | 原厂出厂固件文件 |
| （无用户名）| 1005 | 团队后续写入的自定义文件 |

板子的 `/etc/passwd` 中没有 UID 1005 的对应条目，所以显示为数字。

---

## 四、init.d 脚本概览

`/etc/init.d/` 中由团队添加的自定义脚本（owner 1005）：

| 文件 | 时间 | 作用 |
|---|---|---|
| `start_daemon` | 2024-11-11 | 调用 `/opt/start_daemon.sh` |
| `start_rtms` | 2024-11-11 | 启动 io_mng、sw_mng |
| `start_emac_le` | 2025-04-11 | 网络相关 |
| `start_io_mng` | 2025-04-10 | IO 管理 |
| `start_spi` | 2024-11-11 | SPI 相关 |
| `start_sshd` | 2024-11-11 | SSH 服务 |

注意：板子上没有 `rc.local`（`find / -name "rc.local"` 无结果）。

---

## 五、完整启动链路

### 5.1 链路一：modem_mng 启动链路

```
开机
  └── /etc/rc5.d/S20rtms
        ├── /usr/bin/io_mng > /dev/null 2>&1 &
        ├── /usr/bin/sw_mng > /dev/null 2>&1 &         ← 进程管理器/看门狗
        └── /usr/bin/rtms_monitor.sh > /dev/null 2>&1 & ← 此文件不存在，静默失败
```

`sw_mng` 启动后读取 `/opt/apps.json`，按 runlevel 顺序启动并持续监控各进程：

```
sw_mng (PID 626)
  └── /opt/apps.json
        ├── runlevel 1: /usr/bin/modem_mng   ← 拨号管理
        ├── runlevel 1: /usr/bin/sys_mng      ← 系统管理
        ├── runlevel 2: /media/sdcard/erk-root/erk_daemon1.sh
        ├── runlevel 3: /usr/bin/network_mng  ← 网络管理
        └── runlevel 3: /opt/model_client
```

**注意：** `S20rtms` 与 `/etc/init.d/start_rtms` 内容完全相同，是同一脚本的两个副本。

### 5.2 链路二：start_daemon 链路（与 modem_mng 无关）

```
开机
  └── /etc/rc5.d/S100start_daemon
        └── /etc/init.d/start_daemon
              └── /opt/start_daemon.sh
                    ├── 防重入检查（/tmp/start_daemon.pid）
                    ├── 执行一次性初始化（/opt/part.sh）
                    ├── 处理 OTA 升级包（/usrdata/ir_ota.tar.gz）
                    ├── 挂载 SD 卡（/dev/mmcblk0p1 → /media/sdcard，最多重试 5 次）
                    ├── 安装/更新 erk-agent（从 /usrdata/erk-agent*.tar.gz）
                    ├── 加载内核模块（cdc-acm.ko）
                    ├── 启动 sshd
                    ├── 启动 mosquitto（MQTT broker）
                    ├── 启动 factoryApp / get_sim_info / factory_client
                    ├── 启动 erk-daemon.sh（若存在）
                    └── while true 循环（每 30 秒）监控并重启：
                          ├── cloud_client / ota_client（或按 device_type 启动对应 client）
                          ├── mosquitto
                          └── factory_client
```

---

## 六、`/opt/apps.json` 完整内容

```json
[{
    "program":   "/usr/bin/modem_mng",
    "arguments": "",
    "runlevel":  1
}, {
    "program":   "/usr/bin/network_mng",
    "arguments": "",
    "runlevel":  3
}, {
    "program":   "/media/sdcard/erk-root/erk_daemon1.sh",
    "arguments": "",
    "runlevel":  2
}, {
    "program":   "/opt/model_client",
    "arguments": "",
    "runlevel":  3
}, {
    "program":   "/usr/bin/sys_mng",
    "arguments": "",
    "runlevel":  1
}]
```

---

## 七、sw_mng 看门狗验证实验

### 7.1 实验过程

```sh
# 第一次：发送 SIGTERM
kill 826
sleep 5
ps aux | grep modem_mng
# 结果：PID 仍为 826，进程未消失
```

第一次 kill 后 PID 不变，原因待确认：可能是 `modem_mng` 捕获并忽略了 SIGTERM，也可能是 `sw_mng` 在极短时间内以相同 PID 重启了进程（概率较低但不能排除）。

```sh
# 第二次：发送 SIGKILL
kill -9 826
# 随后多次查询，modem_mng 消失
# 等待一段时间后
ps aux | grep modem_mng
# 结果：modem_mng 以新 PID 4628 重新出现
```

### 7.2 实验结论

| 观察 | 结论 |
|---|---|
| `kill -9` 后进程消失 | SIGKILL 成功终止了进程 |
| 等待后出现新 PID 4628 | `sw_mng` 确实有看门狗功能，会重拉死掉的进程 |
| kill 后网络（ping 8.8.8.8）保持畅通 | 数据通道由底层驱动/modem 硬件维持，modem_mng 死后连接不会立刻断 |
| 轮询间隔较长 | 从 kill 到重启耗时若干十秒（精确值未测量，见下方命令） |

`strings /usr/bin/sw_mng` 中确认的看门狗相关字符串：
```
"program": "/usr/bin/modem_mng",
modem_mng is not running. Attempting to start it.
Cannot execute modem_mng:
```

### 7.3 精确测量看门狗轮询间隔

```sh
kill -9 $(pgrep modem_mng) && start=$(date +%s) && \
until pgrep modem_mng > /dev/null; do sleep 1; done && \
echo "重启耗时: $(($(date +%s) - start)) 秒"
```

---

## 八、未调查的项目（后续可跟进）

| 项目 | 路径 | 备注 |
|---|---|---|
| 网络检查脚本 | `/etc/rc5.d/S60start_check_network` | 内容未查看，可能与网络恢复有关 |
| 网络管理进程 | `/usr/bin/network_mng` | apps.json runlevel 3，功能未调查 |
| 系统管理进程 | `/usr/bin/sys_mng` | apps.json runlevel 1，功能未调查 |
| sw_mng 源码 | 无 | 轮询间隔、重试策略、重试次数上限均未知 |
| SIGTERM 行为 | `/usr/bin/modem_mng` | 第一次 kill 后 PID 未变的原因未最终确认 |

---

## 九、关键遗留文件（脚本引用但实际不存在）

| 文件 | 被引用位置 | 状态 |
|---|---|---|
| `/usr/bin/rtms_monitor.sh` | `S20rtms` / `start_rtms` | 不存在，启动时静默失败 |
| `/etc/rc5.d/S101rtms` | `start_daemon`（chmod +x） | 不存在 |
