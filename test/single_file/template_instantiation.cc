template<int N> void foo() {
  int A[N] = {0};
}

int main() {
  foo<1 + 2>();
}
