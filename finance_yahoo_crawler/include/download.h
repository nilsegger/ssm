#include <stdlib.h>

#define DOWNLOAD_OK 0
#define DOWNLOAD_ERROR 1

typedef struct MemoryStruct {
  char *memory;
  size_t size;
} memory_struct_t;

/**
 * Downloads data from url and saves it to memory.
 *
 * @param url URL to download data from.
 * @param chunk memory_struct_t to write to
 *
 * @returns DOWNLOAD_OK on success
 */
int download_file(const char* url, memory_struct_t* chunk);

/**
 * Frees a memory struct.
 */
void free_memory_struct(memory_struct_t* chunk);
