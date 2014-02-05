#include <stdio.h>
#include <stdlib.h>

#include "parser.h"
#include "analysis.h"
#include "mikami.h"

#define DEFAULT_WIRE_SIZE 0.01

int print_wire_layers = 1;

//set bigger values for more warning/status messages
int print_status = 1;
int print_warnings = 1;

#ifdef MIN
#undef MIN
#endif
#define MIN(a,b) ((a) < (b) ? (a) : (b))

loop_type max_loop = MIN(8,MAX_LOOP);
#undef MIN

void print_help() {
    printf("$cadet libcell.txt chip_dimension.txt placement.txt netlist.txt [wire_size]\n\n");
}

int main(int argc, char *argv[]) {
    if (argc != 5 && argc != 6) {
        print_help();
        exit(EXIT_FAILURE);
    }

    int i;
    for (i=0; i<argc; ++i)
        fprintf(stderr,"%s ",argv[i]);
    fprintf(stderr,"\n");
    for (i=0; i<argc; ++i)
        fprintf(stdout,"%s ",argv[i]);
    fprintf(stdout,"\n");

    struct analysis_info soc;
    parse(argv[1],argv[2],argv[3],argv[4],&soc);

    double total_nets = soc.pending_nets;
    double wire_size = (argc == 6) ? atof(argv[5]) : DEFAULT_WIRE_SIZE;

    printf("Total %4lu nets to route ...\n",soc.pending_nets);

    fprintf(stderr,"use wire size %g\n",wire_size);
    analyse(&soc,wire_size);

    //print_layer(soc.layer,soc.grid_width,soc.grid_height);
    unsigned long failed = 0;

    soc.max_loop = max_loop;
    failed = route_mikami(&soc);

    //print_grid(soc.grid,soc.grid_width,soc.grid_height);

    if (print_wire_layers)
        print_layers(&soc);

    if (failed) {
        printf("%lu nets (%05.2f%%) failed to route in %d wire layers (wire size %.2f)\n",
               failed,failed/total_nets*100,soc.layer_num,wire_size);
    }
    else
        printf("all nets succesfully routed in %d wire layers, total wire length %.2f, wire size %.2f\n",
               soc.layer_num,soc.wire_len,wire_size);

    clear(&soc);
    return 0;
}
