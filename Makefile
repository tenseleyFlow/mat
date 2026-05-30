# mat — portable Makefile (GNU make and BSD make compatible).
#
# Run ./configure first; it writes config.mk and src/config_generated.h.
# Recipes use literal filenames and `mkdir -p` (no order-only prereqs, no $^/$<)
# so the same rules work under both make flavors.

include config.mk

CFLAGS = $(CONF_CFLAGS) $(FEATURE_CFLAGS) $(WARNFLAGS) -Isrc -Ilib/paige/include
LDFLAGS =

HDRS = src/compat.h src/config.h src/config_generated.h src/err.h \
       src/iobuf.h src/input.h src/fastpath.h src/cli.h \
       src/counter.h src/expand.h src/cooked.h src/scan.h \
       src/term.h src/style.h src/interactive.h src/width.h src/conf.h src/render.h src/matpager.h

# Bespoke pager, vendored as a submodule. Compiled with mat's flags; distinct
# object names so paige's term.c doesn't collide with mat's term.c.
PAIGE_OBJS = build/paige_term.o build/paige_pager.o

OBJS = build/main.o build/cli.o build/err.o build/iobuf.o \
       build/input.o build/fastpath.o build/counter.o build/expand.o \
       build/cooked.o build/scan.o build/term.o build/style.o \
       build/interactive.o build/width.o build/conf.o build/render.o build/matpager.o $(PAIGE_OBJS)

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

build/width.o: src/width.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c src/width.c -o build/width.o

build/conf.o: src/conf.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c src/conf.c -o build/conf.o

build/render.o: src/render.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c src/render.c -o build/render.o

build/matpager.o: src/matpager.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c src/matpager.c -o build/matpager.o

build/paige_term.o: lib/paige/src/term.c lib/paige/src/term.h lib/paige/include/paige.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Ilib/paige/src -c lib/paige/src/term.c -o build/paige_term.o

build/paige_pager.o: lib/paige/src/pager.c lib/paige/src/term.h lib/paige/include/paige.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Ilib/paige/src -c lib/paige/src/pager.c -o build/paige_pager.o

test: mat
	sh tests/run.sh

bench: mat
	sh bench/run.sh

fmt:
	clang-format -i src/*.c src/*.h

tidy: mat
	clang-tidy src/*.c -- $(CFLAGS)

asan:
	$(CC) $(CFLAGS) -fsanitize=address,undefined -g -Ilib/paige/src -o mat-asan \
	    src/main.c src/cli.c src/err.c src/iobuf.c src/input.c src/fastpath.c \
	    src/counter.c src/expand.c src/cooked.c src/scan.c src/term.c \
	    src/style.c src/interactive.c src/width.c src/conf.c src/render.c src/matpager.c \
	    lib/paige/src/term.c lib/paige/src/pager.c

install: mat
	mkdir -p $(PREFIX)/bin $(PREFIX)/share/man/man1
	cp mat $(PREFIX)/bin/mat
	cp man/mat.1 $(PREFIX)/share/man/man1/mat.1
	mkdir -p $(PREFIX)/share/bash-completion/completions
	cp completions/mat.bash $(PREFIX)/share/bash-completion/completions/mat
	mkdir -p $(PREFIX)/share/zsh/site-functions
	cp completions/mat.zsh $(PREFIX)/share/zsh/site-functions/_mat
	mkdir -p $(PREFIX)/share/fish/vendor_completions.d
	cp completions/mat.fish $(PREFIX)/share/fish/vendor_completions.d/mat.fish

clean:
	rm -rf build mat mat-asan tests/build

.PHONY: all test bench fmt tidy asan install clean
