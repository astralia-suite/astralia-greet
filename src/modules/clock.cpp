#include "modules/clock.h"

#include <array>
#include <ctime>

#include "config/clock_config.h"

namespace modules {

namespace {

std::string format_time(const std::string &format, const std::tm &now) {
    if (format.empty()) {
        return {};
    }
    std::array<char, 256> buffer{};
    auto length = std::strftime(buffer.data(), buffer.size(), format.c_str(), &now);
    return {buffer.data(), length};
}

bool shows_seconds(const std::string &format) {
    for (auto token : {"%S", "%T", "%r", "%s", "%X", "%c"}) {
        if (format.find(token) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

Clock::Clock()
    : geometry_(config::clock::geometry), hour_format_(config::clock::hour_format), minute_format_(config::clock::minute_format), font_(render::text_font(config::clock::time_size)), hour_color_(config::accent), minute_color_(config::fg), align_(render::TextAlign::Center) {
    seconds_ = shows_seconds(hour_format_) || shows_seconds(minute_format_);
}

void Clock::layout(int width, int height) {
    rect_ = render::place(geometry_, width, height);
}

render::Rect Clock::bounds() const {
    return rect_;
}

bool Clock::visible(const app::GreeterState &state) const {
    return state.view == app::View::Clock;
}

void Clock::draw(cairo_t *cr, const app::GreeterState &state) {
    if (!visible(state)) {
        return;
    }
    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_r(&now, &local);
    int line = rect_.height / 2;
    render::draw_text(cr, format_time(hour_format_, local), font_, hour_color_, {rect_.x, rect_.y, rect_.width, line}, align_);
    render::draw_text(cr, format_time(minute_format_, local), font_, minute_color_, {rect_.x, rect_.y + line, rect_.width, rect_.height - line}, align_);
}

bool Clock::click(int, int, app::GreeterState &state) {
    if (!visible(state)) {
        return false;
    }
    state.view = app::View::Login;
    state.focus = app::Focus::Password;
    state.message.clear();
    state.session_menu_open = false;
    state.user_menu_open = false;
    return true;
}

std::optional<std::chrono::milliseconds> Clock::next_tick() const {
    using namespace std::chrono;
    auto now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    auto period = seconds_ ? 1000 : 60000;
    return milliseconds(period - now % period + 20);
}

} // namespace modules
