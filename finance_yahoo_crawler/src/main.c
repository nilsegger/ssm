/*
 * Running with valgrind. Make sure DCMAKE_BUILD_TYPE is set to debug. Works best if set in CMakeLists.txt.
 * cmake --build . -- -o0 && valgrind --leak-check=yes --track-origins=yes -s ./playground
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include <sqlite3.h>

#include "valor.h" 
#include "download.h"
#include "data.h"
#include "db.h"

int prepare_database(const char* db_file, sqlite3** db) {
	char* sql_err_msg = 0;
	int sql_rc = sqlite3_open_v2(db_file, db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOFOLLOW, NULL);
	if(sql_rc) {
		fprintf(stderr, "Unable to open \"%s\" database %s\n", db_file, sqlite3_errmsg(*db));
		return EXIT_FAILURE;
	} 

	sql_rc = create_table(*db, CREATE_SHARE_REFERENCES_TABLE);
	if(!sql_rc) sql_rc = create_table(*db, CREATE_DAILY_SHARE_VALUE_TALBE);
	if(sql_rc) {
		sqlite3_close(*db);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

int main(int argc, char** argv) {

	if(argc != 4) {
		fprintf(stderr, "Usage: ./yc [REFERENCE_FILE] [SAVE_FOLDER] [DB_FILE]\n");
		return EXIT_FAILURE;
	}

	sqlite3* db = NULL;
	if(prepare_database(argv[3], &db) == EXIT_FAILURE) return EXIT_FAILURE;	

	valor_symbol_t* valor_symbol = NULL;
	uint8_t ec = find_valors(argv[1], &valor_symbol);
	if(ec == VALOR_OK) {
		for(valor_symbol_t* iter = valor_symbol; iter != NULL; iter = iter->next) {
			char url[2048];
			const char template[] = "https://query1.finance.yahoo.com/v7/finance/download/%s.SW?period1=%lu&period2=%lu&interval=1d&events=history&includeAdjustedClose=true";
			sprintf(url, template, iter->symbol, (uint64_t)iter->first_day, (uint64_t)time(NULL));

			memory_struct_t chunk;
			pthread_t share_parse_thread_id;
			if(download_file(url, &chunk) != DOWNLOAD_OK) {
				printf("Failed to download %s\n\t%s\n", iter->symbol, url);
				continue;
			} 
			char file[255];
			const char file_template[] = "%s/%s.SW.csv";
			sprintf(file, file_template, argv[2], iter->symbol);
			FILE* fp = fopen(file, "w");
			fwrite(chunk.memory, sizeof(char), chunk.size, fp);
			fclose(fp);

			parse_share_file_args_t share_args = {chunk.memory, chunk.size, NULL};
			pthread_create(&share_parse_thread_id, NULL, parse_share_file, &share_args);
				
			sleep(1);
			pthread_join(share_parse_thread_id, NULL);
			if(share_args.result == DATA_OK) {
				for(share_value_t* iter = share_args.root; iter != NULL; iter = iter->next) {
					printf("%lu;%f;%f;%f;%lu\n", iter->date_timestamp, iter->close, iter->high, iter->low, iter->volume);
				}
				free_share_value_list(share_args.root);
			} else {
				fprintf(stderr, "Failed to parse data of %s. Code: %i\n.", file, share_args.result);
			}
			free(chunk.memory);
			break;
		}
		free_valor_symbols(valor_symbol);
	} else {
		fprintf(stderr, "Failed finding valor symbols with code: %d\n", ec);	
	}

	
	sqlite3_close(db);
	return 0;
}
