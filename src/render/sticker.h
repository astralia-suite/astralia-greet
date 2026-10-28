#pragma once

#include <cairo.h>

#include "config/theme_config.h"

#include "render/layout.h"
#include "render/style.h"

namespace render {

config::Color mix(const config::Color &from, const config::Color &to, double amount);
void rounded_box(cairo_t *cr, const Box &box, double radius, const config::Color &fill, double border = 0.0, const config::Color &outline = {});
void sticker(cairo_t *cr, const Box &box, double radius, const config::Color &fill, const config::Color &outline, double border, double shadow, double ring);
void selectable(cairo_t *cr, const Box &box, bool selected, bool hovered, const config::Color &idle, const Style &style);

} // namespace render
