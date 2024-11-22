#include "augrey_test.h"

#define M 256
#define N 100

int main(){
    srand(42);
    
    int ptrs_per_line = 1;  // 1 to 8 for 64B line

    int aop_ind_scale = U64S_PER_LINE + 1 - ptrs_per_line;
    int aop_mem = M * CACHE_LINE_SIZE * aop_ind_scale + AOP_ALIGN_WIN;
    int buf_mem = BUF_MEM + BUF_ALIGN_WIN;

    printf("Initializing aop.\n");
    volatile uint64_t **aop = mmap(0, aop_mem, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(aop != MAP_FAILED);
    memset(aop, 0, aop_mem);
    ADDR_PTR curr = (ADDR_PTR)aop;
    curr += (-curr) & (AOP_ALIGN_WIN - 1);
    aop = (volatile uint64_t **)curr;

    printf("Initializing source buffer.\n");
    volatile uint64_t *data_buffer = mmap(0, buf_mem, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(data_buffer != MAP_FAILED);
    for(uint64_t i = 0; i < buf_mem / sizeof(uint64_t); i++) {
        data_buffer[i] = rand() & (MSB_MASK - 1);
    }
    curr = (ADDR_PTR)data_buffer;
    curr += (-curr) & (BUF_ALIGN_WIN - 1);
    data_buffer = (volatile uint64_t *)curr;

    printf("Allocating pointers to aop from source buffer.\n");
    uint64_t idx = 1;
    for(int i = 0; i < M; i += 1) {
        aop[i * aop_ind_scale] = &data_buffer[idx * U64S_PER_LINE];
        idx += 2;
    }
    // Testing below:
    // rand_idx = 1;
    // for (int j = 0; j < M ; j++) {
    //     assert(*(aop[j * aop_ind_scale]) = data_buffer[rand_idx * U64S_PER_LINE]);
    //     rand_idx = prng(rand_idx);
    // }

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
    printf("Allocating thrash memory.\n");
    int thr_mem = ((38 * 1024 * 1024) + (80 * 1024));
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

    int test_reps = 1;

    double err_c = 0.0;
    for(int i = 0; i < test_reps; i++){
        printf("everything_still_in_cache_test's iteration %d.\n", i);
        err_c += everything_still_in_cache_test(aop, aop_ind_scale, thrash_arr, thr_mem);
    }
    printf("everything_still_in_cache_test's error rate: %.3f%%\n", err_c / test_reps);

    err_c = 0.0;
    for(int i = 0; i < test_reps; i++){
        printf("not_overwritten_in_cache_test's iteration %d.\n", i);
        err_c += not_overwritten_in_cache_test(aop, aop_ind_scale, thrash_arr, thr_mem);
    }
    printf("not_overwritten_in_cache_test's error rate: %.3f%%\n", err_c / test_reps);

    err_c = 0.0;
    for(int i = 0; i < test_reps; i++){
        printf("not_brought_in_cache_test's iteration %d.\n", i);
        err_c += not_brought_in_cache_test(aop, aop_ind_scale, thrash_arr, thr_mem);
    }
    printf("not_brought_in_cache_test's error rate: %.3f%%\n", err_c / test_reps);

    err_c = 0.0;
    for(int i = 0; i < test_reps; i++){
        printf("others_still_in_cache_test's iteration %d.\n", i);
        err_c += others_still_in_cache_test(aop, aop_ind_scale, thrash_arr, thr_mem);
    }
    printf("others_still_in_cache_test's error rate: %.3f%%\n", err_c / test_reps);

    return 0;
}

double everything_still_in_cache_test(volatile uint64_t** arr, int ind_scale, volatile uint64_t* thrash_arr, int thrash_size){
    char* msg = malloc(sizeof(char) * 100);
    int err_c = 0;
    uint64_t *time_taken = malloc(sizeof(uint64_t) * M), MOD = M * ind_scale;

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

        INST_SYNC;

        // bring everything into the cache.
        for(int j = 0; j < M; j++){
            maccess((ADDR_PTR)(arr[(j * ind_scale) % MOD]));
        }

        INST_SYNC;

        // ensure everything is in cache.
        for(int j = 0; j < M; j++){
            time_taken[j] = maccess_t((ADDR_PTR)(arr[(j * ind_scale) % MOD]));
            INST_SYNC;
        }

        for(int j = 0; j < M; j++){
            if(time_taken[j] > HIT_CYCLES_MAX){
                // printf("Did not persist in cache, checking %d:%s.\n", 
                // j, convertToBinary(tgt, msg));
                // return false;
                err_c++;
            }
        }

        INST_SYNC;
    }

    free(msg);
    free(time_taken);
    return err_c * 100.0 / (M * M);
}

double not_overwritten_in_cache_test(volatile uint64_t** arr, int ind_scale, volatile uint64_t* thrash_arr, int thrash_size){
    char* msg = malloc(sizeof(char) * 100);
    int err_c = 0;
    uint64_t time_taken, MOD = M * ind_scale;
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

        INST_SYNC;

        // bring ith.
        tgt = (ADDR_PTR)(arr[(i * ind_scale) % MOD]);
        maccess(tgt);

        INST_SYNC;

        // bring everything else into the cache.
        for(int j = 0; j < M; j++){
            if(j != i) maccess((ADDR_PTR)(arr[(j * ind_scale) % MOD]));
            INST_SYNC;
        }

        time_taken = maccess_t(tgt);
        if(time_taken > HIT_CYCLES_MAX){
            // printf("maccess didn't persist %d:%s.\n",
            // i, convertToBinary(tgt, msg));
            err_c++;
        }

        INST_SYNC;
    }

    free(msg);
    return (100.0 * err_c) / M;
}

double not_brought_in_cache_test(volatile uint64_t** arr, int ind_scale, volatile uint64_t* thrash_arr, int thrash_size){
    char* msg = malloc(sizeof(char) * 100);
    int err_c = 0;
    uint64_t time_taken, MOD = M * ind_scale;
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

        INST_SYNC;

        // bring everything into the cache.
        for(int j = 0; j < M; j++){
            maccess((ADDR_PTR)(arr[(j * ind_scale) % MOD]));
        }

        INST_SYNC;

        // flush ith.
        tgt = (ADDR_PTR)(arr[(i * ind_scale) % MOD]);
        clflush(tgt);

        INST_SYNC;

        for(int j = 0; j < M; j++){
            // confirm jth still in cache.
            if(j != i) maccess((ADDR_PTR)(arr[(j * ind_scale) % MOD]));
            INST_SYNC;
        }
        time_taken = maccess_t(tgt);
        if(time_taken < HIT_CYCLES_MAX){
            // printf("clflush didn't persist %d:%s.\n",
            // i, convertToBinary(tgt, msg));
            err_c++;
        }

        INST_SYNC;
    }

    free(msg);
    return (err_c * 100.0) / M;
}

double others_still_in_cache_test(volatile uint64_t** arr, int ind_scale, volatile uint64_t* thrash_arr, int thrash_size){
    char* msg = malloc(sizeof(char) * 100);
    int err_c = 0;
    uint64_t *time_taken = malloc(sizeof(uint64_t) * M), MOD = M * ind_scale;
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

        INST_SYNC;

        // bring everything into the cache.
        for(int j = 0; j < M; j++){
            maccess((ADDR_PTR)(arr[(j * ind_scale) % MOD]));
        }

        INST_SYNC;

        // flush ith.
        tgt = (ADDR_PTR)(arr[(i * ind_scale) % MOD]);
        clflush(tgt);

        INST_SYNC;

        for(int j = 0; j < M; j++){
            if(j != i) time_taken[j] = maccess_t((ADDR_PTR)(arr[(j * ind_scale) % MOD]));
            INST_SYNC;
        }

        for(int j = 0; j < M; j++){
            // confirm jth still in cache.
            if(j != i && time_taken[j] > HIT_CYCLES_MAX){
                // printf("Got evicted from cache, checking %d:%ld:%s against %d:%ld:%s.\n",
                // i, tgt, convertToBinary(tgt, msg),
                // j, agnst, convertToBinary(agnst, msg));
                err_c++;
            }
        }

        INST_SYNC;
    }

    free(time_taken);
    free(msg);
    return (err_c * 100.0) / ((M - 1) * M);
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
