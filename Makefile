CC      ?= cc
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
CFLAGS  += -Iinclude
LDLIBS  =

BUILD   := build
LIB     := $(BUILD)/libarenac.a

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

test: all
	$(BUILD)/test_arena
	$(BUILD)/test_slab

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
