#include <stdio.h>
#include <stdlib.h>

#include "parser.h"

void print_help() {
    printf("$cadet libcell.txt chip_dimension.txt placement.txt netlist.txt\n\n");
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        print_help();
        exit(EXIT_FAILURE);
    }

    struct analysis_info analysis;
    parse(argv[1],argv[2],argv[3],argv[4],&analysis);

    return 0;
}
