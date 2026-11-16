#include "monitor/terminal_ui.hpp"
#include "monitor/presentation.hpp"
#include <ncurses.h>
#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

namespace monitor {
namespace {
void line(int y, const std::string& text, bool highlight = false) {
    if (y < 0 || y >= LINES) return;
    if (highlight) attron(A_REVERSE);
    mvaddnstr(y,0,text.c_str(),std::max(0,COLS-1));
    if (highlight) attroff(A_REVERSE);
}
std::string memory_line(const char* label, const Result<MemoryUsage>& memory, bool ram) {
    if (!memory) return std::string(label)+": unavailable ("+safe_text(memory.error)+")";
    const auto& m = *memory.value;
    return std::string(label)+": "+format_bytes(m.used)+" used / "+format_bytes(m.total)+
        " total | "+format_bytes(m.free)+(ram ? " available" : " free")+(m.total == 0 && !ram ? " (no swap)" : "");
}
std::string row_text(const ProcessRow& row) {
    const auto& s = row.record.stat;
    std::ostringstream text;
    text << std::right << std::setw(7) << s.identity.pid << ' ' << std::setw(7) << s.ppid << ' '
         << s.state << ' ' << std::setw(5) << s.threads << ' ' << std::setw(8) << format_percentage(row.cpu) << ' '
         << std::setw(10) << (row.record.rss_bytes ? format_bytes(*row.record.rss_bytes) : "n/a") << ' '
         << std::left << std::setw(12) << safe_text(row.user).substr(0,12) << ' ' << safe_text(s.name);
    return text.str();
}
}
TerminalUI::TerminalUI() {
    if (!::isatty(STDIN_FILENO) || !::isatty(STDOUT_FILENO)) throw std::runtime_error("an interactive terminal is required");
    auto* screen = newterm(nullptr,stdout,stdin);
    if (!screen) throw std::runtime_error("cannot initialize ncurses; check TERM and terminfo");
    screen_ = screen;
    set_term(screen);
    if (cbreak() == ERR || noecho() == ERR || keypad(stdscr,TRUE) == ERR) {
        endwin(); delscreen(screen); screen_ = nullptr;
        throw std::runtime_error("cannot configure terminal input");
    }
    curs_set(0);
    set_escdelay(25);
}
TerminalUI::~TerminalUI() {
    if (screen_) { endwin(); delscreen(static_cast<SCREEN*>(screen_)); }
}
int TerminalUI::read_key(int timeout_ms) { timeout(timeout_ms); return getch(); }
void TerminalUI::update(const ProcessSnapshot& processes) {
    view_ = process_view(processes.rows,options_);
    selected_ = reconcile_selection(selected_,processes.rows,view_);
    if (!initialized_) {
        if (!view_.empty()) selected_ = processes.rows[view_.front()].record.stat.identity;
        initialized_ = true;
    }
}
bool TerminalUI::handle_input(int key, const ProcessSnapshot& processes) {
    if (key == ERR || key == KEY_RESIZE) return true;
    if (COLS < 80 || LINES < 24) return key != 'q';
    if (editing_) {
        if (key == 27 || key == '\n' || key == KEY_ENTER) editing_ = false;
        else if (key == KEY_BACKSPACE || key == 127 || key == 8) { if (!options_.filter.empty()) options_.filter.pop_back(); }
        else if (key >= 32 && key < 127 && options_.filter.size() < 256) options_.filter += static_cast<char>(key);
        update(processes);
        return true;
    }
    if (key == 'q') return false;
    if (key == 'h' || key == '?') { help_ = !help_; return true; }
    if (key == 27) { help_ = false; details_open_ = false; return true; }
    if (help_) return true;
    if (key == 'd' || key == '\n' || key == KEY_ENTER) { details_open_ = !details_open_; detail_scroll_ = 0; return true; }
    if (details_open_) {
        if (key == 'j' || key == KEY_DOWN) ++detail_scroll_;
        if ((key == 'k' || key == KEY_UP) && detail_scroll_ > 0) --detail_scroll_;
        if (key == KEY_NPAGE) detail_scroll_ += static_cast<std::size_t>(std::max(1,LINES-4));
        if (key == KEY_PPAGE) detail_scroll_ -= std::min(detail_scroll_,static_cast<std::size_t>(std::max(1,LINES-4)));
        return true;
    }
    if (key == '/' || key == 'p') {
        options_.filter_mode = key == 'p' ? FilterMode::pid : FilterMode::name;
        options_.filter.clear(); editing_ = true;
    } else if (key == 'x') options_.filter.clear();
    else if (key == 'c') options_.sort = SortKey::cpu;
    else if (key == 'm') options_.sort = SortKey::memory;
    else if (key == 'i') options_.sort = SortKey::pid;
    else if (key == 'r') options_.ascending = !options_.ascending;
    else if (key == ']') ++core_page_;
    else if (key == '[' && core_page_ > 0) --core_page_;
    else if (key == 'j' || key == KEY_DOWN || key == 'k' || key == KEY_UP || key == KEY_NPAGE || key == KEY_PPAGE) {
        if (!view_.empty()) {
            std::size_t index = 0;
            for (std::size_t n=0;n<view_.size();++n) if (selected_ == processes.rows[view_[n]].record.stat.identity) index=n;
            const auto step = static_cast<std::size_t>((key==KEY_NPAGE || key==KEY_PPAGE) ? std::max(1,LINES-10) : 1);
            if (!selected_) index = 0;
            else if (key=='j' || key==KEY_DOWN || key==KEY_NPAGE) index += std::min(step,view_.size()-1-index);
            else index -= std::min(index,step);
            selected_ = processes.rows[view_[index]].record.stat.identity;
        }
    }
    update(processes);
    return true;
}
void TerminalUI::render(const SystemSnapshot& system, const ProcessSnapshot& processes,
                         const std::optional<ProcessDetails>& details) {
    erase();
    if (COLS < 80 || LINES < 24) {
        line(0,"Terminal too small: resize to at least 80x24. q quits."); refresh(); return;
    }
    line(0,"Linux Process Monitor | read-only | h help | q quit");
    if (help_) {
        const std::vector<std::string> help{
            "Keys", "j/k or arrows: select process   PgUp/PgDn: scroll", "c: sort CPU   m: sort RSS   i: sort PID   r: reverse order",
            "/: name filter   p: exact PID filter   x: clear filter", "Enter/Esc: finish editing filter (query remains active)",
            "d/Enter: toggle details   Esc: close overlay", "Details: j/k or PgUp/PgDn scroll the complete command line",
            "[ / ]: previous/next logical CPU page", "h/?: toggle help   q: quit (outside filter editor)", "",
            "CPU: successive non-idle counters; idle and I/O-wait count as idle.",
            "Process CPU: 100% = one logical CPU; multiple threads may exceed 100%.",
            "RAM used = total - available; swap used = total - free. Binary units.",
            "pending: first sample; n/a/unavailable: missing or invalid data.",
            "Visible processes depend on /proc permissions and namespaces."};
        for (std::size_t i=0;i<help.size();++i) line(static_cast<int>(i)+2,help[i]);
    } else if (details_open_) {
        std::vector<std::string> content;
        const ProcessRow* row = nullptr;
        for (const auto& candidate : processes.rows) if (selected_ == candidate.record.stat.identity) row=&candidate;
        if (!row) content.emplace_back("Selected process is no longer available or is hidden by the filter.");
        else {
            const auto& s = row->record.stat;
            content.push_back("PID: "+std::to_string(s.identity.pid)+"  PPID: "+std::to_string(s.ppid)+"  State: "+s.state+"  Threads: "+std::to_string(s.threads));
            content.push_back("Name: "+safe_text(s.name));
            content.push_back("User: "+safe_text(row->user)+"  CPU: "+format_percentage(row->cpu)+"  RSS: "+(row->record.rss_bytes ? format_bytes(*row->record.rss_bytes) : "unavailable"));
            if (!details || details->identity != s.identity) content.emplace_back("Details pending");
            else {
                content.push_back("Start: "+safe_text(details->start_time));
                if (details->state != DetailState::valid)
                    content.push_back((details->state == DetailState::gone ? "Process gone: " : "Command line unavailable: ")+safe_text(details->error));
                else {
                    content.emplace_back("Command line (quoted arguments):");
                    content.push_back(format_arguments(details->arguments));
                }
            }
        }
        std::vector<std::string> wrapped;
        for (const auto& text : content) {
            auto lines = wrap_text(text,static_cast<std::size_t>(COLS-1));
            wrapped.insert(wrapped.end(),lines.begin(),lines.end());
        }
        const auto capacity=static_cast<std::size_t>(LINES-4);
        detail_scroll_=std::min(detail_scroll_,wrapped.size()>capacity ? wrapped.size()-capacity : 0);
        for (std::size_t i=0;i<capacity && i+detail_scroll_<wrapped.size();++i) line(static_cast<int>(i)+2,wrapped[i+detail_scroll_]);
        line(LINES-1,"Details | j/k/PgUp/PgDn scroll | d/Esc back | q quit");
    } else {
        const auto aggregate=system.cpus.find(-1);
        line(1,"Overall CPU: "+(aggregate==system.cpus.end() ? std::string("unavailable") : format_percentage(aggregate->second)));
        line(2,memory_line("RAM",system.memory.ram,true));
        line(3,memory_line("Swap",system.memory.swap,false));
        std::vector<std::string> cores;
        for (const auto& [id,value] : system.cpus) if (id>=0) cores.push_back("CPU"+std::to_string(id)+" "+format_percentage(value));
        const auto per_line=static_cast<std::size_t>((COLS-1)/16), page_size=per_line*2;
        const auto pages=std::max<std::size_t>(1,(cores.size()+page_size-1)/page_size);
        core_page_=std::min(core_page_,pages-1);
        for (std::size_t i=0;i<page_size && core_page_*page_size+i<cores.size();++i) {
            mvaddnstr(4+static_cast<int>(i/per_line),static_cast<int>((i%per_line)*16),cores[core_page_*page_size+i].c_str(),15);
        }
        const char* sort=options_.sort==SortKey::cpu ? "CPU" : options_.sort==SortKey::memory ? "RSS" : "PID";
        line(6,std::string(editing_ ? "Editing " : "Filter ")+(options_.filter_mode==FilterMode::pid ? "PID: " : "name: ")+options_.filter+
             " | Sort "+sort+(options_.ascending ? " asc" : " desc")+" | CPU page "+std::to_string(core_page_+1)+"/"+std::to_string(pages));
        std::string error=system.cpu_error;
        if (!system.memory_error.empty()) error += " "+system.memory_error;
        if (!processes.error.empty()) error += " "+processes.error;
        line(7,error.empty() ? std::to_string(view_.size())+" matching processes" : "Source error: "+safe_text(error));
        line(8,"    PID    PPID S   THR     CPU%        RSS USER         NAME",true);
        const auto capacity=static_cast<std::size_t>(LINES-10);
        for (std::size_t i=0;i<view_.size();++i) if (selected_ == processes.rows[view_[i]].record.stat.identity) {
            if(i<scroll_) scroll_=i;
            if(i>=scroll_+capacity) scroll_=i-capacity+1;
        }
        scroll_=std::min(scroll_,view_.size()>capacity ? view_.size()-capacity : 0);
        if (view_.empty()) line(9,"No matching processes");
        for(std::size_t i=0;i<capacity && scroll_+i<view_.size();++i) {
            const auto& row=processes.rows[view_[scroll_+i]];
            line(9+static_cast<int>(i),row_text(row),selected_==row.record.stat.identity);
        }
        line(LINES-1,"j/k select | c/m/i sort | r reverse | / name | p PID | x clear | d details");
    }
    refresh();
}
}
