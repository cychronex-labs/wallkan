project(
  'cwalk',
  'c',
  version: '1.2.9',
  license: 'MIT',
  default_options: [
    'warning_level=0',
    'werror=false',
  ],
)

cwalk_h = include_directories('.')

cwalk_c = static_library(
  'cwalk',
  'cwalk.c',
  include_directories: cwalk_h,
)

cwalk_dep = declare_dependency(
  include_directories: cwalk_h,
  link_with: cwalk_c,
)
