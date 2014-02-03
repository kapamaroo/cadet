#include <stdio.h>
#include <stdlib.h>

#include "parser.h"
#include "analysis.h"
#include "mikami.h"

#define DEFAULT_WIRE_SIZE 0.01

void print_help() {
    printf("$cadet libcell.txt chip_dimension.txt placement.txt netlist.txt [wire_size]\n\n");
}

int main(int argc, char *argv[]) {
    if (argc != 5 && argc != 6) {
        print_help();
        exit(EXIT_FAILURE);
    }

    struct analysis_info soc;
    parse(argv[1],argv[2],argv[3],argv[4],&soc);

    double wire_size = (argc == 6) ? atof(argv[5]) : DEFAULT_WIRE_SIZE;
    fprintf(stderr,"use wire size %g\n",wire_size);
    analyse(&soc,wire_size);

    //print_layer(soc.layer,soc.grid_width,soc.grid_height);
    unsigned long failed = route_mikami(&soc);

    //print_grid(soc.grid,soc.grid_width,soc.grid_height);

#if 0
    unsigned long i;
    for (i=0; i<soc.layer_num; ++i) {
        printf("LAYER %lu\n",i);
        print_layer(soc.layer[i],soc.grid_width,soc.grid_height);
        printf("\n");
    }
#endif

    if (failed)
        printf("%lu netlists failed to route in %lu wire layers\n",
               failed,soc.layer_num);
    else
        printf("all netlists succesfully routed in %lu wire layers\n",
               soc.layer_num);

    clear(&soc);
    return 0;
}
