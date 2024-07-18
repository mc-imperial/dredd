long bar() {}

void foo() {
  unsigned x = (decltype(bar())) 'a';
}
