#include <array>

#include "service/xserver_service.h"

#include "test/check.h"

void test_xauth() {
    std::array<std::uint8_t, 16> cookie{};
    for (std::size_t i = 0; i < cookie.size(); ++i) {
        cookie[i] = static_cast<std::uint8_t>(i);
    }
    auto entry = service::xauth_entry("3", cookie);
    std::vector<std::uint8_t> expected{0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01, '3', 0x00, 18};
    for (char c : std::string_view("MIT-MAGIC-COOKIE-1")) {
        expected.push_back(static_cast<std::uint8_t>(c));
    }
    expected.push_back(0x00);
    expected.push_back(16);
    expected.insert(expected.end(), cookie.begin(), cookie.end());
    CHECK(entry == expected);

    auto root = test::temp_dir("xdisplay");
    CHECK(service::find_free_display(root) == 0);
    test::write_text(root / ".X0-lock", "1");
    test::write_text(root / ".X11-unix" / "X1", "");
    CHECK(service::find_free_display(root) == 2);
    std::filesystem::remove_all(root);

    CHECK(service::user_auth_path("/run/user/1000", 1000, 0) == "/run/user/1000/xauth_astralia_1000-0");
    CHECK(service::user_auth_path("/run/user/1000", 1000, 0) != service::user_auth_path("/run/user/1000", 1000, 1));

    config::XorgSettings settings;
    auto argv = service::xorg_command(settings, 2, 1, "/run/astralia-greet/xauth-2");
    CHECK(argv.size() == 10);
    CHECK(argv[0] == "/usr/bin/Xorg" && argv[1] == ":2" && argv[2] == "vt1" && argv[3] == "-auth");
    CHECK(argv[5] == "-nolisten" && argv[6] == "tcp");
    CHECK(argv[9] == "-noreset");

    auto home = test::temp_dir("xresources");
    auto system_file = home / "system-Xresources";
    CHECK(service::xrdb_commands(system_file, home).empty());
    test::write_text(home / ".Xresources", "Xft.dpi: 96\n");
    auto user_only = service::xrdb_commands(system_file, home);
    CHECK(user_only.size() == 1);
    std::vector<std::string> expected_user{"xrdb", "-merge", (home / ".Xresources").string()};
    CHECK(user_only[0] == expected_user);
    test::write_text(system_file, "*background: #000000\n");
    auto both = service::xrdb_commands(system_file, home);
    CHECK(both.size() == 2);
    CHECK(both[0].back() == system_file.string() && both[1].back() == (home / ".Xresources").string());
    std::filesystem::remove_all(home);
}
