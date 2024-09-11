#include <vector>

std::vector<unsigned int> bar() {
    std::vector<unsigned int> res(4);
    return res;
}

int main() {
    unsigned foo = bar()[0];
}
