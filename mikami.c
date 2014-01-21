#include <stdio.h>
#include "analysis.h"

void mikami(struct placement_info *source, struct placement_info *drain, layer_element *layer,
            const unsigned long width, const unsigned long height);

void route_mikami(struct analysis_info *soc, const double wire_size) {
    unsigned long i;
    unsigned long j;
    for (i=0; i<soc->netlist.next; ++i) {
        struct net_info *netlist = (struct net_info *)(soc->netlist.data) + i;
        for (j=0; j<netlist->num_drain; ++j)
            mikami(netlist->source,netlist->drain[j],soc->layer,soc->grid_width,soc->grid_height);
    }
}

void mikami(struct placement_info *source, struct placement_info *drain, layer_element *layer,
            const unsigned long width, const unsigned long height) {
    //implement me
    //printf("mikami se lew\n");

}
