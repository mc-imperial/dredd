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

#include <iostream>
#include <thread>
#include <vector>

#include "consumer.h"
#include "locked_queue.h"
#include "producer.h"

int main() {
#define NUM_ELEMS 8192
  queue::LockedQueue producer_to_consumers(1024);
  queue::LockedQueue consumers_to_producer(8);
  int result = 0;
  std::vector<std::thread> threads;

  threads.emplace_back(Producer, std::ref(producer_to_consumers),
                       std::ref(consumers_to_producer), NUM_ELEMS, 8,
                       std::ref(result));

  for (size_t i = 0; i < 8; i++) {
    threads.emplace_back(Consumer, std::ref(producer_to_consumers),
                         std::ref(consumers_to_producer), NUM_ELEMS, 8);
  }

  for (auto &thread : threads) {
    thread.join();
  }

  std::cout << result << std::endl;
  return 0;
}
