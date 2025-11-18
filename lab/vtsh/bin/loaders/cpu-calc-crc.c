#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CRC_TABLE_SIZE 256U
#define BITS_PER_BYTE 8U
#define DEFAULT_FRAGMENT_COUNT 2000U
#define DEFAULT_ITERATIONS 1U
#define FRAGMENT_LENGTH 4096U
#define RNG_SEED 12345U
#define BYTES_PER_MEBIBYTE (1024.0 * 1024.0)
#define DECIMAL_BASE 10

static const uint32_t LOW_BYTE_MASK = 0xFFU;

static uint32_t crc32_table[CRC_TABLE_SIZE];

static void crc32_make_table(void) {
  const uint32_t POLY = 0xEDB88320U;
  for (uint32_t i = 0; i < CRC_TABLE_SIZE; i++) {
    uint32_t crc = i;
    for (uint32_t j = 0; j < BITS_PER_BYTE; j++) {
      crc = (crc & 1U) ? (POLY ^ (crc >> 1U)) : (crc >> 1U);
    }
    crc32_table[i] = crc;
  }
}

static inline uint32_t crc32_update(
    uint32_t crc, const void* data, size_t len
) {
  const uint8_t* ptr = (const uint8_t*)data;
  crc = ~crc;
  for (size_t i = 0; i < len; i++) {
    crc = crc32_table[(crc ^ ptr[i]) & LOW_BYTE_MASK] ^ (crc >> BITS_PER_BYTE);
  }
  return ~crc;
}

static inline uint32_t lcg_next(uint32_t* state) {
  const uint32_t LCG_MULTIPLIER = 1103515245U;
  const uint32_t LCG_INCREMENT = 12345U;
  *state = (*state) * LCG_MULTIPLIER + LCG_INCREMENT;
  return *state;
}

static void fill_fragment(uint8_t* buf, size_t n, uint32_t* seed) {
  const uint8_t alphabet[] =
      "abcdefghijklmnopqrstuvwxyz"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "0123456789 _-.,;:/\\[]{}()*!@#$%^&+=?";
  const size_t alphabet_size = sizeof(alphabet) - 1;
  for (size_t i = 0; i < n; i++) {
    uint32_t rand_val = lcg_next(seed);
    buf[i] = alphabet[rand_val % alphabet_size];
  }
}

int main(int argc, char** argv) {
  size_t fragment_count = DEFAULT_FRAGMENT_COUNT;
  size_t iterations = DEFAULT_ITERATIONS;

  if (argc > 3) {
    int print_result =
        fprintf(stderr, "Usage: %s [fragments] [iterations]\n", argv[0]);
    if (print_result < 0) {
      perror("fprintf usage");
    }
    return 1;
  }

  if (argc >= 2) {
    char* endptr = NULL;
    unsigned long long val = strtoull(argv[1], &endptr, DECIMAL_BASE);
    if (*endptr != '\0' || val == 0) {
      int print_result =
          fprintf(stderr, "Invalid fragments value: %s\n", argv[1]);
      if (print_result < 0) {
        perror("fprintf fragments");
      }
      return 1;
    }
    fragment_count = (size_t)val;
  }

  if (argc == 3) {
    char* endptr = NULL;
    unsigned long long val = strtoull(argv[2], &endptr, DECIMAL_BASE);
    if (*endptr != '\0' || val == 0) {
      int print_result =
          fprintf(stderr, "Invalid iterations value: %s\n", argv[2]);
      if (print_result < 0) {
        perror("fprintf iterations");
      }
      return 1;
    }
    iterations = (size_t)val;
  }

  crc32_make_table();

  uint8_t* buffer = malloc(FRAGMENT_LENGTH);
  if (!buffer) {
    perror("malloc buffer");
    return 1;
  }

  uint32_t seed = RNG_SEED;
  uint32_t crc = 0;
  size_t total_bytes = 0;

  for (size_t it = 0; it < iterations; ++it) {
    for (size_t i = 0; i < fragment_count; ++i) {
      lcg_next(&seed);
      fill_fragment(buffer, FRAGMENT_LENGTH, &seed);
      crc = crc32_update(crc, buffer, FRAGMENT_LENGTH);
      total_bytes += FRAGMENT_LENGTH;
    }
  }

  printf("crc32=0x%08" PRIx32 "\n", crc);
  printf(
      "bytes=%zu fragments=%zu iterations=%zu frag_len=%u\n",
      total_bytes,
      fragment_count,
      iterations,
      FRAGMENT_LENGTH
  );

  free(buffer);
  return 0;
}
