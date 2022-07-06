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

#include "math/trig.h"
#include <stdexcept>

/**
 * Approximate the value of sin(x)
 *
 * Uses n terms of the series expansion for sin to approximate the value of sin(x).
 *
 * @param x The value to apply sin to.
 * @param n The number of terms of the series expansion to generate.
 * @return the value of sin at x.
 * */
double SinN(const double &x, const int &n) {

  if (n <= 0) throw std::out_of_range("The number of terms must be a positive integer.");

  // First term from series expansion
  double t = x;
  double sum = 0;

  for (int i = 1; i <= n; i++) {
    sum += t;
    // calculate the next term from the previous.
    t *= (-1 * x * x) / ((2 * i + 1) * (2 * i));
  }

  return sum;
}

/**
 * Approximate the value of cos(x)
 *
 * Uses n terms of the series expansion for cos to approximate the value of cos(x).
 *
 * @param x The value to apply cos to.
 * @param n The number of terms of the series expansion to generate.
 * @return the value of cos at x.
 * */
double CosN(const double &x, const int &n) {

  if (n <= 0) throw std::out_of_range("The number of terms must be a positive integer.");

  // First term from series expansion
  double t = 1;
  double sum = 0;

  for (int i = 0; i <= n; ++i) {
    sum += t;
    // calculate the next term from the previous.
    t *= (-1 * x * x) / ((2 * i + 1) * (2 * i + 2));
  }

  return sum;
}

/**
 * Approximate the value of tan(x)
 *
 * Uses n terms of the series expansion of sin and cos to approximate the value of tan(x).
 *
 * @param x The value to apply cos to.
 * @param n The number of terms of the series expansion to generate.
 * @return the value of cos at x.
 * */
double TanN(const double &x, const int &n) {
  return SinN(x, n) / CosN(x, n);
}

/**
 * Approximate the value of sec(x)
 *
 * Uses n terms of the series expansion of cos to approximate the value of sec(x).
 *
 * @param x The value to apply sec to.
 * @param n The number of terms of the series expansion to generate.
 * @return the value of sec at x.
 * */
double SecN(const double &x, const int &n) {
  return 1 / CosN(x, n);
}

/**
 * Approximate the value of cosec(x)
 *
 * Uses n terms of the series expansion of sin to approximate the value of cosec(x).
 *
 * @param x The value to apply cosec to.
 * @param n The number of terms of the series expansion to generate.
 * @return the value of cosec at x.
 * */
double CosecN(const double &x, const int &n) {
  return 1 / SinN(x, n);
}

/**
 * Approximate the value of cot(x)
 *
 * Uses n terms of the series expansion of sin and cos to approximate the value of cot(x).
 *
 * @param x The value to apply cot to.
 * @param n The number of terms of the series expansion to generate.
 * @return the value of cot at x.
 * */
double CotN(const double &x, const int &n) {
  return 1 / TanN(x, n);
}