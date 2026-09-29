#include "render/cursor.h"

#include <cairo.h>

namespace render {

void draw_cursor(std::uint8_t *data, int size, int stride) {
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
}

} // namespace render
