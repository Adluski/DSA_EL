# Results

Every number below is copied from the output files in this folder, made by a
clean run on 2026-11-25 (`make clean`, then `make demo` / `make test` /
`make bench` in each phase). Speedup columns are computed from those numbers.
Machine-readable copy: `bench.csv`.

## Tests

All test binaries are built with AddressSanitizer + UBSan.

| Phase | Test cases | Checks | Failures | Source |
|---|---:|---:|---:|---|
| 1 — wavelet tree | 33 | 738,417 | 0 | `phase1_test.txt` |
| 2 — PGM-index | 30 | 5,235,501 | 0 | `phase2_test.txt` |
| 3 — combined queries | 4 | 24,093 | 0 | `phase3_test.txt` |

## Phase 1 — wavelet tree vs brute scan vs position lists

Data: `gen_events`, 32 event types. Source: `phase1_bench.txt`.
Brute scan ran fewer queries (it is O(n) per query).

| n | Method | Memory (B) | B/symbol | rank ns/q | count ns/q | Queries |
|---|---|---:|---:|---:|---:|---:|
| 100,000 | wavelet tree | 96,936 | 0.97 | 36.4 | 50.5 | 1,000,000 |
| 100,000 | brute scan | 100,000 | 1.00 | 68,896.0 | 32,981.6 | 20,000 |
| 100,000 | position lists | 400,264 | 4.00 | 114.3 | 215.6 | 1,000,000 |
| 1,000,000 | wavelet tree | 940,704 | 0.94 | 52.1 | 75.1 | 1,000,000 |
| 1,000,000 | brute scan | 1,000,000 | 1.00 | 751,686.5 | 380,201.5 | 2,000 |
| 1,000,000 | position lists | 4,000,264 | 4.00 | 181.2 | 383.6 | 1,000,000 |
| 10,000,000 | wavelet tree | 9,378,192 | 0.94 | 185.6 | 411.0 | 1,000,000 |
| 10,000,000 | brute scan | 10,000,000 | 1.00 | 7,850,131.0 | 3,863,790.4 | 200 |
| 10,000,000 | position lists | 40,000,264 | 4.00 | 452.1 | 909.8 | 1,000,000 |

Wavelet tree and position lists gave the same answers at every n.
Position lists cannot answer `access(i)` without also storing the raw array (+1 B/symbol).

## Phase 2 — PGM-index vs binary search

Source: `phase2_bench.txt`. ns/lookup = median of 5 runs of 1,000,000 lookups.
Both methods also store the keys (8 B/key); "Index (B)" is the PGM's extra memory.
Speedup = binary search ns ÷ PGM ns.

| Dataset | n | Method | Segments | Levels | Index (B) | ns/lookup | Speedup |
|---|---|---|---:|---:|---:|---:|---:|
| evenly spaced | 1,000,000 | binary search | – | – | 0 | 168.5 | 1.00× |
| evenly spaced | 1,000,000 | PGM ε=4 | 1 | 1 | 1,080 | 15.6 | 10.80× |
| evenly spaced | 1,000,000 | PGM ε=16 | 1 | 1 | 1,080 | 17.8 | 9.47× |
| evenly spaced | 1,000,000 | PGM ε=64 | 1 | 1 | 1,080 | 19.0 | 8.87× |
| random uniform | 1,000,000 | binary search | – | – | 0 | 201.3 | 1.00× |
| random uniform | 1,000,000 | PGM ε=4 | 17,792 | 3 | 430,632 | 143.0 | 1.41× |
| random uniform | 1,000,000 | PGM ε=16 | 1,411 | 2 | 34,944 | 104.8 | 1.92× |
| random uniform | 1,000,000 | PGM ε=64 | 92 | 2 | 3,288 | 134.0 | 1.50× |
| bursty | 1,000,000 | binary search | – | – | 0 | 293.8 | 1.00× |
| bursty | 1,000,000 | PGM ε=4 | 6,775 | 4 | 172,128 | 232.3 | 1.26× |
| bursty | 1,000,000 | PGM ε=16 | 2,072 | 3 | 51,384 | 235.8 | 1.25× |
| bursty | 1,000,000 | PGM ε=64 | 1,613 | 2 | 39,792 | 223.2 | 1.32× |
| evenly spaced | 10,000,000 | binary search | – | – | 0 | 656.2 | 1.00× |
| evenly spaced | 10,000,000 | PGM ε=4 | 1 | 1 | 1,080 | 72.5 | 9.05× |
| evenly spaced | 10,000,000 | PGM ε=16 | 1 | 1 | 1,080 | 78.0 | 8.41× |
| evenly spaced | 10,000,000 | PGM ε=64 | 1 | 1 | 1,080 | 119.4 | 5.50× |
| random uniform | 10,000,000 | binary search | – | – | 0 | 708.7 | 1.00× |
| random uniform | 10,000,000 | PGM ε=4 | 179,079 | 4 | 4,323,552 | 402.7 | 1.76× |
| random uniform | 10,000,000 | PGM ε=16 | 14,005 | 3 | 337,320 | 312.0 | 2.27× |
| random uniform | 10,000,000 | PGM ε=64 | 946 | 2 | 23,784 | 331.4 | 2.14× |
| bursty | 10,000,000 | binary search | – | – | 0 | 626.9 | 1.00× |
| bursty | 10,000,000 | PGM ε=4 | 67,136 | 4 | 1,696,392 | 329.9 | 1.90× |
| bursty | 10,000,000 | PGM ε=16 | 19,946 | 3 | 484,512 | 276.2 | 2.27× |
| bursty | 10,000,000 | PGM ε=64 | 15,416 | 3 | 371,424 | 396.4 | 1.58× |

PGM and binary search gave the same answers in every row.

## Phase 3 — "how many events of type(s) X in time [A, B)?"

Data: bursty, 32 types, PGM ε=16. Source: `phase3_bench.txt`.
ns/q = median of 5 runs. "8 types" = a block of 8 consecutive types x..x+7.
Memory = timestamps (8 B each) + what the method adds.

| n | Method | Memory (B) | B/record | 1 type ns/q | 8 types ns/q | Queries |
|---|---|---:|---:|---:|---:|---:|
| 1,000,000 | (a) brute scan | 9,000,000 | 9.00 | 1,226,733.3 | 1,026,397.0 | 200 |
| 1,000,000 | (b) PGM + wavelet tree | 8,992,088 | 8.99 | 472.1 | 465.2 | 1,000,000 |
| 1,000,000 | (c) binary search + wavelet tree | 8,940,704 | 8.94 | 573.6 | 593.0 | 1,000,000 |
| 1,000,000 | (d) per-type lists | 8,000,264 | 8.00 | **421.3** | 3,077.6 | 1,000,000 |
| 10,000,000 | (a) brute scan | 90,000,000 | 9.00 | 11,598,974.2 | 12,746,087.5 | 20 |
| 10,000,000 | (b) PGM + wavelet tree | 89,862,704 | 8.99 | 1,424.0 | **1,053.0** | 1,000,000 |
| 10,000,000 | (c) binary search + wavelet tree | 89,378,192 | 8.94 | 1,604.6 | 1,523.7 | 1,000,000 |
| 10,000,000 | (d) per-type lists | 80,000,264 | 8.00 | **992.9** | 7,302.1 | 1,000,000 |

All four methods gave the same answers. (d) cannot tell the type of record i
without also keeping the types array (+1 B/record).

## What the numbers show

- **The wavelet tree beats position lists on speed and memory.** It answers rank 2.4–3.5× faster and count 2.2–5.1× faster, using 0.94 B/symbol against 4.00 B/symbol. It is 650× to 42,000× faster than scanning the array.
- **The PGM-index's speedup depends on the shape of the data.** On evenly spaced keys it is 5.5–10.8× faster than binary search, with a 1,080-byte index. On random and bursty keys the best ε gives only 1.3–2.3×.
- **Bigger ε is not always better.** ε=64 shrinks the index but was slower than ε=16 on 5 of 6 datasets. On bursty data at 10M, ε=64 (396 ns) is 44% slower than ε=16 (276 ns). No ε wins everywhere: ε=4 was fastest twice, ε=16 three times, and ε=64 once. ε=4 on random data at 10M needs a 4.3 MB index for less speed than ε=16.
- **The combined structure wins on multi-type queries.** For 8 types, PGM + wavelet tree (b) is 6.6× faster than per-type lists at 1M (465 vs 3,078 ns) and 6.9× faster at 10M (1,053 vs 7,302 ns). Swapping PGM for binary search (c) makes queries 13–45% slower, so the PGM layer helps in every Phase 3 case.
- **Where we lose:** per-type lists (d) beat us on single-type queries: 421 vs 472 ns at 1M, and 993 vs 1,424 ns at 10M, where (b) is 43% slower. They also use less memory (8.00 vs 8.99 B/record). The wavelet tree saves only about 6% over a plain 1-byte type array (0.94 vs 1.00 B/symbol), so its value is speed and flexibility, not compression.
