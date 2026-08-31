# 11 — logger_sd.c + logger_sd.h

## 一、文件职责概述

SD 卡日志系统（对齐 CLAUDE.md v1.28.1/v1.28.2）：
- 按天分文件夹 `LOG_ROOT/YYYY-MM-DD/dial_HHMMSS.log` 存放，保留 40 天；
- 时钟未同步（1970）期日志进 `unsynced/`，按数量保留最新 20 个；
- SD 不可用时降级到控制台（printf）；
- `dial_log` 用**递归锁**保护主循环与 SDK 回调线程并发写（v1.28.2 修 use-after-free）。

对外仅 3 个接口（logger_sd.h）：`log_init` / `log_close` / `dial_log`（`dial_log` 带 `__attribute__((format(printf,1,2)))` 编译期格式检查，.h:35）。其余函数全 `static` 内部实现。

## 二、关键宏与全局状态
| 名称 | 值 | 含义（行号） |
|---|---|---|
| `SDCARD_PATH` | `/media/sdcard` | SD 挂载点（c:19） |
| `LOG_ROOT` | `/media/sdcard/dial_log` | 日志根（c:20） |
| `LOG_UNSYNCED_DIR` | `LOG_ROOT/unsynced` | 未同步期目录（c:21） |
| `MIN_FREE_SPACE_MB` | 500 | 初始化最低可用空间门槛（c:22） |
| `LOG_RETAIN_DAYS` | 40 | 日期文件夹保留天数（c:29） |
| `UNSYNCED_KEEP` | 20 | unsynced/、legacy/ 按数量保留（c:30） |
| `LOG_FREE_FLOOR_MB` | 1024 | 剩余空间兜底门槛，低于即从最旧删（c:31） |
| `CLOCK_SANE_EPOCH` | 1577836800（2020-01-01） | 低于此视为时钟未同步（c:34） |
| `g_log_fp` | FILE* static | 当前日志文件句柄（c:41） |
| `g_cur_daykey` | char[16] static | 当前文件所属天键（c:42） |
| `g_log_mtx` | pthread_mutex_t | **递归锁**，pthread_once 初始化（c:51-60） |

## 三、并发/锁设计（v1.28.2 核心，c:44-60, 490-514）
- **为何递归锁**：`dial_log` 被主循环和 SDK 指示回调线程（libql_sdk.so 内 epoll 线程）并发调用；换文件分支要 `fclose(g_log_fp)`+重开。无锁时另一线程 `fprintf` 撞已关闭句柄 → use-after-free。
- **为何必须递归**：持锁期间 `run_cleanup → remove_logdir/cleanup_dir_keep_newest` 会嵌套回调 `dial_log`（c:250, 418）。递归锁允许本线程重入不自死锁。
- **为何 pthread_once 运行期初始化**：本工具链 libc 无 `PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP`，改用 `pthread_once(&g_log_mtx_once, log_mtx_init)`（c:49-52, 458, 490），POSIX 通用跨 libc。
- Makefile 须链 `-lpthread`（CLAUDE.md 约定）。

## 四、逐函数分析

### `int is_sdcard_mounted(void)`（c:62-83）
- 读 `/proc/mounts`，逐行 `fscanf`，挂载点 == `/media/sdcard` 即返回 1。**非 static，对外可见**（唯一非 static 的辅助函数）。

### `static int check_sdcard_available(void)`（c:88-118）
- ① `is_sdcard_mounted()` 否 → 写 `/tmp/sdcard_avl`=000 并返回 0；② `statvfs` 算可用 MB，`< MIN_FREE_SPACE_MB(500)` 返回 0；否则 1。`log_init` 用的是这个**即时**版本。

### `static int check_sdcard_available_wait_30s(void)`（c:123-194，`__attribute__((unused))`）
- 备用：最多轮询等 30s SD 就绪。标 unused 保留而不触发 warning（c:120-122）。**当前未启用**。

### `static int clock_is_sane(void)`（c:197-200）
- `time(NULL) >= CLOCK_SANE_EPOCH`（≥2020 视为已同步）。

### `static void current_daykey(char*, size_t)`（c:203-213）
- 时钟可信 → `strftime "%Y-%m-%d"`；否则 → `"unsynced"`。

### `static unsigned long long sd_free_mb(void)`（c:216-222）
- `statvfs` 算剩余 MB；**失败返回 `(unsigned long long)-1`（极大值）**，避免误触发兜底删除。

### `static int parse_daydir(const char*, time_t*)`（c:225-242）
- 校验名字恰为 `YYYY-MM-DD`（长度 10 + sscanf + 月日范围），解析为**当天正午 epoch**（`tm_hour=12` 避开午夜±DST 把年龄算偏一天，c:236）。

### `static void remove_logdir(const char *name)`（c:245-251）
- `system("rm -rf 'LOG_ROOT/name'")`。注释强调 path 全由内部常量+目录名拼成、无外部输入（c:244）。打 `[LOGCLEAN]` 日志（**持锁下会嵌套回 dial_log**）。

### `static void cleanup_by_age(void)`（c:255-272）
- 时钟不可信直接 return；遍历 `LOG_ROOT`，`parse_daydir` 成功的日期夹，年龄 > 40 天且非当前活动夹 `g_cur_daykey` 则 `remove_logdir`。**绝不删活动文件夹**（c:267）。

### `static void cleanup_space_floor(void)`（c:275-315）
- 剩余 ≥ 1024MB 直接返回；否则收集所有日期夹名（动态数组 realloc），**字典序==时间序升序排序**，从最旧删起，达标即停，跳过活动夹。

### `static void run_cleanup(void)`（c:318-324）
- `cleanup_by_age()` + `cleanup_space_floor()` + `cleanup_dir_keep_newest(unsynced, 20)`。

### `static int open_log_for_today(void)`（c:327-367）
- 按天键定目录（unsynced 或 `LOG_ROOT/daykey`）→ `mkdir -p` → **追加模式 `fopen(...,"a")`** 打开 `dial_YYYYMMDD_HHMMSS.log`（追加避免同秒重开丢内容、天键切换不覆盖，c:352）。成功写一行 `=== Dial Log Opened ... ===` 并 fflush，更新 `g_cur_daykey`；失败清空 daykey 返回 -1。

### `static void migrate_legacy_flat_logs(void)`（c:372-424）
- 一次性把旧方案平铺在 `LOG_ROOT` 下的 `dial_*.log` 按文件名日期 `rename` 归入 `YYYY-MM-DD/`，无法解析的进 `legacy/`。全同盘 `rename`（O(1) 元数据、不拷贝），legacy/ 按数量保留 20。

### `void log_init(void)`（c:426-455）
- 幂等（`g_log_fp!=NULL` 直接返回）；SD 检查失败 → 仅控制台；否则 `mkdir -p LOG_ROOT` → 迁移旧文件 → `open_log_for_today` → 成功则开机 `run_cleanup()` 一次。注释提到可选硬锁 `TZ=CST-8`（c:430-435，默认注释关闭）。

### `void log_close(void)`（c:457-471）—— **持锁**
- 持 `g_log_mtx`，写 `=== Program Exit ... ===`，`fclose` 并置 NULL。

### `void dial_log(const char *fmt, ...)`（c:473-515）—— **热路径，持锁**
- 先无锁部分：格式化时间戳 + **无条件输出到 stdout**（printf/vprintf，c:483-487）。
- 再持 `g_log_mtx`：若 `current_daykey != g_cur_daykey`（跨零点/未同步→已同步）则 `fclose`+`open_log_for_today`+`run_cleanup`（换文件，c:499-504）；随后若 `g_log_fp` 有效则 `fprintf` 时间戳+内容并 **fflush（掉电不丢最近日志，c:511）**。

## 五、关键点 / 潜在问题
1. **stdout 输出在锁外**（c:483-487）：多线程并发时控制台输出可能交错，但文件写入在锁内是安全的。这是有意取舍（控制台仅辅助）。
2. **`check_sdcard_available` 写 `/tmp/sdcard_avl` 用 `system("echo ...")`**（c:91）——fork/shell 开销，且在 SIGCHLD handler 场景下 `system` 返回值可能不可信（此处未取返回值，无影响）。
3. **多处 `system()`**（rm -rf、mkdir -p、echo）：功能正常但依赖 shell；path 均内部拼接无注入风险（作者已注释说明）。
4. `is_sdcard_mounted` 是唯一非 static 辅助（c:62），但 logger_sd.h 未声明，外部若想用需自行 extern（当前无外部调用）。
5. 日志根 `LOG_ROOT` 与 CLAUDE.md 部署速查一致（`/media/sdcard/dial_log`）。降级到控制台后 `dial_log` 仍工作（走 stdout，文件分支因 `g_log_fp==NULL` 跳过）。
