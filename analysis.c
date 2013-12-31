#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

#include "analysis.h"

//debug function
static void check_analysis(struct analysis_info *a) {
    assert(a);
    unsigned long i;

    for (i=0; i<a->placement.next; ++i) {
        //bottom left placement, check if there is enough space
        struct placement_info *p = (struct placement_info*)(a->placement.data) + i;
        switch (p->type) {
        case I_INPUT:
            //assure that input is on the edges
            assert(p->x == 0 || p->x == a->chip.width);
            assert(p->y == 0 || p->y == a->chip.height);
            break;
        case I_OUTPUT:
            //assure that output is on the edges
            assert(p->x == 0 || p->x == a->chip.width);
            assert(p->y == 0 || p->y == a->chip.height);
            break;
        case I_CELL:
            assert(p->x + p->cell->size_x <= a->chip.width);
            assert(p->y + p->cell->size_y <= a->chip.width);
            break;
        }
    }
}

static inline void *_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static inline void *_calloc(size_t nmemb, size_t size) {
    void *ptr = calloc(nmemb,size);
    if (!ptr) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void analyse(struct analysis_info *analysis) {
    check_analysis(analysis);
    assert(analysis->wire_size != 0.0);

    unsigned long dim_x = ceil(analysis->chip.width / analysis->wire_size);
    unsigned long dim_y = ceil(analysis->chip.height / analysis->wire_size);

    grid_element *grid = (grid_element *)_calloc(dim_x*dim_y, sizeof(grid_element));

    //set placement on grid
#warning implement me!
}
