#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h> // usleep

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

void free_stock_daily_values_list(stock_daily_value_t* root) {
	while(root != NULL) {
		stock_daily_value_t* temp = root;
		root = root->next;
		free(temp);
	}
}

int count_stocks(sqlite3* db, int64_t* count) {
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

	*count = sqlite3_column_int64(count_stmt, 0);
	sqlite3_finalize(count_stmt);
	return EXIT_SUCCESS;
}

int download_stocks_daily_values(sqlite3* db, const char* folder, time_t start, time_t end) {

	int64_t stocks_count;	
	if(count_stocks(db, &stocks_count) == EXIT_FAILURE) {
		return EXIT_FAILURE;
	}

	printf("Counted %lu stocks.\n", stocks_count);
	const char select_stmt[] = "SELECT source, isin, valor FROM stocks;";	
	sqlite3_stmt* stmt = NULL;
	int ec = sqlite3_prepare_v2(db, select_stmt, -1, &stmt, NULL);
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
			const char file_name[256];
			memset((void*)file_name, 0, 256);
			const char template[] = "%s/%s.csv";
			sprintf((char*)file_name, template, folder, isin);

			FILE* fp = fopen(file_name, "w");
			if(fwrite(memory.memory, sizeof(char),  memory.size, fp) != memory.size) {
				fprintf(stderr, "Failed to write %ld to %s.\n", memory.size, file_name);
			}
			fclose(fp);
		}

		free(memory.memory);
		ec = sqlite3_step(stmt);
		printf("Finished \"%s\" %ld from %ld stocks.\n", isin, i, stocks_count);
		i++;
		usleep(300);
	}
	if(ec != SQLITE_DONE) {
		print_sql_error("reading stocks rows", db, ec);
		sqlite3_finalize(stmt);
		return EXIT_FAILURE;
	}
	sqlite3_finalize(stmt);
	return EXIT_SUCCESS;
}

int select_isins(sqlite3* db, const char** isin) {
	const char* isin_select = "SELECT isin FROM stocks;";
	sqlite3_stmt* stmt = NULL;
	int ec = sqlite3_prepare_v2(db, isin_select, -1, &stmt, NULL);
	if(ec) {
		print_sql_error("Select isins from stocks", db, ec);
		return EXIT_FAILURE;
	}

	int64_t i = 0;
	ec = sqlite3_step(stmt);
	while(ec == SQLITE_ROW) {
		const char* column_isin = (const char*)sqlite3_column_text(stmt, 0);
		strcpy((char*)isin[i], (char*)column_isin);
		i++;
		ec = sqlite3_step(stmt);
	}
	if(ec != SQLITE_DONE) {
		print_sql_error("reading isin from stocks", db, ec);
		sqlite3_finalize(stmt);
		return EXIT_FAILURE;
	}
	sqlite3_finalize(stmt);
	return EXIT_SUCCESS;
}

int find_most_promising_future_averages(sqlite3* db, uint32_t compare_last_n_days, uint32_t average_future_of_n_stocks, uint32_t average_future_n_days_of_stocks) {
	int64_t stocks_count;
	if(count_stocks(db, &stocks_count) == EXIT_FAILURE) {
		return EXIT_FAILURE;
	}
	const char** isin = calloc(stocks_count, sizeof(char*));
	for(int i = 0; i < stocks_count; i++) {
		isin[i] = malloc(sizeof(char) * 12 + 1); // isin has size of 12
	}
	if(select_isins(db, isin) == EXIT_FAILURE) {
		return EXIT_FAILURE;
	}

	for(int i = 0; i < stocks_count; i++) {
		const char* current_isin = isin[i];
	}

	for(int i = 0; i < stocks_count; i++) {
		free((void*)isin[i]);
	}
	free(isin);
	return EXIT_SUCCESS;
}
