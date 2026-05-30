# mat — portable Makefile (GNU make and BSD make compatible).
#
# Run ./configure first; it writes config.mk and src/config_generated.h.
# Recipes use literal filenames and `mkdir -p` (no order-only prereqs, no $^/$<)
# so the same rules work under both make flavors.

include config.mk

CFLAGS = $(CONF_CFLAGS) $(FEATURE_CFLAGS) $(WARNFLAGS) -Isrc
LDFLAGS =

HDRS = src/compat.h src/config.h src/config_generated.h src/err.h \
       src/iobuf.h src/input.h src/fastpath.h src/cli.h \
       src/counter.h src/expand.h src/cooked.h src/scan.h \
       src/term.h src/style.h src/interactive.h

OBJS = build/main.o build/cli.o build/err.o build/iobuf.o \
       build/input.o build/fastpath.o build/counter.o build/expand.o \
       build/cooked.o build/scan.o build/term.o build/style.o \
       build/interactive.o

all: mat

mat: $(OBJS)
	$(CC) $(LDFLAGS) -o mat $(OBJS)

build/main.o: src/main.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c src/main.c -o build/main.o

build/cli.o: src/cli.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c src/cli.c -o build/cli.o

build/err.o: src/err.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c src/err.c -o build/err.o

build/iobuf.o: src/iobuf.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c src/iobuf.c -o build/iobuf.o

build/input.o: src/input.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c src/input.c -o build/input.o

build/fastpath.o: src/fastpath.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c src/fastpath.c -o build/fastpath.o

build/counter.o: src/counter.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c src/counter.c -o build/counter.o

build/expand.o: src/expand.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c src/expand.c -o build/expand.o

build/cooked.o: src/cooked.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c src/cooked.c -o build/cooked.o

build/scan.o: src/scan.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c src/scan.c -o build/scan.o

build/term.o: src/term.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c src/term.c -o build/term.o

build/style.o: src/style.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c src/style.c -o build/style.o

build/interactive.o: src/interactive.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c src/interactive.c -o build/interactive.o

test: mat
	sh tests/run.sh

bench: mat
	sh bench/run.sh

fmt:
	clang-format -i src/*.c src/*.h

tidy: mat
	clang-tidy src/*.c -- $(CFLAGS)

asan:
	$(CC) $(CFLAGS) -fsanitize=address,undefined -g -Isrc -o mat-asan \
	    src/main.c src/cli.c src/err.c src/iobuf.c src/input.c src/fastpath.c \
	    src/counter.c src/expand.c src/cooked.c src/scan.c src/term.c \
	    src/style.c src/interactive.c

install: mat
	mkdir -p $(PREFIX)/bin
	cp mat $(PREFIX)/bin/mat

clean:
	rm -rf build mat mat-asan tests/build

.PHONY: all test bench fmt tidy asan install clean
