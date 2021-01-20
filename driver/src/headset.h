// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <memory>

#include <wmr/headset_interface.hpp>
#include <wmr/headset_spec.hpp>
#include <wmr/oasis_hid_interface.hpp>
#include <wmr/vendor_hid_interface.hpp>

#include "libusb_event_thread.hpp"

namespace wmr {

class Headset : public HeadsetInterface {
 public:
  Headset(const HeadsetSpec& spec, libusbcpp::ContextBase::BasePointer ctx,
          std::unique_ptr<OasisHidInterface> oasis_hid, std::unique_ptr<CameraInterface> camera,
          std::unique_ptr<VendorHidInterface> vendor_hid);

 private:
  void Open() final;
  void Close() final;
  CameraInterface& Camera() final { return *camera_; }
  OasisHidInterface& OasisHid() final { return *oasis_hid_; }
  VendorHidInterface& VendorHid() final { return *vendor_hid_; }

  HeadsetSpec spec_;
  LibusbEventThread usb_thread_;
  std::unique_ptr<OasisHidInterface> oasis_hid_;
  std::unique_ptr<CameraInterface> camera_;
  std::unique_ptr<VendorHidInterface> vendor_hid_;
};

}  // namespace wmr
