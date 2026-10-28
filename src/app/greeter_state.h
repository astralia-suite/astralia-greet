#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/strings.h"

#include "service/bluetooth_service.h"
#include "service/network_service.h"
#include "service/session_service.h"
#include "service/user_service.h"

namespace app {

enum class Focus { Password,
                   Session };

enum class View { Clock,
                  Login };

enum class Request { None,
                     Login,
                     Reboot,
                     PowerOff,
                     Suspend,
                     ToggleWifi,
                     ActivateNetwork,
                     ToggleBluetooth,
                     ToggleBluetoothDevice,
                     ConnectNetwork,
                     ScanNetworks };

struct GreeterState {
    std::vector<service::User> users;
    std::size_t user_index = 0;
    std::string password;
    std::vector<service::Session> sessions;
    std::size_t session_index = 0;
    bool session_menu_open = false;
    bool user_menu_open = false;
    bool network_menu_open = false;
    bool bluetooth_menu_open = false;
    service::NetworkSnapshot network;
    service::BluetoothSnapshot bluetooth;
    View view = View::Clock;
    Focus focus = Focus::Password;
    std::string message;
    bool message_is_error = false;
    bool busy = false;
    bool caps_lock = false;
    std::string layout;
    Request request = Request::None;
    std::string request_target;
    std::string wifi_ap;
    std::string wifi_ssid;
    std::string wifi_password;
    std::size_t wifi_min_length = 0;

    bool wifi_dialog_open() const {
        return !wifi_ap.empty();
    }

    void clear_wifi_dialog() {
        core::secure_clear(wifi_password);
        wifi_ap.clear();
        wifi_ssid.clear();
        wifi_min_length = 0;
    }
};

} // namespace app
