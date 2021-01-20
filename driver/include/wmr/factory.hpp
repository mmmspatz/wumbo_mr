// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <memory>

#include "oasis_hid_interface.hpp"

#ifndef WUMBO_PUBLIC
#if defined _WIN32 || defined __CYGWIN__
#define WUMBO_PUBLIC __declspec(dllimport)
#else
#define WUMBO_PUBLIC
#endif
#endif

namespace wmr {

struct WUMBO_PUBLIC Factory {
  static std::unique_ptr<OasisHidInterface> CreateOasisHid(unsigned short vendor_id,
                                                           unsigned short product_id,
                                                           const wchar_t *serial_number = nullptr);
};

}  // namespace wmr