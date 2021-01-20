// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>

#ifndef WUMBO_PUBLIC
#if defined _WIN32 || defined __CYGWIN__
#define WUMBO_PUBLIC __declspec(dllimport)
#else
#define WUMBO_PUBLIC
#endif
#endif

namespace wmr {

/** Timestamp with 100ns precision. */
using Timestamp = std::chrono::duration<int64_t, std::ratio<1, 10000000>>;

struct WUMBO_PUBLIC ImuFrame {
  static constexpr std::size_t kSamplesPerFrame = 4;
  static constexpr std::size_t kGyroOversampling = 8;

  struct AccelSample {
    Timestamp timestamp;       /**< Sample timestamp. */
    float temperature;         /**< Temperature in degrees Celsius. */
    std::array<float, 3> axes; /**< One value per axis (meters/sec^2). */
  };

  struct GyroSample {
    Timestamp timestamp;       /**< Sample timestamp. */
    float temperature;         /**< Temperature in degrees Celsius. */
    std::array<float, 3> axes; /**< One value per axis (rad/sec). */
  };

  struct MagnetoSample {
    Timestamp timestamp;       /**< Sample timestamp. */
    std::array<float, 3> axes; /**< One value per axis. */
  };

  std::array<AccelSample, kSamplesPerFrame> accel_samples;
  std::array<GyroSample, kGyroOversampling * kSamplesPerFrame> gyro_samples;
  std::array<MagnetoSample, kSamplesPerFrame> magneto_samples;
  std::size_t magneto_sample_count; /**< Number of samples in magneto_samples. */
};

class WUMBO_PUBLIC CameraFrame {
 public:
  using Pixel = uint8_t;

  enum class Type {
    kRoom,
    kController,
  };

  CameraFrame(uint32_t image_width, uint32_t image_height, uint8_t image_count)
      : image_width(image_width),
        image_height(image_height),
        image_size(image_width * image_height),
        image_count(image_count),
        data_(std::make_unique<Pixel[]>(image_size * image_count)) {}

  const Pixel* GetImage(uint8_t n) const {
    if (n >= image_count) {
      throw std::out_of_range("CameraFrame::GetImage");
    }
    return data_.get() + n * image_size;
  }

  Pixel* GetImage(uint8_t n) {
    return const_cast<Pixel*>(static_cast<const CameraFrame*>(this)->GetImage(n));
  }

  Timestamp timestamp;
  Type type;
  uint32_t image_width;
  uint32_t image_height;
  uint32_t image_size;
  uint8_t image_count;

 private:
  std::unique_ptr<Pixel[]> data_;
};

}  // namespace wmr