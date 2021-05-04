#include <sqlite3.h>

// https://stackoverflow.com/questions/15334558/compiler-gets-warnings-when-using-strptime-function-c
#define __USE_XOPEN
#define _GNU_SOURCE
#include <time.h>

/**
 * Parses given file and inserts name, isin, isc, and currency into database.
 *
 * @param file File to read from.
 * @param db Database to write to.
 *
 * @returns 0 on success.
 */ 
int six_stock_parse_reference(const char* file, sqlite3* db);

/**
 * Creates url from valor, which can be used to download stock data from.
 *
 * @param valor_symbol Stock to which a download url should be created.
 * @param buffer Pointer to buffer to write to. Should be size 2048. (Max URL Size)
 * @param start Date start, can be 0 if from first day of stock is wanted
 * @param end Date end
 *
 */
void get_finance_yahoo_six_stock_url(const char* valor_symbol, char* buffer, time_t start, time_t end);
