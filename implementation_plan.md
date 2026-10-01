# Cursor themes on the greeter

Goal: the DRM hardware cursor shows an XCursor theme (`/usr/share/icons/<theme>/cursors/left_ptr`) instead of the hand-drawn arrow.

## Approach

`libXcursor` (already installed, 1.2.3) reads theme files without an X connection: `XcursorLibraryLoadImage("left_ptr", theme, size)` returns ARGB pixels (premultiplied, same as cairo `ARGB32`) plus `xhot`/`yhot`. The theme name follows `index.theme` `Inherits=`, so `"default"` resolves to whatever `/usr/share/icons/default` points at.

## Files

- `[NEW] src/config/display_config.h` — `cursor_theme` (default `"default"`, i.e. the system-wide theme) and `cursor_size` (24).
- `[MODIFY] meson.build` — add `dependency('xcursor')` to `astralia_greet_deps`.
- `[MODIFY] src/render/cursor.{h,cpp}` — `draw_cursor` takes theme and size, loads `left_ptr` with Xcursor, copies it row by row into the 64x64 buffer (cropped to 64), returns the hotspot. Missing theme/image → the current hand-drawn arrow, hotspot `{1, 1}`.
- `[MODIFY] src/service/display_service.{h,cpp}` — store the hotspot; `move_cursor`/`show_cursor` pass `x - hot_x`, `y - hot_y` to `drmModeMoveCursor` (the arrow tip currently sits at the top-left, so themes with a centered hotspot would be off without this).
- `[MODIFY] important/index.md`, `important/critical-knowledge.md` — note the theme lookup and the hotspot offset.

## Out of scope

- `--window` test mode: the X server already draws the host session's themed cursor.
- `--preview`: no cursor is drawn.
- Cursor sizes above 64 px: DRM cursor planes are commonly capped at 64x64; larger images get cropped.

## Open question

Should `cursor_theme` default to `"default"` (system-wide theme), or name your theme directly (`"Keqing"`)?

## Verification

`./build.sh` and `meson test -C build`, then `sudo ./build/astralia-greet --vt 3` from a TTY to check the cursor.
