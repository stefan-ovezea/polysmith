#pragma once

// ── Laser cutting generator ──────────────────────────────────────
//
// Sketch profile → world curves → kerf offset → lead-in/out + pierce
// → toolpath IR.  M4/M5/M6/M7 extend this module with ordering,
// leads, tabs, and engrave fill (laser_order.cpp, laser_leads.cpp,
// laser_tabs.cpp, laser_fill.cpp).

#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

#include "core/cam/cam2d.h"
#include "core/cam/cam_generator.h"
#include "core/sketch/sketch_feature_parameters.h"

namespace polysmith::core::laser {

// Sentinel for "no enclosing loop".
inline constexpr size_t kNoParent = std::numeric_limits<size_t>::max();

// One planned cut loop.  Loops are planned in sketch-local (or
// world-XY for face-derived) coordinates and emitted through the
// sketch plane frame.
struct PlannedLoop {
  std::vector<cam2d::OffsetSegment> segments;
  std::vector<cam2d::XY> samples;  // sampled offset contour (cached)
  cam2d::XY centroid{0.0, 0.0};
  double area = 0.0;
  double length = 0.0;
  int depth = 0;    // nesting level: outer = 0, its holes = 1, ...
  size_t group = 0;      // region group index
  size_t parent = kNoParent;  // enclosing loop
  bool is_hole = false;
  bool skipped = false;  // set when the loop is dropped (warn + continue)
  cam2d::XY pierce{0.0, 0.0};  // resolved pierce point
  // The kerf offset went to the loop's INTERIOR side (kerf_side
  // "inside", or auto on a hole).  Interior-side leads must be
  // spoke-style (centroid → pierce) — a straight tangent line cannot
  // lie inside a closed contour.
  bool kerf_inside = false;
  // Face-derived loops already live in WORLD XY (sampled from the
  // body's wire geometry); sketch loops live in sketch-local 2D and
  // map through the sketch plane frame.
  bool isWorldXY = false;
  double worldZ = 0.0;  // cut-plane Z for world-XY loops
};

// Sketch frames: cached plane_frame when present, else the hardcoded
// origin-plane frame table (sketches on origin planes may not cache a
// frame — the history dependency pass resolves it on demand).
std::optional<SketchFeatureParameters::SketchPlaneFrame> resolve_sketch_frame(
    const SketchFeatureParameters& sketch);

CamGenerateResult generate_laser_cut_toolpath(
    const polysmith::core::CamGenerateContext& context);

}  // namespace polysmith::core::laser
