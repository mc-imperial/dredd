unsigned int glob;

struct S {

  ~S() {
    glob = 12;
  }
  
  unsigned int& operator[](int) {
    return glob;
  }
};

unsigned int foo() {
  glob = 42;
  return S()[0];
}
