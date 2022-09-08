int foo(int x) {
  // It is important that "y" is not mutated in "auto y"
  if (auto y = x) {
    return y;
  }
  return 2;
}
