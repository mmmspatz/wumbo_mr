// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <System.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>
#include <wmr/calibration.hpp>
#include <wmr/create_headset.hpp>
#include <wmr/headset_interface.hpp>
#include <wmr/headset_specifications/hp_reverb_g2.hpp>

using namespace wmr;

static std::atomic_int g_signal = 0;
static void SignalHandler(int which) { g_signal = which; }

/** Decouples live camera framerate from processing. */
struct FrameBuffer {
  bool CamCallback(CameraInterface::FrameHandle frame) {
    if (frame->type == CameraFrame::Type::kRoom) {
      {
        std::lock_guard l(m_);
        avail_ = frame;
      }
      avail_cv_.notify_one();
    }

    return true;
  }

  bool ImuCallback(OasisHidInterface::ImuFrameHandle frame) {
    std::lock_guard l(m_);

    for (std::size_t i = 0; i < ImuFrame::kSamplesPerFrame; ++i) {
      double sample_time = std::chrono::duration_cast<std::chrono::duration<double>>(
                               frame->accel_samples[i].timestamp)
                               .count();

      float gx{}, gy{}, gz{};
      for (std::size_t g = 0; g < ImuFrame::kGyroOversampling; ++g) {
        auto smp = frame->gyro_samples[i * ImuFrame::kGyroOversampling + g];
        gx += smp.axes[0];
        gy += smp.axes[1];
        gz += smp.axes[2];
      }
      gx /= ImuFrame::kGyroOversampling;
      gy /= ImuFrame::kGyroOversampling;
      gz /= ImuFrame::kGyroOversampling;

      imu_frames_.emplace(frame->accel_samples[i].axes[0], frame->accel_samples[i].axes[1],
                          frame->accel_samples[i].axes[2], gx, gy, gz, sample_time);
    }

    return true;
  }

  double Get(cv::Mat& left, cv::Mat& right, std::vector<ORB_SLAM3::IMU::Point>& imu_frames) {
    std::unique_lock l(m_);
    avail_cv_.wait(l, [this]() { return static_cast<bool>(avail_); });

    referenced_ = std::move(avail_);

    double frame_time =
        std::chrono::duration_cast<std::chrono::duration<double>>(referenced_->timestamp).count();

    left = cv::Mat(referenced_->image_height, referenced_->image_width, CV_8UC1,
                   const_cast<uint8_t*>(referenced_->GetImage(0)));

    right = cv::Mat(referenced_->image_height, referenced_->image_width, CV_8UC1,
                    const_cast<uint8_t*>(referenced_->GetImage(1)));

    imu_frames.clear();
    while (imu_frames_.front().t < frame_time) {
      imu_frames.push_back(imu_frames_.front());
      imu_frames_.pop();
    }

    return frame_time;
  }

  std::queue<ORB_SLAM3::IMU::Point> imu_frames_;
  CameraInterface::FrameHandle avail_, referenced_;
  std::mutex m_;
  std::condition_variable avail_cv_;
};

/** Proof of concept demonstrating headtracking using the front facing cameras. */
int main(int argc, char** argv) {
  // Catch CTRL-C and other fun signals
  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  spdlog::set_level(spdlog::level::trace);

  auto headset = CreateHeadset(headset_specifications::kHpReverbG2);

  Calibration cal;
  cal.ParseJson(headset->OasisHid().ReadCalibration());

  auto cal_left = cal.cameras().at(0);
  auto cal_right = cal.cameras().at(1);

  cv::Mat rect_left, rect_right;
  cv::Mat proj_left, proj_right;
  cv::Mat q;

  cv::stereoRectify(cal_left.camera_mat, cal_left.dist_coeffs, cal_right.camera_mat,
                    cal_right.dist_coeffs, cal_left.size, cal_right.rotation, cal_right.translation,
                    rect_left, rect_right, proj_left, proj_right, q);

  cv::Mat map1_left, map1_right;
  cv::Mat map2_left, map2_right;
  cv::initUndistortRectifyMap(cal_left.camera_mat, cal_left.dist_coeffs, rect_left,
                              proj_left.rowRange(0, 3).colRange(0, 3), cal_left.size, CV_32F,
                              map1_left, map2_left);
  cv::initUndistortRectifyMap(cal_right.camera_mat, cal_right.dist_coeffs, rect_right,
                              proj_right.rowRange(0, 3).colRange(0, 3), cal_right.size, CV_32F,
                              map1_right, map2_right);

  headset->Open();

  // headset->VendorHid().WakeDisplay();

  headset->Camera().SetExpGain(0, 0x1770, 0x00ff);
  headset->Camera().SetExpGain(1, 0x1770, 0x00ff);
  headset->Camera().SetExpGain(4, 0x1770, 0x00ff);
  headset->Camera().SetExpGain(5, 0x1770, 0x00ff);

  FrameBuffer fb;
  headset->Camera().RegisterFrameCallback([&fb](auto f) { return fb.CamCallback(f); });
  headset->OasisHid().RegisterImuFrameCallback([&fb](auto f) { return fb.ImuCallback(f); });

  cv::Mat img_l, img_r;
  cv::Mat imgrect_l, imgrect_r;
  std::vector<ORB_SLAM3::IMU::Point> imu_frames;

  ORB_SLAM3::System SLAM(argv[1], argv[2], ORB_SLAM3::System::STEREO, true);

  while (g_signal == 0) {
    double frame_time = fb.Get(img_l, img_r, imu_frames);

    cv::remap(img_l, imgrect_l, map1_left, map2_left, cv::INTER_LINEAR);
    cv::remap(img_r, imgrect_r, map1_right, map2_right, cv::INTER_LINEAR);

    SLAM.TrackStereo(imgrect_l, imgrect_r, frame_time, imu_frames);
  }

  headset->Close();

  SLAM.Shutdown();

  return 0;
}
