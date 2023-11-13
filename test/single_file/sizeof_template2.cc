bool f(int);

template <typename a> struct b {
  bool c() { return f(sizeof(a)); }
};
