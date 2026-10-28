#pragma once

#include <cairo.h>
#include <string>
#include <string_view>
#include <vector>

#include "config/theme_config.h"

#include "render/layout.h"
#include "render/panel_content.h"
#include "render/style.h"

namespace render::widgets {

struct DeviceRow {
    const char *glyph = nullptr;
    const char *action_glyph = nullptr;
    std::string title;
    std::string subtitle;
    std::string busy_text;
    config::Color background;
    config::Color glyph_color;
    config::Color title_color;
    config::Color action_color;
    bool busy = false;
};

struct ListRow {
    enum class Kind { Banner,
                      Error,
                      Message,
                      Section,
                      Device };

    Kind kind = Kind::Message;
    std::string text;
    DeviceRow device;
    int index = 0;
};

struct PasswordDialog {
    std::string ssid;
    std::string placeholder;
    std::string connect_text;
    std::string cancel_text;
    std::size_t length = 0;
    bool can_connect = false;
};

double password_dialog_height(const Style &style);
void password_dialog(cairo_t *cr, const Box &box, const PasswordDialog &dialog, const Style &style, PanelPaint &paint, int close_id, int connect_id, int cancel_id);
double list_height(const std::vector<ListRow> &rows, const Style &style);
void paint_list(cairo_t *cr, const Box &view, const std::vector<ListRow> &rows, const Style &style, PanelPaint &paint, int action_id);
void centered_text(cairo_t *cr, std::string_view text, const std::string &font, const config::Color &color, const Box &box);
void toggle(cairo_t *cr, const Box &box, bool on, const Style &style);
void icon_button(cairo_t *cr, const Box &box, const char *glyph, const config::Color &color, const Style &style, bool hovered = false, bool selected = false);
void section_label(cairo_t *cr, std::string_view text, const Box &box, const Style &style);
void device_row(cairo_t *cr, const Box &box, const DeviceRow &row, PanelPaint &paint, int action_id, int index, const Style &style);

} // namespace render::widgets
