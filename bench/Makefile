CC := gcc
CFLAGS := -Wall -Wextra -std=c11
LDFLAGS := -luring # Additional libraries to link against, if needed

# Get all C source files in the current directory
SRCS := $(wildcard *.c)

# Generate corresponding object file names for each source file
OBJS := $(SRCS:.c=.o)

SHARED_OBJS :=

SYNTHETIC_DIR := root/

# Create a synthetic directory here
$(SYNTHETIC_DIR):
	dune exec ../fs-gen/gen.exe 25 4

# final executables
TARGET_STAT_BENCH := stat_bench

.PHONY: all clean

all: $(TARGET_STAT_BENCH)

# Rule to compile each source file into an object file
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to link all object files into the final executable 'stat_bench'
$(TARGET_STAT_BENCH): $(SHARED_OBJS) stat_bench.o
	$(CC) $^ -o $@ $(LDFLAGS)

$(TARGET_URING_STATX): uring_statx.o
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(OBJS) $(TARGET_STAT_BENCH) $(SYNTHETIC_DIR)

CLEAR-CACHE='sync; echo 3 | sudo tee /proc/sys/vm/drop_caches'
bench: $(TARGET_STAT_BENCH) $(SYNTHETIC_DIR)
	sudo -v
	hyperfine --warmup 3 \
		  --prepare $(CLEAR-CACHE) \
		  --export-markdown stat_bench.md \
		  --parameter-list stat-type stat,statx_all,stat_one './stat_bench root {stat-type}'
