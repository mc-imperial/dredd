template<typename T> void bloop(T& x) { }

struct foo {
  int b : 2;
};

int main() {
  const foo d = foo();
  bloop(d.b);
}