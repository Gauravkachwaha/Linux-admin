#include "monitor/process_monitor.hpp"
#include <filesystem>
#include <unistd.h>
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
std::string process_fixture(std::string name = "a (b) c)", int pid = 42, unsigned start = 100) {
    // Fields 3..24; irrelevant fields deliberately remain zero.
    return std::to_string(pid) + " (" + name + ") S 1 0 0 0 0 0 0 0 0 0 20 10 0 0 0 0 2 0 " +
           std::to_string(start) + " 4096 3";
}
void process_parser_tests() {
    using namespace monitor;
    auto p = parse_process_stat(process_fixture(), 42);
    check(p && p.value->name == "a (b) c)" && p.value->ppid == 1 && p.value->threads == 2, "complex process name and columns");
    check(p.value->user_ticks == 20 && p.value->system_ticks == 10 && p.value->rss_pages == 3, "process stat positions");
    check(!parse_process_stat(process_fixture(), 43), "PID mismatch");
    check(parse_process_stat(process_fixture()+" 0 0 0",42).value.has_value(), "trailing stat fields");
    check(!parse_process_stat("42 (broken) S 1",42), "truncated stat");
    check(!parse_process_stat("42 broken S 1",42), "missing name delimiters");
    check(parse_process_stat(process_fixture("x",42,101),42).value->identity != p.value->identity, "PID reuse identity");
    check(parse_uid("Name: a\nUid: 1 2 3 4") == 2, "effective UID");
    check(!parse_uid("Uid: 1 bad 3 4") && !parse_uid("Name: x"), "missing malformed UID");
    check(rss_bytes(3,8192) == 24576 && !rss_bytes(-1,4096) && !rss_bytes(1,0), "page conversion");
    check(!rss_bytes(std::numeric_limits<std::int64_t>::max(),4096), "RSS overflow");
    check(!valid_constants({0,4096}) && !valid_constants({100,-1}), "invalid constants");
    auto path = std::filesystem::temp_directory_path() / ("monitor-fixture-"+std::to_string(::getpid()));
    std::filesystem::create_directories(path / "42");
    { std::ofstream(path/"42"/"stat") << process_fixture(); }
    { std::ofstream(path/"42"/"status") << "Uid: 1 2 3 4\n"; }
    std::filesystem::create_directories(path/"43"); // disappeared before stat read
    ProcessMonitor collector({100,4096}, path.string());
    auto records = collector.collect();
    check(records && records.value->size() == 1 && records.value->front().uid == 2, "fixture collection skips vanished entry");
    std::filesystem::remove(path/"42"/"status");
    records = collector.collect();
    check(records && records.value->size() == 1 && !records.value->front().uid, "missing status keeps process");
    std::filesystem::remove_all(path);
    check(!collector.collect(), "directory failure distinct from empty");
}
void process_cpu_tests() {
    using namespace monitor;
    ProcessRecord first;
    first.stat = *parse_process_stat(process_fixture(),42).value;
    auto next = first;
    next.timestamp += std::chrono::seconds(2);
    next.stat.user_ticks += 400;
    check(process_cpu(next,&first,100).value == 200, "process 200% using elapsed seconds");
    check(process_cpu(first,nullptr,100).state == Availability::pending, "new process pending");
    check(process_cpu(first,&first,100).state == Availability::unavailable, "zero process elapsed");
    check(process_cpu(first,&next,100).state == Availability::unavailable, "process regression");
    auto reuse = next;
    reuse.stat.identity.start_ticks++;
    check(process_cpu(reuse,&first,100).state == Availability::pending, "reused PID pending");
    auto overflow = next;
    overflow.stat.user_ticks = std::numeric_limits<Counter>::max();
    check(process_cpu(overflow,&first,100).state == Availability::unavailable, "process sum overflow");
    check(process_cpu(next,&first,0).state == Availability::unavailable, "invalid tick constant");
    ProcessMonitor monitor({100,4096});
    auto send = [&](const ProcessRecord& record) { return monitor.update({std::vector<ProcessRecord>{record}, {}}); };
    check(send(first).rows[0].cpu.state == Availability::pending, "tracker first sample");
    auto result = send(next);
    check(result.rows[0].cpu.value == 200 && result.rows[0].user == "unavailable", "tracker rate/missing UID");
    check(send(reuse).rows[0].cpu.state == Availability::pending, "tracker PID reuse");
    check(!monitor.update({{}, "denied"}).error.empty(), "tracker source failure");
    check(send(reuse).rows[0].cpu.state == Availability::pending, "tracker recovery baseline");
    monitor.update({std::vector<ProcessRecord>{}, {}});
    check(send(reuse).rows[0].cpu.state == Availability::pending, "tracker prunes gone process");
    check(send(reuse).rows[0].cpu.state == Availability::unavailable, "tracker invalid pair");
    check(send(reuse).rows[0].cpu.state == Availability::pending, "tracker invalid resets");
    check(resolve_user(std::numeric_limits<unsigned int>::max()) == std::to_string(std::numeric_limits<unsigned int>::max()), "unknown UID numeric fallback");
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
    ProcessMonitor processes(system_constants());
    auto records = processes.collect();
    check(records && !records.value->empty(), "live process enumeration");
    bool self = false;
    for (const auto& record : *records.value) if (record.stat.identity.pid == ::getpid()) self = true;
    check(self, "own process visible");
    auto snapshot = processes.sample();
    auto again = processes.sample();
    check(snapshot.error.empty() && !again.rows.empty(), "live process snapshots");
    for (const auto& row : snapshot.rows) if (row.record.stat.identity.pid == ::getpid())
        check(row.user == resolve_user(::geteuid()), "live user lookup");
}
int main(int argc, char**) {
    try {
        cpu_tests();
        memory_tests();
        system_tests();
        process_parser_tests();
        process_cpu_tests();
        if (argc > 1) live_tests();
        std::cout << "All checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
