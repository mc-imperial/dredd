#include <initializer_list>

class foo {
public:
  foo(std::initializer_list<short>) {}
};

int main() { 
    foo{(long) 2}; 
}
