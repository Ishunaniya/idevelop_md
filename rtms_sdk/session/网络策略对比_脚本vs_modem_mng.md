# 网络策略对比：check_network.sh vs modem_mng

> 代码出处：
> - 脚本：`/home/tronlong/lyp/zqzb/roamlink_deploy_rtms_newC_260522/check_network.sh`
> - modem_mng：`eg25/dial/dial.c`、`roamlink/roamlink.h`

---

## 策略编号定义（两边一致）

| 值 | 名称 | 含义 |
|---|---|---|
| 1 | PREFER_ROAMLINK | 优先 Roamlink，失败可切换到物理 SIM |
| 2 | PREFER_SIM | 优先物理 SIM，失败可切换到 Roamlink |
| 3 | FORCE_ROAMLINK | 强制 Roamlink，不切换 |
| 4 | FORCE_SIM | 强制物理 SIM，不切换（**默认值**） |

配置文件：`/usrdata/network.ini`，字段 `network_select`；缺失时默认策略 4。

---

## 脚本逻辑（check_network.sh）

### 主循环结构

```
启动对应通道
↓
无条件 sleep 600（10 分钟）
↓
check_celluar_network（每 2s 检测一次，最多 10 次，共 20s）
↓
失败：按策略切换或重启
成功：sleep 1800（30 分钟）后再下次检测
网络仍断：sleep 600（10 分钟）后再检测
```

### 各策略行为

**策略 1（PREFER_ROAMLINK）**
- 启动 RBMaster 服务，等 10 分钟
- 检测失败 → 停 Roamlink，启动 dial（切到物理 SIM）
- 检测成功 → 等 1800s

**策略 2（PREFER_SIM）**
- 启动 dial，等 10 分钟
- 检测失败 → 停 dial，启动 RBMaster 服务（切到 Roamlink）
- 检测成功 → 等 1800s

**策略 3（FORCE_ROAMLINK）**
- 启动 RBMaster 服务，等 10 分钟
- 检测失败 → 停 Roamlink，重新启动 Roamlink（不切 SIM）
- 检测成功 → 等 1800s

**策略 4（FORCE_SIM）**
- 启动 dial，等 10 分钟
- 检测失败 → 停 dial，重新启动 dial（不切 Roamlink）
- 检测成功 → 等 1800s

### 脚本局限

- 切换后**无回切机制**，切过去就一直用那个，下次检测失败才切回来
- 策略 4 失败只是重启 dial 进程，无分级恢复
- license 缺失时无总超时，`while true` 永远等待，不会降级

---

## modem_mng 逻辑（dial.c）

### 相关超时常量（roamlink.h）

| 常量 | 值 | 含义 |
|---|---|---|
| `ROAMLINK_SWITCH_TIMEOUT_SEC` | 180s（3 分钟） | SIM 失败多久后切 Roamlink |
| `ROAMLINK_CONNECT_TIMEOUT_SEC` | 300s（5 分钟） | 发出 start 后等待首次 ping 通的最长时间 |
| `ROAMLINK_FAIL_TIMEOUT_SEC` | 300s（5 分钟） | ping 通后再次失联超时切回 SIM |
| `ROAMLINK_NO_DATA_TIMEOUT_SEC` | 120s（2 分钟） | 业务层 rx_packets 无增长的切换阈值 |
| `SIM_FALLBACK_RETRY_SEC` | 300s（5 分钟） | 策略 1：SIM 备用稳定多久后尝试回切 Roamlink |
| `ROAMLINK_FALLBACK_RETRY_SEC` | 300s（5 分钟） | 策略 2：Roamlink 备用稳定多久后尝试回切 SIM |
| `LICENSE_WAIT_TIMEOUT_SEC` | 300s（5 分钟） | license 下载等待总超时，超时降级 FORCE_SIM |

### 各策略行为

**策略 1（PREFER_ROAMLINK）**
- 启动时直接走 Roamlink 通道（`dial_stat_roamlink_starting`）
- Roamlink 5 分钟内无法 ping 通 → 切回物理 SIM
- Roamlink ping 通后再次失联 5 分钟 → 切回物理 SIM
- Roamlink 无数据流量 2 分钟 → 切回物理 SIM
- **SIM 备用稳定 300s 后，自动尝试切回 Roamlink（循环回切）**

**策略 2（PREFER_SIM）**
- 启动时走物理 SIM 通道
- SIM 失败 3 分钟 → 切换到 Roamlink
- Roamlink 5 分钟内无法 ping 通 → 重启 Roamlink 服务
- Roamlink ping 通后再次失联 5 分钟 → 切回物理 SIM
- **Roamlink 备用稳定 300s 后，自动尝试切回物理 SIM（循环回切）**

**策略 3（FORCE_ROAMLINK）**
- 启动时直接走 Roamlink 通道
- Roamlink 失败（connect timeout / fail timeout / 无数据流量 2 分钟）→ 重启 RBMaster 服务，回到 starting 状态重新等待
- **永不切换到物理 SIM**

**策略 4（FORCE_SIM）**
- 启动时走物理 SIM 通道
- **永不切换到 Roamlink**
- 断网后走 L1/L2/L3 分级恢复：
  - **L1（60s）**：软重拨（stop + start data call）
  - **L2（5min）**：射频复位（AT+CFUN=0 → AT+CFUN=1）
  - **L3（30min）**：硬重启（AT+CFUN=1,1 + exit）

---

## 差异汇总

| 对比项 | 脚本 | modem_mng |
|---|---|---|
| 初始等待时间 | 无条件 sleep 600（10 分钟） | 无固定等待，由状态机驱动 |
| SIM 失败切 Roamlink | 检测失败后立即切（~20s） | 3 分钟（ROAMLINK_SWITCH_TIMEOUT_SEC） |
| Roamlink 失败切 SIM | 检测失败后立即切（~20s） | 5 分钟（ROAMLINK_CONNECT/FAIL_TIMEOUT_SEC） |
| 回切机制（策略 1/2） | 无，切过去就保持 | 有，备用通道稳定 300s 后自动回切，循环 |
| 策略 4 失败恢复 | 重启 dial 进程 | L1/L2/L3 三级递进，最终硬重启模块 |
| 业务层无流量检测 | 无 | 有（120s 无 rx_packets 增长触发切换） |
| license 等待超时 | 无，永远等待 | 300s 超时后降级 FORCE_SIM 继续运行 |

---

## 以策略 1（PREFER_ROAMLINK）为例：modem_mng 状态机完整流程

脚本策略 1 只有三步：启动 → 等 10 分钟 → 检测失败就切 SIM，没有后续。

modem_mng 策略 1 的完整状态流转：

```
启动
↓
Roamlink 通道激活（dial_stat_roamlink_starting）
↓
[等待首次 ping 通，最长 300s（ROAMLINK_CONNECT_TIMEOUT_SEC）]
  · 超时仍未通 → 切回物理 SIM，进入 dial_stat_reg_check
↓
Roamlink ping 通（dial_stat_roamlink_active）
↓
[持续 ping 监控]
  · ping 失联超过 300s（ROAMLINK_FAIL_TIMEOUT_SEC）→ 切回物理 SIM
  · rx_packets 无增长超过 120s（ROAMLINK_NO_DATA_TIMEOUT_SEC）→ 切回物理 SIM
↓
切回物理 SIM（SIM 备用，dial_stat_net_connected）
  · 记录 sim_fallback_ts
↓
[SIM 备用期间持续计时]
  · SIM 稳定 300s（SIM_FALLBACK_RETRY_SEC）→ 自动尝试切回 Roamlink
  · 切回失败 → 重置计时，再等 300s 继续尝试（循环）
  · 切回成功 → 回到 dial_stat_roamlink_starting，重走上述流程
```

脚本策略 1 在 "切到 SIM" 这一步就结束了；modem_mng 在 SIM 备用期间仍持续尝试恢复首选通道，形成闭环。






  策略定义（两边数值一致，含义相同）

  ┌─────┬──────────────────────────────────┐
  │ 值  │               名称               │
  ├─────┼──────────────────────────────────┤
  │ 1   │ 优先 Roamlink（PREFER_ROAMLINK） │
  ├─────┼──────────────────────────────────┤
  │ 2   │ 优先 SIM（PREFER_SIM）           │
  ├─────┼──────────────────────────────────┤
  │ 3   │ 强制 Roamlink（FORCE_ROAMLINK）  │
  ├─────┼──────────────────────────────────┤
  │ 4   │ 强制 SIM（FORCE_SIM，默认值）    │
  └─────┴──────────────────────────────────┘

  ---
  脚本 check_network.sh 的逻辑

  主循环：先启动对应通道，无条件 sleep 600（10分钟），再调 check_celluar_network

  策略1（优先 Roamlink）
  - 启动 RBMaster 服务，等 10 分钟
  - 检测失败 → 切换到 SIM（check_celluar_network 1，重启 dial）
  - 检测成功 → 等 1800s 再下次检测

  策略2（优先 SIM）
  - 启动 dial，等 10 分钟
  - 检测失败 → 切换到 Roamlink（check_celluar_network 1，启动 RBMaster 服务）
  - 检测成功 → 等 1800s
  
  策略3（强制 Roamlink）
  - 启动 RBMaster 服务，等 10 分钟
  - 检测失败 → 重启同一服务，不切换（check_celluar_network 0）
  - 检测成功 → 等 1800s

  策略4（强制 SIM）
  - 启动 dial，等 10 分钟
  - 检测失败 → 重启 dial，不切换（check_celluar_network 0）
  - 检测成功 → 等 1800s
  
  ▎ 脚本没有"切回"逻辑。切过去就一直用那个，下次失败再切回来。

  ---
  modem_mng（dial.c）的逻辑

  策略1（优先 Roamlink）
  - 启动时直接走 Roamlink 通道（dial_stat_roamlink_starting）
  - Roamlink 5 分钟内无法 ping 通 → 切回 SIM（ROAMLINK_CONNECT_TIMEOUT_SEC=300s）
  - Roamlink ping 通后再次失联 5 分钟 → 切回 SIM（ROAMLINK_FAIL_TIMEOUT_SEC=300s）
  - SIM 备用稳定 300 秒后，自动尝试切回 Roamlink（SIM_FALLBACK_RETRY_SEC=300s，循环回切）

  策略2（优先 SIM）
  - 启动时走 SIM 通道
  - SIM 失败 3 分钟 → 切换到 Roamlink（ROAMLINK_SWITCH_TIMEOUT_SEC=180s）
  - Roamlink 备用稳定 300 秒后，自动尝试切回 SIM（ROAMLINK_FALLBACK_RETRY_SEC=300s，循环回切）

  策略3（强制 Roamlink）
  - 启动时直接走 Roamlink 通道
  - Roamlink 失败（connect timeout / fail timeout / 无数据流量 2 分钟）→ 重启 RBMaster 服务，回到 starting 状态重新等待，永不切 SIM

  策略4（强制 SIM）
  - 启动时走 SIM 通道，永不切 Roamlink
  - 断网后走 L1/L2/L3 分级恢复：
    - L1（60s）：软重拨（stop + start data call）
    - L2（5min）：CFUN 射频复位（AT+CFUN=0 → AT+CFUN=1）
    - L3（30min）：硬重启（AT+CFUN=1,1 + exit）

  ---
  最大的两处差异
  
  1. 回切机制：脚本没有；modem_mng 策略1/2 各自有 300s 稳定后自动回切的循环机制。
  2. 策略4 的恢复手段：脚本只是重启 dial 进程；modem_mng 有三级递进恢复，最终会触发模块硬重启。
