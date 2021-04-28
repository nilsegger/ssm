#include <stdio.h>
#include <string.h>

#include "csv_parser.h"
#include "references/six_stock_references.h"

void six_stock_reference_next(csv_easy_parse_args_t* args) {
	const char* insert_stmt = "INSERT INTO share_references(id, short_name, isin, currency, isc) VALUES (NULL, ?, ?, ?, ?)";
	sqlite3_stmt* stmt = NULL;
	int ec = sqlite3_prepare_v2((sqlite3*)args->custom, insert_stmt, -1, &stmt, NULL);
	if(ec) {
		fprintf(stderr, "Failed to prepare insert reference for %s.\n", args->field_data[0]);
	} else {
		ec = sqlite3_bind_text(stmt, 1, args->field_data[0], strlen(args->field_data[0]) * sizeof(char), NULL);
		ec = sqlite3_bind_text(stmt, 2, args->field_data[1], strlen(args->field_data[1]) * sizeof(char), NULL);
		ec = sqlite3_bind_text(stmt, 3, args->field_data[2], strlen(args->field_data[2]) * sizeof(char), NULL);
		ec = sqlite3_bind_text(stmt, 4, args->field_data[3], strlen(args->field_data[3]) * sizeof(char), NULL);
		if(ec) {
			fprintf(stderr, "Failed to bind text for %s.\n", args->field_data[0]);
		} else {
			ec = sqlite3_step(stmt);
			if(ec != SQLITE_DONE) {
				fprintf(stderr, "Failed to insert reference of %s.\n", args->field_data[0]);
			}
		}
		sqlite3_finalize(stmt);
	}

}

int six_stock_parse_reference(const char* file, sqlite3* db) {
	size_t fields_indices[] = {0, 4, 8, 12};
	csv_easy_parse_args_t args = {';', 4, fields_indices, six_stock_reference_next, (void*)db};
	csv_easy_parse_file(file, &args);


	return 0;
}
