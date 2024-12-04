#include "augrey_test.h"

#define M 512
#define MAIN_TEST_REPS 3
#define TEST_SIZE 256

#define PAGE_SIZE (4 * 1024)
#define L1_LINE_SIZE 64

#define N (M - TEST_SIZE)

void cpuid_chk(){
  char* _a = malloc(sizeof(char) * 100);
  char* _b = malloc(sizeof(char) * 100);
  char* _c = malloc(sizeof(char) * 100);
  char* _d = malloc(sizeof(char) * 100);
  uint64_t a, b, c, d;
  asm volatile(
            "mov $0x4, %%eax\n\t"
            "mov $0x1, %%ecx\n\t"
            "cpuid\n\t"
	       : /*output*/ "=a"(a), "=b"(b), "=c"(c), "=d"(d)
	       : /*input*/
	       : /*clobbers*/);
  _a = convertToBinary(a, _a);
  printf("A: %s\n", _a);
  _b = convertToBinary(b, _b);
  printf("B: %s\n", _b);
  _c = convertToBinary(c, _c);
  printf("C: %s\n", _c);
  _d = convertToBinary(d, _d);
  printf("D: %s\n", _d);
  free(_a);
  free(_b);
  free(_c);
  free(_d);
}

int main(int argc, char **argv){
    // cpuid_chk();
    // exit(0);

    if(argc != 3) return 1;
    srand(42);

    int store_bypass, err;
    sscanf(argv[2], "%d", &store_bypass);
    // if(store_bypass == 0){
    //     err = prctl(PR_SET_SPECULATION_CTRL, PR_SPEC_STORE_BYPASS, PR_SPEC_DISABLE, 0L, 0L);
    // }
    // else{
    //     err = prctl(PR_SET_SPECULATION_CTRL, PR_SPEC_STORE_BYPASS, PR_SPEC_ENABLE, 0L, 0L);
    // }
    // if(err == -1) return errno;

    int ptrs_per_line;  // 1 to 8 for 64B line
    sscanf(argv[1], "%d", &ptrs_per_line);
    // printf("%d ptrs/line, DDPD_U = %d:\n", ptrs_per_line, store_bypass);
    printf("%d ptrs/line, DDPD_U = SSPD = %d:\n", ptrs_per_line, store_bypass);

    int aop_ind_scale = U64S_PER_LINE + 1 - ptrs_per_line;
    int aop_mem = M * CACHE_LINE_SIZE * aop_ind_scale + AOP_ALIGN_WIN;
    int buf_mem = BUF_MEM + BUF_ALIGN_WIN;

    // printf("Initializing aop.\n");
    uint64_t **aop = mmap(0, aop_mem, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(aop != MAP_FAILED);
    for(uint64_t i = 0; i < aop_mem / sizeof(uint64_t); i++) {
        aop[i] = (uint64_t*)(rand() & (MSB_MASK - 1));
    }
    // memset((uint64_t*)aop, 0, aop_mem);
    ADDR_PTR curr = (ADDR_PTR)aop;
    curr += (-curr) & (AOP_ALIGN_WIN - 1);
    aop = (uint64_t**)curr;

    // printf("Initializing source buffer.\n");
    uint64_t *data_buffer = mmap(0, buf_mem, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(data_buffer != MAP_FAILED);
    for(uint64_t i = 0; i < buf_mem / sizeof(uint64_t); i++) {
        data_buffer[i] = rand() & (MSB_MASK - 1);
    }
    curr = (ADDR_PTR)data_buffer;
    curr += (-curr) & (BUF_ALIGN_WIN - 1);
    data_buffer = (uint64_t*)curr;

    // printf("Allocating pointers to aop from source buffer.\n");
    for(int i = 0; i < M; i++) {
        aop[i * aop_ind_scale] = &data_buffer[ind_gen(i)];
    }
    // Testing uniqueness of allocator below:
    for(int i = 0; i < M; i++) {
        for(int j = 0; j < M ; j++) {
            if(i != j && (aop[i * aop_ind_scale] - aop[j * aop_ind_scale]) == 0){
                printf("The impossible has happend.\n");
                exit(1);
            }
        }
    }

    /*
    Caches (sum of all):      
        L1d:                    48 KiB (1 instance)
        L1i:                    32 KiB (1 instance)
        L2:                     2 MiB (1 instance)
        L3:                     36 MiB (1 instance)
    */
    // printf("Allocating thrash memory.\n");
    int thr_mem = ((38 * 1024 * 1024) + (80 * 1024)) * 4, tgt_ind;
    uint64_t *thrash_arr = mmap(0, thr_mem, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    for(uint64_t i = 0; i < thr_mem / sizeof(uint64_t); i++) {
        tgt_ind = (SELF_ALLOC_PRIME * (i + 1)) % (thr_mem / sizeof(uint64_t));
        thrash_arr[i] = (ADDR_PTR)&thrash_arr[tgt_ind]; // rand() & (MSB_MASK - 1);
    }

    // filling in the important addresses that must be flushed.
    for(uint64_t i = 0; i < M; i++) {
        tgt_ind = (AOP_ALLOC_PRIME * (i + 1)) % (thr_mem / sizeof(uint64_t));
        thrash_arr[tgt_ind] = (ADDR_PTR)&aop[i * aop_ind_scale];
    }
    for(uint64_t i = 0; i < M; i++) {
        tgt_ind = (AOP_ALLOC_PRIME * (i + M + 1)) % (thr_mem / sizeof(uint64_t));
        thrash_arr[tgt_ind] = (ADDR_PTR)aop[i * aop_ind_scale];
    }

    ddp_poc(aop, aop_ind_scale, thrash_arr, thr_mem, data_buffer);

    int test_reps = 5;

    double err_c = 0.0;
    for(int i = 0; i < test_reps; i++){
        printf("cache_reset_agent_test's iteration %d.\n", i);
        err_c += cache_reset_agent_test(aop, aop_ind_scale, thrash_arr, thr_mem, data_buffer);
    }
    printf("cache_reset_agent_test's net error rate: %.3f%%\n", err_c / test_reps);

    err_c = 0.0;
    for(int i = 0; i < test_reps; i++){
        printf("everything_still_in_cache_test's iteration %d.\n", i);
        err_c += everything_still_in_cache_test(aop, aop_ind_scale, thrash_arr, thr_mem, data_buffer);
    }
    printf("everything_still_in_cache_test's net error rate: %.3f%%\n", err_c / test_reps);

    err_c = 0.0;
    for(int i = 0; i < test_reps; i++){
        printf("not_overwritten_in_cache_test's iteration %d.\n", i);
        err_c += not_overwritten_in_cache_test(aop, aop_ind_scale, thrash_arr, thr_mem, data_buffer, (rand() % M) * aop_ind_scale);
    }
    printf("not_overwritten_in_cache_test's net error rate: %.3f%%\n", err_c / test_reps);

    err_c = 0.0;
    for(int i = 0; i < test_reps; i++){
        printf("not_brought_in_cache_test's iteration %d.\n", i);
        err_c += not_brought_in_cache_test(aop, aop_ind_scale, thrash_arr, thr_mem, data_buffer, (rand() % M) * aop_ind_scale);
    }
    printf("not_brought_in_cache_test's net error rate: %.3f%%\n", err_c / test_reps);

    err_c = 0.0;
    for(int i = 0; i < test_reps; i++){
        printf("others_still_in_cache_test's iteration %d.\n", i);
        err_c += others_still_in_cache_test(aop, aop_ind_scale, thrash_arr, thr_mem, data_buffer, (rand() % M) * aop_ind_scale);
    }
    printf("others_still_in_cache_test's net error rate: %.3f%%\n", err_c / test_reps);

    // test_gen_eviction_set(aop[1]);

    //  ----------------------------------------------------------------------------------------

    // int thr_ind;
    // uint64_t time_taken[MAIN_TEST_REPS + 1][M];
    // uint64_t MOD = M * aop_ind_scale, THR_MAX_IND = thr_mem / sizeof(uint64_t);
    // for(int i = 0; i <= MAIN_TEST_REPS; i++){
    //     memset(time_taken[i], 0, sizeof(uint64_t) * M);
    // }

    // // For preventing unwanted compiler optimizations and adding
    // // data dependencies between instructions.
    // uint64_t __trash = 0;

    // // printf("Beginning main test.\n");
    // for(int i = 0; i < MAIN_TEST_REPS; i++){
    //     // thrash cache
    //     for (int j = 0; j < THR_MAX_IND - 2; j++) {
    //         __trash += (thrash_arr[j] ^ __trash) & 0b1111;
    //         __trash += (thrash_arr[j + 1] ^ __trash) & 0b1111;
    //         __trash += (thrash_arr[j + 2] ^ __trash) & 0b1111;
    //     }
    //     INST_SYNC;
    //     for (int j = 0; j < THR_MAX_IND; j++) {
    //         thr_ind = j % THR_MAX_IND;
    //         clflush(thrash_arr[thr_ind]);
    //         thr_ind = (thr_ind + 1) % THR_MAX_IND;
    //         clflush(thrash_arr[thr_ind]);
    //         thr_ind = (thr_ind + 1) % THR_MAX_IND;
    //         clflush(thrash_arr[thr_ind]);
    //     }

    //     INST_SYNC;

    //     // bring training component into the cache.
    //     // for(int j = 0; j < N; j++){
    //     //     __trash += (aop[(j * aop_ind_scale) % MOD] ^ __trash) & 0b1111;
    //     // }
    //     if(N == 1){
    //         __trash += aop[0];
    //         __trash += aop[0];
    //         __trash += aop[0];
    //     }
    //     else if(N == 2){
    //         __trash += aop[0];
    //         __trash += aop[aop_ind_scale];
    //         __trash += aop[0];
    //         __trash += aop[aop_ind_scale];
    //         __trash += aop[0];
    //         __trash += aop[aop_ind_scale];
    //     }
    //     else{
    //         for(int j = 0; j < (N - 2) * aop_ind_scale; j += aop_ind_scale){
    //             __trash += aop[j];
    //             __trash += aop[j + aop_ind_scale];
    //             __trash += aop[j + aop_ind_scale + aop_ind_scale];
    //         }
    //     }        

    //     INST_SYNC;

    //     // measure testing component latencies.
    //     for(int j = N; j < M; j++){
    //         time_taken[i][j] = maccess_t((ADDR_PTR)(&data_buffer[ind_gen(j)]));
    //     }

    //     INST_SYNC;
    // }

    // // get average times and calc fraction brought in.
    // int ddp_hit_c = 0, hit_time_sum = 0;
    // double avg_hit_time = 0.0;
    // for(int i = 0; i < MAIN_TEST_REPS; i++){
    //     for(int j = N; j < M; j++){
    //         if(time_taken[i][j] < HIT_CYCLES_MAX){
    //             ddp_hit_c++;
    //             time_taken[MAIN_TEST_REPS][j] += time_taken[i][j];
    //         }
    //     }
    // }
    // for(int j = 0; j < M; j++){
    //     hit_time_sum += time_taken[MAIN_TEST_REPS][j];
    // }
    // avg_hit_time = hit_time_sum * 1.0 / ddp_hit_c;

    // // resetting accumulator
    // memset(time_taken[MAIN_TEST_REPS], 0, sizeof(uint64_t) * M);

    // for(int i = 0; i < MAIN_TEST_REPS; i++){
    //     // thrash cache
    //     for (int j = 0; j < THR_MAX_IND - 2; j++) {
    //         __trash += (thrash_arr[j] ^ __trash) & 0b1111;
    //         __trash += (thrash_arr[j + 1] ^ __trash) & 0b1111;
    //         __trash += (thrash_arr[j + 2] ^ __trash) & 0b1111;
    //     }
    //     INST_SYNC;
    //     for (int j = 0; j < THR_MAX_IND; j++) {
    //         thr_ind = j % THR_MAX_IND;
    //         clflush(thrash_arr[thr_ind]);
    //         thr_ind = (thr_ind + 1) % THR_MAX_IND;
    //         clflush(thrash_arr[thr_ind]);
    //         thr_ind = (thr_ind + 1) % THR_MAX_IND;
    //         clflush(thrash_arr[thr_ind]);
    //     }

    //     INST_SYNC;

    //     // measure testing component latencies.
    //     for(int j = N; j < M; j++){
    //         time_taken[i][j] = maccess_t((ADDR_PTR)(&data_buffer[ind_gen(j)]));
    //     }

    //     INST_SYNC;
    // }

    // // get average times and calc fraction not brought in.
    // int miss_c = 0, miss_time_sum = 0;
    // double avg_miss_time = 0.0;
    // for(int i = 0; i < MAIN_TEST_REPS; i++){
    //     for(int j = N; j < M; j++){
    //         if(time_taken[i][j] > HIT_CYCLES_MAX){
    //             miss_c++;
    //             time_taken[MAIN_TEST_REPS][j] += time_taken[i][j];
    //         }
    //     }
    // }
    // for(int j = 0; j < M; j++){
    //     miss_time_sum += time_taken[MAIN_TEST_REPS][j];
    // }
    // avg_miss_time = miss_time_sum * 1.0 / miss_c;

    // printf("Data brought in with    training: %.2f%%\t (%.2f\t of %d)\t with avg hit  time: %.1f\n", 
    // ddp_hit_c * 100.0 / ((M-N) * MAIN_TEST_REPS), ddp_hit_c * 1.0 / MAIN_TEST_REPS, (M-N), avg_hit_time);
    // printf("Data brought in without training: %.2f%%\t (%.2f\t of %d)\t with avg miss time: %.1f\n",
    // 100 - (miss_c * 100.0 / ((M-N) * MAIN_TEST_REPS)), (M-N) - (miss_c * 1.0 / MAIN_TEST_REPS), (M-N), avg_miss_time);

    return 0;
}

void test_gen_eviction_set(uint64_t tgt){
    int src_mem_size = 1 * 1024 * 1024, len = 0;
    volatile uint64_t src_mem = (uint64_t)mmap(0, src_mem_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    for(uint64_t addr = src_mem; addr < src_mem + src_mem_size; addr += sizeof(uint64_t)){
        maccess(tgt);
        INST_SYNC;
        for(uint64_t test = addr + sizeof(uint64_t); test < src_mem + src_mem_size; test += sizeof(uint64_t)){
            maccess(test);
            INST_SYNC;
        }
        if(maccess_t(tgt) < HIT_CYCLES_MAX){
            *((uint64_t*)(src_mem + sizeof(uint64_t) * len)) = addr;
            len++;
        }
    }
    INST_SYNC;
    clflush(tgt);
    printf("%ld\n", maccess_t(tgt));
    maccess(tgt);
    INST_SYNC;
    for(int i = 0; i < len; i++){
        maccess(*((uint64_t*)(src_mem + sizeof(uint64_t) * i)));
    }
    printf("%ld\n", maccess_t(tgt));
}

double ddp_poc(uint64_t** arr, int ind_scale, 
            uint64_t* thrash_arr, int thrash_size,
            uint64_t* data_buffer){
    // int thr_ind;
    // uint64_t time_taken[3 * M], MOD = M * ind_scale;

    // // For preventing unwanted compiler optimizations and adding
    // // data dependencies between instructions.
    // uint64_t __trash = 0, THR_MAX_IND = thrash_size / sizeof(uint64_t);

    // // thrash cache.
    // for (int j = 0; j < THR_MAX_IND - 2; j++) {
    //     __trash += (thrash_arr[j] ^ __trash) & 0b1111;
    //     __trash += (thrash_arr[j + 1] ^ __trash) & 0b1111;
    //     __trash += (thrash_arr[j + 2] ^ __trash) & 0b1111;
    // }
    // INST_SYNC;
    // for (int j = 0; j < THR_MAX_IND; j++) {
    //     thr_ind = j % THR_MAX_IND;
    //     clflush(thrash_arr[thr_ind]);
    //     thr_ind = (thr_ind + 1) % THR_MAX_IND;
    //     clflush(thrash_arr[thr_ind]);
    //     thr_ind = (thr_ind + 1) % THR_MAX_IND;
    //     clflush(thrash_arr[thr_ind]);
    // }

    // INST_SYNC;

    // // access everything.
    // for(int j = 0; j < M * ind_scale; j += ind_scale){
    //     maccess(arr[j]);
    // }

    // INST_SYNC;

    // // check from bottom up that all gone.
    // for(int i = 0; i < M; i++){
    //     time_taken[i] = maccess_t((ADDR_PTR)(&data_buffer[ind_gen(i)]));
    // }

    // INST_SYNC;

    // // thrash cache.
    // for (int j = 0; j < THR_MAX_IND - 2; j++) {
    //     __trash += (thrash_arr[j] ^ __trash) & 0b1111;
    //     __trash += (thrash_arr[j + 1] ^ __trash) & 0b1111;
    //     __trash += (thrash_arr[j + 2] ^ __trash) & 0b1111;
    // }
    // INST_SYNC;
    // for (int j = 0; j < THR_MAX_IND; j++) {
    //     thr_ind = j % THR_MAX_IND;
    //     clflush(thrash_arr[thr_ind]);
    //     thr_ind = (thr_ind + 1) % THR_MAX_IND;
    //     clflush(thrash_arr[thr_ind]);
    //     thr_ind = (thr_ind + 1) % THR_MAX_IND;
    //     clflush(thrash_arr[thr_ind]);
    // }

    // INST_SYNC;

    // // check from bottom up that all gone.
    // for(int i = 0; i < M; i++){
    //     time_taken[i + M] = maccess_t((ADDR_PTR)(&data_buffer[ind_gen(i)]));
    // }

    // INST_SYNC;

    int thr_ind, REPS = 20;
    uint64_t time_taken[REPS], MOD = M * ind_scale;

    // For preventing unwanted compiler optimizations and adding
    // data dependencies between instructions.
    uint64_t __trash = 0, THR_MAX_IND = thrash_size / sizeof(uint64_t);

    size_t test_offset = ind_gen(N);
    uint64_t mode = 0;

    // thrash cache.
    // for(int j = 0; j < M; j++){
    //     clflush((ADDR_PTR)(&data_buffer[ind_gen(j)]));
    // }
    // for(int j = 0; j < M; j++){
    //     clflush((ADDR_PTR)(&arr[(j * ind_scale) % MOD]));
    // }
    for (int j = 0; j < THR_MAX_IND - 2; j++) {
        __trash += (thrash_arr[j] ^ __trash) & 0b1111;
        __trash += (thrash_arr[j + 1] ^ __trash) & 0b1111;
        __trash += (thrash_arr[j + 2] ^ __trash) & 0b1111;
    }
    INST_SYNC;
    for (int j = 0; j < THR_MAX_IND; j++) {
        thr_ind = j % THR_MAX_IND;
        clflush(thrash_arr[thr_ind]);
        thr_ind = (thr_ind + 1) % THR_MAX_IND;
        clflush(thrash_arr[thr_ind]);
        thr_ind = (thr_ind + 1) % THR_MAX_IND;
        clflush(thrash_arr[thr_ind]);
    }

    INST_SYNC;

    for(int k = 0; k < REPS; k++){
        if(mode == 0){
            // attack mode
            for(int i = N; i < M; i++){
                arr[i * ind_scale] = &data_buffer[ind_gen(i)];
            }
        }
        else{
            // base mode
            for(int i = N; i < M; i++){
                arr[i * ind_scale] = (uint64_t*)(rand() & (MSB_MASK - 1));
            }
        }

        // flush cache
        // for(int j = 0; j < M; j++){
        //     clflush((ADDR_PTR)(&data_buffer[ind_gen(j)]));
        // }
        // for(int j = 0; j < M; j++){
        //     clflush((ADDR_PTR)(&arr[(j * ind_scale) % MOD]));
        // }
        for (int j = 0; j < THR_MAX_IND - 2; j++) {
            __trash += (thrash_arr[j] ^ __trash) & 0b1111;
            __trash += (thrash_arr[j + 1] ^ __trash) & 0b1111;
            __trash += (thrash_arr[j + 2] ^ __trash) & 0b1111;
        }
        INST_SYNC;
        for (int j = 0; j < THR_MAX_IND; j++) {
            thr_ind = j % THR_MAX_IND;
            clflush(thrash_arr[thr_ind]);
            thr_ind = (thr_ind + 1) % THR_MAX_IND;
            clflush(thrash_arr[thr_ind]);
            thr_ind = (thr_ind + 1) % THR_MAX_IND;
            clflush(thrash_arr[thr_ind]);
        }

        // waste loops
        for(uint64_t i=0; i<10000; i++) {
            __trash = (__trash + 1) & 0xffff;
            __trash = __trash * __trash;
        }

        INST_SYNC;

        // bring TLB entry
        maccess((ADDR_PTR)(&data_buffer[(test_offset+16)]));

        // aop access pattern
        volatile uint64_t **aop_arr = (volatile uint64_t**)arr;
        // for (int j = 0; j < N; j++) {
        //     __trash += *(aop_arr[(j * ind_scale) % MOD]) & MSB_MASK;
        // }
        for(int j = 0; j < (N - 2) * ind_scale; j += ind_scale){
            __trash += *(aop_arr[j % MOD]) & MSB_MASK;
            __trash += *(aop_arr[(j + ind_scale) % MOD]) & MSB_MASK;
            __trash += *(aop_arr[(j + ind_scale + ind_scale) % MOD]) & MSB_MASK;
        }

        // wait for DMP
        for(uint64_t i=0; i<10000; i++) {
            __trash = (__trash + 1) & 0xffff;
            __trash = __trash * __trash;
        }

        INST_SYNC;

        // time access ptr
        __trash = maccess_t((ADDR_PTR)(&data_buffer[test_offset]));

        if(mode == 0) {
            // atk mode
            time_taken[(REPS >> 1) + (k >> 1)] = __trash;
        } else {
            // base mode
            time_taken[k >> 1] = __trash;
        }
        printf("iter: %d time: %ld mode: %d\n", k, __trash, mode);
        mode = (k + 1) % 2;
    }

    __trash = 0;
    for(int i = 0; i < (REPS >> 1); i++){
        __trash += time_taken[i];
    }
    printf("Base: %.2f\n", __trash * 1.0f / (REPS >> 1));

    __trash = 0;
    for(int i = (REPS >> 1); i < REPS; i++){
        __trash += time_taken[i];
    }
    printf("Attk: %.2f\n", __trash * 1.0f / (REPS >> 1));

    return 0;

    // // access till N.
    // for(int j = 0; j < N * ind_scale; j += ind_scale){
    //     maccess(arr[j]);
    // }

    // INST_SYNC;

    // // check from bottom up that all gone.
    // for(int i = 0; i < M; i++){
    //     time_taken[i + 2 * M] = maccess_t((ADDR_PTR)(&data_buffer[ind_gen(i)]));
    // }

    // INST_SYNC;

    // // ith : direct access, i+Mth : no access, i+2Mth : indirect access
    // for(int i = 0; i < M; i++){
    //     // printf("%ld %ld %ld\n", time_taken[i + M], time_taken[i + 2 * M], time_taken[i]);
    //     printf("%d %ld\n", i, time_taken[i + 2 * M]);
    // }

    return 0;
}

double cache_reset_agent_test(uint64_t** arr, int ind_scale, 
                            uint64_t* thrash_arr, int thrash_size,
                            uint64_t* data_buffer){
    int err_c1 = 0, err_c2 = 0, thr_ind;
    uint64_t time_taken[2 * M], MOD = M * ind_scale;

    // For preventing unwanted compiler optimizations and adding
    // data dependencies between instructions.
    uint64_t __trash = 0, THR_MAX_IND = thrash_size / sizeof(uint64_t);

    // access everything.
    volatile uint64_t **aop_arr = (volatile uint64_t**)arr;
    for(int j = 0; j < (M - 2) * ind_scale; j += ind_scale){
        __trash += *(aop_arr[j % MOD]) & MSB_MASK;
        __trash += *(aop_arr[(j + ind_scale) % MOD]) & MSB_MASK;
        __trash += *(aop_arr[(j + ind_scale + ind_scale) % MOD]) & MSB_MASK;
    }

    INST_SYNC;

    // thrash cache.
    for (int j = 0; j < THR_MAX_IND - 2; j++) {
        __trash += (thrash_arr[j] ^ __trash) & 0b1111;
        __trash += (thrash_arr[j + 1] ^ __trash) & 0b1111;
        __trash += (thrash_arr[j + 2] ^ __trash) & 0b1111;
    }
    INST_SYNC;
    for (int j = 0; j < THR_MAX_IND; j++) {
        thr_ind = j % THR_MAX_IND;
        clflush(thrash_arr[thr_ind]);
        thr_ind = (thr_ind + 1) % THR_MAX_IND;
        clflush(thrash_arr[thr_ind]);
        thr_ind = (thr_ind + 1) % THR_MAX_IND;
        clflush(thrash_arr[thr_ind]);
    }
    // for(uint32_t page_offset=0; page_offset<PAGE_SIZE; \
    //     page_offset+=L1_LINE_SIZE) {
    //     // inner loop for ways
    //     for(uint32_t page_idx=0; page_idx<(thrash_size - 2*PAGE_SIZE); page_idx+=PAGE_SIZE) {
    //         __trash += (thrash_arr[(page_idx+page_offset)/sizeof(uint64_t)] ^ __trash) & 0b1111;
    //         __trash += (thrash_arr[(page_idx+page_offset+PAGE_SIZE)/sizeof(uint64_t)] ^ __trash) & 0b1111;
    //         __trash += (thrash_arr[(page_idx+page_offset+2*PAGE_SIZE)/sizeof(uint64_t)] ^ __trash) & 0b1111;
    //     }
    // }

    // for(int j = 0; j < M; j++){
    //     clflush(arr[(j * ind_scale) % MOD]);
    // }
    // for(int j = 0; j < M; j++){
    //     clflush((ADDR_PTR)(&arr[(j * ind_scale) % MOD]));
    // }

    INST_SYNC;

    // check from bottom up that all gone.
    for(int i = 0; i < M; i++){
        time_taken[i] = maccess_t((ADDR_PTR)(&data_buffer[ind_gen(i)]));
    }

    INST_SYNC;

    for(int i = 0; i < M; i++){
        time_taken[i + M] = maccess_t((ADDR_PTR)(&arr[(i * ind_scale) % MOD]));
    }

    INST_SYNC;

    for(int i = 0; i < M; i++){
        if(time_taken[i] < HIT_CYCLES_MAX) err_c1++;
    }
    for(int i = M; i < (M << 1); i++){
        if(time_taken[i] < HIT_CYCLES_MAX) err_c2++;
    }

    printf("bad array pointers: \t%d\t of \t%d\t (%.2f%%)\n", err_c2, M, (100.0 * err_c2 / M));
    printf("bad data  pointers: \t%d\t of \t%d\t (%.2f%%)\n", err_c1, M, (100.0 * err_c1 / M));

    return (err_c1 + err_c2) * 100.0 / (M << 1);
}

double everything_still_in_cache_test(uint64_t** arr, int ind_scale, 
                                     uint64_t* thrash_arr, int thrash_size,
                                     uint64_t* data_buffer){
    int err_c1 = 0, err_c2 = 0, thr_ind;
    uint64_t time_taken[2 * M], MOD = M * ind_scale;

    // For preventing unwanted compiler optimizations and adding
    // data dependencies between instructions.
    uint64_t __trash = 0, THR_MAX_IND = thrash_size / sizeof(uint64_t);

    // thrash cache
    for (int j = 0; j < THR_MAX_IND - 2; j++) {
        __trash += (thrash_arr[j] ^ __trash) & 0b1111;
        __trash += (thrash_arr[j + 1] ^ __trash) & 0b1111;
        __trash += (thrash_arr[j + 2] ^ __trash) & 0b1111;
    }
    INST_SYNC;
    for (int j = 0; j < THR_MAX_IND; j++) {
        thr_ind = j % THR_MAX_IND;
        clflush(thrash_arr[thr_ind]);
        thr_ind = (thr_ind + 1) % THR_MAX_IND;
        clflush(thrash_arr[thr_ind]);
        thr_ind = (thr_ind + 1) % THR_MAX_IND;
        clflush(thrash_arr[thr_ind]);
    }

    INST_SYNC;

    // bring everything into the cache.
    for(int j = 0; j < (M - 2) * ind_scale; j += ind_scale){
        maccess(arr[j % MOD]);
        maccess(arr[(j + ind_scale) % MOD]);
        maccess(arr[(j + ind_scale + ind_scale) % MOD]);
    }

    INST_SYNC;

    // ensure everything is in cache.
    for(int i = 0; i < M; i++){
        time_taken[i] = maccess_t((ADDR_PTR)(&data_buffer[ind_gen(i)]));
    }

    INST_SYNC;

    for(int i = 0; i < M; i++){
        time_taken[i + M] = maccess_t((ADDR_PTR)(&arr[(i * ind_scale) % MOD]));
    }

    INST_SYNC;

    for(int j = 0; j < M; j++){
        if(time_taken[j] > HIT_CYCLES_MAX) err_c1++;
    }
    for(int j = M; j < (M << 1); j++){
        if(time_taken[j] > HIT_CYCLES_MAX) err_c2++;
    }

    printf("bad array of pointers: \t%d\t of \t%d\t (%.2f%%)\n", err_c2, M, (100.0 * err_c2 / M));
    printf("bad data  of pointers: \t%d\t of \t%d\t (%.2f%%)\n", err_c1, M, (100.0 * err_c1 / M));

    return (err_c1 + err_c2) * 100.0 / (M << 1);
}

double not_overwritten_in_cache_test(uint64_t** arr, int ind_scale, 
                                     uint64_t* thrash_arr, int thrash_size,
                                     uint64_t* data_buffer, ADDR_PTR tgt_ind){
    int err_c = 0, thr_ind;
    uint64_t time_taken, MOD = M * ind_scale;
    ADDR_PTR tgt = arr[tgt_ind];

    // For preventing unwanted compiler optimizations and adding
    // data dependencies between instructions.
    uint64_t __trash = 0, THR_MAX_IND = thrash_size / sizeof(uint64_t);

    // thrash cache
    for (int j = 0; j < THR_MAX_IND - 2; j++) {
        __trash += (thrash_arr[j] ^ __trash) & 0b1111;
        __trash += (thrash_arr[j + 1] ^ __trash) & 0b1111;
        __trash += (thrash_arr[j + 2] ^ __trash) & 0b1111;
    }
    INST_SYNC;
    for (int j = 0; j < THR_MAX_IND; j++) {
        thr_ind = j % THR_MAX_IND;
        clflush(thrash_arr[thr_ind]);
        thr_ind = (thr_ind + 1) % THR_MAX_IND;
        clflush(thrash_arr[thr_ind]);
        thr_ind = (thr_ind + 1) % THR_MAX_IND;
        clflush(thrash_arr[thr_ind]);
    }

    INST_SYNC;

    // bring ith.
    maccess(tgt);

    INST_SYNC;

    // bring everything else into the cache.
    for(int j = 0; j < M; j++){
        if(j != tgt_ind) maccess((ADDR_PTR)(&data_buffer[ind_gen(j)]));
    }

    INST_SYNC;

    time_taken = maccess_t(tgt);
    if(time_taken > HIT_CYCLES_MAX) err_c++;

    return (100.0 * err_c);
}

double not_brought_in_cache_test(uint64_t** arr, int ind_scale, 
                                 uint64_t* thrash_arr, int thrash_size,
                                 uint64_t* data_buffer, ADDR_PTR tgt_ind){
    int err_c = 0, thr_ind;
    uint64_t time_taken, MOD = M * ind_scale;
    ADDR_PTR tgt = arr[tgt_ind];

    // For preventing unwanted compiler optimizations and adding
    // data dependencies between instructions.
    uint64_t __trash = 0, THR_MAX_IND = thrash_size / sizeof(uint64_t);

    // thrash cache
    for (int j = 0; j < THR_MAX_IND - 2; j++) {
        __trash += (thrash_arr[j] ^ __trash) & 0b1111;
        __trash += (thrash_arr[j + 1] ^ __trash) & 0b1111;
        __trash += (thrash_arr[j + 2] ^ __trash) & 0b1111;
    }
    INST_SYNC;
    for (int j = 0; j < THR_MAX_IND; j++) {
        thr_ind = j % THR_MAX_IND;
        clflush(thrash_arr[thr_ind]);
        thr_ind = (thr_ind + 1) % THR_MAX_IND;
        clflush(thrash_arr[thr_ind]);
        thr_ind = (thr_ind + 1) % THR_MAX_IND;
        clflush(thrash_arr[thr_ind]);
    }

    INST_SYNC;

    // bring everything into the cache.
    for(int j = 0; j < (M - 2) * ind_scale; j += ind_scale){
        maccess(arr[j % MOD]);
        maccess(arr[(j + ind_scale) % MOD]);
        maccess(arr[(j + ind_scale + ind_scale) % MOD]);
    }

    INST_SYNC;

    // flush ith.
    clflush(tgt);

    INST_SYNC;

    for(int j = 0; j < M; j++){
        if(j != tgt_ind) maccess((ADDR_PTR)(&data_buffer[ind_gen(j)]));
    }

    INST_SYNC;

    // confirm not brought back in.
    time_taken = maccess_t(tgt);
    if(time_taken < HIT_CYCLES_MAX) err_c++;

    return (err_c * 100.0);
}

double others_still_in_cache_test(uint64_t** arr, int ind_scale, 
                                 uint64_t* thrash_arr, int thrash_size,
                                 uint64_t* data_buffer, ADDR_PTR tgt_ind){
    int err_c = 0, thr_ind;
    uint64_t time_taken[M], MOD = M * ind_scale;
    ADDR_PTR tgt = arr[tgt_ind];

    // For preventing unwanted compiler optimizations and adding
    // data dependencies between instructions.
    uint64_t __trash = 0, THR_MAX_IND = thrash_size / sizeof(uint64_t);

    // thrash cache.
    for (int j = 0; j < THR_MAX_IND - 2; j++) {
        __trash += (thrash_arr[j] ^ __trash) & 0b1111;
        __trash += (thrash_arr[j + 1] ^ __trash) & 0b1111;
        __trash += (thrash_arr[j + 2] ^ __trash) & 0b1111;
    }
    INST_SYNC;
    for (int j = 0; j < THR_MAX_IND; j++) {
        thr_ind = j % THR_MAX_IND;
        clflush(thrash_arr[thr_ind]);
        thr_ind = (thr_ind + 1) % THR_MAX_IND;
        clflush(thrash_arr[thr_ind]);
        thr_ind = (thr_ind + 1) % THR_MAX_IND;
        clflush(thrash_arr[thr_ind]);
    }

    INST_SYNC;

    // bring everything into the cache.
    for(int j = 0; j < (M - 2) * ind_scale; j += ind_scale){
        maccess(arr[j % MOD]);
        maccess(arr[(j + ind_scale) % MOD]);
        maccess(arr[(j + ind_scale + ind_scale) % MOD]);
    }

    INST_SYNC;

    // flush ith.
    clflush(tgt);

    INST_SYNC;

    for(int j = 0; j < M; j++){
        if(j != tgt_ind) time_taken[j] = maccess_t((ADDR_PTR)(&data_buffer[ind_gen(j)]));
    }

    INST_SYNC;

    for(int j = 0; j < M; j++){
        // confirm jth still in cache.
        if(j != tgt_ind && time_taken[j] > HIT_CYCLES_MAX) err_c++;
    }

    return (err_c * 100.0) / (M - 1);
}

char* convertToBinary(uint64_t num, char* msg) {
    for(int k = 0; k < 64; k++){
        msg[k] = '\0';
    }
    uint8_t binary[64];
    memset(binary, 0, 64);
    int i = 0, c = 0;

    while (num != 0) {
        binary[i++] = num % 2;
        num >>= 1;
    }

    for (int j = 63; j >= 0; j--) {
        if(binary[j] == 0) msg[c] = '0';
        else msg[c] = '1';
        c++;
        if(j%8==0)
        {
            msg[c] = '|';
            c++;
        }
    }
    msg[c - 1] = '\0';
    return msg;
}
