#include "monitor/system_monitor.hpp"
#include "monitor/parsing.hpp"
#include "monitor/metrics.hpp"
#include "proc_io.hpp"
namespace monitor {
SystemSnapshot SystemMonitor::sample() {
    const auto cpu = detail::read_file("/proc/stat");
    const auto memory = detail::read_file("/proc/meminfo");
    return update(cpu, memory, Clock::now());
}
SystemSnapshot SystemMonitor::update(const Result<std::string>& cpu, const Result<std::string>& memory,
                                      Clock::time_point timestamp) {
    SystemSnapshot output;
    output.timestamp = timestamp;
    auto parsed = cpu ? parse_cpu(*cpu.value) : Result<CpuSample>{{}, cpu.error};
    std::map<int, CpuCounters> next;
    if (parsed) {
        output.boot_time = parsed.value->boot_time;
        for (const auto& [id, current] : parsed.value->cpus) {
            const auto old = previous_.find(id);
            auto usage = cpu_usage(current, old == previous_.end() ? nullptr : &old->second);
            output.cpus.emplace(id, usage);
            if (usage.state != Availability::unavailable) next.emplace(id, current);
        }
    } else output.cpu_error = parsed.error;
    previous_ = std::move(next);
    if (memory) output.memory = memory_usage(parse_memory(*memory.value));
    else {
        output.memory_error = memory.error;
        output.memory.ram.error = output.memory.swap.error = memory.error;
    }
    return output;
}
}
