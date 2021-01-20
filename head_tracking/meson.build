deps = [
  dependency('spdlog', version : '>=1.8.0', default_options : ['compile_library=true']),
  dependency('opencv4'),
  dependency('ORB_SLAM3', fallback : ['ORB_SLAM3', 'ORB_SLAM3_dep']),
]

executable(
  'head_tracking',
  'head_tracking.cpp',
  include_directories : [libwmrdrv_inc, libwmrcal_inc],
  link_with: [libwmrdrv, libwmrcal],
  dependencies: deps,
)
