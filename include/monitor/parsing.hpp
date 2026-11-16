#pragma once
#include "monitor/types.hpp"
#include <string_view>
namespace monitor {
Result<CpuSample> parse_cpu(std::string_view text);
}
