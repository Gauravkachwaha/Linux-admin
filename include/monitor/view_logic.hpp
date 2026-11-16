#pragma once
#include "monitor/types.hpp"
namespace monitor {
enum class SortKey { cpu, memory, pid };
enum class FilterMode { name, pid };
struct ViewOptions {
    SortKey sort{SortKey::cpu};
    bool ascending{false};
    FilterMode filter_mode{FilterMode::name};
    std::string filter;
};
std::vector<std::size_t> process_view(const std::vector<ProcessRow>& rows, const ViewOptions& options);
std::optional<ProcessIdentity> reconcile_selection(std::optional<ProcessIdentity> selected,
    const std::vector<ProcessRow>& rows, const std::vector<std::size_t>& view);
}
