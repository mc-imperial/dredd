#include <stdio.h>

void doit(unsigned zero, unsigned ten) {
  if(zero != 0) {
    printf("0_");
  } else {
    printf("1_");
  }
  if(0 != zero) {
    printf("0_");
  } else {
    printf("1_");
  }
  if (ten != 0) {
    printf("1_");
  } else {
    printf("0_");
  }
  if (0 != ten) {
    printf("1_");
  } else {
    printf("0_");
  }
}
