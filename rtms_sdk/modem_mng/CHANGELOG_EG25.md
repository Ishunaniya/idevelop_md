## EG25 拨号模块更新说明

### 版本：1.30

#### 1. 拨号流程对齐 open_dial_for_artery
- **统一拨号状态机**：`dial_task` 中的拨号状态机与 `open_dial_for_artery` 保持一致（SIM 初始化、注册检查、APN 选择、数据呼叫、连接维持等）。
- **Data Call 启动逻辑优化**：使用 `dail_start_data_call(dial_mng_t *p_dial_mng)`，仅在需要时调用 `QL_Data_Call_Init`，避免重复初始化，并启用 SDK 的自动重连（`reconnect = true`）。

#### 2. 日志与诊断增强
- **SD 卡日志接入**：
  - 复用 `logger_sd.c` / `logger_sd.h`，在 EG25 入口 `dialer_eg25.c` 中调用 `log_init()`。
  - 当 `/media/sdcard` 挂载且剩余空间 ≥ 500MB 时，将日志写入 `/media/sdcard/dial_log/`。
  - 通过 `dial_log(...)` 输出关键拨号和恢复流程日志。
- **心跳日志**：
  - 在 `dial_task` 中每 30 秒记录一次心跳信息：`SIM` 状态、网络注册状态 `REG`、信号强度 `CSQ`。
  - 日志格式示例：`[HEARTBEAT] SIM:1 | REG:1 | CSQ:25`。
- **运营商与射频操作日志**：
  - 在运营商选择阶段（`dial_stat_select_oper`）记录当前尝试/移除的运营商 PLMN。
  - 在射频重置阶段（`dial_stat_stop_cfun` / `dial_stat_start_cfun`）记录 `AT+CFUN=0/1` 的发送与结果。

#### 3. 网络与重置策略调整
- **移除外部 `ql_netcall` 依赖**：
  - 删除对 `ql_netcall -p 1 -r 5 -d` 的调用，不再依赖外部脚本进行拨号。
  - 所有拨号与重拨逻辑统一由本进程内部状态机完成。
- **禁用整机重启**：
  - `reset_modem()` 中去掉 `system("reboot -f")` 等整机重启操作，仅记录日志提示重启已被禁用。
  - 仍保留 `modem_need_reset` 标志，用于后续扩展更温和的恢复策略。

#### 4. 模块信息上报修正（IMSI / ICCID / IMEI / 运营商）
- **IMSI / ICCID**：
  - `getImsi()`：使用 `QL_MCM_SIM_GetIMSI` 获取并缓存 IMSI，仅返回 IMSI，不再篡改 IMEI/ICCID。
  - `getIccid()`：使用 `QL_MCM_SIM_GetICCID` 获取并缓存 ICCID，仅返回 ICCID。
- **IMEI**：
  - `getImei()`（EG25 分支）：通过 AT 命令 `AT+CGSN` 获取模块真实 IMEI，从应答中提取连续 15 位数字，并进行静态缓存。
- **运营商名称**：
  - `getOperatorName()`（EG25 分支）：通过 AT 命令 `AT+COPS?` 读取当前注册运营商，从返回的 `+COPS: 1,0,"XXXX",7` 中解析出双引号内的运营商名。
  - 增加 60 秒缓存机制，避免高频调用时反复下发 AT。

#### 5. 其他细节优化
- **AT 端口复用**：
  - `at_init()` 使用静态 `fd`，全进程只打开一次 `/dev/smd8`，避免多处重复 open/close 导致冲突。
  - 拨号线程与 `nanomsg` 请求处理共用同一个 AT 通道。
- **代码清理**：
  - 去除重复的 “已联网则仅监控” 代码片段，减少逻辑冗余。
  - 删除与地区宏 `RUSSIA` 相关的条件编译，简化 EG25 拨号路径。

