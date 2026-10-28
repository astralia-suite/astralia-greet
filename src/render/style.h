#pragma once

#include <string>

namespace render {

struct Style {
    double scale = 1.0;

    double radius_card = 0.0;
    double radius_widget = 0.0;
    double border_thin = 0.0;
    double border_thick = 0.0;
    double shadow_offset = 0.0;
    double shadow_widget = 0.0;
    double ring_width = 0.0;

    double roller = 0.0;
    double overhang = 0.0;
    double cap = 0.0;
    double band = 0.0;
    double cap_flare = 0.0;

    double width = 0.0;
    double max_height = 0.0;
    double padding = 0.0;
    double header_height = 0.0;
    double header_divider_gap = 0.0;
    double header_tag_pad = 0.0;
    double content_gap = 0.0;
    double close_button = 0.0;
    double gap_below_icon = 0.0;
    double screen_margin = 0.0;
    double toggle_width = 0.0;
    double toggle_height = 0.0;

    double row_gap = 0.0;
    double list_spacing = 0.0;
    double row_icon_gap = 0.0;
    double row_text_left = 0.0;
    double device_row_height = 0.0;
    double section_height = 0.0;
    double banner_height = 0.0;
    double empty_height = 0.0;
    double action_gap = 0.0;
    double icon_size = 0.0;
    double dialog_spacer = 0.0;
    double dialog_top_margin = 0.0;
    double dialog_button = 0.0;
    double dialog_button_pad = 0.0;
    double field_height = 0.0;
    double dot_size = 0.0;
    double dot_gap = 0.0;

    double wheel_pixels = 0.0;
    int text_height = 0;

    std::string text_font;
    std::string title_font;
    std::string icon_font;
};

Style make_style(double scale);

} // namespace render
