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

int prepare_stocks(sqlite3* db, const char* data_folder, size_t average_n_results, size_t compare_n_days, size_t average_future_n_days);
