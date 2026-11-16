#include "monitor/presentation.hpp"
#include <iomanip>
#include <sstream>
namespace monitor {
std::string safe_text(std::string_view text) {
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    for (char ch : text) {
        const auto byte = static_cast<unsigned char>(ch);
        if (byte >= 32 && byte < 127) result += ch;
        else { result += "\\x"; result += hex[byte >> 4]; result += hex[byte & 15]; }
    }
    return result;
}
std::string format_bytes(Counter bytes) {
    constexpr const char* units[] = {"B","KiB","MiB","GiB","TiB","PiB","EiB"};
    double value = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (value >= 1024 && unit < 6) { value /= 1024; ++unit; }
    std::ostringstream output;
    output << std::fixed << std::setprecision(unit == 0 ? 0 : 1) << value << ' ' << units[unit];
    return output.str();
}
std::string format_percentage(Percentage value) {
    if (value.state == Availability::pending) return "pending";
    if (value.state != Availability::valid) return "n/a";
    std::ostringstream output;
    output << std::fixed << std::setprecision(1) << value.value << '%';
    return output.str();
}
std::string format_arguments(const std::vector<std::string>& arguments) {
    if (arguments.empty()) return "(empty command line)";
    std::string text;
    for (const auto& argument : arguments) {
        if (!text.empty()) text += ' ';
        text += '"';
        for (char ch : safe_text(argument)) {
            if (ch == '"' || ch == '\\') text += '\\';
            text += ch;
        }
        text += '"';
    }
    return text;
}
std::vector<std::string> wrap_text(std::string_view text, std::size_t width) {
    std::vector<std::string> lines;
    if (width == 0) return lines;
    while (!text.empty()) {
        lines.emplace_back(text.substr(0,width));
        text.remove_prefix(std::min(width,text.size()));
    }
    return lines;
}
}
