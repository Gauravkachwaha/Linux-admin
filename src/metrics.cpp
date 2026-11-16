#include "monitor/metrics.hpp"
#include <limits>
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
}
