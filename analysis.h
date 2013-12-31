#ifndef __ANALYSIS_H__
#define __ANALYSIS_H__

#include "pool.h"

typedef char grid_element;

struct analysis_info {
    double wire_size;
    struct chip_info chip;

    struct pool_info libcell;
    struct pool_info placement;
    struct pool_info netlist;
};

void analyse(struct analysis_info *analysis);

#endif
