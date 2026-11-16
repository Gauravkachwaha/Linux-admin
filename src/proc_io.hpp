#pragma once
#include "monitor/types.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <utility>

namespace monitor::detail {
class FileDescriptor {
public:
    explicit FileDescriptor(int value) : value_(value) {}
    ~FileDescriptor() { if (value_ >= 0) ::close(value_); }
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    int get() const { return value_; }
private:
    int value_;
};
inline Result<std::string> read_file(const std::string& path, int directory = AT_FDCWD) {
    FileDescriptor fd(::openat(directory, path.c_str(), O_RDONLY | O_CLOEXEC));
    if (fd.get() < 0) return {{}, path + ": " + std::strerror(errno), errno};
    std::string text;
    char buffer[4096];
    for (;;) {
        const auto size = ::read(fd.get(), buffer, sizeof(buffer));
        if (size == 0) return {std::move(text), {}};
        if (size < 0) {
            if (errno == EINTR) continue;
            return {{}, path + ": " + std::strerror(errno), errno};
        }
        text.append(buffer, static_cast<std::size_t>(size));
    }
}
}
