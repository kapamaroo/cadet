#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "pool.h"
#include "parser_core.h"
#include "parser.h"
#include "analysis.h"

struct chip_info parse_chip_dim(struct file_info *input);
struct pool_info parse_libcell(struct file_info *input);
struct pool_info parse_placement(struct file_info *input, struct pool_info libcell);
struct pool_info parse_netlist(struct file_info *input, struct pool_info placement);

//debug function
static void check_placement(struct placement_info *placement) {
    assert(placement);
    assert(placement->cell);

    switch (placement->type) {
    case I_INPUT:
        assert(!strcmp(placement->cell->name,"input"));
        return;
    case I_OUTPUT:
        assert(!strcmp(placement->cell->name,"output"));
        return;
    case I_CELL:
        assert(strcmp(placement->cell->name,"input"));
        assert(strcmp(placement->cell->name,"output"));
        return;
    }
}

//debug function
static void check_net(struct net_info *net) {
    assert(net);
    assert(net->source);

    switch (net->source->type) {
    case I_CELL:
    case I_INPUT:
        assert(net->num_drain >= 1);
        break;
    case I_OUTPUT:
        //assert(net->num_drain == 1);
        assert(net->num_drain >= 1);
        //some outputs connect to more than 1 cells ??
        //break;  //disable warning
        if (net->num_drain > 1) {
            printf(" * WARNING: multiple inputs/cells connected to output:");
            printf("   %lu-to-1:    {",net->num_drain);
            unsigned long i;
            //print all but last
            for (i=0; i<net->num_drain-1; ++i)
                printf("%s, ",net->drain[i]->name);
            //print last
            printf("%s}  -->  {%s}\n",net->drain[i]->name,net->source->name);
        }
        break;
    }

    unsigned long i;
    for (i=0; i<net->num_drain; ++i) {
        assert(net->drain[i]);
    }
}

void parse(const char *libcell, const char *chip_dim,
           const char *placement, const char *netlist,
           struct analysis_info *analysis) {
    assert(analysis);

    struct file_info *current_input = NULL;

    current_input = open_file(libcell);
    struct pool_info _libcell = parse_libcell(current_input);
    close_file(&current_input);

    current_input = open_file(chip_dim);
    struct chip_info _chip = parse_chip_dim(current_input);
    close_file(&current_input);

    current_input = open_file(placement);
    struct pool_info _placement = parse_placement(current_input,_libcell);
    close_file(&current_input);

    current_input = open_file(netlist);
    struct pool_info _netlist = parse_netlist(current_input,_placement);
    close_file(&current_input);

    analysis->chip = _chip;
    analysis->libcell = _libcell;
    analysis->placement = _placement;
    analysis->netlist = _netlist;
}

struct chip_info parse_chip_dim(struct file_info *input) {
    assert(input);

    struct chip_info output;
    memset(&output,0,sizeof(struct chip_info));

    //get height
    while (*input->pos++ != '=') ;
    parse_eat_whitechars(input);
    output.height = parse_value(input,NULL,"chip height");

    //get width
    while (*input->pos++ != '=') ;
    parse_eat_whitechars(input);
    output.width = parse_value(input,NULL,"chip width");

    return output;
}

struct pool_info parse_libcell(struct file_info *input) {
    assert(input);

    struct pool_info output;
    memset(&output,0,sizeof(struct pool_info));
    output.element_size = sizeof(struct libcell_info);

    //add two extra cells one of types 'input' and 'output'
    grow(&output,NULL);
    struct libcell_info *cell_input = (struct libcell_info *)(output.data) + output.next++;
    cell_input->name = "input";
    cell_input->size_x = 0.0;
    cell_input->size_y = 0.0;

    grow(&output,NULL);
    struct libcell_info *cell_output = (struct libcell_info *)(output.data) + output.next++;
    cell_output->name = "output";
    cell_output->size_x = 0.0;
    cell_output->size_y = 0.0;

    //skip first line
    discard_line(input);
    parse_eat_newline(input);

    while (input->pos) {
        //parse line
        grow(&output,NULL);
        struct libcell_info *cell = (struct libcell_info *)(output.data) + output.next++;
        memset(cell,0,sizeof(struct libcell_info));

        //parse name
        parse_eat_whitechars(input);
        cell->name = parse_string(input,"libcell name");

        //parse size_x
        parse_eat_whitechars(input);
        cell->size_x = parse_value(input,NULL,"size x");

        //parse size_y
        parse_eat_whitechars(input);
        cell->size_y = parse_value(input,NULL,"size y");

        int bad_chars = discard_line(input);
        if (bad_chars)
            printf("%s:***  WARNING  ***  %d bad characters at end of line %lu\n",
                   __FUNCTION__,bad_chars,input->line_num);
        parse_eat_newline(input);
    }

    return output;
}

struct pool_info parse_placement(struct file_info *input, struct pool_info libcell){
    assert(input);

    struct pool_info output;
    memset(&output,0,sizeof(struct pool_info));
    output.element_size = sizeof(struct placement_info);

    while (input->pos) {
        //parse line
        grow(&output,NULL);
        struct placement_info *placement = (struct placement_info *)(output.data) + output.next++;
        memset(placement,0,sizeof(struct placement_info));

        //parse libcell name
        parse_eat_whitechars(input);
        char *libcell_name = parse_string(input,"libcell_name");

        struct libcell_info *cell = (struct libcell_info *)(libcell.data);
        if (strcmp(libcell_name,cell[0].name) == 0) {
            placement->cell = &cell[0];
            placement->type = I_INPUT;
        }
        else if (strcmp(libcell_name,cell[1].name) == 0) {
            placement->cell = &cell[1];
            placement->type = I_OUTPUT;
        }
        else {
            unsigned long i;
            for (i=2; i<libcell.next; ++i) {
                if (strcmp(libcell_name,cell[i].name) == 0) {
                    placement->cell = &cell[i];
                    placement->type = I_CELL;
                    break;
                }
            }
        }

        if (!placement->cell) {
            printf("error:%lu:unknown libcell '%s' - exit\n",input->line_num,libcell_name);
            exit(EXIT_FAILURE);
        }
        free(libcell_name);

        //parse name
        parse_eat_whitechars(input);
        placement->name = parse_string(input,"placement name");

        //parse size_x
        parse_eat_whitechars(input);
        placement->x = parse_value(input,NULL,"placement x");

        //parse size_y
        parse_eat_whitechars(input);
        placement->y = parse_value(input,NULL,"placement y");

        int bad_chars = discard_line(input);
        if (bad_chars)
            printf("%s:***  WARNING  ***  %d bad characters at end of line %lu\n",
                   __FUNCTION__,bad_chars,input->line_num);
        parse_eat_newline(input);
        check_placement(placement);
    }

    return output;
}

static struct placement_info *get_placement(const char *name,struct pool_info pool){
    unsigned long i;
    for (i=0; i<pool.next; ++i) {
        struct placement_info *placement = (struct placement_info *)(pool.data) + i;
        if (strcmp(name,placement->name) == 0)
            return placement;
    }
    return NULL;
}

struct pool_info parse_netlist(struct file_info *input, struct pool_info placement){
    assert(input);

    struct pool_info output;
    memset(&output,0,sizeof(struct pool_info));
    output.element_size = sizeof(struct net_info);

    while (input->pos) {
        //parse net
        grow(&output,NULL);
        struct net_info *net = (struct net_info *)(output.data) + output.next++;
        memset(net,0,sizeof(struct net_info));

        //parse name
        parse_eat_whitechars(input);
        net->name = parse_string(input,"net name");

        //parse source
        parse_eat_whitechars(input);
        char *source_name = parse_string(input,"source name");
        net->source = get_placement(source_name,placement);
        if (!net->source) {
            printf("error:%lu:unknown placement '%s' - exit\n",input->line_num,source_name);
            exit(EXIT_FAILURE);
        }

        net->source->output_gates++;
        free(source_name);

        while (*input->pos != ';' && net->num_drain < MAX_NET_DRAIN) {
            //parse drain
            parse_eat_whitechars(input);
            unsigned long i = net->num_drain++;

            char *drain_name = parse_string(input,"drain name");
            net->drain[i] = get_placement(drain_name,placement);
            free(drain_name);

            if (!net->drain[i]){
                printf("error:%lu:unknown placement '%s' - exit\n",input->line_num,source_name);
                exit(EXIT_FAILURE);
            }
            net->drain[i]->input_gates++;
        }

        if (*input->pos == ';')
            input->pos++;

        int bad_chars = discard_line(input);
        if (bad_chars)
            printf("%s:***  WARNING  ***  %d bad characters at end of line %lu\n",
                   __FUNCTION__,bad_chars,input->line_num);
        parse_eat_newline(input);
        check_net(net);
    }

    return output;
}
