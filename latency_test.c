
#include "util.h"

//**************************
// ADJUST THESE PARAMETERS IF NEEDED
//**************************
//Num Latency Buckets.
#define NUM_LAT_BUCKETS (400)
//Each Bucket Step.
#define BUCKET_LAT_STEP (1)
//Num Iterations
#define NUM_ITER        (10000)
//---------------------------------------------------------------------------

uint64_t* hits_lat_histogram ;
uint64_t* misses_lat_histogram ;
int num_hit_accesses = 0;
int num_miss_accesses = 0;

uint8_t sample_memory[1024*4096] __attribute__ ((aligned (4096))); 
ADDR_PTR addr;

//---------------------------------------------------------------------------

int main(int argc, char **argv)
{

  //Initialize the sample_memory
  srand(42);
  for (int i=0; i< 1024*4096; i++){
    sample_memory[i] = rand()%256;
  }

  //Initialize the address to test.
  uint8_t* sample_addr = &sample_memory[rand()% (1024*4096)];
  ADDR_PTR addr = (ADDR_PTR)sample_addr;
  
  //Initialize the latency histogram variables
  misses_lat_histogram = (uint64_t*) calloc((NUM_LAT_BUCKETS+1), sizeof(uint64_t));
  hits_lat_histogram =   (uint64_t*) calloc((NUM_LAT_BUCKETS+1), sizeof(uint64_t));

  //---------------------------------------
  //1. Initialize State

  maccess(addr);
  clflush(addr);

  //---------------------------------------------------------------------------

  //2. Get Misses Histogram.
  uint64_t time_taken;

  for (int i=0; i< NUM_ITER;i++){
    //Flush
    clflush(addr);
    
    //Load and Time Access
    time_taken = maccess_t(addr);

    //Update appropriate latency bucket of histogram
    if(time_taken <= NUM_LAT_BUCKETS){
      misses_lat_histogram[time_taken] += 1;
    } else {
      misses_lat_histogram[NUM_LAT_BUCKETS] += 1;
    }
    num_miss_accesses++;
  }

  //---------------------------------------------------------------------------

  //3. Get Hits Histogram.

  maccess(addr);

  //Perform the following NUM_ITER times:
  for (int i=0; i< NUM_ITER;i++){
    time_taken = maccess_t(addr);
    if(time_taken <= NUM_LAT_BUCKETS){
      hits_lat_histogram[time_taken] += 1;
    } else {
      hits_lat_histogram[NUM_LAT_BUCKETS] += 1;
    }
    num_hit_accesses++;
  }


  //----------------------------------------------------------------
  // NO NEED TO CHANGE ANYTHING BELOW THIS LINE
  //----------------------------------------------------------------

  //Print the Histogram
  printf("Total Number of Accesses: %d (Hits and Misses each)\n",NUM_ITER);
  
  puts("-------------------------------------------------------");
  printf("Latency(cycles)\tHits-Count \t\t Miss-Count \n");
  puts("-------------------------------------------------------");

  for (int i=0; i<NUM_LAT_BUCKETS ;i++){
    printf("%d   \t %15ld  \t %15ld \n",
	   i*BUCKET_LAT_STEP, hits_lat_histogram[i], misses_lat_histogram[i]);
  }
  printf("%d+   \t %15ld  \t %15ld \n",NUM_LAT_BUCKETS*BUCKET_LAT_STEP, hits_lat_histogram[NUM_LAT_BUCKETS], misses_lat_histogram[NUM_LAT_BUCKETS]);
  
  puts("-------------------------------------------------------");
  printf("Total  \t %15d  \t %15d \n",num_hit_accesses,num_miss_accesses);
  puts("-------------------------------------------------------");
  return 0;
}

