#pragma once

#include <optional>
#include <string>
#include <vector>

namespace polysmith::core {

struct SketchDimension {
  std::string id;
  std::string kind;
  std::string entity_id;
  // Secondary entity for relational dimensions (e.g. the second line
  // of an angle dimension). Empty for unary dimensions like
  // line_length / circle_radius.
  std::string secondary_entity_id;
  // For "angle" dimensions, the angle in radians. For other kinds
  // this field carries the natural numeric value (length, radius).
  double value;
  // Optional formula expression (e.g. "width * 2"). When non-empty,
  // the resolved `value` is recomputed from this expression during
  // refresh_sketch_derived_state. Empty string = plain numeric value.
  std::string expression;
  // When true, this is a reference-only (driven) dimension. Its value
  // is kept in sync with the geometry during refresh_sketch_derived_state
  // but editing it does not drive the geometry. Default false (driving).
  bool driven = false;
  // When true, this dimension was created automatically at entity-commit
  // time (auto-dimension). Auto-dimensions only become driving constraints
  // when the user types a value (non-empty expression). Manual dimensions
  // (is_auto = false) are always driving constraints at their current
  // measured value, regardless of expression.
  bool is_auto = false;
  // For circle_radius dimensions: controls whether the UI displays the
  // value as radius or diameter. Empty string = diameter (default, for
  // backward compat). "radius" = display the raw radius value.
  std::string display_as;
  // Optional sketch-local label position. When set, viewport generation
  // uses it as a presentation override without changing the solved
  // dimension value or constrained geometry.
  std::optional<double> label_x;
  std::optional<double> label_y;
  // Virtual pivot for angle dimensions between lines that don't share
  // an endpoint or have an endpoint on the other's segment.  When set,
  // all pivot-dependent code (render, solver, update) reads from these
  // instead of recomputing from line endpoint proximity.
  std::optional<double> pivot_x;
  std::optional<double> pivot_y;

  // ── Vertex unification (Phase 3) ────────────────────────────
  // ID of the planegcs / SketchConstraint that this dimension
  // enforces.  Empty for display-only (auto) dimensions and until
  // the constraint system is refactored in Phase 4.
  std::string constraint_id;
};

}  // namespace polysmith::core
