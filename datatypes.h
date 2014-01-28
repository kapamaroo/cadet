#ifndef __DATATYPES_H__
#define __DATATYPES_H__

#include <string.h>
#include <assert.h>
#include "pool.h"

#define MAX_NET_DRAIN 128

struct double_size {
    double x;
    double y;
};

struct ulong_size {
    unsigned long x;
    unsigned long y;
};

struct dim_size {
    struct double_size fsize;
    struct ulong_size usize;
};

struct chip_info {
    struct dim_size dim;
};

struct libcell_info {
    char *name;
    struct dim_size dim;
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
    struct dim_size dim;

    //number of input/output
    struct pool_info input_slots;
    struct dim_size output_slot;

    unsigned long input_gates;
    unsigned long output_gates;
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

#endif
