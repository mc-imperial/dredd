#include <stdio.h>

unsigned LE(unsigned, unsigned);

void comparison(unsigned x, unsigned y) {
  printf("LE_%u_%u_%u_",
	 x,
	 y,
	 LE(x, y));
}

int main() {
  comparison(2, 2);
  comparison(1, 10);
  comparison(100, 0);
}
