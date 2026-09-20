project(
  'wallkan',
  'c',
  version : '0.1.0',
  meson_version : '>= 1.3.0',
  default_options : [
    'c_std=gnu11',
    'warning_level=3',
    'werror=false',
  ],
)
add_project_arguments(
[
'-D_GNU_SOURCE',
'-DVK_USE_PLATFORM_WAYLAND_KHR'
], language: 'c')

inc = include_directories('src')



wayland_client = dependency('wayland-client')

wayland_scanner = find_program('wayland-scanner', native: true, required: true)

layer_shell_xml = files('protocols/wlr-layer-shell-unstable-v1.xml')
layer_shell_h = custom_target(
  'wlr-layer-shell-h',
  input: layer_shell_xml,
  output: 'wlr-layer-shell-unstable-v1-client-protocol.h',
  command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)
layer_shell_c = custom_target(
  'wlr-layer-shell-c',
  input: layer_shell_xml,
  output: 'wlr-layer-shell-unstable-v1-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)

wayland_protos = dependency('wayland-protocols')
wl_proto_dir = wayland_protos.get_variable('pkgdatadir')
xdg_shell_xml = wl_proto_dir / 'stable/xdg-shell/xdg-shell.xml'

xdg_shell_h = custom_target(
  'xdg-shell-h',
  input: xdg_shell_xml,
  output: 'xdg-shell.h',
  command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)

xdg_shell_c = custom_target(
  'xdg-shell-c',
  input: xdg_shell_xml,
  output: 'xdg-shell.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)

vulkan_dep = dependency('vulkan')

cwalk_dep = dependency('cwalk', fallback: ['cwalk', 'cwalk_dep'])
yyjson_dep = dependency('yyjson', fallback: ['yyjson', 'yyjson_dep'])

dependencies = [
  wayland_client,
  cwalk_dep,
  yyjson_dep,
  vulkan_dep
]

sources = files(
  'src/wallkan.c',
  'src/err.c',
  'src/events.c',
  'src/window/window.c',
  'src/window/outputs.c',
  'src/ipc.c',
  'src/renderer/instance.c',
  'src/renderer/device.c',
  'src/renderer/renderer.c',
  'src/renderer/swapchain.c',
)

exec = executable(
  'wallkan',
  [
    sources,
    xdg_shell_h,
    xdg_shell_c,
    layer_shell_h,
    layer_shell_c
  ],
  include_directories : inc,
  dependencies : dependencies,
  install : true,
)
