#include "service/user_service.h"

#include "test/check.h"

void test_user_filter() {
    auto users = service::parse_passwd("root:x:0:0::/root:/usr/bin/bash\n"
                                       "# comment\n"
                                       "zoe:x:1001:1001:Zoe Q,,,:/home/zoe:/usr/bin/zsh\n"
                                       "alice:x:1000:1000::/home/alice:/usr/bin/bash\n"
                                       "svc:x:1002:1002::/var/svc:/usr/bin/nologin\n"
                                       "hidden:x:1003:1003::/home/hidden:/bin/bash\n"
                                       "nobody:x:65534:65534:Nobody:/:/usr/bin/nologin\n"
                                       "broken:x:abc:1::/:/bin/sh\n"
                                       "short:x:1\n");
    CHECK(users.size() == 6);
    CHECK(users[1].display_name == "Zoe Q");
    CHECK(users[2].display_name == "alice");

    config::UserSettings settings;
    settings.hide_users = {"hidden"};
    auto shown = service::filter_users(users, settings);
    CHECK(shown.size() == 2);
    CHECK(shown[0].name == "alice" && shown[1].name == "zoe");
    CHECK(shown[1].uid == 1001 && shown[1].home == "/home/zoe");

    auto root = test::temp_dir("avatars");
    service::User user{"alice", "alice", (root / "home").string(), "/bin/sh", 1000, 1000, {}};
    CHECK(service::find_avatar(user, root / "icons").empty());
    test::write_text(root / "home" / ".face", "x");
    CHECK(service::find_avatar(user, root / "icons") == root / "home" / ".face");
    test::write_text(root / "icons" / "alice", "x");
    CHECK(service::find_avatar(user, root / "icons") == root / "icons" / "alice");
    std::filesystem::remove_all(root);
}
