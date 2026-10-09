#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Slic3r {

// Plain 1-based filament IDs, outside in. An empty string disables the override.
// Reject the whole list on malformed input rather than shifting wall positions.
inline bool parse_wall_loop_filaments(std::string_view text, std::vector<unsigned int>& ids)
{
    ids.clear();
    const auto is_space   = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    size_t     pos        = 0;
    const auto skip_space = [&]() {
        while (pos < text.size() && is_space(text[pos]))
            ++pos;
    };
    skip_space();
    if (pos == text.size())
        return true;
    while (pos < text.size()) {
        unsigned int id    = 0;
        const size_t start = pos;
        while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
            const unsigned int digit = unsigned(text[pos++] - '0');
            if (id > (std::numeric_limits<unsigned int>::max() - digit) / 10) {
                ids.clear();
                return false;
            }
            id = id * 10 + digit;
        }
        if (pos == start || id == 0) {
            ids.clear();
            return false;
        }
        ids.emplace_back(id);
        skip_space();
        if (pos == text.size())
            return true;
        if (text[pos++] != ',') {
            ids.clear();
            return false;
        }
        skip_space();
    }
    ids.clear(); // A trailing comma is not an empty wall assignment.
    return false;
}

inline bool wall_loop_filaments_in_range(const std::vector<unsigned int>& ids, size_t num_physical)
{
    return std::all_of(ids.begin(), ids.end(), [num_physical](unsigned int id) { return id > 0 && id <= num_physical; });
}

// Paths without a wall index, including gap fill and thin walls, use the regular wall filament.
inline unsigned int wall_loop_filament(const std::vector<unsigned int>& ids, int inset_idx, unsigned int wall_filament, size_t num_physical)
{
    if (ids.empty() || inset_idx < 0)
        return wall_filament;
    const unsigned int id = ids[std::min(size_t(inset_idx), ids.size() - 1)];
    return id > 0 && id <= num_physical ? id : wall_filament;
}

// Keep repeated entries and wall positions. Like wall_filament, a reference to a
// deleted filament resets the override; removing an entry would shift every inner wall.
inline bool remap_wall_loop_filaments(std::string& text, const std::vector<unsigned int>& id_remap, size_t num_filaments)
{
    std::vector<unsigned int> ids;
    if (!parse_wall_loop_filaments(text, ids))
        return false;
    std::string remapped;
    for (unsigned int id : ids) {
        if (id >= id_remap.size() || id_remap[id] == 0 || id_remap[id] > num_filaments)
            return false;
        if (!remapped.empty())
            remapped += ',';
        remapped += std::to_string(id_remap[id]);
    }
    text = std::move(remapped);
    return true;
}

} // namespace Slic3r
