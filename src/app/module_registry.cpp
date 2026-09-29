#include "app/module_registry.h"

#include "modules/background.h"
#include "modules/clock.h"
#include "modules/clock_panel.h"
#include "modules/date.h"
#include "modules/hint.h"
#include "modules/password.h"
#include "modules/power.h"
#include "modules/session_picker.h"
#include "modules/user_menu.h"

namespace app {

std::vector<std::unique_ptr<Module>> create_modules(const config::PowerSettings &power) {
    std::vector<std::unique_ptr<Module>> list;
    list.push_back(std::make_unique<modules::Background>());
    list.push_back(std::make_unique<modules::ClockPanel>());
    list.push_back(std::make_unique<modules::Date>());
    list.push_back(std::make_unique<modules::Clock>());
    list.push_back(std::make_unique<modules::Password>());
    list.push_back(std::make_unique<modules::Hint>());
    if (power.allow_suspend || power.allow_reboot || power.allow_poweroff) {
        list.push_back(std::make_unique<modules::Power>(power));
    }
    list.push_back(std::make_unique<modules::UserMenu>());
    list.push_back(std::make_unique<modules::SessionPicker>());
    return list;
}

} // namespace app
