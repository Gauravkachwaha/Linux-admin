#pragma once
#include "monitor/types.hpp"
namespace monitor {
Clock::time_point next_deadline(Clock::time_point deadline, Clock::time_point now);
int input_timeout(Clock::time_point deadline, Clock::time_point now);
bool timing_discontinuity(double steady_elapsed, double boot_elapsed, double realtime_elapsed);
}
