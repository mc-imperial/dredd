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

#include "math/exp.h"
#include "mathtest/test_funcs.h"
#include "gtest/gtest.h"

namespace math {
namespace math_test {
namespace {

TEST(ExpTest, HandlesZeroInput) { EXPECT_EQ(ExpN(0, 100), 1); }

TEST(ExpTest, HandlesPositiveInput) {
  const double eOne = 2.7182818285;
  const double eTwo = 7.389056099;
  ASSERT_TRUE(IsWithin(ExpN(1, 1000), eOne, 0.0001));
  ASSERT_TRUE(IsWithin(ExpN(2, 1000), eTwo, 0.0001));
}

TEST(ExpTest, HandlesNegativeInput) {
  const double eMinusOne = 0.3678794412;
  const double eMinusTwo = 0.1353352832;
  ASSERT_TRUE(IsWithin(ExpN(-1, 1000), eMinusOne, 0.0001));
  ASSERT_TRUE(IsWithin(ExpN(-2, 1000), eMinusTwo, 0.0001));
}

TEST(ExpTest, NMustBePositive) {
  try {
    ExpN(0, -10);
    FAIL() << "Expected std::out_of_range";
  } catch (std::out_of_range const &e) {
    EXPECT_EQ(e.what(),
              std::string("The number of terms must be a positive integer."));
  } catch (...) {
    FAIL() << "Expected std::out_of_range";
  }

  try {
    ExpN(0, 0);
    FAIL() << "Expected std::out_of_range";
  } catch (std::out_of_range const &e) {
    EXPECT_EQ(e.what(),
              std::string("The number of terms must be a positive integer."));
  } catch (...) {
    FAIL() << "Expected std::out_of_range";
  }
}

TEST(PowTest, HandlesZeroExponent) {
  EXPECT_EQ(Pow(0, 0), 1);
  EXPECT_EQ(Pow(5, 0), 1);
  EXPECT_EQ(Pow(53253.34623, 0), 1);
  EXPECT_EQ(Pow(-8, 0), 1);
  EXPECT_EQ(Pow(-32537.2634, 0), 1);
}

TEST(PowTest, HandlesPositiveExponent) {
  EXPECT_EQ(Pow(9, 1), 9);
  EXPECT_EQ(Pow(5, 3), 125);
  EXPECT_EQ(Pow(7, 5), 16807);
  EXPECT_EQ(Pow(15, 6), 11390625);
}

TEST(PowTest, HandlesNegativeExponent) {
  EXPECT_TRUE(IsWithin(Pow(10, -1), 0.1, 0.0001));
  EXPECT_TRUE(IsWithin(Pow(5, -3), double(1) / 125, 0.0001));
  EXPECT_TRUE(IsWithin(Pow(15, -4), 0.0000197530864, 0.0001));
}

TEST(Log2Test, HandlesPositiveInputs) {
  EXPECT_EQ(Log2(1), 0);
  EXPECT_EQ(Log2(2), 1);
  EXPECT_EQ(Log2(1073741824), 30);
}

TEST(Log2Test, XMustBePositive) {
  try {
    Log2(0);
    FAIL() << "Expected std::out_of_range";
  } catch (std::out_of_range const &e) {
    EXPECT_EQ(e.what(),
              std::string("Can only calculate the log of positive values."));
  } catch (...) {
    FAIL() << "Expected std::out_of_range";
  }

  try {
    Log2(-30);
    FAIL() << "Expected std::out_of_range";
  } catch (std::out_of_range const &e) {
    EXPECT_EQ(e.what(),
              std::string("Can only calculate the log of positive values."));
  } catch (...) {
    FAIL() << "Expected std::out_of_range";
  }
}

TEST(LogTest, HandlesPositiveInputs) {
  ASSERT_TRUE(IsWithin(Log(5, 3), 1.46497352, 0.0001));
}

} // namespace
} // namespace math_test
} // namespace math