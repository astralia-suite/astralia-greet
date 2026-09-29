#pragma once

#include <string>

namespace service {

class VtService {
  public:
    ~VtService();

    static int find_free();
    bool open(int number);
    int number() const;
    std::string path() const;
    bool activate();
    bool enter_graphics(int release_signal, int acquire_signal);
    void leave_graphics();
    bool hide_text();
    void show_text();
    bool take_as_stdin();
    void ack_release();
    void ack_acquire();
    bool switch_to(int number);
    void close();

  private:
    int fd_ = -1;
    int number_ = 0;
    int saved_keyboard_mode_ = 0;
    bool graphics_ = false;
};

} // namespace service
