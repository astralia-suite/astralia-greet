#include "core/desktop_entry.h"

#include "test/check.h"

void test_desktop_entry() {
    auto entry = core::parse_desktop_entry("[Desktop Entry]\n"
                                           "Name=Plasma\n"
                                           "Name[de]=Plasma DE\n"
                                           "Comment=KDE\n"
                                           "Exec=startplasma-x11 %U --flag 100%%\n"
                                           "TryExec=startplasma-x11\n"
                                           "DesktopNames=KDE;Plasma;\n"
                                           "[Desktop Action other]\n"
                                           "Name=Ignored\n");
    CHECK(entry.has_value());
    CHECK(entry->name == "Plasma");
    CHECK(entry->exec == "startplasma-x11 --flag 100%");
    CHECK(entry->try_exec == "startplasma-x11");
    CHECK(entry->desktop_names.size() == 2 && entry->desktop_names[1] == "Plasma");
    CHECK(!entry->hidden);

    CHECK(!core::parse_desktop_entry("[Desktop Entry]\nName=NoExec\n"));
    CHECK(!core::parse_desktop_entry("Name=x\nExec=y\n"));
    auto hidden = core::parse_desktop_entry("[Desktop Entry]\nName=H\nExec=h\nHidden=true\n");
    CHECK(hidden && hidden->hidden);
    CHECK(core::strip_field_codes("sway %f") == "sway");
}
