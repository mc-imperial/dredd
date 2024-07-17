#include <iostream>

void foo() {
  const int b = 2;
  __builtin_frame_address(b);
  std::cout << b << std::endl;
}
