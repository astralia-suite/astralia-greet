#pragma once

#include <cairo.h>
#include <cstdint>
#include <filesystem>
#include <memory>

#include "config/theme_config.h"

namespace render {

struct SurfaceDeleter {
    void operator()(cairo_surface_t *surface) const;
};

using SurfacePtr = std::unique_ptr<cairo_surface_t, SurfaceDeleter>;

class Canvas {
  public:
    Canvas(int width, int height);
    ~Canvas();
    Canvas(const Canvas &) = delete;
    Canvas &operator=(const Canvas &) = delete;

    bool valid() const;
    int width() const;
    int height() const;
    cairo_t *context() const;
    const std::uint8_t *data() const;
    int stride() const;
    bool write_png(const std::filesystem::path &path) const;

  private:
    int width_ = 0;
    int height_ = 0;
    SurfacePtr surface_;
    cairo_t *cr_ = nullptr;
};

void set_color(cairo_t *cr, const config::Color &color);
void rounded_rect(cairo_t *cr, double x, double y, double width, double height, double radius);

} // namespace render
