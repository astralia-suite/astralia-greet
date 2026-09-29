#include "modules/user_menu.h"

#include <string>
#include <vector>

#include "config/user_menu_config.h"

#include "render/text.h"

namespace modules {

UserMenu::UserMenu() {
    namespace defaults = config::user_menu;
    dropdown_.font = render::text_font(defaults::font_size);
    dropdown_.indicator_font = render::icon_font(defaults::indicator_size);
    dropdown_.indicator_size = defaults::indicator_size;
    dropdown_.indicator_gap = defaults::indicator_gap;
    dropdown_.max_items = defaults::max_items;
    dropdown_.item_height = defaults::item_height;
    dropdown_.menu_gap = defaults::menu_gap;
    dropdown_.corner_radius = defaults::corner_radius;
    dropdown_.border_width = defaults::border_width;
    dropdown_.color = config::muted;
    dropdown_.focus_color = config::accent;
    dropdown_.menu_color = config::field;
    dropdown_.menu_border = config::field_border;
    dropdown_.pill_color = defaults::pill_color;
}

void UserMenu::layout(int width, int height) {
    rect_ = render::place(config::user_menu::geometry, width, height);
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
