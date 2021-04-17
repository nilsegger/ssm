/*
 * Running with valgrind. Make sure DCMAKE_BUILD_TYPE is set to debug. Works best if set in CMakeLists.txt.
 * cmake --build . -- -o0 && valgrind --leak-check=yes --track-origins=yes -s ./playground
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "valor.h" 
#include "download.h"

int main(int argc, char** argv) {

	if(argc == 2) {
		fprintf(stderr, "Usage: ./yc [REFERENCE_FILE] [SAVE_FOLDER]\n");
		return -1;
	}

	valor_symbol_t* valor_symbol = NULL;
	uint8_t ec = find_valors(argv[1], &valor_symbol);
	if(ec == VALOR_OK) {
		for(valor_symbol_t* iter = valor_symbol; iter != NULL; iter = iter->next) {
			char url[2048];
			const char template[] = "https://query1.finance.yahoo.com/v7/finance/download/%s.SW?period1=%lu&period2=%lu&interval=1d&events=history&includeAdjustedClose=true";
			sprintf(url, template, iter->symbol, (uint64_t)iter->first_day, (uint64_t)time(NULL));

			memory_struct_t chunk;
			if(download_file(url, &chunk) != DOWNLOAD_OK) {
				printf("Failed to download %s\n\t%s\n", iter->symbol, url);
			} else {
				char file[255];
				const char file_template[] = "%s/%s.SW.csv";
				sprintf(file, file_template, argv[2], iter->symbol);
				FILE* fp = fopen(file, "w");
				fwrite(chunk.memory, sizeof(char), chunk.size, fp);
				fclose(fp);
			}
			free(chunk.memory);
		}
		free_valor_symbols(valor_symbol);
	} else {
		fprintf(stderr, "Failed finding valor symbols with code: %d\n", ec);	
	}

	
	return 0;
}
