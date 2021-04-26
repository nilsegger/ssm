/**
 * Code for parsing .csv files of stock value and simultaniously inserting it into db.
 * It will be run in separate thread while main thread is waiting one sec in order to not overload finance.yahoo.com
 */
#include <stdbool.h>

#include <csv.h>
#include <stdlib.h>

#include "data.h"

#define CSV_BUFFER_SIZE 1024

#define FAILED_TO_PARSE_DATE 1 

typedef struct csv_tracker {
	bool is_header;
	uint8_t error;
	long unsigned fields;
	share_value_t* list;
	share_value_t* current;
} csv_tracker_t;

share_value_t* malloc_share_value(share_value_t** ptr) {
	(*ptr) = malloc(sizeof(share_value_t));
	(*ptr)->date_timestamp = 0;
	(*ptr)->close = 0;
	(*ptr)->high = 0;
	(*ptr)->low = 0;
	(*ptr)->volume = 0;
	(*ptr)->next = NULL;
	return *ptr;
}

void field_callback(void* s, size_t len, void* data) {
	csv_tracker_t * tracker = (csv_tracker_t*)data;
	if(tracker->is_header) {
	       	return; // Skip header row
	} else if(tracker->fields == 0) {
		if(tracker->list == NULL) {
			malloc_share_value(&tracker->list);
			tracker->current = tracker->list;
		} else if(tracker->current != NULL) {
			malloc_share_value(&(tracker->current->next));
			tracker->current = tracker->current->next;
		}
		struct tm time = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		if(strptime(s, "%Y-%m-%d", &time) != NULL) {
			tracker->current->date_timestamp = mktime(&time);
		} else {
			fprintf(stderr, "Failed to parse datetime.\n");
			tracker->error = FAILED_TO_PARSE_DATE;
		}
	} else if(tracker->fields == 2) {
		tracker->current->high = atof(s);	
	} else if(tracker->fields == 3) {
		tracker->current->low = atof(s);	
	} else if(tracker->fields == 4) {
		tracker->current->close = atof(s);	
	} else if(tracker->fields == 6) {
		tracker->current->volume = atoll(s);	
	}
	tracker->fields++;
}

void row_callback(int c, void* data) {
	csv_tracker_t * tracker = (csv_tracker_t*)data;
	tracker->fields = 0;
	tracker->is_header = false;
}

int parse_file(const char* file_path, share_value_t** root) {
	FILE* fp = NULL;
	struct csv_parser parser;
	char buffer[CSV_BUFFER_SIZE];
	size_t bytes_read;
	uint8_t r = DATA_OK;
	csv_tracker_t tracker = {true, 0, 0, NULL};
	if(csv_init(&parser, CSV_APPEND_NULL) != 0) {	
		return DATA_INIT_ERROR;
	} 
	csv_set_delim(&parser, ',');
	// TODO simply take buffer parameter of file contents instead of writing it to a file, or do both for redundancy
	fp = fopen(file_path, "rb");
	if(!fp) {
		r = DATA_FILE_NOT_FOUND;
	} else {
		while((bytes_read=fread(buffer, 1, CSV_BUFFER_SIZE, fp)) > 0) {
			if(csv_parse(&parser, buffer, bytes_read, field_callback, row_callback, &tracker) != bytes_read) {
				r = DATA_PARSING_ERROR;
				break;
			}
		}
		csv_fini(&parser, field_callback, row_callback, &tracker);	
		fclose(fp);
	}
	csv_free(&parser);
	if(r == DATA_OK) {
		*root = tracker.list;
	}
	return r;
}
