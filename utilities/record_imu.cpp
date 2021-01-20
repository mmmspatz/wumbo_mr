// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <thread>

#include <wmr/create_headset.hpp>
#include <wmr/headset_interface.hpp>
#include <wmr/headset_specifications/hp_reverb_g2.hpp>

using namespace wmr;

static std::atomic_int g_signal = 0;
static void SignalHandler(int which) { g_signal = which; }

/** Record IMU data to CSV files. */
int main(int argc, char** argv) {
  // Catch CTRL-C and other fun signals
  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  spdlog::set_level(spdlog::level::trace);

  auto headset = CreateHeadset(headset_specifications::kHpReverbG2);

  std::ofstream csv_accel, csv_gyro, csv_magneto;
  csv_accel.open("accel.csv");
  csv_gyro.open("gyro.csv");
  csv_magneto.open("magneto.csv");

  headset->OasisHid().RegisterImuFrameCallback([&](auto f) {
    for (auto& smp : f->accel_samples) {
      csv_accel << std::chrono::duration_cast<std::chrono::nanoseconds>(smp.timestamp).count()
                << ",";

      for (float axis : smp.axes) {
        csv_accel << axis << ",";
      }

      csv_accel << smp.temperature << ",";

      csv_accel << std::endl;
    }

    for (auto& smp : f->gyro_samples) {
      csv_gyro << std::chrono::duration_cast<std::chrono::nanoseconds>(smp.timestamp).count()
               << ",";

      for (float axis : smp.axes) {
        csv_gyro << axis << ",";
      }

      csv_gyro << smp.temperature << ",";

      csv_gyro << std::endl;
    }

    for (std::size_t i = 0; i < f->magneto_sample_count; ++i) {
      auto& smp = f->magneto_samples[i];

      csv_magneto << std::chrono::duration_cast<std::chrono::nanoseconds>(smp.timestamp).count()
                  << ",";

      for (float axis : smp.axes) {
        csv_magneto << axis << ",";
      }

      csv_magneto << std::endl;
    }

    return true;
  });

  headset->OasisHid().StartImu();

  while (g_signal == 0) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  headset->OasisHid().StopImu();
  csv_accel.close();
  csv_gyro.close();
  csv_magneto.close();

  return 0;
}
