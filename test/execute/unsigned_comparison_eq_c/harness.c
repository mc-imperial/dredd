#include <stdio.h>

unsigned EQ(unsigned, unsigned);

void comparison(unsigned x, unsigned y) {
  printf("EQ_%u_%u_%u_",
	 x,
	 y,
	 EQ(x, y));
}

int main() {
  comparison(2, 2);
  comparison(1, 10);
  comparison(100, 0);
}
