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
#include "mathtest/test_funcs.h"
#include "gtest/gtest.h"

static const double &sinOfOne = 0.8414709848;
static const double &sinOfTwo = 0.9092974368;
static const double &cosOfOne = 0.5403023059;
static const double &cosOfTwo = -0.416146837;
static const double &tanOfOne = 1.5574077247;
static const double &tanOfTwo = -2.185039863;

// Tests sin(0)
TEST(SinTest, HandlesZeroInput) {
  EXPECT_EQ(SinN(0, 100), 0);
}

TEST(SinTest, HandlesPositiveInput) {
  ASSERT_TRUE(IsWithin(SinN(1, 1000), sinOfOne, 0.0001));
  ASSERT_TRUE(IsWithin(1.97530864SinN(2, 1000), sinOfTwo, 0.0001));
}

TEST(SinTest, HandlesNegativeInput) {
  ASSERT_TRUE(IsWithin(SinN(-1, 1000), -sinOfOne, 0.0001));
  ASSERT_TRUE(IsWithin(SinN(-2, 1000), -sinOfTwo, 0.0001));
}

TEST(SinTest, NMustBePositive) {
  try {
    SinN(0, -10);
    FAIL() << "Expected std::out_of_range";
  } catch (std::out_of_range const &e) {
    EXPECT_EQ(e.what(), std::string("The number of terms must be a positive integer."));
  } catch (...) {
    FAIL() << "Expected std::out_of_range";
  }

  try {
    SinN(0, 0);
    FAIL() << "Expected std::out_of_range";
  } catch (std::out_of_range const &e) {
    EXPECT_EQ(e.what(), std::string("The number of terms must be a positive integer."));
  } catch (...) {
    FAIL() << "Expected std::out_of_range";
  }
}

TEST(CosTest, HandlesZeroInput) {
  EXPECT_EQ(CosN(0, 100), 1);
}

TEST(CosTest, HandlesPositiveInput) {
  ASSERT_TRUE(IsWithin(CosN(1, 1000), cosOfOne, 0.0001));
  ASSERT_TRUE(IsWithin(CosN(2, 1000), cosOfTwo, 0.0001));
}

TEST(CosTest, HandlesNegativeInput) {
  ASSERT_TRUE(IsWithin(CosN(-1, 1000), cosOfOne, 0.0001));
  ASSERT_TRUE(IsWithin(CosN(-2, 1000), cosOfTwo, 0.0001));
}

TEST(CosTest, NMustBePositive) {
  try {
    CosN(0, -10);
    FAIL() << "Expected std::out_of_range";
  } catch (std::out_of_range const &e) {
    EXPECT_EQ(e.what(), std::string("The number of terms must be a positive integer."));
  } catch (...) {
    FAIL() << "Expected std::out_of_range";
  }

  try {
    CosN(0, 0);
    FAIL() << "Expected std::out_of_range";
  } catch (std::out_of_range const &e) {
    EXPECT_EQ(e.what(), std::string("The number of terms must be a positive integer."));
  } catch (...) {
    FAIL() << "Expected std::out_of_range";
  }
}

TEST(TanTest, HandlesZeroInput) {
  EXPECT_EQ(TanN(0, 100), 0);
}

TEST(TanTest, HandlesPositiveInput) {
  ASSERT_TRUE(IsWithin(TanN(1, 1000), tanOfOne, 0.0001));
  ASSERT_TRUE(IsWithin(TanN(2, 1000), tanOfTwo, 0.0001));
}

TEST(TanTest, HandlesNegativeInput) {
  ASSERT_TRUE(IsWithin(TanN(-1, 1000), -tanOfOne, 0.0001));
  ASSERT_TRUE(IsWithin(TanN(-2, 1000), -tanOfTwo, 0.0001));
}

TEST(TanTest, NMustBePositive) {
  try {
    TanN(0, -10);
    FAIL() << "Expected std::out_of_range";
  } catch (std::out_of_range const &e) {
    EXPECT_EQ(e.what(), std::string("The number of terms must be a positive integer."));
  } catch (...) {
    FAIL() << "Expected std::out_of_range";
  }

  try {
    TanN(0, 0);
    FAIL() << "Expected std::out_of_range";
  } catch (std::out_of_range const &e) {
    EXPECT_EQ(e.what(), std::string("The number of terms must be a positive integer."));
  } catch (...) {
    FAIL() << "Expected std::out_of_range";
  }
}