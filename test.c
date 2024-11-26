#include <sys/mman.h>
#include <time.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include <stdint.h>
#define RND_INIT 8

#define MSB_MASK    0x8000000000000000ULL

#define L1_SIZE (100 * 1024)
#define L2_SIZE (3 * 1024 * 1024)

#define L2_LINE_SIZE 64
#define L1_LINE_SIZE 64

#define KB 1024ULL
#define MB (1024*KB)

#define PAGE_SIZE               (16 * KB)

// Use a Lehmer RNG as PRNG
// https://en.wikipedia.org/wiki/Lehmer_random_number_generator
#define PNRG_a 75
#define PRNG_m 8388617
#define prng(x) ((PNRG_a * x) % PRNG_m)

#define SIZE_DATA_ARRAY (PRNG_m * L2_LINE_SIZE)
#define SIZE_THRASH_ARRAY ((L1_SIZE + L2_SIZE) * 8)

#define ADDR_CHECK(addr1, addr2) ((addr1 >> 32) == (addr2 >> 32))


#define REPETITIONS 32

// Merges arr[left_idx...mid_idx] and arr[mid_idx+1..right_idx]
void merge_8B(uint64_t* array, int left_idx, int mid_idx, int right_idx) {
    int i, j, k;
    int n1 = mid_idx - left_idx + 1;
    int n2 = right_idx - mid_idx;

    /* create temp arrays */
    uint64_t L[n1], R[n2];

    /* Copy data to temp arrays L[] and R[] */
    for (i = 0; i < n1; i++)
        L[i] = array[left_idx + i];
    for (j = 0; j < n2; j++)
        R[j] = array[mid_idx + 1 + j];

    /* Merge the temp arrays back into arr[l..r]*/
    i = 0; // Initial index of first subarray
    j = 0; // Initial index of second subarray
    k = left_idx; // Initial index of merged subarray
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            array[k] = L[i];
            i++;
        }
        else {
            array[k] = R[j];
            j++;
        }
        k++;
    }

    /* Copy the remaining elements of L[], if there are any */
    while (i < n1) {
        array[k] = L[i];
        i++;
        k++;
    }

    /* Copy the remaining elements of R[], if there are any */
    while (j < n2) {
        array[k] = R[j];
        j++;
        k++;
    }
}

void merge_4B(uint32_t* array, int left_idx, int mid_idx, int right_idx) {
    int i, j, k;
    int n1 = mid_idx - left_idx + 1;
    int n2 = right_idx - mid_idx;

    /* create temp arrays */
    uint32_t L[n1], R[n2];

    /* Copy data to temp arrays L[] and R[] */
    for (i = 0; i < n1; i++)
        L[i] = array[left_idx + i];
    for (j = 0; j < n2; j++)
        R[j] = array[mid_idx + 1 + j];

    /* Merge the temp arrays back into arr[l..r]*/
    i = 0; // Initial index of first subarray
    j = 0; // Initial index of second subarray
    k = left_idx; // Initial index of merged subarray
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            array[k] = L[i];
            i++;
        }
        else {
            array[k] = R[j];
            j++;
        }
        k++;
    }

    /* Copy the remaining elements of L[], if there are any */
    while (i < n1) {
        array[k] = L[i];
        i++;
        k++;
    }

    /* Copy the remaining elements of R[], if there are any */
    while (j < n2) {
        array[k] = R[j];
        j++;
        k++;
    }
}

void merge_sort_8B(uint64_t* array, int left_idx, int right_idx)
{
    if (left_idx < right_idx) {
        // Same as (l+r)/2, but avoids overflow for large l and h
        int mid_idx = left_idx + (right_idx - left_idx) / 2;

        // Sort first and second halves
        merge_sort_8B(array, left_idx, mid_idx);
        merge_sort_8B(array, mid_idx + 1, right_idx);

        merge_8B(array, left_idx, mid_idx, right_idx);
    }
}

void merge_sort_4B(uint32_t* array, int left_idx, int right_idx)
{
    if (left_idx < right_idx) {
        // Same as (l+r)/2, but avoids overflow for large l and h
        int mid_idx = left_idx + (right_idx - left_idx) / 2;

        // Sort first and second halves
        merge_sort_4B(array, left_idx, mid_idx);
        merge_sort_4B(array, mid_idx+1, right_idx);

        merge_4B(array, left_idx, mid_idx, right_idx);
    }
}

void sort(void* array, int num_elements, int element_size) {
    if (element_size == 8)
        merge_sort_8B((uint64_t*)array, 0, num_elements-1);
    else if (element_size == 4)
        merge_sort_4B((uint32_t*)array, 0, num_elements-1);
    else {
        printf("ERROR: haven't implemented shuffle() for %d byte\n", element_size);
        exit(1);
    }
}

uint64_t max_8B(uint64_t* arr, int n) {
    sort(arr, n, sizeof(uint64_t));
    return arr[n-1];
}

uint64_t min_8B(uint64_t* arr, int n) {
    sort(arr, n, sizeof(uint64_t));
    return arr[0];
}

float mean_8B(uint64_t* arr, int n) {
    float sum = 0.0;
    for (int i = 0; i < n; i++)
        sum += (float)arr[i];
    return sum / n;
}

uint64_t median_8B(uint64_t* arr, int n) {
    sort(arr, n, sizeof(uint64_t));
    return arr[n/2];
}

void maccess(uint64_t addr)
{
  // Use mov instruction.
  asm volatile("mov (%0), %%rdx"
	       : /*output*/
	       : /*input*/ "r"(addr)
	       : /*clobbers*/ "%rdx");
  
  return;
}


uint64_t maccess_t(uint64_t addr)
{
  uint64_t cycles;

  // Use a mov instruction to load an address "addr",
  // which is sandwiched between two rdtscp instructions.
  // Calculate the latency using difference of the output of two rdtscps.
  asm volatile("rdtscp\n\t"
               "shl  $32,     %%rdx\n\t"
	             "or   %%rdx,   %%rax\n\t"
               "mov  (%%rbx), %%rdx\n\t"
               "mov  %%rax,   %%rbx\n\t"
               "rdtscp\n\t"
               "shl  $32,     %%rdx\n\t"
	             "or   %%rdx,   %%rax\n\t"
               "sub  %%rbx,   %%rax"
	       : /*output*/ "=a"(cycles)
	       : /*input*/  "b"(addr)
	       : /*clobbers*/ "%rcx", "%rdx");
  return cycles;
}

void return_size(char* output, size_t bytes) {
    if (bytes <= KB) {
        sprintf(output, "%lluB", (uint64_t)bytes);
    } else if (bytes <= MB) {
        sprintf(output, "%lluKB", bytes / KB);
    } else {
        sprintf(output, "%lluMB", bytes / MB);
    }
}

int main(int argc, char** argv) {
    /*
    argv[1]: core_id (0-3 e core, 4-7 p core)
    argv[2]: size of the victim array
    argv[3]: training length
    argv[4]: test ptr idx
    */
    // read arguments
    size_t victim_size = 512;
    int training_length = 502;
    int test_idx = 101;

    // Allocate memory for data array
    uint64_t* data_buf_addr = 
        mmap(NULL, SIZE_DATA_ARRAY, PROT_READ | PROT_WRITE, 
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    printf("[+] data_buf_addr: %p\n", data_buf_addr);

    // Allocate memory for aop array
    char victim_size_str[10];
    return_size(victim_size_str, victim_size*sizeof(uint64_t));
    uint64_t* aop_addr = 
        mmap(NULL, victim_size*sizeof(uint64_t), PROT_READ | PROT_WRITE, 
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    printf("[+] aop_addr: %p\n", aop_addr);
    assert(ADDR_CHECK((uintptr_t)data_buf_addr, (uintptr_t)aop_addr));

    // Allocate memory for thrash array
    uint64_t *thrash_arr = mmap(0, SIZE_THRASH_ARRAY, PROT_READ | PROT_WRITE,
            MAP_ANON | MAP_PRIVATE, -1, 0);

    // Fill data array with random data
    srand(time(NULL));
    for (int i = 0; i < SIZE_DATA_ARRAY / sizeof(uint64_t); i++) {
        data_buf_addr[i] = rand() & (MSB_MASK - 1);
    }
    // Fill aop array with random data
    for (int i = 0; i < PAGE_SIZE / sizeof(uint64_t); i++) {
        aop_addr[i] = rand() & (MSB_MASK - 1);
    }
    // Fill thrash array with zeros
    for (int i = 0; i < SIZE_THRASH_ARRAY / sizeof(uint64_t); i++) {
        thrash_arr[i] = rand() & (MSB_MASK - 1);
    }

    // Test target setting
    printf("[+] Test target setting:\n");
    uint64_t trash = 0;
    uint64_t rnd_idx = RND_INIT;
    for(int i=0; i<victim_size; i++) {
        aop_addr[i] = (uint64_t)&data_buf_addr[rnd_idx*L2_LINE_SIZE/sizeof(uint64_t)];
        rnd_idx = prng(rnd_idx);
    }
    printf("[+++] test_idx: %d\n", test_idx);
    rnd_idx = RND_INIT;
    for(int i=0; i<test_idx+training_length; i++) {
        rnd_idx = prng(rnd_idx);
    }
    uint64_t test_offset = L2_LINE_SIZE * rnd_idx / sizeof(uint64_t);
    printf("[+++] test ptr: %#llx\n", (uint64_t)&data_buf_addr[test_offset]);

    // Initial latency record array
    uint64_t latency_ptr_base_array[REPETITIONS];
    uint64_t latency_ptr_atk_array[REPETITIONS];

    // Measurement start
    uint64_t latency_ptr = 0;
    uint64_t mode = 0;
    printf("[+] AoP setting:\n");
    printf("[+++] victim_size: %zu entries (%s)\n", victim_size, victim_size_str);
    printf("[+++] Training length: %d\n", training_length);
    for(int i=0; i<REPETITIONS*2; i++) {
        if(mode == 0) {
            // atk mode
            rnd_idx = RND_INIT;
            // skip training
            for(int aop_idx=0; aop_idx<victim_size; aop_idx++) {
                if(aop_idx >= training_length) {
                    aop_addr[aop_idx] = (uint64_t)&data_buf_addr[rnd_idx*L2_LINE_SIZE/sizeof(uint64_t)];
                }
                rnd_idx = prng(rnd_idx);
            }
        } else {
            // base mode
            for(int aop_idx=0; aop_idx<victim_size; aop_idx++) {
                if(aop_idx >= training_length) {
                    aop_addr[aop_idx] = rand() & (MSB_MASK - 1);
                }
            }
        }

        // flush cache
        for(uint32_t page_offset=0; page_offset<PAGE_SIZE; \
            page_offset+=L1_LINE_SIZE) {
            // inner loop for ways
            for(uint32_t page_idx=0; page_idx<(SIZE_THRASH_ARRAY - 2*PAGE_SIZE); page_idx+=PAGE_SIZE) {
                trash += (thrash_arr[(page_idx+page_offset)/sizeof(uint64_t)] ^ trash) & 0b1111;
                trash += (thrash_arr[(page_idx+page_offset+PAGE_SIZE)/sizeof(uint64_t)] ^ trash) & 0b1111;
                trash += (thrash_arr[(page_idx+page_offset+2*PAGE_SIZE)/sizeof(uint64_t)] ^ trash) & 0b1111;
            }
        }

        for (uint64_t i=0; i<10000; i++) {
            trash = (trash + 1) & 0xffff;
            trash = trash * trash;
        }

        // bring TLB entry
        maccess((uint64_t)(&data_buf_addr[test_offset+128]));

        // aop access pattern
        volatile uint64_t **aop = (uint64_t**)aop_addr;

        // Training loop
        for (int j = 0; j < training_length; j++) {
            trash += *aop[j % training_length] & MSB_MASK;
        }

        // wait for DMP
        for (uint64_t i=0; i<1000; i++) {
            trash = (trash + 1) & 0xffff;
            trash = trash * trash;
        }

        // time access ptr
        latency_ptr = maccess_t((uint64_t)(&data_buf_addr[test_offset | (trash & MSB_MASK)]));

        if(mode == 0) {
            // atk mode
            latency_ptr_atk_array[i/2] = latency_ptr;
        } else {
            // base mode
            latency_ptr_base_array[i/2] = latency_ptr;
        }
        mode = !(mode | (trash & MSB_MASK));
    }

    // Store measurements
    FILE *output_file_baseline = fopen("./base.txt", "w");
    FILE *output_file_aop = fopen("./atk.txt", "w");
    if (output_file_baseline == NULL || output_file_aop == NULL) {
            perror("output files");
    }
    fprintf(output_file_baseline, "test\n");
    fprintf(output_file_aop, "test\n");
    for(int i=0; i<REPETITIONS; i++) {
        fprintf(output_file_baseline, "%llu\n",
            latency_ptr_base_array[i]);
        fprintf(output_file_aop, "%llu\n",
            latency_ptr_atk_array[i]);
    }

    // Clean up
    fclose(output_file_baseline);
    fclose(output_file_aop);

    // Print result
    printf("[+] Result:\n");
    uint64_t ptr_atk_min = min_8B(latency_ptr_atk_array, REPETITIONS);
    uint64_t ptr_atk_median = median_8B(latency_ptr_atk_array, REPETITIONS);
    printf("[+++] ATK:\n");
    printf("[+++++] MIN: %llu\n", ptr_atk_min);
    printf("[+++++] MEDIAN: %llu\n", ptr_atk_median);
    uint64_t ptr_base_min = min_8B(latency_ptr_base_array, REPETITIONS);
    uint64_t ptr_base_median = median_8B(latency_ptr_base_array, REPETITIONS);
    printf("[+++] BASE:\n");
    printf("[+++++] MIN: %llu\n", ptr_base_min);
    printf("[+++++] MEDIAN: %llu\n", ptr_base_median);
    return 0;
}