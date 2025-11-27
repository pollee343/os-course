#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DECIMAL_BASE 10
#define BYTE_MASK_8BIT 0xFFu
#define DIRECT_ALIGNMENT 4096u

struct config {
  bool rw;  // true = read
  size_t block_size;
  size_t block_count;
  const char* file_path;
  long range_start;
  long range_end;
  bool use_direct;
  bool access_seq_or_random;  // true = sequence
  long iterations;
};

static void print_usage(const char* prog) {
  int usage_result = fprintf(
      stderr,
      "Usage:\n"
      "  %s rw=<read|write> block_size=<N> block_count=<N>\n"
      "     file=<path> range=<start-end> direct=<on|off>\n"
      "     type=<sequence|random> iters=<N>\n",
      prog
  );
  if (usage_result < 0) {
    perror("fprintf usage");
  }
}

static bool parse_rw(const char* val, bool* out) {
  if (strcmp(val, "read") == 0) {
    *out = true;
    return true;
  }
  if (strcmp(val, "write") == 0) {
    *out = false;
    return true;
  }
  return false;
}

static bool parse_bool_on_off(const char* val, bool* out) {
  if (strcmp(val, "on") == 0) {
    *out = true;
    return true;
  }
  if (strcmp(val, "off") == 0) {
    *out = false;
    return true;
  }
  return false;
}

static bool parse_access(const char* val, bool* out) {
  if (strcmp(val, "sequence") == 0) {
    *out = true;
    return true;
  }
  if (strcmp(val, "random") == 0) {
    *out = false;
    return true;
  }
  return false;
}

static bool parse_range(const char* val, long* start, long* end) {
  char* dash = strchr(val, '-');
  if (!dash) {
    return false;
  }
  char* endptr = NULL;
  *dash = '\0';
  long long start_value = strtoll(val, &endptr, DECIMAL_BASE);
  if (*endptr != '\0') {
    *dash = '-';
    return false;
  }
  long long end_value = strtoll(dash + 1, &endptr, DECIMAL_BASE);
  if (*endptr != '\0') {
    *dash = '-';
    return false;
  }
  *dash = '-';

  if (start_value < 0 || end_value < 0) {
    return false;
  }
  *start = (long)start_value;
  *end = (long)end_value;
  return true;
}

static bool parse_size_t(const char* val, size_t* out) {
  char* endptr = NULL;
  unsigned long long tmp = strtoull(val, &endptr, DECIMAL_BASE);
  if (*endptr != '\0') {
    return false;
  }
  *out = (size_t)tmp;
  return true;
}

static bool parse_args(int argc, char** argv, struct config* cfg) {
  cfg->file_path = NULL;
  cfg->block_size = 0;
  cfg->block_count = 0;
  cfg->range_start = 0;
  cfg->range_end = 0;
  cfg->use_direct = false;
  cfg->rw = true;
  cfg->access_seq_or_random = true;
  cfg->iterations = 1;

  int const AMOUNT_ARGS = 7;

  if (argc < AMOUNT_ARGS) {
    return false;
  }

  for (int i = 1; i < argc; ++i) {
    char* arg = argv[i];
    char* equal_sign = strchr(arg, '=');
    if (!equal_sign) {
      int print_result = fprintf(stderr, "Invalid argument: %s\n", arg);
      if (print_result < 0) {
        perror("fprintf invalid argument");
      }
      return false;
    }
    *equal_sign = '\0';
    const char* key = arg;
    const char* val = equal_sign + 1;

    if (strcmp(key, "rw") == 0) {
      if (!parse_rw(val, &cfg->rw)) {
        int print_result = fprintf(stderr, "Invalid rw value: %s\n", val);
        if (print_result < 0) {
          perror("fprintf invalid rw");
        }
        return false;
      }
    } else if (strcmp(key, "block_size") == 0) {
      if (!parse_size_t(val, &cfg->block_size)) {
        int print_result = fprintf(stderr, "Invalid block_size: %s\n", val);
        if (print_result < 0) {
          perror("fprintf invalid block_size");
        }
        return false;
      }
    } else if (strcmp(key, "block_count") == 0) {
      if (!parse_size_t(val, &cfg->block_count)) {
        int print_result = fprintf(stderr, "Invalid block_count: %s\n", val);
        if (print_result < 0) {
          perror("fprintf invalid block_count");
        }
        return false;
      }
    } else if (strcmp(key, "file") == 0) {
      cfg->file_path = val;
    } else if (strcmp(key, "range") == 0) {
      if (!parse_range(val, &cfg->range_start, &cfg->range_end)) {
        int print_result = fprintf(stderr, "Invalid range: %s\n", val);
        if (print_result < 0) {
          perror("fprintf invalid range");
        }
        return false;
      }
    } else if (strcmp(key, "direct") == 0) {
      if (!parse_bool_on_off(val, &cfg->use_direct)) {
        int print_result = fprintf(stderr, "Invalid direct: %s\n", val);
        if (print_result < 0) {
          perror("fprintf invalid direct");
        }
        return false;
      }
    } else if (strcmp(key, "type") == 0) {
      if (!parse_access(val, &cfg->access_seq_or_random)) {
        int print_result = fprintf(stderr, "Invalid type: %s\n", val);
        if (print_result < 0) {
          perror("fprintf invalid type");
        }
        return false;
      }
    } else if (strcmp(key, "iters") == 0) {
      size_t tmp = 0;
      if (!parse_size_t(val, &tmp)) {
        (void)fprintf(stderr, "Invalid iters: %s\n", val);
        return false;
      }
      cfg->iterations = (long)tmp;
    }

    else {
      int print_result = fprintf(stderr, "Unknown argument key: %s\n", key);
      if (print_result < 0) {
        perror("fprintf unknown key");
      }
      return false;
    }
  }

  if (!cfg->file_path) {
    int print_result = fprintf(stderr, "file=<path> is required\n");
    if (print_result < 0) {
      perror("file=<path> is required\n");
    }
    return false;
  }

  if (cfg->block_size == 0 || cfg->block_count == 0) {
    int print_result =
        fprintf(stderr, "block_size and block_count must be > 0\n");
    if (print_result < 0) {
      perror("block_size and block_count must be > 0\n");
    }
    return false;
  }

  if (cfg->use_direct && (cfg->block_size % DIRECT_ALIGNMENT != 0)) {
    int print_result =
        fprintf(stderr, "For direct=on block_size must be multiple of 4096\n");
    if (print_result < 0) {
      perror("For direct=on block_size must be multiple of 4096\n");
    }
    return false;
  }

  return true;
}

int main(int argc, char** argv) {
  struct config cfg;
  if (!parse_args(argc, argv, &cfg)) {
    print_usage(argv[0]);
    return 1;
  }

  int flags = 0;
  if (cfg.rw) {
    flags = O_RDONLY;
  } else {
    flags = (int)((unsigned)O_WRONLY | (unsigned)O_CREAT);
  }

  if (cfg.use_direct) {
    flags |= (unsigned)O_DIRECT;  // NOLINT(hicpp-signed-bitwise)
  }

  const mode_t FILE_MODE = 0644;
  int file_descriptor = open(cfg.file_path, flags, FILE_MODE);
  if (file_descriptor < 0) {
    perror("open");
    return 1;
  }

  struct stat file_stat;
  if (fstat(file_descriptor, &file_stat) != 0) {
    perror("fstat");
    close(file_descriptor);
    return 1;
  }

  long file_size = file_stat.st_size;

  if (cfg.range_start == 0 && cfg.range_end == 0) {
    if (!cfg.rw && file_size == 0) {
      long desired = (long)cfg.block_size * (long)cfg.block_count;
      if (ftruncate(file_descriptor, desired) != 0) {
        perror("ftruncate");
        close(file_descriptor);
        return 1;
      }
      file_size = desired;
    } else {
      if (file_size == 0) {
        int print_result = fprintf(
            stderr, "File is empty and range=0-0 for read: nothing to do\n"
        );
        ;
        if (print_result < 0) {
          perror("File is empty and range=0-0 for read: nothing to do\n");
          close(file_descriptor);
          return 1;
        }
      }
      cfg.range_start = 0;
      cfg.range_end = file_size;
    }
  }

  if ((cfg.range_end <= cfg.range_start) && (cfg.range_end != 0)) {
    int print_result = fprintf(stderr, "Invalid range: end <= start\n");
    if (print_result < 0) {
      perror("Invalid range: end <= start");
    }
    close(file_descriptor);
    return 1;
  }

  long range_len = cfg.range_end - cfg.range_start;

  if (range_len < (long)cfg.block_size) {
    (void)fprintf(
        stderr,
        "Range is smaller than block_size: file=\"%s\", range=%ld-%ld "
        "(len=%ld), block_size=%zu\n",
        cfg.file_path,
        cfg.range_start,
        cfg.range_end,
        range_len,
        cfg.block_size
    );
    close(file_descriptor);
    return 1;
  }

  long blocks_in_range = range_len / (long)cfg.block_size;
  if (blocks_in_range <= 0) {
    int print_result = fprintf(stderr, "blocks_in_range <= 0\n");
    if (print_result < 0) {
      perror("blocks_in_range <= 0");
    }
    close(file_descriptor);
    return 1;
  }

  void* buf = NULL;
  size_t alignment = cfg.use_direct ? DIRECT_ALIGNMENT : sizeof(void*);
  if (posix_memalign(&buf, alignment, cfg.block_size) != 0) {
    int print_result = fprintf(stderr, "posix_memalign failed\n");
    if (print_result < 0) {
      perror("posix_memalign failed\n");
    }
    close(file_descriptor);
    return 1;
  }

  {
    uint8_t* byte_buffer = (uint8_t*)buf;
    for (size_t ind = 0; ind < cfg.block_size; ++ind) {
      byte_buffer[ind] = (uint8_t)(ind & BYTE_MASK_8BIT);
    }
  }

  const unsigned int seed1 = 123456U;
  unsigned int seed = seed1;  // костыль, разобраться что не так

  long current_block = 0;

  for (size_t iter = 0; iter < (size_t)cfg.iterations; ++iter) {
    for (size_t i = 0; i < cfg.block_count; ++i) {
      long block_index = 0;

      if (cfg.access_seq_or_random) {
        block_index = current_block;
        current_block++;
        if (current_block >= blocks_in_range) {
          current_block = 0;
        }
      } else {
        block_index = (long)(rand_r(&seed) % (unsigned int)blocks_in_range);
      }

      long offset = cfg.range_start + block_index * (long)cfg.block_size;

      ssize_t numb = 0;
      if (cfg.rw) {
        numb = pread(file_descriptor, buf, cfg.block_size, offset);
      } else {
        numb = pwrite(file_descriptor, buf, cfg.block_size, offset);
      }

      if (numb < 0) {
        perror("pread/pwrite");
        iter = (size_t)cfg.iterations;
        break;
      }
      if ((size_t)numb < cfg.block_size) {
        iter = (size_t)cfg.iterations;
        break;
      }
    }
  }

  free(buf);
  close(file_descriptor);

  return 0;
}
