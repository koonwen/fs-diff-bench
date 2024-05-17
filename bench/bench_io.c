#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define QUEUE_DEPTH 64
#define BATCH_SIZE 32

void do_sys_walk(const char *base_path) {
  char path[PATH_MAX];
  struct dirent *dp;
  DIR *dir = opendir(base_path);

  if (!dir) {
    perror("Error opening directory");
    exit(1);
  }

  while ((dp = readdir(dir)) != NULL) {
    /* Ignore . and .. */
    if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
      sprintf(path, "%s/%s", base_path, dp->d_name);

      if (dp->d_type == DT_DIR) {
        do_sys_walk(path);
      } else {
        struct statx buf;
        if (statx(AT_FDCWD, path, 0, STATX_ALL, &buf) == -1) {
          perror("statx");
          exit(1);
        };
	printf("size = %Ld\n", buf.stx_size);
      }
    }
  }

  closedir(dir);
}

/* int do_uring_walk(const char *base_path) { */
/*   char *path; */
/*   struct io_uring ring; */
/*   struct io_uring_sqe *sqe; */
/*   struct statx buf; */
/*   struct io_uring_cqe *cqe; */

/*   if (io_uring_queue_init(10, &ring, 0)) { */
/*     perror("io_uring_queue_init"); */
/*     exit(1); */
/*   }; */

/*   sqe = io_uring_get_sqe(&ring); */

/*   io_uring_prep_statx(sqe, AT_FDCWD, path, 0, STATX_SIZE, &buf); */
/*   io_uring_submit(&ring); */

/*   if (io_uring_wait_cqe(&ring, &cqe)) { */
/*     perror("io_uring_wait_cqe"); */
/*     exit(1); */
/*   }; */

/*   return 0; */
/* } */

void process_statx(struct io_uring *ring, int fd, const char *path) {
  struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
  if (!sqe) {
    fprintf(stderr, "Failed to get SQE\n");
    exit(EXIT_FAILURE);
  }

  struct statx *statxbuf = malloc(sizeof(struct statx));
  if (!statxbuf) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  io_uring_prep_statx(sqe, fd, path, AT_EMPTY_PATH | AT_NO_AUTOMOUNT,
                      STATX_BASIC_STATS, statxbuf);
  io_uring_sqe_set_data(sqe, statxbuf);
}

void process_directory(struct io_uring *ring, const char *dirpath) {
  DIR *dir = opendir(dirpath);
  if (!dir) {
    perror("opendir");
    return;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", dirpath, entry->d_name);

    if (entry->d_type == DT_DIR) {
      process_directory(ring, path);
    } else {
      process_statx(ring, -1, path);
    }

    // Submit requests in batches
    if (io_uring_sq_ready(ring) >= BATCH_SIZE) {
      io_uring_submit(ring);
    }
  }

  closedir(dir);
}

void reap_completions(struct io_uring *ring) {
  struct io_uring_cqe *cqe;
  while (io_uring_peek_cqe(ring, &cqe) == 0) {
    struct statx *statxbuf = io_uring_cqe_get_data(cqe);
    if (cqe->res < 0) {
      fprintf(stderr, "statx failed: %s\n", strerror(-cqe->res));
    } else {
      // Process the statx result here (e.g., print or log it)
      printf("File size: %llu\n", statxbuf->stx_size);
    }

    free(statxbuf);
    io_uring_cqe_seen(ring, cqe);
  }

}
void do_uring_batched_io(char *base_path) {

  struct io_uring ring;
  if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) < 0) {
    perror("io_uring_queue_init");
    exit(1);
  }

  process_directory(&ring, base_path);

  // Submit any remaining requests
  io_uring_submit(&ring);

  // Reap leftover completions
  reap_completions(&ring);

  io_uring_queue_exit(&ring);

}

int main(int argc, char **argv) {
  char *path;
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <directory> <io-type>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  path = argv[1];

  if (strcmp(argv[2], "sys") == 0) {
    do_sys_walk(path);
  } else if (strcmp(argv[2], "uring_batched") == 0) {
    do_uring_batched_io(path);
    /* } else if (strcmp(argv[2], "statx_one")) { */
    /*   dir_walk(path, statx_one_read); */
  } else {
    exit(1);
  }
  return 0;
}
