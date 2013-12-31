#ifndef __POOL_H__
#define __POOL_H__

struct pool_info {
    void *data;
    unsigned long element_size;
    unsigned long size;
    unsigned long next;
};

//return new size
unsigned long grow(struct pool_info *pool, void (*rebuild)(void));

#endif
