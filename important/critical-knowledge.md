# `astralia-greet` critical knowledge

## Build

- `build/` stores absolute paths (`meson-private/coredata.dat`), so `meson setup --reconfigure` fails after the project is moved or renamed. `build.sh` records the project path in `build/.source-dir` and wipes `build/` when it does not match `$PWD`.

## Install

- `ninja install` only replaces files on disk: the running `astralia-greet.service` keeps the old binary until it restarts. `./build.sh install` restarts it. Running sessions survive the restart (`pam_systemd` moved their worker into the user's session scope), so a new login starts a second session next to the old one.
- `./build.sh install` resolves `display-manager.service` with `systemctl show -P Id`; any other display manager is disabled and stopped before `systemctl enable --force` takes over the alias.
- `./build.sh uninstall` only removes what the current `meson.build` installs, so files dropped from `meson.build` must be removed by hand. It ends the running greeter and does not re-enable the previous display manager, so the machine boots to a TTY until one is enabled.

## Process model

- `pam_open_session` must run in the forked session worker, never in the daemon: `pam_systemd` moves the calling process into the user's session scope. The daemon only runs `pam_start`/`pam_authenticate`/`pam_acct_mgmt`, forks, then calls `pam_end(PAM_SUCCESS | PAM_DATA_SILENT)` on its copy of the handle.
- The worker (`service::LaunchService`) is root: it opens the PAM session, starts Xorg if needed, forks the user child (`initgroups` → `setgid` → `setuid` → exec), waits, stops Xorg, closes the PAM session and exits. The daemon only watches the worker PID through `SIGCHLD`.
- PAM env (`XDG_SESSION_TYPE`, `XDG_SESSION_CLASS`, `XDG_SEAT`, `XDG_VTNR`, `XDG_SESSION_DESKTOP`) must be `pam_putenv`'d before `pam_open_session`, otherwise logind registers the wrong session type or no seat.
- The worker lives in the user's session scope, so `loginctl terminate-session`/`terminate-user` sends it `SIGTERM` together with `Xorg` and the session. It ignores `SIGTERM`/`SIGHUP`/`SIGINT` and lets the user child's exit drive cleanup; the user child resets them before `exec`. With the default action the worker died at once, skipped `xorg.stop`/`show_text`/`close_session`, and the daemon's `libseat_switch_session` ran while `Xorg` still held the session VT, so the switch was lost and the screen stayed black on that VT.
- The daemon blocks every handled signal for `signalfd`; every forked child must call `core::reset_signal_state()` or it inherits a blocked mask.

## Seat (logind greeter session)

- Normal path (`managed_`): `SeatService::open` forks a holder child that opens a PAM session (`astralia-greet-greeter`, user `astralia-greet`, `XDG_SESSION_CLASS=greeter`, `XDG_VTNR` = greeter VT) and blocks on a pipe; the daemon sets `XDG_SESSION_ID` and `LIBSEAT_BACKEND=logind`, then `libseat_open_seat` takes control of that session. logind then owns the greeter VT mode (`VT_PROCESS`, `KD_GRAPHICS`, `K_OFF`) and restores it if the daemon dies, so a crash can no longer strand the VT.
- The greeter VT must be activated before `SeatService::open`, otherwise the session is inactive and the enable event never comes (open waits `seat_enable_timeout_ms`, then falls back).
- DRM and evdev are opened with `libseat_open_device`; logind revokes evdev and drops DRM master when another session becomes active, so the greeter cannot read keys during a user session even by mistake. `libseat_close_device` does not close the fd; `SeatService::close_device` does both.
- `libseat.h` has no `extern "C"` guard; include it inside `extern "C" { }`.
- Every user session gets its own free VT (`VtService::find_free`, `VT_OPENQRY`): the worker activates it before `pam_open_session`, logind disables the greeter session (`on_disable` → `deactivate`, then `libseat_disable_seat`). On session end the greeter calls `libseat_switch_session` back to its VT and `on_enable` → `activate()`. If the greeter VT becomes active while a session runs (`Ctrl+Alt+F1`), `on_seat_enable` switches straight back to the session VT.
- The worker clears the session VT (`\33[H\33[2J`) and sets `KD_GRAPHICS` (`VtService::hide_text`) before `VT_ACTIVATE`, and restores `KD_TEXT` (`show_text`) after the session or on a failed start. Activating it in `KD_TEXT` makes `fbcon` show that VT's text console until Xorg or the compositor takes it; in `KD_GRAPHICS` the last greeter frame stays on screen instead.
- A session VT must stay open from before `VT_ACTIVATE` until the session ends. When it became active with no session registered and nothing holding it, logind started `autovt@ttyN` (a getty) on it: the handoff showed that getty's prompt, and the getty kept the VT busy, so `VT_OPENQRY` gave every later login the next VT. The worker keeps its `VtService` fd open until it exits; Wayland sessions also take the VT as stdin and controlling tty (`take_as_stdin`, as root before the privilege drop), like SDDM's `UserSession::setupChildProcess`. X11 sessions skip that: Xorg runs without `-keeptty`, calls `setsid` and makes `vtN` its own controlling tty, so a second `TIOCSCTTY` would fail.
- `input_.close()` runs inside a libinput dispatch (Enter → login → `start_session`): `InputService` defers the real close until `dispatch` returns, otherwise the next `libinput_event_destroy` hits a freed context and aborts.
- If the holder or `libseat_open_seat` fails, the greeter logs it and falls back to the direct path below. If the holder dies later, the daemon quits and systemd restarts it.
- Test a build without touching the installed service: `sudo ./build/astralia-greet --vt 3` from a TTY.

## VT and DRM handoff (fallback path)

- The greeter holds its VT in `VT_PROCESS` mode (`SIGRTMIN` release, `SIGRTMIN + 1` acquire) with `KD_GRAPHICS` and `K_OFF`. On release: drop DRM master, suspend libinput, then `VT_RELDISP 1`. On acquire: `VT_ACKACQ`, then take master, modeset and repaint in full.
- Before launching a session on the same VT: suspend input (closes evdev fds), `drmDropMaster`, then restore `VT_AUTO`/`KD_TEXT`/the saved keyboard mode. Compositors then take the VT and DRM through logind.
- With `K_OFF` the kernel does not handle Ctrl+Alt+Fn, so the greeter maps `XF86Switch_VT_n` keysyms to `VT_ACTIVATE` itself.
- Input is closed (`InputService::close`, no evdev fds, no loop fd) from `start_session` until `on_session_end` reopens it; `suspend` alone is not enough because any `activate()` resumes it. `activate()` refuses while `worker_ > 0`.
- While a session worker runs (`worker_ > 0`), the greeter ignores VT release/acquire signals and all keys/clicks. A stray acquire right after Xorg takes the VT used to resume libinput, so keys typed in the session reached the greeter, every `Enter` became a failed PAM login, and `pam_faillock` locked the account system-wide (`sudo` included).
- Master status belongs to the open file description, so the worker inheriting the DRM fd is harmless once the daemon has dropped master.

## Xorg

- Xorg must run with `-noreset` (in `XorgSettings::args`): otherwise the server regenerates as soon as its last client disconnects, and `xrdb -merge` or the `display_setup` hook are the only clients before the session connects, so `RESOURCE_MANAGER` is wiped and `.Xresources` never reach the session.
- Readiness: Xorg sends `SIGUSR1` to its parent when it starts with `SIGUSR1` set to `SIG_IGN`. The worker blocks `SIGUSR1`/`SIGCHLD` and waits with `sigtimedwait`.
- `xrdb -merge` runs in the user child after the privilege drop, with the session `DISPLAY`/`XAUTHORITY`, before the session `Exec`. Running it as root or before Xorg is ready leaves the resource database empty. Its errors go to the session log.
- Cookies are written as one `FamilyWild` (`0xFFFF`) entry with an empty address, without `libXau`. The server file lives in `/run/astralia-greet/xauth-N` and the user copy in `$XDG_RUNTIME_DIR/xauth_astralia_<uid>-N` (`service::user_auth_path`, `/tmp` when `XDG_RUNTIME_DIR` is unset), `chown`ed to the user. One user file per display: a shared file was overwritten by a second session's cookie ("Authorization required" on the first display) and deleted by `XServerService::stop` when either session ended.

## Rendering budget (X201)

- No render loop. `Greeter::flush` clips to the damage rects, draws only the modules they touch, and copies only those rows into the dumb buffer, followed by `drmModeDirtyFB`.
- The background, avatars and images are decoded and scaled once (per layout or per path), never per frame.
- `Module::visible` returns whether a module shows in the current state. `Greeter::damage_widgets` and module timer ticks skip hidden modules, so a keystroke in the login view repaints only `password`. A module that shows or hides needs no damage of its own: view switches call `damage_all`. Draw and click checks call `visible` too, so keep the three in sync by never duplicating the view test.
- `DisplayService::present` merges overlapping damage rects (`render::merge_overlapping`) before copying, since padded rects overlap and the dumb buffer is write-combined memory. Copying a larger merged area is safe: outside the damaged areas the back buffer already equals what is on screen.
- `Background` passes the screen size to `load_image` in `Fill`/`Fit`/`Stretch` modes: a large JPEG is decoded with libjpeg DCT scaling (`scale_num`/8) and a large PNG with an integer box reduction while `libpng` reads rows (largest factor keeping the screen size), so a 4K wallpaper does not cost a full-size decode and a big `CAIRO_FILTER_GOOD` downscale at startup. `Center` and `Tile` keep the native size. The 4000x2249 `wallpaper.png` decodes to 2000x1125 at 1280x800.
- `load_image` picks the decoder from the magic bytes, not the extension: the old `wallpaper.png` was a JPEG. Interlaced PNGs cannot be reduced row by row and fall back to the full-size cairo decode.
- PNG alpha is premultiplied per pixel before averaging, since cairo `ARGB32` is premultiplied; a PNG without alpha or `tRNS` becomes `RGB24`.
- Estimated X201 costs: idle is two `timerfd` wakes per minute (`clock`, `date`); a view switch is a full 4 MB background paint (plus the `login_dim` fill in the login view, which replaced a second pre-dimmed 4 MB surface) and a 4 MB copy (a few ms); a keystroke repaints only the field. `clock_panel` has no tick of its own: the `clock`/`date` damage rects intersect it, so `flush` repaints it clipped under them (one small alpha fill per tick).
- Pointer motion calls `Module::hover` on every module; it returns `true` only when the hovered element changed (`power` keeps the hovered slot), so moving the pointer repaints nothing until it enters or leaves an icon.
- `render::draw_text` must end with `cairo_new_path`: `pango_cairo_show_layout` leaves a current point, and the next `cairo_arc` would draw a line from it.
- Timers use `CLOCK_BOOTTIME`, so the clock catches up immediately after suspend.
- `--preview` runs the same modules without VT, DRM or input and needs no root. Use `./build.sh test` (writes `preview.png` in the project root, then opens the test window) after any layout or style change.

## Cursor

- The DRM cursor buffer is 64x64; `XcursorLibraryLoadImage` needs no X connection, and its pixels are premultiplied ARGB like the buffer, so rows are copied as is (cropped at 64). `drmModeMoveCursor` places the buffer's top-left, so `DisplayService` subtracts the theme's hotspot.
- `--window` uses the X server's cursor and `--preview` draws none, so only the DRM path shows the theme.

## Test window

- `--window` (`./build.sh test`) runs the real modules, key/click handling and damage tracking in a full-screen X11 window without root, VT, DRM or libinput. It works under Xorg and Xwayland.
- It never launches a session, never writes the state store, skips autologin and VT switching, and only logs power requests. A successful PAM login shows `config::window::test_login_message`.
- PAM runs unprivileged, so only the invoking user's password can succeed (`unix_chkpwd`), and only if `/etc/pam.d/astralia-greet` is installed; otherwise PAM falls back to `other`.
- The keymap comes from the X server, not from the `keyboard_*` settings. Key state is tracked with `xkb_state_update_key` and reloaded on `FocusIn`. `xcb/xkb.h` is not usable from C++ (a struct field is named `explicit`), so XKB state events are not used.
- `Ctrl+Q` or closing the window quits.

## Overlays

- Click dispatch walks modules in reverse draw order, so modules that draw over others (the `user_menu` and `session_picker` drop-downs) must be last in `module_registry`.
- A module whose `bounds()` grows (open drop-down) must only shrink them in `draw`, not when the state changes: the damage computed right after closing still uses the expanded bounds, so the old area is repainted.
- Any click while a drop-down is open closes it; `Greeter::handle_click` enforces this after dispatch for each menu that was open before it.
- The drop-down code lives in `render/dropdown`, not in a module, because modules cannot include each other.

## Views

- `GreeterState::view` switches between the clock and login screens. A module hidden in the current view keeps its `bounds()` and returns `false` from `click`, so clicks pass through it.
- View switches must repaint the whole screen: the background is dimmed in the login view. `after_input` compares `state_.view` with `shown_view_` and calls `damage_all`, which also covers view changes made by modules (`clock` click).
- The login view shows only the dimmed wallpaper and `password`; the screen shows `"Skill Issue"` for every error, so `show_error` logs the real reason with `core::warn`.
- In the clock view a printable key both opens the login view and is typed into the active field; modifier keysyms (`Shift_L`..`Hyper_R`, ISO level/lock keys, `Num_Lock`) never unlock.
- A lock-style card layout (`astralia-shell-hl` `modules/lock`) was built and reverted (plan `020`): the user preferred this layout with the clock group boxed. Modules cannot read each other's geometry, so `clock_panel_config.h` hard-codes a rect around `clock`, `date` and `hint`; move it together with those three geometries. It returns `false` from `click`, so clicks reach `clock`.

## Fonts and icons

- `render::register_fonts()` must run before the first Pango layout: Pango snapshots the fontconfig map on creation and misses fonts added later.
- Icons always use the explicit `tabler-icons` family (`render::icon_font`): Tabler codepoints collide with Codicons inside the ComicShannsMono Nerd Font.
- The text font family is `Comic Shanns Mono` (with spaces), not the file name.

## Style constants

- There is no theme or config file: every value lives in `src/config/`, and changing the look means editing a header and rebuilding. Geometry `x`/`y` are always added to the anchored position (positive = right/down).
- The palette in `theme_config.h`, the background colors, hint `warn_color` and `corner_radius` values mirror `astralia-shell-i3/src/core/palette.h` (`palette` and `metrics`); update both together.
- Every size in `src/config/` is designed for the reference screen `config::reference_width`x`reference_height` (1920x1200) and multiplied by `render::ui_scale` = `min(width / 1920, height / 1200)` (1280x800 → 0.667, 1920x1080 → 0.9, 7680x2160 → 1.8). `render::place` scales geometry; every other pixel value (icon sizes, gaps, radii, item heights, echo size, padding) must be scaled in the module's `layout`, never in its constructor. Line widths go through `render::line_width` (min 1 px).
- Font sizes are Pango points: `render::set_text_scale` sets the default font map resolution to `96 * scale` before the module layouts (`Greeter::resize`, `render_preview`), so a size of 12 draws at about 16 px on the reference screen. `icon_font` takes pixels, which the resolution does not touch, so callers pass scaled sizes.
