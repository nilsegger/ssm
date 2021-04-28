#include <unistd.h>
#include <stdio.h>

#include <sqlite3.h>

#include "db.h"

void print_sql_error(const char* operation, sqlite3* db, int rc) {
	fprintf(stderr, "Failed \"%s\" with code %i. SQL Msg: %s\n", operation, rc, sqlite3_errmsg(db));
}

int create_table(sqlite3* db, const char* sql_stmt) {
	sqlite3_stmt* stmt = NULL;
	int rc = sqlite3_prepare_v2(db, sql_stmt, -1, &stmt, NULL);
	if(!rc) {	
		rc = sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		if(rc == SQLITE_DONE) {
			rc = SQLITE_OK;
		}
	}
	if(rc) {
		print_sql_error("Create table", db, rc);
	}
	return rc;
}

int open_database(const char* db_file, sqlite3** db) {
	char* sql_err_msg = 0;
	int sql_rc = sqlite3_open_v2(db_file, db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOFOLLOW, NULL);
	if(sql_rc) {
		fprintf(stderr, "Unable to open \"%s\" database %s\n", db_file, sqlite3_errmsg(*db));
		return 1;
	}
	return 0;
}

int prepare_database(sqlite3* db) {
	int sql_rc = create_table(db, CREATE_SHARE_REFERENCES_TABLE);
	if(!sql_rc) sql_rc = create_table(db, CREATE_DAILY_SHARE_VALUE_TALBE);
	if(sql_rc) {
		sqlite3_close(db);
		return 1;
	}
	return 0;
}
