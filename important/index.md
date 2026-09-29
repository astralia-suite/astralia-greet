# `astralia-greet` structure

Standalone display manager (SDDM/LightDM class). One root daemon renders the login screen with `cairo` onto a DRM dumb buffer, authenticates with PAM, and launches X11 or Wayland sessions detected from `.desktop` files.

## Root

- `meson.build` — `astralia-greet-core` static library (every source except `main.cpp`), linked by the `astralia-greet` executable and the `astralia-greet-test` binary.
- `build.sh`: no argument (build and unit tests), `test` (renders `preview.png` in the project root, then runs the greeter in a full-screen X11 test window), `uninstall` (disables and stops the service, `ninja uninstall`, reloads systemd), `install` (runs `systemd-sysusers`, reloads systemd, disables any other `display-manager.service`, enables and restarts `astralia-greet.service`). Wipes `build/` when `build/.source-dir` does not match the project path.
- `.clang-format` — formatting style.

## `assets/`

- `wallpaper.png`: background image, installed to `/usr/share/astralia-greet/`; the source tree copy is the fallback (`config::background`).
- `electro.png`: password echo glyph (from `astralia-shell-hl/assets/`), installed next to `wallpaper.png` with the same fallback (`config::password`).
- `fonts/`: `ComicShannsMono-Regular.otf` (text, family `Comic Shanns Mono`) and `tabler-icons.ttf` (icons), copied from `astralia-shell-i3`, installed to `/usr/share/astralia-greet/fonts/`.
- `astralia-greet.service` — systemd unit (`display-manager.service` alias, conflicts with `getty@tty1`).
- `pam/astralia-greet`, `pam/astralia-greet-autologin`, `pam/astralia-greet-greeter` (greeter logind session, like SDDM's `sddm-greeter`) — PAM services installed to `/etc/pam.d/`.
- `sysusers/astralia-greet.conf` — the `astralia-greet` system user for the greeter session, installed to `/usr/lib/sysusers.d/`.

## `src/`

- `main.cpp`: CLI (`--preview`, `--size`, `--window`, `--vt N`, `--version`), registers the bundled fonts, uses the built-in `config::Settings` defaults, runs `app::Greeter` (`Mode::Native` as root, `Mode::Window` for `--window`) or `app::render_preview`.

### `src/core/` — no project dependencies

- `strings` — trim/split/join, bool/int/double parsing, UTF-8 helpers, `secure_clear`.
- `log` — `info`/`warn`/`error` to stderr with journald `<N>` priority prefixes.
- `file` — `read_file`, `write_file`, `write_file_atomic`.
- `ini`: `IniFile` parser/serializer used by the state store and `.desktop` files.
- `desktop_entry` — `[Desktop Entry]` parsing, `Exec` field-code stripping.
- `process` — `Environment`, `spawn`, `exec_command`, `reset_signal_state`, `wait_for`, `find_executable`.
- `event_loop` — `epoll` loop with fd sources, `timerfd` timers (`CLOCK_BOOTTIME`), `signalfd` signals.

### `src/config/`: constants and plain data only, no config files are read

- `greet_config.h`: paths (including the installed and source font dirs), bundled font files, system constants, `config::Settings` and its sections (the struct defaults are the settings).
- `theme_config.h`: `Color`, `Anchor`, `BackgroundMode`, `WidgetGeometry`, the palette and the text font.
- `<module>_config.h`: per-module geometry, point sizes, colors and text (`background`, `date`, `clock`, `clock_panel`, `password`, `session_picker`, `user_menu`, `hint`, `power`).
- `window_config.h` — test window title and messages.

### `src/render/`

- `layout` — `Rect` (`united` bounding box), `merge_overlapping` damage rects, anchor placement (`place`), image fitting (`fit_image`).
- `canvas` — `Canvas` back buffer (`RGB24`, matches DRM `XRGB8888`), `SurfacePtr`, `set_color`, `rounded_rect`.
- `image` — PNG (`libpng`, interlaced PNGs through cairo) and JPEG (`libjpeg`) decoding, `load_image` with an optional minimum size (PNG integer box reduction, JPEG DCT scaling), `paint_image`, `scaled_image` (square `ARGB32` copy, scaled once).
- `text`: `pangocairo` measuring and drawing with ellipsizing, `text_font` (`config::font_family` at a point size), `icon_font` (Tabler font description for a pixel size).
- `dropdown`: `Dropdown` shared by `session_picker` and `user_menu`: pill button (label + `chevron_up` on a pill, radius and padding `height / 2`), menu placement (opens upward when it would overflow), menu drawing and hit testing.- `fonts`: `register_fonts` adds the bundled fonts to fontconfig (`FcConfigAppFontAddFile`), installed dir first, then the source tree.
- `icons.h`: `render::icon` Tabler codepoints (subset of `astralia-shell-i3/src/core/icons.h`) and `font_family`.
- `cursor` — draws the arrow into the hardware cursor buffer.

### `src/service/` — system concerns, never draw UI

- `seat_service` — greeter logind session (holder child + `libseat`), device open/close through logind, enable/disable callbacks, `libseat_switch_session`.
- `vt_service` — VT open/activate, `find_free` (`VT_OPENQRY`), `hide_text` (clears the VT, then `KD_GRAPHICS`)/`show_text` (`KD_TEXT`) around a session VT, `take_as_stdin` (VT as stdin and controlling tty); fallback path only: `VT_PROCESS` switching, `KD_GRAPHICS`/`K_OFF`.
- `display_service` — DRM/KMS output pick, dumb buffer modeset, damage-rect `present` (overlapping rects merged first), hardware cursor; card opened through `seat_service` when the seat is up, master drop/acquire only in the fallback.
- `input_service` — `libinput` + `xkbcommon`, key repeat, pointer/touch clicks, caps lock / layout query; evdev opened through `seat_service` when the seat is up; `close` drops libinput entirely (keymap kept) for the session lifetime, deferred until `dispatch` returns; `activate()` reopens it.
- `user_service` — `/etc/passwd` parsing and filtering, avatar lookup, `getpwnam_r` fallback.
- `session_service` — X11/Wayland/custom session detection, duplicate labelling, lookup by id or name.
- `auth_service` — PAM handle, conversation, session open/close, PAM env.
- `xserver_service` — `Xauthority` entries, free display search, Xorg spawn with `SIGUSR1` readiness, display-setup hook, `xrdb -merge` of `/etc/X11/Xresources` and `~/.Xresources`.
- `launch_service` — forks the session worker (ignores `SIGTERM`/`SIGHUP`/`SIGINT` so it always cleans up, activates the session VT when `switch_vt` and keeps it open until exit, PAM session, Xorg, Wayland sessions take the VT as stdin and controlling tty, privilege drop, exec) and builds the session env/command.
- `power_service` — logind `Reboot`/`PowerOff`/`Suspend` over `sd-bus`.- `window_service` — full-screen X11 test window (`xcb` + `cairo-xcb`), damage-rect `present`, keys through `xkbcommon-x11`, clicks, resize/expose/close events.

### `src/app/`

- `state_store` — last user / per-user last session in `/var/lib/astralia-greet/state`.
- `greeter_state.h` — `GreeterState`, `View` (`Clock` idle screen, `Login` input screen), `Focus`, `Request`, `session_menu_open`/`user_menu_open` shared between the greeter and modules.
- `module.h` — `Module` interface (`layout`, `bounds`, `draw`, `visible`, `click`, `next_tick`, `is_backdrop`).
- `module_registry`: instantiates the modules in draw order (the only place that includes `modules/`); `user_menu` and `session_picker` are last so their drop-downs draw on top and get clicks first.
- `greeter` — owns services and modules, key/click handling (clock view: arrows cycle the session, any other key opens the login view; the login view only types the password, `Return` logs in, `Escape` returns to the clock where user and session are changed), login flow, VT and session lifecycle, damage tracking (full repaint on view switch), `render_preview`; `Mode::Native` drives VT/DRM/libinput, `Mode::Window` drives `window_service`.

### `src/modules/` — one UI element each

- `background`: gradient + `wallpaper.png` (PNG or JPEG decoded near screen size in `Fill`/`Fit`/`Stretch` modes) + dim, pre-rendered once per layout; the login view paints a black `login_dim` layer over it, clipped to the damage rects.
- `date`: `strftime` date line, minute-aligned ticks, clock view only.
- `clock_panel`: rounded `fill` box (black, 0.7 alpha) with an `accent` border behind `clock`, `date` and `hint`, drawn right after `background`; its own geometry covers the three, clock view only.
- `clock` — hours stacked over minutes in two colors, clock view only; a click opens the login view.
- `password` — field at the screen center, one `electro.png` glyph (`echo_size` px, scaled once) per typed character, `mask` text if the image is missing, placeholder, busy text, caret; errors show `"Skill Issue"` in the field center; login view only and the only element drawn there.
- `session_picker`: current session on a pill, bottom edge left of center, with a click-to-open drop-down list that opens upward (`GreeterState::session_menu_open`), arrow keys cycle; clock view only.
- `user_menu`: selected user on a pill, bottom edge right of `session_picker`, with a drop-down list of users (`GreeterState::user_menu_open`); clock view only.
- `hint` — errors outside the login view (autologin), caps lock warning, idle text, optional layout name, centered under the `date` line; clock view only.
- `power` — suspend/reboot/power-off Tabler icons; clock view only.

## `test/`

- `main.cpp` — runner; `check.h` — `CHECK` macro and temp-dir helpers.
- `core/`, `service/`, `app/`, `render/` — `test_*.cpp` mirroring `src/` (strings, ini, desktop entries, session detection, passwd filtering, xauth, session env, state store, layout, image decoding with self-written PNG and JPEG test images).
