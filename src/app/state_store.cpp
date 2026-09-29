#include "app/state_store.h"

#include "core/file.h"

namespace app {

StateStore::StateStore(std::filesystem::path path) : path_(std::move(path)) {}

void StateStore::load() {
    data_ = core::load_ini(path_).value_or(core::IniFile{});
}

bool StateStore::save() const {
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);
    return core::write_file_atomic(path_, core::serialize_ini(data_), 0600);
}

std::optional<std::string> StateStore::last_user() const {
    return data_.get("last", "user");
}

void StateStore::set_last_user(std::string_view user) {
    data_.set("last", "user", user);
}

std::optional<std::string> StateStore::last_session(std::string_view user) const {
    return data_.get("sessions", user);
}

void StateStore::set_last_session(std::string_view user, std::string_view session) {
    data_.set("sessions", user, session);
}

} // namespace app
