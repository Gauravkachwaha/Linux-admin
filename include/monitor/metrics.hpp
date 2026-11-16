#pragma once
#include "monitor/types.hpp"
namespace monitor {
bool valid_constants(SystemConstants constants);
std::optional<Counter> rss_bytes(std::int64_t pages, long page_size);
MemorySnapshot memory_usage(const MemoryFields& fields);
Percentage cpu_usage(const CpuCounters& current, const CpuCounters* previous);
}
