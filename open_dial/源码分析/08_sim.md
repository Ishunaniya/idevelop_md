# 08 — sim/sim.c + sim/sim.h

## 一、文件职责概述

SIM 域封装：`ql_sim_init/deinit`、读 IMSI/ICCID、SIM app_state 枚举→字符串。文件头仍标注 sample `m_sim.c`。运行期真正使用的是 `sim_init`、`sim_get_iccid`、`sim_app_state_str`（dial.c 调用）。

## 二、关键数据结构、宏（sim.h）
| 名称 | 值 | 含义 |
|---|---|---|
| `SIM_OP_INIT/DEINIT/GET_IMSI/GET_ICCID` | 0/1/2/3 | 操作码（sim.h:71-74，**未在 sim.c 使用**） |
| `SIM_BUF_SIZE` | 32 | sim_mng_t 缓冲（sim.h:75） |
| `SIM_INIT_ERROR` | -1 | 初始化错误码（sim.h:77；`#if 0` 段内有重复定义被禁用，sim.h:78-83） |
| `sim_op_stat_enu` | get_iccid/get_imsi/get_deinit/suc | 状态枚举（sim.h:84-90，未使用） |
| `sim_mng_t` | `{sim_op_st, rsv[3], sim_iccid[32], sim_imsi[32]}` | 未使用 |
| `DIAL_STAT_NONE/SIM_CARD_CHECK` | 0x00/0x01 | 未使用（sim.h:101-102） |

- **注意**：sim.h 声明了 `sim_op`、`sim_process`、`sim_op_handler`（sim.h:107-109），但 **sim.c 中均无定义**——若被引用会链接失败，当前无调用者。

## 三、逐函数分析

### `int sim_init(void)`（`sim.c:25-40`）
- `ql_sim_init()`，OK 打 `[SIM] sim_init OK` 返回 QL_ERR_OK；否则打 `[SIM] sim_init failed, ret=<真实SDK码>` 返回 -1。
- **符合 CLAUDE.md**：失败详情在此打真实 SDK 码，dial.c 只做退避。

### `int sim_deinit(void)`（`sim.c:42-57`）
- `ql_sim_deinit()`，OK 返回 0，失败返回 -1。**工程内无调用者**（守护进程永不退出，不 deinit）。

### `int sim_get_imsi(char* imsi)`（`sim.c:59-111`）
- 硬编码 `input=1` → `slot=QL_SIM_SLOT_1`；随后 `input=0` → `app_type=QL_SIM_APP_TYPE_UNKNOWN`（switch 分支都是死路径，input 已固定）。
- `ql_sim_get_imsi(slot, app_type, imsi, QL_SIM_IMSI_LENGTH+1)`，OK 返回 0，失败打日志返回 -1。
- **注意**：slot/app_type 选择逻辑是 sample 遗留的"伪交互"（input 写死），实际固定 SLOT_1 + APP_TYPE_UNKNOWN。**运行期 dial.c 用的是 at.c 的 `get_imsi_at_safe`（AT+CIMI），此 `sim_get_imsi` 无调用者**（需实测确认）。

### `int sim_get_iccid(char* iccid)`（`sim.c:113-144`）—— **运行期使用**
- 硬编码 `slot=QL_SIM_SLOT_1`；`ql_sim_get_iccid(slot, iccid, QL_SIM_ICCID_LENGTH+1)`，OK 返回 QL_ERR_OK，失败打日志返回 -1。
- dial.c:334 调用，结果作为 `set_apn(iccid)` 的匹配键。

### `const char *sim_app_state_str(QL_SIM_APP_STATE_E s)`（`sim.c:150-166`）—— **运行期使用**
- 枚举→字符串：UNKNOWN/DETECTED/PIN1_REQ/PUK1_REQ/INIT/PERSO_*/PIN1_BLOCKED/ILLEGAL/READY，default→UNKNOWN。dial.c 心跳与 ST_SIM 用它打印 SIM 状态。

## 四、潜在问题 / 需实测确认

1. **sim.h 声明多处无实现**：`sim_op`/`sim_process`/`sim_op_handler`（sim.h:107-109）在 sim.c 无定义；`sim_mng_t`/操作码宏/状态枚举均为 sample 遗留未使用。若无引用不影响编译。
2. **`sim_get_imsi` 疑似死代码**：运行期 IMSI 走 AT+CIMI（at.c），此函数固定 SLOT_1/APP_TYPE_UNKNOWN 且无调用者。**需实测确认是否完全未用**。
3. **`sim_deinit` 无调用者**：守护进程不退出，SIM 从不 deinit。
4. slot 选择用写死的 `input=1/0` 伪逻辑（sim.c:63-99,117），可读性差（sample 遗留），实际只支持 SLOT_1。多卡场景需改造（代码未体现多卡支持）。
