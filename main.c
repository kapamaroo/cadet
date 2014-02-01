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
    analyse(&soc,wire_size);

    //print_layer(soc.layer,soc.grid_width,soc.grid_height);
    unsigned long failed = route_mikami(&soc,wire_size);

    //print_grid(soc.grid,soc.grid_width,soc.grid_height);
    print_layer(soc.layer,soc.grid_width,soc.grid_height);

    if (failed) {
        printf("%lu netlists failed to route\n",failed);
        return 1;
    }

    //success!
    printf("all netlists succesfully routed\n");
    return 0;
}
