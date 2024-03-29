/*
 * Running with valgrind. Make sure DCMAKE_BUILD_TYPE is set to debug. Works best if set in CMakeLists.txt.
 * cmake --build . -- -o0 && valgrind --leak-check=yes --track-origins=yes -s ./playground
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>

#include <sqlite3.h>

// https://stackoverflow.com/questions/15334558/compiler-gets-warnings-when-using-strptime-function-c
#define __USE_XOPEN
#define _GNU_SOURCE
#include <time.h>

#include "download.h"
#include "db.h"
#include "csv_parser.h"
#include "references/six_stock_references.h"
#include "data.h"
#include "supervisor.h"

int parse_date_optarg(const char* input, time_t *buffer) {
	if(strcmp(input, "today") == 0) {
		*buffer = (time_t)time(NULL);
		return EXIT_SUCCESS;
	} else if(strcmp(input, "first") == 0) {
		*buffer = 0;
		return EXIT_SUCCESS;
	} else {
		struct tm time = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		if(strptime(input, "%d-%m-%Y", &time) != NULL) {
			*buffer  = mktime(&time);
			return EXIT_SUCCESS;
		} else {
			return EXIT_FAILURE;
		}
	}
}


int main(int argc, char** argv) {
	int c;

	bool prepare_db = false;
	const char db_file[256];
	memset((void*)db_file, 0, 256);

	const char six_reference_file[256];
	memset((void*)six_reference_file, 0, 256);

	bool download_stocks = false;
	time_t date_start = -1;
	time_t date_end = -1;

	bool should_find_most_promising_future_averages = false;
	size_t average_n_results = 0;
	size_t compare_n_days = 0;
	size_t average_future_n_days = 0;
	size_t ignore_last_n_days = 0;
	size_t threads = 1;

	const char data_folder[256];
	memset((void*)data_folder, 0, 256);

	const char out_folder[256];
	memset((void*)out_folder, 0, 256);

	bool find_best_options = false;

	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			{"prepare", no_argument, 0, 0},
			{"db-file", required_argument, 0,  0},
			{"six-reference",  required_argument, 0, 0},
			{"download-stocks",  no_argument, 0, 0},
			{"date-start",  required_argument, 0, 0},
			{"date-end",  required_argument, 0, 0},
			{"data-folder",  required_argument, 0, 0},
			{"out-folder",  required_argument, 0, 0},
			{"find-prom",  no_argument, 0, 0},
			{"find-options",  no_argument, 0, 0},
			{"average-n-results",  required_argument, 0, 0},
			{"compare-n-days",  required_argument, 0, 0},
			{"ignore-last-n-days",  required_argument, 0, 0},
			{"average-future-n-days",  required_argument, 0, 0},
			{"threads",  required_argument, 0, 0},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "abc:d:012", long_options, &option_index);

		if (c == -1)
			break;
		const char* option = long_options[option_index].name;
		if(strcmp(option, "prepare") == 0) {
			prepare_db = true;
		} else if(strcmp(option, "db-file") == 0) {
			strcpy((char*)db_file, optarg);
		} else if(strcmp(option, "six-reference") == 0) {
			strcpy((char*)six_reference_file, optarg);
		} else if(strcmp(option, "download-stocks") == 0) {
			download_stocks = true;
		} else if(strcmp(option, "date-start") == 0) {
			if(parse_date_optarg(optarg, &date_start) == EXIT_FAILURE) {
				fprintf(stderr, "Failed to parse date of date-start.\n");
				exit(EXIT_FAILURE);
			}
		} else if(strcmp(option, "date-end") == 0) {
			if(parse_date_optarg(optarg, &date_end) == EXIT_FAILURE) {
				fprintf(stderr, "Failed to parse date of date-end.\n");
				exit(EXIT_FAILURE);
			}
		} else if(strcmp(option, "data-folder") == 0) {
			strcpy((char*)data_folder, optarg);
		
		} else if(strcmp(option, "out-folder") == 0) {
			strcpy((char*)out_folder, optarg);
		} else if(strcmp(option, "find-prom") == 0) {
			should_find_most_promising_future_averages = true;
		} else if(strcmp(option, "find-options") == 0) {
			find_best_options = true;
		} else if(strcmp(option, "average-n-results") == 0) {
			char* end;
			average_n_results = strtoll(optarg, &end, 10);
		} else if(strcmp(option, "compare-n-days") == 0) {
			char* end;
			compare_n_days = strtoll(optarg, &end, 10);
		} else if(strcmp(option, "ignore-last-n-days") == 0) {
			char* end;
			ignore_last_n_days = strtoll(optarg, &end, 10);
		}else if(strcmp(option, "average-future-n-days") == 0) {
			char* end;
			average_future_n_days = strtoll(optarg, &end, 10);
		} else if(strcmp(option, "threads") == 0) {
			char* end;
			threads = strtoll(optarg, &end, 10);
		}

		switch (c) {
			case 0:
				printf("option %s", long_options[option_index].name);
				if (optarg)
					printf(" with arg %s", optarg);
				printf("\n");
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

	if(download_stocks) {
		if(date_start == -1 || date_end == -1) {
			fprintf(stderr, "Please set --date-start and --date-end to use download_stocks.\n");
			exit(EXIT_FAILURE);
		}

		if(data_folder[0] == 0) {
			fprintf(stderr, "Please set --data-folder to use download_stocks.\n");
			exit(EXIT_FAILURE);
		}
		
		if(download_stocks_daily_values(db, data_folder, date_start, date_end)) {
			fprintf(stderr, "Failed to download stocks daily value.\n");
			exit(EXIT_FAILURE);
		}		
	}

	if(should_find_most_promising_future_averages) {
		stock_t* stocks;
		int64_t stocks_count = 0;
		stock_future_trend_result_t* stock_most_prom_results;
		if(*data_folder == 0 || *out_folder == 0) {
			fprintf(stderr, "Missing arguments --date-folder and --out-folder.\n");
			exit(EXIT_FAILURE);
		} else if(average_n_results == 0 || compare_n_days == 0 || average_future_n_days == 0) {
			fprintf(stderr, "Missing either --average-n-results --compare-n-days or --average-future-n-days.\n");
			exit(EXIT_FAILURE);
		}
		else if(prepare_stocks(db, data_folder, &stocks, &stocks_count)
			       	|| find_most_promising_stocks(out_folder, average_n_results, compare_n_days, ignore_last_n_days, average_future_n_days, threads, stocks, stocks_count, &stock_most_prom_results) == EXIT_FAILURE) {
			exit(EXIT_FAILURE);
		} else {
			save_find_most_promising_result(stock_most_prom_results, out_folder, compare_n_days, ignore_last_n_days);
			free_stock_future_trend_results_list(stock_most_prom_results);
			free_stocks(stocks, stocks_count);
		}
	}

	if(find_best_options) {
		best_options_result_t result;
		if(*data_folder == 0 || *out_folder == 0) {
			fprintf(stderr, "Missing arguments --date-folder and --out-folder.\n");
			exit(EXIT_FAILURE);
		} else if(average_future_n_days == 0) {
			fprintf(stderr, "Missing --average-future-n-days.\n");
			exit(EXIT_FAILURE);
		}
		else if(find_best_result(db, data_folder, out_folder, average_future_n_days, threads, &result)) {
			exit(EXIT_FAILURE);			
		} else {
			printf("Best result using average-n-results %ld with compare-n-days %ld. Average trend of %f.\n", result.average_n_results, result.compare_n_days, result.average_trend);
		}
	}
	
	sqlite3_close(db);
	return 0;
}
