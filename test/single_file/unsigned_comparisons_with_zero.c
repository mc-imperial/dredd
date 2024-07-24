#include <stdbool.h>

// Check that optimisations are applied when an unsigned zero literal is used.

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

// Check that optimisations are applied when a signed zero literal is used.

bool x_eq_signed_zero(unsigned x) {
  return x == 0;
}

bool x_ne_signed_zero(unsigned x) {
  return x != 0;
}

bool x_gt_signed_zero(unsigned x) {
  return x > 0;
}

bool x_ge_signed_zero(unsigned x) {
  return x >= 0;
}

bool x_lt_signed_zero(unsigned x) {
  return x < 0;
}

bool x_le_signed_zero(unsigned x) {
  return x <= 0;
}

bool signed_zero_eq_x(unsigned x) {
  return 0 == x;
}

bool signed_zero_ne_x(unsigned x) {
  return 0 != x;
}

bool signed_zero_gt_x(unsigned x) {
  return 0 > x;
}

bool signed_zero_ge_x(unsigned x) {
  return 0 >= x;
}

bool signed_zero_lt_x(unsigned x) {
  return 0 < x;
}

bool signed_zero_le_x(unsigned x) {
  return 0 <= x;
}
