#include "render/text.h"

#include <format>
#include <pango/pangocairo.h>
#include <string>

#include "render/canvas.h"
#include "render/icons.h"

namespace render {

namespace {

PangoLayout *make_layout(cairo_t *cr, std::string_view text, std::string_view font) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    std::string description(font);
    PangoFontDescription *desc = pango_font_description_from_string(description.c_str());
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);
    pango_layout_set_text(layout, text.data(), static_cast<int>(text.size()));
    return layout;
}

PangoAlignment to_pango(TextAlign align) {
    switch (align) {
    case TextAlign::Left:
        return PANGO_ALIGN_LEFT;
    case TextAlign::Right:
        return PANGO_ALIGN_RIGHT;
    default:
        return PANGO_ALIGN_CENTER;
    }
}

} // namespace

void set_text_scale(double scale) {
    pango_cairo_font_map_set_resolution(PANGO_CAIRO_FONT_MAP(pango_cairo_font_map_get_default()), 96.0 * scale);
}

std::string text_font(int size, bool bold) {
    return std::format("{}{} {}", config::font_family, bold ? " Bold" : "", size);
}

std::string icon_font(int pixel_size) {
    return std::format("{} {}px", icon::font_family, pixel_size);
}

TextSize measure_text(cairo_t *cr, std::string_view text, std::string_view font) {
    PangoLayout *layout = make_layout(cr, text, font);
    TextSize size;
    pango_layout_get_pixel_size(layout, &size.width, &size.height);
    g_object_unref(layout);
    return size;
}

void draw_text(cairo_t *cr, std::string_view text, std::string_view font, const config::Color &color, const Rect &box, TextAlign align) {
    if (text.empty()) {
        return;
    }
    PangoLayout *layout = make_layout(cr, text, font);
    pango_layout_set_width(layout, box.width * PANGO_SCALE);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
    pango_layout_set_alignment(layout, to_pango(align));
    int width = 0;
    int height = 0;
    pango_layout_get_pixel_size(layout, &width, &height);
    set_color(cr, color);
    cairo_move_to(cr, box.x, box.y + (box.height - height) / 2.0);
    pango_cairo_show_layout(cr, layout);
    cairo_new_path(cr);
    g_object_unref(layout);
}

} // namespace render
