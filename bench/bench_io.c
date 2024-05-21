#include <linux/limits.h>
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
        printf("%s\n", path);
        do_sys_walk(path);
      } else {
        struct statx buf;
        if (statx(AT_FDCWD, path, 0, STATX_ALL, &buf) == -1) {
          perror("statx");
          exit(1);
        };
      }
    }
  }

  closedir(dir);
}

#define QUEUE_DEPTH 3
struct statx_info {
  char path_name[PATH_MAX];
  struct statx statxbuf;
  struct io_uring_cqe cqe;
};

struct io_uring_cqe **initialize_cqes(int queue_depth) {

  struct io_uring_cqe *cqes =
    (struct io_uring_cqe *) malloc((sizeof(struct io_uring_cqe) * queue_depth));

  struct io_uring_cqe **cqes_ptr =
    (struct io_uring_cqe **) malloc((sizeof(struct io_uring_cqe*) * queue_depth));

  for (int i = 0; i < queue_depth; i++) {
        cqes_ptr[i] = &cqes[i];
  };
  /* Need to remember to free this */
  return cqes_ptr;
}



void do_uring_batched_walk(const char *base_path, struct io_uring *ring, struct io_uring_cqe **cqe_ptr) {
  static int count = 0;
  static struct statx statx_buf[QUEUE_DEPTH];
  static path_name name_buf[QUEUE_DEPTH];
  char *path = name_buf[count];
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
        printf("%s\n", path);
        do_uring_batched_walk(path, ring);
      } else {
        printf("count=%d\n", count);
        if (count < QUEUE_DEPTH) {
          struct io_uring_sqe *sqe;
          sqe = io_uring_get_sqe(ring);
          sqe->user_data = count;
          io_uring_prep_statx(sqe, AT_FDCWD, path, 0, STATX_SIZE,
                              &statx_buf[count]);
          count++;
        } else {
          /* drain completion queue */
          if (!io_uring_submit_and_get_events(ring)) {
            perror("io_uring_submit_and_get_events");
            exit(1);
          };
          io_uring_peek_batch_cqe(ring, cqe_ptr, QUEUE_DEPTH);
          for (int i = 0; i < QUEUE_DEPTH; i++) {
            printf("Size of %s: %Ld, CQE user_data=%Lu", name_buf[i],
                   statx_buf[i].stx_size, cqe_ptr[i]->user_data);
            io_uring_cqe_seen(ring, cqe_ptr[i]);
          }
          count = 0;
        }
      }
    }
  }
  closedir(dir);
}

void reap_completions(struct io_uring *ring, struct io_uring_cqe **cqe_ptr, int count){
  /* Last submit and read */
  if (count > 0) {
    if (io_uring_submit_and_get_events(ring)) {
      perror("io_uring_submit_and_get_events");
      exit(1);
    }
    io_uring_peek_batch_cqe(ring, cqe_ptr, QUEUE_DEPTH);
    for (int i = 0; i < QUEUE_DEPTH; i++) {
      printf("Size of %s: %Ld, CQE user_data=%Lu", name_buf[i],
             statx_buf[i].stx_size, cqe_ptr[i]->user_data);
      io_uring_cqe_seen(ring, cqe_ptr[i]);
    }
    count = 0;
  }
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
    struct io_uring ring;
    io_uring_queue_init(QUEUE_DEPTH, &ring, 0);
    do_uring_batched_walk(path, &ring);
  } else {
    exit(1);
  }
  return 0;
}
