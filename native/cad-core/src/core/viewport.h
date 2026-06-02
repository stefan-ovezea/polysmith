#pragma once

#include <optional>
#include <vector>

#include "core/dof_counter.h"
#include "core/body_compiler.h"
#include "core/document.h"
#include "core/snap_engine.h"

namespace polysmith::core {

struct ViewportBoxPrimitive {
  std::string id;
  std::string label;
  double width;
  double height;
  double depth;
  double x_offset;
  double center_x;
  double center_y;
  double center_z;
  bool is_selected;
  std::optional<std::string> appearance_color;
};

struct ViewportCylinderPrimitive {
  std::string id;
  std::string label;
  double radius;
  double height;
  double x_offset;
  double center_x;
  double center_y;
  double center_z;
  bool is_selected;
  std::optional<std::string> appearance_color;
};

struct ViewportSketchPlaneFrame {
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

struct ViewportPolygonExtrudePrimitive {
  std::string id;
  std::string label;
  std::string plane_id;
  std::optional<ViewportSketchPlaneFrame> plane_frame;
  std::vector<SketchProfilePoint> profile_points;
  std::vector<std::vector<SketchProfilePoint>> inner_loops;
  double depth;
  bool is_selected;
  std::optional<std::string> appearance_color;
};

struct ViewportSolidFace {
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

  // owner_id is the body id that owns this face. For body-derived faces
  // (the only kind today) face_id is "<owner_id>:face:<index>" where the
  // index comes from TopExp::MapShapes(TopAbs_FACE) on the body shape, so
  // ids are stable as long as topology is unchanged.
  std::string face_id;
  std::string owner_id;
  std::string owner_kind;
  std::string label;
  // "planar" -> the underlying surface is a plane; sketch-on-face is
  // allowed and `plane_frame` is meaningful.
  // "non-planar" -> curved surface (fillet, cylinder side, etc.);
  // sketch-on-face is rejected; `plane_frame` is a representative frame
  // (face center + a derived normal) and shouldn't drive sketching.
  std::string sketchability;
  double center_x;
  double center_y;
  double center_z;
  double normal_x;
  double normal_y;
  double normal_z;
  PlaneFrame plane_frame;
  // Legacy analytical face dimensions. Used by per-feature analytical
  // face emission for legacy box/cylinder primitives that aren't
  // body-derived. Body-derived faces leave these at 0 and rely on the
  // triangulation arrays below.
  double width;
  double height;
  double radius;
  // World-space triangulation of the actual face. The UI uses this to
  // build a real BufferGeometry per face, which is both visually
  // accurate and gives precise raycasting (the picker hits the face
  // shape, not an analytical bounding rectangle). Empty for legacy
  // analytical faces — those still build their pick mesh from
  // (width, height, radius) + plane_frame on the UI side.
  // Layout: positions are flat x0,y0,z0,x1,y1,z1,...; indices are
  // triangle vertex indices into positions.
  std::vector<double> triangle_positions;
  std::vector<int> triangle_indices;
  bool is_selected;
  std::optional<std::string> appearance_color;
};

struct ViewportReferencePlane {
  std::string id;
  std::string label;
  // "xy" | "yz" | "xz" for the three origin planes — the renderer
  // applies a hardcoded rotation for those (legacy path). For
  // construction planes we set this to "custom" and ship a real
  // `plane_frame` instead; the renderer uses the frame to position
  // and orient the quad in world space.
  std::string orientation;
  double center_x;
  double center_y;
  double center_z;
  double width;
  double height;
  bool is_selected;
  bool is_active_sketch_plane;
  // World-space frame for construction planes. nullopt for origin
  // planes, which the renderer continues to handle via `orientation`.
  std::optional<PlaneFrame> plane_frame;
};

struct ViewportReferenceAxis {
  std::string id;
  std::string label;
  std::string axis;
  double start_x;
  double start_y;
  double start_z;
  double end_x;
  double end_y;
  double end_z;
};

struct ViewportReferencePoint {
  std::string id;
  std::string label;
  double x;
  double y;
  double z;
  bool is_selected;
};

struct ViewportHelixPrimitive {
  std::string id;
  std::string label;
  std::vector<double> points;
  bool is_selected;
};

struct ViewportSketchLinePrimitive {
  std::string line_id;
  std::string start_point_id;
  std::string end_point_id;
  std::string plane_id;
  double start_x;
  double start_y;
  double start_z;
  double end_x;
  double end_y;
  double end_z;
  bool is_selected;
  std::optional<std::string> constraint;
  bool is_construction = false;
  // True for entities generated by the in-progress Mirror tool
  // (`pending_mirror.generated_lines`). The UI renders them as a
  // dashed translucent preview; they're not selectable, not
  // hover-targetable, and don't appear in the points/dimensions
  // arrays. False for committed sketch lines.
  bool is_preview = false;
  std::string dof_status;
};

struct ViewportSketchCirclePrimitive {
  std::string circle_id;
  std::string plane_id;
  std::optional<ViewportSketchPlaneFrame> plane_frame;
  double center_x;
  double center_y;
  double center_z;
  double radius;
  bool is_selected;
  bool is_construction = false;
  // See `ViewportSketchLinePrimitive::is_preview`.
  bool is_preview = false;
  std::string dof_status;
};

// 2D arc primitive emitted to the viewport. Carries the arc's
// endpoints and center in world coordinates plus its radius and ccw
// flag so the renderer can sample the polyline locally without
// re-projecting back through the sketch plane. Endpoint world
// coordinates are kept alongside `start_point_id` / `end_point_id`
// so consumers (snapping, highlights) can look up the same shared
// SketchPoint that lines use without an extra lookup.
struct ViewportSketchArcPrimitive {
  std::string arc_id;
  std::string start_point_id;
  std::string end_point_id;
  std::string plane_id;
  std::optional<ViewportSketchPlaneFrame> plane_frame;
  double center_x;
  double center_y;
  double center_z;
  double radius;
  double start_x;
  double start_y;
  double start_z;
  double end_x;
  double end_y;
  double end_z;
  bool ccw;
  bool is_selected;
  bool is_construction = false;
  // See `ViewportSketchLinePrimitive::is_preview`.
  bool is_preview = false;
  std::string dof_status;
};

struct ViewportSketchPointPrimitive {
  std::string point_id;
  std::string plane_id;
  std::string kind;
  double position_x;
  double position_y;
  double position_z;
  bool is_fixed;
  bool is_selected;
  std::string dof_status;
};

struct ViewportSketchDimensionPrimitive {
  std::string dimension_id;
  std::string plane_id;
  std::string kind;
  std::string entity_id;
  std::string label;
  bool is_selected;
  double anchor_start_x;
  double anchor_start_y;
  double anchor_start_z;
  double anchor_end_x;
  double anchor_end_y;
  double anchor_end_z;
  double dimension_start_x;
  double dimension_start_y;
  double dimension_start_z;
  double dimension_end_x;
  double dimension_end_y;
  double dimension_end_z;
  double label_x;
  double label_y;
  double label_z;

  // Angle arc geometry (for angle/line_angle kinds; zero otherwise)
  double arc_center_x = 0.0;
  double arc_center_y = 0.0;
  double arc_center_z = 0.0;
  double arc_radius = 0.0;
  double arc_start_angle = 0.0;  // radians
  double arc_end_angle = 0.0;    // radians
  bool arc_ccw = false;

  // Reference line (for angle/line_angle kinds; zero otherwise)
  double ref_line_start_x = 0.0;
  double ref_line_start_y = 0.0;
  double ref_line_start_z = 0.0;
  double ref_line_end_x = 0.0;
  double ref_line_end_y = 0.0;
  double ref_line_end_z = 0.0;
};

struct ViewportSketchConstraintPrimitive {
  std::string constraint_id;
  std::string plane_id;
  std::string kind;
  std::string entity_id;
  std::optional<std::string> related_entity_id;
  std::string label;
  bool is_selected;
  double position_x;
  double position_y;
  double position_z;
};

struct ViewportSketchPolygonPrimitive {
  std::string polygon_id;
  std::string plane_id;
  std::optional<ViewportSketchPlaneFrame> plane_frame;
  // World-space corner positions as a flat xyzw list for each corner.
  std::vector<double> corner_x;
  std::vector<double> corner_y;
  std::vector<double> corner_z;
  int sides;
  std::string mode;
  double center_x;
  double center_y;
  double center_z;
  double radius;
  bool is_selected;
  bool is_construction = false;
  bool is_preview = false;
  std::string dof_status;
};

struct ViewportSketchProfilePrimitive {
  std::string profile_id;
  std::string plane_id;
  std::optional<ViewportSketchPlaneFrame> plane_frame;
  std::string profile_kind;
  std::vector<SketchProfilePoint> profile_points;
  std::vector<std::vector<SketchProfilePoint>> inner_loops;
  double start_x;
  double start_y;
  double width;
  double height;
  double radius;
  bool is_selected;
};

struct ViewportBodySummary {
  std::string id;
  std::string label;
  double center_x = 0.0;
  double center_y = 0.0;
  double center_z = 0.0;
  double width = 0.0;
  double height = 0.0;
  double depth = 0.0;
  BodyLocalFrame local_frame;
};

struct ViewportEdgePrimitive {
  // Stable across viewport snapshots when body topology is unchanged:
  // "<owner_body_id>:edge:<index>" where index is the position in
  // TopExp::MapShapes(TopAbs_EDGE) for the body. Selection state is
  // therefore preserved across mode/depth tweaks even when the user
  // hasn't changed which edges exist.
  std::string id;
  std::string owner_body_id;
  // "line" for straight segments (2 sample points), "circle" for full
  // circles, "curve" for everything else (general curve sampled to a
  // polyline). Drives nothing in the core but lets the renderer pick a
  // tighter tessellation budget if it wants to.
  std::string kind;
  // Flat polyline samples in world space: x0, y0, z0, x1, y1, z1, ...
  // The renderer connects consecutive points with line segments.
  std::vector<double> points;
  // Exact length of the edge in millimetres (the document's units).
  // Computed by OCCT (BRepGProp::LinearProperties) so it is accurate
  // for arcs and curves, not just the sampled polyline. Surfaced here
  // (rather than via a separate measure IPC) because the viewport
  // already enumerates every edge for picking — adding the length is
  // O(1) extra work per edge and avoids a second round-trip whenever
  // the UI wants to show "selected edge: X mm".
  double length;
  bool is_selected;
};

struct ViewportVertexPrimitive {
  // "<owner_body_id>:vertex:<index>" where index is the 0-based position
  // in TopExp::MapShapes(TopAbs_VERTEX) for the body. Stable across
  // viewport snapshots when body topology is unchanged.
  std::string id;
  std::string owner_body_id;
  // World-space position of the vertex.
  double x;
  double y;
  double z;
  bool is_selected;
};

struct ViewportMeshPrimitive {
  std::string id;
  // Triangulated body geometry in world space.
  // Layout matches three.js BufferGeometry attributes: each vertex
  // takes three consecutive entries in `positions` and `normals`,
  // and `indices` is a flat list of triangle vertex indices.
  std::vector<double> positions;
  std::vector<double> normals;
  std::vector<int> indices;
  bool is_selected;
  std::optional<std::string> appearance_color;
};

// Translucent red preview of the cutter volume for an in-progress cut
// extrude. Emitted only while the cut feature is the currently selected
// feature (i.e. the user is editing it via the floating panel). The UI
// renders this in red so the user can see exactly which volume is
// about to be removed, mirroring common CAD workflow's cut preview.
struct ViewportCutPreview {
  // The feature id of the cut extrude this preview belongs to.
  std::string id;
  // World-space triangulation of the cutter shape. Same layout as
  // ViewportMeshPrimitive.
  std::vector<double> positions;
  std::vector<double> normals;
  std::vector<int> indices;
};

struct ViewportSceneBounds {
  double center_x;
  double center_y;
  double center_z;
  double width;
  double height;
  double depth;
  double max_dimension;
};

struct ViewportToolpathPoint {
  double x;
  double y;
  double z;
  bool is_rapid;
};

struct ViewportToolpathPrimitive {
  std::string id;
  std::string label;
  std::vector<ViewportToolpathPoint> points;
};

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
  std::vector<ViewportSketchPolygonPrimitive> sketch_polygons;
  std::vector<ViewportSketchArcPrimitive> sketch_arcs;
  std::vector<ViewportSketchPointPrimitive> sketch_points;
  std::vector<ViewportSketchDimensionPrimitive> sketch_dimensions;
  std::vector<ViewportSketchConstraintPrimitive> sketch_constraints;
  std::vector<ViewportSketchProfilePrimitive> sketch_profiles;
  // Per-entity DOF status array. Each entry mirrors the entity-id of
  // the corresponding line/circle/polygon/arc/point, with the status
  // string ("under", "full", "over"). Empty vector when unknown.
  std::vector<EntityDofResult> dof_statuses;
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

ViewportState build_viewport_state(const std::optional<DocumentState>& document);

}  // namespace polysmith::core
