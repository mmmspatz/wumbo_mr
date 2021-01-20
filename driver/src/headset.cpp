
// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include "headset.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include <wmr/oasis_hid_interface.hpp>

#include "oasis_hid.hpp"

std::string w2s(std::wstring w) { return std::string(w.begin(), w.end()); }
std::wstring s2w(std::string s) { return std::wstring(s.begin(), s.end()); }

namespace wmr {
Headset::Headset(const HeadsetSpec& spec, libusbcpp::ContextBase::BasePointer ctx,
                 std::unique_ptr<OasisHidInterface> oasis_hid,
                 std::unique_ptr<CameraInterface> camera,
                 std::unique_ptr<VendorHidInterface> vendor_hid)
    : spec_(spec),
      usb_thread_(ctx),
      oasis_hid_(std::move(oasis_hid)),
      camera_(std::move(camera)),
      vendor_hid_(std::move(vendor_hid)) {}

void Headset::Open() {
  oasis_hid_->StartImu();
  camera_->StartStream();
}

void Headset::Close() {
  oasis_hid_->StopImu();
  camera_->StopStream();
}

}  // namespace wmr
