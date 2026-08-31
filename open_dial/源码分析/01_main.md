# 01 — main.c + _public.h

## 一、文件职责概述

- **`main.c`**：程序入口。负责：网卡等待、启动期配置读取、SIM 状态读取、单实例检查、SIGCHLD/SIGINT 信号安装、CFUN 限频重置、版本号写文件、日志系统初始化、CP dump 捕获安装，最后进入 `dial_loop()` 运行期主循环（`main.c:264-405`）。
- **`_public.h`**：全局公共头，集中 `#include` 标准库、Quectel SDK 头（`ql_type.h/ql_data_call.h/ql_sim.h/ql_nw.h`）、各业务模块头，以及日志宏 `log.h`，并定义三个公网 APN 相关宏。

## 二、关键宏、全局变量

### _public.h
| 宏 | 值 | 含义 |
|---|---|---|
| `LOG_TAG` | `"DIAL"` | 日志标签（`_public.h:30`） |
| `LOG_NDEBUG` | `1` | 关闭 debug 日志（`_public.h:31`） |
| `DATA_CALL_APN_PUBLIC` | `6` | 公网 APN profile 索引（`_public.h:35`）。**对应 CLAUDE.md 强约定：APN profile 必须绑 6，不能写 1** |
| `DATA_CALL_ID_PUBLIC` | `1` | 公网 data call id（`_public.h:36`） |
| `APN_NAME_PUBLIC` | `"apnpublic"` | 公网 APN 名（`_public.h:37`） |

### main.c
| 宏/变量 | 值 | 含义 |
|---|---|---|
| `MAIN_VERSION` | `1` | 主版本（`main.c:22`） |
| `SUB_VERSION` | `28` | 子版本（`main.c:23`） |
| `PATCH_VERSION` | `5` | 补丁版本（`main.c:24`）。**注意：CLAUDE.md 头部声明 V1.28.3，此处为 1.28.5，与最新提交 `01f4e3e … 版本号1.28.5` 一致；CLAUDE.md 版本头未同步更新（偏差见 00_总览）** |
| `exit_count` | `static int 0` | 退出计数缓存（`main.c:26`） |
| `MAX_CALLS` | `10` | CFUN 最大调用次数（`main.c:28`） |
| `MIN_INTERVAL` | `600` | CFUN 最小调用间隔（秒）（`main.c:29`） |
| `CFUN_FILE` | `/tmp/cfun_count.txt` | 调用次数持久化文件（`main.c:30`） |
| `TIME_FILE` | `/tmp/cfun_last_call.txt` | 上次调用时间文件（`main.c:31`） |

## 三、逐函数分析

### `double get_system_uptime()` （`main.c:34-45`）
- 打开 `/proc/uptime`，`fscanf("%lf")` 读第一个字段（系统运行秒数），`fclose`，返回。失败返回 0。
- **潜在点**：`fscanf` 返回值未检查；文件打开失败仅 `perror` 返回 0，调用方 `restart_cfun` 用它做间隔判断，返回 0 会让 `current_time - last_call_time` 变负，从而**跳过**限频（不足 600 秒判定成立而拒绝调用）——实际是趋于保守（不调用），非破坏性。

### `double read_last_call_time()` （`main.c:49-59`）
- 读 `TIME_FILE`，文件不存在返回 0；否则 `fscanf("%lf")` 返回值。返回值未检查。

### `void write_last_call_time(double current_time)` （`main.c:62-68`）
- 以 `"%.0f"` 写入 `TIME_FILE`。打开失败静默返回。

### `int read_cfun_count()` / `void write_cfun_count(int count)` （`main.c:71-90`）
- 读/写 `CFUN_FILE` 中的整数调用计数。文件不存在读返回 0。

### `void restart_cfun()` （`main.c:93-117`）
- CFUN 限频重置逻辑：
  - `read_cfun_count() >= MAX_CALLS(10)` → `log_to_file` 记录并 return（`main.c:96-99`）。
  - `get_system_uptime() - read_last_call_time() < MIN_INTERVAL(600)` → 拒绝（`main.c:103-106`）。
  - 满足则 `Ql_SendAT("AT+CFUN=0")` → `sleep(5)` → `Ql_SendAT("AT+CFUN=1")`（`main.c:109-111`），随后 `write_cfun_count(count+1)`、`write_last_call_time(current_time)`（`main.c:114-115`）。
- **调用 SDK/系统 API**：`Ql_SendAT`（返回值未检查）。
- **注意**：此限频用的是 `/proc/uptime`（单调递增，重启归零），跨重启计数持久化在 `/tmp`（`/tmp` 通常 tmpfs，重启即清空 → 重启后计数与时间均归零，限频失效于重启后首次）。**需实测确认 `/tmp` 是否 tmpfs**。

### `#if 0` 段 `stop_cfun`/`start_cfun`（`main.c:119-158`）
- 已被 `#if 0` 禁用，不参与编译。

### `dial_mng_t *dial_mng_new(void)` （`main.c:160-170`）
- `calloc(1, sizeof(dial_mng_t))`，失败 `perror` 返回 NULL；否则 `dial_st=dial_stat_none`，`clock_gettime(CLOCK_MONOTONIC, &dial_timer)`，返回。

### `void sig_handler(int signum)` （`main.c:172-180`）
- 仅处理 `SIGINT`：置 `g_sigint_received = 1`（async-signal-safe），注释明确说明不能在此 `free()`。

### `void sigchld_handler(int sig)` （`main.c:183-186`）
- `while(waitpid(-1, NULL, WNOHANG) > 0)` 回收所有已退出子进程。

### `void write_version_log(...)` （`main.c:189-198`）
- 以 `"Version: %d.%02d.%d\r\n"` 写入指定文件（main 调用时传 `/tmp/dial_version`）。
- **注意**：版本行用 `\r\n`（CRLF）。这是写 `/tmp/dial_version` 供 `cat` 查看，非 DNS 文件，无 CLAUDE.md 中"DNS 用纯 LF"约束冲突。

### `static int is_interface_present(const char *ifname)` （`main.c:202-225`）
- `socket(AF_INET, SOCK_DGRAM, 0)`；`strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1)`；`ioctl(sock, SIOCGIFINDEX, &ifr)>=0` 返回 1 否则 0；`close(sock)`。
- 用 `SIOCGIFINDEX` 检测网卡索引存在性。

### `int wait_for_interface(const char *ifname, int timeout_sec)` （`main.c:230-255`）
- 循环最多 `timeout_sec` 次，每次 `is_interface_present` 成功即返回 1；否则每 2 秒打印一次等待日志，`sleep(1)`，`elapsed++`。超时返回 0。

### `int check_pid_running(pid_t pid)` （`main.c:258-262`）
- **实际实现忽略 pid 参数**，返回 `check_process("dial") >= 2`（即系统里名为 dial 的进程数 ≥2 视为已在运行）。
- **潜在点**：依赖 `check_process` 的匹配方式（见 03_misc）；命令行传入的 `-1` 无意义（注释亦说明"任意填入"）。

### `int main(int argc, char *argv[])` （`main.c:264-405`）
主流程顺序：
1. `wait_for_interface("ecm0", 30)`：失败则 `sleep(2); return -1`（`main.c:277-286`）。
2. `read_config()`（读配置）、`check_sim_status()`（读 SIM）、`g_config.signal_strength = get_signal_csq()`、`g_config.ql_netd_status = 0`、`print_config()`（`main.c:290-295`）。
3. 单实例：`check_pid_running(-1)` 为真 → 打印并 `exit(EXIT_FAILURE)`（`main.c:299-302`）。
4. 安装 SIGCHLD：`sa.sa_flags = SA_RESTART | SA_NOCLDSTOP`（`main.c:305-312`）。
   - **潜在点**：`sa` 为栈变量，仅设置了 `sa_handler`/`sa_mask`/`sa_flags`，未显式清零其余字段（`sa_restorer` 等），一般无害。
5. `exit_count = read_exit_count()`；若 `>=20`：重置为 0 写回，`restart_cfun()`（`main.c:315-323`）。
6. `ALOGI` 打印版本 + `write_version_log("/tmp/dial_version", …)`（`main.c:325-327`）。
7. `p_dial_mng = dial_mng_new()`，失败 return -1（`main.c:329-333`）。
8. `signal(SIGINT, sig_handler)`（`main.c:334`）。`SIGTERM` 被注释掉（`main.c:335`）。
9. `log_init()`（日志系统初始化，注释称自动检查挂载/空间，不满足降级控制台）、`dial_log("Program started …")`、`setup_cp_dump_capture()`（`main.c:340-342`）。
10. `p_dial_mng->dial_st = dial_stat_none`（`main.c:344`）。
11. `#if 0` 段的 ql_rild 检查被禁用（`main.c:349-370`）。
12. **Keep-alive 预检**（`main.c:375-394`）：若 `test_can_ping_google(NULL)` 通 → `update_network_status(1)`，进入 `while(1)` 监视：ping 通则 `sleep(6)` 且 `retry_cnt=0`；ping 失败 `retry_cnt++`，`>10` 时 break 去重拨，否则 `sleep(1)`。
    - **注意/偏差**：注释写"org 10 --> 600次10分钟"（`main.c:387`），但代码实际阈值是 `>10`（约 10 秒连续失败），**代码与注释描述不一致，实际为 10 次**。
13. `update_network_status(0)`（`main.c:398`）。
14. `dial_loop(on_network_connected, NULL)`（`main.c:401`），返回后 `log_close(); return 0`（`main.c:403-404`）。

## 四、关键分支/阈值

| 位置 | 阈值/分支 |
|---|---|
| `main.c:277` | ecm0 网卡等待超时 30s，失败 return -1 |
| `main.c:316` | `exit_count >= 20` → 触发 CFUN 重置 |
| `main.c:96` | CFUN 累计调用 `>= 10` 拒绝 |
| `main.c:103` | CFUN 调用间隔 `< 600s` 拒绝 |
| `main.c:387` | keep-alive 连续 ping 失败 `> 10` 次退出监视去重拨 |

## 五、潜在问题 / 需实测确认

1. **版本号双源**：`main.c` 宏为 1.28.5，CLAUDE.md 头声明 1.28.3。以 `main.c` 编译产物为准（`/tmp/dial_version` 会写 1.28.5）。
2. **CFUN 限频跨重启失效**：计数/时间文件在 `/tmp`，若为 tmpfs 则重启清零。**需实测 `/tmp` 挂载类型**。
3. **`fscanf` 返回值全部未检查**（`get_system_uptime`/`read_last_call_time`/`read_cfun_count`）：文件内容异常时读到未初始化/旧值，实际风险偏保守（趋向不触发 CFUN）。
4. **keep-alive 阈值注释与代码不符**（`main.c:387`）：注释宣称 600 次/10 分钟，实际 `>10`。
5. `check_pid_running` 形参未用，单实例判据完全依赖 `check_process("dial") >= 2` 的语义（见 03_misc 核对）。
6. `restart_cfun` 里 `Ql_SendAT` 返回值未检查；`AT+CFUN=0/1`（非 1,1），只射频重置不整机复位。
