#include <string>

#include "core/strings.h"

#include "test/check.h"

void test_strings() {
    CHECK(core::trim("  a b \t") == "a b");
    CHECK(core::trim("   ").empty());
    auto parts = core::split(" a, b;;c ,", ",;");
    CHECK(parts.size() == 3 && parts[0] == "a" && parts[1] == "b" && parts[2] == "c");
    CHECK(core::split_words(" -nolisten  tcp ").size() == 2);
    CHECK(core::join({"a", "b"}, ":") == "a:b");
    CHECK(core::parse_bool("Yes") == true);
    CHECK(core::parse_bool("off") == false);
    CHECK(!core::parse_bool("maybe"));
    CHECK(core::parse_int(" 42 ") == 42);
    CHECK(!core::parse_int("4x"));
    CHECK(core::parse_double("0.5") == 0.5);
    CHECK(core::utf8_length("a•b") == 3);
    CHECK(core::utf8_first("été") == "é");
    std::string text = "aé";
    core::utf8_pop_back(text);
    CHECK(text == "a");
    std::string secret = "hunter2";
    core::secure_clear(secret);
    CHECK(secret.empty());
}
