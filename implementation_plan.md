# Resolution-independent layout

Goal: the greeter looks the same, in proportion, on every screen from 1366x768 to 7680x2160. Every size in `src/config/` is designed for the reference screen 1920x1200 (CF-SV8) and multiplied by one factor.

## Approach

`scale = min(width / 1920, height / 1200)`. Every target screen is 16:10 or wider, so in practice this is `height / 1200`: 1280x800 → 0.667, 1366x768 → 0.64, 1920x1080 → 0.9, 3840x2160 and 7680x2160 → 1.8.

- Geometry: `render::place` takes the scale and multiplies `x`/`y`/`width`/`height`.
- Text: the default Pango font map resolution becomes `96 * scale`, so the point sizes in `src/config/` are left unchanged. Set once per layout, before any text is measured.
- Pixel values outside `place` (icon px sizes, gaps, item heights, radii, the echo image, padding) are multiplied in each module's `layout`.
- Line widths never go below 1 px.
- No minimum text size is needed: the smallest text (password, 13 pt) is ~11 px at 0.64.

## Files

- `[MODIFY] src/config/theme_config.h` — `reference_width` (1920), `reference_height` (1200).
- `[MODIFY] src/render/layout.{h,cpp}` — `ui_scale(width, height)`, `scaled(int, scale)`, `scaled(double, scale)`, `line_width(double, scale)` (min 1 px); `place` gets a `scale` parameter.
- `[MODIFY] src/render/text.{h,cpp}` — `set_text_scale(scale)` sets the default font map resolution.
- `[MODIFY] src/app/greeter.cpp` — `resize` and `render_preview` call `set_text_scale(ui_scale(...))` before the module layouts.
- `[MODIFY] src/modules/{clock,date,hint,clock_panel}.cpp` — pass the scale to `place`; `clock_panel` scales radius and border.
- `[MODIFY] src/modules/password.{h,cpp}` — keep the decoded echo image, scale it to `echo_size * scale` in `layout`; scale padding, caret gap, caret and border widths.
- `[MODIFY] src/modules/power.{h,cpp}` — icon size, spacing, font and width computed in `layout`.
- `[MODIFY] src/modules/{session_picker,user_menu}.cpp` — fill the `Dropdown` pixel fields (fonts, item height, gaps, radius, border) in `layout`.
- `[MODIFY] test/render/test_layout.cpp` — existing `place` checks at scale 1, plus `ui_scale` and a scaled `place`.
- `[MODIFY] important/knowledge.md`, `important/index.md` — document the reference screen and scaling.

## Out of scope

- Capping the layout width on ultrawide screens (corner widgets sit at the far corners of 7680x2160).
- Physical (mm) sizing.

## Verification

`./build.sh` (build + unit tests), then `--preview` at 1280x800, 1366x768, 1920x1200 and 7680x2160.
