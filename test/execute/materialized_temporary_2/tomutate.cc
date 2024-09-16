#include <optional>

struct S {
  unsigned f;
  ~S() {
    f = 12;
  }
};

struct T {
  std::optional<S> g;
  std::optional<S> getg() const { return g; }
};

unsigned bar(std::optional<S> g) {
  return g->f;
}

int foo() {
    T x;
    x.g = S{42};
    return bar(S{x.getg()->f});
}
