#include <cairo.h>
#include <cstdint>
#include <cstdio>
#include <jpeglib.h>
#include <vector>

#include "render/canvas.h"
#include "render/image.h"

#include "test/check.h"

namespace {

bool write_jpeg(const std::filesystem::path &path, int width, int height) {
    std::FILE *file = std::fopen(path.c_str(), "wb");
    if (!file) {
        return false;
    }
    jpeg_compress_struct info{};
    jpeg_error_mgr error{};
    info.err = jpeg_std_error(&error);
    jpeg_create_compress(&info);
    jpeg_stdio_dest(&info, file);
    info.image_width = static_cast<JDIMENSION>(width);
    info.image_height = static_cast<JDIMENSION>(height);
    info.input_components = 3;
    info.in_color_space = JCS_RGB;
    jpeg_set_defaults(&info);
    jpeg_start_compress(&info, TRUE);
    std::vector<JSAMPLE> row(static_cast<std::size_t>(width) * 3, 0x80);
    JSAMPROW rows[] = {row.data()};
    while (info.next_scanline < info.image_height) {
        jpeg_write_scanlines(&info, rows, 1);
    }
    jpeg_finish_compress(&info);
    jpeg_destroy_compress(&info);
    std::fclose(file);
    return true;
}

std::uint32_t pixel_at(cairo_surface_t *surface, int x, int y) {
    cairo_surface_flush(surface);
    auto *pixels = reinterpret_cast<const std::uint32_t *>(cairo_image_surface_get_data(surface));
    return pixels[y * (cairo_image_surface_get_stride(surface) / 4) + x];
}

} // namespace

void test_image() {
    auto root = test::temp_dir("image");
    auto png = root / "red.png";
    {
        render::Canvas canvas(8, 4);
        cairo_set_source_rgb(canvas.context(), 1, 0, 0);
        cairo_paint(canvas.context());
        CHECK(canvas.write_png(png));
    }
    auto image = render::load_image(png);
    CHECK(image != nullptr);
    CHECK(cairo_image_surface_get_width(image.get()) == 8);
    CHECK(cairo_image_surface_get_height(image.get()) == 4);

    auto scaled = render::scaled_image(image.get(), 16);
    CHECK(cairo_image_surface_get_width(scaled.get()) == 16);
    CHECK(cairo_image_surface_get_height(scaled.get()) == 16);
    cairo_surface_flush(scaled.get());
    auto *pixels = reinterpret_cast<const std::uint32_t *>(cairo_image_surface_get_data(scaled.get()));
    auto stride = cairo_image_surface_get_stride(scaled.get()) / 4;
    CHECK(pixels[8 * stride + 8] == 0xFFFF0000u);

    auto wide_png = root / "wide.png";
    {
        render::Canvas canvas(9, 5);
        cairo_set_source_rgb(canvas.context(), 0, 1, 0);
        cairo_paint(canvas.context());
        CHECK(canvas.write_png(wide_png));
    }
    auto png_full = render::load_image(wide_png);
    CHECK(png_full != nullptr && cairo_image_surface_get_width(png_full.get()) == 9 && cairo_image_surface_get_height(png_full.get()) == 5);
    auto png_reduced = render::load_image(wide_png, 4, 2);
    CHECK(png_reduced != nullptr && cairo_image_surface_get_width(png_reduced.get()) == 5 && cairo_image_surface_get_height(png_reduced.get()) == 3);
    CHECK(png_reduced != nullptr && cairo_image_surface_get_format(png_reduced.get()) == CAIRO_FORMAT_RGB24);
    CHECK(png_reduced != nullptr && pixel_at(png_reduced.get(), 4, 2) == 0xFF00FF00u);
    auto png_unreduced = render::load_image(wide_png, 20, 20);
    CHECK(png_unreduced != nullptr && cairo_image_surface_get_width(png_unreduced.get()) == 9);

    auto alpha_png = root / "alpha.png";
    {
        render::SurfacePtr source(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 4, 4));
        cairo_t *cr = cairo_create(source.get());
        cairo_set_source_rgba(cr, 0, 0, 1, 128.0 / 255.0);
        cairo_paint(cr);
        cairo_destroy(cr);
        CHECK(cairo_surface_write_to_png(source.get(), alpha_png.c_str()) == CAIRO_STATUS_SUCCESS);
    }
    auto alpha = render::load_image(alpha_png, 2, 2);
    CHECK(alpha != nullptr && cairo_image_surface_get_width(alpha.get()) == 2 && cairo_image_surface_get_format(alpha.get()) == CAIRO_FORMAT_ARGB32);
    CHECK(alpha != nullptr && pixel_at(alpha.get(), 1, 1) == 0x80000080u);

    auto jpeg = root / "wide.jpg";
    CHECK(write_jpeg(jpeg, 1200, 576));
    auto full = render::load_image(jpeg);
    CHECK(full != nullptr && cairo_image_surface_get_width(full.get()) == 1200);
    auto reduced = render::load_image(jpeg, 300, 140);
    CHECK(reduced != nullptr && cairo_image_surface_get_width(reduced.get()) == 300 && cairo_image_surface_get_height(reduced.get()) == 144);
    auto unreduced = render::load_image(jpeg, 1280, 800);
    CHECK(unreduced != nullptr && cairo_image_surface_get_width(unreduced.get()) == 1200);

    test::write_text(root / "junk.png", "not an image");
    CHECK(render::load_image(root / "junk.png") == nullptr);
    CHECK(render::load_image(root / "absent.png") == nullptr);
    std::filesystem::remove_all(root);
}
