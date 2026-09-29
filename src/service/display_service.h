#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <xf86drmMode.h>

#include "render/canvas.h"
#include "render/layout.h"

#include "service/seat_service.h"

namespace service {

class DisplayService {
  public:
    ~DisplayService();

    void use_seat(SeatService *seat);
    bool open(std::string_view output);
    int width() const;
    int height() const;
    void present(const render::Canvas &canvas, const std::vector<render::Rect> &damage);
    void move_cursor(int x, int y);
    bool drop_master();
    bool acquire_master();
    void close();

  private:
    struct Buffer {
        std::uint32_t handle = 0;
        std::uint32_t pitch = 0;
        std::uint32_t fb = 0;
        std::uint64_t size = 0;
        std::uint8_t *map = nullptr;
    };

    bool pick_output(int fd, std::string_view output);
    bool create_buffer(Buffer &buffer, int width, int height, bool framebuffer);
    void destroy_buffer(Buffer &buffer);
    bool modeset();
    void show_cursor();
    bool managed() const;

    SeatService *seat_ = nullptr;
    int fd_ = -1;
    std::uint32_t connector_ = 0;
    std::uint32_t crtc_ = 0;
    drmModeModeInfo mode_{};
    std::string name_;
    drmModeCrtc *saved_crtc_ = nullptr;
    Buffer front_;
    Buffer cursor_;
    bool cursor_ok_ = false;
    bool cursor_shown_ = false;
    int cursor_x_ = 0;
    int cursor_y_ = 0;
};

} // namespace service
