# `astralia-greet` developing conventions

## Commenting

- No comments across the code base.
- Exceptions:
  - Namespace comment.
  - Comments that group constants together, wherever those constants live (`src/config/` or a file's own constants).
  - License/attribution notices for third-party code.

## Formatting

- Command: `clang-format -i <filename>.cpp`.
- Style taken from `.clang-format`.

## Module boundary

- A module is a greeter UI element: `src/modules/<name>.h`+`.cpp` plus, when split, its private components under `src/modules/<name>/`.
- A module is not allowed to include files from another module, its private components included.
- A module shall manage its internal works, without bleeding into `main.cpp`.
- `main.cpp` shall not include specific components belonging to a module.

## Config headers

- `src/config/*.h` holds constants and plain data types only, no function bodies (helpers that compute from a config value live with their consumer).
- One config header per module or service, named `<name>_config.h`; a module's private components share it rather than getting their own.

## Service structure

- `src/service/` holds as many services as needed, but limited to one pair of `**_service.{h,cpp}` per service.
- A service owns one system concern (PAM, VT, DRM, input, sessions, X server, power) and never draws.
- Modules read services; services never include modules.

## Includes

- `meson.build` adds `include_directories('src')` to both the `astralia-greet` and `astralia-greet-test` targets.
- Every local `#include` is root-relative from `src/`, e.g. `#include "core/log.h"`, never `../` or a bare filename.
- `test/**` includes `src/` headers the same root-relative way, e.g. `#include "core/ini.h"`; test helpers are included from the project root, e.g. `#include "test/check.h"`.
- `src/modules/**` is included only by `src/app/module_registry.cpp`.
- Header order:
    - system headers (`<header>`), one block; a blank line may split it only where include order matters and `clang-format` would otherwise reorder it (e.g. an `extern "C"` block)
    - one blank line
    - local headers:
        - `"dir1/local_header.h"`
        - blank
        - `"dir2/local_header.h"`

## Performance budget

- Target hardware is the ThinkPad X201 (Intel GMA HD, 1280x800, dual-core Arrandale i5): every feature must stay idle at ~0% CPU.
- No continuous render loop: redraw only on input, timer (clock) or state change, and only the damaged region.
- No GPU/GL dependency; rendering is `cairo` on a DRM dumb buffer.
- Images are decoded and scaled once at load, never per frame.
