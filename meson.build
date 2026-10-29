project(
  'astralia-greet',
  'cpp',
  version: '0.1.0',
  default_options: [
    'prefix=/usr',
    'cpp_std=c++23',
    'warning_level=3',
    'buildtype=release',
  ],
)

libdrm_dep = dependency('libdrm')
cairo_dep = dependency('cairo')
pangocairo_dep = dependency('pangocairo')
fontconfig_dep = dependency('fontconfig')
xkbcommon_dep = dependency('xkbcommon')
libinput_dep = dependency('libinput')
libudev_dep = dependency('libudev')
libsystemd_dep = dependency('libsystemd')
libjpeg_dep = dependency('libjpeg')
libpng_dep = dependency('libpng')
xcb_dep = dependency('xcb')
cairo_xcb_dep = dependency('cairo-xcb')
xkbcommon_x11_dep = dependency('xkbcommon-x11')
libseat_dep = dependency('libseat')
xcursor_dep = dependency('xcursor')
pam_dep = meson.get_compiler('cpp').find_library('pam')

inc = include_directories('src')
test_inc = include_directories('.')

data_dir = get_option('prefix') / get_option('datadir') / 'astralia-greet'

add_project_arguments(
  '-DASTRALIA_GREET_DATA_DIR="@0@"'.format(data_dir),
  '-DASTRALIA_GREET_VERSION="@0@"'.format(meson.project_version()),
  '-DASTRALIA_GREET_SOURCE_DIR="@0@"'.format(meson.project_source_root()),
  language: 'cpp',
)

deps = [
  libdrm_dep,
  cairo_dep,
  pangocairo_dep,
  fontconfig_dep,
  xkbcommon_dep,
  libinput_dep,
  libudev_dep,
  libsystemd_dep,
  libjpeg_dep,
  libpng_dep,
  xcb_dep,
  cairo_xcb_dep,
  xkbcommon_x11_dep,
  libseat_dep,
  xcursor_dep,
  pam_dep,
]

sources = [
  'src/core/strings.cpp',
  'src/core/log.cpp',
  'src/core/file.cpp',
  'src/core/ini.cpp',
  'src/core/desktop_entry.cpp',
  'src/core/process.cpp',
  'src/core/event_loop.cpp',
  'src/core/bus.cpp',
  'src/render/layout.cpp',
  'src/render/canvas.cpp',
  'src/render/cursor.cpp',
  'src/render/image.cpp',
  'src/render/text.cpp',
  'src/render/fonts.cpp',
  'src/render/dropdown.cpp',
  'src/render/style.cpp',
  'src/render/sticker.cpp',
  'src/render/scroll.cpp',
  'src/render/widgets.cpp',
  'src/render/scroll_panel.cpp',
  'src/service/user_service.cpp',
  'src/service/session_service.cpp',
  'src/service/auth_service.cpp',
  'src/service/xserver_service.cpp',
  'src/service/launch_service.cpp',
  'src/service/power_service.cpp',
  'src/service/network_service.cpp',
  'src/service/bluetooth_service.cpp',
  'src/service/vt_service.cpp',
  'src/service/seat_service.cpp',
  'src/service/display_service.cpp',
  'src/service/input_service.cpp',
  'src/service/window_service.cpp',
  'src/modules/background.cpp',
  'src/modules/clock.cpp',
  'src/modules/clock_panel.cpp',
  'src/modules/date.cpp',
  'src/modules/password.cpp',
  'src/modules/session_picker.cpp',
  'src/modules/user_menu.cpp',
  'src/modules/hint.cpp',
  'src/modules/power.cpp',
  'src/modules/network.cpp',
  'src/modules/network/content.cpp',
  'src/modules/bluetooth.cpp',
  'src/modules/bluetooth/content.cpp',
  'src/app/state_store.cpp',
  'src/app/module_registry.cpp',
  'src/app/greeter.cpp',
]

core = static_library(
  'astralia-greet-core',
  sources,
  include_directories: inc,
  dependencies: deps,
)

executable(
  'astralia-greet',
  'src/main.cpp',
  link_with: core,
  include_directories: inc,
  dependencies: deps,
  install: true,
  install_dir: get_option('bindir'),
)

unit = executable(
  'astralia-greet-test',
  'test/main.cpp',
  'test/core/test_strings.cpp',
  'test/core/test_ini.cpp',
  'test/core/test_desktop_entry.cpp',
  'test/service/test_session_detect.cpp',
  'test/service/test_user_filter.cpp',
  'test/service/test_xauth.cpp',
  'test/service/test_launch_env.cpp',
  'test/service/test_radio.cpp',
  'test/app/test_state_store.cpp',
  'test/render/test_layout.cpp',
  'test/render/test_scroll.cpp',
  'test/render/test_image.cpp',
  link_with: core,
  include_directories: [inc, test_inc],
  dependencies: deps,
)

test('astralia-greet', unit)

install_data('assets/wallpaper.png', 'assets/electro.png', install_dir: data_dir)
install_subdir('assets/fonts', install_dir: data_dir)
install_data('assets/astralia-greet.service', install_dir: get_option('prefix') / 'lib' / 'systemd' / 'system')
install_data(
  'assets/pam/astralia-greet',
  'assets/pam/astralia-greet-autologin',
  'assets/pam/astralia-greet-greeter',
  install_dir: get_option('sysconfdir') / 'pam.d',
)
install_data('assets/sysusers/astralia-greet.conf', install_dir: get_option('prefix') / 'lib' / 'sysusers.d')
