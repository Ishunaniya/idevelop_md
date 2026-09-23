/* 最刁钻用例：在"内部 first>0 且数据绕回"的状态下触发扩容(8->16->32)，
 * 看 copy_buffer 的两段拷贝会不会错位/丢数据。用整数指针做严格顺序校验。
 *
 * 编译运行(在 apps/modem_mng 目录下):
 *   gcc -std=c99 -fsanitize=address,undefined -I cc_deque \
 *       md/test/eg25_cops_lock_selfcheck/cc_deque_test2.c \
 *       cc_deque/cc_deque.c -o /tmp/cc_deque_test2
 *   ASAN_OPTIONS=detect_leaks=0 /tmp/cc_deque_test2
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cc_deque.h"

int main(void)
{
    CC_Deque *d; cc_deque_new(&d);
    long vals[64];
    for (int i=0;i<64;i++) vals[i]=1000+i;

    int fail=0, next=0;      /* next = 下一个要 add 的 vals 下标 */
    int expect_head=0;       /* 逻辑队首应等于 vals[expect_head] */

    /* 反复 add 3 / remove 1，让 first 不断前移、数据绕回，同时 size 增长跨越 8/16/32 */
    for (int round=0; round<20; round++) {
        for (int a=0;a<3 && next<64;a++) cc_deque_add_last(d,&vals[next++]);
        if (cc_deque_size(d)>0) {
            long *rm=NULL; cc_deque_remove_first(d,(void**)&rm);
            if (*rm != vals[expect_head]) {
                printf("[FAIL] round %d remove: got %ld expect %ld\n",round,*rm,vals[expect_head]);
                fail=1;
            }
            expect_head++;
        }
        /* 全序校验：逻辑 index i 应为 vals[expect_head+i] */
        size_t sz=cc_deque_size(d);
        for (size_t i=0;i<sz;i++){
            long *p=NULL; cc_deque_get_at(d,i,(void**)&p);
            if(!p||*p!=vals[expect_head+i]){
                printf("[FAIL] round %d get_at(%zu): got %ld expect %ld (cap=%zu)\n",
                       round,i,p?*p:-1,vals[expect_head+i],cc_deque_capacity(d));
                fail=1; break;
            }
        }
    }
    printf("final size=%zu capacity=%zu\n",cc_deque_size(d),cc_deque_capacity(d));
    printf(fail?"===== FAIL =====\n":"===== PASS: 扩容+绕回 全程顺序正确 =====\n");
    return fail;
}
