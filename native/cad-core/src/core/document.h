#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/box_feature.h"
#include "core/appearance.h"
#include "core/cylinder_feature.h"
#include "core/export.h"
#include "core/extrude_feature.h"
#include "core/feature.h"
#include "core/loft_feature.h"
#include "core/parameter.h"
#include "core/revolve_feature.h"
#include "core/sketch_profile.h"
#include "core/sketch_feature.h"
#include "core/sweep_feature.h"

namespace polysmith::core {

// Lightweight result for drag_sketch_point — avoids returning full
// DocumentState on every pointer-move during an endpoint drag.
struct DragPointResult {
  double x;
  double y;
  std::optional<std::string> snap_label;
  std::optional<std::string> host_entity_id;
  std::optional<std::string> host_point_id;
};

struct DocumentState {
  std::string id;
  std::string name;
  std::string units;
  int revision;
  std::optional<std::string> selected_feature_id;
  std::optional<std::string> selected_reference_id;
  std::optional<std::string> selected_face_id;
  // Edge selection is a (possibly empty) ordered list rather than a
  // single optional id, so multi-edge fillet / chamfer selection is
  // first-class. Order is insertion order: the most recent click is
  // appended (or removed, when toggling). All other selection
  // categories remain single-id; widening them later is a small
  // change but unnecessary for current tooling.
  std::vector<std::string> selected_edge_ids;
  // Vertex selection is also a list (same insertion-order semantics
  // as edges) so the UI can show "distance between two vertices" in
  // the selection readout. Anything beyond two is permitted at the
  // storage layer; consumers that only handle pairs (e.g. the
  // distance display) read the first two entries.
  std::vector<std::string> selected_vertex_ids;
  std::optional<std::string> active_sketch_plane_id;
  std::optional<std::string> active_sketch_face_id;
  std::optional<std::string> active_sketch_feature_id;
  std::optional<std::string> active_sketch_tool;
  std::optional<std::string> selected_sketch_point_id;
  std::optional<std::string> selected_sketch_entity_id;
  std::vector<std::string> selected_sketch_point_ids;
  std::vector<std::string> selected_sketch_entity_ids;
  std::optional<std::string> selected_sketch_dimension_id;
  std::optional<std::string> selected_sketch_profile_id;
  std::vector<std::string> selected_sketch_profile_ids;
  // Parametric timeline cursor. nullopt means the cursor is at the end
  // of history; otherwise the value is the number of non-root actions
  // included in the viewport rollback preview.
  std::optional<int> timeline_cursor;
  std::vector<FeatureEntry> feature_history;
  std::vector<ParameterEntry> parameters;
  DocumentAppearance appearance;
  SelectionFilter selection_filter;
};

struct SessionState {
  int document_count;
  std::optional<std::string> active_document_id;
  bool can_undo;
  bool can_redo;
};

class DocumentManager {
 public:
  DocumentState create_document();
  DocumentState add_box_feature(const BoxFeatureParameters& parameters);
  DocumentState add_cylinder_feature(const CylinderFeatureParameters& parameters);
  DocumentState update_box_feature(const std::string& feature_id,
                                   const BoxFeatureParameters& parameters);
  DocumentState update_cylinder_feature(
      const std::string& feature_id,
      const CylinderFeatureParameters& parameters);
  DocumentState update_extrude_depth(const std::string& feature_id,
                                     double depth);
  DocumentState update_extrude_mode(const std::string& feature_id,
                                    const std::string& mode);
  DocumentState update_extrude_target_body(
      const std::string& feature_id,
      const std::optional<std::string>& target_body_id);
  DocumentState update_extrude_parameters(
      const std::string& feature_id,
      const ExtrudeFeatureParameters& parameters);
  DocumentState update_extrude_profiles(
      const std::string& feature_id,
      const std::vector<std::string>& profile_ids);
  DocumentState loft_profiles(const std::vector<std::string>& profile_ids,
                              bool ruled = false);
  DocumentState update_loft_profiles(
      const std::string& feature_id,
      const std::vector<std::string>& profile_ids);
  DocumentState update_loft_ruled(const std::string& feature_id, bool ruled);
  DocumentState revolve_profile(const std::string& profile_id,
                                const std::string& axis_entity_id,
                                double angle_degrees = 360.0);
  DocumentState update_revolve_profile(const std::string& feature_id,
                                       const std::string& profile_id);
  DocumentState update_revolve_axis(const std::string& feature_id,
                                    const std::string& axis_entity_id);
  DocumentState update_revolve_angle(const std::string& feature_id,
                                     double angle_degrees);
  DocumentState sweep_profile(const std::string& profile_id,
                              const std::string& path_entity_id);
  DocumentState update_sweep_profile(const std::string& feature_id,
                                     const std::string& profile_id);
  DocumentState update_sweep_path(const std::string& feature_id,
                                  const std::string& path_entity_id);
  DocumentState rename_feature(const std::string& feature_id,
                               const std::string& name);
  // Toggle a feature's suppressed flag. Suppressed features are
  // excluded from body compilation (see body_compiler.cpp) and legacy
  // primitive emission (viewport.cpp), but remain in feature_history
  // so they can be reactivated. Throws if the id doesn't exist or
  // refers to the root feature, which can't be suppressed.
  DocumentState set_feature_suppressed(const std::string& feature_id,
                                       bool suppressed);
  DocumentState delete_feature(const std::string& feature_id);
  DocumentState undo();
  DocumentState redo();
  DocumentState set_timeline_cursor(int included_action_count);
  DocumentState select_feature(const std::string& feature_id);
  DocumentState select_reference(const std::string& reference_id);
  DocumentState select_face(const std::string& face_id);
  // Select an edge. When `additive` is false the edge replaces the
  // current edge selection (the typical click). When true, the edge
  // is toggled into the current selection set (shift-click): if it's
  // already selected it is removed, otherwise appended. Other
  // selection categories (face / vertex / reference / feature) are
  // always cleared, matching the single-edge legacy behavior so a new
  // edge selection doesn't leave a stale face highlight.
  DocumentState select_edge(const std::string& edge_id, bool additive);
  // Same toggle / replace semantics as select_edge: shift-click adds
  // (or removes) the vertex from the current vertex selection set;
  // plain click replaces. Other selection categories are cleared.
  DocumentState select_vertex(const std::string& vertex_id, bool additive);
  // Create a fillet feature on one or more edges of an existing body.
  // The edge ids must already exist in the current viewport_state.edges
  // (the viewport is the source of truth for available edge ids); we
  // parse the body owner out of the first edge id and require every
  // edge to share the same owner. Default radius 1.0mm — the UI panel
  // updates it via update_fillet_radius for live preview.
  DocumentState create_fillet(const std::vector<std::string>& edge_ids,
                              double radius);
  DocumentState update_fillet_radius(const std::string& feature_id,
                                     double radius);
  // Replace the edge set on an existing fillet feature. Used by the
  // floating panel's "edit edges" interaction: the user clicks edges
  // in the viewport while the panel is open and the UI re-issues the
  // full set on each toggle. The new set must be non-empty and every
  // edge must belong to the feature's existing target body — adding
  // edges from a different body has no well-defined fillet semantics.
  DocumentState update_fillet_edges(const std::string& feature_id,
                                    const std::vector<std::string>& edge_ids);
  // Flip an in-progress fillet's `is_pending` flag to false. Called by
  // the UI's panel-Confirm path once the user is done picking edges /
  // dialing in the radius. After this point the body's edge identity
  // follows the post-fillet topology again. Does not push an undo
  // step — the original create_fillet's push covers the entire panel
  // session in one shot.
  DocumentState confirm_fillet(const std::string& feature_id);
  DocumentState create_chamfer(const std::vector<std::string>& edge_ids,
                               double distance);
  DocumentState update_chamfer_distance(const std::string& feature_id,
                                        double distance);
  DocumentState update_chamfer_edges(
      const std::string& feature_id,
      const std::vector<std::string>& edge_ids);
  // See `confirm_fillet` for semantics.
  DocumentState confirm_chamfer(const std::string& feature_id);
  DocumentState create_shell(const std::string& face_id, double thickness);
  DocumentState update_shell_thickness(const std::string& feature_id,
                                       double thickness);
  DocumentState confirm_shell(const std::string& feature_id);
  // Create a parametric offset construction plane. The source must
  // resolve via `resolve_plane_source_frame` (origin plane,
  // construction plane feature id, or "<body_id>:face:<index>"
  // planar face). The resulting feature is selected and shows up in
  // the timeline and the Construction hierarchy category.
  DocumentState create_offset_plane(const std::string& source_plane_id,
                                    double offset);
  DocumentState create_midplane(const std::string& first_source_id,
                                const std::string& second_source_id);
  DocumentState create_tangent_plane(const std::string& source_face_id);
  DocumentState create_angle_plane(const std::string& source_plane_id,
                                   const std::string& source_axis_id,
                                   double angle_degrees);
  DocumentState create_construction_axis(const std::string& source_id);
  DocumentState create_construction_point(const std::string& source_id);
  DocumentState create_hole(const std::string& face_id,
                            double center_x,
                            double center_y,
                            double center_z,
                            const HoleFeatureParameters& parameters);
  DocumentState update_hole_parameters(const std::string& feature_id,
                                       const HoleFeatureParameters& parameters);
  DocumentState confirm_hole(const std::string& feature_id);
  DocumentState create_helix(const std::string& axis_source_id,
                             const HelixFeatureParameters& parameters);
  DocumentState update_helix_parameters(const std::string& feature_id,
                                        const HelixFeatureParameters& parameters);
  DocumentState create_thread(const ThreadFeatureParameters& parameters);
  DocumentState update_thread_parameters(const std::string& feature_id,
                                         const ThreadFeatureParameters& parameters);
  DocumentState confirm_thread(const std::string& feature_id);
  DocumentState create_fastener(const FastenerFeatureParameters& parameters);
  DocumentState update_fastener_parameters(
      const std::string& feature_id,
      const FastenerFeatureParameters& parameters);
  DocumentState create_move(const std::string& target_body_id,
                            const MoveFeatureParameters& parameters);
  DocumentState update_move_parameters(const std::string& feature_id,
                                       const MoveFeatureParameters& parameters);
  DocumentState confirm_move(const std::string& feature_id);
  DocumentState create_body_copy(const std::string& source_body_id,
                                 const std::string& copy_mode);
  DocumentState unlink_body_copy(const std::string& feature_id);
  DocumentState set_body_color(const std::string& body_id,
                               const std::string& color);
  DocumentState set_face_color(const std::string& face_id,
                               const std::string& color);
  DocumentState clear_body_color(const std::string& body_id);
  DocumentState clear_face_color(const std::string& face_id);
  DocumentState clear_appearance_overrides();
  // Drive an existing construction plane's offset. The frame is
  // re-derived from the source's current frame, so chained planes
  // / face-based sources update correctly.
  DocumentState update_offset_plane(const std::string& feature_id,
                                    double offset);
  DocumentState update_angle_plane(const std::string& feature_id,
                                   double angle_degrees);
  DocumentState start_sketch_on_plane(const std::string& reference_id);
  DocumentState start_sketch_on_face(
      const std::string& face_id,
      const SketchFeatureParameters::SketchPlaneFrame& plane_frame);
  DocumentState set_sketch_tool(const std::string& tool);
  DocumentState update_sketch_line(const std::string& line_id,
                                   double start_x,
                                   double start_y,
                                   double end_x,
                                   double end_y);
  DocumentState update_sketch_point(const std::string& point_id,
                                    double x,
                                    double y);
  DragPointResult drag_sketch_point(const std::string& point_id,
                                     double cursor_x,
                                     double cursor_y);
  DocumentState set_sketch_line_constraint(
      const std::string& line_id,
      const std::optional<std::string>& constraint);
  DocumentState set_sketch_equal_length_constraint(
      const std::string& line_id,
      const std::optional<std::string>& other_line_id);
  DocumentState set_sketch_perpendicular_constraint(
      const std::string& line_id,
      const std::optional<std::string>& other_line_id);
  // Make a sketch line tangent to a circle. Pass an empty
  // `circle_id` to clear an existing tangent relation.
  DocumentState set_sketch_tangent_constraint(const std::string& line_id,
                                              const std::string& circle_id);
  // Mirror tool — contextual modeling pending preview lifecycle. Each
  // method maps directly onto the sketch_feature core ops; the
  // wrapping here is bookkeeping (undo, refresh, selection).
  // Note: only `commit` writes a permanent change, so it's the
  // only one that pushes an undo state.
  DocumentState start_mirror_preview();
  DocumentState update_mirror_preview_axis(const std::string& axis_line_id);
  DocumentState update_mirror_preview_objects(
      const std::vector<std::string>& object_ids);
  DocumentState commit_mirror_preview();
  DocumentState cancel_mirror_preview();
  DocumentState set_sketch_parallel_constraint(
      const std::string& line_id,
      const std::optional<std::string>& other_line_id);
  DocumentState clear_sketch_line_constraints(const std::string& line_id);
  DocumentState set_sketch_coincident_constraint(const std::string& point_id,
                                                 const std::string& other_point_id);
  DocumentState set_sketch_point_fixed(const std::string& point_id,
                                       bool is_fixed);
  DocumentState update_sketch_circle(const std::string& circle_id,
                                     double center_x,
                                     double center_y,
                                     double radius);
  // Add an angle dimension between two sketch lines that share an
  // endpoint. The dimension's `value` is initialized to the current
  // angle (radians); subsequent `update_sketch_dimension` calls
  // drive the second line's rotation about the shared endpoint.
  DocumentState add_sketch_angle_dimension(const std::string& first_line_id,
                                           const std::string& second_line_id);
  DocumentState add_sketch_distance_dimension(
      const std::string& first_entity_id,
      const std::string& second_entity_id);
  // Single-entity dimension creation through the Dimension tool.
  // Each method finds the active sketch feature, pushes an undo state,
  // delegates to the sketch_feature core helper, selects the new
  // dimension, and bumps the geometry revision.
  DocumentState add_sketch_line_length_dimension(const std::string& line_id);
  DocumentState add_sketch_circle_radius_dimension(
      const std::string& circle_id,
      std::optional<std::string> display_as = std::nullopt);
  DocumentState add_sketch_polygon_radius_dimension(
      const std::string& polygon_id);
  DocumentState add_sketch_point_distance_dimension(
      const std::string& point_a_id,
      const std::string& point_b_id);
  DocumentState update_sketch_dimension(const std::string& dimension_id,
                                        double value,
                                        std::optional<std::string> expression = std::nullopt);
  DocumentState update_sketch_dimension_label_position(
      const std::string& dimension_id,
      double label_x,
      double label_y);
  DocumentState select_sketch_profile(const std::string& profile_id,
                                      bool additive = false);
  DocumentState extrude_profile(
      const std::string& profile_id,
      double depth,
      const std::string& mode = "new_body",
      const std::optional<std::string>& target_body_id = std::nullopt,
      const std::optional<ExtrudeFeatureParameters>& parameters =
          std::nullopt);
  DocumentState extrude_profiles(
      const std::vector<std::string>& profile_ids,
      double depth,
      const std::string& mode = "new_body",
      const std::optional<std::string>& target_body_id = std::nullopt,
      const std::optional<ExtrudeFeatureParameters>& parameters =
          std::nullopt);
  DocumentState extrude_open_entities(
      const std::vector<std::string>& entity_ids,
      double depth,
      const std::string& mode = "new_body",
      const std::optional<std::string>& target_body_id = std::nullopt,
      const std::optional<ExtrudeFeatureParameters>& parameters =
          std::nullopt);
  DocumentState extrude_face(
      const std::string& face_id,
      double depth,
      const std::string& mode = "new_body",
      const std::optional<std::string>& target_body_id = std::nullopt,
      const std::optional<ExtrudeFeatureParameters>& parameters =
          std::nullopt);
  DocumentState add_sketch_line(double start_x,
                                double start_y,
                                double end_x,
                                double end_y,
                                bool is_construction = false);
  // Toggle the construction-line flag on an existing sketch line.
  // Construction lines render dashed and are skipped during profile
  // detection so they don't seal pickable faces.
  DocumentState set_sketch_line_construction(const std::string& line_id,
                                             bool is_construction);
  // Bind a sketch point to the midpoint of a host line; the solver
  // re-pulls the point on every edit. Pass an empty `host_line_id`
  // to remove an existing anchor.
  DocumentState set_sketch_midpoint_anchor(const std::string& point_id,
                                           const std::string& host_line_id);
  // Bind a sketch point to a host line's body at parametric position
  // `t` in [0, 1]. Pass an empty `host_line_id` to clear the anchor.
  DocumentState set_sketch_point_line_anchor(const std::string& point_id,
                                             const std::string& host_line_id,
                                             double t);
  DocumentState add_sketch_rectangle(double start_x,
                                     double start_y,
                                     double end_x,
                                     double end_y,
                                     bool is_construction = false);
  DocumentState add_sketch_circle(double center_x,
                                  double center_y,
                                  double radius,
                                  bool is_construction = false);
  // Add a regular polygon to the active sketch. mode is one of
  // "circumscribed", "inscribed", "edge".
  DocumentState add_sketch_polygon(int sides,
                                   const std::string& mode,
                                   double start_x,
                                   double start_y,
                                   double end_x,
                                   double end_y,
                                   bool is_construction = false);
  // Add an arc to the active sketch. `mode` selects how the three
  // input points are interpreted:
  //   - "three_point": (start, end, anchor) — anchor lies on the arc
  //     and fixes the bulge / radius. Center is the circumcenter of
  //     the three points.
  //   - "center_start_end": (start, end, anchor) — anchor is the
  //     arc's center. Radius derives from |center - start|; the end
  //     point is snapped onto the resulting circle so the arc closes
  //     cleanly.
  // Returns the document with the new arc and its endpoint points
  // appended.
  DocumentState add_sketch_arc(double start_x,
                               double start_y,
                               double end_x,
                               double end_y,
                               double anchor_x,
                               double anchor_y,
                               const std::string& mode,
                               bool is_construction = false);
  // Sketch fillet — round a corner shared by two sketch lines into
  // a tangent arc. The corner is identified by the sketch point id
  // shared by both lines; the relationship is parametric (see
  // `SketchFillet` in feature.h) so the fillet survives subsequent
  // sketch edits and has its own update / delete commands.
  DocumentState add_sketch_fillet(const std::string& corner_point_id,
                                  const std::string& line_a_id,
                                  const std::string& line_b_id,
                                  double radius);
  DocumentState update_sketch_fillet_radius(const std::string& fillet_id,
                                            double radius);
  DocumentState delete_sketch_fillet(const std::string& fillet_id);
  DocumentState delete_sketch_dimension(const std::string& dimension_id);
  DocumentState update_sketch_dimension_display(const std::string& dimension_id,
                                                const std::string& display_as);
  DocumentState delete_sketch_selection(
      const std::vector<std::string>& entity_ids,
      const std::vector<std::string>& point_ids,
      const std::vector<std::string>& profile_ids);
  DocumentState select_sketch_point(const std::string& point_id,
                                    bool additive = false);
  DocumentState select_sketch_entity(const std::string& entity_id,
                                     bool additive = false);
  DocumentState select_sketch_dimension(const std::string& dimension_id);
  DocumentState finish_sketch();
  DocumentState reenter_sketch(const std::string& feature_id);
  DocumentState project_face_into_sketch(const std::string& face_id);
  // Modal Project tool: dispatch one of these per click depending on
  // what the raycaster hit. Each routes through `project_*_into_sketch`
  // and shares the active-sketch validation + idempotency check (see
  // `projected_sources` / `projected_points`).
  DocumentState project_profile_into_sketch(const std::string& profile_id);
  DocumentState project_edge_into_sketch(const std::string& edge_id);
  DocumentState project_vertex_into_sketch(const std::string& vertex_id);
  DocumentState clear_selection();
  // Parameter CRUD — document-scoped named numeric parameters that can
  // be referenced by name in sketch dimension expressions.
  DocumentState add_parameter(const std::string& name,
                              const std::string& expression,
                              const std::string& kind = "length");
  DocumentState update_parameter(const std::string& name,
                                 const std::string& expression,
                                 const std::string& kind = "length");
  DocumentState delete_parameter(const std::string& name);

  // Trim a sketch entity at its intersection points. The entity is
  // split at all intersections with other non-construction entities and
  // only the segment under (click_x, click_y) is kept. Phase 1 handles
  // lines only. Single undoable action via full sketch snapshot.
  DocumentState trim_sketch_entity(const std::string& entity_id,
                                   double click_x,
                                   double click_y);

  // Update the per-session selection filter. The filter controls which
  // geometric element types are visible, selectable, snappable, and
  // constrainable. Stored on DocumentState for persistence in saved
  // files, but in v1 it's also settable per-session from the UI.
  DocumentState update_selection_filter(const SelectionFilter& filter);

  ExportResult export_document_as_step(const std::string& file_path) const;
  ExportResult export_document_as_stl(const std::string& file_path) const;
  ExportResult export_body_as_stl(const std::string& file_path,
                                  const std::string& body_id) const;
  void save_document_to_path(const std::string& file_path) const;
  DocumentState load_document_from_path(const std::string& file_path);
  std::optional<DocumentState> get_document() const;
  SessionState get_session_state() const;

 private:
  FeatureEntry make_root_feature();
  void require_document() const;
  void push_undo_state();
  void clear_redo_stack();
  // Re-resolve face-based plane frames, propagate broken-dependency
  // state, and bump the document revision. Use this from any mutator
  // that can affect upstream body geometry (extrudes, fillets/chamfers,
  // suppress, delete, undo/redo, sketch profile edits, etc.). Pure
  // selection mutators stick with the plain revision bump because the
  // refresh would be a no-op.
  void bump_geometry_revision();

  int next_document_id_ = 1;
  int next_feature_id_ = 1;
  int next_sketch_line_id_ = 1;
  int next_sketch_circle_id_ = 1;
  int next_sketch_polygon_id_ = 1;
  // Independent counter for arc ids ("arc-N"). Endpoint point ids
  // ("point-N") still come from `next_sketch_line_id_` so arc and
  // line endpoints share the same id space, keeping the points
  // graph uniform.
  int next_sketch_arc_id_ = 1;
  // Independent counter for sketch fillet ids ("fillet-N"). Trim
  // points and the generated arc still come from the existing line /
  // arc counters so the id namespaces stay uniform.
  int next_sketch_fillet_id_ = 1;
  // Independent counter for the Project tool's standalone projected
  // points ("projected-point-N"). Kept separate from the line / arc
  // endpoint counter so user-visible debug ids stay readable.
  int next_sketch_projected_point_id_ = 1;
  // Counter for `SketchProjection` record ids ("projection-N"). The
  // record itself isn't user-visible (it's an internal link between
  // a body source and the sketch entities it generated), but a
  // monotonic id keeps debugging and serialization predictable.
  int next_sketch_projection_id_ = 1;
  int document_count_ = 0;
  std::optional<DocumentState> document_;
  std::vector<DocumentState> undo_stack_;
  std::vector<DocumentState> redo_stack_;
};

}  // namespace polysmith::core
