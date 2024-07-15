#include <stdio.h>

unsigned NE(unsigned, unsigned);

void comparison(unsigned x, unsigned y) {
  printf("NE_%u_%u_%u_",
	 x,
	 y,
	 NE(x, y));
}

int main() {
  comparison(2, 2);
  comparison(1, 10);
  comparison(100, 0);
}
