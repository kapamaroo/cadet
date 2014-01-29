#include <stdio.h>
#include "analysis.h"

#define MAX_LOOP 1024

layer_element *layer = NULL;
unsigned long width = 0;
unsigned long height = 0;

#define LAYER(x,y) (layer[(x)*width + (y)])

int mikami(struct ulong_size S, struct ulong_size T);

int route_mikami(struct analysis_info *soc, const double wire_size) {
    layer = soc->layer;
    width = soc->grid_width;
    height = soc->grid_height;

    unsigned long i;
    unsigned long j;
    unsigned long net = 0;

    for (i=0; i<soc->netlist.next; ++i) {
        struct net_info *netlist = (struct net_info *)(soc->netlist.data) + i;
        for (j=0; j<netlist->num_drain; ++j) {
            net++;
            printf("__________    routing net %4lu ...\n",net);
            unsigned long next_input = netlist->drain[j]->next_free_input_slot++;
            int error = mikami(netlist->source->output_slot.usize,
                               netlist->drain[j]->input_slots[next_input].usize);
            if (error)
                return error;
        }
    }
    return 0;
}

static inline int blocked(const layer_element l) {
    if ( l == L_WIRE || l == L_VIA || l == L_IO)
        return 1;
    return 0;
}

void try_left(struct ulong_size P) {
    unsigned long j = P.y;
    if (j == 0)
        return;
    j--;
    while (j) {
        if (blocked(LAYER(P.x,j)))
            return;
        LAYER(P.x,j) |= L_TRY_WIRE_LEFT;
        j--;
    }
    //check first column
    if (blocked(LAYER(P.x,0)))
        return;
    LAYER(P.x,0) |= L_TRY_WIRE_LEFT;
}

void try_right(struct ulong_size P) {
    unsigned long j = P.y;
    j++;
    while (j < width) {
        if (blocked(LAYER(P.x,j)))
            return;
        LAYER(P.x,j) |= L_TRY_WIRE_RIGHT;
        j++;
    }
}

void try_up(struct ulong_size P) {
    unsigned long i = P.x;
    if (i == 0)
        return;
    i--;
    while (i) {
        if (blocked(LAYER(i,P.y)))
            return;
        LAYER(i,P.y) |= L_TRY_WIRE_UP;
        i--;
    }
    //check first row
    if (blocked(LAYER(0,P.y)))
        return;
    LAYER(0,P.y) |= L_TRY_WIRE_UP;
}

void try_down(struct ulong_size P) {
    unsigned long i = P.x;
    i++;
    while (i < height) {
        if (blocked(LAYER(i,P.y)))
            return;
        LAYER(i,P.y) |= L_TRY_WIRE_DOWN;
        i++;
    }
}

/////////////////////////////////////////////////////////////
void mark_left(struct ulong_size P) {
    unsigned long j = P.y;
    if (j == 0)
        return;
    j--;
    while (j) {
        if (LAYER(P.x,j) == L_IO)
            return;
        LAYER(P.x,j) = L_WIRE;
        j--;
    }
    //check first column
    if (LAYER(P.x,0) == L_IO)
        return;
    LAYER(P.x,0) = L_WIRE;
}

void mark_right(struct ulong_size P) {
    unsigned long j = P.y;
    j++;
    while (j < width) {
        if (LAYER(P.x,j) == L_IO)
            return;
        LAYER(P.x,j) = L_WIRE;
        j++;
    }
}

void mark_up(struct ulong_size P) {
    unsigned long i = P.x;
    if (i == 0)
        return;
    i--;
    while (i) {
        if (LAYER(i,P.y) == L_IO)
            return;
        LAYER(i,P.y) = L_WIRE;
        i--;
    }
    //check first row
    if (LAYER(0,P.y) == L_IO)
        return;
    LAYER(0,P.y) = L_WIRE;
}

void mark_down(struct ulong_size P) {
    unsigned long i = P.x;
    i++;
    while (i < height) {
        if (LAYER(i,P.y) == L_IO)
            return;
        LAYER(i,P.y) = L_WIRE;
        i++;
    }
}

void clean_layer() {
    unsigned long i;
    unsigned long j;
    for (i=0; i<width; ++i)
        for (j=0; j<height; ++j)
            if (LAYER(i,j) != L_EMPTY && !blocked(LAYER(i,j)))
                LAYER(i,j) = L_EMPTY;
}

static inline layer_element get_connection(const layer_element l) {
    if (l == (L_TRY_WIRE_LEFT & L_TRY_WIRE_RIGHT))
        return L_WIRE;

    if (l == (L_TRY_WIRE_LEFT & L_TRY_WIRE_UP))
        return L_VIA;
    if (l == (L_TRY_WIRE_LEFT & L_TRY_WIRE_DOWN))
        return L_VIA;

    if (l == (L_TRY_WIRE_RIGHT & L_TRY_WIRE_UP))
        return L_VIA;
    if (l == (L_TRY_WIRE_RIGHT & L_TRY_WIRE_DOWN))
        return L_VIA;

    if (l == (L_TRY_WIRE_UP & L_TRY_WIRE_DOWN))
        return L_WIRE;

    return l;
}

void mark_path(const struct ulong_size p, const layer_element l) {
    //mark the opposite directions

    if (l == (L_TRY_WIRE_LEFT & L_TRY_WIRE_RIGHT)) {
        LAYER(p.x,p.y) = L_WIRE;
        mark_right(p);
        mark_left(p);
    }
    else if (l == (L_TRY_WIRE_LEFT & L_TRY_WIRE_UP)) {
        LAYER(p.x,p.y) = L_VIA;
        mark_right(p);
        mark_down(p);
    }
    else if (l == (L_TRY_WIRE_LEFT & L_TRY_WIRE_DOWN)) {
        LAYER(p.x,p.y) = L_VIA;
        mark_right(p);
        mark_up(p);
    }
    else if (l == (L_TRY_WIRE_RIGHT & L_TRY_WIRE_UP)) {
        LAYER(p.x,p.y) = L_VIA;
        mark_left(p);
        mark_down(p);
    }
    else if (l == (L_TRY_WIRE_RIGHT & L_TRY_WIRE_DOWN)) {
        LAYER(p.x,p.y) = L_VIA;
        mark_right(p);
        mark_down(p);
    }
    else if (l == (L_TRY_WIRE_UP & L_TRY_WIRE_DOWN)) {
        LAYER(p.x,p.y) = L_WIRE;
        mark_down(p);
        mark_up(p);
    }
}

int has_intersection() {
    unsigned long i;
    unsigned long j;
    for (i=0; i<width; ++i) {
        for (j=0; j<height; ++j) {
            layer_element status = get_connection(LAYER(i,j));
            if (status == L_WIRE || status == L_VIA) {
                //found intersection
                struct ulong_size p = { .x = i, .y = j };
                mark_path(p,LAYER(i,j));
                return 1;
            }
        }
    }
    return 0;
}

int mikami(struct ulong_size S, struct ulong_size T) {
    //return 0 on success

#if 1
    assert(LAYER(S.x,S.y) == L_IO);
    assert(LAYER(T.x,T.y) == L_IO);
#else
    if (LAYER(S.x,S.y) != L_IO) {
        printf("------%d------\n",LAYER(S.x,S.y));
        assert(0);
    }
    if (LAYER(T.x,T.y) != L_IO) {
        printf("------%d------\n",LAYER(T.x,T.y));
        assert(0);
    }
#endif

    int path_found = 0;
    int loop = 0;

    while (!path_found) {
        loop++;

        if (loop == MAX_LOOP)  break;

        try_up(S);
        try_down(S);
        try_left(S);
        try_right(S);

        try_up(T);
        try_down(T);
        try_left(T);
        try_right(T);

        path_found = has_intersection();
        clean_layer();
    }

#if 1
    assert(LAYER(S.x,S.y) == L_IO);
    assert(LAYER(T.x,T.y) == L_IO);
#else
    if (LAYER(S.x,S.y) != L_IO) {
        printf("------%d------\n",LAYER(S.x,S.y));
        assert(0);
    }
    if (LAYER(T.x,T.y) != L_IO) {
        printf("------%d------\n",LAYER(T.x,T.y));
        assert(0);
    }
#endif

    if (loop == MAX_LOOP) {
        printf("max loop limit (%d) reached\n",MAX_LOOP);
        return 1;
    }
    else if (loop != 1)
        printf("#####    loop %4d\n",loop);

    return 0;
}
