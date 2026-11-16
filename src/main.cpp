#include "monitor/process_monitor.hpp"
#include "monitor/terminal_ui.hpp"
#include "monitor/sampling.hpp"
#include <csignal>
#include <ctime>
#include <iostream>
#include <stdexcept>
namespace {
volatile std::sig_atomic_t stop_requested = 0;
extern "C" void request_stop(int) { stop_requested = 1; }
double boot_seconds() {
    timespec value{};
    if (clock_gettime(CLOCK_BOOTTIME,&value) != 0) throw std::runtime_error("cannot read boot clock");
    return static_cast<double>(value.tv_sec) + static_cast<double>(value.tv_nsec)/1e9;
}
double realtime_seconds() {
    return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
}
}
int main() {
    try {
        struct sigaction action{};
        action.sa_handler=request_stop;
        sigemptyset(&action.sa_mask);
        if (sigaction(SIGINT,&action,nullptr) != 0 || sigaction(SIGTERM,&action,nullptr) != 0)
            throw std::runtime_error("cannot install shutdown handlers");
        monitor::SystemMonitor system_monitor;
        monitor::ProcessMonitor process_monitor(monitor::system_constants());
        monitor::TerminalUI ui;
        auto last_sample=monitor::Clock::now();
        auto last_boot=boot_seconds(), last_realtime=realtime_seconds();
        auto system=system_monitor.sample();
        auto processes=process_monitor.sample(&stop_requested);
        ui.update(processes);
        auto deadline=last_sample+std::chrono::seconds(1);
        std::optional<monitor::ProcessDetails> details;
        bool dirty=true;
        while (!stop_requested) {
            const auto now=monitor::Clock::now();
            bool sampled=false;
            if(now>=deadline) {
                const auto boot=boot_seconds(), realtime=realtime_seconds();
                if(monitor::timing_discontinuity(std::chrono::duration<double>(now-last_sample).count(),
                                                boot-last_boot,realtime-last_realtime)) {
                    system_monitor.reset(); process_monitor.reset();
                }
                last_sample=now; last_boot=boot; last_realtime=realtime;
                system=system_monitor.sample();
                processes=process_monitor.sample(&stop_requested);
                ui.update(processes);
                deadline=monitor::next_deadline(deadline,monitor::Clock::now());
                sampled=true; dirty=true;
            }
            if (ui.wants_details() && ui.selected_identity()) {
                if (sampled || !details || details->identity != *ui.selected_identity()) {
                    details=process_monitor.read_details(*ui.selected_identity(),system.boot_time);
                    dirty=true;
                }
            } else details.reset();
            if(dirty) { ui.render(system,processes,details); dirty=false; }
            const auto key=ui.read_key(monitor::input_timeout(deadline,monitor::Clock::now()));
            if (!ui.handle_input(key,processes)) break;
            // ncurses uses -1 for no input; source reads stay tied to deadlines.
            if(key != -1) dirty=true;
        }
    } catch (const std::exception& error) {
        std::cerr << "Linux Process Monitor: " << error.what() << '\n';
        return 1;
    }
}
