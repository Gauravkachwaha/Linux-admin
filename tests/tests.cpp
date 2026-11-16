#include "monitor/system_monitor.hpp"
#include <fstream>
#include <iterator>
#include "monitor/parsing.hpp"
#include "monitor/metrics.hpp"
#include <cmath>
#include <limits>
#include <iostream>
#include <stdexcept>
#include <string_view>

void check(bool condition, std::string_view description) {
    if (!condition) throw std::runtime_error(std::string(description));
}
void cpu_tests() {
    using namespace monitor;
    auto a = parse_cpu("cpu 10 0 10 80 0 0 0 0 5 0\ncpu7 1 0 0 9 0 0 0 0\nbtime 100\n");
    auto b = parse_cpu("cpu7 2 0 0 18 0 0 0 0\ncpu 30 0 20 140 10 0 0 0 25 0\ncpu2 0 0 0 0 0 0 0 0\n");
    check(a && b && a.value->boot_time == 100, "CPU parsing and boot time");
    check(std::abs(cpu_usage(b.value->cpus.at(-1), &a.value->cpus.at(-1)).value - 30.0) < 0.001, "CPU 30%, guest excluded, iowait idle");
    check(cpu_usage(b.value->cpus.at(7), &a.value->cpus.at(7)).value == 10, "cores matched by ID");
    check(cpu_usage(b.value->cpus.at(2), nullptr).state == Availability::pending, "new core pending");
    check(cpu_usage(a.value->cpus.at(-1), &a.value->cpus.at(-1)).state == Availability::unavailable, "zero delta");
    check(cpu_usage(a.value->cpus.at(-1), &b.value->cpus.at(-1)).state == Availability::unavailable, "regression");
    auto huge = CpuCounters{{std::numeric_limits<Counter>::max(),1,0,0,0,0,0,0}};
    check(cpu_usage(huge, nullptr).state == Availability::unavailable, "counter overflow");
    check(!parse_cpu("cpu 1 2 3"), "missing counters");
    check(!parse_cpu("cpu x 0 0 0 0 0 0 0"), "malformed counter");
    check(!parse_cpu("cpu -1 0 0 0 0 0 0 0"), "negative counter");
    check(!parse_cpu("cpu0 0 0 0 0 0 0 0 0"), "missing aggregate");
    check(!parse_cpu("cpu 18446744073709551616 0 0 0 0 0 0 0"), "parse overflow");
    check(!parse_cpu("cpu 0 0 0 0 0 0 0 0\ncpu 0 0 0 0 0 0 0 0"), "duplicate core");
    check(a.value->cpus.size() == 2 && b.value->cpus.size() == 3, "core changes");
}
void memory_tests() {
    using namespace monitor;
    auto m = memory_usage(parse_memory("MemTotal: 100 kB\nMemAvailable: 40 kB\nSwapTotal: 20 kB\nSwapFree: 5 kB\n"));
    check(m.ram && m.swap && m.ram.value->used == 60*1024 && m.swap.value->used == 15*1024, "memory calculations");
    m = memory_usage(parse_memory("SwapTotal: 0 kB\nSwapFree: 0 kB"));
    check(!m.ram && m.swap && m.swap.value->used == 0, "independent no swap");
    m = memory_usage(parse_memory("MemTotal: 100 kB\nMemAvailable: 40 kB\nSwapTotal: 1 kB\nSwapFree: 2 kB"));
    check(m.ram && !m.swap, "invalid swap independent");
    for (auto bad : {"-1", "oops", "18446744073709551616", "18446744073709551615"})
        check(!parse_memory(std::string("MemTotal: ")+bad+" kB").total, "invalid memory number");
    check(!parse_memory("MemTotal: 10 MB").total, "wrong unit");
    check(!parse_memory("MemTotal: 10 kB extra").total, "unexpected suffix");
    check(!parse_memory("MemTotal: 10 kB\nMemTotal: 20 kB").total, "duplicate field");
    check(!memory_usage(parse_memory("MemTotal: 1 kB\nMemAvailable: 2 kB")).ram, "invalid available");
}
void system_tests() {
    using namespace monitor;
    SystemMonitor monitor;
    Result<std::string> cpu{"cpu 1 0 0 9 0 0 0 0\ncpu5 1 0 0 9 0 0 0 0", {}};
    Result<std::string> mem{"MemTotal: 100 kB\nMemAvailable: 20 kB", {}};
    auto a = monitor.update(cpu, mem, {});
    check(a.cpus.at(-1).state == Availability::pending && a.memory.ram, "first system sample");
    cpu.value = "cpu 2 0 0 18 0 0 0 0";
    auto b = monitor.update(cpu, mem, {});
    check(b.cpus.size() == 1 && b.cpus.at(-1).value == 10, "core pruning/system delta");
    b = monitor.update({{}, "denied"}, mem, {});
    check(b.cpus.empty() && b.cpu_error == "denied" && b.memory.ram, "isolated source failure");
    b = monitor.update(cpu, {{}, "gone"}, {});
    check(b.cpus.at(-1).state == Availability::pending && !b.memory.ram, "recovery fresh baseline");
    b = monitor.update(cpu, mem, {});
    check(b.cpus.at(-1).state == Availability::unavailable, "invalid pair");
    b = monitor.update(cpu, mem, {});
    check(b.cpus.at(-1).state == Availability::pending, "invalid pair resets baseline");
}
void live_tests() {
    using namespace monitor;
    SystemMonitor monitor;
    auto a = monitor.sample();
    auto b = monitor.sample();
    check(a.cpu_error.empty() && b.cpu_error.empty() && a.memory.ram, "live system sources");
    std::ifstream input("/proc/meminfo");
    std::string raw{std::istreambuf_iterator<char>(input), {}};
    check(a.memory.ram.value->total == parse_memory(raw).total, "live total memory agrees with source");
    check(!b.cpus.empty(), "live CPU enumeration");
}
int main(int argc, char**) {
    try {
        cpu_tests();
        memory_tests();
        system_tests();
        if (argc > 1) live_tests();
        std::cout << "All checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
