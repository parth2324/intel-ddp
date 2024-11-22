#include "augrey_test.h"

int main(){
    uint64_t M = 100;
    uint64_t* arr = gen_array(M);
    if(!test_uniqueness(arr, M)) printf("Fail.\n");
    else printf("Pass.\n");
    printf("--------------------------------------------------------------------------\n");
    if(!test_uniqueness(arr, M)) printf("Fail.\n");
    else printf("Pass.\n");
    return 0;
}

uint64_t* gen_array(uint64_t M){
    void* src = malloc(sizeof(uint8_t) * (M + 1) * 64);
    ADDR_PTR curr = (ADDR_PTR)(src);
    curr += (-curr) & 63;
    uint64_t* arr = malloc(sizeof(uint64_t) * M);
    for(int i = 0; i < M; i++){
        arr[i] = curr;
        *((uint8_t*)curr) = 0;
        curr += 64;
    }
    return arr;
}

bool test_uniqueness(uint64_t* arr, uint64_t M){
    // first bring everything into the cache.
    for(int i = 0; i < M; i++){
        maccess(arr[i]);
    }
    char *msg = malloc(sizeof(char) * 100);
    uint64_t time_taken;
    // ensure everything is in cache.
    for(int i = 0; i < M; i++){
        time_taken = maccess_t(arr[i]);
        if(time_taken > HIT_CYCLES_MAX){
            printf("Did not persist in cache, checking %d:%s.\n", 
            i, convertToBinary(arr[i], msg));
            // return false;
        }
    }
    // evict one and check all others in, cautiously.
    for(int i = 0; i < M; i++){
        clflush(arr[i]);
        for(int j = 0; j < i; j++){
            time_taken = maccess_t(arr[j]);
            if(time_taken > HIT_CYCLES_MAX){
                printf("Got evicted from cache, checking %d:%ld:%s against %d:%ld:%s.\n",
                i, arr[i], convertToBinary(arr[i], msg),
                j, arr[j], convertToBinary(arr[j], msg));
                // return false;
            }
        }
        time_taken = maccess_t(arr[i]);
        clflush(arr[i]);
        if(time_taken < HIT_CYCLES_MAX){
            printf("Got brought back in cache, checking values before %d:%s.\n",
            i, convertToBinary(arr[i], msg));
            // return false;
        }
        for(int j = i + 1; j < M; j++){
            time_taken = maccess_t(arr[j]);
            if(time_taken > HIT_CYCLES_MAX){
                printf("Got evicted from cache, checking %d:%ld:%s against %d:%ld:%s.\n", 
                i, arr[i], convertToBinary(arr[i], msg),
                j, arr[j], convertToBinary(arr[j], msg));
                // return false;
            }
        }
        time_taken = maccess_t(arr[i]);
        if(time_taken < HIT_CYCLES_MAX){
            printf("Got brought back in cache, checking values after %d:%s.\n",
            i, convertToBinary(arr[i], msg));
            // return false;
        }
    }
    free(msg);
    return true;
}

char* convertToBinary(uint64_t num, char* msg) {
    for(int k = 0; k < 64; k++){
        msg[k] = '\0';
    }
    uint8_t binary[64]; // Assuming integer is 32 bits
    int i = 0, c = 0; // Index for binary array

    while (num != 0) {
        binary[i++] = num % 2; // Get the remainder when divided by 2
        num /= 2; // Divide the number by 2
    }

    // Print the binary representation in reverse order
    for (int j = 17; j >= 0; j--) {
        if(binary[j] == 0) msg[c] = '0';
        else msg[c] = '1';
        c++;
    }
    msg[c] = '\0';
    return msg;
}
