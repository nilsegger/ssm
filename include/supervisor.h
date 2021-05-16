#include <stdint.h>
#include <stddef.h>

#include <sqlite3.h>

typedef struct best_options_result {
	size_t average_n_results;
	size_t compare_n_days;
	double average_trend;
} best_options_result_t;
	       
int find_best_result(sqlite3* db, const char* data_folder, const char* out_folder, const size_t average_future_n_days, const size_t cores, best_options_result_t* result);

