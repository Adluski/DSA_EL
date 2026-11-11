/* query.h - time-range queries over event records sorted by time.
 * The PGM-index turns times into positions; the wavelet tree counts types
 * between those positions. Time ranges [A, B] include both ends. */
#ifndef QUERY_H
#define QUERY_H

#include "../phase1/wavelet.h"
#include "../phase2/pgm.h"

typedef struct {
    const uint64_t *ts;   /* timestamps, sorted; not owned */
    size_t n;
    Pgm pgm;              /* timestamp -> position */
    WtNode *wt;           /* event type of every record, same order as ts */
} EventIndex;

/* n >= 1. Types must be in 0..sigma-1. */
static inline EventIndex ev_build(const uint64_t *ts, const uint8_t *type, size_t n,
                                  int sigma, size_t eps)
{
    EventIndex ix = {.ts = ts, .n = n};
    ix.pgm = pgm_build(ts, n, eps);
    ix.wt = wt_build(type, n, 0, sigma - 1);
    return ix;
}

static inline void ev_free(EventIndex *ix)
{
    pgm_free(&ix->pgm);
    wt_free(ix->wt);
}

/* Records with A <= time <= B are exactly positions [l, r):
 * l = first time >= A, r = first time > B (= first time >= B + 1).
 * B + 1 would overflow for B = UINT64_MAX, so then r = n.
 * If A > B the range is empty: return l = r = 0. */
static inline void ev_range(const EventIndex *ix, uint64_t A, uint64_t B, size_t *l, size_t *r)
{
    if (A > B) {
        *l = *r = 0;
        return;
    }
    *l = pgm_lookup(&ix->pgm, A);
    *r = B == UINT64_MAX ? ix->n : pgm_lookup(&ix->pgm, B + 1);
}

/* How many events of this type have A <= time <= B. */
static inline size_t count_in_time(const EventIndex *ix, int type, uint64_t A, uint64_t B)
{
    size_t l, r;
    ev_range(ix, A, B, &l, &r);
    return wt_count(ix->wt, type, l, r);
}

/* How many events with type in x..y have A <= time <= B. */
static inline size_t count_types_in_time(const EventIndex *ix, int x, int y, uint64_t A, uint64_t B)
{
    size_t l, r;
    ev_range(ix, A, B, &l, &r);
    return wt_range_count(ix->wt, x, y, l, r);
}

/* First event of this type with time >= A. Returns 1 and stores its time in
 * *time, or returns 0 if there is none.
 * p = first position with time >= A; rank(type, p) events of this type come
 * before p, so the one we want is occurrence number rank + 1. */
static inline int first_after(const EventIndex *ix, int type, uint64_t A, uint64_t *time)
{
    size_t p = pgm_lookup(&ix->pgm, A);
    int64_t pos = wt_select(ix->wt, type, wt_rank(ix->wt, type, p) + 1);
    if (pos < 0)
        return 0;
    *time = ix->ts[pos];
    return 1;
}

#endif
