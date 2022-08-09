struct S {
  int a : 3;
  int b : 3;
};

void foo() {
  struct S myS;
  myS.a = myS.b;
  myS.a++;
  --myS.b;
}
