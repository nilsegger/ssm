#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h> // usleep

#include "csv_parser.h"
#include "download.h"
#include "db.h"

#include "macro.h"
#include "references/six_stock_references.h"
#include "data.h"

typedef struct stock_value {
	time_t date;
	double closing, high, low;
	uint64_t volume;
} stock_value_t;

typedef struct stock {
	const char* isin;
	stock_value_t* vals;
	size_t vals_len;
	bool loaded;
} stock_t;

typedef struct load_stock_container {
	stock_t* stock;
	size_t len; // len will contain count of how many rows were succesfully read
} load_stock_container_t;

/**
 * Parses high, low, closing, volume and date from csv and creates linked list to args->custom.
 */
void load_stocks_values_callback(csv_easy_parse_args_t* args) {
	load_stock_container_t* container = args->custom;
	stock_t* stock = container->stock;
	stock_value_t* value = &stock->vals[container->len]; 

	struct tm time = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	if(strptime(args->field_data[0], "%Y-%m-%d", &time) != NULL) {
		value->date = mktime(&time);

		char* end_ptr;
		value->high = strtod(args->field_data[1], &end_ptr);
		value->low = strtod(args->field_data[2], &end_ptr);
		value->closing = strtod(args->field_data[3], &end_ptr);
		value->volume = strtol(args->field_data[4], &end_ptr, 10);

		if(value->high != 0.0 && value->low != 0.0 && value->closing != 0.0) {
			container->len++;
		} else {
			DEBUG("Failed to parse high, low or closing of %s: \"%s\" \"%s\" \"%s\" \"%s\"\n", stock->isin, args->field_data[1], args->field_data[2], args->field_data[3], args->field_data[4]);
		}
	} else {
		DEBUG("Failed to parse date for %s: \"%s\"\n", stock->isin, args->field_data[0]);
	}
}

int count_stocks(sqlite3* db, int64_t* count) {
	const char select_count_stmt[] = "SELECT COUNT(id) FROM stocks;";	
	sqlite3_stmt* count_stmt = NULL;
	int ec = sqlite3_prepare_v2(db, select_count_stmt, -1, &count_stmt, NULL);
	if(!ec) {
		ec = sqlite3_step(count_stmt);
		if(ec == SQLITE_ROW) {
			*count = sqlite3_column_int64(count_stmt, 0);
			sqlite3_finalize(count_stmt);
			return EXIT_SUCCESS; 
		} 
		sqlite3_finalize(count_stmt);
	}
	DEBUG("Failed to select count from stocks.\n");
	print_sql_error(db, ec);
	return EXIT_FAILURE;
}

/**
 * Writes file name of downloaded stock data to buffer.
 *
 * @param data_folder Folder in which the file should be saved to
 * @param stock_isin Isin of current stock.
 * @param buffer Buffer to write to.
 */
void get_stock_file_name(const char* data_folder, const char* stock_isin, const char* buffer) {
	const char template[] = "%s/%s.csv";
	sprintf((char*)buffer, template, data_folder, stock_isin);
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
		DEBUG("Failed to prepare sql stmt for selecting stocks.\n");
		print_sql_error(db, ec);
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
			fprintf(stderr, "Received %lu from %s.\n", memory.response_code, url);
		} else {
			const char file_name[256];
			get_stock_file_name(folder, isin, file_name);
			
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
		print_sql_error(db, ec);
		sqlite3_finalize(stmt);
		return EXIT_FAILURE;
	}
	sqlite3_finalize(stmt);
	return EXIT_SUCCESS;
}


/**
 * Loads isin from database and copies it to stock_t
 *
 * @param db Database to read from
 * @param stocks Stocks pointer to arr
 *
 * @returns 0 On success
 */
int load_stock_reference_data(sqlite3* db, stock_t* stocks) {
	const char* isin_select = "SELECT isin FROM stocks;";
	sqlite3_stmt* stmt = NULL;
	int ec = sqlite3_prepare_v2(db, isin_select, -1, &stmt, NULL);
	if(!ec) {
		int64_t i = 0;
		ec = sqlite3_step(stmt);
		while(ec == SQLITE_ROW) {
			const char* column_isin = (const char*)sqlite3_column_text(stmt, 0);
			stocks[i].isin = malloc(sizeof(char) * (12 + 1));
			strcpy((char*)stocks[i].isin, column_isin);
			i++;
			ec = sqlite3_step(stmt);
		}
		sqlite3_finalize(stmt);
		if(ec != SQLITE_DONE) {
			// free allocated memory, i has been keeping track of how many have been allocated
			for(int j = 0; j < i; j++) {
				free((void*)stocks[j].isin);
			}	
		}
	}
	if(ec && ec != SQLITE_DONE) {
		DEBUG("Failed to select from stocks.\n");
		print_sql_error(db, ec);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

/**
 * Loads all data from stocks from their representive csv files.
 *
 * @param stocks Pointer to stocks arr
 * @param stocks_len Count of stocks
 * @param data_folder location on drive where csv files are stored
 * @param compare_n_days Amount of days whilch will be compared
 * @param average_future_n_days Amoung of days which will get averaged 
 */
void load_stocks_values(stock_t* stocks, size_t stocks_len, const char* data_folder, size_t compare_n_days, size_t average_future_n_days) {
	for(size_t i = 0; i < stocks_len; i++) {
		char stock_file_name[256];
		get_stock_file_name(data_folder, stocks[i].isin, stock_file_name);		


		size_t row_count;
		if(csv_count_rows_file(stock_file_name, ',', &row_count)) {
			// no data has been allocated
			DEBUG("Failed to load %s.\n", stock_file_name);
			continue;
		}	

		if(row_count < compare_n_days + average_future_n_days + 1) {
			DEBUG("%s does not contain enough rows for comparing and averaging future.\n", stock_file_name);
			continue;
		}
		stocks[i].vals = malloc(sizeof(stock_value_t) * row_count);	

		load_stock_container_t container = {&stocks[i], 0};
		size_t data_fields_indices[] = {0, 2, 3, 4, 6};
		csv_easy_parse_args_t csv_args = {',', 5, data_fields_indices, load_stocks_values_callback, &container};
		if(csv_easy_parse_file(stock_file_name, &csv_args)) {
			DEBUG("Failed to parse %s.\n", stock_file_name);
			free(stocks[i].vals);
			continue;
		}
		stocks[i].loaded = true;
		stocks[i].vals_len = container.len;
	}
}

void find_similar_stock_trends(stock_t* current, stock_t* others, size_t others_len, size_t compare_n_days) {
	// check if currents last compare_n_days are not spaced out in a longer time period
	time_t week_in_s = 8 * 24 * 60 * 60; // using 8 for a little bit of space
	for(size_t i = current->vals_len - compare_n_days; i < current->vals_len; i++) {
		if(current->vals[i].date - week_in_s > current->vals[i - 1].date) {
			// Last compare_n_days of current are not consecutive
			DEBUG("Last %ld days of %s are not consecutive. %ld:%ld\n", compare_n_days, current->isin, current->vals[i].date, current->vals[i -1].date);
			return;
		}	
	}
	// start comparing
	for(size_t i = 0; i < others_len; i++) {
		stock_t* other = &others[i];
		if(other == current || !other->loaded) continue;
			
		for(size_t j = 1; j < other->vals_len; j++) {
			stock_value_t* current_val_prev = &current->vals[current->vals_len - 1 - compare_n_days + j];	
			stock_value_t* current_val = &current->vals[current->vals_len - compare_n_days + j];	

			stock_value_t* other_val_prev = &other->vals[j - 1];
			stock_value_t* other_val = &other->vals[j];
		}	
	}
}

/**
 * Prepares stocks data for comparison.
 */
int prepare_stocks(sqlite3* db, const char* data_folder, size_t compare_n_days, size_t average_future_n_days) {
	// Count stocks and fetch each isin
	int64_t stocks_count;
	if(count_stocks(db, &stocks_count)) {
		return EXIT_FAILURE;
	}

	stock_t* stocks = malloc(sizeof(stock_t) * stocks_count);
	for(size_t i = 0; i < stocks_count; i++) {
		stocks[i].loaded = false;
		stocks[i].vals = NULL;
		stocks[i].vals_len = 0;
		stocks[i].isin = NULL;
	}
	
	if(load_stock_reference_data(db, stocks)) {
		free(stocks);
		return EXIT_FAILURE;
	}
	
	load_stocks_values(stocks, stocks_count, data_folder, compare_n_days, average_future_n_days);

	DEBUG("Loaded all data. Beginning with comparisons.\n");

	for(size_t i = 0; i < stocks_count; i++) {
		if(stocks[i].loaded) {
			find_similar_stock_trends(&stocks[i], stocks, stocks_count, compare_n_days);
		}
	}

	for(size_t i = 0; i < stocks_count; i++) {
		free((void*)stocks[i].isin);
		if(stocks[i].loaded) {
			free(stocks[i].vals);
		}
	}

	free(stocks);
	return EXIT_SUCCESS;
}

