#include "render/style.h"

#include <cmath>

#include "config/scroll_config.h"
#include "config/style_config.h"

#include "render/layout.h"
#include "render/text.h"

namespace render {

Style make_style(double scale) {
    namespace defaults = config::scroll;
    namespace look = config::style;
    Style style;
    style.scale = scale;

    style.radius_card = scaled(look::radius_card, scale);
    style.radius_widget = scaled(look::radius_widget, scale);
    style.border_thin = line_width(look::border_thin, scale);
    style.border_thick = line_width(look::border_thick, scale);
    style.shadow_offset = scaled(look::shadow_offset, scale);
    style.shadow_widget = scaled(look::shadow_widget, scale);
    style.ring_width = line_width(look::ring_width, scale);

    style.roller = scaled(defaults::roller, scale);
    style.overhang = scaled(defaults::overhang, scale);
    style.cap = scaled(defaults::cap, scale);
    style.band = scaled(defaults::band, scale);
    style.cap_flare = scaled(defaults::cap_flare, scale);

    style.width = scaled(defaults::width, scale);
    style.max_height = scaled(defaults::max_height, scale);
    style.padding = scaled(defaults::padding, scale);
    style.header_height = scaled(defaults::header_height, scale);
    style.header_divider_gap = scaled(defaults::header_divider_gap, scale);
    style.header_tag_pad = scaled(defaults::header_tag_pad, scale);
    style.content_gap = scaled(defaults::content_gap, scale);
    style.close_button = scaled(defaults::close_button, scale);
    style.gap_below_icon = scaled(defaults::gap_below_icon, scale);
    style.screen_margin = scaled(defaults::screen_margin, scale);
    style.toggle_width = scaled(defaults::toggle_width, scale);
    style.toggle_height = scaled(defaults::toggle_height, scale);

    style.row_gap = scaled(defaults::row_gap, scale);
    style.list_spacing = scaled(defaults::list_spacing, scale);
    style.row_icon_gap = scaled(defaults::row_icon_gap, scale);
    style.row_text_left = scaled(defaults::row_text_left, scale);
    style.device_row_height = scaled(defaults::device_row_height, scale);
    style.section_height = scaled(defaults::section_height, scale);
    style.banner_height = scaled(defaults::banner_height, scale);
    style.empty_height = scaled(defaults::empty_height, scale);
    style.action_gap = scaled(defaults::action_gap, scale);
    style.icon_size = scaled(defaults::icon_size, scale);
    style.dialog_spacer = scaled(defaults::dialog_spacer, scale);
    style.dialog_top_margin = scaled(defaults::dialog_top_margin, scale);
    style.dialog_button = scaled(defaults::dialog_button, scale);
    style.dialog_button_pad = scaled(defaults::dialog_button_pad, scale);
    style.field_height = scaled(defaults::field_height, scale);
    style.dot_size = scaled(defaults::dot_size, scale);
    style.dot_gap = scaled(defaults::dot_gap, scale);

    style.wheel_pixels = scaled(defaults::wheel_pixels, scale);

    style.text_font = text_font(defaults::text_size);
    style.title_font = text_font(defaults::title_size, true);
    style.icon_font = icon_font(static_cast<int>(std::lround(style.icon_size)));
    return style;
}

} // namespace render
