#include <stdio.h>
#include <stdlib.h>

#include "parser.h"

#define DEFAULT_WIRE_SIZE 0.1

void print_help() {
    printf("$cadet libcell.txt chip_dimension.txt placement.txt netlist.txt [wire_size]\n\n");
}

int main(int argc, char *argv[]) {
    if (argc != 5 && argc != 6) {
        print_help();
        exit(EXIT_FAILURE);
    }

    struct analysis_info analysis;
    parse(argv[1],argv[2],argv[3],argv[4],&analysis);

    double wire_size = (argc == 6) ? atof(argv[5]) : DEFAULT_WIRE_SIZE;
    analysis.wire_size = wire_size;

    return 0;
}
