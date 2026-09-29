#include "core/process.h"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

#include "core/strings.h"

namespace core {

void Environment::set(std::string_view key, std::string_view value) {
    vars_.insert_or_assign(std::string(key), std::string(value));
}

void Environment::set_entry(std::string_view entry) {
    auto equals = entry.find('=');
    if (equals == std::string_view::npos || equals == 0) {
        return;
    }
    set(entry.substr(0, equals), entry.substr(equals + 1));
}

std::optional<std::string> Environment::get(std::string_view key) const {
    auto found = vars_.find(key);
    if (found == vars_.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::vector<std::string> Environment::entries() const {
    std::vector<std::string> out;
    out.reserve(vars_.size());
    for (const auto &[key, value] : vars_) {
        out.push_back(key + "=" + value);
    }
    return out;
}

const std::map<std::string, std::string, std::less<>> &Environment::vars() const {
    return vars_;
}

void reset_signal_state() {
    sigset_t mask;
    sigemptyset(&mask);
    sigprocmask(SIG_SETMASK, &mask, nullptr);
    for (int signal = 1; signal < NSIG; ++signal) {
        if (signal != SIGKILL && signal != SIGSTOP) {
            std::signal(signal, SIG_DFL);
        }
    }
}

void exec_command(const std::vector<std::string> &argv, const Environment &env) {
    if (argv.empty()) {
        _exit(127);
    }
    clearenv();
    for (const auto &[key, value] : env.vars()) {
        setenv(key.c_str(), value.c_str(), 1);
    }
    std::vector<char *> args;
    args.reserve(argv.size() + 1);
    for (const auto &arg : argv) {
        args.push_back(const_cast<char *>(arg.c_str()));
    }
    args.push_back(nullptr);
    execvp(args[0], args.data());
    _exit(127);
}

pid_t spawn(const std::vector<std::string> &argv, const Environment &env) {
    pid_t pid = fork();
    if (pid == 0) {
        reset_signal_state();
        exec_command(argv, env);
    }
    return pid;
}

int wait_for(pid_t pid) {
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            return -1;
        }
    }
    return status;
}

std::optional<std::filesystem::path> find_executable(std::string_view name, std::string_view path_list) {
    if (name.empty()) {
        return std::nullopt;
    }
    if (name.find('/') != std::string_view::npos) {
        std::string path(name);
        return access(path.c_str(), X_OK) == 0 ? std::optional<std::filesystem::path>(path) : std::nullopt;
    }
    for (const auto &dir : split(path_list, ":")) {
        auto candidate = std::filesystem::path(dir) / name;
        if (access(candidate.c_str(), X_OK) == 0) {
            return candidate;
        }
    }
    return std::nullopt;
}

} // namespace core
