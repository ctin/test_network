#include "utils/realloc.h"
#include "utils/error_handler.h"

#include <stdlib.h>

void* Realloc(void *ptr, int n) {
  ptr = realloc(ptr, n);
  if (!ptr && n != 0) {
    FatalError("out of memory");
  }
  return ptr;
}
