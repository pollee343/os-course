#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORD_LENGTH 8
#define ALPHABET_SIZE 26

static const char* const OUT_PATH = "./loaders/data/out_auto.txt";

static const char* const TMP_A = "./loaders/data/A.txt";
static const char* const TMP_B = "./loaders/data/B.txt";
// todo переписать чтобы команда принимала на вход названия входящих уже
// созданных файлов
static void make_word(char* word, unsigned seed) {
  static const unsigned LCG_MULTIPLIER = 1103515245U;
  static const unsigned LCG_INCREMENT = 12345U;
  for (int i = 0; i < WORD_LENGTH; i++) {
    seed = seed * LCG_MULTIPLIER + LCG_INCREMENT;
    word[i] = (char)('a' + (seed % ALPHABET_SIZE));
  }
  word[WORD_LENGTH] = '\0';
}

static int write_table(
    const char* path, long long row_count, long long offset, unsigned seed
) {
  FILE* file_stream = fopen(path, "w");
  if (!file_stream)

  {
    return 0;
  }
  (void)fprintf(file_stream, "%lld\n", row_count);
  for (long long idx = 0; idx < row_count; idx++) {
    char word[WORD_LENGTH + 1];
    make_word(word, seed + (unsigned)idx);
    long long row_id = idx + offset;
    (void)fprintf(file_stream, "%lld %s\n", row_id, word);
  }
  (void)fclose(file_stream);
  return 1;
}

static void read_count(FILE* file_stream, long long* row_count) {
  (void)fscanf(file_stream, "%lld", row_count);
  int symbol = 0;
  while ((symbol = fgetc(file_stream)) != '\n' && symbol != EOF) {
  }
}

static void read_id_word(
    FILE* file_stream, long long* ind, char word[WORD_LENGTH + 1]
) {
  (void)fscanf(file_stream, "%lld %8s", ind, word);
  word[WORD_LENGTH] = '\0';
}

static long long mark_data_start(FILE* file_stream) {
  long pos = ftell(file_stream);
  return (pos < 0) ? 0 : (long long)pos;
}

static int do_join_files(
    const char* pathA, const char* pathB, const char* pathOut
) {
  FILE* file_A = fopen(pathA, "r");
  if (!file_A) {
    return 1;
  }
  FILE* file_B = fopen(pathB, "r");
  if (!file_B) {
    (void)fclose(file_A);
    return 1;
  }

  long long str_numb_A = 0;
  long long str_numb_B = 0;
  read_count(file_A, &str_numb_A);
  read_count(file_B, &str_numb_B);

  long long offile_A = mark_data_start(file_A);
  long long offile_B = mark_data_start(file_B);

  long long matches = 0;

  (void)fseek(file_A, offile_A, SEEK_SET);
  for (long long i = 0; i < str_numb_A; i++) {
    long long idA = 0;
    char word_A[WORD_LENGTH + 1];
    read_id_word(file_A, &idA, word_A);

    (void)fseek(file_B, offile_B, SEEK_SET);
    for (long long j = 0; j < str_numb_B; j++) {
      long long idB = 0;
      char word_B[WORD_LENGTH + 1];
      read_id_word(file_B, &idB, word_B);
      if (idA == idB) {
        matches++;
      }
    }
  }

  FILE* file_out = fopen(pathOut, "w");
  if (!file_out) {
    (void)fclose(file_A);
    (void)fclose(file_B);
    return 1;
  }
  (void)fprintf(file_out, "%lld\n", matches);

  (void)fseek(file_A, offile_A, SEEK_SET);
  for (long long i = 0; i < str_numb_A; i++) {
    long long idA = 0;
    char word_A[WORD_LENGTH + 1];
    read_id_word(file_A, &idA, word_A);

    (void)fseek(file_B, offile_B, SEEK_SET);
    for (long long j = 0; j < str_numb_B; j++) {
      long long idB = 0;
      char word_B[WORD_LENGTH + 1];
      read_id_word(file_B, &idB, word_B);
      if (idA == idB) {
        (void)fprintf(file_out, "%lld %s %s\n", idA, word_A, word_B);
      }
    }
  }

  (void)fclose(file_out);
  (void)fclose(file_A);
  (void)fclose(file_B);
  return 0;
}

int main(int argc, char** argv) {
  if (argc != 3) {
    return 2;
  }
  const int DECIMAL_BASE = 10;
  long long str_numb_A = strtoll(argv[1], NULL, DECIMAL_BASE);
  long long str_numb_B = strtoll(argv[2], NULL, DECIMAL_BASE);

  long long offsetB = str_numb_B / 3;
  const unsigned seedA = 12345;
  const unsigned seedB = 54321;

  if (!write_table(TMP_A, str_numb_A, 0, seedA)) {
    return 1;
  }
  if (!write_table(TMP_B, str_numb_B, offsetB, seedB)) {
    return 1;
  }

  int res = do_join_files(TMP_A, TMP_B, OUT_PATH);

  // remove(TMP_A);
  // remove(TMP_B);
  return res;
}
