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
#define LAYER_TRY(x,y)    (LAYER(x,y).try)
#define LAYER_STATUS(x,y) ((LAYER(x,y)).loop_status)

static inline unsigned char GET_LAYER_STATUS(unsigned long x, unsigned long y) {
    const unsigned char s = LAYER_STATUS(x,y);
    //if (s & L_TRY)
    //    return L_TRY;
    return s & ((1 << NON_LOOP_BITS) - 1);
}

static inline unsigned char GET_LAYER_LOOP(unsigned long x, unsigned long y) {
    const unsigned char s = LAYER_STATUS(x,y);
    if (s & L_TRY)
        return s >> NON_LOOP_BITS;
    assert(s & L_TRY);
    return 0;
}

static inline void SET_LAYER_LOOP_TRY(unsigned long x, unsigned long y,
                                      unsigned char loop) {
    assert(loop >> LOOP_BITS == 0 && "loop overflow");
    //clean previous loop
    //auto mark as L_TRY
    //keep other status bits
    unsigned char status_bits = LAYER_STATUS(x,y) & ((1 << NON_LOOP_BITS) - 1);
    status_bits |= L_TRY;
    LAYER_STATUS(x,y) = (loop << NON_LOOP_BITS) | status_bits;
}

static void clean_layer_after_net() {
    unsigned long i;
    unsigned long j;
    for (i=0; i<height; ++i)
        for (j=0; j<width; ++j) {
            LAYER_STATUS(i,j) &= (L_WIRE | L_VIA | L_IO);
            LAYER_TRY(i,j) = TRY_EMPTY;
        }
}

static void clean_layer_after_path() {
    unsigned long i;
    unsigned long j;
    for (i=0; i<height; ++i)
        for (j=0; j<width; ++j)
            if (LAYER_STATUS(i,j) & (L_WIRE | L_VIA | L_IO)) {
                if (LAYER_STATUS(i,j) & L_TRY) {
                    SET_LAYER_LOOP_TRY(i,j,0);  //mark them as no loop
                    //LAYER_TRY(i,j) = TRY_EMPTY;
                }
            }
            else {
                LAYER_STATUS(i,j) = L_EMPTY;
                LAYER_TRY(i,j) = TRY_EMPTY;
            }
}

static void reset_layer() {
    unsigned long i;
    unsigned long j;
    for (i=0; i<height; ++i)
        for (j=0; j<width; ++j) {
            LAYER_STATUS(i,j) &= L_IO;
            LAYER_TRY(i,j) = TRY_EMPTY;
        }
}

static int mikami(const struct ulong_size S, const struct ulong_size T,
                  const loop_type max_loop, const unsigned long net);
static unsigned long mikami_one_layer(struct pool_info *netlist_pool,
                                      const loop_type max_loop,
                                      const int layer_num);
static unsigned long mikami_one_net(struct net_info *net,
                                    const loop_type max_loop,
                                    const int layer_num,
                                    const unsigned long idx);
static layer_element *create_layer(struct analysis_info *soc);
static void quicksort(struct net_info *a, unsigned long l, unsigned long r);

static void assure_io(const struct ulong_size S, const struct ulong_size T) {
#if 1
    assert(GET_LAYER_STATUS(S.x,S.y) == L_IO);
    assert(GET_LAYER_STATUS(T.x,T.y) == L_IO);
    assert(LAYER_TRY(S.x,S.y) == TRY_EMPTY);
    assert(LAYER_TRY(T.x,T.y) == TRY_EMPTY);
#else
    if (GET_LAYER_STATUS(S.x,S.y) != L_IO) {
        fprintf(stderr,"------S %d------\n",GET_LAYER_STATUS(S.x,S.y));
        assert(0);
        exit(EXIT_FAILURE);
    }
    if (GET_LAYER_STATUS(T.x,T.y) != L_IO) {
        fprintf(stderr,"------T %d------\n",GET_LAYER_STATUS(T.x,T.y));
        assert(0);
        exit(EXIT_FAILURE);
    }
#endif
}

static inline layer_element *__reset_layer(struct analysis_info *soc) {
    reset_layer();
    soc->layer_num++;
    return layer;
}

static unsigned long layer_wire_length(const unsigned long via_factor) {
    unsigned long len = 0;
    unsigned long idx;
    for (idx=0; idx<width*height; ++idx) {
        if (layer[idx].loop_status == L_WIRE)
            len++;
        if (layer[idx].loop_status == L_VIA)
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

    assert(L_TRY == 1);

    const loop_type max_loop = soc->max_loop;
    const double total_nets = soc->pending_nets;
    while (layer) {
        unsigned long failed_nets;
        unsigned long routed_nets;

        struct pool_info *netlist = &soc->netlist;
        quicksort((struct net_info *)(netlist->data),0,netlist->next);

        failed_nets = mikami_one_layer(netlist,max_loop,soc->layer_num);
        routed_nets = soc->pending_nets - failed_nets;
        soc->pending_nets = failed_nets;

        if (routed_nets) {
            double wire_len = layer_wire_length(soc->layer_num) * soc->wire_size;
            soc->wire_len += wire_len;
            if (print_status >= 1)
                printf("wire layer #%03d  %4lu nets routed (%05.2f%%), wire length %9.2f\n",
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
    }
    return soc->pending_nets;
}

static unsigned long mikami_one_layer(struct pool_info *netlist_pool,
                                      const loop_type max_loop,
                                      const int layer_num) {
    unsigned long i;
    unsigned long failed = 0;
    unsigned long limit = netlist_pool->next;

    //backward
    for (i=0; i<netlist_pool->next; ++i) {
        unsigned long idx = netlist_pool->next - i - 1;
        struct net_info *net = (struct net_info *)(netlist_pool->data) + idx;
        int ok = mikami_one_net(net,max_loop,layer_num,idx);
        if (!ok) {
            failed++;
            limit = idx;
            break;
        }
    }

    //forward
    for (i=0; i<limit; ++i) {
        unsigned long idx = i;
        struct net_info *net = (struct net_info *)(netlist_pool->data) + idx;
        int ok = mikami_one_net(net,max_loop,layer_num,idx);
        if (!ok)
            failed++;
    }

    return failed;
}

static unsigned long mikami_one_net(struct net_info *net,
                                    const loop_type max_loop,
                                    const int layer_num,
                                    const unsigned long idx) {
    //return 0 on failure

    if (NET_ROUTED(net)) {
        if (print_status >= 2)
            printf("   skip %4lu (routed in layer %d)\n",
                   idx,net->status);
        return 1;
    }

    unsigned long next_input = net->drain->next_free_input_slot;
    struct ulong_size S = net->source->slot[0].usize;  //output
    struct ulong_size T = net->drain->slot[next_input].usize;  //input

    int ok = mikami(S,T,max_loop,idx);
    if (ok) {
        if (net->drain->type == I_CELL)
            net->drain->next_free_input_slot++;
        net->status = layer_num;
    }
    clean_layer_after_net();
    return ok;
}

static inline int blocked(const unsigned long x, const unsigned long y) {
    const unsigned char s = LAYER_STATUS(x,y);
    if (s == L_EMPTY || (s & L_TRY))
        return 0;
    return 1;
}

static inline int available(const unsigned long x, const unsigned long y,
                            const loop_type loop) {
    if (LAYER_STATUS(x,y) == L_EMPTY)
        return 1;
    if (GET_LAYER_LOOP(x,y) == loop)
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
        if (blocked(P.x,j))
            return;
        if (!available(P.x,j,loop))
            return;
        SET_LAYER_LOOP_TRY(P.x,j,loop);
        LAYER_TRY(P.x,j) |= direction;
        j--;
    }
    //check first column
    if (blocked(P.x,0))
        return;
    if (!available(P.x,0,loop))
        return;
    SET_LAYER_LOOP_TRY(P.x,0,loop);
    LAYER_TRY(P.x,0) |= direction;
}

static void try_right(struct ulong_size P, const loop_type loop,
                      const unsigned char direction) {
    unsigned long j = P.y;
    j++;
    while (j < width) {
        if (blocked(P.x,j))
            return;
        if (!available(P.x,j,loop))
            return;
        SET_LAYER_LOOP_TRY(P.x,j,loop);
        LAYER_TRY(P.x,j) |= direction;
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
        if (blocked(i,P.y))
            return;
        if (!available(i,P.y,loop))
            return;
        SET_LAYER_LOOP_TRY(i,P.y,loop);
        LAYER_TRY(i,P.y) |= direction;
        i--;
    }
    //check first row
    if (blocked(0,P.y))
        return;
    if (!available(0,P.y,loop))
        return;
    SET_LAYER_LOOP_TRY(0,P.y,loop);
    LAYER_TRY(0,P.y) |= direction;
}

static void try_down(struct ulong_size P, const loop_type loop,
                     const unsigned char direction) {
    unsigned long i = P.x;
    i++;
    while (i < height) {
        if (blocked(i,P.y))
            return;
        if (!available(i,P.y,loop))
            return;
        SET_LAYER_LOOP_TRY(i,P.y,loop);
        LAYER_TRY(i,P.y) |= direction;
        i++;
    }
}

/////////////////////////////////////////////////////////////
//  return L_TRY and sets *next point to mark_path()
/////////////////////////////////////////////////////////////
#define MARK_POINT(x,y,b) do { LAYER_STATUS(x,y) |= b; } while (0);

static inline int end_point(const unsigned long x, const unsigned long y) {
    if (GET_LAYER_STATUS(x,y) & (L_WIRE | L_VIA | L_IO))
        return 1;
    return 0;
}

static int mark_left(struct ulong_size P, loop_type loop, struct ulong_size *next) {
    unsigned long j = P.y;
    if (j == 0)
        return L_INVALID;
    j--;
    while (j) {
        if (end_point(P.x,j))
            return L_IO;
        if (GET_LAYER_LOOP(P.x,j) < loop) {
            next->x = P.x;
            next->y = j;
            return L_TRY;
        }
        MARK_POINT(P.x,j,L_WIRE);
        j--;
    }
    //check first column
    if (end_point(P.x,0))
        return L_IO;
    if (GET_LAYER_LOOP(P.x,0) < loop) {
        next->x = P.x;
        next->y = 0;
        return L_TRY;
    }
    MARK_POINT(P.x,0,L_WIRE);
    return L_INVALID;
}

static int mark_right(struct ulong_size P, loop_type loop,
                      struct ulong_size *next) {
    unsigned long j = P.y;
    j++;
    while (j < width) {
        if (end_point(P.x,j))
            return L_IO;
        if (GET_LAYER_LOOP(P.x,j) < loop) {
            next->x = P.x;
            next->y = j;
            return L_TRY;
        }
        MARK_POINT(P.x,j,L_WIRE);
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
        if (end_point(i,P.y))
            return L_IO;
        if (GET_LAYER_LOOP(i,P.y) < loop) {
            next->x = i;
            next->y = P.y;
            return L_TRY;
        }
        MARK_POINT(i,P.y,L_WIRE);
        i--;
    }
    //check first row
    if (end_point(0,P.y))
        return L_IO;
    if (GET_LAYER_LOOP(0,P.y) < loop) {
        next->x = 0;
        next->y = P.y;
        return L_TRY;
    }
    MARK_POINT(0,P.y,L_WIRE);
    return L_INVALID;
}

static int mark_down(struct ulong_size P, loop_type loop, struct ulong_size *next) {
    unsigned long i = P.x;
    i++;
    while (i < height) {
        if (end_point(i,P.y))
            return L_IO;
        if (GET_LAYER_LOOP(i,P.y) < loop) {
            next->x = i;
            next->y = P.y;
            return L_TRY;
        }
        MARK_POINT(i,P.y,L_WIRE);
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
                GET_LAYER_STATUS(P.x,P.y),                              \
                LAYER_TRY(P.x,P.y),                                     \
                GET_LAYER_LOOP(P.x,P.y));                               \
        assert(0 && "mark_path() unknown intersection!");               \
        exit(EXIT_FAILURE);                                             \
    } while (0);

    //mark the opposite directions

    assert(loop > 0);

    const unsigned char s = LAYER_TRY(P.x,P.y);

    if (end_point(P.x,P.y))
        return;

    //from source
    if (s == (S_TRY_WIRE_LEFT | T_TRY_WIRE_RIGHT)) {
        MARK_POINT(P.x,P.y,L_WIRE);
        RECURSION_MARK_PATH(right);
        RECURSION_MARK_PATH(left);
    }
    else if (s == (S_TRY_WIRE_LEFT | T_TRY_WIRE_UP)) {
        MARK_POINT(P.x,P.y,L_VIA);
        RECURSION_MARK_PATH(right);
        RECURSION_MARK_PATH(down);
    }
    else if (s == (S_TRY_WIRE_LEFT | T_TRY_WIRE_DOWN)) {
        MARK_POINT(P.x,P.y,L_VIA);
        RECURSION_MARK_PATH(right);
        RECURSION_MARK_PATH(up);
    }
    else if (s == (S_TRY_WIRE_RIGHT | T_TRY_WIRE_UP)) {
        MARK_POINT(P.x,P.y,L_VIA);
        RECURSION_MARK_PATH(left);
        RECURSION_MARK_PATH(down);
    }
    else if (s == (S_TRY_WIRE_RIGHT | T_TRY_WIRE_DOWN)) {
        MARK_POINT(P.x,P.y,L_VIA);
        RECURSION_MARK_PATH(left);
        RECURSION_MARK_PATH(up);
    }
    else if (s == (S_TRY_WIRE_UP | T_TRY_WIRE_DOWN)) {
        MARK_POINT(P.x,P.y,L_WIRE);
        RECURSION_MARK_PATH(down);
        RECURSION_MARK_PATH(up);
    }

    //from term
    else if (s == (T_TRY_WIRE_LEFT | S_TRY_WIRE_RIGHT)) {
        MARK_POINT(P.x,P.y,L_WIRE);
        RECURSION_MARK_PATH(right);
        RECURSION_MARK_PATH(left);
    }
    else if (s == (T_TRY_WIRE_LEFT | S_TRY_WIRE_UP)) {
        MARK_POINT(P.x,P.y,L_VIA);
        RECURSION_MARK_PATH(right);
        RECURSION_MARK_PATH(down);
    }
    else if (s == (T_TRY_WIRE_LEFT | S_TRY_WIRE_DOWN)) {
        MARK_POINT(P.x,P.y,L_VIA);
        RECURSION_MARK_PATH(right);
        RECURSION_MARK_PATH(up);
    }
    else if (s == (T_TRY_WIRE_RIGHT | S_TRY_WIRE_UP)) {
        MARK_POINT(P.x,P.y,L_VIA);
        RECURSION_MARK_PATH(left);
        RECURSION_MARK_PATH(down);
    }
    else if (s == (T_TRY_WIRE_RIGHT | S_TRY_WIRE_DOWN)) {
        MARK_POINT(P.x,P.y,L_VIA);
        RECURSION_MARK_PATH(left);
        RECURSION_MARK_PATH(up);
    }
    else if (s == (T_TRY_WIRE_UP | S_TRY_WIRE_DOWN)) {
        MARK_POINT(P.x,P.y,L_WIRE);
        RECURSION_MARK_PATH(down);
        RECURSION_MARK_PATH(up);
    }

    //mid-points with one possible direction
    else if (s == S_TRY_WIRE_LEFT || s == T_TRY_WIRE_LEFT) {
        MARK_POINT(P.x,P.y,L_VIA);
        RECURSION_MARK_PATH(right);
    }
    else if (s == S_TRY_WIRE_RIGHT || s == T_TRY_WIRE_RIGHT) {
        MARK_POINT(P.x,P.y,L_VIA);
        RECURSION_MARK_PATH(left);
    }
    else if (s == S_TRY_WIRE_UP || s == T_TRY_WIRE_UP) {
        MARK_POINT(P.x,P.y,L_VIA);
        RECURSION_MARK_PATH(down);
    }
    else if (s == S_TRY_WIRE_DOWN || s == T_TRY_WIRE_DOWN) {
        MARK_POINT(P.x,P.y,L_VIA);
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

        MARK_POINT(P.x,P.y,L_VIA);
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

        MARK_POINT(P.x,P.y,L_VIA);
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

#undef MARK_POINT

static inline unsigned char get_connection(const unsigned long x,
                                           const unsigned long y,
                                           const loop_type loop) {
    if (GET_LAYER_STATUS(x,y) != L_TRY)
        return L_TRY;
    if (GET_LAYER_LOOP(x,y) != loop)
        return L_TRY;

    const unsigned char s = LAYER_TRY(x,y);

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

    return GET_LAYER_STATUS(x,y);
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

    /*
       search in the fast path first
      .-----------------------------.
      |                             |
      |             up              |
      |                             |
      |------- S ---------.---------|
      |        :          :         |
      |  left  :  fast    :  right  |
      |        :    path  :         |
      |--------`--------- T --------|
      |                             |
      |            down             |
      |                             |
      `-----------------------------`
     */

    //fast path
    for (i=uprow; i<downrow; ++i) {
        for (j=leftcol; j<rightcol; ++j) {
            const unsigned char status = get_connection(i,j,loop);
            if (status == L_WIRE || status == L_VIA) {
                //found intersection
                struct ulong_size p = { .x = i, .y = j };
                mark_path(p,loop,S,T);
                return 2;
            }
        }
    }

    //up
    for (i=0; i<uprow; ++i) {
        for (j=0; j<width; ++j) {
            const unsigned char status = get_connection(i,j,loop);
            if (status == L_WIRE || status == L_VIA) {
                //found intersection
                struct ulong_size p = { .x = i, .y = j };
                mark_path(p,loop,S,T);
                return 1;
            }
        }
    }

    //left
    for (i=uprow; i<downrow; ++i) {
        for (j=0; j<leftcol; ++j) {
            const unsigned char status = get_connection(i,j,loop);
            if (status == L_WIRE || status == L_VIA) {
                //found intersection
                struct ulong_size p = { .x = i, .y = j };
                mark_path(p,loop,S,T);
                return 1;
            }
        }
    }

    //right
    for (i=uprow; i<downrow; ++i) {
        for (j=rightcol; j<width; ++j) {
            const unsigned char status = get_connection(i,j,loop);
            if (status == L_WIRE || status == L_VIA) {
                //found intersection
                struct ulong_size p = { .x = i, .y = j };
                mark_path(p,loop,S,T);
                return 1;
            }
        }
    }

    //down
    for (i=downrow; i<height; ++i) {
        for (j=0; j<width; ++j) {
            const unsigned char status = get_connection(i,j,loop);
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
                GET_LAYER_STATUS(P.x,P.y),
                LAYER_TRY(P.x,P.y),
                GET_LAYER_LOOP(P.x,P.y));
        assert(0);
        exit(EXIT_FAILURE);
    }
}

static int try_again(const loop_type loop) {
    assert(loop >= 1);

    unsigned long i;
    unsigned long j;
    for (i=0; i<height; ++i) {
        for (j=0; j<width; ++j) {
            if (GET_LAYER_STATUS(i,j) & L_TRY && GET_LAYER_LOOP(i,j) == loop-1) {
                struct ulong_size P = { .x = i, .y = j };
                expand(P,loop);
            }
        }
    }

#if 0
    unsigned long empty = 0;
    for (i=0; i<height; ++i)
        for (j=0; j<width; ++j)
            if (GET_LAYER_STATUS(i,j) == L_EMPTY)
                empty++;
    return empty == 0;
#endif

    return 0;
}

static int expand_common_wires(const loop_type loop) {
    assert(loop == 1);
    return try_again(loop);
}

static int mikami(const struct ulong_size S, const struct ulong_size T,
                  const loop_type max_loop, const unsigned long net) {
    //return 0 on failure, else the loop number of path

    if (!max_loop) {
        if (print_status >= 3)
            fprintf(stderr,"in mikami() call: no max_loop limit, fail\n");
        return 0;
    }

    assure_io(S,T);

    loop_type loop = 1;

    expand_source(S,loop);
    expand_term(T,loop);
    expand_common_wires(loop);

    int path_found = has_intersection(loop,S,T);
    int full = 0;
    while (!path_found && !full) {
        if (loop == max_loop)  break;
        loop++;
        full = try_again(loop);
        path_found = has_intersection(loop,S,T);
    }
    clean_layer_after_path();

    assure_io(S,T);

    //LAYER(S.x,S.y).loop_status = L_START;
    //LAYER(T.x,T.y).loop_status = L_TERM;

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

    if (!path_found)
        return 0;
    return loop;
}

static inline layer_element *create_layer(struct analysis_info *soc) {
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
            if (GET_LAYER_STATUS(i,j) == L_IO)
                new_layer[idx].loop_status = L_IO;
        }
    }

    soc->layer[soc->layer_num++] = new_layer;
    return new_layer;
}

//Quick sort implementation
//Initial code from:
//http://www.comp.dit.ie/rlawlor/Alg_DS/sorting/quickSort.c
static unsigned long partition(struct net_info *a, unsigned long  l,
                               unsigned long r) {
    double pivot = a[l].weight;
    unsigned long i = l;
    unsigned long j = r;

    struct net_info tmp;

    while (1) {
        do ++i; while (i < r && a[i].weight <= pivot);
        do --j; while (a[j].weight > pivot);
        if( i >= j ) break;
        tmp = a[i]; a[i] = a[j]; a[j] = tmp;
    }
    tmp = a[l]; a[l] = a[j]; a[j] = tmp;
    return j;
}

static void quicksort(struct net_info *a, unsigned long l, unsigned long r) {
    unsigned long j;
    if (l < r) {
        j = partition(a,l,r);
        quicksort(a,l,j);
        quicksort(a,j+1,r);
    }
}
