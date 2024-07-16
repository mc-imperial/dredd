#include <stdio.h>

int main() {
  int x = 10;
  switch (x) {
  case 2:
    printf("bar\n");
    printf("buz\n");
  default:
    printf("baz\n");
  }
}
