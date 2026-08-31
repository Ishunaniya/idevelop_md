# 13 — test_utils/test_utils.c + test_utils.h

## 一、文件职责概述

Quectel SDK sample 自带的**测试/交互辅助库**（版权 Quectel 2018，作者 tyler.kuang）。原用于 sample 的 stdin 交互菜单，本工程中**部分函数已被运行期实际复用**（非纯 sample 死代码）。`test_utils.h` 由 `_public.h:27` 全局 include，故所有源文件都可见其接口。

**实测调用情况**（grep 全工程）：
| 函数 | 是否运行期使用 | 调用方 |
|---|---|---|
| `t_get_int` | ✅ | nw/nw.c |
| `t_get_hex` | ✅ | nw/nw.c |
| `ql_data_call_state_str` | ✅ | nw/nw.c |
| `diff` | ✅ | data_call/data_call.c |
| `Ql_SendAT` | ✅ | main.c |
| `t_get_char` / `t_get_string` / `t_get_int_list` / `t_get_float_list` / `get_error_msg` | ❌ 无调用者 | —（sample 遗留） |
| `run_cmd` | 间接（被 Ql_SendAT 调用） | test_utils.c 内部；**未在 .h 声明** |

## 二、逐函数分析

### stdin 读取族（sample 交互辅助）
统一约定返回值：`0 成功 / 1 读到空回车 / -1 非法输入`（.h:49-121）。
- `int t_get_int(int*)`（c:37-70）：`fgets` 一行 → `strtol(base10)`，尾字符非 `\n` 判非法。
- `int t_get_hex(uint32_t*)`（c:82-115）：同上但 `strtol(base16)`。**注意内部用 `int dat` 承接再赋给 uint32**（c:84），超大 16 进制值有截断/符号问题（需实测，但仅 nw.c 交互路径用）。
- `int t_get_char(int*)`（c:127-157）：要求恰好 1 字符 + `\n`。
- `int t_get_string(char*,int)`（c:169-198）：去掉行尾 `\n`，`strncpy(str_buf, buf, str_len-1)`。**未强制补 `\0`**：若源串长度 ≥ str_len-1，strncpy 不补终止符（潜在越界读，需实测；当前无调用者）。
- `int t_get_int_list(int*,int*)`（c:211-262）/ `int t_get_float_list(float*,int*)`（c:275-326）：`strtok_r` 按分隔符切分为整数/浮点数组，`dat_len` 入参为容量、出参为实际读到个数；超容量提前返回 0。

### `struct timespec diff(struct timespec start, struct timespec end)`（c:328-342）—— **运行期使用**
- 计算两 timespec 差值，正确处理 `tv_nsec` 借位（`<0` 时借 1 秒 + 加 1e9 ns）。data_call.c 用于耗时统计。

### `const char *ql_data_call_state_str(int state)`（c:344-373）—— **运行期使用**
- data call 状态枚举 → 字符串：NONE/CREATE/IDLE/CONNECTING/PARTIAL_V4_CONNECTED/PARTIAL_V6_CONNECTED/CONNECTED/DISCONNECTED/ERROR/DELETE，default→`"UNKNOW"`（拼写如此，c:372）。nw.c 打日志用。

### `const char* get_error_msg(int errcode)`（c:377-400）
- QL 错误码 → 字符串（-1001…-1023 部分），default 返回 `"Unknown status u can lookup in ql_type.h"`。**当前无调用者**（其它模块打真实数字码）。

### `int run_cmd(char *cmd, ...)`（c:404-415）
- `vsnprintf` 格式化到 256 字节 buf → **无条件 `printf("EXEC: %s\n")`**（`if(1)` 恒真，c:411）→ `system(buf)` 返回其结果。**未在 test_utils.h 声明**，仅供内部/隐式声明使用。

### `int Ql_SendAT(char *atCmd)`（c:417-420）—— **运行期使用**（main.c）
- `run_cmd("serial_atcmd %s", atCmd)`，即通过外部 `serial_atcmd` 工具发 AT。main.c 用它下发 AT 命令。

## 三、关键点 / 潜在问题
1. **live 与 dead 混杂**：本文件不是纯 sample 死代码——`diff`/`ql_data_call_state_str`/`t_get_int`/`t_get_hex`/`Ql_SendAT` 是运行期依赖，删除会导致链接失败。清理时须区分。
2. **`run_cmd` 未在头文件声明**（c:404）：`Ql_SendAT` 在同文件内调用无碍；若他处调用会触发隐式声明 warning（与 CLAUDE.md「零 warning」相关，需实测确认无外部调用）。
3. **`run_cmd` 恒打 `EXEC:` 到 stdout**（c:411 `if(1)`）：每次 `Ql_SendAT` 都会 printf，非 dial_log，不落 SD。
4. **`t_get_hex` int 承接 uint32**（c:84）、**`t_get_string` strncpy 不补 `\0`**（c:195）为 sample 固有隐患；当前仅交互/无调用路径，风险有限但需实测确认边界。
5. `Ql_SendAT` 经 `system("serial_atcmd ...")` 走 shell，与 at/ 模块的 `serial_atcmd` 解析层并存，属两条发 AT 途径。
