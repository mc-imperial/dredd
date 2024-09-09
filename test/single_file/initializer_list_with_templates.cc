#include <memory>
class a {
public:
  a(int);
  virtual void b();
};
template <typename> class c : a {
  using a::a;
  void b() {
    struct {
      short d;
    } e{0};
  }
};

void f() { std::make_shared<c<int>>(2); }
