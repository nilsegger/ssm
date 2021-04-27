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

