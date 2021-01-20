// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include "hp_reverb_hid.hpp"

//#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/spdlog.h>

#include "hid_device.hpp"

namespace wmr {

HpReverbHid::HpReverbHid(std::unique_ptr<HidDevice> hid_dev) : hid_dev_(std::move(hid_dev)) {
  reader_5_ = std::make_shared<MysteryReport5Reader>();
  hid_dev_->RegisterReportReader(MyteryReport5::kReportId, reader_5_);

  reader_1_ = std::make_shared<MysteryReport1Reader>();
  hid_dev_->RegisterReportReader(MyteryReport1::kReportId, reader_1_);
}

void HpReverbHid::WakeDisplay() {
  spdlog::trace("HpReverbHid::Init");

  MyteryReport80 tx80_ = {MyteryReport80::kReportId, 0x01};
  MyteryReport80 rx80_ = {MyteryReport80::kReportId};
  for (int i = 0; i < 4; ++i) {
    hid_dev_->SetFeatureReport({reinterpret_cast<uint8_t *>(&tx80_), sizeof(MyteryReport80)});
    hid_dev_->GetFeatureReport(&rx80_, sizeof(MyteryReport80));
  }

  MyteryReport9 rx9_{MyteryReport9::kReportId};
  hid_dev_->GetFeatureReport(&rx9_, sizeof(MyteryReport9));

  MyteryReport8 rx8_{MyteryReport8::kReportId};
  hid_dev_->GetFeatureReport(&rx8_, sizeof(MyteryReport8));

  MyteryReport6 rx6_{MyteryReport6::kReportId};
  hid_dev_->GetFeatureReport(&rx6_, sizeof(MyteryReport6));

  MyteryReport4 tx4_{MyteryReport4::kReportId, 0x01};
  hid_dev_->SetFeatureReport({reinterpret_cast<uint8_t *>(&tx4_), sizeof(MyteryReport4)});
}

void HpReverbHid::MysteryReport5Reader::Update(Report report) {
  assert(report[0] == MyteryReport5::kReportId);

  auto as_struct = reinterpret_cast<const MyteryReport5 *>(report.data());
  if (report.size() != MyteryReport5::kReportSize) {
    spdlog::warn("MyteryReport5 has wrong size ({})", report.size());
  } else {
    spdlog::debug("MyteryReport5 ");

    // spdlog::debug("MyteryReport5 {:n}",
    // spdlog::to_hex(as_struct->data.begin(),
    //                                                    as_struct->data.end()));
  }
}

void HpReverbHid::MysteryReport1Reader::Update(Report report) {
  assert(report[0] == MyteryReport1::kReportId);

  auto as_struct = reinterpret_cast<const MyteryReport1 *>(report.data());
  if (report.size() != MyteryReport1::kReportSize) {
    spdlog::warn("MyteryReport1 has wrong size ({})", report.size());
  } else {
    spdlog::debug("MyteryReport1 {:x} {:04x}", as_struct->unknown_8, as_struct->unknown_16);
  }
}
}  // namespace wmr