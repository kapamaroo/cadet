#ifndef __DATATYPES_H__
#define __DATATYPES_H__

#include <string.h>
#include <assert.h>

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

#endif
