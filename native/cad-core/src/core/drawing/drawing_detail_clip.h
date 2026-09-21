#pragma once

#include <array>

#include "core/drawing/drawing_types.h"

namespace polysmith::core {

// ══════════════════════════════════════════════════════════════════
//  Detail-view window clipping (ISO 128-3 enlarged features)
// ══════════════════════════════════════════════════════════════════
//
// A detail view's projection is its parent's projection CLIPPED to
// the detail circle (center + radius in the parent's view-mm).  The
// clip is pure geometry: provenance (source witnesses) is copied
// verbatim, so the detail inherits the parent's TNP behavior —
// nothing is ever re-identified.
//
// Rules:
//   • lines are clipped to their inside span (analytic segment-circle
//     intersection);
//   • circle arcs to their inside angular intervals (the sub-arcs'
//     endpoints AND start/end angles are recomputed together — the
//     flatten and the SVG backend derive geometry from both);
//   • ellipses / bsplines / other curves are kept only when fully
//     inside (endpoints + midpoint) — a window crossing an ellipse
//     drops it (documented V1 limitation; detail circles rarely cut
//     silhouette ellipses);
//   • a record circle that fully CONTAINS the window is dropped
//     (its geometry never appears inside the window);
//   • fully-outside records are dropped (never zero-length pieces).

/// Clips `source` to the circular window.  The result is a fresh
/// ProjectionResult with `source_revision`/`stale` copied from the
/// input (the caller re-stamps the revision for its cache).
ProjectionResult clip_projection_to_circle(const ProjectionResult& source,
                                           const std::array<double, 2>& center,
                                           double radius);

}  // namespace polysmith::core
