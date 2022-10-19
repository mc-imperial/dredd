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

#include "locked_queue.h"

namespace queue {

LockedQueue::LockedQueue(size_t capacity) : count_(0), head_(0), tail_(0) {
  contents_.resize(capacity);
}

void LockedQueue::Enq(const int &elem) {
  std::unique_lock<std::mutex> lock(mutex_);
  not_full_.wait(lock, [this]() -> bool { return count_ < contents_.size(); });
  contents_[tail_] = elem;
  tail_ = (tail_ + 1) % contents_.size();
  count_++;
  not_empty_.notify_one();
}

int LockedQueue::Deq() {
  std::unique_lock<std::mutex> lock(mutex_);
  not_empty_.wait(lock, [this]() -> bool { return count_ > 0; });
  const int &result = contents_[head_];
  head_ = (head_ + 1) % contents_.size();
  count_--;
  not_full_.notify_one();
  return result;
}

} // namespace queue
