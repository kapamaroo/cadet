#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include "pool.h"

unsigned long grow(struct pool_info *pool, void (*rebuild)(void)) {
    //returns the new size

    assert(pool->element_size);

    if (pool->size == ULONG_MAX) {
        printf("Out of memory - exit.\n");
        exit(EXIT_FAILURE);
    }

    assert(pool->next <= pool->size);

    //no need to grow
    if (pool->next < pool->size)
        return pool->size;

    //if we use more than half bits of unsigned long, increase size
    //with a constant, else duplicate it
    unsigned long bits = sizeof(unsigned long) * CHAR_BIT;
    unsigned long inc = 1 << 8;

    if (pool->size == 0)
        pool->size = inc;
    else if (ULONG_MAX - pool->size < inc)
        pool->size = ULONG_MAX;
    else if (pool->size >> (bits/2))
        pool->size += inc;
    else
        pool->size <<= 1;

    //NOTE: pool->data may be NULL
    void *new_data = (void *)realloc(pool->data,pool->size * pool->element_size);
    if (!new_data) {
        printf("Out of memory: realloc failed - exit.\n");
        exit(EXIT_FAILURE);
    }

    void *old_data = pool->data;
    pool->data = new_data;
    if (old_data != new_data && rebuild)
        rebuild();

    return pool->size;
}
