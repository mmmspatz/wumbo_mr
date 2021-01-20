// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <spdlog/spdlog.h>

#include <memory>
#include <thread>
#include <type_traits>
#include <utility>

#include <libusbcpp/context.hpp>
#include <libusbcpp/device_handle.hpp>
#include <libusbcpp/device_list.hpp>
#include <wmr/create_headset.hpp>

#include "camera.hpp"
#include "headset.h"
#include "hp_reverb_hid.hpp"
#include "libusb_event_thread.hpp"
#include "oasis_hid.hpp"

namespace wmr {

std::vector<libusbcpp::Device::Pointer> FilterVidPid(libusbcpp::DeviceList& devices, uint16_t vid,
                                                     uint16_t pid) {
  std::vector<libusbcpp::Device::Pointer> matching_devs;
  std::copy_if(devices.begin(), devices.end(), std::back_inserter(matching_devs),
               [vid, pid](libusbcpp::Device::Pointer dev) {
                 auto desc = dev->GetDeviceDescriptor();
                 return (desc.idVendor == vid && desc.idProduct == pid);
               });

  return matching_devs;
}

std::shared_ptr<HeadsetInterface> CreateHeadset(const HeadsetSpec& spec) {
  auto ctx = libusbcpp::Context::Create();

  auto dev_list = ctx->GetDeviceList();

  auto hid_devs = FilterVidPid(*dev_list, spec.hid_comms_dev.vid, spec.hid_comms_dev.pid);

  auto cam_devs = FilterVidPid(*dev_list, spec.camera_dev.vid, spec.camera_dev.pid);

  auto vendor_hid_devs = FilterVidPid(*dev_list, spec.vendor_hid_dev.vid, spec.vendor_hid_dev.pid);

  if (hid_devs.size() != 1 || cam_devs.size() != 1 || vendor_hid_devs.size() != 1) {
    throw std::runtime_error("Headset not found");
  }

  auto& hid_dev = hid_devs.at(0);
  auto& cam_dev = cam_devs.at(0);
  auto& vendor_hid_dev = vendor_hid_devs.at(0);

  // Get the serial number of hid_dev
  auto hid_desc = hid_dev->GetDeviceDescriptor();
  auto hid_dev_hnd = hid_dev->Open();
  auto hid_sn = hid_dev_hnd->GetStringDescriptorAscii(hid_desc.iSerialNumber);
  std::wstring hid_sn_w(hid_sn.begin(), hid_sn.end());

  auto oasis_hid =
      std::make_unique<HidDevice>(hid_desc.idVendor, hid_desc.idProduct, hid_sn_w.c_str());

  // Get the serial number of vendor_hid_dev
  auto vendor_hid_desc = vendor_hid_dev->GetDeviceDescriptor();
  auto vendor_hid_dev_hnd = vendor_hid_dev->Open();
  auto vendor_hid_sn = vendor_hid_dev_hnd->GetStringDescriptorAscii(vendor_hid_desc.iSerialNumber);
  std::wstring vendor_hid_sn_w(vendor_hid_sn.begin(), vendor_hid_sn.end());

  auto vendor_hid = std::make_unique<HidDevice>(vendor_hid_desc.idVendor, vendor_hid_desc.idProduct,
                                                vendor_hid_sn_w.c_str());

  auto camera = std::make_unique<Camera>(spec, cam_dev);

  return std::make_shared<Headset>(spec, ctx, std::make_unique<OasisHid>(std::move(oasis_hid)),
                                   std::move(camera),
                                   std::make_unique<HpReverbHid>(std::move(vendor_hid)));
}

}  // namespace wmr
