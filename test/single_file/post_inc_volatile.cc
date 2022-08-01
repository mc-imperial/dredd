int main() {
  volatile int x = 9;
  volatile int y = 43;
  int z;
  z = x++ + y++;
  return z;
}