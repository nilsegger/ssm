#include <sqlite3.h>

/**
 * Parses given file and inserts name, isin, isc, and currency into database.
 *
 * @param file File to read from.
 * @param db Database to write to.
 *
 * @returns 0 on success.
 */ 
int six_stock_parse_reference(const char* file, sqlite3* db);
