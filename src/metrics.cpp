#include "monitor/metrics.hpp"
#include <limits>
#include <cmath>
namespace monitor {
Percentage cpu_usage(const CpuCounters& current, const CpuCounters* previous) {
    Counter total{}, old_total{};
    for (std::size_t i = 0; i < current.values.size(); ++i) {
        const auto now = current.values[i];
        if (now > std::numeric_limits<Counter>::max() - total) return {};
        total += now;
        if (previous) {
            const auto old = previous->values[i];
            if (now < old || old > std::numeric_limits<Counter>::max() - old_total) return {};
            old_total += old;
        }
    }
    if (!previous) return {Availability::pending, 0};
    const auto elapsed = total - old_total;
    const auto idle = (current.values[3] - previous->values[3]) +
                      (current.values[4] - previous->values[4]);
    if (elapsed == 0 || idle > elapsed) return {};
    return {Availability::valid, 100.0 * static_cast<double>(elapsed - idle) / static_cast<double>(elapsed)};
}
MemorySnapshot memory_usage(const MemoryFields& fields) {
    const auto group = [](std::optional<Counter> total, std::optional<Counter> free) -> Result<MemoryUsage> {
        if (!total || !free) return {{}, "missing/invalid memory field"};
        if (*free > *total) return {{}, "free/available exceeds total"};
        return {MemoryUsage{*total, *total - *free, *free}, {}};
    };
    return {group(fields.total, fields.available), group(fields.swap_total, fields.swap_free)};
}
bool valid_constants(SystemConstants constants) { return constants.ticks > 0 && constants.page_size > 0; }
std::optional<Counter> rss_bytes(std::int64_t pages, long page_size) {
    if (pages < 0 || page_size <= 0) return {};
    const auto count = static_cast<Counter>(pages), size = static_cast<Counter>(page_size);
    if (count > std::numeric_limits<Counter>::max() / size) return {};
    return count * size;
}
Percentage process_cpu(const ProcessRecord& current, const ProcessRecord* previous, long ticks) {
    const auto& now = current.stat;
    if (ticks <= 0 || now.user_ticks > std::numeric_limits<Counter>::max() - now.system_ticks) return {};
    if (!previous || previous->stat.identity != now.identity) return {Availability::pending, 0};
    const auto& old = previous->stat;
    if (now.user_ticks < old.user_ticks || now.system_ticks < old.system_ticks ||
        old.user_ticks > std::numeric_limits<Counter>::max() - old.system_ticks) return {};
    const double elapsed = std::chrono::duration<double>(current.timestamp - previous->timestamp).count();
    if (elapsed <= 0 || !std::isfinite(elapsed)) return {};
    const auto delta = (now.user_ticks - old.user_ticks) + (now.system_ticks - old.system_ticks);
    const double value = 100.0 * (static_cast<double>(delta) / static_cast<double>(ticks)) / elapsed;
    if (!std::isfinite(value)) return {};
    return {Availability::valid, value};
}
}
