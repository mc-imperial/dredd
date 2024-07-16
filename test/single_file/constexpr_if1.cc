#include <iostream>

template <int a> void foo() {
  if constexpr (a) {
    std::cout << "Hi";
  };
}

int main() {
  foo<0>;
}
