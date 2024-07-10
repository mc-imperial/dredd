#include <stdio.h>

void switch_cases(int c) {
  printf("a_");
  switch (c) {
  case 2:
  case 3:
  case 4:
    printf("b_");
    printf("c_");
    break;
  case 5:
  default:
    printf("d_");
  }
  printf("e_");
}
