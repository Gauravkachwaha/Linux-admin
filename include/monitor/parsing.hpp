#pragma once
#include "monitor/types.hpp"
#include <string_view>
namespace monitor {
MemoryFields parse_memory(std::string_view text);
Result<CpuSample> parse_cpu(std::string_view text);
}
