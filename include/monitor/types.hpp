#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace monitor {
using Counter = std::uint64_t;
template<class T> struct Result {
    std::optional<T> value;
    std::string error;
    explicit operator bool() const { return value.has_value(); }
};
enum class Availability { valid, pending, unavailable };
struct Percentage {
    Availability state{Availability::unavailable};
    double value{};
};
struct CpuCounters { std::array<Counter, 8> values{}; };
struct CpuSample {
    // -1 identifies the aggregate; nonnegative keys are logical CPU IDs.
    std::map<int, CpuCounters> cpus;
    std::optional<Counter> boot_time;
};
struct MemoryFields {
    std::optional<Counter> total, available, swap_total, swap_free;
};
struct MemoryUsage { Counter total{}, used{}, free{}; };
struct MemorySnapshot { Result<MemoryUsage> ram, swap; };
}
