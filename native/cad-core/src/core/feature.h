#pragma once

#include <optional>
#include <string>
#include <vector>

namespace polysmith::core {

struct SketchProfilePoint {
  double x;
  double y;
};

struct BoxFeatureParameters {
  double width;
  double height;
  double depth;
};

struct CylinderFeatureParameters {
  double radius;
  double height;
};

struct PlaneFrame {
  double origin_x;
  double origin_y;
  double origin_z;
  double x_axis_x;
  double x_axis_y;
  double x_axis_z;
  double y_axis_x;
  double y_axis_y;
  double y_axis_z;
  double normal_x;
  double normal_y;
  double normal_z;
};

struct ExtrudeFeatureParameters {
  struct SideParameters {
    // "distance", "through_all", "to_object", or "to_next".
    std::string extent_type = "distance";
    // Resolved side distance. For dynamic extents this stores the latest
    // solved distance so older readers and the shape builder still have a
    // concrete prism length.
    double distance = 10.0;
    // Positive distance from the profile plane along this side's direction
    // before material starts.
    double start_offset = 0.0;
    double taper_angle_degrees = 0.0;
    // Face id or body id used by to_object / through_all.
    std::optional<std::string> target_reference_id;
  };

  struct ThinParameters {
    bool enabled = false;
    double thickness = 1.0;
    // "center", "inside", or "outside".
    std::string placement = "center";
  };

  std::string sketch_feature_id;
  std::string profile_id;
  std::vector<std::string> profile_ids;
  // For thin extrudes from open sketch chains. Empty for closed-profile
  // and planar-face extrudes.
  std::vector<std::string> open_entity_ids;
  std::string plane_id;
  std::optional<PlaneFrame> plane_frame;
  std::string profile_kind;
  double start_x;
  double start_y;
  double width;
  double height;
  double radius;
  std::vector<SketchProfilePoint> profile_points;
  std::vector<std::vector<SketchProfilePoint>> inner_loops;
  std::vector<std::vector<SketchProfilePoint>> additional_profile_points;
  std::vector<std::vector<std::vector<SketchProfilePoint>>> additional_inner_loops;
  double depth;
  // "one_side", "symmetric", or "two_sides". Legacy files omit this and
  // load as one_side using `depth`.
  std::string extent_mode = "one_side";
  SideParameters side1{};
  std::optional<SideParameters> side2;
  ThinParameters thin{};
  // "new_body" (default): produces an independent solid body.
  // "join": fuses the extrude with `target_body_id` if set, else the
  //         most recent existing body.
  // "cut":  subtracts the extrude from the same target choice as "join".
  // "intersect": keeps only the volume common to the target and extrusion.
  std::string mode = "new_body";
  // "new_body", "join", "cut", or "intersect". `mode` stores the current
  // effective operation so old consumers keep working.
  std::string operation = "new_body";
  // "replace_target" or "new_body" for intersect mode.
  std::string intersect_result = "replace_target";
  // Optional explicit target body for boolean modes. Stored as the root
  // feature id of the target body (the same id reported in
  // `viewport_state.bodies`). Empty / unset means "most recent body" so
  // single-body workflows keep working without UI changes.
  std::optional<std::string> target_body_id;
};

struct LoftSectionParameters {
  std::string sketch_feature_id;
  std::string profile_id;
  std::string plane_id;
  std::optional<PlaneFrame> plane_frame;
  std::vector<SketchProfilePoint> profile_points;
};

struct LoftFeatureParameters {
  std::vector<LoftSectionParameters> sections;
  bool ruled = false;
};

struct RevolveFeatureParameters {
  std::string sketch_feature_id;
  std::string profile_id;
  std::string plane_id;
  std::optional<PlaneFrame> plane_frame;
  std::string profile_kind;
  std::vector<SketchProfilePoint> profile_points;
  std::vector<std::vector<SketchProfilePoint>> inner_loops;
  std::string axis_sketch_feature_id;
  std::string axis_entity_id;
  double axis_start_x;
  double axis_start_y;
  double axis_start_z;
  double axis_end_x;
  double axis_end_y;
  double axis_end_z;
  double angle_degrees = 360.0;
};

struct SweepFeatureParameters {
  struct PathSegment {
    std::string entity_id;
    std::string kind;  // "line" | "arc"
    double start_x = 0.0;
    double start_y = 0.0;
    double start_z = 0.0;
    double end_x = 0.0;
    double end_y = 0.0;
    double end_z = 0.0;
    double center_x = 0.0;
    double center_y = 0.0;
    double center_z = 0.0;
    double mid_x = 0.0;
    double mid_y = 0.0;
    double mid_z = 0.0;
    double radius = 0.0;
    bool ccw = true;
  };

  std::string sketch_feature_id;
  std::string profile_id;
  std::string plane_id;
  std::optional<PlaneFrame> plane_frame;
  std::string profile_kind;
  std::vector<SketchProfilePoint> profile_points;
  std::vector<std::vector<SketchProfilePoint>> inner_loops;
  std::string path_sketch_feature_id;
  std::string path_entity_id;
  double path_start_x;
  double path_start_y;
  double path_start_z;
  double path_end_x;
  double path_end_y;
  double path_end_z;
  std::vector<PathSegment> path_segments;
};

struct HoleFeatureParameters {
  std::string target_body_id;
  std::string source_face_id;
  PlaneFrame plane_frame;
  double center_x = 0.0;
  double center_y = 0.0;
  // "simple", "counterbore", "countersink", or "spotface".
  std::string hole_type = "simple";
  // "blind" or "through_all".
  std::string extent_type = "blind";
  double diameter = 5.0;
  double depth = 10.0;
  double counterbore_diameter = 8.0;
  double counterbore_depth = 2.0;
  double countersink_diameter = 8.0;
  double countersink_angle_degrees = 82.0;
  // "custom", "metric", or "imperial".
  std::string standard = "custom";
  std::string standard_size;
  // "clearance", "tap_drill", or "threaded".
  std::string hole_fit = "clearance";
  bool thread_enabled = false;
  std::string thread_spec;
  double thread_pitch = 0.0;
  double major_diameter = 0.0;
  double minor_diameter = 0.0;
  double thread_depth = 10.0;
  // "cosmetic" or "modeled".
  std::string thread_representation = "cosmetic";
  bool is_pending = false;
};

struct HelixFeatureParameters {
  std::string axis_source_id;
  double axis_start_x = 0.0;
  double axis_start_y = 0.0;
  double axis_start_z = 0.0;
  double axis_end_x = 0.0;
  double axis_end_y = 0.0;
  double axis_end_z = 1.0;
  double radius = 2.5;
  double pitch = 1.0;
  double height = 10.0;
  double turns = 10.0;
  std::string handedness = "right";
  double start_angle_degrees = 0.0;
  std::vector<double> points;
};

struct ThreadFeatureParameters {
  std::string target_body_id;
  std::string axis_source_id;
  std::string mode = "external";
  std::string standard = "custom";
  std::string size;
  double major_diameter = 5.0;
  double minor_diameter = 4.0;
  double pitch = 0.8;
  double length = 10.0;
  double thread_angle_degrees = 60.0;
  double start_offset = 0.0;
  std::string handedness = "right";
  std::string representation = "cosmetic";
  bool is_pending = false;
};

struct FastenerFeatureParameters {
  std::string standard = "metric";
  std::string size = "M5";
  double diameter = 5.0;
  double minor_diameter = 4.2;
  double pitch = 0.8;
  double length = 20.0;
  double thread_length = 16.0;
  std::string head_type = "socket_head";
  std::string drive_type = "hex_socket";
  std::string thread_representation = "cosmetic";
};

struct MoveFeatureParameters {
  std::string target_body_id;
  double translation_x = 0.0;
  double translation_y = 0.0;
  double translation_z = 0.0;
  double rotation_x_degrees = 0.0;
  double rotation_y_degrees = 0.0;
  double rotation_z_degrees = 0.0;
  bool is_pending = false;
};

struct BodyCopyFeatureParameters {
  std::string source_body_id;
  std::string copy_mode = "linked";
  std::string source_body_name;
  std::string serialized_shape;
  double local_x_axis_x = 1.0;
  double local_x_axis_y = 0.0;
  double local_x_axis_z = 0.0;
  double local_y_axis_x = 0.0;
  double local_y_axis_y = 1.0;
  double local_y_axis_z = 0.0;
  double local_z_axis_x = 0.0;
  double local_z_axis_y = 0.0;
  double local_z_axis_z = 1.0;
};

// Edge-modifying body operation. `target_body_id` is the body root feature
// id whose edges are being filleted/chamfered. `edge_ids` mirrors the
// `<body_id>:edge:<index>` strings emitted by viewport_state.edges so the
// body_compiler can re-resolve the edges via TopExp::MapShapes on the
// target body shape at the moment the feature is replayed.
struct FilletFeatureParameters {
  std::string target_body_id;
  std::vector<std::string> edge_ids;
  double radius;
  // True while the floating panel is open and the user is still picking
  // edges / dialing in the radius. Body compiler still applies the
  // fillet so the user gets a real geometry preview, but it ALSO
  // retains the pre-fillet shape on the compiled body so viewport edge
  // ids stay stable during the session — picks resolve against the
  // pre-fillet topology, sidestepping the index reshuffle that would
  // otherwise happen every time OCCT mutates the shape. Flipped to
  // false by `confirm_fillet`, after which body_compiler behaves as
  // before. Defaults false so persisted documents keep the same
  // semantics they had before pending was introduced.
  bool is_pending = false;
};

struct ChamferFeatureParameters {
  std::string target_body_id;
  std::vector<std::string> edge_ids;
  // Symmetric chamfer distance from the edge along both adjacent faces.
  double distance;
  // See FilletFeatureParameters::is_pending — same semantics.
  bool is_pending = false;
};

struct ShellFeatureParameters {
  std::string target_body_id;
  std::vector<std::string> removed_face_ids;
  double thickness;
  // Same pending semantics as fillet/chamfer. While the panel is open
  // the viewport can keep selection stable against the pre-shell body.
  bool is_pending = false;
};

// Parametric offset construction plane.
//
// `plane_type` identifies the construction-plane recipe:
//   * "offset" — offset from `source_plane_id`.
//   * "midplane" — halfway between `source_plane_ids[0]` and
//     `source_plane_ids[1]`.
//   * "tangent" — tangent to `source_plane_id`, which is expected to
//     be a body face id.
//
// Source ids mirror what the rest of the codebase calls a
// "selectable plane":
//   * "ref-plane-xy", "ref-plane-yz", "ref-plane-xz" — origin planes.
//   * Another construction-plane feature id ("feature-N") — chained
//     offsets work for free, since each construction plane resolves
//     into a real `PlaneFrame` during recompute.
//   * A sketch profile id — resolves to its owning sketch plane,
//     centered on the profile region.
//   * A planar body face id of the form "<body_id>:face:<index>" —
//     the dependency walker re-resolves the face frame against the
//     compiled body before re-deriving this plane.
//
// `offset` is a signed distance along the source's normal in the
// document's units (mm at v1).
//
// `plane_frame` is the cached world-space frame, kept on the feature
// so consumers (viewport, sketches placed on this plane, etc.) can
// read it without re-walking history. The frame is rewritten in
// `refresh_history_dependencies` after every geometry edit, so it's
// always coherent with the current document state.
struct ConstructionPlaneFeatureParameters {
  std::string plane_type = "offset";
  std::string source_plane_id;
  std::vector<std::string> source_plane_ids;
  std::string source_axis_id;
  double offset;
  double angle_degrees = 0.0;
  PlaneFrame plane_frame;
};

struct ConstructionAxisFeatureParameters {
  std::string source_id;
  std::string source_kind;
  double start_x = 0.0;
  double start_y = 0.0;
  double start_z = 0.0;
  double end_x = 0.0;
  double end_y = 0.0;
  double end_z = 0.0;
};

struct ConstructionPointFeatureParameters {
  std::string source_id;
  std::string source_kind;
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
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

// Live link between a body face / edge / vertex and the sketch
// entities that the Project tool generated from it. Stored on the
// sketch so that `refresh_sketch_projections` (run as part of the
// dependency walker before the sketch's derived-state pass) can
// re-resolve the source on every recompute and patch the cached
// coords on the matching `lines` / `circles` / `arcs` /
// `projected_points` entries in place. End result: editing an
// upstream feature whose body the projection points at moves the
// projected geometry in lockstep, mirroring mainstream CAD's behaviour.
//
// `source_kind` mirrors the topology id's middle segment for body
// projections ("face", "edge", "vertex"); sketch profile projections
// use "profile" for UI identity. `generated_*` ids are the entity ids
// the project methods minted; the refresher walks them by id,
// finds the entity in the sketch, and rewrites its coords.
//
// `dependency_broken` is true when the most recent refresh failed
// to resolve the source (body deleted, edge curve type changed
// into something we can't project, etc.). The generated entities
// are left frozen at their last-known coords and the parent
// feature surfaces a warning via the existing `dependency_broken`
// machinery.
struct SketchProjection {
  std::string id;
  std::string source_id;
  std::string source_kind; // "face" | "edge" | "vertex" | "profile"
  std::vector<std::string> generated_line_ids;
  std::vector<std::string> generated_circle_ids;
  std::vector<std::string> generated_arc_ids;
  // For vertex projections only — the `SketchProjectedPoint::id`
  // that was minted. Empty for face / edge projections.
  std::string generated_point_id;
  bool dependency_broken = false;
  std::string dependency_warning;
};

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
};

// A point anchored to the midpoint of a line. The anchored point is
// also (typically) an endpoint of some other line; the solver pulls
// that endpoint to (start+end)/2 of the host line on every edit so
// the relation stays satisfied. Created automatically when the user
// snaps a sketch line endpoint to a midpoint snap target.
struct SketchMidpointAnchor {
  std::string id;
  std::string point_id;
  std::string line_id;
};

// A point anchored to the body of a line (not just its midpoint).
// The solver re-projects the bound point onto the host line on every
// edit, parametrized by `t` in [0, 1]. Created automatically when the
// user starts/ends a draft on another line's body via the line-body
// snap. Distinct from `SketchMidpointAnchor`, which is a degenerate
// special case at t=0.5.
struct SketchPointLineAnchor {
  std::string id;
  std::string point_id;
  std::string line_id;
  // Stored fraction along the host line at the time the anchor was
  // created. The solver uses this to keep the bound point at the
  // same relative position even when the host line moves; without
  // it, every solve would re-project to the closest point on the
  // moving line, which can drift.
  double t;
};

// A general geometric or dimensional constraint between sketch
// entities or points. Used for constraint types not yet covered by
// `SketchLineRelation` (equal_length, perpendicular) or inline
// `SketchLine.constraint` (horizontal, vertical) — e.g. concentric,
// symmetric, or coincident constraints created by the inference
// engine at entity-commit time.
struct SketchConstraint {
    std::string constraint_id;       // "constraint-{N}"
    std::string kind;                // "coincident", "concentric", ...
    std::vector<std::string> target_ids;
    double value = 0.0;              // for dimensional constraints
    bool driven = false;             // true = reference only
};

// User-configurable selection filter that controls which geometric
// element types are visible, selectable, snappable, and constrainable.
// Stored per-session (persisted to localStorage on the TS side in v1).
struct SelectionFilter {
    // Sketch geometry toggles
    bool select_curves        = true;
    bool select_points        = true;
    bool select_construction  = false;
    bool select_constraints   = true;

    // Snap toggles (derived from selection filter)
    bool snap_endpoint        = true;
    bool snap_midpoint        = true;
    bool snap_center          = true;
    bool snap_intersection    = true;
    bool snap_nearest         = true;
    bool snap_quadrant        = true;
    bool snap_perpendicular   = true;
    bool snap_parallel        = true;
    bool snap_tangent         = true;
    bool snap_grid            = true;
    bool snap_grid_line       = true;
    bool snap_polar           = true;

    // Global settings
    int tolerance_px           = 10;
    int polar_angle_degrees    = 15;
    std::vector<std::string> snap_priority;
    bool magnetic_pull         = true;
};

struct SketchLineRelation {
  std::string id;
  std::string kind;
  std::string first_line_id;
  std::string second_line_id;
};

// 2D parametric corner fillet on the sketch plane. Replaces a corner
// formed by two intersecting `SketchLine`s with a tangent
// `SketchArc`, trimming each line back to its tangent point. The
// fillet is parametric: lines A and B store their *trim* endpoint
// (not the original corner) and the arc's two endpoints, but the
// `corner_point_id` is preserved so the relationship can be
// re-solved on every recompute (when one of the lines moves) and
// fully reverted on `delete_sketch_fillet`.
//
// Lifecycle:
//   - Create: `add_sketch_fillet(corner, line_a, line_b, radius)`
//     allocates the two trim points + the arc, mutates lines A and
//     B to swap `corner_point_id` for the new trim point ids, and
//     appends a SketchFillet record.
//   - Recompute: every `refresh_sketch_derived_state` runs
//     `enforce_sketch_fillets`, which re-derives the trim positions
//     and arc parameters from the current line endpoints. This
//     keeps the fillet tangent when the user drags the line on the
//     opposite end ("far" endpoint) of either filleted line.
//   - Edit radius: `update_sketch_fillet_radius` rewrites `radius`
//     and re-runs the recompute pass.
//   - Delete: `delete_sketch_fillet` restores each line's filleted
//     endpoint back to `corner_point_id`, removes the arc + trim
//     points + the record itself, leaving the original corner
//     intact.
//
// v1 limitation: only line-line fillets (no line-arc, arc-arc).
struct SketchFillet {
  std::string id;
  // The pre-fillet shared corner. The fillet keeps this point alive
  // in `parameters.points` (re-emitted by `rebuild_sketch_points`)
  // even when no other line / arc still references it, because
  // delete needs to restore it as the lines' shared endpoint.
  std::string corner_point_id;
  // Cached corner coords, updated by `enforce_sketch_fillets` on
  // every recompute. Without these the point would have nowhere to
  // pull its coords from once both filleted lines have been
  // mutated to reference the trim points instead.
  double corner_x;
  double corner_y;
  std::string line_a_id;
  std::string line_b_id;
  // Generated geometry — owned by the fillet, regenerated by the
  // recompute pass on every sketch edit.
  std::string trim_a_point_id;
  std::string trim_b_point_id;
  std::string arc_id;
  double radius;
};

struct SketchProfileRegion {
  std::string id;
  std::string kind;
  std::vector<std::string> point_ids;
  std::vector<std::string> line_ids;
  std::vector<SketchProfilePoint> points;
  // Inner loops cut out of this profile region. v1 uses this for the
  // common "circle inside polygon" case so selecting the outer area
  // extrudes a face with a circular hole, while selecting the circle
  // separately extrudes the disk.
  std::vector<std::vector<SketchProfilePoint>> inner_loops;
  std::optional<std::string> source_circle_id;
  double center_x;
  double center_y;
  double radius;
};

struct SketchFeatureParameters {
  struct SketchPlaneFrame {
    double origin_x;
    double origin_y;
    double origin_z;
    double x_axis_x;
    double x_axis_y;
    double x_axis_z;
    double y_axis_x;
    double y_axis_y;
    double y_axis_z;
    double normal_x;
    double normal_y;
    double normal_z;
  };

  std::string plane_id;
  std::optional<SketchPlaneFrame> plane_frame;
  std::string active_tool;
  std::vector<SketchLine> lines;
  std::vector<SketchCircle> circles;
  std::vector<SketchPolygon> polygons;
  std::vector<SketchArc> arcs;
  std::vector<SketchPoint> points;
  std::vector<SketchDimension> dimensions;
  std::vector<SketchLineRelation> line_relations;
  std::vector<SketchConstraint> constraints;
  std::vector<SketchMidpointAnchor> midpoint_anchors;
  std::vector<SketchPointLineAnchor> point_line_anchors;
  // Parametric corner fillets. Each entry's `arc_id` and trim point
  // ids reference real entities in `arcs` / `points`; the recompute
  // pass keeps those entities in sync with the fillet's `radius` and
  // the current line endpoints.
  std::vector<SketchFillet> fillets;
  // Free-standing points placed by the Project tool (one per
  // projected body vertex). Re-emitted into `points` by every
  // `rebuild_sketch_points` pass with `kind = "projected"` and
  // `is_fixed = true` so the user can't drag them; deduplicated by
  // `source_id` so a second click on the same vertex is a no-op.
  std::vector<SketchProjectedPoint> projected_points;
  // Live links between body sources (face / edge / vertex) and the
  // sketch entities the Project tool generated from them. Walked
  // by `refresh_sketch_projections` on every recompute so that
  // editing the upstream geometry moves the projected lines /
  // circles / arcs / points in lockstep. Doubles as the dedup
  // index for the Project tool: a second click on the same source
  // is a no-op when an entry with that `source_id` already exists.
  std::vector<SketchProjection> projections;
  // Legacy ids-only field. Kept only for backwards-compatible
  // deserialization of older `.polysmith` documents — those saves
  // didn't record per-projection generated entity ids, so they
  // can't participate in live linking until re-projected. New
  // project actions only push to `projections`; this vector is
  // never read at runtime.
  std::vector<std::string> projected_sources;
  std::vector<SketchProfileRegion> profiles;

  // Transient state for an in-progress Mirror tool invocation.
  // Lives on the sketch only between `start_mirror_preview` and
  // either `commit_mirror_preview` (the geometry becomes real) or
  // `cancel_mirror_preview` (the geometry is discarded).
  //
  // The generated geometry is kept *separate* from the main
  // `lines`/`circles` arrays so that:
  //   - dimensions, points, and relations don't get polluted by
  //     entities the user might back out of,
  //   - regenerating after each parameter change is just a clear
  //     and rebuild — no risk of leaving orphan dimensions.
  // On commit, the generated entities are folded back into the
  // main arrays via `add_sketch_line`/`add_sketch_circle` so they
  // pick up dimensions and constraint inference normally.
  struct PendingMirror {
    std::optional<std::string> axis_line_id;
    std::vector<std::string> object_ids;
    std::vector<SketchLine> generated_lines;
    std::vector<SketchCircle> generated_circles;
  };
  std::optional<PendingMirror> pending_mirror;

  // DOF count from the planegcs solver after the last solve.
  // -1 = solver hasn't run yet (or no constraints exist).
  int solver_dofs = -1;
};

struct FeatureEntry {
  std::string id;
  std::string kind;
  std::string name;
  std::string status;
  std::string parameters_summary;
  // When true, the feature is excluded from body compilation and from
  // legacy primitive emission. The feature still appears in the
  // timeline / hierarchy (rendered dimmed by the UI) and can be
  // unsuppressed later. Downstream features that reference a
  // suppressed parent (e.g. an extrude whose sketch is suppressed)
  // silently no-op via the existing "missing input" fallbacks.
  bool suppressed = false;
  // Set by `refresh_history_dependencies` when this feature references
  // upstream geometry (a face-based sketch plane, an extrude on a
  // sketch, etc.) that can no longer be resolved against the current
  // document state — e.g. the original face was consumed by a later
  // boolean cut. The frame stays at its last-known value so the UI
  // still has something to render; the timeline surfaces the warning
  // via this flag plus the message below.
  bool dependency_broken = false;
  // Human-readable explanation of the broken dependency (shown as the
  // tooltip on the warning-coloured timeline button). Empty when
  // `dependency_broken` is false.
  std::string dependency_warning;
  std::optional<BoxFeatureParameters> box_parameters;
  std::optional<CylinderFeatureParameters> cylinder_parameters;
  std::optional<ExtrudeFeatureParameters> extrude_parameters;
  std::optional<SketchFeatureParameters> sketch_parameters;
  std::optional<FilletFeatureParameters> fillet_parameters;
  std::optional<ChamferFeatureParameters> chamfer_parameters;
  std::optional<ShellFeatureParameters> shell_parameters;
  std::optional<ConstructionPlaneFeatureParameters> construction_plane_parameters;
  std::optional<ConstructionAxisFeatureParameters> construction_axis_parameters;
  std::optional<ConstructionPointFeatureParameters> construction_point_parameters;
  std::optional<LoftFeatureParameters> loft_parameters;
  std::optional<RevolveFeatureParameters> revolve_parameters;
  std::optional<SweepFeatureParameters> sweep_parameters;
  std::optional<HoleFeatureParameters> hole_parameters;
  std::optional<HelixFeatureParameters> helix_parameters;
  std::optional<ThreadFeatureParameters> thread_parameters;
  std::optional<FastenerFeatureParameters> fastener_parameters;
  std::optional<MoveFeatureParameters> move_parameters;
  std::optional<BodyCopyFeatureParameters> body_copy_parameters;
};

}  // namespace polysmith::core
