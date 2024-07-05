int foo(int* x, int y) {
  // No mutation inside the `alignof` should occur
  return alignof(x[y + 3]) + alignof(y);
}
