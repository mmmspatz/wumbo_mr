// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <memory>

namespace libusbcpp {
namespace c {
#include <libusb-1.0/libusb.h>
}

namespace detail {

template <class T, class Impl, auto del, auto... del_bound_extra_args>
class Base {
 public:
  using Pointer = std::shared_ptr<Impl>;
  using BasePointer = std::shared_ptr<Base>;

  /** Return a raw pointer to the managed object. */
  T* ptr() const { return ptr_; }

  // Not movable or copyable
  Base(const Base&) = delete;
  Base& operator=(const Base&) = delete;

 protected:
  static constexpr struct ConstructTag {
    // explicit constructor prevents conversion from empty initializer list
    explicit constexpr ConstructTag() = default;
  } kConstructTag{};

  explicit Base(T* ptr) : ptr_(ptr) {}
  virtual ~Base() { del(ptr_, del_bound_extra_args...); }

 private:
  T* ptr_;
};

}  // namespace detail

// Forward declarations of implementations
class Context;
class DeviceList;
class Device;
class DeviceHandle;
class Transfer;

/** Base class for Context. */
struct ContextBase : detail::Base<c::libusb_context, Context, c::libusb_exit>,
                     std::enable_shared_from_this<ContextBase> {
  /** Create an instance of Context.
   * Defined in context.hpp.
   */
  static std::shared_ptr<Context> Create();

  /** Create an instance of DeviceList.
   * Defined in device_list.hpp.
   */
  std::shared_ptr<DeviceList> GetDeviceList();

  /** Create an instance of DeviceHandle.
   * Defined in device_handle.hpp.
   */
  std::shared_ptr<DeviceHandle> OpenWithVidPid(uint16_t vendor_id, uint16_t product_id);

 protected:
  using Base::Base;
};

/** Base class for DeviceList. */
struct DeviceListBase : detail::Base<c::libusb_device*, DeviceList, c::libusb_free_device_list, 1> {
  friend std::shared_ptr<DeviceList> ContextBase::GetDeviceList();

  /** Create an instance of Device.
   * Defined in device.hpp.
   */
  std::shared_ptr<Device> operator[](std::size_t i) const;

 protected:
  DeviceListBase(c::libusb_device** ptr, ContextBase::BasePointer ctx) : Base{ptr}, ctx_{ctx} {}

  ContextBase::BasePointer ctx_;
};

/** Base class for Device. */
struct DeviceBase : detail::Base<c::libusb_device, Device, c::libusb_unref_device> {
  friend std::shared_ptr<Device> DeviceListBase::operator[](std::size_t i) const;

  /** Create an instance of DeviceHandle.
   * Defined in device_handle.hpp.
   */
  std::shared_ptr<DeviceHandle> Open();

 protected:
  DeviceBase(c::libusb_device* ptr, ContextBase::BasePointer ctx) : Base{ptr}, ctx_{ctx} {}

  ContextBase::BasePointer ctx_;
};

/** Base class for DeviceHandle. */
struct DeviceHandleBase : detail::Base<c::libusb_device_handle, DeviceHandle, c::libusb_close>,
                          std::enable_shared_from_this<DeviceHandleBase> {
  friend std::shared_ptr<DeviceHandle> ContextBase::OpenWithVidPid(uint16_t vendor_id,
                                                                   uint16_t product_id);
  friend std::shared_ptr<DeviceHandle> DeviceBase::Open();

  /** Create an instance of Transfer.
   * Defined in transfer.hpp.
   */
  std::shared_ptr<Transfer> AllocTransfer(int iso_packets = 0);

 protected:
  using Base::Base;
};

/** Base class for Transfer. */
struct TransferBase : detail::Base<c::libusb_transfer, Transfer, c::libusb_free_transfer> {
  friend std::shared_ptr<Transfer> DeviceHandleBase::AllocTransfer(int iso_packets);

 protected:
  using Base::Base;
};

}  // namespace libusbcpp
