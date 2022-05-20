#pragma once


void VecExpand(char **data, int *length, int *capacity, int memsz);

void VecSpliceImpl(char **data, int *length, int *capacity, int memsz, int start, int count);

#define Vec(T)\
  struct { T *data; int length, capacity; }


#define VecUnpack(v)\
  (char**)&(v)->data, &(v)->length, &(v)->capacity, sizeof(*(v)->data)


#define VecInit(v)\
  memset((v), 0, sizeof(*(v)))


#define VecDeinit(v)\
  free((v)->data)


#define VecClear(v)\
  ((v)->length = 0)


#define VecPush(v, val)\
  ( VecExpand(VecUnpack(v)),\
    (v)->data[(v)->length++] = (val) )


#define VecSplice(v, start, count)\
  ( VecSpliceImpl(VecUnpack(v), start, count),\
    (v)->length -= (count) )

