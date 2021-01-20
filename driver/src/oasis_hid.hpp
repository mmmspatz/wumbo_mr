// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <chrono>
#include <future>
#include <list>

#include <wmr/oasis_hid_interface.hpp>

#include "frame_pool.hpp"
#include "hid_device.hpp"

namespace wmr {

class OasisHid : public OasisHidInterface {
 public:
  OasisHid(std::unique_ptr<HidDevice> hid_dev);
  ~OasisHid();

 private:
  static constexpr std::size_t kFramePoolSize = 3;

  using BufferView = HidDevice::BufferView;

  enum HidCommands {
    kUnknown0 = 0x04,
    kUnknown1 = 0x08,
    kUnknown2 = 0x05,
    kUnknown3 = 0x06,
    kUnknown4 = 0x07,
    kUnknown5 = 0x09,
    kUnknown6 = 0x02,  // actually uint16 set to 0x0102

    // second word32 set to 0x065b045e
    kUnknown7 = 0x03,  // actually uint16 set to 0xPP03 where PP is some arg
  };

  struct __attribute__((packed)) FwReport {
    static constexpr uint8_t kReportId = 0x02;
    static constexpr std::size_t kReportSize = 64;

    enum Command : uint8_t {
      kCmdInitialImageDownloadReq = 0x01,
      kCmdPeripheralDownloadData = 0x02,
      kCmdPeripheralCompleteData = 0x03,

      kCmdStartCalibrationRead = 0x04,
      kCmdStartDeviceInfoRead = 0x06,
      kCmdStartFlashLogRead = 0x0d,

      kCmdImuInit = 0x07,
      kCmdAckDataReceived = 0x08,
      kCmdImuStop = 0x0b,
      kCmdResetDevice = 0x0c,

      kCmdEraseFlashLog = 0x0e,
    };

    uint8_t report_id;
    uint8_t command;
    uint8_t data[0x3E];
  };
  static_assert(sizeof(FwReport) == FwReport::kReportSize);

  struct __attribute__((packed)) CommandReport {
    static constexpr uint8_t kReportId = 0x16;
    static constexpr std::size_t kReportSize = 64;
    static constexpr uint32_t kMagic = 0x65b045e;

    uint8_t report_id;
    uint8_t command_id;
    uint32_t unknown8_2;
    uint32_t unknown32_3;
    uint16_t unknown16_7;
    uint16_t unknown16_9;
    uint16_t unknown16_b;
    uint16_t unknown16_d;
    uint8_t _reserved[46];
  };
  static_assert(sizeof(CommandReport) == CommandReport::kReportSize);

  struct __attribute__((packed)) CalibrationHeader {
    uint16_t header_size;            // 6
    uint16_t header_version;         // 5
    uint32_t calibration_blob_size;  // 7
    char make[0x40];                 // 8
    char model[0x40];                // 9

    char _pad0[0x7b];

    // offset 0x103
    uint16_t presence_sensor_usb_vid;          // 14 start 0x103
    uint16_t presence_sensor_hid_vendor_page;  // 15 start 0x105
    uint8_t presence_sensor_hid_vendor_usage;  // 16 start 0x107
    uint32_t calibration_fw_major_ver;         // 17 start 0x108
    uint32_t calibration_fw_minor_ver;         // 18 start 0x10C
    uint32_t calibration_fw_rev_num;           // 19 start 0x110
    char license_key;                          // 11 start 0x114

    char _pad1[0xa8];

    // offset 0x1c3
    char friendly_name[0x40];           // 10 start 0x1c3
    char product_board_revision[0x20];  // 12 start 0x203
    char manufacturing_date;            // 13 start 0x223

    // TODO 0x01 indicates flash logs should be red
    // uint8_t flash_logs_present;  // start 0x285
  };

  /*
  // TODO
  struct DeviceInformation {
    uint32_t bootloader_major_ver;  // 20 start 0x30
    bootloader_minor_ver;           // 21 start 0x38
    bootloader_revision_number;     // 22
    firmware_major_ver;             // 23
    firmware_minor_ver;             // 24
    fpga_fw_major_ver;              // 25
    fpga_fw_minor_ver;              // 26
    fpga_fw_revision_number;        // 27
    device_info_flags;              // 28
    bth_fw_major_ver;               // 29
    bth_fw_minor_ver;               // 30
    bth_fw_revision_number;         // 31
    hashed_serial_number;           // 32
    serial_number;                  // 33
    device_release_number;          // 34
  }
  */

  /** There are three types of payload we can read from the device. */
  enum class PayloadType : uint8_t {
    kDeviceInfo = 0,
    kCalibration = 1,
    kFlashLog = 1,
  };

  void StartImu() final;
  void StopImu() final;
  void RegisterImuFrameCallback(ImuFrameCallback cb) final;
  std::string ReadCalibration() final;
  std::basic_string<uint8_t> ReadDeviceInfo() final;
  void WriteHidCmd(uint8_t command, uint8_t mystery_byte = 0) final;

  void WriteFwCmd(uint8_t command, BufferView data = {});
  std::basic_string<uint8_t> ReadFirmwarePayload(PayloadType type);

  void WriteFwCmdWaitAck(uint8_t command, BufferView data = {}, int timeout_ms = 100);

  std::string UnscrambleCalibration(BufferView scrambled_json);

  void RunCallbacks(ImuFrameHandle frame);

  struct FwCmdAckReader : HidDevice::ReportReader {
    void Update(Report) final { got_ack_.set_value(); }
    bool Finished() final { return true; }  // oneshot

    std::promise<void> got_ack_;
  };

  struct FwPayloadReader : HidDevice::ReportReader {
    void Update(Report report) final;
    bool Finished() final { return finished_; };

    OasisHid* parent_;

    PayloadType payload_type_;
    bool got_data_read_start_ = false;
    bool finished_ = false;
    uint32_t payload_size_ = 0;
    std::basic_string<uint8_t> payload_rbuff_;
    std::promise<std::basic_string<uint8_t>> payload_promise_;
  };

  struct ImuReportReader : HidDevice::ReportReader {
    static constexpr float kAccelPrecision = 1e-3f;
    static constexpr float kGyroPrecision = 1e-3f;
    static constexpr float kMagnetoPrecision = 1e-8f;
    static constexpr float kTempPrecision = 1e-2f;
    static constexpr std::size_t kImuStartupDiscardNSamples = 100;
    static constexpr std::chrono::milliseconds kSamplePeriod{1};

    struct __attribute__((packed)) ImuReport {
      static constexpr uint8_t kReportId = 0x01;
      static constexpr std::size_t kReportSize = 381;

      static constexpr uint32_t kMagic = 0x2b6f6c44;

      uint8_t id;                     // 0x00
      uint16_t temperature[4];        // 0x001
      uint64_t gyro_timestamp[4];     // 0x009
      int16_t gyro[3][32];            // 0x029
      uint64_t accel_timestamp[4];    // 0x0E9
      int32_t accel[3][4];            // 0x109
      uint64_t magneto_timestamp[4];  // 0x139
      int16_t magneto[3][4];          // 0x159
      uint32_t n_usb_frame;           // 0x171
      uint32_t unknown32_175;         // 0x175
      uint32_t magic;                 // 0x179
    };
    static_assert(sizeof(ImuReport) == ImuReport::kReportSize);

    void Update(Report report) final;

    OasisHid* parent_;
    Timestamp prev_sample_time_{-1};
    uint64_t sample_count_{0};
    uint32_t stale_frame_count_{0};
  };

  struct FwLogReportReader : HidDevice::ReportReader {
    struct __attribute__((packed)) FwLogReport {
      static constexpr uint8_t kReportId = 0x03;
      static constexpr std::size_t kReportSize = 509;

      static constexpr uint32_t kMagic = 0x2b6f6c44;
      static constexpr std::size_t kMaxLogCount = 8;
      static constexpr std::size_t kMaxLogSize = 56;

      struct __attribute__((packed)) Log {
        uint32_t time;
        uint16_t count;
        uint8_t level;
        std::array<char, kMaxLogSize> msg;
      };

      uint8_t report_id;
      uint32_t magic;
      std::array<Log, kMaxLogCount> logs;
    };
    static_assert(sizeof(FwLogReport) == FwLogReport::kReportSize);

    void Update(Report report) final;
  };

  struct McEventReportReader : HidDevice::ReportReader {
    struct __attribute__((packed)) McEventReport {
      static constexpr uint8_t kReportId = 0x17;
      static constexpr std::size_t kReportSize = 7;

      uint8_t report_id;
      uint8_t unknown8_1;
      uint8_t unknown8_2;
      uint16_t unknown16_3;
      uint16_t unknown16_5;
    };
    static_assert(sizeof(McEventReport) == McEventReport::kReportSize);

    void Update(Report report) final;
  };

  struct CommandReportReader : HidDevice::ReportReader {
    void Update(Report report) final;
  };

  struct WicedReportReader : HidDevice::ReportReader {
    struct __attribute__((packed)) WicedReport {
      static constexpr uint8_t kReportId = 0x05;
      static constexpr std::size_t kReportSize = 509;

      static constexpr std::size_t kMaxDebugPrintSize = 503;

      uint8_t report_id;
      uint8_t _reserved8_1;
      uint8_t skip_if_not_2;
      uint8_t hci_group;
      uint16_t size;  // size of msg
      std::array<char, kMaxDebugPrintSize> msg;
    };
    static_assert(sizeof(WicedReport) == WicedReport::kReportSize);

    void Update(Report report) final;
  };

  std::unique_ptr<HidDevice> hid_dev_;
  std::list<ImuFrameCallback> imu_frame_callbacks_;
  std::mutex imu_frame_callbacks_m_;
  FramePool<ImuFrame> imu_frame_pool_;
  std::shared_ptr<ImuReportReader> imu_report_reader_;
  std::shared_ptr<FwLogReportReader> fw_log_report_reader_;
  std::shared_ptr<McEventReportReader> mc_event_report_reader_;
  std::shared_ptr<CommandReportReader> command_report_reader_;
  std::shared_ptr<WicedReportReader> wiced_report_reader_;
};

}  // namespace wmr
