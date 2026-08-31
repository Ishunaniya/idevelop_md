# test_absys 完整升级、验收和异常分支指南

## 1. 适用范围与最高优先级风险

本指南只适用于本目录的 `quectel_AB_OTA.img`：

```text
MD5: 1cae3817a6e84db3fcfb00173ca4fc04
SHA-256: b3df54dddac322a52ffe9383eb14c6b48208c9ef159455a471276e7989bebbc6
```

目标是把当前已恢复并同步完成的 R02A04 A/B 设备升级到完整 R03A02，并在新 R03
首次启动的 preinit 阶段自动更新 OBM。

第一次只能在满足以下条件的实验设备进行：

1. USB SWDownloader 已实际验证能识别、下载和救砖。
2. 串口从菜单 2 之前持续记录到系统完全启动，不能只看 SSH。
3. 稳定供电；切槽后的首次启动尤其禁止断电、复位和拔线。
4. 现场具备源 R02A04 全量线刷包和恢复人员。
5. 一次只测一台，完成全部检查和观察期后再扩面。

OBM 是全局启动区，不是 A/B 槽。升级 OBM 后即使切回旧 A 槽，也不会自动恢复旧
OBM。R02A04 与新 OBM 的回退兼容性尚未被本地事实证明。

## 2. 菜单含义

| 菜单 | API | 含义 | 允许使用的条件 |
|---|---|---|---|
| `0` | `ql_abfota_start_update` | 校验并写 FOTA 到非激活槽 | 升级开始；当前槽健康 |
| `1` | `ql_abfota_get_update_status` | 查询状态、进度、exit code | 任意时刻只读检查 |
| `2` | `ql_absys_switch` | 设置切槽并触发重启 | 只允许在 `WRITEDONE` 后 |
| `3` | `ql_absys_sync` | 用当前激活槽同步另一槽 | 只在当前系统完全健康且决定放弃回滚时 |
| `4` | `ql_absys_get_cur_active_part` | 查询当前 A/B 槽 | 升级前后记录 |
| `5` | `ql_absys_getstatus` | 查询 A/B 分区状态 | 升级前后记录 |
| `-1` | — | 退出 | — |

菜单 2 不等价于普通 `reboot`。菜单 3 会用当前槽覆盖另一槽，是不可轻率执行的收尾
动作。

## 3. 升级前检查

### 3.1 设备基线

```sh
serial_atcmd AT+QGMR
cat /etc/quectel-project-version
ps | grep '[d]ial'
cat /tmp/network_status
cat /NVM/.fota_obm_state 2>/dev/null || echo "no obm state"
df -h /media/sdcard
```

当前实验对象按已有日志应为：

```text
EC200ACNTAR02A04M2G_OCPU
active part A
FOTA SUCCEED
absys partition status succeed
```

如果实际结果不同，先停止并重新记录，不能套用上述假设。

`/NVM/.fota_obm_state=1` 是重要分支：最终 R03 脚本看到 `1` 会直接跳过 OBM
升级。遇到该值先确认设备当前 OBM 是否已经是目标版本，不要未经移远确认直接删除标志。

### 3.2 test_absys 基线

进入 `test_absys`，依次执行：

```text
1  查询 FOTA 状态
4  查询激活槽
5  查询分区健康
```

正常起点应是：`SUCCEED`、已知健康槽、`succeed`。

- 如果是 `NEEDSYNC`：先确认当前槽就是希望保留的健康基线，再决定是否菜单 3。
- 如果是 `WRITEDONE`：说明已有未切换升级，不能直接覆盖；先查包来源和操作历史。
- 如果是 `FAILED` 或分区损坏：停止本次升级，先处理现状。

### 3.3 上传和校验

把本目录 `quectel_AB_OTA.img` 传到设备，例如：

```sh
scp quectel_AB_OTA.img root@<IP>:/media/sdcard/fota.img
scp test_absys root@<IP>:/media/sdcard/test_absys
ssh root@<IP> "chmod 0755 /media/sdcard/test_absys"
ssh root@<IP> "sync; md5sum /media/sdcard/fota.img; ls -lh /media/sdcard/fota.img"
```

设备侧必须得到：

```text
1cae3817a6e84db3fcfb00173ca4fc04  /media/sdcard/fota.img
```

MD5 或大小不一致立即停止。不要在 FAT/SD 卡仍写缓存时断电。

## 4. 标准升级流程

### 阶段 A：写非激活槽

1. 保持串口记录和稳定供电。
2. 进入 `./test_absys`。
3. 输入菜单 `0`。
4. 输入 `/media/sdcard/fota.img`。
5. 等待进度到 100%，看到 `Update in-active partition SUCCEED`。
6. 输入菜单 `1`，必须为：

```text
Current fota progress: 100
Current fota state: WRITEDONE
Current fota exit code: 0
```

写入期间禁止：重复菜单 0、菜单 2、菜单 3、普通 reboot、关机或断电。

### 阶段 B：切槽和首次启动

1. 只有 `WRITEDONE + exit code 0` 才输入菜单 `2`。
2. `test_absys` 将调用 switch 并重启；SSH/控制台中断是正常现象。
3. 从重启开始持续保存完整串口。预期会先由 OBM 引导新槽，然后在 R03 preinit 中
   检查/升级 OBM。
4. 串口若出现以下内容，绝对不能断电或复位：

```text
[QUECTEL] WARNING: OBM Upgrading... DO NOT POWEROFF OR REBOOT!!!
```

5. 等到看到 `OBM Upgrade success!` 并且系统完整进入 OpenWrt 登录，再做下一步。

若脚本判断当前 OBM MD5 已匹配，也可能只显示“OBM is up-to-date”，不实际写入。

### 阶段 C：新槽验收，暂不 sync

系统起来后先执行：

```sh
serial_atcmd AT+QGMR
cat /etc/quectel-project-version
cat /system/etc/mversion
ps | grep '[d]ial'
cat /tmp/network_status
ls -l /boot/quectel_obm*
cat /NVM/.fota_obm_state 2>/dev/null
dmesg | tail -200
```

再进入 `test_absys`，依次输入 `1`、`4`、`5`。典型成功中间态应为：

- `AT+QGMR`：`EC200ACNTAR03A02M2G_OCPU`
- 项目版本：`EC200ACNTAR03A02M2G_OCPU`
- mversion：包含 `rls988`
- 激活槽从 A 变为 B（如果起点是 A）
- FOTA 状态：通常为 `NEEDSYNC`
- 分区状态：`succeed`
- `/NVM/.fota_obm_state`：成功更新后应为 `1`
- `start_prog`、`/usr/dial/dial` 进程存在
- 网络、平台连接、CAN、GNSS、OTA、日志和业务功能正常
- 无 CP DUMP、verity 错误、反复重启或持续内核异常

此时禁止立刻菜单 3。至少完成规定的业务回归和观察期，保留旧槽作为回滚路径。

### 阶段 D：最终 sync

只有项目负责人确认新 R03 系统、OBM、网络和业务全部通过，且接受覆盖旧槽后，才执行：

1. `test_absys` 菜单 `3`。
2. 等待 `do AB sync succeed`。
3. 菜单 `1`：应为 `SUCCEED`、exit code 0。
4. 菜单 `4`：仍为新槽。
5. 菜单 `5`：`succeed`。
6. 再重启一次并复查版本和业务。

```text
SUCCEED
  └─菜单0→ UPDATE/BACKUP → WRITEDONE
                                └─菜单2→ reboot/R03/OBM → NEEDSYNC
                                                              └─验收通过→菜单3→ SUCCEED
```

## 5. 异常分支

### 5.1 菜单 0 立即失败或 exit code 非 0

- 不执行菜单 2 或 3。
- 保存完整输出、包 MD5、剩余空间、菜单 1/4/5 结果。
- 检查路径、SD 卡、供电、FOTA 服务和包是否损坏。
- 原因不清时不要反复菜单 0。

### 5.2 长时间停在 UPDATE/BACKUP

- 保持供电，禁止 reboot/switch/sync。
- 定期只用菜单 1 查询。
- 保存日志和卡住的百分比。
- 只有明确到 `WRITEDONE` 才可切槽。

### 5.3 WRITEDONE 但尚未菜单 2

设备仍运行旧槽，写好的新槽尚未启用。继续时先再次确认包 MD5和状态，再菜单 2；
普通 reboot 不能替代 switch。

### 5.4 菜单 2 后仍是原槽或 R02A04

可能是 switch 未生效，也可能是新槽启动失败后自动回滚。必须结合：

- 完整串口启动日志
- 菜单 1、4、5
- `AT+QGMR`
- `/NVM/.fota_obm_state`
- 是否出现 `Fatal Error Code`、verity、CP crash 或 reboot count

不要盲目再次菜单 2，避免重新进入失败槽。

### 5.5 再次出现 `IMAP Error Format / Fatal Error Code 0x00000505`

旧存量包在旧 OBM 引导 R03 B 槽时曾出现该错误。本包已加入移远要求的 OBM更新
链路，但真机是否消除该问题仍需实测。

- 禁止菜单 3。
- 保留从复位到错误和自动回滚的完整串口日志。
- 让 A/B 自身完成重试/回滚，不连续打断启动。
- 自动回到 R02 后，查菜单 1/4/5、版本、dial和网络。
- 无法回滚则停止远程尝试，使用 USB SWDownloader 恢复。

### 5.6 OBM 更新失败

脚本会写后读回并校验，最多尝试 3 次。看到失败或 MD5 不匹配时：

- 仍然保持供电，不人工重启。
- 保存 `[QUECTEL]` 全部输出和后续启动结果。
- 若系统还能起来，记录 `/NVM/.fota_obm_state` 和当前槽，不手工再次执行
  `ql_ota_obm`。
- 若不能启动，使用 SWDownloader；不要依赖 A/B sync 修复全局 OBM。

### 5.7 断电/复位

- 写非激活槽时中断：上电后只查菜单 1/4/5，不直接 switch。
- 切槽/启动时中断：先判断当前槽和是否自动回滚。
- OBM 写入警告期间中断：按可能损坏 OBM 处理，优先 SWDownloader，不反复上电碰运气。
- sync 时中断：上电查询槽状态和分区健康，不假设同步完成。

### 5.8 新 R03 启动，但业务异常

- 禁止菜单 3，保留旧槽。
- 收集进程、动态库、配置、启动脚本、网络、CAN/GNSS、云端、CP DUMP 和系统日志。
- 是否主动菜单 2 切回旧槽，必须同时考虑“OBM 已是新版本且为全局”的兼容风险；
  首次实验应在操作前与移远确认回退策略。
- 无法确认安全回退时，用已验证的 USB 线刷方案恢复。

### 5.9 自动回滚后 NEEDSYNC

只有确认已经回到健康 R02A04、激活槽正确、dial/网络正常、分区状态 succeed，才可考虑
菜单 3，用健康旧槽覆盖失败的新槽。这样会清除失败现场，必须先保存日志和征得负责人
同意。OBM 是全局区，菜单 3 不会把它恢复为旧版。

### 5.10 新槽 NEEDSYNC 且状态异常

绝对禁止菜单 3，否则可能把异常系统同步到另一槽并丢失回滚路径。保存现场，依靠自动
回滚或使用 USB 线刷。

### 5.11 `/NVM/.fota_obm_state=1` 但 OBM 未确认

脚本会因标志为 1 直接跳过，不会先比对目标 MD5。这不是推测，是脚本真实顺序。
不要擅自删标志或手动写 MTD；把状态、历史升级记录和串口 OBM 版本提供给移远确认。

### 5.12 分区状态损坏

菜单 5 非 `succeed` 时停止 switch/sync。记录当前槽、FOTA 状态、损坏槽、启动和 FOTA
日志；按移远恢复方法或 USB 线刷处理。

## 6. 验收表

| 检查项 | 方法 | 放行条件 |
|---|---|---|
| 包一致性 | 设备 `md5sum` | `1cae...fc04` |
| CP/项目版本 | `AT+QGMR`、project-version | R03A02 |
| OTAD | `/system/etc/mversion` | rls988 |
| 激活槽 | 菜单 4 | 已切到新槽 |
| FOTA 中间态 | 菜单 1 | 验收前 NEEDSYNC，sync 后 SUCCEED |
| 分区健康 | 菜单 5 | succeed |
| OBM | 串口、状态文件 | 无失败；成功或已匹配；state=1 |
| 启动 | 串口/dmesg | 无 fatal、verity、循环重启 |
| 核心应用 | `start_prog`、`dial` | 存在且稳定运行 |
| 网络/业务 | 实际功能测试 | 全部通过 |
| CP DUMP | 日志/持续观察 | 无新增 |
| 回滚预案 | USB SWDownloader | 已验证可执行 |

## 7. 单台记录模板

```text
设备编号/IMEI/IP：
硬件批次/Flash 型号：
操作人/日期：
升级包 MD5/SHA-256：
升级前 QGMR/project/mversion：
升级前 FOTA 状态/激活槽/分区健康：
升级前 OBM 串口版本/.fota_obm_state：
菜单 0 开始/结束时间、WRITEDONE 输出：
菜单 2 时间：
完整串口日志路径：
OBM 脚本输出和结果：
升级后 QGMR/project/mversion：
升级后 FOTA 状态/激活槽/分区健康：
dial/网络/CAN/GNSS/云端/OTA/日志验证：
CP DUMP/verity/fatal/reboot 异常：
观察时长：
是否批准菜单 3、批准人、时间：
菜单 3 及最终 SUCCEED 结果：
是否回滚/线刷、原因和最终状态：
```
