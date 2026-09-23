/* cops_unlock_test.c —— 方案A"注册超时幂等 COPS=0 解锁"判定逻辑的单元测试
 *
 * 目的：不依赖 SDK/模组，用 mock 的 Ql_SendAT 验证 reg_timeout 里那段
 * "先 AT+COPS? 查，非自动才发 AT+COPS=0" 的决策是否正确。
 *
 * 编译运行：
 *   gcc -std=c99 -fsanitize=address,undefined cops_unlock_test.c -o cops_test
 *   ASAN_OPTIONS=detect_leaks=0 ./cops_test
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ── mock 层：模拟一台模组的 COPS 状态 + 记录发过哪些 AT ─────────────── */
static const char *g_mock_cops_reply; /* AT+COPS? 该返回什么，如 "+COPS: 1" / "+COPS: 0" / NULL(无应答) */
static int  g_mock_cops0_fails;       /* 非0 表示 AT+COPS=0 返回失败 */
static char g_sent[8][64];            /* 记录依次发出的 AT 命令 */
static int  g_sent_n;

/* 模拟 Ql_SendAT：把命令记进 g_sent；对 COPS? 把 mock 应答写进 rsp；返回 0=OK,非0=失败 */
static int mock_Ql_SendAT(int fd, const char *cmd, const char *finalRsp,
                          long to, char *rsp)
{
    (void)fd; (void)finalRsp; (void)to;
    strncpy(g_sent[g_sent_n], cmd, 63); g_sent[g_sent_n][63] = 0; g_sent_n++;

    if (strcmp(cmd, "AT+COPS?") == 0) {
        if (rsp) { rsp[0] = 0; if (g_mock_cops_reply) strncpy(rsp, g_mock_cops_reply, 1023); }
        return g_mock_cops_reply ? 0 : 1;   /* NULL 应答 = 命令超时失败 */
    }
    if (strcmp(cmd, "AT+COPS=0") == 0) {
        if (rsp) strncpy(rsp, g_mock_cops0_fails ? "ERROR" : "OK", 1023);
        return g_mock_cops0_fails ? 1 : 0;
    }
    return 0;
}

/* ── 被测逻辑：与 dial.c reg_timeout 顶部那段等价（去掉 mutex/日志） ──────
 * 返回：0=已是自动跳过, 1=发了 COPS=0 解锁, -1=COPS? 查询失败仍强发 COPS=0 */
static int cops_unlock_if_manual(int fd, char *rsp)
{
    memset(rsp, 0, 1024);
    mock_Ql_SendAT(fd, "AT+COPS?", "OK", 3000, rsp);
    if (strstr(rsp, "+COPS: 0") != NULL) {
        return 0;                       /* 已自动 → 幂等跳过，不发 COPS=0 */
    }
    memset(rsp, 0, 1024);
    int ret = mock_Ql_SendAT(fd, "AT+COPS=0", "OK", 30000, rsp);
    return (ret == 0) ? 1 : -1;
}

/* ── 断言小工具 ─────────────────────────────────────────────────────── */
static int g_fail = 0;
static void reset_mock(const char *cops_reply, int cops0_fails) {
    g_mock_cops_reply = cops_reply; g_mock_cops0_fails = cops0_fails;
    g_sent_n = 0; memset(g_sent, 0, sizeof(g_sent));
}
static void expect_sent(int idx, const char *want, const char *tag) {
    if (idx >= g_sent_n || strcmp(g_sent[idx], want) != 0) {
        printf("[FAIL:%s] 第%d条AT: got '%s' expect '%s'\n", tag, idx,
               idx < g_sent_n ? g_sent[idx] : "(none)", want);
        g_fail = 1;
    }
}
static void expect_count(int want, const char *tag) {
    if (g_sent_n != want) { printf("[FAIL:%s] 共发%d条AT, 期望%d\n", tag, g_sent_n, want); g_fail = 1; }
}

int main(void)
{
    char rsp[1024];

    /* 用例1：手动锁死(COPS:1) → 应先查、再发 COPS=0 解锁 */
    reset_mock("+COPS: 1", 0);
    int r = cops_unlock_if_manual(0, rsp);
    if (r != 1) { printf("[FAIL:manual-lock] 返回 %d 期望 1\n", r); g_fail = 1; }
    expect_count(2, "manual-lock");
    expect_sent(0, "AT+COPS?",  "manual-lock");
    expect_sent(1, "AT+COPS=0", "manual-lock");
    if (!g_fail) printf("[OK] 用例1 手动锁死 → 查+解锁\n");

    /* 用例2：已是自动(COPS:0) → 幂等跳过，不发 COPS=0（省一次无谓重注册） */
    reset_mock("+COPS: 0", 0);
    r = cops_unlock_if_manual(0, rsp);
    if (r != 0) { printf("[FAIL:already-auto] 返回 %d 期望 0\n", r); g_fail = 1; }
    expect_count(1, "already-auto");
    expect_sent(0, "AT+COPS?", "already-auto");
    if (!g_fail) printf("[OK] 用例2 已自动 → 跳过\n");

    /* 用例3：漫游锁在别的运营商模式(COPS:1,含运营商) → 视为非自动，解锁 */
    reset_mock("+COPS: 1,2,\"26201\"", 0);
    r = cops_unlock_if_manual(0, rsp);
    if (r != 1) { printf("[FAIL:manual-with-oper] 返回 %d 期望 1\n", r); g_fail = 1; }
    expect_sent(1, "AT+COPS=0", "manual-with-oper");
    if (!g_fail) printf("[OK] 用例3 手动+运营商 → 解锁\n");

    /* 用例4：COPS? 查询无应答(超时) → 保守起见仍强发 COPS=0 */
    reset_mock(NULL, 0);
    r = cops_unlock_if_manual(0, rsp);
    if (r != 1) { printf("[FAIL:query-timeout] 返回 %d 期望 1(仍解锁)\n", r); g_fail = 1; }
    expect_sent(1, "AT+COPS=0", "query-timeout");
    if (!g_fail) printf("[OK] 用例4 查询超时 → 仍解锁\n");

    /* 用例5：COPS=0 本身失败 → 返回 -1(上层记 error,后续 CFUN 兜底) */
    reset_mock("+COPS: 1", 1);
    r = cops_unlock_if_manual(0, rsp);
    if (r != -1) { printf("[FAIL:unlock-fail] 返回 %d 期望 -1\n", r); g_fail = 1; }
    if (!g_fail) printf("[OK] 用例5 解锁失败 → -1(交后续CFUN)\n");

    printf(g_fail ? "\n===== FAIL =====\n" : "\n===== ALL PASS =====\n");
    return g_fail;
}
