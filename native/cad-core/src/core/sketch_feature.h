#pragma once

#include "core/feature.h"
#include "core/parameter.h"

namespace polysmith::core {

FeatureEntry create_sketch_feature(
    int feature_index,
    const std::string& plane_id,
    std::optional<SketchFeatureParameters::SketchPlaneFrame> plane_frame = std::nullopt);
// Re-run the post-edit sketch recompute pipeline (anchors, fillets,
// rebuild_sketch_points, projected-point lock, profile detection).
// Most public mutators in sketch_feature.cpp call this internally; the
// Project tool's `project_vertex_into_sketch` path needs to call it
// directly because it appends to `projected_points` *after* every
// mutator has already returned.
void refresh_sketch_derived_state(FeatureEntry& feature);
void set_sketch_tool(FeatureEntry& feature, const std::string& tool);
void update_sketch_line(FeatureEntry& feature,
                        const std::string& line_id,
                        double start_x,
                        double start_y,
                        double end_x,
                        double end_y);
void update_sketch_point(FeatureEntry& feature,
                         const std::string& point_id,
                         double x,
                         double y);
void set_sketch_line_constraint(FeatureEntry& feature,
                                const std::string& line_id,
                                const std::optional<std::string>& constraint);
void set_sketch_equal_length_constraint(
    FeatureEntry& feature,
    const std::string& line_id,
    const std::optional<std::string>& other_line_id);
void set_sketch_perpendicular_constraint(
    FeatureEntry& feature,
    const std::string& line_id,
    const std::optional<std::string>& other_line_id);
void set_sketch_parallel_constraint(
    FeatureEntry& feature,
    const std::string& line_id,
    const std::optional<std::string>& other_line_id);
// Clear all constraints on a sketch line: both the inline H/V
// constraint and every line‑line relation (equal‑length,
// perpendicular, parallel, tangent) that involves this line.
// Dimensions and anchors are left untouched.
void clear_sketch_line_constraints(FeatureEntry& feature,
                                   const std::string& line_id);
void set_sketch_coincident_constraint(
    FeatureEntry& feature,
    const std::string& point_id,
    const std::string& other_point_id);
void set_sketch_point_fixed(FeatureEntry& feature,
                            const std::string& point_id,
                            bool is_fixed);
void update_sketch_circle(FeatureEntry& feature,
                          const std::string& circle_id,
                          double center_x,
                          double center_y,
                          double radius);
// Add a new "angle" dimension between two sketch lines that share an
// endpoint. The dimension is stored with `entity_id = first_line_id`,
// `secondary_entity_id = second_line_id`, and `value` initialized to
// the current angle (radians) between their outgoing direction
// vectors. Throws if the two lines do not share an endpoint within
// the coincident tolerance.
void add_sketch_angle_dimension(FeatureEntry& feature,
                                const std::string& first_line_id,
                                const std::string& second_line_id);
void add_sketch_distance_dimension(FeatureEntry& feature,
                                   const std::string& first_entity_id,
                                   const std::string& second_entity_id);
void update_sketch_dimension(FeatureEntry& feature,
                             const std::string& dimension_id,
                             double value,
                             std::optional<std::string> expression = std::nullopt);
// Re-evaluate every dimension that has a non-empty `expression` field
// against the given parameter table. Dimension values are overwritten
// with the resolved result; dimensions whose expressions fail to
// resolve keep their last good value silently.
void reify_dimension_expressions(
    FeatureEntry& feature,
    const std::vector<struct ParameterEntry>& parameters);
void add_sketch_line(FeatureEntry& feature,
                     int line_index,
                     double start_x,
                     double start_y,
                     double end_x,
                     double end_y,
                     bool is_construction = false,
                     bool snap_start = true);
void add_sketch_rectangle(FeatureEntry& feature,
                          int& next_line_index,
                          double start_x,
                          double start_y,
                          double end_x,
                          double end_y,
                          bool is_construction = false);
// Toggle the construction-line flag on an existing line. Construction
// lines render dashed in the viewport and are filtered out of profile
// detection so they don't seal pickable faces / extrude sources.
void set_sketch_line_construction(FeatureEntry& feature,
                                  const std::string& line_id,
                                  bool is_construction);
// Bind a sketch point to the midpoint of a host line. The point is
// pulled to the line's midpoint immediately and re-pulled on every
// subsequent edit (via `enforce_midpoint_anchors` inside the derived
// state refresh). Pass an empty `host_line_id` to remove an existing
// anchor for the point.
void set_sketch_midpoint_anchor(FeatureEntry& feature,
                                const std::string& point_id,
                                const std::string& host_line_id);
// Bind a sketch point to a host line's body at parametric position
// `t` in [0, 1]. The solver re-projects the bound point on every
// edit so it rides along with the host. Pass an empty
// `host_line_id` to remove an existing anchor for the point.
void set_sketch_point_line_anchor(FeatureEntry& feature,
                                  const std::string& point_id,
                                  const std::string& host_line_id,
                                  double t);

// Constrain a sketch line to be tangent to a circle. The relation
// is one-directional: the line's *end* point is driven onto the
// closer of the two tangent points from its start to the circle.
// Pass an empty `circle_id` to clear any existing tangent relation
// for the line. Throws when the line's start is inside or on the
// circle (no real tangent exists).
void set_sketch_tangent_constraint(FeatureEntry& feature,
                                   const std::string& line_id,
                                   const std::string& circle_id);

void add_sketch_circle(FeatureEntry& feature,
                       int circle_index,
                       double center_x,
                       double center_y,
                       double radius,
                       bool is_construction = false);

// Build a regular polygon on the sketch. mode is "circumscribed",
// "inscribed", or "edge". For circumscribed/inscribed the polygon is
// computed from (center, radius, sides). For edge mode the polygon is
// computed from (start, end, sides) and center/radius are derived.
void add_sketch_polygon(FeatureEntry& feature,
                        int polygon_index,
                        int sides,
                        const std::string& mode,
                        double start_x,
                        double start_y,
                        double end_x,
                        double end_y,
                        bool is_construction = false);

// Build a SketchArc on the feature. Caller is expected to have already
// computed the arc's center, radius, and ccw direction from whichever
// creation flow they ran (three-point or center+start+end). The arc's
// endpoint points are added to the points list as kind="endpoint" and
// flagged is_fixed=true so v1 keeps the cached params authoritative.
// `start_point_index` / `end_point_index` are the trailing integers
// used to mint the synthesized point ids; they're sourced from the
// shared sketch line counter so arc endpoints can't collide with
// line endpoints.
void add_sketch_arc(FeatureEntry& feature,
                    int arc_index,
                    int start_point_index,
                    int end_point_index,
                    double start_x,
                    double start_y,
                    double end_x,
                    double end_y,
                    double center_x,
                    double center_y,
                    double radius,
                    bool ccw,
                    bool is_construction = false);

// ---------------------------------------------------------------
// Sketch fillet — parametric corner fillet between two sketch lines.
//
// Round a corner shared by two `SketchLine`s by trimming each line
// back to its tangent point and inserting a tangent `SketchArc`
// between them. The relationship is recorded as a `SketchFillet` on
// the sketch so the result stays parametric: the recompute pass
// re-derives the trim positions whenever the user moves either
// line, and `update_sketch_fillet_radius` rewrites the radius and
// re-runs the recompute. `delete_sketch_fillet` restores the
// original corner.
//
// Validation in `add_sketch_fillet` is strict because a malformed
// fillet would corrupt the lines:
//   - both lines must reference `corner_point_id` as one of their
//     endpoints
//   - the two lines must not be parallel (cross product within
//     `kCoincidentTolerance`)
//   - the trim distance `r / tan(theta/2)` must fit on each line
//   - neither line may already be filleted at this corner
// ---------------------------------------------------------------
void add_sketch_fillet(FeatureEntry& feature,
                       int fillet_index,
                       int trim_a_point_index,
                       int trim_b_point_index,
                       int arc_index,
                       const std::string& corner_point_id,
                       const std::string& line_a_id,
                       const std::string& line_b_id,
                       double radius);
void update_sketch_fillet_radius(FeatureEntry& feature,
                                 const std::string& fillet_id,
                                 double radius);
void delete_sketch_fillet(FeatureEntry& feature,
                          const std::string& fillet_id);
void delete_sketch_dimension(FeatureEntry& feature,
                             const std::string& dimension_id);

// Single-entity dimension creation (for the Dimension tool clicking a
// lone line, circle, or polygon that has no auto-dimension — e.g.
// because the fusion-style on-demand system deleted it). Each function
// validates the target entity exists, checks that the dimension id
// isn't already in use, and pushes a SketchDimension computed from the
// entity's current geometry.
void add_sketch_line_length_dimension(FeatureEntry& feature,
                                      const std::string& line_id);
void add_sketch_circle_radius_dimension(FeatureEntry& feature,
                                        const std::string& circle_id,
                                        std::optional<std::string> display_as = std::nullopt);
void add_sketch_polygon_radius_dimension(FeatureEntry& feature,
                                         const std::string& polygon_id);
void add_sketch_point_distance_dimension(FeatureEntry& feature,
                                         const std::string& point_a_id,
                                         const std::string& point_b_id);

// ---------------------------------------------------------------
// Mirror tool — contextual modeling pending preview lifecycle.
//
// The mirror feature follows the canonical action pattern (see
// `wiki/polysmith.wiki/Contextual-Modeling-Workflow.md`):
//   1. `start_mirror_preview` — opens a transient `pending_mirror`
//      with empty selections. No geometry yet.
//   2. `update_mirror_preview_axis` / `update_mirror_preview_objects`
//      — set parameters; preview geometry is regenerated each time
//      so the viewport snapshot reflects the current selection.
//   3. `commit_mirror_preview` — moves the generated geometry into
//      the sketch's main arrays as real entities (with dimensions
//      and inferred constraints), and clears `pending_mirror`.
//   4. `cancel_mirror_preview` — discards the generated geometry
//      and clears `pending_mirror`. No state escapes the tool.
// ---------------------------------------------------------------
// Sketch trim tool — line trimming (Phase 1).
//
// Given a sketch entity id and a click position in sketch-local
// coordinates, find all intersections between the target entity and
// other non-construction sketch entities, split the target at the
// intersection points, identify the segment under the click, and
// update the target to keep only that segment. The trimmed-away
// portions are discarded. Phase 1 handles lines only.
//
// Throws when the entity is not found or the click position does not
// correspond to any segment. Refresh is NOT called here — the
// caller (document layer) handles that.
void trim_sketch_entity(FeatureEntry& feature,
                        const std::string& entity_id,
                        double click_x,
                        double click_y);

// ---------------------------------------------------------------
void start_mirror_preview(FeatureEntry& feature);
void update_mirror_preview_axis(FeatureEntry& feature,
                                const std::string& axis_line_id);
void update_mirror_preview_objects(
    FeatureEntry& feature,
    const std::vector<std::string>& object_ids);
void commit_mirror_preview(FeatureEntry& feature,
                           int& next_line_index,
                           int& next_circle_index);
void cancel_mirror_preview(FeatureEntry& feature);

}  // namespace polysmith::core
