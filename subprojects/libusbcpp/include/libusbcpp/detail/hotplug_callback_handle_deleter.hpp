// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <memory>

#include "../core.hpp"

namespace libusbcpp {

namespace detail {

/** Custom deleter used in return type of Context::HotplugRegisterCallback.
 * Like many functions in this library, the return type of Context::HotplugRegisterCallback is a
 * specialization of std::unique_ptr with a custom deleter that calls the C API function that frees
 * the held resource. In this case, however, the resource handle is not a pointer, it's an int.
 * HotplugCallbackHandleDeleter::pointer allows us to sneak that int into a std::unique_ptr anyways.
 */
struct HotplugCallbackHandleDeleter {
  /** Wrapper for libusb_hotplug_callback_handle.
   * Meets the named requirements of a "NullablePointer".
   */
  class pointer {
   public:
    pointer(c::libusb_hotplug_callback_handle hnd = kNullValue) : hnd_(hnd) {}
    pointer(std::nullptr_t) : hnd_(kNullValue) {}

    explicit operator bool() const { return hnd_ != kNullValue; }

    friend bool operator==(pointer l, pointer r) { return l.hnd_ == r.hnd_; }

    friend bool operator!=(pointer l, pointer r) { return !(l == r); }

    /** Indirection operator is just a getter. */
    c::libusb_hotplug_callback_handle& operator*() { return hnd_; }

   private:
    // Valid handles are positive, so -1 is a sensible "null" value
    static constexpr c::libusb_hotplug_callback_handle kNullValue = -1;

    c::libusb_hotplug_callback_handle hnd_;
  };

  HotplugCallbackHandleDeleter(ContextBase::BasePointer ctx) : ctx_(ctx) {}

  void operator()(pointer hnd) { c::libusb_hotplug_deregister_callback(ctx_->ptr(), *hnd); }

 private:
  ContextBase::BasePointer ctx_;
};

}  // namespace detail

}  // namespace libusbcpp
