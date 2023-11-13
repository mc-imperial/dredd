void foo() {
  int a[2] = {1, 2};
  if (true) {
    auto [x, y] = a;
    auto& [xr, yr] = a;
    xr = y++;
    yr = x++;
  }
}
