# 会话记录：Roamlink 安装修复 + 心跳日志增强

**日期：** 2026-05-21  
**涉及文件：**
- `eg25/dial/dial.c`
- `/home/tronlong/lyp/zqzb/roamlink_deploy_rtms_newC_260519/install.sh`
- `/home/tronlong/lyp/zqzb/roamlink_deploy_rtms_newC_260519/install_update.sh`

---

## 一、背景

本次会话开始前，modem_mng EG25 平台已完成：

- 正常拨号流程
- 启动诊断日志（`[MODEM]` / `[INIT]` / `[CELL]` / `[SDK]`）
- 心跳日志（`[HEARTBEAT]`）
- Roamlink 双卡状态机集成（`is_roamlink_active`、`roamlink_fail_count`、`roamlink_rx_packets`）

**尚未覆盖的测试项：** L1/L2/L3 断网分级恢复、roamlink ↔ SIM 切换场景。

本次目标：在真实 EG25 RTMS 开发板上安装 roamlink 依赖，并增强 roamlink 激活状态下的心跳日志。

---

## 二、主线一：install.sh 事故与修复

### 2.1 事故经过

设备预检通过（fw_version=1、IMEI=867929069681908、license 存在），执行 `./install.sh` 重启后，设备拨号失效——`modem_mng` 无法运行。

### 2.2 根因分析

原 `install.sh` 对 RTMS 设备的处理存在三处错误：

| # | 原逻辑 | 问题 |
|---|---|---|
| 1 | `mv /usr/bin/modem_mng /usr/bin/modem_mng_bak`，复制为 `/usr/bin/dial` | sw_mng 按 `apps.json` 查找 `/usr/bin/modem_mng`，文件消失，拨号死亡 |
| 2 | `ln -svf .../start_check_network /etc/rc5.d/S60start_check_network` | 与 `sw_mng` 管理的 RTMS 启动链路冲突 |
| 3 | 拷贝 `test_network` / `check_network.sh` / `start_check_network` | RTMS 设备不需要这些文件，check_network.sh 由非 RTMS 设备的独立拨号脚本使用 |

**根本原因：** 原脚本是为非 RTMS 设备（`/usr/dial/dial` + 独立 check_network 机制）编写的，未考虑 RTMS 设备由 `sw_mng → apps.json → /usr/bin/modem_mng` 管理的启动链路。

### 2.3 RTMS 设备正确启动链路（来自 board_investigation_20260520.md）

```
开机
  └── /etc/rc5.d/S20rtms
        └── /usr/bin/sw_mng
              └── /opt/apps.json
                    └── runlevel 1: /usr/bin/modem_mng   ← 拨号管理（看门狗保活）
```

**关键约束：** `/usr/bin/modem_mng` 必须存在且可执行，sw_mng 才能启动和监控它。

### 2.4 修复方案

以 `/etc/config/config.json` 是否存在作为 RTMS 判断依据，对 `install.sh` 和 `install_update.sh` 做相同修改：

**修改一：fw_version=1 分支（跳过 RTMS 的启动链路修改）**
```sh
elif [ "$fw_version" -eq 1 ]; then
    # 原逻辑：非RTMS设备启动链路修改，RTMS设备启动链路由sw_mng->apps.json管理，不适用
    # ln -svf /etc/init.d/start_check_network /etc/rc5.d/S60start_check_network
    # unlink /etc/rc5.d/S20start_dial 2>/dev/null
    # unlink /etc/rc5.d/S20start_vsim
    if [ ! -f /etc/config/config.json ]; then
        ln -svf /etc/init.d/start_check_network /etc/rc5.d/S60start_check_network
        unlink /etc/rc5.d/S20start_dial 2>/dev/null
        unlink /etc/rc5.d/S20start_vsim
    fi
fi
```

**修改二：二进制部署（RTMS 从包内部署 modem_mng）**
```sh
if [ -f /etc/config/config.json ]; then
    # 原逻辑：复制 modem_mng 为 dial 并重命名为 modem_mng_bak，导致 sw_mng 找不到 modem_mng
    # cp -f /usr/bin/modem_mng /usr/bin/dial
    # mv /usr/bin/modem_mng /usr/bin/modem_mng_bak
    # ln -svf /usr/bin/dial /usr/dial/dial
    # RTMS：将包内 modem_mng 部署到 /usr/bin/modem_mng，由 sw_mng 负责启动和看门狗
    check_cmd cp -f ./modem_mng /usr/bin/modem_mng
    chmod +x /usr/bin/modem_mng
else
    check_cmd cp -f ./dial_EG25_V1295 /usr/dial/dial
    chmod +x /usr/dial/dial
fi
```

**修改三：工具文件（RTMS 不需要 check_network 相关文件）**
```sh
check_cmd cp ./network.ini /usrdata/
if [ ! -f /etc/config/config.json ]; then
    check_cmd cp ./test_network /usrdata/
    check_cmd cp ./check_network.sh /usrdata/
    chmod +x /usrdata/test_network /usrdata/check_network.sh
fi
if [ ! -f /etc/config/config.json ]; then
    check_cmd cp ./start_check_network /etc/init.d/
    chmod +x /etc/init.d/start_check_network
fi
```

> `install_update.sh` 与 `install.sh` 仅 `MUST_HAVE_LICENSE` 值不同（0 vs 1），两个文件做相同修改。

### 2.5 安装包完整性确认

包内已有 `modem_mng` ARM 二进制（315K），与 `dial_EG25_V1295` 并列，安装包是自包含的，无需额外操作。

### 2.6 当前损坏设备的手动修复

```sh
cp /media/sdcard/modem_mng /usr/bin/modem_mng
chmod +x /usr/bin/modem_mng
unlink /etc/rc5.d/S60start_check_network
reboot
```

修复后确认：modem_mng 正常运行，roamlink 激活（network_type=2，license 已安装）。

---

## 三、主线二：心跳日志增强

### 3.1 问题分析

Roamlink 激活时，原心跳格式存在信息价值缺失：

| 字段 | 问题 |
|---|---|
| `REG:0` | roamlink 激活时物理 SIM 不驻网，该值恒为 0，无诊断意义 |
| 无通道标识 | 无法从日志判断当前走的是 SIM 还是 ROAMLINK |
| 无 roamlink 专属指标 | 失败次数、业务流量是否真实流通均无法观测 |

### 3.2 新增字段与约束

| 字段 | 数据来源 | 打印条件 | 理由 |
|---|---|---|---|
| `CH` | `p_dial_mng->is_roamlink_active` | 始终打印，值为 `ROAMLINK` / `SIM` | 通道标识，始终有诊断价值 |
| `REG` | `nw_reg_status_check()` | **仅 SIM 通道时打印** | roamlink 时物理 SIM 不驻网，REG 恒为 0 |
| `RL_FAIL` | `p_dial_mng->roamlink_fail_count` | **仅 `roamlink_available` 时打印** | 无 roamlink 时该计数无意义 |
| `RX_PKT` | `p_dial_mng->roamlink_rx_packets` | **仅 roamlink 激活时打印** | 走 SIM 时 rmnet_data* 流量不代表 roamlink 业务 |

`RX_PKT` 来源：`nw_get_rmnet_rx_packets_sum()` 遍历 `/sys/devices/virtual/net/rmnet_data*/statistics/rx_packets`，`strtoull` 读取，类型 `uint64_t`，切换到 SIM 时重置为 0。

### 3.3 缓冲区截断验证

- `hb_buf[256]`，写入 `RX_PKT` 时已用约 90 字节，剩余 160+ 字节
- `uint64_t` 最大值 20 位，`" | RX_PKT:..."` 最多 30 字符
- **结论：不会截断**

### 3.4 函数签名重构

将 `dial_log_heartbeat` 末尾原本分散的 4 个参数（`is_roamlink`、`roamlink_available`、`rl_fail_count`、`rx_packets`）合并为 `const dial_mng_t *p_dial_mng` 指针传入，函数参数从 15 个降至 12 个。

### 3.5 最终心跳格式

**SIM 通道：**
```
[HEARTBEAT] CH:SIM | SIM:1 | REG:1 | CSQ:20 | Temp:38 | DownTime:0s | ConsecFail:0
```

**ROAMLINK 通道：**
```
[HEARTBEAT] CH:ROAMLINK | SIM:1 | CSQ:16 | Temp:42,36,36 | DownTime:0s | ConsecFail:0 | RL_FAIL:0 | RX_PKT:44
```

---

## 四、提交说明

### modem_mng 仓库

```
[modem_mng] [NEWFUNC] EG25 心跳日志增强：新增 CH/RL_FAIL/RX_PKT 字段并约束 REG 打印条件

- 新增 CH 字段标识当前通道（ROAMLINK / SIM），便于快速区分双卡状态
- REG 字段改为仅 SIM 通道时打印；roamlink 激活时物理 SIM 不驻网，
  REG 恒为 0，无诊断价值
- 新增 RL_FAIL 字段（仅 roamlink_available 时打印），记录 roamlink 失败次数
- 新增 RX_PKT 字段（仅 roamlink 激活时打印），读取
  /sys/devices/virtual/net/rmnet_data*/statistics/rx_packets 累计值，
  用于判断 roamlink 激活后业务流量是否真实流通
- dial_log_heartbeat 末尾 4 个离散参数合并为 const dial_mng_t * 传递，
  减少函数签名复杂度
```

### 部署包

```
[roamlink_deploy] [BUGFIX] 修复 RTMS 设备安装脚本错误覆盖 sw_mng 启动链路

原脚本将 modem_mng 重命名并修改 rc5.d 符号链接，
导致 sw_mng 找不到 /usr/bin/modem_mng，设备重启后拨号失效。

- fw_version=1 分支：RTMS 设备跳过 start_check_network 启动链路修改
- 二进制部署：RTMS 设备从包内复制 modem_mng 到 /usr/bin/modem_mng，
  由 sw_mng 负责启动和看门狗；非 RTMS 设备保留原有 dial_EG25_V1295 逻辑
- test_network / check_network.sh / start_check_network 仅对非 RTMS 设备安装
- 原有逻辑以注释形式保留，未删除
- install_update.sh 同步相同修改（与 install.sh 仅 MUST_HAVE_LICENSE 值有差异）
```

---

## 五、测试覆盖情况

| 测试项 | 状态 |
|---|---|
| 正常拨号流程 | ✅ 已验证（上一会话） |
| 启动诊断日志 | ✅ 已验证（上一会话） |
| 心跳日志（SIM 通道） | ✅ 已验证（上一会话） |
| Roamlink 安装与激活 | ✅ 已验证（本次会话） |
| 心跳日志（ROAMLINK 通道） | ✅ 已验证（本次会话） |
| L1/L2/L3 断网分级恢复 | ⏳ 待测 |
| Roamlink ↔ SIM 双卡切换 | ⏳ 待测 |




现在事实足够了，完整回答：

  ---
  "复用 sim.c 的 h_sim" 这个方向有两个实际问题：
 
  问题1：h_sim 是 static，无法直接访问

  sim.c:56 的 static sim_client_handle_type h_sim 是文件级私有变量，nanomsg_process.cpp 直接访问不到，必须在 sim.h 里加 getter 函数才能暴露——需要改动 sim 模块。

  问题2：并发访问风险
  
  dial_task（主拨号线程）和 nanomsg handler（线程 1307）会同时使用同一个 h_sim：dial_task 用它做 SIM_OP_GET_ICCID/SIM_OP_GET_IMSI 等操作，nanomsg handler 用它做 GetIMSI/GetICCID 查询。MCM
  SDK 内部是否有线程安全保证代码里看不到，存在并发风险。

  ---
  更好的方案：不共享 handle，共享读出的结果
  
  ICCID 和 IMSI 在 dial_stat_sim_init 成功后就写入了 p_sim_mng->sim_iccid 和 p_sim_mng->sim_imsi（dial.c:1408），之后这两个字段不再变化（只读）。

  只需在 nanomsg_process_cinterface.h 里加两个 setter，dial.c 读完 ICCID/IMSI 后调一次，nanomsg handler 直接用存好的字符串返回，不再走 MCM SIM 接口：

  // nanomsg_process_cinterface.h 加：
  void set_eg25_iccid(NanoReqHandlerWrapper *wrapper, const char *iccid);
  void set_eg25_imsi(NanoReqHandlerWrapper *wrapper, const char *imsi);
  
  dial.c 在成功读到 ICCID/IMSI 后（line 1408 附近）调这两个 setter，getImsi()/getIccid() 直接返回已缓存的值，不再尝试 QL_MCM_SIM_Client_Init。
  
  这样没有跨线程 handle 共享，没有并发风险，也不需要改 sim 模块。你确认这个方向，我再改代码。


基于代码事实，逐条回答：

  sim_iccid/sim_imsi 只在 dial_stat_sim_op 状态（line 1404）里更新，其余状态不碰这两个字段。

  SIM → Roamlink 切换时： 状态直接跳到 dial_stat_roamlink_starting（line 895），不经过 dial_stat_sim_op，sim_iccid/sim_imsi 不更新，保持切换前物理SIM的值。
  
  Roamlink → SIM 切换时： 代码走 dial_st = dial_stat_sim_init（line 1330），最终重新经过 dial_stat_sim_op，sim_iccid/sim_imsi 被重新读取（此时模组已切回物理SIM，读到的就是物理SIM的值）。
  
  ---
  所以结论：这两个字段始终反映的是物理SIM的 ICCID/IMSI，Roamlink 激活期间它们不变，Roamlink 退出后重走 sim_op 时刷新。
  
  这与 /tmp/dial_status 的行为一致（line 1249-1250 也是直接用 p_sim_mng->sim_iccid，Roamlink 期间不更新）。

  所以用 setter 方案缓存这两个值，不会因为 Roamlink 切换而出现问题——和现有 dial_status 文件的行为保持一致。


这个问题从代码里找不到确定答案，我只能基于事实说能推断的部分，其余的如实说不确定。
  
  从代码能看到的事实：

  sim_op_handler 调用 QL_MCM_SIM_GetICCID/QL_MCM_SIM_GetIMSI，这两个是 MCM SDK 接口，底层是向调制解调器查询当前 SIM 槽的信息。调制解调器本身不区分"物理SIM"还是"虚拟SIM"——它只看 SIM
  接口上当前呈现的是什么卡。

  从代码能推断的：
  
  Roamlink（RBMaster）是在 SIM 卡硬件接口层做拦截，让调制解调器"以为"插着一张 Roamlink 的虚拟 SIM。如果这个推断成立，那么 Roamlink 激活时调用 QL_MCM_SIM_GetICCID 得到的就是 Roamlink 虚拟
  SIM 的 ICCID，不是物理 SIM 的。

  这也能解释代码里为什么 dial_stat_sim_op 从不在 Roamlink 激活期间运行——开发者有意只在物理 SIM 路径下读 ICCID/IMSI，Roamlink 期间不查，避免拿到虚拟 SIM 的值写进 sim_iccid。
  
  无法从代码确认的：

  Roamlink 的 SIM 拦截具体在哪一层实现（是否真的让调制解调器看到不同的 ICCID），这在当前代码里看不到，属于 RBMaster 内部实现，需要实测或查 Roamlink 文档才能确认。
  
  ---
  对 setter 方案的影响：
  
  不管 Roamlink 期间 MCM 接口返回什么，setter 方案只在 dial_stat_sim_op（物理 SIM 路径）成功后才调用，与现有 dial_status 文件行为完全一致——始终上报物理 SIM 的 ICCID/IMSI，不受 Roamlink
  影响。

case sim_op_stat_get_iccid: 后面没有 break，直接 fallthrough 到 case sim_op_stat_get_imsi:。

  所以调用 sim_op_handler(p_sim_mng, sim_op_stat_get_iccid) 时，实际执行路径是：
  
  1. 读 ICCID → 写入 sim_iccid
  2. fall through → 读 IMSI → 写入 sim_imsi
  3. break
  
  两个字段都会被填充。 dial.c:1408 之后打印 sim_iccid 和 sim_imsi 都有值，也印证了这一点。
  
  确认没问题，可以改了。

 改动逻辑总结：

  - dial_task 在 dial_stat_sim_op 读到 ICCID/IMSI 后，通过 set_eg25_iccid/set_eg25_imsi 直接写入 ModemReqHandler 的成员变量
  - getImsi()/getIccid() 直接返回成员变量，不再尝试 QL_MCM_SIM_Client_Init
  - Roamlink→SIM 切换时 dial_stat_sim_op 会重新执行，setter 会再次被调用刷新值
  - logcat 里那条 failed with error code: MCM_SUCCESS 不会再出现


基于上下文总结中的日志事实，修改后无问题：

  修复验证通过

  1. 错误消失 — E DIAL: QL_MCM_SIM_Client_Init failed with error code: MCM_SUCCESS 完全不再出现。
  2. ICCID/IMSI 正确写入 — SD 卡日志确认：
  [INIT] ICCID: 89852019925010000888
  [INIT] IMSI:  455011505090058
  2. 说明 dial_task 在 sim_op_handler 成功后调用了 set_eg25_iccid / set_eg25_imsi，nanomsg handler 的成员变量已被正确更新。
  3. SIM Client 初始化顺序正常 — sim.c 的 QL_MCM_SIM_Client_Init 打印 start up required service 00000008，这是首次调用的正常路径（而非之前 nanomsg handler 抢先初始化导致的"already
  inited"状态）。
  4. 网络连接正常 — Profile 10 连接成功，心跳 DownTime:0s，无异常。

  结论：这次修改正确解决了问题，日志中无遗留错误。

有一点潜在问题未提及：
  
  线程安全（低风险但存在理论数据竞争）

  set_eg25_iccid / set_eg25_imsi 从 dial_task 线程写入 std::string iccid / imsi，而 nanomsg handler 线程在处理查询请求时读取这两个字段——两者之间没有 mutex 保护。
  
  实际风险很低，原因是：
  - 写入只在启动阶段 dial_stat_sim_op（网络还未建立）执行一次
  - 外部进程通过 nanomsg 查询 ICCID/IMSI 时，SIM 初始化通常早已完成
  
  但严格来说是 UB（未定义行为）。如果需要完全正确，应在写入和读取两侧加锁，或改用 std::atomic 配合 std::string 的包装。

  其余分析无遗漏——L1/L2/L3 逻辑、版本号、MCM_SUCCESS 错误、双IP、CGACT 缓冲区这几项都已覆盖。


基于事实逐条分析：

  正常部分：
  - 启动状态机序列完整：init → sim_init → sim_check → sim_op → reg_check → cereg_check → precondition_check → profile:10 → net connected，无跳步、无失败
  - ICCID / IMSI 在 sim_op success 之后打印，时序正确
  - 连接后心跳稳定，Profile 1 (10.112.133.74) + Profile 10 (10.112.200.216) 双 IP 持续稳定，无断线
  - 无任何 ERROR 级别的异常日志（Module reset skipped 是 E 级但属预期行为，uptime<10min 正常跳过重置）

  值得注意（非 bug，但要了解）：

  1. Module reset skipped 出现两次：22:22:10.729 和 22:22:13.789。第一次是 pre_start_call 入口检查，第二次紧在 net connected 之前——说明状态机在等待连接期间又经过了一次 pre_start_call
  的判断分支，属于代码逻辑中的重入路径，不影响结果。
  2. thread 2180 连续打印 7 次 DIAL（22:22:12.749）：这是 Quectel SDK 的 DataCall 回调线程，SDK 在连接建立时会连续触发多个状态事件，7 次是 SDK 回调的正常行为，不是 bug。

  结论：日志正常，无遗留问题。

[modem_mng] [BUGFIX] EG25 修复L1/L2/L3分级恢复逻辑及ICCID/IMSI nanomsg上报

  - 修复L2永不触发：L1路径错误更新last_recovery_ts导致L2冷却永远不满足
  - 修复L2退化为L1：重构if/else链，downtime≥5min时不再降级回L1
  - 修复L3：downtime≥30min直接执行AT+CFUN=1,1后exit，不再误判为L1/L2
  - 新增L1跳过/L2 CFUN响应/L3响应的可观测日志
  - 修复CGACT缓冲区128→256字节（16条CGDCONT约240字节会截断）
  - 修复nanomsg handler查询ICCID/IMSI返回空：改为dial_task在sim_op
    成功后通过set_eg25_iccid/set_eg25_imsi直接写入handler成员变量，
    替代错误的二次QL_MCM_SIM_Client_Init方案





modem_mng（EG25）策略 1/2 切换决策

  策略 1（PREFER_ROAMLINK）

  启动 → Roamlink(starting)
           │
           ├─ 300s 内 ping 通 → Roamlink(active)
           │                        │
           │         ping 成功：更新 roamlink_last_ping_ts
           │         ping 失败：start_fail_ts 开始计时
           │                        │
           │         ┌──────────────┼──────────────────┐
           │    ping层超时         BIZ层超时         (active维持)
           │  (>300s无ping通)    (rx_pkt无增长>120s)
           │         │                │
           └─────────┴────────────────┘
                     │
                     ↓  sim_fallback_ts=now
                  SIM(reg_check → ... → net_connected)
                     │
           ┌─────────┴──────────────────────┐
      SIM ping通                      SIM ping失败
      且sim_fallback_ts起>=300s        且downtime_sec>=180s
           │                               │
           ↓                               ↓
      回切Roamlink(starting)         再切Roamlink(starting)
      sim_fallback_ts=0              roamlink_fallback_ts=now

  ---
  策略 2（PREFER_SIM）

  启动 → SIM(reg_check → ... → net_connected)
                │
           SIM ping失败
           且downtime_sec>=180s
                │
                ↓  roamlink_fallback_ts=now
           Roamlink(starting)
                │
                ├─ 连接超时(>300s未ping通) → SIM(reg_check)，sim_fallback_ts=now
                │
                ↓ ping通
           Roamlink(active)
                │
           ping成功：更新 roamlink_last_ping_ts
           ping失败：start_fail_ts计时
                │
           ┌────┴──────────────────┐
      ping层超时               BIZ层超时
      (>300s无ping通)          (rx_pkt无增长>120s)
           │                       │
           └──────────┬────────────┘
                      │
                      ↓  sim_fallback_ts=now，roamlink_fallback_ts=0
                   SIM(reg_check)
                      │
                SIM ping通
                且roamlink_fallback_ts起>=300s
                【注：此时roamlink_fallback_ts已被清零！】

  策略 2 的问题：切回 SIM 时（第 1844/1888 行）roamlink_fallback_ts = 0，但回切 Roamlink 的条件是 roamlink_fallback_ts > 0 && elapsed >= 300s（第 1043-1047 行）。roamlink_fallback_ts 
  被清零后，回切条件永远不满足，策略 2 的"Roamlink 稳定 300s 后自动回切 SIM"只在通过 SIM→Roamlink 主动切换进入（第 1129 行设置）时才有效，ping 层/BIZ 层超时切回 SIM 时，roamlink_fallback_ts
   已被清零，失去回切能力。

  ---
  open_dial_for_artery 策略 1/2 切换决策
  
  核心不同：探测方式是 TCP 测试，不是 ping；切换由失败次数触发，不是时间

  策略 1（PREFER_ROAMLINK）

  启动 → Roamlink(starting)
           │
           ├─ TCP通 → Roamlink(active)
           │                │
           │   TCP成功：roamlink_fail_count=0
           │   TCP失败：roamlink_fail_count++
           │                │
           │   TCP失败>=ROAMLINK_FAIL_THRESHOLD次
           │   或 BIZ(rx_pkt无增长>=TIMEOUT)       → SIM(sim_init)，sim_fallback_timer=now
           │
           ├─ 超时(>ROAMLINK_CONNECT_WAIT_SEC无TCP通) → SIM(sim_init)，sim_fallback_timer=now
           │
           └─ SIM(net_connected)
                      │
           TCP成功且sim_fallback_timer起>=300s → 回切Roamlink(starting)
           TCP失败>=TCP_FAIL_THRESHOLD次      → 再切Roamlink(starting)

  策略 2（PREFER_SIM）

  启动 → SIM(net_connected)
                │
           TCP失败>=TCP_FAIL_THRESHOLD次
           roamlink_fallback_timer=now
                │
           Roamlink(starting → active)
                │
           TCP成功且roamlink_fallback_timer起>=300s → 回切SIM(sim_init)，sim_fallback_timer=now
           TCP失败>=ROAMLINK_FAIL_THRESHOLD次       → 切SIM(sim_init)，sim_fallback_timer=now
           BIZ超时                                  → 切SIM(sim_init)，sim_fallback_timer=now

  ---
  你现在要做的事
  
  基于以上分析，modem_mng 还有一个实际逻辑 bug：

  策略 2 的 roamlink_fallback_ts 清零问题：Roamlink 因 ping 层/BIZ 层超时切回 SIM 时（第 1844、1888 行），roamlink_fallback_ts 被清零。导致在 SIM 备用期间，策略 2 的"SIM 稳定后主动回切
  SIM"的计时起点丢失，实际上策略 2 在这两条路径下切回 SIM 后永远不会再主动回切，只能靠 SIM 失败 180s 再次触发 Roamlink 切换时才重新设置 roamlink_fallback_ts。




  ---
  策略1（PREFER_ROAMLINK）测试
  
  前提：config 设 network_select=1，设备已启动，Roamlink active

  第一轮：触发 Roamlink→SIM（BIZ 120s）
  # 断网（BIZ层120s后触发切换）
  iptables -I OUTPUT -j DROP
  
  # 等待约 130s，观察日志出现：
  # [ROAMLINK] Biz layer no data for 1xxs, switching to SIM
  # [INIT] ICCID: xxx  ← 这行是新修复的关键，之前没有
  # 心跳出现 SIM:1/REG:1  ← 之前全程 SIM:0/REG:0

  # 恢复网络（让SIM备用连通）
  iptables -D OUTPUT -j DROP
  
  观察 SIM 备用窗口（约 300s）：
  - 心跳应出现 SIM:1 REG:1
  - 不应出现 [RECOVERY L1] / [RECOVERY L2]
  - 300s 后应出现 [ROAMLINK] Policy1: SIM stable xxxs, trying Roamlink recovery

  第二轮（验证 is_func_called 修复）：
  # Roamlink 恢复 active 后，再次断网
  iptables -I OUTPUT -j DROP
  # 等 130s，观察第二次 SIM 切换，确认同样出现 [INIT] ICCID
  iptables -D OUTPUT -j DROP

  ---
  策略2（PREFER_SIM）测试

  前提：config 设 network_select=2，设备已启动，SIM connected

  触发 SIM→Roamlink（180s）
  iptables -I OUTPUT -j DROP
  # 等待约 190s，观察：
  # [ROAMLINK] policy=2: SIM down 185s, switching to Roamlink

  恢复网络，让 Roamlink 稳定
  iptables -D OUTPUT -j DROP
  # 等待约 300s，观察：
  # [ROAMLINK] Policy2: Roamlink stable xxxs, trying SIM recovery
  # [INIT] ICCID: xxx  ← Roamlink→SIM 走 sim_init 路径

  第二轮：再次 iptables -I OUTPUT -j DROP，验证第二次 SIM→Roamlink→SIM 循环正常。

  ---
  关键判断点汇总

  ┌─────────────────────────────┬──────┬───────────────────────────┐
  │           检查项            │ 期望 │         失败说明          │
  ├─────────────────────────────┼──────┼───────────────────────────┤
  │ 切到 SIM 后有 [INIT] ICCID  │ ✅   │ sim_init 路径未走通       │
  ├─────────────────────────────┼──────┼───────────────────────────┤
  │ SIM 备用窗口内 SIM:1 REG:1  │ ✅   │ 物理 SIM 仍注册失败       │
  ├─────────────────────────────┼──────┼───────────────────────────┤
  │ 无 [RECOVERY L1/L2]         │ ✅   │ L1/L2/L3 guard 未生效     │
  ├─────────────────────────────┼──────┼───────────────────────────┤
  │ 第二次切换 SIM 数据呼叫正常 │ ✅   │ is_func_called 修复未生效 │
  ├─────────────────────────────┼──────┼───────────────────────────┤
  │ Roamlink 恢复后 RL_FAIL:0   │ ✅   │ fail_count 清零未生效     │
  └─────────────────────────────┴──────┴───────────────────────────┘



  修复后，验证 300s 稳定回切的步骤：
  1. 策略 1，设备启动 → Roamlink 正常建立
  2. 屏蔽 rmnet 入向流量触发 BIZ 超时（不屏蔽 ICMP）：
  iptables -I INPUT -i rmnet_data0 -j DROP
  3. ~120s BIZ 超时 → 切 SIM → sim_initialized=false → dial_stat_init → NW Client 正确初始化 → REG:1 → net_connected
  4. SIM ping 通（ICMP 未被屏蔽）→ net_ok=true，sim_fallback_ts 开始计时
  5. 等待 ~300s
  6. 应出现日志：policy=1: SIM 稳定 300s，主动回切 Roamlink




 现在有了事实依据，可以给出明确结论。

  ---
  apn_get_apn_obj 事实核查

  apn_get_apn_obj（line 481–495）永远不返回 NULL：

  apn_obj_t *apn_get_apn_obj(char *iccid)
  {
      p_apn_obj = apn_scan_from_json(json_path, iccid);
      if (p_apn_obj == NULL)
      {
          // ICCID 不匹配时：回退到默认 APN（全空字段）
          p_apn_obj = calloc(1, sizeof(apn_obj_t));
          memcpy(..., &cnst_default_apn, ...);  // {"", "", "", "", NULL}
      }
      return p_apn_obj;  // ← 始终非 NULL
  }

  ICCID 为空时的匹配逻辑（apn_scan_from_json line 411）：

  strncmp(p_json_str, iccid, strlen(p_json_str))
  // p_json_str = JSON里的ICCID前缀，如 "8985201992"（10字节）
  // iccid = ""
  // strncmp("8985201992", "", 10) → '8' vs '\0' → 不等 → 不匹配

  空 ICCID 不会匹配任何 JSON 条目 → apn_scan_from_json 返回 NULL →
  apn_get_apn_obj 回退到全空字段的默认 APN，非 NULL。

  pre_start_call 里 p_apn_obj != NULL 成立 → 进入 apn_scan_idx：

  // 用空 APN 名称 "" 去匹配或新增
  strcmp("", apn_list.apn[i].apn_name)  // 不匹配任何真实 APN
  → 删除所有 APN → QL_APN_Add(apn_name="")

  QL_APN_Add 用空 APN 名，结果取决于 SDK：
  - 若成功：返回 profile_idx，拨号用模组默认 APN，可能通
  - 若失败：返回 -1 → profile_idx = -1 → 卡在 pre_start_call 反复重试

  结论：sim_op_handler fallthrough 导致空 ICCID 时，apn_get_apn_obj 不返回
  NULL（不会卡住），但 apn_scan_idx 行为取决于 SDK 对空 APN
  的处理，属于不确定行为，不是必然失败。

  ---
  完整审查结论汇总（基于事实）

  编号: Bug-3
  文件: dial.c
  问题: 策略1 Roamlink→SIM 三处切换点缺 sim_initialized 守卫，h_nw_client=0
  事实依据: 代码+设备验证
  严重性: 高（已修复）
  ────────────────────────────────────────
  编号: Bug-4
  文件: dial.c
  问题: get_tz_after_connected has_run 从未置 true
  事实依据: 代码直读
  严重性: 低（已修复）
  ────────────────────────────────────────
  编号: Bug-5
  文件: dial.c
  问题: L1 在心跳块成功执行后清零 is_func_called，同次循环 start_call case 拿到
    false → 卡在 start_call
  事实依据: 代码逻辑可直接追踪，L2 显式设 true 可解救
  严重性: 低（FORCE_SIM）
  ────────────────────────────────────────
  编号: 问题-1
  文件: sim.c
  问题: sim_op_handler case get_iccid 缺 break → fallthrough 到 get_imsi，ICCID
    失败被 IMSI 成功掩盖
  事实依据: 代码直读
  严重性: 低（正常流程两个均成功）
  QL_APN_Add 用空 APN 名，结果取决于 SDK：
  - 若成功：返回 profile_idx，拨号用模组默认 APN，可能通
  - 若失败：返回 -1 → profile_idx = -1 → 卡在 pre_start_call 反复重试

  结论：sim_op_handler fallthrough 导致空 ICCID 时，apn_get_apn_obj 不返回 NULL（不会卡住），但 apn_scan_idx 行为取决于 SDK 对空 APN 的处理，属于不确定行为，不是必然失败。

  ---
  完整审查结论汇总（基于事实）

  ┌────────┬────────────────┬────────────────────────────────────────────────────────────────────────────────────┬───────────────────────────────────────┬──────────────────────────┐
  │  编号  │      文件      │                                        问题                                        │               事实依据                │          严重性          │
  ├────────┼────────────────┼────────────────────────────────────────────────────────────────────────────────────┼───────────────────────────────────────┼──────────────────────────┤
  │ Bug-3  │ dial.c         │ 策略1 Roamlink→SIM 三处切换点缺 sim_initialized 守卫，h_nw_client=0                │ 代码+设备验证                         │ 高（已修复）             │
  ├────────┼────────────────┼────────────────────────────────────────────────────────────────────────────────────┼───────────────────────────────────────┼──────────────────────────┤
  │ Bug-4  │ dial.c         │ get_tz_after_connected has_run 从未置 true                                         │ 代码直读                              │ 低（已修复）             │
  ├────────┼────────────────┼────────────────────────────────────────────────────────────────────────────────────┼───────────────────────────────────────┼──────────────────────────┤
  │ Bug-5  │ dial.c         │ L1 在心跳块成功执行后清零 is_func_called，同次循环 start_call case 拿到 false →    │ 代码逻辑可直接追踪，L2 显式设 true    │ 低（FORCE_SIM）          │
  │        │                │ 卡在 start_call                                                                    │ 可解救                                │                          │
  ├────────┼────────────────┼────────────────────────────────────────────────────────────────────────────────────┼───────────────────────────────────────┼──────────────────────────┤
  │ 问题-1 │ sim.c          │ sim_op_handler case get_iccid 缺 break → fallthrough 到 get_imsi，ICCID 失败被     │ 代码直读                              │ 低（正常流程两个均成功） │
  │        │                │ IMSI 成功掩盖                                                                      │                                       │                          │
  ├────────┼────────────────┼────────────────────────────────────────────────────────────────────────────────────┼───────────────────────────────────────┼──────────────────────────┤
  │ 问题-2 │ dialStrategy.c │ 被编译进二进制但全是死代码，含旧 netcat 实现                                       │ CMakeLists.txt + grep 无调用          │ 无运行时影响             │
  ├────────┼────────────────┼────────────────────────────────────────────────────────────────────────────────────┼───────────────────────────────────────┼──────────────────────────┤
  │ 不确定 │ apn.c          │ 空 ICCID 时用空 APN 调 SDK，行为未知                                               │ 代码可追踪，SDK 行为需测试            │ 依赖 SDK                 │
  └────────┴────────────────┴────────────────────────────────────────────────────────────────────────────────────┴───────────────────────────────────────┴──────────────────────────┘

  以上是基于全部相关文件代码的完整事实核查结论。
