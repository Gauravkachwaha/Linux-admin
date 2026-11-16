#pragma once
#include "monitor/types.hpp"
#include "monitor/system_monitor.hpp"
#include "monitor/view_logic.hpp"
namespace monitor {
class TerminalUI {
public:
    TerminalUI();
    ~TerminalUI();
    TerminalUI(const TerminalUI&) = delete;
    TerminalUI& operator=(const TerminalUI&) = delete;
    int read_key(int timeout_ms);
    bool handle_input(int key, const ProcessSnapshot& processes);
    void update(const ProcessSnapshot& processes);
    void render(const SystemSnapshot& system, const ProcessSnapshot& processes,
                const std::optional<ProcessDetails>& details);
    std::optional<ProcessIdentity> selected_identity() const { return selected_; }
    bool wants_details() const { return details_open_; }
private:
    ViewOptions options_;
    std::vector<std::size_t> view_;
    std::optional<ProcessIdentity> selected_;
    std::size_t scroll_{}, detail_scroll_{}, core_page_{};
    bool initialized_{false}, editing_{false}, help_{false}, details_open_{false};
    void* screen_{};
};
}
