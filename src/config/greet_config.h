#pragma once

#include <array>
#include <string>
#include <vector>

namespace config {

// Paths
inline constexpr const char *state_path = "/var/lib/astralia-greet/state";
inline constexpr const char *runtime_dir = "/run/astralia-greet";
inline constexpr const char *font_dir = ASTRALIA_GREET_DATA_DIR "/fonts";
inline constexpr const char *source_font_dir = ASTRALIA_GREET_SOURCE_DIR "/assets/fonts";
inline constexpr const char *accounts_icon_dir = "/var/lib/AccountsService/icons";
inline constexpr const char *session_log = ".local/share/astralia-greet/session.log";

// Fonts
inline constexpr std::array<const char *, 2> font_files{"ComicShannsMono-Regular.otf", "tabler-icons.ttf"};

// X resources
inline constexpr const char *xrdb_path = "xrdb";
inline constexpr const char *system_xresources = "/etc/X11/Xresources";
inline constexpr const char *user_xresources = ".Xresources";

// System
inline constexpr const char *pam_service = "astralia-greet";
inline constexpr const char *pam_autologin_service = "astralia-greet-autologin";
inline constexpr const char *pam_greeter_service = "astralia-greet-greeter";
inline constexpr const char *greeter_user = "astralia-greet";
inline constexpr int seat_enable_timeout_ms = 3000;
inline constexpr int holder_stop_timeout_ms = 1000;
inline constexpr const char *seat = "seat0";
inline constexpr const char *default_path = "/usr/local/sbin:/usr/local/bin:/usr/bin:/usr/sbin:/bin:/sbin";
inline constexpr int xorg_ready_timeout_seconds = 10;
inline constexpr int xorg_stop_timeout_ms = 5000;
inline constexpr int relogin_min_session_seconds = 10;
inline constexpr int damage_padding = 4;

enum class SessionType { X11,
                         Wayland };

struct GeneralSettings {
    int vt = 1;
    std::string output;
    bool numlock = false;
    std::string keyboard_model;
    std::string keyboard_layout;
    std::string keyboard_variant;
    std::string keyboard_options;
    int repeat_delay = 400;
    int repeat_rate = 30;
    std::string default_session;
    bool remember_last_user = true;
    bool remember_last_session = true;
};

struct UserSettings {
    int minimum_uid = 1000;
    int maximum_uid = 60000;
    std::vector<std::string> hide_users;
    std::vector<std::string> hide_shells{"/usr/bin/nologin", "/usr/sbin/nologin", "/sbin/nologin", "/bin/false", "/usr/bin/false"};
};

struct AutologinSettings {
    std::string user;
    std::string session;
    bool relogin = false;
};

struct CustomSession {
    std::string id;
    std::string name;
    std::string exec;
    SessionType type = SessionType::X11;
    std::vector<std::string> desktop_names;
};

struct SessionSettings {
    std::vector<std::string> x11_dirs{"/usr/share/xsessions"};
    std::vector<std::string> wayland_dirs{"/usr/share/wayland-sessions"};
    std::vector<CustomSession> custom;
};

struct XorgSettings {
    std::string path = "/usr/bin/Xorg";
    std::vector<std::string> args{"-nolisten", "tcp", "-background", "none", "-noreset"};
    std::string display_setup;
    std::string session_wrapper;
};

struct WaylandSettings {
    std::string session_wrapper;
};

struct PowerSettings {
    bool allow_reboot = true;
    bool allow_poweroff = true;
    bool allow_suspend = true;
};

struct Settings {
    GeneralSettings general;
    UserSettings users;
    AutologinSettings autologin;
    SessionSettings sessions;
    XorgSettings xorg;
    WaylandSettings wayland;
    PowerSettings power;
};

} // namespace config
