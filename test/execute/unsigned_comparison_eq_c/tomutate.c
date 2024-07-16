#include <stdio.h>

void doit(unsigned zero, unsigned one, unsigned two_a, unsigned two_b, unsigned ten, unsigned onehundred) {
  if(two_a == two_b) {
    printf("1_");
  } else {
    printf("0_");
  }
  if (one == ten) {
    printf("0_");
  } else {
    printf("1_");
  }
  if (onehundred == zero) {
    printf("0_");
  } else {
    printf("1_");
  }
}
