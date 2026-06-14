#include "core/viewport/viewport.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRepGProp_Face.hxx>
#include <GProp_GProps.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Poly_Triangulation.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include "core/geometry/body_compiler.h"
#include "core/document/appearance.h"
#include "core/sketch/dof_counter.h"
#include "core/geometry/feature_shape.h"
#include "core/geometry/refresh_dependents.h"
#include "core/sketch/sketch_profile.h"

namespace polysmith::core {
namespace {

#include "core/viewport/impl/common_helpers.inc"
#include "core/viewport/impl/sketch_primitives.inc"
#include "core/viewport/impl/body_shape_helpers.inc"

struct CompiledBodyViewportCache {
  bool populated = false;
  std::string document_id;
  int revision = -1;
  std::optional<int> timeline_cursor;
  CompiledBodies compiled;
};

struct ViewportBodyTopologyCache {
  bool populated = false;
  std::string document_id;
  int revision = -1;
  std::optional<int> timeline_cursor;
  std::optional<std::string> selected_face_id;
  std::vector<std::string> selected_edge_ids;
  std::vector<std::string> selected_vertex_ids;
  std::vector<ViewportBodySummary> bodies;
  std::vector<ViewportEdgePrimitive> edges;
  std::vector<ViewportVertexPrimitive> vertices;
  std::vector<ViewportSolidFace> solid_faces;
};

CompiledBodies compile_bodies_for_viewport(const DocumentState& document) {
  static std::mutex cache_mutex;
  static CompiledBodyViewportCache cache;

  std::lock_guard<std::mutex> lock(cache_mutex);
  if (cache.populated && cache.document_id == document.id &&
      cache.revision == document.revision &&
      cache.timeline_cursor == document.timeline_cursor) {
    return cache.compiled;
  }

  CompiledBodies compiled = compile_bodies(document);
  cache.populated = true;
  cache.document_id = document.id;
  cache.revision = document.revision;
  cache.timeline_cursor = document.timeline_cursor;
  cache.compiled = compiled;
  return compiled;
}

void append_cached_body_topology(
    const DocumentState& document,
    const CompiledBodies& compiled_bodies,
    std::vector<ViewportBodySummary>& bodies,
    std::vector<ViewportEdgePrimitive>& edges,
    std::vector<ViewportVertexPrimitive>& vertices,
    std::vector<ViewportSolidFace>& solid_faces) {
  static std::mutex cache_mutex;
  static ViewportBodyTopologyCache cache;

  std::lock_guard<std::mutex> lock(cache_mutex);
  if (cache.populated && cache.document_id == document.id &&
      cache.revision == document.revision &&
      cache.timeline_cursor == document.timeline_cursor &&
      cache.selected_face_id == document.selected_face_id &&
      cache.selected_edge_ids == document.selected_edge_ids &&
      cache.selected_vertex_ids == document.selected_vertex_ids) {
    bodies.insert(bodies.end(), cache.bodies.begin(), cache.bodies.end());
    edges.insert(edges.end(), cache.edges.begin(), cache.edges.end());
    vertices.insert(vertices.end(), cache.vertices.begin(), cache.vertices.end());
    solid_faces.insert(solid_faces.end(),
                       cache.solid_faces.begin(),
                       cache.solid_faces.end());
    return;
  }

  ViewportBodyTopologyCache next{};
  next.populated = true;
  next.document_id = document.id;
  next.revision = document.revision;
  next.timeline_cursor = document.timeline_cursor;
  next.selected_face_id = document.selected_face_id;
  next.selected_edge_ids = document.selected_edge_ids;
  next.selected_vertex_ids = document.selected_vertex_ids;

  for (const auto& body : compiled_bodies.bodies) {
    std::string label = body.id;
    std::string body_kind;
    for (const auto& feature : document.feature_history) {
      if (feature.id == body.id) {
        if (!feature.name.empty()) {
          label = feature.name;
        }
        body_kind = feature.kind;
        break;
      }
    }

    const BodyBounds bounds = bounds_for_shape(body.shape);
    next.bodies.push_back(ViewportBodySummary{
        .id = body.id,
        .label = label,
        .center_x = bounds.center_x,
        .center_y = bounds.center_y,
        .center_z = bounds.center_z,
        .width = bounds.width,
        .height = bounds.height,
        .depth = bounds.depth,
        .local_frame = body.local_frame,
    });

    const TopoDS_Shape& edge_pick_shape =
        body.pick_shape.IsNull() ? body.shape : body.pick_shape;
    enumerate_body_edges(edge_pick_shape,
                         body.id,
                         document.selected_edge_ids,
                         next.edges);
    enumerate_body_vertices(body.shape,
                            body.id,
                            document.selected_vertex_ids,
                            next.vertices);

    if (body_kind != "box" && body_kind != "cylinder") {
      enumerate_body_faces(body.shape,
                           body.id,
                           body_kind,
                           document,
                           document.selected_face_id,
                           next.solid_faces);
    }
  }

  bodies.insert(bodies.end(), next.bodies.begin(), next.bodies.end());
  edges.insert(edges.end(), next.edges.begin(), next.edges.end());
  vertices.insert(vertices.end(), next.vertices.begin(), next.vertices.end());
  solid_faces.insert(solid_faces.end(),
                     next.solid_faces.begin(),
                     next.solid_faces.end());
  cache = std::move(next);
}

}  // namespace

ViewportState build_viewport_state(const std::optional<DocumentState>& document) {
#include "core/viewport/impl/empty_viewport_state.inc"
#include "core/viewport/impl/viewport_working_sets.inc"
#include "core/viewport/impl/cut_preview_emit.inc"
#include "core/viewport/impl/body_summary_emit.inc"

#include "core/viewport/impl/feature_history_emit.inc"

#include "core/viewport/impl/legacy_face_cleanup.inc"
#include "core/viewport/impl/scene_bounds_emit.inc"
#include "core/viewport/impl/snap_candidate_emit.inc"
#include "core/viewport/impl/dof_status_emit.inc"
#include "core/viewport/impl/face_appearance_finalize.inc"
#include "core/viewport/impl/cam_toolpath_emit.inc"
#include "core/viewport/impl/viewport_state_return.inc"
}

}  // namespace polysmith::core
