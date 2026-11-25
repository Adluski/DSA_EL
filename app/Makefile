# Text menu over the Phase 3 index. Outputs go in build/.
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra
HEADERS = ../phase3/query.h ../phase1/wavelet.h ../phase2/pgm.h ../common/gen.h

.PHONY: menu clean

menu: build/menu
	./build/menu

build/menu: menu.c $(HEADERS) | build
	$(CC) $(CFLAGS) -O2 -o $@ menu.c -lm

build:
	mkdir -p build

clean:
	rm -rf build
