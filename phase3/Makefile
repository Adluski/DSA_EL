# Phase 3: combined time-range queries. Outputs go in build/.
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra
HEADERS = query.h ../phase1/wavelet.h ../phase2/pgm.h ../common/gen.h

.PHONY: demo test bench clean

demo: build/demo
	./build/demo

# sanitizers catch out-of-bounds reads, undefined behaviour and memory leaks
test: build/test
	./build/test

bench: build/bench
	./build/bench

build/demo: demo.c $(HEADERS) | build
	$(CC) $(CFLAGS) -O2 -o $@ demo.c -lm

build/test: test.c $(HEADERS) | build
	$(CC) $(CFLAGS) -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=undefined -o $@ test.c -lm

# -mpopcnt: use the CPU popcount instruction instead of a slow software loop
build/bench: bench.c $(HEADERS) | build
	$(CC) $(CFLAGS) -O2 -mpopcnt -o $@ bench.c -lm

build:
	mkdir -p build

clean:
	rm -rf build
