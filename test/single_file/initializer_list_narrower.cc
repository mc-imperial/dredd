#include <initializer_list>

class foo {
public:
  foo(std::initializer_list<unsigned int>) {}
};

int main() { 
    foo{+2}; 
}
