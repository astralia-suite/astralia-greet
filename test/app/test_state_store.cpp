#include "app/state_store.h"

#include "test/check.h"

void test_state_store() {
    auto root = test::temp_dir("state");
    auto path = root / "nested" / "state";
    {
        app::StateStore store(path);
        store.load();
        CHECK(!store.last_user());
        store.set_last_user("alice");
        store.set_last_session("alice", "wayland/hyprland");
        store.set_last_session("bob", "x11/i3");
        CHECK(store.save());
    }
    app::StateStore reloaded(path);
    reloaded.load();
    CHECK(reloaded.last_user() == "alice");
    CHECK(reloaded.last_session("alice") == "wayland/hyprland");
    CHECK(reloaded.last_session("bob") == "x11/i3");
    CHECK(!reloaded.last_session("carol"));
    CHECK((std::filesystem::status(path).permissions() & std::filesystem::perms::others_read) == std::filesystem::perms::none);
    std::filesystem::remove_all(root);
}
