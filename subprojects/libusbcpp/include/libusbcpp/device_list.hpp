// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>
#include <iterator>
#include <memory>

#include "core.hpp"
#include "device.hpp"
#include "error.hpp"

namespace libusbcpp {

class DeviceList : public DeviceListBase {
 public:
  DeviceList(ConstructTag, ContextBase::BasePointer ctx, c::libusb_device** list,
             std::size_t n_devices)
      : DeviceListBase{list, ctx}, n_devices_{n_devices} {}

  struct Iter {
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = std::shared_ptr<Device>;
    using pointer = value_type*;
    using reference = value_type&;

    Iter(const DeviceList& list, std::size_t idx) : list_(list), idx_(idx) {}

    reference operator*() {
      if (!dev_) dev_ = list_[idx_];
      return dev_;
    }

    pointer operator->() {
      if (!dev_) dev_ = list_[idx_];
      return &dev_;
    }

    Iter& operator++() {
      dev_.reset();
      idx_++;
      return *this;
    }

    Iter operator++(int) {
      Iter tmp = *this;
      ++(*this);
      return tmp;
    }

    friend bool operator==(const Iter& a, const Iter& b) { return a.idx_ == b.idx_; };

    friend bool operator!=(const Iter& a, const Iter& b) { return a.idx_ != b.idx_; };

   private:
    const DeviceList& list_;
    std::size_t idx_;
    value_type dev_;
  };

  Iter begin() { return Iter(*this, 0); }
  Iter end() { return Iter(*this, n_devices_); }

  std::size_t size() const { return n_devices_; }

 private:
  std::size_t n_devices_;
};

inline DeviceList::Pointer ContextBase::GetDeviceList() {
  c::libusb_device** list;
  auto n_devices = ChkRet(c::libusb_get_device_list(ptr(), &list));

  return std::make_shared<DeviceList>(DeviceListBase::kConstructTag, shared_from_this(), list,
                                      n_devices);
}

}  // namespace libusbcpp
