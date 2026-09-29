#include "service/power_service.h"

#include <systemd/sd-bus.h>

#include "core/log.h"

namespace service {

namespace {

bool call_logind(const char *method) {
    sd_bus *bus = nullptr;
    if (sd_bus_open_system(&bus) < 0) {
        core::error("cannot connect to the system bus");
        return false;
    }
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int result = sd_bus_call_method(bus, "org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", method, &error, nullptr, "b", 0);
    if (result < 0) {
        core::error("logind {} failed: {}", method, error.message ? error.message : "unknown error");
    }
    sd_bus_error_free(&error);
    sd_bus_unref(bus);
    return result >= 0;
}

} // namespace

bool PowerService::reboot() {
    return call_logind("Reboot");
}

bool PowerService::power_off() {
    return call_logind("PowerOff");
}

bool PowerService::suspend() {
    return call_logind("Suspend");
}

} // namespace service
