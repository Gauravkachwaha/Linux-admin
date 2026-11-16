#include "monitor/parsing.hpp"
#include <charconv>
#include <sstream>

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
}
