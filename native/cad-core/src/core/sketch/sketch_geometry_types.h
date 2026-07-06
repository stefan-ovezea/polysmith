#pragma once

#include <optional>
#include <string>
#include <vector>

namespace polysmith::core {

struct SketchProfilePoint {
  double x;
  double y;
};

struct SketchLine {
  std::string id;
  std::string start_point_id;
  std::string end_point_id;
  double start_x;
  double start_y;
  double end_x;
  double end_y;
  std::optional<std::string> constraint;
  // When true, the H/V constraint on this line is a reference (driven)
  // constraint. The solver ignores it; the UI renders it in parentheses.
  bool constraint_driven = false;
  // True when the line is a "construction" line (contextual modeling
  // dashed reference geometry). Construction lines participate in
  // snapping and constraints, but are excluded from profile loop
  // detection so they don't seal profiles for face picking / extrude
  // sources. Defaults to false; older saves are loaded as solid.
  bool is_construction = false;
};

struct SketchCircle {
  std::string id;
  double center_x;
  double center_y;
  double radius;
  bool is_construction = false;
};

// Regular polygon on the sketch plane. Supports three creation modes:
//   "circumscribed" — polygon circumscribed around a circle (center+radius).
//   "inscribed"     — polygon inscribed in a circle (center+radius).
//   "edge"          — polygon defined by one edge (two points).
struct SketchPolygon {
  std::string id;
  double center_x;
  double center_y;
  double radius;
  int sides;          // >= 3
  std::string mode;   // "circumscribed" | "inscribed" | "edge"
  double start_x;
  double start_y;
  double end_x;
  double end_y;
  bool is_construction = false;
};

// 2D arc on the sketch plane. Stored as start/end endpoint ids (so it
// participates in the shared point graph just like a SketchLine) plus
// a fully-cached (center, radius, ccw) triple. v1 freezes the arc's
// shape at creation: the endpoint points are flagged is_fixed=true so
// the user can't drag them off the cached arc, and there is no
// constraint / dimension support on arcs yet. Editing flows on arcs
// (drag-to-reshape, radius dimension, etc.) are deliberately left
// for a follow-up so the loop / extrude integration here doesn't have
// to worry about the cached params drifting from the endpoints.
struct SketchArc {
  std::string id;
  std::string start_point_id;
  std::string end_point_id;
  // Cached shape parameters. Endpoint coordinates duplicate the
  // owning SketchPoint coordinates, mirroring how SketchLine caches
  // its endpoints, so consumers (renderer, profile builder) don't
  // have to chase pointer references for every paint.
  double center_x;
  double center_y;
  double radius;
  double start_x;
  double start_y;
  double end_x;
  double end_y;
  // True when the sweep from start to end runs counter-clockwise in
  // sketch-plane coordinates. Combined with the cached endpoints this
  // disambiguates which side of the chord the arc bulges to (a major
  // vs minor arc).
  bool ccw;
  bool is_construction = false;
};

struct SketchPoint {
  std::string id;
  std::string kind;
  double x;
  double y;
  bool is_fixed;

  // ── Vertex unification fields (Phase 1) ─────────────────────
  // IDs of the geometry entities (lines, circles, arcs) that own
  // this vertex.  For a line endpoint this is [line_id]; for a
  // shared corner it is [line_a_id, line_b_id].  Populated in
  // Phase 2; empty for now.
  std::vector<std::string> geometry_owner_ids;

  // True when this point was produced by the Project tool from
  // 3D body geometry.
  bool is_projected = false;

  // Source of the projection (only meaningful when is_projected).
  // e.g. "edge_midpoint", "vertex", "face_center"
  std::optional<std::string> source_type;
  // Feature id of the body the projection came from.
  std::optional<std::string> source_feature_id;
  // Edge id if the source was an edge midpoint / endpoint.
  std::optional<std::string> source_edge_id;
};

// Standalone sketch point produced by the Project tool when the user
// projects a body vertex onto the active sketch plane. Unlike line /
// arc / circle endpoints, projected points are not derived from any
// other sketch entity — they have to be re-emitted by
// `rebuild_sketch_points` from this list directly. The cached (x, y)
// is the projected location in sketch-local coordinates; `source_id`
// records the body vertex id (`<body>:vertex:<index>`) so the
// projection is idempotent (clicking the same vertex twice is a
// no-op) and can be located by future edits.
struct SketchProjectedPoint {
  std::string id;
  std::string source_id;
  double x;
  double y;
};

}  // namespace polysmith::core
