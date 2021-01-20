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
#include <numeric>
#include <string>
#include <thread>

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/imgcodecs.hpp>
#include <wmr/create_headset.hpp>
#include <wmr/headset_interface.hpp>
#include <wmr/headset_specifications/hp_reverb_g2.hpp>
#include <wmr/types.hpp>

using namespace wmr;

static std::atomic_int g_signal = 0;
static void SignalHandler(int which) { g_signal = which; }

/** Record IMU data and images from the front facing cameras.
 * Useful as input for kalibr.
 */
int main(int argc, char** argv) {
  // Catch CTRL-C and other fun signals
  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  spdlog::set_level(spdlog::level::trace);

  auto headset = CreateHeadset(headset_specifications::kHpReverbG2);

  headset->Open();

  headset->Camera().SetExpGain(0, 0x1770, 0xFF);
  headset->Camera().SetExpGain(1, 0x1770, 0xFF);
  headset->Camera().SetExpGain(4, 0x1770, 0xFF);
  headset->Camera().SetExpGain(5, 0x1770, 0xFF);

  headset->Camera().RegisterFrameCallback([](auto f) {
    if (f->type == CameraFrame::Type::kRoom) {
      cv::Mat_<uint8_t> left(f->image_height, f->image_width, const_cast<uint8_t*>(f->GetImage(0)));

      cv::Mat_<uint8_t> right(f->image_height, f->image_width,
                              const_cast<uint8_t*>(f->GetImage(1)));

      auto time = std::to_string(
          std::chrono::duration_cast<std::chrono::nanoseconds>(f->timestamp).count());

      // NOTE: cam0 & cam1 dirs must already exist
      cv::imwrite("cam0/" + time + ".png", left);
      cv::imwrite("cam1/" + time + ".png", right);
    }

    return true;
  });

  std::ofstream csv;
  csv.open("imu0.csv");

  headset->OasisHid().RegisterImuFrameCallback([&csv](auto f) {
    for (std::size_t i = 0; i < ImuFrame::kSamplesPerFrame; ++i) {
      csv << std::chrono::duration_cast<std::chrono::nanoseconds>(f->accel_samples[i].timestamp)
                 .count()
          << ",";

      float gx{}, gy{}, gz{};
      for (std::size_t g = 0; g < ImuFrame::kGyroOversampling; ++g) {
        auto smp = f->gyro_samples[i * ImuFrame::kGyroOversampling + g];
        gx += smp.axes[0];
        gy += smp.axes[1];
        gz += smp.axes[2];
      }
      gx /= ImuFrame::kGyroOversampling;
      gy /= ImuFrame::kGyroOversampling;
      gz /= ImuFrame::kGyroOversampling;

      csv << gx << "," << gy << "," << gz << ",";

      for (float axis : f->accel_samples[i].axes) {
        csv << axis << ",";
      }

      csv << std::endl;
    }

    return true;
  });

  cv::Mat img_l, img_r;

  while (g_signal == 0) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  headset->Close();
  csv.close();

  return 0;
}
