#include <stdio.h>
#include <string.h>
#include "cc_deque.h"

/* 复刻 dial.c 里 deque_oper 的用法：
 *   parse_oper_list_info: 循环 cc_deque_add_last(字符串)
 *   select_oper:          cc_deque_get_at(0) 读队首, cc_deque_remove_first 弹出
 *   list_oper:            size==0 时重新灌
 * 重点验证：元素个数 > DEFAULT_CAPACITY(8) 时，读回的顺序/内容是否正确。
 *
 * 编译运行(在 apps/modem_mng 目录下):
 *   gcc -std=c99 -fsanitize=address,undefined -I cc_deque \
 *       md/test/eg25_cops_lock_selfcheck/cc_deque_test.c \
 *       cc_deque/cc_deque.c -o /tmp/cc_deque_test
 *   ASAN_OPTIONS=detect_leaks=0 /tmp/cc_deque_test
 */

static const char *opers[] = {
    "46000","46001","46011","26201","26202","26203","23201","23203",
    "22801","20201","21401","23430"  /* 12 个，模拟德国漫游扫出一大堆 */
};
#define N ((int)(sizeof(opers)/sizeof(opers[0])))

static int check_order(CC_Deque *d, const char **expect, int n, const char *tag)
{
    if ((int)cc_deque_size(d) != n) {
        printf("[FAIL:%s] size=%zu expect=%d\n", tag, cc_deque_size(d), n);
        return 1;
    }
    int bad = 0;
    for (int i = 0; i < n; i++) {
        char *p = NULL;
        cc_deque_get_at(d, i, (void**)&p);
        if (!p || strcmp(p, expect[i]) != 0) {
            printf("[FAIL:%s] idx %d: got '%s' expect '%s'\n", tag, i, p?p:"(null)", expect[i]);
            bad = 1;
        }
    }
    if (!bad) printf("[OK:%s] %d elems in-order, capacity=%zu\n", tag, n, cc_deque_capacity(d));
    return bad;
}

int main(void)
{
    CC_Deque *d;
    cc_deque_new(&d);
    printf("initial capacity = %zu\n", cc_deque_capacity(d));

    /* 1) 灌 12 个（跨过 8 触发扩容），验证顺序 */
    for (int i = 0; i < N; i++) cc_deque_add_last(d, (void*)opers[i]);
    int fail = check_order(d, opers, N, "populate-12");

    /* 2) 模拟 select_oper：逐个 get_at(0)+remove_first，验证弹出顺序 */
    for (int i = 0; i < N; i++) {
        char *head = NULL;
        cc_deque_get_at(d, 0, (void**)&head);
        if (!head || strcmp(head, opers[i]) != 0) {
            printf("[FAIL:pop] step %d: head='%s' expect '%s'\n", i, head?head:"(null)", opers[i]);
            fail = 1;
        }
        char *rm = NULL;
        cc_deque_remove_first(d, (void**)&rm);
    }
    printf("[INFO] after draining, size=%zu first-index-internal hidden\n", cc_deque_size(d));

    /* 3) 模拟 list_oper 重灌：空队列上再灌 12 个（此时内部 first 已不在 0，考验环形+扩容） */
    for (int i = 0; i < N; i++) cc_deque_add_last(d, (void*)opers[i]);
    fail |= check_order(d, opers, N, "refill-after-drain");

    /* 4) 半弹再灌：弹 5 个再灌 5 个，制造 first>last 的环绕，再读全序 */
    for (int i = 0; i < 5; i++) { char *rm=NULL; cc_deque_remove_first(d,(void**)&rm); }
    const char *more[] = {"AAA01","AAA02","AAA03","AAA04","AAA05"};
    for (int i = 0; i < 5; i++) cc_deque_add_last(d, (void*)more[i]);
    /* 期望顺序：opers[5..11] 然后 more[0..4] */
    const char *expect2[12];
    int k=0;
    for (int i=5;i<N;i++) expect2[k++]=opers[i];
    for (int i=0;i<5;i++) expect2[k++]=more[i];
    fail |= check_order(d, expect2, k, "wrap-mixed");

    printf(fail ? "\n===== RESULT: FAIL (cc_deque 有问题) =====\n"
                : "\n===== RESULT: PASS (cc_deque >8 无问题) =====\n");
    return fail;
}
