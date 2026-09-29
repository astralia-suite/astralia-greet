#include "modules/background.h"

#include <filesystem>

#include "config/background_config.h"

#include "core/log.h"

#include "render/image.h"

namespace modules {

void Background::layout(int width, int height) {
    namespace defaults = config::background;
    rect_ = {0, 0, width, height};
    surface_.reset(cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height));
    cairo_t *cr = cairo_create(surface_.get());
    cairo_pattern_t *gradient = cairo_pattern_create_linear(0, 0, 0, height);
    cairo_pattern_add_color_stop_rgb(gradient, 0, defaults::color.r, defaults::color.g, defaults::color.b);
    cairo_pattern_add_color_stop_rgb(gradient, 1, defaults::color2.r, defaults::color2.g, defaults::color2.b);
    cairo_set_source(cr, gradient);
    cairo_paint(cr);
    cairo_pattern_destroy(gradient);
    std::error_code ec;
    std::filesystem::path path = std::filesystem::exists(defaults::image, ec) ? defaults::image : defaults::source_image;
    bool scaled = defaults::mode == config::BackgroundMode::Fill || defaults::mode == config::BackgroundMode::Fit || defaults::mode == config::BackgroundMode::Stretch;
    if (auto image = scaled ? render::load_image(path, width, height) : render::load_image(path)) {
        render::paint_image(cr, image.get(), rect_, defaults::mode);
    } else {
        core::warn("cannot load background image {}", path.string());
    }
    if (defaults::dim > 0.0) {
        cairo_set_source_rgba(cr, 0, 0, 0, defaults::dim);
        cairo_paint(cr);
    }
    cairo_destroy(cr);
}

render::Rect Background::bounds() const {
    return rect_;
}

void Background::draw(cairo_t *cr, const app::GreeterState &state) {
    cairo_set_source_surface(cr, surface_.get(), 0, 0);
    cairo_paint(cr);
    if (state.view == app::View::Login) {
        cairo_set_source_rgba(cr, 0, 0, 0, config::background::login_dim);
        cairo_paint(cr);
    }
}

bool Background::is_backdrop() const {
    return true;
}

} // namespace modules
