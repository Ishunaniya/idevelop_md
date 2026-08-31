# 03 — misc.c + misc.h

## 一、文件职责概述

`misc.c` 是通用工具集合：AT 命令 popen 封装、SIM/网络注册状态检查、退出计数持久化、idle 状态判定、网络服务重启（ql_rild/ql_netd/ql_netcall）、进程计数、网络状态文件读写、单调时钟/时间戳、CPU 温度、连通性 ping、目录清理。`misc.h` 声明其全部对外函数。

> 外部全局：`g_callid`（定义于 data_call.c:5）、`NW_STATUS_PATH="/tmp/network_status"`（nw.h:10）、`g_config`（reboot_conf）。

## 二、逐函数分析

### `char *executeATCommand(const char *cmd)`（`misc.c:9-45`）
- `popen(cmd,"r")` → 循环 `fgets` 到动态 `realloc` 的 result，返回堆字符串（调用方负责 free）。popen 失败释放并返回 NULL；realloc 失败释放返回 NULL。
- **潜在点**：`malloc(1)` 返回值未判空即 `*result='\0'`（`misc.c:12-13`），极端 OOM 下解引用空指针（实际几乎不触发）。

### `int extractSignalValue(const char *input)`（`misc.c:48-63`）
- `strstr(input,"+CSQ: ")` 后 `sscanf("%d")` 取信号值；未找到返回 -1 并打错误。

### `int checkSimCardStatus()`（`misc.c:72-114`）
- 先 `at+creg?`：含 `+CREG: 0,1` 或 `+CREG: 0,5` 视为注册正常，否则返回 2；response 为 NULL 返回 2。
- 再 `at+cpin?`：含 `READY` 返回 0，否则返回 1；NULL 返回 1。
- 返回：0=SIM/网络均正常，1=SIM 不正常，2=网络注册不正常。

### `int checkCommandSuccess(const char *cmd)`（`misc.c:116-132`）/ `int isModuleOK()`（`misc.c:134-151`）
- 执行命令（isModuleOK 固定 `serial_atcmd at`），输出含 `OK` 返回 1，否则/失败返回 0。

### `int read_exit_count()` / `void write_exit_count(int count)`（`misc.c:154-181`）
- 读写 `EXIT_COUNT_FILE=/tmp/exit_count.txt` 的整数。read 文件不存在返回 0；write 失败打 errno。**fscanf 返回值未检查**。

### `int isIdleState()`（`misc.c:185-232`）
- `wlan0` 不存在 → 返回 1（idle）。
- `iw wlan0 info` → `parseOutput type` → 若 AP 模式：`hostapd_cli status` 解析 `num_sta[0]=`，`>0` 则 isIdle=0；若 STA 模式：`iw wlan0 link` 不含 "Not connected" 则 isIdle=0。
- **调用方**：`restartNetworkServices`（misc.c）与 diag（见 09）。**dial_loop 主恢复逻辑并未调用 isIdleState**（L3 exit 无空闲门控，与 CLAUDE.md "后续另加更高一级+空闲门控"待办一致）。

### `void restartNetworkServices()`（`misc.c:236-285`）
- `isIdleState()==0`（非 idle，有 wifi 使用者）→ 直接 return，不重启。
- 否则 `killall -9 ql_rild`，双重 fork 起 `/usr/bin/ql_rild` 和 `/usr/bin/ql_netd`（孙进程被 init 收养避免僵尸，中间进程 waitpid 回收）。
- **注意**：此函数在本工程内**未见被调用**（需实测确认；grep 仅其自身定义）。

### `int runCommand(const char* command, char* output, int outputSize)`（`misc.c:290-321`）
- `popen` 逐行 append 到 output（带溢出保护 `strlen(output)+strlen(line) < outputSize`）。成功 0，popen/pclose 失败 -1。

### `int parseOutput(const char* output, const char* key, char* value, int valueSize)`（`misc.c:326-348`）
- `strstr(key)` 后跳空格，取到 `\n` 或串尾，`strncpy` 到 value（截断到 valueSize-1），末尾补 `\0`。找到返回 0，否则 -1。

### `void log_to_file(...)`（`misc.c:350-378`）
- **当前 `#if 1` 生效版本是空函数体**（`misc.c:351-353`）——`log_to_file` 实际什么都不做。`#else` 分支（写 `/media/sdcard/call_fault.log`）未编译。
- **注意**：main.c 的 `restart_cfun`/`log_to_file` 调用因此**无任何输出**（见潜在问题）。

### `void readCallIdFromFile()` / `void writeCallIdToFile()`（`misc.c:382-412`）
- 读写 `/tmp/callid` 中的 `g_callid`。文件不存在时 read 置 `g_callid=DATA_CALL_ID_PUBLIC(1)` 并写回。
- **注意**：`g_callid` 默认 1，但 dial_loop 用的是硬编码 `g_call_id=4`（局部变量，与全局 `g_callid` 无关）。这两个是不同变量。

### ql_netcall 管理（`misc.c:416-455`）
- `start_ql_netcall`：`system("ql_netcall -p 1 -r 5 -d &")`。
- `is_ql_netcall_running`：`check_process("ql_netcall") >= 1`。
- `stop_ql_netcall`：`killall -9 ql_netcall`。
- `restart_ql_netcall`：stop→sleep(1)→start，`while(1)` 监视直到运行才 break（每 5s 检查）。
- **注意**：这组函数在运行期 dial_loop 中未被调用（历史遗留/备用）。

### `int check_process(const char *process_name)`（`misc.c:457-482`）
- 参数校验：NULL/空/>64 → 返回 -1。
- `pgrep -x <name> | wc -l`（**`-x` 精确匹配，符合 CLAUDE.md 约定**），popen 读进程数返回。popen 失败 -1。
- 会 `printf` 进程数（调试输出）。

### `int restart_ql_netd()`（`misc.c:484-501`）
- `killall -9 ql_netd`→sleep(3)→fork→子进程 `execl /usr/bin/ql_netd`。成功 0，fork 失败 -1。工程内未见被调用。

### 网络状态文件（`misc.c:505-553`）
- `read_status()`（`misc.c:505`；**非 static，misc.h 无声明，实际仅本文件内 update_network_status 使用**）：读 `NW_STATUS_PATH=/tmp/network_status` 整数，失败/空 -1。
- `write_status(int)`：写 `"%d\n"`（**LF 结尾**），成功 0 失败 -1。
- `update_network_status(int new_status)`：先读当前值，相同则不写返回 0；不同则写，成功打 `[WARN] Status updated`。
  - **注意**：状态更新日志用 `[WARN]` 前缀（`misc.c:547`），语义上是 info 级，标签不当（非 bug）。

### 时间/温度工具
- `now_ms()`（`misc.c:557-562`）：`CLOCK_MONOTONIC` → 毫秒。
- `get_timestamp(buf,size)`（`misc.c:564-570`）：`localtime` + `strftime "%Y-%m-%d %H:%M:%S"`。
- `clean_str`（static unused，`misc.c:573`）：去 `\n`/`\r`，未使用。
- `get_cpu_temp()`（`misc.c:585-592`）：读 `/sys/class/thermal/thermal_zone0/temp`，`raw/1000` °C；失败或 raw<0 返回 -1。注释含 EC200A 温度规格。

### `bool test_can_ping_google(const char *interface_name)`（`misc.c:599-644`）
- 构造 `ping -c 1 -W 2 8.8.8.8 2>/dev/null`（指定接口时加 `-I <if>`），popen 逐行读，含 `ttl=`/`TTL=` 即 `is_success=true` 并 break。
- **有意忽略 pclose 返回值**（注释说明为规避 SIGCHLD 干扰），只信读到的内容。
- ping 目标是 **8.8.8.8**，函数名却叫 google（历史命名，实际 ping DNS）。

### 目录清理（`misc.c:648-699`）
- `dir_entry_t{name, mtime}` + `cmp_mtime_asc`（升序，最旧在前）。
- `cleanup_dir_keep_newest(dir, max_keep)`：opendir 统计非隐藏项数；`<=max_keep` 直接返回。malloc entries，rewinddir 收集普通文件（stat+S_ISREG）的 name/mtime；`n>max_keep` 则 qsort 升序，删除最旧的 `n-max_keep` 个（unlink 成功打日志），最后打统计。
- **潜在点**：第一次统计用 `d_name[0]!='.'` 计数（含子目录），第二次只收 `S_ISREG` 普通文件，`n` 可能 < count；malloc 用 count 规模，安全（偏大）。

## 三、关键分支/阈值

| 位置 | 内容 |
|---|---|
| `misc.c:80` | CREG 判定 `0,1`/`0,5` 为已注册 |
| `misc.c:99` | CPIN 含 READY 为正常 |
| `misc.c:193` | wlan0 不存在即视为 idle |
| `misc.c:465` | `pgrep -x` 精确匹配（CLAUDE.md 约定） |
| `misc.c:540` | 状态相同则跳过写文件 |
| `misc.c:627` | ping 成功判据 `ttl=`/`TTL=` |

## 四、潜在问题 / 需实测确认

1. **`log_to_file` 是空函数**（`misc.c:351-353`，`#if 1`）：main.c `restart_cfun` 里的 `log_to_file("CFUN…")` 全部无输出。CFUN 限频事件不会落任何日志。**需确认是否有意（改用 dial_log 体系后废弃 log_to_file）**。
2. **`checkSimCardStatus`/`checkCommandSuccess`/`isModuleOK`/`restartNetworkServices`/`restart_ql_netd`/`restart_ql_netcall` 等在运行期 dial_loop 未见调用**（部分仅 main 启动期 check_sim_status 相关，部分完全无调用者）——需实测确认是否死代码。
3. **`get_cpu_temp` 依赖 `thermal_zone0`**：EC200A 上该 zone 是否即模组温度需实测（代码未体现映射关系）。
4. `check_process` 每次调用 `printf` 进程数到 stdout（`misc.c:478`），运行期噪声（非致命）。
5. `read_status`/`get_cpu_temp`/`read_exit_count` 等 `fscanf` 返回值多处未检查，异常文件内容读到未初始化值（多为保守失败）。
