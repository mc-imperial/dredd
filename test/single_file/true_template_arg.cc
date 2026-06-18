template <bool> class C {
public:
  C(int*) {}
};

using S = C<true>;

void f() {
  S SC(0);
}

bool g() {
  S SC(0);
  int x;
  x = 0;
}
