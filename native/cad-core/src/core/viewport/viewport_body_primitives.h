#pragma once

#include <optional>
#include <string>
#include <vector>
#include "core/geometry/body_compiler.h"

namespace polysmith::core {

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

}  // namespace polysmith::core
