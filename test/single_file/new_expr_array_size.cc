class foo {
public:
  foo(int x) {}
};

int main() { 
  new foo[2]{4, 5}; 
}
