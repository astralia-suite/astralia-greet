#include "service/vt_service.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <format>
#include <linux/kd.h>
#include <linux/vt.h>
#include <string_view>
#include <sys/ioctl.h>
#include <unistd.h>

#include "core/log.h"

namespace service {

VtService::~VtService() {
    close();
}

int VtService::find_free() {
    int console = ::open("/dev/tty0", O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (console < 0) {
        core::error("cannot open /dev/tty0: {}", std::strerror(errno));
        return 0;
    }
    int free_vt = 0;
    bool found = ioctl(console, VT_OPENQRY, &free_vt) == 0 && free_vt > 0;
    ::close(console);
    if (!found) {
        core::error("no free VT");
        return 0;
    }
    return free_vt;
}

bool VtService::open(int number) {
    if (number <= 0) {
        number = find_free();
    }
    if (number <= 0) {
        return false;
    }
    number_ = number;
    fd_ = ::open(path().c_str(), O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (fd_ < 0) {
        core::error("cannot open {}: {}", path(), std::strerror(errno));
        return false;
    }
    if (ioctl(fd_, KDGKBMODE, &saved_keyboard_mode_) < 0 || saved_keyboard_mode_ == K_OFF) {
        saved_keyboard_mode_ = K_UNICODE;
    }
    return true;
}

int VtService::number() const {
    return number_;
}

std::string VtService::path() const {
    return std::format("/dev/tty{}", number_);
}

bool VtService::activate() {
    if (ioctl(fd_, VT_ACTIVATE, number_) < 0) {
        core::error("VT_ACTIVATE {} failed: {}", number_, std::strerror(errno));
        return false;
    }
    ioctl(fd_, VT_WAITACTIVE, number_);
    return true;
}

bool VtService::enter_graphics(int release_signal, int acquire_signal) {
    vt_mode mode{};
    mode.mode = VT_PROCESS;
    mode.relsig = static_cast<short>(release_signal);
    mode.acqsig = static_cast<short>(acquire_signal);
    if (ioctl(fd_, VT_SETMODE, &mode) < 0) {
        core::error("VT_SETMODE failed: {}", std::strerror(errno));
        return false;
    }
    ioctl(fd_, KDSKBMODE, K_OFF);
    ioctl(fd_, KDSETMODE, KD_GRAPHICS);
    graphics_ = true;
    return true;
}

void VtService::leave_graphics() {
    if (fd_ < 0 || !graphics_) {
        return;
    }
    vt_mode mode{};
    mode.mode = VT_AUTO;
    ioctl(fd_, VT_SETMODE, &mode);
    ioctl(fd_, KDSETMODE, KD_TEXT);
    ioctl(fd_, KDSKBMODE, saved_keyboard_mode_);
    graphics_ = false;
}

bool VtService::hide_text() {
    constexpr std::string_view clear = "\33[H\33[2J";
    if (::write(fd_, clear.data(), clear.size()) < 0) {
        core::warn("cannot clear {}: {}", path(), std::strerror(errno));
    }
    if (ioctl(fd_, KDSETMODE, KD_GRAPHICS) < 0) {
        core::warn("KD_GRAPHICS on {} failed: {}", path(), std::strerror(errno));
        return false;
    }
    return true;
}

void VtService::show_text() {
    if (fd_ >= 0) {
        ioctl(fd_, KDSETMODE, KD_TEXT);
    }
}

bool VtService::take_as_stdin() {
    if (fd_ < 0 || dup2(fd_, STDIN_FILENO) < 0 || setsid() < 0) {
        return false;
    }
    return ioctl(STDIN_FILENO, TIOCSCTTY, 0) == 0;
}

void VtService::ack_release() {
    ioctl(fd_, VT_RELDISP, 1);
}

void VtService::ack_acquire() {
    ioctl(fd_, VT_RELDISP, VT_ACKACQ);
}

bool VtService::switch_to(int number) {
    return ioctl(fd_, VT_ACTIVATE, number) == 0;
}

void VtService::close() {
    leave_graphics();
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

} // namespace service
