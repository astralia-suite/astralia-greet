#include "service/display_service.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <format>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>
#include <xf86drm.h>

#include "core/log.h"

#include "render/cursor.h"

namespace service {

namespace {

constexpr int cursor_size = 64;

bool is_internal(std::uint32_t type) {
    return type == DRM_MODE_CONNECTOR_LVDS || type == DRM_MODE_CONNECTOR_eDP || type == DRM_MODE_CONNECTOR_DSI;
}

std::string connector_name(const drmModeConnector *connector) {
    const char *type = drmModeGetConnectorTypeName(connector->connector_type);
    return std::format("{}-{}", type ? type : "Unknown", connector->connector_type_id);
}

std::uint32_t find_crtc(int fd, const drmModeRes *resources, const drmModeConnector *connector) {
    if (connector->encoder_id) {
        if (drmModeEncoder *encoder = drmModeGetEncoder(fd, connector->encoder_id)) {
            std::uint32_t crtc = encoder->crtc_id;
            drmModeFreeEncoder(encoder);
            if (crtc) {
                return crtc;
            }
        }
    }
    for (int e = 0; e < connector->count_encoders; ++e) {
        drmModeEncoder *encoder = drmModeGetEncoder(fd, connector->encoders[e]);
        if (!encoder) {
            continue;
        }
        for (int c = 0; c < resources->count_crtcs; ++c) {
            if (encoder->possible_crtcs & (1u << c)) {
                std::uint32_t crtc = resources->crtcs[c];
                drmModeFreeEncoder(encoder);
                return crtc;
            }
        }
        drmModeFreeEncoder(encoder);
    }
    return 0;
}

} // namespace

DisplayService::~DisplayService() {
    close();
}

void DisplayService::use_seat(SeatService *seat) {
    seat_ = seat;
}

bool DisplayService::managed() const {
    return seat_ && seat_->is_open();
}

bool DisplayService::open(std::string_view output) {
    for (int index = 0; index < 8; ++index) {
        auto path = std::format("/dev/dri/card{}", index);
        int fd = managed() ? seat_->open_device(path) : ::open(path.c_str(), O_RDWR | O_CLOEXEC);
        if (fd < 0) {
            continue;
        }
        if (!pick_output(fd, output)) {
            if (managed()) {
                seat_->close_device(fd);
            } else {
                ::close(fd);
            }
            continue;
        }
        fd_ = fd;
        saved_crtc_ = drmModeGetCrtc(fd_, crtc_);
        if (!create_buffer(front_, width(), height(), true)) {
            core::error("cannot allocate a framebuffer on {}", path);
            close();
            return false;
        }
        cursor_ok_ = create_buffer(cursor_, cursor_size, cursor_size, false);
        if (cursor_ok_) {
            render::draw_cursor(cursor_.map, cursor_size, static_cast<int>(cursor_.pitch));
        }
        if (!modeset()) {
            close();
            return false;
        }
        core::info("display {} on {} at {}x{}", name_, path, width(), height());
        return true;
    }
    core::error("no usable display found");
    return false;
}

bool DisplayService::pick_output(int fd, std::string_view output) {
    std::uint64_t dumb = 0;
    if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &dumb) < 0 || !dumb) {
        return false;
    }
    drmModeRes *resources = drmModeGetResources(fd);
    if (!resources) {
        return false;
    }
    drmModeConnector *chosen = nullptr;
    int best = -1;
    for (int i = 0; i < resources->count_connectors; ++i) {
        drmModeConnector *connector = drmModeGetConnector(fd, resources->connectors[i]);
        if (!connector) {
            continue;
        }
        int score = -1;
        if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) {
            score = is_internal(connector->connector_type) ? 2 : 1;
            if (!output.empty() && connector_name(connector) == output) {
                score = 3;
            }
        }
        if (score > best) {
            if (chosen) {
                drmModeFreeConnector(chosen);
            }
            chosen = connector;
            best = score;
        } else {
            drmModeFreeConnector(connector);
        }
    }
    bool ok = false;
    if (chosen) {
        std::uint32_t crtc = find_crtc(fd, resources, chosen);
        if (crtc) {
            mode_ = chosen->modes[0];
            for (int m = 0; m < chosen->count_modes; ++m) {
                if (chosen->modes[m].type & DRM_MODE_TYPE_PREFERRED) {
                    mode_ = chosen->modes[m];
                    break;
                }
            }
            connector_ = chosen->connector_id;
            crtc_ = crtc;
            name_ = connector_name(chosen);
            ok = true;
        }
        drmModeFreeConnector(chosen);
    }
    drmModeFreeResources(resources);
    return ok;
}

bool DisplayService::create_buffer(Buffer &buffer, int width, int height, bool framebuffer) {
    drm_mode_create_dumb create{};
    create.width = static_cast<std::uint32_t>(width);
    create.height = static_cast<std::uint32_t>(height);
    create.bpp = 32;
    if (drmIoctl(fd_, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
        return false;
    }
    buffer.handle = create.handle;
    buffer.pitch = create.pitch;
    buffer.size = create.size;
    if (framebuffer && drmModeAddFB(fd_, create.width, create.height, 24, 32, buffer.pitch, buffer.handle, &buffer.fb) < 0) {
        destroy_buffer(buffer);
        return false;
    }
    drm_mode_map_dumb map{};
    map.handle = buffer.handle;
    if (drmIoctl(fd_, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
        destroy_buffer(buffer);
        return false;
    }
    void *data = mmap(nullptr, buffer.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, static_cast<off_t>(map.offset));
    if (data == MAP_FAILED) {
        destroy_buffer(buffer);
        return false;
    }
    buffer.map = static_cast<std::uint8_t *>(data);
    std::memset(buffer.map, 0, buffer.size);
    return true;
}

void DisplayService::destroy_buffer(Buffer &buffer) {
    if (buffer.map) {
        munmap(buffer.map, buffer.size);
    }
    if (buffer.fb) {
        drmModeRmFB(fd_, buffer.fb);
    }
    if (buffer.handle) {
        drm_mode_destroy_dumb destroy{};
        destroy.handle = buffer.handle;
        drmIoctl(fd_, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    }
    buffer = {};
}

bool DisplayService::modeset() {
    if (drmModeSetCrtc(fd_, crtc_, front_.fb, 0, 0, &connector_, 1, &mode_) < 0) {
        core::error("modeset on {} failed: {}", name_, std::strerror(errno));
        return false;
    }
    if (cursor_shown_) {
        cursor_shown_ = false;
        show_cursor();
    }
    return true;
}

void DisplayService::show_cursor() {
    if (!cursor_ok_ || cursor_shown_) {
        return;
    }
    if (drmModeSetCursor(fd_, crtc_, cursor_.handle, cursor_size, cursor_size) < 0) {
        cursor_ok_ = false;
        return;
    }
    cursor_shown_ = true;
    drmModeMoveCursor(fd_, crtc_, cursor_x_, cursor_y_);
}

int DisplayService::width() const {
    return mode_.hdisplay;
}

int DisplayService::height() const {
    return mode_.vdisplay;
}

void DisplayService::present(const render::Canvas &canvas, const std::vector<render::Rect> &damage) {
    if (!front_.map) {
        return;
    }
    render::Rect screen{0, 0, std::min(width(), canvas.width()), std::min(height(), canvas.height())};
    std::vector<render::Rect> areas;
    areas.reserve(damage.size());
    for (const auto &area : damage) {
        areas.push_back(area.intersected(screen));
    }
    std::vector<drmModeClip> clips;
    for (const auto &rect : render::merge_overlapping(std::move(areas))) {
        for (int y = rect.y; y < rect.bottom(); ++y) {
            auto *dst = front_.map + static_cast<std::size_t>(y) * front_.pitch + static_cast<std::size_t>(rect.x) * 4;
            const auto *src = canvas.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(canvas.stride()) + static_cast<std::size_t>(rect.x) * 4;
            std::memcpy(dst, src, static_cast<std::size_t>(rect.width) * 4);
        }
        clips.push_back({static_cast<std::uint16_t>(rect.x), static_cast<std::uint16_t>(rect.y), static_cast<std::uint16_t>(rect.right()), static_cast<std::uint16_t>(rect.bottom())});
    }
    if (!clips.empty()) {
        drmModeDirtyFB(fd_, front_.fb, clips.data(), static_cast<std::uint32_t>(clips.size()));
    }
}

void DisplayService::move_cursor(int x, int y) {
    cursor_x_ = x;
    cursor_y_ = y;
    if (!cursor_shown_) {
        show_cursor();
        return;
    }
    drmModeMoveCursor(fd_, crtc_, x, y);
}

bool DisplayService::drop_master() {
    if (fd_ < 0) {
        return false;
    }
    if (cursor_shown_) {
        drmModeSetCursor(fd_, crtc_, 0, 0, 0);
    }
    if (managed()) {
        return true;
    }
    if (drmDropMaster(fd_) < 0) {
        core::warn("drmDropMaster failed: {}", std::strerror(errno));
        return false;
    }
    return true;
}

bool DisplayService::acquire_master() {
    if (fd_ < 0) {
        return false;
    }
    if (!managed() && drmSetMaster(fd_) < 0) {
        core::warn("drmSetMaster failed: {}", std::strerror(errno));
    }
    return modeset();
}

void DisplayService::close() {
    if (fd_ < 0) {
        return;
    }
    if (cursor_shown_) {
        drmModeSetCursor(fd_, crtc_, 0, 0, 0);
        cursor_shown_ = false;
    }
    if (saved_crtc_) {
        drmModeSetCrtc(fd_, saved_crtc_->crtc_id, saved_crtc_->buffer_id, saved_crtc_->x, saved_crtc_->y, &connector_, 1, &saved_crtc_->mode);
        drmModeFreeCrtc(saved_crtc_);
        saved_crtc_ = nullptr;
    }
    destroy_buffer(cursor_);
    destroy_buffer(front_);
    if (managed()) {
        seat_->close_device(fd_);
    } else {
        ::close(fd_);
    }
    fd_ = -1;
}

} // namespace service
