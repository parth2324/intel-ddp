#include "augrey_test.h"

#define M 256
#define N 100

int main(){
    srand(42);
    
    int ptrs_per_line = 1;  // 1 to 8 for 64B line

    int aop_ind_scale = U64S_PER_LINE + 1 - ptrs_per_line;
    int aop_mem = M * CACHE_LINE_SIZE * aop_ind_scale + AOP_ALIGN_WIN;
    int buf_mem = PRNG_m + BUF_ALIGN_WIN;

    volatile uint64_t **aop = mmap(0, aop_mem, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(aop != MAP_FAILED);
    memset(aop, 0, aop_mem);

    ADDR_PTR curr = (ADDR_PTR)aop;
    curr += (-curr) & (AOP_ALIGN_WIN - 1);
    aop = (volatile uint64_t **)curr;

    volatile uint64_t *data_buffer = mmap(0, buf_mem, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(data_buffer != MAP_FAILED);
    curr = (ADDR_PTR)data_buffer;
    curr += (-curr) & (BUF_ALIGN_WIN - 1);
    data_buffer = (volatile uint64_t *)curr;

    for(uint64_t i = 0; i < buf_mem / sizeof(uint64_t); i++) {
        data_buffer[i] = rand() & (MSB_MASK - 1);
    }

    uint64_t rand_idx = 1;
    for(int i = 0; i < M; i += 1) {
        aop[i * aop_ind_scale] = &data_buffer[rand_idx * U64S_PER_LINE];
        rand_idx = prng(rand_idx);
    }
    // testing below
    rand_idx = 1;
    for (int j = 0; j < M ; j++) {
        assert(*(aop[j * aop_ind_scale]) = data_buffer[rand_idx * U64S_PER_LINE]);
        rand_idx = prng(rand_idx);
    }

    // For preventing unwanted compiler optimizations and adding
    // data dependencies between instructions.
    uint64_t __trash = 0;

    /*
    Caches (sum of all):      
        L1d:                    48 KiB (1 instance)
        L1i:                    32 KiB (1 instance)
        L2:                     2 MiB (1 instance)
        L3:                     36 MiB (1 instance)
    */
    int thr_mem = ((38 * 1024 * 1024) + (80 * 1024)) * 8;
    volatile uint64_t *thrash_arr = mmap(0, thr_mem, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    for(uint64_t i = 0; i < thr_mem / sizeof(uint64_t); i++) {
        thrash_arr[i] = rand() & (MSB_MASK - 1);
    }

    int repetitions = 1000;
    uint64_t *times_to_load_test_ptr_baseline =
        malloc(repetitions * sizeof(uint64_t));
    uint64_t *times_to_load_test_ptr_aop =
        malloc(repetitions * sizeof(uint64_t));
    uint64_t *times_to_load_train_ptr_baseline =
        malloc(repetitions * sizeof(uint64_t));
    uint64_t *times_to_load_train_ptr_aop =
        malloc(repetitions * sizeof(uint64_t));
    uint64_t *curr_test_base = times_to_load_test_ptr_baseline;
    uint64_t *curr_test_aop = times_to_load_test_ptr_aop;
    uint64_t *curr_train_base = times_to_load_train_ptr_baseline;
    uint64_t *curr_train_aop = times_to_load_train_ptr_aop;

    if(!test_uniqueness(aop, aop_ind_scale, thrash_arr, thr_mem)) printf("Fail.\n");
    else printf("Pass.\n");
    printf("--------------------------------------------------------------------------\n");
    if(!test_uniqueness(aop, aop_ind_scale, thrash_arr, thr_mem)) printf("Fail.\n");
    else printf("Pass.\n");
    return 0;
}

bool test_uniqueness(volatile uint64_t** arr, int ind_scale, volatile uint64_t* thrash_arr, int thrash_size){
    char* msg = malloc(sizeof(char) * 100);
    uint64_t time_taken;
    ADDR_PTR tgt, agnst;

    // For preventing unwanted compiler optimizations and adding
    // data dependencies between instructions.
    uint64_t __trash = 0;

    for(int i = 0; i < M; i++){
        // thrash cache
        for (int j = 0; j < thrash_size / sizeof(uint64_t) - 2; j++) {
            __trash += (thrash_arr[j] ^ __trash) & 0b1111;
            __trash += (thrash_arr[j + 1] ^ __trash) & 0b1111;
            __trash += (thrash_arr[j + 2] ^ __trash) & 0b1111;
        }

        // bring everything into the cache.
        for(int j = 0; j < M; j++){
            maccess((ADDR_PTR)(arr[j * ind_scale]));
        }

        // ensure everything is in cache.
        for(int j = 0; j < M; j++){
            tgt = (ADDR_PTR)(arr[j * ind_scale]);
            time_taken = maccess_t(tgt);
            if(time_taken > HIT_CYCLES_MAX){
                printf("Did not persist in cache, checking %d:%s.\n", 
                j, convertToBinary(tgt, msg));
                // return false;
            }
        }

        // flush ith.
        tgt = (ADDR_PTR)(arr[i * ind_scale]);
        clflush(tgt);

        for(int j = 0; j < i; j++){
            // confirm jth before ith still in cache.
            agnst = (ADDR_PTR)(arr[j * ind_scale]);
            time_taken = maccess_t(agnst);
            if(time_taken > HIT_CYCLES_MAX){
                printf("Got evicted from cache, checking %d:%ld:%s against %d:%ld:%s.\n",
                i, tgt, convertToBinary(tgt, msg),
                j, agnst, convertToBinary(agnst, msg));
                // return false;
            }

            // confirm ith not brought in cache.
            time_taken = maccess_t(tgt);
            clflush(tgt);
            if(time_taken < HIT_CYCLES_MAX){
                printf("Got brought back in cache, checking values before %d:%s.\n",
                i, convertToBinary(tgt, msg));
                // return false;
            }
        }
        for(int j = i + 1; j < M; j++){
            // confirm jth after ith still in cache.
            agnst = (ADDR_PTR)(arr[j * ind_scale]);
            time_taken = maccess_t(agnst);
            if(time_taken > HIT_CYCLES_MAX){
                printf("Got evicted from cache, checking %d:%ld:%s against %d:%ld:%s.\n",
                i, tgt, convertToBinary(tgt, msg),
                j, agnst, convertToBinary(agnst, msg));
                // return false;
            }

            // confirm ith not brought in cache.
            time_taken = maccess_t(tgt);
            clflush(tgt);
            if(time_taken < HIT_CYCLES_MAX){
                printf("Got brought back in cache, checking values before %d:%s.\n",
                i, convertToBinary(tgt, msg));
                // return false;
            }
        }
    }

    free(msg);
    return true;
}

char* convertToBinary(uint64_t num, char* msg) {
    for(int k = 0; k < 64; k++){
        msg[k] = '\0';
    }
    uint8_t binary[64];
    int i = 0, c = 0;

    while (num != 0) {
        binary[i++] = num % 2;
        num >>= 1;
    }

    for (int j = 17; j >= 0; j--) {
        if(binary[j] == 0) msg[c] = '0';
        else msg[c] = '1';
        c++;
    }
    msg[c] = '\0';
    return msg;
}
