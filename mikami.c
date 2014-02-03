#include <stdio.h>
#include <stdlib.h>
#include "analysis.h"
#include "toolbox.h"

extern int print_status;

#ifdef MIN
#undef MIN
#endif
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#ifdef MAX
#undef MAX
#endif
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define MIN3(a,b,c) (MIN(MIN(a,b),c))
#define MAX3(a,b,c) (MAX(MAX(a,b),c))

#define MIN4(a,b,c,d) (MIN(MIN(a,b),MIN(c,d)))
#define MAX4(a,b,c,d) (MAX(MAX(a,b),MAX(c,d)))

static layer_element *layer = NULL;
static unsigned long width = 0;
static unsigned long height = 0;

//they depend on global variables
#define LAYER(x,y) (layer[(x)*width + (y)])
#define LAYER_STATUS(x,y) (LAYER(x,y).status)
#define LAYER_TRY(x,y)    (LAYER(x,y).try)
#define LAYER_LOOP(x,y)   (LAYER(x,y).loop)

static int mikami(const struct ulong_size S, const struct ulong_size T,
                  const loop_type max_loop, const unsigned long net);
static unsigned long mikami_one_layer(struct analysis_info *soc,
                                      const loop_type max_loop);
static layer_element *create_layer(struct analysis_info *soc);

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
        fprintf(stderr,"------S %d------\n",LAYER_STATUS(S.x,S.y));
        assert(0);
        exit(EXIT_FAILURE);
    }
    if (LAYER_STATUS(T.x,T.y) != L_IO) {
        fprintf(stderr,"------T %d------\n",LAYER_STATUS(T.x,T.y));
        assert(0);
        exit(EXIT_FAILURE);
    }
#endif
}

static void reset_layer();
static layer_element *__reset_layer(struct analysis_info *soc) {
    reset_layer();
    soc->layer_num++;
    return layer;
}

static unsigned long layer_wire_length(const unsigned long via_factor) {
    unsigned long len = 0;
    unsigned long idx;
    for (idx=0; idx<width*height; ++idx) {
        if (layer[idx].status == L_WIRE)
            len++;
        if (layer[idx].status == L_VIA)
            len += 1 + via_factor;
    }
    return len;
}

unsigned long route_mikami(struct analysis_info *soc) {
    //work on last layer
    assert(soc->layer_num == 1);
    layer = soc->layer[soc->layer_num - 1];
    assert(layer);
    width = soc->grid_width;
    height = soc->grid_height;

    printf("Total %4lu nets to route ...\n",soc->pending_nets);

    const loop_type max_loop = soc->max_loop;
    const double total_nets = soc->pending_nets;
    while (1) {
        unsigned long failed_nets;
        unsigned long routed_nets;

        failed_nets = mikami_one_layer(soc,max_loop);
        routed_nets = soc->pending_nets - failed_nets;
        soc->pending_nets = failed_nets;

        if (routed_nets) {
            double wire_len = layer_wire_length(soc->layer_num) * soc->wire_size;
            soc->wire_len += wire_len;
            if (print_status >= 1)
                printf("wire layer #%03lu  %4lu nets routed (%05.2f%%), wire length %9.2f\n",
                       soc->layer_num,routed_nets,
                       routed_nets/total_nets*100,wire_len);
        }

        if (!failed_nets)
            break;
        else if (!routed_nets)
            break;

        //set global variable
        //layer = create_layer(soc);
        layer = __reset_layer(soc);
        if (!layer)
            break;
    }
    return soc->pending_nets;
}

static unsigned long mikami_one_layer(struct analysis_info *soc,
                                      const loop_type max_loop) {
    unsigned long i;
    unsigned long j;
    unsigned long net = 0;
    unsigned long failed = 0;
    for (i=0; i<soc->netlist.next; ++i) {
        struct net_info *netlist = (struct net_info *)(soc->netlist.data) + i;
        for (j=0; j<netlist->num_drain; ++j,++net) {
            if (netlist->successfully_routed[j]) {
                if (print_status >= 2)
                    printf("   skip %4lu (routed in layer %d)\n",
                           net,netlist->successfully_routed[j]);
                continue;
            }
            unsigned long next_input = netlist->drain[j]->next_free_input_slot;
            struct ulong_size S = netlist->source->output_slot.usize;
            struct ulong_size T = netlist->drain[j]->input_slots[next_input].usize;
            int error = mikami(S,T,max_loop,net);
            if (error)
                failed++;
            else {
                netlist->drain[j]->next_free_input_slot++;
                netlist->successfully_routed[j] = soc->layer_num;
            }
        }
    }
    return failed;
}

static inline int blocked(const layer_element el) {
    const unsigned char s = el.status;
    if (s == L_IO || s == L_WIRE || s == L_VIA)
        return 1;
    return 0;
}

static inline int available(const layer_element el, const loop_type loop) {
    if (el.status == L_EMPTY || (el.status == L_TRY && el.loop == loop))
        return 1;
    return 0;
}

static void try_left(struct ulong_size P, const loop_type loop,
                     const unsigned char direction) {
    unsigned long j = P.y;
    if (j == 0)
        return;
    j--;
    while (j) {
        if (blocked(LAYER(P.x,j)))
            return;
        if (!available(LAYER(P.x,j),loop))
            return;
        LAYER_STATUS(P.x,j) = L_TRY;
        LAYER_TRY(P.x,j) |= direction;
        LAYER_LOOP(P.x,j) = loop;
        j--;
    }
    //check first column
    if (blocked(LAYER(P.x,0)))
        return;
    if (!available(LAYER(P.x,0),loop))
        return;
    LAYER_STATUS(P.x,0) = L_TRY;
    LAYER_TRY(P.x,0) |= direction;
    LAYER_LOOP(P.x,0) = loop;
}

static void try_right(struct ulong_size P, const loop_type loop,
                      const unsigned char direction) {
    unsigned long j = P.y;
    j++;
    while (j < width) {
        if (blocked(LAYER(P.x,j)))
            return;
        if (!available(LAYER(P.x,j),loop))
            return;
        LAYER_STATUS(P.x,j) = L_TRY;
        LAYER_TRY(P.x,j) |= direction;
        LAYER_LOOP(P.x,j) = loop;
        j++;
    }
}

static void try_up(struct ulong_size P, const loop_type loop,
                   const unsigned char direction) {
    unsigned long i = P.x;
    if (i == 0)
        return;
    i--;
    while (i) {
        if (blocked(LAYER(i,P.y)))
            return;
        if (!available(LAYER(i,P.y),loop))
            return;
        LAYER_STATUS(i,P.y) = L_TRY;
        LAYER_TRY(i,P.y) |= direction;
        LAYER_LOOP(i,P.y) = loop;
        i--;
    }
    //check first row
    if (blocked(LAYER(0,P.y)))
        return;
    if (!available(LAYER(0,P.y),loop))
        return;
    LAYER_STATUS(0,P.y) = L_TRY;
    LAYER_TRY(0,P.y) |= direction;
    LAYER_LOOP(0,P.y) = loop;
}

static void try_down(struct ulong_size P, const loop_type loop,
                     const unsigned char direction) {
    unsigned long i = P.x;
    i++;
    while (i < height) {
        if (blocked(LAYER(i,P.y)))
            return;
        if (!available(LAYER(i,P.y),loop))
            return;
        LAYER_STATUS(i,P.y) = L_TRY;
        LAYER_TRY(i,P.y) |= direction;
        LAYER_LOOP(i,P.y) = loop;
        i++;
    }
}

/////////////////////////////////////////////////////////////
//  return L_TRY and sets *next point to mark_path()
/////////////////////////////////////////////////////////////

static int mark_left(struct ulong_size P, loop_type loop, struct ulong_size *next) {
    unsigned long j = P.y;
    if (j == 0)
        return L_INVALID;
    j--;
    while (j) {
        if (LAYER_STATUS(P.x,j) == L_IO)
            return L_IO;
        if (LAYER_LOOP(P.x,j) < loop) {
            next->x = P.x;
            next->y = j;
            return L_TRY;
        }
        LAYER_STATUS(P.x,j) = L_WIRE;
        j--;
    }
    //check first column
    if (LAYER_STATUS(P.x,0) == L_IO)
        return L_IO;
    if (LAYER_LOOP(P.x,0) < loop) {
        next->x = P.x;
        next->y = 0;
        return L_TRY;
    }
    LAYER_STATUS(P.x,0) = L_WIRE;
    return L_INVALID;
}

static int mark_right(struct ulong_size P, loop_type loop,
                      struct ulong_size *next) {
    unsigned long j = P.y;
    j++;
    while (j < width) {
        if (LAYER_STATUS(P.x,j) == L_IO)
            return L_IO;
        if (LAYER_LOOP(P.x,j) < loop) {
            next->x = P.x;
            next->y = j;
            return L_TRY;
        }
        LAYER_STATUS(P.x,j) = L_WIRE;
        j++;
    }
    return L_INVALID;
}

static int mark_up(struct ulong_size P, loop_type loop, struct ulong_size *next) {
    unsigned long i = P.x;
    if (i == 0)
        return L_INVALID;
    i--;
    while (i) {
        if (LAYER_STATUS(i,P.y) == L_IO)
            return L_IO;
        if (LAYER_LOOP(i,P.y) < loop) {
            next->x = i;
            next->y = P.y;
            return L_TRY;
        }
        LAYER_STATUS(i,P.y) = L_WIRE;
        i--;
    }
    //check first row
    if (LAYER_STATUS(0,P.y) == L_IO)
        return L_IO;
    if (LAYER_LOOP(0,P.y) < loop) {
        next->x = 0;
        next->y = P.y;
        return L_TRY;
    }
    LAYER_STATUS(0,P.y) = L_WIRE;
    return L_INVALID;
}

static int mark_down(struct ulong_size P, loop_type loop, struct ulong_size *next) {
    unsigned long i = P.x;
    i++;
    while (i < height) {
        if (LAYER_STATUS(i,P.y) == L_IO)
            return L_IO;
        if (LAYER_LOOP(i,P.y) < loop) {
            next->x = i;
            next->y = P.y;
            return L_TRY;
        }
        LAYER_STATUS(i,P.y) = L_WIRE;
        i++;
    }
    return L_INVALID;
}

static void mark_path(const struct ulong_size P, const loop_type loop,
                      const struct ulong_size S, const struct ulong_size T) {

#define RECURSION_MARK_PATH(direction)                  \
    do {                                                \
        struct ulong_size a;                            \
        if (mark_ ## direction (P,loop,&a) == L_TRY)    \
            mark_path(a,loop-1,S,T);                    \
    } while (0);

#define MARK_PATH_FAILURE_CODE                                          \
    do {                                                                \
        fprintf(stderr,"\n____status=0x%02x\ttry=0x%02x\tloop=%d\n",    \
                LAYER_STATUS(P.x,P.y),                                  \
                LAYER_TRY(P.x,P.y),                                     \
                LAYER_LOOP(P.x,P.y));                                   \
        assert(0 && "mark_path() unknown intersection!");               \
        exit(EXIT_FAILURE);                                             \
    } while (0);

    //mark the opposite directions

    assert(loop > 0);

    const unsigned char s = LAYER_TRY(P.x,P.y);

    if (LAYER_STATUS(P.x,P.y) == L_IO)
        return;

    //from source
    if (s == (S_TRY_WIRE_LEFT | T_TRY_WIRE_RIGHT)) {
        LAYER_STATUS(P.x,P.y) = L_WIRE;
        RECURSION_MARK_PATH(right);
        RECURSION_MARK_PATH(left);
    }
    else if (s == (S_TRY_WIRE_LEFT | T_TRY_WIRE_UP)) {
        LAYER_STATUS(P.x,P.y) = L_VIA;
        RECURSION_MARK_PATH(right);
        RECURSION_MARK_PATH(down);
    }
    else if (s == (S_TRY_WIRE_LEFT | T_TRY_WIRE_DOWN)) {
        LAYER_STATUS(P.x,P.y) = L_VIA;
        RECURSION_MARK_PATH(right);
        RECURSION_MARK_PATH(up);
    }
    else if (s == (S_TRY_WIRE_RIGHT | T_TRY_WIRE_UP)) {
        LAYER_STATUS(P.x,P.y) = L_VIA;
        RECURSION_MARK_PATH(left);
        RECURSION_MARK_PATH(down);
    }
    else if (s == (S_TRY_WIRE_RIGHT | T_TRY_WIRE_DOWN)) {
        LAYER_STATUS(P.x,P.y) = L_VIA;
        RECURSION_MARK_PATH(left);
        RECURSION_MARK_PATH(up);
    }
    else if (s == (S_TRY_WIRE_UP | T_TRY_WIRE_DOWN)) {
        LAYER_STATUS(P.x,P.y) = L_WIRE;
        RECURSION_MARK_PATH(down);
        RECURSION_MARK_PATH(up);
    }

    //from term
    else if (s == (T_TRY_WIRE_LEFT | S_TRY_WIRE_RIGHT)) {
        LAYER_STATUS(P.x,P.y) = L_WIRE;
        RECURSION_MARK_PATH(right);
        RECURSION_MARK_PATH(left);
    }
    else if (s == (T_TRY_WIRE_LEFT | S_TRY_WIRE_UP)) {
        LAYER_STATUS(P.x,P.y) = L_VIA;
        RECURSION_MARK_PATH(right);
        RECURSION_MARK_PATH(down);
    }
    else if (s == (T_TRY_WIRE_LEFT | S_TRY_WIRE_DOWN)) {
        LAYER_STATUS(P.x,P.y) = L_VIA;
        RECURSION_MARK_PATH(right);
        RECURSION_MARK_PATH(up);
    }
    else if (s == (T_TRY_WIRE_RIGHT | S_TRY_WIRE_UP)) {
        LAYER_STATUS(P.x,P.y) = L_VIA;
        RECURSION_MARK_PATH(left);
        RECURSION_MARK_PATH(down);
    }
    else if (s == (T_TRY_WIRE_RIGHT | S_TRY_WIRE_DOWN)) {
        LAYER_STATUS(P.x,P.y) = L_VIA;
        RECURSION_MARK_PATH(left);
        RECURSION_MARK_PATH(up);
    }
    else if (s == (T_TRY_WIRE_UP | S_TRY_WIRE_DOWN)) {
        LAYER_STATUS(P.x,P.y) = L_WIRE;
        RECURSION_MARK_PATH(down);
        RECURSION_MARK_PATH(up);
    }

    //mid-points with one possible direction
    else if (s == S_TRY_WIRE_LEFT || s == T_TRY_WIRE_LEFT) {
        LAYER_STATUS(P.x,P.y) = L_VIA;
        RECURSION_MARK_PATH(right);
    }
    else if (s == S_TRY_WIRE_RIGHT || s == T_TRY_WIRE_RIGHT) {
        LAYER_STATUS(P.x,P.y) = L_VIA;
        RECURSION_MARK_PATH(left);
    }
    else if (s == S_TRY_WIRE_UP || s == T_TRY_WIRE_UP) {
        LAYER_STATUS(P.x,P.y) = L_VIA;
        RECURSION_MARK_PATH(down);
    }
    else if (s == S_TRY_WIRE_DOWN || s == T_TRY_WIRE_DOWN) {
        LAYER_STATUS(P.x,P.y) = L_VIA;
        RECURSION_MARK_PATH(up);
    }

    //mid-points with multiple directions
    else if (s & S_TRY_ANY) {
        unsigned long invalid = MAX(width,height);
        unsigned long l = (P.x <= S.x) ? P.x - S.x : invalid;
        unsigned long r = (P.x >  S.x) ? S.x - P.x : invalid;
        unsigned long u = (P.y <= S.y) ? S.y - P.y : invalid;
        unsigned long d = (P.y >  S.y) ? P.y - S.y : invalid;

        if (!(LAYER_TRY(P.x,P.y) & S_TRY_WIRE_LEFT))
            l = invalid;
        if (!(LAYER_TRY(P.x,P.y) & S_TRY_WIRE_RIGHT))
            r = invalid;
        if (!(LAYER_TRY(P.x,P.y) & S_TRY_WIRE_UP))
            u = invalid;
        if (!(LAYER_TRY(P.x,P.y) & S_TRY_WIRE_DOWN))
            d = invalid;

        LAYER_STATUS(P.x,P.y) = L_VIA;
        if (MIN4(l,r,u,d) == l) {
            RECURSION_MARK_PATH(right);
        }
        else if (MIN3(r,u,d) == r) {
            RECURSION_MARK_PATH(left);
        }
        else if (MIN(u,d) == u) {
            RECURSION_MARK_PATH(down);
        }
        else if (d != invalid) {
            RECURSION_MARK_PATH(up);
        }
        else
            MARK_PATH_FAILURE_CODE;
    }
    else if (s & T_TRY_ANY) {
        unsigned long invalid = MAX(width,height);
        unsigned long l = (P.x <= T.x) ? P.x - T.x : width;
        unsigned long r = (P.x >  T.x) ? T.x - P.x : width;
        unsigned long u = (P.y <= T.y) ? T.y - P.y : height;
        unsigned long d = (P.y >  T.y) ? P.y - T.y : height;

        if (!(LAYER_TRY(P.x,P.y) & T_TRY_WIRE_LEFT))
            l = invalid;
        if (!(LAYER_TRY(P.x,P.y) & T_TRY_WIRE_RIGHT))
            r = invalid;
        if (!(LAYER_TRY(P.x,P.y) & T_TRY_WIRE_UP))
            u = invalid;
        if (!(LAYER_TRY(P.x,P.y) & T_TRY_WIRE_DOWN))
            d = invalid;

        LAYER_STATUS(P.x,P.y) = L_VIA;
        if (MIN4(l,r,u,d) == l) {
            RECURSION_MARK_PATH(right);
        }
        else if (MIN3(r,u,d) == r) {
            RECURSION_MARK_PATH(left);
        }
        else if (MIN(u,d) == u) {
            RECURSION_MARK_PATH(down);
        }
        else if (d != invalid) {
            RECURSION_MARK_PATH(up);
        }
        else
            MARK_PATH_FAILURE_CODE;
    }

    //unknown case
    else {
        MARK_PATH_FAILURE_CODE;
    }

#undef RECURSION_MARK_PATH
#undef MARK_PATH_FAILURE_CODE
}

static inline unsigned char get_connection(const layer_element el) {
    const unsigned char s = el.try;

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

    return el.status;
}

static int has_intersection(const loop_type loop,
                            const struct ulong_size S, const struct ulong_size T) {
    unsigned long i;
    unsigned long j;

    //search first inside the rectangle
    unsigned long uprow = MIN(S.x,T.x);
    unsigned long downrow = MAX(S.x,T.x);
    unsigned long leftcol = MIN(S.y,T.y);
    unsigned long rightcol = MAX(S.y,T.y);

    for (i=uprow; i<downrow; ++i) {
        for (j=leftcol; j<rightcol; ++j) {
            if (LAYER_STATUS(i,j) != L_TRY)
                continue;
            if (LAYER_LOOP(i,j) != loop)
                continue;
            const unsigned char status = get_connection(LAYER(i,j));
            if (status == L_WIRE || status == L_VIA) {
                //found intersection
                struct ulong_size p = { .x = i, .y = j };
                mark_path(p,loop,S,T);
                return 2;
            }
        }
    }

    for (i=0; i<height; ++i) {
        for (j=0; j<width; ++j) {
            if (LAYER_STATUS(i,j) != L_TRY)
                continue;
            if (LAYER_LOOP(i,j) != loop)
                continue;
            const unsigned char status = get_connection(LAYER(i,j));
            if (status == L_WIRE || status == L_VIA) {
                //found intersection
                struct ulong_size p = { .x = i, .y = j };
                mark_path(p,loop,S,T);
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
    for (i=0; i<height; ++i)
        for (j=0; j<width; ++j)
            if (LAYER_STATUS(i,j) == L_TRY)
                clear_layer_element(i,j);
}

static void reset_layer() {
    unsigned long i;
    unsigned long j;
    for (i=0; i<height; ++i)
        for (j=0; j<width; ++j)
            if (LAYER_STATUS(i,j) != L_IO)
                clear_layer_element(i,j);
}

static void expand_source(const struct ulong_size S, const loop_type loop) {
    try_up   (S,loop,S_TRY_WIRE_UP);
    try_down (S,loop,S_TRY_WIRE_DOWN);
    try_left (S,loop,S_TRY_WIRE_LEFT);
    try_right(S,loop,S_TRY_WIRE_RIGHT);
}

static void expand_term(const struct ulong_size T, const loop_type loop) {
    try_up   (T,loop,T_TRY_WIRE_UP);
    try_down (T,loop,T_TRY_WIRE_DOWN);
    try_left (T,loop,T_TRY_WIRE_LEFT);
    try_right(T,loop,T_TRY_WIRE_RIGHT);
}

static void expand( struct ulong_size P, const loop_type loop) {
    //from source
    if (LAYER_TRY(P.x,P.y) & S_TRY_HORIZONTAL) {
        try_up  (P,loop,S_TRY_WIRE_UP);
        try_down(P,loop,S_TRY_WIRE_DOWN);
    }
    else if (LAYER_TRY(P.x,P.y) & S_TRY_VERTICAL) {
        try_left(P,loop,S_TRY_WIRE_LEFT);
        try_right(P,loop,S_TRY_WIRE_RIGHT);
    }

    //from term
    else if (LAYER_TRY(P.x,P.y) & T_TRY_HORIZONTAL) {
        try_up  (P,loop,T_TRY_WIRE_UP);
        try_down(P,loop,T_TRY_WIRE_DOWN);
    }
    else if (LAYER_TRY(P.x,P.y) & T_TRY_VERTICAL) {
        try_left(P,loop,T_TRY_WIRE_LEFT);
        try_right(P,loop,T_TRY_WIRE_RIGHT);
    }
    else {
        fprintf(stderr,"\n____status=%d\ttry=%d\tloop=%d\n",
                LAYER_STATUS(P.x,P.y),
                LAYER_TRY(P.x,P.y),
                LAYER_LOOP(P.x,P.y));
        assert(0);
        exit(EXIT_FAILURE);
    }
}

static int try_again(const loop_type loop) {
    assert(loop > 1);

    unsigned long i;
    unsigned long j;
    for (i=0; i<height; ++i) {
        for (j=0; j<width; ++j) {
            if (LAYER_STATUS(i,j) == L_TRY && LAYER_LOOP(i,j) == loop-1) {
                struct ulong_size P = { .x = i, .y = j };
                expand(P,loop);
            }
        }
    }
#if 0
    unsigned long empty = 0;
    for (i=0; i<height; ++i)
        for (j=0; j<width; ++j)
            if (LAYER_STATUS(i,j) == L_EMPTY)
                empty++;
    return empty == 0;
#else
    return 0;
#endif
}

static int mikami(const struct ulong_size S, const struct ulong_size T,
                  const loop_type max_loop, const unsigned long net) {
    //return 0 on success

    if (!max_loop) {
        if (print_status >= 3)
            fprintf(stderr,"in mikami() call: no max_loop limit, fail\n");
        return 1;
    }

    assure_io(S,T);

    loop_type loop = 1;

    expand_source(S,loop);
    expand_term(T,loop);

    int path_found = has_intersection(loop,S,T);
    int full = 0;
    while (!path_found && !full) {
        if (loop == max_loop)  break;
        loop++;
        full = try_again(loop);
        path_found = has_intersection(loop,S,T);
    }
    clean_layer();

    assure_io(S,T);

    //LAYER(S.x,S.y).status = L_START;
    //LAYER(T.x,T.y).status = L_TERM;

    if (print_status >= 2) {
        printf("net     %4lu ",net);
        if (path_found == 1)
            //if (loop != 1)
            printf("routed: loop %4d\n",loop);
        else if (path_found == 2)
            printf("routed: loop %4d (fast path)\n",loop);
        else if (loop == max_loop)
            printf("failed: max loop limit (%d) reached\n",max_loop);
        else if (full)
            printf("failed: layer is full (loop=%d)\n",loop);
    }
    return path_found ? 0 : 1;
}

static layer_element *create_layer(struct analysis_info *soc) {
    assert(soc);

    if (soc->layer_num == MAX_LAYERS) {
        printf("Cannot create more layers (MAX_LAYERS = %d)\n",MAX_LAYERS);
        return NULL;
    }

    layer_element *new_layer = NULL;
    new_layer = (layer_element *)_calloc(soc->grid_width * soc->grid_height,
                                         sizeof(layer_element));

    assert(soc->layer_num > 0);
    unsigned long i;
    unsigned long j;
    for (i=0; i<height; ++i) {
        for (j=0; j<width; ++j) {
            unsigned long idx = i*soc->grid_width + j;
            if (LAYER_STATUS(i,j) == L_IO)
                new_layer[idx].status = L_IO;
        }
    }

    soc->layer[soc->layer_num++] = new_layer;
    return new_layer;
}
