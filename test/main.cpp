#include <cstdio>

#include "test/check.h"

void test_strings();
void test_ini();
void test_desktop_entry();
void test_session_detect();
void test_user_filter();
void test_xauth();
void test_launch_env();
void test_state_store();
void test_layout();
void test_image();

int main() {
    struct Case {
        const char *name;
        void (*run)();
    };
    constexpr Case cases[]{
        {"strings", test_strings},
        {"ini", test_ini},
        {"desktop_entry", test_desktop_entry},
        {"session_detect", test_session_detect},
        {"user_filter", test_user_filter},
        {"xauth", test_xauth},
        {"launch_env", test_launch_env},
        {"state_store", test_state_store},
        {"layout", test_layout},
        {"image", test_image},
    };
    for (const auto &c : cases) {
        int before = test::failures;
        c.run();
        std::printf("%-16s %s\n", c.name, test::failures == before ? "ok" : "FAILED");
    }
    std::printf("%d checks, %d failures\n", test::checks, test::failures);
    return test::failures == 0 ? 0 : 1;
}
