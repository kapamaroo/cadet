#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

#include "analysis.h"

#define TOL 1e-5

static inline int __eq(const double a, const double b) {
    return fabs(a - b) <= TOL;
}

static inline void print_placement(struct placement_info *p) {
    assert(p);
    printf("%s %s %lf %lf\t#libcell size (x:%lf y:%lf)\n",
           p->cell->name, p->name,p->x,p->y,p->cell->size_x,p->cell->size_y);
}

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
            assert(__eq(p->x,0.0) || __eq(p->x,a->chip.width) ||
                   __eq(p->y,0.0) || __eq(p->y,a->chip.height));
            break;
        case I_OUTPUT:
            //assure that output is on the edges
            assert(__eq(p->x,0.0) || __eq(p->x,a->chip.width) ||
                   __eq(p->y,0.0) || __eq(p->y,a->chip.height));
            break;
        case I_CELL: {
            if (!(p->x + p->cell->size_x < a->chip.width ||
                  __eq(p->x + p->cell->size_x,a->chip.width))) {
                print_placement(p);
                printf("chip size (width:%lf height:%lf)\n",a->chip.width, a->chip.height);
                printf("placement on x puts cell out of chip - exit\n");
                exit(EXIT_FAILURE);
            }

            if (!(p->y + p->cell->size_y < a->chip.height ||
                  __eq(p->y + p->cell->size_y,a->chip.height))) {
                print_placement(p);
                printf("chip size (width:%lf height:%lf)\n",a->chip.width, a->chip.height);
                printf("placement on y puts cell out of chip - exit\n");
                exit(EXIT_FAILURE);
            }

            break;
        }
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

void put_placement(struct placement_info *p, const double wire_size,
                   struct analysis_info *soc) {
    assert(p);
    assert(p->type == I_CELL);
    assert(soc);
    assert(soc->grid);

    //reminder: bottom left point placement
    unsigned long cell_x = floor(p->cell->size_x / wire_size);
    unsigned long start_x = floor(p->x / wire_size);
    unsigned long end_x = start_x + cell_x;
    if (end_x > soc->grid_width) {
        print_placement(p);
        printf("end_x     : %lu\ngrid_width: %lu\nexit\n",end_x,soc->grid_width);
        exit(EXIT_FAILURE);
    }

    unsigned long cell_y = floor(p->cell->size_y / wire_size);
    unsigned long start_y = floor(p->y / wire_size);
    unsigned long end_y = start_y + cell_y;
    if (end_y < cell_y) {
        print_placement(p);
        printf("end_y : %lu\ncell_y: %lu\nexit\n",end_y,cell_y);
        exit(EXIT_FAILURE);
    }

    unsigned long i;
    unsigned long j;
    for (i=start_x; i<end_x; ++i)
        for (j=start_y; j<end_y; ++j)
            soc->grid[i*soc->grid_width + j] = G_BLOCKED;
}

void analyse(struct analysis_info *soc, const double wire_size) {
    check_analysis(soc);
    assert(wire_size != 0.0);

    double cell_grid_x = floor(soc->chip.width / wire_size);
    double cell_grid_y = floor(soc->chip.height / wire_size);

    soc->wire_size = wire_size;
    soc->grid_width = cell_grid_x;
    soc->grid_height = cell_grid_y;

    soc->grid = (grid_element *)_calloc(soc->grid_width * soc->grid_height,
                                        sizeof(grid_element));

    //set placement on grid

    unsigned long i;
    for (i=0; i<soc->placement.next; ++i) {
        struct placement_info *placement =
            (struct placement_info *)(soc->placement.data) + i;
        if (placement->type == I_CELL)
            put_placement(placement,wire_size,soc);
    }
}
