template<typename T>
T foo(T a) {
  return a + 2;
}

void foo() {
  int x;
  x = foo(3);
}
