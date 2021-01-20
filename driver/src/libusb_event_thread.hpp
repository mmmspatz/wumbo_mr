// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <atomic>
#include <thread>

#include <libusbcpp/core.hpp>

namespace wmr {

class LibusbEventThread {
 public:
  LibusbEventThread(libusbcpp::ContextBase::BasePointer ctx);
  ~LibusbEventThread();

 private:
  static constexpr int kLoopTimeoutSec = 1;
  void EventThreadFunc();

  libusbcpp::ContextBase::BasePointer ctx_;
  std::atomic_flag run_;
  std::thread event_thread_;
};

}  // namespace wmr
