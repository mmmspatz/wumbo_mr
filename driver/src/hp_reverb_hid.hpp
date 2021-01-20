// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

#include <wmr/vendor_hid_interface.hpp>

#include "hid_device.hpp"

namespace wmr {

class HpReverbHid : public VendorHidInterface {
 public:
  HpReverbHid(std::unique_ptr<HidDevice> hid_dev);

  void WakeDisplay() final;

 private:
  // set then get as feature 4x at startup
  struct __attribute__((packed)) MyteryReport80 {
    static constexpr uint8_t kReportId = 0x50;
    static constexpr std::size_t kReportSize = 64;

    uint8_t report_id;
    uint8_t mystery_byte_1;
    std::array<uint8_t, 62> data;
  };
  static_assert(sizeof(MyteryReport80) == MyteryReport80::kReportSize);

  // get as feature once at startup
  struct __attribute__((packed)) MyteryReport9 {
    static constexpr uint8_t kReportId = 0x09;
    static constexpr std::size_t kReportSize = 64;

    uint8_t report_id;
    std::array<uint8_t, 63> data;
  };
  static_assert(sizeof(MyteryReport9) == MyteryReport9::kReportSize);

  // get as feature once at startup
  struct __attribute__((packed)) MyteryReport8 {
    static constexpr uint8_t kReportId = 0x08;
    static constexpr std::size_t kReportSize = 64;

    uint8_t report_id;
    std::array<uint8_t, 63> data;
  };
  static_assert(sizeof(MyteryReport8) == MyteryReport8::kReportSize);

  // get as feature once at startup
  struct __attribute__((packed)) MyteryReport6 {
    static constexpr uint8_t kReportId = 0x06;
    static constexpr std::size_t kReportSize = 2;

    uint8_t report_id;
    uint8_t value;
  };
  static_assert(sizeof(MyteryReport6) == MyteryReport6::kReportSize);

  // Set as feature to 0x01 once at startup
  struct __attribute__((packed)) MyteryReport4 {
    static constexpr uint8_t kReportId = 0x04;
    static constexpr std::size_t kReportSize = 2;

    uint8_t report_id;
    uint8_t value;
  };
  static_assert(sizeof(MyteryReport4) == MyteryReport4::kReportSize);

  // Read as interrupt during operation
  struct __attribute__((packed)) MyteryReport5 {
    static constexpr uint8_t kReportId = 0x05;
    static constexpr std::size_t kReportSize = 33;

    uint8_t report_id;
    std::array<uint8_t, 32> data;
  };
  static_assert(sizeof(MyteryReport5) == MyteryReport5::kReportSize);

  struct MysteryReport5Reader : HidDevice::ReportReader {
    void Update(Report report) final;
  };

  // Read as interrupt during operation
  struct __attribute__((packed)) MyteryReport1 {
    static constexpr uint8_t kReportId = 0x01;
    static constexpr std::size_t kReportSize = 4;

    uint8_t report_id;
    uint8_t unknown_8;
    uint16_t unknown_16;
  };
  static_assert(sizeof(MyteryReport1) == MyteryReport1::kReportSize);

  struct MysteryReport1Reader : HidDevice::ReportReader {
    void Update(Report report) final;
  };

  std::unique_ptr<HidDevice> hid_dev_;
  std::shared_ptr<HidDevice::ReportReader> reader_5_;
  std::shared_ptr<HidDevice::ReportReader> reader_1_;
};

}  // namespace wmr
