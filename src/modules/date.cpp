#include "modules/date.h"

#include <array>
#include <ctime>

#include "config/date_config.h"

namespace modules {

Date::Date()
    : geometry_(config::date::geometry), format_(config::date::format), font_(render::text_font(config::date::font_size)), color_(config::fg), align_(render::TextAlign::Center) {}

void Date::layout(int width, int height) {
    rect_ = render::place(geometry_, width, height);
}

render::Rect Date::bounds() const {
    return rect_;
}

bool Date::visible(const app::GreeterState &state) const {
    return state.view == app::View::Clock;
}

void Date::draw(cairo_t *cr, const app::GreeterState &state) {
    if (format_.empty() || !visible(state)) {
        return;
    }
    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_r(&now, &local);
    std::array<char, 256> buffer{};
    auto length = std::strftime(buffer.data(), buffer.size(), format_.c_str(), &local);
    render::draw_text(cr, {buffer.data(), length}, font_, color_, rect_, align_);
}

std::optional<std::chrono::milliseconds> Date::next_tick() const {
    using namespace std::chrono;
    auto now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    return milliseconds(60000 - now % 60000 + 20);
}

} // namespace modules
