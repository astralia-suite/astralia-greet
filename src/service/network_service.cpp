#include "service/network_service.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <set>

#include "core/log.h"

namespace service {

namespace {

// NetworkManager D-Bus names
constexpr const char *nm_name = "org.freedesktop.NetworkManager";
constexpr const char *nm_path = "/org/freedesktop/NetworkManager";
constexpr const char *nm_interface = "org.freedesktop.NetworkManager";
constexpr const char *settings_path = "/org/freedesktop/NetworkManager/Settings";
constexpr const char *settings_interface = "org.freedesktop.NetworkManager.Settings";
constexpr const char *profile_interface = "org.freedesktop.NetworkManager.Settings.Connection";
constexpr const char *device_interface = "org.freedesktop.NetworkManager.Device";
constexpr const char *active_interface = "org.freedesktop.NetworkManager.Connection.Active";
constexpr const char *wireless_interface = "org.freedesktop.NetworkManager.Device.Wireless";
constexpr const char *access_point_interface = "org.freedesktop.NetworkManager.AccessPoint";
constexpr const char *properties_interface = "org.freedesktop.DBus.Properties";

// NetworkManager enum values
constexpr std::uint32_t state_connecting = 40;
constexpr std::uint32_t state_connected_local = 50;
constexpr std::uint32_t device_type_wifi = 2;
constexpr std::uint32_t active_activating = 1;
constexpr std::uint32_t active_activated = 2;
constexpr std::uint32_t ap_flag_privacy = 0x1;
constexpr std::uint32_t key_mgmt_mask = 0xFF00;
constexpr std::uint32_t key_mgmt_psk = 0x100;
constexpr std::uint32_t key_mgmt_802_1x = 0x200;
constexpr std::uint32_t key_mgmt_sae = 0x400;

constexpr const char *wifi_profile_type = "802-11-wireless";
constexpr std::chrono::milliseconds scan_refresh_delay{500};
constexpr std::chrono::milliseconds open_refresh_delay{20};

sd_bus_message *fetch(sd_bus *bus, const char *path, const char *interface, const char *name, const char *type) {
    sd_bus_error failure = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = nullptr;
    int result = sd_bus_get_property(bus, nm_name, path, interface, name, &failure, &reply, type);
    sd_bus_error_free(&failure);
    return result >= 0 ? reply : nullptr;
}

bool get_bool(sd_bus *bus, const char *path, const char *interface, const char *name, bool &out) {
    auto *reply = fetch(bus, path, interface, name, "b");
    if (!reply) {
        return false;
    }
    int value = 0;
    bool ok = sd_bus_message_read_basic(reply, 'b', &value) >= 0;
    sd_bus_message_unref(reply);
    out = value != 0;
    return ok;
}

bool get_number(sd_bus *bus, const char *path, const char *interface, const char *name, std::uint32_t &out) {
    auto *reply = fetch(bus, path, interface, name, "u");
    if (!reply) {
        return false;
    }
    bool ok = sd_bus_message_read_basic(reply, 'u', &out) >= 0;
    sd_bus_message_unref(reply);
    return ok;
}

bool get_text(sd_bus *bus, const char *path, const char *interface, const char *name, char type, std::string &out) {
    const char signature[2]{type, '\0'};
    auto *reply = fetch(bus, path, interface, name, signature);
    if (!reply) {
        return false;
    }
    const char *text = nullptr;
    bool ok = sd_bus_message_read_basic(reply, type, &text) >= 0 && text;
    sd_bus_message_unref(reply);
    out = ok ? text : "";
    return ok;
}

std::vector<std::string> take_paths(char **list) {
    std::vector<std::string> paths;
    if (!list) {
        return paths;
    }
    for (char **item = list; *item; ++item) {
        paths.emplace_back(*item);
        std::free(*item);
    }
    std::free(list);
    return paths;
}

bool get_paths(sd_bus *bus, const char *path, const char *interface, const char *name, std::vector<std::string> &out) {
    auto *reply = fetch(bus, path, interface, name, "ao");
    if (!reply) {
        return false;
    }
    char **list = nullptr;
    bool ok = sd_bus_message_read_strv(reply, &list) >= 0;
    sd_bus_message_unref(reply);
    out = take_paths(list);
    return ok;
}

bool read_profile(sd_bus_message *settings, std::string &id, std::string &type, std::string &ssid) {
    if (sd_bus_message_enter_container(settings, 'a', "{sa{sv}}") < 0) {
        return false;
    }
    while (sd_bus_message_enter_container(settings, 'e', "sa{sv}") > 0) {
        const char *section = nullptr;
        if (sd_bus_message_read(settings, "s", &section) >= 0 && section) {
            auto properties = core::Bus::read_properties(settings);
            if (std::string_view(section) == "connection") {
                id = properties["id"].text;
                type = properties["type"].text;
            } else if (std::string_view(section) == wifi_profile_type) {
                ssid = properties["ssid"].text;
            }
        }
        sd_bus_message_exit_container(settings);
    }
    sd_bus_message_exit_container(settings);
    return true;
}

struct AccessPoint {
    std::string path;
    std::string ssid;
    int strength = 0;
    NetworkSecurity security = NetworkSecurity::Open;
};

std::vector<AccessPoint> read_access_points(sd_bus *bus, const std::string &device) {
    std::vector<std::string> paths;
    if (!get_paths(bus, device.c_str(), wireless_interface, "AccessPoints", paths)) {
        return {};
    }
    std::map<std::string, AccessPoint> strongest;
    for (const auto &path : paths) {
        sd_bus_error failure = SD_BUS_ERROR_NULL;
        sd_bus_message *reply = nullptr;
        int result = sd_bus_call_method(bus, nm_name, path.c_str(), properties_interface, "GetAll", &failure, &reply, "s", access_point_interface);
        sd_bus_error_free(&failure);
        if (result < 0) {
            continue;
        }
        auto properties = core::Bus::read_properties(reply);
        sd_bus_message_unref(reply);
        const std::string &ssid = properties["Ssid"].text;
        if (ssid.empty()) {
            continue;
        }
        AccessPoint access_point{path, ssid, static_cast<int>(properties["Strength"].number), classify_security(properties["Flags"].number, properties["WpaFlags"].number, properties["RsnFlags"].number)};
        auto [found, inserted] = strongest.try_emplace(ssid, access_point);
        if (!inserted && access_point.strength > found->second.strength) {
            found->second = access_point;
        }
    }
    std::vector<AccessPoint> list;
    for (auto &[ssid, access_point] : strongest) {
        list.push_back(std::move(access_point));
    }
    return list;
}

void open_section(sd_bus_message *message, const char *name) {
    sd_bus_message_open_container(message, 'e', "sa{sv}");
    sd_bus_message_append(message, "s", name);
    sd_bus_message_open_container(message, 'a', "{sv}");
}

void close_section(sd_bus_message *message) {
    sd_bus_message_close_container(message);
    sd_bus_message_close_container(message);
}

void put_string(sd_bus_message *message, const char *key, const char *value) {
    sd_bus_message_open_container(message, 'e', "sv");
    sd_bus_message_append(message, "s", key);
    sd_bus_message_open_container(message, 'v', "s");
    sd_bus_message_append(message, "s", value);
    sd_bus_message_close_container(message);
    sd_bus_message_close_container(message);
}

void put_bytes(sd_bus_message *message, const char *key, const std::string &value) {
    sd_bus_message_open_container(message, 'e', "sv");
    sd_bus_message_append(message, "s", key);
    sd_bus_message_open_container(message, 'v', "ay");
    sd_bus_message_append_array(message, 'y', value.data(), value.size());
    sd_bus_message_close_container(message);
    sd_bus_message_close_container(message);
}

void log_failure(sd_bus_message *reply, const char *action) {
    if (sd_bus_message_is_method_error(reply, nullptr)) {
        const sd_bus_error *failure = sd_bus_message_get_error(reply);
        core::warn("NetworkManager {} failed: {}", action, failure && failure->message ? failure->message : "unknown error");
    }
}

} // namespace

NetworkLink classify_link(std::uint32_t nm_state, std::string_view primary_type, bool activating) {
    if (activating || nm_state == state_connecting) {
        return NetworkLink::Connecting;
    }
    if (nm_state >= state_connected_local) {
        return primary_type == wifi_profile_type ? NetworkLink::Wifi : NetworkLink::Wired;
    }
    return NetworkLink::None;
}

NetworkSecurity classify_security(std::uint32_t flags, std::uint32_t wpa_flags, std::uint32_t rsn_flags) {
    std::uint32_t management = (wpa_flags | rsn_flags) & key_mgmt_mask;
    if (management & key_mgmt_802_1x) {
        return NetworkSecurity::Enterprise;
    }
    if (management & key_mgmt_psk) {
        return NetworkSecurity::Psk;
    }
    if (management & key_mgmt_sae) {
        return NetworkSecurity::Sae;
    }
    if (management != 0) {
        return NetworkSecurity::Enterprise;
    }
    return (flags & ap_flag_privacy) ? NetworkSecurity::Wep : NetworkSecurity::Open;
}

const char *security_label(NetworkSecurity security) {
    switch (security) {
    case NetworkSecurity::Open:
        return "Open";
    case NetworkSecurity::Wep:
        return "WEP";
    case NetworkSecurity::Psk:
        return "WPA2";
    case NetworkSecurity::Sae:
        return "WPA3";
    case NetworkSecurity::Enterprise:
        return "Enterprise";
    }
    return "";
}

bool security_supported(NetworkSecurity security) {
    return security == NetworkSecurity::Open || security == NetworkSecurity::Psk || security == NetworkSecurity::Sae;
}

int signal_band(int percent) {
    if (percent > 75) {
        return 3;
    }
    if (percent > 50) {
        return 2;
    }
    if (percent > 25) {
        return 1;
    }
    return 0;
}

bool is_wifi_profile(std::string_view type) {
    return type == wifi_profile_type;
}

NetworkService::NetworkService(core::EventLoop &loop) : loop_(loop), bus_(loop) {}

NetworkService::~NetworkService() {
    if (refresh_timer_ >= 0) {
        loop_.remove_timer(refresh_timer_);
    }
    if (scan_slot_) {
        sd_bus_slot_unref(scan_slot_);
    }
}

bool NetworkService::start() {
    if (!bus_.open()) {
        return false;
    }
    bus_.on_closed = [this] {
        scan_slot_ = nullptr;
        watching_ = false;
        snapshot_ = {};
        if (on_change) {
            on_change();
        }
    };
    auto signal = +[](sd_bus_message *, void *userdata, sd_bus_error *) -> int {
        static_cast<NetworkService *>(userdata)->schedule_refresh();
        return 0;
    };
    auto profiles_signal = +[](sd_bus_message *, void *userdata, sd_bus_error *) -> int {
        auto *self = static_cast<NetworkService *>(userdata);
        self->profiles_dirty_ = true;
        self->schedule_refresh();
        return 0;
    };
    constexpr const char *rules[]{
        "type='signal',sender='org.freedesktop.NetworkManager',path='/org/freedesktop/NetworkManager',interface='org.freedesktop.NetworkManager',member='PropertiesChanged'",
        "type='signal',sender='org.freedesktop.NetworkManager',interface='org.freedesktop.NetworkManager.Connection.Active',member='StateChanged'",
        "type='signal',sender='org.freedesktop.DBus',interface='org.freedesktop.DBus',member='NameOwnerChanged',arg0='org.freedesktop.NetworkManager'",
    };
    constexpr const char *profile_rules[]{
        "type='signal',sender='org.freedesktop.NetworkManager',interface='org.freedesktop.NetworkManager.Settings'",
        "type='signal',sender='org.freedesktop.NetworkManager',interface='org.freedesktop.NetworkManager.Settings.Connection',member='Updated'",
    };
    for (const char *rule : rules) {
        if (sd_bus_add_match(bus_.handle(), nullptr, rule, signal, this) < 0) {
            core::warn("cannot watch NetworkManager signals");
        }
    }
    for (const char *rule : profile_rules) {
        if (sd_bus_add_match(bus_.handle(), nullptr, rule, profiles_signal, this) < 0) {
            core::warn("cannot watch NetworkManager profiles");
        }
    }
    refresh(false);
    return true;
}

const NetworkSnapshot &NetworkService::snapshot() const {
    return snapshot_;
}

void NetworkService::schedule_refresh(std::chrono::milliseconds delay) {
    if (refresh_timer_ >= 0) {
        return;
    }
    refresh_timer_ = loop_.add_timer(delay, std::chrono::milliseconds{0}, [this] {
        refresh_timer_ = -1;
        refresh(true);
    });
}

void NetworkService::refresh(bool notify) {
    NetworkSnapshot next;
    next.available = query(next);
    if (!next.available) {
        next = {};
    }
    bus_.process();
    if (next == snapshot_) {
        return;
    }
    snapshot_ = std::move(next);
    if (notify && on_change) {
        on_change();
    }
}

bool NetworkService::query(NetworkSnapshot &out) {
    active_paths_.clear();
    sd_bus *bus = bus_.handle();
    if (!bus || !bus_.name_has_owner(nm_name)) {
        return false;
    }
    std::uint32_t state = 0;
    std::string primary;
    std::vector<std::string> actives;
    std::vector<std::string> devices;
    if (!get_bool(bus, nm_path, nm_interface, "WirelessEnabled", out.wifi_enabled) || !get_number(bus, nm_path, nm_interface, "State", state) || !get_text(bus, nm_path, nm_interface, "PrimaryConnectionType", 's', primary) || !get_paths(bus, nm_path, nm_interface, "ActiveConnections", actives) || !get_paths(bus, nm_path, nm_interface, "Devices", devices)) {
        return false;
    }
    for (const auto &device : devices) {
        std::uint32_t type = 0;
        if (get_number(bus, device.c_str(), device_interface, "DeviceType", type) && type == device_type_wifi) {
            out.wifi_present = true;
            wifi_device_ = device;
            break;
        }
    }
    std::map<std::string, std::uint32_t> active_states;
    bool activating = false;
    for (const auto &active : actives) {
        std::string profile;
        std::uint32_t active_state = 0;
        if (get_text(bus, active.c_str(), active_interface, "Connection", 'o', profile) && get_number(bus, active.c_str(), active_interface, "State", active_state)) {
            active_paths_[profile] = active;
            active_states[profile] = active_state;
            activating = activating || active_state == active_activating;
        }
    }
    out.link = classify_link(state, primary, activating);
    std::string dropped = settle_pending(active_states);

    sd_bus_error failure = SD_BUS_ERROR_NULL;
    sd_bus_message *listing = nullptr;
    int result = sd_bus_call_method(bus, nm_name, settings_path, settings_interface, "ListConnections", &failure, &listing, "");
    sd_bus_error_free(&failure);
    if (result < 0) {
        return false;
    }
    char **list = nullptr;
    result = sd_bus_message_read_strv(listing, &list);
    sd_bus_message_unref(listing);
    auto profiles = take_paths(list);
    if (result < 0) {
        return false;
    }
    std::vector<AccessPoint> access_points;
    if (watching_ && out.wifi_present && out.wifi_enabled) {
        access_points = read_access_points(bus, wifi_device_);
    }
    std::map<std::string, const AccessPoint *> in_range;
    for (const auto &access_point : access_points) {
        in_range[access_point.ssid] = &access_point;
    }
    struct Ranked {
        NetworkEntry entry;
        int strength = 0;
    };
    std::vector<Ranked> ranked;
    std::set<std::string> saved_names;
    if (profiles_dirty_) {
        profile_cache_.clear();
        profiles_dirty_ = false;
    }
    std::erase_if(profile_cache_, [&](const auto &item) { return std::ranges::find(profiles, item.first) == profiles.end(); });
    for (const auto &profile : profiles) {
        if (!profile_cache_.contains(profile)) {
            SavedProfile saved;
            sd_bus_message *settings = nullptr;
            failure = SD_BUS_ERROR_NULL;
            result = sd_bus_call_method(bus, nm_name, profile.c_str(), profile_interface, "GetSettings", &failure, &settings, "");
            sd_bus_error_free(&failure);
            if (result < 0) {
                continue;
            }
            std::string id;
            std::string type;
            std::string ssid;
            if (read_profile(settings, id, type, ssid) && is_wifi_profile(type)) {
                saved.name = ssid.empty() ? id : ssid;
                saved.wifi = true;
            }
            sd_bus_message_unref(settings);
            profile_cache_[profile] = std::move(saved);
        }
        const auto &saved = profile_cache_[profile];
        if (profile == dropped || !saved.wifi) {
            continue;
        }
        const std::string &name = saved.name;
        auto found = active_states.find(profile);
        bool known = found != active_states.end();
        bool active = known && found->second == active_activated;
        bool connecting = known && found->second == active_activating;
        auto range = in_range.find(name);
        bool visible = range != in_range.end();
        saved_names.insert(name);
        if (active || connecting || visible) {
            NetworkEntry entry{profile, visible ? range->second->path : std::string{}, name, visible ? signal_band(range->second->strength) : 3, visible ? range->second->security : NetworkSecurity::Open, true, active, connecting, true};
            ranked.push_back({std::move(entry), visible ? range->second->strength : 100});
        }
    }
    for (const auto &access_point : access_points) {
        if (!saved_names.contains(access_point.ssid)) {
            ranked.push_back({NetworkEntry{"", access_point.path, access_point.ssid, signal_band(access_point.strength), access_point.security, false, false, false, true}, access_point.strength});
        }
    }
    std::ranges::sort(ranked, [](const Ranked &a, const Ranked &b) {
        if (a.entry.band != b.entry.band) {
            return a.entry.band > b.entry.band;
        }
        return a.strength != b.strength ? a.strength > b.strength : a.entry.name < b.entry.name;
    });
    for (auto &item : ranked) {
        out.entries.push_back(std::move(item.entry));
    }
    out.error = error_;
    return true;
}

std::string NetworkService::settle_pending(const std::map<std::string, std::uint32_t> &active_states) {
    if (pending_profile_.empty()) {
        return {};
    }
    auto found = active_states.find(pending_profile_);
    if (found != active_states.end() && found->second == active_activated) {
        pending_profile_.clear();
        pending_name_.clear();
        error_.clear();
        return {};
    }
    if (found != active_states.end()) {
        return {};
    }
    auto reply = +[](sd_bus_message *message, void *userdata, sd_bus_error *) -> int {
        log_failure(message, "profile cleanup");
        static_cast<NetworkService *>(userdata)->schedule_refresh();
        return 0;
    };
    std::string dropped = pending_profile_;
    bus_.call_async(nm_name, dropped.c_str(), profile_interface, "Delete", reply, this, 0, nullptr);
    error_ = "Could not connect to " + pending_name_;
    pending_profile_.clear();
    pending_name_.clear();
    return dropped;
}

void NetworkService::begin_scan() {
    error_.clear();
    if (!watching_) {
        watching_ = true;
        auto signal = +[](sd_bus_message *, void *userdata, sd_bus_error *) -> int {
            static_cast<NetworkService *>(userdata)->schedule_refresh(scan_refresh_delay);
            return 0;
        };
        if (sd_bus_add_match(bus_.handle(), &scan_slot_, "type='signal',sender='org.freedesktop.NetworkManager',interface='org.freedesktop.NetworkManager.Device.Wireless'", signal, this) < 0) {
            core::warn("cannot watch Wi-Fi scan results");
        }
    }
    request_scan();
    schedule_refresh(open_refresh_delay);
}

void NetworkService::end_scan() {
    if (!watching_) {
        return;
    }
    watching_ = false;
    error_.clear();
    if (scan_slot_) {
        sd_bus_slot_unref(scan_slot_);
        scan_slot_ = nullptr;
    }
}

void NetworkService::scan() {
    if (watching_) {
        request_scan();
    }
}

void NetworkService::request_scan() {
    sd_bus *bus = bus_.handle();
    if (!bus || wifi_device_.empty()) {
        return;
    }
    sd_bus_message *message = nullptr;
    if (sd_bus_message_new_method_call(bus, &message, nm_name, wifi_device_.c_str(), wireless_interface, "RequestScan") < 0) {
        return;
    }
    sd_bus_message_open_container(message, 'a', "{sv}");
    sd_bus_message_close_container(message);
    bus_.send_async(message, +[](sd_bus_message *, void *, sd_bus_error *) -> int { return 0; }, this, 0);
    sd_bus_message_unref(message);
}

void NetworkService::connect(const std::string &ap, const std::string &password) {
    auto found = std::ranges::find_if(snapshot_.entries, [&](const NetworkEntry &entry) { return entry.ap == ap && !entry.saved; });
    sd_bus *bus = bus_.handle();
    if (!bus || wifi_device_.empty() || found == snapshot_.entries.end() || !security_supported(found->security) || (found->security != NetworkSecurity::Open && password.empty())) {
        return;
    }
    error_.clear();
    pending_name_ = found->name;
    sd_bus_message *message = nullptr;
    if (sd_bus_message_new_method_call(bus, &message, nm_name, nm_path, nm_interface, "AddAndActivateConnection") < 0) {
        return;
    }
    sd_bus_message_open_container(message, 'a', "{sa{sv}}");
    open_section(message, "connection");
    put_string(message, "type", wifi_profile_type);
    put_string(message, "id", found->name.c_str());
    close_section(message);
    open_section(message, wifi_profile_type);
    put_bytes(message, "ssid", found->name);
    put_string(message, "mode", "infrastructure");
    close_section(message);
    if (found->security != NetworkSecurity::Open) {
        open_section(message, "802-11-wireless-security");
        put_string(message, "key-mgmt", found->security == NetworkSecurity::Sae ? "sae" : "wpa-psk");
        put_string(message, "psk", password.c_str());
        close_section(message);
    }
    sd_bus_message_close_container(message);
    sd_bus_message_append(message, "oo", wifi_device_.c_str(), ap.c_str());
    auto reply = +[](sd_bus_message *message, void *userdata, sd_bus_error *) -> int {
        auto *self = static_cast<NetworkService *>(userdata);
        if (sd_bus_message_is_method_error(message, nullptr)) {
            const sd_bus_error *failure = sd_bus_message_get_error(message);
            core::warn("NetworkManager connect failed: {}", failure && failure->message ? failure->message : "unknown error");
            self->error_ = "Could not connect to " + self->pending_name_;
            self->pending_name_.clear();
        } else {
            const char *profile = nullptr;
            const char *active = nullptr;
            if (sd_bus_message_read(message, "oo", &profile, &active) >= 0 && profile) {
                self->pending_profile_ = profile;
            }
        }
        self->schedule_refresh();
        return 0;
    };
    bus_.send_async(message, reply, this, 0);
    sd_bus_message_unref(message);
    schedule_refresh();
}

void NetworkService::set_wifi_enabled(bool enabled) {
    sd_bus_error failure = SD_BUS_ERROR_NULL;
    int result = sd_bus_set_property(bus_.handle(), nm_name, nm_path, nm_interface, "WirelessEnabled", &failure, "b", enabled ? 1 : 0);
    if (result < 0) {
        core::warn("NetworkManager cannot switch Wi-Fi: {}", failure.message ? failure.message : "unknown error");
    }
    sd_bus_error_free(&failure);
    bus_.process();
    schedule_refresh();
}

void NetworkService::toggle(const std::string &profile) {
    auto found = std::ranges::find(snapshot_.entries, profile, &NetworkEntry::path);
    if (found == snapshot_.entries.end()) {
        return;
    }
    auto reply = +[](sd_bus_message *message, void *userdata, sd_bus_error *) -> int {
        log_failure(message, "request");
        static_cast<NetworkService *>(userdata)->schedule_refresh();
        return 0;
    };
    if (found->active || found->connecting) {
        auto active = active_paths_.find(profile);
        if (active != active_paths_.end()) {
            bus_.call_async(nm_name, nm_path, nm_interface, "DeactivateConnection", reply, this, 0, "o", active->second.c_str());
        }
        return;
    }
    bus_.call_async(nm_name, nm_path, nm_interface, "ActivateConnection", reply, this, 0, "ooo", profile.c_str(), "/", "/");
}

} // namespace service
