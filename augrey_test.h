#include <assert.h>
#include "util.h"

#define HIT_CYCLES_MAX 250
#define CACHE_LINE_SIZE 64
#define MSB_MASK ((uint64_t)-1)
#define AOP_ALIGN_WIN 2 * 1024 * 1024
#define BUF_ALIGN_WIN CACHE_LINE_SIZE
#define U64S_PER_LINE (CACHE_LINE_SIZE / sizeof(uint64_t))
#define PNRG_a 75
#define BUF_MEM 256 * 1024 * 1024
// #define PRNG_r (BUF_MEM / sizeof(uint64_t))
// #define prng(x) ((PNRG_a * x) % PRNG_r)

typedef long int intptr_t;

int main();
bool test_uniqueness(volatile uint64_t** arr, int ind_scale, volatile uint64_t* thrash_arr, int thrash_size);
char* convertToBinary(uint64_t num, char* msg);
