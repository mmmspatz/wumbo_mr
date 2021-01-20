executable(
  'dump_calibration',
  'dump_calibration.cpp',
  include_directories : libwmrdrv_inc,
  link_with: libwmrdrv,
)

executable(
  'record',
  'record.cpp',
  include_directories : libwmrdrv_inc,
  link_with: libwmrdrv,
  dependencies: [
    dependency('spdlog', version : '>=1.8.0', default_options : ['compile_library=true']),
    dependency('opencv4'),
  ],
)

executable(
  'record_imu',
  'record_imu.cpp',
  include_directories : libwmrdrv_inc,
  link_with: libwmrdrv,
  dependencies: [
    dependency('spdlog', version : '>=1.8.0', default_options : ['compile_library=true']),
  ],
)
