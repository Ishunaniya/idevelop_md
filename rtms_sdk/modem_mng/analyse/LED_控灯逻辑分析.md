# LED 控灯逻辑分析记录（EC200A / EG25）

> 本文档基于对仓库源码的实际核查整理，所有结论均附文件路径与行号。
> 后续可据此提出源码修改需求。
>
> - 仓库根：`/home/tronlong/lyp/code/rtms_sdk`
> - 工作目录：`apps/modem_mng`
> - 当前分支：`develop/rtms_sdk_v1.3_20240408_dc_switch`
> - 整理日期：2026-06-01

---

## 0. 涉及文件

| 角色 | 路径 |
|---|---|
| EC200A 应用入口 / 主循环 | `apps/modem_mng/dialer_ec200a.cpp` |
| EG25 拨号状态机 / 主循环 | `apps/modem_mng/eg25/dial/dial.c` |
| 控灯库 C 包装层 | `3rdparty/ledctl/led_control_c_api.cpp` |
| 控灯库 C 头文件 | `3rdparty/ledctl/led_control_c_api.h` |
| 控灯库实现 | `3rdparty/ledctl/led_control.cpp` |
| 控灯库类声明 | `3rdparty/ledctl/led_control.hpp` |
| LED 枚举类型 | `3rdparty/ledctl/led_type.h`（构建产物副本：`build_eg25g/build_dir/include/led_type.h`） |
| 下游命令接收方（同仓库另一 app） | `apps/io_mng/src/cmd/cmd.c`、`apps/io_mng/src/cmd/cmd.h`、`apps/io_mng/src/cmd/cmd_settings.c`、`apps/io_mng/src/io_mng/io_mng.h` |

构建关系：`3rdparty/CMakeLists.txt:70-71` 通过 `add_subdirectory(ledctl)` 编译该库（`option(USE_LIBLEDCTL ... ON)`，第 26 行）；`apps/modem_mng/CMakeLists.txt:211` 安装 `libledcontrol.so`。两个平台共用同一套控灯库。

---

## 1. 控灯机制（两平台共用）

### 1.1 接口
C 包装层（`led_control_c_api.h`）：

```c
LEDControlHandle LEDControl_create(const char* addr);
void LEDControl_destroy(LEDControlHandle handle);
void LEDControl_controlLight(LEDControlHandle handle, LED_ID_t LED_ch, bool on);   // 常亮/熄灭
void LEDControl_blinkLight (LEDControlHandle handle, LED_ID_t LED_ch, int high, int low); // 闪烁
int  LEDControl_getLedIdType(LEDControlHandle handle);                              // 0=pro, 其它=vbox
```

### 1.2 创建与选灯
两平台启动时：
- `LEDControl_create("tcp://127.0.0.1:26008")`（nanomsg PUB 地址）。
- `LEDControl_getLedIdType()` 判定机型：返回 `0` → pro，使用 `LED_NET_GREEN`；否则 → vbox，使用 `EX_GPIO_LED_NET`。即控制的是**网络指示灯**。
  - 机型判定逻辑（`led_control.cpp:45-72`）：读 `/opt/product_led_type`，内容含 `pro/PRO` 返回 0，否则返回 1；文件不存在则读 `/etc/rtms-product-type` 的 productCode 在 `PRODUCT_TYPES` 表中匹配，匹配不到默认返回 0。

### 1.3 LED_ID_t 类型（`led_type.h`）
```c
typedef enum { LED_CAN_GREEN=0, LED_CAN_REDLED, LED_NET_GREEN, LED_NET_WIFI, LED_MAX_NUM } LED_ID_EC200A_PRO_t;
typedef enum { EX_GPIO_LED_NET=0, EX_GPIO_LED_CAN, EX_GPIO_LED_GPS, EX_GPIO_LED_WIFI, EX_GPIO_LED_SYS, LED_NUM_MAX } LED_ID_EC200A_VBOX_t;
typedef union { LED_ID_EC200A_PRO_t pro; LED_ID_EC200A_VBOX_t vbox; } LED_ID_t;
```

### 1.4 三种灯态语义
| 调用 | 含义 |
|---|---|
| `blinkLight(handle, id, 0x01, 0xf4)` | 闪烁（mode=1，脉冲）——拨号中 / 通道切换中 |
| `controlLight(handle, id, 1)` | 常亮——已连通 |
| `controlLight(handle, id, 0)` | 熄灭——断网 |

### 1.5 命令实际下发路径（`led_control.cpp:122-194`）
- `controlLight` / `blinkLight` 组装 `CmdLED` 结构后调用 `sendCmdLED()`。
- `sendCmdLED()`（122-171 行）：
  - 把命令字节写入文件 `/tmp/cmdled`；
  - **真正的发送被注释**：`system("nanocat --pub --connect ... -F /tmp/cmdled -d 1")`（162 行，注释“使用 system 函数执行命令 -- 不控灯”）与 `nn_send(...)`（165-170 行）**均处于注释状态**。

### 1.6 下游消费者：io_mng（新增核查，2026-07-01）

> 追问“控灯最终到底是驱动执行还是 MCU 执行”，向下游追了一层，结论是：**协议格式证明设计意图是转发给 io_mng 再转 MCU，但接收端同样是死代码，链路上下游都断。**

**事实 A —— `sendCmdLED` 组装的字节流是 io_mng 的通用命令协议 `cmd_request_t`，非巧合**：
- `led_control.cpp:132-142` 组装的 20 字节 `msg`：`pid`(4B) + `0x34,0x00`（cmd_tag，2B）+ `cmd_idx`(1B)+ `cmd_len=0x08`(1B) + `rsv`(4B) + 8B LED 负载。
- `apps/io_mng/src/cmd/cmd.h:33-41` 的 `cmd_request_t` 结构体逐字段同构：`pid, cmd_tag, cmd_idx, cmd_len, rsv[4], cmd_bytes[]`。
- `cmd_tag = 0x0034` = 十进制 **52**，正好等于 `apps/io_mng/src/cmd/cmd.h:9-18` 里 `cmd_tag_e` 的 `CMD_LED_CTRL`（`CMD_DO_CTRL=50, CMD_AO_CTRL=51, CMD_LED_CTRL=52`）。
- 发送地址 `tcp://127.0.0.1:26008`（`led_control.hpp` / `led_control_c_api.cpp` 传入）与 `apps/io_mng/src/io_mng/io_mng.h:84,118` 定义的 `IPC_URL_CMD_SUB`（io_mng 的 nanomsg **SUB** 接收端口）完全一致，`cmd.c:191` `init_pubsub_endpoint(...)` 在该端口上 bind 接收。
- 同一端口/协议还被 `apps/bs_client_heavy_truck`、`bs_client_port_crane`、`bs_client_dump_truck` 的 `src/cmd/cmd.c` 用来下发 `CMD_DO_CTRL`/`CMD_AO_CTRL`（继电器控制）——证明这是同一套“app → io_mng → （推断）MCU”通用命令通道，灯控只是其中一个 `cmd_tag`。

**推断（非本仓库代码可证）**：RK3576 侧没有直接操作 LED 的内核驱动/`sysfs` 代码（`apps/io_mng` 的 `gpio.c`/`gpio_imx.c` 只覆盖 ACC/IG/SLEEP/RTC_IRQ/SPI CS 等引脚，无 LED），按此前对 `CMD_TIME_SET`（RTC 授时）的调研（`apps/io_mng/rk3576_时间RTC_原理图分析.md`），这类通用命令历来是转发给 MCU 固件执行物理动作。据此推断，**灯控设计上最终执行者是 MCU，而非 Linux 驱动**——但这一步未在本仓库代码中直接证实，MCU 固件不在此仓库。

**事实 B —— 但即使消息真的发出去了，io_mng 接收后也没人转发**：
- `cmd.c:149-166`（`cmd_ipc_rcv_cb`）非 IMX6 分支调用 `push_cmd_req_item()`（`cmd.c:109-146`），只是 `calloc`+`memcpy`+`List_push_end` 塞进 `pCmdMng->cmd_req_lst`（`cmd.c:140-141`，链表定义见 `cmd.h:69`）。
- 对 `apps/io_mng/src/` 全目录 grep `cmd_req_lst`：**只有 `cmd.c:140`（push）和 `cmd.c:204`（`List_list(NULL)` 初始化）两处，全仓库找不到任何 `List_pop`/`List_get`/遍历它的代码**——这是一个只进不出的队列，没有任何代码把它转发到 `apps/io_mng/src/spi/spi.c`（io_mng↔MCU 已知的 SPI 链路，`spi_send_pkt` 本身也查无调用方）或任何串口。
- IMX6 平台分支（`cmd.c:63-105` `parse_cmd_req_item`）只特判处理 `CMD_NMEA_REQ`/`CMD_GPS_COM_REQ`，同样不处理 `CMD_LED_CTRL`。
- 这不是灯控独有的问题：`CMD_TIME_SET`（`cmd_settings.c:85-129`，RTC 授时）也是走同一条 `push_cmd_req_item → cmd_req_lst`，同样查无下游消费者。

**结论直接回答“到底谁执行”**：按协议格式和端口号，设计意图是 io_mng 转发给 MCU 执行；但按当前代码事实，`led_control.cpp` 发送端已注释（P1），`io_mng` 接收端的 `cmd_req_lst` 只推不拉——**链路两端都是死代码，目前物理上没有任何一方在真正执行灯控指令**。

---

## 2. 当前代码（HEAD）控灯逻辑

### 2.1 EC200A（`dialer_ec200a.cpp`）—— 集中式
LED 相关行（grep 结果）：

| 行号 | 内容 |
|---|---|
| 1665 | `LEDControl_create(...)` |
| 1667 | `LEDControl_getLedIdType(handle)` |
| 1677 | `int currentLEDState = -1; // Added to track the previous LED state` |
| 1680 | 启动闪灯 `blinkLight` |
| 1743 / 1755 | （状态相关）闪灯 |
| 2094 / 2118 / 2202 | Roamlink 通道切换处闪灯 |
| 2150 | `currentLEDState = 2; // 亮灯`（ping 成功路径） |
| 2263 | `currentLEDState = 0; // 灭灯`（ping 失败、未触发恢复路径） |
| 2337 | 循环末尾 `switch(currentLEDState)` 统一下发 |
| 2340 | `case 0: if(handle) LEDControl_controlLight(handle, ledId, 0);` |
| 2343-2345 | `case 1:` 闪灯 + `sleep(1)` + 闪灯 |
| 2349 | `case 2: LEDControl_controlLight(handle, ledId, 1);` |

逻辑：在主循环（每 50ms，`usleep(50*1000)`）内根据 ping 结果设置 `currentLEDState`，循环末尾统一 `switch` 下发。连通常亮、断网灭灯，切换通道时闪烁。

### 2.2 EG25（`eg25/dial/dial.c`）—— 散落式
LED 相关行（grep 结果）：

| 行号 | 内容 |
|---|---|
| 857 | `LEDControl_create(...)` |
| 858 | `LEDControl_getLedIdType(handle)` |
| 868 | `int lastLEDState = -1; // Added to track the previous LED state` |
| 869 | 启动闪灯 |
| 887 | 启动时若已连通 → `controlLight(1)` |
| 927 | `begin dial` 再次闪灯 |
| 995 | 心跳 ping 成功（`net_ok`）→ `controlLight(1)` |
| 1049 / 1073 | Roamlink 回切处闪灯 |
| 1092 | ping 连续失败首次（`start_fail_ts==0` 转变）→ `controlLight(0)` |
| 1145 | SIM→Roamlink 切换处闪灯 |
| 1217 / 1224 | L1 / L2 恢复处闪灯 |
| 1626 | 状态机 `dial_stat_net_connected` → `controlLight(1)` |
| 1632 | `connection lost` → `controlLight(0)` |
| 1830-1948 | 多处闪灯（Roamlink 相关分支） |
| 2028 | `LEDControl_destroy(handle)`（`while(true)` 后，不可达） |

逻辑：没有集中收口的 switch，控灯调用直接散落在心跳 ping 路径与拨号状态机各分支中。

---

## 3. 提交 `3bc83208a46c34f8333dd59141d97d6efbff2436` 之前的控灯逻辑

- 目标提交：`3bc83208` = `[NEWFUNC] 新增 Roamlink 双卡切换逻辑（EG25/EC200A）`，作者 yuping.liu，2026-05-18。
- 其父提交（即“之前”的状态）：`98428d2fe0ee103e936baa162a6dc85a0fd311ab`。
- 注意：该时点仓库布局中文件已在 `apps/modem_mng/` 下。

### 3.1 EC200A @父提交（结构与现在几乎相同）
LED 行：974（create）、976（getLedIdType）、986（`currentLEDState=-1`）、989（启动闪灯）、1104（`=2` 亮灯）、1145（`=0` 灭灯）、1191（末尾 switch）、1194/1197/1199/1203（switch 各 case）。

- 主循环 `ST_PING`：ping 成功 `currentLEDState=2`，ping 失败（未触发恢复）`currentLEDState=0`，末尾 switch 统一下发。
- **与现在唯一区别**：没有 Roamlink 切换处的闪灯（现在 1743/2094/2118/2202 是 Roamlink 提交之后才加的）。
- 行为：连通常亮、断网灭灯，逻辑完整。

### 3.2 EG25 @父提交（比现在简单得多，且与 EC200A 不对称）
全文仅 5 处 LED 调用：401（启动闪灯）、441（begin dial 闪灯）、418（启动已连通 → `controlLight(1)`）、740（状态机 `dial_stat_net_connected` → `controlLight(1)`）、928（`destroy`，不可达）。

关键事实：
- **当时 EG25 完全没有任何 `controlLight(…,0)`（熄灯）调用**。掉线时（`connection lost`，742 行附近）只切回 `dial_stat_reg_check`，**不熄灯**；L1/L2/L3 恢复也不动灯。
- 当时 EG25 灯态只有“闪烁→常亮”，**断网后灯不灭**（停留在上一状态）。
- 心跳 / ping 检测段当时**不驱动灯**，灯完全由拨号状态机驱动。
- `lastLEDState` 同样声明后从未使用。

### 3.3 库底层 @父提交
- `led_control_c_api.cpp` 的 `LEDControl_getLedIdType` 已经缺 `return`（同现在）。
- `led_control.cpp` 的 `sendCmdLED` 已是“写文件 + 注释掉 system/nn_send”（同现在，注释“不控灯”在 161 行）。
- 结论：库层两个问题**早于** `3bc83208`，非该提交引入。

### 3.4 `3bc83208`（及其后）才补上的控灯（对照）
- 两平台在 SIM↔Roamlink 切换点新增 `blinkLight`；
- **EG25 在心跳 ping 路径补了 ping 通→`controlLight(1)`、连续失败→`controlLight(0)`，并在 `connection lost` 处补了 `controlLight(0)`**——即“EG25 断网灭灯”是这条提交（及之后）才补齐的。

---

## 4. 已确认的问题清单

> 严重度为主观排序，供后续修改决策参考。

### P1 —【严重，影响两平台】底层根本不真正下发控灯命令
- 位置：`3rdparty/ledctl/led_control.cpp:122-171` `sendCmdLED()`。
- 事实：只把命令写入 `/tmp/cmdled`；`system(nanocmd)`（162 行）与 `nn_send(...)`（165-170 行）均被注释。
- 影响：上层所有 `controlLight/blinkLight` 调用最终不驱动任何灯。

### P2 —【严重，UB，影响两平台】`LEDControl_getLedIdType` C 包装缺 `return`
- 位置：`3rdparty/ledctl/led_control_c_api.cpp:23-25`。
  ```cpp
  int LEDControl_getLedIdType(LEDControlHandle handle) {
      ((LEDControl*)handle)->getLedIdType();   // 缺 return
  }
  ```
- 事实：非 void 函数走到末尾不返回是未定义行为，返回值为寄存器残值。
- 影响：`dialer_ec200a.cpp:1667`、`eg25/dial/dial.c:858` 据此判定 pro/vbox，**选灯不可靠**（即使 P1 修好仍可能选错灯）。

### P3 —【逻辑，未按意图去重 / 每轮重复下发】
- 注释意图：两处均有 `// Added to track the previous LED state`，但**都未与上次状态比较再下发**。
- EC200A：`currentLEDState` 跨循环常驻，末尾 switch 每 50ms 执行一次 → 连通时约 20 次/秒重复 `controlLight(1)`。
- EG25：`lastLEDState`（`dial.c:868`）**声明后从未使用**（死变量）；`controlLight(1)` 在 `net_ok`（995）与 `dial_stat_net_connected`（1626）每轮重复下发。

### P4 —【死代码】EC200A 闪灯 case 永不触发
- 位置：`dialer_ec200a.cpp:2342-2346` `case 1`（含 `sleep(1)`）。
- 事实：`currentLEDState` 全程只被赋 `2`（2150）或 `0`（2263），初值 -1，**永远不为 1**。

### P5 —【健壮性，次要】EC200A switch 判空不一致
- `case 0`（2340）有 `if(handle)`，`case 2`（2349）与闪灯分支无判空。
- 缓解：`LEDControl_create` 用 `new`，正常不返回 null（失败时抛异常）。

### P6 —【资源，次要】EG25 `LEDControl_destroy` 不可达
- 位置：`eg25/dial/dial.c:2028`，位于 `while(true)` 之后，句柄不会释放。

### P7 —【一致性 / 可维护性】两平台控灯写法不统一
- EC200A 集中式（`currentLEDState` + 末尾 switch）；EG25 散落 inline，控灯点分散十余处，易漏改。

### P8 —【严重，跨 app，非 modem_mng 可单独修复】下游 io_mng 侧接收队列只推不拉，即使 P1 修好命令也到不了 MCU
- 位置：`apps/io_mng/src/cmd/cmd.c:109-146`（`push_cmd_req_item`）、`cmd.c:140-141`（`pCmdMng->cmd_req_lst`）。
- 事实：`sendCmdLED` 组装的字节流字段级对齐 io_mng 的 `cmd_request_t`/`CMD_LED_CTRL`（`cmd_tag=0x34`=52，见 1.6 节），发送端口 `26008` 与 io_mng 的 `IPC_URL_CMD_SUB` 一致，证明设计上是转发给 io_mng（再推断转 MCU）。但 `cmd_req_lst` 全仓库只有 push、无 pop/遍历，是死队列；`apps/io_mng/src/spi/spi.c` 的 `spi_send_pkt`（推断的 MCU 物理链路）也查无调用方。
- 影响：即使先修好 P1（启用真正发送），灯控命令送到 io_mng 后仍会在 `cmd_req_lst` 里堆积、不会被转发到 MCU——**必须同时修 io_mng 侧才能让灯真正被点亮**，单独改 `modem_mng`/`ledctl` 不够。
- 附带发现：同一断链模式也影响 `CMD_TIME_SET`（`cmd_settings.c:85-129`，RTC 授时），非灯控独有。

---

## 5. 关键命令参考（复核用）

```bash
# 目标提交与其父
git show -s --format='%H %an %ad %s' 3bc83208a46c34f8333dd59141d97d6efbff2436
git rev-parse 3bc83208^   # -> 98428d2fe0ee103e936baa162a6dc85a0fd311ab

# 查看父提交某文件（注意：<rev>:<path> 中 path 相对仓库根）
git show 98428d2f:apps/modem_mng/dialer_ec200a.cpp | grep -nE "LEDControl_|currentLEDState|blinkLight|controlLight"
git show 98428d2f:apps/modem_mng/eg25/dial/dial.c   | grep -nE "LEDControl_|lastLEDState|blinkLight|controlLight"
```

---

## 6. 待办（后续根据本文件提出修改时填写）

- [ ] 是否启用 P1 真正下发（选 `nn_send` 还是 `system(nanocat)`）？
- [ ] P2 补 `return`。
- [ ] 是否引入状态去重（P3），消除每轮重复下发。
- [ ] 是否清理 P4 死代码 / P5 判空 / P6 不可达 destroy。
- [ ] 是否统一两平台控灯风格（P7）。
- [ ] P8（跨 app）：io_mng 侧是否补上 `cmd_req_lst` 消费/转发逻辑？需协调 `io_mng` 维护方，不是 `modem_mng` 单侧能改完的。




