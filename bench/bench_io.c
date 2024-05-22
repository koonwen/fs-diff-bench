#include <linux/limits.h>
#include <linux/stat.h>
#define _GNU_SOURCE
#include <assert.h>
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
#define DIRECTORY_SZ 4096
static uint64_t size_in_bytes = 0;

void do_sys_walk(const char *base_path) {
  char path[PATH_MAX];
  struct dirent *dp;
  DIR *dir = opendir(base_path);
  size_in_bytes += DIRECTORY_SZ;

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
        size_in_bytes += buf.stx_size;
        /* printf("%s: size=%Ld\n", path, buf.stx_size); */
      }
    }
  }

  closedir(dir);
}

struct statx_info {
  char path_name[PATH_MAX];
  struct statx statxbuf;
};

void do_uring_batched_walk(const char *base_path, struct io_uring *ring) {
  struct io_uring_sqe *sqe;
  struct io_uring_cqe *cqe;
  struct statx_info *statx_info;
  char path[PATH_MAX];
  struct dirent *dp;
  DIR *dir = opendir(base_path);
  size_in_bytes += DIRECTORY_SZ;

  if (!dir) {
    perror("Error opening directory");
    exit(1);
  }

  while ((dp = readdir(dir)) != NULL) {
    /* Ignore . and .. */
    if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
      sprintf(path, "%s/%s", base_path, dp->d_name);

      if (dp->d_type == DT_DIR) {
        do_uring_batched_walk(path, ring);
      } else {

        while ((sqe = io_uring_get_sqe(ring)) == NULL) {

          /* Submit & Drain everything */
          for (int count = io_uring_submit(ring); count > 0; count--) {

            if ((io_uring_wait_cqe(ring, &cqe)) != 0) {
              perror("io_uring_wait_cqe");
              exit(1);
            };

            statx_info = (struct statx_info *)io_uring_cqe_get_data(cqe);
            /* printf("%s: size=%Ld\n", statx_info->path_name, */
            /*        statx_info->statxbuf.stx_size); */
            size_in_bytes += statx_info->statxbuf.stx_size;

            io_uring_cqe_seen(ring, cqe);

            free(statx_info);
          };
        }

        /* Add statx call to submission queue */
        statx_info = (struct statx_info *)malloc(sizeof(*statx_info));
        strcpy(statx_info->path_name, path);
        /* Need to pass statx_info->path_name because path is reused */
        io_uring_prep_statx(sqe, AT_FDCWD, statx_info->path_name, 0, STATX_ALL,
                            &statx_info->statxbuf);
        io_uring_sqe_set_data(sqe, statx_info);
      }
    }
  }
  closedir(dir);
}

static int count = 0;
static int reaped = 0;

void do_uring_batched_sqpoll_walk(const char *base_path, struct io_uring *ring) {
  /* Keep track of number of submissions */
  struct io_uring_sqe *sqe;
  struct io_uring_cqe *cqe;
  struct statx_info *statx_info;
  char path[PATH_MAX];
  struct dirent *dp;
  DIR *dir = opendir(base_path);
  size_in_bytes += DIRECTORY_SZ;

  if (!dir) {
    perror("Error opening directory");
    exit(1);
  }

  while ((dp = readdir(dir)) != NULL) {
    /* Ignore . and .. */
    if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
      sprintf(path, "%s/%s", base_path, dp->d_name);

      if (dp->d_type == DT_DIR) {
        do_uring_batched_sqpoll_walk(path, ring);
      } else {

        /* Try clearing the completion queue as much as possible */
        while ((io_uring_peek_cqe(ring, &cqe)) == 0) {
          statx_info = (struct statx_info *)io_uring_cqe_get_data(cqe);

          /* printf("%s: size=%Ld\n", statx_info->path_name, */
          /*        statx_info->statxbuf.stx_size); */
          size_in_bytes += statx_info->statxbuf.stx_size;

          io_uring_cqe_seen(ring, cqe);

          free(statx_info);
          reaped++;
        }

        /* Busywait for sqe, io_uring_sqring_wait buggy */
        while ((sqe = io_uring_get_sqe(ring)) == NULL) {
        };

        /* Add statx call to submission queue */
        statx_info = (struct statx_info *)malloc(sizeof(*statx_info));
        strcpy(statx_info->path_name, path);
        /* Need to pass statx_info->path_name because path is reused */
        io_uring_prep_statx(sqe, AT_FDCWD, statx_info->path_name, 0, STATX_ALL,
                            &statx_info->statxbuf);
        io_uring_sqe_set_data(sqe, statx_info);
        io_uring_submit(ring);
        count++;
      }
    }
  }
  closedir(dir);
}

void drain_completion_queue(struct io_uring *ring, bool sq_poll) {

  struct io_uring_cqe *cqe;
  struct statx_info *statx_info;
  int left;

  if (sq_poll) {
    io_uring_submit(ring);
    left = count - reaped;
  } else {
    /* Submit and flush whatever's left */
    left = io_uring_submit(ring);
  }
  for (; left > 0; left--) {
    if (io_uring_wait_cqe(ring, &cqe) != 0) {
      perror("io_uring_wait_cqe");
      exit(1);
    };
    statx_info = (struct statx_info *)io_uring_cqe_get_data(cqe);

    /* printf("[Draining] %s: size=%Ld\n", statx_info->path_name, */
    /*        statx_info->statxbuf.stx_size); */
    size_in_bytes += statx_info->statxbuf.stx_size;

    io_uring_cqe_seen(ring, cqe);

    free(statx_info);
  }
  return;
}

int main(int argc, char **argv) {
  char *path;
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <directory> <io-type>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  path = argv[1];

  if (strcmp(argv[2], "sys") == 0) {
    printf("Running sys\n\n");
    do_sys_walk(path);
  } else if (strcmp(argv[2], "uring_batched") == 0) {
    printf("Running uring_batched\n\n");
    struct io_uring ring;

    if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) != 0) {
      perror("io_uring_queue_init");
      exit(1);
    };

    do_uring_batched_walk(path, &ring);

    drain_completion_queue(&ring, false);
    io_uring_queue_exit(&ring);
  } else if (strcmp(argv[2], "uring_batched_sqpoll") == 0) {
    printf("Running uring_batched_sqpoll\n\n");
    struct io_uring ring;
    if (io_uring_queue_init(QUEUE_DEPTH, &ring,
                            IORING_FEAT_NODROP | IORING_SETUP_SQPOLL |
                                IORING_FEAT_SQPOLL_NONFIXED) != 0) {
      perror("io_uring_queue_init");
      exit(1);
    };

    do_uring_batched_sqpoll_walk(path, &ring);

    drain_completion_queue(&ring, true);
    io_uring_queue_exit(&ring);
  } else {
    exit(1);
  }
  printf("Size of Filesystem = %lu Bytes\n", size_in_bytes);
  return 0;
}
