/* test.c - compare every wavelet tree operation against brute force. */
#include <stdio.h>
#include "wavelet.h"
#include "../common/gen.h"

#define QUERIES 1000

static long checks, failures;

/* Record one check. On a mismatch print it, but keep going so we see every
 * failing case (only the first 20 are printed). x and y are the query arguments. */
static void expect(int ok, const char *name, const char *op, long x, long y)
{
    checks++;
    if (!ok) {
        failures++;
        if (failures <= 20)
            printf("FAIL: %s %s(%ld, %ld)\n", name, op, x, y);
    }
}

/* Brute-force rank: scan positions [0, i). */
static size_t brute_rank(const uint8_t *a, int c, size_t i)
{
    size_t r = 0;
    for (size_t j = 0; j < i; j++)
        r += a[j] == c;
    return r;
}

/* Brute-force select: scan until the k-th c. */
static int64_t brute_select(const uint8_t *a, size_t n, int c, size_t k)
{
    if (k == 0)
        return -1;
    for (size_t j = 0; j < n; j++)
        if (a[j] == c && --k == 0)
            return (int64_t)j;
    return -1;
}

static void check(const uint8_t *a, size_t n, int sigma, uint64_t *s, const char *name)
{
    long before = failures;
    WtNode *root = wt_build(a, n, 0, sigma - 1);

    for (size_t i = 0; i < n; i++)
        expect(wt_access(root, i) == a[i], name, "access", (long)i, 0);

    /* fixed edge cases for every symbol: whole range, last occurrence, one past it.
     * c = sigma is outside the alphabet and must give 0 / -1. */
    for (int c = 0; c <= sigma; c++) {
        size_t occ = brute_rank(a, c, n);
        expect(wt_rank(root, c, n) == occ, name, "rank", c, (long)n);
        expect(wt_rank(root, c, 0) == 0, name, "rank", c, 0);
        expect(wt_select(root, c, 0) == -1, name, "select", c, 0);
        expect(wt_select(root, c, occ) == brute_select(a, n, c, occ), name, "select", c, (long)occ);
        expect(wt_select(root, c, occ + 1) == -1, name, "select", c, (long)occ + 1);
    }

    for (int q = 0; q < QUERIES; q++) {
        int c = (int)rng_below(s, (uint64_t)sigma);
        size_t i = rng_below(s, n + 1);
        expect(wt_rank(root, c, i) == brute_rank(a, c, i), name, "rank", c, (long)i);

        size_t occ = brute_rank(a, c, n);
        size_t k = 1 + rng_below(s, occ + 1);   /* 1..occ+1, so sometimes missing */
        expect(wt_select(root, c, k) == brute_select(a, n, c, k), name, "select", c, (long)k);

        size_t l = rng_below(s, n + 1), r = rng_below(s, n + 1);
        if (l > r) {
            size_t t = l;
            l = r;
            r = t;
        }
        size_t cnt = 0;
        for (size_t j = l; j < r; j++)
            cnt += a[j] == c;
        /* count has three arguments, more than expect() prints, so it
         * reports its own failure with the full input and both answers */
        size_t got = wt_count(root, c, l, r);
        checks++;
        if (got != cnt) {
            failures++;
            if (failures <= 20)
                printf("FAIL: %s count(%d, %zu, %zu) expected=%zu got=%zu\n",
                       name, c, l, r, cnt, got);
        }

        /* range_count over the same [l, r) with four type ranges:
         * random (x > y happens, and y = sigma is past the alphabet),
         * a single type x = y, the full alphabet, and x > y on purpose */
        int xy[4][2] = {{(int)rng_below(s, (uint64_t)sigma + 1), (int)rng_below(s, (uint64_t)sigma + 1)},
                        {c, c}, {0, sigma - 1}, {c + 1, c}};
        for (int t = 0; t < 4; t++) {
            int x = xy[t][0], y = xy[t][1];
            size_t want = 0;
            for (size_t j = l; j < r; j++)
                want += a[j] >= x && a[j] <= y;
            got = wt_range_count(root, x, y, l, r);
            checks++;
            if (got != want) {
                failures++;
                if (failures <= 20)
                    printf("FAIL: %s range_count(%d, %d, %zu, %zu) expected=%zu got=%zu\n",
                           name, x, y, l, r, want, got);
            }
        }
    }

    wt_free(root);
    printf("%-28s %s\n", name, failures == before ? "ok" : "FAILED");
}

int main(void)
{
    uint64_t s = 42;
    const size_t sizes[] = {1, 2, 63, 64, 65, 128, 1000, 100000};
    const int sigmas[] = {2, 3, 8, 32};
    char name[64];

    for (size_t si = 0; si < sizeof sizes / sizeof sizes[0]; si++) {
        for (size_t gi = 0; gi < sizeof sigmas / sizeof sigmas[0]; gi++) {
            size_t n = sizes[si];
            int sigma = sigmas[gi];
            uint8_t *a = malloc(n);
            for (size_t i = 0; i < n; i++)
                a[i] = (uint8_t)rng_below(&s, (uint64_t)sigma);
            snprintf(name, sizeof name, "random n=%zu sigma=%d", n, sigma);
            check(a, n, sigma, &s, name);
            free(a);
        }
    }

    /* the real event-log generator (types come in runs) */
    size_t n = 100000;
    uint8_t *a = malloc(n);
    gen_events(n, 7, a, NULL);
    check(a, n, 32, &s, "gen_events n=100000 sigma=32");
    free(a);

    printf("\n%ld checks, %ld failures\n", checks, failures);
    return failures != 0;
}
