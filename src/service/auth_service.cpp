#include "service/auth_service.h"

#include <cstdlib>
#include <cstring>

#include "core/log.h"

namespace service {

namespace {

char *duplicate(std::string_view text) {
    auto *out = static_cast<char *>(std::malloc(text.size() + 1));
    if (!out) {
        return nullptr;
    }
    std::memcpy(out, text.data(), text.size());
    out[text.size()] = '\0';
    return out;
}

} // namespace

AuthService::~AuthService() {
    end(false);
}

int AuthService::converse(int count, const pam_message **messages, pam_response **responses, void *data) {
    auto *self = static_cast<AuthService *>(data);
    auto *replies = static_cast<pam_response *>(std::calloc(static_cast<std::size_t>(count), sizeof(pam_response)));
    if (!replies) {
        return PAM_BUF_ERR;
    }
    for (int i = 0; i < count; ++i) {
        switch (messages[i]->msg_style) {
        case PAM_PROMPT_ECHO_OFF:
            replies[i].resp = duplicate(self->password_);
            break;
        case PAM_PROMPT_ECHO_ON:
            replies[i].resp = duplicate(self->user_);
            break;
        case PAM_ERROR_MSG:
        case PAM_TEXT_INFO:
            self->last_message_ = messages[i]->msg ? messages[i]->msg : "";
            break;
        default:
            break;
        }
    }
    *responses = replies;
    return PAM_SUCCESS;
}

bool AuthService::start(std::string_view service, std::string_view user, std::string_view tty) {
    user_ = user;
    conversation_ = pam_conv{&AuthService::converse, this};
    std::string service_name(service);
    int result = pam_start(service_name.c_str(), user_.c_str(), &conversation_, &handle_);
    if (result != PAM_SUCCESS) {
        core::error("pam_start({}) failed: {}", service_name, pam_strerror(handle_, result));
        handle_ = nullptr;
        return false;
    }
    std::string tty_name(tty);
    pam_set_item(handle_, PAM_TTY, tty_name.c_str());
    return true;
}

std::expected<void, std::string> AuthService::authenticate(std::string_view password) {
    if (!handle_) {
        return std::unexpected("Authentication is unavailable");
    }
    password_ = password;
    last_message_.clear();
    int result = pam_authenticate(handle_, 0);
    if (result == PAM_SUCCESS) {
        result = pam_acct_mgmt(handle_, 0);
    }
    password_ = {};
    if (result == PAM_NEW_AUTHTOK_REQD) {
        return std::unexpected("Password expired, change it from a console");
    }
    if (result != PAM_SUCCESS) {
        return std::unexpected(last_message_.empty() ? std::string(pam_strerror(handle_, result)) : last_message_);
    }
    return {};
}

bool AuthService::put_env(std::string_view key, std::string_view value) {
    std::string entry = std::string(key) + "=" + std::string(value);
    return handle_ && pam_putenv(handle_, entry.c_str()) == PAM_SUCCESS;
}

bool AuthService::open_session() {
    if (!handle_) {
        return false;
    }
    int result = pam_setcred(handle_, PAM_ESTABLISH_CRED);
    if (result != PAM_SUCCESS) {
        core::error("pam_setcred failed: {}", pam_strerror(handle_, result));
        return false;
    }
    result = pam_open_session(handle_, 0);
    if (result != PAM_SUCCESS) {
        core::error("pam_open_session failed: {}", pam_strerror(handle_, result));
        pam_setcred(handle_, PAM_DELETE_CRED);
        return false;
    }
    session_open_ = true;
    return true;
}

void AuthService::close_session() {
    if (!handle_ || !session_open_) {
        return;
    }
    pam_close_session(handle_, 0);
    pam_setcred(handle_, PAM_DELETE_CRED);
    session_open_ = false;
}

std::vector<std::string> AuthService::environment() const {
    std::vector<std::string> out;
    if (!handle_) {
        return out;
    }
    char **list = pam_getenvlist(handle_);
    if (!list) {
        return out;
    }
    for (char **entry = list; *entry; ++entry) {
        out.emplace_back(*entry);
        std::free(*entry);
    }
    std::free(list);
    return out;
}

void AuthService::end(bool silent) {
    if (!handle_) {
        return;
    }
    pam_end(handle_, silent ? (PAM_SUCCESS | PAM_DATA_SILENT) : PAM_SUCCESS);
    handle_ = nullptr;
    session_open_ = false;
}

} // namespace service
