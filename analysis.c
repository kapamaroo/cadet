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
           p->cell->name, p->name,p->dim.fsize.x,p->dim.fsize.y,
           p->cell->dim.fsize.x,p->cell->dim.fsize.y);
}

static inline void print_grid(grid_element *grid,
                              const unsigned long width,
                              const unsigned long height) {
    unsigned long i;
    unsigned long j;
    for (i=0; i<width; ++i) {
        for (j=0; j<height; ++j) {
            char value = grid[i*width + j] ? '#' : ' ';
            printf("%c",value);
        }
        printf("\n");
    }
}

static inline void print_layer(layer_element *layer,
                              const unsigned long width,
                              const unsigned long height) {
    unsigned long i;
    unsigned long j;
    for (i=0; i<width; ++i) {
        for (j=0; j<height; ++j) {
            char value = layer[i*width + j] ? '#' : ' ';
            printf("%c",value);
        }
        printf("\n");
    }
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
        case I_OUTPUT:
            //assure that input/output is on the edges
            assert(__eq(p->dim.fsize.x,0.0) || __eq(p->dim.fsize.x,a->chip.dim.fsize.x) ||
                   __eq(p->dim.fsize.y,0.0) || __eq(p->dim.fsize.y,a->chip.dim.fsize.y));
            break;
        case I_CELL: {
            if (!(p->dim.fsize.x + p->cell->dim.fsize.x < a->chip.dim.fsize.x ||
                  __eq(p->dim.fsize.x + p->cell->dim.fsize.x,a->chip.dim.fsize.x))) {
                print_placement(p);
                printf("chip size (width:%lf height:%lf)\n",
                       a->chip.dim.fsize.x,a->chip.dim.fsize.y);
                printf("placement on x puts cell out of chip - exit\n");
                exit(EXIT_FAILURE);
            }

            if (!(p->dim.fsize.y + p->cell->dim.fsize.y < a->chip.dim.fsize.y ||
                  __eq(p->dim.fsize.y + p->cell->dim.fsize.y,a->chip.dim.fsize.y))) {
                print_placement(p);
                printf("chip size (width:%lf height:%lf)\n",
                       a->chip.dim.fsize.x,a->chip.dim.fsize.y);
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
    assert(soc->layer);

#if 1
    assert(p->input_gates || p->output_gates);
    //printf("___________________%lu\n",p->output_gates);
#else
    if (!p->input_gates && !p->output_gates)
        printf(" * WARNING: unused placement '%s'\n",p->name);
    else {
        if (!p->output_gates)
            printf(" * WARNING: unused output for placement '%s'\n",p->name);
        else if (p->output_gates != 1)
            printf(" * WARNING: multiple output for placement '%s'\n",p->name);

        if (!p->input_gates)
            printf(" * WARNING: no input for placement '%s'\n",p->name);
    }
#endif

    //reminder: bottom left point placement
    p->cell->dim.usize.x = floor(p->cell->dim.fsize.x / wire_size);
    p->dim.usize.x = floor(p->dim.fsize.x / wire_size);
    unsigned long cell_x = p->cell->dim.usize.x;
    unsigned long start_x = p->dim.usize.x;
    unsigned long end_x = start_x + cell_x;
    if (end_x > soc->grid_width) {
        print_placement(p);
        printf("end_x     : %lu\ngrid_width: %lu\nexit\n",end_x,soc->grid_width);
        exit(EXIT_FAILURE);
    }

    p->cell->dim.usize.y = floor(p->cell->dim.fsize.y / wire_size);
    p->dim.usize.y = floor(p->dim.fsize.y / wire_size);
    unsigned long cell_y = p->cell->dim.usize.y;
    unsigned long start_y = p->dim.usize.y;
    unsigned long end_y = start_y + cell_y;
    if (end_y > soc->grid_height) {
        print_placement(p);
        printf("end_y : %lu\ncell_y: %lu\nexit\n",end_y,cell_y);
        exit(EXIT_FAILURE);
    }

    unsigned long i;
    unsigned long j;
    for (i=start_x; i<end_x; ++i)
        for (j=start_y; j<end_y; ++j)
            soc->grid[i*soc->grid_width + j] = G_BLOCKED;

    unsigned long total_io = p->input_gates + p->output_gates;
    unsigned long vertical_space = (end_y - start_y)/(total_io+1);

    //fixed xaxis
    i = start_x + (end_x - start_x)/2;

    //find inputs' coordinates
    unsigned long input_slot = 0;
    for (input_slot = 0; input_slot<p->input_gates; ++input_slot) {
        j = start_y + input_slot * vertical_space;

        p->input_slots[input_slot].usize.x = i;
        p->input_slots[input_slot].usize.y = j;

        soc->layer[i*soc->grid_width + j] = L_IO;
    }

    //find output's coordinates
    j = start_y + input_slot * vertical_space;

    p->output_slot.usize.x = i;
    p->output_slot.usize.y = j;
}

void put_chip_io(struct placement_info *io, const double wire_size,
                 struct analysis_info *soc) {
    assert(io);
    assert(io->type == I_INPUT || io->type == I_OUTPUT);
    assert(soc);
    assert(soc->layer);

    io->dim.usize.x = floor(io->dim.fsize.x / wire_size);
    io->dim.usize.y = floor(io->dim.fsize.y / wire_size);
    unsigned long x = io->dim.usize.x;
    unsigned long y = io->dim.usize.y;
    soc->layer[x*soc->grid_width + y] = L_IO;
}

static void create_grid_and_layers(struct analysis_info *soc, const double wire_size) {
    assert(wire_size != 0.0);

    soc->chip.dim.usize.x = floor(soc->chip.dim.fsize.x / wire_size);
    soc->chip.dim.usize.y = floor(soc->chip.dim.fsize.y / wire_size);
    double cell_grid_x = soc->chip.dim.usize.x;
    double cell_grid_y = soc->chip.dim.usize.y;

    soc->wire_size = wire_size;
    soc->grid_width = cell_grid_x;
    soc->grid_height = cell_grid_y;

    soc->grid = (grid_element *)_calloc(soc->grid_width * soc->grid_height,
                                        sizeof(grid_element));

    soc->layer = (layer_element *)_calloc(soc->grid_width * soc->grid_height,
                                          sizeof(layer_element));

    //set placement on grid

    unsigned long i;
    for (i=0; i<soc->placement.next; ++i) {
        struct placement_info *placement =
            (struct placement_info *)(soc->placement.data) + i;
        if (placement->type == I_CELL)
            put_placement(placement,wire_size,soc);
        else if (placement->type == I_INPUT || placement->type == I_OUTPUT)
            put_chip_io(placement,wire_size,soc);
        else
            assert(0 && "bad placement type");
    }

    //print_grid(soc->grid,soc->grid_width,soc->grid_height);
    //print_layer(soc->layer,soc->grid_width,soc->grid_height);
}

void analyse(struct analysis_info *soc, const double wire_size) {
    check_analysis(soc);
    create_grid_and_layers(soc,wire_size);
}
