#pragma once

#include <optional>
#include <string>
#include <vector>
#include "core/sketch/sketch_constraint_types.h"
#include "core/sketch/sketch_dimension_types.h"
#include "core/sketch/sketch_fillet_types.h"
#include "core/sketch/sketch_geometry_types.h"
#include "core/sketch/sketch_mirror_types.h"
#include "core/sketch/sketch_profile_types.h"
#include "core/sketch/sketch_projection_types.h"

namespace polysmith::core {

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

  // Whether the last solver pass converged (Success or Converged).
  // When false, the solver failed to find a solution — typically
  // because the constraint system is infeasible (contradictory
  // dimension values) rather than over-constrained.
  bool solver_ok = true;

  // Solver diagnostics from the last solve. Populated by
  // refresh_sketch_derived_state after each solver pass.
  // -1 = solver hasn't run or has no conflicts.
  int solver_conflicting_count = -1;
  int solver_redundant_count = -1;

  // Persistent mirror relations. When the user commits a mirror with
  // the "persistent" toggle on, each source→mirror pair is stored
  // here so it can be re-mirrored on every recompute and shown as a
  // constraint badge. Empty vector = no persistent mirrors.
  std::vector<SketchMirrorRelation> mirror_relations;

  // --- Append-mode freeze state machine (transient) ---
  // Set by mutators BEFORE calling refresh_sketch_derived_state when
  // the mutation should only affect a subset of the sketch (new entity
  // creation, constraint application). When non-empty:
  //   1. All existing points are temporarily frozen (is_fixed = true)
  //   2. Points referenced by these focus entity IDs are unfrozen
  //   3. Solver runs — only focus entities move
  //   4. Original is_fixed values are restored
  // Cleared by refresh_sketch_derived_state after the solver pass.
  // Empty vector = normal mode (no freeze, all non-fixed points
  // participate in the solve).
  std::vector<std::string> pending_append_focus_ids;

  // ── Vertex unification (Phase 4) ────────────────────────────
  // Monotonic counter for assigning vertex-N IDs.  Incremented by
  // rebuild_sketch_points for every new unique point.
  int next_vertex_index = 1;
};

}  // namespace polysmith::core
