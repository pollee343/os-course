#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CRC_TABLE_SIZE 256U
#define BITS_PER_BYTE 8U
#define FRAGMENT_COUNT 2000U
#define FRAGMENT_LENGTH 4096U
#define RNG_SEED 12345U
#define BYTES_PER_MEBIBYTE (1024.0 * 1024.0)

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
  const uint8_t* p = (const uint8_t*)data;
  crc = ~crc;
  for (size_t i = 0; i < len; i++) {
    crc = crc32_table[(crc ^ p[i]) & 0xFFU] ^ (crc >> BITS_PER_BYTE);
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

static double exec_seconds(struct timespec start, struct timespec finish) {
  const double NANOSECONDS_IN_SECOND = 1e9;
  long long ns = (finish.tv_sec - start.tv_sec) * 1000000000LL +
                 (finish.tv_nsec - start.tv_nsec);
  return (double)ns / NANOSECONDS_IN_SECOND;
}

int main(void) {
  crc32_make_table();

  uint8_t* buffer = malloc(FRAGMENT_LENGTH);
  if (!buffer) {
    return 1;
  }

  uint32_t seed = RNG_SEED;
  uint32_t crc = 0;
  size_t total_bytes = 0;

  struct timespec start;
  struct timespec finish;
  clock_gettime(CLOCK_MONOTONIC, &start);

  for (size_t i = 0; i < FRAGMENT_COUNT; i++) {
    (void)lcg_next(&seed);
    fill_fragment(buffer, FRAGMENT_LENGTH, &seed);
    crc = crc32_update(crc, buffer, FRAGMENT_LENGTH);
    total_bytes += FRAGMENT_LENGTH;
  }

  clock_gettime(CLOCK_MONOTONIC, &finish);
  free(buffer);

  double seconds = exec_seconds(start, finish);
  double mib = (double)total_bytes / BYTES_PER_MEBIBYTE;
  double mibps = seconds > 0.0 ? mib / seconds : 0.0;

  printf("crc32=0x%08" PRIx32 "\n", crc);
  printf(
      "bytes=%zu fragments=%u frag_len=%u\n",
      total_bytes,
      FRAGMENT_COUNT,
      FRAGMENT_LENGTH
  );
  printf("time=%.3f s throughput=%.2f MiB/s\n", seconds, mibps);
  return 0;
}
