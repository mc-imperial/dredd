class foo {
public:
  virtual void bar() = 0;
};

class a : foo {
  void bar() override;
};

void __dredd_prelude_start();

void a::bar() {
  int x = 2;
}
