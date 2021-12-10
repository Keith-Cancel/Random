#include <stdint.h>
#include <stddef.h>

static const uint64_t c = UINT64_C(0xFFFFFFFFFFFFFFFF) / 10 + 1;

uint32_t fastmod10_lemire(uint32_t n ) {
  uint64_t lowbits = c * n;
  return ((__uint128_t)lowbits * 10) >> 64;
}

static const uint64_t c2 = 0xffffffffffffffff / 10 + 1;

unsigned fastmod10_no_128(uint32_t n) {
    uint64_t low = n;
    low  *= c2;
    low >>= 60;
    low  *= 5;
    return (low + 4) >> 3;
}


unsigned mod10(uint32_t a) {
    return a % 10;
}