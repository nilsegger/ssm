/*
 * Running with valgrind. Make sure DCMAKE_BUILD_TYPE is set to debug. Works best if set in CMakeLists.txt.
 * cmake --build . -- -o0 && valgrind --leak-check=yes --track-origins=yes -s ./playground
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>

#include <sqlite3.h>

#include "download.h"
#include "db.h"
#include "csv_parser.h"
#include "references/six_stock_references.h"

int main(int argc, char** argv) {
	int c;

	bool prepare_db = false;
	const char db_file[256];
	memset((void*)db_file, 0, 256);

	const char six_reference_file[256];
	memset((void*)six_reference_file, 0, 256);

	memset((void*)db_file, 0, 256);
	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			{"prepare", no_argument, 0, 'p'},
			{"db", required_argument, 0,  'd'},
			{"six-reference",  required_argument, 0, 's'},
			/*{"delete",  required_argument, 0,  0 },
			{"verbose", no_argument,       0,  0 },
			{"create",  required_argument, 0, 'c'},
			{"file",    required_argument, 0,  0 },*/
			{0,         0,                 0,  0 }
		};

		c = getopt_long(argc, argv, "abc:d:012", long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			case 0:
				printf("option %s", long_options[option_index].name);
				if (optarg)
					printf(" with arg %s", optarg);
				printf("\n");
				break;
			case 'p':
				prepare_db = true;
				break;
			case 'd':
				strcpy((char*)db_file, optarg);
				break;
			case 's':
				strcpy((char*)six_reference_file, optarg);
				break;
			case '?':
				break;
			default:
				printf("?? getopt returned character code 0%o ??\n", c);
		}
	}

	if (optind < argc) {
		printf("non-option ARGV-elements: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		printf("\n");
		exit(EXIT_FAILURE);
	}

	sqlite3* db = NULL;
	if(db_file[0] == 0) {
		fprintf(stderr, "Required options missing. --db <path to database file>\n");
		exit(EXIT_FAILURE);
	} 
	if(open_database(db_file, &db)) {
		exit(EXIT_FAILURE);
	}
	if(prepare_db && prepare_database(db) == 1) {
		fprintf(stderr, "Failed to prepare database.\n");
		exit(EXIT_FAILURE);
	}

	if(six_reference_file[0] != 0 && six_stock_parse_reference(six_reference_file, db)) {
		fprintf(stderr, "Failed to parse six stock reference file.\n");
		exit(EXIT_FAILURE);
	}
	
	sqlite3_close(db);
	return 0;
}
