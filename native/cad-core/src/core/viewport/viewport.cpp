#include "core/viewport/viewport.h"
#include "core/diagnostics/logger.h"
#include "core/cam/cam_resolution.h"
#include "core/drawing/drawing_sheet.h"
#include "core/geometry/face_signature.h"

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
#include <unordered_map>
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
#include "core/sketch/constraint_solver.h"
#include "core/sketch/spline_math.h"
#include "core/geometry/feature_shape.h"
#include "core/geometry/plane_frame_derivation.h"
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

// Imported (STEP/IGES) bodies above this edge count switch to the budgeted
// edge pass below: no per-vertex pick entries, and only the longest edges up
// to kOversizedImportMaxEdges. See the comment in
// append_cached_body_topology for the payload and scene-object budget this
// protects.
constexpr int kMaxImportPickEdgeCount = 8000;

// Budget for an oversized import's structural edges. A real STEP PCB carries
// ~78k edges (a 47.5 MB event with ~250k pick objects when every one was
// emitted), yet what the eye needs is the board outline, the part outlines and
// the step edges — all of them long. Keeping the edges at or above 1 mm up to
// this count turns a flat silhouette back into a readable model while staying
// inside a few MB.
constexpr double kOversizedImportMinEdgeLength = 1.0;
constexpr std::size_t kOversizedImportMaxEdges = 8000;

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

  // Body kind and label by id. The loop below used to scan the whole feature
  // history once per body, which is quadratic now that an imported assembly
  // arrives as one body per part (1 091 of them on the ESP32-P4C5 carrier).
  std::unordered_map<std::string, const FeatureEntry*> features_by_id;
  features_by_id.reserve(document.feature_history.size() * 2);
  for (const auto& feature : document.feature_history) {
    features_by_id.emplace(feature.id, &feature);
  }

  // Edge budget for imported bodies is a DOCUMENT budget, not a per-body one.
  // An assembly split into a thousand parts has ~50 edges per body — nothing
  // trips a per-body limit — while the total is what floods the payload.
  int total_import_edges = 0;
  for (const auto& body : compiled_bodies.bodies) {
    const auto feature_it = features_by_id.find(body.id);
    if (feature_it == features_by_id.end()) {
      continue;
    }
    const std::string& kind = feature_it->second->kind;
    if (kind != "step_import" && kind != "iges_import") {
      continue;
    }
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edge_map;
    TopExp::MapShapes(body.shape, TopAbs_EDGE, edge_map);
    total_import_edges += edge_map.Extent();
  }
  const bool oversized_import_document =
      total_import_edges > kMaxImportPickEdgeCount;

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
    const auto feature_it = features_by_id.find(body.id);
    if (feature_it != features_by_id.end()) {
      if (!feature_it->second->name.empty()) {
        label = feature_it->second->name;
      }
      body_kind = feature_it->second->kind;
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
        import_edge_count > kMaxImportPickEdgeCount ||
        oversized_import_document;

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
    } else if (oversized_import) {
      // An oversized import still gets its STRUCTURAL edges. Dropping them
      // entirely (the old behaviour) left a 24k-face board as one flat
      // silhouette with nothing to read: no part outlines, no board edge.
      // The long edges are exactly the ones that carry that structure, so
      // keep those up to the document budget — the 78k-edge event that
      // motivated the cap was 47.5 MB.
      //
      // `max_count` is compared against the ACCUMULATED output, and `next`
      // is shared by every body in this pass, so this one constant is
      // already the document-wide cap. Decrementing it per body would halve
      // the budget on every call.
      enumerate_body_edges(edge_pick_shape,
                           body.id,
                           document.selected_edge_ids,
                           /*facet_filter=*/nullptr,
                           next.edges,
                           /*min_length=*/kOversizedImportMinEdgeLength,
                           /*max_count=*/kOversizedImportMaxEdges);
    }

    // A box or cylinder body normally gets its faces from the legacy
    // primitive emitter, which builds analytic "front"/"top"/... faces
    // instead of enumerating OCCT topology. That emitter SKIPS any feature a
    // boolean consumed (solid_feature_emit.inc), and once a cut/join/
    // intersect — or a fillet, chamfer or shell — has replaced the primitive
    // with a real B-rep, this is the only place left that can produce its
    // faces. Keying this off the feature KIND alone left such a body with no
    // faces at all: a body that still renders as a mesh, but has no pick
    // proxy, cannot be selected, cannot host a sketch, and cannot take a
    // second feature. That is what "I can add a cylinder body but I cannot
    // make a hole with the extrusion" looked like — the cut ran, the body
    // survived, and every face of it had vanished.
    const bool faces_from_legacy_primitive =
        (body_kind == "box" || body_kind == "cylinder") &&
        compiled_bodies.consumed_feature_ids.find(body.id) ==
            compiled_bodies.consumed_feature_ids.end();
    if (!faces_from_legacy_primitive && body_kind != "mesh_import") {
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
#include "core/viewport/impl/face_import_color_fallback.inc"
#include "core/viewport/impl/cam_toolpath_emit.inc"
#include "core/viewport/impl/drawing_sheet_emit.inc"
#include "core/viewport/impl/viewport_state_return.inc"
}

}  // namespace polysmith::core
