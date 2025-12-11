# ================================
#   FAIR CPU BASELINE MAKEFILE
#   (Portable, Reproducible, O2)
# ================================

# -------- Config --------
MODE ?= release

# -------- Tools ---------
CC := gcc

# -------- Sources -------
SRCS  := $(wildcard *.c)
OBJS  := $(SRCS:.c=.o)
DEPS  := rtree.h
TARGET := rtree_cpu_baseline

# -------- Flags ---------
CSTD    := -std=c11
WARN    := -Wall -Wextra -Wshadow -Wundef
OPT     := -O2                     # FAIR BASELINE
DEBUG   := -g
THREADS := -pthread

# No CPU-specific optimizations (portable baseline)
CPUFLAGS :=                        # intentionally empty

# No LTO, no unroll, no -march, no strict alias
EXTRA :=                           # intentionally empty

# Release vs Debug mode
ifeq ($(MODE),debug)
  CFLAGS := $(CSTD) $(WARN) $(DEBUG) $(CPUFLAGS) $(THREADS) $(EXTRA)
else
  CFLAGS := $(CSTD) $(WARN) $(OPT) $(CPUFLAGS) $(THREADS) $(EXTRA)
endif

# Link libs
LDFLAGS := $(THREADS) -lm

# -------- Build Rules --------
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	./$(TARGET)
	rm -f *.o
	
print-flags:
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"

.PHONY: all clean print-flags
