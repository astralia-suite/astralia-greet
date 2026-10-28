#include "service/bluetooth_service.h"
#include "service/network_service.h"

#include <string>

#include "test/check.h"

void test_radio() {
    using service::NetworkLink;
    CHECK(service::classify_link(70, "802-11-wireless", false) == NetworkLink::Wifi);
    CHECK(service::classify_link(70, "802-3-ethernet", false) == NetworkLink::Wired);
    CHECK(service::classify_link(50, "", false) == NetworkLink::Wired);
    CHECK(service::classify_link(20, "", false) == NetworkLink::None);
    CHECK(service::classify_link(40, "", false) == NetworkLink::Connecting);
    CHECK(service::classify_link(70, "802-11-wireless", true) == NetworkLink::Connecting);
    CHECK(service::is_wifi_profile("802-11-wireless"));
    CHECK(!service::is_wifi_profile("802-3-ethernet"));
    using service::NetworkSecurity;
    CHECK(service::classify_security(0, 0, 0) == NetworkSecurity::Open);
    CHECK(service::classify_security(1, 0, 0) == NetworkSecurity::Wep);
    CHECK(service::classify_security(1, 0x108, 0) == NetworkSecurity::Psk);
    CHECK(service::classify_security(1, 0, 0x108) == NetworkSecurity::Psk);
    CHECK(service::classify_security(1, 0, 0x400) == NetworkSecurity::Sae);
    CHECK(service::classify_security(1, 0, 0x108 | 0x400) == NetworkSecurity::Psk);
    CHECK(service::classify_security(1, 0, 0x200) == NetworkSecurity::Enterprise);
    CHECK(service::classify_security(1, 0, 0x800) == NetworkSecurity::Enterprise);
    CHECK(service::security_supported(NetworkSecurity::Open) && service::security_supported(NetworkSecurity::Psk) && service::security_supported(NetworkSecurity::Sae));
    CHECK(!service::security_supported(NetworkSecurity::Wep) && !service::security_supported(NetworkSecurity::Enterprise));
    CHECK(std::string(service::security_label(NetworkSecurity::Psk)) == "WPA2");
    CHECK(std::string(service::security_label(NetworkSecurity::Sae)) == "WPA3");
    CHECK(service::signal_band(100) == 3 && service::signal_band(76) == 3);
    CHECK(service::signal_band(75) == 2 && service::signal_band(51) == 2);
    CHECK(service::signal_band(50) == 1 && service::signal_band(26) == 1);
    CHECK(service::signal_band(25) == 0 && service::signal_band(0) == 0);
    CHECK(service::device_label("Keyboard", "K380", "AA:BB") == "Keyboard");
    CHECK(service::device_label("", "K380", "AA:BB") == "K380");
    CHECK(service::device_label("", "", "AA:BB") == "AA:BB");
}
