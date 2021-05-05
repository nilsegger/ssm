#include <stdint.h>

// https://stackoverflow.com/questions/15334558/compiler-gets-warnings-when-using-strptime-function-c
#define __USE_XOPEN
#define _GNU_SOURCE
#include <time.h>

#include <sqlite3.h>

/**
 * Reads stocks table data and downloads stocks data from finance.yahoo into folder
 *
 * @param db Database to read stock references from.
 * @param folder Folder to save files to
 * @param start Download only data from this point on
 * @param end Download all data till end.
 *
 * @returns 0 if successful.
 */
int download_stocks_daily_values(sqlite3* db, const char* folder, time_t start, time_t end);

int find_most_promising_future_averages(sqlite3* db, const char* data_folder, uint32_t compare_last_n_days, uint32_t average_future_of_n_stocks, uint32_t average_future_n_days_of_stocks);
