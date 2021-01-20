// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cassert>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace wmr {

template <class Frame>
class FramePool {
 public:
  template <class... Args>
  FramePool(std::size_t n_slots, const Args&... args) : n_slots_(n_slots) {
    assert(n_slots_ > 0);

    // Construct Frames in places
    slots_.reserve(n_slots_);
    for (std::size_t i = 0; i < n_slots_; ++i) {
      slots_.emplace_back(std::piecewise_construct, std::make_tuple(), std::tie(args...));
    }

    // Initialize singly linked list
    on_deck_ = &slots_[0];
    for (std::size_t i = 0; i < n_slots_ - 1; ++i) {
      slots_[i].first = &slots_[i + 1];
    }
    slots_[n_slots_ - 1].first = nullptr;
  }

  ~FramePool() {
    // Wait until all slots are returned to the pool
    std::unique_lock l{on_deck_m_};
    on_deck_cv_.wait(l, [this]() {
      std::size_t n_free_slots = 0;
      for (Slot* s = on_deck_; s; s = s->first) {
        ++n_free_slots;
      }

      return n_free_slots == n_slots_;
    });
  }

  std::shared_ptr<Frame> Allocate() {
    std::lock_guard l{on_deck_m_};

    Slot* s = on_deck_;
    if (!s) throw std::bad_alloc();
    on_deck_ = s->first;

    auto deleter = [this](Slot* slot) {
      {
        std::lock_guard l{on_deck_m_};
        slot->first = on_deck_;
        on_deck_ = slot;
      }
      on_deck_cv_.notify_one();
    };

    // Use std::shared_ptr's aliasing constructor
    return {std::shared_ptr<Slot>(s, deleter), &s->second};
  }

 private:
  // first points to the Slot Allocate should return next
  struct Slot : std::pair<Slot*, Frame> {
    using std::pair<Slot*, Frame>::pair;
  };

  std::size_t n_slots_;
  std::vector<Slot> slots_;
  Slot* on_deck_;
  std::mutex on_deck_m_;
  std::condition_variable on_deck_cv_;
};

}  // namespace wmr
