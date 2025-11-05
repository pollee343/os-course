#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define COMMAND_NOT_FOUND 127
#define MEGABYTE 1048576
#define MAX_SEGS_OR_ARGS 128
#define SEG_BUF 4096

static long long exec_time(struct timespec start, struct timespec finish) {
  const long long NANOSECONDS_IN_SECOND = (long long)1e9;
  return (finish.tv_sec - start.tv_sec) * NANOSECONDS_IN_SECOND +
         (finish.tv_nsec - start.tv_nsec);
}

static void trim(char* str) {
  size_t count = strlen(str);
  while (count && (str[count - 1] == ' ' || str[count - 1] == '\t' ||
                   str[count - 1] == '\n' || str[count - 1] == '\r')) {
    str[--count] = 0;
  }
  size_t ind = 0;
  while (str[ind] == ' ' || str[ind] == '\t' || str[ind] == '\r') {
    ind++;
  }
  if (ind) {
    memmove(str, str + ind, count - ind + 1);
  }
}

static void split_words(char* line, char** argv, int* argc, int maxv) {
  *argc = 0;
  char* saveptr = NULL;
  char* word = strtok_r(line, " \t\r", &saveptr);
  while (word && *argc < maxv - 1) {
    argv[(*argc)++] = word;
    word = strtok_r(NULL, " \t\r", &saveptr);
  }
  argv[*argc] = NULL;
}

struct child_ctx {
  char** argv;
};

static int child_proc_func(void* arg) {
  struct child_ctx* ctx = (struct child_ctx*)arg;
  if (ctx->argv[0] && strcmp(ctx->argv[0], "./shell") == 0) {
    char self[PATH_MAX];
    ssize_t path_len = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (path_len >= 0) {
      self[path_len] = '\0';
      ctx->argv[0] = self;
      execv(self, ctx->argv);
    }
  }

  execvp(ctx->argv[0], ctx->argv);

  if (errno == COMMAND_NOT_FOUND) {
    (void)!write(
        STDOUT_FILENO, "Command not found\n", sizeof("Command not found\n") - 1
    );
    _exit(COMMAND_NOT_FOUND);
  } else {
    int err = errno;
    static const int ERR_TEXT_BUFFER_SIZE = 256;
    char buf[ERR_TEXT_BUFFER_SIZE];
    strerror_r(err, buf, sizeof(buf));

    dprintf(STDERR_FILENO, "execvp: %s: %s\n", ctx->argv[0], buf);
    _exit(COMMAND_NOT_FOUND);
  }
}

static int run_argv(char** argv) {
  if (!argv[0]) {
    return 0;
  }

  if (strcmp(argv[0], "exit") == 0) {
    _exit(0);
  }

  if (strcmp(argv[0], "cd") == 0) {
    const char* dir = argv[1] ? argv[1] : getenv("HOME");
    if (!dir) {
      dir = ".";
    }
    if (chdir(dir) != 0) {
      perror("cd");
      return 1;
    }
    return 0;
  }

  struct timespec start;
  struct timespec finish;
  clock_gettime(CLOCK_MONOTONIC, &start);

  const size_t STACK_SIZE = MEGABYTE;

  void* stack = malloc(STACK_SIZE);
  if (!stack) {
    perror("malloc");
    return COMMAND_NOT_FOUND;
  }
  void* stack_top = (char*)stack + STACK_SIZE;

  struct child_ctx ctx = {.argv = argv};

  pid_t pid = clone(child_proc_func, stack_top, SIGCHLD, &ctx);
  if (pid < 0) {
    perror("clone");
    free(stack);
    return COMMAND_NOT_FOUND;
  }

  int status = 0;
  if (waitpid(pid, &status, 0) == -1) {
    perror("waitpid");
  }
  clock_gettime(CLOCK_MONOTONIC, &finish);

  unsigned ustatus = (unsigned)status;

  const double MICROSECONDS_IN_SECOND = 1e6;
  double exec_time_val =
      (double)exec_time(start, finish) / MICROSECONDS_IN_SECOND;

  if (WIFEXITED(ustatus) || WIFSIGNALED(ustatus) || WIFSTOPPED(ustatus)) {
    (void)fprintf(stderr, "time=%.3f ms\n", exec_time_val);
  }

  int res = WIFEXITED(ustatus) ? WEXITSTATUS(ustatus) : COMMAND_NOT_FOUND;
}

static int parse_and_segments(char* line, char* segs[], int maxseg) {
  int seg_count = 0;
  char* ptr = line;
  while (*ptr && seg_count < maxseg) {
    char* start = ptr;
    while (*ptr) {
      if (*ptr == '\'' || *ptr == '"') {
        char quote = *ptr++;
        while (*ptr && *ptr != quote) {
          ptr++;
        }
        if (*ptr) {
          ptr++;
        }
        continue;
      }
      if (ptr[0] == '&' && ptr[1] == '&') {
        break;
      }
      ptr++;
    }
    size_t len = (size_t)(ptr - start);
    while (len && (start[len - 1] == ' ' || start[len - 1] == '\t' ||
                   start[len - 1] == '\r')) {
      len--;
    }
    while (*start == ' ' || *start == '\t' || *start == '\r') {
      start++;
      len--;
    }
    if (len) {
      start[len] = 0;
      segs[seg_count++] = start;
    }
    if (ptr[0] == '&' && ptr[1] == '&') {
      ptr += 2;
    }
    while (*ptr == ' ' || *ptr == '\t' || *ptr == '\r') {
      ptr++;
    }
  }
  return seg_count;
}

int main(void) {
  (void)setvbuf(stdin, NULL, _IONBF, 0);
  (void)setvbuf(stdout, NULL, _IONBF, 0);
  (void)signal(SIGINT, SIG_IGN);

  char* line = NULL;
  size_t cap = 0;

  while (1) {
    (void)fprintf(stderr, "vtsh> ");
    (void)fflush(stderr);
    ssize_t read_len = getline(&line, &cap, stdin);
    if (read_len < 0) {
      break;
    }
    trim(line);
    if (!*line) {
      continue;
    }

    char* segs[MAX_SEGS_OR_ARGS];
    int nseg = parse_and_segments(line, segs, MAX_SEGS_OR_ARGS);
    int last_status = 0;

    for (int i = 0; i < nseg; i++) {
      if (i > 0 && last_status != 0) {
        break;
      }
      char buf[SEG_BUF];
      strncpy(buf, segs[i], sizeof(buf) - 1);
      buf[sizeof(buf) - 1] = 0;

      char* argv[MAX_SEGS_OR_ARGS];
      int argc = 0;
      split_words(buf, argv, &argc, MAX_SEGS_OR_ARGS);
      last_status = run_argv(argv);
    }
  }
  free(line);
  return 0;
}
