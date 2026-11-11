/* test_pgm.c - compare PGM-index lookups against plain binary search. */
#include <stdio.h>
#include <inttypes.h>
#include "pgm.h"
#include "../common/gen.h"

static long checks, failures;

/* Plain binary search: position of the first key >= q. */
static size_t lower_bound(const uint64_t *keys, size_t n, uint64_t q)
{
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (keys[mid] < q)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

/* One lookup check. On failure print the full input: dataset, key, eps,
 * expected, got. Only the first 20 failures are printed. */
static void check_query(const Pgm *p, const char *name, uint64_t q)
{
    size_t want = lower_bound(p->keys, p->n, q), got = pgm_lookup(p, q);
    checks++;
    if (want != got) {
        failures++;
        if (failures <= 20)
            printf("FAIL lookup: %s key=%" PRIu64 " eps=%zu expected=%zu got=%zu\n",
                   name, q, p->eps, want, got);
    }
}

static void check(const uint64_t *keys, size_t n, const char *name, uint64_t *s)
{
    const size_t epss[] = {4, 16, 64};
    for (int e = 0; e < 3; e++) {
        long before = failures;
        Pgm p = pgm_build(keys, n, epss[e]);

        /* The defining promise of level 0: for the first copy of every key,
         * the predicted position is within +/- eps of the true position. */
        size_t sg = 0;
        for (size_t i = 0; i < n; i++) {
            if (i > 0 && keys[i] == keys[i - 1])
                continue;
            while (sg + 1 < p.lv[0].m && p.lv[0].seg[sg + 1].key <= keys[i])
                sg++;
            const Segment *g = &p.lv[0].seg[sg];
            double pred = (double)g->pos + g->slope * (double)(keys[i] - g->key);
            checks++;
            if (fabs(pred - (double)i) > (double)p.eps) {
                failures++;
                if (failures <= 20)
                    printf("FAIL eps bound: %s key=%" PRIu64 " eps=%zu expected=%zu got=%.3f\n",
                           name, keys[i], p.eps, i, pred);
            }
        }

        /* every key, and its neighbours (often not present) */
        for (size_t i = 0; i < n; i++) {
            check_query(&p, name, keys[i]);
            check_query(&p, name, keys[i] - 1);   /* wraps to UINT64_MAX for key 0: still a valid query */
            check_query(&p, name, keys[i] + 1);
        }
        /* extremes, and below the minimum / above the maximum */
        check_query(&p, name, 0);
        check_query(&p, name, UINT64_MAX);
        if (keys[0] > 0)
            check_query(&p, name, keys[0] - 1);
        if (keys[n - 1] < UINT64_MAX)
            check_query(&p, name, keys[n - 1] + 1);
        /* random keys inside the key range, and anywhere in uint64 */
        uint64_t span = keys[n - 1] - keys[0] + 1;   /* 0 only if the range is all of uint64 */
        for (int q = 0; q < 20000; q++) {
            check_query(&p, name, span ? keys[0] + rng_below(s, span) : rng_next(s));
            check_query(&p, name, rng_next(s));
        }

        printf("%-34s eps=%-3zu segments=%-7zu levels=%d  %s\n", name, p.eps, p.lv[0].m,
               p.nlv, failures == before ? "ok" : "FAILED");
        pgm_free(&p);
    }
}

int main(void)
{
    uint64_t s = 42;
    size_t n;
    uint64_t *k = malloc(200000 * sizeof(uint64_t));

    k[0] = 77;
    check(k, 1, "n=1", &s);

    k[0] = 5;
    k[1] = 5;
    check(k, 2, "n=2 duplicate", &s);

    n = 1000;
    for (size_t i = 0; i < n; i++)
        k[i] = 123456;
    check(k, n, "1000 equal keys", &s);

    /* Why pgm_build adds the point (key + 1, position after the run):
     * 200 copies of 10, then 1000 and 2000. A query of 11 must return 200,
     * but a line through (10, 0) and (1000, 200) predicts about 0 there,
     * and the +/- eps window around 0 never reaches 200. */
    n = 202;
    for (size_t i = 0; i < 200; i++)
        k[i] = 10;
    k[200] = 1000;
    k[201] = 2000;
    check(k, n, "duplicate run then a gap", &s);

    n = 10000;   /* values 0..1999: about 5 copies of each, includes key 0 */
    for (size_t i = 0; i < n; i++)
        k[i] = rng_below(&s, 2000);
    qsort(k, n, sizeof k[0], cmp_u64);
    check(k, n, "random, many duplicates", &s);

    n = 10000;   /* long duplicate runs with big gaps between them */
    for (size_t i = 0; i < n; i++)
        k[i] = rng_below(&s, 20) * 1000000;
    qsort(k, n, sizeof k[0], cmp_u64);
    check(k, n, "20 values, long duplicate runs", &s);

    n = 100000;
    for (size_t i = 0; i < n; i++)
        k[i] = rng_below(&s, 1ULL << 40);
    qsort(k, n, sizeof k[0], cmp_u64);
    check(k, n, "random uniform", &s);

    n = 10000;
    for (size_t i = 0; i < n; i++)
        k[i] = 1000 + 7 * i;
    check(k, n, "evenly spaced", &s);

    n = 10000;   /* near the top of uint64, includes UINT64_MAX itself */
    for (size_t i = 0; i < n - 1; i++)
        k[i] = UINT64_MAX - rng_below(&s, 100000);
    k[n - 1] = UINT64_MAX;
    qsort(k, n, sizeof k[0], cmp_u64);
    check(k, n, "near UINT64_MAX", &s);

    n = 200000;
    uint8_t *types = malloc(n);
    gen_events(n, 7, types, k);
    check(k, n, "bursty generator", &s);
    free(types);

    free(k);
    printf("\n%ld checks, %ld failures\n", checks, failures);
    return failures != 0;
}
