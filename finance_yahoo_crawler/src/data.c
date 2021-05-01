#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// https://stackoverflow.com/questions/15334558/compiler-gets-warnings-when-using-strptime-function-c
#define __USE_XOPEN
#define _GNU_SOURCE
#include <time.h>

#include "csv_parser.h"
#include "download.h"
#include "db.h"

#include "references/six_stock_references.h"
#include "data.h"

typedef struct stock_daily_value {
	const char* isin;
	time_t date;
	double high, low, closing;
	int64_t volume;
	struct stock_daily_value* next;
} stock_daily_value_t;

typedef struct data_container {
	sqlite3* db;
	const char* isin;
	stock_daily_value_t* root;
	stock_daily_value_t** next;
} data_container_t;


void stock_daily_data_download_callback(csv_easy_parse_args_t* args) {
	data_container_t* container = args->custom;
	stock_daily_value_t* value = malloc(sizeof(stock_daily_value_t));
	value->next = NULL;
	value->isin = container->isin;

	struct tm time = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	if(strptime(args->field_data[0], "%Y-%m-%d", &time) != NULL) {
		value->date = mktime(&time);
	} else {
		fprintf(stderr, "Failed to parse date \"%s\".\n", args->field_data[0]);
		value->date = -1;
	}

	char* end_ptr;
	value->high = strtod(args->field_data[1], &end_ptr);
	value->low = strtod(args->field_data[2], &end_ptr);
	value->closing = strtod(args->field_data[3], &end_ptr);
	value->volume = strtol(args->field_data[4], &end_ptr, 10);

	if(container->root == NULL) {
		container->root = value;
		container->next = &value->next;
	} else {
		*container->next = value;
		container->next = &value->next;
	}
	
}

int insert_daily_stock_values(sqlite3* db, stock_daily_value_t* root) {
	const char* insert_stmt = "INSERT INTO daily_stock_values(id, isin, date, high_price, low_price, closing_price, volume) VALUES (NULL, ?, ?, ?, ?, ?, ?);";
	sqlite3_stmt* stmt = NULL;
	int ec = sqlite3_prepare_v2(db, insert_stmt, -1, &stmt, NULL);
	if(ec) {
		fprintf(stderr, "Failed to prepare insert stock daily value. ");
		print_sql_error("Insert", db, ec);
		return EXIT_FAILURE;
	} else {
		while(root != NULL) {
			ec = sqlite3_bind_text(stmt, 1, root->isin, strlen(root->isin) * sizeof(char), NULL);
			ec = sqlite3_bind_int64(stmt, 2, root->date);
			ec = sqlite3_bind_double(stmt, 3, root->high);
			ec = sqlite3_bind_double(stmt, 4, root->low);
			ec = sqlite3_bind_double(stmt, 5, root->closing);
			ec = sqlite3_bind_int64(stmt, 6, root->volume);

			if(ec) {
				fprintf(stderr, "Failed to bind data for %s. ", root->isin);
				print_sql_error("Insert", db, ec);
			} 
			if(!ec) {
				ec = sqlite3_step(stmt);
				if(ec != SQLITE_DONE) {
					fprintf(stderr, "Failed to insert daily stock value of %s. ", root->isin);
					print_sql_error("Insert", db, ec);
				}
			}
			ec = sqlite3_reset(stmt);
			if(ec) {
				fprintf(stderr, "Failed to reset stmt for %s.\n", root->isin);
				print_sql_error("Reset", db, ec);
			}
			sqlite3_clear_bindings(stmt);
			if(ec) {
				fprintf(stderr, "Failed to clear bindings for %s.\n", root->isin);
				print_sql_error("Reset", db, ec);
			}
			root = root->next;

		}
		sqlite3_finalize(stmt);
	}
	return EXIT_SUCCESS;
}

void free_stock_daily_values_list(stock_daily_value_t* root) {
	while(root != NULL) {
		stock_daily_value_t* temp = root;
		root = root->next;
		free(temp);
	}
}

int download_stocks_daily_values(sqlite3* db, time_t start, time_t end) {

	const char select_count_stmt[] = "SELECT COUNT(id) FROM stocks;";	
	sqlite3_stmt* count_stmt = NULL;
	int ec = sqlite3_prepare_v2(db, select_count_stmt, -1, &count_stmt, NULL);
	if(ec) {
		print_sql_error("Count stocks", db, ec);
		return EXIT_FAILURE;
	}
	ec = sqlite3_step(count_stmt);
	if(ec != SQLITE_ROW) {
		print_sql_error("Count stocks step", db, ec);
		sqlite3_finalize(count_stmt);
		return EXIT_FAILURE;
	}

	int64_t stocks_count = 	sqlite3_column_int64(count_stmt, 0);
	sqlite3_finalize(count_stmt);

	printf("Counted %lu stocks.\n", stocks_count);

	const char select_stmt[] = "SELECT source, isin, valor FROM stocks;";	
	sqlite3_stmt* stmt = NULL;
	ec = sqlite3_prepare_v2(db, select_stmt, -1, &stmt, NULL);
	if(ec) {
		print_sql_error("Select stocks", db, ec);
		return EXIT_FAILURE;
	}

	int64_t i = 1;

	ec = sqlite3_step(stmt);
	while(ec == SQLITE_ROW) {
		const char* source = (const char*)sqlite3_column_text(stmt, 0);	
		const char* isin = (const char*)sqlite3_column_text(stmt, 1);	
		const char* valor = (const char*)sqlite3_column_text(stmt, 2);	
		
		char url[2048];
		if(strcmp(source, "six") == 0) {
			get_finance_yahoo_six_stock_url(valor, url, start, end);
		}
		memory_struct_t memory; 
		if(download_file(url, &memory)) {
			fprintf(stderr, "Failed to download file from %s.\n", url);
			ec = sqlite3_step(stmt);
			continue;
		} else if(memory.response_code != 200) {
			fprintf(stderr, "Received %lu from %s.", memory.response_code, url);
		} else {
			data_container_t container = {db, isin, NULL, NULL};
			size_t field_indices[] = {0, 2, 3, 4, 6};
			csv_easy_parse_args_t csv_args = {',', 5, field_indices, stock_daily_data_download_callback, (void*)&container};
			if(csv_easy_parse_memory(memory.memory, memory.size, &csv_args)) {
				fprintf(stderr, "Failed to parse from memory for file from %s.\n", url);
			} else {
				if(insert_daily_stock_values(db, container.root)) {
					fprintf(stderr, "Failed to insert daily stock values.\n");
				}
				free_stock_daily_values_list(container.root);
			}
		}

		free(memory.memory);
		ec = sqlite3_step(stmt);
		printf("Finished \"%s\" %ld from %ld stocks.\n", isin, i, stocks_count);
		/*free((void*)source);
		free((void*)isin);
		free((void*)valor);*/
		i++;
	}
	if(ec != SQLITE_DONE) {
		print_sql_error("reading stocks rows", db, ec);
		sqlite3_finalize(stmt);
		return EXIT_FAILURE;
	}
	sqlite3_finalize(stmt);
	return EXIT_SUCCESS;
}
