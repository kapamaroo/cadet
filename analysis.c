#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

#include "analysis.h"
#include "toolbox.h"

#define TOL 1e-5

extern int print_warnings;

static inline int __eq(const double a, const double b) {
    return fabs(a - b) <= TOL;
}

static inline void print_placement(struct placement_info *p) {
    assert(p);
    fprintf(stderr, "%s %s %lf %lf\t#libcell size (x:%lf y:%lf)\n",
            p->cell->name, p->name,p->dim.fsize.x,p->dim.fsize.y,
            p->cell->dim.fsize.x,p->cell->dim.fsize.y);
}

void print_grid(grid_element *grid,
                const unsigned long width,
                const unsigned long height) {
    unsigned long i;
    unsigned long j;
    for (i=0; i<height; ++i) {
        for (j=0; j<width; ++j) {
            char value = grid[i*width + j] ? '#' : ' ';
            fprintf(stderr,"%c",value);
        }
        fprintf(stderr,"\n");
    }
}

void print_layers(struct analysis_info *soc) {
    unsigned long i;
    for (i=0; i<soc->layer_num; ++i) {
        if (!soc->layer[i])
            break;
        fprintf(stderr,"LAYER %lu\n",i);
        print_layer(soc->layer[i],soc->grid_width,soc->grid_height);
        fprintf(stderr,"\n");
    }
}

void print_layer(layer_element *layer,
                 const unsigned long width,
                 const unsigned long height) {
    unsigned long i;
    unsigned long j;
    for (i=0; i<height; ++i) {
        fprintf(stderr,"|");
        for (j=0; j<width; ++j) {
            char c;
            if (layer[i*width + j].loop_status & L_TRY)
                layer[i*width + j].loop_status = L_TRY;
            switch (layer[i*width + j].loop_status) {
            case L_EMPTY:           c = ' ';  break;
            case L_IO:              c = 'O';  break;
            case L_WIRE:            c = 'X';  break;
            case L_VIA:             c = '#';  break;
            case L_START:           c = 'S';  break;
            case L_TERM:            c = 'T';  break;
            case L_TRY: {
                c = '-';
                switch (layer[i*width + j].try) {
                case S_TRY_WIRE_LEFT:   c = 'l';  break;
                case S_TRY_WIRE_RIGHT:  c = 'r';  break;
                case S_TRY_WIRE_UP:     c = 'u';  break;
                case S_TRY_WIRE_DOWN:   c = 'd';  break;

                case T_TRY_WIRE_LEFT:   c = 'L';  break;
                case T_TRY_WIRE_RIGHT:  c = 'R';  break;
                case T_TRY_WIRE_UP:     c = 'U';  break;
                case T_TRY_WIRE_DOWN:   c = 'D';  break;
                }
                break;
            }
            default:                c = '+';  break;
            }
            fprintf(stderr,"%c",c);
        }
        fprintf(stderr,"|\n");
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
                fprintf(stderr,"chip size (width:%lf height:%lf)\n",
                        a->chip.dim.fsize.x,a->chip.dim.fsize.y);
                printf("error: placement on x puts cell out of chip - exit\n");
                exit(EXIT_FAILURE);
            }

            if (!(p->dim.fsize.y + p->cell->dim.fsize.y < a->chip.dim.fsize.y ||
                  __eq(p->dim.fsize.y + p->cell->dim.fsize.y,a->chip.dim.fsize.y))) {
                print_placement(p);
                fprintf(stderr,"chip size (width:%lf height:%lf)\n",
                        a->chip.dim.fsize.x,a->chip.dim.fsize.y);
                printf("error: placement on y puts cell out of chip - exit\n");
                exit(EXIT_FAILURE);
            }

            break;
        }
        }
    }
}

void put_placement(struct placement_info *p, const double wire_size,
                   struct analysis_info *soc) {
    assert(p);
    assert(p->type == I_CELL);
    assert(soc);
    assert(soc->layer[0]);

    if (print_warnings < 2)
        ;  //ok
    else if (p->input_gates && p->output_gates)
        ;  //ok
    else if (!p->input_gates && !p->output_gates)
        printf("***  WARNING  ***  unused placement '%s'\n",p->name);
    else if (!p->output_gates)
        printf("***  WARNING  ***  unused output for placement '%s'\n",p->name);
    else if (!p->input_gates)
        printf("***  WARNING  ***  no input for placement '%s'\n",p->name);

    //reminder: bottom left point placement
    p->cell->dim.usize.x = floor(p->cell->dim.fsize.x / wire_size);
    p->dim.usize.x = floor(p->dim.fsize.x / wire_size);
    unsigned long cell_x = p->cell->dim.usize.x;
    unsigned long start_x = p->dim.usize.x;
    unsigned long end_x = start_x + cell_x;
    if (end_x > soc->grid_height) {
        print_placement(p);
        printf("error: end_x=%lu\theight=%lu\texit\n",end_x,soc->grid_height);
        exit(EXIT_FAILURE);
    }

    p->cell->dim.usize.y = floor(p->cell->dim.fsize.y / wire_size);
    p->dim.usize.y = floor(p->dim.fsize.y / wire_size);
    unsigned long cell_y = p->cell->dim.usize.y;
    unsigned long start_y = p->dim.usize.y;
    unsigned long end_y = start_y + cell_y;
    if (end_y > soc->grid_width) {
        print_placement(p);
        printf("error: end_y=%lu\twidth=%lu\texit\n",end_y,soc->grid_width);
        exit(EXIT_FAILURE);
    }

    unsigned long i;
    unsigned long j;

#if 0
    assert(soc->grid);
    for (i=start_x; i<end_x; ++i)
        for (j=start_y; j<end_y; ++j)
            soc->grid[i*soc->grid_width + j] = G_BLOCKED;
#endif

    //consider just one output
    unsigned long total_io = p->input_gates + 1;
    p->slot = (struct dim_size *)_calloc(total_io,sizeof(struct dim_size));

    unsigned long y_space = (end_y - start_y)/(total_io+1);
    unsigned long x_space = (end_x - start_x)/(total_io+1);

    //find io coordinates
    unsigned long _slot = 0;
    for (_slot = 0; _slot<total_io; ++_slot) {
        i = start_x + (_slot + 1) * x_space;
        j = start_y + (_slot + 1) * y_space;

        assert(i < soc->grid_height);
        assert(j < soc->grid_width);

        p->slot[_slot].usize.x = i;
        p->slot[_slot].usize.y = j;

        soc->layer[0][i*soc->grid_width + j].loop_status = L_IO;
    }
    p->next_free_input_slot = 1;
}

void put_chip_io(struct placement_info *io, const double wire_size,
                 struct analysis_info *soc) {
    assert(io);
    assert(io->type == I_INPUT || io->type == I_OUTPUT);
    assert(soc);
    assert(soc->layer[0]);

    io->dim.usize.x = floor(io->dim.fsize.x / wire_size);
    io->dim.usize.y = floor(io->dim.fsize.y / wire_size);
    unsigned long x = io->dim.usize.x;
    unsigned long y = io->dim.usize.y;

    //if on boundary, put them inside chip
    if (x == soc->grid_height)
        x--;
    if (y == soc->grid_width)
        y--;

    if (x >= soc->grid_height) {
        fprintf(stderr,"x:(%g) : %lu >= %lu\n",io->dim.fsize.x,x,soc->grid_height);
        assert(0);
        printf("error: io.x : out of bounds index - exit\n");
        exit(EXIT_FAILURE);
    }
    if (y >= soc->grid_width) {
        fprintf(stderr,"y:(%g) : %lu >= %lu\n",io->dim.fsize.y,y,soc->grid_width);
        assert(0);
        printf("error: io.y : out of bounds index - exit\n");
        exit(EXIT_FAILURE);
    }

    io->slot = (struct dim_size *)_calloc(1,sizeof(struct dim_size));
    io->slot[0].usize.x = x;
    io->slot[0].usize.y = y;
    io->next_free_input_slot = 0;

    soc->layer[0][x*soc->grid_width + y].loop_status = L_IO;
}

static void create_grid_and_layers(struct analysis_info *soc) {
    double wire_size = soc->wire_size;

    soc->chip.dim.usize.x = ceil(soc->chip.dim.fsize.x / wire_size);
    soc->chip.dim.usize.y = ceil(soc->chip.dim.fsize.y / wire_size);

    //chip height is the number of rows
    //chip width is the number of columns

    soc->grid_width = soc->chip.dim.usize.y;
    soc->grid_height = soc->chip.dim.usize.x;

#if 0
    soc->grid = (grid_element *)_calloc(soc->grid_width * soc->grid_height,
                                        sizeof(grid_element));
#endif

    soc->layer[0] = (layer_element *)_calloc(soc->grid_width * soc->grid_height,
                                             sizeof(layer_element));
    soc->layer_num = 1;

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
}

void analyse(struct analysis_info *soc, const double wire_size) {
    check_analysis(soc);
    assert(!__eq(wire_size,0.0));
    soc->wire_size = wire_size;
    create_grid_and_layers(soc);
}

void clear(struct analysis_info *soc) {
    assert(soc);

    if (soc->grid)
        free(soc->grid);

    unsigned long i;

    //the first 2 are static "input"/"output" strings, see parser.c
    for (i=2; i<soc->libcell.next; ++i) {
        struct libcell_info *libcell =
            (struct libcell_info *)(soc->libcell.data) + i;
        free(libcell->name);
    }
    free(soc->libcell.data);

    for (i=0; i<soc->placement.next; ++i) {
        struct placement_info *placement =
            (struct placement_info *)(soc->placement.data) + i;
        free(placement->name);
    }
    free(soc->placement.data);

    for (i=0; i<soc->netlist.next; ++i) {
        struct net_info *net =
            (struct net_info *)(soc->netlist.data) + i;
        free(net->name);
    }
    free(soc->netlist.data);

    for (i=0; i<soc->layer_num; ++i) {
        if (!soc->layer[i])
            break;
        free(soc->layer[i]);
    }

    memset(soc,0,sizeof(struct analysis_info));
}
