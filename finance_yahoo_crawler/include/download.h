#include <stdlib.h>

#define DOWNLOAD_OK 0
#define DOWNLOAD_ERROR 1

typedef struct MemoryStruct {
  char *memory;
  size_t size;
} memory_struct_t;

int download_file(const char* url, memory_struct_t* chunk);
void free_memory_struct(memory_struct_t* chunk);
