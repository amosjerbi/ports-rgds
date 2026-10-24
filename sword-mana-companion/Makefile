CC ?= gcc
CFLAGS := -std=gnu11 -O2 -Wall -Wno-unused-function $(shell sdl2-config --cflags)
LIBS := $(shell sdl2-config --libs) -lSDL2_ttf -lm
SRCS := src/main.c src/ra_client.c
build/som-bottom: $(SRCS) src/som_data.h src/ui_common.h src/ra_client.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(LIBS)
clean:
	rm -rf build
.PHONY: clean
