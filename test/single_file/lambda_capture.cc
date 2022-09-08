void foo() {
  int x = 2;
  // It is important that "x" is not mutated in "[x]"
  auto f = [x]() { return x; };
}
