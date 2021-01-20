// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include "libusb_event_thread.hpp"

#include <spdlog/spdlog.h>

namespace wmr {

LibusbEventThread::LibusbEventThread(libusbcpp::ContextBase::BasePointer ctx) : ctx_(ctx) {
  run_.test_and_set();
  event_thread_ = std::thread([this]() { EventThreadFunc(); });
}

LibusbEventThread::~LibusbEventThread() {
  spdlog::trace("stopping LibusbEventThread::EventThreadFunc");

  run_.clear();
  event_thread_.join();
}

void LibusbEventThread::EventThreadFunc() {
  struct timeval tv {};
  tv.tv_sec = kLoopTimeoutSec;
  while (run_.test_and_set(std::memory_order_acquire)) {
    libusbcpp::c::libusb_handle_events_timeout(ctx_->ptr(), &tv);
  }
  spdlog::trace("LibusbEventThread::EventThreadFunc exit");
}

}  // namespace wmr
