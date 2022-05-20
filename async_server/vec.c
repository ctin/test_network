#include "async_server/vec.h"
#include "utils/realloc.h"
#include <string.h>

void VecExpand(char **data, int *length, int *capacity, int memsz) {
  if (*length + 1 > *capacity) {
    if (*capacity == 0) {
      *capacity = 1;
    } else {
      *capacity <<= 1;
    }
    *data = Realloc(*data, *capacity * memsz);
  }
}

void VecSpliceImpl(
  char **data, int *length, int *capacity, int memsz, int start, int count
) {
  (void) capacity;
  memmove(*data + start * memsz,
          *data + (start + count) * memsz,
          (*length - start - count) * memsz);
}