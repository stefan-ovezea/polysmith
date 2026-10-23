// ── Zero-length line cleanup (S6) ─────────────────────────────
// A line the SOLVER has collapsed (both endpoints on the same point) cannot be
// drawn or extruded, so it is removed together with every record that
// referenced it. Before S6 that happened silently: the sketch simply looked like
// the user's dimension had never existed.
//
// Note the reachability: a direct vertex move onto the other endpoint is
// REFUSED by the degenerate-drop guard ("Sketch lines must have non-zero length
// (moving connected points)"), so this pass is a safety net for geometry that
// collapses through the solver, not a path a plain drag can take. It runs inside
// a command's refresh, i.e. after that command took its undo snapshot — so undo
// restores the line and everything removed with it in one step.
#pragma once

#include <string>
#include <vector>

namespace polysmith::core {

struct CollapsedLineCleanupReport {
  int lines = 0;
  int dimensions = 0;
  int constraints = 0;
  int relations = 0;

  int total_removed() const {
    return dimensions + constraints + relations;
  }
};

// The single user-visible sentence for a cleanup, so its wording is testable
// without a logger sink.
inline std::string format_collapsed_line_warning(
    const std::string& line_id,
    const CollapsedLineCleanupReport& report) {
  return "collapsed line " + line_id + " removed " +
         std::to_string(report.dimensions) + " dimension(s), " +
         std::to_string(report.constraints) + " constraint(s) and " +
         std::to_string(report.relations) + " relation(s)";
}

}  // namespace polysmith::core
