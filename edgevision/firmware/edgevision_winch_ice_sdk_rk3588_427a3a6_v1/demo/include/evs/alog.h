/* ============================================================================
 * alog.h  —  算法库侧统一日志头文件（推荐用法）
 *
 * 算法库只需 include 本头文件即可打日志，完全不需要宿主的任何头文件：
 *
 *     #define ALG_MODULE "odom"     // 可选：模块名，缺省 "algo"
 *     #include "evs/alog.h"
 *
 *     ALOG_DEBUG("x=%f", x);
 *     ALOG_INFO("frame %d ok", i);
 *     ALOG_WARN("slow");
 *     ALOG_ERROR("failed, retry");
 *
 * 特性：
 *   1) 自动携带 模块名 + 文件:行号，无需手动传参；
 *   2) 回调未注册时零开销（不格式化、不调用）；
 *   3) 多个算法模块各自 #define ALG_MODULE 即可在日志中区分；
 *   4) 与宿主配合：宿主调用 alg_init(cb) 或 alg_log_set_cb(cb) 注册回调。
 *
 * 工程接入：把 src/core/algo.cpp 编进算法库（libevs_core.so / libevs.a）。
 * 说明见 docs/算法库日志接入说明.docx。
 * ==========================================================================*/
#ifndef EVS_ALOG_H
#define EVS_ALOG_H

#ifdef __cplusplus
extern "C" {
#endif

/* 日志级别 0~5（与宿主 zlog 级别对应） */
enum {
    ALG_LOG_DEBUG = 0,
    ALG_LOG_INFO = 1,
    ALG_LOG_NOTICE = 2,
    ALG_LOG_WARN = 3,
    ALG_LOG_ERROR = 4,
    ALG_LOG_FATAL = 5
};

/* 日志回调：由宿主实现并注册
 * level : 0~5 日志级别
 * tag   : 模块名（ALG_MODULE）
 * file  : 日志来源文件名
 * line  : 日志来源行号
 * msg   : 已格式化消息文本（不含换行）
 * 注意  : msg/tag/file 指针仅在本次调用期间有效 */
typedef void (*alg_log_cb)(int level, const char* tag, const char* file, int line,
                           const char* msg);

/* 注册日志回调（宿主调用；alg_init 内部也会调用） */
void alg_log_set_cb(alg_log_cb cb);

/* 仅在尚未注册时设置默认回调（库内 Logger 桥接用，不覆盖宿主） */
void alg_log_set_default_cb(alg_log_cb cb);

/* 是否已有回调（含默认桥） */
int alg_log_has_cb(void);

/* 推荐入口：初始化算法库日志（注册回调，返回 0 成功） */
int alg_init(alg_log_cb log_cb);

#ifdef __cplusplus
}
#endif

/* 模块名：include 前可 #define ALG_MODULE "xxx" 覆盖，缺省 "algo" */
#ifndef ALG_MODULE
#define ALG_MODULE "algo"
#endif

#ifdef __cplusplus
extern "C" {
#endif
void alg_log_emit(int level, const char* tag, const char* file, int line, const char* fmt,
                  ...)
#ifdef __GNUC__
    __attribute__((format(printf, 5, 6)))
#endif
    ;
#ifdef __cplusplus
}
#endif

#define ALOG_DEBUG(...) \
    alg_log_emit(ALG_LOG_DEBUG, ALG_MODULE, __FILE__, __LINE__, __VA_ARGS__)
#define ALOG_INFO(...) \
    alg_log_emit(ALG_LOG_INFO, ALG_MODULE, __FILE__, __LINE__, __VA_ARGS__)
#define ALOG_NOTICE(...) \
    alg_log_emit(ALG_LOG_NOTICE, ALG_MODULE, __FILE__, __LINE__, __VA_ARGS__)
#define ALOG_WARN(...) \
    alg_log_emit(ALG_LOG_WARN, ALG_MODULE, __FILE__, __LINE__, __VA_ARGS__)
#define ALOG_ERROR(...) \
    alg_log_emit(ALG_LOG_ERROR, ALG_MODULE, __FILE__, __LINE__, __VA_ARGS__)
#define ALOG_FATAL(...) \
    alg_log_emit(ALG_LOG_FATAL, ALG_MODULE, __FILE__, __LINE__, __VA_ARGS__)

#endif /* EVS_ALOG_H */
