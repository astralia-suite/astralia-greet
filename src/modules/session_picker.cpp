#include "modules/session_picker.h"

#include <vector>

#include "config/session_picker_config.h"

#include "render/text.h"

namespace modules {

namespace {

std::string session_label(const service::Session &session) {
    if (!config::session_picker::show_type) {
        return session.name;
    }
    return session.name + (session.type == config::SessionType::Wayland ? " · Wayland" : " · X11");
}

} // namespace

SessionPicker::SessionPicker() {
    namespace defaults = config::session_picker;
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

void SessionPicker::layout(int width, int height) {
    rect_ = render::place(config::session_picker::geometry, width, height);
    screen_height_ = height;
}

render::Rect SessionPicker::bounds() const {
    return dropdown_.bounds(rect_, open_, session_count_, screen_height_);
}

bool SessionPicker::visible(const app::GreeterState &state) const {
    return state.view == app::View::Clock;
}

void SessionPicker::draw(cairo_t *cr, const app::GreeterState &state) {
    open_ = state.session_menu_open;
    session_count_ = state.sessions.size();
    if (!visible(state)) {
        return;
    }
    if (state.sessions.empty()) {
        render::draw_text(cr, config::session_picker::empty_text, dropdown_.font, config::error, rect_, render::TextAlign::Center);
        return;
    }
    auto color = state.focus == app::Focus::Session ? dropdown_.focus_color : dropdown_.color;
    dropdown_.draw_button(cr, rect_, session_label(state.sessions[state.session_index]), color);
    if (open_) {
        std::vector<std::string> labels;
        for (const auto &session : state.sessions) {
            labels.push_back(session_label(session));
        }
        dropdown_.draw_menu(cr, dropdown_.menu_rect(rect_, labels.size(), screen_height_), labels, state.session_index);
    }
}

bool SessionPicker::click(int x, int y, app::GreeterState &state) {
    if (!visible(state)) {
        return false;
    }
    state.focus = app::Focus::Session;
    if (rect_.contains(x, y)) {
        open_ = !state.session_menu_open && !state.sessions.empty();
        session_count_ = state.sessions.size();
        state.session_menu_open = open_;
        return true;
    }
    if (state.session_menu_open) {
        auto menu = dropdown_.menu_rect(rect_, state.sessions.size(), screen_height_);
        if (auto index = dropdown_.hit(menu, x, y, state.session_index, state.sessions.size())) {
            state.session_index = *index;
        }
        state.session_menu_open = false;
    }
    return true;
}

} // namespace modules
