#pragma once
#include <array>
#include <chrono>
#include <compare>
#include <vector>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace monitor {
using Clock = std::chrono::steady_clock;
using Counter = std::uint64_t;
template<class T> struct Result {
    std::optional<T> value;
    std::string error;
    int error_number{};
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
struct ProcessIdentity {
    int pid{};
    Counter start_ticks{};
    auto operator<=>(const ProcessIdentity&) const = default;
};
struct ProcessStat {
    ProcessIdentity identity;
    int ppid{};
    std::string name;
    char state{};
    Counter user_ticks{}, system_ticks{}, threads{};
    std::int64_t rss_pages{};
};
struct ProcessRecord {
    ProcessStat stat;
    std::optional<unsigned int> uid;
    std::optional<Counter> rss_bytes;
    Clock::time_point timestamp;
};
struct SystemConstants { long ticks{}, page_size{}; };
struct ProcessRow {
    ProcessRecord record;
    std::string user;
    Percentage cpu;
};
struct ProcessSnapshot {
    std::vector<ProcessRow> rows;
    std::string error;
};
enum class DetailState { valid, gone, unavailable, malformed };
struct ProcessDetails {
    ProcessIdentity identity;
    DetailState state{DetailState::unavailable};
    std::vector<std::string> arguments;
    std::string start_time{"unavailable"};
    std::string error;
};
}
