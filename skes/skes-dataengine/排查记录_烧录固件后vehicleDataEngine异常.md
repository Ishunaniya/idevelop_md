# 排查记录：烧录 make package 固件后 vehicleDataEngine 异常

> 会话日期：2026-06-19
> 设备：RK3576-Tronlong
> 进程：`vehicleDataEngine`（skes-dataengine，C++17）
> 好的版本基线：`d8a1a9e`（feat/jira::v0.1.0.3.alpha30）

---

## 一、现象（用户提供的设备终端记录）

```text
root@RK3576-Tronlong:~# touch /tmp/vde_log_enable
root@RK3576-Tronlong:~# killall vehicleDataEngine
root@RK3576-Tronlong:~# ps aux | grep vehicleDataEngine
root      5583  0.0  0.0   6472  1204 pts/0    S+   18:17   0:00 grep vehicleDataEngine
root@RK3576-Tronlong:~# tail -f /tmp/vehicleDataEngine.log
tail: cannot open '/tmp/vehicleDataEngine.log' for reading: No such file or directory
tail: no files remaining
root@RK3576-Tronlong:~# tail -f /tmp/vehicleDataEngine.log
[SkesQtBridge] PUB 已连接 tcp://127.0.0.1:19225
[SkesQtBridge] SUB 已连接 tcp://127.0.0.1:19226 (filter: /skes/filter)
[SkesQtBridge] io_mng命令总线已连接 tcp://127.0.0.1:26008
[CanTxMgr] 已启动 (tick=100ms)
[SkesQtBridge] 启动完成 (推送间隔=0.5s)
[main] 启动完成，进入事件循环...
[保养提示] 首次保养 总工时=2500.0h
[故障信息] 新增 type=8(保养提示) code=8000 time=2026-06-18 18:17:25
[GPS] nn_recv: Interrupted system call
[NetworkStatusQuery] nn_recv failed: Connection timed out
[GPS] nn_recv: Interrupted system call
[GPS] nn_recv: Interrupted system call
... (持续刷屏 [GPS] nn_recv: Interrupted system call)
^C
root@RK3576-Tronlong:~# ps aux | grep veh
root      7804  100  0.1 185664  7160 ?        Sl   18:17   1:37 ./vehicleDataEngine
root     19014  0.0  0.0   6472  1284 pts/0    S+   18:19   0:00 grep veh
```

**用户问题**：工作区源码烧入设备后变成这样，之前（`d8a1a9e`）都是好的，是什么原因？

可观察到的"异常"事实：
1. `killall` 后进程没了，但很快又冒出一个**全新 PID（7804）**，且 **CPU 占用 100%**。
2. 日志里 `[GPS] nn_recv: Interrupted system call` 持续刷屏。
3. `[NetworkStatusQuery] nn_recv failed: Connection timed out`。

---

## 二、逐条事实分析（基于源码核实）

### 1. "killall 杀掉又冒出来 + 100% CPU" —— 守护进程重新拉起，PID 变化属正常

- `skes-scripts/skes-sepd.sh` 是常驻守护循环：
  ```sh
  run() { ... while :; do run_seps; sleep 5; done }
  ```
  `run_seps` 中 `check_proc_exists ${sep}`，发现进程没了就 `run_sep_once ./${sep}`，启动器是 `run.sh`。
- 所以 `killall` 后 **≤5 秒内被守护进程重新拉起**，PID 从 5583（其实那是 grep 自己）变成全新的 **7804**，日志里又出现整套开机横幅 —— 这是**新进程**，不是旧进程没死。
- 结论：单纯 `killall vehicleDataEngine` 永远停不掉，必须停守护进程（走 sepd 的 kill 路径）。**PID 变化本身不是 bug。**

### 2. "第一次 tail 没文件、第二次有了" —— 新的日志开关机制，属正常

`skes-apps/skes-dataengine/run.sh`：
```sh
LOG=/tmp/vehicleDataEngine.log
FLAG=/tmp/vde_log_enable
if [ -f "$FLAG" ]; then
    exec ./vehicleDataEngine "$@" >> "$LOG" 2>&1
else
    exec ./vehicleDataEngine "$@" > /dev/null 2>&1
fi
```
- 日志文件只有在 `/tmp/vde_log_enable` 存在 **且进程（重新）启动后**才会创建。
- `sep.ini` 的入口已改为 `cmd=run.sh`。
- **关键认知：这套日志开关是新增的（commit `840b98e`）。在此之前的固件 stdout 全丢 `/dev/null`，你从来看不到任何输出。** 所以 `[GPS] Interrupted` / `modem timed out` 这类信息，很可能在旧固件里一直存在，只是不可见。"现在能看到报错" ≠ "现在才坏"。

### 3. `[GPS] nn_recv: Interrupted system call` —— EINTR，被子进程信号打断

GPS 接收线程（`main.cpp:303-323`）是**阻塞** `nn_recv`，出错时打印并 `sleep(1)`：
```cpp
int bytes = nn_recv(sock, &buf, NN_MSG, 0);   // 阻塞
...
} else {
    std::cerr << "[GPS] nn_recv: " << nn_strerror(nn_errno()) << "\n";
    sleep(1);
}
```
- "Interrupted system call" = **EINTR**：阻塞的 `nn_recv` 被信号（SIGCHLD）打断。
- 信号源：本进程反复 fork 子进程 —— `main.cpp:81` 的 `networkProbeThread` 每 5s `system("ping 8.8.8.8")`，以及 `SkesQtBridge.cpp` 里的 `system(...)` 调用。
- 这条本身基本无害（有 sleep(1) 兜底），但把 EINTR 当错误打印是代码异味（应静默重试）。

### 4. `[NetworkStatusQuery] nn_recv failed: Connection timed out` —— 对端 modem_mng_v2 没起来

`core/NetworkStatusQuery.cpp:135-205`：用 NN_REQ 连 `modem_mng_v2`（默认端口 38001），1000ms 超时。设备上该对端进程没运行/没应答 → REQ 等不到回复 → 超时。属"依赖的 SEP 没起齐"，非本程序逻辑 bug。

### 5. 100% CPU —— 静态层面只能定位到结构性隐患

三个接收循环对比：
- **GPS 线程**（`main.cpp`）：出错有 `sleep(1)` → 有节流，单独不会 100%。
- **CAN 线程**（`can/can_receiver.cpp:66-71`）：`nn_poll` 出错直接 `return -1`，线程退出 → 不会 100%（但 CAN 接收会永久死掉，另一个隐患）。
- **`NanoSubscriber::loop`（`core/NanoSubscriber.cpp:64-87`）出错分支为空**（只有注释"可加重试/休眠"，无 sleep/break/节流）：
  ```cpp
  while (running_) {
      int n = nn_recv(sock_, &buf, NN_MSG, 0);
      if (n >= 0) { ... }
      else {
          // 可加重试/休眠   <-- 空，若 nn_recv 持续返回错误则空转占满一个核
      }
  }
  ```
- 这是代码里唯一"出错不节流"的接收循环，是 100% CPU 的首要结构性嫌疑。**但无法纯静态断定它就是当时占满核的线程**，需上机 `top -H` 确认。

---

## 三、第一次判断（被自我否定）

最初怀疑 commit `48c41d1`（2026-03-12）引入的：
1. `networkProbeThread`（`system(ping)` 每 5s → SIGCHLD → EINTR 噪声）；
2. 持久化路径迁移到 `/data/forklift_persistent.json`（新烧机器 /data 可能空/未挂载 → 首启失败）。

**否定原因**：`48c41d1` 是 3 月的提交，早已包含在"好"的基线 `d8a1a9e`(alpha30) 里，因此不是本次回归。
用户也确认 `/data` 不是空的。**回归只可能在 `d8a1a9e` 之后。**

---

## 四、锁定回归范围：`d8a1a9e..HEAD` + 工作区未提交改动

```text
$ git diff --stat d8a1a9e..HEAD
 .../skes-dataengine/core/RealtimeStatusManager.cpp | 168 ++++++--------
 skes-scripts/install.sh                            |   2 +-
 skes-ui/skes-main_fork-lift/VehicleController.cpp  |  56 ++++--
 skes-ui/skes-main_fork-lift/VehicleController.h    |   6 +-

$ git log --oneline d8a1a9e..HEAD
 2be064e Merge branch 'main_ui' ...
 67870fa fix/jira::1.install:修正拼写错误导致可能脚本不能正常kill掉的问题;
 6bd11d9 feat/jira::BCM故障码:接入faultInfo.faultList(BCMERRF/K字符串码);

$ git status --short
 M skes-3rd-party/curl-git/build.sh
 M skes-apps/CMakeLists.txt
 M skes-apps/skes-dataengine/vehicles/forklift/SkesQtBridge.cpp   ← 未提交！
 M skes-ui/CMakeLists.txt
 M skes-ui/skes-main_fork-lift/VehicleController.cpp
```

dataengine 侧，相对好基线只有两处差异：

| 来源 | 内容 | 状态 |
|---|---|---|
| commit `6bd11d9` | `RealtimeStatusManager.cpp` BCM 故障码（faultList 字符串码，按 CAN 帧处理，仅 changed 时重建，非明显 100% 来源） | 已提交 |
| **工作区未提交** | `SkesQtBridge.cpp` 新增"系统时间/时区/NTP"整套功能（200+ 行） | **未 commit** |

> `make package` 是从**当前工作区**打的，所以未提交代码也进了固件。

### 铁证：未提交代码确实在固件里

日志中这一行只存在于工作区未提交的新代码（`IOMNG_CMD_URL` / "io_mng命令总线已连接"），`d8a1a9e` 没有：
```
[SkesQtBridge] io_mng命令总线已连接 tcp://127.0.0.1:26008
```
**故确认：烧入固件 = `d8a1a9e` + `6bd11d9`(BCM故障码) + 未提交的"时间功能"。**

### 这段未提交代码"启动时"就改变系统行为

`SkesQtBridge.cpp` 的 `loadPersistent()`（开机即执行）新增：
```cpp
applyTimezone(s_settings[SET_TIMEZONE_IDX]);     // system("ln -sf /usr/share/zoneinfo/... /etc/localtime")
setNtpEnabled(s_settings[SET_AUTO_DATETIME]!=0); // 默认ON → system(S49chrony start) + system(S49ntp start)
```
即 **vehicleDataEngine 一启动就改 `/etc/localtime`、同时拉起 chronyd + ntpd（两者都抢 UDP123，本身冲突）、并新开连 26008 的 socket**。再叠加每 5s 的 `system(ping)`，启动期 `system()`/子进程明显增多 —— 与 `[GPS] Interrupted system call` 变密集吻合。这些 `d8a1a9e` 完全没有。

---

## 五、当前结论（怀疑 + 确信度分层）

### 首要嫌疑（硬证据，非猜测）
**工作区里那段未提交的"时间/NTP"功能（`SkesQtBridge.cpp`）。**
- 是 `d8a1a9e` 之后唯一"启动时主动跑 shell / 动系统"的新运行时代码；
- 日志 `io_mng命令总线已连接 26008` 证明它确实进了固件；
- 同时起 chronyd 和 ntpd 是冲突配置，叠加 `system(ping)` 使子进程/信号变多。

### 不敢打包票的部分（必须上机量才能确认）
**100% CPU 具体是哪个线程，无法纯静态断定。** EINTR 每 5s 一次不足以烧满一个核，所以一定还有更高频触发源或某个空转循环（如 `NanoSubscriber::loop`）在转，这点光看代码定不下来。

需在设备上执行：
```sh
top  -H -p $(pidof vehicleDataEngine)              # 看哪个 TID 占 100%
ps   -L -p $(pidof vehicleDataEngine) -o tid,pcpu,comm
strace -f -p <占100%的TID> 2>&1 | head -50         # 它反复做什么 syscall / 被什么信号打断
```

---

## 六、推荐的定位步骤（二分排查）

当前是拿"脏工作区"在烧，变量太多。建议：

```sh
# 1) 暂存未提交改动，回到纯提交状态
git stash

# 2) 从干净状态重新打包、烧录（若想连 6bd11d9 一起排除，可 git checkout d8a1a9e 再打）
make package
```

- 重烧后**正常** → 实锤是 stash 掉的未提交改动（首要怀疑：SkesQtBridge 时间/NTP 功能，启动就跑 system()）。
- 重烧后**仍异常** → 范围缩到 `6bd11d9`（BCM 故障码），再单独回退验证。

---

## 七、附：本次涉及的关键代码位置

| 文件 | 位置 | 说明 |
|---|---|---|
| `skes-apps/skes-dataengine/run.sh` | 53-57 | 日志开关（新增，commit 840b98e） |
| `skes-apps/skes-dataengine/sep.ini` | `cmd=run.sh` | 启动入口经 run.sh |
| `skes-scripts/skes-sepd.sh` | `while :; do run_seps; sleep 5; done` | 守护进程，5s 重新拉起 |
| `skes-apps/skes-dataengine/main.cpp` | 79-85 | `networkProbeThread`，每 5s `system(ping)` |
| `skes-apps/skes-dataengine/main.cpp` | 303-325 | GPS `LocationReceiverThread`，阻塞 nn_recv + sleep(1) |
| `skes-apps/skes-dataengine/core/NanoSubscriber.cpp` | 64-87 | **出错分支为空，空转隐患** |
| `skes-apps/skes-dataengine/can/can_receiver.cpp` | 66-71 | nn_poll 出错即 return，CAN 线程退出 |
| `skes-apps/skes-dataengine/core/NetworkStatusQuery.cpp` | 135-205 | 4G 查询 REQ，对端无应答即超时 |
| `skes-apps/skes-dataengine/vehicles/forklift/SkesQtBridge.cpp` | 254-460(新增) | **未提交的时间/时区/NTP 功能；loadPersistent 启动即 system()** |
| `skes-apps/skes-dataengine/core/RealtimeStatusManager.cpp` | (6bd11d9) | BCM 故障码 faultList |

---

## 八、一句话总结

不是"环境变了"或"固件包本身坏了"，而是 **这版固件是从带未提交代码的脏工作区构建的** —— 比好基线 `d8a1a9e` 多了 `6bd11d9` 的 BCM 故障码，以及一整套**还没提交、开机就执行 shell（改时区 / 起 chronyd+ntpd）的时间设置功能**。`io_mng命令总线已连接 26008` 这行日志已证明它在固件里。先 `git stash` 从干净状态重烧一次即可分清；100% CPU 的确切线程用 `top -H` + `strace` 一秒锁定。

---

## 附录 A：未提交的"系统时间/时区/NTP"功能完整拆解（SkesQtBridge.cpp）

> 这是相对好基线 `d8a1a9e` 唯一新增的、且**开机就主动动系统**的运行时代码，约 200+ 行，全部**未提交**。
> 方案说明（代码顶部注释）：MCU 电池 RTC(PCF85163) 为时间权威；系统钟存 UTC，时区仅影响本地换算/显示。
> 契约见 rtms_sdk：`apps/io_mng/rk3576_MCU_RTC_接口契约.md`（epoch 口径 = UTC）。

### A.1 新增的全局状态与设置项

```cpp
// 设置项枚举新增 3 项（掉电保存）
SET_AUTO_DATETIME,   // 自动设置日期时间（0=手动/RTC 1=NTP自动）
SET_TIMEZONE_IDX,    // 时区索引（0~70，对应 UI 下拉）
SET_24H_FORMAT,      // 24 小时制（0/1，纯显示）

#define IOMNG_CMD_URL "tcp://127.0.0.1:26008"   // io_mng 命令总线 SUB(bind)，本侧 PUB 连入
static int         s_iomng_cmd_sock = -1;        // → io_mng 下发 CMD_TIME_SET
static std::string s_pending_date;               // UI 下发日期 "yyyy/MM/dd"
static std::string s_pending_time;               // UI 下发时间 "HH:mm" 或 "h:mm AP"
static const char* const TZ_TABLE[] = { ... };   // UI 下拉 index→IANA 时区名，共 71 项(0~70)
```

### A.2 新增的函数（数据流）

| 函数 | 作用 | 关键调用 |
|---|---|---|
| `sendTimeSetToMcu(uint32_t utc_epoch)` | 下发 `CMD_TIME_SET`(tag=100) 给 io_mng → 转发 MCU 写 PCF85163 持久化。载荷 = UTC epoch 秒(LE)，16 字节包：pid(4)+tag(2)+idx(1)+len=4(1)+rsv(4)+epoch(4) | `nn_send(s_iomng_cmd_sock, pkt, 16, NN_DONTWAIT)` |
| `setSystemClockUTC(int64_t utc_epoch)` | 立即设系统钟 CLOCK_REALTIME(=UTC)，需 root | `clock_settime(CLOCK_REALTIME, &ts)` |
| `applyTimezone(int idx)` | 写 `/etc/localtime` + `setenv(TZ)` + `tzset()`；越界回退 0(北京) | **`system("ln -sf /usr/share/zoneinfo/<zone> /etc/localtime")`** |
| `setNtpEnabled(bool on)` | ON=起 chronyd+ntpd；OFF=停两者+兜底 killall | **`system("/etc/init.d/S49chrony start/stop")` + `system("/etc/init.d/S49ntp start/stop")` + `system("killall chronyd ntpd")`** |
| `tryApplySystemTime()` | UI 本地日期+时间凑齐 → `mktime` 按当前时区算 UTC epoch → ①`setSystemClockUTC` 立即生效 ②`sendTimeSetToMcu` 持久化到 MCU RTC | 含 12/24 小时制(AM/PM)解析 |

> 数据流总览：
> - 手动设时：UI → `tryApplySystemTime()` → ① `clock_settime` 立即生效 ② `CMD_TIME_SET` → io_mng(26008) → MCU 写 PCF85163
> - 开机灌钟：MCU 读 PCF85163 → io_mng `set_time_from_mcu` → `date -s`（io_mng 侧已实现，本侧不改）
> - 时区：index → `/etc/localtime`
> - 自动开关：ON 启用 NTP / OFF 停 NTP，手动+RTC 为准

### A.3 启动时就执行（关键回归点）

`loadPersistent()`（开机即调用）末尾新增：
```cpp
applyTimezone(s_settings[SET_TIMEZONE_IDX]);        // 开机即 system("ln -sf .../etc/localtime")
setNtpEnabled(s_settings[SET_AUTO_DATETIME] != 0);  // 默认 autoSetDateTime=1 → 开机即起 chronyd + ntpd
```
**即 vehicleDataEngine 一启动就改 `/etc/localtime`、同时拉起 chronyd 与 ntpd（两者都抢 UDP123，配置冲突）。** `d8a1a9e` 完全没有这些。

### A.4 新增的 UI 命令处理（handleCmdControl）

新增 5 个 property 分支：
| property | 行为 |
|---|---|
| `autoSetDateTime` | `setNtpEnabled(v)` + 持久化 + 回写 `chache.system` |
| `timezoneIndex` | `applyTimezone(v)` + 持久化 + 回写 |
| `twentyFourHoursFormat` | 纯显示，仅持久化供 UI 回读 |
| `systemDate` | 存 `s_pending_date` → `tryApplySystemTime()` |
| `systemTime` | 存 `s_pending_time` → `tryApplySystemTime()` |

### A.5 start()/stop() 新增 socket 管理

```cpp
// start()：新建 io_mng 命令总线 PUB，connect 到 26008（失败不致命，仅 RTC 持久化不可用）
s_iomng_cmd_sock = nn_socket(AF_SP, NN_PUB);
nn_connect(s_iomng_cmd_sock, IOMNG_CMD_URL);   // 日志打印 "[SkesQtBridge] io_mng命令总线已连接 ..."  ← 铁证来源

// stop()：新增 s_iomng_cmd_sock 的 nn_close 清理
```
持久化文件落盘新增三字段：`autoSetDateTime / timezoneIndex / twentyFourHoursFormat`；
500ms 推送(buildAndPublishStatus)也新增这三项 addProp，供 UI 重启后恢复设置页状态。

---

## 附录 B：/data 持久化文件的三处实际位置（grep 实测）

```text
core/RealtimeStatusManager.cpp:101   #define PERSISTENT_DATA_FILE "/data/forklift_persistent.json"
core/RealtimeStatusManager.cpp:2295  // 注释：与 SkesQtBridge 共享同一文件
vehicles/forklift/SkesQtBridge.cpp:65 #define PERSISTENT_FILE  "/data/forklift_persistent.json"
profiles/forklift/vehicle_profile.json:14  "persistent_file": "/data/forklift_persistent.json"
tools/skes_qt_bridge_tester.cpp:49   // 测试说明里引用
```
> 注：该 `/data` 迁移来自 3 月的 `48c41d1`，**已在好基线 d8a1a9e 内**，故本次回归与它无关；用户也确认 `/data` 非空。此处仅留档备查。

---

## 附录 C：`67870fa` install.sh 拼写修正（kill 脚本）

```diff
commit 67870fa  fix/jira::1.install:修正拼写错误导致可能脚本不能正常kill掉的问题
--- a/skes-scripts/install.sh
+++ b/skes-scripts/install.sh
@@ make_install() @@
     killall skes-main
     echo "kill all apps..."
-    $2/apps/skes-apps.sh --action kill
+    $2/apps/skes-appd.sh --action kill   # 文件名拼错(apps→appd)，导致安装时 kill 旧 app 失败
```
> 只改 install.sh 安装流程，不影响 vehicleDataEngine 运行期行为；与本次 100% CPU/EINTR 无直接关系，留档。

---

## 附录 D：本次会话执行过的主要核查命令（可复现）

```sh
# 定位日志/接收逻辑来源
grep -rn "nn_recv: Interrupted\|NetworkStatusQuery" --include=*.cpp --include=*.h skes-apps/skes-dataengine
grep -rn "nn_recv\|\[GPS\]" skes-apps/skes-dataengine/main.cpp

# 守护进程/日志开关
grep -rn "while\|restart\|killall" skes-scripts/*.sh
sed -n '150,175p' skes-scripts/skes-sepd.sh
cat skes-apps/skes-dataengine/run.sh skes-apps/skes-dataengine/sep.ini

# 第一次怀疑及否定（48c41d1 在好基线之前）
git log --oneline -S 'ping -c 1 -W 1 8.8.8.8' -- skes-apps/skes-dataengine/main.cpp   # → 48c41d1
git show --stat 48c41d1

# 锁定真实回归范围
git diff --stat d8a1a9e..HEAD
git log   --oneline d8a1a9e..HEAD
git status --short
git show  6bd11d9 -- skes-apps/skes-dataengine/core/RealtimeStatusManager.cpp
git diff  -- skes-apps/skes-dataengine/vehicles/forklift/SkesQtBridge.cpp   # 未提交时间功能
git diff  d8a1a9e -- skes-apps/skes-dataengine/vehicles/forklift/SkesQtBridge.cpp
grep -rn '/data/forklift_persistent.json' --include=*.cpp --include=*.h --include=*.json skes-apps/skes-dataengine
git show  67870fa -- skes-scripts/install.sh
```
