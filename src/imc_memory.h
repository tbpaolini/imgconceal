/* Wrapper around the standard memory management functions, in order to include error checking. */

#ifndef _IMC_MEMORY_H
#define _IMC_MEMORY_H

#include "imc_includes.h"

// Allocate 'mem_size' bytes of memory
void *imc_malloc(size_t mem_size);

// Allocate 'item_count' elements of 'item_size' bytes, all initialized to zero
void *imc_calloc(size_t item_count, size_t item_size);

// Re-allocate 'ptr' to the new size of 'mem_size' bytes.
void *imc_realloc(void *ptr, size_t mem_size);

// Free the memory allocated by 'imc_malloc()', 'imc_realloc()' or 'imc_calloc()'
void imc_free(void *ptr);

// Set a memory region to zero, then free it
void imc_clear_free(void *ptr, size_t mem_size);

#endif  //_IMC_MEMORY_H