#include <stdbool.h>
#include <stdlib.h>

/**
 * Struct which contains important options for parsing.
 *
 * @param delim Delimeter to use for spliting fields
 * @param fields_count Amount of fields to extract
 * @param fields_indices Indices of fields to extract
 * @param next Callback function which is called everytime, a row has been read.
 * @param custom Void pointer to whatever you whish
 */
typedef struct csv_easy_parse_args {
	const char delim;
	size_t fields_count;
	size_t* fields_indices;
	void (*next)(struct csv_easy_parse_args*);
	void* custom;
	bool is_header;
	size_t field_index;
	size_t next_field_index;
	char** field_data;
} csv_easy_parse_args_t;

/**
 * Reads from file and parses as csv.
 *
 * @param file_path Path to file to read from
 * @param args parse_args_t* Options for parsing. 
 *
 * @returns Returns 0 if successful.
 */
int csv_easy_parse_file(const char* file_path, csv_easy_parse_args_t* args);

/**
 * Reads from memory and parses as csv.
 *
 * @param memory Memory pointer
 * @param size Size of memory 
 * @param args parse_args_t* Options for parsing. 
 *
 * @returns Returns 0 if successful.
 */
int csv_easy_parse_memory(char* memory, size_t size, csv_easy_parse_args_t* args);

/**
 * Parses complete csv and counts rows.
 *
 * @param file_path File to read from.
 * @param delim Delimeter used by csv
 * @param rows Pointer to size_t to write to. Is set to 0 at beginning.
 *
 * @returns 0 on Success
 */
int csv_count_rows_file(const char* file_path, const char delim, size_t* rows);

/**
 * Parses complete csv and counts rows.
 *
 * @param memory Memory to read from.
 * @param size Memory size.
 * @param delim Delimeter used by csv
 * @param rows Pointer to size_t to write to. Is set to 0 at beginning.
 *
 * @returns 0 on Success
 */
int csv_count_rows_memory(char* memory, size_t size, const char delim, size_t* rows);
