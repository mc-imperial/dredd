#include <stdio.h>

unsigned GE(unsigned, unsigned);

void comparison(unsigned x, unsigned y) {
  printf("GE_%u_%u_%u_",
	 x,
	 y,
	 GE(x, y));
}

int main() {
  comparison(2, 2);
  comparison(1, 10);
  comparison(100, 0);
}
