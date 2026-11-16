#include "monitor/terminal_ui.hpp"
#include <csignal>
namespace {
volatile std::sig_atomic_t stop = 0;
extern "C" void shutdown_requested(int) { stop=1; }
}
int main(int argc, char**) {
    std::signal(SIGINT,shutdown_requested);
    std::signal(SIGTERM,shutdown_requested);
    monitor::SystemSnapshot system;
    for(int i=-1;i<32;++i) system.cpus[i]={monitor::Availability::valid,42.5};
    system.memory.ram={monitor::MemoryUsage{4096,1024,3072},{}};
    system.memory.swap={monitor::MemoryUsage{0,0,0},{}};
    monitor::ProcessSnapshot processes;
    for(int i=0;i<40;++i) {
        monitor::ProcessRow row;
        row.record.stat.identity={100+i,1000};
        row.record.stat.ppid=1; row.record.stat.name="fixture-"+std::to_string(i);
        row.record.stat.state='S'; row.record.stat.threads=2; row.record.rss_bytes=4096;
        row.user="tester"; row.cpu={monitor::Availability::valid,125};
        processes.rows.push_back(row);
    }
    if(argc>1) { processes.rows.clear(); processes.error="fixture access denied"; }
    monitor::TerminalUI ui;
    ui.update(processes);
    while(!stop) {
        std::optional<monitor::ProcessDetails> details;
        if(ui.selected_identity()) {
            details=monitor::ProcessDetails{*ui.selected_identity(),monitor::DetailState::valid,
                {"/bin/fixture", "unsafe\x1b[31m",std::string(4000,'a')+"END_OF_LONG_ARGUMENT"},"2000-01-01 00:00:00 UTC (+0000)",{}};
        }
        ui.render(system,processes,details);
        if(!ui.handle_input(ui.read_key(100),processes)) break;
    }
}
