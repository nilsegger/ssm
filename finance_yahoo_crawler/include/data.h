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

/**
 * Parses file of share daily values and creates linked list of share_value_t
 * @param file_path Path to file which contains daily share value
 * @param root Linked list will be created to this pointer
 *
 * @return Returns DATA_OK if successfull, anything else is an error.
 */
int parse_file(const char* file_path, share_value_t** root);
