#include <stdio.h>

unsigned EQ(unsigned, unsigned);
unsigned NE(unsigned, unsigned);
unsigned GE(unsigned, unsigned);
unsigned GT(unsigned, unsigned);
unsigned LE(unsigned, unsigned);
unsigned LT(unsigned, unsigned);

void all_comparisons(unsigned x, unsigned y) {
  printf("compare_%u_%u_EQ_%u_NE_%u_GE_%u_GT_%u_LE_%u_LT_%u_",
	 x,
	 y,
	 EQ(x, y),
	 NE(x, y),
	 GE(x, y),
	 GT(x, y),
	 LE(x, y),
	 LT(x, y));
}

int main() {
  all_comparisons(2, 2);
  all_comparisons(1, 10);
  all_comparisons(100, 0);
}
