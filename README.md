# DSA_EL - search index over machine event logs (C11)

Simulated machine event logs: each record has a timestamp and an event type
(0..31). The goal is to answer "how many events of type X between time A
and B?" quickly:

- a **PGM-index** turns times A and B into record positions (phase 2),
- a **wavelet tree** counts type X between those positions (phase 1).

The combined query is not built yet.

## Layout

- `common/gen.h` - seeded generator: event types come in runs, timestamps
  are bursty (busy periods, quiet periods, rare long downtime).
- `phase1/` - bit vector with rank/select, and the wavelet tree.
- `phase2/` - PGM-index (not started).

## Phase 1: wavelet tree

Run these from inside `phase1/` (needs gcc, Linux/WSL):

    make demo    # 12-symbol example, prints the tree node by node + queries
    make test    # compares every operation with brute force (ASan + UBSan)
    make bench   # rank/count time and memory: wavelet tree vs scan vs
                 # sorted position lists, n = 1e5, 1e6, 1e7

Operations: `access(i)`, `rank(c, i)` (c's in [0, i)), `select(c, k)`
(position of the k-th c, k starts at 1, -1 if missing), `count(c, l, r)`.
Positions start at 0. Binaries go to `phase1/build/`.
