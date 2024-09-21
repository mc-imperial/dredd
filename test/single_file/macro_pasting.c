#define MINUS -
#define ONE 1

int a () {
  return MINUS ONE;
}

#define ADD_SUFFIX(X) X##L

#define NUM_L -ADD_SUFFIX(2)

int b () {
  return NUM_L;
}

#define ADD_PARENS(X) (X)

#define NUM_PARENS ADD_PARENS(5)

int c() {
  return NUM_PARENS;
}
