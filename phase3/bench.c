/* bench.c - time-range counting on bursty data: four methods compared on
 * time per query and total memory. Median of 5 runs. */
#define _POSIX_C_SOURCE 199309L   /* -std=c11 hides clock_gettime without this */
#include <stdio.h>
#include <time.h>
#include "query.h"
#include "../common/gen.h"

#define SIGMA 32
#define EPS 16      /* PGM error bound used for (b) */
#define Q 1000000   /* queries for (b), (c), (d); the scan gets fewer */
#define RUNS 5

/* The data and queries are file-level so that answer() can reach them. */
static const uint64_t *TS;
static const uint8_t *TY;
static size_t N;
static EventIndex IX;
static uint64_t *LIST;                /* timestamps grouped by type: type c is */
static size_t START[SIGMA + 1];       /* LIST[START[c] .. START[c+1]), sorted */
static uint64_t QA[Q], QB[Q];         /* time range [A, B] of each query */
static int QX1[Q], QX8[Q];            /* type (0..31), or first of 8 types (0..24) */

static const char *METHOD[4] = {"(a) brute scan", "(b) PGM + WT", "(c) bsearch + WT",
                                "(d) per-type lists"};

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

/* Sort the RUNS timings (insertion sort) and take the middle one. */
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

/* First position in a[0..n) with value >= key. */
static size_t lower_bound(const uint64_t *a, size_t n, uint64_t key)
{
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (a[mid] < key)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

/* Answer query q with method m. kind 0 = one type, kind 1 = 8 types x..x+7.
 * One function for all methods, so every method pays the same call cost. */
static size_t answer(int m, int kind, int q)
{
    uint64_t A = QA[q], B = QB[q];   /* B < UINT64_MAX here, so B + 1 is safe */
    int x = kind ? QX8[q] : QX1[q], y = kind ? x + 7 : x;
    size_t cnt = 0, l, r;

    switch (m) {
    case 0:   /* look at every record */
        for (size_t i = 0; i < N; i++)
            cnt += TS[i] >= A && TS[i] <= B && TY[i] >= x && TY[i] <= y;
        return cnt;
    case 1:
        return kind ? count_types_in_time(&IX, x, y, A, B) : count_in_time(&IX, x, A, B);
    case 2:   /* same as (b) but plain binary search instead of the PGM-index */
        l = lower_bound(TS, N, A);
        r = lower_bound(TS, N, B + 1);
        return kind ? wt_range_count(IX.wt, x, y, l, r) : wt_count(IX.wt, x, l, r);
    default:  /* one binary search pair per type in the range */
        for (int c = x; c <= y; c++) {
            const uint64_t *list = LIST + START[c];
            size_t m2 = START[c + 1] - START[c];
            cnt += lower_bound(list, m2, B + 1) - lower_bound(list, m2, A);
        }
        return cnt;
    }
}

int main(void)
{
    const size_t sizes[] = {1000000, 10000000};
    uint64_t s = 99;

    for (int si = 0; si < 2; si++) {
        N = sizes[si];
        uint64_t *ts = malloc(N * sizeof(uint64_t));
        uint8_t *ty = malloc(N);
        gen_events(N, 1, ty, ts);
        TS = ts;
        TY = ty;
        IX = ev_build(ts, ty, N, SIGMA, EPS);

        /* (d): counting sort by type keeps each type's timestamps in time order */
        for (int c = 0; c <= SIGMA; c++)
            START[c] = 0;
        for (size_t i = 0; i < N; i++)
            START[ty[i] + 1]++;
        for (int c = 0; c < SIGMA; c++)
            START[c + 1] += START[c];
        LIST = malloc(N * sizeof(uint64_t));
        size_t fill[SIGMA];
        for (int c = 0; c < SIGMA; c++)
            fill[c] = START[c];
        for (size_t i = 0; i < N; i++)
            LIST[fill[ty[i]]++] = ts[i];

        /* queries made before timing: A anywhere in the data's time span,
         * window length up to 1/20 of that span */
        uint64_t span = ts[N - 1] - ts[0];
        for (int q = 0; q < Q; q++) {
            QA[q] = ts[0] + rng_below(&s, span + 1);
            QB[q] = QA[q] + rng_below(&s, span / 20 + 1);
            QX1[q] = (int)rng_below(&s, SIGMA);
            QX8[q] = (int)rng_below(&s, SIGMA - 7);
        }

        size_t mem[4] = {
            N * sizeof(uint64_t) + N,                                   /* times + types */
            N * sizeof(uint64_t) + pgm_bytes(&IX.pgm) + wt_bytes(IX.wt),
            N * sizeof(uint64_t) + wt_bytes(IX.wt),
            N * sizeof(uint64_t) + sizeof START,
        };
        int qb = (int)(200000000 / N);   /* the scan is O(n) per query: fewer queries */

        printf("n = %zu (bursty), PGM eps = %d\n", N, EPS);
        printf("%-19s %12s %9s %12s %12s %9s\n", "method", "memory(B)", "B/record",
               "1 type ns/q", "8 types ns/q", "queries");

        size_t sums[4][2];
        for (int m = 0; m < 4; m++) {
            int nq = m == 0 ? qb : Q;
            double ns[2];
            for (int kind = 0; kind < 2; kind++) {
                double t[RUNS];
                for (int r = 0; r < RUNS; r++) {
                    size_t sum = 0;   /* printed/compared, so loops are not removed */
                    double t0 = now();
                    for (int q = 0; q < nq; q++)
                        sum += answer(m, kind, q);
                    t[r] = now() - t0;
                    sums[m][kind] = sum;
                }
                ns[kind] = median(t) * 1e9 / nq;
            }
            printf("%-19s %12zu %9.2f %12.1f %12.1f %9d\n", METHOD[m], mem[m],
                   (double)mem[m] / (double)N, ns[0], ns[1], nq);
        }

        /* the scan answered only the first qb queries; compare it to (b) on those */
        int match = 1;
        for (int kind = 0; kind < 2; kind++) {
            size_t ref = 0;
            for (int q = 0; q < qb; q++)
                ref += answer(1, kind, q);
            if (ref != sums[0][kind] || sums[1][kind] != sums[2][kind] || sums[1][kind] != sums[3][kind])
                match = 0;
        }
        printf("answers of all four methods %s\n\n", match ? "match" : "DO NOT MATCH");

        ev_free(&IX);
        free(LIST);
        free(ts);
        free(ty);
    }
    printf("memory = timestamps (8 B each) + what the method adds.\n"
           "(a) also stores types (1 B each); (b),(c) store types in the wavelet tree;\n"
           "(d) stores timestamps split by type; it cannot tell the type of record i\n"
           "    (access) without also keeping the types array (+1 B/record).\n"
           "ns/q = median of %d runs.\n", RUNS);
    return 0;
}
