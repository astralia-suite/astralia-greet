#include "render/canvas.h"

#include <algorithm>
#include <numbers>

namespace render {

void SurfaceDeleter::operator()(cairo_surface_t *surface) const {
    cairo_surface_destroy(surface);
}

Canvas::Canvas(int width, int height)
    : width_(width), height_(height), surface_(cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height)) {
    if (cairo_surface_status(surface_.get()) == CAIRO_STATUS_SUCCESS) {
        cr_ = cairo_create(surface_.get());
    }
}

Canvas::~Canvas() {
    if (cr_) {
        cairo_destroy(cr_);
    }
}

bool Canvas::valid() const {
    return cr_ && cairo_status(cr_) == CAIRO_STATUS_SUCCESS;
}

int Canvas::width() const {
    return width_;
}

int Canvas::height() const {
    return height_;
}

cairo_t *Canvas::context() const {
    return cr_;
}

const std::uint8_t *Canvas::data() const {
    return cairo_image_surface_get_data(surface_.get());
}

int Canvas::stride() const {
    return cairo_image_surface_get_stride(surface_.get());
}

bool Canvas::write_png(const std::filesystem::path &path) const {
    cairo_surface_flush(surface_.get());
    return cairo_surface_write_to_png(surface_.get(), path.c_str()) == CAIRO_STATUS_SUCCESS;
}

void set_color(cairo_t *cr, const config::Color &color) {
    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
}

void rounded_rect(cairo_t *cr, double x, double y, double width, double height, double radius) {
    radius = std::clamp(radius, 0.0, std::min(width, height) / 2.0);
    constexpr double quarter = std::numbers::pi / 2.0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + width - radius, y + radius, radius, -quarter, 0.0);
    cairo_arc(cr, x + width - radius, y + height - radius, radius, 0.0, quarter);
    cairo_arc(cr, x + radius, y + height - radius, radius, quarter, 2.0 * quarter);
    cairo_arc(cr, x + radius, y + radius, radius, 2.0 * quarter, 3.0 * quarter);
    cairo_close_path(cr);
}

} // namespace render
