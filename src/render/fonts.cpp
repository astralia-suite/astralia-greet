#include "render/fonts.h"

#include <filesystem>
#include <fontconfig/fontconfig.h>

#include "config/greet_config.h"

#include "core/log.h"

namespace render {

namespace {

bool add_font(const char *name) {
    for (const char *dir : {config::font_dir, config::source_font_dir}) {
        auto path = std::filesystem::path(dir) / name;
        std::error_code ec;
        if (std::filesystem::is_regular_file(path, ec) && FcConfigAppFontAddFile(nullptr, reinterpret_cast<const FcChar8 *>(path.c_str()))) {
            return true;
        }
    }
    core::warn("cannot load the font {}", name);
    return false;
}

} // namespace

bool register_fonts() {
    bool all = true;
    for (const char *name : config::font_files) {
        all = add_font(name) && all;
    }
    return all;
}

} // namespace render
