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
#include "gtest/gtest.h"

namespace math {
namespace math_test {
namespace {

TEST(CeilTest, HandlesZeroInput) { EXPECT_EQ(Ceil(0), 0); }

TEST(CeilTest, HandlesIntegerInput) {
  EXPECT_EQ(Ceil(3), 3);
  EXPECT_EQ(Ceil(58325), 58325);
  EXPECT_EQ(Ceil(643), 643);
  EXPECT_EQ(Ceil(9832065), 9832065);
  EXPECT_EQ(Ceil(1243), 1243);
}

TEST(CeilTest, HandlesDecimalInput) {
  EXPECT_EQ(Ceil(45.235), 46);
  EXPECT_EQ(Ceil(3252.2352653), 3253);
  EXPECT_EQ(Ceil(643.324636), 644);
  EXPECT_EQ(Ceil(9832065.634643), 9832066);
  EXPECT_EQ(Ceil(25.23512), 26);
}

TEST(ModTest, HandlesZeroInput) { EXPECT_EQ(Mod(0), 0); }

TEST(ModTest, HandlesPositiveInput) {
  EXPECT_EQ(Mod(3), 3);
  EXPECT_EQ(Mod(58325), 58325);
  EXPECT_EQ(Mod(643.32523), 643.32523);
  EXPECT_EQ(Mod(9832065.325235), 9832065.325235);
  EXPECT_EQ(Mod(1243.32532), 1243.32532);
}

TEST(ModTest, HandlesNegativeInput) {
  EXPECT_EQ(Mod(-45.235), 45.235);
  EXPECT_EQ(Mod(-3252.2352653), 3252.2352653);
  EXPECT_EQ(Mod(-643.324636), 643.324636);
  EXPECT_EQ(Mod(-9832065), 9832065);
  EXPECT_EQ(Mod(-25), 25);
}

TEST(FactorialTest, HandlesZeroInput) { EXPECT_EQ(Factorial(0), 1); }

TEST(FactorialTest, HandlesPositiveInput) {
  EXPECT_EQ(Factorial(1), 1);
  EXPECT_EQ(Factorial(2), 2);
  EXPECT_EQ(Factorial(15), 1307674368000);
}

TEST(FactorialTest, HandlesNegativeInput) {
  try {
    Factorial(-10);
    FAIL() << "Expected std::out_of_range";
  } catch (std::out_of_range const &e) {
    EXPECT_EQ(e.what(),
              std::string("Can only take factorial of non-negative values."));
  } catch (...) {
    FAIL() << "Expected std::out_of_range";
  }
}

TEST(CombTest, HandlesZeroInput) { EXPECT_EQ(Comb(0, 0), 1); }

TEST(CombTest, HandlesPositiveInput) {
  EXPECT_EQ(Comb(15, 7), 6435);
  EXPECT_EQ(Comb(6, 4), 15);
  EXPECT_EQ(Comb(20, 7), 77520);
}

TEST(CombTest, HandlesNegativeInput) {
  try {
    Comb(-10, 1);
    FAIL() << "Expected std::out_of_range";
  } catch (std::out_of_range const &e) {
    EXPECT_EQ(e.what(), std::string("n and k must be positive."));
  } catch (...) {
    FAIL() << "Expected std::out_of_range";
  }

  try {
    Comb(1, -5);
    FAIL() << "Expected std::out_of_range";
  } catch (std::out_of_range const &e) {
    EXPECT_EQ(e.what(), std::string("n and k must be positive."));
  } catch (...) {
    FAIL() << "Expected std::out_of_range";
  }

  try {
    Comb(-1, -8);
    FAIL() << "Expected std::out_of_range";
  } catch (std::out_of_range const &e) {
    EXPECT_EQ(e.what(), std::string("n and k must be positive."));
  } catch (...) {
    FAIL() << "Expected std::out_of_range";
  }
}

} // namespace
} // namespace math_test
} // namespace math