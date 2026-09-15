project(
  'yyjson',
  'c',
  version: '0.13.0',
  license: 'MIT',
  default_options: [
    'warning_level=0',
    'werror=false',
  ],
)

yyjson_h = include_directories('.')

yyjson_c = static_library(
  'yyjson',
  'yyjson.c',
  include_directories: yyjson_h,
)

yyjson_dep = declare_dependency(
  include_directories: yyjson_h,
  link_with: yyjson_c,
)
