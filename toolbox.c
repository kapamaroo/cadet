#include <stdio.h>
#include <stdlib.h>

void *_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *_calloc(size_t nmemb, size_t size) {
    void *ptr = calloc(nmemb,size);
    if (!ptr) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    return ptr;
}
