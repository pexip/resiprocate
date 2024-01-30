project('resiprocate',
  ['c', 'cpp'],
  version : '0.0.1',
  meson_version : '>= 0.57.0',
  subproject_dir: '.',
    default_options : [
    'c_std=c99',
    'cpp_std=c++03',
    'warning_level=2'
  ]
)

host_system = host_machine.system()
cpu_family = host_machine.cpu_family()

cpp_project_arguments = [
  '-DUSE_ARES',
]

add_project_arguments(cpp_project_arguments, language: 'cpp')

base_dir = include_directories('.')

subdir('rutil')
subdir('resip')
