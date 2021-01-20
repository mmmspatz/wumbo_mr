libwmrcal_inc = include_directories('include')

libwmrcal_headers = [
  'include/wmr/calibration.hpp',
]

libwmrcal_sources = [
  'src/calibration.cpp',
]

libwmrcal_deps = [
  dependency('nlohmann_json'),
  dependency('opencv4'),
]

libwmrcal = library(
  'wmrcal',
  libwmrcal_sources,
  install: true,
  cpp_args: lib_cpp_args,
  gnu_symbol_visibility : 'hidden',
  include_directories : libwmrcal_inc,
  dependencies : libwmrcal_deps,
)

libs += libwmrcal

install_headers(libwmrcal_headers, subdir: meson.project_name())
