# Text menu over the Phase 3 index. Outputs go in build/.
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra
HEADERS = ../phase3/query.h ../phase1/wavelet.h ../phase2/pgm.h ../common/gen.h

.PHONY: menu gui clean

menu: build/menu
	./build/menu

build/menu: menu.c $(HEADERS) | build
	$(CC) $(CFLAGS) -O2 -o $@ menu.c -lm

# raylib window version. Needs app/vendor/ (raylib 5.5 Linux release + raygui 4.0
# raygui-4.0.h), downloaded with curl; not in git. -isystem hides vendor warnings.
RL = vendor/raylib-5.5_linux_amd64
gui: build/gui
	./build/gui

build/gui: gui.c $(HEADERS) | build
	$(CC) $(CFLAGS) -O2 -isystem vendor -isystem $(RL)/include -o $@ gui.c \
		$(RL)/lib/libraylib.a -lGL -lm -lpthread -ldl -lrt -lX11

build:
	mkdir -p build

clean:
	rm -rf build
