// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <stdexcept>

#include "core.hpp"

namespace libusbcpp {

template <c::libusb_error error_code>
struct Error : std::exception {
  const char *what() const noexcept final { return c::libusb_error_name(error_code); }
};

inline unsigned int ChkRet(int ret) {
  if (ret >= 0) {
    return static_cast<unsigned int>(ret);
  }

  switch (ret) {
#define HANDLE(ERROR_CODE) \
  case c::ERROR_CODE:      \
    throw Error<c::ERROR_CODE>();
    HANDLE(LIBUSB_ERROR_IO)
    HANDLE(LIBUSB_ERROR_INVALID_PARAM)
    HANDLE(LIBUSB_ERROR_ACCESS)
    HANDLE(LIBUSB_ERROR_NO_DEVICE)
    HANDLE(LIBUSB_ERROR_NOT_FOUND)
    HANDLE(LIBUSB_ERROR_BUSY)
    HANDLE(LIBUSB_ERROR_TIMEOUT)
    HANDLE(LIBUSB_ERROR_OVERFLOW)
    HANDLE(LIBUSB_ERROR_PIPE)
    HANDLE(LIBUSB_ERROR_INTERRUPTED)
    HANDLE(LIBUSB_ERROR_NO_MEM)
    HANDLE(LIBUSB_ERROR_NOT_SUPPORTED)
    HANDLE(LIBUSB_ERROR_OTHER)
#undef HANDLE
    default:
      throw std::runtime_error("Unknown libusb error code");
  }
}

}  // namespace libusbcpp