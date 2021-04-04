/*
 * Running with valgrind. Make sure DCMAKE_BUILD_TYPE is set to debug. Works best if set in CMakeLists.txt.
 * cmake --build . -- -o0 && valgrind --leak-check=yes --track-origins=yes -s ./playground
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <time.h>
#include <math.h>
#include <assert.h>
#include <string.h>

#include <csv.h>

#define FALSE 0
#define TRUE 1
#define CSV_BUFFER_SIZE 1024

typedef struct csv_tracker {
	uint8_t is_header;
	long unsigned fields;
} csv_tracker_t;

void find_valor_symbol_fc(void* s, size_t len, void* data) {
	csv_tracker_t * tracker = (csv_tracker_t*)data;
	if(tracker->is_header) {
	       	return; // Skip header row
	} else if(tracker->fields == 5) {
		char valor_symbol[12];	
		strcpy(valor_symbol, s);	
		printf("ValorSymbol: %s\n", valor_symbol);
	}
	tracker->fields++;
}

void row_track_rc(int c, void* data) {
	((csv_tracker_t*)data)->fields = 0;
	((csv_tracker_t*)data)->is_header = FALSE;
}

int main(int argc, char** argv) {
	if(argc == 1) {
		fprintf(stderr, "Usage: ./yc <file>\n");
		return -1;
	}

	FILE* fp = NULL;
	struct csv_parser parser;
	char buffer[CSV_BUFFER_SIZE];
	size_t bytes_read;
	csv_tracker_t tracker = {TRUE, 0};

	if(csv_init(&parser, CSV_APPEND_NULL) != 0) {	
		printf("Unable to init csv.\n");
		return -1;
	} else {
		printf("CSV initialized\n");
	}

	csv_set_delim(&parser, ';');

	fp = fopen(argv[1], "rb");
	if(!fp) {
		printf("Unable to open file %s.", argv[1]);
	} else {
		while((bytes_read=fread(buffer, 1, 1024, fp)) > 0) {
			if(csv_parse(&parser, buffer, bytes_read, find_valor_symbol_fc, row_track_rc, &tracker) != bytes_read) {
				fprintf(stderr, "Error while parsing file: %s\n", csv_strerror(csv_error(&parser)));
				break;
			}
		}
		csv_fini(&parser, find_valor_symbol_fc, row_track_rc, &tracker);	
		fclose(fp);
	}
	csv_free(&parser);
	return 0;
}
