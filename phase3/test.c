/* test.c - compare the three time-range queries against a scan of the records. */
#include <stdio.h>
#include <inttypes.h>
#include "query.h"
#include "../common/gen.h"

#define QUERIES 1000

static long checks, failures;

static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

/* Run all three queries for one input and compare with brute force.
 * Every failure prints the full input, the expected and the got value. */
static void check_query(const EventIndex *ix, const uint8_t *ty, const char *name,
                        int c, int x, int y, uint64_t A, uint64_t B)
{
    size_t want1 = 0, want2 = 0;
    int want_found = 0;
    uint64_t want_t = 0;
    for (size_t i = 0; i < ix->n; i++) {
        if (ix->ts[i] >= A && ix->ts[i] <= B) {
            want1 += ty[i] == c;
            want2 += ty[i] >= x && ty[i] <= y;
        }
        if (!want_found && ix->ts[i] >= A && ty[i] == c) {
            want_found = 1;
            want_t = ix->ts[i];
        }
    }

    size_t got1 = count_in_time(ix, c, A, B);
    size_t got2 = count_types_in_time(ix, x, y, A, B);
    uint64_t got_t = 0;
    int got_found = first_after(ix, c, A, &got_t);

    /* only the first 20 failures are printed */
    checks += 3;
    if (got1 != want1) {
        failures++;
        if (failures <= 20)
            printf("FAIL: %s count_in_time(type=%d, A=%" PRIu64 ", B=%" PRIu64 ") expected=%zu got=%zu\n",
                   name, c, A, B, want1, got1);
    }
    if (got2 != want2) {
        failures++;
        if (failures <= 20)
            printf("FAIL: %s count_types_in_time(x=%d, y=%d, A=%" PRIu64 ", B=%" PRIu64
                   ") expected=%zu got=%zu\n", name, x, y, A, B, want2, got2);
    }
    if (got_found != want_found || got_t != want_t) {
        failures++;
        if (failures <= 20)
            printf("FAIL: %s first_after(type=%d, A=%" PRIu64 ") expected=%s%" PRIu64
                   " got=%s%" PRIu64 "\n", name, c, A, want_found ? "time " : "none ", want_t,
                   got_found ? "time " : "none ", got_t);
    }
}

static void check(const uint64_t *ts, const uint8_t *ty, size_t n, int sigma,
                  const char *name, uint64_t *s)
{
    long before = failures;
    EventIndex ix = ev_build(ts, ty, n, sigma, 16);
    int c = (int)rng_below(s, (uint64_t)sigma);
    uint64_t mid = ts[n / 2];

    /* fixed edge cases; the type ranges include x = y and x > y */
    check_query(&ix, ty, name, c, c, c, mid + 5, mid);                 /* A > B */
    check_query(&ix, ty, name, c, 0, sigma - 1, 0, mid);               /* A before the first record */
    check_query(&ix, ty, name, c, c + 1, c, mid, UINT64_MAX);          /* B after the last record */
    check_query(&ix, ty, name, c, 0, sigma - 1, mid, ts[n - 1] + 1);
    check_query(&ix, ty, name, c, 2, sigma - 1, 0, UINT64_MAX);        /* everything */
    check_query(&ix, ty, name, c, c, c, ts[0], ts[0]);                 /* one exact time */
    check_query(&ix, ty, name, c, 0, c, ts[n - 1] + 1, UINT64_MAX);    /* after all records */
    for (size_t i = 0; i + 1 < n; i++)
        if (ts[i + 1] - ts[i] >= 2) {   /* empty time range inside a gap */
            check_query(&ix, ty, name, c, 0, sigma - 1, ts[i] + 1, ts[i + 1] - 1);
            break;
        }

    /* random queries: A and B a bit beyond the data on both sides */
    uint64_t lo = ts[0] > 1000 ? ts[0] - 1000 : 0, span = ts[n - 1] + 1000 - lo + 1;
    for (int q = 0; q < QUERIES; q++) {
        uint64_t A = lo + rng_below(s, span), B = lo + rng_below(s, span);
        if (A > B && rng_below(s, 4) != 0) {   /* keep about 1 in 4 of the A > B cases */
            uint64_t t = A;
            A = B;
            B = t;
        }
        c = (int)rng_below(s, (uint64_t)sigma + 1);   /* c = sigma: outside the alphabet */
        int x = (int)rng_below(s, (uint64_t)sigma + 1), y = (int)rng_below(s, (uint64_t)sigma + 1);
        check_query(&ix, ty, name, c, x, y, A, B);
        check_query(&ix, ty, name, c, c, c, A, B);   /* single type range x = y */
    }

    ev_free(&ix);
    printf("%-36s %s\n", name, failures == before ? "ok" : "FAILED");
}

int main(void)
{
    uint64_t s = 42;
    size_t n = 100000;
    uint64_t *ts = malloc(n * sizeof(uint64_t));
    uint8_t *ty = malloc(n);

    ts[0] = 500;
    ty[0] = 3;
    check(ts, ty, 1, 8, "n=1", &s);

    n = 5000;   /* times 0..19999, so many duplicate timestamps */
    for (size_t i = 0; i < n; i++) {
        ts[i] = rng_below(&s, 20000);
        ty[i] = (uint8_t)rng_below(&s, 8);
    }
    qsort(ts, n, sizeof ts[0], cmp_u64);
    check(ts, ty, n, 8, "random, 8 types, duplicate times", &s);

    n = 20000;
    for (size_t i = 0; i < n; i++) {
        ts[i] = rng_below(&s, 1ULL << 40);
        ty[i] = (uint8_t)rng_below(&s, 32);
    }
    qsort(ts, n, sizeof ts[0], cmp_u64);
    check(ts, ty, n, 32, "random, 32 types", &s);

    n = 100000;
    gen_events(n, 7, ty, ts);
    check(ts, ty, n, 32, "bursty generator", &s);

    free(ts);
    free(ty);
    printf("\n%ld checks, %ld failures\n", checks, failures);
    return failures != 0;
}
