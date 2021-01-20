// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include "camera.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <iterator>
#include <stdexcept>

#include <libusbcpp/error.hpp>
#include <libusbcpp/transfer.hpp>

namespace wmr {

Camera::Camera(const HeadsetSpec& spec, libusbcpp::Device::Pointer dev)
    : spec_(spec),
      dev_handle_(dev->Open()),
      frame_pool_(kFramePoolSize, spec_.camera_width, spec.camera_height, spec_.n_cameras) {
  // Get the config descriptor
  libusbcpp::Device::ConfigDescriptor config;
  try {
    config = dev->GetActiveConfigDescriptor();
  } catch (libusbcpp::Error<libusbcpp::c::LIBUSB_ERROR_NOT_FOUND>&) {
    dev_handle_->SetConfiguration(1);
    config = dev->GetActiveConfigDescriptor();
  }

  // Find the interface descriptor for kInterfaceNumber
  const libusbcpp::c::libusb_interface_descriptor* iface_desc = nullptr;
  for (uint8_t i = 0; i < config->bNumInterfaces; ++i) {
    auto* d = config->interface[i].altsetting;  // only consider altsetting[0]
    if (d->bInterfaceNumber == kInterfaceNumber) {
      iface_desc = d;
      break;
    }
  }
  if (!iface_desc) {
    throw std::runtime_error("Device doesn't have interface number kInterfaceNumber");
  }

  // Search for a read/write pair of bulk endpoints
  read_ep_ = 0xF0;  // 0xF0 is an invalid address
  write_ep_ = 0xF0;
  for (uint8_t j = 0; j < iface_desc->bNumEndpoints; ++j) {
    auto& ep_desc = iface_desc->endpoint[j];
    if ((ep_desc.bmAttributes & 0x3) != libusbcpp::c::LIBUSB_TRANSFER_TYPE_BULK) continue;

    if (ep_desc.bEndpointAddress & libusbcpp::c::LIBUSB_ENDPOINT_IN && read_ep_ == 0xF0) {
      read_ep_ = ep_desc.bEndpointAddress;
    } else if (write_ep_ == 0xF0) {
      write_ep_ = ep_desc.bEndpointAddress;
    } else {
      throw std::runtime_error("Interface has multiple bulk endpoint pairs");
    }
  }
  if (read_ep_ == 0xF0 || write_ep_ == 0xF0) {
    throw std::runtime_error("Bulk endpoint pair not found");
  }

  spdlog::debug("Camera found endpoints on interface {}: r:{:x} w::{:x}", kInterfaceNumber,
                read_ep_, write_ep_);

  iface_claim_hnd_ = dev_handle_->ClaimInterface(kInterfaceNumber);

  // Gratuitous stop command
  SendStartStopCommand(false);

  // Allocate transfers
  for (std::size_t i = 0; i < kRxSlotCount; ++i) {
    auto& trans = rx_transfers_.emplace_back(dev_handle_->AllocTransfer());
    auto& buff = rx_buffers_.emplace_back(dev_handle_->DevMemAlloc(spec_.camera_xfer_size));
    trans->FillBulkTransfer(read_ep_, buff.get(), spec_.camera_xfer_size, TransferCallback, this,
                            0);
  }
}

void Camera::StartStream() {
  assert(!streaming_);
  spdlog::trace("Camera::StartStream");

  // Reset state
  got_first_frame_ = false;

  // Start looped transfers
  for (auto& trans : rx_transfers_) {
    trans->AsStruct()->Submit();
    ++outstanding_transfer_count_;
  }

  // Start consuming completed transfers
  streaming_ = true;
  stream_thread_ = std::thread([this]() { Stream(); });

  // Start the headset camera
  SendStartStopCommand(true);
}

void Camera::StopStream() {
  spdlog::trace("Camera::StopStream");

  SendStartStopCommand(false);
  CancelAllTransfers();

  stream_thread_.join();
}

void Camera::SetExpGain(uint16_t camera_type, uint16_t exposure, uint16_t gain) {
  auto& state = exp_gain_state_.at(camera_type);

  if (state.exposure == exposure && state.gain == gain && state.cache_use_count < 60) {
    ++state.cache_use_count;
  } else {
    spdlog::trace("Camera::SetExpGain: camera_type={} exposure={}, gain={}", camera_type, exposure,
                  gain);

    SetExpGainCommand cmd{kMagic, 0x12, 0x80, camera_type, exposure, gain, camera_type};

    int actual_length;
    dev_handle_->BulkTransfer(write_ep_, reinterpret_cast<uint8_t*>(&cmd), SetExpGainCommand::kSize,
                              &actual_length, 100);

    if (actual_length != SetExpGainCommand::kSize) {
      throw std::runtime_error("BulkTransfer didn't consume all bytes");
    }

    state.exposure = exposure;
    state.gain = gain;
    state.cache_use_count = 0;
  }
}

void Camera::RegisterFrameCallback(FrameCallback cb) {
  std::lock_guard l{frame_callbacks_m_};
  frame_callbacks_.push_back(std::move(cb));
}

void Camera::SendStartStopCommand(bool start) {
  StartStopCommand cmd{kMagic, 0x0c, (uint16_t)(start ? 0x81 : 0x82)};

  int actual_length;
  dev_handle_->BulkTransfer(write_ep_, reinterpret_cast<uint8_t*>(&cmd), StartStopCommand::kSize,
                            &actual_length, 100);

  if (actual_length != StartStopCommand::kSize) {
    throw std::runtime_error("BulkTransfer didn't consume all bytes");
  }
}

void Camera::Stream() {
  spdlog::trace("Camera::ReadFrames: thread started");

  while (outstanding_transfer_count_) {
    std::unique_lock l{completed_rx_transactions_m_};
    completed_rx_transactions_cv_.wait(l, [this]() { return !completed_rx_transactions_.empty(); });

    auto trans = completed_rx_transactions_.front();
    completed_rx_transactions_.pop();
    l.unlock();

    if (trans->status == libusbcpp::c::LIBUSB_TRANSFER_COMPLETED && streaming_) {
      // Handle then resubmit this transfer
      HandleFrame(trans);
      trans->Submit();
    } else {
      // Don't resubmit, we're done.

      if (outstanding_transfer_count_ == kRxSlotCount) {
        spdlog::trace("Camera::Stream: Reaping transfers...");
        SendStartStopCommand(false);
        CancelAllTransfers();
        streaming_ = false;
      }
      --outstanding_transfer_count_;

      switch (trans->status) {
        case libusbcpp::c::LIBUSB_TRANSFER_COMPLETED:
          spdlog::trace(
              "Camera::Stream: Reap transfer w/ status "
              "LIBUSB_TRANSFER_COMPLETED");
          break;
        case libusbcpp::c::LIBUSB_TRANSFER_CANCELLED:
          spdlog::trace(
              "Camera::Stream: Reap transfer w/ status "
              "LIBUSB_TRANSFER_CANCELLED");
          break;
        case libusbcpp::c::LIBUSB_TRANSFER_ERROR:
          spdlog::error("Camera::Stream: Reap transfer w/ status LIBUSB_TRANSFER_ERROR");
          break;
        case libusbcpp::c::LIBUSB_TRANSFER_TIMED_OUT:
          spdlog::error(
              "Camera::Stream: Reap transfer w/ status "
              "LIBUSB_TRANSFER_TIMED_OUT");
          break;
        case libusbcpp::c::LIBUSB_TRANSFER_STALL:
          spdlog::error("Camera::Stream: Reap transfer w/ status LIBUSB_TRANSFER_STALL");
          break;
        case libusbcpp::c::LIBUSB_TRANSFER_NO_DEVICE:
          spdlog::error(
              "Camera::Stream: Reap transfer w/ status "
              "LIBUSB_TRANSFER_NO_DEVICE");
          break;
        case libusbcpp::c::LIBUSB_TRANSFER_OVERFLOW:
          spdlog::error(
              "Camera::Stream: Reap transfer w/ status "
              "LIBUSB_TRANSFER_OVERFLOW");
          break;
      }
    }
  }

  spdlog::trace("Camera::ReadFrames: thread exiting");
}

void Camera::HandleFrame(const libusbcpp::TransferStruct* trans) {
  if (ValidateFrame(trans->buffer, trans->actual_length)) {
    auto processed_frame = CopyFrame(trans->buffer);

    // Run callbacks
    std::lock_guard l{frame_callbacks_m_};
    auto it = frame_callbacks_.begin();
    while (it != frame_callbacks_.end()) {
      FrameCallback& cb = *it;
      auto prev = it;
      ++it;

      if (!cb(processed_frame)) {
        frame_callbacks_.erase(prev);
      }
    }

    got_first_frame_ |= true;

  } else if (got_first_frame_) {
    throw std::runtime_error("Camera::HandleFrame: Encountered invalid frame mid-stream");
  }
}

void Camera::TransferCallback(libusbcpp::c::libusb_transfer* trans) {
  auto cam = static_cast<Camera*>(trans->user_data);
  auto trans_struct = static_cast<libusbcpp::TransferStruct*>(trans);
  {
    std::lock_guard l{cam->completed_rx_transactions_m_};
    cam->completed_rx_transactions_.push(trans_struct);
  }
  cam->completed_rx_transactions_cv_.notify_one();
}

void Camera::CancelAllTransfers() {
  for (auto& trans : rx_transfers_) {
    try {
      trans->AsStruct()->Cancel();
    } catch (libusbcpp::Error<libusbcpp::c::LIBUSB_ERROR_NOT_FOUND>&) {
      // Transfer is not in progress, already complete, or already cancelled.
    }
  }
}

bool Camera::ValidateFrame(const uint8_t* frame, std::size_t size) {
  // Check frame size
  if (size != spec_.camera_frame_size) {
    spdlog::warn("Camera::ValidateFrame: wrong frame size (expected={:x}, actual={:x})",
                 spec_.camera_frame_size, size);
    return false;
  }

  // Check frame footer for magic
  auto footer = reinterpret_cast<const FrameFooter*>(frame + spec_.camera_frame_footer_offset);
  if (footer->magic != kMagic) {
    spdlog::warn("Camera::ValidateFrame: frame footer has bad magic (magic=0x{:08x})",
                 footer->magic);
    return false;
  }

  // Check frame footer for timestamp
  if (footer->timestamp == 0) {
    spdlog::warn("Camera::ValidateFrame: frame footer has no timestamp");
    return false;
  }

  auto first_segment_header = reinterpret_cast<const SegmentHeader*>(frame);

  // Check for dropped frames
  if (got_first_frame_ && first_segment_header->frame_number != prev_frame_number_ + 1) {
    spdlog::warn(
        "Camera::ValidateFrame: Dropped frame (prev_frame_number={}, "
        "current={})",
        prev_frame_number_, first_segment_header->frame_number);
    return false;
  }
  prev_frame_number_ = first_segment_header->frame_number;

  // The frame is divided into 24KiB (may vary by device) "segments". Each
  // segment starts with a 32 byte header.
  for (std::size_t segment_idx = 0; segment_idx < spec_.camera_segment_count; ++segment_idx) {
    auto segment_header =
        reinterpret_cast<const SegmentHeader*>(frame + segment_idx * spec_.camera_segment_size);

    // Cheack header for magic
    if (segment_header->magic != kMagic) {
      spdlog::warn(
          "Camera::ValidateFrame: segment header has bad magic "
          "(segment_idx ={}, magic=0x{:08x})",
          segment_idx, segment_header->magic);
      return false;
    }

    // All segments belong to the same frame
    if (segment_header->frame_number != first_segment_header->frame_number) {
      spdlog::warn(
          "Camera::ValidateFrame: segment has unexpected frame_number "
          "(expected={} actual={})",
          first_segment_header->frame_number, segment_header->frame_number);
      return false;
    }

    // Segments are sequential starting at 0
    if (segment_header->segment_number != segment_idx) {
      spdlog::warn(
          "Camera::ValidateFrame: segment has unexpected segment_number "
          "(expected={} actual={})",
          segment_idx, segment_header->segment_number);
      return false;
    }
  }

  return true;
}

Camera::FrameHandle Camera::CopyFrame(const uint8_t* frame) {
  auto footer = reinterpret_cast<const FrameFooter*>(frame + spec_.camera_frame_footer_offset);

  auto processed_frame = frame_pool_.Allocate();

  switch (footer->frame_type) {
    case FrameFooter::kFrameTypeRoom:
      processed_frame->type = CameraFrame::Type::kRoom;
      break;
    case FrameFooter::kFrameTypeController:
      processed_frame->type = CameraFrame::Type::kController;
      break;
    default:
      throw std::runtime_error("Camera::CopyFrame: Unknown frame_type");
  }

  processed_frame->timestamp = Timestamp(footer->timestamp);

  std::vector<std::size_t> bytes_copied_per_image(spec_.n_cameras, 0);
  std::size_t frame_offset = kSegmentHeaderSize;
  std::size_t cam_idx = 0;

  // The first row of each image contains metadata
  for (; cam_idx < spec_.n_cameras; ++cam_idx) {
    // TODO what is the metatadata?
    frame_offset += spec_.camera_width;
    assert(frame_offset <= spec_.camera_segment_size);
  }
  cam_idx = 0;

  // The raw frame data has segment headers inserted into it, and the
  // individual camera images are stacked horizontally. Excise the headers and
  // un-shuffle the rows from individual images.
  while (true) {
    while (frame_offset % spec_.camera_segment_size) {
      auto block_size =
          std::min(spec_.camera_segment_size - frame_offset % spec_.camera_segment_size,
                   spec_.camera_width - bytes_copied_per_image[cam_idx] % spec_.camera_width);

      if (frame_offset + block_size >= spec_.camera_frame_size) {
        throw std::runtime_error("Camera::CopyFrame: Ran out of bytes in raw frame");
      }

      auto src_begin = frame + frame_offset;
      auto src_end = src_begin + block_size;
      auto dst_begin = processed_frame->GetImage(cam_idx) + bytes_copied_per_image[cam_idx];
      std::copy(src_begin, src_end, dst_begin);

      frame_offset += block_size;
      bytes_copied_per_image[cam_idx] += block_size;

      // If we finished reading a row, move to the next camera
      if (block_size && bytes_copied_per_image[cam_idx] % spec_.camera_width == 0) {
        cam_idx = (cam_idx + 1) % spec_.n_cameras;

        // If the image for the next camera is already complete, we're done
        if (bytes_copied_per_image[cam_idx] == spec_.camera_width * spec_.camera_height) {
          return processed_frame;
        }
      }
    }

    // Seek past the segment header
    frame_offset += kSegmentHeaderSize;
  }
}

}  // namespace wmr