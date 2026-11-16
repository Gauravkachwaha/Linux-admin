#pragma once
#include "monitor/types.hpp"
#include <string_view>
namespace monitor {
std::vector<std::string> parse_cmdline(std::string_view text);
std::optional<double> parse_uptime(std::string_view text);
Result<ProcessStat> parse_process_stat(std::string_view text, int expected_pid);
std::optional<unsigned int> parse_uid(std::string_view text);
MemoryFields parse_memory(std::string_view text);
Result<CpuSample> parse_cpu(std::string_view text);
}
