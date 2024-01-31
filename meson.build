project('resiprocate',
  ['c', 'cpp'],
  version : '0.0.1',
  meson_version : '>= 0.57.0',
  subproject_dir: '.',
    default_options : [
    'c_std=c99',
    'cpp_std=none',
    'warning_level=2'
  ]
)

host_system = host_machine.system()
cpu_family = host_machine.cpu_family()
cc = meson.get_compiler('c')

project_arguments = [
  '-DUSE_ARES',
]

if host_system == 'windows'
  project_arguments += [
    '-DWIN32',
 ]
endif

if host_system == 'linux'
  project_arguments += [
    '-D_DEFAULT_SOURCE',
 ]
endif

add_project_arguments(project_arguments, language: 'c')
add_project_arguments(project_arguments, language: 'cpp')

base_dir = include_directories('.')
winsock2_dep = cc.find_library('ws2_32', required : host_system == 'windows')
winmm_dep = cc.find_library('winmm', required : host_system == 'windows')

subdir('rutil')
subdir('resip')
