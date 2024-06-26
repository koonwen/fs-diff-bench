CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -g
LDFLAGS := -luring # Additional libraries to link against, if needed

# Get all C source files in the current directory
SRCS := $(wildcard *.c)

# Generate corresponding object file names for each source file
OBJS := $(SRCS:.c=.o)

SHARED_OBJS :=

SYNTHETIC_DIR := root/

# final executables
EXE := bench_stat bench_io

.PHONY: all clean

all: $(EXE)

bench_stat: bench_stat.o
	$(CC) -o $@ $<

bench_io: bench_io.o
	$(CC) -o $@ $< -luring

# Rule to compile each source file into an object file
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(OBJS) $(SYNTHETIC_DIR) $(EXE)

# Generate a big synthetic directory
$(SYNTHETIC_DIR):
	dune exec ../fs-gen/gen.exe 25 4

CLEAR-CACHE='sync; echo 3 | sudo tee /proc/sys/vm/drop_caches'
time_stat: $(TARGET_BENCH_STAT) $(SYNTHETIC_DIR)
	sudo -v
	hyperfine --warmup 3 \
		  --prepare $(CLEAR-CACHE) \
		  --export-markdown bench_stat.md \
		  --parameter-list stat-type stat,statx_all,statx_one './bench_stat root {stat-type}'

time_io: $(TARGET_BENCH_STAT) $(SYNTHETIC_DIR)
	sudo -v
	hyperfine --warmup 3 \
		  --prepare $(CLEAR-CACHE) \
		  --export-markdown bench_io.md \
		  --parameter-list backend sys,uring_batched,uring_batched_sqpoll './bench_io root {backend}'
