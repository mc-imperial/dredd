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

#include "producer.h"

#include <cassert>

void Producer(queue::LockedQueue &producer_to_consumers,
              queue::LockedQueue &consumers_to_producer, size_t num_elems,
              size_t num_consumers, int &result) {
  assert((num_elems % num_consumers) == 0);
  for (int i = 0; i < num_elems; i++) {
    producer_to_consumers.Enq(1);
  }
  result = 0;
  for (int i = 0; i < num_consumers; i++) {
    result += consumers_to_producer.Deq();
  }
}
