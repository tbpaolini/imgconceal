/* Wrapper around the standard memory management functions, in order to include error checking. */

#include "imc_includes.h"

// Exit with an error if memory could not be allocated
static void __exit_no_mem()
{
    fprintf(stderr, "Error: No enough memory\n");
    abort();
}

// Allocate 'mem_size' bytes of memory
void *imc_malloc(size_t mem_size)
{
    void *ptr = malloc(mem_size);
    if (ptr == NULL) __exit_no_mem();
    return ptr;
}

// Allocate 'item_count' elements of 'item_size' bytes, all initialized to zero
void *imc_calloc(size_t item_count, size_t item_size)
{
    void *ptr = calloc(item_count, item_size);
    if (ptr == NULL) __exit_no_mem();
    return ptr;
}

// Re-allocate 'ptr' to the new size of 'mem_size' bytes.
void *imc_realloc(void *ptr, size_t mem_size)
{
    void *new_ptr = realloc(ptr, mem_size);
    if (new_ptr == NULL) __exit_no_mem();
    return new_ptr;
}

// Free the memory allocated by 'imc_malloc()', 'imc_realloc()' or 'imc_calloc()'
void imc_free(void *ptr)
{
    free(ptr);
}

// Set a memory region to zero, then free it
void imc_clear_free(void *ptr, size_t mem_size)
{
    sodium_memzero(ptr, mem_size);
    imc_free(ptr);
}