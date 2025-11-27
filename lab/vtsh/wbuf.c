#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORD_LENGTH 8
#define ALPHABET_SIZE 26
#define DECIMAL_BASE 10
#define DEFAULT_ITERATIONS 1U

static const char* const DATA_DIR =
    "/home/pollee/os/os-course/lab/vtsh/bin/loaders/data";

static const char* const OUT_DIR =
    "/home/pollee/os/os-course/lab/vtsh/bin/loaders/out";

static void safe_fclose(FILE* stream, const char* what) {
  if (!stream) {
    return;
  }
  if (fclose(stream) == EOF) {
    perror(what);
  }
}

static void read_count(FILE* file_stream, long long* row_count) {
  int read_items = fscanf(file_stream, "%lld", row_count);
  if (read_items != 1) {
    perror("fscanf row_count");
  }
  int symbol = 0;
  while ((symbol = fgetc(file_stream)) != '\n' && symbol != EOF) {
  }
}

static void read_id_word(
    FILE* file_stream, long long* ind, char word[WORD_LENGTH + 1]
) {
  int read_items = fscanf(file_stream, "%lld %8s", ind, word);
  if (read_items != 2) {
    perror("fscanf id + word");
  }
  word[WORD_LENGTH] = '\0';
}

static long long mark_data_start(FILE* file_stream) {
  long pos = ftell(file_stream);
  return (pos < 0) ? 0 : (long long)pos;
}

static int do_join_files(
    const char* pathA,
    const char* pathB,
    const char* pathOut,
    long long iterations
) {
  FILE* file_A = fopen(pathA, "r");
  if (!file_A) {
    perror("fopen file_A");
    return 1;
  }

  FILE* file_B = fopen(pathB, "r");
  if (!file_B) {
    perror("fopen file_B");
    safe_fclose(file_A, "fclose file_A");
    return 1;
  }

  long long str_numb_A = 0;
  long long str_numb_B = 0;
  read_count(file_A, &str_numb_A);
  read_count(file_B, &str_numb_B);

  long long offile_A = mark_data_start(file_A);
  long long offile_B = mark_data_start(file_B);

  long long matches = 0;

  for (long long it = 0; it < iterations; ++it) {
    matches = 0;

    int seek_result = fseek(file_A, offile_A, SEEK_SET);
    if (seek_result != 0) {
      perror("fseek file_A");
      safe_fclose(file_A, "fclose file_A");
      safe_fclose(file_B, "fclose file_B");
      return 1;
    }

    for (long long i = 0; i < str_numb_A; i++) {
      long long idA = 0;
      char word_A[WORD_LENGTH + 1];
      read_id_word(file_A, &idA, word_A);

      int seek_result_B = fseek(file_B, offile_B, SEEK_SET);
      if (seek_result_B != 0) {
        perror("fseek file_B");
        safe_fclose(file_A, "fclose file_A");
        safe_fclose(file_B, "fclose file_B");
        return 1;
      }

      for (long long j = 0; j < str_numb_B; j++) {
        long long idB = 0;
        char word_B[WORD_LENGTH + 1];
        read_id_word(file_B, &idB, word_B);
        if (idA == idB) {
          matches++;
        }
      }
    }
  }

  FILE* file_out = fopen(pathOut, "w");
  if (!file_out) {
    perror("fopen file_out");
    safe_fclose(file_A, "fclose file_A");
    safe_fclose(file_B, "fclose file_B");
    return 1;
  }

  int print_result = fprintf(file_out, "%lld\n", matches);
  if (print_result < 0) {
    perror("fprintf matches");
    safe_fclose(file_out, "fclose file_out");
    safe_fclose(file_A, "fclose file_A");
    safe_fclose(file_B, "fclose file_B");
    return 1;
  }

  int return_code = fseek(file_A, offile_A, SEEK_SET);
  if (return_code != 0) {
    perror("fseek file_A");
  }

  for (long long i = 0; i < str_numb_A; i++) {
    long long idA = 0;
    char word_A[WORD_LENGTH + 1];
    read_id_word(file_A, &idA, word_A);

    int return_code_B = fseek(file_B, offile_B, SEEK_SET);
    if (return_code_B != 0) {
      perror("fseek file_B");
    }

    for (long long j = 0; j < str_numb_B; j++) {
      long long idB = 0;
      char word_B[WORD_LENGTH + 1];
      read_id_word(file_B, &idB, word_B);
      if (idA == idB) {
        int written = fprintf(file_out, "%lld %s %s\n", idA, word_A, word_B);
        if (written < 0) {
          perror("fprintf idA word_A word_B");
        }
      }
    }
  }

  safe_fclose(file_out, "fclose file_out");
  safe_fclose(file_A, "fclose file_A");
  safe_fclose(file_B, "fclose file_B");

  return 0;
}

int main(int argc, char** argv) {
  if (argc > 4) {
    int print_result =
        fprintf(stderr, "Usage: %s <fileA> <fileB> <iterations>\n", argv[0]);
    if (print_result < 0) {
      perror("fprintf usage error");
    }

    int print_result2 = fprintf(
        stderr,
        "  fileA, fileB are basenames in %s \n"
        "  iterations\n",
        DATA_DIR
    );
    if (print_result2 < 0) {
      perror("fprintf usage line 2");
    }

    return 2;
  }

  const char* fileA_name = argv[1];
  const char* fileB_name = argv[2];

  long long iterations = DEFAULT_ITERATIONS;

  if (argc == 4) {
    const char* iterations_str = argv[3];
    char* endptr = NULL;
    long long parsed = strtoll(iterations_str, &endptr, DECIMAL_BASE);

    if (*endptr != '\0' || parsed <= 0) {
      (void)fprintf(stderr, "Invalid iterations value: %s\n", iterations_str);
      return 1;
    }
    iterations = parsed;
  }

  char path_A[PATH_MAX];
  char path_B[PATH_MAX];
  char path_out[PATH_MAX];

  int lenA = snprintf(path_A, sizeof(path_A), "%s/%s", DATA_DIR, fileA_name);
  if (lenA < 0 || (size_t)lenA >= sizeof(path_A)) {
    perror("snprintf path_A");
    return 1;
  }

  int lenB = snprintf(path_B, sizeof(path_B), "%s/%s", DATA_DIR, fileB_name);
  if (lenB < 0 || (size_t)lenB >= sizeof(path_B)) {
    perror("snprintf path_B");
    return 1;
  }

  int lenOut = snprintf(
      path_out,
      sizeof(path_out),
      "%s/out_%s_%s.txt",
      OUT_DIR,
      fileA_name,
      fileB_name
  );
  if (lenOut < 0 || (size_t)lenOut >= sizeof(path_out)) {
    perror("snprintf path_out");
    return 1;
  }

  int res = do_join_files(path_A, path_B, path_out, iterations);
  return res;
}
