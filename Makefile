CC ?= cc
CSTD ?= c2x
WARN := -Wall -Wextra -Wpedantic -Werror
OPT ?= -O2
SAN ?= -fsanitize=address,undefined
CPPFLAGS := -Iinclude
CFLAGS := $(OPT) -g -std=$(CSTD) $(WARN)
LDFLAGS :=

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin

LIB_SRCS := src/coop.c
LIB_OBJS := $(LIB_SRCS:src/%.c=$(OBJ_DIR)/%.o)
TEST_SRCS := tests/test_coop.c
TEST_BIN := $(BIN_DIR)/test_coop

.PHONY: all clean test

all: $(TEST_BIN)

$(OBJ_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TEST_BIN): $(LIB_OBJS) $(TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SAN) $(LIB_OBJS) $(TEST_SRCS) $(LDFLAGS) $(SAN) -o $@

test: $(TEST_BIN)
	$(TEST_BIN)

clean:
	rm -rf $(BUILD_DIR)
