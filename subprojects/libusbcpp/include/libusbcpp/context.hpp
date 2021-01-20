// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <memory>

#include "core.hpp"
#include "detail/delete_functor.hpp"
#include "detail/hotplug_callback_handle_deleter.hpp"
#include "error.hpp"

namespace libusbcpp {

class Context : public ContextBase {
 public:
  using SsEndpointCompanionDescriptor =
      std::unique_ptr<c::libusb_ss_endpoint_companion_descriptor,
                      detail::DeleteFunctor<c::libusb_free_ss_endpoint_companion_descriptor>>;

  using Usb2ExtensionDescriptor =
      std::unique_ptr<c::libusb_usb_2_0_extension_descriptor,
                      detail::DeleteFunctor<c::libusb_free_usb_2_0_extension_descriptor>>;

  using SsUsbDeviceCapabilityDescriptor =
      std::unique_ptr<c::libusb_ss_usb_device_capability_descriptor,
                      detail::DeleteFunctor<c::libusb_free_ss_usb_device_capability_descriptor>>;

  using ContainerIdDescriptor =
      std::unique_ptr<c::libusb_container_id_descriptor,
                      detail::DeleteFunctor<c::libusb_free_container_id_descriptor>>;

  using HotplugCallbackHandle =
      std::unique_ptr<c::libusb_hotplug_callback_handle, detail::HotplugCallbackHandleDeleter>;

  Context(ConstructTag)
      : ContextBase{[]() {
          c::libusb_context* ctx;
          c::libusb_init(&ctx);
          return ctx;
        }()} {}

  // Descriptors

  SsEndpointCompanionDescriptor GetSsEndpointCompanionDescriptor(
      const c::libusb_endpoint_descriptor* endpoint) {
    c::libusb_ss_endpoint_companion_descriptor* ep_comp;
    ChkRet(c::libusb_get_ss_endpoint_companion_descriptor(ptr(), endpoint, &ep_comp));

    return SsEndpointCompanionDescriptor(ep_comp);
  }

  Usb2ExtensionDescriptor GetUsb2ExtensionDescriptor(
      c::libusb_bos_dev_capability_descriptor* dev_cap) {
    c::libusb_usb_2_0_extension_descriptor* usb_2_0_extension;
    c::libusb_get_usb_2_0_extension_descriptor(ptr(), dev_cap, &usb_2_0_extension);
    return Usb2ExtensionDescriptor(usb_2_0_extension);
  }

  SsUsbDeviceCapabilityDescriptor GetSsUsbDeviceCapabilityDescriptor(
      c::libusb_bos_dev_capability_descriptor* dev_cap) {
    c::libusb_ss_usb_device_capability_descriptor* ss_usb_device_cap;
    c::libusb_get_ss_usb_device_capability_descriptor(ptr(), dev_cap, &ss_usb_device_cap);
    return SsUsbDeviceCapabilityDescriptor(ss_usb_device_cap);
  }

  ContainerIdDescriptor GetContainerIdDescriptor(c::libusb_bos_dev_capability_descriptor* dev_cap) {
    c::libusb_container_id_descriptor* container_id;
    c::libusb_get_container_id_descriptor(ptr(), dev_cap, &container_id);

    return ContainerIdDescriptor(container_id);
  }

  // Device hotplug event notification

  HotplugCallbackHandle HotplugRegisterCallback(c::libusb_hotplug_event events,
                                                c::libusb_hotplug_flag flags, int vendor_id,
                                                int product_id, int dev_class,
                                                c::libusb_hotplug_callback_fn cb_fn,
                                                void* user_data) {
    c::libusb_hotplug_callback_handle callback_handle;
    ChkRet(c::libusb_hotplug_register_callback(ptr(), events, flags, vendor_id, product_id,
                                               dev_class, cb_fn, user_data, &callback_handle));

    return HotplugCallbackHandle(callback_handle,
                                 detail::HotplugCallbackHandleDeleter{shared_from_this()});
  }
};

inline ContextBase::Pointer ContextBase::Create() {
  return std::make_shared<Context>(kConstructTag);
}

}  // namespace libusbcpp
