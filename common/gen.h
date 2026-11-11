/* gen.h - shared generator for simulated machine event logs.
 * Used by phase1 (event types) and phase2 (timestamps). */
#ifndef GEN_H
#define GEN_H

#include <stdint.h>
#include <stddef.h>

/* splitmix64 random generator. We use our own instead of rand() so the
 * same seed gives the same data on every machine and compiler. */
static inline uint64_t rng_next(uint64_t *state)
{
    uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* Random number in [0, m). The modulo bias is tiny for the small m we use. */
static inline uint64_t rng_below(uint64_t *state, uint64_t m)
{
    return rng_next(state) % m;
}

/* Fill type[0..n) with event types 0..31 and ts[0..n) with strictly
 * increasing timestamps. ts may be NULL if only types are needed. */
static inline void gen_events(size_t n, uint64_t seed, uint8_t *type, uint64_t *ts)
{
    uint64_t s = seed;
    uint8_t fault[3];   /* the few types a machine repeats while in one state */
    int busy = 1;       /* busy period = small gaps, quiet period = large gaps */
    uint64_t t = 1000;

    for (int j = 0; j < 3; j++)
        fault[j] = (uint8_t)rng_below(&s, 32);

    for (size_t i = 0; i < n; i++) {
        /* about every 20 records the machine moves to a new state */
        if (rng_below(&s, 100) < 5)
            for (int j = 0; j < 3; j++)
                fault[j] = (uint8_t)rng_below(&s, 32);

        /* 90% of events come from the current state, 10% are random noise */
        if (rng_below(&s, 100) < 10)
            type[i] = (uint8_t)rng_below(&s, 32);
        else
            type[i] = fault[rng_below(&s, 3)];

        /* switch between busy and quiet roughly every 500 records */
        if (rng_below(&s, 500) == 0)
            busy = !busy;
        uint64_t gap = busy ? 1 + rng_below(&s, 10) : 100 + rng_below(&s, 1000);
        /* rare long downtime (machine off) */
        if (rng_below(&s, 20000) == 0)
            gap += 1000000 + rng_below(&s, 9000000);
        t += gap;   /* gap >= 1, so timestamps strictly increase */
        if (ts)
            ts[i] = t;
    }
}

#endif
