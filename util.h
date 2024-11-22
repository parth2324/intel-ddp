#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


#ifndef UTIL_H_
#define UTIL_H_

#define ADDR_PTR uint64_t
#define CYCLES uint64_t

//Channel parameters
#define TX_INTERVAL_DEF           0x00001E00
#define SYNC_TIME_MASK_DEF        0x00001FFF
#define SYNC_JITTER_DEF              0x00100

//Shared memory
#define DEFAULT_FILE_NAME       "README.md"
#define DEFAULT_FILE_OFFSET	 21 * 64
#define DEFAULT_FILE_SIZE	  4096
#define CACHE_BLOCK_SIZE	    64
#define MAX_BUFFER_LEN	    1024

//Colors
#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */


//Functions to help Flush+Reload
void     clflush(ADDR_PTR addr);     //Flush Address
void     maccess(ADDR_PTR addr);     //Load Address
CYCLES   maccess_t(ADDR_PTR addr);   //Load And Time the Access
uint64_t rdtscp(void);               //Get Timestamp

//String Conversion
char *string_to_binary(char *s);
char *conv_char(char *data, int size, char *msg);

#endif
