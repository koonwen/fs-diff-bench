#define _GNU_SOURCE
#include <fcntl.h>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <linux/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv) {
  char *path;
  struct io_uring ring;
  struct io_uring_sqe *sqe;
  struct statx buf;
  struct io_uring_cqe *cqe;

  if (io_uring_queue_init(10, &ring, 0)) {
    perror("io_uring_queue_initn");
    exit(1);
  };

  if (argc < 2)
    exit(1);

  path = argv[1];

  sqe = io_uring_get_sqe(&ring);

  io_uring_prep_statx(sqe, AT_FDCWD, path, 0, STATX_SIZE, &buf);
  io_uring_submit(&ring);

  if (io_uring_wait_cqe(&ring, &cqe)) {
    perror("io_uring_wait_cqe");
    exit(1);
  };

  printf("%s: size %lld\n", path, buf.stx_size);
  return 0;
}
