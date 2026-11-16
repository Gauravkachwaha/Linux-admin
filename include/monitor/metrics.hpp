#pragma once
#include "monitor/types.hpp"
#include <ctime>
namespace monitor {
std::optional<std::time_t> start_epoch(Counter start_ticks, long ticks, double boot_epoch);
std::optional<double> approximate_boot(double realtime, double uptime);
std::string format_start(std::time_t epoch);
Percentage process_cpu(const ProcessRecord& current, const ProcessRecord* previous, long ticks);
bool valid_constants(SystemConstants constants);
std::optional<Counter> rss_bytes(std::int64_t pages, long page_size);
MemorySnapshot memory_usage(const MemoryFields& fields);
Percentage cpu_usage(const CpuCounters& current, const CpuCounters* previous);
}
