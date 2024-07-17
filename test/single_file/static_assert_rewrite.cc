int foo() {
  const int a = 42;
  static_assert(a);
  return a;
}
