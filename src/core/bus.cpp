#include "core/bus.h"

#include <chrono>
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <string_view>
#include <sys/epoll.h>

#include "core/log.h"

namespace core {

namespace {

constexpr std::uint64_t sync_timeout_us = 3'000'000;

std::uint64_t monotonic_us() {
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<std::uint64_t>(now.tv_sec) * 1'000'000 + static_cast<std::uint64_t>(now.tv_nsec) / 1000;
}

} // namespace

Bus::Bus(EventLoop &loop) : loop_(loop) {}

Bus::~Bus() {
    close();
}

bool Bus::open() {
    if (bus_) {
        return true;
    }
    if (int result = sd_bus_open_system(&bus_); result < 0) {
        error("cannot connect to the system bus: {}", std::strerror(-result));
        bus_ = nullptr;
        return false;
    }
    sd_bus_set_method_call_timeout(bus_, sync_timeout_us);
    if (!loop_.add_fd(sd_bus_get_fd(bus_), EPOLLIN, [this](std::uint32_t) { process(); })) {
        close();
        return false;
    }
    return true;
}

void Bus::close() {
    if (timer_ >= 0) {
        loop_.remove_timer(timer_);
        timer_ = -1;
    }
    if (!bus_) {
        return;
    }
    loop_.remove_fd(sd_bus_get_fd(bus_));
    sd_bus_flush_close_unref(bus_);
    bus_ = nullptr;
}

bool Bus::is_open() const {
    return bus_ != nullptr;
}

sd_bus *Bus::handle() const {
    return bus_;
}

void Bus::process() {
    if (!bus_) {
        return;
    }
    int result = 0;
    do {
        result = sd_bus_process(bus_, nullptr);
    } while (result > 0);
    if (result < 0) {
        warn("system bus connection lost: {}", std::strerror(-result));
        close();
        if (on_closed) {
            on_closed();
        }
        return;
    }
    arm_timeout();
}

void Bus::arm_timeout() {
    if (timer_ >= 0) {
        loop_.remove_timer(timer_);
        timer_ = -1;
    }
    std::uint64_t deadline = 0;
    if (!bus_ || sd_bus_get_timeout(bus_, &deadline) < 0 || deadline == UINT64_MAX) {
        return;
    }
    auto now = monotonic_us();
    auto delay_ms = deadline > now ? (deadline - now + 999) / 1000 : 1;
    timer_ = loop_.add_timer(std::chrono::milliseconds{static_cast<std::chrono::milliseconds::rep>(delay_ms)}, std::chrono::milliseconds{0}, [this] {
        timer_ = -1;
        process();
    });
}

bool Bus::name_has_owner(const char *name) {
    if (!bus_) {
        return false;
    }
    sd_bus_error failure = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = nullptr;
    int owned = 0;
    int result = sd_bus_call_method(bus_, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameHasOwner", &failure, &reply, "s", name);
    if (result >= 0) {
        result = sd_bus_message_read(reply, "b", &owned);
    }
    sd_bus_message_unref(reply);
    sd_bus_error_free(&failure);
    return result >= 0 && owned != 0;
}

bool Bus::send_async(sd_bus_message *message, sd_bus_message_handler_t handler, void *userdata, std::uint64_t timeout_us) {
    if (!bus_ || !message) {
        return false;
    }
    int result = sd_bus_call_async(bus_, nullptr, message, handler, userdata, timeout_us);
    if (result < 0) {
        warn("system bus call failed: {}", std::strerror(-result));
        return false;
    }
    arm_timeout();
    return true;
}

bool Bus::call_async(const char *destination, const char *path, const char *interface, const char *member, sd_bus_message_handler_t handler, void *userdata, std::uint64_t timeout_us, const char *types, ...) {
    if (!bus_) {
        return false;
    }
    sd_bus_message *message = nullptr;
    int result = sd_bus_message_new_method_call(bus_, &message, destination, path, interface, member);
    if (result >= 0 && types && *types) {
        va_list arguments;
        va_start(arguments, types);
        result = sd_bus_message_appendv(message, types, arguments);
        va_end(arguments);
    }
    if (result >= 0) {
        result = sd_bus_call_async(bus_, nullptr, message, handler, userdata, timeout_us);
    }
    sd_bus_message_unref(message);
    if (result < 0) {
        warn("{} {} failed: {}", interface, member, std::strerror(-result));
        return false;
    }
    arm_timeout();
    return true;
}

Properties Bus::read_properties(sd_bus_message *message) {
    Properties properties;
    if (sd_bus_message_enter_container(message, SD_BUS_TYPE_ARRAY, "{sv}") < 0) {
        return properties;
    }
    while (sd_bus_message_enter_container(message, SD_BUS_TYPE_DICT_ENTRY, "sv") > 0) {
        const char *key = nullptr;
        if (sd_bus_message_read(message, "s", &key) >= 0 && key) {
            char type = 0;
            const char *contents = nullptr;
            sd_bus_message_peek_type(message, &type, &contents);
            PropertyValue value;
            if (type == SD_BUS_TYPE_VARIANT && contents && std::string_view(contents) == "ay") {
                sd_bus_message_enter_container(message, SD_BUS_TYPE_VARIANT, contents);
                const void *bytes = nullptr;
                std::size_t size = 0;
                if (sd_bus_message_read_array(message, SD_BUS_TYPE_BYTE, &bytes, &size) >= 0 && bytes) {
                    value.text.assign(static_cast<const char *>(bytes), size);
                }
                sd_bus_message_exit_container(message);
            } else if (type == SD_BUS_TYPE_VARIANT && contents && contents[0] && !contents[1]) {
                char kind = contents[0];
                sd_bus_message_enter_container(message, SD_BUS_TYPE_VARIANT, contents);
                if (kind == SD_BUS_TYPE_STRING || kind == SD_BUS_TYPE_OBJECT_PATH) {
                    const char *text = nullptr;
                    if (sd_bus_message_read_basic(message, kind, &text) >= 0 && text) {
                        value.text = text;
                    }
                } else if (kind == SD_BUS_TYPE_BOOLEAN) {
                    int flag = 0;
                    sd_bus_message_read_basic(message, kind, &flag);
                    value.flag = flag != 0;
                } else if (kind == SD_BUS_TYPE_UINT32) {
                    sd_bus_message_read_basic(message, kind, &value.number);
                } else if (kind == SD_BUS_TYPE_BYTE) {
                    std::uint8_t byte = 0;
                    sd_bus_message_read_basic(message, kind, &byte);
                    value.number = byte;
                } else {
                    sd_bus_message_skip(message, contents);
                }
                sd_bus_message_exit_container(message);
            } else {
                sd_bus_message_skip(message, "v");
            }
            properties[key] = std::move(value);
        }
        sd_bus_message_exit_container(message);
    }
    sd_bus_message_exit_container(message);
    return properties;
}

} // namespace core
