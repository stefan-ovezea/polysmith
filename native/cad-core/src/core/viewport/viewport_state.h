#pragma once

#include <vector>
#include "core/sketch/dof_counter.h"
#include "core/sketch/sketch_types.h"
#include "core/viewport/viewport_body_primitives.h"
#include "core/viewport/viewport_cam_primitives.h"
#include "core/viewport/viewport_reference_primitives.h"
#include "core/viewport/viewport_solid_primitives.h"
#include "core/viewport/viewport_sketch_primitives.h"
#include "core/viewport/viewport_snap_types.h"

namespace polysmith::core {

struct ViewportState {
  bool has_active_document;
  std::vector<ViewportBoxPrimitive> boxes;
  std::vector<ViewportCylinderPrimitive> cylinders;
  std::vector<ViewportPolygonExtrudePrimitive> polygon_extrudes;
  std::vector<ViewportSolidFace> solid_faces;
  std::vector<ViewportReferencePlane> reference_planes;
  std::vector<ViewportReferenceAxis> reference_axes;
  std::vector<ViewportReferencePoint> reference_points;
  std::vector<ViewportHelixPrimitive> helices;
  std::vector<ViewportSketchLinePrimitive> sketch_lines;
  std::vector<ViewportSketchCirclePrimitive> sketch_circles;
  std::vector<ViewportSketchEllipsePrimitive> sketch_ellipses;
  std::vector<ViewportSketchSplinePrimitive> sketch_splines;
  std::vector<ViewportSketchPolygonPrimitive> sketch_polygons;
  std::vector<ViewportSketchArcPrimitive> sketch_arcs;
  std::vector<ViewportSketchVertexPrimitive> sketch_vertices;
  std::vector<ViewportSketchDimensionPrimitive> sketch_dimensions;
  std::vector<ViewportSketchConstraintPrimitive> sketch_constraints;
  std::vector<ViewportSketchProfilePrimitive> sketch_profiles;
  // Per-entity DOF status array. Each entry mirrors the entity-id of
  // the corresponding line/circle/polygon/arc/point, with the status
  // string ("under", "full", "over"). Empty vector when unknown.
  std::vector<EntityDofResult> dof_statuses;
  // Total DOF count from the planegcs solver (-1 if not available).
  int solver_dofs = -1;
  // Number of conflicting / redundant constraints from the last solve.
  // -1 = solver hasn't run. Coupled with solver_dofs.
  int solver_conflicting_count = -1;
  int solver_redundant_count = -1;
  std::vector<ViewportMeshPrimitive> meshes;
  std::vector<ViewportCutPreview> cut_previews;
  // Available bodies (in document order) that boolean-mode extrudes can
  // target. Each entry's `id` is the root feature id of the body, and
  // `label` mirrors the human-readable feature name.
  std::vector<ViewportBodySummary> bodies;
  // Selectable edges (one entry per unique edge of every compiled body).
  // The renderer materializes these as line objects, raycasts against
  // them for picking, and calls `select_edge` with the entry's `id`.
  std::vector<ViewportEdgePrimitive> edges;
  // Selectable vertices (one entry per unique vertex of every compiled
  // body). Picked first in the raycast chain so they sit on top of edges
  // and faces. Empty when the document contains no boolean / fillet /
  // chamfer features (legacy primitive renderers don't emit vertices).
  std::vector<ViewportVertexPrimitive> vertices;
  std::vector<ViewportToolpathPrimitive> toolpaths;
  double scene_width;
  double scene_height;
  double scene_depth;
  ViewportSceneBounds scene_bounds;
  // Pre-computed snap targets for the active sketch, gated by the
  // document's SelectionFilter. The TS side checks distance against
  // these on each mouse move instead of computing locally.
  std::vector<SnapCandidate> snap_candidates;
  // Echo of the document's current selection filter so the UI can gate
  // snap / selection / highlight behavior without an extra IPC round-trip.
  SelectionFilter selection_filter;
};

}  // namespace polysmith::core
