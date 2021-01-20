libwmrdrv_inc = include_directories('include')

libwmrdrv_headers = [
  'include/wmr/camera_interface.hpp',
  'include/wmr/create_headset.hpp',
  'include/wmr/factory.hpp',
  'include/wmr/headset_interface.hpp',
  'include/wmr/headset_spec.hpp',
  'include/wmr/headset_specifications/hp_reverb_g2.hpp',
  'include/wmr/oasis_hid_interface.hpp',
  'include/wmr/vendor_hid_interface.hpp',
]

libwmrdrv_sources = [
  'src/camera.cpp',
  'src/create_headset.cpp',
  'src/factory.cpp',
  'src/headset.cpp',
  'src/hid_device.cpp',
  'src/hp_reverb_hid.cpp',
  'src/libusb_event_thread.cpp',
  'src/oasis_hid.cpp',
]

libwmrdrv_deps = [
  dependency('threads'),
  cc.find_library('atomic'),
  dependency('hidapi-hidraw', version : '>= 0.10.0'),
  dependency('libusbcpp'),
  dependency('spdlog', version : '>=1.8.0', default_options : ['compile_library=true']),
]

libwmrdrv = library(
  'wmrdrv',
  libwmrdrv_sources,
  install: true,
  cpp_args: lib_cpp_args,
  gnu_symbol_visibility : 'hidden',
  include_directories : libwmrdrv_inc,
  dependencies : libwmrdrv_deps,
)

libs += libwmrdrv

install_headers(libwmrdrv_headers, subdir: meson.project_name())
