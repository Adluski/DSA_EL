# DSA_EL - search index over machine event logs (C11)

Simulated machine event logs: each record has a timestamp and an event type
(0..31), sorted by time. Goal: "how many events of type X between time A
and B?" A **PGM-index** turns A and B into record positions, and a
**wavelet tree** counts type X between those positions.

- `common/gen.h` - seeded generator (types come in runs, bursty times).
- `phase1/` wavelet tree, `phase2/` PGM-index, `phase3/` combined queries.

Run `make` inside each phase folder (gcc, Linux/WSL). Binaries go to
`<phase>/build/`. Tests use ASan + UBSan.

## Phase 1: wavelet tree

    make demo    # 12-symbol example, prints the tree node by node + queries
    make test    # every operation vs brute force
    make bench   # rank/count vs scan vs sorted position lists, n = 1e5..1e7

Operations: `access(i)`, `rank(c, i)` (c's in [0, i)), `select(c, k)`
(k-th c, k from 1, -1 if none), `count(c, l, r)`, `range_count(x, y, l, r)`
(types x..y in [l, r)). Positions start at 0.

## Phase 2: PGM-index

    make test    # lookup vs binary search, eps = 4, 16, 64
    make bench   # time, memory, segments: 3 datasets, n = 1e6 and 1e7

`lookup(q)` returns the position of the first key >= q (n if none).

## Phase 3: combined queries

    make demo    # 12 records, 4 queries printed step by step
    make test    # all queries vs a scan of the records
    make bench   # scan vs PGM+WT vs binary search+WT vs per-type lists

`count_in_time(type, A, B)`, `count_types_in_time(x, y, A, B)`,
`first_after(type, A)`. Time ranges include both A and B.

## Real data (Loghub BGL 2k sample)

Source: BGL 2k sample from Loghub,
https://raw.githubusercontent.com/logpai/loghub/master/BGL/BGL_2k.log_structured.csv
(saved as `data/loghub/BGL_2k.log_structured.csv`).

    gcc -std=c11 -Wall -Wextra -O2 -o tools/build/loghub_convert tools/loghub_convert.c
    tools/build/loghub_convert data/loghub/BGL_2k.log_structured.csv \
        data/loghub/bgl_records.txt data/loghub/bgl_types.txt

Each line becomes one record `time type`: time = the Unix `Timestamp` (seconds),
type = its `EventTemplate`; the 31 most frequent templates are types 0..30,
all others are type 31 ("other"). `bgl_types.txt` lists type -> count, template.
Load `bgl_records.txt` with option 6 in `app/menu` or "Load file" in `app/gui`.

Citation: Zhu, J., et al. (2023). Loghub: A Large Collection of System Log
Datasets for AI-driven Log Analytics. ISSRE.
