#include <iostream>

void foo() {
    const int b = 2;
    static_assert(b);
    std::cout << b << std::endl;
}