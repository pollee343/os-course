#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* OUT_PATH = "./bin/loaders/data/out_auto.txt";
static const char* TMP_A = "A.txt";
static const char* TMP_B = "B.txt";

static void make_word(char* word, unsigned seed) {
  for (int i = 0; i < 8; i++) {
    seed = seed * 1103515245u + 12345u;
    word[i] = 'a' + (seed % 26);
  }
  word[8] = '\0';
}

static int write_table(
    const char* path, long long row_count, long long offset, unsigned seed
) {
  FILE* file_stream = fopen(path, "w");
  if (!file_stream)
    return 0;
  fprintf(file_stream, "%lld\n", row_count);
  for (long long idx = 0; idx < row_count; idx++) {
    char word[9];
    make_word(word, seed + (unsigned)idx);
    long long row_id = idx + offset;
    fprintf(file_stream, "%lld %s\n", row_id, word);
  }
  fclose(file_stream);
  return 1;
}

static void read_count(FILE* file_stream, long long* row_count) {
  (void)fscanf(file_stream, "%lld", row_count);
  int ch;
  while ((ch = fgetc(file_stream)) != '\n' && ch != EOF) {
  }
}

static void read_id_word(FILE* file_stream, long long* id, char word[9]) {
  (void)fscanf(file_stream, "%lld %8s", id, word);
  word[8] = '\0';
}

static long long mark_data_start(FILE* file_stream) {
  long pos = ftell(file_stream);
  return (pos < 0) ? 0 : (long long)pos;
}

static int do_join_files(
    const char* pathA, const char* pathB, const char* pathOut
) {
  FILE* file_A = fopen(pathA, "r");
  if (!file_A)
    return 1;
  FILE* file_B = fopen(pathB, "r");
  if (!file_B) {
    fclose(file_A);
    return 1;
  }

  long long str_numb_A = 0, str_numb_B = 0;
  read_count(file_A, &str_numb_A);
  read_count(file_B, &str_numb_B);

  long long offile_A = mark_data_start(file_A);
  long long offile_B = mark_data_start(file_B);

  long long matches = 0;

  fseek(file_A, offile_A, SEEK_SET);
  for (long long i = 0; i < str_numb_A; i++) {
    long long idA;
    char word_A[9];
    read_id_word(file_A, &idA, word_A);

    fseek(file_B, offile_B, SEEK_SET);
    for (long long j = 0; j < str_numb_B; j++) {
      long long idB;
      char word_B[9];
      read_id_word(file_B, &idB, word_B);
      if (idA == idB)
        matches++;
    }
  }

  FILE* file_out = fopen(pathOut, "w");
  if (!file_out) {
    fclose(file_A);
    fclose(file_B);
    return 1;
  }
  fprintf(file_out, "%lld\n", matches);

  fseek(file_A, offile_A, SEEK_SET);
  for (long long i = 0; i < str_numb_A; i++) {
    long long idA;
    char word_A[9];
    read_id_word(file_A, &idA, word_A);

    fseek(file_B, offile_B, SEEK_SET);
    for (long long j = 0; j < str_numb_B; j++) {
      long long idB;
      char word_B[9];
      read_id_word(file_B, &idB, word_B);
      if (idA == idB)
        fprintf(file_out, "%lld %s %s\n", idA, word_A, word_B);
    }
  }

  fclose(file_out);
  fclose(file_A);
  fclose(file_B);
  return 0;
}

int main(int argc, char** argv) {
  if (argc != 3)
    return 2;

  long long str_numb_A = strtoll(argv[1], NULL, 10);
  long long str_numb_B = strtoll(argv[2], NULL, 10);

  long long offsetB = str_numb_B / 3;
  unsigned seedA = 12345, seedB = 54321;

  if (!write_table(TMP_A, str_numb_A, 0, seedA))
    return 1;
  if (!write_table(TMP_B, str_numb_B, offsetB, seedB))
    return 1;

  int res = do_join_files(TMP_A, TMP_B, OUT_PATH);

  remove(TMP_A);
  remove(TMP_B);
  return res;
}
