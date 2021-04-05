#include <stdint.h>
#include <string.h>
#include <csv.h>

#include "valor.h"

#define FALSE 0
#define TRUE 1
#define CSV_BUFFER_SIZE 1024

typedef struct csv_tracker {
	uint8_t is_header;
	long unsigned fields;
	valor_symbol_t* list;
	valor_symbol_t* current;
} csv_tracker_t;

void malloc_valor_symbol(valor_symbol_t** owner, void* s) {
	*owner = (valor_symbol_t*)malloc(sizeof(valor_symbol_t));
	(*owner)->next = NULL;	
	(*owner)->symbol = (char*)malloc(strlen(s) + 1);
	strcpy((*owner)->symbol, s);	
}

void find_valor_symbol_fc(void* s, size_t len, void* data) {
	csv_tracker_t * tracker = (csv_tracker_t*)data;
	if(tracker->is_header) {
	       	return; // Skip header row
	} else if(tracker->fields == 5) {
		if(tracker->list == NULL) {
			malloc_valor_symbol(&tracker->list, s);
			tracker->current = tracker->list;
		} else if(tracker->current != NULL) {
			malloc_valor_symbol(&(tracker->current->next), s);
			tracker->current = tracker->current->next;
		}
	}
	tracker->fields++;
}

void row_track_rc(int c, void* data) {
	((csv_tracker_t*)data)->fields = 0;
	((csv_tracker_t*)data)->is_header = FALSE;
}

int find_valors(const char* file_path, valor_symbol_t** valor_symbol) {
	uint8_t r = VALOR_OK;
	FILE* fp = NULL;
	struct csv_parser parser;
	char buffer[CSV_BUFFER_SIZE];
	size_t bytes_read;
	csv_tracker_t tracker = {TRUE, 0, NULL};
	if(csv_init(&parser, CSV_APPEND_NULL) != 0) {	
		return VALOR_INIT_ERROR;
	} 
	csv_set_delim(&parser, ';');
	fp = fopen(file_path, "rb");
	if(!fp) {
		r = VALOR_FILE_NOT_FOUND;
	} else {
		while((bytes_read=fread(buffer, 1, CSV_BUFFER_SIZE, fp)) > 0) {
			if(csv_parse(&parser, buffer, bytes_read, find_valor_symbol_fc, row_track_rc, &tracker) != bytes_read) {
				r = VALOR_PARSING_ERROR;
				break;
			}
		}
		csv_fini(&parser, find_valor_symbol_fc, row_track_rc, &tracker);	
		fclose(fp);
	}
	csv_free(&parser);
	if(r == VALOR_OK) {
		*valor_symbol = tracker.list;
	} else if(*valor_symbol != NULL) {
		free_valor_symbols(*valor_symbol);
	}
	return r;
}

void free_valor_symbols(valor_symbol_t* valor_symbol) {
	valor_symbol_t* current = valor_symbol;
	while(current != NULL) {
		valor_symbol_t* temp = current->next;
		free(current->symbol);
		free(current);
		current = temp;
	}
}
