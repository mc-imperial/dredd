template<typename T, int N>
class bar {};

void foo() {
  const int count = 42;
  bar<bar<int, count * 2>, count> a;
}