project('wmr', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++17'])

project_description = 'Free and Open Source support software for WMR devices.'

# These arguments are only used to build the shared libraries
# not the executables that use the library.
lib_cpp_args = []
if target_machine.system() == 'windows' or target_machine.system() == 'cygwin'
  lib_cpp_args += '-DWUMBO_PUBLIC="__declspec(dllexport)"'
else
  lib_cpp_args += '-DWUMBO_PUBLIC=__attribute__((visibility("default")))'
endif

cc =  meson.get_compiler('cpp')


libs = []

subdir('driver')
subdir('calibration')
subdir('utilities')
subdir('head_tracking')

pkg_mod = import('pkgconfig')
pkg_mod.generate(
  name : meson.project_name(),
  filebase : meson.project_name(),
  description : project_description,
  libraries : libs,
)
