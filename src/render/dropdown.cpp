#include "render/dropdown.h"

#include <algorithm>
#include <cmath>

#include "render/canvas.h"
#include "render/icons.h"
#include "render/sticker.h"
#include "render/text.h"

namespace render {

std::size_t Dropdown::visible_count(std::size_t total) const {
    return std::min(total, static_cast<std::size_t>(std::max(1, max_items)));
}

std::size_t Dropdown::first_visible(std::size_t index, std::size_t total) const {
    auto visible = visible_count(total);
    return index >= visible ? index - visible + 1 : 0;
}

int Dropdown::padding() const {
    return static_cast<int>(std::lround(style.border_thick + style.ring_width));
}

Rect Dropdown::menu_rect(const Rect &button, std::size_t total, int screen_height) const {
    int height = static_cast<int>(visible_count(total)) * item_height + padding() * 2 + static_cast<int>(std::lround(style.shadow_offset));
    int y = button.bottom() + menu_gap;
    if (y + height > screen_height) {
        y = button.y - menu_gap - height;
    }
    return {button.x, y, button.width, height};
}

Rect Dropdown::bounds(const Rect &button, bool open, std::size_t total, int screen_height) const {
    if (!open || total == 0) {
        return button;
    }
    auto menu = menu_rect(button, total, screen_height);
    int top = std::min(button.y, menu.y);
    int bottom = std::max(button.bottom(), menu.bottom());
    return {button.x, top, button.width, bottom - top};
}

void Dropdown::draw_button(cairo_t *cr, const Rect &button, std::string_view label, bool focused, const config::Color &text_color) const {
    selectable(cr, to_box(button), focused, false, config::style::accent_container, style);
    int body = button.height - static_cast<int>(std::lround(style.shadow_widget));
    int padding = body / 2;
    int lead = icon ? indicator_size + indicator_gap : 0;
    int limit = std::max(0, button.width - padding * 2 - lead - indicator_gap - indicator_size);
    int width = std::min(measure_text(cr, label, font).width, limit);
    int content = lead + width + indicator_gap + indicator_size;
    int x = static_cast<int>(button.center_x()) - content / 2;
    if (icon) {
        draw_text(cr, icon, indicator_font, text_color, {x, button.y, indicator_size, body}, TextAlign::Center);
        x += lead;
    }
    draw_text(cr, label, font, text_color, {x, button.y, width, body}, TextAlign::Left);
    draw_text(cr, icon::chevron_up, indicator_font, text_color, {x + width + indicator_gap, button.y, indicator_size, body}, TextAlign::Center);
}

void Dropdown::draw_menu(cairo_t *cr, const Rect &menu, const std::vector<std::string> &labels, std::size_t index) const {
    sticker(cr, to_box(menu), style.radius_card, config::style::surface, config::style::ink, style.border_thick, style.shadow_offset, style.ring_width);
    int inset = padding();
    auto first = first_visible(index, labels.size());
    auto visible = visible_count(labels.size());
    for (std::size_t row = 0; row < visible; ++row) {
        auto item = first + row;
        Rect box{menu.x + inset, menu.y + inset + static_cast<int>(row) * item_height, menu.width - inset * 2, item_height - row_gap};
        selectable(cr, to_box(box), item == index, false, config::style::paper, style);
        draw_text(cr, labels[item], font, config::style::text, {box.x, box.y, box.width, box.height - static_cast<int>(std::lround(style.shadow_widget))}, TextAlign::Center);
    }
}

std::optional<std::size_t> Dropdown::hit(const Rect &menu, int x, int y, std::size_t index, std::size_t total) const {
    int rows_top = menu.y + padding();
    if (!menu.contains(x, y) || y < rows_top) {
        return std::nullopt;
    }
    auto item = first_visible(index, total) + static_cast<std::size_t>((y - rows_top) / item_height);
    if (item >= total || item >= first_visible(index, total) + visible_count(total)) {
        return std::nullopt;
    }
    return item;
}

} // namespace render
