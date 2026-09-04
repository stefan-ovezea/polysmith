#pragma once

#include <string>

#include "core/cam/cam_types.h"

namespace polysmith::core {

// Shared infrastructure for milling (2.5D/3D) generators.  M0 carries
// only the tool-axis guard; the geometry helpers (z-level planning,
// pocket loop assignment, zigzag fill, contour leads, hole ordering)
// land with the M1 pocket work.
namespace milling {

// Rejects tool-axis modes no generator implements yet.  Every milling
// generator calls this first: "3_plus_2" and "rotary_continuous" are
// reserved for future rotary generators, so a document carrying them
// must degrade with a clear error instead of silently emitting 3-axis
// moves.  Empty is treated as "fixed_z" (lenient for old documents).
bool check_tool_axis_supported(const CamOperation& op, std::string& error);

}  // namespace milling
}  // namespace polysmith::core
