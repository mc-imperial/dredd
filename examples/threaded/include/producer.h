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

#ifndef PRODUCER_H_
#define PRODUCER_H_

#include <cstddef>

#include "locked_queue.h"

void Producer(queue::LockedQueue &producer_to_consumers,
              queue::LockedQueue &consumers_to_producer, size_t num_elems,
              size_t num_consumers, int &result);

#endif // PRODUCER_H_
