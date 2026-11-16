#pragma once
#include "monitor/types.hpp"
namespace monitor {
MemorySnapshot memory_usage(const MemoryFields& fields);
Percentage cpu_usage(const CpuCounters& current, const CpuCounters* previous);
}
