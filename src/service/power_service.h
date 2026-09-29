#pragma once

namespace service {

class PowerService {
  public:
    bool reboot();
    bool power_off();
    bool suspend();
};

} // namespace service
