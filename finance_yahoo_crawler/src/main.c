/*
 * Running with valgrind. Make sure DCMAKE_BUILD_TYPE is set to debug. Works best if set in CMakeLists.txt.
 * cmake --build . -- -o0 && valgrind --leak-check=yes --track-origins=yes -s ./playground
 */

#include <stdio.h>
#include <stdint.h>

#include "valor.h" 

int main(int argc, char** argv) {
	if(argc == 1) {
		fprintf(stderr, "Usage: ./yc <file>\n");
		return -1;
	}
	valor_symbol_t* valor_symbol = NULL;
	uint8_t ec = find_valors(argv[1], &valor_symbol);
	if(ec == VALOR_OK) {
		for(valor_symbol_t* iter = valor_symbol; iter != NULL; iter = iter->next) {
			printf("ValorSymbol: %s\n", iter->symbol);
		}
		free_valor_symbols(valor_symbol);
	} else {
		fprintf(stderr, "Failed finding valor symbols with code: %d\n", ec);	
	}

	return 0;
}
