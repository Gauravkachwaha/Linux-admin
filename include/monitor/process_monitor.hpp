#pragma once
#include "monitor/types.hpp"
#include <csignal>
namespace monitor {
SystemConstants system_constants();
std::string resolve_user(unsigned int uid);
class ProcessMonitor {
public:
    explicit ProcessMonitor(SystemConstants constants, std::string root = "/proc");
    Result<std::vector<ProcessRecord>> collect(const volatile std::sig_atomic_t* stop = nullptr) const;
    ProcessSnapshot sample(const volatile std::sig_atomic_t* stop = nullptr);
    ProcessSnapshot update(const Result<std::vector<ProcessRecord>>& records);
    void reset() { previous_.clear(); }
private:
    std::map<ProcessIdentity, ProcessRecord> previous_;
    std::map<unsigned int, std::string> users_;
    SystemConstants constants_;
    std::string root_;
};
}
