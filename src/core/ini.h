#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace core {

using IniSection = std::map<std::string, std::string, std::less<>>;

class IniFile {
  public:
    void add_section(std::string_view section);
    void set(std::string_view section, std::string_view key, std::string_view value);
    std::optional<std::string> get(std::string_view section, std::string_view key) const;
    std::string get_or(std::string_view section, std::string_view key, std::string_view fallback) const;
    bool get_bool(std::string_view section, std::string_view key, bool fallback) const;
    int get_int(std::string_view section, std::string_view key, int fallback) const;
    double get_double(std::string_view section, std::string_view key, double fallback) const;
    std::optional<std::vector<std::string>> get_list(std::string_view section, std::string_view key) const;
    bool has(std::string_view section) const;
    const IniSection *section(std::string_view name) const;
    std::vector<std::string> subsections(std::string_view prefix) const;
    const std::vector<std::string> &section_names() const;

  private:
    std::vector<std::string> order_;
    std::map<std::string, IniSection, std::less<>> sections_;
};

IniFile parse_ini(std::string_view text);
std::optional<IniFile> load_ini(const std::filesystem::path &path);
std::string serialize_ini(const IniFile &ini);

} // namespace core
