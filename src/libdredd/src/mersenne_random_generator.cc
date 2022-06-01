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

#include "libdredd/mersenne_random_generator.h"

#include <cassert>

namespace dredd {

MersenneRandomGenerator::MersenneRandomGenerator(std::mt19937& twister)
    : twister_(twister) {}

size_t MersenneRandomGenerator::GetSize(size_t bound) {
  assert(bound > 0 && "Bound must be positive");
  return std::uniform_int_distribution<size_t>(0, bound - 1)(twister_);
}

}  // namespace dredd
