#ifndef __ANALYSIS_H__
#define __ANALYSIS_H__

#include "pool.h"
#include "datatypes.h"

#define G_EMPTY    0
#define G_BLOCKED  1
#define G_INPUT    2
#define G_OUTPUT   3

#define L_INVALID  -1

//status
#define L_EMPTY            0
#define L_TRY             (1 << 0)
#define L_WIRE            (1 << 1)
#define L_VIA             (1 << 2)
#define L_IO              (1 << 3)

#define L_START           (1 << 4)
#define L_TERM            (1 << 5)

#define TRY_EMPTY  0

//from source
#define S_TRY_WIRE_LEFT   (1 << 0)
#define S_TRY_WIRE_RIGHT  (1 << 1)
#define S_TRY_WIRE_UP     (1 << 2)
#define S_TRY_WIRE_DOWN   (1 << 3)

#define S_TRY_ANY (S_TRY_WIRE_LEFT | S_TRY_WIRE_RIGHT | S_TRY_WIRE_UP | S_TRY_WIRE_DOWN)

//from term
#define T_TRY_WIRE_LEFT   (1 << 4)
#define T_TRY_WIRE_RIGHT  (1 << 5)
#define T_TRY_WIRE_UP     (1 << 6)
#define T_TRY_WIRE_DOWN   (1 << 7)

#define T_TRY_ANY (T_TRY_WIRE_LEFT | T_TRY_WIRE_RIGHT | T_TRY_WIRE_UP | T_TRY_WIRE_DOWN)

#define T_TRY_HORIZONTAL (T_TRY_WIRE_LEFT | T_TRY_WIRE_RIGHT)
#define T_TRY_VERTICAL   (T_TRY_WIRE_UP | T_TRY_WIRE_DOWN)

#define S_TRY_HORIZONTAL (S_TRY_WIRE_LEFT | S_TRY_WIRE_RIGHT)
#define S_TRY_VERTICAL   (S_TRY_WIRE_UP | S_TRY_WIRE_DOWN)

#define MAX_LOOP 128
typedef unsigned char loop_type;  //7 bits only, if low bit is 1 aka L_TRY

struct _layer_element_ {
    unsigned char loop_status;  //uses the lower 4 bits only
    unsigned char try;     //[  hi 4 bits for term  |  low 4 bits for source  ]
};

typedef unsigned char grid_element;
typedef struct _layer_element_ layer_element;

#define MAX_LAYERS 8

struct analysis_info {
    struct chip_info chip;

    struct pool_info libcell;
    struct pool_info placement;
    struct pool_info netlist;

    double wire_size;
    double wire_len;   //total for all layers

    //normalized (wire grid) dimensions regarding wire_size
    unsigned long grid_width;
    unsigned long grid_height;

    grid_element *grid;

    layer_element *layer[MAX_LAYERS];
    int layer_num;

    loop_type max_loop;
    unsigned long pending_nets;
};

void analyse(struct analysis_info *soc, const double wire_size);
void print_grid(grid_element *layer,
                const unsigned long width,const unsigned long height);
void print_layers(struct analysis_info *soc);
void print_layer(layer_element *layer,
                 const unsigned long width,const unsigned long height);
void clear(struct analysis_info *soc);

#endif
