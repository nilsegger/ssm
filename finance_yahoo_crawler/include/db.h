#include <sqlite3.h>

#define CREATE_SHARE_REFERENCES_TABLE "\
CREATE TABLE IF NOT EXISTS share_references (\
	id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,\
	short_name TEXT NOT NULL,\
	isin TEXT UNIQUE NOT NULL,\
	isc INTEGER NOT NULL,\
	currency TEXT\
);"

#define CREATE_DAILY_SHARE_VALUE_TALBE "\
CREATE TABLE IF NOT EXISTS daily_share_values (\
	id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,\
	isin TEXT NOT NULL,\
	date INTEGER NOT NULL,\
	closing_price REAL NOT NULL,\
	low_price REAL NOT NULL,\
	high_price REAL NOT NULL,\
	FOREIGN KEY(isin) REFERENCES share_references(isin)\
);"

/**
 * Prints sql error to stderr.
 *
 * @param operation Short descriptive name of operation which was unsucessful.
 * @param db Reference to sqlite3 database.
 * @param rc resulting code of unsuccesful operation.
 */
void print_sql_error(const char* operation, sqlite3* db, int rc);

/*
 * Creates table or returns unsuccessful error code.
 *
 * @param db Reference to database file in which the new table should be created.
 * @param sql_stmt actual SQL statement of CREATE TABLE.
 *
 * @returns Resulting error code of sql operations.
 */

int create_table(sqlite3* db, const char* sql_stmt);







