#include <stdio.h>
#include "analysis.h"

static layer_element *layer = NULL;
static unsigned long width = 0;
static unsigned long height = 0;

//they depend on global variables
#define LAYER(x,y) (layer[(x)*width + (y)])
#define LAYER_STATUS(x,y) (layer[(x)*width + (y)].status)
#define LAYER_TRY(x,y) (layer[(x)*width + (y)].try)
#define LAYER_LOOP(x,y) (layer[(x)*width + (y)].loop)

static int mikami(const struct ulong_size S, const struct ulong_size T);

static void assure_io(const struct ulong_size S, const struct ulong_size T) {
#if 1
    assert(LAYER_STATUS(S.x,S.y) == L_IO);
    assert(LAYER_STATUS(T.x,T.y) == L_IO);
    assert(LAYER_TRY(S.x,S.y) == TRY_EMPTY);
    assert(LAYER_TRY(T.x,T.y) == TRY_EMPTY);
    assert(LAYER_LOOP(S.x,S.y) == 0);
    assert(LAYER_LOOP(T.x,T.y) == 0);
#else
    if (LAYER_STATUS(S.x,S.y) != L_IO) {
        printf("------%d------\n",LAYER_STATUS(S.x,S.y));
        assert(0);
    }
    if (LAYER_STATUS(T.x,T.y) != L_IO) {
        printf("------%d------\n",LAYER_STATUS(T.x,T.y));
        assert(0);
    }
#endif
}

int route_mikami(struct analysis_info *soc, const double wire_size) {
    layer = soc->layer;
    width = soc->grid_width;
    height = soc->grid_height;

    unsigned long i;
    unsigned long j;
    unsigned long net = 0;
    int status = 0;

    for (i=0; i<soc->netlist.next; ++i) {
        struct net_info *netlist = (struct net_info *)(soc->netlist.data) + i;
        for (j=0; j<netlist->num_drain; ++j) {
            net++;
            //printf("__________    routing net %4lu ...\n",net);
            unsigned long next_input = netlist->drain[j]->next_free_input_slot++;
            int error = mikami(netlist->source->output_slot.usize,
                               netlist->drain[j]->input_slots[next_input].usize);
            if (error) {
                status++;
                //return status;  //abort routing
                break;            //ignore netlist
            }
        }
    }
    return status;
}

static inline int blocked(const layer_element el) {
    const unsigned char s = el.status;
    if (s == L_IO || s == L_WIRE || s == L_VIA)
        return 1;
    return 0;
}

static void try_left(struct ulong_size P, const unsigned char loop,
                     const unsigned char direction) {
    unsigned long j = P.y;
    if (j == 0)
        return;
    j--;
    while (j) {
        if (blocked(LAYER(P.x,j)))
            return;
        LAYER_STATUS(P.x,j) = L_TRY;
        LAYER_TRY(P.x,j) |= direction;
        LAYER_LOOP(P.x,j) = loop;
        j--;
    }
    //check first column
    if (blocked(LAYER(P.x,0)))
        return;
    LAYER_STATUS(P.x,0) = L_TRY;
    LAYER_TRY(P.x,0) |= direction;
    LAYER_LOOP(P.x,0) = loop;
}

static void try_right(struct ulong_size P, const unsigned char loop,
                      const unsigned char direction) {
    unsigned long j = P.y;
    j++;
    while (j < width) {
        if (blocked(LAYER(P.x,j)))
            return;
        LAYER_STATUS(P.x,j) = L_TRY;
        LAYER_TRY(P.x,j) |= direction;
        LAYER_LOOP(P.x,j) = loop;
        j++;
    }
}

static void try_up(struct ulong_size P, const unsigned char loop,
                   const unsigned char direction) {
    unsigned long i = P.x;
    if (i == 0)
        return;
    i--;
    while (i) {
        if (blocked(LAYER(i,P.y)))
            return;
        LAYER_STATUS(i,P.y) = L_TRY;
        LAYER_TRY(i,P.y) |= direction;
        LAYER_LOOP(i,P.y) = loop;
        i--;
    }
    //check first row
    if (blocked(LAYER(0,P.y)))
        return;
    LAYER_STATUS(0,P.y) = L_TRY;
    LAYER_TRY(0,P.y) |= direction;
    LAYER_LOOP(0,P.y) = loop;
}

static void try_down(struct ulong_size P, const unsigned char loop,
                     const unsigned char direction) {
    unsigned long i = P.x;
    i++;
    while (i < height) {
        if (blocked(LAYER(i,P.y)))
            return;
        LAYER_STATUS(i,P.y) = L_TRY;
        LAYER_TRY(i,P.y) |= direction;
        LAYER_LOOP(i,P.y) = loop;
        i++;
    }
}

/////////////////////////////////////////////////////////////
static void mark_left(struct ulong_size P) {
    unsigned long j = P.y;
    if (j == 0)
        return;
    j--;
    while (j) {
        if (LAYER_STATUS(P.x,j) == L_IO)
            return;
        LAYER_STATUS(P.x,j) = L_WIRE;
        j--;
    }
    //check first column
    if (LAYER_STATUS(P.x,0) == L_IO)
        return;
    LAYER_STATUS(P.x,0) = L_WIRE;
}

static void mark_right(struct ulong_size P) {
    unsigned long j = P.y;
    j++;
    while (j < width) {
        if (LAYER_STATUS(P.x,j) == L_IO)
            return;
        LAYER_STATUS(P.x,j) = L_WIRE;
        j++;
    }
}

static void mark_up(struct ulong_size P) {
    unsigned long i = P.x;
    if (i == 0)
        return;
    i--;
    while (i) {
        if (LAYER_STATUS(i,P.y) == L_IO)
            return;
        LAYER_STATUS(i,P.y) = L_WIRE;
        i--;
    }
    //check first row
    if (LAYER_STATUS(0,P.y) == L_IO)
        return;
    LAYER_STATUS(0,P.y) = L_WIRE;
}

static void mark_down(struct ulong_size P) {
    unsigned long i = P.x;
    i++;
    while (i < height) {
        if (LAYER_STATUS(i,P.y) == L_IO)
            return;
        LAYER_STATUS(i,P.y) = L_WIRE;
        i++;
    }
}

static inline unsigned char get_connection(const layer_element el) {
    const unsigned char s = el.status;

    //from source
    if (s == (S_TRY_WIRE_LEFT | T_TRY_WIRE_RIGHT))    return L_WIRE;

    if (s == (S_TRY_WIRE_LEFT | T_TRY_WIRE_UP))       return L_VIA;
    if (s == (S_TRY_WIRE_LEFT | T_TRY_WIRE_DOWN))     return L_VIA;

    if (s == (S_TRY_WIRE_RIGHT | T_TRY_WIRE_UP))      return L_VIA;
    if (s == (S_TRY_WIRE_RIGHT | T_TRY_WIRE_DOWN))    return L_VIA;

    if (s == (S_TRY_WIRE_UP | T_TRY_WIRE_DOWN))       return L_WIRE;


    //from term
    if (s == (T_TRY_WIRE_LEFT | S_TRY_WIRE_RIGHT))    return L_WIRE;

    if (s == (T_TRY_WIRE_LEFT | S_TRY_WIRE_UP))       return L_VIA;
    if (s == (T_TRY_WIRE_LEFT | S_TRY_WIRE_DOWN))     return L_VIA;

    if (s == (T_TRY_WIRE_RIGHT | S_TRY_WIRE_UP))      return L_VIA;
    if (s == (T_TRY_WIRE_RIGHT | S_TRY_WIRE_DOWN))    return L_VIA;

    if (s == (T_TRY_WIRE_UP | S_TRY_WIRE_DOWN))       return L_WIRE;

    return s;
}

static void mark_path(const struct ulong_size p, const layer_element el) {
    //mark the opposite directions

    const unsigned char s = LAYER_STATUS(p.x,p.y);

    //from source
    if (s == (S_TRY_WIRE_LEFT | T_TRY_WIRE_RIGHT)) {
        LAYER_STATUS(p.x,p.y) = L_WIRE;
        mark_right(p);
        mark_left(p);
    }
    else if (s == (S_TRY_WIRE_LEFT | T_TRY_WIRE_UP)) {
        LAYER_STATUS(p.x,p.y) = L_VIA;
        mark_right(p);
        mark_down(p);
    }
    else if (s == (S_TRY_WIRE_LEFT | T_TRY_WIRE_DOWN)) {
        LAYER_STATUS(p.x,p.y) = L_VIA;
        mark_right(p);
        mark_up(p);
    }
    else if (s == (S_TRY_WIRE_RIGHT | T_TRY_WIRE_UP)) {
        LAYER_STATUS(p.x,p.y) = L_VIA;
        mark_left(p);
        mark_down(p);
    }
    else if (s == (S_TRY_WIRE_RIGHT | T_TRY_WIRE_DOWN)) {
        LAYER_STATUS(p.x,p.y) = L_VIA;
        mark_right(p);
        mark_down(p);
    }
    else if (s == (S_TRY_WIRE_UP | T_TRY_WIRE_DOWN)) {
        LAYER_STATUS(p.x,p.y) = L_WIRE;
        mark_down(p);
        mark_up(p);
    }

    else

    //from term
    if (s == (T_TRY_WIRE_LEFT | S_TRY_WIRE_RIGHT)) {
        LAYER_STATUS(p.x,p.y) = L_WIRE;
        mark_right(p);
        mark_left(p);
    }
    else if (s == (T_TRY_WIRE_LEFT | S_TRY_WIRE_UP)) {
        LAYER_STATUS(p.x,p.y) = L_VIA;
        mark_right(p);
        mark_down(p);
    }
    else if (s == (T_TRY_WIRE_LEFT | S_TRY_WIRE_DOWN)) {
        LAYER_STATUS(p.x,p.y) = L_VIA;
        mark_right(p);
        mark_up(p);
    }
    else if (s == (T_TRY_WIRE_RIGHT | S_TRY_WIRE_UP)) {
        LAYER_STATUS(p.x,p.y) = L_VIA;
        mark_left(p);
        mark_down(p);
    }
    else if (s == (T_TRY_WIRE_RIGHT | S_TRY_WIRE_DOWN)) {
        LAYER_STATUS(p.x,p.y) = L_VIA;
        mark_right(p);
        mark_down(p);
    }
    else if (s == (T_TRY_WIRE_UP | S_TRY_WIRE_DOWN)) {
        LAYER_STATUS(p.x,p.y) = L_WIRE;
        mark_down(p);
        mark_up(p);
    }

    assert(0 && "mark_path() has been called without intersection!");
}

static int has_intersection() {
    unsigned long i;
    unsigned long j;
    for (i=0; i<width; ++i) {
        for (j=0; j<height; ++j) {
            if (LAYER_STATUS(i,j) != L_TRY)
                continue;
            const unsigned char status = get_connection(LAYER(i,j));
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

static inline void clear_layer_element(const unsigned long x,
                                       const unsigned long y) {
    LAYER_STATUS(x,y) = L_EMPTY;
    LAYER_TRY(x,y) = TRY_EMPTY;
    LAYER_LOOP(x,y) = 0;
}

static void clean_layer() {
    unsigned long i;
    unsigned long j;
    for (i=0; i<width; ++i)
        for (j=0; j<height; ++j)
            if (LAYER_STATUS(i,j) == L_TRY)
                clear_layer_element(i,j);
}

static void expand_source(const struct ulong_size S, const unsigned char loop) {
    try_up   (S,loop,S_TRY_WIRE_UP);
    try_down (S,loop,S_TRY_WIRE_DOWN);
    try_left (S,loop,S_TRY_WIRE_LEFT);
    try_right(S,loop,S_TRY_WIRE_RIGHT);
}

static void expand_term(const struct ulong_size T, const unsigned char loop) {
    try_up   (T,loop,T_TRY_WIRE_UP);
    try_down (T,loop,T_TRY_WIRE_DOWN);
    try_left (T,loop,T_TRY_WIRE_LEFT);
    try_right(T,loop,T_TRY_WIRE_RIGHT);
}

static void try_again(const unsigned char loop) {
    //implement me!
}

static int mikami(const struct ulong_size S, const struct ulong_size T) {
    //return 0 on success

    assure_io(S,T);

    const unsigned char max_loop = 2;
    unsigned char loop = 1;

    expand_source(S,loop);
    expand_term(T,loop);

    int path_found = has_intersection();
    while (!path_found) {
        if (loop == max_loop)  break;
        try_again(loop);
        path_found = has_intersection();
        loop++;
    }
    clean_layer();

    assure_io(S,T);

    if (loop == max_loop) {
        printf("max loop limit (%d) reached\n",max_loop);
        return 1;
    }
#if 0
    else if (loop != 1)
        printf("#####    loop %4d\n",loop);
#endif

    return 0;
}
