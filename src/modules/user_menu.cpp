#include "modules/user_menu.h"

#include <algorithm>
#include <string>
#include <vector>

#include "config/user_menu_config.h"

#include "render/icons.h"
#include "render/text.h"

namespace modules {

UserMenu::UserMenu() {
    namespace defaults = config::user_menu;
    dropdown_.font = render::text_font(defaults::font_size);
    dropdown_.max_items = defaults::max_items;
    dropdown_.color = config::muted;
    dropdown_.focus_color = config::accent;
    dropdown_.pill_color = defaults::pill_color;
    dropdown_.menu_border = config::field_border;
    dropdown_.icon = render::icon::user;
}

void UserMenu::layout(int width, int height) {
    namespace defaults = config::user_menu;
    auto scale = render::ui_scale(width, height);
    rect_ = render::place(defaults::geometry, width, height, scale);
    dropdown_.indicator_size = render::scaled(defaults::indicator_size, scale);
    dropdown_.indicator_font = render::icon_font(dropdown_.indicator_size);
    dropdown_.indicator_gap = render::scaled(defaults::indicator_gap, scale);
    dropdown_.item_height = std::max(1, render::scaled(defaults::item_height, scale));
    dropdown_.menu_gap = render::scaled(defaults::menu_gap, scale);
    dropdown_.corner_radius = render::scaled(defaults::corner_radius, scale);
    dropdown_.border_width = render::line_width(defaults::border_width, scale);
    screen_height_ = height;
}

render::Rect UserMenu::bounds() const {
    return dropdown_.bounds(rect_, open_, user_count_, screen_height_);
}

bool UserMenu::visible(const app::GreeterState &state) const {
    return state.view == app::View::Clock && !state.users.empty();
}

void UserMenu::draw(cairo_t *cr, const app::GreeterState &state) {
    open_ = state.user_menu_open;
    user_count_ = state.users.size();
    if (!visible(state)) {
        return;
    }
    dropdown_.draw_button(cr, rect_, state.users[state.user_index].display_name, dropdown_.color);
    if (open_) {
        std::vector<std::string> labels;
        for (const auto &user : state.users) {
            labels.push_back(user.display_name);
        }
        dropdown_.draw_menu(cr, dropdown_.menu_rect(rect_, labels.size(), screen_height_), labels, state.user_index);
    }
}

bool UserMenu::click(int x, int y, app::GreeterState &state) {
    if (!visible(state)) {
        return false;
    }
    if (rect_.contains(x, y)) {
        open_ = !state.user_menu_open;
        user_count_ = state.users.size();
        state.user_menu_open = open_;
        return true;
    }
    if (state.user_menu_open) {
        auto menu = dropdown_.menu_rect(rect_, state.users.size(), screen_height_);
        if (auto index = dropdown_.hit(menu, x, y, state.user_index, state.users.size())) {
            state.user_index = *index;
            state.focus = app::Focus::Password;
        }
        state.user_menu_open = false;
    }
    return true;
}

} // namespace modules
