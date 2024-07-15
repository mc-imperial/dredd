#include <stdio.h>

unsigned GT(unsigned, unsigned);

void comparison(unsigned x, unsigned y) {
  printf("GT_%u_%u_%u_",
	 x,
	 y,
	 GT(x, y));
}

int main() {
  comparison(2, 2);
  comparison(1, 10);
  comparison(100, 0);
}
