#include <stdint.h>

// https://stackoverflow.com/questions/15334558/compiler-gets-warnings-when-using-strptime-function-c
#define __USE_XOPEN
#define _GNU_SOURCE
#include <time.h>

#define DATA_OK 0
#define DATA_INIT_ERROR 1
#define DATA_FILE_NOT_FOUND 2
#define DATA_PARSING_ERROR 3

typedef struct share_value {
	time_t date_timestamp;	
	float close;
	float high;
	float low;
	uint64_t volume;
	struct share_value* next;
} share_value_t;

typedef struct parse_share_file_args {
	char* buffer;
	size_t len;
	share_value_t* root;
	uint8_t result;
} parse_share_file_args_t;
/**
 * Parses file of share daily values and creates linked list of share_value_t
 * @param buffer In memory buffer of share file 
 * @param len Lenght of memory buffer
 * @param root Linked list will be created to this pointer
 *
 * @return Returns DATA_OK if successfull, anything else is an error.
 */
void* parse_share_file(void* args);

void free_share_value_list(share_value_t* root);
