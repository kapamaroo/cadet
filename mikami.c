#include <stdio.h>
#include "analysis.h"

static layer_element *layer = NULL;
static unsigned long width = 0;
static unsigned long height = 0;

//they depend on global variables
#define LAYER(x,y) (layer[(x)*width + (y)])
#define LAYER_STATUS(x,y) (LAYER(x,y).status)
#define LAYER_TRY(x,y)    (LAYER(x,y).try)
#define LAYER_LOOP(x,y)   (LAYER(x,y).loop)

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

unsigned long route_mikami(struct analysis_info *soc, const double wire_size) {
    layer = soc->layer;
    width = soc->grid_width;
    height = soc->grid_height;

    unsigned long i;
    unsigned long j;
    unsigned long net = 0;
    unsigned long failed = 0;

    for (i=0; i<soc->netlist.next; ++i) {
        struct net_info *netlist = (struct net_info *)(soc->netlist.data) + i;
        for (j=0; j<netlist->num_drain; ++j) {
            net++;
            //printf("__________    routing net %4lu ...\n",net);
            unsigned long next_input = netlist->drain[j]->next_free_input_slot++;
            int error = mikami(netlist->source->output_slot.usize,
                               netlist->drain[j]->input_slots[next_input].usize);
            if (error) {
                failed++;
                //return failed;  //abort routing
                fprintf(stderr,"netlist %lu failed\n",net);
                break;            //ignore netlist
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

static inline int available(const layer_element el, const unsigned char loop) {
    if (el.status == L_EMPTY || (el.status == L_TRY && el.loop == loop))
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

static void try_right(struct ulong_size P, const unsigned char loop,
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

static void try_up(struct ulong_size P, const unsigned char loop,
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

static void try_down(struct ulong_size P, const unsigned char loop,
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

static int mark_left(struct ulong_size P, unsigned char loop, struct ulong_size *next) {
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

static int mark_right(struct ulong_size P, unsigned char loop, struct ulong_size *next) {
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

static int mark_up(struct ulong_size P, unsigned char loop, struct ulong_size *next) {
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

static int mark_down(struct ulong_size P, unsigned char loop, struct ulong_size *next) {
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

static void mark_path(const struct ulong_size p, const unsigned char loop) {
#define RECURSION_MARK_PATH(dir1,dir2)          \
    {                                           \
        struct ulong_size a;                    \
        if (mark_ ## dir1 (p,loop,&a) == L_TRY) \
            mark_path(a,loop-1);                \
        if (mark_ ## dir2 (p,loop,&a) == L_TRY) \
            mark_path(a,loop-1);                \
    }

    //mark the opposite directions

    assert(loop > 0);

    const unsigned char s = LAYER_TRY(p.x,p.y);

    if (LAYER_STATUS(p.x,p.y) == L_IO)
        return;

    //from source
    if (s == (S_TRY_WIRE_LEFT | T_TRY_WIRE_RIGHT)) {
        LAYER_STATUS(p.x,p.y) = L_WIRE;
        RECURSION_MARK_PATH(right,left);
    }
    else if (s == (S_TRY_WIRE_LEFT | T_TRY_WIRE_UP)) {
        LAYER_STATUS(p.x,p.y) = L_VIA;
        RECURSION_MARK_PATH(right,down);
    }
    else if (s == (S_TRY_WIRE_LEFT | T_TRY_WIRE_DOWN)) {
        LAYER_STATUS(p.x,p.y) = L_VIA;
        RECURSION_MARK_PATH(right,up);
    }
    else if (s == (S_TRY_WIRE_RIGHT | T_TRY_WIRE_UP)) {
        LAYER_STATUS(p.x,p.y) = L_VIA;
        RECURSION_MARK_PATH(left,down);
    }
    else if (s == (S_TRY_WIRE_RIGHT | T_TRY_WIRE_DOWN)) {
        LAYER_STATUS(p.x,p.y) = L_VIA;
        RECURSION_MARK_PATH(left,up);
    }
    else if (s == (S_TRY_WIRE_UP | T_TRY_WIRE_DOWN)) {
        LAYER_STATUS(p.x,p.y) = L_WIRE;
        RECURSION_MARK_PATH(down,up);
    }

    //from term
    else if (s == (T_TRY_WIRE_LEFT | S_TRY_WIRE_RIGHT)) {
        LAYER_STATUS(p.x,p.y) = L_WIRE;
        RECURSION_MARK_PATH(right,left);
    }
    else if (s == (T_TRY_WIRE_LEFT | S_TRY_WIRE_UP)) {
        LAYER_STATUS(p.x,p.y) = L_VIA;
        RECURSION_MARK_PATH(right,down);
    }
    else if (s == (T_TRY_WIRE_LEFT | S_TRY_WIRE_DOWN)) {
        LAYER_STATUS(p.x,p.y) = L_VIA;
        RECURSION_MARK_PATH(right,up);
    }
    else if (s == (T_TRY_WIRE_RIGHT | S_TRY_WIRE_UP)) {
        LAYER_STATUS(p.x,p.y) = L_VIA;
        RECURSION_MARK_PATH(left,down);
    }
    else if (s == (T_TRY_WIRE_RIGHT | S_TRY_WIRE_DOWN)) {
        LAYER_STATUS(p.x,p.y) = L_VIA;
        RECURSION_MARK_PATH(left,up);
    }
    else if (s == (T_TRY_WIRE_UP | S_TRY_WIRE_DOWN)) {
        LAYER_STATUS(p.x,p.y) = L_WIRE;
        RECURSION_MARK_PATH(down,up);
    }
    else {
        struct ulong_size a;
        switch (LAYER_TRY(p.x,p.y)) {
            //from source
        case S_TRY_WIRE_LEFT:
        case T_TRY_WIRE_LEFT:
            LAYER_STATUS(p.x,p.y) = L_VIA;
            if (mark_right(p,loop,&a) == L_TRY)
                mark_path(a,loop-1);
            break;
        case S_TRY_WIRE_RIGHT:
        case T_TRY_WIRE_RIGHT:
            LAYER_STATUS(p.x,p.y) = L_VIA;
            if (mark_left(p,loop,&a) == L_TRY)
                mark_path(a,loop-1);
            break;
        case S_TRY_WIRE_UP:
        case T_TRY_WIRE_UP:
            LAYER_STATUS(p.x,p.y) = L_VIA;
            if (mark_down(p,loop,&a) == L_TRY)
                mark_path(a,loop-1);
            break;
        case S_TRY_WIRE_DOWN:
        case T_TRY_WIRE_DOWN:
            LAYER_STATUS(p.x,p.y) = L_VIA;
            if (mark_up(p,loop,&a) == L_TRY)
                mark_path(a,loop-1);
            break;
        default:
#if 1
            printf("\n_____________________status=%d\ttry=%d\tloop=%d\n",
                   LAYER_STATUS(p.x,p.y),
                   LAYER_TRY(p.x,p.y),
                   LAYER_LOOP(p.x,p.y));
            assert(0 && "mark_path() has been called without intersection!");
#endif
        }
    }

#undef RECURSION_MARK_PATH
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

static int has_intersection(const unsigned char loop) {
    unsigned long i;
    unsigned long j;
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
                mark_path(p,loop);
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

static void expand( struct ulong_size P, const unsigned char loop) {
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
        printf("\n_____________________status=%d\ttry=%d\tloop=%d\n",
               LAYER_STATUS(P.x,P.y),
               LAYER_TRY(P.x,P.y),
               LAYER_LOOP(P.x,P.y));
        assert(0);
    }
}

static int try_again(const unsigned char loop) {
    assert(loop > 1);

    unsigned long i;
    unsigned long j;
    unsigned long empty = 0;
    for (i=0; i<height; ++i) {
        for (j=0; j<width; ++j) {
            if (LAYER_STATUS(i,j) == L_EMPTY)
                empty++;
            if (LAYER_STATUS(i,j) == L_TRY && LAYER_LOOP(i,j) == loop-1) {
                struct ulong_size P = { .x = i, .y = j };
                expand(P,loop);
            }
        }
    }
    return empty == 0;
}

static int mikami(const struct ulong_size S, const struct ulong_size T) {
    //return 0 on success

    assure_io(S,T);

    const unsigned char max_loop = 254;
    unsigned char loop = 1;

    expand_source(S,loop);
    expand_term(T,loop);

    int path_found = has_intersection(loop);
    int full = 0;
    while (!path_found && !full) {
        if (loop == max_loop)  break;
        loop++;
        full = try_again(loop);
        path_found = has_intersection(loop);
    }
    clean_layer();

    assure_io(S,T);

    //LAYER(S.x,S.y).status = L_START;
    //LAYER(T.x,T.y).status = L_TERM;

    if (loop == max_loop) {
        fprintf(stderr,"max loop limit (%d) reached\n",max_loop);
        return 1;
    }
    else if (full) {
        fprintf(stderr,"layer is full (loop=%d)\n",loop);
        return 1;
    }
#if 0
    else if (loop != 1)
        fprintf(stderr,"#####    loop %4d\n",loop);
#endif

    return 0;
}
