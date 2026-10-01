#include "render/cursor.h"

#include <X11/Xcursor/Xcursor.h>
#include <algorithm>
#include <cairo.h>
#include <cstring>

#include "config/display_config.h"

#include "core/log.h"

namespace render {

namespace {

bool draw_theme_cursor(std::uint8_t *data, int size, int stride, Hotspot &hotspot) {
    XcursorImage *image = XcursorLibraryLoadImage("left_ptr", config::display::cursor_theme, config::display::cursor_size);
    if (!image) {
        core::warn("cursor theme {} has no left_ptr, using the built-in arrow", config::display::cursor_theme);
        return false;
    }
    int width = std::min(static_cast<int>(image->width), size);
    int height = std::min(static_cast<int>(image->height), size);
    std::memset(data, 0, static_cast<std::size_t>(stride) * static_cast<std::size_t>(size));
    for (int y = 0; y < height; ++y) {
        std::memcpy(data + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride), image->pixels + static_cast<std::size_t>(y) * image->width, static_cast<std::size_t>(width) * 4);
    }
    hotspot = {static_cast<int>(image->xhot), static_cast<int>(image->yhot)};
    XcursorImageDestroy(image);
    return true;
}

} // namespace

Hotspot draw_cursor(std::uint8_t *data, int size, int stride) {
    Hotspot hotspot{1, 1};
    if (draw_theme_cursor(data, size, stride, hotspot)) {
        return hotspot;
    }
    cairo_surface_t *surface = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_ARGB32, size, size, stride);
    cairo_t *cr = cairo_create(surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_translate(cr, 1.5, 1.5);
    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, 0, 17);
    cairo_line_to(cr, 4.5, 12.8);
    cairo_line_to(cr, 7.5, 19.5);
    cairo_line_to(cr, 10, 18.4);
    cairo_line_to(cr, 7, 11.8);
    cairo_line_to(cr, 12.5, 11.8);
    cairo_close_path(cr);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, 1.0);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_stroke(cr);
    cairo_destroy(cr);
    cairo_surface_flush(surface);
    cairo_surface_destroy(surface);
    return hotspot;
}

} // namespace render
