#pragma once
#include "monitor/types.hpp"
#include <string_view>
namespace monitor {
std::string safe_text(std::string_view text);
std::string format_bytes(Counter bytes);
std::string format_percentage(Percentage value);
std::string format_arguments(const std::vector<std::string>& arguments);
std::vector<std::string> wrap_text(std::string_view text, std::size_t width);
}
