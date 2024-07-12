#include <iostream>

void foo() {
  const int b = 2;
  int a[b + b]{10, 11, 12, 13};
  std::cout << b << std::endl;
}
