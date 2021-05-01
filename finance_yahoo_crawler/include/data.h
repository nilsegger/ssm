#include <time.h>
#include <sqlite3.h>

/**
 * Reads stocks table data and downloads stocks data from finance.yahoo
 *
 * @param db Database to read stock references from and save data to.
 * @param start Download only data from this point on
 * @param end Download all data till end.
 *
 * @returns 0 if successful.
 */
int download_stocks_daily_values(sqlite3* db, time_t start, time_t end);
