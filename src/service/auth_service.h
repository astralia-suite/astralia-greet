#pragma once

#include <expected>
#include <security/pam_appl.h>
#include <string>
#include <string_view>
#include <vector>

namespace service {

class AuthService {
  public:
    AuthService() = default;
    ~AuthService();
    AuthService(const AuthService &) = delete;
    AuthService &operator=(const AuthService &) = delete;

    bool start(std::string_view service, std::string_view user, std::string_view tty);
    std::expected<void, std::string> authenticate(std::string_view password);
    bool put_env(std::string_view key, std::string_view value);
    bool open_session();
    void close_session();
    std::vector<std::string> environment() const;
    void end(bool silent);

  private:
    static int converse(int count, const pam_message **messages, pam_response **responses, void *data);

    pam_handle_t *handle_ = nullptr;
    pam_conv conversation_{};
    std::string user_;
    std::string_view password_;
    std::string last_message_;
    bool session_open_ = false;
};

} // namespace service
