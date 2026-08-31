# eg25 vs open_dial_for_artery 差异分析报告

> 生成时间：2026-05-27  
> 参考仓库（新版）：`/home/tronlong/lyp/code/open_dial_for_artery`  
> 目标仓库（待对比）：`/home/tronlong/lyp/code/rtms_sdk/apps/modem_mng/eg25`

---

## 一、open_dial 提交历史概览

| 提交哈希 | 提交说明 | 改动方向 |
|----------|----------|----------|
| 14118bf | [CHGLOG] 忽略 md 文档目录 | 构建/文档 |
| 9d15833 | [BUGFIX] 增加注册超时切换策略，修复断线重拨卡死及互斥锁死锁问题 | **高优先级 Bug Fix** |
| e20da07 | [CHGLOG] 增加变量值校验及异常处理并返回错误 | 健壮性/错误处理 |
| 3b5f41c | [CHGLOG] 日志降频 | 日志优化 |
| bfc3098 | [CHGLOG] 日志降频 | 日志优化 |
| 86b46cb | [BUGFIX] 增加/opt/conf.ini缺失判断 | **Bug Fix** |
| b1ca8d0 | [BUGFIX] 系统测试问题修复 | **Bug Fix** |
| 9c1ec55 | [NEWFUNC] V1.29.7 双通道切换逻辑内置化 | **新功能** |
| bcd3b90 | 修改日志存储与默认的子运营商切换模式 | 配置/行为变更 |
| e3bb1dd | V1.29.4 强制使用profile_idx=1进行拨号 | **重要逻辑变更** |
| 54ce60a | V1.29.3 增加apn add 的容错 | 健壮性 |
| aee0d86 | V1.29.2 增加Default Profile操作 | 功能增强 |
| 180fc2d | V1.29 TEST2 修改apn.json，屏蔽不需切运营商的卡的切换，拨号过程中避免重复调用 | **重要逻辑变更** |
| 2923dd1 | V1.29 去掉流量监测、修改状态机跳转，增加CEREG检测 | 状态机重构 |
| 983f7cb | 修改拨号APN设置模式，V1.28 | APN逻辑变更 |
| e8de9cc | 修改切换超时时间为30秒，超时次数为2次，V1.27 | 超时参数调整 |
| 31a419f | 增加时区功能，V1.26 | **新功能** |
| b6e0ac4 | 增加AT指令读取的延时，避免系统占用率过高导致Kill | 稳定性改善 |
| ffa1e3e | 删除无用代码，V1.24 | 代码清理 |
| 5a0dd50 | 修复Ql_send函数Bug | **Bug Fix** |
| 0a422dd | 加入自动切运营商的功能 | **新功能** |
| b97dfa9 | 修改apn,usr_name,pwd字段长度，适配不同SIM配置，V1.22 | 配置扩展 |
| 35784dc | 删除CFUN操作，避免对SIM卡的影响，V1.21 | **重要逻辑变更** |
| 917e204 | 调整联网和SIM卡检测超时时间为2分钟，V1.20 | 超时调整 |
| e48af83 | 修复未加入DefaultProfile导致的联网问题 | **Bug Fix** |
| 9f0c3b9 | 修复sim卡检查前未初始化导致无法拨上号，V1.17 | **Bug Fix** |
| 7d72b2e | 修复状态机缺失break语句，V1.16 | **Bug Fix** |
| 5ceafe7 | 增加拨号成功但1分钟内无通讯时重连网络，V1.15 | 稳定性增强 |
| 26642c5 | 加入驻网重连功能，V1.14 | 功能增强 |
| 8d120ae | 重新梳理拨号流程，V1.13 | 重构 |
| 5966700 | 修正APN处理问题，V1.12 | Bug Fix |

**关键改动方向总结：**
- `9d15833`：**最重要的 BugFix**，解决断线重拨卡死和互斥锁死锁
- `e3bb1dd`：强制 `profile_idx=1`，高通平台重要适配
- `9c1ec55`：双通道切换逻辑内置化（新功能）
- `2923dd1`：状态机重构，增加 CEREG 检测
- `35784dc`：删除 CFUN 操作以保护 SIM 卡稳定性

---

## 二、文件结构对比

### open_dial_for_artery/src 文件结构

```
src/
├── apn/          apn.c  apn.h
├── at/           at.c   at.h
├── cc_deque/     cc_common.c  cc_common.h  cc_deque.c  cc_deque.h
├── dial/         dial.c  dial.h
├── json/         cJSON.c  cJSON.h
├── nw/           nw.c  nw.h
├── opt_iniparser/ dictionary.c  dictionary.h  iniparser.c  iniparser.h
├── roamlink/     roamlink.c  roamlink.h
├── seas_log/     seas_log.c  seas_log.h
├── sim/          sim.c  sim.h
├── status/       status.c  status.h
└── tz/           tz.c   tz.h
```

### eg25 文件结构

```
eg25/
├── apn/              apn.c  apn.h
├── at/               at.c   at.h
├── dial/             dial.c  dial.h
├── json_old/         cJSON.c  cJSON.h
├── nw/               nw.c   nw.h
├── opt_iniparser/    dictionary.c  dictionary.h  iniparser.c  iniparser.h
├── roamlink_dial/    dialStrategy.c  dialStrategy.h   ← 结构有差异
├── sim/              sim.c  sim.h
└── tz/               tz.c   tz.h
```

### 结构差异汇总

| 模块 | open_dial | eg25 | 说明 |
|------|-----------|------|------|
| apn | ✅ apn.c/h | ✅ apn.c/h | 均存在 |
| at | ✅ at.c/h | ✅ at.c/h | 均存在 |
| dial | ✅ dial.c/h | ✅ dial.c/h | 均存在 |
| nw | ✅ nw.c/h | ✅ nw.c/h | 均存在 |
| tz | ✅ tz.c/h | ✅ tz.c/h | 均存在 |
| sim | ✅ sim.c/h | ✅ sim.c/h | 均存在 |
| roamlink | ✅ roamlink.c/h | ⚠️ roamlink_dial/dialStrategy.c/h | **目录名和文件名均不同** |
| status | ✅ status.c/h | ❌ 缺失 | eg25 无 status 模块 |
| seas_log | ✅ seas_log.c/h | ❌ 缺失 | eg25 无自定义日志模块 |
| cc_deque | ✅ cc_deque.c/h + cc_common | ❌ 缺失 | eg25 无双端队列模块 |
| json | ✅ json/cJSON | ⚠️ json_old/cJSON | eg25 使用旧版 cJSON |

> **注**：eg25 的 roamlink 功能实际由共享模块 `/home/tronlong/lyp/code/rtms_sdk/apps/modem_mng/roamlink/roamlink.c` 实现，该模块基于 open_dial 的 roamlink.c，并用 `#ifdef USE_EG25_DIAL` 做平台适配。

---

## 三、逐模块深度对比

---

### 3.1 模块：at（at.c / at.h）

#### 🔴 严重 Bug：`timeout.tv_usec` 计算错误

**open_dial（正确）**
```c
// src/at/at.c
timeout.tv_usec = (timeout_ms % 1000) * 1000;
```

**eg25（错误）**
```c
// eg25/at/at.c，第 25 行
timeout.tv_usec = timeout_ms % 1000;   // ← 缺少 * 1000
```

`tv_usec` 单位是微秒，正确值应为 `(ms % 1000) * 1000`。eg25 代码直接将毫秒余数赋给微秒字段，导致实际超时精度误差达 1000 倍（最大误差 999μs vs 应有 999ms）。

#### 🔴 严重 Bug：`select()` 超时结构体未在循环内重置

**open_dial（正确）**
```c
// src/at/at.c
while (1) {
    timeout.tv_sec  = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;  // 每次迭代重置
    ret = select(smd_fd + 1, &rfds, NULL, NULL, &timeout);
    ...
}
```

**eg25（错误）**
```c
// eg25/at/at.c
timeout.tv_sec  = timeout_ms / 1000;
timeout.tv_usec = timeout_ms % 1000;   // 在循环外设置一次
while (1) {
    FD_ZERO(&rfds);
    FD_SET(smd_fd, &rfds);
    ret = select(smd_fd + 1, &rfds, NULL, NULL, &timeout);  // timeout 被消耗后为 0
    ...
}
```

POSIX 规定 `select()` 会修改 `timeout` 参数（Linux 上返回剩余时间或 0）。第一次 `select()` 返回后 `timeout` 被清零，后续迭代将立即返回，导致 AT 命令读取在首帧后退化为忙等轮询，CPU 占用率异常升高甚至引发进程被 Kill。

#### ✅ eg25 优势：`smd_fd` 防重复打开

```c
// eg25/at/at.c，at_init()
static int smd_fd = -1;
if (smd_fd >= 0) return smd_fd;   // 已打开则直接返回
```

open_dial 无此保护，重复调用 `at_init()` 会泄漏文件描述符。

#### 待同步改动

| 优先级 | 改动 |
|--------|------|
| 🔴 必须 | 修复 `tv_usec = timeout_ms % 1000` → `tv_usec = (timeout_ms % 1000) * 1000` |
| 🔴 必须 | 将 `timeout` 初始化移入 `while(1)` 循环内 |

---

### 3.2 模块：apn（apn.c / apn.h）

#### 🔴 Bug：ICCID 前缀匹配缺少空字符串保护

**open_dial（正确）**
```c
// src/apn/apn.c，apn_scan_from_json()
size_t prefix_len = strlen(p_json_str);
if (prefix_len > 0 && strncmp(p_json_str, iccid, prefix_len) == 0) {
```

**eg25（错误）**
```c
// eg25/apn/apn.c，第 446 行，apn_scan_from_json()
if (strncmp(p_json_str, iccid, strlen(p_json_str)) == 0) {
```

当 `apn.json` 中某条目的 iccid 字段为空字符串时，`strlen(p_json_str) == 0`，`strncmp` 返回 0，导致该条目**匹配所有 SIM 卡**，覆盖正确的 APN 配置。

#### 🟡 隐患：同时引用两套 JSON 库

```c
// eg25/apn/apn.c 顶部
#include "json-c/json.h"   // 系统 json-c 库
#include "cJSON.h"         // 本地 cJSON 库
```

代码中新功能已切换到 cJSON，但旧代码仍调用 `json_object_*` API。两套库并存增加链接复杂度，且行为语义不同，存在混淆风险。open_dial 已统一使用 cJSON。

#### 🟡 隐患：使用 `memcpy` 而非 `snprintf` 复制字符串

**open_dial（安全）**
```c
snprintf(p_apn_obj->apn,      sizeof(p_apn_obj->apn),      "%s", apn_str);
snprintf(p_apn_obj->usr_name, sizeof(p_apn_obj->usr_name), "%s", usr_str);
snprintf(p_apn_obj->pwd,      sizeof(p_apn_obj->pwd),      "%s", pwd_str);
```

**eg25（潜在越界）**
```c
memcpy(p_apn_obj->apn,      apn_str,  strlen(apn_str));
memcpy(p_apn_obj->usr_name, usr_str,  strlen(usr_str));
memcpy(p_apn_obj->pwd,      pwd_str,  strlen(pwd_str));
```

`memcpy` 不写入 `\0` 终止符，且无边界检查；若源字符串长度超过目标缓冲区则越界写入。

#### 🟡 接口重复：`apn_get_idx()` 与 `apn_get_apn_obj()` 并存

eg25 保留了旧接口 `apn_get_idx()`，同时也添加了新接口 `apn_get_apn_obj()`（与 open_dial 对齐），两者逻辑部分重叠，增加维护负担。建议逐步废弃旧接口。

#### 待同步改动

| 优先级 | 改动 |
|--------|------|
| 🔴 必须 | 在 `strncmp` 前加 `prefix_len > 0` 保护 |
| 🟡 建议 | 移除 `json-c` 依赖，统一使用 cJSON |
| 🟡 建议 | `memcpy` 替换为 `snprintf` 并补充 `\0` 终止 |

---

### 3.3 模块：dial（dial.c / dial.h）

#### 🔴 Bug：`dial_stat_net_connected` 状态缺少 TCP 连通性测试

**open_dial（完整）**
```c
// src/dial/dial.c，dial_stat_net_connected
if (dial_timer >= 60) {
    dial_timer = 0;
    if (nw_tcp_connectivity_test() != 0) {
        // TCP 不通，重置计时器，准备重拨
        dial_timer = 0;
        goto reg_timeout_handler;
    }
}
```

**eg25（缺失 TCP 测试）**
```c
// eg25/dial/dial.c，dial_stat_net_connected
// 仅依赖 nw_get_rmnet_rx_packets_sum() 判断流量，无 TCP 主动探测
if (g_no_traffic_cnt >= NO_TRAFFIC_MAX_CNT) {
    // 触发重拨
}
```

纯粹依赖 rx_packets 计数无法区分"网络已断但计数器还没清零"的窗口期，会导致断网后最长延迟 `NO_TRAFFIC_MAX_CNT × 轮询间隔` 才触发重拨。open_dial 通过 TCP 主动探测可立即检测到网络层断连。

#### 🔴 Bug：注册超时后未发送 `AT+COPS=0` 强制重选网络

**open_dial（完整，commit 9d15833）**
```c
// src/dial/dial.c，wait_for_connect 超时处理
if (!is_oper_select) {
    Ql_SendAT("AT+COPS=0\r\n", resp, sizeof(resp), 3000);
}
goto reg_timeout_handler;
```

**eg25（缺失）**
```c
// eg25/dial/dial.c，超时分支
// 直接跳转重拨，未下发 AT+COPS=0
```

在网络切换失败或驻网僵死时，不发 `AT+COPS=0` 无法触发模组重新自动搜网，可能导致长期无法驻网。

#### 🔴 Bug：断线重拨后 `dial_timer` 未重置（commit 9d15833 核心修复）

**open_dial（修复后）**
```c
reg_timeout_handler:
    dial_timer = 0;   // 重置计时，避免立即再次触发超时逻辑
    dial_state = DIAL_STAT_DISCONNECT;
```

eg25 对应路径未重置 `dial_timer`，重拨进入 `DIAL_STAT_DISCONNECT` 后可能因 `dial_timer` 残留值立即再次触发超时，造成状态机快速循环甚至卡死。

#### ✅ eg25 优势：L1/L2/L3 分级恢复机制

eg25 特有的三级恢复策略，适配 EG25 平台特性：

```c
// eg25/dial/dial.c
// L1: 60s 内软重拨
// L2: 5min 超时发 AT+CFUN=0/AT+CFUN=1 复位射频
// L3: 30min 超时触发系统硬重启
```

此机制是 EG25 的平台适配增强，open_dial 目标平台（EC200A/Artery）不需要，**不应反向同步到 open_dial**。

#### ✅ eg25 优势：心跳日志更丰富

```c
// eg25/dial/dial.c，30s 心跳
// 输出 RSRP / RSRQ / 温度 / Cell ID / DIAG 诊断快照
```

#### ✅ eg25 优势：初始化时打印模组信息

```c
// eg25/dial/dial.c，dial_init()
// 打印 IMEI / 型号 / 固件版本
```

#### 待同步改动

| 优先级 | 改动 |
|--------|------|
| 🔴 必须 | 在 `dial_stat_net_connected` 加入 TCP 主动探测（每 60s 一次） |
| 🔴 必须 | 注册超时后补充 `AT+COPS=0` 强制重选 |
| 🔴 必须 | 重拨时重置 `dial_timer = 0` |

---

### 3.4 模块：nw（nw.c / nw.h）

#### ✅ eg25 改进：更严格的驻网状态检查

**open_dial**
```c
// src/nw/nw.c，nw_reg_status_check()
if (nw_info.data_registration_valid) {
    return 0;   // 只检查字段有效性
}
```

**eg25（更严格）**
```c
// eg25/nw/nw.c，nw_reg_status_check()
if (nw_info.registration_state == E_QL_MCM_NW_SERVICE_FULL) {
    return 0;   // 必须达到 FULL SERVICE
}
```

eg25 的判断更严格，只有完全驻网才认为成功，避免在弱网（LIMITED_SERVICE）时错误地认为联网成功。**建议将此改进同步回 open_dial**。

#### ✅ 两者均已同步的功能

- `nw_tcp_connectivity_test()`：TCP 层主动探测
- `nw_get_rmnet_rx_packets_sum()`：rmnet 接口流量统计
- `nw_at_get_cereg_stat()`：通过 AT+CEREG? 查询注册状态

#### 待同步改动

| 优先级 | 改动 |
|--------|------|
| 🟢 可选 | 将 `E_QL_MCM_NW_SERVICE_FULL` 严格检查同步回 open_dial |

---

### 3.5 模块：sim（sim.c / sim.h）

#### 🔴 Bug：`sim_op_handler()` 缺少 `break`，case 穿透

**open_dial（正确）**
```c
// src/sim/sim.c，sim_op_handler()
case SIM_OP_GET_ICCID:
    sim_op_stat_get_iccid(p_sim_obj);
    break;   // ← 有 break
case SIM_OP_GET_IMSI:
    sim_op_stat_get_imsi(p_sim_obj);
    break;
```

**eg25（错误，第 241 行）**
```c
// eg25/sim/sim.c，sim_op_handler()
case sim_op_stat_get_iccid:
    sim_op_stat_get_iccid_handler(p_sim_obj);
    // ← 缺少 break，穿透到下一个 case
case sim_op_stat_get_imsi:
    sim_op_stat_get_imsi_handler(p_sim_obj);
    break;
```

每次执行 `get_iccid` 后都会额外执行 `get_imsi`，造成多余的 AT 指令下发和潜在的状态机混乱。此问题与 open_dial commit `7d72b2e`（修复状态机缺失break）描述的同类问题一致。

#### 🟡 代码质量：使用硬编码数字而非枚举常量

**open_dial（可读性好）**
```c
sim_op(p_sim_obj, SIM_OP_INIT);
sim_op(p_sim_obj, SIM_OP_GET_ICCID);
```

**eg25（硬编码）**
```c
sim_op(p_sim_obj, 0);   // INIT
sim_op(p_sim_obj, 2);   // GET_ICCID
```

硬编码整数降低可读性，且枚举值顺序变动后极难排查。

#### 待同步改动

| 优先级 | 改动 |
|--------|------|
| 🔴 必须 | `sim_op_stat_get_iccid` case 后补充 `break` |
| 🟡 建议 | 将 `sim_op()` 调用改为使用枚举常量 |

---

### 3.6 模块：tz（tz.c / tz.h）

#### ✅ eg25 改进：更健壮的初始化

**open_dial（简陋）**
```c
// src/tz/tz.c，save_tz_info()
system("touch /usr/dial/tz.ini");   // 只创建空文件，无默认内容
```

**eg25（完整）**
```c
// eg25/tz/tz.c
system("mkdir -p /usr/dial");       // 确保目录存在
// 写入默认配置：[dev]\ntz = 32\n
```

eg25 在目录不存在时能自动创建，并写入有效的默认时区值（32 = UTC+8 CST），避免程序启动时读到空文件。**建议将此改进同步回 open_dial**。

#### 待同步改动

| 优先级 | 改动 |
|--------|------|
| 🟢 可选 | 将 `mkdir -p` + 默认内容写入逻辑同步回 open_dial |

---

### 3.7 模块：roamlink（roamlink.c / dialStrategy.c）

#### 共享模块现状

eg25 的 roamlink 功能实际来自共享模块：

```
/home/tronlong/lyp/code/rtms_sdk/apps/modem_mng/roamlink/roamlink.c
```

该模块基于 open_dial 的 `roamlink.c`，通过 `#ifdef USE_EG25_DIAL` 分支处理平台差异，主要接口均已同步：

| 函数 | open_dial | 共享模块 | 同步状态 |
|------|-----------|----------|----------|
| `roamlink_probe()` | ✅ | ✅ | 已同步 |
| `roamlink_start_service()` | `(int smd_fd)` | `(void)` + `roamlink_set_fd()` | ✅ 适配 |
| `roamlink_license_restore()` | ✅ | ✅ | 已同步 |
| `roamlink_license_backup_reboot()` | ✅ | ✅ | 已同步 |
| `roamlink_read_policy()` | ✅ | ✅ | 已同步 |
| `roamlink_is_master_running()` | /proc + TCP | /proc + TCP | 已同步 |
| `roamlink_start_master()` | ✅ | ✅ | 已同步 |
| `roamlink_send_cmd()` | ✅ | ✅ | 已同步 |
| `roamlink_mark_network_type()` | ✅ | ✅ | 已同步 |
| `roamlink_stop_service()` | ✅ | ✅ | 已同步 |

#### 🟡 遗留：`roamlink_dial/dialStrategy.c` 为废弃代码

```c
// eg25/roamlink_dial/dialStrategy.c
// 使用旧式 system() 调用，已被共享模块取代
system("echo 'RBstartServiceMaster' | nc 127.0.0.1 5568");

// can_ping() 阻塞约 4 秒
system("ping -c 4 %s");

// main_test() 含过时的决策逻辑
```

此文件已无实际调用路径，**建议整体删除**，避免混淆。

#### 待同步改动

| 优先级 | 改动 |
|--------|------|
| 🟡 建议 | 删除 `roamlink_dial/dialStrategy.c` 废弃文件 |

---

## 四、总结

### 4.1 已同步的重要改动

以下 open_dial 的改动在 eg25（包含共享模块）中已有对应实现：

| open_dial 提交 | 内容 | eg25 同步状态 |
|----------------|------|---------------|
| `9c1ec55` | 双通道切换逻辑内置化 | ✅ 通过共享 roamlink 模块同步 |
| `2923dd1` | 增加 CEREG 检测 | ✅ `nw_at_get_cereg_stat()` 已同步 |
| `31a419f` | 时区功能 | ✅ tz 模块已同步（且有改进） |
| `0a422dd` | 自动切运营商 | ✅ apn + cc_deque 逻辑已同步 |
| `35784dc` | 删除 CFUN 操作 | ✅ eg25 L2 恢复仍有 CFUN，但已隔离在恢复逻辑中 |
| `5a0dd50` | 修复 Ql_send Bug | ✅ 已同步 |

---

### 4.2 待同步改动（按优先级排序）

#### 🔴 P0 — 必须修复（影响稳定性的 Bug）

| 编号 | 模块 | 问题 | 修复方式 |
|------|------|------|----------|
| P0-1 | at | `tv_usec = timeout_ms % 1000` 缺少 `* 1000` | 改为 `(timeout_ms % 1000) * 1000` |
| P0-2 | at | `timeout` 结构体在 `while(1)` 外初始化，循环后失效 | 将 `timeout` 赋值移入循环内 |
| P0-3 | dial | `dial_stat_net_connected` 缺少 TCP 主动探测 | 参考 open_dial commit `9d15833` 补充 TCP 探测逻辑 |
| P0-4 | dial | 注册超时后未发 `AT+COPS=0` 强制重选网络 | 超时分支补充 `Ql_SendAT("AT+COPS=0\r\n", ...)` |
| P0-5 | dial | 重拨时未重置 `dial_timer = 0` | `reg_timeout_handler` 路径增加 `dial_timer = 0` |
| P0-6 | sim | `sim_op_stat_get_iccid` case 后缺少 `break`，穿透执行 `get_imsi` | 补充 `break;` |
| P0-7 | apn | ICCID 前缀匹配缺少 `prefix_len > 0` 保护 | 参考 open_dial `apn_scan_from_json()` 修复 |

#### 🟡 P1 — 建议修复（代码健壮性和可维护性）

| 编号 | 模块 | 问题 | 修复方式 |
|------|------|------|----------|
| P1-1 | apn | 同时使用两套 JSON 库（json-c + cJSON） | 移除 json-c 依赖，统一使用 cJSON |
| P1-2 | apn | `memcpy` 复制字符串无边界检查、无 `\0` 终止 | 改为 `snprintf` |
| P1-3 | sim | `sim_op()` 调用使用硬编码整数而非枚举常量 | 引入枚举常量 |
| P1-4 | roamlink | `roamlink_dial/dialStrategy.c` 为废弃代码 | 整体删除 |

#### 🟢 P2 — 可选改进（从 eg25 反向同步到 open_dial）

| 编号 | 模块 | 内容 |
|------|------|------|
| P2-1 | nw | `E_QL_MCM_NW_SERVICE_FULL` 严格驻网检查（eg25 → open_dial） |
| P2-2 | tz | `mkdir -p` + 默认内容初始化（eg25 → open_dial） |
| P2-3 | at | `smd_fd` 防重复打开保护（eg25 → open_dial） |

---

### 4.3 整体评估

**eg25 主要问题集中在 at 模块和 dial 模块**，两个 `select()` 相关的 Bug（P0-1、P0-2）可能导致高 CPU 占用甚至进程被 Kill，是最高优先级问题。dial 模块缺少的三处逻辑（TCP 探测、AT+COPS=0、dial_timer 重置）来自 open_dial 最重要的稳定性提交 `9d15833`，在生产环境中已被验证为关键修复。

**eg25 对平台有独特的增强**（L1/L2/L3 恢复机制、心跳诊断、模组信息打印），这些功能不应反向同步到 open_dial，属于 EG25 平台特有适配。

**共享 roamlink 模块同步状态良好**，主要功能已与 open_dial 对齐，无需额外工作。

总体来看，完成 P0 列表中的 7 处修复后，eg25 的稳定性将显著提升，可达到与 open_dial 同等的可靠性水平。

<!-- GENERATION_COMPLETE: 2026-05-27 -->
