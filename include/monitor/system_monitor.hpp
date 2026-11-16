#pragma once
#include "monitor/types.hpp"
#include <chrono>
namespace monitor {
using Clock = std::chrono::steady_clock;
struct SystemSnapshot {
    std::map<int, Percentage> cpus;
    MemorySnapshot memory;
    std::optional<Counter> boot_time;
    Clock::time_point timestamp;
    std::string cpu_error, memory_error;
};
class SystemMonitor {
public:
    SystemSnapshot sample();
    SystemSnapshot update(const Result<std::string>& cpu, const Result<std::string>& memory,
                          Clock::time_point timestamp);
    void reset() { previous_.clear(); }
private:
    std::map<int, CpuCounters> previous_;
};
}
