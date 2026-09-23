# Quectel AG35-CET&AG35-EUT QuecOpen(SDK) 软件热管理指导

## 文档基本信息
- **文档标题**：AG35-CET&AG35-EUT QuecOpen(SDK) 软件热管理指导
- **厂商**：上海移远通信技术股份有限公司（Quectel Wireless Solutions Co., Ltd.）
- **适用模块**：LTE Standard 模块系列，AG35-CET 与 AG35-EUT
- **方案**：QuecOpen（基于 Linux 的嵌入式开发平台，简化 IoV/车联网应用开发）
- **版本号**：1.0.0（临时版本 / Preliminary）
- **日期**：2024-09-07
- **状态**：临时文件（Preliminary Confidential）
- **总页数**：14 页（正文 13 页编号）
- **联系方式**：电话 +86 21 5108 6236；邮箱 info@quectel.com；技术支持 support@quectel.com
- **底层 SoC**：pxa1826 / ASR1806（驱动文件 asr1806_thermal.c；脚本注释提及 pxa1826 SOC）
- **运行系统**：OpenWrt（命令提示符为 root@OpenWrt:~#）

### 修订记录
- `-`，2024-09-07，Gabriel LI，文档创建
- `1.0.0`，2024-09-07，Gabriel LI，临时版本

### 文档目录结构
1. 引言
2. 热管理机制说明（2.1 热管理机制概述、2.2 默认热管理机制）
3. 热管理机制配置（3.1 查询当前温度、3.2 关闭热管理机制、3.3 修改热管理机制温度阈值）
4. 附录 参考文档与术语缩写

---

## 逐章节内容摘要

### 第1章 引言
- AG35-CET 和 AG35-EUT 模块支持 QuecOpen 方案。QuecOpen 是基于 Linux 的嵌入式开发平台，可简化 IoV（车联网）应用的软件设计与开发，详细信息参考文档 [1]《Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导》。
- 本文档适用于 SDK 构建环境的 QuecOpen 方案，主要介绍 AG35-CET 和 AG35-EUT 模块的软件热管理机制。

### 第2章 热管理机制说明

#### 2.1 热管理机制概述
- QuecOpen 方案下，模块可在 **LTE 网络下**执行热管理机制。当模块达到设定的温度阈值时，对应的热管理策略立即生效。
- 模块会根据 **10 个不同的温度等级**（Level 0~9）执行热管理机制。当模块温度达到对应热缓解等级（Level x，x=0~9）设置的温度阈值时，执行对应热管理策略；当模块温度低于"清除门限"时，退出对应温度等级。
- **状态机迁移（图1 示意图）**——进入阈值（升温）与退出阈值（降温清除门限）：
  - Level 0 ⇄ Level 1：升温 T>85℃ 进入 Level 1；降温 T<82℃ 退回 Level 0
  - Level 1 ⇄ Level 2：T>90℃ 进入；T<87℃ 退回
  - Level 2 ⇄ Level 3：T>95℃ 进入；T<92℃ 退回
  - Level 3 ⇄ Level 4：T>100℃ 进入；T<97℃ 退回
  - Level 4 ⇄ Level 5：T>105℃ 进入；T<102℃ 退回
  - Level 5 ⇄ Level 6：T>110℃ 进入；T<107℃ 退回
  - Level 6 ⇄ Level 7：T>115℃ 进入；T<112℃ 退回
  - Level 7 → Level 8：T>120℃ 进入
  - Level 8 → Level 9：T>123℃ 进入
- 即升温阈值依次为 85/90/95/100/105/110/115/120/123℃，降温清除门限依次为 82/87/92/97/102/107/112℃。

#### 2.2 默认热管理机制（表1）
| 热缓解等级 | 默认温度阈值（进入 / 退出） | 热管理策略 |
|---|---|---|
| 0 | 85℃ < T 以下（T < 85℃） | 无热管理策略，模块正常工作 |
| 1 | 85℃ < T < 90℃（T<82℃ 退出 Level1 进入 Level0） | 无热管理策略，模块正常工作 |
| 2 | 90℃ < T < 95℃（T<87℃ 退出 Level2 进入 Level1） | 无热管理策略，模块正常工作 |
| 3 | 95℃ < T < 100℃（T<92℃ 退出 Level3 进入 Level2） | 无热管理策略，模块正常工作 |
| 4 | 100℃ < T < 105℃（T<97℃ 退出 Level4 进入 Level3） | 最大下行速率限制为 50 Mbps |
| 5 | 105℃ < T < 110℃（T<102℃ 退出 Level5 进入 Level4） | 射频功率降低 3 dBm；最大下行速率限制为 50 Mbps |
| 6 | 110℃ < T < 115℃（T<107℃ 退出 Level6 进入 Level5） | 射频功率降低 6 dBm；最大下行速率限制 50 Mbps；最大上行速率限制 50 Mbps |
| 7 | 115℃ < T < 120℃（T<112℃ 退出 Level7 进入 Level6） | 射频功率降低 9 dBm；最大下行速率限制 20 Mbps；最大上行速率限制 10 Mbps；CPU 频率最高限制 416 MHz |
| 8 | 120℃ < T | 软件关机 |
| 9 | 123℃ < T | 硬件看门狗复位 |
- **备注**：模块进入 Level 8 触发软件关机时，若关机失败，模块进入 Level 9，触发硬件看门狗复位。

### 第3章 热管理机制配置
- 模块热管理机制中，温度控制脚本名称为 `thermal_zone40`。本章介绍如何查询当前温度、关闭热管理机制、修改温度阈值。

#### 3.1 查询当前温度
- 执行 `cat /sys/class/thermal/thermal_zone0/temp` 可查询模块当前温度（单位 0.001℃，即毫摄氏度）。
- 示例：输出 `34000` 表示当前模块温度为 34℃。

#### 3.2 关闭热管理机制
- 热管理机制默认为开启状态。热管理机制逻辑位于内核层，**Level 8~9 均无法关闭**；可通过修改温控脚本 `/ql-ol-rootfs/etc/hotplug.d/thermal/00-tz0throttle` 关闭 **Level 0~7** 的热管理策略。
- 脚本基本结构（见示例代码说明）：
  - 头部注释列出 3 类 thermal zone（pxa1826 SOC 内部温度传感器、电池包内 NTC 热敏电阻、连接到 PMIC GPADC 的传感器）与 3 类用户空间冷却设备（蜂窝收发器限 Tx 功率/关 PS/飞行模式、WiFi 限 Tx 功率、pxa1826 限带宽和 CPU 频率）。
  - 仅在 PROD 模式（`uci get cmdline.PROD`）为 "0" 时执行，否则 `exit 0`。
  - 根据 `$TREND`（1=升温、2=降温）与 `$TSTAGE`（温度等级 x）分支执行限速/降功率/限频或解除限制。
- 脚本说明（文档原文 3 条）：
  1. 模块每次温度变化均会触发执行该脚本进行温度判断。温度上升进入第一个逻辑判断 `"$TREND" == "1"`，温度下降进入第二个逻辑判断 `"$TREND" == "2"`。
  2. 温度等级 `"$TSTAGE" == "x"`，x 即热缓解等级（详见 2.2 章）。**Level 0~7 对应的热管理策略均在应用层进行；Level 8~9 对应的热管理策略为硬件策略，属于芯片保护强制措施。**
  3. 模块支持关闭 Level 0~7 对应的热管理策略，即把脚本中用于执行对应热管理策略的命令或 AT 命令注释掉即可。Level 8~9 对应的热管理策略无法关闭。
- **备注**：为保证模块的可靠性和稳定性，模块正常运行时不建议关闭热管理机制。

#### 3.3 修改热管理机制温度阈值
- 可通过 `ql-ol-kernel/drivers/thermal/asr1806_thermal.c` 文件修改温度阈值。
- 该文件中两个数组分别对应 Level 0~7 等级的**进入温度阈值**（`trips_temp_benchmark`）和**退出温度阈值**（`trips_hyst_benchmark`）。
- 设置完成后，温控脚本 `/ql-ol-rootfs/etc/hotplug.d/thermal/00-tz0throttle` 会在模块温度达到指定值时执行对应的热管理策略。
- **备注**：设置温度阈值时，不可超过 120℃。

### 第4章 附录 参考文档与术语缩写
- **表2 参考文档**：[1] Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导
- **表3 术语和缩写**：
  - CPU：Central Processing Unit，中央处理器
  - LTE：Long Term Evolution，长期演进
  - IoV：Internet of Vehicles，车联网
  - SDK：Software Development Kit，软件开发工具包

---

## 关键 API / 命令 / 文件清单

### Shell 命令
- `cat /sys/class/thermal/thermal_zone0/temp`：查询模块当前温度，输出单位为 0.001℃（毫摄氏度），如 34000=34℃。
- `uci get cmdline.PROD`：读取 PROD 模式标志（脚本中据此决定是否启用热管理，仅 "0" 时执行）。
- `echo <值> > /dev/kmsg`：向内核日志打印调试信息（脚本中输出 TREND/TSTAGE/RF 功率等）。

### AT 命令
- `AT*GRIP=2,<n>`（脚本中 `serial_atcmd at*grip=2,N`）：射频功率（Tx Power）降低控制命令。
  - `at*grip=2,3`：射频功率降低 3 dBm（Level 5）。
  - `at*grip=2,6`：射频功率降低 6 dBm（Level 6）。
  - `at*grip=2,9`：射频功率降低 9 dBm（Level 7）。
  - `at*grip=2,0`：射频功率降低 0 dBm（解除，降温恢复时使用）。

### 关键内核 sysfs / 文件路径
- `/sys/class/thermal/thermal_zone0/temp`：当前温度读取节点。
- `/sys/kernel/debug/tel/psd/data-pathv*/tpt_limit`（脚本变量 `TPT_PATH`）：吞吐量限速节点；写入格式 `下行,?,上行`，如 `echo 20,50,0` 限下行 50Mbps，`echo 20,20,10` 限下行 20Mbps/上行 10Mbps，`echo 0,0,0` 解除限速。
- `/sys/power/cpu_freq_max_pm_qos`：写入 `thermal_throttle 416000` 将 CPU(AP core) 最高频率限制到 416 MHz（Level 7）。
- `/sys/power/cpu_freq_max_pm_unqos`：写入 `thermal_throttle` 解除 CPU 频率限制（降温恢复）。
- `/tmp/dBm`：记录当前射频功率偏移量（dBm offset），供降温恢复时参考。
- `/ql-ol-rootfs/etc/hotplug.d/thermal/00-tz0throttle`：温控脚本（hotplug 触发），实现 Level 0~7 用户空间冷却策略。
- `ql-ol-kernel/drivers/thermal/asr1806_thermal.c`：内核热管理驱动，定义进入/退出温度阈值数组。
- 温度控制脚本名称：`thermal_zone40`。

### 关键数组（asr1806_thermal.c）
- `static int trips_temp_benchmark[TRIP_POINTS_NUM]`：Level 0~7 进入温度阈值（85000/90000/95000/100000/105000/110000/115000/120000，单位毫摄氏度）。
- `static int trips_hyst_benchmark[TRIP_POINTS_NUM]`：Level 0~7 退出（迟滞）温度阈值（82000/87000/92000/97000/102000/107000/112000/120000）。

---

## 示例代码说明

1. **查询温度（3.1）**：
   ```sh
   root@OpenWrt:~# cat /sys/class/thermal/thermal_zone0/temp
   34000     //当前模块温度为 34 ℃
   ```
   说明：sysfs 温度节点以毫摄氏度返回整数，应用层除以 1000 得到摄氏度。

2. **温控脚本 00-tz0throttle 头部与生效条件（3.2）**：
   ```sh
   #!/bin/sh
   # There are 3 kinds of thermal zones ... (pxa1826 SOC / NTC / PMIC GPADC)
   # There are also 3 kinds of user space cooling devices ... (cellular Tx / WiFi Tx / pxa1826 bandwidth&CPU freq)
   devtz=`echo $TREND`
   PRODMODE=`uci get cmdline.PROD 2> /dev/null`
   [ "$PRODMODE" != "0" ] && exit 0
   TPT_PATH="$(echo /sys/kernel/debug/tel/psd/data-pathv*/tpt_limit)"
   ```
   说明：`$TREND`/`$ACTION` 由 hotplug 环境注入。`PRODMODE != "0"` 时直接退出——只有生产模式标志为 0 才启用热管理。`TPT_PATH` 用通配符展开得到吞吐量限速的实际 debugfs 路径。

3. **升温分支（$TREND==1，逐级限制，3.2）**：
   ```sh
   if [ "$devtz" != "" -a "$ACTION" = "change" ]; then
       echo "TREND=$TREND, TSTAGE=$TSTAGE, RF_AbsPwrDown=`cat /tmp/dBm` dBm" > /dev/kmsg
       if [ "$TREND" == "1" ]; then
           if [ "$TSTAGE" == "1" -o "$TSTAGE" == "2" -o "$TSTAGE" == "3" ]; then
               echo "TSTAGE 1/2/3, no user space cooling" > /dev/kmsg   # Level1~3 不限速
           elif [ "$TSTAGE" == "4" ]; then
               echo 20,50,0 > $TPT_PATH                                  # 下行限 50Mbps
           elif [ "$TSTAGE" == "5" ]; then
               serial_atcmd at*grip=2,3                                  # RF -3dBm
               echo 3 > /tmp/dBm
               echo 20,50,0 > $TPT_PATH
           elif [ "$TSTAGE" == "6" ]; then
               serial_atcmd at*grip=2,6                                  # RF -6dBm
               echo 6 > /tmp/dBm
               echo 20,50,50 > $TPT_PATH                                 # 下行50/上行50Mbps
           elif [ "$TSTAGE" == "7" ]; then
               serial_atcmd at*grip=2,9                                  # RF -9dBm
               echo 9 > /tmp/dBm
               echo 20,20,10 > $TPT_PATH                                 # 下行20/上行10Mbps
               echo thermal_throttle 416000 > /sys/power/cpu_freq_max_pm_qos  # CPU限416MHz
           fi
   ```
   说明：温度升高时，从 Level 4 开始逐级加码冷却——先限下行速率，再叠加射频降功率（at*grip），再限上行，最后在 Level 7 限制 CPU 主频。每次降功率都把偏移量写入 /tmp/dBm 以便降温时按相同顺序解除。Level 1~3 仅记录不做冷却。

4. **降温分支（$TREND==2，逐级解除，3.2）**：
   ```sh
   elif [ "$TREND" == "2" ]; then
       if [ "$TSTAGE" == "0" -o "$TSTAGE" == "1" -o "$TSTAGE" == "2" ]; then
           echo "TSTAGE 0/1/2, no user space uncooling" > /dev/kmsg
       elif [ "$TSTAGE" == "3" ]; then
           echo 0,0,0 > $TPT_PATH                          # 解除限速
       elif [ "$TSTAGE" == "4" ]; then
           serial_atcmd at*grip=2,0                         # RF 恢复 0dBm
           echo 0 > /tmp/dBm
           echo 20,50,0 > $TPT_PATH
       elif [ "$TSTAGE" == "5" ]; then
           serial_atcmd at*grip=2,3
           echo 3 > /tmp/dBm
           echo 20,50,0 > $TPT_PATH
       elif [ "$TSTAGE" == "6" ]; then
           serial_atcmd at*grip=2,6
           echo 6 > /tmp/dBm
           echo 20,50,50 > $TPT_PATH
           echo thermal_throttle > /sys/power/cpu_freq_max_pm_unqos   # 解除CPU限频
       fi
   else
       echo "WARNING: not handled thermal stage..." > /dev/kmsg
   fi
   ```
   说明：降温时按当前所处等级回退一档冷却策略——逐步恢复速率、提升射频功率、解除 CPU 限频。注意降温分支用 `cpu_freq_max_pm_unqos` 撤销升温分支的 `cpu_freq_max_pm_qos`。未匹配的等级打印 WARNING。

5. **温度阈值数组（asr1806_thermal.c，3.3）**：
   ```c
   static int trips_temp_benchmark[TRIP_POINTS_NUM] = {
       85000, /* TRIP_POINT_0 */
       90000, /* TRIP_POINT_1 */
       95000, /* TRIP_POINT_2 */
       100000,/* TRIP_POINT_3 */
       105000,/* TRIP_POINT_4 */
       110000,/* TRIP_POINT_5 */
       115000,/* TRIP_POINT_6 */
       120000,/* TRIP_POINT_7 */
   };
   static int trips_hyst_benchmark[TRIP_POINTS_NUM] = {
       82000, 87000, 92000, 97000, 102000, 107000, 112000, 120000,
   };
   ```
   说明：`trips_temp_benchmark` 为各等级进入阈值（升温触发），`trips_hyst_benchmark` 为退出（迟滞清除）阈值（降温触发），单位均为毫摄氏度，与 2.2 表格一一对应。修改这两个数组即可自定义触发温度，但不可超过 120℃，且必须重新编译内核生效。

<!-- GENERATION_COMPLETE: 2026-06-25_03:45 -->
