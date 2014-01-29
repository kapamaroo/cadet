#ifndef __ANALYSIS_H__
#define __ANALYSIS_H__

#include "pool.h"
#include "datatypes.h"

#if 1

enum grid_element_status {
    G_EMPTY = 0,
    G_BLOCKED,
    G_INPUT,
    G_OUTPUT
};

enum layer_element_status {
    L_EMPTY = 0,
    L_TRY_WIRE_LEFT  = 1 << 0,
    L_TRY_WIRE_RIGHT = 1 << 1,
    L_TRY_WIRE_UP    = 1 << 2,
    L_TRY_WIRE_DOWN  = 1 << 3,
    L_WIRE           = 1 << 4,
    L_VIA            = 1 << 5,
    L_IO             = 1 << 6
};

typedef enum grid_element_status grid_element;
typedef enum layer_element_status layer_element;

#else

#define G_EMPTY    0
#define G_BLOCKED  1
#define G_INPUT    2
#define G_OUTPUT   3

#define L_EMPTY            0
#define L_TRY_WIRE_LEFT   (1 << 0)
#define L_TRY_WIRE_RIGHT  (1 << 1)
#define L_TRY_WIRE_UP     (1 << 2)
#define L_TRY_WIRE_DOWN   (1 << 3)
#define L_WIRE            (1 << 4)
#define L_VIA             (1 << 5)
#define L_IO              (1 << 6)

typedef unsigned char grid_element;
typedef unsigned char layer_element;

#endif

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
    layer_element *layer;
};

void analyse(struct analysis_info *soc, const double wire_size);
void print_grid(grid_element *layer,
                const unsigned long width,const unsigned long height);
void print_layer(layer_element *layer,
                 const unsigned long width,const unsigned long height);

#endif
