# 会话总结：屏幕亮度掉电保持 & 亮度映射优化

日期：2026-05-26
涉及文件：`vehicles/forklift/SkesQtBridge.cpp`

---

## 一、totalWorkHours / totalMileage 修正命令安全性确认

**问题**：修正命令（设置总工时/总里程基准值）是否会干扰正常的计时/计程逻辑？

**结论**：完全安全。

**原理**：
- 修正命令只修改持久化 JSON 中的 base 值（累计基准）
- delta 的累加由独立的 libev 定时器周期驱动，与 base 无关
- 修正后下次定时器触发时，`new_total = base + delta` 自然生效
- 两套逻辑互不干涉，修正不影响计时节奏

---

## 二、屏幕亮度设置无明显视觉变化

**问题**：执行 `set_brightness 80` 后屏幕无明显变化。

**根因**：原实现 `BACKLIGHT_HW_MIN = 50`，线性映射下：
- 50% → hw = 152
- 80% → hw = 202
- 差距仅 50/255，视觉几乎感知不到

### 修复一：降低 HW_MIN，拓宽物理范围

在设备上实测：hw=1 / hw=5 / hw=10 视觉效果相近（极低亮度区分不出），
将 `BACKLIGHT_HW_MIN` 从 **50 → 5**，低亮度可感知范围大幅扩宽。

```cpp
// 修改前
#define BACKLIGHT_HW_MIN  50

// 修改后
#define BACKLIGHT_HW_MIN   5    // sysfs 最低值（实测1/5/10效果相近，留余量取5）
#define BACKLIGHT_HW_MAX 255
```

### 修复二：gamma=2 感知曲线，替代线性映射

**线性映射的问题**：人眼对低亮度敏感、对高亮度不敏感（韦伯定律），
线性步进在低档位感知差异极小，在高档位感知差异也压缩。

**gamma=2 幂次曲线**：
- 公式：`hw = HW_MIN + t² × (HW_MAX - HW_MIN)`，其中 `t = pct / 100.0`
- 低档位：t 小，t² 更小，步进被放大 → 低亮度变化更明显
- 高档位：t 接近 1，t² 接近 1，步进被压缩 → 高亮度平滑过渡
- 视觉感知更均匀

```cpp
// 修改前（线性）
static int brightnessToHw(int pct) {
    return BACKLIGHT_HW_MIN + pct * (BACKLIGHT_HW_MAX - BACKLIGHT_HW_MIN) / 100;
}

// 修改后（gamma=2）
static int brightnessToHw(int pct) {
    if (pct <= 0)   return BACKLIGHT_HW_MIN;
    if (pct >= 100) return BACKLIGHT_HW_MAX;
    double t = pct / 100.0;
    return BACKLIGHT_HW_MIN + (int)(t * t * (BACKLIGHT_HW_MAX - BACKLIGHT_HW_MIN));
}
```

### 默认亮度调整：50 → 75

gamma=2 曲线下，视觉等效原来 50% 线性亮度（hw≈152）的百分比约为 75%（hw≈145），
将默认值从 50 改为 75，保持用户升级前后视觉一致。

```cpp
// loadPersistent() 中
s_settings[SET_BRIGHTNESS] = root.value("screenBrightness", 75);  // 原为 50
```

---

## 三、屏幕亮度掉电不保持

**问题**：设置亮度 10%，持久化 JSON 已写入 `screenBrightness: 10`，
但重启后屏幕亮度恢复为硬件默认值（约 50%）。

### 根本原因：RK3576 背光驱动初始化时序竞争

```
vehicleDataEngine 启动
    └─ loadPersistent() → applyBrightness(10) → 写 sysfs  ← 此时驱动未就绪，写入被静默丢弃
                                                              （节点存在但 display pipeline 未完成初始化）

RK3576 display pipeline 初始化完成
    └─ 背光驱动将亮度重置为硬件默认值                        ← 覆盖了之前的写入
```

关键特征：
- sysfs 节点早期存在，写操作不报错，但硬件未响应
- 驱动完成初始化的时间不固定（实测 3 秒 ～ 8 秒以上）
- 初始化完成后运行时稳定，`echo` 写入立即生效且不会再被覆盖

### 方案演进

**方案一：构造函数写一次**
```
结果：驱动未就绪，写入静默丢失 ✗
```

**方案二：5 秒单次 ev_timer**
```
结果：驱动有时超过 5 秒才就绪，timer 触发时仍然失败，且无重试 ✗
```

**方案三：读回验证 + 重试**
```cpp
// 写入后立即读回，若值一致则认为成功，失败则重试
int written = brightnessToHw(pct);
FILE* f = fopen(BACKLIGHT_SYSFS, "r");
fscanf(f, "%d", &actual);
return (actual == written);  // 成功则停止重试
```
```
结果：sysfs 读回成功（内核层面已接受），但驱动初始化完成后仍会把背光重置，
读回无法捕捉"写入成功之后的复位"，导致提前停止重试 ✗
```

**方案三-b：连续 N 次读回成功才停止**
```
思路：连续 3 次读回与写入值一致才认为稳定
结果：驱动复位发生在两次读回之间，仍然会漏掉，依然失败 ✗
```

**方案四（最终方案）：暴力持续写入 30 秒**
```
思路：不依赖读回验证，在 start() 里每 2 秒无条件写一次，持续 30 秒（共 15 次）。
      驱动无论何时完成初始化、无论复位几次，下一个 2 秒写入都会覆盖回正确值。
      30 秒后驱动已完全稳定，不再需要持续写入。
验证：设备上 echo 写入后等待 8 秒，值保持不变，确认运行时不再复位。✓
```

### 最终实现

```cpp
// SkesQtBridge::start() 中，注册 ev_io 和 pub_timer 之后

static ev_timer s_brightness_timer;
static int s_brightness_retry = 0;
s_brightness_retry = 0;   // start() 可能被多次调用，显式归零防止计数累加
ev_timer_init(&s_brightness_timer,
    [](struct ev_loop* l, ev_timer* w, int) {
        applyBrightness(s_settings[SET_BRIGHTNESS]);
        ++s_brightness_retry;
        if (s_brightness_retry >= 15) {
            ev_timer_stop(l, w);
            std::printf("[SkesQtBridge] 亮度写入完成 (持续 30s)\n");
        }
    }, 2.0, 2.0);
ev_timer_start(loop, &s_brightness_timer);
```

**实现要点**：
- libev timer 回调必须是无捕获 lambda（可隐式转换为 C 函数指针）
- 直接写 `[](struct ev_loop* l, ev_timer* w, int){...}` 形式，不能用 `static auto cb = [...]` 赋值（会编译报错）
- `s_brightness_retry` 声明为 `static` 但每次 `start()` 时显式归零

---

## 四、日志噪声问题

**问题**：`applyBrightness` 每次调用都打印日志，30 秒内产生 15 条重复日志，干扰调试输出。

**修复**：将日志从高频调用路径移出，只在语义明确的两处各打一次：

| 位置 | 日志内容 | 触发时机 |
|------|----------|----------|
| `loadPersistent()` | `亮度恢复: XX% (hw=YY)` | 启动时从持久化恢复 |
| retry timer 结束 | `亮度写入完成 (持续 30s)` | 30 秒写入周期结束 |

`applyBrightness()` 本身不打任何日志。

---

## 五、本次会话末段：代码 Review

对 `SkesQtBridge.cpp` 所有改动进行逐段核查：

- **line 571 注释遗留旧值**：`0% → 硬件最低(50/255)` → 修正为 `(5/255)`（HW_MIN 已改为 5）
- **`<cerrno>` include**：read-back 方案删除后遗留，`errno` 已无引用，无害但属冗余，保留不影响编译

---

## 六、最终改动汇总

| 改动点 | 改动前 | 改动后 |
|--------|--------|--------|
| `BACKLIGHT_HW_MIN` | 50 | 5 |
| `brightnessToHw()` | 线性映射 | gamma=2 幂次曲线 |
| `applyBrightness()` | `int` 返回值，含读回和日志 | `void`，静默写入 |
| `loadPersistent()` 默认亮度 | 50 | 75（视觉等效） |
| `loadPersistent()` 初始写入 | 无 | applyBrightness + 一条日志 |
| `screenBrightness` handler 注释 | `(50/255)` | `(5/255)` |
| `start()` 亮度 retry timer | 无 | 每 2 秒写一次，持续 30 秒 |

---

## 七、提交说明

```
skes-dataengine: [BUGFIX] 修复屏幕亮度掉电不保持问题；优化亮度映射范围与感知曲线
```

**展开说明**：
- RK3576 背光驱动初始化期间会覆盖 sysfs 写入，改为 start() 里每 2 秒持续写入 30 秒
- `BACKLIGHT_HW_MIN` 从 50 降至 5，低档可感知范围扩宽
- 引入 gamma=2 幂次映射，低档位变化更明显、高档位平滑过渡
- 默认亮度从 50% 调整为 75%，与改曲线前视觉亮度保持等效
