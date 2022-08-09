int x = 1 + 2;

namespace {
  int y = x + x;

  class A {
    int z = y + y;
    void foo() {
      class B {
        int w = !x;
      };
    }
  };
}
