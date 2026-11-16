#include "monitor/view_logic.hpp"
#include <algorithm>
#include <charconv>
#include <cctype>
namespace monitor {
namespace {
std::string lowercase(std::string text) {
    for (char& ch : text) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return text;
}
}
std::vector<std::size_t> process_view(const std::vector<ProcessRow>& rows, const ViewOptions& options) {
    std::vector<std::size_t> view;
    const auto query = lowercase(options.filter);
    int pid{};
    bool valid_pid = false;
    if (!query.empty()) {
        auto [end,error] = std::from_chars(query.data(),query.data()+query.size(),pid);
        valid_pid = error == std::errc{} && end == query.data()+query.size() && pid > 0;
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& stat = rows[i].record.stat;
        if (query.empty() || (options.filter_mode == FilterMode::pid
            ? valid_pid && stat.identity.pid == pid
            : lowercase(stat.name).find(query) != std::string::npos)) view.push_back(i);
    }
    std::sort(view.begin(),view.end(),[&](std::size_t left, std::size_t right) {
        const auto& a = rows[left]; const auto& b = rows[right];
        const auto compare = [&](auto x, auto y) { return options.ascending ? x < y : x > y; };
        if (options.sort == SortKey::cpu) {
            const bool av = a.cpu.state == Availability::valid, bv = b.cpu.state == Availability::valid;
            if (av != bv) return av;
            if (av && a.cpu.value != b.cpu.value) return compare(a.cpu.value,b.cpu.value);
        } else if (options.sort == SortKey::memory) {
            const auto av = a.record.rss_bytes, bv = b.record.rss_bytes;
            if (av.has_value() != bv.has_value()) return av.has_value();
            if (av && *av != *bv) return compare(*av,*bv);
        } else if (a.record.stat.identity.pid != b.record.stat.identity.pid)
            return compare(a.record.stat.identity.pid,b.record.stat.identity.pid);
        return a.record.stat.identity.pid < b.record.stat.identity.pid;
    });
    return view;
}
std::optional<ProcessIdentity> reconcile_selection(std::optional<ProcessIdentity> selected,
    const std::vector<ProcessRow>& rows, const std::vector<std::size_t>& view) {
    if (selected) for (auto index : view)
        if (rows.at(index).record.stat.identity == *selected) return selected;
    return {};
}
}
