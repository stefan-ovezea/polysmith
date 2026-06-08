#pragma once

#include <set>
#include <string>
#include <vector>

#include <TopoDS_Shape.hxx>

namespace polysmith::core {

struct DocumentState;

struct BodyLocalFrame {
  double x_axis_x = 1.0;
  double x_axis_y = 0.0;
  double x_axis_z = 0.0;
  double y_axis_x = 0.0;
  double y_axis_y = 1.0;
  double y_axis_z = 0.0;
  double z_axis_x = 0.0;
  double z_axis_y = 0.0;
  double z_axis_z = 1.0;
};

// One solid body produced by walking the feature history with boolean
// operators applied. The `id` matches the feature_id of the body's root
// feature (the most recent `new_body` extrude that started the body).
struct CompiledBody {
  std::string id;
  TopoDS_Shape shape;
  BodyLocalFrame local_frame;
  // Stable shape used for edge picking while a fillet/chamfer feature
  // targeting this body is in its pending phase. When non-null the
  // viewport enumerates body edges from this shape (whose topology
  // doesn't change as the user toggles pending edges) instead of
  // `shape`, so edge ids stay stable for the duration of the panel
  // session. Null otherwise; callers should fall back to `shape`.
  TopoDS_Shape pick_shape;
};

struct BodyMesh {
  std::string body_id;
  std::vector<double> vertices;  // x0, y0, z0, x1, y1, z1, ...
  std::vector<int> indices;      // triangle vertex indices into `vertices`
  std::vector<double> normals;   // matching `vertices` layout
};

struct CompiledBodies {
  // Per-body OCCT shapes for downstream consumers (export, etc.).
  std::vector<CompiledBody> bodies;
  // Tessellated mesh data for viewport rendering.
  std::vector<BodyMesh> meshes;
  // Feature ids whose source primitives must be suppressed by the legacy
  // viewport emission, because they participate in a boolean op (the body
  // that contains them is rendered as a `BodyMesh` instead).
  std::set<std::string> consumed_feature_ids;
};

// Walk feature_history once, building per-body OCCT shapes via
// BRepAlgoAPI_Fuse / BRepAlgoAPI_Cut according to each extrude's `mode`,
// then triangulate every body with BRepMesh_IncrementalMesh.
//
// When no extrude has a non-default mode the result still includes one
// CompiledBody per solid feature, but `consumed_feature_ids` stays empty
// and `meshes` only contains the per-body tessellations. Callers are
// responsible for deciding whether to render meshes or fall back to the
// legacy primitive emission.
CompiledBodies compile_bodies(const DocumentState& document);

}  // namespace polysmith::core
