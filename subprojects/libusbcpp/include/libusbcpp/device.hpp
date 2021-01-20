// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "core.hpp"
#include "detail/delete_functor.hpp"
#include "error.hpp"

namespace libusbcpp {

class Device : public DeviceBase {
 public:
  using ConfigDescriptor = std::unique_ptr<c::libusb_config_descriptor,
                                           detail::DeleteFunctor<c::libusb_free_config_descriptor>>;

  Device(ConstructTag, c::libusb_device* dev, ContextBase::BasePointer ctx) : DeviceBase{dev, ctx} {
    c::libusb_ref_device(dev);
  }

  auto GetBusNumber() { return c::libusb_get_bus_number(ptr()); }
  auto GetPortNumber() { return c::libusb_get_port_number(ptr()); }

  auto GetPortNumbers() {
    std::basic_string<uint8_t> port_numbers(7, 0xFF);
    auto port_numbers_len =
        ChkRet(c::libusb_get_port_numbers(ptr(), port_numbers.data(), port_numbers.size()));
    port_numbers.resize(port_numbers_len);

    return port_numbers;
  }

  auto GetAddress() { return c::libusb_get_device_address(ptr()); }
  auto GetSpeed() { return c::libusb_get_device_speed(ptr()); }

  auto GetMaxPacketSize(unsigned char endpoint) {
    return ChkRet(c::libusb_get_max_packet_size(ptr(), endpoint));
  }

  auto GetMaxIsoPacketSize(unsigned char endpoint) {
    return ChkRet(c::libusb_get_max_iso_packet_size(ptr(), endpoint));
  }

  // descriptors

  c::libusb_device_descriptor GetDeviceDescriptor() {
    c::libusb_device_descriptor desc;
    ChkRet(c::libusb_get_device_descriptor(ptr(), &desc));
    return desc;
  }

  ConfigDescriptor GetActiveConfigDescriptor() {
    c::libusb_config_descriptor* config;
    ChkRet(c::libusb_get_active_config_descriptor(ptr(), &config));
    return ConfigDescriptor(config);
  }

  ConfigDescriptor GetConfigDescriptor(uint8_t config_index) {
    c::libusb_config_descriptor* config;
    ChkRet(c::libusb_get_config_descriptor(ptr(), config_index, &config));
    return ConfigDescriptor(config);
  }

  ConfigDescriptor GetConfigDescriptorByValue(uint8_t bConfigurationValue) {
    c::libusb_config_descriptor* config;
    ChkRet(c::libusb_get_config_descriptor_by_value(ptr(), bConfigurationValue, &config));
    return ConfigDescriptor(config);
  }
};

inline Device::Pointer DeviceListBase::operator[](std::size_t i) const {
  return std::make_shared<Device>(DeviceBase::kConstructTag, ptr()[i], ctx_);
}

}  // namespace libusbcpp
