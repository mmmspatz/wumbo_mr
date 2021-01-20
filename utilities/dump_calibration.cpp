// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <fstream>
#include <iostream>
#include <stdexcept>

#include <wmr/factory.hpp>
#include <wmr/headset_specifications/hp_reverb_g2.hpp>

using namespace wmr;

/** Dump the JSON headset calibration to a file. */
int main(int argc, char** argv) {
  auto spec = headset_specifications::kHpReverbG2;
  auto dev = Factory::CreateOasisHid(spec.hid_comms_dev.vid, spec.hid_comms_dev.pid);

  auto f = std::ofstream("config.json");

  if (f.is_open()) {
    auto cal = dev->ReadCalibration();
    f.write(cal.data(), cal.size());
  } else {
    throw std::runtime_error("Failed to open file");
  }
}