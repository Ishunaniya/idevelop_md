# EG912Y / EC200x / EC600S AT 手册：中文整理与工程速查

> 基于 `EG912-at.pdf`（V1.1，2020-07-07）提炼。这是中文工程手册，不是英文 PDF 转储：原手册的全部命令族、QCFG 子项和附录主题均已索引；精确参数范围、完整错误码表和复杂表格以 PDF 为准。

## 先记住这五点

1. PDF 是 **EG912Y、EC200x、EC600S 通用手册**；能力必须由 `ATI`、`AT+CGMR` 和实机响应确认。
2. 本项目的 `AT+QNETDEVCTL` / `AT+QNETDEVSTATUS` 不在该 PDF 内；其语法和状态只能按目标固件专用手册及实机探测处理。
3. `AT+COPS=3,2` 仅将后续 `AT+COPS?` 显示为数字 PLMN，例如 `+COPS: 0,2,"46001",7`；它不发起选网。
4. `OK` 表示命令已接受，不代表已联网；PDP 后还要验证网卡、DHCP、IPv4、默认路由和接口绑定的公网探测。
5. IMEI、IMSI、ICCID、APN、短信正文均需脱敏，不能写入生产日志。

## 推荐拨号诊断链路

```text
AT → ATE0 → AT+CMEE=2 → ATI / AT+CGMR
→ AT+CPIN? → AT+QCCID / AT+CIMI
→ AT+CEREG? / AT+CREG? → AT+CSQ → AT+COPS? / AT+QNWINFO
→ AT+CGDCONT? → AT+CGATT? → AT+CGACT? → AT+CGPADDR
```

- `AT+CPIN?` 必须为 `READY`。
- LTE/EPS 注册通常只接受 `CEREG` 状态 1（本地）或 5（漫游）。2 是搜索中，3 是拒绝。
- 再核对 CID、PDP 类型和 APN，随后确认附着、激活和模组侧 PDP 地址。
- 任何 `ERROR`、`+CME ERROR:`、`+CMS ERROR:`、超时或端口 EOF 都要保留命令标签、注册状态和脱敏上下文。

## 通用约定

| 项目 | 整理结论 |
|---|---|
| 命令形态 | 测试 `AT+X=?`、读取 `AT+X?`、写入 `AT+X=<...>`、执行 `AT+X`；不是每条都支持四种形态。 |
| AT 端口 | 主 UART、USB MODEM、USB AT 可通信；URC 会与同步响应交错，必须统一串行化。 |
| 字符集 | 默认 GSM；`AT+CSCS` 可选 GSM/UCS2/IRA，影响短信和电话簿。 |
| 关机 | `AT+QPOWD` 后不再发命令，等 `POWERED DOWN` 和 STATUS 拉低；手册建议 STATUS 拉低后至少等 3 秒断 VBAT。 |
| 初始化 | 建议 `ATE0` 关闭回显、`AT+CMEE=2` 启用详细错误文本。 |

## 全量命令索引与提炼

### 1. 通用命令（25）

| 命令 | 中文用途与注意事项 |
|---|---|
| `ATI` | 产品信息；用来识别模组。 |
| `AT+GMI` / `AT+CGMI` | 厂商标识。 |
| `AT+GMM` / `AT+CGMM` | 型号标识。 |
| `AT+GMR` / `AT+CGMR` | 固件版本；能力差异的首要依据。 |
| `AT+GSN` / `AT+CGSN` | IMEI/序列标识；脱敏。 |
| `AT&F` | 恢复厂家默认值；线上慎用。 |
| `AT&V` | 显示当前配置；诊断单字母参数。 |
| `AT&W` / `ATZ` | 保存/恢复用户配置；控制 NVM 写入频率。 |
| `ATQ` / `ATV` | 结果码开关/格式；自动化建议保留文本结果码。 |
| `ATE` / `A/` | 回显开关/重发上一命令；初始化用 `ATE0`。 |
| `ATS3` / `ATS4` / `ATS5` | 终止符、格式符、编辑符；不要在业务流程中改。 |
| `ATX` | CONNECT 结果码策略；数据呼叫兼容项。 |
| `AT+CFUN` | 功能/射频级别；变更会影响网络和 SIM。 |
| `AT+CMEE` | 错误码格式；推荐值 2。 |
| `AT+CSCS` | TE 字符集。 |
| `AT+QURCCFG` | URC 输出策略；必须与 URC 解析器一起验证。 |

### 2. 串口与状态控制（8）

| 命令 | 中文用途与注意事项 |
|---|---|
| `AT&C` / `AT&D` | DCD/DTR 行为；误设会造成串口状态变化时掉线。 |
| `AT+IFC` | 硬件流控；两端 RTS/CTS 必须同时配置。 |
| `AT+IPR` | 固定串口速率；模组改速率后主机必须同步。 |
| `AT+CPAS` | 移动台活动状态；不等于网络注册。 |
| `AT+CEER` | 最近扩展错误原因；失败后尽快读取。 |
| `AT+QCFG` | Quectel 扩展配置；写入可能持久化。 |
| `AT+QINDCFG` | 配置特定 URC 指示。 |

`AT+QCFG` 全部 9 个子项：`"gprsattach"`（附着模式）、`"nwscanmode"`（搜索制式）、`"nwscanseq"`（搜索顺序）、`"roamservice"`（漫游）、`"servicedomain"`（服务域）、`"band"`（频段）、`"urc/ri/other"`（RI）、`"urcdelay"`（URC 延迟）、`"urc/cache"`（URC 缓存）。频段、漫游、服务域和 URC 缓存的配置均须量产策略评审。

### 3. (U)SIM（11）

| 命令 | 中文用途与注意事项 |
|---|---|
| `AT+CIMI` | IMSI；SIM 初始化后查询，脱敏。 |
| `AT+CLCK` / `AT+CPWD` | 设施锁/改密码；高风险，不放自动恢复。 |
| `AT+CPIN` | PIN 查询/输入；拨号前必须 `READY`。 |
| `AT+CSIM` / `AT+CRSM` | APDU/受限文件访问；仅由了解 SIM 协议的模块使用。 |
| `AT+QCCID` | ICCID；资产关联可用，日志脱敏。 |
| `AT+QPINC` | PIN 剩余次数；避免锁卡。 |
| `AT+QINISTAT` | SIM 初始化状态。 |
| `AT+QSIMDET` / `AT+QSIMSTAT` | 插卡检测/状态 URC；必须触发业务重连。 |

### 4. 网络服务（9）

| 命令 | 中文用途与注意事项 |
|---|---|
| `AT+COPS` | 查询/选择运营商；数字 PLMN 用 `COPS=3,2` 后再读。 |
| `AT+CREG` | 电路域注册。 |
| `AT+CSQ` | 信号质量；`99` 为未知，强信号不保证可拨号。 |
| `AT+CPOL` / `AT+COPN` | 首选运营商/名称表；不在普通循环改写。 |
| `AT+CTZU` / `AT+CTZR` | NITZ 自动时区/时区 URC。 |
| `AT+QLTS` | 网络同步时间；统一时区和 RTC 策略。 |
| `AT+QNWINFO` | 当前制式、运营商、频段的关键诊断信息。 |

### 5. 电话簿（5）

`AT+CNUM`（本机号码）、`AT+CPBF`（查找）、`AT+CPBR`（读取）、`AT+CPBS`（选择存储）、`AT+CPBW`（写入）。号码未写入 SIM 时 `CNUM` 空结果正常；读写受 `CSCS` 与存储位置影响，`CPBW` 会改写 SIM/ME 数据。

### 6. 短信（18）

| 命令组 | 覆盖命令 | 中文用途 |
|---|---|---|
| 服务与格式 | `AT+CSMS`、`AT+CMGF`、`AT+CSCA`、`AT+CPMS` | 短信服务、文本/PDU、短信中心、存储。 |
| 读写删除 | `AT+CMGD`、`AT+CMGL`、`AT+CMGR`、`AT+CMGW`、`AT+CMSS` | 删除、列举、读取、保存、从存储发送；内容脱敏。 |
| 发送 | `AT+CMGS`、`AT+CMMS`、`AT+QCMGS` | 单条、连续、拼接发送；`CMGS` 的 `>` 提示和 Ctrl-Z 是独立状态机。 |
| 事件与参数 | `AT+CNMA`、`AT+CNMI`、`AT+CSCB`、`AT+CSDH`、`AT+CSMP`、`AT+QCMGR` | 确认、URC、广播、文本头、发送参数、拼接读取。 |

短信失败优先记录 `+CMS ERROR`、字符集、存储容量、`CNMI` 配置；不要只记录“发送失败”。

### 7. 分组数据 / PDP（16）

| 命令 | 中文用途与拨号关系 |
|---|---|
| `AT+CGATT` | PS 附着/去附着。 |
| `AT+CGDCONT` | CID、PDP 类型、APN；拨号核心配置。 |
| `AT+CGQREQ` / `AT+CGQMIN` | 请求/最低传统 QoS。 |
| `AT+CGEQREQ` / `AT+CGEQMIN` | 3G 扩展 QoS。 |
| `AT+CGACT` | 按 CID 激活/去激活 PDP。 |
| `AT+CGDATA` | 进入数据态；端口不再可按 AT 命令态使用。 |
| `AT+CGPADDR` | 查询模组侧 PDP 地址。 |
| `AT+CGCLASS` | GPRS 移动台类别。 |
| `AT+CGREG` / `AT+CEREG` | PS/EPS 注册；LTE 优先看 `CEREG`。 |
| `AT+CGEREP` | 分组域事件 URC。 |
| `AT+CGSMS` | MO 短信承载选择。 |
| `AT+QGDCNT` / `AT+QAUGDCNT` | 流量计数/自动保存；不等同 Linux 网卡统计。 |

建议 PDP 状态机：`CPIN READY` → `CEREG 1/5` → 核对 `CGDCONT` → 查询/发起 `CGATT` → 查询/发起 `CGACT` → `CGPADDR` → 主机网卡/DHCP/路由/公网验证。

### 8. 硬件相关（3）

| 命令 | 中文用途与注意事项 |
|---|---|
| `AT+QPOWD` | 正常关机；等待 `POWERED DOWN`。 |
| `AT+CCLK` | 读写模组时钟。 |
| `AT+QSCLK` | 慢时钟/休眠；需和 DTR、URC 唤醒共同验证。 |

## 附录：全部主题

| 主题 | 工程用途 |
|---|---|
| `AT&F` 默认值、`AT&W` 可保存项、`ATZ` 可恢复项 | 配置持久化与恢复策略。 |
| CME 错误码、CMS 错误码 | 必须启用 `CMEE=2` 并保留错误上下文。 |
| URC 汇总 | SIM、网络、短信、电源等异步事件；不可与同步响应混淆。 |
| GSM/IRA 短信字符转换表 | UCS2/文本短信编码的依据。 |
| `AT+CEER` 释放原因文本 | 注册、PDP、网络释放失败的深度定位。 |

## 与当前 IMX6ULL 拨号程序的对应

| 阶段 | AT 依据 | 还必须做的主机侧验证 |
|---|---|---|
| 模组发现 | `AT`、`ATI`、`CGMR` | 重枚举后重新发现 AT 端口。 |
| SIM | `CPIN`、`QCCID`、`CIMI` | 身份信息脱敏。 |
| 注册 | `CEREG`、`COPS`、`CSQ`、`QNWINFO` | 区分未注册、搜索中、被拒绝和漫游。 |
| PDP | `CGDCONT`、`CGATT`、`CGACT`、`CGPADDR` | APN 合法性；不输出明文。 |
| 专用数据链路 | `QNETDEV*` | 本 PDF 未覆盖，按实机与专用文档。 |
| 成功判定 | 无单一 AT 命令 | 网卡、DHCP、IPv4、默认路由、接口绑定公网探测均成功。 |

## 原始资料

- [原始 PDF：EG912-at.pdf](EG912-at.pdf)
- 原手册逻辑页 1–167，PDF 物理页 168 页。
- 需要精确参数值、完整 QoS 表、完整 CME/CMS 表、URC 逐条格式或字符集转换时，按本索引回查 PDF；不要依据摘要猜测。
