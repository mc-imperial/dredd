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
// limitations under the License./

#ifndef MATHTEST_TEST_FUNCS_H
#define MATHTEST_TEST_FUNCS_H

#include "gtest/gtest.h"

namespace math {
namespace math_test {

::testing::AssertionResult IsWithin(double val, double correct,
                                    double percentageDifference);

} // namespace math_test
} // namespace math

#endif // MATHTEST_TEST_FUNCS_H
