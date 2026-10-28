#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "core/bus.h"
#include "core/event_loop.h"

namespace service {

enum class NetworkLink { None,
                         Connecting,
                         Wifi,
                         Wired };

enum class NetworkSecurity { Open,
                             Wep,
                             Psk,
                             Sae,
                             Enterprise };

struct NetworkEntry {
    std::string path;
    std::string ap;
    std::string name;
    int band = 0;
    NetworkSecurity security = NetworkSecurity::Open;
    bool saved = false;
    bool active = false;
    bool connecting = false;
    bool in_range = false;

    bool operator==(const NetworkEntry &) const = default;
};

struct NetworkSnapshot {
    bool available = false;
    bool wifi_present = false;
    bool wifi_enabled = false;
    NetworkLink link = NetworkLink::None;
    std::string error;
    std::vector<NetworkEntry> entries;

    bool operator==(const NetworkSnapshot &) const = default;
};

NetworkLink classify_link(std::uint32_t nm_state, std::string_view primary_type, bool activating);
NetworkSecurity classify_security(std::uint32_t flags, std::uint32_t wpa_flags, std::uint32_t rsn_flags);
const char *security_label(NetworkSecurity security);
bool security_supported(NetworkSecurity security);
int signal_band(int percent);
bool is_wifi_profile(std::string_view type);

class NetworkService {
  public:
    explicit NetworkService(core::EventLoop &loop);
    ~NetworkService();
    NetworkService(const NetworkService &) = delete;
    NetworkService &operator=(const NetworkService &) = delete;

    bool start();
    const NetworkSnapshot &snapshot() const;
    void set_wifi_enabled(bool enabled);
    void toggle(const std::string &profile);
    void begin_scan();
    void end_scan();
    void scan();
    void connect(const std::string &ap, const std::string &password);

    std::function<void()> on_change;

  private:
    void schedule_refresh(std::chrono::milliseconds delay = std::chrono::milliseconds{100});
    void refresh(bool notify);
    bool query(NetworkSnapshot &out);
    void request_scan();
    std::string settle_pending(const std::map<std::string, std::uint32_t> &active_states);

    core::EventLoop &loop_;
    core::Bus bus_;
    NetworkSnapshot snapshot_;
    std::map<std::string, std::string> active_paths_;
    struct SavedProfile {
        std::string name;
        bool wifi = false;
    };

    std::map<std::string, SavedProfile> profile_cache_;
    bool profiles_dirty_ = true;
    std::string wifi_device_;
    std::string pending_profile_;
    std::string pending_name_;
    std::string error_;
    sd_bus_slot *scan_slot_ = nullptr;
    bool watching_ = false;
    int refresh_timer_ = -1;
};

} // namespace service
