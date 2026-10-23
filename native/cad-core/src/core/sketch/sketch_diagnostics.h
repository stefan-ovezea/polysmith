// ── Sketch loose-end diagnostics (S5) ─────────────────────────
// A sketch that looks closed but is not is the most common reason an extrude
// refuses ("no profile found"), and the user gets no hint about WHICH ends are
// the problem. This pass names them from the vertex table the profile walk
// already builds:
//
//   open    - an endpoint used by exactly one curve, i.e. the end of a chain.
//             (A corner where two curves meet has two owners and is fine.)
//   untied  - two OPEN endpoints from different curves that sit within the
//             tolerance of each other but are not the same vertex. Visually
//             they look joined; the profile walk sees a gap.
//
// Derived vertices (text glyphs, slot corners) are excluded: they belong to a
// generated entity and the user cannot tie their ends. Projected vertices are
// excluded for the same reason.
#pragma once

#include <string>
#include <vector>

namespace polysmith::core {

struct SketchLooseEnd {
  std::string vertex_id;
  double x = 0.0;
  double y = 0.0;
  std::vector<std::string> owner_ids;
};

struct SketchUntiedPair {
  std::string vertex_a_id;
  std::string vertex_b_id;
  double gap = 0.0;
};

struct SketchDiagnostics {
  std::vector<SketchLooseEnd> open;
  std::vector<SketchUntiedPair> untied;
};

// Two ends closer than this are "untied": they look joined to the user but the
// profile walk still sees two vertices. Matches the gap the UI hints at
// ("close the gap"), well below any modelling scale.
inline constexpr double kUntiedEndToleranceMm = 0.05;

}  // namespace polysmith::core
