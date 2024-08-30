namespace bar {
  void foo(int x);

  enum {B = 1};

  struct C {
    operator int();
    friend void baz(int x);
  };
}

void func() {
  // All of these calls rely on ADL
  foo(bar::B);
  foo((bar::B));
  bar::C c;
  foo(c);
  foo((c));
  baz(c);
  baz((c));
}
