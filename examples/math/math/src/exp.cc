// Copyright 2022 The Dredd Project Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "math/exp.h"
#include "math/number_theoretic.h"
#include <iostream>
#include <stdexcept>

namespace math {

/**
 * Approximate the value of e^x
 *
 * Uses n terms of the series expansion for e^x to approximate e^x.
 *
 * @param x the argument of e^x.
 * @param n the number of terms of the series expansion to generate.
 * @return the value of e^x
 */
double ExpN(const double &x, const int &n) {

  if (n <= 0)
    throw std::out_of_range("The number of terms must be a positive integer.");

  double sum = 1;

  for (int i = n - 1; i > 0; --i) {
    sum = 1 + x * sum / i;
  }

  return sum;
}

/**
 * Calculates a^b
 *
 * @param base the value you want to be multiplied.
 * @param exp the amount of times you want to multiply base by itself.
 * @return base^exp.
 */
long double Pow(const double &base, const int &exp) {
  long double result = 1;

  for (int i = 0; i < Mod(exp); ++i) {
    result *= base;
  }

  return (exp < 0) ? 1 / result : result;
}

/**
 * Calculates the binary logarithm
 *
 * @param x the number to take log base 2 of.
 * @return The number such that 2^result = x.
 */
long double Log2(const int &x) {
  if (x <= 0)
    throw std::out_of_range("Can only calculate the log of positive values.");

  int n = x;
  int logValue = -1;
  while (n) {
    logValue++;
    n >>= 1;
  }

  long double result = logValue;
  long double constant = 1;
  long double y = Pow(2, -logValue) * x;
  long double z = y;
  if (y != 1) {
    for (int i = 0; i < 100; ++i) {
      int m = 0;
      while (!(2 <= z && z < 4)) {
        z *= z;
        ++m;
      }
      constant *= Pow(2, -m);
      result += constant;
      z /= 2;
    }
  }
  return result;
}

/**
 * Calculates log base b of a.
 *
 * @param a the value to take the logarithm of.
 * @param b the base of the logarithm.
 * @return log base b of a.
 */
long double Log(const int &a, const int &b) { return Log2(a) / Log2(b); }

} // namespace math