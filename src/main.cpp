#include <clocale>
#include <csignal>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unistd.h>

#include "app/greeter.h"

#include "config/greet_config.h"

#include "core/event_loop.h"
#include "core/log.h"

#include "render/fonts.h"

namespace {

struct Options {
    std::optional<std::filesystem::path> preview;
    int width = 1280;
    int height = 800;
    bool window = false;
    std::optional<int> vt;
};

void usage() {
    std::puts("usage: astralia-greet [options]\n"
              "  --preview OUT.png  render the greeter to a PNG and exit\n"
              "  --size WxH         preview size (default 1280x800)\n"
              "  --window           run the greeter in a full-screen X11 test window\n"
              "  --vt N             run the greeter on VT N instead of the default\n"
              "  --version          print the version");
}

std::optional<Options> parse(int argc, char **argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        auto value = [&]() -> std::optional<std::string> {
            if (i + 1 >= argc) {
                return std::nullopt;
            }
            return std::string(argv[++i]);
        };
        if (arg == "--help" || arg == "-h") {
            usage();
            return std::nullopt;
        }
        if (arg == "--version") {
            std::puts(ASTRALIA_GREET_VERSION);
            return std::nullopt;
        }
        if (arg == "--window") {
            options.window = true;
            continue;
        }
        auto next = value();
        if (!next) {
            usage();
            return std::nullopt;
        }
        if (arg == "--preview") {
            options.preview = *next;
        } else if (arg == "--size") {
            if (std::sscanf(next->c_str(), "%dx%d", &options.width, &options.height) != 2 || options.width <= 0 || options.height <= 0) {
                usage();
                return std::nullopt;
            }
        } else if (arg == "--vt") {
            int vt = 0;
            if (std::sscanf(next->c_str(), "%d", &vt) != 1 || vt <= 0) {
                usage();
                return std::nullopt;
            }
            options.vt = vt;
        } else {
            usage();
            return std::nullopt;
        }
    }
    return options;
}

} // namespace

int main(int argc, char **argv) {
    auto options = parse(argc, argv);
    if (!options) {
        return argc > 1 && (std::string_view(argv[1]) == "--help" || std::string_view(argv[1]) == "-h" || std::string_view(argv[1]) == "--version") ? 0 : 2;
    }
    std::setlocale(LC_ALL, "");
    tzset();
    std::signal(SIGPIPE, SIG_IGN);
    render::register_fonts();

    config::Settings settings;
    if (options->vt) {
        settings.general.vt = *options->vt;
    }

    if (options->preview) {
        if (!app::render_preview(settings, *options->preview, options->width, options->height)) {
            core::error("cannot render preview to {}", options->preview->string());
            return 1;
        }
        return 0;
    }

    if (!options->window && geteuid() != 0) {
        core::error("astralia-greet must run as root (use --preview or --window to test it)");
        return 1;
    }
    core::EventLoop loop;
    if (!loop.valid()) {
        return 1;
    }
    app::Greeter greeter(loop, settings, options->window ? app::Greeter::Mode::Window : app::Greeter::Mode::Native);
    loop.add_signal(SIGTERM, [&loop] { loop.quit(); });
    loop.add_signal(SIGINT, [&loop] { loop.quit(); });
    if (!greeter.start()) {
        core::error("greeter failed to start");
        return 1;
    }
    loop.run();
    return 0;
}
