#include "monitor/process_monitor.hpp"
#include "monitor/terminal_ui.hpp"
#include <csignal>
#include <iostream>
namespace {
volatile std::sig_atomic_t stop_requested = 0;
extern "C" void request_stop(int) { stop_requested = 1; }
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
        auto system=system_monitor.sample();
        auto processes=process_monitor.sample(&stop_requested);
        ui.update(processes);
        std::optional<monitor::ProcessDetails> details;
        while (!stop_requested) {
            if (ui.wants_details() && ui.selected_identity()) {
                if (!details || details->identity != *ui.selected_identity())
                    details=process_monitor.read_details(*ui.selected_identity(),system.boot_time);
            } else details.reset();
            ui.render(system,processes,details);
            if (!ui.handle_input(ui.read_key(100),processes)) break;
        }
    } catch (const std::exception& error) {
        std::cerr << "Linux Process Monitor: " << error.what() << '\n';
        return 1;
    }
}
