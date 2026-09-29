#include "render/image.h"

#include <algorithm>
#include <array>
#include <csetjmp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <jpeglib.h>
#include <png.h>

namespace render {

namespace {

struct JpegError {
    jpeg_error_mgr base;
    std::jmp_buf jump;
};

void jpeg_fail(j_common_ptr info) {
    auto *error = reinterpret_cast<JpegError *>(info->err);
    std::longjmp(error->jump, 1);
}

SurfacePtr decode_png_full(const std::filesystem::path &path) {
    SurfacePtr surface(cairo_image_surface_create_from_png(path.c_str()));
    if (cairo_surface_status(surface.get()) != CAIRO_STATUS_SUCCESS) {
        return nullptr;
    }
    return surface;
}

unsigned pick_png_factor(unsigned width, unsigned height, int min_width, int min_height) {
    if (min_width <= 0 || min_height <= 0) {
        return 1;
    }
    unsigned factor = 1;
    while ((width + factor) / (factor + 1) >= static_cast<unsigned>(min_width) && (height + factor) / (factor + 1) >= static_cast<unsigned>(min_height)) {
        ++factor;
    }
    return factor;
}

SurfacePtr decode_png(const std::filesystem::path &path, int min_width, int min_height) {
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) {
        return nullptr;
    }
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png ? png_create_info_struct(png) : nullptr;
    if (!info) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        std::fclose(file);
        return nullptr;
    }
    cairo_surface_t *volatile surface = nullptr;
    png_bytep volatile row = nullptr;
    std::uint32_t *volatile sums = nullptr;
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        std::fclose(file);
        std::free(row);
        std::free(sums);
        if (surface) {
            cairo_surface_destroy(surface);
        }
        return nullptr;
    }
    png_init_io(png, file);
    png_read_info(png, info);
    if (png_get_interlace_type(png, info) != PNG_INTERLACE_NONE) {
        png_destroy_read_struct(&png, &info, nullptr);
        std::fclose(file);
        return decode_png_full(path);
    }
    png_uint_32 width = png_get_image_width(png, info);
    png_uint_32 height = png_get_image_height(png, info);
    bool alpha = (png_get_color_type(png, info) & PNG_COLOR_MASK_ALPHA) || png_get_valid(png, info, PNG_INFO_tRNS);
    png_set_expand(png);
    png_set_strip_16(png);
    png_set_gray_to_rgb(png);
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    png_read_update_info(png, info);
    unsigned factor = pick_png_factor(width, height, min_width, min_height);
    unsigned out_width = (width + factor - 1) / factor;
    unsigned out_height = (height + factor - 1) / factor;
    surface = cairo_image_surface_create(alpha ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24, static_cast<int>(out_width), static_cast<int>(out_height));
    row = static_cast<png_bytep>(std::malloc(png_get_rowbytes(png, info)));
    sums = static_cast<std::uint32_t *>(std::calloc(static_cast<std::size_t>(out_width) * 4, sizeof(std::uint32_t)));
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS || !row || !sums) {
        png_error(png, "out of memory");
    }
    unsigned char *data = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    for (png_uint_32 y = 0; y < height; ++y) {
        png_read_row(png, row, nullptr);
        for (png_uint_32 x = 0; x < width; ++x) {
            const png_byte *pixel = row + x * 4;
            std::uint32_t a = pixel[3];
            std::uint32_t *sum = sums + (x / factor) * 4;
            sum[0] += alpha ? (pixel[0] * a + 127) / 255 : pixel[0];
            sum[1] += alpha ? (pixel[1] * a + 127) / 255 : pixel[1];
            sum[2] += alpha ? (pixel[2] * a + 127) / 255 : pixel[2];
            sum[3] += a;
        }
        if ((y + 1) % factor != 0 && y + 1 != height) {
            continue;
        }
        std::uint32_t rows = y % factor + 1;
        auto *out = reinterpret_cast<std::uint32_t *>(data + static_cast<std::size_t>(y / factor) * static_cast<std::size_t>(stride));
        for (unsigned x = 0; x < out_width; ++x) {
            std::uint32_t count = rows * std::min(factor, width - x * factor);
            std::uint32_t *sum = sums + x * 4;
            std::uint32_t a = alpha ? (sum[3] + count / 2) / count : 0xFFu;
            out[x] = (a << 24) | ((sum[0] + count / 2) / count << 16) | ((sum[1] + count / 2) / count << 8) | (sum[2] + count / 2) / count;
            sum[0] = sum[1] = sum[2] = sum[3] = 0;
        }
    }
    cairo_surface_mark_dirty(surface);
    png_read_end(png, nullptr);
    png_destroy_read_struct(&png, &info, nullptr);
    std::fclose(file);
    std::free(row);
    std::free(sums);
    return SurfacePtr(surface);
}

void pick_jpeg_scale(jpeg_decompress_struct &info, int min_width, int min_height) {
    if (min_width <= 0 || min_height <= 0) {
        return;
    }
    constexpr unsigned denom = 8;
    for (unsigned num = 1; num < denom; ++num) {
        auto width = (info.image_width * num + denom - 1) / denom;
        auto height = (info.image_height * num + denom - 1) / denom;
        if (width >= static_cast<unsigned>(min_width) && height >= static_cast<unsigned>(min_height)) {
            info.scale_num = num;
            info.scale_denom = denom;
            return;
        }
    }
}

SurfacePtr decode_jpeg(const std::filesystem::path &path, int min_width, int min_height) {
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) {
        return nullptr;
    }
    jpeg_decompress_struct info{};
    JpegError error{};
    info.err = jpeg_std_error(&error.base);
    error.base.error_exit = jpeg_fail;
    cairo_surface_t *volatile surface = nullptr;
    if (setjmp(error.jump)) {
        jpeg_destroy_decompress(&info);
        std::fclose(file);
        if (surface) {
            cairo_surface_destroy(surface);
        }
        return nullptr;
    }
    jpeg_create_decompress(&info);
    jpeg_stdio_src(&info, file);
    jpeg_read_header(&info, TRUE);
    info.out_color_space = JCS_RGB;
    pick_jpeg_scale(info, min_width, min_height);
    jpeg_start_decompress(&info);
    surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, static_cast<int>(info.output_width), static_cast<int>(info.output_height));
    unsigned char *data = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    JSAMPARRAY row = (*info.mem->alloc_sarray)(reinterpret_cast<j_common_ptr>(&info), JPOOL_IMAGE, info.output_width * 3, 1);
    while (info.output_scanline < info.output_height) {
        auto y = info.output_scanline;
        jpeg_read_scanlines(&info, row, 1);
        auto *out = reinterpret_cast<std::uint32_t *>(data + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride));
        for (JDIMENSION x = 0; x < info.output_width; ++x) {
            const JSAMPLE *pixel = row[0] + x * 3;
            out[x] = 0xFF000000u | (static_cast<std::uint32_t>(pixel[0]) << 16) | (static_cast<std::uint32_t>(pixel[1]) << 8) | pixel[2];
        }
    }
    cairo_surface_mark_dirty(surface);
    jpeg_finish_decompress(&info);
    jpeg_destroy_decompress(&info);
    std::fclose(file);
    return SurfacePtr(surface);
}

} // namespace

SurfacePtr load_image(const std::filesystem::path &path, int min_width, int min_height) {
    std::array<unsigned char, 4> magic{};
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream.read(reinterpret_cast<char *>(magic.data()), magic.size())) {
            return nullptr;
        }
    }
    if (magic[0] == 0x89 && magic[1] == 'P' && magic[2] == 'N' && magic[3] == 'G') {
        return decode_png(path, min_width, min_height);
    }
    if (magic[0] == 0xFF && magic[1] == 0xD8 && magic[2] == 0xFF) {
        return decode_jpeg(path, min_width, min_height);
    }
    return nullptr;
}

void paint_image(cairo_t *cr, cairo_surface_t *image, const Rect &area, config::BackgroundMode mode) {
    int width = cairo_image_surface_get_width(image);
    int height = cairo_image_surface_get_height(image);
    cairo_save(cr);
    cairo_rectangle(cr, area.x, area.y, area.width, area.height);
    cairo_clip(cr);
    if (mode == config::BackgroundMode::Tile) {
        cairo_pattern_t *pattern = cairo_pattern_create_for_surface(image);
        cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
        cairo_matrix_t matrix;
        cairo_matrix_init_translate(&matrix, -area.x, -area.y);
        cairo_pattern_set_matrix(pattern, &matrix);
        cairo_set_source(cr, pattern);
        cairo_paint(cr);
        cairo_pattern_destroy(pattern);
    } else {
        auto placement = fit_image(width, height, area.width, area.height, mode);
        cairo_translate(cr, area.x + placement.x, area.y + placement.y);
        cairo_scale(cr, placement.scale_x, placement.scale_y);
        cairo_set_source_surface(cr, image, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
        cairo_paint(cr);
    }
    cairo_restore(cr);
}

SurfacePtr scaled_image(cairo_surface_t *image, int size) {
    SurfacePtr scaled(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size));
    cairo_t *cr = cairo_create(scaled.get());
    paint_image(cr, image, {0, 0, size, size}, config::BackgroundMode::Fill);
    cairo_destroy(cr);
    return scaled;
}

} // namespace render
