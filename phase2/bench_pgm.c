/* bench_pgm.c - PGM-index lookup vs plain binary search: time and memory. */
#define _POSIX_C_SOURCE 199309L   /* -std=c11 hides clock_gettime without this */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "pgm.h"
#include "../common/gen.h"

#define Q 1000000
#define RUNS 5      /* each timing is repeated; we report the median */

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

/* Median of RUNS timings: sort them (insertion sort, only 5 values) and
 * take the middle one, so one slow run (e.g. the OS interrupting) does not
 * change the result. */
static double median(double *t)
{
    for (int i = 1; i < RUNS; i++)
        for (int j = i; j > 0 && t[j] < t[j - 1]; j--) {
            double x = t[j];
            t[j] = t[j - 1];
            t[j - 1] = x;
        }
    return t[RUNS / 2];
}

static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

int main(void)
{
    const size_t sizes[] = {1000000, 10000000};
    const char *names[] = {"evenly spaced", "random uniform", "bursty"};
    const size_t epss[] = {4, 16, 64};
    uint64_t s = 99;
    uint64_t *qs = malloc(Q * sizeof(uint64_t));

    printf("%-15s %-9s %-14s %9s %7s %11s %12s %10s\n", "dataset", "n", "method",
           "segments", "levels", "index(B)", "keys(B)", "ns/lookup");

    for (int si = 0; si < 2; si++) {
        size_t n = sizes[si];
        uint64_t *k = malloc(n * sizeof(uint64_t));
        uint8_t *types = malloc(n);

        for (int d = 0; d < 3; d++) {
            if (d == 0) {
                for (size_t i = 0; i < n; i++)
                    k[i] = 1000 + 1000 * (uint64_t)i;
            } else if (d == 1) {
                for (size_t i = 0; i < n; i++)
                    k[i] = rng_below(&s, 1ULL << 50);
                qsort(k, n, sizeof k[0], cmp_u64);
            } else {
                gen_events(n, 1, types, k);
            }

            /* make all queries before timing: half are keys that exist,
             * half are random values between the smallest and largest key */
            for (int q = 0; q < Q; q++)
                qs[q] = q % 2 ? k[rng_below(&s, n)] : k[0] + rng_below(&s, k[n - 1] - k[0] + 1);

            /* baseline: binary search over the whole array, no index.
             * Answers are summed and compared, so the compiler cannot drop
             * the loops; the sum of one run is enough for the comparison. */
            double t[RUNS];
            size_t base_sum = 0;
            for (int r = 0; r < RUNS; r++) {
                base_sum = 0;
                double t0 = now();
                for (int q = 0; q < Q; q++) {
                    size_t lo = 0, hi = n;
                    while (lo < hi) {
                        size_t mid = lo + (hi - lo) / 2;
                        if (k[mid] < qs[q])
                            lo = mid + 1;
                        else
                            hi = mid;
                    }
                    base_sum += lo;
                }
                t[r] = now() - t0;
            }
            printf("%-15s %-9zu %-14s %9s %7s %11d %12zu %10.1f\n", names[d], n,
                   "binary search", "-", "-", 0, n * sizeof(uint64_t), median(t) * 1e9 / Q);

            int all_match = 1;

            for (int e = 0; e < 3; e++) {
                Pgm p = pgm_build(k, n, epss[e]);
                size_t sum = 0;
                for (int r = 0; r < RUNS; r++) {
                    sum = 0;
                    double t0 = now();
                    for (int q = 0; q < Q; q++)
                        sum += pgm_lookup(&p, qs[q]);
                    t[r] = now() - t0;
                }
                if (sum != base_sum)
                    all_match = 0;
                char method[32];
                snprintf(method, sizeof method, "PGM eps=%zu", epss[e]);
                printf("%-15s %-9zu %-14s %9zu %7d %11zu %12zu %10.1f\n", names[d], n, method,
                       p.lv[0].m, p.nlv, pgm_bytes(&p), n * sizeof(uint64_t), median(t) * 1e9 / Q);
                pgm_free(&p);
            }
            printf("answers of PGM (all eps) and binary search %s\n\n",
                   all_match ? "match" : "DO NOT MATCH");
        }
        free(k);
        free(types);
    }
    printf("index(B) = memory of the index alone; both methods also need keys(B).\n");
    printf("segments = segments in the bottom level.\n");
    printf("ns/lookup = median of %d runs of %d lookups.\n", RUNS, Q);
    free(qs);
    return 0;
}
