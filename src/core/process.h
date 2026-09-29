#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <vector>

namespace core {

class Environment {
  public:
    void set(std::string_view key, std::string_view value);
    void set_entry(std::string_view entry);
    std::optional<std::string> get(std::string_view key) const;
    std::vector<std::string> entries() const;
    const std::map<std::string, std::string, std::less<>> &vars() const;

  private:
    std::map<std::string, std::string, std::less<>> vars_;
};

void reset_signal_state();
[[noreturn]] void exec_command(const std::vector<std::string> &argv, const Environment &env);
pid_t spawn(const std::vector<std::string> &argv, const Environment &env);
int wait_for(pid_t pid);
std::optional<std::filesystem::path> find_executable(std::string_view name, std::string_view path_list);

} // namespace core
