// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>

#include "core.hpp"
#include "detail/delete_functor.hpp"
#include "device.hpp"
#include "error.hpp"

namespace libusbcpp {

class DeviceHandle : public Device, public DeviceHandleBase {
 public:
  // resolve ambiguity caused by multiple inheritance
  using DeviceHandleBase::BasePointer;
  using DeviceHandleBase::Pointer;
  using DeviceHandleBase::ptr;

  using BosDescriptor = std::unique_ptr<c::libusb_bos_descriptor,
                                        detail::DeleteFunctor<c::libusb_free_bos_descriptor>>;

  DeviceHandle(DeviceHandleBase::ConstructTag, c::libusb_device* dev, ContextBase::BasePointer ctx,
               c::libusb_device_handle* dev_handle)
      : Device{DeviceBase::kConstructTag, dev, ctx}, DeviceHandleBase{dev_handle} {}

  auto GetConfiguration() {
    int config;
    ChkRet(c::libusb_get_configuration(ptr(), &config));
    return config;
  }

  void SetConfiguration(int configuration) {
    ChkRet(c::libusb_set_configuration(ptr(), configuration));
  }

  auto ClaimInterface(int interface_number) {
    ChkRet(c::libusb_claim_interface(ptr(), interface_number));

    auto deleter = [dev_handle = shared_from_this(), interface_number](void*) {
      ChkRet(c::libusb_release_interface(dev_handle->ptr(), interface_number));
    };

    return std::unique_ptr<void, decltype(deleter)>((void*)1, deleter);
  }

  void SetInterfaceAltSetting(int interface_number, int alternate_setting) {
    ChkRet(c::libusb_set_interface_alt_setting(ptr(), interface_number, alternate_setting));
  }

  void ClearHalt(unsigned char endpoint) { ChkRet(c::libusb_clear_halt(ptr(), endpoint)); }

  void ResetDevice() { ChkRet(c::libusb_reset_device(ptr())); }

  bool KernelDriverActive(int interface_number) {
    return ChkRet(c::libusb_kernel_driver_active(ptr(), interface_number));
  }

  bool DetachKernelDriver(int interface_number) {
    return ChkRet(c::libusb_detach_kernel_driver(ptr(), interface_number));
  }

  int AttachKernelDriver(int interface_number) {
    return ChkRet(c::libusb_attach_kernel_driver(ptr(), interface_number));
  }

  bool SetAutoDetachKernelDriver(int enable) {
    return ChkRet(c::libusb_set_auto_detach_kernel_driver(ptr(), enable));
  }

  // Descriptors

  BosDescriptor GetBosDescriptor() {
    c::libusb_bos_descriptor* bos;
    c::libusb_get_bos_descriptor(ptr(), &bos);
    return BosDescriptor(bos);
  }

  std::basic_string<uint8_t> GetStringDescriptorAscii(
      uint8_t desc_index, std::basic_string<uint8_t>::size_type max_size = 512) {
    std::basic_string<uint8_t> desc(max_size, 0);
    auto length =
        ChkRet(c::libusb_get_string_descriptor_ascii(ptr(), desc_index, desc.data(), desc.size()));
    desc.resize(length);

    return desc;
  }

  std::basic_string<uint8_t> GetDescriptor(uint8_t desc_type, uint8_t desc_index,
                                           std::basic_string<uint8_t>::size_type max_size = 512) {
    std::basic_string<uint8_t> desc(max_size, 0);
    auto length =
        ChkRet(c::libusb_get_descriptor(ptr(), desc_type, desc_index, desc.data(), desc.size()));
    desc.resize(length);

    return desc;
  }

  std::basic_string<uint8_t> GetStringDescriptor(
      uint8_t desc_index, uint16_t langid, std::basic_string<uint8_t>::size_type max_size = 512) {
    std::basic_string<uint8_t> desc(max_size, 0);
    auto length = ChkRet(
        c::libusb_get_string_descriptor(ptr(), desc_index, langid, desc.data(), desc.size()));
    desc.resize(length);

    return desc;
  }

  // Async IO

  auto DevMemAlloc(std::size_t length) {
    auto buff = c::libusb_dev_mem_alloc(ptr(), length);

    auto deleter = [dev_handle = shared_from_this(), length](unsigned char* m) {
      ChkRet(c::libusb_dev_mem_free(dev_handle->ptr(), m, length));
    };

    return std::unique_ptr<unsigned char, decltype(deleter)>(buff, deleter);
  }

  // Synchronous IO

  void ControlTransfer(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
                       unsigned char* data, uint16_t wLength, unsigned int timeout) {
    ChkRet(c::libusb_control_transfer(ptr(), bmRequestType, bRequest, wValue, wIndex, data, wLength,
                                      timeout));
  }

  void BulkTransfer(unsigned char endpoint, unsigned char* data, int length, int* actual_length,
                    unsigned int timeout) {
    ChkRet(c::libusb_bulk_transfer(ptr(), endpoint, data, length, actual_length, timeout));
  }

  void InterruptTransfer(unsigned char endpoint, unsigned char* data, int length,
                         int* actual_length, unsigned int timeout) {
    ChkRet(c::libusb_interrupt_transfer(ptr(), endpoint, data, length, actual_length, timeout));
  }
};

inline DeviceHandle::Pointer DeviceBase::Open() {
  c::libusb_device_handle* dev_handle = nullptr;
  ChkRet(c::libusb_open(ptr(), &dev_handle));

  return std::make_shared<DeviceHandle>(DeviceHandleBase::kConstructTag, ptr(), ctx_, dev_handle);
}

inline DeviceHandle::Pointer ContextBase::OpenWithVidPid(uint16_t vendor_id, uint16_t product_id) {
  c::libusb_device_handle* dev_handle =
      c::libusb_open_device_with_vid_pid(ptr(), vendor_id, product_id);

  if (!dev_handle) {
    throw std::runtime_error("Device not found");
  }

  return std::make_shared<DeviceHandle>(DeviceHandleBase::kConstructTag,
                                        c::libusb_get_device(dev_handle), shared_from_this(),
                                        dev_handle);
}

}  // namespace libusbcpp
