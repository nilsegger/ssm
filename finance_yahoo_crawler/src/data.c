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

typedef struct data_container {
	sqlite3* db;
	const char* isin;
} data_container_t;

void stock_daily_data_download_callback(csv_easy_parse_args_t* args) {
	data_container_t* container = args->custom;
	const char* insert_stmt = "INSERT INTO daily_stock_values(id, isin, date, high_price, low_price, closing_price, volume) VALUES (NULL, ?, ?, ?, ?, ?, ?);";
	sqlite3_stmt* stmt = NULL;
	int ec = sqlite3_prepare_v2(container->db, insert_stmt, -1, &stmt, NULL);
	if(ec) {
		fprintf(stderr, "Failed to prepare insert daily stock value for %s. ", container->isin);
		print_sql_error("Insert", container->db, ec);
	} else {
		ec = sqlite3_bind_text(stmt, 1, container->isin, strlen(container->isin) * sizeof(char), NULL);

		time_t date = 0;
		struct tm time = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		if(strptime(args->field_data[0], "%Y-%m-%d", &time) != NULL) {
			date = mktime(&time);
		} else {
			fprintf(stderr, "Failed to parse date of row in %s.\n", container->isin);
			sqlite3_finalize(stmt);
			return;
		}
		ec = sqlite3_bind_int64(stmt, 2, date);
		char* end_ptr;
		ec = sqlite3_bind_double(stmt, 3, strtod(args->field_data[1], &end_ptr));
		ec = sqlite3_bind_double(stmt, 4, strtod(args->field_data[2], &end_ptr));
		ec = sqlite3_bind_double(stmt, 5, strtod(args->field_data[3], &end_ptr));
		ec = sqlite3_bind_int64(stmt, 6, strtol(args->field_data[4], &end_ptr, 10));

		if(ec) {
			fprintf(stderr, "Failed to bind data for %s. ", container->isin);
			print_sql_error("Insert", container->db, ec);
		} 
		if(!ec) {
			ec = sqlite3_step(stmt);
			if(ec != SQLITE_DONE) {
				fprintf(stderr, "Failed to insert daily stock value of %s. ", container->isin);
				print_sql_error("Insert", container->db, ec);
			}
		}
		sqlite3_finalize(stmt);
	}
}

int download_stocks_daily_values(sqlite3* db, time_t start, time_t end) {
	const char select_stmt[] = "SELECT source, isin, valor FROM stocks;";	

	sqlite3_stmt* stmt = NULL;
	int ec = sqlite3_prepare_v2(db, select_stmt, -1, &stmt, NULL);
	if(ec) {
		print_sql_error("Select stocks", db, ec);
		return EXIT_FAILURE;
	}

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
			data_container_t container = {db, isin};
			size_t field_indices[] = {0, 2, 3, 4, 6};
			csv_easy_parse_args_t csv_args = {',', 5, field_indices, stock_daily_data_download_callback, (void*)&container};
			if(csv_easy_parse_memory(memory.memory, memory.size, &csv_args)) {
				fprintf(stderr, "Failed to parse from memory for file from %s.\n", url);
			}
		}

		free(memory.memory);
		ec = sqlite3_step(stmt);
		ec = SQLITE_DONE;
		sleep(1);
	}
	if(ec != SQLITE_DONE) {
		print_sql_error("reading stocks rows", db, ec);
		sqlite3_finalize(stmt);
		return EXIT_FAILURE;
	}
	sqlite3_finalize(stmt);
	return EXIT_SUCCESS;
}
