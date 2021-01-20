// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <array>
#include <string>
#include <vector>

#include <opencv2/core/mat.hpp>

#ifndef WUMBO_PUBLIC
#if defined _WIN32 || defined __CYGWIN__
#define WUMBO_PUBLIC __declspec(dllimport)
#else
#define WUMBO_PUBLIC
#endif
#endif

namespace wmr {

/** Translates calibration JSON into OpenCV formats. */
class WUMBO_PUBLIC Calibration {
 public:
  union CameraMat {
    struct Params {
      double fx;
      double _0_1;
      double cx;
      double _1_0;
      double fy;
      double cy;
      double _2_0;
      double _2_1;
      double _2_2 = 1.0;
    } params;

    std::array<double, 9> array;
  };

  union DistCoeffs {
    struct Params {
      double k1;
      double k2;
      double p1;
      double p2;
      double k3;
      double k4;
      double k5;
      double k6;
    } params;

    std::array<double, 8> array;
  };

  struct CameraCalibration {
    cv::Mat_<double> camera_mat;
    cv::Mat_<double> dist_coeffs;
    cv::Mat_<double> rotation;
    cv::Mat_<double> translation;
    cv::Size size;
  };

  void ParseJson(std::string_view json_s);

  const std::vector<CameraCalibration>& cameras() { return cameras_; }

 private:
  std::vector<CameraCalibration> cameras_;
};
}  // namespace wmr