// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include "hid_device.hpp"

#include <hidapi.h>
#include <spdlog/spdlog.h>

#include <stdexcept>

namespace wmr {

HidDevice::HidDevice(unsigned short vendor_id, unsigned short product_id,
                     const wchar_t *serial_number)
    : hid_dev_(hid_open(vendor_id, product_id, serial_number)) {
  if (!hid_dev_) {
    throw std::runtime_error("Failed to open HID device");
  }

  run_.test_and_set();
  reader_thread_ = std::thread([this]() { ReadThreadFunc(); });
}

HidDevice::~HidDevice() {
  run_.clear();
  reader_thread_.join();
}

void HidDevice::WriteReport(BufferView report) {
  auto bytes_written =
      hid_write(static_cast<hid_device *>(hid_dev_.get()), report.data(), report.size());
  if (bytes_written < 0) {
    throw std::runtime_error("hid_write failed");
  } else if (static_cast<BufferView::size_type>(bytes_written) != report.size()) {
    throw std::runtime_error("hid_write didn't consume entire buffer");
  }
}

void HidDevice::SetFeatureReport(BufferView report) {
  auto bytes_written = hid_send_feature_report(static_cast<hid_device *>(hid_dev_.get()),
                                               report.data(), report.size());
  if (bytes_written < 0) {
    throw std::runtime_error("hid_send_feature_report failed");
  } else if (static_cast<BufferView::size_type>(bytes_written) != report.size()) {
    throw std::runtime_error("hid_send_feature_report didn't consume entire buffer");
  }
}

void HidDevice::GetFeatureReport(void *report, std::size_t report_size) {
  auto bytes_read = hid_get_feature_report(static_cast<hid_device *>(hid_dev_.get()),
                                           static_cast<unsigned char *>(report), report_size);
  if (bytes_read < 0) {
    throw std::runtime_error("hid_send_feature_report failed");
  } else if (static_cast<BufferView::size_type>(bytes_read) != report_size) {
    throw std::runtime_error("hid_get_feature_report didn't consume entire buffer");
  }
}

void HidDevice::RegisterReportReader(Byte report_id, std::shared_ptr<ReportReader> reader) {
  std::lock_guard l(report_readers_m_);
  assert(!report_readers_[report_id].lock());
  report_readers_[report_id] = reader;
}

void HidDevice::DeregisterReportReader(Byte report_id) {
  std::lock_guard l(report_readers_m_);
  report_readers_[report_id].reset();
}

void HidDevice::ReadThreadFunc() {
  std::array<Byte, kMaxReportSize> rbuff;
  while (run_.test_and_set(std::memory_order_acquire)) {
    auto bytes_read = hid_read_timeout(static_cast<hid_device *>(hid_dev_.get()), rbuff.data(),
                                       rbuff.size(), kReadLoopTimeoutMs);

    if (bytes_read < 0) {
      throw std::runtime_error("HidDevice: hid_read_timeout failed");
    } else if (bytes_read == 0) {
      // kReadLoopTimeoutMs elapsed
      continue;
    }

    auto report_id = rbuff[0];
    std::shared_ptr<ReportReader> reader;
    {
      std::lock_guard l(report_readers_m_);
      reader = report_readers_[report_id].lock();
    }
    if (reader) {
      reader->Update(BufferView{rbuff.data(), static_cast<BufferView::size_type>(bytes_read)});
      if (reader->Finished()) DeregisterReportReader(report_id);
    }
  }
}

void HidDevice::HidDevDeleter::operator()(void *dev) { hid_close(static_cast<hid_device *>(dev)); }

}  // namespace wmr
