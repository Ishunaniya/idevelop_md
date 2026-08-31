# 日志分级实现分析报告

> 分析日期：2026-04-28  
> 涉及仓库：
> - **A仓库**：`/home/tronlong/lyp/code/open_dial_for_artery`（feature/dual_channel_switch 分支）
> - **B仓库**：`/home/tronlong/lyp/ntrk_code/open_dial`

---

## 一、A仓库（open_dial_for_artery）日志分级实现

### 1.1 宏定义原文

文件：`src/seas_log/seas_log.h`

```c
/* Level enum */
#define SEAS_LEVEL_DEBUG 0
#define SEAS_LEVEL_INFO 1
#define SEAS_LEVEL_NOTICE 2
#define SEAS_LEVEL_WARNING 3
#define SEAS_LEVEL_ERROR 4
#define SEAS_LEVEL_CRITICAL 5
#define SEAS_LEVEL_ALL 6
#define SEAS_LEVEL_SILENT 7

/* Color customization */
#define SEAS_COLOR_DEBUG "\x1B[34;1m"
#define SEAS_COLOR_INFO "\x1B[36m"
#define SEAS_COLOR_NOTICE "\x1B[32;1m"
#define SEAS_COLOR_WARNING "\x1B[33m"
#define SEAS_COLOR_ERROR "\x1B[31m"
#define SEAS_COLOR_CRITICAL "\x1B[41;1m"
#define SEAS_COLOR_ALL "\x1B[35;1m"

#define SEAS_EMIT_LOG(color, level, fmt, ...)                          \
    do                                                                 \
    {                                                                  \
        seas_emit_log(color, level, __FILE__, __func__, __LINE__, fmt, \
                      ##__VA_ARGS__);                                  \
    } while (0)
#define ENABLE_SEAS_LOG
#ifdef ENABLE_SEAS_LOG
#define SEAS_LOG_ALL(fmt, ...)                                          \
    do                                                                  \
    {                                                                   \
        if (seas_log_get_level() <= SEAS_LEVEL_ALL)                     \
        {                                                               \
            SEAS_EMIT_LOG(SEAS_COLOR_ALL, "[ALL]", fmt, ##__VA_ARGS__); \
        }                                                               \
    } while (0)
#define SEAS_LOG_DEBUG(fmt, ...)                                            \
    do                                                                      \
    {                                                                       \
        if (seas_log_get_level() <= SEAS_LEVEL_DEBUG)                       \
        {                                                                   \
            SEAS_EMIT_LOG(SEAS_COLOR_DEBUG, "[DEBUG]", fmt, ##__VA_ARGS__); \
        }                                                                   \
    } while (0)
#define SEAS_LOG_INFO(fmt, ...)                                           \
    do                                                                    \
    {                                                                     \
        if (seas_log_get_level() <= SEAS_LEVEL_INFO)                      \
        {                                                                 \
            SEAS_EMIT_LOG(SEAS_COLOR_INFO, "[INFO]", fmt, ##__VA_ARGS__); \
        }                                                                 \
    } while (0)
#define SEAS_LOG_NOTICE(fmt, ...)                                             \
    do                                                                        \
    {                                                                         \
        if (seas_log_get_level() <= SEAS_LEVEL_NOTICE)                        \
        {                                                                     \
            SEAS_EMIT_LOG(SEAS_COLOR_NOTICE, "[NOTICE]", fmt, ##__VA_ARGS__); \
        }                                                                     \
    } while (0)
#define SEAS_LOG_WARNING(fmt, ...)                                              \
    do                                                                          \
    {                                                                           \
        if (seas_log_get_level() <= SEAS_LEVEL_WARNING)                         \
        {                                                                       \
            SEAS_EMIT_LOG(SEAS_COLOR_WARNING, "[WARNING]", fmt, ##__VA_ARGS__); \
        }                                                                       \
    } while (0)
#define SEAS_LOG_ERROR(fmt, ...)                                            \
    do                                                                      \
    {                                                                       \
        if (seas_log_get_level() <= SEAS_LEVEL_ERROR)                       \
        {                                                                   \
            SEAS_EMIT_LOG(SEAS_COLOR_ERROR, "[ERROR]", fmt, ##__VA_ARGS__); \
        }                                                                   \
    } while (0)
#define SEAS_LOG_FATAL(fmt, ...)                                            \
    do                                                                      \
    {                                                                       \
        if (seas_log_get_level() <= SEAS_LEVEL_FATAL)                       \
        {                                                                   \
            SEAS_EMIT_LOG(SEAS_COLOR_FATAL, "[FATAL]", fmt, ##__VA_ARGS__); \
        }                                                                   \
    } while (0)
#define SEAS_LOG_ENABLED(level) (seas_log_get_level() <= (SEAS_LEVEL_##level))
#else
#define SEAS_LOG_ALL(fmt, ...)
#define SEAS_LOG_DEBUG(fmt, ...)
#define SEAS_LOG_INFO(fmt, ...)
#define SEAS_LOG_NOTICE(fmt, ...)
#define SEAS_LOG_WARNING(fmt, ...)
#define SEAS_LOG_ERROR(fmt, ...)
#define SEAS_LOG_FATAL(fmt, ...)
#define SEAS_LOG_ENABLED(level) (0)
#endif
```

### 1.2 调用方式

在各源文件中统一使用 `SEAS_LOG_级别(fmt, ...)` 格式：

```c
/* main.c */
SEAS_LOG_INFO("DIAL Version: %d.%02d.%02d\r\n", MAIN_VERSION, SUB_VERSION, TEST_VERSION);

/* src/dial/dial.c */
SEAS_LOG_ERROR("dial_mng_new: calloc failed");
SEAS_LOG_INFO("dial_mng_new: policy=%d (SIM), stopping Roamlink service first",
              p_dial_mng->network_select);
SEAS_LOG_DEBUG("the profile index %d start data call success", data_call.profile_idx);

/* src/roamlink/roamlink.c */
SEAS_LOG_ERROR("roamlink: RBMaster not found at %s → NO_PACKAGE", ROAMLINK_MASTER_PATH);
SEAS_LOG_INFO("roamlink: probe OK (RBMaster + conf.ini + license valid, size=%lld bytes)",
              (long long)st.st_size);
```

实际使用频率：`SEAS_LOG_INFO` 和 `SEAS_LOG_ERROR` 占绝大多数，`SEAS_LOG_DEBUG` 少量，`SEAS_LOG_NOTICE` / `SEAS_LOG_WARNING` / `SEAS_LOG_FATAL` **在全仓库中均未实际调用**。

---

## 二、B仓库（ntrk_code/open_dial）日志分级实现

### 2.1 Android 兼容宏（来自 SDK log.h）

文件：`_public.h`（配置部分）

```c
#define LOG_TAG "DIAL"
#define LOG_NDEBUG 1

#include "include/log.h"
```

文件：`SDK/.../include/log.h`（核心宏）

```c
/* ALOGV — VERBOSE，LOG_NDEBUG=1 时完全禁用 */
#ifndef ALOGV
#if LOG_NDEBUG
#define ALOGV(...)   ((void)0)
#else
#define ALOGV(...) ((void)ALOG(LOG_VERBOSE, LOG_TAG, __VA_ARGS__))
#endif
#endif

/* ALOGD — DEBUG */
#ifndef ALOGD
#define ALOGD(...) ((void)ALOG(LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#endif

/* ALOGI — INFO */
#ifndef ALOGI
#define ALOGI(...) ((void)ALOG(LOG_INFO, LOG_TAG, __VA_ARGS__))
#endif

/* ALOGW — WARN */
#ifndef ALOGW
#define ALOGW(...) ((void)ALOG(LOG_WARN, LOG_TAG, __VA_ARGS__))
#endif

/* ALOGE — ERROR */
#ifndef ALOGE
#define ALOGE(...) ((void)ALOG(LOG_ERROR, LOG_TAG, __VA_ARGS__))
#endif

/* 向后兼容别名 */
#ifndef LOGD
#define LOGD ALOGD
#endif
#ifndef LOGI
#define LOGI ALOGI
#endif
#ifndef LOGW
#define LOGW ALOGW
#endif
#ifndef LOGE
#define LOGE ALOGE
#endif
```

### 2.2 自定义 dial_log() 函数

文件：`logger_sd.c`

```c
void dial_log(const char *fmt, ...) {
    va_list args;
    char time_str[32];
    time_t now;
    time(&now);
    struct tm *t = localtime(&now);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    printf("[%s] ", time_str);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    if (g_log_fp) {
        fprintf(g_log_fp, "[%s] ", time_str);
        va_start(args, fmt);
        vfprintf(g_log_fp, fmt, args);
        va_end(args);
        fflush(g_log_fp);
    }
}
```

### 2.3 L1/L2/L3 的实际含义

**B仓库中不存在 L1/L2/L3 宏定义。** L1/L2/L3 是嵌在 `dial_log()` 字符串中的语义标签，表示三级故障恢复策略：

文件：`dial.c`

```c
/* L1 — 软恢复：重启数据呼叫 */
dial_log("[RECOVERY L1] Skip: REG not ready (REG:%d), waiting for L2.\n", g_last_reg_stat);
dial_log("[RECOVERY L1] Stopping Data Call & Cleaning PDP...\n");
dial_log("[RECOVERY L1] Restarting Data Call...\n");

/* L2 — 中级恢复：射频重置（飞行模式切换） */
dial_log("[RECOVERY L2] Toggling RF (Airplane Mode)...\n");
system("serial_atcmd at+cfun=0");
sleep(3);
system("serial_atcmd at+cfun=1");
dial_log("[RECOVERY L2] RF ON. Waiting 10s for registration...\n");

/* L3 — 硬恢复：模组完整重启 */
dial_log("[RECOVERY L3] FATAL: Network down 30mins. Sending AT+CFUN=1,1 then exit.\n");
system("serial_atcmd at+cfun=1,1");
sleep(20);
dial_log("[RECOVERY L3] Exiting. start_prog will reinitialize.\n");
exit(1);
```

---

## 三、差异对比表

| 维度 | A仓库（open_dial_for_artery） | B仓库（ntrk_code/open_dial） |
|------|-------------------------------|------------------------------|
| **日志框架** | 自研 seas_log（irootech/ROOTCLOUD） | Android ALOG* 宏 + 自定义 dial_log() |
| **级别定义方式** | `#define SEAS_LEVEL_XXX N` 数值宏 | Android log.h 枚举（LOG_VERBOSE/DEBUG/INFO/WARN/ERROR） |
| **级别数量** | 定义了8个（DEBUG/INFO/NOTICE/WARNING/ERROR/CRITICAL/ALL/SILENT） | Android 5级（VERBOSE/DEBUG/INFO/WARN/ERROR） |
| **调用形式** | `SEAS_LOG_INFO(fmt, ...)` | `ALOGI(fmt, ...)` / `LOGD(fmt, ...)` / `dial_log(fmt, ...)` |
| **输出目标** | stdout（终端） + 可选 SD 卡轮转文件 | stdout/logcat + SD 卡文件（dial_log） |
| **运行时级别过滤** | `seas_log_get_level()` 函数，运行时可调 | LOG_NDEBUG 编译时静态控制（VERBOSE 被完全禁用） |
| **编译时开关** | `#define ENABLE_SEAS_LOG` 硬编码 always-on，无法关闭 | `#if LOG_NDEBUG` 可在编译时精确控制 VERBOSE |
| **L1/L2/L3** | **不存在**此概念，恢复事件用 INFO/ERROR 级别记录 | L1/L2/L3 是 dial_log 字符串中的文本标签，非宏，代表三级恢复策略 |
| **时间戳** | seas_emit_log() 内生成，精确到秒（含文件名/函数名/行号） | dial_log() 生成时间戳；ALOG* 依赖 Android logd |
| **NOTICE/WARNING 级别** | 宏已定义，但全仓库无一处调用 | 无 NOTICE；WARN 通过 ALOGW 调用 |
| **FATAL 级别** | 宏 SEAS_LOG_FATAL 存在但引用未定义的 SEAS_LEVEL_FATAL | 无 FATAL 宏，L3 恢复直接调用 exit(1) |

---

## 四、问题结论

### 问题1（严重）：SEAS_LOG_FATAL 引用未定义标识符，调用即编译报错

`seas_log.h` 第114-121行定义了 `SEAS_LOG_FATAL` 宏，但宏体内引用了 `SEAS_LEVEL_FATAL` 和 `SEAS_COLOR_FATAL`，而这两个标识符在同文件及整个仓库中均未定义：

```c
/* 宏体中引用的 SEAS_LEVEL_FATAL 和 SEAS_COLOR_FATAL 均不存在 */
#define SEAS_LOG_FATAL(fmt, ...)                                            \
    do                                                                      \
    {                                                                       \
        if (seas_log_get_level() <= SEAS_LEVEL_FATAL)   /* ← 未定义 */     \
        {                                                                   \
            SEAS_EMIT_LOG(SEAS_COLOR_FATAL, "[FATAL]", fmt, ##__VA_ARGS__); /* ← 未定义 */ \
        }                                                                   \
    } while (0)
```

已定义的最高业务级别是 `SEAS_LEVEL_CRITICAL=5`，对应颜色宏 `SEAS_COLOR_CRITICAL` 已定义，但没有配套的 `SEAS_LOG_CRITICAL` 宏。即：级别枚举和颜色宏、调用宏三者之间存在不一致：
- `SEAS_LEVEL_CRITICAL` 有定义，`SEAS_COLOR_CRITICAL` 有定义，但 **`SEAS_LOG_CRITICAL` 宏不存在**
- `SEAS_LOG_FATAL` 宏存在，但 `SEAS_LEVEL_FATAL` 和 `SEAS_COLOR_FATAL` **均不存在**

当前代码中 `SEAS_LOG_FATAL` 从未被调用，因此未触发编译错误，属于隐藏的潜在炸弹。

### 问题2（中等）：ENABLE_SEAS_LOG 硬编码，无法编译时关闭

```c
#define ENABLE_SEAS_LOG   /* 第64行：硬编码，无条件激活 */
#ifdef ENABLE_SEAS_LOG
...
#endif
```

`#define ENABLE_SEAS_LOG` 紧靠在 `#ifdef` 之前，导致条件编译保护形同虚设，无法通过 Makefile 传入 `-DDISABLE_SEAS_LOG` 等方式关闭。B仓库的 `LOG_NDEBUG` 方式更规范。

### 问题3（低）：NOTICE/WARNING 级别定义但无实际用途

全仓库中 `SEAS_LOG_NOTICE` 和 `SEAS_LOG_WARNING` 从未被调用，两个日志级别占据枚举空间但无实际语义承载，导致级别体系形同虚设——实际上只有 DEBUG/INFO/ERROR 三个级别在使用。

### 问题4（信息）：A仓库无 L1/L2/L3 概念，B仓库的 L1/L2/L3 不是宏

两个仓库都不存在 L1/L2/L3 日志级别宏定义。B仓库的 L1/L2/L3 是故障恢复策略的语义标签，通过 `dial_log("[RECOVERY L1] ...")` 文本形式传递，并非任何宏或枚举。A仓库迁移时未引入此概念，恢复事件全部通过 `SEAS_LOG_ERROR` 记录，虽无结构化分层但功能上等价。
