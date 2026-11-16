#pragma once
#include "monitor/types.hpp"
#include <csignal>
namespace monitor {
SystemConstants system_constants();
class ProcessMonitor {
public:
    explicit ProcessMonitor(SystemConstants constants, std::string root = "/proc");
    Result<std::vector<ProcessRecord>> collect(const volatile std::sig_atomic_t* stop = nullptr) const;
private:
    SystemConstants constants_;
    std::string root_;
};
}
