#include <cstdlib>
#include <iostream>

[[noreturn]] static int foo() {
  std::cout << "Aborting\n";
  std::abort();
}
