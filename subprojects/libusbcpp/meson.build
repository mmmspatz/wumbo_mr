project('libusbcpp', 'cpp',
  license : 'MIT',
  version : '0.1',
  default_options : ['cpp_std=c++17'],
)

libusbcpp_dep = declare_dependency(
  version: meson.project_version(),
  include_directories : include_directories('include'),
  dependencies: dependency('libusb-1.0', version : '>= 1.0.23'),
)

meson.override_dependency('libusbcpp', libusbcpp_dep)
