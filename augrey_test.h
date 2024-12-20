#include <assert.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <errno.h>
#include "util.h"

#define INST_SYNC asm volatile("cpuid")

#define HIT_CYCLES_MAX 150
#define CACHE_LINE_SIZE 64
#define MSB_MASK ((uint64_t)-1)
#define AOP_ALIGN_WIN (4 * 1024)
#define BUF_ALIGN_WIN CACHE_LINE_SIZE
#define U64S_PER_LINE (CACHE_LINE_SIZE / sizeof(uint64_t))
#define PNRG_a 75
#define BUF_MEM (256 * 1024 * 1024)
#define BUF_MOD (BUF_MEM / (sizeof(uint64_t) * U64S_PER_LINE))
// 1398107, 37799 are primes
#define BUF_GEN_PRIME 131071
#define SELF_ALLOC_PRIME 1398107
#define AOP_ALLOC_PRIME 151
#define ind_gen(x) (((BUF_GEN_PRIME * x) % BUF_MOD) * U64S_PER_LINE)
// #define PRNG_r (BUF_MEM / sizeof(uint64_t))
// #define prng(x) ((PNRG_a * x) % PRNG_r)

typedef long int intptr_t;

int main();
void test_gen_eviction_set(uint64_t tgt);
double ddp_poc(uint64_t** arr, int ind_scale, 
            uint64_t* thrash_arr, int thrash_size, 
            uint64_t* data_buffer);
double cache_reset_agent_test( uint64_t** arr, int ind_scale, 
                               uint64_t* thrash_arr, int thrash_size, 
                               uint64_t* data_buffer);
double everything_still_in_cache_test( uint64_t** arr, int ind_scale, 
                                       uint64_t* thrash_arr, int thrash_size, 
                                       uint64_t* data_buffer);
double not_overwritten_in_cache_test( uint64_t** arr, int ind_scale, 
                                      uint64_t* thrash_arr, int thrash_size, 
                                      uint64_t* data_buffer, ADDR_PTR tgt_ind);
double not_brought_in_cache_test( uint64_t** arr, int ind_scale, 
                                  uint64_t* thrash_arr, int thrash_size, 
                                  uint64_t* data_buffer, ADDR_PTR tgt_ind);
double others_still_in_cache_test( uint64_t** arr, int ind_scale, 
                                   uint64_t* thrash_arr, int thrash_size, 
                                   uint64_t* data_buffer, ADDR_PTR tgt_ind);
char* convertToBinary(uint64_t num, char* msg);
