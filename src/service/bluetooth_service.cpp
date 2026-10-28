#include "service/bluetooth_service.h"

#include <algorithm>
#include <chrono>

#include "core/log.h"

namespace service {

namespace {

// BlueZ D-Bus names
constexpr const char *bluez_name = "org.bluez";
constexpr const char *adapter_interface = "org.bluez.Adapter1";
constexpr const char *device_interface = "org.bluez.Device1";

constexpr std::chrono::milliseconds refresh_delay{100};
constexpr std::uint64_t connect_timeout_us = 40'000'000;

} // namespace

std::string device_label(std::string_view alias, std::string_view name, std::string_view address) {
    for (auto candidate : {alias, name, address}) {
        if (!candidate.empty()) {
            return std::string(candidate);
        }
    }
    return {};
}

BluetoothService::BluetoothService(core::EventLoop &loop) : loop_(loop), bus_(loop) {}

BluetoothService::~BluetoothService() {
    if (refresh_timer_ >= 0) {
        loop_.remove_timer(refresh_timer_);
    }
}

bool BluetoothService::start() {
    if (!bus_.open()) {
        return false;
    }
    bus_.on_closed = [this] {
        snapshot_ = {};
        if (on_change) {
            on_change();
        }
    };
    auto signal = +[](sd_bus_message *, void *userdata, sd_bus_error *) -> int {
        static_cast<BluetoothService *>(userdata)->schedule_refresh();
        return 0;
    };
    constexpr const char *rules[]{
        "type='signal',sender='org.bluez',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged'",
        "type='signal',sender='org.bluez',interface='org.freedesktop.DBus.ObjectManager'",
        "type='signal',sender='org.freedesktop.DBus',interface='org.freedesktop.DBus',member='NameOwnerChanged',arg0='org.bluez'",
    };
    for (const char *rule : rules) {
        if (sd_bus_add_match(bus_.handle(), nullptr, rule, signal, this) < 0) {
            core::warn("cannot watch BlueZ signals");
        }
    }
    refresh(false);
    return true;
}

const BluetoothSnapshot &BluetoothService::snapshot() const {
    return snapshot_;
}

void BluetoothService::schedule_refresh() {
    if (refresh_timer_ >= 0) {
        return;
    }
    refresh_timer_ = loop_.add_timer(refresh_delay, std::chrono::milliseconds{0}, [this] {
        refresh_timer_ = -1;
        refresh(true);
    });
}

void BluetoothService::refresh(bool notify) {
    BluetoothSnapshot next;
    next.available = query(next);
    if (!next.available) {
        next = {};
    }
    bus_.process();
    if (next == snapshot_) {
        return;
    }
    snapshot_ = std::move(next);
    if (notify && on_change) {
        on_change();
    }
}

bool BluetoothService::query(BluetoothSnapshot &out) {
    adapter_path_.clear();
    sd_bus *bus = bus_.handle();
    if (!bus || !bus_.name_has_owner(bluez_name)) {
        return false;
    }
    sd_bus_error failure = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = nullptr;
    int result = sd_bus_call_method(bus, bluez_name, "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects", &failure, &reply, "");
    sd_bus_error_free(&failure);
    if (result < 0 || sd_bus_message_enter_container(reply, 'a', "{oa{sa{sv}}}") < 0) {
        sd_bus_message_unref(reply);
        return false;
    }
    while (sd_bus_message_enter_container(reply, 'e', "oa{sa{sv}}") > 0) {
        const char *path = nullptr;
        if (sd_bus_message_read(reply, "o", &path) >= 0 && path && sd_bus_message_enter_container(reply, 'a', "{sa{sv}}") >= 0) {
            while (sd_bus_message_enter_container(reply, 'e', "sa{sv}") > 0) {
                const char *interface = nullptr;
                if (sd_bus_message_read(reply, "s", &interface) >= 0 && interface) {
                    auto properties = core::Bus::read_properties(reply);
                    std::string_view name(interface);
                    if (name == adapter_interface && adapter_path_.empty()) {
                        adapter_path_ = path;
                        out.powered = properties["Powered"].flag;
                    } else if (name == device_interface && properties["Paired"].flag) {
                        out.entries.push_back({path, device_label(properties["Alias"].text, properties["Name"].text, properties["Address"].text), properties["Connected"].flag, path == pending_path_});
                    }
                }
                sd_bus_message_exit_container(reply);
            }
            sd_bus_message_exit_container(reply);
        }
        sd_bus_message_exit_container(reply);
    }
    sd_bus_message_unref(reply);
    std::ranges::sort(out.entries, {}, &BluetoothEntry::name);
    return !adapter_path_.empty();
}

void BluetoothService::set_powered(bool powered) {
    if (adapter_path_.empty()) {
        return;
    }
    sd_bus_error failure = SD_BUS_ERROR_NULL;
    int result = sd_bus_set_property(bus_.handle(), bluez_name, adapter_path_.c_str(), adapter_interface, "Powered", &failure, "b", powered ? 1 : 0);
    if (result < 0) {
        core::warn("BlueZ cannot switch the adapter: {}", failure.message ? failure.message : "unknown error");
    }
    sd_bus_error_free(&failure);
    bus_.process();
    schedule_refresh();
}

void BluetoothService::toggle_device(const std::string &path) {
    if (!pending_path_.empty()) {
        return;
    }
    auto found = std::ranges::find(snapshot_.entries, path, &BluetoothEntry::path);
    if (found == snapshot_.entries.end()) {
        return;
    }
    auto reply = +[](sd_bus_message *message, void *userdata, sd_bus_error *) -> int {
        auto *self = static_cast<BluetoothService *>(userdata);
        if (sd_bus_message_is_method_error(message, nullptr)) {
            const sd_bus_error *failure = sd_bus_message_get_error(message);
            core::warn("BlueZ request failed: {}", failure && failure->message ? failure->message : "unknown error");
        }
        self->pending_path_.clear();
        self->schedule_refresh();
        return 0;
    };
    if (bus_.call_async(bluez_name, path.c_str(), device_interface, found->connected ? "Disconnect" : "Connect", reply, this, connect_timeout_us, nullptr)) {
        pending_path_ = path;
        schedule_refresh();
    }
}

} // namespace service
