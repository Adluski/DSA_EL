# Phase 2: PGM-index. Outputs go in build/.
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra

.PHONY: test bench clean

# sanitizers catch out-of-bounds reads, undefined behaviour and memory leaks
test: build/test_pgm
	./build/test_pgm

bench: build/bench_pgm
	./build/bench_pgm

build/test_pgm: test_pgm.c pgm.h ../common/gen.h | build
	$(CC) $(CFLAGS) -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=undefined -o $@ test_pgm.c -lm

build/bench_pgm: bench_pgm.c pgm.h ../common/gen.h | build
	$(CC) $(CFLAGS) -O2 -o $@ bench_pgm.c -lm

build:
	mkdir -p build

clean:
	rm -rf build
