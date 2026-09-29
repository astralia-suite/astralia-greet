#pragma once

#include <cairo.h>
#include <filesystem>

#include "config/theme_config.h"

#include "render/canvas.h"
#include "render/layout.h"

namespace render {

SurfacePtr load_image(const std::filesystem::path &path, int min_width = 0, int min_height = 0);
void paint_image(cairo_t *cr, cairo_surface_t *image, const Rect &area, config::BackgroundMode mode);
SurfacePtr scaled_image(cairo_surface_t *image, int size);

} // namespace render
