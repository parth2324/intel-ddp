#include "util.h"


//----------------------------------------------------------------
// PRIMITIVES FOR FLUSH+RELOAD CHANNEL
//----------------------------------------------------------------

/* Flush a cache block of address "addr" */
extern inline __attribute__((always_inline))
void clflush(ADDR_PTR addr)
{
  // Use clflush instruction.
  asm volatile ("clflush (%0)"
		: /*output*/
		: /*input*/ "r"(addr)
		: /*clobbers*/ );
}


/* Load address "addr" */
void maccess(ADDR_PTR addr)
{
  // Use mov instruction.
  asm volatile("mov (%0), %%rdx"
	       : /*output*/
	       : /*input*/ "r"(addr)
	       : /*clobbers*/ "%rdx");
  
  return;
}


/* Loads addr and measure the access time */
CYCLES maccess_t(ADDR_PTR addr)
{
  CYCLES cycles;

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


/* Returns Time Stamp Counter (using rdtscp function)*/
extern inline __attribute__((always_inline))
uint64_t rdtscp(void) {
  uint64_t cycles;
  asm volatile ("rdtscp\n"
		"shl $32,%%rdx\n"
		"or %%rdx, %%rax\n"		      
		: /* outputs */ "=a" (cycles));
  return cycles;
}

//----------------------------------------------------------------
// NO NEED TO CHANGE ANYTHING BELOW THIS LINE
//----------------------------------------------------------------

/*
 * Convert a given ASCII string to a binary string.
 * From:
 * https://stackoverflow.com/questions/41384262/convert-string-to-binary-in-c
 */
char *string_to_binary(char *s)
{
    if (s == NULL) return 0; /* no input string */
    size_t len = strlen(s) ;

    // Each char is one byte (8 bits) and + 1 at the end for null terminator
    char *binary = malloc(len * 8 + 1);
    binary[0] = '\0';
	
    for (size_t i = 0; i < len; ++i) {
        char ch = s[i];
        for (int j = 7; j >= 0; --j) {
            if (ch & (1 << j)) {
                strcat(binary, "1");
            } else {
                strcat(binary, "0");
            }
        }
    }    
    return binary;
}

/*
 * Convert 8 bit data stream into character and return
 */
char *conv_char(char *data, int size, char *msg)
{
    for (int i = 0; i < size; i++) {
        char tmp[8];
        int k = 0;

        for (int j = i * 8; j < ((i + 1) * 8); j++) {
            tmp[k++] = data[j];
        }

        char tm = strtol(tmp, 0, 2);
        msg[i] = tm;
    }

    msg[size] = '\0';
    return msg;
}

/*
 * Prints help menu
 */
void print_help() {

	printf("-f,\tFile to be shared between sender/receiver\n"
		"-o,\tSelected offset into shared file\n"
		"-s,\tTime period on which sender and receiver sync on each bit\n"
	       	"-i,\tTime interval for sending a single bit within a sync period\n");

}
