// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include "oasis_hid.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <exception>
#include <future>
#include <mutex>
#include <stdexcept>

#include "oasis_hid_calibration_key.hpp"

namespace wmr {

OasisHid::OasisHid(std::unique_ptr<HidDevice> hid_dev)
    : hid_dev_(std::move(hid_dev)), imu_frame_pool_(kFramePoolSize) {
  fw_log_report_reader_ = std::make_shared<FwLogReportReader>();
  hid_dev_->RegisterReportReader(FwLogReportReader::FwLogReport::kReportId, fw_log_report_reader_);

  // mc_event_report_reader_ = std::make_shared<McEventReportReader>();
  // hid_dev_->RegisterReportReader(McEventReportReader::McEventReport::kReportId,
  // mc_event_report_reader_);

  command_report_reader_ = std::make_shared<CommandReportReader>();
  hid_dev_->RegisterReportReader(CommandReport::kReportId, command_report_reader_);

  wiced_report_reader_ = std::make_shared<WicedReportReader>();
  hid_dev_->RegisterReportReader(WicedReportReader::WicedReport::kReportId, wiced_report_reader_);

  WriteFwCmdWaitAck(FwReport::kCmdImuStop);
}

OasisHid::~OasisHid() { WriteFwCmdWaitAck(FwReport::kCmdImuStop); }

void OasisHid::StartImu() {
  imu_report_reader_ = std::make_shared<ImuReportReader>();
  imu_report_reader_->parent_ = this;  // Safe-ish, since it's among the first members destructed
  hid_dev_->RegisterReportReader(ImuReportReader::ImuReport::kReportId, imu_report_reader_);

  WriteFwCmdWaitAck(OasisHid::FwReport::kCmdImuInit);
}

void OasisHid::StopImu() {
  WriteFwCmdWaitAck(OasisHid::FwReport::kCmdImuStop);
  imu_report_reader_.reset();
}

void OasisHid::RegisterImuFrameCallback(ImuFrameCallback cb) {
  std::lock_guard l{imu_frame_callbacks_m_};
  imu_frame_callbacks_.push_back(std::move(cb));
}

std::string OasisHid::ReadCalibration() {
  auto payload = ReadFirmwarePayload(PayloadType::kCalibration);

  auto header = reinterpret_cast<const CalibrationHeader *>(payload.data());

  auto json_offset = header->header_size + sizeof(header->header_size);
  BufferView scrambled_json = BufferView(payload).substr(json_offset);

  return UnscrambleCalibration(scrambled_json);
}

std::basic_string<uint8_t> OasisHid::ReadDeviceInfo() {
  return ReadFirmwarePayload(PayloadType::kDeviceInfo);
}

std::string OasisHid::UnscrambleCalibration(BufferView scrambled_json) {
  // Credit here goes to Max Thomas, who figured this out for OpenHMD
  // see: https://github.com/OpenHMD/OpenHMD/issues/179#issuecomment-433687825

  std::string json(scrambled_json.size(), '\0');
  for (std::size_t i = 0; i < scrambled_json.size(); ++i) {
    json[i] = scrambled_json[i] ^ kCalibrationKey[i % kCalibrationKey.size()];
  }

  return json;
}

std::basic_string<uint8_t> OasisHid::ReadFirmwarePayload(PayloadType type) {
  // Set up callback
  auto reader = std::make_shared<FwPayloadReader>();
  reader->parent_ = this;  // Safe, since reader goes out of scope at return
  reader->payload_type_ = type;
  hid_dev_->RegisterReportReader(FwReport::kReportId, reader);

  // LUT mapping payload type to a payload read start command
  static constexpr std::array<uint8_t, 3> type_to_cmd{FwReport::kCmdStartDeviceInfoRead,
                                                      FwReport::kCmdStartCalibrationRead,
                                                      FwReport::kCmdStartFlashLogRead};

  // Start payload read
  WriteFwCmd(type_to_cmd.at(static_cast<std::size_t>(type)));

  // Wait for read to complete
  auto fut = reader->payload_promise_.get_future();
  if (fut.wait_for(std::chrono::seconds(1)) == std::future_status::ready) {
    return fut.get();
  } else {
    throw std::runtime_error("OasisHid::ReadFirmwarePayload: Timeout");
  }
}

void OasisHid::FwPayloadReader::Update(Report report) {
  try {
    if (report.size() < 2) {
      throw std::runtime_error("Report too short");
    }

    enum class FwPayloadTxState {
      kDataReadStart = 0,
      kDataReadPayload = 1,
      kDataReadEnd = 2,
    };

    // Byte 1 indicates the state of the transmitter
    switch (static_cast<FwPayloadTxState>(report[1])) {
      case FwPayloadTxState::kDataReadStart: {
        if (got_data_read_start_) {
          throw std::runtime_error("Repeated DATA_READ_START");
        }
        got_data_read_start_ = true;

        if (report.size() < 7) {
          throw std::runtime_error("DATA_READ_START report too short");
        }

        // Byte 2 mirrors the payload type of the command that started the read
        if (static_cast<PayloadType>(report[2]) != payload_type_) {
          throw std::runtime_error("DATA_READ_START indicates wrong payload type");
        }

        // Bytes 3-6 are big-endian size of the payload
        payload_size_ = (report[3] << 24) | (report[4] << 16) | (report[5] << 8) | (report[6] << 0);
        payload_rbuff_.reserve(payload_size_);

        parent_->WriteFwCmd(FwReport::kCmdAckDataReceived);
      } break;

      case FwPayloadTxState::kDataReadPayload: {
        if (!got_data_read_start_) {
          throw std::runtime_error("DATA_READ_PAYLOAD came before DATA_READ_START");
        }

        if (report.size() < 3) {
          throw std::runtime_error("DATA_READ_PAYLOAD report too short");
        }

        std::size_t chunk_size = report[2];
        if (chunk_size + 3 > report.size()) {
          throw std::runtime_error("chunk_size larger than remainder of report");
        }

        if (payload_rbuff_.size() + chunk_size > payload_size_) {
          throw std::runtime_error("chunk_size implies too-large payload");
        }

        payload_rbuff_.append(report, 3, chunk_size);

        parent_->WriteFwCmd(FwReport::kCmdAckDataReceived);
      } break;

      case FwPayloadTxState::kDataReadEnd: {
        if (!got_data_read_start_) {
          throw std::runtime_error("DATA_READ_END came before DATA_READ_START");
        }

        if (payload_rbuff_.size() != payload_size_) {
          throw std::runtime_error("DATA_READ_END before payload complete");
        }

        // Success!
        payload_promise_.set_value(std::move(payload_rbuff_));

        finished_ = true;

        // Note: Don't ACK DATA_READ_END
      } break;

      default:
        throw std::runtime_error("Unknown FwPayloadTxState");
    }

  } catch (...) {
    finished_ = true;

    payload_promise_.set_exception(std::current_exception());
  }
}

void OasisHid::WriteFwCmd(uint8_t command, BufferView data) {
  // Marshal buffer
  FwReport buff{};
  buff.report_id = FwReport::kReportId;
  buff.command = command;
  assert(data.size() <= sizeof(buff.data));
  std::copy(data.begin(), data.end(), buff.data);

  // Do write
  hid_dev_->WriteReport({reinterpret_cast<uint8_t *>(&buff), sizeof(FwReport)});
}

void OasisHid::WriteFwCmdWaitAck(uint8_t command, BufferView data, int timeout_ms) {
  // Set up callback
  auto reader = std::make_shared<FwCmdAckReader>();
  hid_dev_->RegisterReportReader(FwReport::kReportId, reader);

  // Do write
  WriteFwCmd(command, data);

  // Wait for ACK
  auto fut = reader->got_ack_.get_future();
  if (fut.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::ready) {
    fut.get();
  } else {
    throw std::runtime_error("OasisHid::WriteFwCmdWaitAck: Timeout");
  }
}

void OasisHid::WriteHidCmd(uint8_t command, uint8_t mystery_byte) {
  // Marshal buffer
  CommandReport buff{};
  buff.report_id = CommandReport::kReportId;
  buff.command_id = command;

  // TODO Some commands have other mystery bytes

  // Do write
  hid_dev_->SetFeatureReport({reinterpret_cast<uint8_t *>(&buff), sizeof(CommandReport)});
}

void OasisHid::RunCallbacks(ImuFrameHandle frame) {
  // Run callbacks
  std::lock_guard l{imu_frame_callbacks_m_};
  auto it = imu_frame_callbacks_.begin();
  while (it != imu_frame_callbacks_.end()) {
    ImuFrameCallback &cb = *it;
    auto prev = it++;

    if (!cb(frame)) {
      imu_frame_callbacks_.erase(prev);
    }
  }
}

void OasisHid::ImuReportReader::Update(Report report) {
  assert(report[0] == ImuReport::kReportId);

  auto as_struct = reinterpret_cast<const ImuReport *>(report.data());

  if (report.size() != ImuReport::kReportSize) {
    spdlog::warn("ImuReport has wrong size ({})", report.size());
    return;
  }

  if (as_struct->magic != ImuReport::kMagic) {
    spdlog::warn("ImuReport has bad magic ({:04x})", as_struct->magic);
    return;
  }

  sample_count_ += ImuFrame::kSamplesPerFrame;
  if (sample_count_ < kImuStartupDiscardNSamples) return;

  auto frame = parent_->imu_frame_pool_.Allocate();

  // Sanitize the one buffer we might not completely overwrite
  frame->magneto_samples = {};
  frame->magneto_sample_count = 0;

  for (std::size_t smp_idx = 0; true; ++smp_idx) {
    // Run callbacks and break when frame is complete
    if (smp_idx == ImuFrame::kSamplesPerFrame) {
      parent_->RunCallbacks(frame);
      break;
    }

    Timestamp sample_time(as_struct->accel_timestamp[smp_idx]);
    Timestamp delta_t =
        (prev_sample_time_.count() > 0) ? sample_time - prev_sample_time_ : kSamplePeriod;
    prev_sample_time_ = sample_time;

    if (delta_t.count() <= 0) {
      stale_frame_count_++;
      break;
    }

    if (delta_t > 2 * kSamplePeriod) {
      spdlog::warn(
          "OasisHid::ImuReportReader: encountered gap sample_count_={}, sample_time={}*100ns "
          "delta_t={}*100ns",
          sample_count_, prev_sample_time_.count(), delta_t.count());
      delta_t = 2 * kSamplePeriod;
    }

    // Accelerometer
    frame->accel_samples[smp_idx].timestamp = Timestamp(as_struct->accel_timestamp[smp_idx]);
    frame->accel_samples[smp_idx].temperature = as_struct->temperature[smp_idx] * kTempPrecision;
    frame->accel_samples[smp_idx].axes[0] = as_struct->accel[0][smp_idx] * kAccelPrecision;
    frame->accel_samples[smp_idx].axes[1] = as_struct->accel[1][smp_idx] * kAccelPrecision;
    frame->accel_samples[smp_idx].axes[2] = as_struct->accel[2][smp_idx] * kAccelPrecision;

    // Gyro
    auto gyro_delta_t = delta_t / ImuFrame::kGyroOversampling;
    for (std::size_t j = 0; j < ImuFrame::kGyroOversampling; ++j) {
      auto gyro_idx = smp_idx * ImuFrame::kGyroOversampling + j;

      // gyro_timestamp[smp_idx] corresponds to the last of the kGyroOversampling gyro samples in
      // this adc sample period.
      frame->gyro_samples[gyro_idx].timestamp =
          Timestamp(as_struct->gyro_timestamp[smp_idx]) -
          (ImuFrame::kGyroOversampling - 1 - j) * gyro_delta_t;
      frame->gyro_samples[gyro_idx].temperature = as_struct->temperature[smp_idx] * kTempPrecision;
      frame->gyro_samples[gyro_idx].axes[0] = as_struct->gyro[0][gyro_idx] * kGyroPrecision;
      frame->gyro_samples[gyro_idx].axes[1] = as_struct->gyro[1][gyro_idx] * kGyroPrecision;
      frame->gyro_samples[gyro_idx].axes[2] = as_struct->gyro[2][gyro_idx] * kGyroPrecision;
    }

    // Magnetometer
    // Frame contains up to ImuFrame::kSamplesPerFrame magneto samples.
    // Valid samples have nonzero timestamps.
    if (as_struct->magneto_timestamp[smp_idx]) {
      auto m = frame->magneto_sample_count++;

      frame->magneto_samples[m].timestamp = Timestamp(as_struct->magneto_timestamp[smp_idx]);
      frame->magneto_samples[m].axes[0] = as_struct->magneto[0][smp_idx] * kMagnetoPrecision;
      frame->magneto_samples[m].axes[1] = as_struct->magneto[1][smp_idx] * kMagnetoPrecision;
      frame->magneto_samples[m].axes[2] = as_struct->magneto[2][smp_idx] * kMagnetoPrecision;
    }
  }

  // Heartbeat
  if (sample_count_ % 6000 == 0) {
    spdlog::info("OasisHid::ImuReportReader: sample_count_ = {}", sample_count_);
  }

  // Report stale samples once per second
  if (sample_count_ % 1000 == 0 && stale_frame_count_) {
    spdlog::warn("OasisHid::ImuReportReader: Dropped {} stale frames", stale_frame_count_);
    stale_frame_count_ = 0;
  }
}

void OasisHid::FwLogReportReader::Update(Report report) {
  assert(report[0] == FwLogReport::kReportId);

  auto as_struct = reinterpret_cast<const FwLogReport *>(report.data());

  if (report.size() != FwLogReport::kReportSize) {
    spdlog::warn("FwLogReport has wrong size ({})", report.size());
  } else if (as_struct->magic != FwLogReport::kMagic) {
    spdlog::warn("FwLogReport has bad magic ({:04x})", as_struct->magic);
  } else {
    for (auto &log : as_struct->logs) {
      if (log.msg[0] == 0) break;

      spdlog::debug("[FWLogReport] [time={} level={}] {:.{}s}", log.time, log.level, log.msg.data(),
                    log.msg.size());
    }
  }
}

void OasisHid::McEventReportReader::Update(Report report) {
  assert(report[0] == McEventReport::kReportId);

  auto as_struct = reinterpret_cast<const McEventReport *>(report.data());

  if (report.size() != McEventReport::kReportSize) {
    spdlog::warn("McEventReport has wrong size ({})", report.size());
  } else {
    spdlog::info("[McEventReport] {:x} {:x} {:02x} {:02x}", as_struct->unknown8_1,
                 as_struct->unknown8_2, as_struct->unknown16_3, as_struct->unknown16_5);
  }
}

void OasisHid::CommandReportReader::Update(Report report) {
  assert(report[0] == CommandReport::kReportId);

  auto as_struct = reinterpret_cast<const CommandReport *>(report.data());

  if (report.size() != CommandReport::kReportSize) {
    spdlog::warn("CommandReport has wrong size ({})", report.size());
  } else if (as_struct->command_id != 8 && as_struct->command_id != 9) {
    spdlog::warn("CommandReport has unexpected command_id {}", as_struct->command_id);
  } else {
    spdlog::info(
        "[CommandReport] [command_id = {:x}] {:x} {:04x} {:02x} {:02x} {:02x} "
        "{:02x}",
        as_struct->command_id, as_struct->unknown8_2, as_struct->unknown32_3,
        as_struct->unknown16_7, as_struct->unknown16_9, as_struct->unknown16_b,
        as_struct->unknown16_d);
  }
}

void OasisHid::WicedReportReader::Update(Report report) {
  assert(report[0] == WicedReport::kReportId);

  auto as_struct = reinterpret_cast<const WicedReport *>(report.data());

  if (report.size() != WicedReport::kReportSize) {
    // FIXME this always fires
    spdlog::warn("WicedReport has wrong size ({})", report.size());
  } else if (as_struct->hci_group != 2) {
    // ignore
  } else if (as_struct->size + 1U > WicedReport::kMaxDebugPrintSize) {
    spdlog::warn("WicedReport has invalid size field ({})", as_struct->size);
  } else {
    spdlog::info("[WicedReport] [hci_group = {}] {:.{}s}", as_struct->hci_group,
                 as_struct->msg.data(), as_struct->size);
  }
}

}  // namespace wmr
