#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "core/bus.h"
#include "core/event_loop.h"

namespace service {

struct BluetoothEntry {
    std::string path;
    std::string name;
    bool connected = false;
    bool connecting = false;

    bool operator==(const BluetoothEntry &) const = default;
};

struct BluetoothSnapshot {
    bool available = false;
    bool powered = false;
    std::vector<BluetoothEntry> entries;

    bool operator==(const BluetoothSnapshot &) const = default;
};

std::string device_label(std::string_view alias, std::string_view name, std::string_view address);

class BluetoothService {
  public:
    explicit BluetoothService(core::EventLoop &loop);
    ~BluetoothService();
    BluetoothService(const BluetoothService &) = delete;
    BluetoothService &operator=(const BluetoothService &) = delete;

    bool start();
    const BluetoothSnapshot &snapshot() const;
    void set_powered(bool powered);
    void toggle_device(const std::string &path);

    std::function<void()> on_change;

  private:
    void schedule_refresh();
    void refresh(bool notify);
    bool query(BluetoothSnapshot &out);

    core::EventLoop &loop_;
    core::Bus bus_;
    BluetoothSnapshot snapshot_;
    std::string adapter_path_;
    std::string pending_path_;
    int refresh_timer_ = -1;
};

} // namespace service
