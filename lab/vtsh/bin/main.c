#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static long long exec_time(struct timespec start, struct timespec finish) {
  return (finish.tv_sec - start.tv_sec) * 1000000000LL +
         (finish.tv_nsec - start.tv_nsec);
}

static void trim(char* str) {
  size_t count = strlen(str);
  while (count && (str[count - 1] == ' ' || str[count - 1] == '\t' ||
                   str[count - 1] == '\n' || str[count - 1] == '\r'))
    str[--count] = 0;
  size_t i = 0;
  while (str[i] == ' ' || str[i] == '\t' || str[i] == '\r')
    i++;
  if (i)
    memmove(str, str + i, count - i + 1);
}

static void split_words(char* line, char** argv, int* argc, int maxv) {
  *argc = 0;
  char* word = strtok(line, " \t\r");
  while (word && *argc < maxv - 1) {
    argv[(*argc)++] = word;
    word = strtok(NULL, " \t\r");
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

  if (errno == ENOENT) {
    (void)!write(
        STDOUT_FILENO, "Command not found\n", strlen("Command not found\n")
    );
    _exit(127);
  } else {
    dprintf(STDERR_FILENO, "execvp: %s: %s\n", ctx->argv[0], strerror(errno));
    _exit(127);
  }
}

static int run_argv(char** argv) {
  if (!argv[0])
    return 0;

  if (strcmp(argv[0], "exit") == 0) {
    exit(0);
  }

  if (strcmp(argv[0], "cd") == 0) {
    const char* dir = argv[1] ? argv[1] : getenv("HOME");
    if (!dir)
      dir = ".";
    if (chdir(dir) != 0) {
      perror("cd");
      return 1;
    }
    return 0;
  }

  struct timespec start, finish;
  clock_gettime(CLOCK_MONOTONIC, &start);

  const size_t STACK_SIZE = 1 << 20;
  void* stack = malloc(STACK_SIZE);
  if (!stack) {
    perror("malloc");
    return 127;
  }
  void* stack_top = (char*)stack + STACK_SIZE;

  struct child_ctx ctx = {.argv = argv};

  pid_t pid = clone(child_proc_func, stack_top, SIGCHLD, &ctx);
  if (pid < 0) {
    perror("clone");
    free(stack);
    return 127;
  }

  int status = 0;
  if (waitpid(pid, &status, 0) == -1) {
    perror("waitpid");
  }
  clock_gettime(CLOCK_MONOTONIC, &finish);
  double exec_time_val = exec_time(start, finish) / 1e6;

  if (WIFEXITED(status) || WIFSIGNALED(status) || WIFSTOPPED(status)) {
    fprintf(stderr, "time=%.3f ms\n", exec_time_val);
  }

  int res = WIFEXITED(status) ? WEXITSTATUS(status) : 127;
  free(stack);
  return res;
}

static int parse_and_segments(char* line, char* segs[], int maxseg) {
  int seg_count = 0;
  char* ptr = line;
  while (*ptr && seg_count < maxseg) {
    char* start = ptr;
    while (*ptr) {
      if (*ptr == '\'' || *ptr == '"') {
        char quote = *ptr++;  //кавычки - quote
        while (*ptr && *ptr != quote)
          ptr++;
        if (*ptr)
          ptr++;
        continue;
      }
      if (ptr[0] == '&' && ptr[1] == '&')
        break;
      ptr++;
    }
    size_t len = (size_t)(ptr - start);
    while (len && (start[len - 1] == ' ' || start[len - 1] == '\t' ||
                   start[len - 1] == '\r'))
      len--;
    while (*start == ' ' || *start == '\t' || *start == '\r') {
      start++;
      len--;
    }
    if (len) {
      start[len] = 0;
      segs[seg_count++] = start;
    }
    if (ptr[0] == '&' && ptr[1] == '&')
      ptr += 2;
    while (*ptr == ' ' || *ptr == '\t' || *ptr == '\r')
      ptr++;
  }
  return seg_count;
}

int main(void) {
  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);
  signal(SIGINT, SIG_IGN);

  char* line = NULL;
  size_t cap = 0;


  while (1) {
    fprintf(stderr, "vtsh> ");
    fflush(stderr);
    ssize_t read_len = getline(&line, &cap, stdin);
    if (read_len < 0)
      break;
    trim(line);
    if (!*line)
      continue;

    char* segs[128];
    int nseg = parse_and_segments(line, segs, 128);
    int last_status = 0;

    for (int i = 0; i < nseg; i++) {
      if (i > 0 && last_status != 0)
        break;
      char buf[4096];
      strncpy(buf, segs[i], sizeof(buf) - 1);
      buf[sizeof(buf) - 1] = 0;

      char* argv[128];
      int argc = 0;
      split_words(buf, argv, &argc, 128);
      last_status = run_argv(argv);
    }
  }
  free(line);
  return 0;
}
