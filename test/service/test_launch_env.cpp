#include "service/launch_service.h"

#include "test/check.h"

void test_launch_env() {
    service::User user{"alice", "Alice", "/home/alice", "/usr/bin/zsh", 1000, 1000, {}};
    service::Session wayland{"wayland/hyprland", "Hyprland", "", "Hyprland", config::SessionType::Wayland, {"Hyprland"}};
    service::Session x11{"x11/i3", "i3", "", "i3", config::SessionType::X11, {}};

    auto env = service::session_environment(user, wayland, 1);
    CHECK(env.get("HOME") == "/home/alice");
    CHECK(env.get("USER") == "alice");
    CHECK(env.get("SHELL") == "/usr/bin/zsh");
    CHECK(env.get("XDG_SESSION_TYPE") == "wayland");
    CHECK(env.get("XDG_SESSION_DESKTOP") == "hyprland");
    CHECK(env.get("XDG_CURRENT_DESKTOP") == "Hyprland");
    CHECK(env.get("XDG_VTNR") == "1");
    CHECK(env.get("XDG_SEAT") == "seat0");
    CHECK(service::session_environment(user, x11, 2).get("XDG_SESSION_TYPE") == "x11");
    CHECK(!service::session_environment(user, x11, 2).get("XDG_CURRENT_DESKTOP"));

    config::Settings settings;
    auto argv = service::session_command(wayland, user, settings);
    CHECK((argv == std::vector<std::string>{"/usr/bin/zsh", "-l", "-c", "exec Hyprland"}));
    settings.xorg.session_wrapper = "/etc/astralia-greet/Xsession --verbose";
    argv = service::session_command(x11, user, settings);
    CHECK((argv == std::vector<std::string>{"/etc/astralia-greet/Xsession", "--verbose", "i3"}));
    user.shell.clear();
    CHECK(service::session_command(wayland, user, settings)[0] == "/bin/sh");

    core::Environment merged;
    merged.set_entry("XDG_RUNTIME_DIR=/run/user/1000");
    merged.set_entry("=bad");
    merged.set_entry("novalue");
    CHECK(merged.entries() == std::vector<std::string>{"XDG_RUNTIME_DIR=/run/user/1000"});
    CHECK(core::find_executable("sh", "/nonexistent:/bin:/usr/bin").has_value());
    CHECK(!core::find_executable("definitely-not-a-binary", "/bin"));
}
