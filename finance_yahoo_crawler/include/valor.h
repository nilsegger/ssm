typedef struct valor_symbol {
	char* symbol;
	struct valor_symbol* next;
} valor_symbol_t;

int find_valors(const char* file_path, valor_symbol_t** valor_symbol);
void free_valor_symbols(valor_symbol_t* valor_symbol);
