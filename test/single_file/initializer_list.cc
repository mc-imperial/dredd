#include <cstddef>

struct A {
  size_t x;
  size_t y;
};

void foo(A arg);

void bar() {
  foo({0, 0});
}
