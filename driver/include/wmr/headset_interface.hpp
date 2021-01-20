// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "camera_interface.hpp"
#include "oasis_hid_interface.hpp"
#include "vendor_hid_interface.hpp"

#ifndef WUMBO_PUBLIC
#if defined _WIN32 || defined __CYGWIN__
#define WUMBO_PUBLIC __declspec(dllimport)
#else
#define WUMBO_PUBLIC
#endif
#endif

namespace wmr {

struct WUMBO_PUBLIC HeadsetInterface {
  virtual ~HeadsetInterface() = default;

  virtual void Open() = 0;
  virtual void Close() = 0;
  virtual CameraInterface& Camera() = 0;
  virtual OasisHidInterface& OasisHid() = 0;
  virtual VendorHidInterface& VendorHid() = 0;
};

}  // namespace wmr
