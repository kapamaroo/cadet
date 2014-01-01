#ifndef __ANALYSIS_H__
#define __ANALYSIS_H__

#include "pool.h"
#include "datatypes.h"

enum grid_element_status {
    G_EMPTY = 0,
    G_BLOCKED,
    G_WIRE,
    G_VIA
};

typedef enum grid_element_status grid_element;

struct analysis_info {
    struct chip_info chip;

    struct pool_info libcell;
    struct pool_info placement;
    struct pool_info netlist;

    double wire_size;

    //normalized (wire grid) dimensions regarding wire_size
    unsigned long grid_width;
    unsigned long grid_height;
    grid_element *grid;
};

void analyse(struct analysis_info *soc, const double wire_size);

#endif
