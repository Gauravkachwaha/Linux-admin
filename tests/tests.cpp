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
int main() {
    try {
        cpu_tests();
        std::cout << "All checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
