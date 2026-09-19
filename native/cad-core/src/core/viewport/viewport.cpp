#include "core/viewport/viewport.h"
#include "core/diagnostics/logger.h"
#include "core/cam/cam_resolution.h"
#include "core/drawing/drawing_projection.h"
#include "core/drawing/drawing_runtime.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
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
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GeomLProp_SLProps.hxx>
#include <Poly_Triangulation.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Circ.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include "core/geometry/body_compiler.h"
#include "core/viewport/facet_edge_filter.h"
#include "core/cam/cam_runtime.h"
#include "core/cam/toolpath.h"
#include "core/cam/toolpath_geometry.h"
#include "core/document/appearance.h"
#include "core/sketch/dof_counter.h"
#include "core/sketch/spline_math.h"
#include "core/geometry/feature_shape.h"
#include "core/geometry/refresh_dependents.h"
#include "core/sketch/sketch_profile.h"
#include "core/sketch/impl/private_vertex_lookup_helpers.inc"

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

// Imported (STEP/IGES) bodies above this edge count emit ONLY their
// body mesh primitive — no per-edge/per-vertex/per-face pick entries.
// See the comment in append_cached_body_topology for the payload and
// scene-object budget this protects.
constexpr int kMaxImportPickEdgeCount = 8000;

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

  // mesh_import bodies emit NO per-face pick entries: their rendered
  // body mesh primitive (in `meshes`) already carries the body id for
  // the raycaster, and emitting one face entry per triangle cost ~3.5MB
  // of JSON per refresh (measured on a fan panel) — the oversized
  // viewport events were implicated in UI freezes after projections.
  // Clicking the mesh body's surface resolves to a "primitive" hit that
  // the UI routes to the body projection / body selection.
  //
  // Imported assemblies above kMaxImportPickEdgeCount edges skip the
  // per-edge/per-vertex pick entries: a STEP-imported PCB board
  // (78k edges, ~156k vertices, ~15k faces) produced a 47.5MB viewport
  // event whose ~250k edge/vertex/face pick objects made the UI
  // sluggish after every import, even once the seam-analysis rewrite
  // cut the rebuild from ~30 minutes to ~3 seconds. Their FACES still
  // emit decimated pick proxies (see decimate_pick_faces in
  // enumerate_body_faces) so sketch-on-face placement, face selection,
  // and face-based features keep working without the edge/vertex
  // flood. Small imported parts stay fully pickable unchanged.

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

    // Oversized-import budget check: one O(E) UNIQUE edge count
    // (MapShapes, not an explorer — explorers count occurrences, one
    // per adjacent face, roughly doubling the count), computed only for
    // imported kinds (see kMaxImportPickEdgeCount).
    int import_edge_count = 0;
    if (body_kind == "step_import" || body_kind == "iges_import") {
      NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edge_map;
      TopExp::MapShapes(body.shape, TopAbs_EDGE, edge_map);
      import_edge_count = edge_map.Extent();
    }
    const bool oversized_import =
        import_edge_count > kMaxImportPickEdgeCount;

    // Imported meshes emit NO per-edge/per-vertex pick entries: their
    // raw triangle soup carries ~10k facet edges + ~4.7k facet
    // vertices (~2MB of JSON per refresh on a fan panel), and the
    // facet-vertex cloud reads as stray "highlighted points".
    // Converted bodies (mesh_to_body) DO emit edges and vertices so
    // the Project tool can pick individual outline segments and
    // corners, but filtered to the SEMANTIC ones: FacetEdgeFilter
    // drops triangulation seams between coplanar faces (flat region
    // interiors), keeping the outline, steps, and hole rims.
    if (body_kind != "mesh_import" && !oversized_import) {
      // One shared FacetEdgeFilter for the mesh_to_body body — its
      // face-normal cache is reused by both enumerations (building it
      // twice doubled the viewport rebuild cost).
      const std::unique_ptr<FacetEdgeFilter> facet_filter =
          body_kind == "mesh_to_body"
              ? std::make_unique<FacetEdgeFilter>(body.shape)
              : nullptr;
      enumerate_body_edges(edge_pick_shape,
                           body.id,
                           document.selected_edge_ids,
                           facet_filter.get(),
                           next.edges);
      enumerate_body_vertices(body.shape,
                              body.id,
                              document.selected_vertex_ids,
                              facet_filter.get(),
                              next.vertices);
    }

    if (body_kind != "box" && body_kind != "cylinder" &&
        body_kind != "mesh_import") {
      enumerate_body_faces(body.shape,
                           body.id,
                           body_kind,
                           document,
                           document.selected_face_id,
                           /*decimate_pick_faces=*/oversized_import,
                           next.solid_faces);
    }
  }

  // Store the rebuilt topology in the cache FIRST, then copy it out to
  // the caller. Moving INTO the caller before assigning the cache left
  // the cache holding moved-from shells (empty triangle geometry, empty
  // summaries) — every subsequent cache hit served gutted faces, which
  // silently killed face picking after the cache key stopped changing.
  cache = std::move(next);
  bodies.insert(bodies.end(), cache.bodies.begin(), cache.bodies.end());
  edges.insert(edges.end(), cache.edges.begin(), cache.edges.end());
  vertices.insert(vertices.end(), cache.vertices.begin(), cache.vertices.end());
  solid_faces.insert(solid_faces.end(),
                     cache.solid_faces.begin(),
                     cache.solid_faces.end());
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
#include "core/viewport/impl/drawing_sheet_emit.inc"
#include "core/viewport/impl/viewport_state_return.inc"
}

}  // namespace polysmith::core
