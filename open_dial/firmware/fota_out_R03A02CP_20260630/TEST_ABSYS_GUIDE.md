# test_absys 完整使用与异常分支指南

> 适用于 `存量_FOTA_test_absys/quectel_AB_OTA.img`。当前包仅完成结构验证，
> 尚未真机验证；第一次必须在可用 USB SWDownloader 救砖的实验设备执行。

## 菜单

| 菜单 | 作用 | 使用条件 |
|---|---|---|
| `0` | 写 FOTA 包到非激活槽 | 开始升级 |
| `1` | 查询 FOTA 状态 | 全程只读检查 |
| `2` | switch，设置切槽标志并重启 | 仅 `WRITEDONE` 后 |
| `3` | sync，用当前槽同步另一槽 | 仅当前系统健康且 `NEEDSYNC` 时 |
| `4` | 查询激活槽 | 升级前后记录 |
| `5` | 查询分区健康状态 | 升级前后记录 |
| `-1` | 退出 | — |

不要使用旧版裸 `fota_update`：它缺少 switch 和重启后的 sync。

## 升级前

1. 验证实验设备的 USB 线刷救砖能力，保持稳定供电并连接串口。
2. 上传并校验：

```bash
scp 存量_FOTA_test_absys/quectel_AB_OTA.img root@<IP>:/media/sdcard/fota.img
scp test_absys root@<IP>:/data/test_absys
ssh root@<IP> "chmod +x /data/test_absys"
ssh root@<IP> "md5sum /media/sdcard/fota.img"
```

预期 MD5：`ff2260ba030173cccd5f880680d93392`。不一致则停止。

3. 记录基线：

```bash
ssh root@<IP> "serial_atcmd at+qgmr; cat /etc/quectel-project-version"
ssh -t root@<IP> /data/test_absys
```

依次输入 `1`、`4`、`5`。正常应为 `SUCCEED`、某一激活槽、
`succeed/is_damaged=0`。升级前若是 `NEEDSYNC`，须确认当前系统健康后先菜单 `3`
收尾；其他中间/失败状态按后文处理。

## 标准流程

```bash
ssh root@<IP> "kill -SIGINT \$(pgrep -x dial)"
ssh -t root@<IP> /data/test_absys
```

1. 输入 `0`，再输入 `/media/sdcard/fota.img`。
2. 等待写完。期间不退出、不重启/断电、不重复 `0`，不执行 `2`/`3`。
3. 输入 `1`，只有显示 `WRITEDONE` 才继续。
4. 输入 `2`，调用 `ql_absys_switch()` 设置切槽标志并重启。SSH 断开正常。
   普通 reboot 不等价于菜单 `2`。
5. 重启后检查：

```bash
ssh root@<IP> \
  "serial_atcmd at+qgmr; cat /etc/quectel-project-version; cat /tmp/network_status"
ssh -t root@<IP> /data/test_absys
```

6. 输入 `1`（预期 `NEEDSYNC`）、`4`（预期已换槽）、`5`（预期健康）。
7. 确认系统稳定、CP 为 R03A02/rls988、dial/网络正常且无新 CP DUMP。
8. 仅在上述检查通过后输入 `3`，然后用 `1`、`4`、`5` 验证：
   `SUCCEED`、仍为新槽、分区健康。

```text
SUCCEED → UPDATE/BACKUP → WRITEDONE
                              └─菜单2→ reboot → NEEDSYNC ─菜单3→ SUCCEED
```

sync 会用当前槽覆盖另一槽；新系统未确认健康时禁止提前 sync。

## 异常分支

### 菜单 0 立即失败/API 非 0

不执行 `2`。保存返回值和输出，检查路径、权限、空间、MD5；用 `1`、`4`、`5`
记录状态。原因未清前不反复重试。

### 长时间 UPDATE/BACKUP

保持供电，不 switch、sync 或 reboot；保存 SSH/串口输出，用 `1` 观察进度。
只有 `WRITEDONE` 才允许菜单 `2`。

### FAILED

不执行 `2`。记录 `exit_code`、日志、槽位及健康状态；检查 MD5、空间、供电和 FOTA
服务。由研发/移远确认清理方法后再重试，不盲目 sync。

### WRITEDONE 但未 switch

当前仍运行旧槽。继续时再次确认 `WRITEDONE` 后执行 `2`；暂停则保留现场。
不要用普通 reboot 代替 `2`。

### 菜单 2 后仍是原槽/旧版本

用 `1`、`4`、`5`、`AT+QGMR` 收集证据，保存启动日志，判断 switch 未生效还是自动
回滚。不要盲目再次输入 `2`。

### 新槽卡 OBM、CP crash、verity 或反复重启

早期 cpimage-only 包曾出现此分支；当前包虽不同但仍不含 OBM。

1. 禁止 sync。
2. 保存完整串口日志。
3. 给 A/B 自行重试/回滚机会，不连续打断。
4. 必要时彻底断电再上电，观察是否回滚原槽。
5. 无法回滚则停止远程操作，使用 USB SWDownloader 救砖。

### 自动回滚后 NEEDSYNC

确认 `4` 已回原槽、`AT+QGMR` 已回原版本、原系统/dial/网络正常且 `5` 健康。
此时可输入 `3`，用好的原槽覆盖失败槽；再查 `1`=`SUCCEED`、`4`=原槽、
`5`=健康。**回滚后禁止再次输入 `2`**，否则可能重新进入失败槽。

### NEEDSYNC，但新系统异常

禁止 `3`，避免把异常系统复制到另一槽。保存日志并依靠 A/B 回滚；主动恢复须按移远
明确指导，无法恢复则 USB 线刷。

### 重启后直接 SUCCEED

仍需检查 `4`、`5`、版本、dial、网络和 CP DUMP。版本或槽位不符则停止扩面。

### is_damaged 非 0

停止 switch/sync，记录激活槽、FOTA 状态、损坏槽和日志；保持当前系统，向移远确认
恢复方法，或实验室 USB 线刷。

### 过程中断电

- 写入阶段：上电先查 `1`、`4`、`5`，不直接重刷/switch。
- switch/启动阶段：判断进入新槽还是自动回滚。
- sync 阶段：上电查询状态和分区健康，不假定同步完成。

### test_absys 无法执行

```bash
ls -l /data/test_absys
file /data/test_absys
ldd /data/test_absys
```

检查权限、架构及 `libql_lib_absys.so`/`libql_lib_fota.so`。不要用裸 reboot 或写
Flash 命令拼凑流程。

## 验收

| 检查项 | 方法 | 正常结果 |
|---|---|---|
| FOTA | 菜单 `1` | `SUCCEED` |
| 激活槽 | 菜单 `4` | 正常升级后换槽；回滚后为原槽 |
| 分区 | 菜单 `5` | `succeed/is_damaged=0` |
| CP | `serial_atcmd at+qgmr` | R03A02/rls988；回滚时 R02A04 |
| 应用 | dial、网络、日志 | 正常 |
| 目标问题 | 持续观察 | 不再发生 CP DUMP |

## 单台设备记录模板

```text
设备/IP/操作人/时间：
升级包及 MD5：
升级前版本、槽位、FOTA 状态、分区健康：
菜单 0 时间及 WRITEDONE 日志：
菜单 2 时间及重启结果：
升级后版本、槽位、FOTA 状态：
菜单 3 结果及最终分区健康：
dial/网络/CP DUMP：
是否回滚：
异常日志路径及最终结论：
```

## 已知风险

1. CP R03A02 + AP R02A04 的跨基线兼容性仍需移远书面确认。
2. 本包不含 OBM；是否必须随 R02→R03 同升尚未闭环。
3. 非激活槽 dm-verity 尚未真机验证。
4. 结构验证通过不等于真机验证通过。

依据见 `../../session/session_fota_cpimage_packaging_20260623.md` §17、§18、§22，
以及本目录 `README.md`、`MANIFEST.md`。
