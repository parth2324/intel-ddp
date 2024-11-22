#include "util.h"

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

extern inline __attribute__((always_inline))
void mfence()
{
  // Use mfence instruction.
  asm volatile ("mfence"
		: /*output*/
		: /*input*/
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
