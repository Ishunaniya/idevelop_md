# 叉车工时、里程专用 IPC

## 目的与边界

`Nu0011`（点位 59，累计工时）和 `Nu0012`（点位 60，累计里程）仍由 Stub 原有的 10 秒 DTP/SCP 定时任务上报。二者的权威数据源改为 SKES 的本地业务数据端点：`ipc:///tmp/skes-workmeter.ipc`。

这条新链路与 CAN 传输明确隔离：

```text
SKES 0x31C -> PUB tcp://127.0.0.1:26002 -> io_mng -> SocketCAN can0 -> 实车 CAN/模拟器
实车 CAN -> SocketCAN can0 -> io_mng -> PUB tcp://127.0.0.1:16002 -> Stub 点表

SKES 累计工时、里程 -> PUSH ipc:///tmp/skes-workmeter.ipc -> Stub -> Nu0011/Nu0012 -> DTP/SCP
```

现有 `io_mng` 不会把 26002 的 CAN 发送输入镜像到 16002 的 CAN 接收输出。因此本 IPC 不会发送任何额外 CAN 帧，也不修改 26002、16002 或 io_mng。

## 端点归属与报文

Stub 是 IPC 路径的唯一所有者：启动绑定 `NN_PULL` 前清理遗留的 `/tmp/skes-workmeter.ipc`，退出释放时删除该路径。SKES 仅以 `NN_PUSH` 连接，绝不删除路径。

SKES 启动后立即发送；工时或里程变化时立即发送；无变化时每 10 秒发送一次保活。报文为：

```json
{"version":1,"session":"UUID","seq":42,"work_hours":123,"mileage":456}
```

`work_hours`、`mileage` 是与 CAN `0x31C` 完全相同的 uint24 原始累计值，取值范围为 `[0, 0xFFFFFF]`。`session` 每次 SKES 初始化都会改变；同一 session 内 `seq` 必须递增。Stub 会拒绝格式错误、超范围和乱序报文；SKES 重启后的新 session 即使序号从 1 开始也会被接受。

Stub 将超过 30 秒未更新的数据视为失效。专用数据有效时，Stub 会移除点表可能产生的 59、60 后再写入专用值，确保同一份 DTP 不会出现重复点位。

## 设备验收

先开启 Stub 日志并重启服务：

```sh
cat >/tmp/stub-forklift.ini <<'EOF'
[log]
level=2
EOF
# 使用设备现有的服务/启动脚本重启 stub-forklift
```

也可不停服务切换等级。每次 `SIGUSR1` 使等级循环加一；连续发送并观察 `logger level change to 2` 即可：

```sh
pid="$(pidof stub-forklift)"
kill -USR1 "$pid"
tail -f /tmp/stub-forklift.log
```

验收命令：

```sh
grep -E 'received SKES workmeter|DTP includes SKES workmeter' /tmp/stub-forklift.log
```

应先出现 `received SKES workmeter`，随后在常规 10 秒 DTP 周期内出现 `DTP includes SKES workmeter`。DTP JSON 应包含 `"index":59,"name":"Nu0011"` 和 `"index":60,"name":"Nu0012"`。

平台最终落库还依赖有效整车编号。已有的 `vehicle code is empty` 与本 IPC 数据链路无关，但未解决前不能据平台页面断言本次数据上报已完成。

