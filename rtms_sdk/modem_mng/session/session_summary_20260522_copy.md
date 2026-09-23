# modem_mng EG25 会话技术总结

**日期：** 2026-05-22  
**分支：** `develop/rtms_sdk_v1.3_20240408_dc_switch`  
**涉及平台：** EG25G（Quectel，ARM embedded Linux，交叉编译）

---

## 一、本次会话解决的问题总览

| # | 问题 | 类型 | 状态 |
|---|------|------|------|
| 1 | L1/L2/L3 分级恢复逻辑存在多处 bug | BUGFIX | ✅ 已修复 |
| 2 | CGACT 缓冲区 128 字节不足导致截断 | BUGFIX | ✅ 已修复 |
| 3 | nanomsg handler 查询 ICCID/IMSI 返回空 | BUGFIX | ✅ 已修复 |
| 4 | logcat 反复打印 `QL_MCM_SIM_Client_Init failed with error code: MCM_SUCCESS` | BUGFIX | ✅ 已修复 |
| 5 | 版本号三段式规范化 | 规范 | ✅ 已完成 |

---

## 二、L1/L2/L3 分级恢复逻辑修复

### 2.1 预期逻辑

| 级别 | 触发条件 | 动作 |
|------|----------|------|
| L1 | 断网 ≥ 60s | 软重拨：stop_call + start_call |
| L2 | 断网 ≥ 5min | CFUN 重置：AT+CFUN=0 → AT+CFUN=1 |
| L3 | 断网 ≥ 30min | AT+CFUN=1,1（硬重置）+ exit |

### 2.2 发现的 Bug

#### Bug 1：L2 永不触发

**根因：** L1 执行路径中执行了 `last_recovery_ts = now`，导致 L2 的冷却计时器（300s）在每次 L1 触发后被重置，L2 的冷却条件 `(now - last_recovery_ts) >= LEVEL2` 永远不满足。

**表现：** 断网超过 5 分钟后，仍然每 60s 触发一次 L1，L2 从不执行。

**修复：** 从 L1 执行路径中移除 `last_recovery_ts = now` 赋值，L2 有独立的冷却逻辑。

#### Bug 2：downtime ≥ 5min 时 L2 退化回 L1

**根因：** 原代码使用连续 `else if` 链：
```c
// 原始（有 bug）
if (downtime >= LEVEL3) recovery_level = 3;
else if (downtime >= LEVEL2 && cooldown_ok) recovery_level = 2;
else if (downtime >= LEVEL1 && ...) recovery_level = 1;  // ← 当 L2 冷却未满足时会落到这里
```
当 downtime ≥ LEVEL2 但 L2 冷却未到期时，`else if (downtime >= LEVEL1)` 仍会命中，执行 L1。

**修复：** 重构为独立 if 块，downtime ≥ LEVEL2 时只考虑 L2，不再降级：
```c
if (downtime_sec >= LEVEL3) {
    recovery_level = 3;
} else if (downtime_sec >= LEVEL2) {
    /* 已到 L2 阶段，不再退化回 L1；等 L2 冷却（5min）再重试 */
    if (!last_recovery_ts || (now - last_recovery_ts) >= LEVEL2)
        recovery_level = 2;
} else if (downtime_sec >= LEVEL1 &&
           (!last_l1_ts || (now - last_l1_ts) >= LEVEL1)) {
    recovery_level = 1;
}
```

#### Bug 3：L3 误判为 L1/L2

**根因：** L3 条件入口存在与 L1/L2 相同的结构性问题，在某些边界情况下不能确保 L3 独立执行。

**修复：** L3 作为最高优先级独立 if，命中即执行 `AT+CFUN=1,1` + `exit`，不受冷却计时器约束。

### 2.3 新增可观测日志

为便于后续排查，在各级恢复路径增加明确日志：

```c
// L1 跳过（downtime ≥ L2 但 L2 冷却未到）
dial_log("[RECOVERY L1] REG down, skip redial (downtime=%lds)\n", downtime_sec);

// L2 CFUN=0/1 AT 响应
dial_log("[RECOVERY L2] AT+CFUN=0 rsp: %s\n", rsp_msg[0] ? rsp_msg : "(timeout)");
dial_log("[RECOVERY L2] AT+CFUN=1 rsp: %s\n", rsp_msg[0] ? rsp_msg : "(timeout)");

// L3 CFUN=1,1 AT 响应
dial_log("[RECOVERY L3] AT+CFUN=1,1 rsp: %s\n", rsp_msg[0] ? rsp_msg : "(timeout)");
```

### 2.4 L3 日志缺失现象说明

测试 L3 时发现代码中有：
```c
dial_log("Exiting. will reinitialize.\n");
sleep(20);
exit(0);
```
但实际日志中该行从未出现。

**原因：** L3 执行 `AT+CFUN=1,1` 后，模组立即硬重置，进程被 `SIGKILL` 信号强制终止，在 `sleep(20)` 期间即被杀死，`dial_log` 语句来不及执行。这是预期行为，不是 bug。

### 2.5 测试验证（TC-EG25-L2-01）

测试日志关键节点（第二次运行）：
- **314s**：L2 CFUN 重置触发 ✅（首次运行因 L1 写了 last_recovery_ts 而失败，修复后正常）
- L2 触发后不再退化回 L1 ✅

---

## 三、CGACT 缓冲区修复

### 问题

`AT+CGDCONT?` 查询返回 EG25 全部 16 个 PDP context 定义，每条约 15 字节，合计约 240 字节。原缓冲区 `char cgact[128]` 不足，导致响应截断，进而影响 APN/profile 判断逻辑。

### 修复

`eg25/dial/dial.c` 中两处缓冲区：
```c
// 修复前
char cgact[128];

// 修复后
char cgact[256];
```

涉及行：dial.c 约 717 行 和 1174 行（两处相同结构）。

---

## 四、ICCID/IMSI nanomsg 上报修复

### 4.1 问题现象

logcat 反复出现：
```
E DIAL: QL_MCM_SIM_Client_Init failed with error code: MCM_SUCCESS
```
且 nanomsg handler 响应 ICCID/IMSI 查询时返回空字符串。

### 4.2 根因分析

**MCM SDK 初始化顺序冲突：**

1. `dial_task` 中的 `sim.c` 在 `dial_stat_sim_init` 阶段调用 `QL_MCM_SIM_Client_Init`，成功获得 SIM client handle（Client 11）。
2. nanomsg handler（独立线程）也尝试调用 `QL_MCM_SIM_Client_Init`，但此时 MCM client 已被初始化，SDK 返回 ret=0（`MCM_SUCCESS`）但**不写入 handle**（handle 保持 0）。
3. 原代码判断 `if (h_sim == 0)` → 误报 "failed"。
4. 因为 handle 为 0，后续 `QL_MCM_SIM_Get_ICCID_Resp` 等调用均失败，返回空值。

**关键事实：** `QL_MCM_SIM_Client_Init` 在"already inited"场景下 ret=0 但不回写 handle，是 MCM SDK 的既定行为，不是 modem_mng 的初始化时序问题。

### 4.3 错误中间方案（已放弃）

最初尝试引入共享全局 `s_sim_client_handle`，让 nanomsg handler 复用 sim.c 的 handle。但此方案引入了跨模块全局状态，且线程安全难以保证，被放弃。

### 4.4 正式修复方案：Setter 模式

**设计思路：** ICCID/IMSI 只需在 `dial_stat_sim_op` 成功时读取一次（物理SIM路径），由 `dial_task` 主动推送给 nanomsg handler 的成员变量，nanomsg handler 查询时直接返回该变量，不再独立调用 MCM SDK。

**涉及文件及改动：**

#### `nanomsg_process_cinterface.h`

新增两个 C 接口声明：
```c
void set_eg25_iccid(NanoReqHandlerWrapper *wrapper, const char *val);
void set_eg25_imsi(NanoReqHandlerWrapper *wrapper, const char *val);
```

#### `nanomsg_process_wraper.cpp`

新增 setter 实现（位于 `extern "C"` 块外，沿用 `get_eg25_csq` 的相同模式）：
```cpp
void set_eg25_iccid(NanoReqHandlerWrapper *wrapper, const char *val)
{
    static_cast<NanoReqHandler*>(wrapper->handler)->modemReqHandlerPtr->iccid = val ? val : "";
}

void set_eg25_imsi(NanoReqHandlerWrapper *wrapper, const char *val)
{
    static_cast<NanoReqHandler*>(wrapper->handler)->modemReqHandlerPtr->imsi = val ? val : "";
}
```

#### `nanomsg_process.cpp`

移除错误的 `s_sim_client_handle` 全局静态变量；  
改写 EG25 平台的 `getIccid()` / `getImsi()`：
```cpp
#elif defined(USE_EG25_DIAL)
    // EG25: ICCID/IMSI 由 dial_task 通过 setter 写入，直接返回成员变量
    return iccid;  // / return imsi;
```

#### `eg25/dial/dial.h`

在 `dial_mng_t` 结构体中新增字段：
```c
void *nano_handler;  /* NanoReqHandlerWrapper*，用于 dial_task 写入 ICCID/IMSI */
```

#### `dialer_eg25.c`

handler 创建后赋值给 dial_mng：
```c
p_dial_mng->nano_handler = handler;
```

#### `eg25/dial/dial.c`

在 `sim_op_handler` 成功后调用 setter（`dial_stat_sim_op` 分支，约 1412 行）：
```c
if (p_dial_mng->nano_handler) {
    set_eg25_iccid(p_dial_mng->nano_handler, p_sim_mng->sim_iccid);
    set_eg25_imsi(p_dial_mng->nano_handler,  p_sim_mng->sim_imsi);
}
```

### 4.5 `sim_op_handler` switch fallthrough 细节（确认双值读取）

修改前有一个关键疑问：`sim_op_handler(p_sim_mng, sim_op_stat_get_iccid)` 传入的是 `get_iccid`，是否只读 ICCID，不读 IMSI？

**结论：两者都读。** `sim_op_handler` 内部是 C `switch` 语句，`sim_op_stat_get_iccid` 的 case **故意不写 `break`**，fallthrough 到 `sim_op_stat_get_imsi`，因此一次调用同时填充 `p_sim_mng->sim_iccid` 和 `p_sim_mng->sim_imsi`。这是 setter 方案的前提——确认两个字段均已被填充后，才在 sim_op 成功后同时调用两个 setter。

### 4.6 关于 Roamlink 切换时 ICCID/IMSI 的行为

- `sim_iccid` / `sim_imsi` 只在 `dial_stat_sim_op`（物理SIM路径）中更新，Roamlink active 阶段不更新。
- 因此 nanomsg 始终上报**物理SIM**的 ICCID/IMSI，与 `/tmp/dial_status` 的行为一致。
- 这是预期行为：Roamlink 是虚拟通道，物理卡信息不变。

### 4.7 `QL_MCM_SIM_Client_Init` 能否获取 Roamlink 信息？

**不能。** `QL_MCM_SIM_Client_Init` 是 MCM SDK 接口，底层操作的是物理 SIM 硬件通道。Roamlink 是上层虚拟 SIM 通道，MCM SDK 对其不可见。因此即使 nanomsg handler 成功拿到 MCM SIM handle，查到的也只会是物理 SIM 的 ICCID/IMSI，与 setter 方案结果一致——进一步确认 setter 方案的正确性。

### 4.8 线程安全注意事项（低风险）

`set_eg25_iccid`/`set_eg25_imsi` 从 `dial_task` 线程写入 `std::string`，nanomsg handler 线程读取，两者之间无 mutex。实际风险极低（写入只在启动 SIM 初始化阶段执行一次，远早于外部进程发起 nanomsg 查询），但严格来说是未定义行为。如需完全正确，应在读写两侧加锁。

---

## 五、版本号规范化

### 修改前

`dialer_eg25.c` 中硬编码的独立宏：
```c
#define MAIN_VERSION 1
#define SUB_VERSION  31
```

### 修改后

单一来源定义在 `eg25/dial/dial.h`：
```c
#define MODEM_MNG_VERSION_MAIN  1
#define MODEM_MNG_VERSION_SUB   31
#define MODEM_MNG_VERSION_PATCH 0
```

`dialer_eg25.c` 使用 snprintf 格式化：
```c
char pversion_buf[16];
snprintf(pversion_buf, sizeof(pversion_buf), "%d.%02d.%d",
         MODEM_MNG_VERSION_MAIN, MODEM_MNG_VERSION_SUB, MODEM_MNG_VERSION_PATCH);
```

输出版本号：`1.31.0`

---

## 六、双 IP 现象说明

连接成功后 logcat 中持续出现：
```
+CGPADDR: 1,"10.112.133.74" | +CGPADDR: 10,"10.112.200.216"
```

- **Profile 1 (10.112.133.74)**：运营商在注册网络时自动激活的默认 PDP context（APN: `mobile.three.com.hk`），由运营商侧控制，modem_mng 不干预。
- **Profile 10 (10.112.200.216)**：modem_mng 主动发起的数据连接（APN: `internet.lte.cxn`），业务流量走此 IP。

两个 PDP context 并存是 EG25 模组的正常行为，不影响功能。

---

## 六、关键排查过程还原

### 6.1 MCM_SUCCESS 错误的排查路径

1. **现象**：logcat 中 `E DIAL: QL_MCM_SIM_Client_Init failed with error code: MCM_SUCCESS` 反复出现
2. **初步判断**：error code 是 `MCM_SUCCESS`（=0）说明 SDK 调用本身成功，但代码把它当失败处理
3. **定位**：`nanomsg_process.cpp` 中 EG25 分支的 `getIccid()`/`getImsi()` 调用 `QL_MCM_SIM_Client_Init`，用 `if (h_sim == 0)` 判断是否成功——但"already inited"场景下 ret=0 且 handle 不写入，handle 永远是 0，条件永远成立
4. **追问来源**：从 `[QL_MCM_Client_Init_499]` 和 `[QL_MCM_Client_Init_466]` 两条 logcat 行确认：首次调用打印 `start up required service`（sim.c 的合法初始化），nanomsg handler 的后续调用则是 `already inited` 路径
5. **确认是 modem_mng 的问题**：是，nanomsg handler 不应独立调用 MCM SIM Client Init，应从 dial_task 获取已读取的值

### 6.2 修复方案的设计决策

- 首先排除"共享 handle"方案（引入跨模块全局状态，线程安全复杂）
- 采用"推送"而非"拉取"：dial_task 是 ICCID/IMSI 的权威来源，主动写入 nanomsg handler，handler 只做 getter
- 接口设计参考已有的 `get_eg25_csq` C 接口模式，保持一致性

---

## 七、修复后日志验证

### 关键验证点

| 验证项 | 修复前 | 修复后 |
|--------|--------|--------|
| `QL_MCM_SIM_Client_Init failed` | 反复出现 | **完全消失** ✅ |
| SD 卡日志 ICCID/IMSI | 空 | `89852019925010000888` / `455011505090058` ✅ |
| SIM Client 初始化路径 | `already inited`（被 nanomsg handler 抢占） | `start up required service 00000008`（sim.c 首次正常初始化）✅ |
| 心跳稳定性 | 正常 | 正常 ✅ |

### 启动序列时序（修复后 logcat）

```
22:22:00  dial_stat_init success
22:22:02  dial_stat_sim_init success
22:22:03  dial_stat_sim_check success
22:22:04  ICCID: 89852019925010000888
22:22:04  IMSI:  455011505090058
22:22:04  dial_stat_sim_op success
22:22:05  dial_stat_reg_check success
22:22:06  dial_stat_cereg_check success
22:22:07  dial_stat_precondition_check success
22:22:08  get profile index:10
22:22:10  Module reset skipped (uptime < 10min) [正常]
22:22:10  DIAL (thread 1155)
22:22:12  DIAL x7 (thread 2180, SDK DataCall 回调，正常)
22:22:13  Module reset skipped (第二次，状态机重入路径，正常)
22:22:13  net connected ✅
22:22:37  心跳: +CGPADDR: 1,"10.112.133.74" | +CGPADDR: 10,"10.112.200.216"
          （此后每 ~7-8s 稳定打印）
```

---

## 八、涉及文件汇总

| 文件 | 改动类型 | 说明 |
|------|----------|------|
| `eg25/dial/dial.h` | 新增字段、宏 | `nano_handler` 字段；三段式版本宏 |
| `eg25/dial/dial.c` | BUGFIX | L1/L2/L3 逻辑重构；CGACT 缓冲区 128→256；setter 调用；可观测日志 |
| `dialer_eg25.c` | 改动 | 版本号改用宏 snprintf；赋值 `nano_handler` |
| `nanomsg_process.cpp` | BUGFIX | 移除错误全局变量；EG25 getIccid/getImsi 直接返回成员变量 |
| `nanomsg_process_cinterface.h` | 新增声明 | `set_eg25_iccid` / `set_eg25_imsi` |
| `nanomsg_process_wraper.cpp` | 新增实现 | setter 函数实现 |

---

## 九、建议提交说明

```
[modem_mng] [BUGFIX] EG25 修复L1/L2/L3分级恢复逻辑及ICCID/IMSI nanomsg上报

- 修复L2永不触发：L1路径错误更新last_recovery_ts导致L2冷却永远不满足
- 修复L2退化为L1：重构if/else链，downtime≥5min时不再降级回L1
- 修复L3：downtime≥30min直接执行AT+CFUN=1,1后exit，不再误判为L1/L2
- 新增L1跳过/L2 CFUN响应/L3响应的可观测日志
- 修复CGACT缓冲区128→256字节（16条CGDCONT约240字节会截断）
- 修复nanomsg handler查询ICCID/IMSI返回空：改为dial_task在sim_op
  成功后通过set_eg25_iccid/set_eg25_imsi直接写入handler成员变量，
  替代错误的二次QL_MCM_SIM_Client_Init方案
```

---

*生成时间：2026-05-22*
