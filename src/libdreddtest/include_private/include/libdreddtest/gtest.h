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

#ifndef LIBDREDDTEST_GTEST_H
#define LIBDREDDTEST_GTEST_H

// This header file serves to act as a barrier between the gtest header file
// and test files that include them.  It uses compiler pragmas to disable
// diagnostics, in order to ignore warnings generated during the processing of
// the header file without having to compromise on freedom from warnings
// elsewhere in the project.

#if defined(__clang__)
#pragma clang diagnostic push
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4388)
#pragma warning(disable : 4668)
#endif

#include "gtest/gtest.h"  // IWYU pragma: export

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif  // LIBDREDDTEST_GTEST_H
