/* bench.c - rank and count: wavelet tree vs brute-force scan vs sorted
 * position lists per event type. Time per query and memory in bytes. */
#define _POSIX_C_SOURCE 199309L   /* -std=c11 hides clock_gettime without this */
#include <stdio.h>
#include <time.h>
#include "wavelet.h"
#include "../common/gen.h"

#define SIGMA 32
#define Q 1000000   /* queries for the wavelet tree and the position lists */

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

/* Number of positions < i in the sorted list pos[0..m): binary search. */
static size_t list_rank(const uint32_t *pos, size_t m, size_t i)
{
    size_t lo = 0, hi = m;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (pos[mid] < i)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

int main(void)
{
    const size_t sizes[] = {100000, 1000000, 10000000};
    uint64_t s = 99;
    uint8_t *qc = malloc(Q);
    size_t *ql = malloc(Q * sizeof(size_t)), *qr = malloc(Q * sizeof(size_t));
    size_t sum = 0;   /* every answer is added here and printed, so the
                         compiler cannot delete the timed loops */
    size_t wt_sum, list_sum;   /* tree and lists answer the same queries,
                                  so these must be equal */

    printf("%-9s %-15s %12s %9s %10s %11s %9s\n",
           "n", "method", "memory(B)", "B/symbol", "rank ns/q", "count ns/q", "queries");

    for (int si = 0; si < 3; si++) {
        size_t n = sizes[si];
        uint8_t *a = malloc(n);
        gen_events(n, 1, a, NULL);

        /* make all queries before timing anything; rank uses qr[q] as i */
        for (int q = 0; q < Q; q++) {
            qc[q] = (uint8_t)rng_below(&s, SIGMA);
            ql[q] = rng_below(&s, n + 1);
            qr[q] = rng_below(&s, n + 1);
            if (ql[q] > qr[q]) {
                size_t t = ql[q];
                ql[q] = qr[q];
                qr[q] = t;
            }
        }

        /* (a) wavelet tree */
        WtNode *root = wt_build(a, n, 0, SIGMA - 1);
        size_t before = sum;
        double t0 = now();
        for (int q = 0; q < Q; q++)
            sum += wt_rank(root, qc[q], qr[q]);
        double t1 = now();
        for (int q = 0; q < Q; q++)
            sum += wt_count(root, qc[q], ql[q], qr[q]);
        double t2 = now();
        wt_sum = sum - before;
        size_t mem = wt_bytes(root);
        printf("%-9zu %-15s %12zu %9.2f %10.1f %11.1f %9d\n", n, "wavelet tree", mem,
               (double)mem / (double)n, (t1 - t0) * 1e9 / Q, (t2 - t1) * 1e9 / Q, Q);
        wt_free(root);

        /* (b) brute-force scan of the raw array. A scan costs O(n), so it
         * gets fewer queries; time per query is still comparable. */
        int qb = (int)(2000000000 / n);
        t0 = now();
        for (int q = 0; q < qb; q++)
            for (size_t j = 0; j < qr[q]; j++)
                sum += a[j] == qc[q];
        t1 = now();
        for (int q = 0; q < qb; q++)
            for (size_t j = ql[q]; j < qr[q]; j++)
                sum += a[j] == qc[q];
        t2 = now();
        printf("%-9zu %-15s %12zu %9.2f %10.1f %11.1f %9d\n", n, "brute scan", n, 1.0,
               (t1 - t0) * 1e9 / qb, (t2 - t1) * 1e9 / qb, qb);

        /* (c) one sorted list of positions per type, all in one array:
         * positions of type c are pos[start[c] .. start[c+1]). Built by
         * counting sort, so each list is already in increasing order. */
        size_t start[SIGMA + 1] = {0};
        for (size_t i = 0; i < n; i++)
            start[a[i] + 1]++;
        for (int c = 0; c < SIGMA; c++)
            start[c + 1] += start[c];
        uint32_t *pos = malloc(n * sizeof(uint32_t));
        size_t fill[SIGMA];
        for (int c = 0; c < SIGMA; c++)
            fill[c] = start[c];
        for (size_t i = 0; i < n; i++)
            pos[fill[a[i]]++] = (uint32_t)i;

        before = sum;
        t0 = now();
        for (int q = 0; q < Q; q++) {
            int c = qc[q];
            sum += list_rank(pos + start[c], start[c + 1] - start[c], qr[q]);
        }
        t1 = now();
        for (int q = 0; q < Q; q++) {
            int c = qc[q];
            size_t m = start[c + 1] - start[c];
            sum += list_rank(pos + start[c], m, qr[q]) - list_rank(pos + start[c], m, ql[q]);
        }
        t2 = now();
        list_sum = sum - before;
        mem = n * sizeof(uint32_t) + sizeof start;
        printf("%-9zu %-15s %12zu %9.2f %10.1f %11.1f %9d\n", n, "position lists", mem,
               (double)mem / (double)n, (t1 - t0) * 1e9 / Q, (t2 - t1) * 1e9 / Q, Q);
        printf("answers of wavelet tree and position lists %s\n\n",
               wt_sum == list_sum ? "match" : "DO NOT MATCH");
        free(pos);
        free(a);
    }

    printf("note: position lists cannot answer access(i) on their own; to do that they\n"
           "      would also need the raw array (+1 B/symbol). The wavelet tree can.\n");
    printf("checksum %zu\n", sum);
    free(qc);
    free(ql);
    free(qr);
    return 0;
}
