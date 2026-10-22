# EdgeVision 可观测性：日志 + 调试接口

目标：现场设备"黑箱"变"灰箱"——不连 gdb、不重编译，也能回答三件事：
**在跑吗？跑得动吗？看到了什么？**

---

## 1. 日志系统（`evs/log.h`）

### 1.1 级别与宏

| 级别 | 宏 | 用途 |
|---|---|---|
| Error | `EVS_LOGE` | 不可恢复：加载失败、推理失败 |
| Warn | `EVS_LOGW` | 可降级：超延时预算、相机不匹配、模型释放异常 |
| Info | `EVS_LOGI` | 生命周期：初始化/更新配置/释放 |
| Debug | `EVS_LOGD` | 复用模型、引用计数变化 |
| Trace | `EVS_LOGT` | 每帧一行（帧号/目标数/耗时），仅排障时开 |

格式：`2026-10-02 08:28:10.532 [WARN] vision_task.cpp:242 frame 210.4 ms > budget 200 ms ...`

### 1.2 三个开关（互不冲突）

| 开关 | 接口 | 说明 |
|---|---|---|
| 运行期级别 | `Logger::instance().set_level()` | 默认读环境变量 `EVS_LOG_LEVEL`（`error/warn/info/debug/trace` 或 0..5） |
| 编译期裁剪 | `-DEVS_LOG_LEVEL=3` | NDEBUG 下默认 3（Info），Trace/Debug 的格式化开销直接消失 |
| 输出去向 | `set_sink()` / `set_file()` | 宿主接 syslog/Qt/IPC；文件 sink 追加写；无 sink 时落 stderr |

```cpp
// 宿主接管（Qt 示例）
evs::Logger::instance().set_sink([](evs::LogLevel l, const std::string& line) {
    qDebug().noquote() << QString::fromStdString(line);
});
evs::Logger::instance().set_file("/var/log/evs.log");
```

> **坑**：sink 是全局持有的。若 lambda 按引用捕获局部对象，必须在其析构前
> `set_sink()`/`reset_sink()` 摘掉，否则后续日志会打到悬垂引用（ASan：stack-use-after-scope）。

### 1.3 环形缓冲（远程取证用）

```cpp
Logger::instance().set_ring_capacity(256);   // 默认 256 条，0=关闭
auto lines = Logger::instance().tail(20);    // 最近 20 条
```
`debugSnapshot()` / `library_snapshot()` 会把它们打包进 JSON，运维通过 IPC 拉取即可，
**不需要等现场复现**。

---

## 2. 调试接口（`evs/debug.h`）

### 2.1 配置（YAML `debug:` 段，可热更新）

| 字段 | 默认 | 说明 |
|---|---|---|
| `enabled` | false | 总开关；关时不缓存张量、不落盘 |
| `dump_input` | false | 保存模型输入张量为 P6 PPM |
| `dump_meta` | true | 同帧 JSON 侧车（框/预处理映射/状态） |
| `dump_on_alarm_only` | true | 只存告警帧 |
| `dump_every_n` | 0 | 0=不按周期；1=每帧（排障专用） |
| `dump_dir` | /tmp/evs_debug | 自动创建 |
| `max_dumps` | 100 | 安全阀，写满即停 |

### 2.2 VisionTask 上的 4 个调试 API（不改动既有 4 个主 API）

| 接口 | 返回 | 用途 |
|---|---|---|
| `debugStats()` | `DebugStats` | 帧数/错误数/告警数/超预算数；pre·infer·decode·total 的 last/avg/p95/min/max |
| `debugSnapshot()` | JSON 字符串 | 配置 + 计数器 + 耗时 + 最近日志，一条 IPC 全带走 |
| `setDebugConfig(cfg)` | void | 热开/关、改落盘目录，无需重启 |
| `saveLastInput(path)` | bool | 把最后一帧的模型输入存成 PPM（看模型"到底看到了什么"） |

### 2.3 全局快照

```cpp
std::string s = evs::library_snapshot(20);
// {"version":"0.3.0-scaffold","backend":"mock","log_level":"DEBUG",
//  "models":[{"backend":"mock","model":"models/person.rknn","ref":1,"loaded":true}],
//  "log":["...最近 20 行..."]}
```

### 2.4 落盘文件

- `seq000123_1696200000_Alarm.ppm` —— 模型看到的图（含 letterbox 灰边）
- `seq000123_1696200000_Alarm.json` —— 原始尺寸、预处理映射、目标框（原图相对坐标）、状态

---

## 3. 现场排障 runbook

1. **在跑吗** → `debugStats().frames` 是否在涨、`errors` 是否为 0。
2. **跑得动吗** → `ms.p95` 对比 `latency_budget_ms`；超预算会自动 `EVS_LOGW` 并计入 `over_budget`。
3. **看到了什么** → `setDebugConfig({enabled, dump_input, dump_on_alarm_only})`
   或 `saveLastInput("/tmp/a.ppm")`，把 PPM 拉回来比对。
4. **模型/上下文是否泄漏** → `library_snapshot()` 看 `models[].ref`。
5. **要不要更细** → 提高 `EVS_LOG_LEVEL`（或重编时给 `-DEVS_LOG_LEVEL=5` 打开 Trace 每帧日志）。

---

## 4. 设计约束

- 库不独占任何端口/线程：日志 sink 与快照都由宿主取用。
- 调试关闭时零额外开销：不缓存张量、不拷贝、不排序（p95 每 8 帧重算一次）。
- 日志可在编译期完全裁掉（`-DEVS_LOG_LEVEL=0`），头文件里所有宏退化为 `if(0)`。
