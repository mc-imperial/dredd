int main() {
  int x = 5;
  --x = 2;
  volatile int y = 8;
  --y = 6;
  return x;
}