CC := gcc
CFLAGS := -Wall -Wextra -std=c11
LDFLAGS := -luring # Additional libraries to link against, if needed

# Get all C source files in the current directory
SRCS := $(wildcard *.c)

# Generate corresponding object file names for each source file
OBJS := $(SRCS:.c=.o)

SHARED_OBJS :=

SYNTHETIC_DIR := root/

# final executables
TARGET_BENCH_STAT := bench_stat

# final executables
TARGET_BENCH_IO := bench_io

.PHONY: all clean

all: $(TARGET_BENCH_STAT) $(TARGET_BENCH_IO)


# Rule to compile each source file into an object file
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to link all object files into the final executable 'stat_bench'
$(TARGET_BENCH_STAT): $(SHARED_OBJS) bench_stat.o
	$(CC) $^ -o $@ $(LDFLAGS)

$(TARGET_BENCH_IO): bench_io.o
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(OBJS) $(SYNTHETIC_DIR) $(TARGET_BENCH_STAT) $(TARGET_BENCH_IO)

CLEAR-CACHE='sync; echo 3 | sudo tee /proc/sys/vm/drop_caches'
bench: $(TARGET_BENCH_STAT) $(SYNTHETIC_DIR)
	sudo -v
	hyperfine --warmup 3 \
		  --prepare $(CLEAR-CACHE) \
		  --export-markdown bench_stat.md \
		  --parameter-list stat-type stat,statx_all,statx_one './bench_stat root {stat-type}'

# Create a synthetic directory here
$(SYNTHETIC_DIR):
	dune exec ../fs-gen/gen.exe 25 4
