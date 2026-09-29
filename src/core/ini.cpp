#include "core/ini.h"

#include "core/file.h"
#include "core/strings.h"

namespace core {

void IniFile::add_section(std::string_view section) {
    if (sections_.find(section) == sections_.end()) {
        order_.emplace_back(section);
        sections_.emplace(std::string(section), IniSection{});
    }
}

void IniFile::set(std::string_view section, std::string_view key, std::string_view value) {
    add_section(section);
    sections_.find(section)->second.insert_or_assign(std::string(key), std::string(value));
}

std::optional<std::string> IniFile::get(std::string_view section, std::string_view key) const {
    auto found = sections_.find(section);
    if (found == sections_.end()) {
        return std::nullopt;
    }
    auto entry = found->second.find(key);
    if (entry == found->second.end()) {
        return std::nullopt;
    }
    return entry->second;
}

std::string IniFile::get_or(std::string_view section, std::string_view key, std::string_view fallback) const {
    return get(section, key).value_or(std::string(fallback));
}

bool IniFile::get_bool(std::string_view section, std::string_view key, bool fallback) const {
    auto value = get(section, key);
    return value ? parse_bool(*value).value_or(fallback) : fallback;
}

int IniFile::get_int(std::string_view section, std::string_view key, int fallback) const {
    auto value = get(section, key);
    return value ? parse_int(*value).value_or(fallback) : fallback;
}

double IniFile::get_double(std::string_view section, std::string_view key, double fallback) const {
    auto value = get(section, key);
    return value ? parse_double(*value).value_or(fallback) : fallback;
}

std::optional<std::vector<std::string>> IniFile::get_list(std::string_view section, std::string_view key) const {
    auto value = get(section, key);
    if (!value) {
        return std::nullopt;
    }
    return split(*value, ",;");
}

bool IniFile::has(std::string_view section) const {
    return sections_.contains(section);
}

const IniSection *IniFile::section(std::string_view name) const {
    auto found = sections_.find(name);
    return found == sections_.end() ? nullptr : &found->second;
}

std::vector<std::string> IniFile::subsections(std::string_view prefix) const {
    std::vector<std::string> names;
    for (const auto &name : order_) {
        if (name.size() > prefix.size() && name.starts_with(prefix)) {
            names.push_back(name.substr(prefix.size()));
        }
    }
    return names;
}

const std::vector<std::string> &IniFile::section_names() const {
    return order_;
}

IniFile parse_ini(std::string_view text) {
    IniFile ini;
    std::string current;
    std::size_t start = 0;
    while (start < text.size()) {
        auto end = text.find('\n', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        auto line = trim(text.substr(start, end - start));
        start = end + 1;
        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }
        if (line.front() == '[') {
            auto close = line.find(']');
            if (close == std::string_view::npos) {
                continue;
            }
            current = std::string(trim(line.substr(1, close - 1)));
            ini.add_section(current);
            continue;
        }
        auto equals = line.find('=');
        if (equals == std::string_view::npos) {
            continue;
        }
        auto key = trim(line.substr(0, equals));
        auto value = trim(line.substr(equals + 1));
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }
        if (!key.empty()) {
            ini.set(current, key, value);
        }
    }
    return ini;
}

std::optional<IniFile> load_ini(const std::filesystem::path &path) {
    auto text = read_file(path);
    if (!text) {
        return std::nullopt;
    }
    return parse_ini(*text);
}

std::string serialize_ini(const IniFile &ini) {
    std::string out;
    for (const auto &name : ini.section_names()) {
        if (!name.empty()) {
            out += "[" + name + "]\n";
        }
        for (const auto &[key, value] : *ini.section(name)) {
            out += key + "=" + value + "\n";
        }
        out += "\n";
    }
    return out;
}

} // namespace core
