CC      ?= cc
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
CFLAGS  += -Iinclude -pthread

LDLIBS  ?= -pthread

BUILD   := build
LIB     := $(BUILD)/libarenac.a

PREFIX  ?= /usr/local
DESTDIR ?=

SRCS    := src/arena.c src/slab.c
OBJS    := $(SRCS:src/%.c=$(BUILD)/%.o)

TESTS   := $(BUILD)/test_arena $(BUILD)/test_slab

all: $(LIB) $(TESTS) $(BUILD)/bench

$(BUILD)/%.o: src/%.c include/arena.h include/slab.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB): $(OBJS)
	ar rcs $@ $^

$(BUILD)/test_arena: tests/test_arena.c $(LIB) | $(BUILD)
	$(CC) $(CFLAGS) $< $(LIB) $(LDLIBS) -o $@

$(BUILD)/test_slab: tests/test_slab.c $(LIB) | $(BUILD)
	$(CC) $(CFLAGS) $< $(LIB) $(LDLIBS) -o $@

$(BUILD)/bench: bench/bench.c $(LIB) | $(BUILD)
	$(CC) $(CFLAGS) $< $(LIB) $(LDLIBS) -o $@

$(BUILD):
	mkdir -p $(BUILD)

SAN_TESTS := test_arena test_slab

$(BUILD)/test_%_tsan: tests/test_%.c $(SRCS) | $(BUILD)
	$(CC) $(CFLAGS) -O1 -g -fsanitize=thread -DARENAC_THREADS_FORCE_FALLBACK $< $(SRCS) $(LDLIBS) -o $@

$(BUILD)/test_%_asan: tests/test_%.c $(SRCS) | $(BUILD)
	$(CC) $(CFLAGS) -O1 -g -fsanitize=address -DARENAC_THREADS_FORCE_FALLBACK $< $(SRCS) $(LDLIBS) -o $@

test: $(LIB) $(TESTS) $(SAN_TESTS:%=$(BUILD)/%_tsan) $(SAN_TESTS:%=$(BUILD)/%_asan)
	$(BUILD)/test_arena
	$(BUILD)/test_slab
	@for t in $(SAN_TESTS); do $(BUILD)/$${t}_tsan || exit 1; done
	@for t in $(SAN_TESTS); do $(BUILD)/$${t}_asan || exit 1; done

bench: $(BUILD)/bench
	$(BUILD)/bench

install: $(LIB)
	install -d $(DESTDIR)$(PREFIX)/lib $(DESTDIR)$(PREFIX)/include
	install -m644 $(LIB) $(DESTDIR)$(PREFIX)/lib/libarenac.a
	install -m644 include/arena.h $(DESTDIR)$(PREFIX)/include/arena.h
	install -m644 include/slab.h $(DESTDIR)$(PREFIX)/include/slab.h

clean:
	rm -rf $(BUILD)

.PHONY: all test bench install clean