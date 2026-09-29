#include "core/ini.h"

#include "test/check.h"

void test_ini() {
    auto ini = core::parse_ini("top=1\n"
                               "# comment\n"
                               "; another\n"
                               "[general]\n"
                               "theme = midnight \n"
                               "numlock=yes\n"
                               "vt=2\n"
                               "dim=0.25\n"
                               "quoted=\"  spaced  \"\n"
                               "[users]\n"
                               "hide-users=a, b;c\n"
                               "[widget.clock]\n"
                               "x=4\n"
                               "[widget.password]\n"
                               "y=5\n");
    CHECK(ini.get("", "top") == "1");
    CHECK(ini.get("general", "theme") == "midnight");
    CHECK(ini.get_bool("general", "numlock", false));
    CHECK(ini.get_int("general", "vt", 0) == 2);
    CHECK(ini.get_int("general", "missing", 7) == 7);
    CHECK(ini.get_double("general", "dim", 0.0) == 0.25);
    CHECK(ini.get("general", "quoted") == "  spaced  ");
    auto users = ini.get_list("users", "hide-users");
    CHECK(users && users->size() == 3);
    CHECK(!ini.get_list("users", "absent"));
    auto widgets = ini.subsections("widget.");
    CHECK(widgets.size() == 2 && widgets[0] == "clock" && widgets[1] == "password");

    core::IniFile out;
    out.set("last", "user", "alice");
    out.set("sessions", "alice", "wayland/hyprland");
    auto round = core::parse_ini(core::serialize_ini(out));
    CHECK(round.get("last", "user") == "alice");
    CHECK(round.get("sessions", "alice") == "wayland/hyprland");
}
