#pragma once

#include <string>
#include <vector>

#include "core/cam/cam_types.h"  // Bounds3D

namespace polysmith::core {

// ── Toolpath intermediate representation ──────────────────────────
//
// The single shape every CAM generator emits and every post-processor
// consumes.  Deliberately richer than the legacy CamToolpath (which
// had no arcs, no per-move feedrate, and no laser state): a post-
// processor must be able to serialize G2/G3, feed changes, and laser
// power transitions without re-deriving them from geometry.
//
// Toolpaths are memory-only: they are generated on demand, cached in
// cam_runtime keyed by document revision, and never serialized into
// the document payload (ToolpathCache carries metadata only).

enum class ToolpathMoveKind {
  Rapid,        // G0
  FeedLinear,   // G1
  FeedArcCW,    // G2
  FeedArcCCW,   // G3
};

struct ToolpathMove {
  ToolpathMoveKind kind = ToolpathMoveKind::FeedLinear;
  double x = 0.0, y = 0.0, z = 0.0;  // endpoint (machine coordinates)
  // Arc center as an offset from the arc START point (GRBL I/J
  // convention), in the XY plane.
  double i = 0.0, j = 0.0;
  double feedrate_mm_per_min = 0.0;  // 0 = keep previous feed
  double power_percent = 100.0;      // laser S source (mills ignore it)
  bool laser_on = true;              // M3/M4 vs M5 boundary for lasers
  double dwell_seconds = 0.0;        // G4 pause after this move (pierce)
};

struct Toolpath {
  std::string op_id;  // owning CamOperation
  std::vector<ToolpathMove> moves;
  Bounds3D bounds;
  double total_length_mm = 0.0;
  double estimated_time_seconds = 0.0;
  int num_rapids = 0;
  int num_feeds = 0;
  int num_arcs = 0;
  int num_points = 0;  // linearized count, for viewport/UI stats
  // Fixed segment count per full circle for arc linearization
  // (viewport polyline + linearized post).  0 = auto chord tolerance.
  int arc_segments_per_circle = 0;
};

}  // namespace polysmith::core
