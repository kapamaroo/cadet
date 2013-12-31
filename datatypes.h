#ifndef __DATATYPES_H__
#define __DATATYPES_H__

#include <string.h>
#include <assert.h>
#include "pool.h"

#define MAX_NET_DRAIN 128

struct chip_info {
    double width;   //x
    double height;  //y
};

struct libcell_info {
    char *name;
    double size_x;
    double size_y;
};

//consider inputs and outputs as cell instances of 'input'/'output' cells

enum instance_type {
    I_INPUT = 0,
    I_OUTPUT,
    I_CELL
};

struct placement_info {
    enum instance_type type;
    char *name;  //instance name

    struct libcell_info *cell;

    //bottom left point
    double x;
    double y;
};

struct net_info {
    char *name;

    struct placement_info *source;
    struct placement_info *drain[MAX_NET_DRAIN];
    unsigned long num_drain;

    /*
       if (source->type == I_CELL)
           // internal netlist, num_drain >= 1
       else if (source->type == I_INPUT)
           // drains are internal cells, num_drain >= 1
       else if (source->type == I_OUTPUT)
           // drain is internal cell, num_drain == 1
    */
};

static inline void check_placement(struct placement_info *placement) {
    assert(placement);
    assert(placement->cell);

    switch (placement->type) {
    case I_CELL:
        assert(strcmp(placement->cell->name,"input"));
        assert(strcmp(placement->cell->name,"output"));
        return;
    case I_INPUT:
        assert(!strcmp(placement->cell->name,"input"));
        return;
    case I_OUTPUT:
        assert(!strcmp(placement->cell->name,"output"));
        return;
    }
}

static inline void check_net(struct net_info *net) {
    assert(net);
    assert(net->source);

    switch (net->source->type) {
    case I_CELL:
    case I_INPUT:
        assert(net->num_drain >= 1);
        break;
    case I_OUTPUT:
        //some outputs connect to more than 1 cells ??
        //assert(net->num_drain == 1);
        break;
    }

    unsigned long i;
    for (i=0; i<net->num_drain; ++i) {
        assert(net->drain[i]);
    }
}

static inline enum instance_type get_net_type(struct net_info *net) {
    assert(net);
    assert(net->source);
    return net->source->type;
}

struct analysis_info {
    double wire_size;
    struct chip_info chip;

    struct pool_info libcell;
    struct pool_info placement;
    struct pool_info netlist;
};

#endif
