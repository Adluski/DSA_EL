#!/usr/bin/env bash
# Live demo: builds and runs the demos and the test suites (no benchmarks).
set -e
cd "$(dirname "$0")"

heading() {
    echo
    echo "=================================================================="
    echo "  $1"
    echo "=================================================================="
}

heading "PHASE 1 DEMO: wavelet tree"
make -s -C phase1 demo

heading "PHASE 3 DEMO: combined time + type queries"
make -s -C phase3 demo

heading "PHASE 1 TESTS: wavelet tree"
make -s -C phase1 test

heading "PHASE 2 TESTS: PGM-index"
make -s -C phase2 test

heading "PHASE 3 TESTS: combined queries"
make -s -C phase3 test

heading "ALL DEMOS AND TESTS FINISHED"
