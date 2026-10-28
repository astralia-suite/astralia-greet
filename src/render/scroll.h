#pragma once

#include <cairo.h>

#include "render/layout.h"
#include "render/style.h"

namespace render {

struct ScrollGeometry {
    Box paper;
    Box first_roller;
    Box second_roller;
};

ScrollGeometry scroll_geometry(const Box &panel, const Style &style);
Box scroll_bounds(const ScrollGeometry &geometry);
void paint_scroll(cairo_t *cr, const ScrollGeometry &geometry, const Style &style);

} // namespace render
