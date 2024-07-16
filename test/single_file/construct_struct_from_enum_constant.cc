struct foo {
  foo(int) {};
  operator int();
};

enum baz { bar };

int main() { 
  0 ? foo(0) : baz::bar;
}
