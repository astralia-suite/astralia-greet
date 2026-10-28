# `astralia-greet` knowledge

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

## Network and Bluetooth

- The daemon is root, so NetworkManager (polkit) and BlueZ (D-Bus policy) accept its calls: no polkit rule or group is needed. `--window` runs as the invoking user and depends on that user's polkit rights.
- `service::NetworkService` and `service::BluetoothService` each own a `core::Bus`. Property reads and `WirelessEnabled`/`Powered` writes are synchronous (3 s timeout); `ActivateConnection`, `DeactivateConnection` and the BlueZ `Connect`/`Disconnect` (up to 40 s) are asynchronous.
- A synchronous call can queue signals inside sd-bus without making the fd readable again, so every refresh and write calls `Bus::process()` afterwards.
- sd-bus timeouts need a timer: `Bus::arm_timeout` re-arms a one-shot `timerfd` from `sd_bus_get_timeout` after every pass, and its callback clears `timer_` first because a fired one-shot timer fd can be reused by another timer.
- A service is unavailable (and its module hidden) while the bus name has no owner, found with `NameHasOwner` before any property read: reading a property of an absent name would auto-start the service through D-Bus activation. `NameOwnerChanged` triggers a refresh, so services that start later appear without a restart. A lost bus connection marks both unavailable and is not reopened.
- Signals only schedule a 100 ms debounced refresh that rebuilds the whole snapshot; `on_change` fires only when the snapshot differs, so unchanged signals (scan results, RSSI) repaint nothing. NetworkManager matches are limited to the manager `PropertiesChanged`, `Connection.Active.StateChanged` and `Settings` signals to avoid access point churn.
- Only system-owned Wi-Fi profiles are listed; profiles whose secrets live in a user keyring cannot be activated from the greeter. No Bluetooth pairing. Unsupported Wi-Fi security (WEP, 802.1X/enterprise, OWE) is listed as `Not supported` without a connect button.
- Wi-Fi sections (as in the shell): `Connected` (active or connecting profile), `Known` (saved profiles whose SSID is in range, strongest first) and `Available` (in-range SSIDs without a saved profile, strongest first, subtitle = security). Saved profiles are matched to access points by SSID (`802-11-wireless.ssid`, an `ay` read by `Bus::read_properties`), not by connection id; a saved network out of range is hidden. Rows are sorted by `signal_band` (the shell's thresholds 75/50/25) then raw strength; the snapshot stores only the band so signal jitter repaints nothing, and the row glyph is `wifi`/`wifi2`/`wifi1`/`wifi0`.
- Scan lifecycle: access points are read (one `GetAll` per AP, strongest per SSID kept) only while the Network panel is open. `Greeter::sync_scan` (end of `after_input`, `deactivate`, `on_session_end`) calls `NetworkService::begin_scan` on the closed-to-open transition (adds the wireless-device signal match kept in `scan_slot_`, sends `RequestScan`, schedules a refresh 20 ms later so the panel opens at once) and `end_scan` on the way back (drops the match; no AP traffic or wakeups while closed, 0 changes in 3 s measured). While open, AP signals are debounced to one refresh per 500 ms; other NetworkManager signals keep the 100 ms debounce. Saved-profile data (`profile_cache_`) is cached and invalidated by the `Settings` `NewConnection`/`ConnectionRemoved` and `Settings.Connection.Updated` signals, so a refresh no longer re-reads every profile (42 profiles: about 31 ms before, 18 ms with 20 APs after the cache, measured on a desktop CPU; the X201 is estimated at 5-8x).
- Security classification (`classify_security`): key-management bits of `WpaFlags|RsnFlags` (mask `0xFF00`): 802.1X `0x200` or any unknown bit is Enterprise, PSK `0x100` is Psk (also WPA2/WPA3 transition), SAE `0x400` only is Sae, otherwise `PRIVACY` (flag `0x1`) is Wep, else Open.
- Connecting to an unsaved network: `NetworkService::connect(ap, password)` sends `AddAndActivateConnection` with a hand-built `a{sa{sv}}` (connection type and id, `802-11-wireless` ssid bytes and mode, and for secured networks `802-11-wireless-security` with `key-mgmt` `wpa-psk` or `sae` and the `psk`); the daemon is root, so NetworkManager stores a system profile that becomes `Known`. The reply gives the new profile (`pending_profile_`); on a later refresh, if its activation ended without activating (wrong password), the profile is deleted (`Settings.Connection.Delete`) and `snapshot.error` ("Could not connect to <ssid>") shows as an error banner until the next scan start, panel close or connect attempt.
- Password dialog: a card under the panel (`ScrollPanel::dialog_area`, reserved with `PanelContent::dialog_reserve` so the card height leaves room, painted every frame because it changes per keystroke, regions kept in `dialog_regions_`). While `GreeterState::wifi_dialog_open()` (`wifi_ap` non-empty) `Greeter::handle_key` routes keys to `handle_wifi_key` before the menu handling and before the clock-view handling (which would otherwise open the login view): printable keys append to `wifi_password`, `Backspace`/`Ctrl+U` edit, `Return` connects (PSK needs 8 characters, SAE 1), `Escape` cancels the dialog only. Open networks connect at once without a dialog (`request_target` carries the AP path; `wifi_ap` stays empty).
- Password handling: `wifi_password` is reserved up front and cleared with `secure_clear` (`GreeterState::clear_wifi_dialog`) on submit, cancel, `close_menus`, panel close, outside click and unavailability; it is passed to NetworkManager over the system bus only and no log call mentions it.
- `Greeter::on_radio_change` copies the snapshots into `GreeterState`, then for each module damages the old bounds, calls `Module::update`, and damages the new bounds when it returned true (clock view only). `update` hands the snapshot to the panel content and calls `ScrollPanel::invalidate`, which rebuilds the cached surfaces at once when the panel is visible (the card height follows the content, so the bounds must be right before the damage is computed).
- The panels are astralia-shell scroll panels (`render/scroll_panel`, ported from `astralia-shell` `Panel`, `scroll`, `sticker`, `widgets`): a paper card between two rollers, title tag, toggle and close button in the header, sticker rows. Colors come from `config/style_config.h`, shared with the rest of the greeter.
- X201 budget for the panels: header and body are painted once into cached `ARGB32` surfaces (`chrome_`, `body_`) when a panel opens or its snapshot changes; paper, rollers and header are one more cached surface (`shell_`), so a redraw or a wheel step is two blits. The body is painted lazily in bands (`PanelContent::paint` draws the rows whose top lies in `[view.y, view.y + view.h)` at absolute y; `ScrollPanel::paint_body_to` extends the painted range to two view heights past the scroll offset on open and on every wheel step), so a 42-profile list costs about the visible rows. `render::text` caches parsed `PangoFontDescription`s. Measured on a modern desktop CPU at 1280x800 (the X201 is estimated at 5-8x slower): warm open 6-7 ms (cold first open ~17 ms), settled redraw 0.9 ms. There is no animation: a panel appears and disappears at once and no timer exists for it.
- Input rules: a click on the icon toggles the panel; the close button, Escape, Return and any click that no module consumed close it (instantly) (`Module::clicked_outside`, called by `Greeter::handle_click` for every module that did not consume the click, replaces the old forced close for these two). Only the connect/disconnect icon button and the header toggle are regions, not whole rows (as in the shell). A panel hidden by a view change is closed in `draw`.
- Scroll input: `InputService` turns libinput `POINTER_SCROLL_WHEEL` (v120, 120 per notch) and `SCROLL_FINGER`/`SCROLL_CONTINUOUS` (about 15 units per notch) into `on_scroll(x, y, notches)`; `WindowService` maps X11 buttons 4/5 to ∓1 notch. `Greeter::handle_scroll` offers it to the module under the pointer (reverse draw order) through `Module::scroll`; `Up`/`Down` while a radio panel is open call `Greeter::scroll_menus` instead of cycling the session. One notch is `config::scroll::wheel_pixels` (40 reference px, as the shell's X11 panels); touchpad direction is whatever libinput delivers (no natural-scroll setting is applied).

## Fonts and icons

- `render::register_fonts()` must run before the first Pango layout: Pango snapshots the fontconfig map on creation and misses fonts added later.
- Icons always use the explicit `tabler-icons` family (`render::icon_font`): Tabler codepoints collide with Codicons inside the ComicShannsMono Nerd Font.
- The text font family is `Comic Shanns Mono` (with spaces), not the file name.

## Style constants

- There is no theme or config file: every value lives in `src/config/`, and changing the look means editing a header and rebuilding. Geometry `x`/`y` are always added to the anchored position (positive = right/down).
- The whole greeter uses the astralia-shell look: the shell's default theme (`theme_from_seed` with no wallpaper seeds, hue 250, light blue paper) and its sticker style (`ink` outline, offset drop shadow, inner `paper` ring, `selectable` faces with `pop_a` on hover and `accent` when selected). Palette and metrics are `constexpr` values in `config/style_config.h` (`config::style`), generated with the shell's `render/theme.cpp`; regenerate them there when the shell's default theme changes. The old purple greeter palette is gone from `theme_config.h`.
- Mapping from the shell lock screen: `clock_panel` is `sticker(surface)`, `clock` hour `text` and minute `lavender`, both bold, `date` bold `text`, `hint` `text_dim`/`warn`/`critical`, `password` the `pop_b` box with `lock` icon, arrow button and echo dots, pickers/menus/power/network/bluetooth `selectable` stickers on `accent_container`. The login dim is `window_backdrop` over the wallpaper.
- `render::Style` (`render/style.h`, built by `make_style(scale)`) holds every scaled sticker/panel metric and the panel fonts; each module builds one in `layout`.
- Every size in `src/config/` is designed for the reference screen `config::reference_width`x`reference_height` (1920x1200) and multiplied by `render::ui_scale` = `min(width / 1920, height / 1200)` (1280x800 → 0.667, 1920x1080 → 0.9, 7680x2160 → 1.8). `render::place` scales geometry; every other pixel value (icon sizes, gaps, radii, item heights, echo size, padding) must be scaled in the module's `layout`, never in its constructor. Line widths go through `render::line_width` (min 1 px).
- Font sizes are Pango points: `render::set_text_scale` sets the default font map resolution to `96 * scale` before the module layouts (`Greeter::resize`, `render_preview`), so a size of 12 draws at about 16 px on the reference screen. `icon_font` takes pixels, which the resolution does not touch, so callers pass scaled sizes.
- `clock_panel` renders its sticker into a surface once per layout and blits it, so the minute tick costs one clipped blit (0.30 ms against 0.42 ms for the old fill and stroke, full repaint 1.4 ms against 1.7 ms, measured on a desktop CPU at 1280x800).
- The password arrow button is clickable and requests a login (only with text typed and not busy); the focus caret and the focus border of the old field are gone, as in the shell.
- Top-right icon row: `power` (3 buttons of 32 px with 8 px gaps, 24 px from the edge), then `bluetooth`, then `network`, each 32 px wide at y 20; their x offsets (`-144`, `-184`) are hard-coded for three power actions and must move with `power_config.h`.
