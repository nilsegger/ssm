#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h> // usleep

#include "macro.h"
#include "csv_parser.h"
#include "download.h"
#include "db.h"

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

typedef struct stock_comparison_result {
	stock_t* other;
	size_t other_start_index;
	double average_difference;
	struct stock_comparison_result* next;
} stock_comparison_result_t;

typedef struct stock_future_trend_result {
	stock_t* stock;
	double trend;
	struct stock_future_trend_result* next;
} stock_future_trend_result_t;

typedef struct stock_thread_args {
	stock_t* current;
	stock_t* others;
	size_t others_len, average_n_results, compare_n_days, average_future_n_days;
       	stock_future_trend_result_t* result;
	const char* out_folder;
	bool done;
	int return_code;
} stock_thread_args_t;

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
			//DEBUG("Failed to parse high, low or closing of %s: \"%s\" \"%s\" \"%s\" \"%s\"\n", stock->isin, args->field_data[1], args->field_data[2], args->field_data[3], args->field_data[4]);
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

/**
 * Iterates over average_n_results and averages their future trends together.
 *
 */
int stock_average_results_future_trend(stock_comparison_result_t* results, size_t average_n_results, size_t compare_n_days, size_t average_future_n_days, double* result) {
	double average = 0.0;
	for(size_t i = 1; i < average_future_n_days; i++) {
		double daily_average = 0.0;

		stock_comparison_result_t* iter = results;
		for(size_t j = 0; j < average_n_results; j++) {

			stock_value_t* prev_value = &iter->other->vals[iter->other_start_index + compare_n_days + i - 1];
			stock_value_t* value = &iter->other->vals[iter->other_start_index + compare_n_days + i];		

			daily_average += (100.0 / prev_value->closing * value->closing) - 100.0;

			if(iter->next == NULL) {
				DEBUG("There are not enough results.");
				return EXIT_FAILURE;
			}
			iter = iter->next;
		}	
		daily_average /= average_n_results;
		average += daily_average;
	}
	*result = average;
	return EXIT_SUCCESS;
}


double calculate_average_difference(double prev, double current, double other_prev, double other) {
	return (100.0 / prev * current - 100.0) - (100.0 / other_prev * other - 100.0);
}

void insert_comparison_result_into_sorted_list(stock_comparison_result_t** results, double average_difference, stock_t* other, size_t other_start_index, size_t average_n_results) {
	// insert new stock_comparison_result_t into sorted linked list, smallest avg at beg
	stock_comparison_result_t** results_ptr = NULL;
	stock_comparison_result_t* next = NULL;
	if(*results == NULL) {
		results_ptr = results;
	} else {
		if((*results)->average_difference > average_difference) {
			next = *results;
			results_ptr = results;
		} else {
			size_t count = 1;
			for(stock_comparison_result_t* iter = *results; iter != NULL; iter = iter->next) {
				if(count == average_n_results) {
					break;
				} else if(iter->next == NULL) {
					results_ptr = &iter->next;
					break;
				} else if(iter->next->average_difference > average_difference) {
					results_ptr = &iter->next;
					next = iter->next;
					break;
				}
				count++;
			}
		}
	}
	
	if(results_ptr != NULL) {
		*results_ptr = malloc(sizeof(stock_comparison_result_t));
		(*results_ptr)->average_difference = average_difference;
		(*results_ptr)->other = other;
		(*results_ptr)->other_start_index = other_start_index;
		(*results_ptr)->next = next;
	}
}


void save_stock_average_results(stock_t* current, stock_comparison_result_t* results, size_t compare_n_days, const char* out_folder) {
	const char file_path[256];
	get_stock_file_name(out_folder, current->isin, file_path);

	FILE* fp = fopen(file_path, "a");
	if(fp) {
		fprintf(fp, "ISIN,StartDate,EndDate,OtherISIN,OtherStartDate,OtherEndDate,AverageDifference\n");
		size_t count = 0;
		for(stock_comparison_result_t* iter = results; iter != NULL; iter = iter->next) {
			char start_date[11];
			strftime(start_date, 11, "%d-%m-%Y", localtime(&current->vals[current->vals_len - compare_n_days - 1].date));
			char end_date[11];
			strftime(end_date, 11, "%d-%m-%Y", localtime(&current->vals[current->vals_len - 1].date));
			char other_start_date[11];
			strftime(other_start_date, 11, "%d-%m-%Y", localtime(&iter->other->vals[iter->other_start_index].date));
			char other_end_date[11];
			strftime(other_end_date, 11, "%d-%m-%Y", localtime(&iter->other->vals[iter->other_start_index + compare_n_days].date));
			fprintf(fp, "%s,%s,%s,%s,%s,%s,%f\n", current->isin, start_date, end_date, iter->other->isin, other_start_date, other_end_date, iter->average_difference);

			// TODO remove hardcoded 25
			if(count == 25) {
				break;
			}
			count++;
		}
		fclose(fp);
	} else{
		DEBUG("Failed to open file %s for writing.\n", file_path);
	}
}

void* find_similar_stock_trends(void* v) {
	stock_thread_args_t* args = v;
	// check if currents last compare_n_days are not spaced out in a longer time period
	time_t week_in_s = 8 * 24 * 60 * 60; // using 8 for a little bit of space
	for(size_t i = args->current->vals_len - args->compare_n_days; i < args->current->vals_len; i++) {
		if(args->current->vals[i].date - week_in_s > args->current->vals[i - 1].date) {
			// Last compare_n_days of current are not consecutive
			DEBUG("Last %ld days of %s are not consecutive. %ld:%ld\n", args->compare_n_days, args->current->isin, args->current->vals[i].date, args->current->vals[i -1].date);
			args->return_code = EXIT_FAILURE;
			args->done = true;
			return NULL;
		}	
	}

	stock_comparison_result_t* results = NULL;
	// start comparing
	for(size_t i = 0; i < args->others_len; i++) {
		stock_t* other = &((args->others)[i]);
		if(other == args->current || !other->loaded) continue;

		DEBUG("%ld/%ld: Finding similar trend between %s and other %s.\n", i, args->others_len, args->current->isin, other->isin);
			
		for(size_t j = 1; j < other->vals_len - args->compare_n_days - args->average_future_n_days; j++) {
			double avg = 0.0;
			bool has_consecutive_days = true;
			for(size_t k = 0; k < args->compare_n_days; k++) {
				stock_value_t* current_val_prev = &args->current->vals[args->current->vals_len - 1 - args->compare_n_days + k];	
				stock_value_t* current_val = &args->current->vals[args->current->vals_len - args->compare_n_days + k];

				stock_value_t* other_val_prev = &other->vals[j + k - 1];
				stock_value_t* other_val = &other->vals[j + k];

				if(other_val->date - week_in_s > other_val_prev->date) {
					// days are not consecutive, and hence shouldnt really be checked
					// setting j to k skips the non consecutive days
					// DEBUG("Found non consecutive days in %s. %lu:%lu\n", other->isin, other_val->date, other_val_prev->date); 
					j = j + k;
					has_consecutive_days = false;
					break;
				}

				//DEBUG("Current len %ld index %ld\n", current->vals_len, current->vals_len - compare_n_days + k);
				//DEBUG("Other len %ld index %ld\n", other->vals_len, j + k);

				double avg_close = calculate_average_difference(current_val_prev->closing, current_val->closing, other_val_prev->closing, other_val->closing);
				double avg_high = calculate_average_difference(current_val_prev->high, current_val->high, other_val_prev->high, other_val->high);
				double avg_low = calculate_average_difference(current_val_prev->low, current_val->low, other_val_prev->low, other_val->low);

				avg += (avg_close * avg_close + avg_high * avg_high + avg_low * avg_low) / 3.0;
			}

			if(has_consecutive_days) {
				avg = avg / (double)args->compare_n_days;
				insert_comparison_result_into_sorted_list(&results, avg, other, j - 1, args->average_n_results);
			}
		}
	}

	double future_avg_trend;
	int ec = stock_average_results_future_trend(results, args->average_n_results, args->compare_n_days, args->average_future_n_days, &future_avg_trend);

	if(!ec) {
		save_stock_average_results(args->current, results, args->compare_n_days, args->out_folder);
	}

	// Free results in both cases, success and error
	for(stock_comparison_result_t* iter = results; iter != NULL;) {
		stock_comparison_result_t* temp = iter;
		iter = iter->next;
		free(temp);
	}

	if(ec) {
		args->return_code = EXIT_FAILURE;
		args->done = true;
		return NULL;
	}
	
	args->result = malloc(sizeof(stock_future_trend_result_t));
	(args->result)->next = NULL;
	(args->result)->stock = args->current;
	(args->result)->trend = future_avg_trend;

	args->done = true;
	args->return_code = EXIT_SUCCESS;

	return NULL;
}

void insert_future_trend_results_sorted(stock_future_trend_result_t** results, stock_future_trend_result_t* future_trend) {
	if(*results == NULL) {
		*results = future_trend;
	} else if((*results)->trend < future_trend->trend) {
		stock_future_trend_result_t* temp = *results;
		*results = future_trend;
		future_trend->next = temp;
	} else {
		for(stock_future_trend_result_t* iter = *results; iter != NULL; iter = iter->next) {
			if(iter->next == NULL) {
				iter->next = future_trend;
				break;
			} else if(iter->next->trend < future_trend->trend) {
				future_trend->next = iter->next;
				iter->next = future_trend;
				break;
			}
		}
	}
}

void finish_thread(pthread_t* t, stock_thread_args_t* args, stock_future_trend_result_t** results, size_t* finished, size_t stocks_count) {
	pthread_join(*t, NULL);	
	if(!args->return_code) {
		insert_future_trend_results_sorted(results, args->result);
		DEBUG("Finished comparing %s. %ld/%ld\n", args->current->isin, *finished, stocks_count);
	} else {
		DEBUG("Failed to find similar stock trends for %s.\n", args->current->isin);
	}
	(*finished)++;
	args->current = NULL;
	args->result = NULL;
	args->done = false;
	args->return_code = 0;
}

/**
 * Prepares stocks data for comparison.
 */
int prepare_stocks(sqlite3* db, const char* data_folder, const char* out_folder, size_t average_n_results, size_t compare_n_days, size_t average_future_n_days, size_t cores) {
	// Count stocks and fetch each isin
	int64_t stocks_count;
	if(count_stocks(db, &stocks_count)) {
		return EXIT_FAILURE;
	}

	// initialize stocks
	stock_t* stocks = malloc(sizeof(stock_t) * stocks_count);
	for(size_t i = 0; i < stocks_count; i++) {
		stocks[i].loaded = false;
		stocks[i].vals = NULL;
		stocks[i].vals_len = 0;
		stocks[i].isin = NULL;
	}
	
	// load reference data for stocks
	if(load_stock_reference_data(db, stocks)) {
		free(stocks);
		return EXIT_FAILURE;
	}
	
	// load data of stocks
	load_stocks_values(stocks, stocks_count, data_folder, compare_n_days, average_future_n_days);

	DEBUG("Loaded all data. Beginning with comparisons.\n");

	stock_future_trend_result_t* results = NULL;

	pthread_t threads[cores];
	stock_thread_args_t args[cores];
	for(int i = 0; i < cores; i++) {
		args[i].current = NULL;
		args[i].result = NULL;
		args[i].others = stocks;
		args[i].others_len = stocks_count;
		args[i].average_n_results = average_n_results;
		args[i].compare_n_days = compare_n_days;
		args[i].average_future_n_days = average_future_n_days;
		args[i].out_folder = out_folder;
		args[i].done = false;
		args[i].return_code = 0;
	}

	size_t stocks_finished = 0;
	for(size_t i = 0; i < stocks_count; i++) {

		if(!stocks[i].loaded) {
			stocks_finished++;
		       	continue;
		}

		bool found_free_thread = false;
		while(!found_free_thread) {
			// find free thread
			for(int c = 0; c < cores && !found_free_thread; c++) {
				bool free_core = args[c].current == NULL;
				if(!free_core && !args[c].done) {
					continue;	
				} else if(!free_core && args[c].done) {
					finish_thread(&threads[c], &args[c], &results, &stocks_finished, stocks_count);
					args[c].current = &stocks[i];
					pthread_create(&threads[c], NULL, find_similar_stock_trends, (void*)&args[c]);
					found_free_thread = true;
				} else {
					// first time use of core
					args[c].current = &stocks[i];
					pthread_create(&threads[c], NULL, find_similar_stock_trends, (void*)&args[c]);
					found_free_thread = true;
				}
			}	
			if(!found_free_thread) sleep(1);
		}
	}

	for(int i = 0; i < cores; i++) {
		if(args[i].current != NULL && !args[i].done) {
			finish_thread(&threads[i], &args[i], &results, &stocks_finished, stocks_count);
		}
	}

	const char result_file_path[256];
	get_stock_file_name(out_folder, "result", result_file_path);
	FILE* fp = fopen(result_file_path, "a");
	if(fp) {
		fprintf(fp, "ISIN,volume,StartDate,EndDate,Trend\n");
		for(stock_future_trend_result_t* iter = results; iter != NULL; iter = iter->next) {
			char start_date[11];
			strftime(start_date, 11, "%d-%m-%Y", localtime(&iter->stock->vals[iter->stock->vals_len - compare_n_days - 1].date));
			char end_date[11];
			strftime(end_date, 11, "%d-%m-%Y", localtime(&iter->stock->vals[iter->stock->vals_len - 1].date));
			// char trend_date[11];
			// strftime(trend_date, 11, "%d-%m-%Y", localtime(&iter->stock->vals[iter->stock->vals_len - 1].date + (average_future_n_days * 24 * 60 * 60)));
			fprintf(fp, "%s,%ld,%s,%s,%f\n", iter->stock->isin, iter->stock->vals[iter->stock->vals_len - 1].volume, start_date, end_date, iter->trend);
		}
		fclose(fp);
	} else {
		DEBUG("Failed to open results file for writing %s.\n", result_file_path);
	}

	for(stock_future_trend_result_t* iter = results; iter != NULL; ) {
		stock_future_trend_result_t* temp = iter;
		iter = iter->next;
		free(temp);	
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

