#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "core/ini.h"

namespace app {

class StateStore {
  public:
    explicit StateStore(std::filesystem::path path);

    void load();
    bool save() const;
    std::optional<std::string> last_user() const;
    void set_last_user(std::string_view user);
    std::optional<std::string> last_session(std::string_view user) const;
    void set_last_session(std::string_view user, std::string_view session);

  private:
    std::filesystem::path path_;
    core::IniFile data_;
};

} // namespace app
