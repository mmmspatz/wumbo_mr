// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>
#include <queue>
#include <thread>

#include <libusbcpp/device_handle.hpp>
#include <libusbcpp/transfer.hpp>
#include <wmr/camera_interface.hpp>
#include <wmr/headset_spec.hpp>

#include "frame_pool.hpp"

namespace wmr {

class Camera : public CameraInterface {
 public:
  Camera(const HeadsetSpec& spec_, libusbcpp::Device::Pointer dev);

 private:
  static constexpr int kCameraTypeCount = 8;
  static constexpr uint8_t kInterfaceNumber = 3;
  static constexpr uint32_t kMagic = 0x2b6f6c44;
  static constexpr std::size_t kRxSlotCount = 3;
  static constexpr std::size_t kFramePoolSize = 3;

  struct __attribute__((packed)) StartStopCommand {
    static constexpr std::size_t kSize = 12;
    uint32_t magic;
    uint32_t word1;  // 0x0c
    uint16_t word2;  // 0x81 start, 0x82 stop
    uint16_t _reserved2_a;
  };
  static_assert(sizeof(StartStopCommand) == StartStopCommand::kSize);

  // resent at least every 60 tries
  struct __attribute__((packed)) SetExpGainCommand {
    static constexpr std::size_t kSize = 18;
    uint32_t magic;
    uint32_t word1;        // 0x12
    uint16_t word2;        // 0x80
    uint16_t camera_type;  // 0 through 7
    uint16_t exposure;
    uint16_t gain;
    uint16_t camera_type_dup;
  };
  static_assert(sizeof(SetExpGainCommand) == SetExpGainCommand::kSize);

  struct ExpGainState {
    uint16_t exposure;
    uint16_t gain;
    uint16_t cache_use_count;
  };

  struct FrameFooter {
    static constexpr uint16_t kFrameTypeRoom = 0;
    static constexpr uint16_t kFrameTypeController = 2;

    uint64_t timestamp;

    uint64_t sync_timestamp;
    uint32_t usb_frame_number;
    uint32_t magic;
    uint16_t frame_type;
  };

  static constexpr std::size_t kSegmentHeaderSize = 0x20;
  struct SegmentHeader {
    uint32_t magic;
    uint32_t frame_number;    // common among segments
    uint32_t segment_number;  // increments for each segment in frame
    uint32_t mystery_0;
    uint32_t mystery_1;
    uint32_t mystery_2;
    uint32_t mystery_3;
    uint32_t mystery_4;
  };
  static_assert(sizeof(SegmentHeader) == kSegmentHeaderSize);

  void StartStream() final;
  void StopStream() final;
  void SetExpGain(uint16_t camera_type, uint16_t exposure, uint16_t gain) final;
  void RegisterFrameCallback(FrameCallback cb) final;

  void SendStartStopCommand(bool start);
  void Stream();
  void HandleFrame(const libusbcpp::TransferStruct* trans);
  static void TransferCallback(libusbcpp::c::libusb_transfer* trans);
  void CancelAllTransfers();

  bool ValidateFrame(const uint8_t* frame, std::size_t size);
  FrameHandle CopyFrame(const uint8_t* frame);

  std::array<ExpGainState, kCameraTypeCount> exp_gain_state_{};

  bool streaming_{};

  HeadsetSpec spec_;
  libusbcpp::DeviceHandle::Pointer dev_handle_;

  std::list<libusbcpp::Transfer::Pointer> rx_transfers_;
  std::list<std::shared_ptr<unsigned char>> rx_buffers_;
  std::size_t outstanding_transfer_count_{};

  std::queue<libusbcpp::TransferStruct*> completed_rx_transactions_;
  std::mutex completed_rx_transactions_m_;
  std::condition_variable completed_rx_transactions_cv_;

  std::thread stream_thread_;

  std::shared_ptr<void> iface_claim_hnd_;
  uint8_t read_ep_, write_ep_;

  uint32_t prev_frame_number_;
  bool got_first_frame_;

  FramePool<CameraFrame> frame_pool_;

  std::list<FrameCallback> frame_callbacks_;
  std::mutex frame_callbacks_m_;
};

}  // namespace wmr