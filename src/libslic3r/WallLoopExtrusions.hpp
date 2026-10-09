#pragma once

#include "ExtrusionEntityCollection.hpp"
#include "WallLoopFilaments.hpp"

#include <memory>

namespace Slic3r {

// Preserve each complete loop (including overhang segments and seam metadata).
// Buckets are zero-based, while the setting and fallback are one-based.
inline bool split_extrusion_collection_for_wall_loop_filaments(const ExtrusionEntityCollection&                         source,
                                                               const std::vector<unsigned int>&                         ids,
                                                               unsigned int                                             wall_filament,
                                                               size_t                                                   num_physical,
                                                               std::vector<std::unique_ptr<ExtrusionEntityCollection>>& out_by_extruder)
{
    out_by_extruder.clear();
    if (source.entities.empty() || ids.empty() || wall_filament == 0 || wall_filament > num_physical)
        return false;
    out_by_extruder.resize(num_physical);
    ExtrusionEntityCollection flattened = source.flatten(false);
    for (const ExtrusionEntity* entity : flattened.entities) {
        const int          inset_idx = is_perimeter(entity->role()) ? entity->inset_idx : -1;
        const unsigned int id        = wall_loop_filament(ids, inset_idx, wall_filament, num_physical);
        auto&              bucket    = out_by_extruder[id - 1];
        if (!bucket) {
            bucket          = std::make_unique<ExtrusionEntityCollection>();
            bucket->no_sort = source.no_sort;
        }
        bucket->append(*entity);
    }
    return !flattened.entities.empty();
}

} // namespace Slic3r
