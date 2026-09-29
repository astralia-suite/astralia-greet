#include "service/session_service.h"

#include "test/check.h"

void test_session_detect() {
    auto root = test::temp_dir("sessions");
    auto x11 = root / "xsessions";
    auto wayland = root / "wayland-sessions";
    test::write_text(x11 / "plasma.desktop", "[Desktop Entry]\nName=Plasma\nExec=startplasmax11\nDesktopNames=KDE\n");
    test::write_text(wayland / "plasma.desktop", "[Desktop Entry]\nName=Plasma\nExec=startplasmawayland\nDesktopNames=KDE\n");
    test::write_text(wayland / "hyprland.desktop", "[Desktop Entry]\nName=Hyprland\nExec=Hyprland\n");
    test::write_text(wayland / "missing.desktop", "[Desktop Entry]\nName=Missing\nExec=missing\nTryExec=/nonexistent/binary\n");
    test::write_text(x11 / "hidden.desktop", "[Desktop Entry]\nName=Hidden\nExec=h\nHidden=true\n");
    test::write_text(x11 / "openbox.desktop", "[Desktop Entry]\nName=Openbox\nExec=openbox-session\nTryExec=sh\n");
    test::write_text(x11 / "notes.txt", "ignored");

    config::SessionSettings settings;
    settings.x11_dirs = {x11.string(), (root / "absent").string()};
    settings.wayland_dirs = {wayland.string()};
    settings.custom.push_back({"dwm", "dwm", "dwm", config::SessionType::X11, {}});
    settings.custom.push_back({"empty", "Empty", "", config::SessionType::X11, {}});

    auto sessions = service::detect_sessions(settings, "/usr/bin:/bin");
    CHECK(sessions.size() == 5);
    std::vector<std::string> names;
    for (const auto &session : sessions) {
        names.push_back(session.name);
    }
    CHECK(names == (std::vector<std::string>{"dwm", "Hyprland", "Openbox", "Plasma (Wayland)", "Plasma (X11)"}));
    CHECK(sessions[0].id == "custom/dwm");
    CHECK(sessions[1].id == "wayland/hyprland");
    CHECK(sessions[1].type == config::SessionType::Wayland);
    CHECK(sessions[4].desktop_names == std::vector<std::string>{"KDE"});
    CHECK(service::session_desktop_name(sessions[1]) == "hyprland");

    std::filesystem::remove_all(root);
}
