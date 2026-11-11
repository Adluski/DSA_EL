/* pgm.h - PGM-index over sorted uint64 keys.
 * pgm_lookup(q) returns the position of the first key >= q (n if none). */
#ifndef PGM_H
#define PGM_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>

#define PGM_MAX_LEVELS 64

/* One line piece: for a query q >= key, predicted position = pos + slope * (q - key). */
typedef struct {
    uint64_t key;    /* first key this segment covers */
    double slope;
    size_t pos;      /* true position of key in the level below */
} Segment;

typedef struct {
    Segment *seg;
    size_t m;        /* number of segments */
} Level;

/* lv[0] predicts positions in keys[]; lv[l] predicts positions in lv[l-1].seg[].
 * The top level lv[nlv-1] has exactly one segment. */
typedef struct {
    const uint64_t *keys;   /* not owned: the caller keeps the array alive */
    size_t n;
    size_t eps;
    Level lv[PGM_MAX_LEVELS];
    int nlv;
} Pgm;

/* Greedy "shrinking cone". Points (x[i], y[i]) have x strictly increasing and
 * y never decreasing. A segment starts at its first point (x0, y0). A later
 * point (x, y) is predicted within +/- eps only if the slope s satisfies
 *     y - eps <= y0 + s * (x - x0) <= y + eps,
 * i.e. s lies in [(y - y0 - eps) / dx, (y - y0 + eps) / dx]. We keep the
 * intersection [slo, shi] of these ranges over all points taken so far.
 * The range only shrinks, so any slope still inside it works for EVERY point
 * already taken. When a new point would make the range empty, no single line
 * from (x0, y0) fits all of them, so we close the segment and start a new one
 * at that point. slo starts at 0 so the line never goes down (lookup needs that).
 * Writes the segments to out (room for cnt) and returns how many. */
static inline size_t pgm_segments(const uint64_t *x, const uint32_t *y, size_t cnt,
                                  size_t eps, Segment *out)
{
    size_t m = 0, i = 0;
    while (i < cnt) {
        size_t start = i;
        double slo = 0, shi = INFINITY;
        for (i = start + 1; i < cnt; i++) {
            double dx = (double)(x[i] - x[start]);
            double dy = (double)y[i] - (double)y[start];
            double a = (dy - (double)eps) / dx, b = (dy + (double)eps) / dx;
            if (a > shi || b < slo)
                break;   /* cone would become empty */
            if (a > slo)
                slo = a;
            if (b < shi)
                shi = b;
        }
        out[m].key = x[start];
        out[m].pos = y[start];
        /* a one-point segment has shi = infinity: any slope is fine, use 0 */
        out[m].slope = shi == INFINITY ? 0 : (slo + shi) / 2;
        m++;
    }
    return m;
}

/* Build over keys[0..n), sorted, duplicates allowed, n >= 1, n < 2^32, eps >= 1. */
static inline Pgm pgm_build(const uint64_t *keys, size_t n, size_t eps)
{
    Pgm p = {.keys = keys, .n = n, .eps = eps, .nlv = 0};

    /* Level 0 points: each distinct key at its first position.
     * A run of duplicates makes the answer jump by more than 1 just after that
     * key, so we also add the point (key + 1, position after the run). Without
     * it a query between this key and the next could be predicted too far left.
     * Each run has >= 2 keys, so there are at most n + n/2 points. */
    size_t cap = n + n / 2 + 1, cnt = 0;
    uint64_t *x = malloc(cap * sizeof(uint64_t));
    uint32_t *y = malloc(cap * sizeof(uint32_t));
    for (size_t i = 0; i < n;) {
        size_t j = i;
        while (j < n && keys[j] == keys[i])
            j++;
        x[cnt] = keys[i];
        y[cnt++] = (uint32_t)i;
        if (j - i > 1 && j < n && keys[i] + 1 < keys[j]) {
            x[cnt] = keys[i] + 1;
            y[cnt++] = (uint32_t)j;
        }
        i = j;
    }

    /* Build levels until one segment is left. Each level's points are the
     * first keys of the level below, with y = segment index. A segment always
     * holds at least 2 points (any 2 points fit a line), so each level is at
     * most half the size of the one below and the loop ends. */
    for (;;) {
        Level *L = &p.lv[p.nlv++];
        L->seg = malloc(cnt * sizeof(Segment));
        L->m = pgm_segments(x, y, cnt, eps, L->seg);
        L->seg = realloc(L->seg, L->m * sizeof(Segment));
        if (L->m == 1)
            break;
        for (size_t s = 0; s < L->m; s++) {
            x[s] = L->seg[s].key;
            y[s] = (uint32_t)s;
        }
        cnt = L->m;
    }
    free(x);
    free(y);
    return p;
}

static inline void pgm_free(Pgm *p)
{
    for (int l = 0; l < p->nlv; l++)
        free(p->lv[l].seg);
    p->nlv = 0;
}

/* Bytes used by the index itself (segments + struct), not the keys array. */
static inline size_t pgm_bytes(const Pgm *p)
{
    size_t b = sizeof *p;
    for (int l = 0; l < p->nlv; l++)
        b += p->lv[l].m * sizeof(Segment);
    return b;
}

/* Position of the first key >= q. Walk from the top level down: at each
 * level, predict where q falls in the level below, then binary search only
 * a small window around the prediction.
 * Window size: the build guarantees the truth is within eps of the line at
 * every point; a query between two points can add 1 more, and we add 1 more
 * on each side to absorb floating-point rounding. */
static inline size_t pgm_lookup(const Pgm *p, uint64_t q)
{
    /* the ends are handled first, so below keys[0] < q <= keys[n-1] */
    if (q <= p->keys[0])
        return 0;
    if (q > p->keys[p->n - 1])
        return p->n;

    size_t s = 0;   /* segment index in the current level; the top has one */
    for (int l = p->nlv - 1; l >= 0; l--) {
        const Level *L = &p->lv[l];
        const Segment *sg = &L->seg[s];
        size_t below = l == 0 ? p->n : p->lv[l - 1].m;   /* size of the level below */
        size_t next = s + 1 < L->m ? L->seg[s + 1].pos : below;

        /* q >= sg->key here, and the slope is >= 0, so pred >= sg->pos.
         * The answer cannot be past the next segment's start, so clamp. */
        double pred = (double)sg->pos + sg->slope * (double)(q - sg->key);
        size_t pi = pred >= (double)next ? next : (size_t)pred;

        size_t lo = pi > p->eps + 2 ? pi - p->eps - 2 : 0;
        size_t hi = pi + p->eps + 3 < below ? pi + p->eps + 3 : below;

        if (l == 0) {
            /* first key >= q inside keys[lo..hi) */
            while (lo < hi) {
                size_t mid = lo + (hi - lo) / 2;
                if (p->keys[mid] < q)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            return lo;
        }
        /* we need the last segment below with key <= q:
         * find the first one with key > q, then step back one */
        const Segment *down = p->lv[l - 1].seg;
        while (lo < hi) {
            size_t mid = lo + (hi - lo) / 2;
            if (down[mid].key <= q)
                lo = mid + 1;
            else
                hi = mid;
        }
        s = lo - 1;
    }
    return 0;   /* not reached: level 0 always returns */
}

#endif
