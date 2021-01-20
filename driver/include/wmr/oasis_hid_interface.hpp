// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "types.hpp"

#ifndef WUMBO_PUBLIC
#if defined _WIN32 || defined __CYGWIN__
#define WUMBO_PUBLIC __declspec(dllimport)
#else
#define WUMBO_PUBLIC
#endif
#endif

namespace wmr {

struct WUMBO_PUBLIC OasisHidInterface {
  using ImuFrameHandle = std::shared_ptr<const ImuFrame>;

  /** Callback shall return true if it should be called again. */
  using ImuFrameCallback = std::function<bool(ImuFrameHandle)>;

  virtual ~OasisHidInterface() = default;

  virtual void StartImu() = 0;

  virtual void StopImu() = 0;

  virtual void RegisterImuFrameCallback(ImuFrameCallback cb) = 0;

  virtual std::string ReadCalibration() = 0;

  virtual std::basic_string<uint8_t> ReadDeviceInfo() = 0;

  virtual void WriteHidCmd(uint8_t command, uint8_t mystery_byte = 0) = 0;
};

}  // namespace wmr
