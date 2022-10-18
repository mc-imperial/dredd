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

#ifndef LOCKED_QUEUE_H_
#define LOCKED_QUEUE_H_

#include <cassert>
#include <condition_variable>
#include <mutex>
#include <vector>

namespace queue {

class LockedQueue {
public:
  explicit LockedQueue(size_t capacity);

  void Enq(const int &elem);

  int Deq();

private:
  std::vector<int> contents_;
  size_t count_;
  size_t head_;
  size_t tail_;
  std::mutex mutex_;
  std::condition_variable not_full_;
  std::condition_variable not_empty_;
};

} // namespace queue

#endif // LOCKED_QUEUE_H_
