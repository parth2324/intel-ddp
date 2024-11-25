#include "augrey_test.h"

#define M 256
// #define TEST_SIZE 8

// #define N (M - TEST_SIZE)

void cpuid_chk(){
  char* _a = malloc(sizeof(char) * 100);
  char* _b = malloc(sizeof(char) * 100);
  char* _c = malloc(sizeof(char) * 100);
  char* _d = malloc(sizeof(char) * 100);
  uint64_t a, b, c, d;
  asm volatile(
            "mov $0x4, %%eax\n\t"
            "mov $0x2, %%ecx\n\t"
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
    if(store_bypass == 0){
        err = prctl(PR_SET_SPECULATION_CTRL, PR_SPEC_STORE_BYPASS, PR_SPEC_DISABLE, 0L, 0L);
    }
    else{
        err = prctl(PR_SET_SPECULATION_CTRL, PR_SPEC_STORE_BYPASS, PR_SPEC_ENABLE, 0L, 0L);
    }
    if(err == -1) return errno;

    int ptrs_per_line;  // 1 to 8 for 64B line
    sscanf(argv[1], "%d", &ptrs_per_line);
    printf("%d ptrs/line:\n", ptrs_per_line);

    int aop_ind_scale = U64S_PER_LINE + 1 - ptrs_per_line;
    int aop_mem = M * CACHE_LINE_SIZE * aop_ind_scale + AOP_ALIGN_WIN;
    int buf_mem = BUF_MEM + BUF_ALIGN_WIN;

    // printf("Initializing aop.\n");
    volatile uint64_t *aop = mmap(0, aop_mem, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(aop != MAP_FAILED);
    memset((uint64_t*)aop, 0, aop_mem);
    ADDR_PTR curr = (ADDR_PTR)aop;
    curr += (-curr) & (AOP_ALIGN_WIN - 1);
    aop = (volatile uint64_t *)curr;

    // printf("Initializing source buffer.\n");
    volatile uint64_t *data_buffer = mmap(0, buf_mem, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(data_buffer != MAP_FAILED);
    for(uint64_t i = 0; i < buf_mem / sizeof(uint64_t); i++) {
        data_buffer[i] = rand() & (MSB_MASK - 1);
    }
    curr = (ADDR_PTR)data_buffer;
    curr += (-curr) & (BUF_ALIGN_WIN - 1);
    data_buffer = (volatile uint64_t *)curr;

    // printf("Allocating pointers to aop from source buffer.\n");
    for(int i = 0; i < M; i++) {
        aop[i * aop_ind_scale] = (ADDR_PTR)(&data_buffer[ind_gen(i)]);
    }
    // Testing uniqueness of allocator below:
    for(int i = 0; i < M; i++) {
        for(int j = 0; j < M ; j++) {
            if(i != j && aop[i * aop_ind_scale] == aop[j * aop_ind_scale]){
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
    volatile uint64_t *thrash_arr = mmap(0, thr_mem, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    for(uint64_t i = 0; i < thr_mem / sizeof(uint64_t); i++) {
        tgt_ind = (1398107 * (i + 1)) % (thr_mem / sizeof(uint64_t));
        thrash_arr[i] = (ADDR_PTR)&thrash_arr[tgt_ind]; // rand() & (MSB_MASK - 1);
    }

    // filling in the important addresses that must be flushed.
    for(uint64_t i = 0; i < M; i++) {
        tgt_ind = (151 * (i + 1)) % (thr_mem / sizeof(uint64_t));
        thrash_arr[tgt_ind] = (ADDR_PTR)&aop[i * aop_ind_scale];
    }
    for(uint64_t i = 0; i < M; i++) {
        tgt_ind = (151 * (i + M + 1)) % (thr_mem / sizeof(uint64_t));
        thrash_arr[tgt_ind] = aop[i * aop_ind_scale];
    }

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

    int repetitions = 10000;
    uint64_t *time_taken = malloc(sizeof(uint64_t) * M), MOD = M * aop_ind_scale;
    memset(time_taken, 0, sizeof(uint64_t) * M);

    // For preventing unwanted compiler optimizations and adding
    // data dependencies between instructions.
    uint64_t __trash = 0;

    // printf("Beginning main test.\n");
    // for(int i = 0; i < repetitions; i++){
    //     // thrash cache
    //     for(int j = 0; j < M; j++){
    //         clflush((ADDR_PTR)(aop[(j * aop_ind_scale) % MOD]));
    //     }

    //     INST_SYNC;

    //     // bring training component into the cache.
    //     for(int j = 0; j < N; j++){
    //         __trash += (*(aop[(j * aop_ind_scale) % MOD]) ^ __trash) & 0b1111;
    //         INST_SYNC;
    //     }

    //     // measure testing component latencies.
    //     for(int j = N; j < M; j++){
    //         time_taken[j] += maccess_t((ADDR_PTR)(aop[(j * aop_ind_scale) % MOD]));
    //         INST_SYNC;
    //     }
    // }

    // get average times and calc fraction brought in.
    // int ddp_hit_c = 0;
    // double avg_hit_time = 0.0;
    // for(int j = N; j < M; j++){
    //     time_taken[j] /= repetitions;
    //     if(time_taken[j] < HIT_CYCLES_MAX){
    //         ddp_hit_c++;
    //         avg_hit_time += time_taken[j];
    //     }
    // }
    // avg_hit_time /= ddp_hit_c;

    // memset(time_taken, 0, sizeof(uint64_t) * M);
    // for(int i = 0; i < repetitions; i++){
    //     // thrash cache
    //     for(int j = 0; j < M; j++){
    //         clflush((ADDR_PTR)(aop[(j * aop_ind_scale) % MOD]));
    //     }

    //     INST_SYNC;

    //     // measure testing component latencies.
    //     for(int j = N; j < M; j++){
    //         time_taken[j] += maccess_t((ADDR_PTR)(aop[(j * aop_ind_scale) % MOD]));
    //         INST_SYNC;
    //     }
    // }

    // get average times and calc fraction not brought in.
    // int miss_c = 0;
    // double avg_miss_time = 0.0;
    // for(int j = N; j < M; j++){
    //     time_taken[j] /= repetitions;
    //     if(time_taken[j] > HIT_CYCLES_MAX){
    //         miss_c++;
    //         avg_miss_time += time_taken[j];
    //     }
    // }
    // avg_miss_time /= miss_c;

    // printf("SSPD = %d with    training:\t %.3f%%\t     brought in,\t %d\t of\t %d\t with avg hit  time: %.1f\n", 
    // store_bypass, ddp_hit_c * 100.0 / (M - N), ddp_hit_c, (M - N), avg_hit_time);
    // printf("SSPD = %d without training:\t %.3f%%\t not brought in,\t %d\t of\t %d\t with avg miss time: %.1f\n", 
    // store_bypass, miss_c * 100.0 / (M - N), miss_c, (M - N), avg_miss_time);

    // free(time_taken);
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

double cache_reset_agent_test(volatile uint64_t* arr, int ind_scale, 
                              volatile uint64_t* thrash_arr, int thrash_size,
                              volatile uint64_t* data_buffer){
    int err_c1 = 0, err_c2 = 0, thr_ind;
    uint64_t *time_taken = malloc(sizeof(uint64_t) * 2 * M), MOD = M * ind_scale;

    // For preventing unwanted compiler optimizations and adding
    // data dependencies between instructions.
    uint64_t __trash = 0, THR_MAX_IND = thrash_size / sizeof(uint64_t);

    // access everything.
    for(int i = 0; i < M; i++){
        maccess(arr[(i * ind_scale) % MOD]);
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

    printf("bad array of pointers: \t%d\t of \t%d\t (%.2f%%)\n", err_c2, M, (100.0 * err_c2 / M));
    printf("bad data  of pointers: \t%d\t of \t%d\t (%.2f%%)\n", err_c1, M, (100.0 * err_c1 / M));

    free(time_taken);
    return (err_c1 + err_c2) * 100.0 / (M << 1);
}

double everything_still_in_cache_test(volatile uint64_t* arr, int ind_scale, 
                                      volatile uint64_t* thrash_arr, int thrash_size,
                                      volatile uint64_t* data_buffer){
    int err_c1 = 0, err_c2 = 0, thr_ind;
    uint64_t *time_taken = malloc(sizeof(uint64_t) * 2 * M), MOD = M * ind_scale;

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
    for(int j = 0; j < M; j++){
        maccess(arr[(j * ind_scale) % MOD]);
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

    free(time_taken);
    return (err_c1 + err_c2) * 100.0 / (M << 1);
}

double not_overwritten_in_cache_test(volatile uint64_t* arr, int ind_scale, 
                                     volatile uint64_t* thrash_arr, int thrash_size,
                                     volatile uint64_t* data_buffer, ADDR_PTR tgt_ind){
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

double not_brought_in_cache_test(volatile uint64_t* arr, int ind_scale, 
                                 volatile uint64_t* thrash_arr, int thrash_size,
                                 volatile uint64_t* data_buffer, ADDR_PTR tgt_ind){
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
    for(int j = 0; j < M; j++){
        maccess(arr[(j * ind_scale) % MOD]);
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

double others_still_in_cache_test(volatile uint64_t* arr, int ind_scale, 
                                  volatile uint64_t* thrash_arr, int thrash_size,
                                  volatile uint64_t* data_buffer, ADDR_PTR tgt_ind){
    int err_c = 0, thr_ind;
    uint64_t *time_taken = malloc(sizeof(uint64_t) * M), MOD = M * ind_scale;
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
    for(int j = 0; j < M; j++){
        maccess(arr[(j * ind_scale) % MOD]);
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

    free(time_taken);
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
