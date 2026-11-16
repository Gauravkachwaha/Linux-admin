#include "monitor/process_monitor.hpp"
#include "monitor/parsing.hpp"
#include "monitor/metrics.hpp"
#include "proc_io.hpp"
#include <charconv>
#include <dirent.h>
#include <memory>
#include <stdexcept>

namespace monitor {
SystemConstants system_constants() {
    const SystemConstants constants{::sysconf(_SC_CLK_TCK), ::sysconf(_SC_PAGESIZE)};
    if (!valid_constants(constants)) throw std::runtime_error("cannot obtain valid kernel tick/page constants");
    return constants;
}
ProcessMonitor::ProcessMonitor(SystemConstants constants, std::string root)
    : constants_(constants), root_(std::move(root)) {
    if (!valid_constants(constants_)) throw std::invalid_argument("invalid kernel constants");
}
Result<std::vector<ProcessRecord>> ProcessMonitor::collect(const volatile std::sig_atomic_t* stop) const {
    struct CloseDirectory { void operator()(DIR* dir) const { ::closedir(dir); } };
    std::unique_ptr<DIR, CloseDirectory> directory(::opendir(root_.c_str()));
    if (!directory) return {{}, root_ + ": " + std::strerror(errno)};
    std::vector<ProcessRecord> records;
    for (;;) {
        if (stop && *stop) break;
        errno = 0;
        auto* entry = ::readdir(directory.get());
        if (!entry) {
            if (errno != 0) return {{}, "process enumeration: " + std::string(std::strerror(errno))};
            break;
        }
        std::string_view name(entry->d_name);
        if (name.empty() || name.find_first_not_of("0123456789") != std::string_view::npos) continue;
        int pid{};
        const auto [end, error] = std::from_chars(name.data(), name.data()+name.size(), pid);
        if (error != std::errc{} || end != name.data()+name.size() || pid <= 0) continue;
        detail::FileDescriptor process(::openat(::dirfd(directory.get()), entry->d_name,
                                               O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
        if (process.get() < 0) continue;
        const auto text = detail::read_file("stat", process.get());
        const auto timestamp = Clock::now();
        if (!text) continue;
        const auto stat = parse_process_stat(*text.value, pid);
        if (!stat) continue;
        const auto status = detail::read_file("status", process.get());
        const auto verify = detail::read_file("stat", process.get());
        if (!verify) continue;
        const auto identity = parse_process_stat(*verify.value, pid);
        if (!identity || identity.value->identity != stat.value->identity) continue;
        records.push_back({*stat.value, status ? parse_uid(*status.value) : std::nullopt,
                           rss_bytes(stat.value->rss_pages, constants_.page_size), timestamp});
    }
    return {std::move(records), {}};
}
}
