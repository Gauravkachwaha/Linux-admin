#include "monitor/parsing.hpp"
#include <charconv>
#include <sstream>
#include <limits>
#include <set>
#include <cmath>

namespace monitor {
namespace {
template<class T> bool number(std::string_view text, T& value) {
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return !text.empty() && error == std::errc{} && end == text.data() + text.size();
}
}
Result<CpuSample> parse_cpu(std::string_view text) {
    CpuSample sample;
    std::istringstream input{std::string(text)};
    for (std::string line; std::getline(input, line);) {
        std::istringstream fields(line);
        std::string key;
        fields >> key;
        if (key == "btime") {
            std::string token;
            Counter value{};
            if (fields >> token && number(token, value)) sample.boot_time = value;
            continue;
        }
        if (!key.starts_with("cpu")) continue;
        int id = -1;
        if (key != "cpu" && (!number(std::string_view(key).substr(3), id) || id < 0))
            return {{}, "invalid CPU ID"};
        CpuCounters counters;
        for (auto& value : counters.values) {
            std::string token;
            if (!(fields >> token) || !number(token, value)) return {{}, "missing/invalid CPU counter"};
        }
        if (!sample.cpus.emplace(id, counters).second) return {{}, "duplicate CPU ID"};
    }
    if (!sample.cpus.contains(-1)) return {{}, "missing aggregate CPU"};
    return {sample, {}};
}
MemoryFields parse_memory(std::string_view text) {
    MemoryFields result;
    std::set<std::string> seen;
    std::istringstream input{std::string(text)};
    for (std::string line; std::getline(input, line);) {
        std::istringstream fields(line);
        std::string key, token, unit, extra;
        fields >> key;
        std::optional<Counter>* destination = nullptr;
        if (key == "MemTotal:") destination = &result.total;
        if (key == "MemAvailable:") destination = &result.available;
        if (key == "SwapTotal:") destination = &result.swap_total;
        if (key == "SwapFree:") destination = &result.swap_free;
        if (!destination) continue;
        Counter value{};
        if (!seen.insert(key).second || !(fields >> token >> unit) || unit != "kB" ||
            fields >> extra || !number(token, value) || value > std::numeric_limits<Counter>::max()/1024) {
            destination->reset();
            continue;
        }
        *destination = value * 1024;
    }
    return result;
}
Result<ProcessStat> parse_process_stat(std::string_view text, int expected_pid) {
    const auto open = text.find('('), close = text.rfind(')');
    if (open == std::string_view::npos || close == std::string_view::npos || close <= open)
        return {{}, "invalid process name delimiters"};
    auto prefix = text.substr(0, open);
    while (!prefix.empty() && prefix.back() == ' ') prefix.remove_suffix(1);
    ProcessStat result;
    if (!number(prefix, result.identity.pid) || result.identity.pid <= 0 || result.identity.pid != expected_pid)
        return {{}, "PID mismatch"};
    result.name = text.substr(open + 1, close - open - 1);
    std::istringstream suffix{std::string(text.substr(close + 1))};
    std::vector<std::string> fields;
    for (std::string token; suffix >> token;) fields.push_back(std::move(token));
    if (fields.size() < 22 || fields[0].size() != 1 ||
        !number(fields[1], result.ppid) || result.ppid < 0 ||
        !number(fields[11], result.user_ticks) || !number(fields[12], result.system_ticks) ||
        !number(fields[17], result.threads) || !number(fields[19], result.identity.start_ticks) ||
        !number(fields[21], result.rss_pages)) return {{}, "missing/invalid process fields"};
    result.state = fields[0][0];
    return {std::move(result), {}};
}
std::optional<unsigned int> parse_uid(std::string_view text) {
    std::istringstream input{std::string(text)};
    for (std::string line; std::getline(input, line);) {
        std::istringstream fields(line);
        std::string key, real, effective;
        fields >> key;
        if (key != "Uid:") continue;
        unsigned int value{};
        if (fields >> real >> effective && number(effective, value)) return value;
        return {};
    }
    return {};
}
std::vector<std::string> parse_cmdline(std::string_view text) {
    std::vector<std::string> arguments;
    while (!text.empty()) {
        const auto end = text.find('\0');
        if (end == std::string_view::npos) { arguments.emplace_back(text); break; }
        arguments.emplace_back(text.substr(0,end));
        text.remove_prefix(end+1);
    }
    return arguments;
}
std::optional<double> parse_uptime(std::string_view text) {
    const auto end = text.find_first_of(" \t\r\n");
    double value{};
    if (!number(text.substr(0,end),value) || !std::isfinite(value) || value < 0) return {};
    return value;
}
}
