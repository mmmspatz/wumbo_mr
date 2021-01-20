This is a proof of concept that demonstrates head tracking using [ORB-SLAM3](https://github.com/UZ-SLAMLab/ORB_SLAM3)

The calibration data read from the device is used for stereo rectification of the front facing camera feeds. The result is quite exciting!

I have not been able to get ORB-SLAM3 working in `IMU_STEREO` mode.

## Running

Assuming you're in a build dir at the top level of the repo:
```
./head_tracking/head_tracking ../subprojects/ORB_SLAM3_src/Vocabulary/ORBvoc.txt ../head_tracking/orb_slam3_config.yaml
```