/* menu.c - text menu over the Phase 3 event index (PGM-index + wavelet tree).
 * ANSI escape codes only, no libraries. Bad input is rejected, never trusted. */
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../phase3/query.h"
#include "../common/gen.h"

#define SIGMA 32
#define EPS 16
#define MAXN 10000000u
#define HEAD "\033[1;36m"
#define OK "\033[1;32m"
#define BAD "\033[1;31m"
#define DIM "\033[2m"
#define OFF "\033[0m"

typedef struct { uint64_t t; uint8_t ty; } Rec;
/* ts sorted, ty[i] = type of record i, n = 0 when nothing is loaded */
typedef struct { uint64_t *ts; uint8_t *ty; size_t n; EventIndex ix; } Data;

static int by_time(const void *a, const void *b)
{
    uint64_t x = ((const Rec *)a)->t, y = ((const Rec *)b)->t;
    return (x > y) - (x < y);
}

/* One line without its newline (a too-long line is read to its end). 0 at end of input. */
static int read_line(char *buf, size_t size)
{
    if (!fgets(buf, (int)size, stdin))
        return 0;
    size_t len = strlen(buf);
    if (len && buf[len - 1] == '\n')
        buf[len - 1] = '\0';
    else
        for (int c; (c = getchar()) != '\n' && c != EOF;)
            ;
    return 1;
}

/* Parse one unsigned number at *s and move *s past it. No sign allowed. */
static int parse_u64(const char **s, uint64_t *v)
{
    char *end;
    while (isspace((unsigned char)**s))
        (*s)++;
    if (!isdigit((unsigned char)**s))
        return 0;
    errno = 0;
    *v = strtoull(*s, &end, 10);
    *s = end;
    return errno == 0;
}

/* Ask until the answer is a whole number in lo..hi. Returns 0 at end of input. */
static int ask(const char *what, uint64_t lo, uint64_t hi, uint64_t *v)
{
    char buf[128];
    for (;;) {
        printf(HEAD "  %s" OFF DIM " [%" PRIu64 "..%" PRIu64 "]" OFF ": ", what, lo, hi);
        if (!read_line(buf, sizeof buf))
            return 0;
        const char *s = buf;
        if (parse_u64(&s, v) && strspn(s, " \t") == strlen(s) && *v >= lo && *v <= hi)
            return 1;
        printf(BAD "  Not a number in range, try again.\n" OFF);
    }
}

/* Drop the old records and index; make room for n new ones (n >= 1).
 * Returns 0 if out of memory (then nothing is loaded). */
static int reset(Data *d, size_t n)
{
    if (d->n)
        ev_free(&d->ix);
    free(d->ts);
    free(d->ty);
    *d = (Data){.ts = malloc(n * sizeof *d->ts), .ty = malloc(n)};
    if (d->ts && d->ty)
        return (int)(d->n = n);
    printf(BAD "  Out of memory.\n" OFF);
    return 0;
}

int main(void)
{
    Data d = {0};
    char buf[256];
    uint64_t c, x, y, a, b, v;
    setvbuf(stdout, NULL, _IONBF, 0);   /* prompts show before input is read */

    for (;;) {
        printf("\033[2J\033[H" HEAD
               "  ╔══════════════════════════════════════════════╗\n"
               "  ║  Event log queries: PGM-index + wavelet tree ║\n"
               "  ╚══════════════════════════════════════════════╝\n" OFF
               DIM "  records loaded: %zu\n\n" OFF
               "  1  Type my own records\n"
               "  2  Generate N random bursty records\n"
               "  3  Show records (first 30)\n"
               "  4  Count types X..Y in time [A, B]\n"
               "  5  First event of type X at or after time A\n"
               "  0  Quit\n\n", d.n);
        if (!ask("Choice", 0, 5, &c) || c == 0)
            break;
        if (c >= 3 && !d.n) {
            printf(BAD "  No records yet: use 1 or 2 first.\n" OFF);
        } else if (c == 1) {
            printf("  One record per line: " HEAD "time type" OFF " (type 0..31). "
                   "Empty line to finish.\n");
            Rec *r = NULL;
            size_t n = 0;
            while (n < MAXN && printf("  > ") &&
                   read_line(buf, sizeof buf) && buf[strspn(buf, " \t")]) {
                const char *s = buf;
                uint64_t t, ty;
                if (!parse_u64(&s, &t) || !parse_u64(&s, &ty) || ty >= SIGMA ||
                    strspn(s, " \t") != strlen(s)) {
                    printf(BAD "  Skipped: need two numbers, type 0..31.\n" OFF);
                    continue;
                }
                Rec *g = realloc(r, (n + 1) * sizeof *r);
                if (!g)
                    break;
                r = g;
                r[n++] = (Rec){t, (uint8_t)ty};
            }
            if (n && reset(&d, n)) {
                qsort(r, n, sizeof *r, by_time);
                for (size_t i = 0; i < n; i++)
                    d.ts[i] = r[i].t, d.ty[i] = r[i].ty;
            } else if (!n) {
                printf(BAD "  No records entered; old data kept.\n" OFF);
            }
            free(r);
        } else if (c == 2) {
            if (ask("N", 1, MAXN, &v) && ask("Seed", 0, UINT64_MAX, &a) && reset(&d, v))
                gen_events(v, a, d.ty, d.ts);
        } else if (c == 3) {
            printf(HEAD "  %6s %20s %5s\n" OFF, "#", "time", "type");
            for (size_t i = 0; i < d.n && i < 30; i++)
                printf("  %6zu %20" PRIu64 " %5d\n", i, d.ts[i], d.ty[i]);
            if (d.n > 30)
                printf(DIM "  ... %zu more\n" OFF, d.n - 30);
        } else if (c == 4) {
            if (!ask("X (first type)", 0, SIGMA - 1, &x) || !ask("Y (last type)", x, SIGMA - 1, &y) ||
                !ask("A (from time)", 0, UINT64_MAX, &a) || !ask("B (to time)", a, UINT64_MAX, &b))
                continue;
            size_t l, r, brute = 0;
            ev_range(&d.ix, a, b, &l, &r);
            size_t wt = wt_range_count(d.ix.wt, (int)x, (int)y, l, r);
            for (size_t i = 0; i < d.n; i++)
                brute += d.ts[i] >= a && d.ts[i] <= b && d.ty[i] >= x && d.ty[i] <= y;
            printf("\n  PGM: records with time in [A, B] are positions l=%zu .. r=%zu (r excluded)\n"
                   "  wavelet tree answer: %zu\n  brute-force answer:  %zu\n  %s\n",
                   l, r, wt, brute, wt == brute ? OK "match" OFF : BAD "MISMATCH" OFF);
        } else if (c == 5) {
            if (!ask("X (type)", 0, SIGMA - 1, &x) || !ask("A (time)", 0, UINT64_MAX, &a))
                continue;
            if (first_after(&d.ix, (int)x, a, &v))
                printf(OK "\n  First type %" PRIu64 " event at or after %" PRIu64 ": time %" PRIu64 "\n" OFF,
                       x, a, v);
            else
                printf(BAD "\n  No type %" PRIu64 " event at or after %" PRIu64 ".\n" OFF, x, a);
        }
        if (d.n && !d.ix.wt) {   /* reset() cleared the index: build it for the new records */
            d.ix = ev_build(d.ts, d.ty, d.n, SIGMA, EPS);
            printf(OK "  Loaded %zu records, index built (PGM eps=%d + wavelet tree).\n" OFF, d.n, EPS);
        }
        printf(DIM "\n  Press Enter to go back to the menu..." OFF);
        if (!read_line(buf, sizeof buf))
            break;
    }
    if (d.n)
        ev_free(&d.ix);
    free(d.ts);
    free(d.ty);
    printf(OFF "\n  Bye.\n");
    return 0;
}
