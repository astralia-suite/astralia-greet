#pragma once

#include <memory>
#include <vector>

#include "app/module.h"

#include "config/greet_config.h"

namespace app {

std::vector<std::unique_ptr<Module>> create_modules(const config::PowerSettings &power);

} // namespace app
