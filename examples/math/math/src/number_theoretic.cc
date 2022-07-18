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

#include "math/number_theoretic.h"
#include <stdexcept>

namespace math {

/**
 * Calculate the ceiling of the input.
 *
 * @param x the number to apply the ceiling operator to.
 * @return x if x is an interger, or the ceiling of x if it is a decimal.
 */
int Ceil(const double &x) {
  int y = (int) x;
  return (x == y) ? y : y + 1;
}

/**
 * Calculates the absolute value of the input.
 *
 * @param x the value to take the absolute value of.
 * @return x if x is positive or -x if x is negative.
 */
double Mod(const double &x) {
  return x < 0 ? -x : x;
}

/**
 * Calculate the factorial of a number.
 *
 * @param x the number to take the factorial of.
 * @return the factorial of x, equivalent to x!
 */
long long int Factorial(const int &x) {
  if (x < 0) throw std::out_of_range("Can only take factorial of non-negative values.");

  long long int sum = 1;

  for (int i = 2; i <= x; ++i) {
    sum *= i;
  }

  return sum;
}

/**
 * Calculates n choose k
 *
 * Calculate the number of ways to choose k items from a sample of n.
 *
 * @param n the total number of items.
 * @param k the sample size.
 * @return the number of ways to choose k items from a sample of n.
 */
long long int Comb(const int &n, const int &k) {
  if (n < 0 || k < 0) throw std::out_of_range("n and k must be positive.");
  return Factorial(n) / (Factorial(k) * Factorial(n - k));
}

} // namespace math