#pragma once
#include "monitor/types.hpp"
#include <string_view>
namespace monitor {
Result<ProcessStat> parse_process_stat(std::string_view text, int expected_pid);
std::optional<unsigned int> parse_uid(std::string_view text);
MemoryFields parse_memory(std::string_view text);
Result<CpuSample> parse_cpu(std::string_view text);
}
