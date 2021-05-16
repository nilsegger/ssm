#include <stdlib.h>
#include <stdio.h>

#include "macro.h"
#include "data.h"
#include "supervisor.h"

int find_best_result(sqlite3* db, const char* data_folder, const char* out_folder, const size_t average_future_n_days, const size_t cores, best_options_result_t* result) {

	stock_t* stocks;
	int64_t stocks_count = 0;

	if(prepare_stocks(db, data_folder, &stocks, &stocks_count)) {
		return EXIT_FAILURE;
	}

	result->average_n_results = 0;
	result->compare_n_days = 0;
	result->average_trend = -100.0;

	DEBUG("Starting to find best results.\n");
	size_t ignore_last_n_days = average_future_n_days;
	for(size_t average_n_results_iter = 3; average_n_results_iter < 10; average_n_results_iter++) {
		for(size_t compare_n_days_iter = 5; compare_n_days_iter < 30; compare_n_days_iter++) {
			stock_future_trend_result_t* results = NULL;
			if(find_most_promising_stocks(out_folder, average_n_results_iter, compare_n_days_iter, ignore_last_n_days, average_future_n_days, cores, stocks, stocks_count, &results)) {
				continue;
			}

			double average_real_trend = 0.0;
			size_t count = 0;

			for(stock_future_trend_result_t* iter = results; iter != NULL; iter = iter->next) {
				// TODO create params for these
				bool max_trend = iter->trend <= 5.0;
				bool min_volume = iter->stock->vals[iter->stock->vals_len - ignore_last_n_days - 1].volume > 1000;
				bool max_price = iter->stock->vals[iter->stock->vals_len - ignore_last_n_days - 1].closing < 1000.0;
				if(max_trend && min_volume && max_price) {
					double current_value = iter->stock->vals[iter->stock->vals_len - 1].closing;
					double last_value = iter->stock->vals[iter->stock->vals_len - ignore_last_n_days - compare_n_days_iter - 1].closing;
					double real_trend = 100.0 / last_value * current_value - 100.0;
					average_real_trend += real_trend;
					count++;
				}
				if(count == average_n_results_iter) break; 
			}
			if(count > 0) {
				average_real_trend /= (double)count;
				if(average_real_trend > result->average_trend) {
					result->average_trend = average_real_trend;
					result->average_n_results = average_n_results_iter;
					result->compare_n_days = compare_n_days_iter;
				}
			}

			free_stock_future_trend_results_list(results);
		}
	}
	free_stocks(stocks, stocks_count);

	return EXIT_SUCCESS;
}

// TOOD try out the best result method, recalculating the best result for each days
// void find_best_result_month_test
