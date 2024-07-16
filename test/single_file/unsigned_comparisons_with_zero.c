#include <stdbool.h>

bool x_eq_zero(unsigned x) {
  return x == 0u;
}

bool x_ne_zero(unsigned x) {
  return x != 0u;
}

bool x_gt_zero(unsigned x) {
  return x > 0u;
}

bool x_ge_zero(unsigned x) {
  return x >= 0u;
}

bool x_lt_zero(unsigned x) {
  return x < 0u;
}

bool x_le_zero(unsigned x) {
  return x <= 0u;
}

bool zero_eq_x(unsigned x) {
  return 0u == x;
}

bool zero_ne_x(unsigned x) {
  return 0u != x;
}

bool zero_gt_x(unsigned x) {
  return 0u > x;
}

bool zero_ge_x(unsigned x) {
  return 0u >= x;
}

bool zero_lt_x(unsigned x) {
  return 0u < x;
}

bool zero_le_x(unsigned x) {
  return 0u <= x;
}
