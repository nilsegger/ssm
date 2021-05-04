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
	time_t date;
	double high, low, closing;
	int64_t volume;
	struct stock_daily_value* next;
	struct stock_daily_value* prev;
} stock_daily_value_t;

/**
 * Parses high, low, closing, volume and date from csv and creates linked list to args->custom.
 */
void load_stock_daily_values_callback(csv_easy_parse_args_t* args) {
	stock_daily_value_t* value = malloc(sizeof(stock_daily_value_t));
	value->next = NULL;
	value->prev = NULL;

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

	if(args->custom == NULL) {
		args->custom = value;
	} else {
		value->prev = args->custom;
		((stock_daily_value_t*)args->custom)->next = value;
		args->custom = value;
	}
}

void free_stock_daily_values_list(stock_daily_value_t* root) {
	while(root != NULL) {
		stock_daily_value_t* temp = root;
		root = root->next;
		free(temp);
	}
}

void free_stock_daily_values_list_reverse(stock_daily_value_t* end) {
	while(end != NULL) {
		stock_daily_value_t* temp = end;
		end = end->prev;
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

int load_stock_data(const char* data_folder, const char* isin, stock_daily_value_t** root) {
	const char file_name[256];
	get_stock_file_name(data_folder, isin, file_name);
	size_t field_indices[] = {0, 2, 3, 4, 6};
	csv_easy_parse_args_t csv_args = {',', 5, field_indices, load_stock_daily_values_callback, NULL};
	if(csv_easy_parse_file(file_name, &csv_args)) {
		fprintf(stderr, "Failed to load stock data.");
		return EXIT_FAILURE;
	}
	*root = (stock_daily_value_t*)csv_args.custom;
	return EXIT_SUCCESS;
}

/**
 * Finds pointer of daily stock value which equals last_ptr - compare_last_n_days position.
 */
int find_stock_starting_ptr(uint32_t compare_last_n_days, stock_daily_value_t* end, stock_daily_value_t** start) {
	uint64_t compare_last_n_seconds = compare_last_n_days * 24 * 60 * 60;
	stock_daily_value_t* iter = end->prev;
	while(iter != NULL) {
		if(iter->date < end->date - compare_last_n_seconds) {
			*start = iter;
			return EXIT_SUCCESS;
		}
		iter = iter->prev;
	}
	return EXIT_FAILURE;
}

double calculate_average_difference(stock_daily_value_t* start, stock_daily_value_t* other_start, stock_daily_value_t* other_end) {
	/**
	 * Problem with this function is that if there are not the same number of data entries between two dates, the function will return lower avg than they should be...
	 */
	uint32_t count = 0;
	double avg_close_diff = 0;
	double avg_high_diff = 0;
	double avg_low_diff = 0;

	start = start->next;
	other_start = other_start->next;

	while(other_start != other_end && start != NULL) {
		double close_diff = (start->prev->closing / 100.0 * start->closing) - (other_start->prev->closing / 100.0 * other_start->closing);	
		double high_diff = (start->prev->high / 100.0 * start->high) - (other_start->prev->high / 100.0 * other_start->high);	
		double low_diff = (start->prev->low / 100.0 * start->low) - (other_start->prev->low / 100.0 * other_start->low);	

		// get rid of - or +
		close_diff *= close_diff;
		high_diff *= high_diff;
		low_diff *= low_diff;

		avg_close_diff += close_diff; 
		high_diff += high_diff;
		low_diff += low_diff;
		count++;
		other_start = other_start->next;
		start = start->next;
	}

	avg_close_diff /= count;
	avg_high_diff /= count;
	avg_low_diff /= count;

	double avg = (avg_close_diff * 0.5 + avg_high_diff * 0.25 + avg_low_diff * 0.25) / 3.0;

	return avg;
}

typedef struct stock_average {
	stock_daily_value_t* start;
	stock_daily_value_t* other_start;
	stock_daily_value_t* other_end;
	double avg;
	struct stock_average* next;
} stock_average_t;

int find_closest_averages(stock_daily_value_t* start, stock_daily_value_t* other_end, uint32_t compare_last_n_days, uint32_t average_future_of_n_stocks, uint32_t average_future_n_days_of_stocks, stock_average_t** root) {
	uint64_t compare_last_n_seconds = compare_last_n_days * 24 * 60 * 60;
	stock_daily_value_t* iter_start = other_end;
	while(iter_start->prev != NULL) iter_start = iter_start->prev;
	stock_daily_value_t* iter_end; // iter_start + compare_last_n_days
	stock_daily_value_t* iter_stop; // Ptr at which there are no longer enough future days
	if(find_stock_starting_ptr(average_future_n_days_of_stocks, other_end, &iter_stop)) {
		fprintf(stderr, "Unable to find stop pointer.\n");
		return EXIT_FAILURE;
	}
	while(iter_end != NULL) {
		if(iter_end->date > iter_start->date + compare_last_n_seconds) {
			break;
		}
		iter_end = iter_end->next;
	}
	if(iter_end == NULL) {
		fprintf(stderr, "There are not enough daily values for other.\n");
		return EXIT_FAILURE;
	}
	
	while(iter_end != iter_stop) {
		double avg = calculate_average_difference(start, iter_start, iter_end);
		iter_start = iter_start->next;
		iter_end = iter_end->next;

		// TODO create linked list of values	
		
	}	

	return EXIT_SUCCESS;
}

int find_most_promising_future_averages(sqlite3* db, const char* data_folder, uint32_t compare_last_n_days, uint32_t average_future_of_n_stocks, uint32_t average_future_n_days_of_stocks) {
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

	// Start comparing
	for(int i = 0; i < stocks_count; i++) {
		stock_daily_value_t* root_data_end;
		
		if(load_stock_data(data_folder, isin[i], &root_data_end)) {
			// Failed to read stock data, simply continue onto next one.
			continue;
		}

		stock_daily_value_t* start_root_data; // ptr to root_data_end - compare_last_n_days
		if(find_stock_starting_ptr(compare_last_n_days, root_data_end, &start_root_data)) {
			fprintf(stderr, "Failed to find starting ptr to %s.\n", isin[i]);
			free_stock_daily_values_list_reverse(root_data_end);
			continue;
		}

		for(int j = 0; j < stocks_count; j++) {
			if(i == j) continue; // Do not compare same stocks, obv
			stock_daily_value_t* sub_data_end;
			if(load_stock_data(data_folder, isin[j], &sub_data_end)) {
				// Failed to read stock data, simply continue onto next one.
				continue;
			}
		}
	}

	for(int i = 0; i < stocks_count; i++) {
		free((void*)isin[i]);
	}
	free(isin);
	return EXIT_SUCCESS;
}
