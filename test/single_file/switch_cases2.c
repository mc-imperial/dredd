#include <stdio.h>

int main() {
  int x = 10;
  switch (x) {
  case 2:
  case 3:
  case 4:
    printf("bar\n");
    printf("buz\n");
  case 5:
  default:
    printf("baz\n");
  }
}
