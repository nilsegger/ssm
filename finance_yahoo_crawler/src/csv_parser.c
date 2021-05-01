#include <string.h>
#include <csv.h>
#include <unistd.h>

#include "csv_parser.h"

#define CSV_OK 0
#define CSV_FILE_NOT_FOUND 1

#define CSV_BUFFER_SIZE 1024

void csv_field_callback(void* s, size_t len, void* data) {
	csv_easy_parse_args_t* args = data;	
	if(!args->is_header && args->field_index == args->fields_indices[args->next_field_index]) {
		char** new_data_ptr = &(args->field_data[args->next_field_index]);
		*new_data_ptr = malloc(len + 1);
		strcpy(*new_data_ptr, s);
		args->next_field_index++;
	}
	args->field_index++;
}

void csv_row_callback(int c, void* data){
	csv_easy_parse_args_t* args = data;	

	if(!args->is_header) {
		args->next(args);
		for(size_t i = 0; i < args->next_field_index; i++) {
			if(args->field_data[i] == NULL) break;
			free(args->field_data[i]);
		}
	}

	args->field_index = 0;
	args->next_field_index = 0;
	args->is_header = false;
}

int parse_init(struct csv_parser* parser, csv_easy_parse_args_t* args) {
	args->field_index = 0;
	args->next_field_index = 0;
	args->is_header = true;
	int res = csv_init(parser, CSV_APPEND_NULL); 	
	if(!res) {
		csv_set_delim(parser, args->delim);
		args->field_data = malloc(args->fields_count * sizeof(char*));
	}
	return res;
}

void parse_fini(struct csv_parser* parser, csv_easy_parse_args_t* args) {
	csv_fini(parser, csv_field_callback, csv_row_callback, args);	
	csv_free(parser);
	free(args->field_data);
}

int parse(struct csv_parser* parser, csv_easy_parse_args_t* args, char* buffer, size_t size) {
	if(csv_parse(parser, buffer, size, csv_field_callback, csv_row_callback, args) != size) {
		return -1;
	}
	return 0;
}

int csv_easy_parse_file(const char* file_path, csv_easy_parse_args_t* args) {
	FILE* fp = fopen(file_path, "rb");;
	if(!fp) return CSV_FILE_NOT_FOUND;

	struct csv_parser parser;
	char buffer[CSV_BUFFER_SIZE];
	size_t bytes_read;
	int r = parse_init(&parser, args);
	if(r) return r;
	while((bytes_read=fread(buffer, 1, CSV_BUFFER_SIZE, fp)) > 0) {
		r = parse(&parser, args, buffer, bytes_read);
	}
	parse_fini(&parser, args);
	fclose(fp);
	return r;
}

int csv_easy_parse_memory(char* memory, size_t size, csv_easy_parse_args_t* args) {
	struct csv_parser parser;
	int r = parse_init(&parser, args);
	if(r) return r;
	r = parse(&parser, args, memory, size);
	parse_fini(&parser, args);
	return r;

}
