#include "monitor/process_monitor.hpp"
#include "monitor/parsing.hpp"
#include "monitor/metrics.hpp"
#include "proc_io.hpp"
#include <charconv>
#include <dirent.h>
#include <memory>
#include <stdexcept>
#include <pwd.h>

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
std::string resolve_user(unsigned int uid) {
    std::vector<char> buffer(1024);
    passwd entry{};
    passwd* result = nullptr;
    for (;;) {
        const int status = ::getpwuid_r(uid, &entry, buffer.data(), buffer.size(), &result);
        if (status == 0 && result && result->pw_name) return result->pw_name;
        if (status != ERANGE || buffer.size() >= 1024*1024) return std::to_string(uid);
        buffer.resize(buffer.size()*2);
    }
}
ProcessSnapshot ProcessMonitor::sample(const volatile std::sig_atomic_t* stop) { return update(collect(stop)); }
ProcessSnapshot ProcessMonitor::update(const Result<std::vector<ProcessRecord>>& records) {
    ProcessSnapshot snapshot;
    std::map<ProcessIdentity, ProcessRecord> next;
    std::map<unsigned int, std::string> users;
    if (!records) snapshot.error = records.error;
    else for (const auto& record : *records.value) {
        const auto old = previous_.find(record.stat.identity);
        const auto cpu = process_cpu(record, old == previous_.end() ? nullptr : &old->second, constants_.ticks);
        std::string user = "unavailable";
        if (record.uid) {
            auto cached = users_.find(*record.uid);
            if (cached != users_.end()) user = cached->second;
            else if (users.contains(*record.uid)) user = users.at(*record.uid);
            else user = resolve_user(*record.uid);
            users.emplace(*record.uid, user);
        }
        snapshot.rows.push_back({record, std::move(user), cpu});
        if (cpu.state != Availability::unavailable) next.emplace(record.stat.identity, record);
    }
    previous_ = std::move(next);
    users_ = std::move(users);
    return snapshot;
}
}
