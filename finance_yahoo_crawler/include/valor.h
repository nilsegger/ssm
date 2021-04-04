#include <stdint.h>

typedef struct csv_tracker {
	uint8_t is_header;
	long unsigned fields;
} csv_tracker_t;

int find_valors(const char* file_path);
