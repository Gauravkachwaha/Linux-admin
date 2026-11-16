#include "monitor/sampling.hpp"
#include <algorithm>
#include <cmath>
namespace monitor {
Clock::time_point next_deadline(Clock::time_point deadline, Clock::time_point now) {
    if (deadline > now) return deadline;
    const auto missed = std::chrono::duration_cast<std::chrono::seconds>(now-deadline);
    return deadline + missed + std::chrono::seconds(1);
}
int input_timeout(Clock::time_point deadline, Clock::time_point now) {
    if(now >= deadline) return 0;
    const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline-now).count();
    return static_cast<int>(std::clamp<decltype(remaining)>(remaining,0,100));
}
bool timing_discontinuity(double steady_elapsed, double boot_elapsed, double realtime_elapsed) {
    return !std::isfinite(steady_elapsed) || !std::isfinite(boot_elapsed) || !std::isfinite(realtime_elapsed) ||
        steady_elapsed <= 0 || std::abs(boot_elapsed-steady_elapsed)>2.0 ||
        std::abs(realtime_elapsed-steady_elapsed)>2.0;
}
}
