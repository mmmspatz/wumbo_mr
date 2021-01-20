// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstdint>
#include <memory>

#include "core.hpp"
#include "error.hpp"

namespace libusbcpp {

struct TransferStruct : c::libusb_transfer {
  void Submit() { ChkRet(c::libusb_submit_transfer(this)); }

  void Cancel() { ChkRet(c::libusb_cancel_transfer(this)); }

  void SetStreamId(uint32_t stream_id) { c::libusb_transfer_set_stream_id(this, stream_id); }

  uint32_t GetStreamId() { return c::libusb_transfer_get_stream_id(this); }

  unsigned char* ControlTransferGetData() { return c::libusb_control_transfer_get_data(this); }

  c::libusb_control_setup* ControlTransferGetSetup() {
    return c::libusb_control_transfer_get_setup(this);
  }

  void SetIsoPacketLengths(unsigned int length) { c::libusb_set_iso_packet_lengths(this, length); }

  unsigned char* GetIsoPacketBuffer(unsigned int packet) {
    return c::libusb_get_iso_packet_buffer(this, packet);
  }

  unsigned char* GetIsoPacketBufferSimple(unsigned int packet) {
    return c::libusb_get_iso_packet_buffer_simple(this, packet);
  }
};

class Transfer : public TransferBase {
 public:
  Transfer(ConstructTag, c::libusb_transfer* trans, std::shared_ptr<DeviceHandleBase> dev_handle)
      : TransferBase{trans}, dev_handle_{dev_handle} {}

  TransferStruct* AsStruct() { return static_cast<TransferStruct*>(ptr()); }

  void FillControlTransfer(unsigned char* buffer, c::libusb_transfer_cb_fn callback,
                           void* user_data, unsigned int timeout) {
    c::libusb_fill_control_transfer(ptr(), dev_handle_->ptr(), buffer, callback, user_data,
                                    timeout);
  }

  void FillBulkTransfer(unsigned char endpoint, unsigned char* buffer, int length,
                        c::libusb_transfer_cb_fn callback, void* user_data, unsigned int timeout) {
    c::libusb_fill_bulk_transfer(ptr(), dev_handle_->ptr(), endpoint, buffer, length, callback,
                                 user_data, timeout);
  }

  void FillBulkStreamTransfer(unsigned char endpoint, uint32_t stream_id, unsigned char* buffer,
                              int length, c::libusb_transfer_cb_fn callback, void* user_data,
                              unsigned int timeout) {
    c::libusb_fill_bulk_stream_transfer(ptr(), dev_handle_->ptr(), endpoint, stream_id, buffer,
                                        length, callback, user_data, timeout);
  }

  void FillInterruptTransfer(unsigned char endpoint, unsigned char* buffer, int length,
                             c::libusb_transfer_cb_fn callback, void* user_data,
                             unsigned int timeout) {
    c::libusb_fill_interrupt_transfer(ptr(), dev_handle_->ptr(), endpoint, buffer, length, callback,
                                      user_data, timeout);
  }

  void FillIsoTransfer(unsigned char endpoint, unsigned char* buffer, int length,
                       int num_iso_packets, c::libusb_transfer_cb_fn callback, void* user_data,
                       unsigned int timeout) {
    c::libusb_fill_iso_transfer(ptr(), dev_handle_->ptr(), endpoint, buffer, length,
                                num_iso_packets, callback, user_data, timeout);
  }

 private:
  std::shared_ptr<DeviceHandleBase> dev_handle_;
};

inline Transfer::Pointer DeviceHandleBase::AllocTransfer(int iso_packets) {
  if (c::libusb_transfer* trans = c::libusb_alloc_transfer(iso_packets)) {
    return std::make_unique<Transfer>(TransferBase::kConstructTag, trans, shared_from_this());
  } else {
    throw std::bad_alloc();
  }
}

}  // namespace libusbcpp
