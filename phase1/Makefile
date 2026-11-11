# Phase 1: wavelet tree. Outputs go in build/.
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra

.PHONY: demo test bench clean

demo: build/demo
	./build/demo

# sanitizers catch out-of-bounds reads, undefined behaviour and memory leaks
test: build/test
	./build/test

bench: build/bench
	./build/bench

build/demo: demo.c wavelet.h | build
	$(CC) $(CFLAGS) -O2 -o $@ demo.c

build/test: test.c wavelet.h ../common/gen.h | build
	$(CC) $(CFLAGS) -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=undefined -o $@ test.c

# -mpopcnt: use the CPU popcount instruction instead of a slow software loop
build/bench: bench.c wavelet.h ../common/gen.h | build
	$(CC) $(CFLAGS) -O2 -mpopcnt -o $@ bench.c

build:
	mkdir -p build

clean:
	rm -rf build
