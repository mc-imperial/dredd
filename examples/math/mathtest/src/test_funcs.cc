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

#include "mathtest/test_funcs.h"
#include "gtest/gtest.h"

namespace math {
namespace math_test {

::testing::AssertionResult IsWithin(double val, double correct,
                                    double percentageDifference) {
  if (val < 0 && correct < 0)
    val *= -1, correct *= -1;
  if ((val >= correct * (1 - percentageDifference)) &&
      (val <= correct * (1 + percentageDifference)))
    return ::testing::AssertionSuccess();
  else
    return ::testing::AssertionFailure()
           << val << " is not within " << percentageDifference * 100 << "% of "
           << correct;
}

} // namespace math_test
} // namespace math