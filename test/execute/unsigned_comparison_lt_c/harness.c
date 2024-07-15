#include <stdio.h>

unsigned LT(unsigned, unsigned);

void comparison(unsigned x, unsigned y) {
  printf("LT_%u_%u_%u_",
	 x,
	 y,
	 LT(x, y));
}

int main() {
  comparison(2, 2);
  comparison(1, 10);
  comparison(100, 0);
}
