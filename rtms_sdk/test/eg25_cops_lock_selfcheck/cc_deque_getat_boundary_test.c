/* cc_deque_getat_boundary_test.c —— 针对性验证 cc_deque_get_at 的边界修复
 * (index > size 改为 index >= size)：
 *   1) 空 deque 上 get_at(0) 必须返回 CC_ERR_OUT_OF_RANGE，且不得触碰 *out
 *   2) 非空 deque 上所有合法 index(0..size-1) 行为与修复前完全一致
 *   3) 非法 index(== size，即"差一"越界)必须被拒绝
 *   4) 抽干后(remove_first 全部弹出)再 get_at(0) 不得读到陈旧槽位
 *
 * 编译运行(在 apps/modem_mng 目录下):
 *   gcc -std=c99 -fsanitize=address,undefined -I cc_deque \
 *       md/test/eg25_cops_lock_selfcheck/cc_deque_getat_boundary_test.c \
 *       cc_deque/cc_deque.c -o /tmp/boundary_test
 *   ASAN_OPTIONS=detect_leaks=0 /tmp/boundary_test
 */
#include <stdio.h>
#include <string.h>
#include "cc_deque.h"

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("[FAIL] %s\n", msg); g_fail = 1; } } while (0)

int main(void)
{
    CC_Deque *d;
    cc_deque_new(&d);

    /* 1) 空 deque：get_at(0) 必须失败，且不得写 *out（用哨兵值验证未被触碰） */
    {
        char *sentinel = (char *)0xDEADBEEF;
        char *out = sentinel;
        enum cc_stat r = cc_deque_get_at(d, 0, (void **)&out);
        CHECK(r == CC_ERR_OUT_OF_RANGE, "空deque get_at(0) 应返回 CC_ERR_OUT_OF_RANGE");
        CHECK(out == sentinel, "空deque get_at(0) 失败时不应触碰 *out(哨兵应保持不变)");
        if (r == CC_ERR_OUT_OF_RANGE && out == sentinel)
            printf("[OK] 空deque get_at(0): 正确拒绝且不触碰 *out\n");
    }

    /* 2) 灌 3 个元素，验证所有合法 index 正常，且 index==size(3) 越界应被拒绝 */
    {
        char *vals[3] = {"AAA", "BBB", "CCC"};
        int i;
        for (i = 0; i < 3; i++) cc_deque_add_last(d, vals[i]);

        for (i = 0; i < 3; i++) {
            char *out = NULL;
            enum cc_stat r = cc_deque_get_at(d, (size_t)i, (void **)&out);
            CHECK(r == CC_OK && out == vals[i], "合法 index 应正确取值(修复不应影响正常路径)");
        }
        printf("[OK] 3个元素：index 0/1/2 全部正确\n");

        {
            char *sentinel = (char *)0xCAFEBABE;
            char *out = sentinel;
            enum cc_stat r = cc_deque_get_at(d, 3, (void **)&out); /* index==size，差一越界 */
            CHECK(r == CC_ERR_OUT_OF_RANGE, "index==size(差一越界) 应返回 CC_ERR_OUT_OF_RANGE(这正是原bug放行的那个值)");
            CHECK(out == sentinel, "差一越界时不应触碰 *out");
            if (r == CC_ERR_OUT_OF_RANGE && out == sentinel)
                printf("[OK] index==size(差一越界): 正确拒绝(此为原 `>` 判断的漏判点，现已修复)\n");
        }
    }

    /* 3) 全部弹空后再次验证空状态(模拟 dial.c select_oper 抽干候选表后的场景) */
    {
        char *rm; int i;
        for (i = 0; i < 3; i++) cc_deque_remove_first(d, (void **)&rm);
        char *sentinel = (char *)0x12345678;
        char *out = sentinel;
        enum cc_stat r = cc_deque_get_at(d, 0, (void **)&out);
        CHECK(r == CC_ERR_OUT_OF_RANGE, "抽干后 get_at(0) 应返回错误(而不是读到 remove_first 遗留的陈旧槽位)");
        CHECK(out == sentinel, "抽干后 get_at(0) 不应触碰 *out(验证不会读到已free的陈旧指针)");
        if (r == CC_ERR_OUT_OF_RANGE && out == sentinel)
            printf("[OK] 抽干后 get_at(0): 正确拒绝，未读到 remove_first 遗留的陈旧槽位\n");
    }

    printf(g_fail ? "\n===== FAIL =====\n" : "\n===== ALL PASS =====\n");
    return g_fail;
}
