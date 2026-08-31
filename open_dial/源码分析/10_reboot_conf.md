# 10 — reboot_conf/dial_reboot_conf.c + .h

## 一、文件职责概述

早期"断网超时→整机 reboot"的持久化配置与重启机制。**核心重启逻辑当前已停用**：`main.c:292` 明确注释掉了 `handle_dial_up_down()`。当前运行期只用到其中的**读配置 / 读 SIM 状态 / 读信号强度**三项，重启动作（`perform_reboot`/`handle_dial_up_down`/`THRESHOLD_TIME`）为**遗留死代码**（编译进二进制但无调用）。

> 与 CLAUDE.md 一致：v1.28.0 起分级恢复放弃"整机重启"，L3 改为纯 `exit` 重拉。本模块的 `reboot` 机制正是被取代的旧方案。

## 二、配置与结构（dial_reboot_conf.h）
| 宏/字段 | 值/类型 | 含义 |
|---|---|---|
| `CONFIG_FILE` | `/usrdata/dial_config.txt` | 持久化配置文件（.h:5） |
| `REBOOT_FILE` | `/tmp/system_is_rebooting` | 重启标志文件（.h:6） |
| `THRESHOLD_TIME` | 1200（20 分钟） | 断网重启阈值（.h:7，**当前无生效路径**） |
| `Config.uptime` | int | 开机时间（.h:11） |
| `Config.restart_flag` | int | 0 未重启 / 1 已重启（.h:12） |
| `Config.first_disconnect` | int | 首次断网标记（.h:13，读配置时恒置 0） |
| `Config.signal_strength` | int | 信号强度（.h:14） |
| `Config.sim_status` | int | SIM 状态（.h:15，注释称 0 好/1 坏，实际存的是 sim_init/iccid 返回码） |
| `Config.ql_netd_status` | int | ql_netd 运行状态 0 运行/1 未运行（.h:16，**代码中从未赋值**） |
| `g_config` | 全局 Config | 全局配置实例（.c:10，extern .h:20） |

## 三、逐函数分析

### `void read_config(void)`（.c:13-30）—— **运行期使用**（main.c:290）
- 打开 `CONFIG_FILE`；文件不存在则初始化 `uptime=get_uptime()`、`restart_flag=0`、`first_disconnect=0` 并 `write_config()` 落盘后 return（.c:17-24）。
- 存在则 `fscanf("%d,%d", &uptime, &restart_flag)`，`first_disconnect` 恒置 0（.c:27-28）。

### `void write_config(void)`（.c:33-41）
- 以 `"w"` 打开写 `"%d,%d\n"`（只持久化 uptime、restart_flag）。**打开失败直接 `exit(1)`**（.c:36-38）——注意这是硬退出。

### `int get_uptime(void)`（.c:59-72）—— **运行期使用**
- 读 `/proc/uptime` 取浮点秒数取整返回。**读失败 `exit(1)`**（.c:61-64）。上方 44-56 行有一份读整数版被注释。

### `int get_signal_csq(void)`（.c:75-98）—— **运行期使用**（main.c:293）
- `popen("serial_atcmd at+csq")`，逐行找 `+CSQ:`，`sscanf("+CSQ: %d,")` 取第一个数（信号强度）。找不到/失败返回 -1。

### `int is_connection_down(void)`（.c:102-105）
- `system("ping -c 10 8.8.8.8")`，返回值非 0 即视为断开。**运行期连通性检测走 misc.c，此函数无调用者**（需实测确认）。

### `void check_sim_status(void)`（.c:108-122）—— **运行期使用**（main.c:291）
- `sim_init()`；失败 `ALOGE` + `sleep(2)`；成功则 `sim_get_iccid(iccid)`。把返回码存入 `g_config.sim_status`（.c:121）。注释强调"只开机调一次防止多次读写 SIM"（.c:107）。

### `void handle_dial_up_down(void)`（.c:156-177）—— **已停用**
- 逻辑：`check_pre_conditions()` 不满足则跳过；`restart_flag==1` 直接返回；否则若 `当前uptime - g_config.uptime > THRESHOLD_TIME` 则置 `restart_flag=1`、`write_config()`、`perform_reboot()`。
- **main.c:292 已注释此调用 → 整条断网重启链路不生效。**

### `int check_pre_conditions(void)`（.c:125-133）
- 条件：`signal_strength>20 && sim_status>=0 && ql_netd_status==0` 才返回 1。`ql_netd_status` 从未被赋值（默认 0），`signal_strength` 由 `get_signal_csq()` 现取。仅被 `handle_dial_up_down` 调用 → 随之停用。

### `void perform_reboot(void)`（.c:136-153）—— **已停用**
- 写 `REBOOT_FILE`=1 → `sleep(10)` → `system("reboot")`。仅被停用的 `handle_dial_up_down` 调用。

### `void print_config(void)`（.c:181-189）
- 调试打印 g_config 各字段。

## 四、关键点 / 潜在问题
1. **重启机制整体停用**：`handle_dial_up_down`/`perform_reboot`/`check_pre_conditions`/`is_connection_down`/`THRESHOLD_TIME` 均无生效调用路径（main.c:292 注释），属遗留代码。恢复策略已由 dial.c 的 L1/L2/L3 接管。
2. **多处 `exit(1)` 是硬退出**：`write_config`/`get_uptime` 打开 `/proc/uptime` 或配置文件失败即 `exit(1)`。`get_uptime` 在 `read_config` 首次初始化时被调用；若 `/proc/uptime` 不可读会导致进程启动即退出（需实测确认此路径风险）。
3. `sim_status` 字段语义与注释不符：注释说 0 好/1 坏，实际存的是 `sim_init`/`sim_get_iccid` 的返回码（QL_ERR_OK=0 或 -1）。
4. `ql_netd_status` 声明但全程未赋值，`check_pre_conditions` 依赖它（默认 0 恰好满足）——随重启逻辑停用已无影响。
5. **中文 `printf`/`perror` 输出**：本模块沿用 `printf`/`perror` 而非 `dial_log`，日志不落 SD（与其他模块的 dial_log 风格不一致）。
