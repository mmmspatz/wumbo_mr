// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstdint>
#include <functional>

#include "types.hpp"

#ifndef WUMBO_PUBLIC
#if defined _WIN32 || defined __CYGWIN__
#define WUMBO_PUBLIC __declspec(dllimport)
#else
#define WUMBO_PUBLIC
#endif
#endif

namespace wmr {

struct WUMBO_PUBLIC CameraInterface {
 public:
  using FrameHandle = std::shared_ptr<const CameraFrame>;

  /** Callback shall return true if it should be called again. */
  using FrameCallback = std::function<bool(FrameHandle)>;

  virtual ~CameraInterface() = default;

  virtual void StartStream() = 0;
  virtual void StopStream() = 0;
  virtual void SetExpGain(uint16_t camera_type, uint16_t exposure, uint16_t gain) = 0;
  virtual void RegisterFrameCallback(FrameCallback cb) = 0;
};

}  // namespace wmr
