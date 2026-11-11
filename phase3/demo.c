/* demo.c - 12 hand-checkable records, then 4 time-range queries step by step. */
#include <stdio.h>
#include <inttypes.h>
#include "query.h"

#define N 12
static const uint64_t TS[N] = {100, 102, 105, 107, 300, 310, 600, 601, 603, 604, 900, 1000};
static const uint8_t TY[N] = {3, 5, 3, 5, 1, 2, 5, 5, 7, 3, 0, 5};

int main(void)
{
    printf("pos  time  type\n");
    for (int i = 0; i < N; i++)
        printf("%3d  %4" PRIu64 "  %4d\n", i, TS[i], TY[i]);

    EventIndex ix = ev_build(TS, TY, N, 8, 4);   /* types 0..7, eps = 4 */

    /* {x, y, A, B}: x == y means one type (count_in_time),
     * otherwise a type range (count_types_in_time) */
    const uint64_t qs[3][4] = {
        {5, 5, 100, 310},   /* A and B are exact timestamps */
        {5, 5, 301, 650},   /* A and B fall between timestamps */
        {3, 5, 105, 604},   /* types 3, 4 and 5 together */
    };
    for (int q = 0; q < 3; q++) {
        int x = (int)qs[q][0], y = (int)qs[q][1];
        uint64_t A = qs[q][2], B = qs[q][3];
        size_t l, r;
        ev_range(&ix, A, B, &l, &r);
        printf("\nquery %d: type %d..%d, time %" PRIu64 "..%" PRIu64 "\n", q + 1, x, y, A, B);
        printf("  PGM: first time >= %" PRIu64 " is at l = %zu, first time > %" PRIu64
               " is at r = %zu\n", A, l, B, r);
        printf("  types at positions [%zu, %zu):", l, r);
        for (size_t i = l; i < r; i++)
            printf(" %d", TY[i]);
        size_t ans = x == y ? count_in_time(&ix, x, A, B) : count_types_in_time(&ix, x, y, A, B);
        printf("\n  wavelet tree count = %zu\n", ans);
    }

    /* first_after, showing the rank -> select steps */
    int c = 7;
    uint64_t A = 200, t;
    size_t p = pgm_lookup(&ix.pgm, A), rk = wt_rank(ix.wt, c, p);
    printf("\nquery 4: first event of type %d at time >= %" PRIu64 "\n", c, A);
    printf("  PGM: first time >= %" PRIu64 " is at p = %zu\n", A, p);
    printf("  rank(%d, %zu) = %zu earlier type-%d events, so we want occurrence %zu\n",
           c, p, rk, c, rk + 1);
    printf("  select(%d, %zu) = position %lld\n", c, rk + 1, (long long)wt_select(ix.wt, c, rk + 1));
    if (first_after(&ix, c, A, &t))
        printf("  answer: time %" PRIu64 "\n", t);
    else
        printf("  answer: none\n");

    ev_free(&ix);
    return 0;
}
