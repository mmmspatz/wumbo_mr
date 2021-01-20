// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>

namespace wmr {

/** Manages a hidapi device.
 * A reader thread calls hid_read_timeout in a loop and deals each report thus yielded to a single
 * registered ReportReader instance based on its report_id. Reports for which there isn't a
 * registered reader are discarded. This way, multiple readers can listen for reports at once.
 */
struct HidDevice {
  using Byte = uint8_t;
  using BufferView = std::basic_string_view<Byte>;

  HidDevice(unsigned short vendor_id, unsigned short product_id,
            const wchar_t *serial_number = nullptr);
  ~HidDevice();

  static constexpr std::size_t kMaxReportSize = 1024;

  struct ReportReader {
    using Report = BufferView;

    virtual ~ReportReader() = default;
    virtual void Update(BufferView report) = 0;
    virtual bool Finished() { return false; }
  };

  void WriteReport(BufferView report);

  void SetFeatureReport(BufferView report);

  void GetFeatureReport(void *report, std::size_t report_size);

  void RegisterReportReader(Byte report_id, std::shared_ptr<ReportReader> reader);

  void DeregisterReportReader(Byte report_id);

 private:
  struct HidDevDeleter {
    void operator()(void *dev);
  };

  static constexpr int kReadLoopTimeoutMs = 100;

  void ReadThreadFunc();

  std::unique_ptr<void, HidDevDeleter> hid_dev_;
  std::array<std::weak_ptr<ReportReader>, 256> report_readers_;
  std::mutex report_readers_m_;
  std::atomic_flag run_;
  std::thread reader_thread_;
};

}  // namespace wmr
