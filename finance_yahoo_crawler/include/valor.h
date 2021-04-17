#include <stdint.h>

#define VALOR_OK 0
#define VALOR_INIT_ERROR 1
#define VALOR_FILE_NOT_FOUND 2
#define VALOR_PARSING_ERROR 3

typedef struct valor_symbol {
	char* symbol;
	uint64_t first_day;
	struct valor_symbol* next;
} valor_symbol_t;

int find_valors(const char* file_path, valor_symbol_t** valor_symbol);
void free_valor_symbols(valor_symbol_t* valor_symbol);
