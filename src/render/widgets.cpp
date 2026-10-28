#include "render/widgets.h"

#include <algorithm>

#include "config/style_config.h"

#include "render/icons.h"
#include "render/sticker.h"
#include "render/text.h"

namespace render::widgets {

namespace {

Rect snapped(const Box &box) {
    return box.bounding_rect();
}

int line_height(cairo_t *cr, const Style &style) {
    return style.text_height > 0 ? style.text_height : measure_text(cr, "Ag", style.text_font).height;
}

} // namespace

void centered_text(cairo_t *cr, std::string_view text, const std::string &font, const config::Color &color, const Box &box) {
    draw_text(cr, text, font, color, snapped(box), TextAlign::Center);
}

void toggle(cairo_t *cr, const Box &box, bool on, const Style &style) {
    selectable(cr, box, on, false, config::style::surface_alt, style);
    double inset = 4.0 * style.scale;
    double knob = box.h - inset * 2.0;
    double x = on ? box.x + box.w - inset - knob : box.x + inset;
    rounded_box(cr, {x, box.y + inset, knob, knob}, std::min(style.radius_widget, knob / 2.0), config::style::text);
}

void icon_button(cairo_t *cr, const Box &box, const char *glyph, const config::Color &color, const Style &style, bool hovered, bool selected) {
    selectable(cr, box, selected, hovered, config::style::accent_container, style);
    draw_text(cr, glyph, style.icon_font, color, snapped(box), TextAlign::Center);
}

void section_label(cairo_t *cr, std::string_view text, const Box &box, const Style &style) {
    int height = line_height(cr, style);
    draw_text(cr, text, style.text_font, config::style::text_dim, snapped({box.x, box.y + box.h - height, box.w, static_cast<double>(height)}), TextAlign::Left);
}

void device_row(cairo_t *cr, const Box &box, const DeviceRow &row, PanelPaint &paint, int action_id, int index, const Style &style) {
    auto face = mix(config::style::paper, row.background, row.background.a);
    face.a = 1.0;
    sticker(cr, box, style.radius_widget, face, config::style::ink, style.border_thin, style.shadow_widget, 0.0);
    double body = box.h - style.shadow_widget;
    double middle = box.y + body / 2.0;
    draw_text(cr, row.glyph, style.icon_font, row.glyph_color, snapped({box.x + style.row_icon_gap, box.y, style.icon_size, body}), TextAlign::Center);

    double right = box.x + box.w - style.row_icon_gap;
    double actions = 0.0;
    if (row.busy) {
        int width = measure_text(cr, row.busy_text, style.text_font).width;
        actions = width;
        draw_text(cr, row.busy_text, style.text_font, config::style::lavender, snapped({right - width, box.y, static_cast<double>(width), body}), TextAlign::Right);
    } else if (row.action_glyph) {
        Box action{right - style.close_button, box.y + (body - style.close_button) / 2.0, style.close_button, style.close_button};
        icon_button(cr, action, row.action_glyph, row.action_color, style);
        paint.region(action, action_id, index);
        actions = style.close_button;
    }

    double text_x = box.x + style.row_text_left;
    double text_w = std::max(0.0, right - actions - style.action_gap - text_x);
    int title_h = line_height(cr, style);
    if (row.subtitle.empty()) {
        draw_text(cr, row.title, style.text_font, row.title_color, snapped({text_x, middle - title_h / 2.0, text_w, static_cast<double>(title_h)}), TextAlign::Left);
        return;
    }
    draw_text(cr, row.title, style.text_font, row.title_color, snapped({text_x, middle - title_h - 1.0, text_w, static_cast<double>(title_h)}), TextAlign::Left);
    draw_text(cr, row.subtitle, style.text_font, config::style::text_dim, snapped({text_x, middle + 1.0, text_w, static_cast<double>(title_h)}), TextAlign::Left);
}

double list_height(const std::vector<ListRow> &rows, const Style &style) {
    double height = 0.0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        double row = style.device_row_height;
        switch (rows[i].kind) {
        case ListRow::Kind::Banner:
        case ListRow::Kind::Error:
            row = style.banner_height;
            break;
        case ListRow::Kind::Message:
            row = style.empty_height;
            break;
        case ListRow::Kind::Section:
            row = style.section_height;
            break;
        case ListRow::Kind::Device:
            break;
        }
        height += (i > 0 ? style.list_spacing : 0.0) + row;
    }
    return height;
}

void paint_list(cairo_t *cr, const Box &view, const std::vector<ListRow> &rows, const Style &style, PanelPaint &paint, int action_id) {
    double y = 0.0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto &row = rows[i];
        if (i > 0) {
            y += style.list_spacing;
        }
        double height = style.device_row_height;
        if (row.kind == ListRow::Kind::Banner || row.kind == ListRow::Kind::Error) {
            height = style.banner_height;
        } else if (row.kind == ListRow::Kind::Message) {
            height = style.empty_height;
        } else if (row.kind == ListRow::Kind::Section) {
            height = style.section_height;
        }
        if (y < view.y || y >= view.y + view.h) {
            y += height;
            continue;
        }
        switch (row.kind) {
        case ListRow::Kind::Banner:
            rounded_box(cr, {view.x, y, view.w, height}, style.radius_widget, config::style::accent_alpha25);
            centered_text(cr, row.text, style.text_font, config::style::text, {view.x, y, view.w, height});
            break;
        case ListRow::Kind::Error:
            rounded_box(cr, {view.x, y, view.w, height}, style.radius_widget, config::style::critical_alpha15);
            centered_text(cr, row.text, style.text_font, config::style::critical, {view.x, y, view.w, height});
            break;
        case ListRow::Kind::Message:
            centered_text(cr, row.text, style.text_font, config::style::text_dim, {view.x, y, view.w, height});
            break;
        case ListRow::Kind::Section:
            section_label(cr, row.text, {view.x, y, view.w, height}, style);
            break;
        case ListRow::Kind::Device:
            device_row(cr, {view.x, y, view.w, height}, row.device, paint, action_id, row.index, style);
            break;
        }
        y += height;
    }
}

double password_dialog_height(const Style &style) {
    return style.padding + style.dialog_spacer + style.field_height + style.row_gap + style.dialog_button + style.padding;
}

void password_dialog(cairo_t *cr, const Box &box, const PasswordDialog &dialog, const Style &style, PanelPaint &paint, int close_id, int connect_id, int cancel_id) {
    namespace colors = config::style;
    sticker(cr, box, style.radius_card, colors::paper, colors::ink, style.border_thick, style.shadow_offset, style.ring_width);
    Box close{box.x + box.w - style.dialog_top_margin - style.dialog_button, box.y + style.dialog_top_margin, style.dialog_button, style.dialog_button};
    icon_button(cr, close, icon::close, colors::text, style);
    paint.region(close, close_id);
    draw_text(cr, dialog.ssid, style.text_font, colors::text_dim, snapped({box.x + style.padding, close.y, close.x - box.x - style.padding - style.row_gap, style.dialog_button}), TextAlign::Left);

    double x = box.x + style.padding;
    double width = box.w - 2.0 * style.padding;
    Box field{x, box.y + style.padding + style.dialog_spacer, width, style.field_height};
    rounded_box(cr, field, style.radius_widget, colors::text_alpha08, std::max(1.0, style.scale), colors::text_alpha15);
    if (dialog.length == 0) {
        centered_text(cr, dialog.placeholder, style.text_font, colors::text_dim, field);
    } else {
        double step = style.dot_size + style.dot_gap;
        auto capacity = static_cast<std::size_t>(std::max(1.0, (field.w - 2.0 * style.row_gap) / step));
        auto shown = std::min(dialog.length, capacity);
        double row = static_cast<double>(shown) * step - style.dot_gap;
        double left = field.x + (field.w - row) / 2.0;
        for (std::size_t i = 0; i < shown; ++i) {
            rounded_box(cr, {left + static_cast<double>(i) * step, field.y + (field.h - style.dot_size) / 2.0, style.dot_size, style.dot_size}, style.dot_size / 2.0, colors::text);
        }
    }

    double y = field.y + field.h + style.row_gap;
    double connect_w = measure_text(cr, dialog.connect_text, style.text_font).width + style.dialog_button_pad;
    double cancel_w = measure_text(cr, dialog.cancel_text, style.text_font).width + style.dialog_button_pad;
    double left = x + (width - (connect_w + style.row_gap + cancel_w)) / 2.0;
    Box connect{left, y, connect_w, style.dialog_button};
    selectable(cr, connect, false, false, dialog.can_connect ? colors::pop_b : colors::surface_alt, style);
    centered_text(cr, dialog.connect_text, style.text_font, dialog.can_connect ? colors::text : colors::text_muted, connect);
    if (dialog.can_connect) {
        paint.region(connect, connect_id);
    }
    Box cancel{left + connect_w + style.row_gap, y, cancel_w, style.dialog_button};
    selectable(cr, cancel, false, false, colors::accent_container, style);
    centered_text(cr, dialog.cancel_text, style.text_font, colors::text, cancel);
    paint.region(cancel, cancel_id);
}

} // namespace render::widgets
