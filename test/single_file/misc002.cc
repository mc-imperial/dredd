#include <inttypes.h>

void f() {
  if (sizeof(uint32_t) == sizeof(uint64_t))
    ;
}
