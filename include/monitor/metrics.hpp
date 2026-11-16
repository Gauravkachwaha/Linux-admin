#pragma once
#include "monitor/types.hpp"
namespace monitor {
Percentage cpu_usage(const CpuCounters& current, const CpuCounters* previous);
}
