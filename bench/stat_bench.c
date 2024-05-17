#include <linux/stat.h>
#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

void dir_walk(const char *base_path, int (*f)(const char *)) {
  char path[PATH_MAX];
  struct dirent *dp;
  DIR *dir = opendir(base_path);

  if (!dir) {
    perror("Error opening directory");
    return;
  }

  while ((dp = readdir(dir)) != NULL) {
    /* Ignore . and .. */
    if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
      sprintf(path, "%s/%s", base_path, dp->d_name);

      if (dp->d_type == DT_DIR) {
        /* printf("Directory: %s\n", path); */
        dir_walk(path, f);
      } else {
        f(path);
      }
    }
  }

  closedir(dir);
}

int stat_read(const char *path) {
  struct stat buf;
  if (stat(path, &buf)) {
    perror("stat");
    return -1;
  }
  return 0;
}

int statx_all_read(const char *path) {
  struct statx buf;
  if (statx(AT_FDCWD, path, 0, STATX_ALL, &buf) == -1) {
    perror("statx");
    return -1;
  }
  return 0;
}

int statx_one_read(const char *path) {
  struct statx buf;
  if (statx(AT_FDCWD, path, 0, STATX_SIZE, &buf) == -1) {
    perror("statx");
    return -1;
  };
  return 0;
}

int main(int argc, char **argv) {
  char *path;
  /* Check that file has been supplied */
  if (argc < 3)
    return 1;

  path = argv[1];

  if (strcmp(argv[2], "stat")) {
    dir_walk(path, stat_read);
  } else if (strcmp(argv[2], "statx_all")) {
    dir_walk(path, statx_all_read);
  } else if (strcmp(argv[2], "statx_one")) {
    dir_walk(path, statx_one_read);
  } else {
    exit(1);
  }

  return 0;
}
