#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#ifndef WUMBO_PUBLIC
#if defined _WIN32 || defined __CYGWIN__
// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#define WUMBO_PUBLIC __declspec(dllimport)
#else
#define WUMBO_PUBLIC
#endif
#endif

namespace wmr {

struct UsbDevId {
  uint16_t vid;
  uint16_t pid;
};

struct HeadsetSpec {
  std::string_view product_name;
  UsbDevId hid_comms_dev;
  UsbDevId camera_dev;
  UsbDevId vendor_hid_dev;
  std::size_t n_cameras;
  std::size_t camera_width;
  std::size_t camera_height;
  std::size_t camera_xfer_size;
  std::size_t camera_frame_size;
  std::size_t camera_frame_footer_offset;
  std::size_t camera_segment_size;
  std::size_t camera_segment_count;
};

}  // namespace wmr
