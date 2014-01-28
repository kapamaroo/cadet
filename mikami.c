#include <stdio.h>
#include "analysis.h"

void mikami(struct ulong_size S, struct ulong_size T, layer_element *layer,
            const unsigned long width, const unsigned long height);

void route_mikami(struct analysis_info *soc, const double wire_size) {
    unsigned long i;
    unsigned long j;
    for (i=0; i<soc->netlist.next; ++i) {
        struct net_info *netlist = (struct net_info *)(soc->netlist.data) + i;
        for (j=0; j<netlist->num_drain; ++j) {
            unsigned long next_input = netlist->drain[j]->next_free_input_slot++;
            mikami(netlist->source->output_slot.usize,
                   netlist->drain[j]->input_slots[next_input].usize,
                   soc->layer,soc->grid_width,soc->grid_height);
        }
    }
}

void mikami(struct ulong_size S, struct ulong_size T, layer_element *layer,
            const unsigned long width, const unsigned long height) {
    //implement me
    //printf("mikami se lew\n");

}
