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
- `bus`: `Bus` wraps a system-bus `sd_bus` on the event loop (fd watch, timeout timer re-armed after every pass), `name_has_owner`, `call_async`, `read_properties` (`a{sv}` to `Properties`), `on_closed`.

### `src/config/`: constants and plain data only, no config files are read

- `greet_config.h`: paths (including the installed and source font dirs), bundled font files, system constants, `config::Settings` and its sections (the struct defaults are the settings).
- `theme_config.h`: reference screen size, `Color`, `Anchor`, `BackgroundMode`, `WidgetGeometry` and the text font (no palette; colors live in `style_config.h`).
- `style_config.h`: the shell style shared by every module: palette (astralia-shell default theme) and sticker metrics.
- `scroll_config.h`: scroll panel roller, card and row sizes, wheel step and typography.
- `<module>_config.h`: per-module geometry, point sizes, colors and text (`background`, `date`, `clock`, `clock_panel`, `password`, `session_picker`, `user_menu`, `hint`, `power`, `network`, `bluetooth`).
- `window_config.h` — test window title and messages.

### `src/render/`

- `layout` — `Rect` (`united` bounding box), `merge_overlapping` damage rects, `ui_scale` (screen size against the 1920x1200 reference), `scaled`/`line_width`, scaled anchor placement (`place`), image fitting (`fit_image`).
- `canvas` — `Canvas` back buffer (`RGB24`, matches DRM `XRGB8888`), `SurfacePtr`, `set_color`, `rounded_rect`.
- `image` — PNG (`libpng`, interlaced PNGs through cairo) and JPEG (`libjpeg`) decoding, `load_image` with an optional minimum size (PNG integer box reduction, JPEG DCT scaling), `paint_image`, `scaled_image` (square `ARGB32` copy, scaled once).
- `text`: `set_text_scale` (font map resolution), `pangocairo` measuring and drawing with ellipsizing, `text_font` (`config::font_family` at a point size), `icon_font` (Tabler font description for a pixel size).
- `dropdown`: `Dropdown` shared by `session_picker` and `user_menu` in the shell sticker style: pill = `selectable` sticker (`accent_container`, `accent` face when focused) with label and `chevron_up`, menu = `sticker(surface)` card with `selectable` rows (current row `accent`, others `paper`), placement (opens upward when it would overflow), hit testing.
- `sticker`: `rounded_box`, `sticker` (offset shadow, outline, inner ring), `selectable` (hover/selected faces), `mix` (cairo ports of the shell's).
- `scroll`: scroll geometry, bounds and painting (paper, rollers, caps) for the vertical scroll.
- `widgets`: `password_dialog`, `toggle`, `icon_button` (hover/selected, used by `power`, `network`, `bluetooth` and the panels), `section_label`, `device_row`, `ListRow` with `list_height` and `paint_list`.
- `panel_content.h`: `PanelContent` (title, toggle, content height, banded painting), `PanelRegion`, `PanelPaint`.
- `scroll_panel`: `ScrollPanel` (the shell's `Panel` core): placement under an icon, cached chrome/body/shell surfaces, instant open and close, wheel scrolling, region hit testing, an optional dialog card under the panel (`dialog_reserve`/`dialog_height`/`paint_dialog`) and a header action button.
- `fonts`: `register_fonts` adds the bundled fonts to fontconfig (`FcConfigAppFontAddFile`), installed dir first, then the source tree.
- `icons.h`: `render::icon` Tabler codepoints (subset of `astralia-shell-i3/src/core/icons.h`) and `font_family`.
- `cursor` — copies `left_ptr` from the `config::display::cursor_theme` XCursor theme (`libXcursor`) into the hardware cursor buffer and returns its hotspot; falls back to a drawn arrow.

### `src/service/` — system concerns, never draw UI

- `seat_service` — greeter logind session (holder child + `libseat`), device open/close through logind, enable/disable callbacks, `libseat_switch_session`.
- `vt_service` — VT open/activate, `find_free` (`VT_OPENQRY`), `hide_text` (clears the VT, then `KD_GRAPHICS`)/`show_text` (`KD_TEXT`) around a session VT, `take_as_stdin` (VT as stdin and controlling tty); fallback path only: `VT_PROCESS` switching, `KD_GRAPHICS`/`K_OFF`.
- `display_service` — DRM/KMS output pick, dumb buffer modeset, damage-rect `present` (overlapping rects merged first), hardware cursor; card opened through `seat_service` when the seat is up, master drop/acquire only in the fallback.
- `input_service` — `libinput` + `xkbcommon`, key repeat, pointer/touch clicks, wheel and touchpad scrolling (`on_scroll`, in notches), caps lock / layout query; evdev opened through `seat_service` when the seat is up; `close` drops libinput entirely (keymap kept) for the session lifetime, deferred until `dispatch` returns; `activate()` reopens it.
- `user_service` — `/etc/passwd` parsing and filtering, avatar lookup, `getpwnam_r` fallback.
- `session_service` — X11/Wayland/custom session detection, duplicate labelling, lookup by id or name.
- `auth_service` — PAM handle, conversation, session open/close, PAM env.
- `xserver_service` — `Xauthority` entries, free display search, Xorg spawn with `SIGUSR1` readiness, display-setup hook, `xrdb -merge` of `/etc/X11/Xresources` and `~/.Xresources`.
- `launch_service` — forks the session worker (ignores `SIGTERM`/`SIGHUP`/`SIGINT` so it always cleans up, activates the session VT when `switch_vt` and keeps it open until exit, PAM session, Xorg, Wayland sessions take the VT as stdin and controlling tty, privilege drop, exec) and builds the session env/command.
- `power_service` — logind `Reboot`/`PowerOff`/`Suspend` over `sd-bus`.
- `network_service`: NetworkManager over `core::Bus`; `NetworkSnapshot` (Wi-Fi present/enabled, `NetworkLink`, `error`, entries: saved profile path, access point, SSID, signal band, `NetworkSecurity`, saved/active/connecting/in range), `classify_security`, `signal_band`, `security_label`, debounced refresh on signals with a profile cache, `set_wifi_enabled`, `toggle` (activate or deactivate a saved profile), `begin_scan`/`end_scan`/`scan` (access points only while the panel is open), `connect(ap, password)` (`AddAndActivateConnection`, failed profiles are deleted), `on_change`.
- `bluetooth_service`: BlueZ over `core::Bus`; `BluetoothSnapshot` (adapter powered, paired devices with connected/connecting), debounced refresh on signals, `set_powered`, `toggle_device` (async `Connect`/`Disconnect`), `on_change`.
- `window_service` — full-screen X11 test window (`xcb` + `cairo-xcb`), damage-rect `present`, keys through `xkbcommon-x11`, clicks, resize/expose/close events.

### `src/app/`

- `state_store` — last user / per-user last session in `/var/lib/astralia-greet/state`.
- `greeter_state.h` — `GreeterState`, `View` (`Clock` idle screen, `Login` input screen), `Focus`, `Request`, the four `*_menu_open` flags, the `network`/`bluetooth` snapshots and `request_target` shared between the greeter and modules.
- `module.h` — `Module` interface (`layout`, `bounds`, `draw`, `visible`, `click`, `hover`, `scroll`, `update`, `next_tick`, `is_backdrop`); `scroll` takes notches and returns whether the module's offset changed; `clicked_outside` tells a module a click was consumed elsewhere;  `update` is called when asynchronous state changes and returns whether the module's appearance or bounds changed.
- `module_registry`: instantiates the modules in draw order (the only place that includes `modules/`); `user_menu`, `session_picker`, `network` and `bluetooth` are last so their drop-downs draw on top and get clicks first.
- `greeter` — owns services and modules, key/click handling (clock view: arrows cycle the session, any other key opens the login view; the login view only types the password, `Return` logs in, `Escape` returns to the clock where user and session are changed), login flow, VT and session lifecycle, damage tracking (full repaint on view switch), `render_preview`; `Mode::Native` drives VT/DRM/libinput, `Mode::Window` drives `window_service`.

### `src/modules/` — one UI element each

- `background`: gradient fallback (`paper` to `accent_container`) + `wallpaper.png` (PNG or JPEG decoded near screen size in `Fill`/`Fit`/`Stretch` modes) + dim, pre-rendered once per layout; the login view paints the shell's `window_backdrop` over it, clipped to the damage rects.
- `date`: `strftime` date line, bold `text`, minute-aligned ticks, clock view only.
- `clock_panel`: `sticker(surface)` card (ink outline, drop shadow, inner ring) behind `clock`, `date` and `hint`, rendered once per layout and blitted, drawn right after `background`; its own geometry covers the three, clock view only.
- `clock` — hours stacked over minutes, bold, hour in `text`, minute in `lavender`; clock view only; a click opens the login view.
- `password` — the shell's input box at the screen center (`pop_b`, 5 px `ink` border): `lock` icon on the left, an arrow button on the right (`selectable`, accent face when text is typed, hover face; a click requests the login), one `electro.png` glyph (`echo_size` px, scaled once) per typed character, `mask` text if the image is missing, `text_muted` placeholder and busy text, `critical` "Skill Issue" errors; login view only and the only element drawn there.
- `session_picker`: current session on a sticker pill, bottom edge left of center, with a click-to-open sticker drop-down that opens upward (`GreeterState::session_menu_open`), arrow keys cycle, the pill gets the `accent` face while the session has the focus; clock view only.
- `user_menu`: selected user on a sticker pill, bottom edge right of `session_picker`, with a sticker drop-down of users (`GreeterState::user_menu_open`); clock view only.
- `hint` — errors outside the login view (autologin, `critical`), caps lock warning (`warn`), idle text (`text_dim`), optional layout name, centered under the `date` line; clock view only.
- `power` — suspend/reboot/power-off `icon_button` stickers (32 px, hover face), top right; clock view only.
- `network`: Wi-Fi/wired status icon left of `bluetooth`, with an astralia-shell scroll panel (Wi-Fi toggle and rescan button in the header, error and Ethernet banners, "Connected", "Known" (saved, in range) and "Available" sections with signal glyphs and security, connect/disconnect buttons, a password dialog under the card for WPA2/WPA3 networks); content in `modules/network/content.{h,cpp}`; visible only while NetworkManager is available; clock view only.
- `bluetooth`: adapter/connection status icon left of `power`, with an astralia-shell scroll panel (power toggle, "Connected" and "Paired" sections, connect/disconnect buttons); content in `modules/bluetooth/content.{h,cpp}`; visible only while BlueZ has an adapter; clock view only.

## `test/`

- `main.cpp` — runner; `check.h` — `CHECK` macro and temp-dir helpers.
- `core/`, `service/`, `app/`, `render/` — `test_*.cpp` mirroring `src/` (strings, ini, desktop entries, session detection, passwd filtering, xauth, session env, state store, layout, image decoding with self-written PNG and JPEG test images).
