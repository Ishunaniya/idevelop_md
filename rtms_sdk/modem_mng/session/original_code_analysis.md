# 原始代码功能分析（git HEAD / 会话修改之前）

> 本文档记录 git 仓库最后一次 commit（HEAD）的状态，即**我们本轮会话开始前**两个平台的原始逻辑。

---

## 一、分级故障恢复（L1/L2/L3）

**两个平台都有**，逻辑相同，阈值相同。

| 平台 | 文件 | 有L1/L2/L3 |
|------|------|-----------|
| EC200A | `dialer_ec200a.cpp` | ✅ 有 |
| EG25 | `eg25/dial/dial.c` | ✅ 有 |

### 三级阈值

| 等级 | 触发条件 | EC200A动作 | EG25动作 |
|------|---------|-----------|---------|
| L1 | 断网 > 60s | `ql_data_call_stop` + `ql_data_call_start` | `dail_start_data_call`（含注册检查） |
| L2 | 断网 > 5min | `restart_cfun_safe()`（CFUN 0→1） | `AT+CFUN=0` + `AT+CFUN=1`（原始AT命令） |
| L3 | 断网 > 30min | ⚠️ **仅打印日志，硬重启被注释掉** | ⚠️ **仅打印日志，硬重启被注释掉** |

### 原始 L3 的关键细节

两个平台的 L3 原始行为完全一致：
```c
dial_log("[RECOVERY L3] FATAL: Network down for 30mins. But Not REBOOTING MODULE...\n");
sync();
sleep(5);
// system("serial_atcmd at+cfun=1,1");  // ← 硬重启被注释掉，实际什么都不做
```

**结论：原始 L3 是空操作（只记日志）。**

---

## 二、Roamlink 双卡切换

**两个平台原始代码均无 Roamlink 功能。**

| 平台 | 文件 | 有Roamlink |
|------|------|-----------|
| EC200A | `dialer_ec200a.cpp` | ❌ 无（0处引用） |
| EG25 | `eg25/dial/dial.c` | ❌ 无（0处引用） |

---

## 三、我们的会话中新增/修改的内容

以下功能**均为本轮会话新增**，不在原始代码中：

### 新增文件
| 文件 | 功能 |
|------|------|
| `ec200a/roamlink/roamlink_ec200a.{h,c}` | EC200A Roamlink适配层 |
| `eg25/roamlink/roamlink_eg25.{h,c}` | EG25 Roamlink适配层 |
| `status/dial_status.{h,c}` | 通用状态模块，写 `/tmp/dial_status` |

### 修改内容汇总

**L1/L2/L3 改进：**
- EC200A L1：增加 REG 状态前置检查，`last_l1_ts` 独立节流（不再与 L2 共用 `last_recovery_ts`）
- EC200A L2：改为调用 `restart_cfun_safe()`（限制调用次数和最小间隔）
- 两平台 L3：从"注释掉的空操作"改为真正执行 `AT+CFUN=1,1` + `exit(1)`

**Roamlink 集成：**
- 两平台均集成 Roamlink 初始化和通道守护逻辑
- **重构前（本次会话之前）**：Roamlink 切换挂在 L3（30分钟）里触发
- **重构后（本次会话）**：Roamlink 切换独立触发（断网 ≥ 3分钟），L3 只做硬重启

**其他：**
- 两平台均集成 `dial_status` 心跳写入
- Ping 抖动过滤（连续失败 3 次才启动故障计时）
- 断网统计字段（outage_count / last_outage_sec / total_outage_sec）
- `CMakeLists.txt` 加入新文件

---

## 四、参考实现分析：集成双卡切换后还需要L1/L2/L3吗？

> 参考实现：`/home/tronlong/lyp/code/open_dial_for_artery/src/dial/dial.c`

### 结论：参考实现完全没有 L1/L2/L3

参考实现用**状态机 + TCP失败计数**替代了L1/L2/L3的全部职能，这两套机制不共存。

---

### 参考实现的恢复机制

**核心触发器：**

| 常量 | 值 | 含义 |
|------|----|------|
| `TCP_TEST_INTERVAL_SECONDS` | 60s | TCP连通性测试间隔 |
| `TCP_FAIL_THRESHOLD` | 3次 | 连续失败3次（即约3分钟）触发通道切换 |
| `DIAL_TIMEOUT_SECONDS` | 60s | rx_packets超时（无数据包）→ 重拨 |
| `ROAMLINK_CONNECT_WAIT_SEC` | 300s | Roamlink启动后等待连通的最大时间 |
| `ROAMLINK_FAIL_THRESHOLD` | 3次 | Roamlink连续失败3次触发切换 |
| `SIM_FALLBACK_RETRY_SEC` | 300s | 策略1：在SIM备用通道稳定300s后尝试回切Roamlink |
| `ROAMLINK_FALLBACK_RETRY_SEC` | 300s | 策略2：在Roamlink备用通道稳定300s后尝试回切SIM |

**按策略的故障处理行为（无L1/L2/L3）：**

| 策略 | SIM失败时 | Roamlink失败时 | 备注 |
|------|---------|--------------|------|
| 策略1 PREFER_ROAMLINK | 切换到SIM备用，300s后回切Roamlink | 已在SIM备用，无需额外动作 | 主通道=Roamlink |
| 策略2 PREFER_SIM | 切换到Roamlink备用，300s后回切SIM | 切回SIM重拨 | 主通道=SIM |
| 策略3 FORCE_ROAMLINK | 不走SIM路径，不适用 | 重启Roamlink服务（不切SIM） | 永远不走SIM |
| 策略4 FORCE_SIM | 直接重拨SIM（`dial_stat_reg_check`） | 不走Roamlink路径，不适用 | 永远不切Roamlink |

**注册超时（额外补充，仅SIM路径）：**
- 策略1/2 + Roamlink可用：注册超时 → 切换到Roamlink通道
- 策略4 / Roamlink不可用：注册超时 → CFUN重置（0→1）→ 重新注册

---

### 各策略的"恢复手段"与L1/L2/L3对应关系

**策略1/2/3（涉及双卡切换）：**

旧的L1/L2/L3在参考实现中被完全替换，对应关系如下：

| 旧机制 | 对应的参考实现机制 | 是否保留 |
|--------|-----------------|---------|
| L1（60s软重拨） | TCP测试3次失败（~3分钟）→ 通道切换，不再重拨SIM | ❌ 不保留 |
| L2（5min CFUN重置） | Roamlink连通等待超时（300s）→ 切换回SIM重新注册 | ❌ 概念消失 |
| L3（30min硬重启） | **参考实现无此机制**，通道互切无穷循环直到成功 | ❌ 不保留 |

**策略4（FORCE_SIM，永远不切Roamlink）：**

没有Roamlink可切，只能对SIM做恢复操作：

| 触发条件 | 动作 | 对应旧机制 |
|---------|------|---------|
| TCP连续失败3次（~3min） | 直接重拨（`dial_stat_reg_check`） | 类似旧L1，但触发更慢（3min而非60s） |
| 注册超时（120s） | CFUN重置（0→1）→ 重新注册 | 类似旧L2，但触发条件是注册失败，非时间 |
| **无** | **参考实现无30分钟硬重启** | 旧L3不存在 |

---

### 对我们项目的设计建议

**正确做法（跟参考实现对齐）：**

```
策略1/2（双卡切换）  →  完全移除L1/L2/L3，只用TCP失败计数触发通道切换
策略3（强制Roamlink）→  完全移除L1/L2/L3，Roamlink失败只重启服务
策略4（强制SIM）    →  移除基于时间的L1/L2/L3，改为TCP失败计数触发重拨
                        注册超时 → CFUN重置，这个可以保留
```

**当前我们代码的问题（本轮会话的中间状态）：**

我们目前是**混合设计**：保留了基于时间的L1（60s）/L2（5min）/L3（30min），
同时又加了独立的Roamlink触发（3min）。这导致：

- 对策略1/2的设备：Roamlink会在3min时触发，但L1在60s时就已经在重拨SIM了，逻辑冲突
- L3依然存在，但参考实现根本没有30分钟的概念
- 整体比参考实现复杂，且行为难以预期

**建议下一步：按策略拆分恢复路径，彻底移除L1/L2/L3。**

---

## 五、EG25 与 EC200A 原始实现差异

| 对比项 | EC200A (`dialer_ec200a.cpp`) | EG25 (`eg25/dial/dial.c`) |
|--------|------------------------------|--------------------------|
| 语言 | C++ | C |
| 状态机 | 枚举 Stage（ST_STATUS/SIM/SIGNAL/PING/RECOVERY） | 函数式心跳循环 |
| L2 实现 | `restart_cfun_safe()`（封装函数） | 直接发 AT+CFUN=0/1 |
| 拨号API | `ql_data_call_start/stop`（SDK） | `dail_start_data_call`（封装） |
| L1节流 | 原始用 `last_recovery_ts` 统一节流 | 原始用 `last_recovery_ts` 统一节流 |
