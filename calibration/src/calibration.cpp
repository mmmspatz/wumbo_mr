// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <iterator>
#include <stdexcept>

#include <nlohmann/json.hpp>
#include <wmr/calibration.hpp>

#include "opencv2/core/hal/interface.h"

namespace wmr {
void Calibration::ParseJson(std::string_view json_s) {
  auto json = nlohmann::json::parse(json_s);

  auto cameras_j = json["CalibrationInformation"]["Cameras"];

  std::transform(
      cameras_j.cbegin(), cameras_j.cend(), std::back_inserter(cameras_), [](auto cam_j) {
        CameraCalibration cal{};

        cal.size.width = cam_j["SensorWidth"];
        cal.size.height = cam_j["SensorHeight"];

        auto intrinsics = cam_j["Intrinsics"];

        if (intrinsics["ModelType"] != "CALIBRATION_LensDistortionModelRational6KT") {
          throw std::runtime_error("Unsupported camera calibration model");
        } else if (intrinsics["ModelParameterCount"] != 15) {
          throw std::runtime_error("Unexpected ModelParameterCount");
        }

        auto model_params_j = intrinsics["ModelParameters"];
        if (model_params_j.size() != 15) throw std::runtime_error("ModelParameters size is not 15");
        CameraMat camera_mat{};
        camera_mat.params.cx = model_params_j[0];
        camera_mat.params.cy = model_params_j[1];
        camera_mat.params.fx = model_params_j[2];
        camera_mat.params.fy = model_params_j[3];

        // From the Azure Kinect Sensor SDK (src/transformation/mode_specific_calibration.c):
        //  "raw calibration is unitized and 0-cornered, i.e., principal point and focal length are
        //  divided by image dimensions and coordinate (0,0) corresponds to the top left corner of
        //  the top left pixel. Convert to pixelized and 0-centered OpenCV convention used in the
        //  SDK, i.e., principal point and focal length are not normalized and (0,0) represents the
        //  center of the top left pixel."
        camera_mat.params.cx *= cal.size.width;
        camera_mat.params.fx *= cal.size.width;
        camera_mat.params.fy *= cal.size.height;
        camera_mat.params.cy *= cal.size.height;
        camera_mat.params.cx -= 0.5;
        camera_mat.params.cy -= 0.5;

        cal.camera_mat.create(3, 3);
        std::copy(camera_mat.array.cbegin(), camera_mat.array.cend(), cal.camera_mat.begin());

        DistCoeffs dist_coeffs{};
        dist_coeffs.params.k1 = model_params_j[4];
        dist_coeffs.params.k2 = model_params_j[5];
        dist_coeffs.params.k3 = model_params_j[6];
        dist_coeffs.params.k4 = model_params_j[7];
        dist_coeffs.params.k5 = model_params_j[8];
        dist_coeffs.params.k6 = model_params_j[9];
        dist_coeffs.params.p2 = model_params_j[12];
        dist_coeffs.params.p1 = model_params_j[13];
        cal.dist_coeffs.create(8, 1);
        std::copy(dist_coeffs.array.cbegin(), dist_coeffs.array.cend(), cal.dist_coeffs.begin());

        auto rot_j = cam_j["Rt"]["Rotation"];
        if (rot_j.size() != 9) throw std::runtime_error("Rotation size is not 9");
        cal.rotation.create(3, 3);
        std::copy(rot_j.cbegin(), rot_j.cend(), cal.rotation.begin());

        // From the Azure Kinect Sensor SDK (src/calibration/calibration.c):
        //   "raw calibration stores extrinsics in meters."
        auto tran_j = cam_j["Rt"]["Translation"];
        if (tran_j.size() != 3) throw std::runtime_error("Translation size is not 3");
        cal.translation.create(3, 1);
        std::copy(tran_j.cbegin(), tran_j.cend(), cal.translation.begin());

        return cal;
      });
}
}  // namespace wmr