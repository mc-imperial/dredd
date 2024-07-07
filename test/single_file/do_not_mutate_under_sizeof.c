int foo(int* x, int y) {
  // No mutation inside the `sizeof` should occur
  return sizeof(x[y + 3]) + sizeof(y);
}
